#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace pancollapse::count_reference {

inline constexpr std::uint32_t missing = UINT32_MAX;

struct GeneRecord {
    std::uint32_t id_string = missing, name_string = missing, type_set = 0;
    std::uint32_t equivalence[2] = {missing, missing};
    std::uint32_t nested_host[2] = {missing, missing};
    std::uint32_t competition[2] = {2, 2};  // 0 primary, 1 fallback, 2 unknown
};
struct PolicyRecord {
    std::uint32_t gene = missing, category_set = 0, novel_origin = missing, flags = 0;
};
struct ModelRecord {
    std::uint32_t transcript_string = missing, exon_policy = missing, body_policy = missing,
                  strand = 0;
};
struct NodeRecord {
    std::uint64_t id = 0, length = 0, first = 0;
    std::uint32_t count = 0, reserved = 0;
    std::uint64_t score_first = 0;
    std::uint32_t score_count = 0, reserved2 = 0;
};
struct Posting { std::uint32_t model = missing, support_set = 0, flags = 0; };

// A read-only logical posting range. Legacy and native sources expose raw
// Posting rows; compact direct indexes expose canonical delta-varints backed by
// an immutable model-support sidecar. Iteration returns Posting values so no
// decoded global array or per-read cache is required.
class PostingView {
  public:
    class iterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Posting;
        using difference_type = std::ptrdiff_t;
        using reference = Posting;
        using pointer = void;

        iterator() = default;
        Posting operator*() const {
            if (mode_ == Mode::raw) return *raw_;
            if (mode_ == Mode::encoded) return current_;
            throw std::out_of_range("posting iterator dereferenced at end");
        }
        iterator& operator++() {
            if (mode_ == Mode::raw) {
                if (++raw_ == raw_end_) mode_ = Mode::end;
                return *this;
            }
            if (mode_ != Mode::encoded)
                throw std::out_of_range("posting iterator advanced at end");
            encoded_ = next_;
            prior_model_ = current_.model;
            first_ = false;
            if (--remaining_ == 0) {
                if (encoded_ != encoded_end_)
                    throw std::runtime_error("compact posting list has trailing bytes");
                mode_ = Mode::end;
            } else {
                decode();
            }
            return *this;
        }
        iterator operator++(int) { auto copy = *this; ++*this; return copy; }
        friend bool operator==(const iterator& left, const iterator& right) {
            if (left.mode_ == Mode::end || right.mode_ == Mode::end)
                return left.mode_ == right.mode_;
            if (left.mode_ != right.mode_) return false;
            return left.mode_ == Mode::raw ? left.raw_ == right.raw_ :
                left.encoded_ == right.encoded_ && left.remaining_ == right.remaining_;
        }

      private:
        friend class PostingView;
        enum class Mode : std::uint8_t { end, raw, encoded };
        Mode mode_ = Mode::end;
        const Posting* raw_ = nullptr;
        const Posting* raw_end_ = nullptr;
        const std::byte* encoded_ = nullptr;
        const std::byte* encoded_end_ = nullptr;
        const std::byte* next_ = nullptr;
        const std::uint32_t* model_supports_ = nullptr;
        std::size_t model_count_ = 0, support_set_count_ = 0;
        std::size_t remaining_ = 0;
        std::uint32_t prior_model_ = 0;
        bool first_ = true;
        Posting current_;

        static iterator raw(const Posting* begin, const Posting* end) {
            iterator result;
            if (begin != end) {
                result.mode_ = Mode::raw;
                result.raw_ = begin;
                result.raw_end_ = end;
            }
            return result;
        }
        static iterator encoded(const std::byte* begin, const std::byte* end,
                                std::size_t count, const std::uint32_t* supports,
                                std::size_t model_count, std::size_t support_set_count) {
            iterator result;
            if (count == 0) {
                if (begin != end)
                    throw std::runtime_error("empty compact posting list has bytes");
                return result;
            }
            result.mode_ = Mode::encoded;
            result.encoded_ = begin;
            result.encoded_end_ = end;
            result.model_supports_ = supports;
            result.model_count_ = model_count;
            result.support_set_count_ = support_set_count;
            result.remaining_ = count;
            result.decode();
            return result;
        }
        void decode() {
            std::uint64_t code = 0;
            const std::byte* cursor = encoded_;
            if (cursor == encoded_end_)
                throw std::runtime_error("truncated compact posting varint");
            const auto first_byte = std::to_integer<unsigned char>(*cursor);
            if ((first_byte & 0x80U) == 0) {
                code = first_byte;
                ++cursor;
            } else {
                bool complete = false;
                for (unsigned index = 0; index < 6; ++index) {
                    if (cursor == encoded_end_)
                        throw std::runtime_error("truncated compact posting varint");
                    const auto byte = std::to_integer<unsigned char>(*cursor++);
                    const auto payload = static_cast<std::uint64_t>(byte & 0x7fU);
                    if (index == 5 && payload > 1)
                        throw std::runtime_error("compact posting varint overflow");
                    code |= payload << (7 * index);
                    if ((byte & 0x80U) == 0) {
                        if (index != 0 && payload == 0)
                            throw std::runtime_error("non-canonical compact posting varint");
                        complete = true;
                        break;
                    }
                }
                if (!complete) throw std::runtime_error("compact posting varint overflow");
            }
            const auto flags = static_cast<std::uint32_t>(code & 0x0fU);
            const auto delta = code >> 4;
            if (flags == 0 || delta > std::numeric_limits<std::uint32_t>::max())
                throw std::runtime_error("compact posting value is invalid");
            std::uint32_t model = 0;
            if (first_) {
                model = static_cast<std::uint32_t>(delta);
            } else {
                if (delta == 0 || delta > std::numeric_limits<std::uint32_t>::max() - prior_model_)
                    throw std::runtime_error("compact posting model delta is invalid");
                model = prior_model_ + static_cast<std::uint32_t>(delta);
            }
            if (model == missing || model >= model_count_ || model_supports_ == nullptr)
                throw std::runtime_error("compact posting model is invalid");
            const auto support = (flags & 0x0cU) != 0 ? model_supports_[model] : 0;
            if ((flags & 0x0cU) != 0 && support >= support_set_count_)
                throw std::runtime_error("compact posting support set is invalid");
            current_ = {model, support, flags};
            next_ = cursor;
        }
    };

    PostingView() = default;
    PostingView(std::span<const Posting> raw)
        : raw_(raw.data()), count_(raw.size()) {}
    static PostingView encoded(std::span<const std::byte> bytes, std::size_t count,
                               std::span<const std::uint32_t> model_supports,
                               std::size_t support_set_count) {
        if (count == 0 && !bytes.empty())
            throw std::runtime_error("empty compact posting list has bytes");
        PostingView result;
        result.encoded_ = bytes.data();
        result.encoded_bytes_ = bytes.size();
        result.model_supports_ = model_supports.data();
        result.model_count_ = model_supports.size();
        result.support_set_count_ = support_set_count;
        result.count_ = count;
        result.compact_ = true;
        return result;
    }
    std::size_t size() const { return count_; }
    bool empty() const { return count_ == 0; }
    const void* data() const { return compact_ ? static_cast<const void*>(encoded_) : raw_; }
    iterator begin() const {
        if (count_ == 0) return {};
        if (!compact_) return iterator::raw(raw_, raw_ + count_);
        return iterator::encoded(encoded_, encoded_ + encoded_bytes_, count_, model_supports_,
                                 model_count_, support_set_count_);
    }
    iterator end() const { return {}; }
    Posting operator[](std::size_t index) const {
        if (index >= count_) throw std::out_of_range("posting index");
        if (!compact_) return raw_[index];
        auto found = begin();
        while (index-- != 0) ++found;
        return *found;
    }

  private:
    const Posting* raw_ = nullptr;
    const std::byte* encoded_ = nullptr;
    const std::uint32_t* model_supports_ = nullptr;
    std::size_t count_ = 0, encoded_bytes_ = 0, model_count_ = 0, support_set_count_ = 0;
    bool compact_ = false;
};

struct NodeView {
    std::uint64_t length = 0;
    PostingView postings;
    // Opaque list identity comparable only within the same Source and read
    // group. Equal non-missing IDs guarantee an identical ordered logical
    // Posting sequence; zero is a valid identity for an empty list.
    std::uint32_t shared_membership_id = missing;
};
struct ScoringPosting {
    std::uint32_t class_id = missing, forward = 0, reverse = 0;
};
struct ScoringClassRecord {
    std::uint64_t first = 0;
    std::uint32_t count = 0, flags = 0;
};
struct ScoringBinding {
    std::uint32_t transcript_string = missing, policy = missing, multiplicity = 0, reserved = 0;
};
struct EdgeRecord {
    std::uint64_t from = 0, to = 0, first = 0;
    std::uint32_t count = 0, reserved = 0;
};
struct EdgePosting { std::uint32_t model = missing, flags = 0; };
struct SpliceRecord {
    std::uint64_t from = 0, to = 0, first = 0;
    std::uint32_t count = 0, reserved = 0;
};
struct JunctionOwner {
    std::uint32_t transcript_string = missing, flags = 0;
};
struct EncodedSpan {
    std::uint64_t first = 0;
    std::uint32_t bytes = 0, count = 0;
};
struct EncodedBlock {
    std::uint64_t block = 0;
    std::uint64_t mask[4] = {0, 0, 0, 0};
    EncodedSpan values;
};
struct EdgeKey {
    std::uint64_t from = 0, to = 0, rank = 0;
};
struct EdgeBlock {
    std::uint64_t block = 0, mask = 0;
    EncodedSpan values;
};

// Append-only fixed-size slabs prevent an already encoded payload from being
// copied when the next local block is produced. The logical stream remains
// contiguous when written to the count-reference encoded section.
class EncodedBuffer {
  public:
    static constexpr std::size_t kDefaultSlabCapacity = 256ULL * 1024 * 1024;

    explicit EncodedBuffer(std::size_t slab_capacity = kDefaultSlabCapacity);
    EncodedBuffer(const EncodedBuffer&) = default;
    EncodedBuffer& operator=(const EncodedBuffer&) = default;
    EncodedBuffer(EncodedBuffer&& other) noexcept;
    EncodedBuffer& operator=(EncodedBuffer&& other) noexcept;
    void append(std::span<const std::byte> bytes);
    std::uint64_t size() const { return size_; }
    std::uint64_t allocated_bytes() const;
    std::size_t slab_capacity() const { return slab_capacity_; }
    const std::vector<std::vector<std::byte>>& slabs() const { return slabs_; }

  private:
    std::size_t slab_capacity_ = kDefaultSlabCapacity;
    std::uint64_t size_ = 0;
    std::vector<std::vector<std::byte>> slabs_;
};

struct Data {
    std::vector<std::string> strings;
    std::vector<std::vector<std::uint32_t>> sets;
    std::vector<GeneRecord> genes;
    std::vector<PolicyRecord> policies;
    std::vector<ModelRecord> models;
    std::vector<NodeRecord> nodes;
    std::vector<Posting> postings;
    std::vector<ScoringPosting> scoring_postings;
    std::vector<ScoringClassRecord> scoring_classes;
    std::vector<ScoringBinding> scoring_bindings;
    std::vector<EdgeRecord> edges;
    std::vector<EdgePosting> edge_postings;
    std::vector<SpliceRecord> splice_records;
    std::vector<std::uint32_t> splice_targets;
    std::vector<EdgeRecord> junction_records;
    std::vector<JunctionOwner> junction_owners;
    std::vector<EncodedBlock> node_blocks;
    std::vector<EncodedBlock> scoring_blocks;
    std::vector<EdgeKey> edge_keys;
    std::vector<EdgeBlock> edge_blocks;
    std::vector<std::byte> encoded;
    EncodedBuffer encoded_slabs;
    std::string content_id;
};

inline std::uint64_t encoded_size(const Data& data) {
    return static_cast<std::uint64_t>(data.encoded.size()) + data.encoded_slabs.size();
}

// Writes a deterministic, little-endian PCANCOUNT2 file through a same-directory
// temporary path, then atomically renames it into place.
void write(const std::filesystem::path& path, const Data& data);
bool is_count_reference(const std::filesystem::path& path) noexcept;

// Read-ready evidence access. Implementations either mmap prepared postings or
// query already-loaded native graph indexes for the current read's nodes/edges.
// No implementation may prepare global reference state in begin_read_group().
// Posting ranges and edge spans are sorted by model; splice-target spans and junction-owner
// spans are sorted by transcript string. Resolver lookups rely on these orders.
class Source {
  public:
    virtual ~Source() = default;
    virtual const std::vector<std::string>& strings() const = 0;
    virtual const std::vector<std::vector<std::uint32_t>>& sets() const = 0;
    virtual std::span<const GeneRecord> genes() const = 0;
    virtual std::span<const PolicyRecord> policies() const = 0;
    virtual const std::string& content_id() const = 0;
    virtual std::size_t model_count() const = 0;
    virtual ModelRecord model(std::uint32_t id) const = 0;
    // Optional, immutable ID-indexed table, valid for the Source lifetime.
    // Empty means use model(id); exposing a table must not prepare or scan it.
    virtual std::span<const ModelRecord> ready_models() const { return {}; }
    virtual std::size_t scoring_class_count() const = 0;
    virtual ScoringClassRecord scoring_class(std::uint32_t id) const = 0;
    virtual const NodeRecord* node(std::uint64_t id) const = 0;
    virtual std::span<const Posting> node_postings(const NodeRecord& record) const = 0;
    // A view stays valid through the current read group. Compact sources can
    // expose their mapped payload without reconstructing expanded node rows.
    virtual std::optional<NodeView> node_view(std::uint64_t id) const {
        const auto* record = node(id);
        if (!record) return std::nullopt;
        return NodeView{record->length, node_postings(*record)};
    }
    virtual std::span<const ScoringPosting> node_scoring(const NodeRecord& record) const = 0;
    virtual std::span<const ScoringBinding> scoring_bindings(const ScoringClassRecord& record) const = 0;
    virtual std::span<const EdgePosting> edge(std::uint64_t from, std::uint64_t to) const = 0;
    virtual std::span<const std::uint32_t> splice_targets(std::uint64_t a, std::uint64_t b) const = 0;
    virtual std::span<const JunctionOwner> junction_owners(std::uint64_t from, std::uint64_t to) const = 0;
    virtual void begin_read_group() const {}
};

class Reference : public Source {
  public:
    Reference() = default;
    ~Reference();
    Reference(Reference&&) noexcept;
    Reference& operator=(Reference&&) noexcept;
    Reference(const Reference&) = delete;
    Reference& operator=(const Reference&) = delete;

    static Reference load(const std::filesystem::path& path);
    const std::vector<std::string>& strings() const { return strings_; }
    const std::vector<std::vector<std::uint32_t>>& sets() const { return sets_; }
    std::span<const GeneRecord> genes() const;
    std::span<const PolicyRecord> policies() const;
    std::span<const ModelRecord> models() const;
    std::span<const ScoringClassRecord> scoring_classes() const;
    std::size_t model_count() const override { return model_count_; }
    ModelRecord model(std::uint32_t id) const override {
        if (id >= model_count_) throw std::out_of_range("invalid prepared model");
        return models()[id];
    }
    std::size_t scoring_class_count() const override { return scoring_class_count_; }
    ScoringClassRecord scoring_class(std::uint32_t id) const override {
        if (id >= scoring_class_count_) throw std::out_of_range("invalid prepared scoring class");
        return scoring_classes()[id];
    }
    const std::string& content_id() const { return content_id_; }
    const NodeRecord* node(std::uint64_t id) const;
    std::span<const Posting> node_postings(const NodeRecord& record) const;
    std::span<const ScoringPosting> node_scoring(const NodeRecord& record) const;
    std::span<const ScoringBinding> scoring_bindings(const ScoringClassRecord& record) const;
    std::span<const EdgePosting> edge(std::uint64_t from, std::uint64_t to) const;
    std::span<const std::uint32_t> splice_targets(std::uint64_t physical_a,
                                                  std::uint64_t physical_b) const;
    std::span<const JunctionOwner> junction_owners(std::uint64_t encoded_from,
                                                   std::uint64_t encoded_to) const;

  private:
    int fd_ = -1;
    std::uint64_t cache_id_ = 0;
    const std::byte* mapped_ = nullptr;
    std::size_t mapped_size_ = 0;
    const GeneRecord* genes_ = nullptr; std::size_t gene_count_ = 0;
    const PolicyRecord* policies_ = nullptr; std::size_t policy_count_ = 0;
    const ModelRecord* models_ = nullptr; std::size_t model_count_ = 0;
    const NodeRecord* nodes_ = nullptr; std::size_t node_count_ = 0;
    const Posting* postings_ = nullptr; std::size_t posting_count_ = 0;
    const ScoringPosting* scoring_postings_ = nullptr; std::size_t scoring_posting_count_ = 0;
    const ScoringClassRecord* scoring_classes_ = nullptr; std::size_t scoring_class_count_ = 0;
    const ScoringBinding* scoring_bindings_ = nullptr; std::size_t scoring_binding_count_ = 0;
    const EdgeRecord* edges_ = nullptr; std::size_t edge_count_ = 0;
    const EdgePosting* edge_postings_ = nullptr; std::size_t edge_posting_count_ = 0;
    const SpliceRecord* splice_records_ = nullptr; std::size_t splice_record_count_ = 0;
    const std::uint32_t* splice_targets_ = nullptr; std::size_t splice_target_count_ = 0;
    const EdgeRecord* junction_records_ = nullptr; std::size_t junction_record_count_ = 0;
    const JunctionOwner* junction_owners_ = nullptr; std::size_t junction_owner_count_ = 0;
    const EncodedBlock* node_blocks_ = nullptr; std::size_t node_block_count_ = 0;
    const EncodedBlock* scoring_blocks_ = nullptr; std::size_t scoring_block_count_ = 0;
    const EdgeKey* edge_keys_ = nullptr; std::size_t edge_key_count_ = 0;
    const EdgeBlock* edge_blocks_ = nullptr; std::size_t edge_block_count_ = 0;
    const std::byte* encoded_ = nullptr; std::size_t encoded_size_ = 0;
    std::vector<std::string> strings_;
    std::vector<std::vector<std::uint32_t>> sets_;
    std::string content_id_;
    void close() noexcept;
};

}  // namespace pancollapse::count_reference
