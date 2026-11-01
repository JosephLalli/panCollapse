#include "direct_count_compile.hpp"
#include "direct_count_index.hpp"
#include "native_count_index.hpp"
#include "native_count_reference.hpp"

#include <gbwt/dynamic_gbwt.h>
#include <gbwt/fast_locate.h>
#include <xg.hpp>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using pancollapse::count_reference::missing;
using pancollapse::native_count::Data;
using pancollapse::native_count::PathRecord;

void require(bool value, std::string_view message) {
    if (!value) throw std::runtime_error(std::string(message));
}

template<class Callback>
void require_failure(Callback&& callback, std::string_view needle) {
    bool failed = false;
    try {
        callback();
    } catch (const std::exception& error) {
        failed = true;
        require(std::string_view(error.what()).find(needle) != std::string_view::npos,
                "failure had the wrong diagnostic");
    }
    require(failed, "operation unexpectedly succeeded");
}

struct TempDirectory {
    std::filesystem::path path;

    TempDirectory() {
        path = std::filesystem::temp_directory_path() /
            ("pancollapse-direct-compile-" + std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path);
    }

    ~TempDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};

PathRecord path_row(std::uint32_t transcript, std::uint32_t policy,
                    std::uint32_t flags, std::uint32_t strand = 0) {
    PathRecord result;
    result.transcript_string = transcript;
    result.policy = policy;
    result.flags = flags;
    result.strand = strand;
    return result;
}

gbwt::GBWT write_gbwt(const std::filesystem::path& output) {
    gbwt::GBWTBuilder builder(8, 1024, 1);
    const std::vector<std::vector<std::pair<std::uint64_t, bool>>> walks{
        {{1, false}, {2, false}},
        {{1, false}, {2, false}, {2, false}},
        {{3, false}, {4, false}},
        {{3, false}},
        {{3, false}, {4, false}},
        {{4, false}},
        {{6, true}, {5, true}},
        {{6, true}},
        {{6, true}, {5, true}},
        {{5, true}},
        {{7, false}, {8, false}},
        {{7, false}},
        {{7, false}, {8, false}},
        {{8, false}}};
    for (const auto& walk : walks) {
        gbwt::vector_type encoded;
        for (const auto [node, reverse] : walk) {
            encoded.push_back(gbwt::Node::encode(node, reverse));
        }
        builder.insert(encoded, true);
    }
    builder.finish();
    gbwt::GBWT result(builder.index);
    result.addMetadata();
    result.metadata.setSamples({"sample"});
    result.metadata.setContigs({"contig"});
    result.metadata.setHaplotypes(1);
    for (std::size_t path = 0; path < walks.size(); ++path) {
        result.metadata.addPath(0, 0, 0, path);
    }
    std::ofstream stream(output, std::ios::binary);
    result.serialize(stream);
    require(bool(stream), "cannot write test GBWT");
    return result;
}

Data annotation_data(const std::filesystem::path& gbwt, bool complete_bindings) {
    Data data;
    data.strings = {
        "TX_ORDINARY", "TX_SPLICE", "TX_MINUS",
        "TX_MISMATCH_EXON", "TX_MISMATCH_BODY", "G"};
    data.feature_catalog = {"G"};
    data.sets = {{}};
    data.genes = {{5, missing, 0, {0, 0}, {missing, missing}, {0, 0}}};
    data.policies = {{0, 0, missing, 0}};
    data.paths = {
        path_row(0, 0, pancollapse::native_count::exon_path),
        path_row(0, 0, pancollapse::native_count::body_path),
        path_row(1, 0, pancollapse::native_count::exon_path),
        path_row(1, 0, pancollapse::native_count::body_path),
        path_row(1, 0, pancollapse::native_count::exon_path),
        path_row(1, 0, pancollapse::native_count::body_path),
        path_row(2, 0, pancollapse::native_count::exon_path, 1),
        path_row(2, 0, pancollapse::native_count::body_path, 1),
        path_row(2, 0, pancollapse::native_count::exon_path, 1),
        path_row(2, 0, pancollapse::native_count::body_path, 1),
        path_row(3, 0, pancollapse::native_count::exon_path),
        path_row(4, 0, pancollapse::native_count::body_path),
        path_row(3, 0, pancollapse::native_count::exon_path),
        path_row(4, 0, pancollapse::native_count::body_path)};
    if (complete_bindings) {
        data.pairs = {
            {0, 1}, {2, 3}, {4, 5}, {6, 7}, {8, 9}, {10, 11}, {12, 13}};
        for (std::uint32_t model = 0; model < data.pairs.size(); ++model) {
            for (const auto path : {data.pairs[model].exon, data.pairs[model].body}) {
                data.paths[path].model_first = data.model_refs.size();
                data.paths[path].model_count = 1;
                data.model_refs.push_back(model);
            }
        }
    }
    data.path_lookup.resize(data.paths.size());
    std::iota(data.path_lookup.begin(), data.path_lookup.end(), 0U);
    data.gbwt_size_bytes = std::filesystem::file_size(gbwt);
    data.content_id = "direct-compile-fixture";
    return data;
}

std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    require(bool(stream), "cannot read compiled artifact");
    return {std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
}

std::array<std::pair<std::uint64_t, std::uint64_t>, 8>
directions(std::uint64_t a, std::uint64_t b) {
    std::array<std::pair<std::uint64_t, std::uint64_t>, 8> result{};
    std::size_t next = 0;
    for (const auto [from, to] : {std::pair{a, b}, std::pair{b, a}}) {
        for (const bool from_reverse : {false, true}) {
            for (const bool to_reverse : {false, true}) {
                result[next++] = {
                    (from << 1) | static_cast<std::uint64_t>(from_reverse),
                    (to << 1) | static_cast<std::uint64_t>(to_reverse)};
            }
        }
    }
    return result;
}

void compare_reference(const pancollapse::NativeSource& native,
                       const pancollapse::direct_index::Source& direct) {
    require(native.model_count() == direct.model_count(), "model count differs");
    for (std::uint32_t model = 0; model < native.model_count(); ++model) {
        const auto left = native.model(model);
        const auto right = direct.model(model);
        require(left.transcript_string == right.transcript_string &&
                    left.exon_policy == right.exon_policy &&
                    left.body_policy == right.body_policy && left.strand == right.strand,
                "model metadata differs");
    }

    for (std::uint64_t id = 1; id <= 8; ++id) {
        const auto left = native.node_view(id);
        const auto right = direct.node_view(id);
        require(bool(left) == bool(right), "node presence differs");
        require(left && right, "fixture node is missing");
        require(left->length == right->length &&
                    left->postings.size() == right->postings.size() &&
                    std::equal(left->postings.begin(), left->postings.end(),
                               right->postings.begin(),
                               [](const auto& a, const auto& b) {
                                   return a.model == b.model &&
                                       a.support_set == b.support_set && a.flags == b.flags;
                               }),
                "node facts differ");
    }
    require(!direct.node_view(9), "absent node was materialized");

    const auto splice = direct.splice_targets(3, 4);
    require(splice.size() == 1 && splice[0] == 1, "known splice target is missing");
    require(direct.splice_targets(1, 2).empty(),
            "ordinary body adjacency was not suppressed");
    require(direct.splice_targets(7, 8).empty(),
            "body-side transcript ownership was replaced by the exon model label");
    const auto minus_splice = direct.splice_targets(5, 6);
    require(minus_splice.size() == 1 && minus_splice[0] == 2,
            "minus-strand splice target is missing");
    const auto minus_along = direct.junction_owners(13, 11);
    require(minus_along.size() == 1 && minus_along[0].transcript_string == 2 &&
                minus_along[0].flags == 1,
            "minus-strand along-transcript owner is wrong");
    const auto minus_reverse = direct.junction_owners(10, 12);
    require(minus_reverse.size() == 1 && minus_reverse[0].transcript_string == 2 &&
                minus_reverse[0].flags == 2,
            "minus-strand reverse-complement owner is wrong");
    for (std::uint64_t a = 1; a <= 8; ++a) {
        for (std::uint64_t b = a + 1; b <= 8; ++b) {
            const auto native_targets = native.splice_targets(a, b);
            const auto direct_targets = direct.splice_targets(a, b);
            require(native_targets.size() == direct_targets.size() &&
                        std::equal(native_targets.begin(), native_targets.end(),
                                   direct_targets.begin()),
                    "splice differential failed");
            for (const auto [from, to] : directions(a, b)) {
                const auto native_owners = native.junction_owners(from, to);
                const auto direct_owners = direct.junction_owners(from, to);
                require(native_owners.size() == direct_owners.size() &&
                            std::equal(native_owners.begin(), native_owners.end(),
                                       direct_owners.begin(),
                                       [](const auto& left, const auto& right) {
                                           return left.transcript_string == right.transcript_string &&
                                               left.flags == right.flags;
                                       }),
                        "junction differential failed");
            }
        }
    }
}

}  // namespace

int main() try {
    TempDirectory temporary;
    const auto xg_path = temporary.path / "graph.xg";
    const auto gbwt_path = temporary.path / "paths.gbwt";
    const auto ri_path = temporary.path / "paths.ri";
    const auto native_path = temporary.path / "native.pci";
    const auto direct_one = temporary.path / "direct-one.pci";
    const auto direct_four = temporary.path / "direct-four.pci";
    const auto direct_bounded = temporary.path / "direct-bounded.pci";

    xg::XG graph;
    graph.from_enumerators(
        [](const auto& emit) {
            for (handlegraph::nid_t id = 1; id <= 8; ++id) emit("A", id);
        },
        [](const auto&) {}, [](const auto&) {}, false);
    {
        std::ofstream stream(xg_path, std::ios::binary);
        graph.serialize(stream);
        require(bool(stream), "cannot write test XG");
    }
    auto gbwt_index = write_gbwt(gbwt_path);
    {
        gbwt::FastLocate locate(gbwt_index);
        std::ofstream stream(ri_path, std::ios::binary);
        locate.serialize(stream);
        require(bool(stream), "cannot write test r-index");
    }
    pancollapse::native_count::write(native_path, annotation_data(gbwt_path, true));

    pancollapse::direct_index::BuildOptions options;
    options.source_index = native_path;
    options.xg = xg_path;
    options.gbwt = gbwt_path;
    options.r_index = ri_path;
    options.output = direct_one;
    options.threads = 1;
    options.memory_limit_bytes = 1ULL << 30;
    options.provenance_json = "{\"fixture\":true}";
    pancollapse::direct_index::build(options);
    options.output = direct_four;
    options.threads = 4;
    pancollapse::direct_index::build(options);
    require(bytes(direct_one) == bytes(direct_four),
            "one- and four-thread artifacts differ");

    // A 48-byte allowance fits every individual fixture node but not two
    // dense results. This proves that a worker never queues a second result
    // before the first wave is committed.
    options.output = direct_bounded;
    options.testing_node_payload_limit_bytes = 48;
    pancollapse::direct_index::build(options);
    require(bytes(direct_one) == bytes(direct_bounded),
            "bounded ordered waves changed the artifact");
    options.testing_node_payload_limit_bytes = 0;

    const auto native = pancollapse::NativeSource::load(
        native_path, xg_path, gbwt_path, ri_path, 1,
        pancollapse::NativeQueryMode::node_score);
    const auto direct = pancollapse::direct_index::Source::load(direct_one);
    compare_reference(*native, *direct);

    const auto incomplete_path = temporary.path / "incomplete.pci";
    pancollapse::native_count::write(incomplete_path,
                                     annotation_data(gbwt_path, false));
    options.source_index = incomplete_path;
    options.output = temporary.path / "incomplete-direct.pci";
    options.threads = 4;
    require_failure([&] { pancollapse::direct_index::build(options); },
                    "selected path has no model binding");
    require(!std::filesystem::exists(options.output),
            "incomplete binding published an artifact");

    const auto mismatched_path = temporary.path / "mismatched.pci";
    auto mismatched = annotation_data(gbwt_path, true);
    ++mismatched.gbwt_size_bytes;
    pancollapse::native_count::write(mismatched_path, mismatched);
    options.source_index = mismatched_path;
    options.output = temporary.path / "mismatched-direct.pci";
    require_failure([&] { pancollapse::direct_index::build(options); },
                    "native index does not match GBWT byte size");
    require(!std::filesystem::exists(options.output),
            "mismatched binding published an artifact");

    options.source_index = native_path;
    options.output = temporary.path / "under-budget.pci";
    options.threads = 1;
    options.memory_limit_bytes = 1;
    require_failure([&] { pancollapse::direct_index::build(options); },
                    "memory budget has no compiler headroom");
    require(!std::filesystem::exists(options.output),
            "under-budget build published an artifact");

    if (std::numeric_limits<std::size_t>::max() >
        static_cast<std::size_t>(std::numeric_limits<int>::max())) {
        options.output = temporary.path / "thread-width.pci";
        options.threads = static_cast<std::size_t>(std::numeric_limits<int>::max()) + 1;
        options.memory_limit_bytes = 1ULL << 30;
        require_failure([&] { pancollapse::direct_index::build(options); },
                        "thread count is outside the OpenMP range");
    }
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
