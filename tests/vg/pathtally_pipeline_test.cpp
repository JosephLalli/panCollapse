// PathTally steps 2-4 verification: node-to-HST attribution + transcript-id
// collapse, top-score-plus-ties winner selection across a read's alignments, and
// majority-of-bases orientation. Uses synthetic multipath alignments and an
// injected node->HST lookup, so no graph fixture is needed.

#include "pathtally.hpp"

#include <vg/vg.pb.h>

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace {

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

using NodePaths = std::map<int64_t, std::vector<std::pair<std::string, bool>>>;

pathtally::PathLookup make_lookup(const NodePaths& table) {
    return [table](int64_t node_id, const std::function<void(const std::string&, bool)>& emit) {
        auto it = table.find(node_id);
        if (it == table.end()) return;
        for (const auto& [name, rev] : it->second) emit(name, rev);
    };
}

// A single-subpath alignment; each mapping is one node with one match edit.
// read total length = sum of the match lengths, so both read-end bonuses apply.
vg::MultipathAlignment make_alignment(const std::vector<std::pair<int64_t, int64_t>>& nodes,
                                      bool read_reverse) {
    vg::MultipathAlignment mp;
    int64_t total = 0;
    for (const auto& [node_id, len] : nodes) total += len;
    mp.set_sequence(std::string(static_cast<size_t>(total), 'A'));
    vg::Subpath* sp = mp.add_subpath();
    for (const auto& [node_id, len] : nodes) {
        vg::Mapping* m = sp->mutable_path()->add_mapping();
        m->mutable_position()->set_node_id(node_id);
        m->mutable_position()->set_is_reverse(read_reverse);
        vg::Edit* e = m->add_edit();
        e->set_from_length(len);
        e->set_to_length(len);
    }
    return mp;
}

std::vector<std::string> transcript_ids(const std::vector<pathtally::RadTarget>& targets) {
    std::vector<std::string> ids;
    for (const auto& t : targets) ids.push_back(t.transcript);
    return ids;
}

void run() {
    // transcript_id_of: strip _H<n>/_R<n>, keep everything else.
    check(pathtally::transcript_id_of("NM_000063.6_H1") == "NM_000063.6", "strip _H1");
    check(pathtally::transcript_id_of("NM_000063.6_R1") == "NM_000063.6", "strip _R1");
    check(pathtally::transcript_id_of("A_B_H12") == "A_B", "strip keeps inner underscore");
    check(pathtally::transcript_id_of("NM_000063.6") == "NM_000063.6", "no suffix unchanged");
    check(pathtally::transcript_id_of("X_H") == "X_H", "H without digits unchanged");
    check(pathtally::transcript_id_of("X_Q1") == "X_Q1", "non-H/R tag unchanged");

    // Case A: attribution + haplotype collapse. node1 -> {TX1_H1, TX1_H2}, node2 -> {TX1_H1}.
    // Scores (single subpath, both bonuses): node1 = 10 + 5(start) = 15; node2 = 5 + 5(end) = 10.
    {
        vg::MultipathAlignment mp = make_alignment({{1, 10}, {2, 5}}, false);
        NodePaths table = {{1, {{"TX1_H1", false}, {"TX1_H2", false}}}, {2, {{"TX1_H1", false}}}};
        auto tallies = pathtally::tally_read_group({&mp}, make_lookup(table));
        check(tallies["TX1_H1"].score == 25, "A: TX1_H1 score 25");
        check(tallies["TX1_H2"].score == 15, "A: TX1_H2 score 15");
        auto targets = pathtally::select_targets(tallies);
        check(transcript_ids(targets) == std::vector<std::string>{"TX1"}, "A: winner collapses to TX1");
        check(targets.size() == 1 && targets[0].forward, "A: forward");
    }

    // Case B: majority-of-bases orientation. TX2_H1 gets 10 forward bases (node3) and
    // 5 reverse bases (node4, path reversed) -> forward wins.
    {
        vg::MultipathAlignment mp = make_alignment({{3, 10}, {4, 5}}, false);
        NodePaths table = {{3, {{"TX2_H1", false}}}, {4, {{"TX2_H1", true}}}};
        auto tallies = pathtally::tally_read_group({&mp}, make_lookup(table));
        check(tallies["TX2_H1"].forward_bases == 10 && tallies["TX2_H1"].reverse_bases == 5,
              "B: orientation base counts");
        auto targets = pathtally::select_targets(tallies);
        check(targets.size() == 1 && targets[0].transcript == "TX2" && targets[0].forward,
              "B: majority forward");
    }

    // Case C: no HST crosses any node -> no target.
    {
        vg::MultipathAlignment mp = make_alignment({{9, 8}}, false);
        auto tallies = pathtally::tally_read_group({&mp}, make_lookup({}));
        check(pathtally::select_targets(tallies).empty(), "C: no compatible target");
    }

    // Case D: winner pooled across a read's alignments. Record 2's HST (score 20)
    // beats record 1's (score 15); the lower one is not emitted.
    {
        vg::MultipathAlignment r1 = make_alignment({{5, 5}}, false);   // 5 + 10 bonuses = 15
        vg::MultipathAlignment r2 = make_alignment({{6, 10}}, false);  // 10 + 10 bonuses = 20
        NodePaths table = {{5, {{"TXA_H1", false}}}, {6, {{"TXB_H1", false}}}};
        auto tallies = pathtally::tally_read_group({&r1, &r2}, make_lookup(table));
        check(tallies["TXA_H1"].score == 15 && tallies["TXB_H1"].score == 20, "D: pooled scores");
        auto targets = pathtally::select_targets(tallies);
        check(transcript_ids(targets) == std::vector<std::string>{"TXB"}, "D: higher pooled winner only");
    }

    // Case E: exact ties across distinct transcripts are all retained, sorted.
    // Two nodes of equal score (each 5 + one bonus = 10) on distinct transcripts.
    {
        vg::MultipathAlignment mp = make_alignment({{7, 5}, {8, 5}}, false);
        NodePaths table = {{7, {{"TXD_H1", false}}}, {8, {{"TXC_H1", false}}}};
        auto tallies = pathtally::tally_read_group({&mp}, make_lookup(table));
        check(tallies["TXD_H1"].score == 10 && tallies["TXC_H1"].score == 10, "E: tied scores");
        auto targets = pathtally::select_targets(tallies);
        check(transcript_ids(targets) == (std::vector<std::string>{"TXC", "TXD"}), "E: ties retained, sorted");
    }

    std::printf("pathtally pipeline: PASS\n");
}

}  // namespace

int main() {
    run();
    return 0;
}
