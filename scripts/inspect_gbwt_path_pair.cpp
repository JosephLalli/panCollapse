// Read-only reference diagnostic. No read assignments or counting policy changes.
#include <gbwt/gbwt.h>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

int main(int argc, char** argv) try {
    if (argc < 4) throw std::runtime_error("usage: inspect_gbwt_path_pair GBWT EXON_PATH BODY_PATH [BODY_PATH ...]");
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) throw std::runtime_error("cannot open GBWT");
    const auto started = std::chrono::steady_clock::now();
    gbwt::GBWT index;
    index.load(input);
    if (!input || !index.bidirectional() || !index.hasMetadata() || !index.metadata.hasPathNames())
        throw std::runtime_error("requires a complete bidirectional GBWT with metadata");
    std::cerr << "GBWT loaded in " << std::chrono::duration<double>(std::chrono::steady_clock::now()-started).count() << " seconds\n";
    const auto& metadata = index.metadata;
    const auto generic = metadata.sample("_gbwt_ref");
    if (generic >= metadata.samples()) throw std::runtime_error("missing generic path sample");
    std::vector<gbwt::vector_type> walks;
    for (int argument = 2; argument < argc; ++argument) {
        const std::string name = argv[argument];
        const auto contig = metadata.contig(name);
        if (contig >= metadata.contigs()) throw std::runtime_error("missing generic path: " + name);
        const auto paths = metadata.findPaths(generic, contig);
        if (paths.size() != 1) throw std::runtime_error("non-unique generic path: " + name);
        const auto id = paths.front();
        const auto& path = metadata.path(id);
        std::cout << "META\t" << name << '\t' << id << '\t' << metadata.sample(path.sample)
                  << '\t' << metadata.contig(path.contig) << '\t' << path.phase << '\t' << path.count << '\n';
        walks.push_back(index.extract(gbwt::Path::encode(id, false)));
        std::map<uint64_t, size_t> multiplicity;
        for (const auto node : walks.back()) ++multiplicity[gbwt::Node::id(node)];
        size_t repeats = 0;
        for (const auto& [node, count] : multiplicity) { static_cast<void>(node); repeats += count > 1; }
        std::cout << "PATH\t" << name << '\t' << walks.back().size() << '\t' << multiplicity.size() << '\t' << repeats << '\n';
        for (size_t step = 0; step < walks.back().size(); ++step) {
            const auto node = walks.back()[step];
            std::cout << "STEP\t" << name << '\t' << step << '\t' << gbwt::Node::id(node)
                      << '\t' << gbwt::Node::is_reverse(node) << '\n';
        }
    }
    for (size_t body = 1; body < walks.size(); ++body) {
        std::map<uint64_t, std::vector<std::pair<size_t, bool>>> occurrences;
        for (size_t step = 0; step < walks[body].size(); ++step) {
            const auto node = walks[body][step];
            occurrences[gbwt::Node::id(node)].emplace_back(step, gbwt::Node::is_reverse(node));
        }
        std::set<uint64_t> shared;
        size_t same = 0, opposite = 0, repeated_shared = 0;
        bool compatible_same = true, compatible_opposite = true;
        for (size_t exon_step = 0; exon_step < walks.front().size(); ++exon_step) {
            const auto node = walks.front()[exon_step];
            const auto found = occurrences.find(gbwt::Node::id(node));
            if (found == occurrences.end()) continue;
            shared.insert(found->first);
            repeated_shared += found->second.size() > 1;
            size_t same_hits = 0, opposite_hits = 0;
            for (const auto& [body_step, reverse] : found->second) {
                const bool relation = gbwt::Node::is_reverse(node) == reverse;
                same += relation; opposite += !relation;
                same_hits += relation; opposite_hits += !relation;
                std::cout << "SHARED\t" << argv[body+2] << '\t' << exon_step << '\t' << found->first
                          << '\t' << gbwt::Node::is_reverse(node) << '\t' << body_step << '\t' << reverse
                          << '\t' << relation << '\n';
            }
            compatible_same &= same_hits == 1;
            compatible_opposite &= opposite_hits == 1;
        }
        std::cout << "PAIR\t" << argv[2] << '\t' << argv[body+2] << "\tshared_nodes=" << shared.size()
                  << "\tsame_hits=" << same << "\topposite_hits=" << opposite << "\trepeated_shared_steps=" << repeated_shared
                  << "\tcompatible_relations=" << (!shared.empty() && compatible_same) + (!shared.empty() && compatible_opposite) << '\n';
    }
    if (!std::cout) throw std::runtime_error("diagnostic output failed");
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
