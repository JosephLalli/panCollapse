#include "direct_count_index.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace pancollapse::direct_index {
namespace {

static_assert(std::endian::native == std::endian::little,
              "direct indexes require a little-endian host");
static_assert(sizeof(GeneRecord) == 36 && sizeof(PolicyRecord) == 16 &&
              sizeof(ModelRecord) == 16 && sizeof(Posting) == 12 &&
              sizeof(JunctionOwner) == 8, "unexpected record layout");

constexpr std::array<char, 8> magic = {'P', 'C', 'D', 'I', 'R', 'E', 'C', 'T'};
constexpr std::array<char, 8> trailer = {'P', 'C', 'D', 'E', 'N', 'D', '0', '1'};
constexpr std::uint32_t version = 2;
constexpr std::uint32_t complete = 1;
constexpr std::uint64_t endian = 0x0102030405060708ULL;
constexpr std::size_t metadata_words = 14;
constexpr std::size_t scratch_bytes = 64 * 1024;
constexpr std::uint32_t body_flags = 0x0c;

enum Section : std::size_t {
    meta, strings, catalog, sets, genes, policies, models, slots, lists,
    postings, splices, junctions, provenance, section_count,
};
constexpr std::uint64_t header_bytes = 32 + section_count * 16;
constexpr std::uint64_t trailer_bytes = 16;

struct Range { std::uint64_t offset = 0, size = 0; };
struct NodeSlot { std::uint32_t length = 0, list = count_reference::missing; };
struct ListInfo { std::uint64_t offset = 0; std::uint32_t count = 0, reserved = 0; };
struct PairRow {
    std::uint64_t from = 0, to = 0, first = 0;
    std::uint32_t count = 0, reserved = 0;
};
static_assert(sizeof(NodeSlot) == 8 && sizeof(ListInfo) == 16 && sizeof(PairRow) == 32);

[[noreturn]] void fail(std::string_view what) {
    throw std::runtime_error("direct count index: " + std::string(what));
}

std::uint64_t add(std::uint64_t left, std::uint64_t right) {
    if (right > std::numeric_limits<std::uint64_t>::max() - left) fail("integer overflow");
    return left + right;
}
std::uint64_t multiply(std::uint64_t left, std::uint64_t right) {
    if (left != 0 && right > std::numeric_limits<std::uint64_t>::max() / left) fail("integer overflow");
    return left * right;
}
std::uint64_t align8(std::uint64_t value) { return add(value, 7) & ~std::uint64_t{7}; }
std::size_t as_size(std::uint64_t value, std::string_view what) {
    if (value > std::numeric_limits<std::size_t>::max()) fail(what);
    return static_cast<std::size_t>(value);
}
std::size_t allocation(std::size_t count, std::size_t width, std::string_view what) {
    if (width != 0 && count > std::numeric_limits<std::size_t>::max() / width) fail(what);
    return count * width;
}
std::uint64_t bytes(std::size_t count, std::size_t width) {
    return multiply(static_cast<std::uint64_t>(count), static_cast<std::uint64_t>(width));
}

void read_all(int fd, void* value, std::size_t count, std::uint64_t offset) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) fail("file offset");
    auto* cursor = static_cast<std::byte*>(value);
    while (count != 0) {
        const auto result = ::pread(fd, cursor, count, static_cast<off_t>(offset));
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) fail("staging file truncated");
        cursor += result;
        count -= static_cast<std::size_t>(result);
        offset = add(offset, static_cast<std::uint64_t>(result));
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) fail("file offset");
    }
}
void write_at(int fd, const void* value, std::size_t count, std::uint64_t offset) {
    if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) fail("file offset");
    const auto* cursor = static_cast<const std::byte*>(value);
    while (count != 0) {
        const auto result = ::pwrite(fd, cursor, count, static_cast<off_t>(offset));
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) fail("staging write failed");
        cursor += result;
        count -= static_cast<std::size_t>(result);
        offset = add(offset, static_cast<std::uint64_t>(result));
        if (offset > static_cast<std::uint64_t>(std::numeric_limits<off_t>::max())) fail("file offset");
    }
}
std::uint32_t get32(const std::byte* value) {
    return std::uint32_t(std::to_integer<unsigned char>(value[0])) |
           (std::uint32_t(std::to_integer<unsigned char>(value[1])) << 8) |
           (std::uint32_t(std::to_integer<unsigned char>(value[2])) << 16) |
           (std::uint32_t(std::to_integer<unsigned char>(value[3])) << 24);
}
std::uint64_t get64(const std::byte* value) {
    std::uint64_t result = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        result |= std::uint64_t(std::to_integer<unsigned char>(value[index])) << (8 * index);
    }
    return result;
}

struct TempFile {
    std::filesystem::path path;
    int fd = -1;
    std::uint64_t size = 0;
    std::uint64_t flushed_size = 0;
    std::array<std::byte, scratch_bytes> pending{};
    std::size_t pending_size = 0;
    TempFile() = default;
    TempFile(const TempFile&) = delete;
    TempFile& operator=(const TempFile&) = delete;
    ~TempFile() { discard(); }

    void create(const std::filesystem::path& output, std::string_view label) {
        const auto parent = output.has_parent_path() ? output.parent_path() : std::filesystem::path{"."};
        const auto leaf = output.filename().string();
        if (leaf.empty()) fail("output filename");
        std::string pattern = (parent / ("." + leaf + "." + std::string(label) + ".XXXXXX")).string();
        std::vector<char> writable(pattern.begin(), pattern.end());
        writable.push_back('\0');
        fd = ::mkstemp(writable.data());
        if (fd < 0) fail("staging create failed");
        path = writable.data();
        size = 0;
        flushed_size = 0;
        pending_size = 0;
    }
    void flush() {
        if (pending_size == 0) return;
        write_at(fd, pending.data(), pending_size, flushed_size);
        flushed_size = add(flushed_size, pending_size);
        pending_size = 0;
    }
    void append(const void* value, std::size_t count) {
        const auto next = add(size, static_cast<std::uint64_t>(count));
        const auto* cursor = static_cast<const std::byte*>(value);
        while (count != 0) {
            if (pending_size == pending.size()) flush();
            if (pending_size == 0 && count >= pending.size()) {
                write_at(fd, cursor, count, flushed_size);
                flushed_size = add(flushed_size, count);
                break;
            }
            const auto copied = std::min(count, pending.size() - pending_size);
            std::memcpy(pending.data() + pending_size, cursor, copied);
            pending_size += copied;
            cursor += copied;
            count -= copied;
        }
        size = next;
    }
    void read(std::uint64_t offset, void* value, std::size_t count) const {
        if (offset > size || count > size - offset) fail("staging range");
        auto* cursor = static_cast<std::byte*>(value);
        if (offset < flushed_size) {
            const auto disk = static_cast<std::size_t>(std::min<std::uint64_t>(count, flushed_size - offset));
            read_all(fd, cursor, disk, offset);
            cursor += disk;
            count -= disk;
            offset = flushed_size;
        }
        if (count != 0) {
            if (offset < flushed_size || offset - flushed_size > pending_size ||
                count > pending_size - static_cast<std::size_t>(offset - flushed_size)) {
                fail("staging pending range");
            }
            std::memcpy(cursor, pending.data() + static_cast<std::size_t>(offset - flushed_size), count);
        }
    }
    void sync() { flush(); if (fd < 0 || ::fsync(fd) != 0) fail("staging fsync failed"); }
    void close_checked() {
        if (fd < 0) return;
        flush();
        const auto closing = fd;
        fd = -1;
        if (::close(closing) != 0) fail("staging close failed");
    }
    void unlink_checked() {
        if (path.empty()) return;
        if (::unlink(path.c_str()) != 0 && errno != ENOENT) fail("staging unlink failed");
        path.clear();
    }
    void discard() noexcept {
        if (fd >= 0) { static_cast<void>(::close(fd)); fd = -1; }
        pending_size = 0;
        if (!path.empty()) { static_cast<void>(::unlink(path.c_str())); path.clear(); }
    }
};

struct Mapping {
    const std::byte* data = nullptr;
    std::size_t size = 0;
    Mapping() = default;
    Mapping(const Mapping&) = delete;
    Mapping& operator=(const Mapping&) = delete;
    ~Mapping() { if (data != nullptr) static_cast<void>(::munmap(const_cast<std::byte*>(data), size)); }
    void map(int fd, std::uint64_t bytes_to_map) {
        if (bytes_to_map == 0) return;
        const auto amount = as_size(bytes_to_map, "mmap size");
        void* mapped = ::mmap(nullptr, amount, PROT_READ, MAP_PRIVATE, fd, 0);
        if (mapped == MAP_FAILED) fail("mmap failed");
        data = static_cast<const std::byte*>(mapped);
        size = amount;
    }
};

struct Budget {
    explicit Budget(std::size_t maximum) : limit(maximum) {}
    std::size_t limit, used = 0;
    void charge(std::size_t amount) {
        if (amount > limit - used) fail("memory limit");
        used += amount;
    }
    void release(std::size_t amount) {
        if (amount > used) fail("memory accounting");
        used -= amount;
    }
};
void add_memory(std::size_t& total, std::size_t amount) {
    if (amount > std::numeric_limits<std::size_t>::max() - total) fail("metadata memory");
    total += amount;
}
template<class T> std::size_t memory_for(const std::vector<T>& values) {
    return allocation(values.capacity(), sizeof(T), "metadata memory");
}
std::size_t metadata_memory(const Metadata& data) {
    std::size_t result = 0;
    add_memory(result, memory_for(data.strings));
    add_memory(result, memory_for(data.feature_catalog));
    add_memory(result, memory_for(data.sets));
    add_memory(result, memory_for(data.genes));
    add_memory(result, memory_for(data.policies));
    add_memory(result, memory_for(data.models));
    for (const auto& value : data.strings) add_memory(result, value.capacity() + 1);
    for (const auto& value : data.feature_catalog) add_memory(result, value.capacity() + 1);
    for (const auto& value : data.sets) add_memory(result, memory_for(value));
    add_memory(result, data.content_id.capacity() + 1);
    add_memory(result, data.provenance_json.capacity() + 1);
    return result;
}

std::size_t encode_varint(std::uint64_t value, std::array<std::byte, 6>& output) {
    std::size_t count = 0;
    do {
        auto byte = static_cast<unsigned char>(value & 0x7fU);
        value >>= 7;
        if (value != 0) byte |= 0x80U;
        if (count == output.size()) fail("posting varint width");
        output[count++] = static_cast<std::byte>(byte);
    } while (value != 0);
    return count;
}

class PostingEncoder {
  public:
    explicit PostingEncoder(std::span<const Posting> values) : values_(values) {}
    std::size_t read(std::byte* output, std::size_t capacity) {
        std::size_t written = 0;
        while (written < capacity) {
            if (pending_at_ == pending_size_) {
                if (index_ == values_.size()) break;
                const auto model = values_[index_].model;
                const auto delta = index_ == 0 ? model : model - prior_model_;
                const auto code = (static_cast<std::uint64_t>(delta) << 4) | values_[index_].flags;
                pending_size_ = encode_varint(code, pending_);
                pending_at_ = 0;
                prior_model_ = model;
                ++index_;
            }
            const auto amount = std::min(capacity - written, pending_size_ - pending_at_);
            std::memcpy(output + written, pending_.data() + pending_at_, amount);
            written += amount;
            pending_at_ += amount;
        }
        return written;
    }
    bool done() const { return index_ == values_.size() && pending_at_ == pending_size_; }

  private:
    std::span<const Posting> values_;
    std::size_t index_ = 0, pending_at_ = 0, pending_size_ = 0;
    std::uint32_t prior_model_ = 0;
    std::array<std::byte, 6> pending_{};
};

struct EncodedSummary { std::uint64_t digest = 1469598103934665603ULL, bytes = 0; };
EncodedSummary summarize(std::span<const Posting> values, unsigned bits) {
    EncodedSummary result;
    PostingEncoder encoder(values);
    std::array<std::byte, scratch_bytes> scratch{};
    while (!encoder.done()) {
        const auto count = encoder.read(scratch.data(), scratch.size());
        if (count == 0) fail("posting encoder stalled");
        result.bytes = add(result.bytes, count);
        for (std::size_t index = 0; index < count; ++index) {
            result.digest ^= std::to_integer<unsigned char>(scratch[index]);
            result.digest *= 1099511628211ULL;
        }
    }
    auto& digest = result.digest;
    if (bits == 64) return result;
    if (bits == 0) digest = 0;
    else digest &= (std::uint64_t{1} << bits) - 1;
    return result;
}

struct PairKey {
    std::uint64_t from = 0, to = 0;
    bool operator==(const PairKey&) const = default;
};
struct PairHash {
    std::size_t operator()(const PairKey& value) const {
        auto mixed = value.from ^ std::rotl(value.to, 23);
        mixed ^= mixed >> 30; mixed *= 0xbf58476d1ce4e5b9ULL;
        mixed ^= mixed >> 27; mixed *= 0x94d049bb133111ebULL;
        return static_cast<std::size_t>(mixed ^ (mixed >> 31));
    }
};

bool optional_string(std::uint32_t value, const Metadata& data) {
    return value == count_reference::missing || value < data.strings.size();
}
void validate_metadata(const Metadata& data) {
    if (data.models.size() > count_reference::missing) fail("model id width");
    for (const auto& text : data.strings) if (text.size() > UINT32_MAX) fail("string width");
    for (const auto& text : data.feature_catalog) if (text.size() > UINT32_MAX) fail("string width");
    const auto set_ok = [&](std::uint32_t value) { return value < data.sets.size(); };
    for (const auto& gene : data.genes) {
        if (!optional_string(gene.id_string, data) || !optional_string(gene.name_string, data) ||
            !set_ok(gene.type_set)) fail("gene metadata");
        for (std::size_t side = 0; side < 2; ++side) {
            if ((gene.equivalence[side] != count_reference::missing && gene.equivalence[side] >= data.genes.size()) ||
                (gene.nested_host[side] != count_reference::missing && gene.nested_host[side] >= data.genes.size()) ||
                gene.competition[side] > 2) fail("gene relationship");
        }
    }
    for (const auto& policy : data.policies) {
        if (policy.gene >= data.genes.size() || !set_ok(policy.category_set) ||
            (policy.novel_origin != count_reference::missing && policy.novel_origin >= data.genes.size())) {
            fail("policy metadata");
        }
    }
    for (const auto& model : data.models) {
        if (!optional_string(model.transcript_string, data) || model.exon_policy >= data.policies.size() ||
            model.body_policy >= data.policies.size() || model.strand > 1) fail("model metadata");
    }
}
void validate_postings(std::span<const Posting> values, const Metadata& data) {
    std::uint32_t previous = 0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        const auto& value = values[index];
        if (value.model == count_reference::missing || value.model >= data.models.size() ||
            value.support_set >= data.sets.size() || value.flags == 0 ||
            (value.flags & ~std::uint32_t{0x0f}) != 0 || (index != 0 && value.model <= previous)) {
            fail("posting id, flags, or order");
        }
        previous = value.model;
    }
}

std::uint64_t strings_size(const std::vector<std::string>& values) {
    std::uint64_t result = 0;
    for (const auto& value : values) {
        if (value.size() > UINT32_MAX) fail("string width");
        result = add(result, add(4, value.size()));
    }
    return result;
}
std::uint64_t sets_size(const std::vector<std::vector<std::uint32_t>>& values) {
    std::uint64_t result = 0;
    for (const auto& value : values) result = add(result, add(8, bytes(value.size(), sizeof(std::uint32_t))));
    return result;
}
void put32(TempFile& output, std::uint32_t value) {
    std::array<std::byte, 4> raw{static_cast<std::byte>(value), static_cast<std::byte>(value >> 8),
                                 static_cast<std::byte>(value >> 16), static_cast<std::byte>(value >> 24)};
    output.append(raw.data(), raw.size());
}
void put64(TempFile& output, std::uint64_t value) {
    std::array<std::byte, 8> raw{};
    for (std::size_t index = 0; index < raw.size(); ++index) {
        raw[index] = static_cast<std::byte>(value >> (8 * index));
    }
    output.append(raw.data(), raw.size());
}
void put_strings(TempFile& output, const std::vector<std::string>& values) {
    for (const auto& value : values) { put32(output, static_cast<std::uint32_t>(value.size())); output.append(value.data(), value.size()); }
}
void put_sets(TempFile& output, const std::vector<std::vector<std::uint32_t>>& values) {
    for (const auto& value : values) {
        put64(output, value.size());
        output.append(value.data(), allocation(value.size(), sizeof(std::uint32_t), "set section"));
    }
}
template<class T> void put_vector(TempFile& output, const std::vector<T>& values, std::string_view message) {
    output.append(values.data(), allocation(values.size(), sizeof(T), message));
}
void pad(TempFile& output, std::uint64_t target) {
    static constexpr std::array<std::byte, 256> zeroes{};
    if (target < output.size) fail("frame offset");
    while (output.size < target) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(zeroes.size(), target - output.size));
        output.append(zeroes.data(), count);
    }
}
void copy_stream(TempFile& output, TempFile& input) {
    input.flush();
    std::array<std::byte, scratch_bytes> scratch{};
    for (std::uint64_t offset = 0; offset < input.size;) {
        const auto count = static_cast<std::size_t>(std::min<std::uint64_t>(scratch.size(), input.size - offset));
        input.read(offset, scratch.data(), count);
        output.append(scratch.data(), count);
        offset = add(offset, count);
    }
}
void publish(TempFile& file, const std::filesystem::path& output) {
    file.sync(); file.close_checked();
    if (::link(file.path.c_str(), output.c_str()) != 0) fail("output exists or publish failed");
    file.unlink_checked();
    const auto parent = output.has_parent_path() ? output.parent_path() : std::filesystem::path{"."};
    const int directory = ::open(parent.c_str(), O_RDONLY | O_DIRECTORY);
    if (directory < 0) fail("output directory");
    const int synced = ::fsync(directory);
    const int closed = ::close(directory);
    if (synced != 0 || closed != 0) fail("output directory sync");
}

template<class T, class Function>
void visit_rows(const TempFile& file, std::uint64_t first, std::uint64_t count, Function&& function) {
    constexpr std::size_t per_chunk = std::max<std::size_t>(1, scratch_bytes / sizeof(T));
    std::array<T, per_chunk> scratch{};
    const auto start = multiply(first, sizeof(T));
    const auto total = multiply(count, sizeof(T));
    if (start > file.size || total > file.size - start) fail("payload range");
    for (std::uint64_t seen = 0; seen < count;) {
        const auto amount = static_cast<std::size_t>(std::min<std::uint64_t>(per_chunk, count - seen));
        file.read(add(start, multiply(seen, sizeof(T))), scratch.data(), amount * sizeof(T));
        for (std::size_t index = 0; index < amount; ++index) function(scratch[index], seen + index);
        seen = add(seen, amount);
    }
}

}  // namespace

struct Writer::Impl {
    std::filesystem::path output;
    Metadata metadata;
    std::uint64_t max_node_id = 0;
    unsigned digest_bits = 64;
    Budget budget;
    bool sealed = false, done = false;
    std::uint64_t nodes = 0, stored_postings = 0, index_bytes = 0;
    std::size_t intern_charge = 0;
    std::vector<std::uint32_t> model_supports;
    std::vector<std::uint8_t> model_support_seen;
    std::vector<NodeSlot> node_slots;
    std::vector<ListInfo> list_info;
    std::unordered_map<std::uint64_t, std::vector<std::uint32_t>> intern;
    std::vector<PairRow> splice_rows, junction_rows;
    std::unordered_set<PairKey, PairHash> splice_seen, junction_seen;
    TempFile posting_file, splice_file, junction_file;
    Mapping sealed_posting_file;

    Impl(std::filesystem::path requested_output, Metadata requested_metadata, std::uint64_t requested_max,
         std::size_t limit, unsigned requested_bits)
        : output(std::move(requested_output)), metadata(std::move(requested_metadata)),
          max_node_id(requested_max), digest_bits(requested_bits), budget(limit) {
        if (digest_bits > 64) fail("digest bits");
        struct stat state {};
        if (::lstat(output.c_str(), &state) == 0) fail("output already exists");
        if (errno != ENOENT) fail("output inspection");
        validate_metadata(metadata);
        budget.charge(sizeof(Impl));
        budget.charge(metadata_memory(metadata));
        budget.charge(allocation(metadata.models.size(), sizeof(std::uint32_t) + sizeof(std::uint8_t),
                                 "model support memory"));
        model_supports.assign(metadata.models.size(), 0);
        model_support_seen.assign(metadata.models.size(), 0);
        const auto maximum_slots = std::numeric_limits<std::size_t>::max() / sizeof(NodeSlot);
        if (max_node_id >= static_cast<std::uint64_t>(maximum_slots)) fail("node id list limit");
        const auto count = static_cast<std::size_t>(max_node_id) + 1;
        budget.charge(allocation(count, sizeof(NodeSlot), "node id list limit"));
        node_slots.assign(count, NodeSlot{});
        budget.charge(sizeof(ListInfo));
        list_info.push_back({0, 0, 0}); // valid shared empty list
        posting_file.create(output, "postings");
        splice_file.create(output, "splice");
        junction_file.create(output, "junction");
    }
    void open() const { if (done) fail("writer finished"); }
    void require_edges() const { open(); if (!sealed) fail("seal nodes before edge facts"); }
    void endpoint(std::uint64_t id) const {
        if (id > max_node_id || node_slots[static_cast<std::size_t>(id)].list == count_reference::missing) {
            fail("edge endpoint node");
        }
    }
    std::uint64_t list_end(std::uint32_t list) const {
        if (list >= list_info.size()) fail("intern list id");
        return list + 1 < list_info.size() ? list_info[list + 1].offset : posting_file.size;
    }
    void accept_postings(std::span<const Posting> values) {
        validate_postings(values, metadata);
        for (const auto& value : values) {
            if ((value.flags & body_flags) == 0) {
                if (value.support_set != 0) fail("non-body posting has support set");
                continue;
            }
            if (!model_support_seen[value.model]) {
                model_supports[value.model] = value.support_set;
                model_support_seen[value.model] = 1;
            } else if (model_supports[value.model] != value.support_set) {
                fail("model has conflicting body support sets");
            }
        }
    }
    bool same_list(std::uint32_t list, std::span<const Posting> values,
                   std::uint64_t encoded_bytes) const {
        if (list >= list_info.size()) fail("intern list id");
        const auto& saved = list_info[list];
        if (saved.count != values.size()) return false;
        const auto end = list_end(list);
        if (saved.offset > end || end > posting_file.size || end - saved.offset != encoded_bytes)
            return false;
        if (saved.count == 0) return true;
        std::array<std::byte, scratch_bytes> expected{}, actual{};
        PostingEncoder encoder(values);
        for (std::uint64_t offset = 0; offset < encoded_bytes;) {
            const auto amount = static_cast<std::size_t>(
                std::min<std::uint64_t>(scratch_bytes, encoded_bytes - offset));
            if (encoder.read(expected.data(), amount) != amount) fail("posting encoder length");
            posting_file.read(add(saved.offset, offset), actual.data(), amount);
            if (std::memcmp(expected.data(), actual.data(), amount) != 0) return false;
            offset = add(offset, amount);
        }
        if (!encoder.done()) fail("posting encoder length");
        return true;
    }
    std::uint32_t list(std::span<const Posting> values) {
        if (values.size() > UINT32_MAX) fail("posting list width");
        if (values.empty()) return 0;
        const auto summary = summarize(values, digest_bits);
        const auto found = intern.find(summary.digest);
        if (found != intern.end()) {
            for (const auto candidate : found->second)
                if (same_list(candidate, values, summary.bytes)) return candidate;
        }
        if (list_info.size() >= count_reference::missing) fail("list id width");
        constexpr std::size_t list_bookkeeping = sizeof(ListInfo);
        constexpr std::size_t interner_bookkeeping = sizeof(std::uint32_t) + sizeof(std::uint64_t) + 64;
        budget.charge(list_bookkeeping + interner_bookkeeping);
        intern_charge += interner_bookkeeping;
        const auto offset = posting_file.size;
        PostingEncoder encoder(values);
        std::array<std::byte, scratch_bytes> scratch{};
        while (!encoder.done()) {
            const auto count = encoder.read(scratch.data(), scratch.size());
            if (count == 0) fail("posting encoder stalled");
            posting_file.append(scratch.data(), count);
        }
        if (posting_file.size - offset != summary.bytes) fail("posting encoder length");
        const auto id = static_cast<std::uint32_t>(list_info.size());
        list_info.push_back({offset, static_cast<std::uint32_t>(values.size()), 0});
        intern[summary.digest].push_back(id);
        stored_postings = add(stored_postings, values.size());
        return id;
    }
    void add_pair(std::vector<PairRow>& rows, std::unordered_set<PairKey, PairHash>& seen,
                  TempFile& payload, std::uint64_t from, std::uint64_t to,
                  const void* values, std::size_t value_bytes, std::size_t value_count,
                  std::size_t element_width) {
        if (rows.size() >= count_reference::missing || value_count > UINT32_MAX) fail("pair width");
        if (element_width == 0 || payload.size % element_width != 0 ||
            value_bytes != allocation(value_count, element_width, "pair payload width")) {
            fail("pair payload width");
        }
        const PairKey key{from, to};
        if (!seen.insert(key).second) fail("duplicate pair");
        budget.charge(sizeof(PairRow) + sizeof(PairKey) + 64);
        const auto first = payload.size / element_width;
        payload.append(values, value_bytes);
        rows.push_back({from, to, first, static_cast<std::uint32_t>(value_count), 0});
    }
    Stats stats() const {
        return {nodes, static_cast<std::uint64_t>(list_info.size()), stored_postings,
                static_cast<std::uint64_t>(metadata.models.size()), static_cast<std::uint64_t>(splice_rows.size()),
                splice_file.size / sizeof(std::uint32_t), static_cast<std::uint64_t>(junction_rows.size()),
                junction_file.size / sizeof(JunctionOwner), index_bytes};
    }
};

Writer::Writer(std::filesystem::path output, Metadata metadata, std::uint64_t max_node_id,
               std::size_t memory_limit_bytes, unsigned testing_digest_bits)
    : impl_(std::make_unique<Impl>(std::move(output), std::move(metadata), max_node_id,
                                   memory_limit_bytes, testing_digest_bits)) {}
Writer::~Writer() = default;

void Writer::append_node(std::uint64_t id, std::uint64_t length, std::span<const Posting> values) {
    auto& state = *impl_;
    state.open();
    if (state.sealed) fail("nodes sealed");
    if (id > state.max_node_id || length > UINT32_MAX || values.size() > UINT32_MAX) fail("node width");
    auto& slot = state.node_slots[static_cast<std::size_t>(id)];
    if (slot.list != count_reference::missing) fail("duplicate node");
    state.accept_postings(values);
    slot = {static_cast<std::uint32_t>(length), state.list(values)};
    ++state.nodes;
}

void Writer::seal_nodes() {
    auto& state = *impl_;
    state.open();
    if (state.sealed) fail("nodes already sealed");
    state.posting_file.sync();
    state.sealed_posting_file.map(state.posting_file.fd, state.posting_file.size);
    state.sealed = true;
    state.intern.clear(); state.intern.rehash(0);
    state.budget.release(state.intern_charge);
    state.intern_charge = 0;
    const auto support_seen_bytes = allocation(state.model_support_seen.size(), sizeof(std::uint8_t),
                                               "model support memory");
    std::vector<std::uint8_t>().swap(state.model_support_seen);
    state.budget.release(support_seen_bytes);
}

std::optional<count_reference::NodeView> Writer::node_view(std::uint64_t id) const {
    const auto& state = *impl_;
    if (!state.sealed || id > state.max_node_id) return std::nullopt;
    const auto& slot = state.node_slots[static_cast<std::size_t>(id)];
    if (slot.list == count_reference::missing) return std::nullopt;
    if (slot.list >= state.list_info.size()) fail("node list id");
    const auto& list = state.list_info[slot.list];
    if (list.count == 0) return count_reference::NodeView{slot.length, {}};
    const auto end = state.list_end(slot.list);
    if (state.sealed_posting_file.data == nullptr || list.offset > end ||
        end > state.sealed_posting_file.size) fail("node posting range");
    return count_reference::NodeView{slot.length, count_reference::PostingView::encoded(
        {state.sealed_posting_file.data + list.offset, static_cast<std::size_t>(end - list.offset)},
        list.count, state.model_supports, state.metadata.sets.size())};
}

void Writer::append_splice(std::uint64_t physical_a, std::uint64_t physical_b,
                           std::span<const std::uint32_t> targets) {
    auto& state = *impl_; state.require_edges();
    if (physical_a == physical_b) fail("self splice");
    if (physical_b < physical_a) std::swap(physical_a, physical_b);
    state.endpoint(physical_a); state.endpoint(physical_b);
    std::uint32_t prior = 0;
    for (std::size_t index = 0; index < targets.size(); ++index) {
        if (targets[index] == count_reference::missing || targets[index] >= state.metadata.strings.size() ||
            (index != 0 && targets[index] <= prior)) fail("splice target id or order");
        prior = targets[index];
    }
    state.add_pair(state.splice_rows, state.splice_seen, state.splice_file, physical_a, physical_b,
                   targets.data(), targets.size_bytes(), targets.size(), sizeof(std::uint32_t));
}

void Writer::append_junction(std::uint64_t oriented_from, std::uint64_t oriented_to,
                             std::span<const JunctionOwner> owners) {
    auto& state = *impl_; state.require_edges();
    state.endpoint(oriented_from >> 1); state.endpoint(oriented_to >> 1);
    std::uint32_t prior = 0;
    for (std::size_t index = 0; index < owners.size(); ++index) {
        const auto& owner = owners[index];
        if (owner.transcript_string == count_reference::missing || owner.transcript_string >= state.metadata.strings.size() ||
            owner.flags == 0 || (owner.flags & ~std::uint32_t{3}) != 0 ||
            (index != 0 && owner.transcript_string <= prior)) fail("junction owner id or order");
        prior = owner.transcript_string;
    }
    state.add_pair(state.junction_rows, state.junction_seen, state.junction_file, oriented_from, oriented_to,
                   owners.data(), owners.size_bytes(), owners.size(), sizeof(JunctionOwner));
}

Stats Writer::stats() const { return impl_->stats(); }

namespace {

template<class State>
void validate_lists(const State& state) {
    std::uint64_t expected = 0, logical = 0;
    for (std::size_t id = 0; id < state.list_info.size(); ++id) {
        const auto& list = state.list_info[id];
        if (list.offset != expected) fail("list offsets");
        const auto end = state.list_end(static_cast<std::uint32_t>(id));
        if (end < list.offset || end > state.posting_file.size) fail("list offsets");
        if (list.count == 0) {
            if (end != list.offset) fail("empty list payload");
        } else {
            if (state.sealed_posting_file.data == nullptr) fail("posting payload mapping");
            const auto view = count_reference::PostingView::encoded(
                {state.sealed_posting_file.data + list.offset, static_cast<std::size_t>(end - list.offset)},
                list.count, state.model_supports, state.metadata.sets.size());
            std::size_t seen = 0;
            for (const auto value : view) {
                if (value.model >= state.metadata.models.size() || value.flags == 0) fail("stored posting value");
                ++seen;
            }
            if (seen != list.count) fail("stored posting count");
        }
        logical = add(logical, list.count);
        expected = end;
    }
    if (expected != state.posting_file.size || logical != state.stored_postings) fail("posting payload");
    for (const auto& slot : state.node_slots) if (slot.list != count_reference::missing && slot.list >= state.list_info.size()) fail("node list id");
}
template<class Value, class Check>
void validate_pairs(std::vector<PairRow>& rows, const TempFile& payload, bool physical, Check&& check) {
    std::sort(rows.begin(), rows.end(), [](const PairRow& left, const PairRow& right) {
        return std::pair(left.from, left.to) < std::pair(right.from, right.to);
    });
    std::pair<std::uint64_t, std::uint64_t> prior{};
    const auto total = payload.size / sizeof(Value);
    if (payload.size % sizeof(Value) != 0) fail("pair payload width");
    for (std::size_t index = 0; index < rows.size(); ++index) {
        auto& row = rows[index];
        const auto key = std::pair(row.from, row.to);
        if ((physical && row.from >= row.to) || (index != 0 && key <= prior) ||
            row.first > total || row.count > total - row.first) fail("pair row order or range");
        check(row, payload);
        row.reserved = 0;
        prior = key;
    }
}
template<class State>
void validate_deep(State& state) {
    validate_metadata(state.metadata); validate_lists(state);
    validate_pairs<std::uint32_t>(state.splice_rows, state.splice_file, true,
        [&](const PairRow& row, const TempFile& file) {
            std::uint32_t prior = 0;
            visit_rows<std::uint32_t>(file, row.first, row.count, [&](std::uint32_t value, std::uint64_t index) {
                if (value == count_reference::missing || value >= state.metadata.strings.size() ||
                    (index != 0 && value <= prior)) fail("splice target id or order");
                prior = value;
            });
        });
    validate_pairs<JunctionOwner>(state.junction_rows, state.junction_file, false,
        [&](const PairRow& row, const TempFile& file) {
            std::uint32_t prior = 0;
            visit_rows<JunctionOwner>(file, row.first, row.count, [&](const JunctionOwner& value, std::uint64_t index) {
                if (value.transcript_string == count_reference::missing || value.transcript_string >= state.metadata.strings.size() ||
                    value.flags == 0 || (value.flags & ~std::uint32_t{3}) != 0 ||
                    (index != 0 && value.transcript_string <= prior)) fail("junction owner id or order");
                prior = value.transcript_string;
            });
        });
}

template<class State>
std::array<Range, section_count> layout(const State& state) {
    const std::array<std::uint64_t, section_count> sizes{
        add(metadata_words * 8, state.metadata.content_id.size()), strings_size(state.metadata.strings),
        strings_size(state.metadata.feature_catalog), sets_size(state.metadata.sets),
        bytes(state.metadata.genes.size(), sizeof(GeneRecord)), bytes(state.metadata.policies.size(), sizeof(PolicyRecord)),
        add(bytes(state.metadata.models.size(), sizeof(ModelRecord)),
            bytes(state.model_supports.size(), sizeof(std::uint32_t))),
        bytes(state.node_slots.size(), sizeof(NodeSlot)),
        add(bytes(state.list_info.size() + 1, sizeof(std::uint64_t)),
            bytes(state.list_info.size(), sizeof(std::uint32_t))), state.posting_file.size,
        add(bytes(state.splice_rows.size(), sizeof(PairRow)), state.splice_file.size),
        add(bytes(state.junction_rows.size(), sizeof(PairRow)), state.junction_file.size), state.metadata.provenance_json.size()};
    std::array<Range, section_count> result{};
    std::uint64_t offset = header_bytes;
    for (std::size_t index = 0; index < section_count; ++index) {
        result[index] = {offset, sizes[index]}; offset = align8(add(offset, sizes[index]));
    }
    return result;
}
template<class State>
void write_index(State& state) {
    const auto ranges = layout(state);
    const auto final_size = add(align8(add(ranges.back().offset, ranges.back().size)), trailer_bytes);
    TempFile output; output.create(state.output, "index");
    output.append(magic.data(), magic.size()); put32(output, version); put32(output, complete);
    put64(output, endian); put64(output, section_count);
    for (const auto& range : ranges) { put64(output, range.offset); put64(output, range.size); }
    pad(output, ranges[meta].offset);
    put64(output, state.max_node_id); put64(output, state.metadata.strings.size());
    put64(output, state.metadata.feature_catalog.size()); put64(output, state.metadata.sets.size());
    put64(output, state.metadata.genes.size()); put64(output, state.metadata.policies.size());
    put64(output, state.metadata.models.size()); put64(output, state.list_info.size());
    put64(output, state.metadata.content_id.size()); put64(output, state.splice_rows.size());
    put64(output, state.junction_rows.size()); put64(output, state.stored_postings);
    put64(output, state.splice_file.size / sizeof(std::uint32_t)); put64(output, state.junction_file.size / sizeof(JunctionOwner));
    output.append(state.metadata.content_id.data(), state.metadata.content_id.size());
    pad(output, ranges[strings].offset); put_strings(output, state.metadata.strings);
    pad(output, ranges[catalog].offset); put_strings(output, state.metadata.feature_catalog);
    pad(output, ranges[sets].offset); put_sets(output, state.metadata.sets);
    pad(output, ranges[genes].offset); put_vector(output, state.metadata.genes, "gene section");
    pad(output, ranges[policies].offset); put_vector(output, state.metadata.policies, "policy section");
    pad(output, ranges[models].offset); put_vector(output, state.metadata.models, "model section");
    put_vector(output, state.model_supports, "model support section");
    pad(output, ranges[slots].offset); put_vector(output, state.node_slots, "slot section");
    pad(output, ranges[lists].offset);
    for (const auto& list : state.list_info) put64(output, list.offset);
    put64(output, state.posting_file.size);
    for (const auto& list : state.list_info) put32(output, list.count);
    pad(output, ranges[postings].offset); copy_stream(output, state.posting_file);
    pad(output, ranges[splices].offset); put_vector(output, state.splice_rows, "splice rows"); copy_stream(output, state.splice_file);
    pad(output, ranges[junctions].offset); put_vector(output, state.junction_rows, "junction rows"); copy_stream(output, state.junction_file);
    pad(output, ranges[provenance].offset); output.append(state.metadata.provenance_json.data(), state.metadata.provenance_json.size());
    pad(output, final_size - trailer_bytes); output.append(trailer.data(), trailer.size()); put64(output, final_size);
    if (output.size != final_size) fail("final frame size");
    publish(output, state.output); state.index_bytes = final_size;
}

}  // namespace

void Writer::finish() {
    auto& state = *impl_; state.open();
    if (!state.sealed) seal_nodes();
    validate_deep(state); write_index(state); state.done = true;
    state.posting_file.discard(); state.splice_file.discard(); state.junction_file.discard();
}

bool is_index(const std::filesystem::path& path) noexcept {
    const int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) return false;
    std::array<std::byte, 32> header{}; std::size_t seen = 0;
    while (seen < header.size()) {
        const auto result = ::read(fd, header.data() + seen, header.size() - seen);
        if (result < 0 && errno == EINTR) continue;
        if (result <= 0) break;
        seen += static_cast<std::size_t>(result);
    }
    static_cast<void>(::close(fd));
    const auto file_version = seen == header.size() ? get32(header.data() + 8) : 0;
    return seen == header.size() && std::memcmp(header.data(), magic.data(), magic.size()) == 0 &&
           file_version == version &&
           get32(header.data() + 12) == complete &&
           get64(header.data() + 16) == endian && get64(header.data() + 24) == section_count;
}

struct Source::Impl {
    int fd = -1; Mapping map;
    std::vector<std::string> strings, catalog;
    std::vector<std::vector<std::uint32_t>> sets;
    std::string content, provenance;
    std::uint64_t max_node = 0;
    const GeneRecord* genes = nullptr; std::size_t gene_count = 0;
    const PolicyRecord* policies = nullptr; std::size_t policy_count = 0;
    const ModelRecord* models = nullptr; std::size_t model_count = 0;
    const std::uint32_t* model_supports = nullptr;
    const NodeSlot* slots = nullptr; std::size_t slot_count = 0;
    const std::uint64_t* list_offsets = nullptr; std::size_t list_count = 0;
    const std::uint32_t* list_counts = nullptr;
    const std::byte* encoded_postings = nullptr; std::size_t encoded_posting_bytes = 0;
    const PairRow* splice_rows = nullptr; std::size_t splice_count = 0;
    const std::uint32_t* splice_values = nullptr; std::size_t splice_value_count = 0;
    const PairRow* junction_rows = nullptr; std::size_t junction_count = 0;
    const JunctionOwner* junction_values = nullptr; std::size_t junction_value_count = 0;
    Stats stats;
    ~Impl() { if (fd >= 0) static_cast<void>(::close(fd)); }
};

namespace {

template<class T> const T* fixed(const std::byte* base, const Range& range, std::uint64_t count, std::string_view what) {
    if (range.offset % alignof(T) != 0 || range.size != multiply(count, sizeof(T))) fail(what);
    static_cast<void>(as_size(count, what));
    return reinterpret_cast<const T*>(base + as_size(range.offset, what));
}
void parse_strings(const std::byte* base, const Range& range, std::uint64_t count,
                   std::vector<std::string>& output, std::string_view what) {
    if (count > range.size / 4 || count > std::numeric_limits<std::size_t>::max()) fail(what);
    output.reserve(static_cast<std::size_t>(count));
    auto* cursor = base + as_size(range.offset, what); auto* end = cursor + as_size(range.size, what);
    for (std::uint64_t index = 0; index < count; ++index) {
        if (end - cursor < 4) fail(what);
        const auto length = get32(cursor); cursor += 4;
        if (length > static_cast<std::uint64_t>(end - cursor)) fail(what);
        output.emplace_back(reinterpret_cast<const char*>(cursor), length); cursor += length;
    }
    if (cursor != end) fail(what);
}
void parse_sets(const std::byte* base, const Range& range, std::uint64_t count,
                std::vector<std::vector<std::uint32_t>>& output) {
    if (count > range.size / 8 || count > std::numeric_limits<std::size_t>::max()) fail("set framing");
    output.reserve(static_cast<std::size_t>(count));
    auto* cursor = base + as_size(range.offset, "set framing"); auto* end = cursor + as_size(range.size, "set framing");
    for (std::uint64_t index = 0; index < count; ++index) {
        if (end - cursor < 8) fail("set framing");
        const auto members = get64(cursor); cursor += 8;
        if (members > static_cast<std::uint64_t>(end - cursor) / sizeof(std::uint32_t) ||
            members > std::numeric_limits<std::size_t>::max()) fail("set framing");
        const auto* values = reinterpret_cast<const std::uint32_t*>(cursor);
        output.emplace_back(values, values + static_cast<std::size_t>(members));
        cursor += members * sizeof(std::uint32_t);
    }
    if (cursor != end) fail("set framing");
}
using FramedRanges = std::array<Range, section_count>;
template<class State>
FramedRanges frame(const State& state) {
    if (state.map.size < header_bytes + trailer_bytes) fail("truncated frame");
    const auto* base = state.map.data;
    const auto file_version = get32(base + 8);
    if (std::memcmp(base, magic.data(), magic.size()) != 0 ||
        file_version != version || get32(base + 12) != complete ||
        get64(base + 16) != endian || get64(base + 24) != section_count) fail("frame header");
    const auto trailer_at = state.map.size - trailer_bytes;
    if (std::memcmp(base + trailer_at, trailer.data(), trailer.size()) != 0 ||
        get64(base + trailer_at + trailer.size()) != state.map.size) fail("frame trailer");
    FramedRanges ranges{};
    std::uint64_t expected = header_bytes;
    for (std::size_t index = 0; index < section_count; ++index) {
        const auto offset = get64(base + 32 + index * 16), size = get64(base + 40 + index * 16);
        if (offset % 8 != 0 || offset != expected || offset > trailer_at || size > trailer_at - offset) fail("section framing");
        ranges[index] = {offset, size}; expected = align8(add(offset, size));
        if (expected > trailer_at) fail("section framing");
    }
    if (expected != trailer_at) fail("section framing");
    return ranges;
}

}  // namespace

Source::Source() : impl_(std::make_unique<Impl>()) {}
Source::~Source() = default;
Source::Source(Source&&) noexcept = default;
Source& Source::operator=(Source&&) noexcept = default;

std::unique_ptr<Source> Source::load(const std::filesystem::path& path) {
    auto source = std::unique_ptr<Source>(new Source); auto& state = *source->impl_;
    state.fd = ::open(path.c_str(), O_RDONLY); if (state.fd < 0) fail("index open");
    struct stat status {}; if (::fstat(state.fd, &status) != 0 || status.st_size < 0) fail("index stat");
    state.map.map(state.fd, static_cast<std::uint64_t>(status.st_size));
    const auto ranges = frame(state);
    const auto* base = state.map.data;
    if (ranges[Section::meta].size < metadata_words * 8) fail("metadata framing");
    const auto* fields = base + as_size(ranges[Section::meta].offset, "metadata framing");
    state.max_node = get64(fields); const auto nstrings = get64(fields + 8), ncatalog = get64(fields + 16), nsets = get64(fields + 24);
    const auto ngenes = get64(fields + 32), npolicies = get64(fields + 40), nmodels = get64(fields + 48), nlists = get64(fields + 56);
    const auto content_size = get64(fields + 64), nsplices = get64(fields + 72), njunctions = get64(fields + 80);
    const auto npostings = get64(fields + 88), ntargets = get64(fields + 96), nowners = get64(fields + 104);
    if (content_size != ranges[Section::meta].size - metadata_words * 8) fail("content framing");
    if (nmodels > count_reference::missing) fail("model count");
    state.content.assign(reinterpret_cast<const char*>(fields + metadata_words * 8), as_size(content_size, "content size"));
    parse_strings(base, ranges[Section::strings], nstrings, state.strings, "string framing");
    parse_strings(base, ranges[Section::catalog], ncatalog, state.catalog, "catalog framing");
    parse_sets(base, ranges[Section::sets], nsets, state.sets);
    state.genes = fixed<GeneRecord>(base, ranges[Section::genes], ngenes, "gene section"); state.gene_count = as_size(ngenes, "gene count");
    state.policies = fixed<PolicyRecord>(base, ranges[Section::policies], npolicies, "policy section"); state.policy_count = as_size(npolicies, "policy count");
    state.model_count = as_size(nmodels, "model count");
    const auto model_bytes = multiply(nmodels, sizeof(ModelRecord));
    const auto support_bytes = multiply(nmodels, sizeof(std::uint32_t));
    if (ranges[Section::models].size != add(model_bytes, support_bytes)) fail("model section");
    const Range records{ranges[Section::models].offset, model_bytes};
    state.models = fixed<ModelRecord>(base, records, nmodels, "model section");
    state.model_supports = reinterpret_cast<const std::uint32_t*>(
        base + as_size(add(ranges[Section::models].offset, model_bytes), "model support section"));
    const auto maximum_slots = std::numeric_limits<std::size_t>::max() / sizeof(NodeSlot);
    if (state.max_node >= static_cast<std::uint64_t>(maximum_slots)) fail("node id list limit");
    const auto slot_count = add(state.max_node, 1);
    state.slots = fixed<NodeSlot>(base, ranges[Section::slots], slot_count, "node slots"); state.slot_count = as_size(slot_count, "slot count");
    if (nlists == 0 || nlists >= count_reference::missing) fail("list count");
    state.list_count = as_size(nlists, "list count");
    static_cast<void>(as_size(npostings, "posting count"));
    const auto offset_bytes = multiply(add(nlists, 1), sizeof(std::uint64_t));
    const auto count_bytes = multiply(nlists, sizeof(std::uint32_t));
    if (ranges[Section::lists].size != add(offset_bytes, count_bytes)) fail("list section");
    const Range offsets{ranges[Section::lists].offset, offset_bytes};
    state.list_offsets = fixed<std::uint64_t>(base, offsets, add(nlists, 1), "list offsets");
    state.list_counts = reinterpret_cast<const std::uint32_t*>(
        base + as_size(add(ranges[Section::lists].offset, offset_bytes), "list counts"));
    state.encoded_postings = base + as_size(ranges[Section::postings].offset, "posting section");
    state.encoded_posting_bytes = as_size(ranges[Section::postings].size, "posting section");
    if (state.list_offsets[0] != 0 || state.list_offsets[state.list_count] != ranges[Section::postings].size ||
        state.list_counts[0] != 0) fail("list endpoints");
    const auto splice_rows_bytes = multiply(nsplices, sizeof(PairRow)), target_bytes = multiply(ntargets, sizeof(std::uint32_t));
    if (ranges[Section::splices].size != add(splice_rows_bytes, target_bytes)) fail("splice section");
    state.splice_rows = reinterpret_cast<const PairRow*>(base + as_size(ranges[Section::splices].offset, "splice section")); state.splice_count = as_size(nsplices, "splice count");
    state.splice_values = reinterpret_cast<const std::uint32_t*>(base + as_size(add(ranges[Section::splices].offset, splice_rows_bytes), "splice section")); state.splice_value_count = as_size(ntargets, "target count");
    const auto junction_rows_bytes = multiply(njunctions, sizeof(PairRow)), owner_bytes = multiply(nowners, sizeof(JunctionOwner));
    if (ranges[Section::junctions].size != add(junction_rows_bytes, owner_bytes)) fail("junction section");
    state.junction_rows = reinterpret_cast<const PairRow*>(base + as_size(ranges[Section::junctions].offset, "junction section")); state.junction_count = as_size(njunctions, "junction count");
    state.junction_values = reinterpret_cast<const JunctionOwner*>(base + as_size(add(ranges[Section::junctions].offset, junction_rows_bytes), "junction section")); state.junction_value_count = as_size(nowners, "owner count");
    state.provenance.assign(reinterpret_cast<const char*>(base + as_size(ranges[Section::provenance].offset, "provenance")), as_size(ranges[Section::provenance].size, "provenance"));
    state.stats = {0, nlists, npostings, nmodels, nsplices, ntargets, njunctions, nowners, state.map.size};
    return source;
}

const std::vector<std::string>& Source::strings() const { return impl_->strings; }
const std::vector<std::vector<std::uint32_t>>& Source::sets() const { return impl_->sets; }
std::span<const GeneRecord> Source::genes() const { return {impl_->genes, impl_->gene_count}; }
std::span<const PolicyRecord> Source::policies() const { return {impl_->policies, impl_->policy_count}; }
const std::string& Source::content_id() const { return impl_->content; }
std::size_t Source::model_count() const { return impl_->model_count; }
ModelRecord Source::model(std::uint32_t id) const { if (id >= impl_->model_count) fail("model id"); return impl_->models[id]; }
count_reference::ScoringClassRecord Source::scoring_class(std::uint32_t) const { fail("unsupported scoring query"); }

std::optional<count_reference::NodeView> Source::node_view(std::uint64_t id) const {
    const auto& state = *impl_;
    if (id > state.max_node) return std::nullopt;
    const auto& slot = state.slots[static_cast<std::size_t>(id)];
    if (slot.list == count_reference::missing) return std::nullopt;
    if (slot.list >= state.list_count) fail("node list id");
    const auto first = state.list_offsets[slot.list], last = state.list_offsets[slot.list + 1];
    if (first > last || last > state.encoded_posting_bytes) fail("node posting range");
    const auto count = state.list_counts[slot.list];
    if (count == 0) {
        if (first != last) fail("empty node posting range");
        return count_reference::NodeView{slot.length, {}, slot.list};
    }
    return count_reference::NodeView{slot.length, count_reference::PostingView::encoded(
        {state.encoded_postings + first, as_size(last - first, "node posting bytes")}, count,
        {state.model_supports, state.model_count}, state.sets.size()), slot.list};
}

std::span<const std::uint32_t> Source::splice_targets(std::uint64_t a, std::uint64_t b) const {
    if (a == b || impl_->splice_count == 0) return {};
    if (b < a) std::swap(a, b);
    const auto* begin = impl_->splice_rows; const auto* end = begin + impl_->splice_count;
    const auto found = std::lower_bound(begin, end, std::pair(a, b), [](const PairRow& row, const auto& key) { return std::pair(row.from, row.to) < key; });
    if (found == end || found->from != a || found->to != b) return {};
    if (found->first > impl_->splice_value_count || found->count > impl_->splice_value_count - found->first) fail("splice range");
    const auto result = std::span<const std::uint32_t>(impl_->splice_values + found->first, found->count);
    std::uint32_t prior = 0;
    for (std::size_t index = 0; index < result.size(); ++index) {
        if (result[index] == count_reference::missing || result[index] >= impl_->strings.size() || (index != 0 && result[index] <= prior)) fail("splice id or order");
        prior = result[index];
    }
    return result;
}
std::span<const JunctionOwner> Source::junction_owners(std::uint64_t from, std::uint64_t to) const {
    if (impl_->junction_count == 0) return {};
    const auto* begin = impl_->junction_rows; const auto* end = begin + impl_->junction_count;
    const auto found = std::lower_bound(begin, end, std::pair(from, to), [](const PairRow& row, const auto& key) { return std::pair(row.from, row.to) < key; });
    if (found == end || found->from != from || found->to != to) return {};
    if (found->first > impl_->junction_value_count || found->count > impl_->junction_value_count - found->first) fail("junction range");
    const auto result = std::span<const JunctionOwner>(impl_->junction_values + found->first, found->count);
    std::uint32_t prior = 0;
    for (std::size_t index = 0; index < result.size(); ++index) {
        const auto& value = result[index];
        if (value.transcript_string == count_reference::missing || value.transcript_string >= impl_->strings.size() || value.flags == 0 ||
            (value.flags & ~std::uint32_t{3}) != 0 || (index != 0 && value.transcript_string <= prior)) fail("junction id or order");
        prior = value.transcript_string;
    }
    return result;
}
Stats Source::stats() const { return impl_->stats; }
std::vector<std::string> Source::take_feature_catalog() { return std::move(impl_->catalog); }
const std::string& Source::provenance_json() const { return impl_->provenance; }
const count_reference::NodeRecord* Source::node(std::uint64_t) const { fail("unsupported legacy node query"); }
std::span<const Posting> Source::node_postings(const count_reference::NodeRecord&) const { fail("unsupported legacy node query"); }
std::span<const count_reference::ScoringPosting> Source::node_scoring(const count_reference::NodeRecord&) const { fail("unsupported scoring query"); }
std::span<const count_reference::ScoringBinding> Source::scoring_bindings(const count_reference::ScoringClassRecord&) const { fail("unsupported scoring query"); }
std::span<const count_reference::EdgePosting> Source::edge(std::uint64_t, std::uint64_t) const { fail("unsupported legacy edge query"); }

}  // namespace pancollapse::direct_index
