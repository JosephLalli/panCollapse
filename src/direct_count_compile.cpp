#include "direct_count_compile.hpp"

#include "direct_count_index.hpp"
#include "native_count_index.hpp"

#include <gbwt/fast_locate.h>
#include <gbwt/gbwt.h>
#include <xg.hpp>

#include <omp.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <exception>
#include <fstream>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include <unistd.h>

namespace pancollapse::direct_index {
namespace {

constexpr std::uint32_t exon_forward = 1;
constexpr std::uint32_t exon_reverse = 2;
constexpr std::uint32_t body_forward = 4;
constexpr std::uint32_t body_reverse = 8;
constexpr std::uint32_t along = 1;
constexpr std::uint32_t reverse_complement = 2;
constexpr std::uint32_t summary_body = 4;

static_assert(sizeof(ModelRecord) == 16);
static_assert(sizeof(Posting) == 12);

struct Hit {
    std::uint32_t row = 0;
    std::uint8_t orientations = 0;
};

struct Pair {
    std::uint64_t first = 0;
    std::uint64_t second = 0;

    bool operator<(const Pair& other) const {
        return std::tie(first, second) < std::tie(other.first, other.second);
    }
};

struct BuildBudget {
    std::size_t writer = 0;
    std::size_t candidates = 0;
    std::size_t worker_scratch = 0;
    std::size_t batch_headers = 0;
    std::size_t worker_batch = 0;
};

struct NodeResult {
    bool present = false;
    std::uint64_t id = 0;
    std::uint64_t length = 0;
    std::uint64_t raw_occurrences = 0;
    std::vector<Posting> postings;
};

struct SummaryRow {
    std::uint32_t transcript = 0;
    std::uint32_t flags = 0;
};

struct SummaryResult {
    std::uint64_t raw_occurrences = 0;
    std::vector<SummaryRow> rows;
};

struct EdgeResult {
    Pair pair;
    std::uint64_t raw_occurrences = 0;
    std::uint64_t queries = 0;
    std::vector<std::uint32_t> targets;
    std::array<std::vector<JunctionOwner>, 8> owners;
};

[[noreturn]] void bad(std::string_view message) {
    throw std::runtime_error("direct count compile: " + std::string(message));
}

std::size_t checked_add(std::size_t left, std::size_t right, std::string_view what) {
    if (right > std::numeric_limits<std::size_t>::max() - left) bad(what);
    return left + right;
}

std::size_t checked_multiply(std::size_t left, std::size_t right, std::string_view what) {
    if (left != 0 && right > std::numeric_limits<std::size_t>::max() / left) bad(what);
    return left * right;
}

void add_raw(std::uint64_t& total, std::uint64_t value) {
    if (value > std::numeric_limits<std::uint64_t>::max() - total) {
        bad("raw occurrence counter overflow");
    }
    total += value;
}

std::uint64_t oriented(std::uint64_t id, bool reverse) {
    if (id > (std::numeric_limits<std::uint64_t>::max() >> 1)) {
        bad("node id cannot encode orientation");
    }
    return (id << 1) | static_cast<std::uint64_t>(reverse);
}

std::size_t current_resident_bytes() {
    std::ifstream input("/proc/self/statm");
    std::uint64_t virtual_pages = 0;
    std::uint64_t resident_pages = 0;
    if (!(input >> virtual_pages >> resident_pages)) {
        bad("cannot read process resident memory for compiler budget");
    }
    static_cast<void>(virtual_pages);
    const long page_size = ::sysconf(_SC_PAGESIZE);
    if (page_size <= 0 ||
        resident_pages > std::numeric_limits<std::size_t>::max() /
            static_cast<std::size_t>(page_size)) {
        bad("process resident memory exceeds compiler width");
    }
    return static_cast<std::size_t>(resident_pages) * static_cast<std::size_t>(page_size);
}

std::size_t input_file_bytes(const std::filesystem::path& path) {
    const auto bytes = std::filesystem::file_size(path);
    const auto converted = static_cast<std::size_t>(bytes);
    if (static_cast<std::uintmax_t>(converted) != bytes) {
        bad("native input size exceeds compiler width");
    }
    return converted;
}

std::size_t remaining_work_budget(std::size_t total, std::size_t input_resident,
                                  std::size_t native_mapping_reserve,
                                  std::size_t safety_margin) {
    std::size_t reserved = input_resident;
    for (const auto bytes : {native_mapping_reserve, safety_margin}) {
        if (bytes > total || reserved > total - bytes) {
            bad("memory budget has no compiler headroom after loaded inputs and reserves");
        }
        reserved += bytes;
    }
    if (reserved >= total) {
        bad("memory budget has no compiler headroom after loaded inputs and reserves");
    }
    return total - reserved;
}

BuildBudget partition_budget(std::size_t total, std::size_t workers) {
    if (workers == 0 || workers > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        bad("thread count is outside the OpenMP range");
    }

    // The writer owns the dense node directory, list interning, metadata, and
    // derived-row bookkeeping while raw payloads stream to staging files. The
    // remaining quarter is divided between the global candidate vector,
    // per-worker query scratch, and ordered result waves. Each worker retains
    // at most one result, so header storage needs only one slot per worker; the
    // rest is split evenly so a result that fits scratch also fits retention.
    // These are independent ceilings; input indexes are immutable mapped/read
    // structures and are not silently dropped to satisfy this build budget.
    const std::size_t compiler = total / 4;
    BuildBudget result;
    result.writer = total - compiler;
    result.candidates = compiler / 3;
    const std::size_t scratch_and_batch = compiler - result.candidates;
    const std::size_t maximum_header_size = std::max(sizeof(NodeResult), sizeof(EdgeResult));
    const std::size_t preferred_header_bytes = checked_multiply(
        workers, maximum_header_size,
        "ordered-wave header byte count overflow");
    result.batch_headers = std::min(preferred_header_bytes, scratch_and_batch / 4);
    const std::size_t payload_and_scratch = scratch_and_batch - result.batch_headers;
    const std::size_t scratch_total = payload_and_scratch / 2;
    const std::size_t batch_payload_total = payload_and_scratch - scratch_total;
    result.worker_scratch = scratch_total / workers;
    result.worker_batch = batch_payload_total / workers;

    constexpr std::size_t minimum_scratch =
        sizeof(Hit) + sizeof(std::pair<const std::uint32_t, std::uint8_t>) +
        4 * sizeof(void*) + sizeof(Posting);
    if (result.writer == 0 || result.candidates < sizeof(Pair) ||
        result.worker_scratch < minimum_scratch ||
        result.batch_headers < maximum_header_size ||
        result.worker_batch < sizeof(Posting)) {
        bad("memory limit is too small for the requested compiler threads");
    }
    return result;
}

template<class T>
std::size_t vector_bytes(const std::vector<T>& values) {
    return checked_multiply(values.capacity(), sizeof(T), "vector byte count overflow");
}

template<class T>
void bounded_push(std::vector<T>& values, const T& value, std::size_t byte_limit,
                  std::string_view message) {
    const std::size_t item_limit = byte_limit / sizeof(T);
    if (values.size() == item_limit) bad(message);
    if (values.size() == values.capacity()) {
        std::size_t capacity = values.capacity() == 0 ? 1 : values.capacity();
        if (capacity < item_limit) {
            const std::size_t doubled = capacity > item_limit - capacity ? item_limit : capacity * 2;
            capacity = std::max(capacity + 1, doubled);
            capacity = std::min(capacity, item_limit);
        }
        values.reserve(capacity);
    }
    values.push_back(value);
}

struct Inputs {
    native_count::Index index;
    std::unique_ptr<xg::XG> graph;
    std::unique_ptr<gbwt::GBWT> gbwt;
    std::unique_ptr<gbwt::FastLocate> locate;
};

Inputs load(const BuildOptions& options) {
    const auto load_index = [&] { return native_count::Index::load(options.source_index); };
    const auto load_xg = [&] {
        auto result = std::make_unique<xg::XG>();
        std::ifstream input(options.xg, std::ios::binary);
        if (!input) bad("cannot open XG");
        result->deserialize(input);
        return result;
    };
    const auto load_gbwt = [&] {
        auto result = std::make_unique<gbwt::GBWT>();
        std::ifstream input(options.gbwt, std::ios::binary);
        if (!input) bad("cannot open GBWT");
        result->load(input);
        return result;
    };
    const auto load_ri = [&] {
        auto result = std::make_unique<gbwt::FastLocate>();
        std::ifstream input(options.r_index, std::ios::binary);
        if (!input) bad("cannot open r-index");
        result->load(input);
        return result;
    };

    Inputs result;
    if (options.threads >= 4) {
        auto index = std::async(std::launch::async, load_index);
        auto graph = std::async(std::launch::async, load_xg);
        auto gbwt = std::async(std::launch::async, load_gbwt);
        auto locate = std::async(std::launch::async, load_ri);
        result.index = index.get();
        result.graph = graph.get();
        result.gbwt = gbwt.get();
        result.locate = locate.get();
    } else {
        result.index = load_index();
        result.graph = load_xg();
        result.gbwt = load_gbwt();
        result.locate = load_ri();
    }

    if (result.index.gbwt_size_bytes() != 0 &&
        std::filesystem::file_size(options.gbwt) != result.index.gbwt_size_bytes()) {
        bad("native index does not match GBWT byte size");
    }
    if (!result.gbwt->bidirectional() || result.gbwt->sequences() % 2 != 0 ||
        result.index.path_count() != result.gbwt->sequences() / 2) {
        bad("GBWT paths do not match native index");
    }
    result.gbwt->clearMetadata();
    result.locate->setGBWT(*result.gbwt);
    if (result.locate->header.max_length == 0 ||
        result.locate->comp_to_run.size() != result.gbwt->effective() ||
        result.locate->samples.empty() ||
        result.locate->samples.size() != result.locate->last_to_run.size() ||
        result.locate->last.ones() != result.locate->samples.size()) {
        bad("invalid FastLocate structures");
    }
    return result;
}

std::vector<Hit> hits_for(const gbwt::FastLocate& locate, const gbwt::GBWT& gbwt,
                          const native_count::Index& index,
                          std::span<const gbwt::node_type> query,
                          std::size_t scratch_bytes, std::uint64_t& raw) {
    gbwt::size_type packed = gbwt::FastLocate::NO_POSITION;
    const auto state = locate.find(query.begin(), query.end(), packed);
    add_raw(raw, state.size());

    // A repeat-heavy range can be enormous while its selected parent set is
    // tiny. Walk occurrences through the r-index and retain only one row plus
    // orientation mask per selected path.
    constexpr std::size_t row_charge = sizeof(std::pair<const std::uint32_t, std::uint8_t>) +
        4 * sizeof(void*) + sizeof(Hit);
    const std::size_t row_limit = scratch_bytes / row_charge;
    std::map<std::uint32_t, std::uint8_t> rows;
    for (gbwt::size_type visit = 0; !state.empty() && visit < state.size(); ++visit) {
        const auto [sequence, unused] = locate.unpack(packed);
        static_cast<void>(unused);
        if (sequence >= gbwt.sequences()) bad("FastLocate returned invalid sequence");
        const auto row = index.path_row(gbwt::Path::id(sequence));
        if (row != count_reference::missing) {
            auto found = rows.find(row);
            if (found == rows.end()) {
                if (rows.size() == row_limit) {
                    bad("distinct selected paths exceed per-worker scratch budget");
                }
                found = rows.emplace(row, 0).first;
            }
            found->second |= gbwt::Path::is_reverse(sequence) ? 2 : 1;
        }
        if (visit + 1 < state.size()) packed = locate.locateNext(packed);
    }

    std::vector<Hit> hits;
    hits.reserve(rows.size());
    for (const auto [row, orientations] : rows) hits.push_back({row, orientations});
    return hits;
}

Metadata metadata(native_count::Index& index, std::string provenance) {
    const auto paths = index.paths();
    const auto pairs = index.pairs();

    // PCANBND1 permits an annotated path row without a model pair. Direct
    // facts do not: such a row could be observed at runtime but could not be
    // represented as ready model evidence.
    for (const auto& path : paths) {
        if (index.model_ids(path).empty()) bad("selected path has no model binding");
    }

    Metadata result;
    result.strings = index.strings();
    result.feature_catalog = index.take_feature_catalog();
    result.sets = index.sets();
    result.genes.assign(index.genes().begin(), index.genes().end());
    result.policies.assign(index.policies().begin(), index.policies().end());
    result.content_id = index.content_id();
    result.provenance_json = std::move(provenance);
    result.models.reserve(pairs.size());
    for (const auto& pair : pairs) {
        if (pair.exon >= paths.size() || pair.body >= paths.size()) {
            bad("pair path id is invalid");
        }
        const auto& exon = paths[pair.exon];
        const auto& body = paths[pair.body];
        if ((exon.flags & native_count::exon_path) == 0 ||
            (body.flags & native_count::body_path) == 0) {
            bad("pair path roles are invalid");
        }
        // Pair order is the stable model ID used by every node posting.
        result.models.push_back(
            {exon.transcript_string, exon.policy, body.policy, exon.strand});
    }
    return result;
}

std::vector<Posting> node_postings(const std::vector<Hit>& hits,
                                   const native_count::Index& index,
                                   std::size_t scratch_bytes) {
    const auto paths = index.paths();
    const auto pairs = index.pairs();
    std::size_t candidate_count = 0;
    for (const auto hit : hits) {
        if (hit.row >= paths.size()) bad("selected path row is invalid");
        const auto models = index.model_ids(paths[hit.row]);
        if (models.empty()) bad("selected path has no model binding");
        candidate_count = checked_add(candidate_count, models.size(),
                                      "node model fanout overflow");
    }
    if (candidate_count > scratch_bytes / sizeof(Posting)) {
        bad("node model fanout exceeds per-worker scratch budget");
    }

    std::vector<Posting> values;
    values.reserve(candidate_count);
    for (const auto hit : hits) {
        const auto& path = paths[hit.row];
        for (const auto model : index.model_ids(path)) {
            if (model >= pairs.size()) bad("model reference is invalid");
            const auto& pair = pairs[model];
            if (pair.exon != hit.row && pair.body != hit.row) {
                bad("inverse pair does not contain selected path");
            }
            Posting value;
            value.model = model;
            if (pair.exon == hit.row) {
                if (hit.orientations & 1) value.flags |= exon_forward;
                if (hit.orientations & 2) value.flags |= exon_reverse;
            }
            if (pair.body == hit.row) {
                value.support_set = paths[pair.body].support_set;
                if (hit.orientations & 1) value.flags |= body_forward;
                if (hit.orientations & 2) value.flags |= body_reverse;
            }
            values.push_back(value);
        }
    }

    std::sort(values.begin(), values.end(), [](const Posting& left, const Posting& right) {
        return left.model < right.model;
    });
    std::size_t output = 0;
    for (const auto value : values) {
        if (output == 0 || values[output - 1].model != value.model) {
            values[output++] = value;
            continue;
        }
        auto& prior = values[output - 1];
        if ((prior.flags & (body_forward | body_reverse)) != 0 &&
            (value.flags & (body_forward | body_reverse)) != 0 &&
            prior.support_set != value.support_set) {
            bad("conflicting body support");
        }
        if (value.flags & (body_forward | body_reverse)) prior.support_set = value.support_set;
        prior.flags |= value.flags;
    }
    values.resize(output);
    return values;
}

std::uint32_t body_transcript(std::uint32_t model, const native_count::Index& index) {
    const auto pairs = index.pairs();
    const auto paths = index.paths();
    if (model >= pairs.size() || pairs[model].body >= paths.size()) {
        bad("body model is invalid");
    }
    // C is transcript ownership of the body path, not the exon-side model
    // label. This matters even though well-formed production pairs normally
    // carry the same transcript on both sides.
    return paths[pairs[model].body].transcript_string;
}

std::vector<std::uint32_t> body_transcripts(count_reference::NodeView node,
                                            const native_count::Index& index,
                                            std::size_t byte_limit) {
    std::size_t count = 0;
    for (const auto posting : node.postings) {
        if (posting.flags & (body_forward | body_reverse)) ++count;
    }
    if (count > byte_limit / sizeof(std::uint32_t)) {
        bad("body target set exceeds per-worker scratch budget");
    }
    std::vector<std::uint32_t> result;
    result.reserve(count);
    for (const auto posting : node.postings) {
        if (posting.flags & (body_forward | body_reverse)) {
            result.push_back(body_transcript(posting.model, index));
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

bool universal_adjacency(const gbwt::GBWT& gbwt, std::uint64_t a, std::uint64_t b) {
    // This is the same sufficient certificate as NativeSource: in a
    // bidirectional GBWT, one orientation with only the other physical node as
    // outgoing supplies an ordinary body witness for every common body target.
    for (const auto [from, to] : {std::pair{a, b}, std::pair{b, a}}) {
        for (const bool reverse : {false, true}) {
            const auto node = static_cast<gbwt::node_type>(oriented(from, reverse));
            if (!gbwt.contains(node)) continue;
            const auto record = gbwt.record(node);
            if (!record.outgoing.empty() &&
                std::all_of(record.outgoing.begin(), record.outgoing.end(),
                            [&](const auto& edge) {
                                return edge.first != gbwt::ENDMARKER &&
                                    gbwt::Node::id(edge.first) == to;
                            })) {
                return true;
            }
        }
    }
    return false;
}

std::array<std::pair<std::uint64_t, std::uint64_t>, 8> directions(Pair pair) {
    std::array<std::pair<std::uint64_t, std::uint64_t>, 8> result{};
    std::size_t next = 0;
    for (const auto [from, to] : {std::pair{pair.first, pair.second},
                                  std::pair{pair.second, pair.first}}) {
        for (const bool from_reverse : {false, true}) {
            for (const bool to_reverse : {false, true}) {
                result[next++] = {oriented(from, from_reverse), oriented(to, to_reverse)};
            }
        }
    }
    return result;
}

SummaryResult edge_summary(const gbwt::FastLocate& locate, const gbwt::GBWT& gbwt,
                           const native_count::Index& index,
                           std::uint64_t from, std::uint64_t to,
                           std::size_t scratch_bytes) {
    SummaryResult result;
    const std::array<gbwt::node_type, 2> query{
        static_cast<gbwt::node_type>(from), static_cast<gbwt::node_type>(to)};
    auto hits = hits_for(locate, gbwt, index, query, scratch_bytes, result.raw_occurrences);

    constexpr std::size_t map_charge =
        sizeof(std::pair<const std::uint32_t, std::uint32_t>) + 4 * sizeof(void*);
    const std::size_t hit_bytes = vector_bytes(hits);
    if (hit_bytes > scratch_bytes) bad("edge hits exceed per-worker scratch budget");
    const std::size_t row_limit = (scratch_bytes - hit_bytes) /
        (map_charge + sizeof(SummaryRow));
    std::map<std::uint32_t, std::uint32_t> rows;
    const auto paths = index.paths();
    for (const auto hit : hits) {
        if (hit.row >= paths.size()) bad("selected path row is invalid");
        const auto& path = paths[hit.row];
        auto found = rows.find(path.transcript_string);
        if (found == rows.end()) {
            if (rows.size() == row_limit) {
                bad("edge summary exceeds per-worker scratch budget");
            }
            found = rows.emplace(path.transcript_string, 0).first;
        }
        if (path.flags & native_count::exon_path) {
            if (hit.orientations & 1) found->second |= along;
            if (hit.orientations & 2) found->second |= reverse_complement;
        }
        if (path.flags & native_count::body_path) found->second |= summary_body;
    }
    result.rows.reserve(rows.size());
    for (const auto [transcript, flags] : rows) result.rows.push_back({transcript, flags});
    return result;
}

NodeResult build_node(std::uint64_t id, const Inputs& inputs, std::size_t scratch_bytes) {
    NodeResult result;
    result.id = id;
    if (id > static_cast<std::uint64_t>(std::numeric_limits<handlegraph::nid_t>::max()) ||
        !inputs.graph->has_node(static_cast<handlegraph::nid_t>(id))) {
        return result;
    }
    result.present = true;
    const auto handle = inputs.graph->get_handle(static_cast<handlegraph::nid_t>(id), false);
    result.length = inputs.graph->get_length(handle);
    if (result.length > std::numeric_limits<std::uint32_t>::max()) {
        bad("node length exceeds direct-index width");
    }
    const std::array<gbwt::node_type, 1> query{
        static_cast<gbwt::node_type>(oriented(id, false))};
    auto hits = hits_for(*inputs.locate, *inputs.gbwt, inputs.index, query,
                         scratch_bytes, result.raw_occurrences);
    const std::size_t retained = vector_bytes(hits);
    if (retained > scratch_bytes) bad("node hits exceed per-worker scratch budget");
    result.postings = node_postings(hits, inputs.index, scratch_bytes - retained);
    return result;
}

EdgeResult build_edge(Pair pair, const Inputs& inputs, const Writer& writer,
                      std::size_t scratch_bytes, std::size_t batch_bytes) {
    EdgeResult result;
    result.pair = pair;
    const auto left = writer.node_view(pair.first);
    const auto right = writer.node_view(pair.second);
    if (!left || !right) bad("candidate endpoint was not written");

    // Keep four separately enforced scratch partitions. This prevents one
    // high-fanout pair from consuming every worker's ordered-wave allowance.
    const std::size_t body_budget = scratch_bytes / 4;
    const std::size_t query_budget = scratch_bytes / 4;
    const std::size_t summary_budget = scratch_bytes / 4;
    const std::size_t aggregate_budget = scratch_bytes - body_budget - query_budget - summary_budget;
    auto left_body = body_transcripts(*left, inputs.index, body_budget / 3);
    auto right_body = body_transcripts(*right, inputs.index, body_budget / 3);
    const std::size_t used_body = checked_add(vector_bytes(left_body), vector_bytes(right_body),
                                              "body target byte count overflow");
    if (used_body > body_budget) bad("body target sets exceed per-worker scratch budget");
    const std::size_t common_limit = body_budget - used_body;
    std::vector<std::uint32_t> common;
    const auto common_capacity = std::min(left_body.size(), right_body.size());
    if (common_capacity > common_limit / sizeof(std::uint32_t)) {
        bad("common body targets exceed per-worker scratch budget");
    }
    common.reserve(common_capacity);
    std::set_intersection(left_body.begin(), left_body.end(), right_body.begin(), right_body.end(),
                          std::back_inserter(common));
    if (vector_bytes(common) > common_limit) {
        bad("common body targets exceed per-worker scratch budget");
    }
    if (common.empty() || universal_adjacency(*inputs.gbwt, pair.first, pair.second)) {
        return result;
    }

    const auto oriented_edges = directions(pair);
    std::array<std::vector<SummaryRow>, 8> summaries;
    std::size_t summary_bytes = 0;
    for (std::size_t direction = 0; direction < oriented_edges.size(); ++direction) {
        const auto [from, to] = oriented_edges[direction];
        auto summary = edge_summary(*inputs.locate, *inputs.gbwt, inputs.index,
                                    from, to, query_budget);
        add_raw(result.raw_occurrences, summary.raw_occurrences);
        ++result.queries;
        const auto bytes = vector_bytes(summary.rows);
        if (bytes > summary_budget - summary_bytes) {
            bad("eight-direction summaries exceed per-worker scratch budget");
        }
        summary_bytes += bytes;
        summaries[direction] = std::move(summary.rows);
    }

    std::size_t exon_count = 0;
    std::size_t body_count = 0;
    for (const auto& summary : summaries) {
        for (const auto row : summary) {
            exon_count += (row.flags & (along | reverse_complement)) != 0;
            body_count += (row.flags & summary_body) != 0;
        }
    }
    const std::size_t aggregate_count = checked_add(exon_count, body_count,
                                                    "edge aggregate count overflow");
    if (aggregate_count > aggregate_budget / sizeof(std::uint32_t)) {
        bad("edge aggregates exceed per-worker scratch budget");
    }
    std::vector<std::uint32_t> exonic;
    std::vector<std::uint32_t> ordinary;
    exonic.reserve(exon_count);
    ordinary.reserve(body_count);
    for (const auto& summary : summaries) {
        for (const auto row : summary) {
            if (row.flags & (along | reverse_complement)) exonic.push_back(row.transcript);
            if (row.flags & summary_body) ordinary.push_back(row.transcript);
        }
    }
    std::sort(exonic.begin(), exonic.end());
    exonic.erase(std::unique(exonic.begin(), exonic.end()), exonic.end());
    std::sort(ordinary.begin(), ordinary.end());
    ordinary.erase(std::unique(ordinary.begin(), ordinary.end()), ordinary.end());

    const auto target_capacity = std::min(common.size(), exonic.size());
    if (target_capacity > batch_bytes / sizeof(std::uint32_t)) {
        bad("edge targets exceed per-worker ordered-wave budget");
    }
    result.targets.reserve(target_capacity);
    for (const auto transcript : exonic) {
        if (std::binary_search(common.begin(), common.end(), transcript) &&
            !std::binary_search(ordinary.begin(), ordinary.end(), transcript)) {
            result.targets.push_back(transcript);
        }
    }

    std::array<std::size_t, 8> owner_counts{};
    std::size_t owner_count = 0;
    for (std::size_t direction = 0; direction < summaries.size(); ++direction) {
        for (const auto row : summaries[direction]) {
            if ((row.flags & (along | reverse_complement)) != 0 &&
                std::binary_search(result.targets.begin(), result.targets.end(), row.transcript)) {
                ++owner_counts[direction];
                ++owner_count;
            }
        }
    }
    const std::size_t output_bytes = checked_add(
        checked_multiply(result.targets.capacity(), sizeof(std::uint32_t),
                         "edge target byte count overflow"),
        checked_multiply(owner_count, sizeof(JunctionOwner),
                         "edge owner byte count overflow"),
        "edge result byte count overflow");
    if (output_bytes > batch_bytes) {
        bad("edge result exceeds per-worker ordered-wave budget");
    }
    for (std::size_t direction = 0; direction < summaries.size(); ++direction) {
        result.owners[direction].reserve(owner_counts[direction]);
        for (const auto row : summaries[direction]) {
            const auto owner_flags = row.flags & (along | reverse_complement);
            if (owner_flags != 0 &&
                std::binary_search(result.targets.begin(), result.targets.end(), row.transcript)) {
                result.owners[direction].push_back({row.transcript, owner_flags});
            }
        }
    }
    return result;
}

std::size_t payload_bytes(const NodeResult& result) {
    return vector_bytes(result.postings);
}

std::size_t payload_bytes(const EdgeResult& result) {
    std::size_t bytes = vector_bytes(result.targets);
    for (const auto& owners : result.owners) {
        bytes = checked_add(bytes, vector_bytes(owners), "edge batch byte count overflow");
    }
    return bytes;
}

void capture_parallel_error(std::exception_ptr candidate, std::exception_ptr& error,
                            std::atomic<bool>& failed) {
#pragma omp critical(direct_count_compile_error)
    {
        if (!error) error = std::move(candidate);
    }
    failed.store(true, std::memory_order_release);
}

template<class Result, class Compute, class Commit>
std::size_t ordered_batches(std::size_t total, std::size_t workers,
                            std::size_t header_budget, std::size_t worker_batch_budget,
                            Compute&& compute, Commit&& commit) {
    if (total == 0) return 0;
    const std::size_t header_limit = header_budget / sizeof(Result);
    if (header_limit == 0) bad("ordered-wave header budget is empty");
    const std::size_t maximum_width = std::min(header_limit, workers);
    if (maximum_width == 0) bad("ordered-wave width is zero");

    std::vector<Result> results;
    results.reserve(maximum_width);
    std::exception_ptr error;
    std::atomic<bool> failed{false};
    std::size_t next = 0;
    std::size_t first = 0;
    std::size_t count = 0;
    std::size_t waves = 0;
    bool stop = false;

#pragma omp parallel num_threads(static_cast<int>(workers)) shared(results, error, failed, next, first, count, waves, stop)
    {
        for (;;) {
#pragma omp single
            {
                try {
                    if (failed.load(std::memory_order_acquire) || next == total) {
                        stop = true;
                        count = 0;
                    } else {
                        stop = false;
                        first = next;
                        count = std::min({maximum_width,
                                          static_cast<std::size_t>(omp_get_num_threads()),
                                          total - next});
                        // Destroy nested payloads from the preceding batch
                        // before workers allocate the next one.
                        results.clear();
                        results.resize(count);
                    }
                } catch (...) {
                    capture_parallel_error(std::current_exception(), error, failed);
                    stop = true;
                    count = 0;
                }
            }
            if (stop) break;

            // count never exceeds the actual team size, so static scheduling
            // gives every worker at most one completed, retained result.
#pragma omp for schedule(static, 1)
            for (std::size_t offset = 0; offset < count; ++offset) {
                if (failed.load(std::memory_order_acquire)) continue;
                try {
                    auto value = compute(first + offset);
                    const auto bytes = payload_bytes(value);
                    if (bytes > worker_batch_budget) {
                        bad("one ordered-wave result exceeds memory budget");
                    }
                    results[offset] = std::move(value);
                } catch (...) {
                    capture_parallel_error(std::current_exception(), error, failed);
                }
            }

#pragma omp single
            {
                if (!failed.load(std::memory_order_acquire)) {
                    try {
                        for (std::size_t offset = 0; offset < count; ++offset) {
                            commit(first + offset, results[offset]);
                        }
                        next += count;
                        ++waves;
                    } catch (...) {
                        capture_parallel_error(std::current_exception(), error, failed);
                    }
                }
            }
        }
    }
    if (error) std::rethrow_exception(error);
    return waves;
}

}  // namespace

void build(const BuildOptions& options) {
    if (options.threads == 0 || options.memory_limit_bytes == 0) {
        bad("positive threads and memory limit are required");
    }
    if (options.threads > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        bad("thread count is outside the OpenMP range");
    }
    const auto began = std::chrono::steady_clock::now();
    const auto load_began = std::chrono::steady_clock::now();
    std::cerr << "panCollapse: direct count indexing: phase=load state=begin\n";
    auto inputs = load(options);
    // XG, GBWT, and RI are deserialized before this measurement. PCANBND1 is
    // mapped, so reserve its full file size as well; this deliberately
    // double-counts already-resident binding pages instead of relying on them
    // to remain cold under the external cgroup hard cap.
    const std::size_t input_resident = current_resident_bytes();
    const std::size_t native_mapping_reserve = input_file_bytes(options.source_index);
    const std::size_t safety_margin = options.memory_limit_bytes / 10;
    const std::size_t work_limit = remaining_work_budget(
        options.memory_limit_bytes, input_resident, native_mapping_reserve, safety_margin);
    const auto budget = partition_budget(work_limit, options.threads);
    const std::size_t node_payload_limit =
        options.testing_node_payload_limit_bytes == 0
        ? budget.worker_batch
        : std::min(budget.worker_batch, options.testing_node_payload_limit_bytes);
    if (node_payload_limit < sizeof(Posting)) {
        bad("ordered-wave node payload limit is too small");
    }
    auto md = metadata(inputs.index, options.provenance_json);
    const double load_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - load_began).count();
    std::cerr << "panCollapse: direct count indexing: phase=load state=end seconds="
              << load_seconds << "\n"
              << "panCollapse: direct count indexing: phase=budget"
              << " total_bytes=" << options.memory_limit_bytes
              << " input_resident_bytes=" << input_resident
              << " native_mapping_reserve_bytes=" << native_mapping_reserve
              << " safety_margin_bytes=" << safety_margin
              << " work_budget_bytes=" << work_limit
              << " writer_budget_bytes=" << budget.writer
              << " candidate_budget_bytes=" << budget.candidates
              << " worker_scratch_bytes=" << budget.worker_scratch
              << " batch_header_budget_bytes=" << budget.batch_headers
              << " worker_batch_bytes=" << budget.worker_batch
              << " node_payload_limit_bytes=" << node_payload_limit << '\n';

    const auto node_began = std::chrono::steady_clock::now();
    std::cerr << "panCollapse: direct count indexing: phase=nodes state=begin\n";
    std::uint64_t max_node = 0;
    std::uint64_t node_count = 0;
    inputs.graph->for_each_handle([&](const handlegraph::handle_t& handle) {
        max_node = std::max<std::uint64_t>(max_node, inputs.graph->get_id(handle));
        ++node_count;
        return true;
    });
    if (max_node >= std::numeric_limits<std::size_t>::max()) {
        bad("dense node id range exceeds addressable compiler width");
    }

    Writer writer(options.output, std::move(md), max_node, budget.writer);
    std::uint64_t raw_occurrences = 0;
    std::uint64_t written_nodes = 0;
    const std::size_t dense_node_count = static_cast<std::size_t>(max_node) + 1;
    const std::size_t node_waves = ordered_batches<NodeResult>(
        dense_node_count, options.threads, budget.batch_headers, node_payload_limit,
        [&](std::size_t dense_id) {
            return build_node(static_cast<std::uint64_t>(dense_id), inputs,
                              budget.worker_scratch);
        },
        [&](std::size_t dense_id, const NodeResult& result) {
            if (result.present) {
                if (result.id != dense_id) bad("ordered node result id mismatch");
                writer.append_node(result.id, result.length, result.postings);
                add_raw(raw_occurrences, result.raw_occurrences);
                ++written_nodes;
            }
            const auto completed = dense_id + 1;
            if (completed % 1000000 == 0 || completed == dense_node_count) {
                std::cerr << "panCollapse: direct count indexing: phase=nodes"
                          << " completed_id_slots=" << completed << '/' << dense_node_count
                          << " written_nodes=" << written_nodes << '\n';
            }
        });
    writer.seal_nodes();
    if (written_nodes != node_count) bad("XG node count changed during compilation");
    const double node_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - node_began).count();
    std::cerr << "panCollapse: direct count indexing: phase=nodes state=end seconds="
              << node_seconds << " written_nodes=" << written_nodes
              << " ordered_waves=" << node_waves << '\n';

    // Physical candidate discovery retains only numeric endpoint pairs. It
    // never materializes graph node IDs or GBWT path steps globally.
    const auto edge_began = std::chrono::steady_clock::now();
    std::cerr << "panCollapse: direct count indexing: phase=edges state=begin\n";
    std::vector<Pair> candidates;
    std::uint64_t candidate_nodes_scanned = 0;
    inputs.graph->for_each_handle([&](const handlegraph::handle_t& handle) {
        const auto id = static_cast<std::uint64_t>(inputs.graph->get_id(handle));
        for (const bool reverse : {false, true}) {
            const auto node = static_cast<gbwt::node_type>(oriented(id, reverse));
            if (!inputs.gbwt->contains(node)) continue;
            for (const auto& edge : inputs.gbwt->record(node).outgoing) {
                if (edge.first == gbwt::ENDMARKER) continue;
                const auto other = gbwt::Node::id(edge.first);
                if (other == id) continue;
                bounded_push(candidates,
                             {std::min<std::uint64_t>(id, other),
                              std::max<std::uint64_t>(id, other)},
                             budget.candidates,
                             "candidate edge set exceeds compiler memory budget");
            }
        }
        ++candidate_nodes_scanned;
        if (candidate_nodes_scanned % 1000000 == 0) {
            std::cerr << "panCollapse: direct count indexing: phase=edge_candidates"
                      << " scanned_nodes=" << candidate_nodes_scanned << '/' << node_count
                      << " raw_pairs=" << candidates.size() << '\n';
        }
        return true;
    });
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end(),
                                 [](const Pair& left, const Pair& right) {
                                     return left.first == right.first &&
                                         left.second == right.second;
                                 }),
                     candidates.end());
    std::cerr << "panCollapse: direct count indexing: phase=edge_candidates state=end"
              << " scanned_nodes=" << candidate_nodes_scanned
              << " candidate_pairs=" << candidates.size() << '\n';

    std::uint64_t edge_queries = 0;
    const std::size_t edge_waves = ordered_batches<EdgeResult>(
        candidates.size(), options.threads, budget.batch_headers, budget.worker_batch,
        [&](std::size_t offset) {
            return build_edge(candidates[offset], inputs, writer,
                              budget.worker_scratch, budget.worker_batch);
        },
        [&](std::size_t offset, const EdgeResult& result) {
            const auto pair = candidates[offset];
            if (result.pair.first != pair.first || result.pair.second != pair.second) {
                bad("ordered edge result id mismatch");
            }
            add_raw(raw_occurrences, result.raw_occurrences);
            add_raw(edge_queries, result.queries);
            if (!result.targets.empty()) {
                writer.append_splice(pair.first, pair.second, result.targets);
            }
            const auto oriented_edges = directions(pair);
            for (std::size_t direction = 0; direction < oriented_edges.size(); ++direction) {
                if (result.owners[direction].empty()) continue;
                writer.append_junction(oriented_edges[direction].first,
                                       oriented_edges[direction].second,
                                       result.owners[direction]);
            }
            const auto completed = offset + 1;
            if (completed % 1000000 == 0 || completed == candidates.size()) {
                std::cerr << "panCollapse: direct count indexing: phase=edges"
                          << " completed_pairs=" << completed << '/' << candidates.size()
                          << " edge_queries=" << edge_queries << '\n';
            }
        });

    const double edge_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - edge_began).count();
    std::cerr << "panCollapse: direct count indexing: phase=edges state=end seconds="
              << edge_seconds << " candidate_pairs=" << candidates.size()
              << " ordered_waves=" << edge_waves << '\n';
    const auto write_began = std::chrono::steady_clock::now();
    std::cerr << "panCollapse: direct count indexing: phase=write state=begin\n";
    writer.finish();
    const auto storage = writer.stats();
    const double write_seconds = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - write_began).count();
    std::cerr << "panCollapse: direct count indexing: phase=write state=end seconds="
              << write_seconds << " index_bytes=" << storage.index_bytes << '\n';
    std::cerr << "{\"direct_count_compile\":{\"nodes\":" << storage.nodes
              << ",\"unique_lists\":" << storage.unique_lists
              << ",\"stored_postings\":" << storage.stored_postings
              << ",\"models\":" << storage.models
              << ",\"splice_pairs\":" << storage.splice_pairs
              << ",\"splice_targets\":" << storage.splice_targets
              << ",\"junction_rows\":" << storage.junction_rows
              << ",\"junction_owners\":" << storage.junction_owners
              << ",\"index_bytes\":" << storage.index_bytes
              << ",\"candidate_edges\":" << candidates.size()
              << ",\"edge_queries\":" << edge_queries
              << ",\"raw_occurrences\":" << raw_occurrences
              << ",\"node_ordered_waves\":" << node_waves
              << ",\"edge_ordered_waves\":" << edge_waves
              << ",\"memory_budget_bytes\":" << options.memory_limit_bytes
              << ",\"input_resident_bytes\":" << input_resident
              << ",\"native_mapping_reserve_bytes\":" << native_mapping_reserve
              << ",\"safety_margin_bytes\":" << safety_margin
              << ",\"work_budget_bytes\":" << work_limit
              << ",\"load_seconds\":" << load_seconds
              << ",\"node_seconds\":" << node_seconds
              << ",\"edge_seconds\":" << edge_seconds
              << ",\"write_seconds\":" << write_seconds
              << ",\"seconds\":"
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - began).count()
              << "}}\n";
}

}  // namespace pancollapse::direct_index
