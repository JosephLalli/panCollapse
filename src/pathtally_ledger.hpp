#pragma once

// Ledger-based count modes reproducing STARsolo/CellRanger Gene and GeneFull semantics.
//
// For each read we hold, per gene, the aligned-base counts split into exonic and intronic (plus
// orientation). The count mode is a pure rule over that ledger: Gene keeps genes the read is
// exonic for, GeneFull keeps any gene body the read overlaps, and the GeneFull variants add
// exon-overlap tie-breaking and an antisense-exon exclusion. Membership comes from two path->gene
// t2gs (an exon/HST layer and a gene-body layer) read off the graph -- no custom node->gene map.
//
// This file is pure and graph-free so the mode semantics are unit-tested directly. main.cpp
// builds the ledger from the graph and applies the Unique multimapper drop (>1 kept gene) to the
// result.

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace pathtally {

struct GeneLedger {
    int64_t exonic_bases = 0;    // aligned read bases on the gene's exon nodes
    int64_t intronic_bases = 0;  // aligned read bases on the gene's intron nodes (body, not exon)
    int64_t forward_bases = 0;   // aligned bases sense to the gene
    int64_t reverse_bases = 0;   // aligned bases antisense to the gene
};

// Score is the default non-ledger D048 path (handled in main.cpp). The rest reproduce STARsolo
// soloFeatures.
enum class CountMode { Score, Gene, GeneFull, GeneFullExonOverIntron, GeneFullEx50pAS };

// Apply the mode rule to the per-gene ledger; return the kept genes as (gene, sense-forward),
// sorted by gene id (the ledger is a std::map, so iteration is sorted). Does NOT apply the Unique
// multimapper drop -- the caller does, so it can count the drop. read_length is the read's length
// in bases; exon_gate is the exonic fraction of the read a gene needs under Gene mode (default
// 0.5, CellRanger's exonic rule).
inline std::vector<std::pair<std::string, bool>> apply_count_mode(
    CountMode mode, const std::map<std::string, GeneLedger>& ledger, int64_t read_length,
    double exon_gate = 0.5) {
    std::vector<std::pair<std::string, bool>> candidates;
    const double gate_bases = exon_gate * static_cast<double>(read_length);

    for (const auto& [gene, l] : ledger) {
        if (l.exonic_bases + l.intronic_bases <= 0) {
            continue;  // no overlap with this gene body
        }
        const bool forward = l.forward_bases >= l.reverse_bases;
        switch (mode) {
        case CountMode::Gene:
            // The read must be exonic for the gene: >= exon_gate of the read on its exons.
            if (l.exonic_bases > 0 && static_cast<double>(l.exonic_bases) >= gate_bases) {
                candidates.emplace_back(gene, forward);
            }
            break;
        case CountMode::GeneFull:
        case CountMode::GeneFullExonOverIntron:
            candidates.emplace_back(gene, forward);  // any body overlap; tie-break below
            break;
        case CountMode::GeneFullEx50pAS:
            // Do not count a read that is 100% exonic and antisense to the gene.
            if (l.intronic_bases == 0 && !forward) {
                break;
            }
            candidates.emplace_back(gene, forward);
            break;
        case CountMode::Score:
            break;
        }
    }

    // Multi-gene tie-breaks that prefer exonic overlap (only narrow the set; never add).
    if (mode == CountMode::GeneFullExonOverIntron) {
        // Prefer genes the read overlaps 100% within exons.
        bool any = false;
        for (const auto& c : candidates) {
            if (ledger.at(c.first).intronic_bases == 0) { any = true; break; }
        }
        if (any) {
            std::vector<std::pair<std::string, bool>> kept;
            for (const auto& c : candidates) {
                if (ledger.at(c.first).intronic_bases == 0) kept.push_back(c);
            }
            candidates.swap(kept);
        }
    } else if (mode == CountMode::GeneFullEx50pAS) {
        // Prefer genes the read is majority-exonic for (>50% exon of its overlap).
        bool any = false;
        for (const auto& c : candidates) {
            const GeneLedger& l = ledger.at(c.first);
            if (l.exonic_bases > l.intronic_bases) { any = true; break; }
        }
        if (any) {
            std::vector<std::pair<std::string, bool>> kept;
            for (const auto& c : candidates) {
                const GeneLedger& l = ledger.at(c.first);
                if (l.exonic_bases > l.intronic_bases) kept.push_back(c);
            }
            candidates.swap(kept);
        }
    }
    return candidates;
}

}  // namespace pathtally
