#include <iostream>
#include <stdexcept>
#include "direct_count_assignment.hpp"
using namespace pancollapse::direct_count;

static void expect(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}

static AssignmentCandidate c(std::string gene, EvidenceTier tier, EvidenceStrand strand,
                             int score = 100) {
  AssignmentCandidate x;
  x.gene = std::move(gene);
  x.tier = tier;
  x.strand = strand;
  x.score = score;
  x.competition = CompetitionClass::primary;
  return x;
}
int main() {
 try {
  AssignmentFacts f;
  auto r = resolve_assignment(ProfileId::cr7_v1, f);
  expect(!r.barcode_correction_eligible && !r.reaches_umi_filter,
         "featureless evidence is prior-only");

  auto category_only = c("excluded", EvidenceTier::exon, EvidenceStrand::forward);
  category_only.categories = {"readthrough_transcript"};
  f.exact = {category_only};
  r = resolve_assignment(ProfileId::cr7_v1, f);
  expect(r.terminal == AssignmentTerminal::unassigned_no_evidence &&
             r.barcode_correction_eligible && !r.reaches_umi_filter,
         "category-only exit precedes UMI filtering");
  const CountTerminal category_terminal =
      terminal_after_umi_filter(r, BasicUmiStatus::contains_n);
  expect(category_terminal.name == "unassigned_no_evidence" &&
             !(category_terminal.reasons & reason_umi_n),
         "early evidence exit is not relabeled as a UMI drop");

  f.exact = {c("R", EvidenceTier::exon, EvidenceStrand::reverse), c("P", EvidenceTier::partial_exon, EvidenceStrand::forward)};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"P"} &&
             r.barcode_correction_eligible && r.reaches_umi_filter,
         "sense rank selection reaches barcode and UMI stages");
  f.exact = {c("A", EvidenceTier::exon, EvidenceStrand::forward, 100), c("B", EvidenceTier::exon, EvidenceStrand::forward, 94)};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.genes == std::vector<std::string>{"A"} && (r.reasons & reason_score_window_pruned), "score window");
  auto bad = c("bad", EvidenceTier::exon, EvidenceStrand::forward); bad.categories = {"no_source_provenance"}; bad.strong_local_support = true;
  f.exact = {bad}; r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.genes == std::vector<std::string>{"bad"} && (r.reasons & reason_strong_support_exemption), "strong-support exemption");
  bad.protected_primary_protein_nested_host = true;
  auto clean = c("clean", EvidenceTier::exon, EvidenceStrand::forward);
  f.exact = {bad, clean};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"clean"} &&
             !(r.reasons & reason_strong_support_exemption),
         "protected nested host is not exempted");
  const auto unprotected = effective_profile(
      ProfileId::pansc_strict_v1,
      {parse_profile_override(
          "pansc-strict-v1:protect-primary-protein-nested-hosts=false")});
  r = resolve_assignment(unprotected, f);
  expect(r.genes == std::vector<std::string>({"bad", "clean"}) &&
             (r.reasons & reason_strong_support_exemption),
         "protection override disables the host guard");
  f.exact = {c("anti", EvidenceTier::exon, EvidenceStrand::reverse)}; f.gene_fallback = {c("gene", EvidenceTier::gene, EvidenceStrand::forward)};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.genes == std::vector<std::string>{"gene"} && (r.reasons & reason_gene_fallback), "exact-strand Gene fallback");
  // The post-resolution category retry precedes the reverse veto: its restored
  // sense P explanation wins instead of taking the G fallback.
  auto excluded_sense = c("sense", EvidenceTier::partial_exon, EvidenceStrand::forward);
  excluded_sense.categories = {"no_source_provenance"};
  f.exact = {c("anti", EvidenceTier::exon, EvidenceStrand::reverse), excluded_sense};
  f.gene_fallback = {c("wrong_g", EvidenceTier::gene, EvidenceStrand::forward)};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"sense"} &&
         (r.reasons & reason_no_source_retry) && !(r.reasons & reason_gene_fallback), "no-source retry precedes reverse veto");
  // Exact Gene fallback itself filters only the sense G subset.  A clean
  // antisense G row must not prevent last-resort restoration of this sense row.
  auto tagged_g = c("sense_g", EvidenceTier::gene, EvidenceStrand::forward);
  tagged_g.categories = {"no_source_provenance"};
  f.exact = {c("anti", EvidenceTier::exon, EvidenceStrand::reverse)};
  f.gene_fallback = {c("anti_g", EvidenceTier::gene, EvidenceStrand::reverse), tagged_g};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"sense_g"} &&
         (r.reasons & reason_gene_fallback) && (r.reasons & reason_tagged_last_resort), "tagged sense Gene fallback");
  auto primary = c("P", EvidenceTier::exon, EvidenceStrand::forward); primary.competition = CompetitionClass::primary;
  auto fallback = c("F", EvidenceTier::exon, EvidenceStrand::forward); fallback.competition = CompetitionClass::fallback;
  f.exact = {primary, fallback}; f.gene_fallback.clear(); r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.genes == std::vector<std::string>{"P"}, "primary suppresses fallback");
  auto alias = c("PAR_Y", EvidenceTier::exon, EvidenceStrand::forward); alias.equivalence_gene = "PAR";
  auto alias2 = c("PAR_X", EvidenceTier::exon, EvidenceStrand::forward); alias2.equivalence_gene = "PAR";
  f.exact = {alias, alias2}; r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.genes == std::vector<std::string>{"PAR"} && (r.reasons & reason_equivalence_collapsed), "equivalence collapse");
  auto host = c("host", EvidenceTier::body, EvidenceStrand::forward); host.competition = CompetitionClass::primary; host.gene_types = {"protein_coding"};
  auto child = c("child", EvidenceTier::body, EvidenceStrand::forward); child.competition = CompetitionClass::primary; child.gene_types = {"protein_coding"};
  // The frozen projected ledger encodes a positive child preference by putting
  // the reversed edge on the host row.
  host.nested_host = "child";
  f.exact = {host, child}; r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.genes == std::vector<std::string>{"child"}, "projected nesting direction");
  r = resolve_assignment(ProfileId::cr7_v1, f); expect(r.genes.size() == 2, "CR7 ignores panSC nesting"); // CR7 excludes nesting/body/strong-support branches.
  host.nested_host = "child";
  child.nested_host = "host";
  f.exact = {host, child};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes.size() == 2 && !(r.reasons & reason_nested_host_preferred),
         "nested cycle remains unresolved");
  auto four = c("four", EvidenceTier::body, EvidenceStrand::forward); four.competition = CompetitionClass::primary; four.gene_types = {"protein_coding"}; four.body_sample_support = 4;
  auto one = c("one", EvidenceTier::body, EvidenceStrand::forward); one.competition = CompetitionClass::primary; one.gene_types = {"protein_coding"}; one.body_sample_support = 1;
  f.exact = {four, one}; r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.genes == std::vector<std::string>{"four"} && (r.reasons & reason_body_support_preferred), "body support ratio");
  auto par_a = c("par_a", EvidenceTier::body, EvidenceStrand::forward);
  par_a.equivalence_gene = "par";
  par_a.gene_types = {"protein_coding"};
  par_a.body_support_units = {"s1", "s2"};
  auto par_b = c("par_b", EvidenceTier::body, EvidenceStrand::forward);
  par_b.equivalence_gene = "par";
  par_b.gene_types = {"protein_coding"};
  par_b.body_support_units = {"s2", "s3", "s4"};
  auto runner = c("runner", EvidenceTier::body, EvidenceStrand::forward);
  runner.gene_types = {"protein_coding"};
  runner.body_support_units = {"s5"};
  f.exact = {par_a, par_b, runner};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"par"} &&
             (r.reasons & reason_body_support_preferred),
         "body support unions distinct samples across equivalence aliases");
  auto excluded_high = c("excluded_high", EvidenceTier::exon, EvidenceStrand::forward, 100);
  excluded_high.categories = {"readthrough_transcript"};
  auto clean_low = c("clean_low", EvidenceTier::exon, EvidenceStrand::forward, 94);
  f.exact = {excluded_high, clean_low};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"excluded_high"} &&
             (r.reasons & reason_score_window_pruned) &&
             (r.reasons & reason_tagged_last_resort),
         "producer score window precedes category filtering");
  auto absent = c("", EvidenceTier::exon, EvidenceStrand::forward); absent.identity_known = false; absent.categories = {"no_source_provenance"};
  f.exact = {absent}; r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.terminal == AssignmentTerminal::unassigned_no_identity && (r.reasons & reason_no_source_retry) && r.barcode_correction_eligible && r.reaches_umi_filter, "post-resolution missing identity reaches UMI stage");
  const CountTerminal umi_terminal =
      terminal_after_umi_filter(r, BasicUmiStatus::contains_n);
  expect(umi_terminal.name == "umi_n" && (umi_terminal.reasons & reason_umi_n) &&
             (umi_terminal.reasons & reason_no_source_retry),
         "UMI terminal overrides unassigned class while retaining assignment reasons");
  f.has_complete_provenance = false; r = resolve_assignment(ProfileId::pansc_strict_v1, f); expect(r.terminal == AssignmentTerminal::incomplete_facts && (r.reasons & reason_unavailable_fact), "incomplete fact terminal");
  std::cout << "direct_count_assignment smoke: ok\n";
 } catch (const std::exception& error) {
   std::cerr << error.what() << '\n';
   return 1;
 }
}
