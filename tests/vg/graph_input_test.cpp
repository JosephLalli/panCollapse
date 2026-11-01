#include "graph_input.hpp"
#include <gbwt/dynamic_gbwt.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <tuple>
#include <utility>

using pancollapse::GraphInput;
using pancollapse::GraphPathStep;
using Key = std::tuple<std::string, bool>;
using Occurrence = std::tuple<std::string, std::int64_t, bool>;
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
            result.emplace_back(graph.get_path_name(s.path), graph.nodes().get_is_reverse(s.handle));
        }
    });
    std::sort(result.begin(), result.end());
    return result;
}
std::map<std::int64_t, std::vector<Occurrence>> xg_oracle(const xg::XG& graph) {
    std::map<std::int64_t, std::vector<Occurrence>> result;
    graph.for_each_path_handle([&](handlegraph::path_handle_t path) {
        const auto name = graph.get_path_name(path);
        if (name == "unselected") return;
        graph.for_each_step_in_path(path, [&](handlegraph::step_handle_t step) {
            const auto handle = graph.get_handle_of_step(step);
            const auto node = graph.get_id(handle);
            result[node].emplace_back(name, node, graph.get_is_reverse(handle));
        });
    });
    for (auto& [node, values] : result) {
        static_cast<void>(node);
        std::sort(values.begin(), values.end());
    }
    return result;
}
std::vector<Occurrence> gbwt_occurrences(GraphInput& graph, std::int64_t node) {
    std::vector<Occurrence> result;
    graph.for_each_node_occurrence(node, [&](const GraphPathStep& step) {
        const auto name = graph.get_path_name(step.path);
        require(name != "unselected", "unselected GBWT occurrence escaped selection");
        result.emplace_back(name, graph.nodes().get_id(step.handle),
                            graph.nodes().get_is_reverse(step.handle));
    });
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
    gbwt::FastLocate r_index(index);
    std::stringstream r_index_bytes; r_index.serialize(r_index_bytes);
    auto for_gbz = index;
    gbwtgraph::GBZ gbz(std::move(for_gbz), source, nullptr);
    std::stringstream gbz_bytes; gbz.simple_sds_serialize(gbz_bytes);
    const auto oracle = xg_oracle(source);

    GraphInput xg, separate, loaded, adopted, adopted_lifetime, adopted_loaded, combined;
    xg.load_xg(xg_bytes); xg_bytes.clear(); xg_bytes.seekg(0);
    separate.load_xg(xg_bytes); separate.load_gbwt(gbwt_bytes);
    xg_bytes.clear(); xg_bytes.seekg(0); gbwt_bytes.clear(); gbwt_bytes.seekg(0);
    r_index_bytes.clear(); r_index_bytes.seekg(0);
    loaded.load_xg(xg_bytes); loaded.load_gbwt(gbwt_bytes); loaded.load_r_index(r_index_bytes);
    xg_bytes.clear(); xg_bytes.seekg(0); gbwt_bytes.clear(); gbwt_bytes.seekg(0);
    adopted.load_xg(xg_bytes);
    GraphInput gbwt_donor; gbwt_donor.load_gbwt(gbwt_bytes);
    adopted.adopt_gbwt_input(std::move(gbwt_donor));
    {
        xg_bytes.clear(); xg_bytes.seekg(0); gbwt_bytes.clear(); gbwt_bytes.seekg(0);
        adopted_lifetime.load_xg(xg_bytes);
        GraphInput local_donor; local_donor.load_gbwt(gbwt_bytes);
        adopted_lifetime.adopt_gbwt_input(std::move(local_donor));
    }
    xg_bytes.clear(); xg_bytes.seekg(0); gbwt_bytes.clear(); gbwt_bytes.seekg(0);
    r_index_bytes.clear(); r_index_bytes.seekg(0);
    adopted_loaded.load_xg(xg_bytes);
    GraphInput loaded_donor; loaded_donor.load_gbwt(gbwt_bytes); loaded_donor.load_r_index(r_index_bytes);
    adopted_loaded.adopt_gbwt_input(std::move(loaded_donor));
    combined.load_gbz(gbz_bytes);
    GraphInput missing_xg, donor_for_bad_receiver;
    gbwt_bytes.clear(); gbwt_bytes.seekg(0); donor_for_bad_receiver.load_gbwt(gbwt_bytes);
    rejects([&] { missing_xg.adopt_gbwt_input(std::move(donor_for_bad_receiver)); }, "XG-only receiver");
    GraphInput receiver_for_bad_donor, xg_donor;
    xg_bytes.clear(); xg_bytes.seekg(0); receiver_for_bad_donor.load_xg(xg_bytes);
    xg_bytes.clear(); xg_bytes.seekg(0); xg_donor.load_xg(xg_bytes);
    rejects([&] { receiver_for_bad_donor.adopt_gbwt_input(std::move(xg_donor)); }, "GBWT-only donor");
    GraphInput second_donor;
    gbwt_bytes.clear(); gbwt_bytes.seekg(0); second_donor.load_gbwt(gbwt_bytes);
    rejects([&] { adopted.adopt_gbwt_input(std::move(second_donor)); }, "XG-only receiver");
    const std::vector<Key> expected{{"A", false}, {"A", false}, {"A", true},
                                    {"B", true}, {"B", true}};
    require(occurrences(xg, 2) == expected, "independent repeated/reverse XG oracle");
    for (auto* graph : {&separate, &loaded, &adopted, &adopted_lifetime, &adopted_loaded, &combined}) {
        graph->initialize_gbwt(2);
        const auto selected_paths = select(*graph);
        graph->prepare_paths(selected_paths, 2);
        const auto stats = graph->path_preparation_stats();
        require(stats.selected_paths == 3 && stats.selected_steps == 79 &&
                stats.allocated_coordinate_bytes == 0,
                "selected paths retain no coordinate payload");
        require(occurrences(*graph, 2) == expected, "FastLocate must preserve all five visits");
        require(occurrences(*graph, 9).empty(), "graph-only node has no path evidence");
        source.for_each_handle([&](handlegraph::handle_t handle) {
            const auto node = source.get_id(handle);
            const auto found = oracle.find(node);
            const std::vector<Occurrence> empty;
            require(gbwt_occurrences(*graph, node) == (found == oracle.end() ? empty : found->second),
                    "all GBWT path/node occurrence tuples must equal XG");
        });
        for (auto p : selected_paths) {
            const auto name = graph->get_path_name(p);
            std::vector<std::tuple<std::int64_t, bool>> observed, oracle;
            graph->for_each_path_step(p, [&](const auto& s) {
                require(s.ordinal.has_value(), "path iteration provides its local ordinal");
                observed.emplace_back(graph->nodes().get_id(s.handle), graph->nodes().get_is_reverse(s.handle));
            });
            source.for_each_step_in_path(source.get_path_handle(name), [&](handlegraph::step_handle_t s) {
                auto h = source.get_handle_of_step(s);
                oracle.emplace_back(source.get_id(h), source.get_is_reverse(h));
            });
            require(observed == oracle, "ordered walk identity");
            require(graph->get_path_length(p) == source.get_path_length(source.get_path_handle(name)), "path lengths");
        }
        const auto long_path = source.get_path_handle("LONG");
        const auto selected_long = std::find_if(selected_paths.begin(), selected_paths.end(),
            [&](auto path) { return graph->get_path_name(path) == "LONG"; });
        require(selected_long != selected_paths.end(), "long checkpoint path must be selected");
        require(graph->get_path_length(*selected_long) == source.get_path_length(long_path),
                "long checkpoint path length");
    }
    GraphInput membership;
    xg_bytes.clear(); xg_bytes.seekg(0); gbwt_bytes.clear(); gbwt_bytes.seekg(0);
    membership.load_xg(xg_bytes); membership.load_gbwt(gbwt_bytes);
    membership.initialize_gbwt(1); membership.prepare_paths(select(membership), 1);
    std::size_t visits = 0;
    membership.for_each_node_occurrence(2, [&](const auto&) { ++visits; });
    require(visits == 5, "membership preserves repeats without allocating positions");

    index.clearMetadata(); std::stringstream missing; index.serialize(missing);
    GraphInput bad; xg_bytes.clear(); xg_bytes.seekg(0); bad.load_xg(xg_bytes); bad.load_gbwt(missing);
    rejects([&] { bad.initialize_gbwt(1); }, "complete path");
    // Independent invalid indexes exercise startup rejection before any read is processed.
    for (bool bidirectional : {false, true}) {
        gbwt::GBWTBuilder invalid_builder(8, 128, 1);
        gbwt::vector_type walk{static_cast<gbwt::vector_type::value_type>(
            gbwt::Node::encode(bidirectional ? 99 : 1, false))};
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
    require(source.get_path_length(source.get_path_handle("LONG")) == 162,
            "mixed-length long-path oracle");
    std::cout << "graph input occurrence, orientation, selected-walk and no-coordinate checks passed\n";
    return 0;
} catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
