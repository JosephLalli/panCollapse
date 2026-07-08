// Unit tests for the ledger count-mode rules (Gene / GeneFull / GeneFull_ExonOverIntron /
// GeneFull_Ex50pAS). Pure: builds per-gene exon/intron ledgers and checks the kept gene set.

#include "pathtally_ledger.hpp"

#include <cstdio>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace {

using pathtally::CountMode;
using pathtally::GeneLedger;

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

GeneLedger led(int64_t ex, int64_t intr, bool sense) {
    return GeneLedger{ex, intr, sense ? ex + intr : 0, sense ? 0 : ex + intr};
}

std::vector<std::string> genes(const std::vector<std::pair<std::string, bool>>& kept) {
    std::vector<std::string> g;
    for (const auto& k : kept) g.push_back(k.first);
    return g;
}

void run() {
    // Gene: purely-intronic read is not exonic -> excluded.
    check(apply_count_mode(CountMode::Gene, {{"A", led(0, 100, true)}}, 100).empty(),
          "Gene: purely intronic excluded");
    // Gene: fully-exonic read -> kept.
    check(genes(apply_count_mode(CountMode::Gene, {{"A", led(100, 0, true)}}, 100)) ==
              std::vector<std::string>{"A"},
          "Gene: exonic kept");
    // Gene: exon-of-A + intron-of-B -> A only (introns invisible in Gene).
    check(genes(apply_count_mode(CountMode::Gene,
                                 {{"A", led(100, 0, true)}, {"B", led(0, 100, true)}}, 100)) ==
              std::vector<std::string>{"A"},
          "Gene: exon-A + intron-B -> A only");
    // Gene: 50% exonic passes the gate; 30% exonic does not.
    check(!apply_count_mode(CountMode::Gene, {{"A", led(50, 50, true)}}, 100).empty(),
          "Gene: 50% exonic passes");
    check(apply_count_mode(CountMode::Gene, {{"A", led(30, 70, true)}}, 100).empty(),
          "Gene: 30% exonic excluded");

    // GeneFull: any body overlap kept, including purely intronic.
    check(genes(apply_count_mode(CountMode::GeneFull, {{"A", led(0, 100, true)}}, 100)) ==
              std::vector<std::string>{"A"},
          "GeneFull: intronic kept");
    // GeneFull: intergenic (empty ledger) -> nothing.
    check(apply_count_mode(CountMode::GeneFull, {}, 100).empty(), "GeneFull: intergenic excluded");
    // GeneFull: overlaps two gene bodies -> both (Unique drop happens in the caller).
    check(genes(apply_count_mode(CountMode::GeneFull,
                                 {{"A", led(100, 0, true)}, {"B", led(0, 50, true)}}, 100)) ==
              (std::vector<std::string>{"A", "B"}),
          "GeneFull: two bodies -> both");

    // ExonOverIntron: prefer the 100%-exon gene over the intron-overlap gene.
    check(genes(apply_count_mode(CountMode::GeneFullExonOverIntron,
                                 {{"A", led(100, 0, true)}, {"B", led(20, 80, true)}}, 100)) ==
              std::vector<std::string>{"A"},
          "ExonOverIntron: 100%-exon A preferred");
    // ExonOverIntron: neither is 100% exon -> keep both.
    check(genes(apply_count_mode(CountMode::GeneFullExonOverIntron,
                                 {{"A", led(80, 20, true)}, {"B", led(20, 80, true)}}, 100)) ==
              (std::vector<std::string>{"A", "B"}),
          "ExonOverIntron: no 100%-exon -> both");

    // Ex50pAS: prefer the >50%-exon gene.
    check(genes(apply_count_mode(CountMode::GeneFullEx50pAS,
                                 {{"A", led(60, 40, true)}, {"B", led(10, 90, true)}}, 100)) ==
              std::vector<std::string>{"A"},
          "Ex50pAS: >50%-exon A preferred");
    // Ex50pAS: a 100%-exonic ANTISENSE read is excluded for that gene.
    check(apply_count_mode(CountMode::GeneFullEx50pAS, {{"A", led(100, 0, false)}}, 100).empty(),
          "Ex50pAS: 100%-exon antisense excluded");
    // Ex50pAS: a 100%-exonic SENSE read is kept.
    check(!apply_count_mode(CountMode::GeneFullEx50pAS, {{"A", led(100, 0, true)}}, 100).empty(),
          "Ex50pAS: 100%-exon sense kept");
    // Ex50pAS: an antisense read that also has intron overlap is NOT excluded (only 100%-exon is).
    check(!apply_count_mode(CountMode::GeneFullEx50pAS, {{"A", led(50, 50, false)}}, 100).empty(),
          "Ex50pAS: antisense with intron kept");

    std::printf("pathtally ledger: PASS\n");
}

}  // namespace

int main() {
    run();
    return 0;
}
