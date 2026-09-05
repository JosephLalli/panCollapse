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
  // A category-excluded reverse exact row is restored by both the primary
  // tagged-last-resort path and the post-resolution retry. The retry must not
  // erase reverse-winner state before the sense Gene fallback runs.
  auto tagged_anti = c("tagged_anti", EvidenceTier::exon, EvidenceStrand::reverse);
  tagged_anti.categories = {"readthrough_transcript"};
  f.exact = {tagged_anti};
  f.gene_fallback = {c("gene", EvidenceTier::gene, EvidenceStrand::forward)};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"gene"} &&
             (r.reasons & reason_no_source_retry) &&
             (r.reasons & reason_tagged_last_resort) &&
             (r.reasons & reason_gene_fallback),
         "post-resolution retry preserves tagged reverse winner for Gene fallback");
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
  // The novel-paralog policy must override a ledger equivalence that conflicts
  // with the requested output identity in both directions.
  auto novel = c("novel_copy", EvidenceTier::exon, EvidenceStrand::forward);
  novel.equivalence_gene = "stale_equivalence";
  novel.novel_paralog = true;
  novel.novel_origin_gene = "origin";
  f.exact = {novel};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"origin"},
         "lump overrides conflicting novel-paralog equivalence");
  const auto separate = effective_profile(
      ProfileId::pansc_strict_v1,
      {parse_profile_override("pansc-strict-v1:novel-paralog-policy=separate")});
  r = resolve_assignment(separate, f);
  expect(r.genes == std::vector<std::string>{"novel_copy"},
         "separate overrides conflicting novel-paralog equivalence");
  // `both` retains both strands, but its reported sense and any body dominance
  // must not depend on typed-union candidate iteration order.
  const auto both = effective_profile(
      ProfileId::pansc_strict_v1,
      {parse_profile_override("pansc-strict-v1:strand=both"),
       parse_profile_override("pansc-strict-v1:exact-strand-gene-fallback=false")});
  auto both_forward = c("both_forward", EvidenceTier::body, EvidenceStrand::forward);
  both_forward.gene_types = {"protein_coding"};
  both_forward.body_sample_support = 4;
  auto both_reverse = c("both_reverse", EvidenceTier::body, EvidenceStrand::reverse);
  both_reverse.gene_types = {"protein_coding"};
  both_reverse.body_sample_support = 1;
  f.exact = {both_reverse, both_forward};
  const auto both_reverse_first = resolve_assignment(both, f);
  f.exact = {both_forward, both_reverse};
  const auto both_forward_first = resolve_assignment(both, f);
  expect(both_reverse_first.genes == std::vector<std::string>{"both_forward"} &&
             both_reverse_first.winning_strand == EvidenceStrand::forward &&
             both_reverse_first.genes == both_forward_first.genes &&
             both_reverse_first.winning_strand == both_forward_first.winning_strand &&
             both_reverse_first.reasons == both_forward_first.reasons,
         "strand both is invariant to candidate order");
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
  auto external_child = c("external_child", EvidenceTier::exon,
                          EvidenceStrand::forward);
  external_child.nested_host = "host_outside_count_policy";
  auto unrelated = c("unrelated", EvidenceTier::exon, EvidenceStrand::forward);
  f.exact = {external_child, unrelated};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>({"external_child", "unrelated"}) &&
             !(r.reasons & reason_nested_host_preferred),
         "nested host outside the count universe is an inert relation");
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
  // Production candidate construction copies the canonical equivalence row's
  // policy onto every alias. With that input contract, evidence support and row
  // order may choose different payload representatives but never different
  // nesting policy.
  auto equiv_a = c("equiv_a", EvidenceTier::body, EvidenceStrand::forward);
  equiv_a.equivalence_gene = "equiv";
  equiv_a.gene_types = {"protein_coding"};
  equiv_a.body_sample_support = 1;
  equiv_a.nested_host = "peer";
  auto equiv_b = c("equiv_b", EvidenceTier::body, EvidenceStrand::forward);
  equiv_b.equivalence_gene = "equiv";
  equiv_b.gene_types = {"protein_coding"};
  equiv_b.body_sample_support = 3;
  equiv_b.nested_host = "peer";
  auto peer = c("peer", EvidenceTier::body, EvidenceStrand::forward);
  peer.gene_types = {"protein_coding"};
  peer.body_sample_support = 1;
  f.exact = {equiv_a, equiv_b, peer};
  const auto equiv_ab = resolve_assignment(ProfileId::pansc_strict_v1, f);
  f.exact = {peer, equiv_b, equiv_a};
  const auto equiv_ba = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(equiv_ab.genes == std::vector<std::string>{"peer"} &&
             equiv_ab.genes == equiv_ba.genes && equiv_ab.reasons == equiv_ba.reasons,
         "canonical equivalence nesting is invariant to candidate order");
  equiv_a.body_sample_support = 5;
  f.exact = {equiv_b, peer, equiv_a};
  r = resolve_assignment(ProfileId::pansc_strict_v1, f);
  expect(r.genes == std::vector<std::string>{"peer"} &&
             (r.reasons & reason_nested_host_preferred),
         "canonical equivalence nesting is invariant to representative support");
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
