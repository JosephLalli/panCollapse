#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "direct_count.hpp"

namespace pancollapse::direct_count {

// A deliberately small, graph-free assignment seam.  Callers must derive these facts
// from the typed-union ledger before entering this resolver; missing facts are reported,
// never guessed.
enum class EvidenceTier : std::uint8_t { gene = 0, exon = 1, partial_exon = 2, body = 3 };
enum class EvidenceStrand : std::uint8_t { forward = 0, reverse = 1 };
enum class CompetitionClass : std::uint8_t { primary = 0, fallback = 1, unknown = 2 };
enum GeneTypeBit : std::uint32_t { protein_coding = 1U << 0 };
enum AssignmentReason : std::uint64_t {
  reason_none = 0,
  reason_no_exact_evidence = 1ULL << 0,
  reason_score_window_pruned = 1ULL << 1,
  reason_category_excluded = 1ULL << 2,
  reason_tagged_last_resort = 1ULL << 3,
  reason_no_source_retry = 1ULL << 4,
  reason_opposite_strand_veto = 1ULL << 5,
  reason_gene_fallback = 1ULL << 6,
  reason_equivalence_collapsed = 1ULL << 7,
  reason_fallback_suppressed = 1ULL << 8,
  reason_nested_host_preferred = 1ULL << 9,
  reason_body_support_preferred = 1ULL << 10,
  reason_strong_support_exemption = 1ULL << 11,
  reason_missing_identity = 1ULL << 12,
  reason_unavailable_fact = 1ULL << 13,
  // Post-assignment molecule terminals. These bits are carried by the optional
  // per-read diagnostic even when graph assignment itself succeeded or yielded
  // no gene, matching count_cr's UMI-before-gene terminal precedence.
  reason_umi_n = 1ULL << 14,
  reason_umi_homopolymer = 1ULL << 15,
  reason_barcode_uncorrectable = 1ULL << 16,
};
enum class AssignmentTerminal : std::uint8_t {
  assigned_unique, assigned_multigene, unassigned_no_evidence,
  unassigned_opposite_strand, unassigned_no_identity, incomplete_facts,
};
inline constexpr size_t kAssignmentTerminalCount = 6;

inline constexpr std::string_view evidence_tier_name(EvidenceTier tier) {
  switch (tier) {
    case EvidenceTier::gene: return "gene";
    case EvidenceTier::exon: return "exon";
    case EvidenceTier::partial_exon: return "partial_exon";
    case EvidenceTier::body: return "body";
  }
  return "unknown";
}

inline constexpr std::string_view assignment_terminal_name(AssignmentTerminal terminal) {
  switch (terminal) {
    case AssignmentTerminal::assigned_unique: return "assigned_unique";
    case AssignmentTerminal::assigned_multigene: return "assigned_multigene";
    case AssignmentTerminal::unassigned_no_evidence: return "unassigned_no_evidence";
    case AssignmentTerminal::unassigned_opposite_strand: return "unassigned_opposite_strand";
    case AssignmentTerminal::unassigned_no_identity: return "unassigned_no_identity";
    case AssignmentTerminal::incomplete_facts: return "incomplete_facts";
  }
  return "unknown";
}

struct AssignmentCandidate {
  std::string gene;                 // output identity, nonempty when identity_known
  std::string equivalence_gene;     // empty means no validated equivalence relation
  std::string nested_host;          // normalized host identity, if projected nesting applies
  std::int64_t score = 0;
  EvidenceTier tier = EvidenceTier::exon;
  EvidenceStrand strand = EvidenceStrand::forward;
  CompetitionClass competition = CompetitionClass::unknown;
  std::vector<std::string> categories;
  std::vector<std::string> gene_types;
  // Production candidates carry the sample identities themselves so policy
  // equivalence can union them after category/tier/strand selection. The count
  // field remains useful for compact unit fixtures and is populated from the
  // union before the dominance rule runs.
  std::vector<std::string> body_support_units;
  std::uint32_t body_sample_support = 0;
  bool identity_known = true;
  bool novel_paralog = false;
  std::string novel_origin_gene;
  bool strong_local_support = false;
  bool protected_primary_protein_nested_host = false;
};

struct AssignmentFacts {
  bool has_complete_provenance = true;
  std::vector<AssignmentCandidate> exact;
  std::vector<AssignmentCandidate> gene_fallback;
};

struct AssignmentResult {
  AssignmentTerminal terminal = AssignmentTerminal::unassigned_no_evidence;
  std::vector<std::string> genes; // sorted normalized count identities
  std::uint64_t reasons = reason_none;
  EvidenceTier winning_tier = EvidenceTier::gene;
  EvidenceStrand winning_strand = EvidenceStrand::forward;
  // Feature-bearing typed-union evidence enters correct_cb. A strict subset
  // survives Python's early evidence exits and reaches the basic UMI filter.
  bool barcode_correction_eligible = false;
  bool reaches_umi_filter = false;
};

struct CountTerminal {
  std::string name;
  std::uint64_t reasons = reason_none;
};

// Applies the count_cr molecule-stage precedence to an already-resolved graph
// assignment. UMI failures are terminal even when the assignment has no gene.
CountTerminal terminal_after_umi_filter(const AssignmentResult& assignment,
                                        BasicUmiStatus umi_status);

AssignmentResult resolve_assignment(const EffectiveProfile& profile,
                                    const AssignmentFacts& facts);

inline AssignmentResult resolve_assignment(ProfileId profile_id,
                                           const AssignmentFacts& facts) {
  return resolve_assignment(effective_profile(profile_id), facts);
}

} // namespace pancollapse::direct_count
