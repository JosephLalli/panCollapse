#include "native_count_reference.hpp"

#include "native_count_index.hpp"
#include "bounded_reference_cache.hpp"
#include "pod_row_codec.hpp"

#include <gbwt/fast_locate.h>
#include <gbwt/gbwt.h>
#include <xg.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <syncstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace pancollapse {
namespace {

using count_reference::EdgePosting;
using count_reference::JunctionOwner;
using count_reference::ModelRecord;
using count_reference::NodeRecord;
using count_reference::Posting;
using count_reference::ScoringBinding;
using count_reference::ScoringClassRecord;
using count_reference::ScoringPosting;
using count_reference::missing;

constexpr std::uint32_t kExonForward = 1;
constexpr std::uint32_t kExonReverse = 2;
constexpr std::uint32_t kBodyForward = 4;
constexpr std::uint32_t kBodyReverse = 8;
constexpr std::uint32_t kEdgeExon = 1;
constexpr std::uint32_t kEdgeOrdinaryBody = 2;
constexpr std::uint32_t kJunctionAlongTranscript = 1;
constexpr std::uint32_t kJunctionReverseComplement = 2;
constexpr std::uint32_t kEdgeSummaryExonMask = kJunctionAlongTranscript | kJunctionReverseComplement;
constexpr std::uint32_t kEdgeSummaryBody = 4;

std::atomic<std::uint64_t> next_cache_token{1};

template<class T>
void checked_increment(T& value, const char* what) {
    if (value == std::numeric_limits<T>::max()) {
        throw std::overflow_error(std::string("native count ") + what + " overflow");
    }
    ++value;
}

std::uint32_t checked_count(std::size_t value, const char* what) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error(std::string("native count ") + what + " exceeds Source count width");
    }
    return static_cast<std::uint32_t>(value);
}

void atomic_max(std::atomic<std::uint64_t>& target, std::uint64_t value) noexcept {
    std::uint64_t observed = target.load(std::memory_order_relaxed);
    while (observed < value &&
           !target.compare_exchange_weak(observed, value, std::memory_order_relaxed,
                                         std::memory_order_relaxed)) {
    }
}

std::uint64_t oriented(std::uint64_t node, bool reverse) {
    if (node > (std::numeric_limits<std::uint64_t>::max() >> 1)) {
        throw std::runtime_error("native count node id cannot encode orientation");
    }
    return (node << 1) | static_cast<std::uint64_t>(reverse);
}

}  // namespace

struct NativeSource::Impl {
    struct EdgeKey {
        std::uint64_t from = 0, to = 0;
        bool operator==(const EdgeKey&) const = default;
    };
    struct EdgeHash {
        std::size_t operator()(const EdgeKey& value) const noexcept {
            const std::uint64_t mixed = value.from ^
                (value.to + 0x9e3779b97f4a7c15ULL + (value.from << 6) + (value.from >> 2));
            return static_cast<std::size_t>(mixed);
        }
    };
    struct PathHit {
        std::uint32_t row = missing;
        bool reverse_sequence = false;
    };
    struct DistinctPathHit {
        std::uint32_t row = missing;
        std::uint8_t orientations = 0;
    };
    struct NodeEvidence {
        NodeRecord record;
        std::vector<PathHit> hits;
        std::vector<Posting> postings;
        std::vector<std::byte> encoded_postings;
        pod_row_codec::Encoding posting_encoding = pod_row_codec::Encoding::raw;
        std::vector<ScoringPosting> scoring;
        std::vector<std::uint32_t> body_targets;
    };
    struct EdgeEvidence {
        std::vector<PathHit> hits;
        std::vector<EdgePosting> postings;
        struct Summary { std::uint32_t transcript = missing, flags = 0; };
        std::vector<Summary> summary;
    };
    struct EdgeCursor {
        gbwt::SearchState state;
        gbwt::size_type packed = gbwt::FastLocate::NO_POSITION;
        gbwt::size_type visited = 0;
        bool initialized = false;
        std::vector<gbwt::size_type> prefix_sequences;
    };
    struct SpliceEvidence {
        std::vector<std::uint32_t> targets;
    };
    static std::size_t charged_sum(std::size_t left, std::size_t right) noexcept {
        return right > std::numeric_limits<std::size_t>::max() - left
            ? std::numeric_limits<std::size_t>::max() : left + right;
    }
    template<class T>
    static std::size_t buffer_bytes(const std::vector<T>& value) noexcept {
        return value.capacity() > std::numeric_limits<std::size_t>::max() / sizeof(T)
            ? std::numeric_limits<std::size_t>::max() : value.capacity() * sizeof(T);
    }
    static std::size_t node_bytes(const NodeEvidence& value) noexcept {
        return charged_sum(charged_sum(sizeof(value), buffer_bytes(value.hits)),
                           charged_sum(charged_sum(buffer_bytes(value.postings),
                                                   buffer_bytes(value.encoded_postings)),
                                       charged_sum(buffer_bytes(value.scoring),
                                                   buffer_bytes(value.body_targets))));
    }
    static std::size_t edge_bytes(const EdgeEvidence& value) noexcept {
        return charged_sum(charged_sum(sizeof(value), buffer_bytes(value.hits)),
                           charged_sum(buffer_bytes(value.postings), buffer_bytes(value.summary)));
    }
    static std::size_t splice_bytes(const SpliceEvidence& value) noexcept {
        return charged_sum(sizeof(value), buffer_bytes(value.targets));
    }
    struct LeaseAccounting {
        std::atomic<std::uint64_t> active{0};
        std::atomic<std::uint64_t> high_water{0};
    };
    using NodeLease = std::shared_ptr<const NodeEvidence>;
    using EdgeLease = std::shared_ptr<const EdgeEvidence>;
    using SpliceLease = std::shared_ptr<const SpliceEvidence>;
    struct WorkerCache {
        std::unordered_map<std::uint64_t, NodeLease> nodes;
        std::unordered_map<std::uint64_t, std::vector<Posting>> node_postings;
        pod_row_codec::Encoder posting_encoder;
        std::unordered_map<EdgeKey, EdgeLease, EdgeHash> edges;
        std::unordered_map<EdgeKey, std::vector<EdgePosting>, EdgeHash> edge_postings;
        std::unordered_map<std::uint64_t, std::vector<ScoringPosting>> node_scoring;
        std::unordered_map<EdgeKey, SpliceLease, EdgeHash> splices;
        std::unordered_map<EdgeKey, std::vector<JunctionOwner>, EdgeHash> junctions;
        std::unordered_map<std::uint32_t, std::vector<ScoringBinding>> bindings;
        std::shared_ptr<LeaseAccounting> accounting;
        std::uint64_t lease_bytes = 0;

        WorkerCache() = default;
        WorkerCache(const WorkerCache&) = delete;
        WorkerCache& operator=(const WorkerCache&) = delete;
        ~WorkerCache() { clear(); }

        void add_lease_bytes(std::size_t bytes) {
            if (bytes > std::numeric_limits<std::uint64_t>::max() - lease_bytes) {
                throw std::overflow_error("native count worker lease charge overflows uint64");
            }
            auto active = accounting->active.load(std::memory_order_relaxed);
            for (;;) {
                if (bytes > std::numeric_limits<std::uint64_t>::max() - active) {
                    throw std::overflow_error("native count active lease charge overflows uint64");
                }
                if (accounting->active.compare_exchange_weak(
                        active, active + bytes, std::memory_order_relaxed)) break;
            }
            lease_bytes += bytes;
            atomic_max(accounting->high_water, active + bytes);
        }

        void release_lease_bytes() noexcept {
            if (lease_bytes != 0) {
                accounting->active.fetch_sub(lease_bytes, std::memory_order_relaxed);
                lease_bytes = 0;
            }
        }

        void clear() {
            nodes.clear();
            node_postings.clear();
            edges.clear();
            edge_postings.clear();
            node_scoring.clear();
            splices.clear();
            junctions.clear();
            bindings.clear();
            release_lease_bytes();
        }
    };

    native_count::Index index;
    std::unique_ptr<xg::XG> graph;
    std::unique_ptr<gbwt::GBWT> gbwt;
    std::unique_ptr<gbwt::FastLocate> locate;
    // Node and edge builders are leaves. Splice construction acquires node then
    // edge evidence only; neither path can call back into the splice cache.
    static constexpr std::size_t node_cache_bytes = std::size_t{8} << 30;
    static constexpr std::size_t edge_cache_bytes = std::size_t{8} << 30;
    mutable BoundedReferenceCache<std::uint64_t, NodeEvidence> node_cache{
        node_cache_bytes, node_bytes};
    mutable BoundedReferenceCache<EdgeKey, EdgeEvidence, 256, EdgeHash> edge_cache{
        edge_cache_bytes, edge_bytes};
    static constexpr std::size_t splice_cache_bytes = std::size_t{512} << 20;
    mutable BoundedReferenceCache<EdgeKey, SpliceEvidence, 256, EdgeHash> splice_cache{
        splice_cache_bytes, splice_bytes};
    std::shared_ptr<LeaseAccounting> lease_accounting = std::make_shared<LeaseAccounting>();
    NativeQueryMode query_mode = NativeQueryMode::full;
    std::size_t max_probe_sa_steps = 4096;
    bool enable_adjacency_certificate = true;
    std::uint64_t cache_token = 0;
    mutable std::atomic<std::uint64_t> native_queries{0};
    mutable std::atomic<std::uint64_t> native_occurrences{0};
    mutable std::atomic<std::uint64_t> native_max_occurrences{0};
    mutable std::atomic<std::uint64_t> node_queries{0};
    mutable std::atomic<std::uint64_t> edge_queries{0};
    mutable std::atomic<std::uint64_t> edge_model_builds{0};
    mutable std::atomic<std::uint64_t> max_node_occurrences{0};
    mutable std::atomic<std::uint64_t> max_node_models{0};
    mutable std::atomic<std::uint64_t> splice_probe_common_empty{0}, splice_probe_attempts{0};
    mutable std::atomic<std::uint64_t> splice_probe_sa_steps{0}, splice_probe_success{0};
    mutable std::atomic<std::uint64_t> splice_probe_budget_fallback{0};
    mutable std::atomic<std::uint64_t> splice_probe_exhausted_without_proof{0};
    mutable std::atomic<std::uint64_t> splice_adjacency_checks{0}, splice_adjacency_success{0};

    static thread_local std::unordered_map<std::uint64_t, WorkerCache> worker_caches;

    WorkerCache& cache() const {
        auto [found, inserted] = worker_caches.try_emplace(cache_token);
        if (inserted) found->second.accounting = lease_accounting;
        return found->second;
    }

    void record_query(std::uint64_t occurrences, bool is_node) const noexcept {
        native_queries.fetch_add(1, std::memory_order_relaxed);
        native_occurrences.fetch_add(occurrences, std::memory_order_relaxed);
        atomic_max(native_max_occurrences, occurrences);
        if (is_node) {
            node_queries.fetch_add(1, std::memory_order_relaxed);
            atomic_max(max_node_occurrences, occurrences);
        } else {
            edge_queries.fetch_add(1, std::memory_order_relaxed);
        }
    }

    const native_count::PathRecord& path(std::uint32_t row) const {
        const auto paths = index.paths();
        if (row == missing || row >= paths.size()) {
            throw std::runtime_error("native count path row is invalid");
        }
        return paths[row];
    }

    std::uint32_t target(const native_count::PathRecord& path_record) const {
        if (path_record.transcript_string == missing ||
            path_record.transcript_string >= index.strings().size()) {
            throw std::runtime_error("native count path transcript is invalid");
        }
        return path_record.transcript_string;
    }

    std::vector<PathHit> node_hits(std::uint64_t id) const {
        if (id > (std::numeric_limits<std::uint64_t>::max() >> 1)) {
            throw std::runtime_error("native count node id cannot encode GBWT node");
        }
        const auto node = gbwt::Node::encode(id, false);
        if (!gbwt->contains(node) || gbwt->empty(node)) return {};
        const auto packed = locate->decompressSA(node);
        record_query(packed.size(), true);
        std::vector<PathHit> result;
        result.reserve(packed.size());
        for (const auto position : packed) {
            const auto [sequence, ignored_offset] = locate->unpack(position);
            static_cast<void>(ignored_offset);
            if (sequence >= gbwt->sequences()) {
                throw std::runtime_error("native FastLocate returned an invalid sequence id");
            }
            const auto row = index.path_row(gbwt::Path::id(sequence));
            if (row == missing) continue;
            result.push_back({row, gbwt::Path::is_reverse(sequence)});
        }
        return result;
    }

    std::vector<DistinctPathHit> node_score_hits(std::uint64_t id) const {
        if (id > (std::numeric_limits<std::uint64_t>::max() >> 1)) {
            throw std::runtime_error("native count node id cannot encode GBWT node");
        }
        const auto node = gbwt::Node::encode(id, false);
        if (!gbwt->contains(node) || gbwt->empty(node)) return {};
        gbwt::size_type first = gbwt::FastLocate::NO_POSITION;
        const auto state = locate->find(node, first);
        record_query(state.size(), true);
        std::vector<gbwt::size_type> sequences;
        if (!state.empty()) sequences = locate->locate(state, first);
        std::vector<DistinctPathHit> result;
        result.reserve(sequences.size());
        for (const auto sequence : sequences) {
            if (sequence >= gbwt->sequences()) {
                throw std::runtime_error("native FastLocate returned an invalid sequence id");
            }
            const auto row = index.path_row(gbwt::Path::id(sequence));
            if (row == missing) continue;
            result.push_back({row, static_cast<std::uint8_t>(gbwt::Path::is_reverse(sequence) ? 2 : 1)});
        }
        std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
            return left.row < right.row;
        });
        std::size_t output = 0;
        for (const auto& hit : result) {
            if (output == 0 || result[output - 1].row != hit.row) result[output++] = hit;
            else result[output - 1].orientations |= hit.orientations;
        }
        result.resize(output);
        return result;
    }

    std::vector<ScoringPosting> scoring_postings(std::span<const PathHit> hits) const {
        std::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> rows;
        for (const PathHit& hit : hits) {
            auto& score = rows[hit.row];
            if (hit.reverse_sequence) checked_increment(score.second, "raw reverse visit count");
            else checked_increment(score.first, "raw forward visit count");
        }
        std::vector<ScoringPosting> result;
        result.reserve(rows.size());
        for (const auto& [class_id, visits] : rows) {
            result.push_back({class_id, visits.first, visits.second});
        }
        return result;
    }

    NodeEvidence build_node_evidence(std::uint64_t id) const {
        if (id == 0 || id > static_cast<std::uint64_t>(std::numeric_limits<handlegraph::nid_t>::max()) ||
            !graph->has_node(static_cast<handlegraph::nid_t>(id))) {
            throw std::runtime_error("GAMP node is absent from native XG: " + std::to_string(id));
        }
        NodeEvidence result;
        const auto handle = graph->get_handle(static_cast<handlegraph::nid_t>(id), false);
        result.record.id = id;
        result.record.length = graph->get_length(handle);
        std::vector<PathHit> hits;
        std::vector<DistinctPathHit> distinct_hits;
        if (query_mode == NativeQueryMode::node_score) distinct_hits = node_score_hits(id);
        else hits = node_hits(id);

        std::vector<Posting> posting_candidates;
        std::map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> scoring_rows;
        std::vector<std::uint32_t> body_targets;
        const auto pairs = index.pairs();
        const auto add_path = [&](std::uint32_t row, std::uint8_t orientations) {
            const auto& path_record = path(row);
            if (query_mode == NativeQueryMode::node_score &&
                (path_record.flags & native_count::body_path) != 0) {
                body_targets.push_back(target(path_record));
            }
            const auto& models = index.model_ids(path_record);
            for (const std::uint32_t model_id : models) {
                if (model_id >= pairs.size()) {
                    throw std::runtime_error("native count path model reference is invalid");
                }
                const auto& pair = pairs[model_id];
                if (pair.exon != row && pair.body != row) {
                    throw std::runtime_error("native count inverse pair does not contain its path");
                }
                Posting posting;
                posting.model = model_id;
                if (pair.exon == row) {
                    if ((orientations & 1) != 0) posting.flags |= kExonForward;
                    if ((orientations & 2) != 0) posting.flags |= kExonReverse;
                }
                if (pair.body == row) {
                    const auto& body = path(pair.body);
                    posting.support_set = body.support_set;
                    if ((orientations & 1) != 0) posting.flags |= kBodyForward;
                    if ((orientations & 2) != 0) posting.flags |= kBodyReverse;
                }
                posting_candidates.push_back(posting);
            }
        };

        if (query_mode == NativeQueryMode::node_score) {
            for (const DistinctPathHit& hit : distinct_hits) add_path(hit.row, hit.orientations);
        } else {
            for (const PathHit& hit : hits) {
                add_path(hit.row, hit.reverse_sequence ? 2 : 1);
                auto& score = scoring_rows[hit.row];
                if (hit.reverse_sequence) checked_increment(score.second, "raw reverse visit count");
                else checked_increment(score.first, "raw forward visit count");
            }
        }
        std::sort(posting_candidates.begin(), posting_candidates.end(), [](const auto& left, const auto& right) {
            return left.model < right.model;
        });
        result.postings.reserve(posting_candidates.size());
        for (const Posting& candidate : posting_candidates) {
            if (result.postings.empty() || result.postings.back().model != candidate.model) {
                result.postings.push_back(candidate);
                continue;
            }
            Posting& posting = result.postings.back();
            if ((posting.flags & (kBodyForward | kBodyReverse)) != 0 &&
                (candidate.flags & (kBodyForward | kBodyReverse)) != 0 &&
                posting.support_set != candidate.support_set) {
                throw std::runtime_error("native count pair has conflicting body support");
            }
            if ((candidate.flags & (kBodyForward | kBodyReverse)) != 0) {
                posting.support_set = candidate.support_set;
            }
            posting.flags |= candidate.flags;
        }
        result.record.first = 0;
        result.record.count = checked_count(result.postings.size(), "node model fanout");
        result.record.score_first = 0;
        result.record.score_count = checked_count(
            query_mode == NativeQueryMode::node_score ? distinct_hits.size() : scoring_rows.size(),
            "node scoring fanout");
        atomic_max(max_node_models, result.postings.size());
        if (query_mode == NativeQueryMode::node_score) {
            auto& encoder = cache().posting_encoder;
            const auto encoded = encoder.encode(
                std::as_bytes(std::span<const Posting>(result.postings)), 1);
            result.posting_encoding = encoded.encoding;
            if (encoded.encoding == pod_row_codec::Encoding::zstd) {
                result.encoded_postings.assign(encoded.bytes.begin(), encoded.bytes.end());
                std::vector<Posting>().swap(result.postings);
            }
            // Keep common-case workspaces reusable without retaining a rare
            // giant node's scratch/context in every worker for the whole run.
            encoder.release_buffers_over(std::size_t{4} << 20);
            std::sort(body_targets.begin(), body_targets.end());
            body_targets.erase(std::unique(body_targets.begin(), body_targets.end()), body_targets.end());
            // Retain capacity for unique targets, not every raw body occurrence.
            result.body_targets.assign(body_targets.begin(), body_targets.end());
        } else {
            result.hits = std::move(hits);
            result.scoring.reserve(scoring_rows.size());
            for (const auto& [class_id, visits] : scoring_rows) {
                result.scoring.push_back({class_id, visits.first, visits.second});
            }
        }
        return result;
    }

    const NodeEvidence& node_evidence(std::uint64_t id) const {
        WorkerCache& worker = cache();
        const auto existing = worker.nodes.find(id);
        if (existing != worker.nodes.end()) return *existing->second;
        auto value = node_cache.get_or_build(id, [&] { return build_node_evidence(id); });
        const std::size_t bytes = charged_sum(node_bytes(*value),
            sizeof(std::pair<const std::uint64_t, NodeLease>) + 4 * sizeof(void*));
        const auto [found, inserted] = worker.nodes.emplace(id, std::move(value));
        if (inserted) {
            try { worker.add_lease_bytes(bytes); }
            catch (...) { worker.nodes.erase(found); throw; }
        }
        return *found->second;
    }

    std::span<const Posting> node_posting_evidence(const NodeEvidence& evidence) const {
        if (query_mode == NativeQueryMode::full ||
            evidence.posting_encoding == pod_row_codec::Encoding::raw) return evidence.postings;
        WorkerCache& worker = cache();
        const auto existing = worker.node_postings.find(evidence.record.id);
        if (existing != worker.node_postings.end()) return existing->second;
        // Decode outside the shared cache lock. The immutable encoded value is
        // leased for this group; only a complete checked vector is published.
        auto value = pod_row_codec::decode<Posting>(evidence.encoded_postings,
            evidence.posting_encoding, evidence.record.count);
        const auto pair_count = index.pairs().size();
        const auto support_count = index.sets().size();
        std::uint32_t previous = 0;
        bool first = true;
        for (const auto& posting : value) {
            if (posting.model >= pair_count ||
                (!first && posting.model <= previous) ||
                posting.support_set >= support_count ||
                posting.flags == 0 ||
                (posting.flags & ~(kExonForward | kExonReverse | kBodyForward | kBodyReverse)) != 0) {
                throw std::runtime_error("native count decoded node posting is invalid");
            }
            previous = posting.model;
            first = false;
        }
        const auto bytes = charged_sum(buffer_bytes(value),
            sizeof(std::pair<const std::uint64_t, std::vector<Posting>>) + 4 * sizeof(void*));
        const auto [found, inserted] = worker.node_postings.emplace(evidence.record.id, std::move(value));
        if (inserted) {
            try { worker.add_lease_bytes(bytes); }
            catch (...) { worker.node_postings.erase(found); throw; }
        }
        return found->second;
    }

    std::span<const ScoringPosting> node_scoring_evidence(const NodeEvidence& evidence) const {
        if (query_mode == NativeQueryMode::full) {
            return {evidence.scoring.data(), evidence.scoring.size()};
        }
        WorkerCache& worker = cache();
        const auto existing = worker.node_scoring.find(evidence.record.id);
        if (existing != worker.node_scoring.end()) {
            return {existing->second.data(), existing->second.size()};
        }
        auto value = scoring_postings(node_hits(evidence.record.id));
        if (value.size() != evidence.record.score_count) {
            throw std::runtime_error("native count lazy scoring fanout differs from canonical node record");
        }
        const std::size_t bytes = charged_sum(buffer_bytes(value),
            sizeof(std::pair<const std::uint64_t, std::vector<ScoringPosting>>) + 4 * sizeof(void*));
        const auto [found, inserted] = worker.node_scoring.emplace(evidence.record.id, std::move(value));
        if (inserted) {
            try { worker.add_lease_bytes(bytes); }
            catch (...) { worker.node_scoring.erase(found); throw; }
        }
        return {found->second.data(), found->second.size()};
    }

    std::vector<EdgePosting> build_edge_postings(std::span<const PathHit> hits) const {
        edge_model_builds.fetch_add(1, std::memory_order_relaxed);
        std::map<std::uint32_t, EdgePosting> posting_rows;
        const auto pairs = index.pairs();
        for (const PathHit& hit : hits) {
            const auto& path_record = path(hit.row);
            for (const std::uint32_t model_id : index.model_ids(path_record)) {
                if (model_id >= pairs.size()) {
                    throw std::runtime_error("native count path model reference is invalid");
                }
                const auto& pair = pairs[model_id];
                if (pair.exon != hit.row && pair.body != hit.row) {
                    throw std::runtime_error("native count inverse pair does not contain its path");
                }
                EdgePosting& posting = posting_rows[model_id];
                posting.model = model_id;
                if (pair.exon == hit.row) posting.flags |= kEdgeExon;
                if (pair.body == hit.row) posting.flags |= kEdgeOrdinaryBody;
            }
        }
        std::vector<EdgePosting> result;
        result.reserve(posting_rows.size());
        for (const auto& [model, posting] : posting_rows) {
            static_cast<void>(model);
            result.push_back(posting);
        }
        return result;
    }

    std::vector<PathHit> raw_edge_hits(std::uint64_t from, std::uint64_t to,
                                       EdgeCursor* cursor = nullptr) const {
        const std::array<gbwt::node_type, 2> query{
            static_cast<gbwt::node_type>(from), static_cast<gbwt::node_type>(to)};
        EdgeCursor local;
        EdgeCursor& value = cursor == nullptr ? local : *cursor;
        if (!value.initialized) {
            value.state = locate->find(query.begin(), query.end(), value.packed);
            value.initialized = true;
        }
        std::vector<gbwt::size_type> sequences = std::move(value.prefix_sequences);
        if (!value.state.empty()) sequences.reserve(value.state.size());
        for (; !value.state.empty() && value.visited < value.state.size(); ++value.visited) {
            const auto [sequence, ignored_offset] = locate->unpack(value.packed);
            static_cast<void>(ignored_offset);
            sequences.push_back(sequence);
            if (value.visited + 1 < value.state.size()) value.packed = locate->locateNext(value.packed);
        }
        record_query(value.state.empty() ? 0 : value.state.size(), false);
        std::sort(sequences.begin(), sequences.end());
        sequences.erase(std::unique(sequences.begin(), sequences.end()), sequences.end());
        std::vector<PathHit> result;
        // FastLocate::locate() returns one deduplicated sequence id per hit.
        for (const auto sequence : sequences) {
            if (sequence >= gbwt->sequences()) {
                throw std::runtime_error("native FastLocate returned an invalid sequence id");
            }
            const auto row = index.path_row(gbwt::Path::id(sequence));
            if (row == missing) continue;
            result.push_back({row, gbwt::Path::is_reverse(sequence)});
        }
        return result;
    }

    EdgeEvidence build_edge_evidence(std::uint64_t from, std::uint64_t to,
                                     EdgeCursor* cursor = nullptr) const {
        EdgeEvidence result;
        auto hits = raw_edge_hits(from, to, cursor);
        if (query_mode == NativeQueryMode::full) {
            result.hits = std::move(hits);
            result.postings = build_edge_postings(result.hits);
        } else {
            std::map<std::uint32_t, std::uint32_t> summaries;
            for (const PathHit& hit : hits) {
                const auto& path_record = path(hit.row);
                auto& flags = summaries[target(path_record)];
                if ((path_record.flags & native_count::exon_path) != 0) {
                    flags |= hit.reverse_sequence ? kJunctionReverseComplement : kJunctionAlongTranscript;
                }
                if ((path_record.flags & native_count::body_path) != 0) flags |= kEdgeSummaryBody;
            }
            result.summary.reserve(summaries.size());
            for (const auto& [transcript, flags] : summaries) result.summary.push_back({transcript, flags});
        }
        return result;
    }

    const EdgeEvidence& edge_evidence(std::uint64_t from, std::uint64_t to,
                                      EdgeCursor* cursor = nullptr) const {
        const EdgeKey key{from, to};
        WorkerCache& worker = cache();
        const auto existing = worker.edges.find(key);
        if (existing != worker.edges.end()) return *existing->second;
        auto value = edge_cache.get_or_build(key, [&] { return build_edge_evidence(from, to, cursor); });
        const std::size_t bytes = charged_sum(edge_bytes(*value),
            sizeof(std::pair<const EdgeKey, EdgeLease>) + 4 * sizeof(void*));
        const auto [found, inserted] = worker.edges.emplace(key, std::move(value));
        if (inserted) {
            try { worker.add_lease_bytes(bytes); }
            catch (...) { worker.edges.erase(found); throw; }
        }
        return *found->second;
    }

    bool probe_common_body_witnesses(std::uint64_t physical_a, std::uint64_t physical_b,
                                     std::span<const std::uint32_t> common_targets,
                                     std::unordered_map<EdgeKey, EdgeCursor, EdgeHash>& cursors) const {
        if (common_targets.empty()) {
            splice_probe_common_empty.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
        if (max_probe_sa_steps == 0 || common_targets.size() > 4096) {
            splice_probe_budget_fallback.fetch_add(1, std::memory_order_relaxed);
            return false;
        }
        const std::size_t budget = std::min<std::size_t>(4096,
            std::max<std::size_t>(256, common_targets.size() * 4));
        const std::size_t limit = std::min(budget, max_probe_sa_steps);
        std::unordered_set<std::uint32_t> wanted(common_targets.begin(), common_targets.end());
        splice_probe_attempts.fetch_add(1, std::memory_order_relaxed);
        std::size_t steps = 0;
        for (const bool from_reverse : {false, true}) for (const bool to_reverse : {false, true}) {
            const std::array<gbwt::node_type, 2> query{
                static_cast<gbwt::node_type>(oriented(physical_a, from_reverse)),
                static_cast<gbwt::node_type>(oriented(physical_b, to_reverse))};
            const EdgeKey key{static_cast<std::uint64_t>(query[0]), static_cast<std::uint64_t>(query[1])};
            auto& cursor = cursors[key];
            cursor.state = locate->find(query.begin(), query.end(), cursor.packed);
            cursor.initialized = true;
            for (gbwt::size_type occurrence = 0; !cursor.state.empty() && occurrence < cursor.state.size(); ++occurrence) {
                if (steps == limit) {
                    splice_probe_sa_steps.fetch_add(steps, std::memory_order_relaxed);
                    splice_probe_budget_fallback.fetch_add(1, std::memory_order_relaxed);
                    return false;
                }
                const auto [sequence, ignored_offset] = locate->unpack(cursor.packed);
                static_cast<void>(ignored_offset);
                if (sequence >= gbwt->sequences()) throw std::runtime_error("native FastLocate returned an invalid sequence id");
                const auto row = index.path_row(gbwt::Path::id(sequence));
                if (row != missing) {
                    const auto& path_record = path(row);
                    if ((path_record.flags & native_count::body_path) != 0) wanted.erase(target(path_record));
                }
                cursor.prefix_sequences.push_back(sequence);
                ++cursor.visited;
                ++steps;
                if (wanted.empty()) {
                    splice_probe_sa_steps.fetch_add(steps, std::memory_order_relaxed);
                    splice_probe_success.fetch_add(1, std::memory_order_relaxed);
                    return true;
                }
                if (occurrence + 1 < cursor.state.size()) cursor.packed = locate->locateNext(cursor.packed);
            }
        }
        splice_probe_sa_steps.fetch_add(steps, std::memory_order_relaxed);
        splice_probe_exhausted_without_proof.fetch_add(1, std::memory_order_relaxed);
        return false;
    }

    bool universal_adjacency(std::uint64_t a, std::uint64_t b) const {
        for (const auto& [from, to] : {std::pair{a, b}, std::pair{b, a}})
            for (const bool reverse : {false, true}) {
                const auto node = static_cast<gbwt::node_type>(oriented(from, reverse));
                if (!gbwt->contains(node)) continue;
                const auto record = gbwt->record(node);
                if (!record.outgoing.empty() &&
                    std::all_of(record.outgoing.begin(), record.outgoing.end(), [&](const auto& edge) {
                        return edge.first != gbwt::ENDMARKER && gbwt::Node::id(edge.first) == to;
                    })) return true;
            }
        return false;
    }

    SpliceEvidence build_splice_evidence(std::uint64_t physical_a, std::uint64_t physical_b) const {
        SpliceEvidence result;
        if (physical_a == physical_b) return result;
        const auto& a = node_evidence(physical_a);
        const auto& b = node_evidence(physical_b);
        if (query_mode == NativeQueryMode::node_score && enable_adjacency_certificate) {
            splice_adjacency_checks.fetch_add(1, std::memory_order_relaxed);
            // Bidirectionality represents every physical occurrence at either
            // orientation. Universal adjacency supplies an ordinary body-edge
            // witness for every common body target, so E intersect C minus B
            // is empty. Both endpoint validations above remain mandatory.
            if (universal_adjacency(physical_a, physical_b)) {
                splice_adjacency_success.fetch_add(1, std::memory_order_relaxed);
                return result;
            }
        }
        std::vector<std::uint32_t> common_targets;
        if (query_mode == NativeQueryMode::node_score) {
            std::set_intersection(a.body_targets.begin(), a.body_targets.end(),
                                  b.body_targets.begin(), b.body_targets.end(),
                                  std::back_inserter(common_targets));
        } else {
            std::map<std::uint32_t, std::uint8_t> body_membership;
            const auto add_body_membership = [&](const NodeEvidence& node, std::uint8_t bit) {
                std::unordered_set<std::uint32_t> unique_paths;
                unique_paths.reserve(node.hits.size());
                for (const PathHit& hit : node.hits) {
                    if (!unique_paths.insert(hit.row).second) continue;
                    const auto& path_record = path(hit.row);
                    if ((path_record.flags & native_count::body_path) != 0) {
                        body_membership[target(path_record)] |= bit;
                    }
                }
            };
            add_body_membership(a, 1);
            add_body_membership(b, 2);
            for (const auto& [transcript, bits] : body_membership) {
                if (bits == 3) common_targets.push_back(transcript);
            }
        }
        std::unordered_map<EdgeKey, EdgeCursor, EdgeHash> probe_cursors;
        if (query_mode == NativeQueryMode::node_score &&
            probe_common_body_witnesses(physical_a, physical_b, common_targets, probe_cursors)) return result;

        std::unordered_set<std::uint32_t> exonic_adjacency;
        std::unordered_set<std::uint32_t> body_adjacency;
        for (const bool from_reverse : {false, true}) {
            for (const bool to_reverse : {false, true}) {
                const EdgeKey key{oriented(physical_a, from_reverse), oriented(physical_b, to_reverse)};
                auto cursor = probe_cursors.find(key);
                const auto& edge = edge_evidence(key.from, key.to,
                    cursor == probe_cursors.end() ? nullptr : &cursor->second);
                if (query_mode == NativeQueryMode::node_score) {
                    for (const auto& summary : edge.summary) {
                        if ((summary.flags & kEdgeSummaryExonMask) != 0)
                            exonic_adjacency.insert(summary.transcript);
                        if ((summary.flags & kEdgeSummaryBody) != 0)
                            body_adjacency.insert(summary.transcript);
                    }
                } else {
                    for (const PathHit& hit : edge.hits) {
                        const auto& path_record = path(hit.row);
                        const auto transcript = target(path_record);
                        if ((path_record.flags & native_count::exon_path) != 0) {
                            exonic_adjacency.insert(transcript);
                        }
                        if ((path_record.flags & native_count::body_path) != 0) {
                            body_adjacency.insert(transcript);
                        }
                    }
                }
            }
        }
        for (const auto transcript : exonic_adjacency) {
            if (std::binary_search(common_targets.begin(), common_targets.end(), transcript) &&
                body_adjacency.count(transcript) == 0) {
                result.targets.push_back(transcript);
            }
        }
        std::sort(result.targets.begin(), result.targets.end());
        return result;
    }

    const SpliceEvidence& splice_evidence(std::uint64_t physical_a, std::uint64_t physical_b) const {
        if (physical_b < physical_a) std::swap(physical_a, physical_b);
        const EdgeKey key{physical_a, physical_b};
        WorkerCache& worker = cache();
        const auto existing = worker.splices.find(key);
        if (existing != worker.splices.end()) return *existing->second;
        // The splice builder may acquire node then edge cache shards, but those
        // builders never call back into splice construction.
        auto value = splice_cache.get_or_build(
            key, [&] { return build_splice_evidence(physical_a, physical_b); });
        const std::size_t bytes = charged_sum(splice_bytes(*value),
            sizeof(std::pair<const EdgeKey, SpliceLease>) + 4 * sizeof(void*));
        const auto [found, inserted] = worker.splices.emplace(key, std::move(value));
        if (inserted) {
            try { worker.add_lease_bytes(bytes); }
            catch (...) { worker.splices.erase(found); throw; }
        }
        return *found->second;
    }

    std::vector<JunctionOwner>& junction_evidence(std::uint64_t from, std::uint64_t to) const {
        const EdgeKey key{from, to};
        WorkerCache& worker = cache();
        const auto existing = worker.junctions.find(key);
        if (existing != worker.junctions.end()) return existing->second;
        auto [inserted, fresh] = worker.junctions.try_emplace(key);
        if (!fresh) return inserted->second;
        auto& result = inserted->second;
        const auto& splice = splice_evidence(from >> 1, to >> 1);
        if (splice.targets.empty()) return result;
        const std::unordered_set<std::uint32_t> targets(splice.targets.begin(), splice.targets.end());
        std::map<std::uint32_t, std::uint32_t> owners;
        const auto& edge = edge_evidence(from, to);
        // GBWT forward/reverse follows the RNA path; genomic strand is already
        // represented there and must not be applied a second time.
        if (query_mode == NativeQueryMode::node_score) {
            for (const auto& summary : edge.summary) {
                const auto directions = summary.flags & kEdgeSummaryExonMask;
                if (directions != 0 && targets.count(summary.transcript) != 0)
                    owners[summary.transcript] |= directions;
            }
        } else {
            for (const PathHit& hit : edge.hits) {
                const auto& path_record = path(hit.row);
                if ((path_record.flags & native_count::exon_path) == 0) continue;
                const auto transcript = target(path_record);
                if (targets.count(transcript) == 0) continue;
                owners[transcript] |= hit.reverse_sequence ? kJunctionReverseComplement
                                                            : kJunctionAlongTranscript;
            }
        }
        result.reserve(owners.size());
        for (const auto& [transcript, flags] : owners) {
            result.push_back({transcript, flags});
        }
        return result;
    }
};

thread_local std::unordered_map<std::uint64_t, NativeSource::Impl::WorkerCache>
    NativeSource::Impl::worker_caches;

NativeSource::NativeSource(std::unique_ptr<Impl> impl) : impl_(std::move(impl)) {}
NativeSource::~NativeSource() {
    Impl::worker_caches.erase(impl_->cache_token);
}

std::unique_ptr<NativeSource> NativeSource::load(const std::filesystem::path& index_path,
                                                 const std::filesystem::path& xg_path,
                                                 const std::filesystem::path& gbwt_path,
                                                 const std::filesystem::path& r_index_path,
                                                 std::size_t threads,
                                                 NativeQueryMode mode,
                                                 std::size_t max_probe_sa_steps,
                                                 bool enable_adjacency_certificate) {
    auto impl = std::make_unique<Impl>();
    impl->query_mode = mode;
    impl->max_probe_sa_steps = max_probe_sa_steps;
    impl->enable_adjacency_certificate = enable_adjacency_certificate;
    const auto timed_load = [](const char* branch, auto&& loader) {
        const auto started = std::chrono::steady_clock::now();
        std::osyncstream(std::cerr) << "panCollapse: initialization: phase=native_count_load"
                                    << " branch=" << branch << " state=begin\n";
        try {
            auto result = std::forward<decltype(loader)>(loader)();
            std::osyncstream(std::cerr)
                << "panCollapse: initialization: phase=native_count_load branch=" << branch
                << " state=end seconds="
                << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()
                << '\n';
            return result;
        } catch (...) {
            std::osyncstream(std::cerr)
                << "panCollapse: initialization: phase=native_count_load branch=" << branch
                << " state=end outcome=error seconds="
                << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()
                << '\n';
            throw;
        }
    };
    const auto load_index = [&]() {
        return timed_load("index", [&]() { return native_count::Index::load(index_path); });
    };
    const auto load_xg = [&]() {
        return timed_load("xg", [&]() {
            auto graph = std::make_unique<xg::XG>();
            std::ifstream input(xg_path, std::ios::binary);
            if (!input) throw std::runtime_error("cannot open native count XG");
            graph->deserialize(input);
            return graph;
        });
    };
    const auto load_gbwt = [&]() {
        return timed_load("gbwt", [&]() {
            auto gbwt = std::make_unique<gbwt::GBWT>();
            std::ifstream input(gbwt_path, std::ios::binary);
            if (!input) throw std::runtime_error("cannot open native count GBWT");
            gbwt->load(input);
            return gbwt;
        });
    };
    const auto load_r_index = [&]() {
        return timed_load("r_index", [&]() {
            auto locate = std::make_unique<gbwt::FastLocate>();
            std::ifstream input(r_index_path, std::ios::binary);
            if (!input) throw std::runtime_error("cannot open native count r-index");
            locate->load(input);
            return locate;
        });
    };

    if (threads >= 4) {
        auto index_future = std::async(std::launch::async, load_index);
        auto xg_future = std::async(std::launch::async, load_xg);
        auto gbwt_future = std::async(std::launch::async, load_gbwt);
        auto r_index_future = std::async(std::launch::async, load_r_index);
        std::exception_ptr error;
        const auto collect = [&](auto& destination, auto& future) {
            try {
                destination.emplace(future.get());
            } catch (...) {
                if (!error) error = std::current_exception();
            }
        };
        std::optional<native_count::Index> index;
        std::optional<std::unique_ptr<xg::XG>> graph;
        std::optional<std::unique_ptr<gbwt::GBWT>> gbwt;
        std::optional<std::unique_ptr<gbwt::FastLocate>> locate;
        // Consume in this stable order so concurrent I/O failures report the
        // same principal error as the equivalent serial load.
        collect(index, index_future);
        collect(graph, xg_future);
        collect(gbwt, gbwt_future);
        collect(locate, r_index_future);
        if (error) std::rethrow_exception(error);
        impl->index = std::move(*index);
        impl->graph = std::move(*graph);
        impl->gbwt = std::move(*gbwt);
        impl->locate = std::move(*locate);
    } else {
        impl->index = load_index();
        impl->graph = load_xg();
        impl->gbwt = load_gbwt();
        impl->locate = load_r_index();
    }
    if (impl->index.gbwt_size_bytes() != 0 &&
        std::filesystem::file_size(gbwt_path) != impl->index.gbwt_size_bytes()) {
        throw std::runtime_error("native count index does not match GBWT byte size");
    }
    if (!impl->gbwt->bidirectional() || impl->gbwt->sequences() % 2 != 0 ||
        impl->index.path_count() != impl->gbwt->sequences() / 2) {
        throw std::runtime_error("native count GBWT does not provide matching bidirectional paths");
    }
    // Names were resolved into numeric path_lookup rows during indexing.
    // FastLocate uses the unchanged GBWT header/BWT, not this metadata.
    impl->gbwt->clearMetadata();
    impl->locate->setGBWT(*impl->gbwt);
    if (impl->locate->header.max_length == 0 ||
        impl->locate->comp_to_run.size() != impl->gbwt->effective() ||
        impl->locate->samples.empty() ||
        impl->locate->samples.size() != impl->locate->last_to_run.size() ||
        impl->locate->last.ones() != impl->locate->samples.size()) {
        throw std::runtime_error("native count r-index has invalid FastLocate structures");
    }
    impl->cache_token = next_cache_token.fetch_add(1, std::memory_order_relaxed);
    if (impl->cache_token == 0) {
        throw std::overflow_error("native count worker-cache token overflow");
    }
    return std::unique_ptr<NativeSource>(new NativeSource(std::move(impl)));
}

const std::vector<std::string>& NativeSource::strings() const { return impl_->index.strings(); }
const std::vector<std::vector<std::uint32_t>>& NativeSource::sets() const { return impl_->index.sets(); }
std::span<const count_reference::GeneRecord> NativeSource::genes() const { return impl_->index.genes(); }
std::span<const count_reference::PolicyRecord> NativeSource::policies() const { return impl_->index.policies(); }
const std::string& NativeSource::content_id() const { return impl_->index.content_id(); }

std::size_t NativeSource::model_count() const { return impl_->index.pairs().size(); }

ModelRecord NativeSource::model(std::uint32_t id) const {
    const auto pairs = impl_->index.pairs();
    if (id >= pairs.size()) throw std::out_of_range("invalid native count model");
    const auto& pair = pairs[id];
    const auto& exon = impl_->path(pair.exon);
    const auto& body = impl_->path(pair.body);
    if ((exon.flags & native_count::exon_path) == 0 ||
        (body.flags & native_count::body_path) == 0) {
        throw std::runtime_error("native count pair has invalid path roles");
    }
    return {exon.transcript_string, exon.policy, body.policy, exon.strand};
}

std::size_t NativeSource::scoring_class_count() const { return impl_->index.paths().size(); }

ScoringClassRecord NativeSource::scoring_class(std::uint32_t id) const {
    const auto& path_record = impl_->path(id);
    const std::uint32_t count = (path_record.flags & native_count::exon_path) != 0 ? 1 : 0;
    const std::uint32_t flags = (path_record.flags & native_count::body_path) != 0 ? 1 : 0;
    return {id, count, flags};
}

const NodeRecord* NativeSource::node(std::uint64_t id) const {
    return &impl_->node_evidence(id).record;
}

std::span<const Posting> NativeSource::node_postings(const NodeRecord& record) const {
    const auto& evidence = impl_->node_evidence(record.id);
    return impl_->node_posting_evidence(evidence);
}

std::span<const ScoringPosting> NativeSource::node_scoring(const NodeRecord& record) const {
    const auto& evidence = impl_->node_evidence(record.id);
    return impl_->node_scoring_evidence(evidence);
}

std::span<const ScoringBinding>
NativeSource::scoring_bindings(const ScoringClassRecord& record) const {
    if (record.first >= impl_->index.paths().size()) {
        throw std::out_of_range("invalid native count scoring class");
    }
    const auto path_row = static_cast<std::uint32_t>(record.first);
    const auto& path_record = impl_->path(path_row);
    if ((path_record.flags & native_count::exon_path) == 0) return {};
    auto& bindings = impl_->cache().bindings;
    const auto existing = bindings.find(path_row);
    if (existing != bindings.end()) return {existing->second.data(), existing->second.size()};
    auto [inserted, fresh] = bindings.try_emplace(path_row);
    static_cast<void>(fresh);
    auto& values = inserted->second;
    if (path_record.policy >= impl_->index.policies().size()) {
        throw std::runtime_error("native count path policy is invalid");
    }
    values.push_back({impl_->target(path_record), path_record.policy, 1, 0});
    return {values.data(), values.size()};
}

std::span<const EdgePosting> NativeSource::edge(std::uint64_t from, std::uint64_t to) const {
    const auto& evidence = impl_->edge_evidence(from, to);
    if (impl_->query_mode == NativeQueryMode::node_score) {
        const Impl::EdgeKey key{from, to};
        auto& postings = impl_->cache().edge_postings;
        const auto existing = postings.find(key);
        if (existing != postings.end()) return {existing->second.data(), existing->second.size()};
        auto value = impl_->build_edge_postings(impl_->raw_edge_hits(from, to));
        const std::size_t bytes = Impl::charged_sum(Impl::buffer_bytes(value),
            sizeof(std::pair<const Impl::EdgeKey, std::vector<EdgePosting>>) + 4 * sizeof(void*));
        const auto [found, inserted] = postings.emplace(key, std::move(value));
        if (inserted) {
            try { impl_->cache().add_lease_bytes(bytes); }
            catch (...) { postings.erase(found); throw; }
        }
        return {found->second.data(), found->second.size()};
    }
    return {evidence.postings.data(), evidence.postings.size()};
}

std::span<const std::uint32_t>
NativeSource::splice_targets(std::uint64_t physical_a, std::uint64_t physical_b) const {
    const auto& evidence = impl_->splice_evidence(physical_a, physical_b);
    return {evidence.targets.data(), evidence.targets.size()};
}

std::span<const JunctionOwner>
NativeSource::junction_owners(std::uint64_t from, std::uint64_t to) const {
    const auto& owners = impl_->junction_evidence(from, to);
    return {owners.data(), owners.size()};
}

void NativeSource::begin_read_group() const {
    const auto found = Impl::worker_caches.find(impl_->cache_token);
    if (found != Impl::worker_caches.end()) found->second.clear();
}

std::vector<std::string> NativeSource::take_feature_catalog() {
    return impl_->index.take_feature_catalog();
}

void NativeSource::configure_cache_limits(std::size_t node_bytes, std::size_t edge_bytes,
                                         std::size_t splice_bytes) {
    if (node_bytes == 0 || edge_bytes == 0 || splice_bytes == 0) {
        throw std::invalid_argument("native count cache limits must be positive");
    }
    if (impl_->native_queries.load(std::memory_order_relaxed) != 0 ||
        impl_->node_cache.build_count() != 0 || impl_->node_cache.hits() != 0 ||
        impl_->edge_cache.build_count() != 0 || impl_->edge_cache.hits() != 0 ||
        impl_->splice_cache.build_count() != 0 || impl_->splice_cache.hits() != 0) {
        throw std::logic_error("native count cache limits must be configured before queries");
    }
    impl_->node_cache.configure_capacity(node_bytes);
    impl_->edge_cache.configure_capacity(edge_bytes);
    impl_->splice_cache.configure_capacity(splice_bytes);
}

NativeQueryStats NativeSource::stats() const noexcept {
    NativeQueryStats result;
    result.native_queries = impl_->native_queries.load(std::memory_order_relaxed);
    result.native_occurrences = impl_->native_occurrences.load(std::memory_order_relaxed);
    result.native_max_occurrences = impl_->native_max_occurrences.load(std::memory_order_relaxed);
    result.node_queries = impl_->node_queries.load(std::memory_order_relaxed);
    result.edge_queries = impl_->edge_queries.load(std::memory_order_relaxed);
    result.edge_model_builds = impl_->edge_model_builds.load(std::memory_order_relaxed);
    result.max_node_occurrences = impl_->max_node_occurrences.load(std::memory_order_relaxed);
    result.max_node_models = impl_->max_node_models.load(std::memory_order_relaxed);
    result.splice_probe_common_empty = impl_->splice_probe_common_empty.load(std::memory_order_relaxed);
    result.splice_probe_attempts = impl_->splice_probe_attempts.load(std::memory_order_relaxed);
    result.splice_probe_sa_steps = impl_->splice_probe_sa_steps.load(std::memory_order_relaxed);
    result.splice_probe_success = impl_->splice_probe_success.load(std::memory_order_relaxed);
    result.splice_probe_budget_fallback = impl_->splice_probe_budget_fallback.load(std::memory_order_relaxed);
    result.splice_probe_exhausted_without_proof = impl_->splice_probe_exhausted_without_proof.load(std::memory_order_relaxed);
    result.splice_adjacency_checks = impl_->splice_adjacency_checks.load(std::memory_order_relaxed);
    result.splice_adjacency_success = impl_->splice_adjacency_success.load(std::memory_order_relaxed);
    const auto summarize = [](const auto& cache) {
        return NativeEvidenceCacheStats{
            cache.maximum_resident_bytes(), cache.resident_bytes(), cache.high_water_bytes(),
            cache.maximum_entry_bytes(), cache.entries(), cache.build_count(), cache.hits(),
            cache.evictions(), cache.uncached()};
    };
    result.node_cache = summarize(impl_->node_cache);
    result.edge_cache = summarize(impl_->edge_cache);
    result.splice_cache = summarize(impl_->splice_cache);
    result.active_lease_bytes = impl_->lease_accounting->active.load(std::memory_order_relaxed);
    result.active_lease_high_water_bytes =
        impl_->lease_accounting->high_water.load(std::memory_order_relaxed);
    return result;
}

}  // namespace pancollapse
