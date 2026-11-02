#include "direct_count_index.hpp"
#include "model_set_fixture_support.hpp"
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <unordered_map>
#include <unordered_set>

namespace {
template<class T> void unique(std::vector<T>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}
using Pair = std::pair<std::uint64_t, std::uint64_t>;
using pancollapse::test::gene_local_ids;
void permutation_test() {
    using pancollapse::count_reference::missing;
    pancollapse::direct_index::Metadata data;
    data.genes.resize(2); data.policies = {{0, 0, missing, 0}, {1, 0, missing, 0}};
    data.models = {{10, 1, 0, 0}, {11, 0, 1, 1}, {12, missing, 1, 0}, {13, missing, missing, 1}};
    const auto original = data.models;
    const auto order = gene_local_ids(data);
    if (order != std::vector<std::uint32_t>{1, 0, 2, 3} ||
        data.models[0].transcript_string != 11 || data.models[1].transcript_string != 10 ||
        data.models[2].body_policy != 1 || data.models[3].strand != 1)
        throw std::runtime_error("gene-local model permutation changed model records");
    auto sorted = order; unique(sorted);
    if (sorted != std::vector<std::uint32_t>{0, 1, 2, 3}) throw std::runtime_error("model order not bijective");
    for (std::size_t i = 0; i < original.size(); ++i) {
        const auto& a = original[i]; const auto& b = data.models.at(order[i]);
        if (std::tie(a.transcript_string, a.exon_policy, a.body_policy, a.strand) !=
            std::tie(b.transcript_string, b.exon_policy, b.body_policy, b.strand))
            throw std::runtime_error("permutation lost a model field");
    }
    pancollapse::direct_index::Metadata empty;
    if (!gene_local_ids(empty).empty()) throw std::runtime_error("empty permutation failed");
}
// Mirror endpoint propagation in the resolver, including reference-empty
// bridges. This utility selects reference coverage; it never assigns reads.
void collect(const vg::MultipathAlignment& alignment, std::unordered_set<std::uint64_t>& nodes,
             std::set<Pair>& edges) {
    const auto size = alignment.subpath_size();
    std::vector<std::optional<std::uint64_t>> first(size), last(size);
    std::vector<bool> incoming(size, false), reachable(size, false);
    for (int i = 0; i < size; ++i) {
        const auto& subpath = alignment.subpath(i);
        for (const auto& mapping : subpath.path().mapping()) {
            const auto id = static_cast<std::uint64_t>(mapping.position().node_id());
            nodes.insert(id);
            std::uint64_t bases = 0;
            for (const auto& edit : mapping.edit()) bases += edit.from_length();
            if (!bases) continue;
            const auto endpoint = (id << 1) | mapping.position().is_reverse();
            if (!first[i]) first[i] = endpoint;
            if (last[i]) edges.emplace(*last[i], endpoint);
            last[i] = endpoint;
        }
        const auto mark = [&](std::uint32_t next) {
            if (next <= static_cast<std::uint32_t>(i) || next >= static_cast<std::uint32_t>(size))
                throw std::runtime_error("fixture GAMP not topologically ordered");
            incoming[next] = true;
        };
        for (const auto next : subpath.next()) mark(next);
        for (const auto& edge : subpath.connection()) mark(edge.next());
    }
    if (alignment.start_size()) for (const auto start : alignment.start()) {
        if (start >= static_cast<std::uint32_t>(size)) throw std::runtime_error("fixture GAMP start out of range");
        reachable[start] = true;
    } else for (int i = 0; i < size; ++i) reachable[i] = !incoming[i];
    std::vector<std::vector<std::uint64_t>> endpoints(size);
    for (int i = 0; i < size; ++i) if (reachable[i]) {
        auto previous = std::move(endpoints[i]); unique(previous);
        if (first[i]) for (auto from : previous) edges.emplace(from, *first[i]);
        if (last[i]) previous = {*last[i]};
        const auto send = [&](std::uint32_t next) {
            reachable[next] = true;
            endpoints[next].insert(endpoints[next].end(), previous.begin(), previous.end());
        };
        for (const auto next : alignment.subpath(i).next()) send(next);
        for (const auto& edge : alignment.subpath(i).connection()) send(edge.next());
    }
}
}
// Private bounded experiment, not a public conversion/upgrader command.
int main(int argc, char** argv) try {
    if (argc == 2 && std::string(argv[1]) == "--self-test") { permutation_test(); return 0; }
    if (argc != 5) throw std::runtime_error("usage: model_set_fixture V2_INDEX GAMP OUTPUT v2|v3");
    const bool sets = std::string(argv[4]) == "v3";
    if (!sets && std::string(argv[4]) != "v2") throw std::runtime_error("expected v2 or v3");
    std::unordered_set<std::uint64_t> seen;
    std::set<Pair> edges;
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) throw std::runtime_error("fixture GAMP open");
    std::uint64_t records = 0;
    vg::io::for_each<vg::MultipathAlignment>(input, [&](vg::MultipathAlignment& alignment) {
        collect(alignment, seen, edges); ++records;
    });
    std::vector<std::uint64_t> nodes(seen.begin(), seen.end()); unique(nodes);
    if (nodes.empty()) throw std::runtime_error("empty fixture");
    std::cerr << "coverage nodes=" << nodes.size() << " edges=" << edges.size() << " records=" << records << std::endl;
    auto source = pancollapse::direct_index::Source::load(argv[1]);
    if (source->has_model_sets()) throw std::runtime_error("fixture input must be V2");
    pancollapse::direct_index::Metadata metadata;
    metadata.strings = source->strings(); metadata.sets = source->sets();
    metadata.genes.assign(source->genes().begin(), source->genes().end());
    metadata.policies.assign(source->policies().begin(), source->policies().end());
    metadata.models.assign(source->ready_models().begin(), source->ready_models().end());
    metadata.content_id = source->content_id(); metadata.feature_catalog = source->take_feature_catalog();
    metadata.provenance_json = source->provenance_json();
    const auto order = sets ? gene_local_ids(metadata) : std::vector<std::uint32_t>{};
    pancollapse::direct_index::Writer writer(argv[3], std::move(metadata), nodes.back(), 100ULL << 30, 64, sets);
    std::unordered_map<std::uint32_t, std::uint64_t> lists;
    std::vector<pancollapse::count_reference::Posting> values;
    for (const auto id : nodes) {
        const auto node = source->node_view(id);
        if (!node) throw std::runtime_error("fixture node missing from input");
        auto [found, inserted] = lists.try_emplace(node->shared_membership_id, id);
        if (inserted) {
            values.clear(); values.reserve(node->postings.size());
            for (const auto row : node->postings) values.push_back(row);
            if (sets) {
                for (auto& row : values) row.model = order.at(row.model);
                std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) { return a.model < b.model; });
            }
            writer.append_node(id, node->length, values);
        } else writer.append_shared_node(id, node->length, found->second);
    }
    writer.seal_nodes();
    std::set<Pair> physical;
    for (const auto& [from, to] : edges) {
        auto a = from >> 1, b = to >> 1;
        if (a > b) std::swap(a, b);
        if (a != b && physical.emplace(a, b).second)
            writer.append_splice(a, b, source->splice_targets(a, b));
        writer.append_junction(from, to, source->junction_owners(from, to));
    }
    writer.finish();
    const auto stats = writer.stats();
    std::cout << "{\"nodes\":" << stats.nodes << ",\"lists\":" << stats.unique_lists
              << ",\"postings\":" << stats.stored_postings << ",\"bytes\":" << stats.index_bytes
              << ",\"model_id_order\":\"" << (sets ? "gene-stable-bijection" : "original") << "\"}\n";
} catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
