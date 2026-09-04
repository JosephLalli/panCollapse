#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace pancollapse::direct_count {

// These identifiers are persisted provenance. A released definition is immutable;
// policy experiments receive a derived-* identifier and their own effective hash.
enum class ProfileId { cr7_v1, pansc_strict_v1 };
enum class StrandPolicy { forward, reverse, both };
enum class MembershipClass { cellranger_2024_a, conjunctive_2024_a, all_annotated };
enum class NovelParalogPolicy { lump, ignore, separate };
enum class NestedHostRule { none, contradiction_only };
enum class NestedHostAction { none, prefer_independent_protein_child };

struct AssignmentPolicy {
    StrandPolicy strand = StrandPolicy::forward;
    std::int64_t score_window = 5;
    MembershipClass membership = MembershipClass::cellranger_2024_a;
    NovelParalogPolicy novel_paralog = NovelParalogPolicy::lump;
    std::vector<std::string> excluded_categories;
    std::vector<std::string> post_resolution_fallback_categories;
    bool tagged_last_resort = false;
    bool exact_strand_gene_fallback = false;
    NestedHostRule nested_host_rule = NestedHostRule::none;
    NestedHostAction nested_host_action = NestedHostAction::none;
    std::optional<double> body_support_ratio;
    std::vector<std::string> body_support_gene_types;
    bool strong_local_support_exemption = false;
    std::vector<std::string> strong_support_clearable_categories;
    bool protect_primary_protein_nested_hosts = false;
};

struct Profile {
    ProfileId id;
    std::string id_string;
    AssignmentPolicy assignment;
    // These primitives are deliberately not overrideable in v0.10.0.
    std::string barcode_algorithm;
    std::string umi_algorithm;
    std::string multigene_umi_algorithm;
};

enum class OverrideKey {
    strand,
    score_window,
    membership_class,
    novel_paralog_policy,
    excluded_categories,
    post_resolution_fallback_categories,
    tagged_last_resort,
    exact_strand_gene_fallback,
    nested_host_rule,
    nested_host_action,
    body_support_ratio,
    body_support_gene_types,
    strong_local_support_exemption,
    strong_support_clearable_categories,
    protect_primary_protein_nested_hosts,
};

using OverrideValue = std::variant<bool, std::int64_t, double, std::string,
                                   std::vector<std::string>>;

struct ProfileOverride {
    ProfileId base;
    OverrideKey key;
    OverrideValue value;
    std::string canonical_field;
    std::string canonical_value;
};

struct EffectiveProfile {
    Profile profile;
    std::vector<ProfileOverride> overrides;
    std::string effective_sha256;
};

const Profile& profile(ProfileId id);
ProfileId parse_profile_id(std::string_view value);

// Parse <base>:<field>=<value>. Unknown fields and incorrectly typed values fail
// here, before the caller opens GAMP. Barcode and UMI policy fields are absent by
// design, so attempts to override them are rejected as unknown.
ProfileOverride parse_profile_override(std::string_view expression);

// Duplicate fields, cross-base overrides, and contradictory combinations fail.
// When overrides are present the returned id_string is derived-<base>-<12 hex>.
EffectiveProfile effective_profile(ProfileId id,
                                   const std::vector<ProfileOverride>& overrides = {});

// The bundle carries independently certified policy facts for the two frozen
// membership universes. A sensitivity override that changes membership must
// select the matching fact universe; all-annotated retains the base profile's
// competition/equivalence annotations while disabling the membership filter.
ProfileId profile_policy_source(const EffectiveProfile& profile);

struct BarcodeRead {
    std::string raw_barcode;
    std::optional<std::string> quality;
    bool primary = true;
};
struct BarcodeCorrection {
    std::optional<std::string> barcode;
    bool corrected = false;
};

class BarcodeCorrector {
  public:
    explicit BarcodeCorrector(std::vector<std::string> whitelist,
                              double threshold = 0.975);
    void observe(const BarcodeRead& read);  // all reads, before feature filtering
    BarcodeCorrection correct(const BarcodeRead& read) const;

    const std::vector<std::string>& whitelist() const { return whitelist_; }
    std::uint64_t exact_count(std::string_view barcode) const;

  private:
    std::vector<std::string> whitelist_;
    std::map<std::string, std::uint64_t, std::less<>> exact_counts_;
    double threshold_;
};

enum class BasicUmiStatus : std::uint8_t {
    valid,
    contains_n,
    homopolymer,
    empty,
};
BasicUmiStatus basic_umi_status(std::string_view umi);
bool basic_umi_valid(std::string_view umi);
std::map<std::string, std::string>
collapse_1mm_cr(const std::map<std::string, std::uint64_t>& counts);

// This is the seam from graph assignment. A resolver supplies zero/one/many
// candidate genes. The direct Unique rule retains only singleton observations;
// MultiGeneUMI_CR subsequently resolves collisions among retained reads sharing
// a corrected UMI across genes.
struct AssignmentObservation {
    std::string barcode;
    std::string umi;
    std::vector<std::string> genes;
    std::uint64_t read_count = 1;
    std::uint64_t ordinal = 0;
};
struct MoleculeRecord {
    std::string barcode;
    std::string umi;
    std::string gene;
    std::uint64_t supporting_reads = 0;
    std::uint64_t raw_umis_collapsed = 0;
};
struct CountRecord {
    std::string barcode;
    std::string gene;
    std::uint64_t molecules = 0;
};
struct CountResult {
    std::vector<MoleculeRecord> molecules;
    std::vector<CountRecord> counts;
};

CountResult count_observations(std::vector<AssignmentObservation> observations);

}  // namespace pancollapse::direct_count
