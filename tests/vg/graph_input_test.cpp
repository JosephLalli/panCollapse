#include "graph_input.hpp"
#include <gbwt/dynamic_gbwt.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <set>
#include <sstream>
#include <tuple>

using pancollapse::GraphInput;
using pancollapse::GraphPathStep;
using Key = std::tuple<std::string, bool, std::uint64_t>;
void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F action, const std::string& message) {
    try { action(); } catch (const std::exception& e) {
        require(std::string(e.what()).find(message) != std::string::npos, e.what());
        return;
    }
    throw std::runtime_error("expected rejection: " + message);
}
std::vector<handlegraph::path_handle_t> select(GraphInput& graph) {
    std::vector<handlegraph::path_handle_t> result;
    graph.for_each_path_handle([&](auto p) {
        if (graph.get_path_name(p) != "unselected") result.push_back(p);
    });
    return result;
}
std::vector<Key> occurrences(GraphInput& graph, std::int64_t node) {
    std::vector<Key> result;
    graph.for_each_node_occurrence(node, [&](const GraphPathStep& s) {
        if (graph.get_path_name(s.path) != "unselected") {
            result.emplace_back(graph.get_path_name(s.path), graph.nodes().get_is_reverse(s.handle), s.base_position());
        }
    }, true);
    std::sort(result.begin(), result.end());
    return result;
}
int main(int argc, char** argv) try {
    require(argc == 2, "graph_input_test needs its XG fixture");
    xg::XG source;
    std::ifstream input(argv[1], std::ios::binary);
    source.deserialize(input);
    std::stringstream xg_bytes;
    source.serialize(xg_bytes);
    gbwt::GBWTBuilder builder(8, 1000, 1);
    gbwtgraph::MetadataBuilder metadata;
    source.for_each_path_handle([&](handlegraph::path_handle_t p) {
        gbwt::vector_type walk;
        source.for_each_step_in_path(p, [&](handlegraph::step_handle_t s) {
            auto h = source.get_handle_of_step(s);
            walk.push_back(gbwt::Node::encode(source.get_id(h), source.get_is_reverse(h)));
        });
        builder.insert(walk, true);
        metadata.add_generic_path(source.get_path_name(p));
    });
    builder.finish();
    gbwt::GBWT index(builder.index);
    index.addMetadata(); index.metadata = metadata.get_metadata();
    std::stringstream gbwt_bytes; index.serialize(gbwt_bytes);
    auto for_gbz = index;
    gbwtgraph::GBZ gbz(std::move(for_gbz), source, nullptr);
    std::stringstream gbz_bytes; gbz.simple_sds_serialize(gbz_bytes);

    GraphInput xg, separate, combined;
    xg.load_xg(xg_bytes); xg_bytes.clear(); xg_bytes.seekg(0);
    separate.load_xg(xg_bytes); separate.load_gbwt(gbwt_bytes);
    combined.load_gbz(gbz_bytes);
    const std::vector<Key> expected{{"A", false, 3}, {"A", false, 9},
                                    {"A", true, 12}, {"B", true, 0}, {"B", true, 6}};
    require(occurrences(xg, 2) == expected, "independent repeated/reverse XG oracle");
    for (auto* graph : {&separate, &combined}) {
        graph->initialize_gbwt(2);
        graph->prepare_paths(select(*graph), true, 2);
        require(occurrences(*graph, 2) == expected, "FastLocate must preserve all five visits");
        require(occurrences(*graph, 9).empty(), "graph-only node has no path evidence");
        graph->for_each_node_occurrence(2, [&](const GraphPathStep& s) {
            require(s.ordinal.has_value(), "GBWT occurrence needs step ordinal");
            const auto name = graph->get_path_name(s.path);
            if (name == "A") require(*s.ordinal == 1 || *s.ordinal == 3 || *s.ordinal == 5, "A step ordinal");
            if (name == "B") require(*s.ordinal == 0 || *s.ordinal == 2, "B step ordinal");
        });
        for (auto p : select(*graph)) {
            const auto name = graph->get_path_name(p);
            std::vector<std::tuple<std::int64_t, bool, std::uint64_t>> observed, oracle;
            graph->for_each_path_step(p, [&](const auto& s) {
                observed.emplace_back(graph->nodes().get_id(s.handle), graph->nodes().get_is_reverse(s.handle), s.base_position());
            }, true);
            source.for_each_step_in_path(source.get_path_handle(name), [&](handlegraph::step_handle_t s) {
                auto h = source.get_handle_of_step(s);
                oracle.emplace_back(source.get_id(h), source.get_is_reverse(h), source.get_position_of_step(s));
            });
            require(observed == oracle, "ordered walk and coordinate identity");
            require(graph->get_path_length(p) == source.get_path_length(source.get_path_handle(name)), "path lengths");
        }
    }
    GraphInput membership;
    xg_bytes.clear(); xg_bytes.seekg(0); gbwt_bytes.clear(); gbwt_bytes.seekg(0);
    membership.load_xg(xg_bytes); membership.load_gbwt(gbwt_bytes);
    membership.initialize_gbwt(1); membership.prepare_paths(select(membership), false, 1);
    std::size_t visits = 0;
    membership.for_each_node_occurrence(2, [&](const auto& s) { ++visits; require(!s.position, "membership has no prefix positions"); });
    require(visits == 5, "membership preserves repeats without allocating positions");
    rejects([&] { occurrences(membership, 2); }, "path base positions");
    pancollapse::PathPositions widths;
    widths.push(0); widths.push(UINT32_MAX); widths.push(std::uint64_t(UINT32_MAX) + 1); widths.push(UINT64_MAX);
    require(widths.wide() && widths.at(0) == 0 && widths.at(1) == UINT32_MAX &&
            widths.at(2) == std::uint64_t(UINT32_MAX) + 1 && widths.at(3) == UINT64_MAX, "checked coordinate widening");
    pancollapse::PathPositions first_wide;
    first_wide.push(UINT64_MAX); require(first_wide.at(0) == UINT64_MAX, "wide first coordinate");
    index.clearMetadata(); std::stringstream missing; index.serialize(missing);
    GraphInput bad; xg_bytes.clear(); xg_bytes.seekg(0); bad.load_xg(xg_bytes); bad.load_gbwt(missing);
    rejects([&] { bad.initialize_gbwt(1); }, "complete path");
    // Independent invalid indexes exercise startup rejection before any read is processed.
    for (bool bidirectional : {false, true}) {
        gbwt::GBWTBuilder invalid_builder(8, 128, 1);
        gbwt::vector_type walk{gbwt::Node::encode(bidirectional ? 99 : 1, false)};
        invalid_builder.insert(walk, bidirectional);
        invalid_builder.finish();
        gbwt::GBWT invalid_index(invalid_builder.index);
        gbwtgraph::MetadataBuilder names;
        names.add_generic_path("A");
        invalid_index.addMetadata(); invalid_index.metadata = names.get_metadata();
        std::stringstream bytes; invalid_index.serialize(bytes);
        GraphInput invalid;
        xg_bytes.clear(); xg_bytes.seekg(0); invalid.load_xg(xg_bytes); invalid.load_gbwt(bytes);
        rejects([&] { invalid.initialize_gbwt(1); },
                bidirectional ? "node 99 is absent" : "must be bidirectional");
    }
    std::cout << "graph input occurrence, orientation, coordinate, selection and width checks passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
