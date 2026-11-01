#include "count_reference.hpp"
#include "pod_row_codec.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <system_error>

#include <zstd.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

namespace pancollapse::count_reference {
namespace {
static_assert(std::endian::native == std::endian::little,
              "PCANCOUNT1 mmap records require a little-endian host");
static_assert(sizeof(GeneRecord) == 36 && sizeof(PolicyRecord) == 16 &&
              sizeof(ModelRecord) == 16 && sizeof(NodeRecord) == 48 &&
              sizeof(Posting) == 12 && sizeof(EdgeRecord) == 32 &&
              sizeof(EdgePosting) == 8 && sizeof(ScoringPosting) == 12 &&
              sizeof(ScoringClassRecord) == 16 && sizeof(ScoringBinding) == 16 &&
              sizeof(SpliceRecord) == 32 && sizeof(JunctionOwner) == 8 &&
              sizeof(EncodedSpan) == 16 && sizeof(EncodedBlock) == 56 &&
              sizeof(EdgeKey) == 24 && sizeof(EdgeBlock) == 32);

constexpr std::array<char, 16> magic = {'P','C','A','N','C','O','U','N','T','2'};
constexpr std::array<char, 8> trailer = {'P','C','A','N','E','N','D','2'};
enum Section : std::size_t { metadata, strings, sets, genes, policies, models, nodes,
                             postings, scoring_postings, scoring_classes, scoring_bindings,
                             edges, edge_postings, splice_records, splice_targets,
                             junction_records, junction_owners, node_blocks, scoring_blocks,
                             edge_blocks, encoded, edge_keys, section_count };
constexpr std::size_t header_bytes = magic.size();
constexpr std::size_t directory_bytes = section_count * 16;

struct SectionInfo { std::uint64_t offset = 0, size = 0; };
constexpr std::uint32_t raw_payload = std::uint32_t{1} << 31;
std::atomic<std::uint64_t> next_cache_id{1};

[[noreturn]] void invalid(const char* message) { throw std::runtime_error(std::string("invalid count reference: ") + message); }
void checked_add(std::uint64_t& total, std::uint64_t value) {
    if (value > std::numeric_limits<std::uint64_t>::max() - total) invalid("section size overflow");
    total += value;
}
std::uint64_t align8(std::uint64_t value) {
    if (value > std::numeric_limits<std::uint64_t>::max() - 7) invalid("file offset overflow");
    return (value + 7) & ~std::uint64_t{7};
}
void put_u32(std::ostream& out, std::uint32_t value) {
    const std::array<char, 4> bytes{static_cast<char>(value), static_cast<char>(value >> 8),
                                    static_cast<char>(value >> 16), static_cast<char>(value >> 24)};
    out.write(bytes.data(), bytes.size());
}
void put_u64(std::ostream& out, std::uint64_t value) {
    std::array<char, 8> bytes{};
    for (std::size_t i = 0; i < bytes.size(); ++i) bytes[i] = static_cast<char>(value >> (8 * i));
    out.write(bytes.data(), bytes.size());
}
std::uint32_t get_u32(const std::byte*& cursor, const std::byte* end) {
    if (end - cursor < 4) invalid("truncated variable section");
    const auto* bytes = reinterpret_cast<const unsigned char*>(cursor);
    cursor += 4;
    return std::uint32_t(bytes[0]) | (std::uint32_t(bytes[1]) << 8) |
           (std::uint32_t(bytes[2]) << 16) | (std::uint32_t(bytes[3]) << 24);
}
std::uint64_t get_u64(const std::byte* bytes) {
    const auto* value = reinterpret_cast<const unsigned char*>(bytes);
    std::uint64_t result = 0;
    for (std::size_t i = 0; i < 8; ++i) result |= std::uint64_t(value[i]) << (8 * i);
    return result;
}
void pad_to(std::ostream& out, std::uint64_t offset, std::uint64_t target) {
    static constexpr std::array<char, 8> zeros{};
    while (offset < target) {
        const auto chunk = static_cast<std::streamsize>(std::min<std::uint64_t>(zeros.size(), target - offset));
        out.write(zeros.data(), chunk); offset += static_cast<std::uint64_t>(chunk);
    }
}
template<class T> std::uint64_t fixed_size(const std::vector<T>& values) {
    if (values.size() > std::numeric_limits<std::uint64_t>::max() / sizeof(T)) invalid("section size overflow");
    return values.size() * sizeof(T);
}
bool valid_optional_string(std::uint32_t value, const Data& data) {
    return value == missing || value < data.strings.size();
}
void validate_encoded_slabs(const EncodedBuffer& buffer) {
    std::uint64_t used = 0;
    for (const auto& slab : buffer.slabs()) {
        if (slab.size() > buffer.slab_capacity() || slab.size() > slab.capacity()) {
            invalid("encoded slab capacity");
        }
        checked_add(used, slab.size());
    }
    if (used != buffer.size()) invalid("encoded slab size");
}
std::uint32_t encoded_bytes(std::uint32_t bytes) { return bytes & ~raw_payload; }
bool encoded_is_raw(std::uint32_t bytes) { return (bytes & raw_payload) != 0; }
void validate_span(const EncodedSpan& span, const Data& data, std::size_t row_bytes) {
    const auto bytes = encoded_bytes(span.bytes);
    const auto total = encoded_size(data);
    if (span.first > total || bytes > total - span.first ||
        (encoded_is_raw(span.bytes) &&
         (span.count > std::numeric_limits<std::size_t>::max() / row_bytes ||
          bytes != span.count * row_bytes))) invalid("encoded span bounds");
}
template<class T> std::vector<T> decode_rows(const std::byte* encoded, std::size_t encoded_size,
                                              const EncodedSpan& span) {
    const auto bytes = encoded_bytes(span.bytes);
    if (span.first > encoded_size || bytes > encoded_size - span.first ||
        span.count > std::numeric_limits<std::size_t>::max() / sizeof(T)) {
        invalid("encoded span bounds");
    }
    const std::size_t output_bytes = std::size_t(span.count) * sizeof(T);
    const bool raw = encoded_is_raw(span.bytes);
    if (raw && bytes != output_bytes) invalid("raw encoded span size");
    if (!raw && (output_bytes == 0 || bytes == 0)) invalid("compressed encoded span size");
    const auto* input = encoded + span.first;
    try {
        return pod_row_codec::decode<T>({input, bytes}, raw
            ? pod_row_codec::Encoding::raw : pod_row_codec::Encoding::zstd, span.count);
    } catch (const std::runtime_error&) { invalid("compressed encoded span payload"); }
}
template<class T, class Key> struct DecodedRowsCacheEntry {
    std::uint64_t cache_id = 0, age = 0;
    Key key{};
    std::vector<T> rows;
};
template<class T, class Key> struct DecodedRowsCache {
    inline static thread_local std::vector<DecodedRowsCacheEntry<T, Key>> entries;
    inline static thread_local std::size_t bytes = 0;
    inline static thread_local std::uint64_t clock = 0;
};
template<class T, class Key> std::vector<T>* cached_rows(std::uint64_t cache_id, const Key& key) {
    for (auto& entry : DecodedRowsCache<T, Key>::entries) if (entry.cache_id == cache_id && entry.key == key) {
        entry.age = ++DecodedRowsCache<T, Key>::clock;
        return &entry.rows;
    }
    return nullptr;
}
template<class T, class Key> const std::vector<T>& cache_rows(std::uint64_t cache_id, const Key& key,
                                                               std::vector<T>&& rows) {
    auto& cache = DecodedRowsCache<T, Key>::entries;
    auto& cached_bytes = DecodedRowsCache<T, Key>::bytes;
    auto& clock = DecodedRowsCache<T, Key>::clock;
    constexpr std::size_t cache_limit = 2 * 1024 * 1024;
    const auto bytes = rows.size() * sizeof(T);
    if (bytes > cache_limit) {
        static thread_local std::vector<T> uncached;
        uncached = std::move(rows);
        return uncached;
    }
    while ((!cache.empty() && (cached_bytes + bytes > cache_limit || cache.size() >= 128))) {
        const auto evict = std::min_element(cache.begin(), cache.end(),
                                            [](const auto& a, const auto& b) { return a.age < b.age; });
        cached_bytes -= evict->rows.size() * sizeof(T);
        cache.erase(evict);
    }
    cache.push_back({cache_id, ++clock, key, std::move(rows)});
    cached_bytes += cache.back().rows.size() * sizeof(T);
    return cache.back().rows;
}
bool block_contains(const EncodedBlock& block, std::uint64_t rank) {
    if (block.block != rank / 256) return false;
    const auto bit = static_cast<std::uint32_t>(rank % 256);
    return (block.mask[bit / 64] & (std::uint64_t{1} << (bit % 64))) != 0;
}
void validate(const Data& data) {
    validate_encoded_slabs(data.encoded_slabs);
    if (encoded_size(data) < data.encoded.size()) invalid("encoded size overflow");
    if (data.content_id.size() > std::numeric_limits<std::uint32_t>::max()) invalid("content id size");
    for (const auto& value : data.strings) {
        if (value.size() > std::numeric_limits<std::uint32_t>::max()) invalid("string size");
    }
    const auto set_ok = [&](std::uint32_t value) { return value < data.sets.size(); };
    for (const auto& gene : data.genes) {
        if (!valid_optional_string(gene.id_string, data) || !valid_optional_string(gene.name_string, data) || !set_ok(gene.type_set)) invalid("gene string/set id");
        for (std::size_t i = 0; i < 2; ++i) if ((gene.equivalence[i] != missing && gene.equivalence[i] >= data.genes.size()) || (gene.nested_host[i] != missing && gene.nested_host[i] >= data.genes.size()) || gene.competition[i] > 2) invalid("gene field");
    }
    for (const auto& policy : data.policies) if (policy.gene >= data.genes.size() || !set_ok(policy.category_set) || (policy.novel_origin != missing && policy.novel_origin >= data.genes.size())) invalid("policy field");
    for (const auto& model : data.models) if (!valid_optional_string(model.transcript_string, data) || model.exon_policy >= data.policies.size() || model.body_policy >= data.policies.size()) invalid("model field");
    std::uint64_t previous = 0;
    for (std::size_t i = 0; i < data.nodes.size(); ++i) {
        const auto& node = data.nodes[i];
        if ((i != 0 && node.id <= previous) || node.first > data.postings.size() ||
            node.count > data.postings.size() - node.first ||
            node.score_first > data.scoring_postings.size() ||
            node.score_count > data.scoring_postings.size() - node.score_first) {
            invalid("node order or posting range");
        }
        previous = node.id;
    }
    for (const auto& posting : data.postings) if (posting.model >= data.models.size() || !set_ok(posting.support_set)) invalid("posting field");
    for (const auto& posting : data.scoring_postings) {
        if (posting.class_id >= data.scoring_classes.size()) invalid("scoring posting class");
    }
    for (const auto& record : data.scoring_classes) {
        if ((record.flags & ~std::uint32_t{1}) != 0 ||
            record.first > data.scoring_bindings.size() ||
            record.count > data.scoring_bindings.size() - record.first ||
            (record.count == 0 && (record.flags & 1) == 0)) {
            invalid("scoring class field");
        }
        std::pair<std::uint32_t, std::uint32_t> previous_binding{};
        for (std::uint32_t i = 0; i < record.count; ++i) {
            const auto& binding = data.scoring_bindings[record.first + i];
            const auto key = std::pair(binding.transcript_string, binding.policy);
            if (!valid_optional_string(binding.transcript_string, data) ||
                binding.transcript_string == missing || binding.policy >= data.policies.size() ||
                binding.multiplicity == 0 || (i != 0 && key <= previous_binding)) {
                invalid("scoring binding field");
            }
            previous_binding = key;
        }
    }
    std::pair<std::uint64_t, std::uint64_t> previous_edge{};
    for (std::size_t i = 0; i < data.edges.size(); ++i) {
        const auto& edge = data.edges[i]; const auto key = std::pair(edge.from, edge.to);
        if ((i != 0 && key <= previous_edge) || edge.first > data.edge_postings.size() || edge.count > data.edge_postings.size() - edge.first) invalid("edge order or posting range");
        previous_edge = key;
    }
    for (const auto& posting : data.edge_postings) if (posting.model >= data.models.size()) invalid("edge posting field");
    std::pair<std::uint64_t, std::uint64_t> previous_splice{};
    for (std::size_t i = 0; i < data.splice_records.size(); ++i) {
        const auto& record = data.splice_records[i];
        const auto key = std::pair(record.from, record.to);
        if (record.from >= record.to || (i != 0 && key <= previous_splice) ||
            record.first > data.splice_targets.size() ||
            record.count > data.splice_targets.size() - record.first) {
            invalid("splice record field");
        }
        std::uint32_t previous_target = 0;
        for (std::uint32_t j = 0; j < record.count; ++j) {
            const auto target = data.splice_targets[record.first + j];
            if (!valid_optional_string(target, data) || target == missing ||
                (j != 0 && target <= previous_target)) invalid("splice target field");
            previous_target = target;
        }
        previous_splice = key;
    }
    std::pair<std::uint64_t, std::uint64_t> previous_junction{};
    for (std::size_t i = 0; i < data.junction_records.size(); ++i) {
        const auto& record = data.junction_records[i];
        const auto key = std::pair(record.from, record.to);
        if ((i != 0 && key <= previous_junction) ||
            record.first > data.junction_owners.size() ||
            record.count > data.junction_owners.size() - record.first) {
            invalid("junction record field");
        }
        std::uint32_t previous_owner = 0;
        for (std::uint32_t j = 0; j < record.count; ++j) {
            const auto& owner = data.junction_owners[record.first + j];
            if (!valid_optional_string(owner.transcript_string, data) ||
                owner.transcript_string == missing || owner.flags == 0 ||
                (owner.flags & ~std::uint32_t{3}) != 0 ||
                (j != 0 && owner.transcript_string <= previous_owner)) {
                invalid("junction owner field");
            }
            previous_owner = owner.transcript_string;
        }
        previous_junction = key;
    }
    const auto validate_blocks = [&](const std::vector<EncodedBlock>& blocks, std::size_t row_bytes) {
        std::uint64_t previous_block = 0;
        for (std::size_t i = 0; i < blocks.size(); ++i) {
            const auto& block = blocks[i];
            if ((i != 0 && block.block < previous_block) ||
                (block.mask[0] | block.mask[1] | block.mask[2] | block.mask[3]) == 0) {
                invalid("encoded block order or mask");
            }
            validate_span(block.values, data, row_bytes);
            previous_block = block.block;
        }
    };
    validate_blocks(data.node_blocks, sizeof(Posting));
    validate_blocks(data.scoring_blocks, sizeof(ScoringPosting));
    std::pair<std::uint64_t, std::uint64_t> previous_edge_key{};
    for (std::size_t i = 0; i < data.edge_keys.size(); ++i) {
        const auto& edge = data.edge_keys[i];
        const auto key = std::pair(edge.from, edge.to);
        if (i != 0 && key <= previous_edge_key) invalid("edge key order");
        previous_edge_key = key;
    }
    std::pair<std::uint64_t, std::uint64_t> previous_encoded_block{};
    for (std::size_t i = 0; i < data.edge_blocks.size(); ++i) {
        const auto& block = data.edge_blocks[i];
        const auto key = std::pair(block.block, block.values.first);
        if ((i != 0 && key < previous_encoded_block) || block.mask == 0) {
            invalid("encoded edge block order or mask");
        }
        validate_span(block.values, data, sizeof(EdgePosting));
        previous_encoded_block = key;
    }
}
template<class T> const T* fixed_section(const std::byte* base, const SectionInfo& section,
                                          std::size_t file_size, std::size_t& count) {
    if (section.offset % alignof(T) != 0 || section.size % sizeof(T) != 0 ||
        section.offset > file_size || section.size > file_size - section.offset) invalid("fixed section bounds");
    count = static_cast<std::size_t>(section.size / sizeof(T));
    return reinterpret_cast<const T*>(base + section.offset);
}
}  // namespace

EncodedBuffer::EncodedBuffer(std::size_t slab_capacity)
    : slab_capacity_(slab_capacity) {
    if (slab_capacity_ == 0) throw std::invalid_argument("encoded slab capacity must be positive");
}

EncodedBuffer::EncodedBuffer(EncodedBuffer&& other) noexcept
    : slab_capacity_(other.slab_capacity_), size_(other.size_), slabs_(std::move(other.slabs_)) {
    other.size_ = 0;
    other.slabs_.clear();
}

EncodedBuffer& EncodedBuffer::operator=(EncodedBuffer&& other) noexcept {
    if (this != &other) {
        slab_capacity_ = other.slab_capacity_;
        size_ = other.size_;
        slabs_ = std::move(other.slabs_);
        other.size_ = 0;
        other.slabs_.clear();
    }
    return *this;
}

void EncodedBuffer::append(std::span<const std::byte> bytes) {
    while (!bytes.empty()) {
        if (slabs_.empty() || slabs_.back().size() == slab_capacity_) {
            slabs_.emplace_back();
            slabs_.back().reserve(slab_capacity_);
        }
        auto& slab = slabs_.back();
        // A copied buffer preserves used bytes but vector's copy constructor
        // need not preserve reserved capacity. Restore the fixed-capacity
        // invariant before appending to its final partial slab.
        if (slab.capacity() < slab_capacity_) slab.reserve(slab_capacity_);
        const auto amount = std::min(bytes.size(), slab_capacity_ - slab.size());
        if (amount > std::numeric_limits<std::uint64_t>::max() - size_) {
            throw std::overflow_error("encoded slab size overflow");
        }
        slab.insert(slab.end(), bytes.begin(), bytes.begin() + amount);
        size_ += amount;
        bytes = bytes.subspan(amount);
    }
}

std::uint64_t EncodedBuffer::allocated_bytes() const {
    std::uint64_t result = 0;
    for (const auto& slab : slabs_) {
        if (slab.capacity() > std::numeric_limits<std::uint64_t>::max() - result) {
            throw std::overflow_error("encoded slab allocation overflow");
        }
        result += slab.capacity();
    }
    return result;
}

bool is_count_reference(const std::filesystem::path& path) noexcept {
    std::ifstream input(path, std::ios::binary);
    std::array<char, header_bytes> header{};
    return input.read(header.data(), header.size()).gcount() == static_cast<std::streamsize>(header.size()) && header == magic;
}

void write(const std::filesystem::path& path, const Data& data) {
    validate(data);
    std::array<SectionInfo, section_count> sections{};
    std::array<std::uint64_t, section_count> sizes{};
    sizes[metadata] = 4 + data.content_id.size();
    for (const auto& value : data.strings) checked_add(sizes[strings], 4 + value.size());
    for (const auto& value : data.sets) {
        if (value.size() > std::numeric_limits<std::uint64_t>::max() / 4) invalid("set size overflow");
        checked_add(sizes[sets], 4 + 4 * value.size());
    }
    sizes[genes] = fixed_size(data.genes); sizes[policies] = fixed_size(data.policies);
    sizes[models] = fixed_size(data.models); sizes[nodes] = fixed_size(data.nodes);
    sizes[postings] = fixed_size(data.postings); sizes[scoring_postings] = fixed_size(data.scoring_postings);
    sizes[scoring_classes] = fixed_size(data.scoring_classes); sizes[scoring_bindings] = fixed_size(data.scoring_bindings);
    sizes[edges] = fixed_size(data.edges); sizes[edge_postings] = fixed_size(data.edge_postings);
    sizes[splice_records] = fixed_size(data.splice_records); sizes[splice_targets] = fixed_size(data.splice_targets);
    sizes[junction_records] = fixed_size(data.junction_records);
    sizes[junction_owners] = fixed_size(data.junction_owners);
    sizes[node_blocks] = fixed_size(data.node_blocks);
    sizes[scoring_blocks] = fixed_size(data.scoring_blocks);
    sizes[edge_blocks] = fixed_size(data.edge_blocks);
    sizes[encoded] = encoded_size(data);
    sizes[edge_keys] = fixed_size(data.edge_keys);
    std::uint64_t offset = header_bytes + directory_bytes;
    for (std::size_t i = 0; i < section_count; ++i) {
        offset = align8(offset); sections[i] = {offset, sizes[i]}; checked_add(offset, sizes[i]);
    }
    const auto temporary = path.string() + ".tmp." + std::to_string(getpid());
    std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("cannot create count reference temporary file");
    try {
        out.write(magic.data(), magic.size());
        for (const auto& section : sections) { put_u64(out, section.offset); put_u64(out, section.size); }
        std::uint64_t written = header_bytes + directory_bytes;
        const auto begin = [&](Section section) { pad_to(out, written, sections[section].offset); written = sections[section].offset; };
        begin(metadata); put_u32(out, data.content_id.size()); out.write(data.content_id.data(), data.content_id.size()); written += sizes[metadata];
        begin(strings); for (const auto& value : data.strings) { put_u32(out, value.size()); out.write(value.data(), value.size()); } written += sizes[strings];
        begin(sets); for (const auto& value : data.sets) { put_u32(out, value.size()); for (auto member : value) put_u32(out, member); } written += sizes[sets];
        const auto fixed = [&](Section section,const auto& values) {
            begin(section);
            // These explicitly sized records have no implicit padding and the
            // format is little endian, as required by the mmap reader above.
            out.write(reinterpret_cast<const char*>(values.data()),sizes[section]);
            written += sizes[section];
        };
        fixed(genes,data.genes);fixed(policies,data.policies);fixed(models,data.models);
        fixed(nodes,data.nodes);fixed(postings,data.postings);fixed(scoring_postings,data.scoring_postings);
        fixed(scoring_classes,data.scoring_classes);fixed(scoring_bindings,data.scoring_bindings);
        fixed(edges,data.edges);fixed(edge_postings,data.edge_postings);
        fixed(splice_records,data.splice_records);fixed(splice_targets,data.splice_targets);
        fixed(junction_records,data.junction_records);fixed(junction_owners,data.junction_owners);
        fixed(node_blocks,data.node_blocks);fixed(scoring_blocks,data.scoring_blocks);
        fixed(edge_blocks,data.edge_blocks);
        begin(encoded);
        if (!data.encoded.empty()) {
            out.write(reinterpret_cast<const char*>(data.encoded.data()), data.encoded.size());
        }
        for (const auto& slab : data.encoded_slabs.slabs()) {
            if (!slab.empty()) {
                out.write(reinterpret_cast<const char*>(slab.data()), slab.size());
            }
        }
        written += sizes[encoded];
        fixed(edge_keys,data.edge_keys);
        out.write(trailer.data(),trailer.size());
        if (!out) throw std::runtime_error("failed writing count reference");
        out.close();
        if(!out) throw std::runtime_error("failed closing count reference");
        std::filesystem::rename(temporary, path);
    } catch (...) { out.close(); std::error_code ignored; std::filesystem::remove(temporary, ignored); throw; }
}

Reference::~Reference() { close(); }
Reference::Reference(Reference&& other) noexcept { *this = std::move(other); }
Reference& Reference::operator=(Reference&& other) noexcept {
    if (this != &other) {
        close();
        fd_ = other.fd_; cache_id_ = other.cache_id_; mapped_ = other.mapped_; mapped_size_ = other.mapped_size_;
        genes_ = other.genes_; gene_count_ = other.gene_count_;
        policies_ = other.policies_; policy_count_ = other.policy_count_;
        models_ = other.models_; model_count_ = other.model_count_;
        nodes_ = other.nodes_; node_count_ = other.node_count_;
        postings_ = other.postings_; posting_count_ = other.posting_count_;
        scoring_postings_ = other.scoring_postings_; scoring_posting_count_ = other.scoring_posting_count_;
        scoring_classes_ = other.scoring_classes_; scoring_class_count_ = other.scoring_class_count_;
        scoring_bindings_ = other.scoring_bindings_; scoring_binding_count_ = other.scoring_binding_count_;
        edges_ = other.edges_; edge_count_ = other.edge_count_;
        edge_postings_ = other.edge_postings_; edge_posting_count_ = other.edge_posting_count_;
        splice_records_ = other.splice_records_; splice_record_count_ = other.splice_record_count_;
        splice_targets_ = other.splice_targets_; splice_target_count_ = other.splice_target_count_;
        junction_records_ = other.junction_records_; junction_record_count_ = other.junction_record_count_;
        junction_owners_ = other.junction_owners_; junction_owner_count_ = other.junction_owner_count_;
        node_blocks_ = other.node_blocks_; node_block_count_ = other.node_block_count_;
        scoring_blocks_ = other.scoring_blocks_; scoring_block_count_ = other.scoring_block_count_;
        edge_keys_ = other.edge_keys_; edge_key_count_ = other.edge_key_count_;
        edge_blocks_ = other.edge_blocks_; edge_block_count_ = other.edge_block_count_;
        encoded_ = other.encoded_; encoded_size_ = other.encoded_size_;
        strings_ = std::move(other.strings_); sets_ = std::move(other.sets_);
        content_id_ = std::move(other.content_id_);
        other.fd_ = -1; other.cache_id_ = 0; other.mapped_ = nullptr; other.mapped_size_ = 0;
        other.genes_ = nullptr; other.policies_ = nullptr; other.models_ = nullptr;
        other.nodes_ = nullptr; other.postings_ = nullptr; other.scoring_postings_ = nullptr;
        other.scoring_classes_ = nullptr; other.scoring_bindings_ = nullptr; other.edges_ = nullptr;
        other.edge_postings_ = nullptr; other.splice_records_ = nullptr; other.splice_targets_ = nullptr;
        other.junction_records_ = nullptr; other.junction_owners_ = nullptr;
        other.node_blocks_ = nullptr; other.scoring_blocks_ = nullptr; other.edge_keys_ = nullptr; other.edge_blocks_ = nullptr;
        other.encoded_ = nullptr;
        other.gene_count_ = other.policy_count_ = other.model_count_ = other.node_count_ = 0;
        other.posting_count_ = other.scoring_posting_count_ = other.scoring_class_count_ = 0;
        other.scoring_binding_count_ = other.edge_count_ = other.edge_posting_count_ = 0;
        other.splice_record_count_ = other.splice_target_count_ = 0;
        other.junction_record_count_ = other.junction_owner_count_ = 0;
        other.node_block_count_ = other.scoring_block_count_ = other.edge_key_count_ = other.edge_block_count_ = 0;
        other.encoded_size_ = 0;
    }
    return *this;
}
void Reference::close() noexcept { if (mapped_) munmap(const_cast<std::byte*>(mapped_), mapped_size_); if (fd_ >= 0) ::close(fd_); mapped_ = nullptr; mapped_size_ = 0; fd_ = -1; }

Reference Reference::load(const std::filesystem::path& path) {
    Reference result; result.fd_ = ::open(path.c_str(), O_RDONLY);
    result.cache_id_ = next_cache_id.fetch_add(1, std::memory_order_relaxed);
    if (result.fd_ < 0) throw std::system_error(errno, std::generic_category(), "cannot open count reference");
    struct stat status {}; if (fstat(result.fd_, &status) != 0 || status.st_size < static_cast<off_t>(header_bytes + directory_bytes)) invalid("truncated header");
    result.mapped_size_ = static_cast<std::size_t>(status.st_size);
    result.mapped_ = static_cast<const std::byte*>(mmap(nullptr, result.mapped_size_, PROT_READ, MAP_PRIVATE, result.fd_, 0));
    if (result.mapped_ == MAP_FAILED) { result.mapped_ = nullptr; throw std::system_error(errno, std::generic_category(), "cannot mmap count reference"); }
    if (std::memcmp(result.mapped_, magic.data(), magic.size()) != 0) invalid("header magic");
    std::array<SectionInfo, section_count> sections{};
    std::uint64_t expected_offset = header_bytes + directory_bytes;
    for (std::size_t i = 0; i < section_count; ++i) {
        const auto* entry = result.mapped_ + header_bytes + i * 16;
        sections[i] = {get_u64(entry), get_u64(entry + 8)};
        if (sections[i].offset < header_bytes + directory_bytes || sections[i].offset > result.mapped_size_ || sections[i].size > result.mapped_size_ - sections[i].offset) invalid("section bounds");
        if(sections[i].offset!=align8(expected_offset))invalid("section order");
        expected_offset=sections[i].offset+sections[i].size;
    }
    if(expected_offset>result.mapped_size_||result.mapped_size_-expected_offset!=trailer.size()||
       std::memcmp(result.mapped_+expected_offset,trailer.data(),trailer.size())!=0)invalid("incomplete trailer");
    auto variable = [&](Section section, auto consume) {
        const std::byte* cursor = result.mapped_ + sections[section].offset;
        const std::byte* end = cursor + sections[section].size;
        consume(cursor, end); if (cursor != end) invalid("variable section trailing bytes");
    };
    variable(metadata, [&](const std::byte*& cursor, const std::byte* end) { const auto length = get_u32(cursor,end); if (length > static_cast<std::uint64_t>(end-cursor)) invalid("content id bounds"); result.content_id_.assign(reinterpret_cast<const char*>(cursor), length); cursor += length; });
    variable(Section::strings, [&](const std::byte*& cursor, const std::byte* end) { while (cursor != end) { const auto length=get_u32(cursor,end); if(length>static_cast<std::uint64_t>(end-cursor)) invalid("string bounds"); result.strings_.emplace_back(reinterpret_cast<const char*>(cursor),length); cursor+=length; } });
    variable(Section::sets, [&](const std::byte*& cursor, const std::byte* end) { while (cursor != end) { const auto count=get_u32(cursor,end); if(count>static_cast<std::uint64_t>(end-cursor)/4) invalid("set bounds"); auto& values=result.sets_.emplace_back(); values.reserve(count); for(std::uint32_t i=0;i<count;++i) values.push_back(get_u32(cursor,end)); } });
    result.genes_ = fixed_section<GeneRecord>(result.mapped_, sections[Section::genes], result.mapped_size_, result.gene_count_);
    result.policies_ = fixed_section<PolicyRecord>(result.mapped_, sections[Section::policies], result.mapped_size_, result.policy_count_);
    result.models_ = fixed_section<ModelRecord>(result.mapped_, sections[Section::models], result.mapped_size_, result.model_count_);
    result.nodes_ = fixed_section<NodeRecord>(result.mapped_, sections[nodes], result.mapped_size_, result.node_count_);
    result.postings_ = fixed_section<Posting>(result.mapped_, sections[postings], result.mapped_size_, result.posting_count_);
    result.scoring_postings_ = fixed_section<ScoringPosting>(result.mapped_, sections[scoring_postings], result.mapped_size_, result.scoring_posting_count_);
    result.scoring_classes_ = fixed_section<ScoringClassRecord>(result.mapped_, sections[Section::scoring_classes], result.mapped_size_, result.scoring_class_count_);
    result.scoring_bindings_ = fixed_section<ScoringBinding>(result.mapped_, sections[Section::scoring_bindings], result.mapped_size_, result.scoring_binding_count_);
    result.edges_ = fixed_section<EdgeRecord>(result.mapped_, sections[edges], result.mapped_size_, result.edge_count_);
    result.edge_postings_ = fixed_section<EdgePosting>(result.mapped_, sections[edge_postings], result.mapped_size_, result.edge_posting_count_);
    result.splice_records_ = fixed_section<SpliceRecord>(result.mapped_, sections[splice_records], result.mapped_size_, result.splice_record_count_);
    result.splice_targets_ = fixed_section<std::uint32_t>(result.mapped_, sections[Section::splice_targets], result.mapped_size_, result.splice_target_count_);
    result.junction_records_ = fixed_section<EdgeRecord>(result.mapped_, sections[junction_records], result.mapped_size_, result.junction_record_count_);
    result.junction_owners_ = fixed_section<JunctionOwner>(result.mapped_, sections[Section::junction_owners], result.mapped_size_, result.junction_owner_count_);
    result.node_blocks_ = fixed_section<EncodedBlock>(result.mapped_, sections[node_blocks], result.mapped_size_, result.node_block_count_);
    result.scoring_blocks_ = fixed_section<EncodedBlock>(result.mapped_, sections[scoring_blocks], result.mapped_size_, result.scoring_block_count_);
    result.edge_blocks_ = fixed_section<EdgeBlock>(result.mapped_, sections[edge_blocks], result.mapped_size_, result.edge_block_count_);
    result.encoded_ = fixed_section<std::byte>(result.mapped_, sections[encoded], result.mapped_size_, result.encoded_size_);
    result.edge_keys_ = fixed_section<EdgeKey>(result.mapped_, sections[edge_keys], result.mapped_size_, result.edge_key_count_);
    return result;
}

std::span<const GeneRecord> Reference::genes() const { return {genes_, gene_count_}; }
std::span<const PolicyRecord> Reference::policies() const { return {policies_, policy_count_}; }
std::span<const ModelRecord> Reference::models() const { return {models_, model_count_}; }
std::span<const ScoringClassRecord> Reference::scoring_classes() const {
    return {scoring_classes_, scoring_class_count_};
}
const NodeRecord* Reference::node(std::uint64_t id) const {
    const auto found = std::lower_bound(nodes_, nodes_ + node_count_, id, [](const NodeRecord& node, std::uint64_t wanted) { return node.id < wanted; });
    return found != nodes_ + node_count_ && found->id == id ? found : nullptr;
}
std::span<const Posting> Reference::node_postings(const NodeRecord& record) const {
    if (record.first > posting_count_ || record.count > posting_count_ - record.first) invalid("node posting bounds");
    const auto node = std::lower_bound(nodes_, nodes_ + node_count_, record.id,
                                       [](const NodeRecord& value, std::uint64_t id) { return value.id < id; });
    if (node == nodes_ + node_count_ || node->id != record.id) invalid("node record id");
    const auto rank = static_cast<std::uint64_t>(node - nodes_);
    if (auto* cached = cached_rows<Posting>(cache_id_, rank)) return {cached->data(), cached->size()};
    const EncodedBlock* first = node_blocks_;
    const EncodedBlock* node_blocks_end = node_blocks_;
    if (node_block_count_ != 0) {
        node_blocks_end = node_blocks_ + node_block_count_;
        first = std::lower_bound(node_blocks_, node_blocks_end, rank / 256,
                                 [](const EncodedBlock& value, std::uint64_t block) { return value.block < block; });
    }
    bool has_encoded = false;
    for (auto it = first; it != node_blocks_end && it->block == rank / 256; ++it) {
        has_encoded = has_encoded || block_contains(*it, rank);
    }
    if (!has_encoded) {
        const auto result = std::span<const Posting>(postings_ + record.first, record.count);
        for (const auto& posting : result) {
            if (posting.model >= model_count_ || posting.support_set >= sets_.size()) invalid("node posting id");
        }
        return result;
    }
    thread_local std::vector<Posting> rows;
    rows.assign(postings_ + record.first, postings_ + record.first + record.count);
    for (auto it = first; it != node_blocks_end && it->block == rank / 256; ++it) {
        if (!block_contains(*it, rank)) continue;
        auto decoded = decode_rows<Posting>(encoded_, encoded_size_, it->values);
        rows.insert(rows.end(), decoded.begin(), decoded.end());
    }
    std::sort(rows.begin(), rows.end(), [](const Posting& a, const Posting& b) { return a.model < b.model; });
    std::size_t write = 0;
    for (const auto& value : rows) {
        if (value.model >= model_count_ || value.support_set >= sets_.size()) invalid("node posting id");
        if (write != 0 && rows[write - 1].model == value.model) {
            if (rows[write - 1].support_set != value.support_set) invalid("duplicate node posting support");
            rows[write - 1].flags |= value.flags;
        } else rows[write++] = value;
    }
    rows.resize(write);
    const auto& cached = cache_rows<Posting>(cache_id_, rank, std::move(rows));
    return {cached.data(), cached.size()};
}
std::span<const ScoringPosting> Reference::node_scoring(const NodeRecord& record) const {
    if (record.score_first > scoring_posting_count_ ||
        record.score_count > scoring_posting_count_ - record.score_first) {
        invalid("node scoring bounds");
    }
    const auto node = std::lower_bound(nodes_, nodes_ + node_count_, record.id,
                                       [](const NodeRecord& value, std::uint64_t id) { return value.id < id; });
    if (node == nodes_ + node_count_ || node->id != record.id) invalid("node record id");
    const auto rank = static_cast<std::uint64_t>(node - nodes_);
    if (auto* cached = cached_rows<ScoringPosting>(cache_id_, rank)) return {cached->data(), cached->size()};
    const EncodedBlock* first = scoring_blocks_;
    const EncodedBlock* scoring_blocks_end = scoring_blocks_;
    if (scoring_block_count_ != 0) {
        scoring_blocks_end = scoring_blocks_ + scoring_block_count_;
        first = std::lower_bound(scoring_blocks_, scoring_blocks_end, rank / 256,
                                 [](const EncodedBlock& value, std::uint64_t block) { return value.block < block; });
    }
    bool has_encoded = false;
    for (auto it = first; it != scoring_blocks_end && it->block == rank / 256; ++it) {
        has_encoded = has_encoded || block_contains(*it, rank);
    }
    if (!has_encoded) {
        const auto result = std::span<const ScoringPosting>(scoring_postings_ + record.score_first,
                                                            record.score_count);
        for (const auto& posting : result) if (posting.class_id >= scoring_class_count_) invalid("node scoring class id");
        return result;
    }
    thread_local std::vector<ScoringPosting> rows;
    rows.assign(scoring_postings_ + record.score_first, scoring_postings_ + record.score_first + record.score_count);
    for (auto it = first; it != scoring_blocks_end && it->block == rank / 256; ++it) {
        if (!block_contains(*it, rank)) continue;
        auto decoded = decode_rows<ScoringPosting>(encoded_, encoded_size_, it->values);
        rows.insert(rows.end(), decoded.begin(), decoded.end());
    }
    std::sort(rows.begin(), rows.end(), [](const ScoringPosting& a, const ScoringPosting& b) { return a.class_id < b.class_id; });
    std::size_t write = 0;
    for (const auto& value : rows) {
        if (value.class_id >= scoring_class_count_) invalid("node scoring class id");
        if (write != 0 && rows[write - 1].class_id == value.class_id) {
            if (value.forward > std::numeric_limits<std::uint32_t>::max() - rows[write - 1].forward ||
                value.reverse > std::numeric_limits<std::uint32_t>::max() - rows[write - 1].reverse) invalid("duplicate node scoring overflow");
            rows[write - 1].forward += value.forward; rows[write - 1].reverse += value.reverse;
        } else rows[write++] = value;
    }
    rows.resize(write);
    const auto& cached = cache_rows<ScoringPosting>(cache_id_, rank, std::move(rows));
    return {cached.data(), cached.size()};
}
std::span<const ScoringBinding> Reference::scoring_bindings(const ScoringClassRecord& record) const {
    if ((record.flags & ~std::uint32_t{1}) != 0 || record.first > scoring_binding_count_ ||
        record.count > scoring_binding_count_ - record.first ||
        (record.count == 0 && (record.flags & 1) == 0)) {
        invalid("scoring class bounds");
    }
    const auto result = std::span<const ScoringBinding>(scoring_bindings_ + record.first,
                                                        record.count);
    std::pair<std::uint32_t, std::uint32_t> previous{};
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto& binding = result[i];
        const auto key = std::pair(binding.transcript_string, binding.policy);
        if (binding.transcript_string == missing || binding.transcript_string >= strings_.size() ||
            binding.policy >= policy_count_ || binding.multiplicity == 0 ||
            (i != 0 && key <= previous)) invalid("scoring binding id");
        previous = key;
    }
    return result;
}
std::span<const EdgePosting> Reference::edge(std::uint64_t from, std::uint64_t to) const {
    const auto found = std::lower_bound(edges_, edges_ + edge_count_, std::pair(from,to), [](const EdgeRecord& edge, const auto& key) { return std::pair(edge.from,edge.to) < key; });
    const bool has_flat = found != edges_ + edge_count_ && found->from == from && found->to == to;
    const EdgeKey* key_record = edge_keys_;
    const EdgeKey* edge_keys_end = edge_keys_;
    if (edge_key_count_ != 0) {
        edge_keys_end = edge_keys_ + edge_key_count_;
        key_record = std::lower_bound(edge_keys_, edge_keys_end, std::pair(from,to),
                                      [](const EdgeKey& edge, const auto& key) { return std::pair(edge.from, edge.to) < key; });
    }
    const bool has_key = key_record != edge_keys_end && key_record->from == from && key_record->to == to;
    const EdgeBlock* encoded = edge_blocks_;
    const EdgeBlock* edge_blocks_end = edge_blocks_;
    std::uint64_t rank = 0;
    if (has_key) {
        rank = key_record->rank;
        if (edge_block_count_ != 0) {
            edge_blocks_end = edge_blocks_ + edge_block_count_;
            encoded = std::lower_bound(edge_blocks_, edge_blocks_end, rank / 64,
                                       [](const EdgeBlock& edge, std::uint64_t block) { return edge.block < block; });
        }
    }
    bool has_encoded = false;
    for (auto it = encoded; has_key && it != edge_blocks_end && it->block == rank / 64; ++it) {
        has_encoded = has_encoded || (it->mask & (std::uint64_t{1} << (rank % 64))) != 0;
    }
    if (!has_flat && !has_encoded) return {};
    if (!has_encoded) {
        if (found->first > edge_posting_count_ || found->count > edge_posting_count_ - found->first) invalid("edge posting bounds");
        const auto result = std::span<const EdgePosting>(edge_postings_ + found->first, found->count);
        for (const auto& posting : result) if (posting.model >= model_count_) invalid("edge posting id");
        return result;
    }
    const auto cache_key = std::pair(from, to);
    if (auto* cached = cached_rows<EdgePosting>(cache_id_, cache_key)) return {cached->data(), cached->size()};
    thread_local std::vector<EdgePosting> rows;
    rows.clear();
    if (has_flat) {
        if (found->first > edge_posting_count_ || found->count > edge_posting_count_ - found->first) invalid("edge posting bounds");
        rows.insert(rows.end(), edge_postings_ + found->first, edge_postings_ + found->first + found->count);
    }
    for (auto it = encoded; it != edge_blocks_end && it->block == rank / 64; ++it) {
        if ((it->mask & (std::uint64_t{1} << (rank % 64))) == 0) continue;
        auto decoded = decode_rows<EdgePosting>(encoded_, encoded_size_, it->values);
        rows.insert(rows.end(), decoded.begin(), decoded.end());
    }
    std::sort(rows.begin(), rows.end(), [](const EdgePosting& a, const EdgePosting& b) { return a.model < b.model; });
    std::size_t write = 0;
    for (const auto& value : rows) {
        if (value.model >= model_count_) invalid("edge posting id");
        if (write != 0 && rows[write - 1].model == value.model) rows[write - 1].flags |= value.flags;
        else rows[write++] = value;
    }
    rows.resize(write);
    const auto& cached = cache_rows<EdgePosting>(cache_id_, cache_key, std::move(rows));
    return {cached.data(), cached.size()};
}
std::span<const std::uint32_t> Reference::splice_targets(std::uint64_t physical_a,
                                                          std::uint64_t physical_b) const {
    if (physical_b < physical_a) std::swap(physical_a, physical_b);
    if (physical_a == physical_b) return {};
    const auto found = std::lower_bound(
        splice_records_, splice_records_ + splice_record_count_, std::pair(physical_a, physical_b),
        [](const SpliceRecord& record, const auto& key) {
            return std::pair(record.from, record.to) < key;
        });
    if (found == splice_records_ + splice_record_count_ || found->from != physical_a ||
        found->to != physical_b) return {};
    if (found->first > splice_target_count_ || found->count > splice_target_count_ - found->first) {
        invalid("splice target bounds");
    }
    const auto result = std::span<const std::uint32_t>(splice_targets_ + found->first, found->count);
    std::uint32_t previous = 0;
    for (std::size_t i = 0; i < result.size(); ++i) {
        if (result[i] == missing || result[i] >= strings_.size() || (i != 0 && result[i] <= previous)) {
            invalid("splice target id");
        }
        previous = result[i];
    }
    return result;
}
std::span<const JunctionOwner> Reference::junction_owners(std::uint64_t encoded_from,
                                                           std::uint64_t encoded_to) const {
    const auto found = std::lower_bound(
        junction_records_, junction_records_ + junction_record_count_,
        std::pair(encoded_from, encoded_to), [](const EdgeRecord& record, const auto& key) {
            return std::pair(record.from, record.to) < key;
        });
    if (found == junction_records_ + junction_record_count_ || found->from != encoded_from ||
        found->to != encoded_to) return {};
    if (found->first > junction_owner_count_ || found->count > junction_owner_count_ - found->first) {
        invalid("junction owner bounds");
    }
    const auto result = std::span<const JunctionOwner>(junction_owners_ + found->first,
                                                       found->count);
    std::uint32_t previous = 0;
    for (std::size_t i = 0; i < result.size(); ++i) {
        const auto& owner = result[i];
        if (owner.transcript_string == missing || owner.transcript_string >= strings_.size() ||
            owner.flags == 0 || (owner.flags & ~std::uint32_t{3}) != 0 ||
            (i != 0 && owner.transcript_string <= previous)) {
            invalid("junction owner id");
        }
        previous = owner.transcript_string;
    }
    return result;
}

}  // namespace pancollapse::count_reference
