#include "native_count_index.hpp"

#include "direct_count_facts.hpp"

#include <gbwt/gbwt.h>
#include <gbwtgraph/utils.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <exception>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <utility>

#include <omp.h>

namespace pancollapse::native_count {
namespace {

using direct_count::CountFactCatalog;
using direct_count::PreparedAnnotationReference;
using direct_count::ProfileId;
using count_reference::missing;

constexpr std::array<char, 16> magic = {'P', 'C', 'A', 'N', 'B', 'N', 'D', '1'};
constexpr std::array<char, 8> trailer = {'P', 'C', 'A', 'N', 'E', 'N', 'D', '1'};
enum Section : std::size_t {
    metadata_section,
    strings_section,
    catalog_section,
    sets_section,
    genes_section,
    policies_section,
    lookup_section,
    paths_section,
    pairs_section,
    refs_section,
    section_count
};

struct SectionRange {
    std::uint64_t offset = 0, size = 0;
};

static_assert(std::endian::native == std::endian::little,
              "PCANBND1 fixed sections require little-endian hosts");
static_assert(sizeof(count_reference::GeneRecord) == 36);
static_assert(sizeof(count_reference::PolicyRecord) == 16);
static_assert(sizeof(PathRecord) == 32);
static_assert(sizeof(PairRecord) == 8);

[[noreturn]] void invalid(std::string_view why) {
    throw std::runtime_error("native count index: " + std::string(why));
}

std::uint32_t checked_id(std::size_t value) {
    if (value >= missing) invalid("uint32 dictionary range exceeded");
    return static_cast<std::uint32_t>(value);
}

std::uint64_t checked_add(std::uint64_t left, std::uint64_t right, std::string_view what) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) {
        invalid(std::string(what) + " overflows uint64");
    }
    return left + right;
}

std::uint64_t checked_mul(std::uint64_t left, std::uint64_t right, std::string_view what) {
    if (left && right > std::numeric_limits<std::uint64_t>::max() / left) {
        invalid(std::string(what) + " overflows uint64");
    }
    return left * right;
}

std::uint64_t align8(std::uint64_t value) {
    if (value > std::numeric_limits<std::uint64_t>::max() - 7) invalid("section layout overflows");
    return (value + 7) & ~std::uint64_t(7);
}

void put_number(std::ostream& output, std::uint64_t value, unsigned width) {
    for (unsigned i = 0; i < width; ++i) {
        output.put(static_cast<char>((value >> (8 * i)) & 0xff));
    }
}

std::uint64_t get_number(const std::byte* bytes, unsigned width) {
    std::uint64_t value = 0;
    for (unsigned i = 0; i < width; ++i) {
        value |= std::uint64_t(std::to_integer<unsigned char>(bytes[i])) << (8 * i);
    }
    return value;
}

void require_output(std::ostream& output) {
    if (!output) invalid("output write failed");
}

std::uint64_t strings_size(const std::vector<std::string>& values) {
    std::uint64_t total = 0;
    for (const auto& value : values) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
            invalid("string exceeds uint32 framing");
        }
        total = checked_add(total, 4 + value.size(), "string section");
    }
    return total;
}

std::uint64_t sets_size(const std::vector<std::vector<std::uint32_t>>& values) {
    std::uint64_t total = 0;
    for (const auto& value : values) {
        total = checked_add(total, 8, "set section");
        total = checked_add(total, checked_mul(value.size(), 4, "set section"), "set section");
    }
    return total;
}

template<class T> std::uint64_t fixed_size(const std::vector<T>& values, std::string_view name) {
    return checked_mul(values.size(), sizeof(T), name);
}

void write_strings(std::ostream& output, const std::vector<std::string>& values) {
    for (const auto& value : values) {
        put_number(output, value.size(), 4);
        output.write(value.data(), static_cast<std::streamsize>(value.size()));
        require_output(output);
    }
}

void write_sets(std::ostream& output, const std::vector<std::vector<std::uint32_t>>& values) {
    for (const auto& value : values) {
        put_number(output, value.size(), 8);
        for (const auto element : value) put_number(output, element, 4);
        require_output(output);
    }
}

template<class T> void write_fixed(std::ostream& output, const std::vector<T>& values) {
    if (values.empty()) return;
    output.write(reinterpret_cast<const char*>(values.data()),
                 static_cast<std::streamsize>(values.size() * sizeof(T)));
    require_output(output);
}

class Cursor {
  public:
    Cursor(const std::byte* begin, std::uint64_t size)
        : current_(begin), end_(begin + size) {}

    std::uint64_t remaining() const {
        return static_cast<std::uint64_t>(end_ - current_);
    }
    std::uint64_t number(unsigned width) {
        if (remaining() < width) invalid("variable section is truncated");
        const auto result = get_number(current_, width);
        current_ += width;
        return result;
    }
    std::string string(std::uint64_t size) {
        if (size > remaining() || size > std::numeric_limits<std::size_t>::max()) {
            invalid("variable section is truncated");
        }
        std::string result(reinterpret_cast<const char*>(current_), static_cast<std::size_t>(size));
        current_ += size;
        return result;
    }
    bool done() const { return current_ == end_; }

  private:
    const std::byte* current_;
    const std::byte* end_;
};

std::vector<std::string> read_strings(const std::byte* begin, std::uint64_t size) {
    Cursor input(begin, size);
    std::vector<std::string> result;
    while (!input.done()) result.push_back(input.string(input.number(4)));
    return result;
}

std::vector<std::vector<std::uint32_t>> read_sets(const std::byte* begin, std::uint64_t size) {
    Cursor input(begin, size);
    std::vector<std::vector<std::uint32_t>> result;
    while (!input.done()) {
        const auto count = input.number(8);
        if (count > input.remaining() / 4 || count > std::numeric_limits<std::size_t>::max()) {
            invalid("set section count exceeds bounds");
        }
        auto& values = result.emplace_back();
        values.reserve(static_cast<std::size_t>(count));
        for (std::uint64_t i = 0; i < count; ++i) {
            values.push_back(static_cast<std::uint32_t>(input.number(4)));
        }
    }
    return result;
}

void validate_string_set(const Data& data, std::uint32_t set_id, std::string_view what) {
    if (set_id >= data.sets.size()) invalid(std::string(what) + " has invalid set");
    for (const auto value : data.sets[set_id]) {
        if (value >= data.strings.size()) invalid(std::string(what) + " has invalid string");
    }
}

void validate_gene(const Data& data, const count_reference::GeneRecord& gene) {
    if (gene.id_string == missing || gene.id_string >= data.strings.size() ||
        (gene.name_string != missing && gene.name_string >= data.strings.size())) {
        invalid("gene has invalid string");
    }
    validate_string_set(data, gene.type_set, "gene type");
    for (std::size_t profile = 0; profile < 2; ++profile) {
        if ((gene.equivalence[profile] != missing && gene.equivalence[profile] >= data.genes.size()) ||
            (gene.nested_host[profile] != missing && gene.nested_host[profile] >= data.genes.size()) ||
            gene.competition[profile] > 2) {
            invalid("gene has invalid profile policy");
        }
    }
}

void validate_policy(const Data& data, const count_reference::PolicyRecord& policy) {
    if (policy.gene == missing || policy.gene >= data.genes.size() ||
        (policy.novel_origin != missing && policy.novel_origin >= data.genes.size()) ||
        (policy.flags & ~std::uint32_t(7)) != 0) {
        invalid("policy has invalid gene");
    }
    validate_string_set(data, policy.category_set, "policy category");
}

void validate_path(const Data& data, const PathRecord& path) {
    const auto roles = path.flags & (exon_path | body_path);
    if ((roles != exon_path && roles != body_path) || (path.flags & ~(exon_path | body_path)) != 0 ||
        path.transcript_string == missing || path.transcript_string >= data.strings.size() ||
        path.policy == missing || path.policy >= data.policies.size() ||
        path.support_set >= data.sets.size() || path.strand > 1 ||
        path.model_first > data.model_refs.size() ||
        path.model_count > data.model_refs.size() - path.model_first) {
        invalid("path row is invalid");
    }
    validate_policy(data, data.policies[path.policy]);
}

void validate_data(const Data& data) {
    if (data.paths.size() >= missing || data.pairs.size() >= missing) {
        invalid("path or pair count exceeds uint32 range");
    }
    for (const auto& gene : data.genes) validate_gene(data, gene);
    for (const auto& policy : data.policies) validate_policy(data, policy);

    if (data.feature_catalog.size() != data.genes.size() ||
        !std::is_sorted(data.feature_catalog.begin(), data.feature_catalog.end()) ||
        std::adjacent_find(data.feature_catalog.begin(), data.feature_catalog.end()) !=
            data.feature_catalog.end()) {
        invalid("feature catalog is not lexical and unique");
    }
    for (std::size_t i = 0; i < data.genes.size(); ++i) {
        if (data.feature_catalog[i] != data.strings[data.genes[i].id_string]) {
            invalid("feature catalog does not match genes");
        }
    }

    std::vector<std::uint8_t> lookup_seen(data.paths.size(), 0);
    for (const auto row : data.path_lookup) {
        if (row == missing) continue;
        if (row >= data.paths.size() || lookup_seen[row]++) invalid("path lookup is not one-to-one");
    }
    if (std::find(lookup_seen.begin(), lookup_seen.end(), 0) != lookup_seen.end()) {
        invalid("annotated path has no GBWT lookup");
    }

    std::uint64_t expected_ref = 0;
    for (const auto& path : data.paths) {
        validate_path(data, path);
        if (path.model_first != expected_ref) invalid("model refs are not CSR");
        expected_ref = checked_add(expected_ref, path.model_count, "model refs");
    }
    if (expected_ref != data.model_refs.size()) invalid("model refs do not cover CSR");

    for (const auto& pair : data.pairs) {
        if (pair.exon >= data.paths.size() || pair.body >= data.paths.size() ||
            (data.paths[pair.exon].flags & exon_path) == 0 ||
            (data.paths[pair.body].flags & body_path) == 0 ||
            data.policies[data.paths[pair.exon].policy].gene !=
                data.policies[data.paths[pair.body].policy].gene) {
            invalid("pair has invalid roles or gene");
        }
    }
    std::vector<std::uint8_t> pair_seen(data.pairs.size(), 0);
    for (std::size_t row = 0; row < data.paths.size(); ++row) {
        const auto& path = data.paths[row];
        std::uint32_t prior = 0;
        bool first = true;
        for (std::uint64_t offset = 0; offset < path.model_count; ++offset) {
            const auto id = data.model_refs[path.model_first + offset];
            if (id >= data.pairs.size() || (!first && id <= prior)) invalid("invalid model ref order");
            first = false; prior = id;
            const auto& pair = data.pairs[id];
            if (pair.exon == row) {
                if (pair_seen[id] & 1) invalid("duplicate exon pair reference");
                pair_seen[id] |= 1;
            } else if (pair.body == row) {
                if (pair_seen[id] & 2) invalid("duplicate body pair reference");
                pair_seen[id] |= 2;
            } else {
                invalid("model ref is not incident to path");
            }
        }
    }
    if (std::find_if(pair_seen.begin(), pair_seen.end(),
                     [](std::uint8_t value) { return value != 3; }) != pair_seen.end()) {
        invalid("pair inverse references are incomplete");
    }
}

void sync_file(const std::filesystem::path& path, bool directory = false) {
    const int fd = open(path.c_str(), O_RDONLY | (directory ? O_DIRECTORY : 0));
    if (fd < 0) invalid("cannot open output for fsync");
    const int result = fsync(fd);
    close(fd);
    if (result != 0) invalid("output fsync failed");
}

template<class Writer> void atomic_publish(const std::filesystem::path& requested, Writer&& writer) {
    const auto destination = std::filesystem::absolute(requested).lexically_normal();
    if (std::filesystem::exists(destination)) invalid("destination already exists");
    std::filesystem::create_directories(destination.parent_path());
    std::string pattern = destination.string() + ".tmp.XXXXXX";
    const int staged_fd = mkstemp(pattern.data());
    if (staged_fd < 0) invalid("cannot create staged output");
    close(staged_fd);
    const std::filesystem::path staged(pattern);
    bool published = false;
    try {
        writer(staged);
        sync_file(staged);
        if (link(staged.c_str(), destination.c_str()) != 0) {
            invalid("cannot publish (destination exists or link failed)");
        }
        published = true;
        if (unlink(staged.c_str()) != 0) {
            throw std::runtime_error("native count index was published at " + destination.string() +
                                     ", but staged cleanup failed");
        }
        sync_file(destination.parent_path(), true);
    } catch (...) {
        if (!published) {
            const int ignored = unlink(staged.c_str());
            static_cast<void>(ignored);
        }
        throw;
    }
}

struct ParentInfo {
    std::uint32_t transcript = missing;
    std::uint32_t policy = missing;
    std::uint32_t support_set = 0;
    std::uint32_t linked_exon = missing;
    std::uint8_t exon = 0;
    std::uint8_t strand = 0;
};

struct ParentRange {
    std::uint32_t exon_first = 0, exon_next = 0;
    std::uint32_t body_first = 0, body_next = 0;
};

void increment(std::uint32_t& value, std::string_view what) {
    if (value == std::numeric_limits<std::uint32_t>::max()) invalid(std::string(what) + " overflows");
    ++value;
}

bool missing_sample(std::string_view sample) {
    return sample.empty() || sample == "." || sample == "NA" || sample == "N/A" ||
           sample == "None" || sample == "nan";
}

class Dictionaries {
  public:
    explicit Dictionaries(Data& output) : output_(output) {
        intern_set({});
    }

    std::uint32_t string(std::string_view value) {
        auto [found, inserted] = strings_.try_emplace(std::string(value), 0);
        if (inserted) {
            found->second = checked_id(output_.strings.size());
            output_.strings.push_back(found->first);
        }
        return found->second;
    }

    std::uint32_t intern_set(std::vector<std::uint32_t> values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
        auto [found, inserted] = sets_.try_emplace(std::move(values), 0);
        if (inserted) {
            found->second = checked_id(output_.sets.size());
            output_.sets.push_back(found->first);
        }
        return found->second;
    }

    std::uint32_t string_set(const std::vector<std::string>& values) {
        std::vector<std::uint32_t> ids;
        ids.reserve(values.size());
        for (const auto& value : values) ids.push_back(string(value));
        return intern_set(std::move(ids));
    }

  private:
    Data& output_;
    std::unordered_map<std::string, std::uint32_t> strings_;
    std::map<std::vector<std::uint32_t>, std::uint32_t> sets_;
};

std::uint64_t name_hash(std::string_view value) {
    std::uint64_t result = 1469598103934665603ULL;
    for (const unsigned char character : value) {
        result ^= character;
        result *= 1099511628211ULL;
    }
    return result;
}

struct NameRow {
    std::uint64_t hash = 0;
    std::uint32_t row = missing;
};

} // namespace

struct Index::Impl {
    int fd = -1;
    const std::byte* mapped = nullptr;
    std::size_t mapped_size = 0;
    std::vector<std::string> strings;
    std::vector<std::string> feature_catalog;
    std::vector<std::vector<std::uint32_t>> sets;
    const count_reference::GeneRecord* genes = nullptr;
    std::size_t gene_count = 0;
    const count_reference::PolicyRecord* policies = nullptr;
    std::size_t policy_count = 0;
    const std::uint32_t* path_lookup = nullptr;
    std::size_t lookup_count = 0;
    const PathRecord* paths = nullptr;
    std::size_t path_count = 0;
    const PairRecord* pairs = nullptr;
    std::size_t pair_count = 0;
    const std::uint32_t* model_refs = nullptr;
    std::size_t ref_count = 0;
    // Certificates for immutable mapped rows, not a second copy of the index.
    // Concurrent first readers may repeat checks; a failed check publishes no
    // certificate for that check. There is no validating/spin state.
    static constexpr std::uint8_t path_valid = 1, model_refs_valid = 2;
    std::unique_ptr<std::atomic<std::uint8_t>[]> validated;
    std::uint64_t gbwt_size = 0;
    std::string content_id;

    void validate_string_set(std::uint32_t set_id, std::string_view what) const {
        if (set_id >= sets.size()) invalid(std::string(what) + " has invalid set");
        for (const auto value : sets[set_id]) {
            if (value >= strings.size()) invalid(std::string(what) + " has invalid string");
        }
    }
    void validate_policy(std::uint32_t policy_id) const {
        if (policy_id == missing || policy_id >= policy_count) invalid("path has invalid policy");
        const auto& policy = policies[policy_id];
        if (policy.gene == missing || policy.gene >= gene_count ||
            (policy.novel_origin != missing && policy.novel_origin >= gene_count) ||
            (policy.flags & ~std::uint32_t(7)) != 0) {
            invalid("policy has invalid gene");
        }
        validate_string_set(policy.category_set, "policy category");
        const auto& gene = genes[policy.gene];
        if (gene.id_string == missing || gene.id_string >= strings.size() ||
            (gene.name_string != missing && gene.name_string >= strings.size())) {
            invalid("gene has invalid string");
        }
        validate_string_set(gene.type_set, "gene type");
        for (std::size_t profile = 0; profile < 2; ++profile) {
            if ((gene.equivalence[profile] != missing && gene.equivalence[profile] >= gene_count) ||
                (gene.nested_host[profile] != missing && gene.nested_host[profile] >= gene_count) ||
                gene.competition[profile] > 2) {
                invalid("gene has invalid profile policy");
            }
        }
    }
    void validate_path(const PathRecord& path) const {
        const auto roles = path.flags & (exon_path | body_path);
        if ((roles != exon_path && roles != body_path) ||
            (path.flags & ~(exon_path | body_path)) != 0 ||
            path.transcript_string == missing || path.transcript_string >= strings.size() ||
            path.support_set >= sets.size() || path.strand > 1 ||
            path.model_first > ref_count || path.model_count > ref_count - path.model_first) {
            invalid("path row is invalid");
        }
        validate_policy(path.policy);
    }
    // Callers have already checked row provenance/bounds. The backing index is
    // immutable for this instance's lifetime, as required by reference caches.
    void validate_row(std::size_t row) const {
        if ((validated[row].load(std::memory_order_acquire) & path_valid) != 0) return;
        validate_path(paths[row]);
        validated[row].fetch_or(path_valid, std::memory_order_release);
    }

    ~Impl() {
        if (mapped) munmap(const_cast<std::byte*>(mapped), mapped_size);
        if (fd >= 0) close(fd);
    }
};

namespace {

std::size_t checked_fixed_count(std::uint64_t bytes, std::size_t element_size, std::string_view name) {
    if (bytes % element_size || bytes / element_size > std::numeric_limits<std::size_t>::max()) {
        invalid(std::string(name) + " fixed section is malformed");
    }
    return static_cast<std::size_t>(bytes / element_size);
}

} // namespace

Index::Index() : impl_(std::make_unique<Impl>()) {}
Index::~Index() = default;
Index::Index(Index&&) noexcept = default;
Index& Index::operator=(Index&&) noexcept = default;

void write(const std::filesystem::path& path, const Data& data) {
    validate_data(data);

    std::array<std::uint64_t, section_count> sizes{};
    sizes[metadata_section] = checked_add(16, data.content_id.size(), "metadata section");
    sizes[strings_section] = strings_size(data.strings);
    sizes[catalog_section] = strings_size(data.feature_catalog);
    sizes[sets_section] = sets_size(data.sets);
    sizes[genes_section] = fixed_size(data.genes, "gene section");
    sizes[policies_section] = fixed_size(data.policies, "policy section");
    sizes[lookup_section] = fixed_size(data.path_lookup, "lookup section");
    sizes[paths_section] = fixed_size(data.paths, "path section");
    sizes[pairs_section] = fixed_size(data.pairs, "pair section");
    sizes[refs_section] = fixed_size(data.model_refs, "model-ref section");

    std::array<SectionRange, section_count> sections{};
    std::uint64_t next = magic.size() + section_count * 16;
    for (std::size_t section = 0; section < section_count; ++section) {
        next = align8(next);
        sections[section] = {next, sizes[section]};
        next = checked_add(next, sizes[section], "section layout");
    }

    atomic_publish(path, [&](const std::filesystem::path& staged) {
        std::ofstream output(staged, std::ios::binary | std::ios::trunc);
        if (!output) invalid("cannot open staged output");
        output.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        for (const auto& section : sections) {
            put_number(output, section.offset, 8);
            put_number(output, section.size, 8);
        }
        require_output(output);
        std::uint64_t written = magic.size() + section_count * 16;
        auto start_section = [&](Section section) {
            const auto target = sections[section].offset;
            std::array<char, 8> zero{};
            while (written < target) {
                const auto count = static_cast<std::streamsize>(std::min<std::uint64_t>(
                    target - written, zero.size()));
                output.write(zero.data(), count);
                written += static_cast<std::uint64_t>(count);
            }
            require_output(output);
        };
        auto finish_section = [&](Section section) {
            written = checked_add(written, sections[section].size, "output layout");
        };

        start_section(metadata_section);
        put_number(output, data.gbwt_size_bytes, 8);
        put_number(output, data.content_id.size(), 8);
        output.write(data.content_id.data(), static_cast<std::streamsize>(data.content_id.size()));
        require_output(output);
        finish_section(metadata_section);

        start_section(strings_section); write_strings(output, data.strings); finish_section(strings_section);
        start_section(catalog_section); write_strings(output, data.feature_catalog); finish_section(catalog_section);
        start_section(sets_section); write_sets(output, data.sets); finish_section(sets_section);
        start_section(genes_section); write_fixed(output, data.genes); finish_section(genes_section);
        start_section(policies_section); write_fixed(output, data.policies); finish_section(policies_section);
        start_section(lookup_section); write_fixed(output, data.path_lookup); finish_section(lookup_section);
        start_section(paths_section); write_fixed(output, data.paths); finish_section(paths_section);
        start_section(pairs_section); write_fixed(output, data.pairs); finish_section(pairs_section);
        start_section(refs_section); write_fixed(output, data.model_refs); finish_section(refs_section);
        output.write(trailer.data(), static_cast<std::streamsize>(trailer.size()));
        output.flush();
        require_output(output);
        output.close();
        if (!output) invalid("cannot close staged output");
    });
}

bool is_index(const std::filesystem::path& path) noexcept {
    try {
        std::ifstream input(path, std::ios::binary);
        std::array<char, magic.size()> candidate{};
        return input.read(candidate.data(), static_cast<std::streamsize>(candidate.size())).gcount() ==
                   static_cast<std::streamsize>(candidate.size()) &&
               candidate == magic;
    } catch (...) {
        return false;
    }
}

Index Index::load(const std::filesystem::path& path) {
    Index result;
    result.impl_->fd = open(path.c_str(), O_RDONLY);
    if (result.impl_->fd < 0) invalid("cannot open input");
    struct stat status {};
    if (fstat(result.impl_->fd, &status) != 0 || status.st_size < 0) invalid("cannot stat input");
    const auto minimum = magic.size() + section_count * 16 + trailer.size();
    if (static_cast<std::uintmax_t>(status.st_size) < minimum ||
        static_cast<std::uintmax_t>(status.st_size) > std::numeric_limits<std::size_t>::max()) {
        invalid("input is too short or too large");
    }
    result.impl_->mapped_size = static_cast<std::size_t>(status.st_size);
    void* mapped = mmap(nullptr, result.impl_->mapped_size, PROT_READ, MAP_PRIVATE, result.impl_->fd, 0);
    if (mapped == MAP_FAILED) invalid("cannot mmap input");
    result.impl_->mapped = static_cast<const std::byte*>(mapped);
    if (std::memcmp(result.impl_->mapped, magic.data(), magic.size()) != 0) invalid("invalid magic");

    std::array<SectionRange, section_count> sections{};
    std::uint64_t previous = magic.size() + section_count * 16;
    for (std::size_t section = 0; section < section_count; ++section) {
        const auto* entry = result.impl_->mapped + magic.size() + section * 16;
        sections[section] = {get_number(entry, 8), get_number(entry + 8, 8)};
        if (sections[section].offset != align8(previous) ||
            sections[section].offset > result.impl_->mapped_size ||
            sections[section].size > result.impl_->mapped_size - sections[section].offset) {
            invalid("section framing is invalid");
        }
        previous = checked_add(sections[section].offset, sections[section].size, "section framing");
    }
    if (previous > result.impl_->mapped_size - trailer.size() ||
        previous + trailer.size() != result.impl_->mapped_size ||
        std::memcmp(result.impl_->mapped + previous, trailer.data(), trailer.size()) != 0) {
        invalid("trailer framing is invalid");
    }
    const auto section = [&](Section id) { return result.impl_->mapped + sections[id].offset; };
    if (sections[metadata_section].size < 16) invalid("metadata section is short");
    result.impl_->gbwt_size = get_number(section(metadata_section), 8);
    const auto content_size = get_number(section(metadata_section) + 8, 8);
    if (content_size != sections[metadata_section].size - 16 ||
        content_size > std::numeric_limits<std::size_t>::max()) {
        invalid("metadata section is invalid");
    }
    result.impl_->content_id.assign(reinterpret_cast<const char*>(section(metadata_section) + 16),
                                    static_cast<std::size_t>(content_size));
    result.impl_->strings = read_strings(section(strings_section), sections[strings_section].size);
    result.impl_->feature_catalog = read_strings(section(catalog_section), sections[catalog_section].size);
    result.impl_->sets = read_sets(section(sets_section), sections[sets_section].size);

    result.impl_->genes = reinterpret_cast<const count_reference::GeneRecord*>(section(genes_section));
    result.impl_->gene_count = checked_fixed_count(sections[genes_section].size,
        sizeof(count_reference::GeneRecord), "gene");
    result.impl_->policies = reinterpret_cast<const count_reference::PolicyRecord*>(section(policies_section));
    result.impl_->policy_count = checked_fixed_count(sections[policies_section].size,
        sizeof(count_reference::PolicyRecord), "policy");
    result.impl_->path_lookup = reinterpret_cast<const std::uint32_t*>(section(lookup_section));
    result.impl_->lookup_count = checked_fixed_count(sections[lookup_section].size,
        sizeof(std::uint32_t), "lookup");
    result.impl_->paths = reinterpret_cast<const PathRecord*>(section(paths_section));
    result.impl_->path_count = checked_fixed_count(sections[paths_section].size,
        sizeof(PathRecord), "path");
    result.impl_->pairs = reinterpret_cast<const PairRecord*>(section(pairs_section));
    result.impl_->pair_count = checked_fixed_count(sections[pairs_section].size,
        sizeof(PairRecord), "pair");
    result.impl_->model_refs = reinterpret_cast<const std::uint32_t*>(section(refs_section));
    result.impl_->ref_count = checked_fixed_count(sections[refs_section].size,
        sizeof(std::uint32_t), "model-ref");
    // C++20 value-initializes each atomic byte to zero. No path table is scanned
    // or validated eagerly: certificates are filled on first successful access.
    result.impl_->validated = std::make_unique<std::atomic<std::uint8_t>[]>(result.impl_->path_count);
    return result;
}

const std::vector<std::string>& Index::strings() const { return impl_->strings; }
const std::vector<std::string>& Index::feature_catalog() const { return impl_->feature_catalog; }
std::vector<std::string> Index::take_feature_catalog() { return std::move(impl_->feature_catalog); }
const std::vector<std::vector<std::uint32_t>>& Index::sets() const { return impl_->sets; }
std::span<const count_reference::GeneRecord> Index::genes() const {
    return {impl_->genes, impl_->gene_count};
}
std::span<const count_reference::PolicyRecord> Index::policies() const {
    return {impl_->policies, impl_->policy_count};
}
const std::string& Index::content_id() const { return impl_->content_id; }
std::size_t Index::path_count() const { return impl_->lookup_count; }
std::uint64_t Index::gbwt_size_bytes() const { return impl_->gbwt_size; }
std::span<const PathRecord> Index::paths() const { return {impl_->paths, impl_->path_count}; }
std::span<const PairRecord> Index::pairs() const { return {impl_->pairs, impl_->pair_count}; }

std::uint32_t Index::path_row(std::uint64_t gbwt_path_id) const {
    if (gbwt_path_id >= impl_->lookup_count) throw std::out_of_range("native GBWT path id");
    const auto row = impl_->path_lookup[gbwt_path_id];
    if (row == missing) return row;
    if (row >= impl_->path_count) invalid("path lookup points outside path rows");
    impl_->validate_row(row);
    return row;
}

std::span<const std::uint32_t> Index::model_ids(const PathRecord& path) const {
    if (!impl_->path_count) invalid("path row does not belong to index");
    const auto first = reinterpret_cast<std::uintptr_t>(impl_->paths);
    const auto address = reinterpret_cast<std::uintptr_t>(&path);
    const auto bytes = impl_->path_count * sizeof(PathRecord);
    if (address < first || address - first > bytes - sizeof(PathRecord) ||
        (address - first) % sizeof(PathRecord) != 0) {
        invalid("path row does not belong to index");
    }
    const auto row = static_cast<std::size_t>((address - first) / sizeof(PathRecord));
    // Keep span safety/provenance checks even after deep validation is cached.
    if (path.model_first > impl_->ref_count ||
        path.model_count > impl_->ref_count - path.model_first) invalid("path row is invalid");
    impl_->validate_row(row);
    const auto values = std::span<const std::uint32_t>(
        impl_->model_refs + path.model_first, path.model_count);
    if ((impl_->validated[row].load(std::memory_order_acquire) & Impl::model_refs_valid) != 0)
        return values;
    for (const auto model : values) {
        if (model >= impl_->pair_count) invalid("model ref points outside pairs");
        const auto& pair = impl_->pairs[model];
        if (pair.exon >= impl_->path_count || pair.body >= impl_->path_count ||
            (pair.exon != row && pair.body != row)) {
            invalid("model ref is not incident to path");
        }
        impl_->validate_row(pair.exon);
        impl_->validate_row(pair.body);
        if ((impl_->paths[pair.exon].flags & exon_path) == 0 ||
            (impl_->paths[pair.body].flags & body_path) == 0 ||
            impl_->policies[impl_->paths[pair.exon].policy].gene !=
                impl_->policies[impl_->paths[pair.body].policy].gene) {
            invalid("pair has invalid roles or gene");
        }
    }
    impl_->validated[row].fetch_or(Impl::model_refs_valid, std::memory_order_release);
    return values;
}

void build(const BuildOptions& options) {
    const auto started = std::chrono::steady_clock::now();
    if (options.output.empty() || std::filesystem::exists(options.output)) {
        invalid("output must be a new path");
    }
    if ((options.source_index.empty() == options.count_bundle.empty()) || options.gbwt.empty() ||
        options.threads == 0) {
        invalid("native compiler requires one fact source, GBWT, output, and threads");
    }
    const std::vector<ProfileId> profiles{ProfileId::cr7_v1, ProfileId::pansc_strict_v1};
    PreparedAnnotationReference source;
    std::cerr << "panCollapse: native count indexing: phase=annotation_load_begin\n";
    if (!options.source_index.empty()) {
        source = CountFactCatalog::load_prepared_annotation_index_trusted(options.source_index, profiles);
    } else {
        const auto bundle = direct_count::load_count_fact_bundle(options.count_bundle);
        source.ledger = path_identity::read(bundle.files.at("path_identity_ledger").path);
        source.facts = CountFactCatalog::load(bundle, source.ledger);
    }

    const auto parent_count = source.facts.parents().size();
    const auto row_count = source.ledger.rows_by_path.size();
    if (parent_count != source.ledger.identities_by_parent.size() || row_count >= missing ||
        parent_count >= missing) {
        invalid("annotation source has incompatible Parent or row cardinality");
    }
    std::cerr << "panCollapse: native count indexing: phase=annotation rows=" << row_count
              << " Parents=" << parent_count << '\n';

    Data data;
    data.content_id = source.facts.content_id();
    Dictionaries dictionaries(data);
    std::map<std::string, std::uint32_t, std::less<>> gene_ids;
    for (const auto& [name, gene] : source.facts.genes()) {
        const auto id = checked_id(data.genes.size());
        gene_ids.emplace(name, id);
        count_reference::GeneRecord record;
        record.id_string = dictionaries.string(name);
        record.type_set = dictionaries.string_set(gene.gene_types);
        if (gene.gene_name) record.name_string = dictionaries.string(*gene.gene_name);
        data.feature_catalog.push_back(name);
        data.genes.push_back(record);
    }
    for (const auto& [name, id] : gene_ids) {
        for (std::size_t profile = 0; profile < profiles.size(); ++profile) {
            const auto& fact = source.facts.profile_gene(profiles[profile], name);
            data.genes[id].equivalence[profile] = fact.equivalence_gene.empty()
                ? id : gene_ids.at(fact.equivalence_gene);
            data.genes[id].nested_host[profile] = fact.nested_host.empty()
                ? missing : gene_ids.at(fact.nested_host);
            data.genes[id].competition[profile] = static_cast<std::uint32_t>(fact.competition);
        }
    }

    std::vector<ParentInfo> parents(parent_count);
    std::vector<ParentRange> ranges(parent_count);
    std::unordered_map<const void*, std::uint32_t> parent_rank;
    parent_rank.reserve(parent_count);
    std::map<std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>, std::uint32_t>
        policy_ids;
    std::unordered_map<std::string, std::uint32_t> sample_ids;
    for (std::size_t rank = 0; rank < parent_count; ++rank) {
        const auto fact = source.facts.parent_by_rank(rank);
        const auto* identity = source.ledger.identities_by_parent.at_rank(rank).second;
        if (!parent_rank.emplace(identity->unique_parent.storage_identity(), checked_id(rank)).second) {
            invalid("duplicate Parent identity storage");
        }
        auto& parent = parents[rank];
        parent.exon = identity->feature_layer == "exon";
        parent.linked_exon = parent.exon ? checked_id(rank) :
            checked_id(source.ledger.identities_by_parent.index_of(identity->exon_unique_parent));
        parent.transcript = dictionaries.string(source.ledger.target_transcript(*identity));
        parent.strand = identity->strand == "-" ? 1 : 0;
        if (!parent.exon && !missing_sample(fact.sample.view())) {
            auto [sample, inserted] = sample_ids.try_emplace(
                std::string(fact.sample.view()), checked_id(sample_ids.size()));
            static_cast<void>(inserted);
            parent.support_set = dictionaries.intern_set({sample->second});
        }
        const auto flags = (fact.novel_paralog ? 1u : 0u) |
            (fact.strong_local_support ? 2u : 0u) |
            (fact.protected_primary_protein_nested_host ? 4u : 0u);
        const auto key = std::make_tuple(gene_ids.at(fact.count_gene.str()),
            dictionaries.string_set(fact.raw_categories),
            fact.novel_origin_gene.empty() ? missing : gene_ids.at(fact.novel_origin_gene.str()),
            flags);
        auto [policy, fresh] = policy_ids.try_emplace(key, checked_id(data.policies.size()));
        if (fresh) {
            data.policies.push_back(
                {std::get<0>(key), std::get<1>(key), std::get<2>(key), std::get<3>(key)});
        }
        parent.policy = policy->second;
        if ((rank + 1) % 1000000 == 0)
            std::cerr << "panCollapse: native count indexing: phase=numeric_parents completed="
                      << rank + 1 << '/' << parent_count << '\n';
    }
    for (const auto& parent : parents) {
        if (!parent.exon && (parent.linked_exon >= parents.size() ||
                             !parents[parent.linked_exon].exon)) {
            invalid("body Parent is not linked to an exon Parent");
        }
    }

    std::vector<std::uint32_t> row_parent(row_count);
    for (std::size_t rank = 0; rank < row_count; ++rank) {
        const auto& row = source.ledger.rows_by_path.at_rank(rank).second;
        const auto found = parent_rank.find(row.annotation.unique_parent.storage_identity());
        const auto parent_id = found == parent_rank.end()
            ? checked_id(source.ledger.identities_by_parent.index_of(row.annotation.unique_parent))
            : found->second;
        row_parent[rank] = parent_id;
        const auto& parent = parents[parent_id];
        if (parent.exon) increment(ranges[parent_id].exon_next, "exon row count");
        else increment(ranges[parent.linked_exon].body_next, "body row count");
    }
    parent_rank.clear();
    parent_rank.rehash(0);

    std::uint64_t exon_count = 0, body_count = 0;
    for (auto& range : ranges) {
        const auto exons = range.exon_next, bodies = range.body_next;
        range.exon_first = checked_id(exon_count);
        exon_count = checked_add(exon_count, exons, "exon rows");
        range.exon_next = range.exon_first;
        range.body_first = checked_id(body_count);
        body_count = checked_add(body_count, bodies, "body rows");
        range.body_next = range.body_first;
    }
    std::vector<std::uint32_t> exon_rows(static_cast<std::size_t>(exon_count));
    std::vector<std::uint32_t> body_rows(static_cast<std::size_t>(body_count));
    data.paths.resize(row_count);
    for (std::size_t rank = 0; rank < row_count; ++rank) {
        const auto parent_id = row_parent[rank];
        const auto& parent = parents[parent_id];
        auto& path = data.paths[rank];
        path.transcript_string = parent.transcript;
        path.policy = parent.policy;
        path.support_set = parent.support_set;
        path.flags = parent.exon ? exon_path : body_path;
        path.strand = parent.strand;
        if (parent.exon) {
            exon_rows[ranges[parent_id].exon_next++] = checked_id(rank);
        } else {
            body_rows[ranges[parent.linked_exon].body_next++] = checked_id(rank);
        }
    }
    row_parent.clear();
    row_parent.shrink_to_fit();

    std::cerr << "panCollapse: native count indexing: phase=raw-pairs exon_paths=" << exon_count
              << " body_paths=" << body_count << '\n';
    for (std::size_t parent_id = 0; parent_id < parent_count; ++parent_id) {
        if (!parents[parent_id].exon) continue;
        const auto& range = ranges[parent_id];
        if (range.exon_first == range.exon_next && range.body_first == range.body_next) continue;
        if (range.exon_first == range.exon_next || range.body_first == range.body_next) {
            invalid("linked exon Parent does not have both exon and body paths");
        }
        for (auto exon = range.exon_first; exon < range.exon_next; ++exon) {
            for (auto body = range.body_first; body < range.body_next; ++body) {
                const auto exon_row = exon_rows[exon], body_row = body_rows[body];
                if (data.policies[data.paths[exon_row].policy].gene !=
                    data.policies[data.paths[body_row].policy].gene) {
                    invalid("linked exon/body facts change counted gene");
                }
                if (data.pairs.size() >= missing) invalid("raw pair count exceeds uint32 range");
                data.pairs.push_back({exon_row, body_row});
            }
        }
    }
    ranges.clear(); ranges.shrink_to_fit();
    exon_rows.clear(); exon_rows.shrink_to_fit();
    body_rows.clear(); body_rows.shrink_to_fit();

    std::vector<std::uint32_t> incident(data.paths.size(), 0);
    for (const auto& pair : data.pairs) {
        increment(incident[pair.exon], "path pair incidence");
        increment(incident[pair.body], "path pair incidence");
    }
    std::uint64_t ref_count = 0;
    for (std::size_t row = 0; row < data.paths.size(); ++row) {
        data.paths[row].model_first = ref_count;
        data.paths[row].model_count = 0;
        ref_count = checked_add(ref_count, incident[row], "model refs");
    }
    if (ref_count > std::numeric_limits<std::size_t>::max()) invalid("model refs exceed address space");
    data.model_refs.resize(static_cast<std::size_t>(ref_count));
    for (std::size_t id = 0; id < data.pairs.size(); ++id) {
        const auto& pair = data.pairs[id];
        auto& exon = data.paths[pair.exon];
        auto& body = data.paths[pair.body];
        data.model_refs[exon.model_first + exon.model_count++] = checked_id(id);
        data.model_refs[body.model_first + body.model_count++] = checked_id(id);
    }
    incident.clear(); incident.shrink_to_fit();
    parents.clear(); parents.shrink_to_fit();
    sample_ids.clear(); sample_ids.rehash(0);
    policy_ids.clear();
    source.facts.release_parent_facts();

    std::cerr << "panCollapse: native count indexing: phase=name-index rows=" << row_count
              << " raw_pairs=" << data.pairs.size() << " model_refs=" << data.model_refs.size() << '\n';
    std::vector<NameRow> names(row_count);
    const int workers = static_cast<int>(std::min<std::size_t>(options.threads, INT_MAX));
    #pragma omp parallel for schedule(static) num_threads(workers)
    for (std::size_t row = 0; row < row_count; ++row) {
        const auto& name = source.ledger.rows_by_path.at_rank(row).first;
        names[row] = {name_hash(name.view()), checked_id(row)};
    }
    std::sort(names.begin(), names.end(), [](const NameRow& left, const NameRow& right) {
        return left.hash != right.hash ? left.hash < right.hash : left.row < right.row;
    });

    std::ifstream input(options.gbwt, std::ios::binary);
    if (!input) invalid("cannot open GBWT");
    gbwt::GBWT gbwt;
    gbwt.load(input);
    if (!gbwt.bidirectional() || !gbwt.hasMetadata() || !gbwt.metadata.hasPathNames() ||
        !gbwt.metadata.hasSampleNames() || !gbwt.metadata.hasContigNames() ||
        gbwt.sequences() % 2 != 0 || gbwt.metadata.paths() != gbwt.sequences() / 2) {
        invalid("GBWT requires complete bidirectional path metadata");
    }
    for (std::size_t path_id = 0; path_id < gbwt.metadata.paths(); ++path_id) {
        const auto& name = gbwt.metadata.path(path_id);
        if (name.sample >= gbwt.metadata.samples() || name.contig >= gbwt.metadata.contigs()) {
            invalid("GBWT path metadata has invalid sample or contig");
        }
    }
    data.path_lookup.assign(gbwt.metadata.paths(), missing);
    std::vector<std::uint8_t> bound(row_count, 0);
    const auto reference_samples = gbwtgraph::parse_reference_samples_tag(gbwt);
    std::atomic<std::size_t> bound_count{0};
    std::exception_ptr binding_error;
    std::mutex binding_mutex;
    auto find_row = [&](std::string_view name) {
        const auto hash = name_hash(name);
        const auto first = std::lower_bound(names.begin(), names.end(), hash,
            [](const NameRow& row, std::uint64_t wanted) { return row.hash < wanted; });
        for (auto candidate = first; candidate != names.end() && candidate->hash == hash; ++candidate) {
            if (source.ledger.rows_by_path.at_rank(candidate->row).first.view() == name) {
                return candidate->row;
            }
        }
        return missing;
    };
    std::cerr << "panCollapse: native count indexing: phase=gbwt-bind metadata_paths="
              << gbwt.metadata.paths() << '\n';
    #pragma omp parallel for schedule(dynamic, 4096) num_threads(workers)
    for (std::size_t path_id = 0; path_id < gbwt.metadata.paths(); ++path_id) {
        try {
            const auto sense = gbwtgraph::get_path_sense(gbwt, path_id, reference_samples);
            const auto name = gbwtgraph::compose_path_name(gbwt, path_id, sense);
            const auto row = find_row(name);
            if (row == missing) continue;
            if (std::atomic_ref<std::uint8_t>(bound[row]).exchange(1)) {
                invalid("duplicate GBWT path binding");
            }
            data.path_lookup[path_id] = row;
            bound_count.fetch_add(1, std::memory_order_relaxed);
        } catch (...) {
            std::lock_guard guard(binding_mutex);
            if (!binding_error) binding_error = std::current_exception();
        }
    }
    if (binding_error) std::rethrow_exception(binding_error);
    if (bound_count.load(std::memory_order_relaxed) != row_count) {
        const auto missing_row = std::find(bound.begin(), bound.end(), 0);
        invalid("ledger path is absent from GBWT metadata at lexical row " +
                std::to_string(std::distance(bound.begin(), missing_row)));
    }
    names.clear(); names.shrink_to_fit();
    bound.clear(); bound.shrink_to_fit();
    source.ledger = {};

    data.gbwt_size_bytes = static_cast<std::uint64_t>(std::filesystem::file_size(options.gbwt));
    std::cerr << "panCollapse: native count indexing: phase=write paths=" << data.paths.size()
              << " pairs=" << data.pairs.size() << " lookup=" << data.path_lookup.size() << '\n';
    write(options.output, data);
    std::cout << "{\"schema\":\"pancollapse-native-count-index-v1\",\"status\":\"complete\",\"paths\":"
              << data.paths.size() << ",\"pairs\":" << data.pairs.size() << ",\"size_bytes\":"
              << std::filesystem::file_size(options.output) << ",\"seconds\":"
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()
              << "}\n";
}

} // namespace pancollapse::native_count
