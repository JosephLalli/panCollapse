#pragma once

#include "direct_count_assignment.hpp"
#include "path_identity_ledger.hpp"

#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace pancollapse::direct_count {

inline constexpr std::string_view kCountFactBundleSchema = "panSC-count-facts-v1";

struct CountFactFile {
    std::filesystem::path path;
    std::string schema;
    std::string sha256;
    std::string source_sha256;
    std::uint64_t size_bytes = 0;
    std::uint64_t source_size_bytes = 0;
};

struct CountFactBundle {
    std::filesystem::path root;
    std::filesystem::path manifest_path;
    std::string content_id;
    std::string transcript_filter_mode;
    std::map<std::string, CountFactFile> files;
};

// Opens a bundle directory or its MANIFEST.json, validates its content address,
// resolves only paths contained by the bundle root, and verifies every bundled
// file's size and SHA-256. This is intentionally complete before the caller
// deserializes XG or reads one GAMP record.
CountFactBundle load_count_fact_bundle(const std::filesystem::path& bundle_or_manifest);

struct GeneFact {
    std::string count_gene;
    std::vector<std::string> gene_types;
    std::optional<std::string> gene_name;
};

struct ProfileGeneFact {
    ProfileId profile_id = ProfileId::cr7_v1;
    std::string count_gene;
    CompetitionClass competition = CompetitionClass::unknown;
    std::string competition_reason;
    std::string equivalence_gene;
    std::string source_annotation;
    std::string nested_host;
};

struct ParentFact {
    std::string unique_parent;
    std::string count_gene;
    std::string runtime_gene;
    std::string canonical_transcript;
    std::string sample;
    std::string feature_layer;
    std::vector<std::string> raw_categories;
    bool strong_local_support = false;
    bool protected_primary_protein_nested_host = false;
    bool novel_paralog = false;
    std::string novel_origin_gene;
};

class CountFactCatalog {
  public:
    static CountFactCatalog load(const CountFactBundle& bundle,
                                 const path_identity::PathIdentityLedger& path_ledger);

    const ParentFact& parent(std::string_view unique_parent) const;
    const GeneFact& gene(std::string_view count_gene) const;
    const ProfileGeneFact& profile_gene(ProfileId profile_id,
                                        std::string_view count_gene) const;
    const std::map<std::string, ParentFact, std::less<>>& parents() const {
        return parents_;
    }
    const std::map<std::string, GeneFact, std::less<>>& genes() const {
        return genes_;
    }

    AssignmentCandidate candidate(const EffectiveProfile& profile,
                                  std::string_view unique_parent, std::int64_t score,
                                  EvidenceTier tier, EvidenceStrand strand,
                                  std::uint32_t body_sample_support = 0) const;

    AssignmentCandidate candidate(ProfileId profile_id,
                                  std::string_view unique_parent, std::int64_t score,
                                  EvidenceTier tier, EvidenceStrand strand,
                                  std::uint32_t body_sample_support = 0) const {
        return candidate(effective_profile(profile_id), unique_parent, score, tier,
                         strand, body_sample_support);
    }

  private:
    std::map<std::string, ParentFact, std::less<>> parents_;
    std::map<std::string, GeneFact, std::less<>> genes_;
    std::map<ProfileId, std::map<std::string, ProfileGeneFact, std::less<>>>
        profile_genes_;
};

std::string sha256_file(const std::filesystem::path& path);

}  // namespace pancollapse::direct_count
