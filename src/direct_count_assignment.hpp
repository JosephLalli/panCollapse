#pragma once

#include <cstdint>
#include <limits>
#include <optional>
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
  // Every gene the read overlaps is FALLBACK in every profile and there are at least two of
  // them, so no profile can count it; tally and exact evidence were skipped.
  reason_fallback_only_skipped = 1ULL << 17,
  // Input-complexity guard, before graph assignment or UMI filtering.
  reason_high_complexity = 1ULL << 18,
};
enum class AssignmentTerminal : std::uint8_t {
  assigned_unique, assigned_multigene, unassigned_no_evidence,
  unassigned_opposite_strand, unassigned_no_identity, incomplete_facts,
  unassigned_high_complexity,
};
inline constexpr size_t kAssignmentTerminalCount = 7;

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
    case AssignmentTerminal::unassigned_high_complexity: return "unassigned_high_complexity";
  }
  return "unknown";
}

// The resolver has one policy engine for its diagnostic text representation and
// its native numeric representation.  These are intentionally aliases rather
// than independently maintained structs: a policy branch cannot acquire
// representation-specific fields.
template<class Id>
Id assignment_missing_id();

template<>
inline std::string assignment_missing_id<std::string>() {
  return {};
}

inline constexpr std::uint32_t kNumericAssignmentMissing =
    std::numeric_limits<std::uint32_t>::max();

template<>
inline std::uint32_t assignment_missing_id<std::uint32_t>() {
  return kNumericAssignmentMissing;
}

template<class Id>
struct BasicAssignmentCandidate {
  Id gene = assignment_missing_id<Id>(); // output identity, nonmissing when identity_known
  Id equivalence_gene = assignment_missing_id<Id>(); // missing means no validated equivalence relation
  Id nested_host = assignment_missing_id<Id>(); // normalized host identity, if projected nesting applies
  std::int64_t score = 0;
  EvidenceTier tier = EvidenceTier::exon;
  EvidenceStrand strand = EvidenceStrand::forward;
  CompetitionClass competition = CompetitionClass::unknown;
  std::vector<Id> categories;
  std::vector<Id> gene_types;
  // Production candidates carry the sample identities themselves so policy
  // equivalence can union them after category/tier/strand selection. The count
  // field remains useful for compact unit fixtures and is populated from the
  // union before the dominance rule runs.
  std::vector<Id> body_support_units;
  std::uint32_t body_sample_support = 0;
  bool identity_known = true;
  bool novel_paralog = false;
  Id novel_origin_gene = assignment_missing_id<Id>();
  bool strong_local_support = false;
  bool protected_primary_protein_nested_host = false;

  bool operator==(const BasicAssignmentCandidate&) const = default;
};

template<class Id>
struct BasicAssignmentFacts {
  bool has_complete_provenance = true;
  std::vector<BasicAssignmentCandidate<Id>> exact;
  std::vector<BasicAssignmentCandidate<Id>> gene_fallback;
};

template<class Id>
struct BasicAssignmentResult {
  AssignmentTerminal terminal = AssignmentTerminal::unassigned_no_evidence;
  std::vector<Id> genes; // sorted normalized count identities
  std::uint64_t reasons = reason_none;
  EvidenceTier winning_tier = EvidenceTier::gene;
  EvidenceStrand winning_strand = EvidenceStrand::forward;
  // Feature-bearing typed-union evidence enters correct_cb. A strict subset
  // survives Python's early evidence exits and reaches the basic UMI filter.
  bool barcode_correction_eligible = false;
  bool reaches_umi_filter = false;
};

using AssignmentCandidate = BasicAssignmentCandidate<std::string>;
using AssignmentFacts = BasicAssignmentFacts<std::string>;
using AssignmentResult = BasicAssignmentResult<std::string>;
using NumericAssignmentCandidate = BasicAssignmentCandidate<std::uint32_t>;
using NumericAssignmentFacts = BasicAssignmentFacts<std::uint32_t>;
using NumericAssignmentResult = BasicAssignmentResult<std::uint32_t>;

// Scalar policy is copied once into the native profile. Its label vectors use
// strings in the diagnostic oracle and dictionary IDs in the native resolver.
template<class Id>
struct BasicAssignmentPolicy {
  StrandPolicy strand = StrandPolicy::forward;
  std::int64_t score_window = 5;
  MembershipClass membership = MembershipClass::cellranger_2024_a;
  NovelParalogPolicy novel_paralog = NovelParalogPolicy::lump;
  std::vector<Id> excluded_categories;
  std::vector<Id> post_resolution_fallback_categories;
  bool tagged_last_resort = false;
  bool exact_strand_gene_fallback = false;
  NestedHostRule nested_host_rule = NestedHostRule::none;
  NestedHostAction nested_host_action = NestedHostAction::none;
  std::optional<double> body_support_ratio;
  std::vector<Id> body_support_gene_types;
  bool strong_local_support_exemption = false;
  std::vector<Id> strong_support_clearable_categories;
  bool protect_primary_protein_nested_hosts = false;
};

struct CountTerminal {
  std::string name;
  std::uint64_t reasons = reason_none;
};

// Applies the count_cr molecule-stage precedence to an already-resolved graph
// assignment. UMI failures are terminal even when the assignment has no gene.
CountTerminal terminal_after_umi_filter(const AssignmentResult& assignment,
                                        BasicUmiStatus umi_status);

enum class AssignmentAlgorithm { legacy, node_score_v1 };
AssignmentResult resolve_assignment(const EffectiveProfile& profile,
                                    const AssignmentFacts& facts,
                                    AssignmentAlgorithm algorithm = AssignmentAlgorithm::legacy);

inline AssignmentResult resolve_assignment(ProfileId profile_id,
                                           const AssignmentFacts& facts) {
  return resolve_assignment(effective_profile(profile_id), facts);
}

} // namespace pancollapse::direct_count
