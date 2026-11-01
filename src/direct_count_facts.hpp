#pragma once

#include "direct_count_assignment.hpp"
#include "direct_count_compatibility.hpp"
#include "path_identity_ledger.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

namespace pancollapse::direct_count {

// The existing count-gene normalization, also used to partition conflicting
// internal transcript targets. It does not apply profile-specific gene lumping.
std::string normalized_count_gene_identity(const path_identity::AnnotationIdentity& identity);

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
    CompactIdentityText unique_parent;
    CompactIdentityText count_gene;
    CompactIdentityText runtime_gene;
    CompactIdentityText canonical_transcript;
    CompactIdentityText sample;
    CompactIdentityText feature_layer;
    std::vector<std::string> raw_categories;
    bool strong_local_support = false;
    bool protected_primary_protein_nested_host = false;
    bool novel_paralog = false;
    CompactIdentityText novel_origin_gene;
};

// Factories visit lexical ledger Parents. A row retains only a non-owning pointer
// to the finalized ledger identity and policy dictionary IDs. The ledger must
// outlive this table; its finalized immutable identity storage keeps those
// pointers stable across moving the ledger.
class ParentFactTable {
  public:
    static constexpr std::uint32_t kMissingId = std::numeric_limits<std::uint32_t>::max();

    struct CompactParentFact {
        const path_identity::AnnotationIdentity* identity = nullptr;
        std::uint32_t count_gene_id = kMissingId;
        std::uint32_t category_set_id = kMissingId;
        std::uint32_t novel_origin_gene_id = kMissingId;
        std::uint8_t flags = 0;
    };
    static_assert(sizeof(CompactParentFact) <= 24,
                  "Parent fact rows must stay within the compact 24-byte budget");

    // Iteration materializes a ParentFact by value. Its result belongs to that
    // expression/loop iteration only; callers must not bind a long-lived reference
    // expecting storage inside this table.
    using Row = std::pair<CompactIdentityText, ParentFact>;
    class const_iterator {
      public:
        using value_type = Row;
        using difference_type = std::ptrdiff_t;
        using iterator_category = std::forward_iterator_tag;
        using reference = value_type;
        using pointer = void;

        const_iterator() = default;
        value_type operator*() const { return table_->row_by_rank(rank_); }
        const_iterator& operator++() {
            ++rank_;
            return *this;
        }
        const_iterator operator++(int) {
            const_iterator prior = *this;
            ++*this;
            return prior;
        }
        friend bool operator==(const const_iterator&, const const_iterator&) = default;

      private:
        friend class ParentFactTable;
        const_iterator(const ParentFactTable* table, std::size_t rank)
            : table_(table), rank_(rank) {}

        const ParentFactTable* table_ = nullptr;
        std::size_t rank_ = 0;
    };

    const_iterator begin() const { return {this, 0}; }
    const_iterator end() const { return {this, rows_.size()}; }
    std::size_t size() const { return rows_.size(); }
    void reserve(std::size_t size) { rows_.reserve(size); }

    // Drop the compact Parent rows and their policy dictionaries. No Parent API
    // may be used afterwards; catalog gene and profile facts are retained.
    void clear() {
        std::vector<CompactParentFact>().swap(rows_);
        std::vector<CompactIdentityText>().swap(genes_);
        decltype(gene_ids_)().swap(gene_ids_);
        std::vector<std::vector<std::string>>().swap(category_sets_);
        decltype(category_ids_by_hash_)().swap(category_ids_by_hash_);
    }

    static constexpr std::size_t compact_record_bytes() { return sizeof(CompactParentFact); }
    std::size_t category_dictionary_size() const { return category_sets_.size(); }
    std::size_t category_reference_count() const { return rows_.size(); }
    std::size_t gene_dictionary_size() const { return genes_.size(); }

    const_iterator find(std::string_view key) const {
        const auto found = std::lower_bound(
            rows_.begin(), rows_.end(), key,
            [](const CompactParentFact& row, std::string_view value) {
                return row.identity->unique_parent.view() < value;
            });
        if (found == rows_.end() || found->identity->unique_parent != key) return end();
        return {this, static_cast<std::size_t>(found - rows_.begin())};
    }

    ParentFact parent_by_rank(std::size_t rank) const {
        if (rank >= rows_.size()) throw std::out_of_range("count Parent fact rank is out of range");
        return materialize(rows_[rank]);
    }
    const CompactIdentityText& parent_key(std::size_t rank) const {
        if (rank >= rows_.size()) throw std::out_of_range("count Parent fact rank is out of range");
        return rows_[rank].identity->unique_parent;
    }

    // `value` is an ingestion-only materialization. Identity text is validated
    // against the stable ledger row and then discarded rather than cached per Parent.
    void emplace(const path_identity::AnnotationIdentity* identity, ParentFact value) {
        if (identity == nullptr) throw std::runtime_error("count Parent fact lacks ledger identity");
        if (!rows_.empty() && !(rows_.back().identity->unique_parent < identity->unique_parent)) {
            throw std::runtime_error("count Parent facts must be unique and lexical");
        }
        if (value.unique_parent != identity->unique_parent || value.runtime_gene != identity->gene_id ||
            value.canonical_transcript != identity->canonical_transcript ||
            value.sample != identity->sample || value.feature_layer != identity->feature_layer) {
            throw std::runtime_error("count Parent fact identity disagrees with path identity ledger");
        }
        if (value.count_gene.empty()) throw std::runtime_error("count Parent fact lacks count gene");
        if (value.novel_paralog && value.novel_origin_gene.empty()) {
            throw std::runtime_error("novel-paralog Parent fact lacks origin gene");
        }
        rows_.push_back(CompactParentFact{
            identity,
            intern_gene(std::move(value.count_gene)),
            intern_categories(std::move(value.raw_categories)),
            value.novel_origin_gene.empty() ? kMissingId : intern_gene(std::move(value.novel_origin_gene)),
            static_cast<std::uint8_t>((value.strong_local_support ? kStrongLocalSupport : 0u) |
                                      (value.protected_primary_protein_nested_host ? kProtectedHost : 0u) |
                                      (value.novel_paralog ? kNovelParalog : 0u))});
    }

  private:
    static constexpr std::uint8_t kStrongLocalSupport = 1u;
    static constexpr std::uint8_t kProtectedHost = 2u;
    static constexpr std::uint8_t kNovelParalog = 4u;
    friend class CountFactCatalog;
    void compiled_dictionaries(std::vector<CompactIdentityText> genes,
                               std::vector<std::vector<std::string>> categories) {
        if (!rows_.empty() || !genes_.empty() || !category_sets_.empty())
            throw std::logic_error("compiled Parent dictionaries require empty storage");
        genes_ = std::move(genes); category_sets_ = std::move(categories);
        for (uint32_t i = 0; i < genes_.size(); ++i) gene_ids_.emplace(genes_[i], i);
        for (uint32_t i = 0; i < category_sets_.size(); ++i)
            category_ids_by_hash_[category_hash(category_sets_[i])].push_back(i);
    }
    void append_compiled(const path_identity::AnnotationIdentity* identity,
                         uint32_t gene, uint32_t categories, uint32_t origin, uint8_t flags) {
        if (!identity || gene >= genes_.size() || categories >= category_sets_.size() || flags > 7 ||
            (origin != kMissingId && origin >= genes_.size()) || ((flags & kNovelParalog) && origin == kMissingId) ||
            (!rows_.empty() && !(rows_.back().identity->unique_parent < identity->unique_parent)))
            throw std::runtime_error("invalid compiled Parent record");
        rows_.push_back({identity, gene, categories, origin, flags});
    }

    static std::size_t category_hash(const std::vector<std::string>& categories) {
        std::size_t result = categories.size();
        for (const std::string& category : categories) {
            result ^= std::hash<std::string_view>{}(category) + 0x9e3779b9u +
                      (result << 6u) + (result >> 2u);
        }
        return result;
    }
    std::uint32_t intern_gene(CompactIdentityText gene) {
        const auto existing = gene_ids_.find(gene);
        if (existing != gene_ids_.end()) return existing->second;
        if (genes_.size() >= kMissingId) {
            throw std::overflow_error("count Parent fact gene dictionary exceeds uint32 IDs");
        }
        const auto id = static_cast<std::uint32_t>(genes_.size());
        genes_.push_back(std::move(gene));
        gene_ids_.emplace(genes_.back(), id);
        return id;
    }
    std::uint32_t intern_categories(std::vector<std::string> categories) {
        const std::size_t hash = category_hash(categories);
        auto& candidates = category_ids_by_hash_[hash];
        for (const std::uint32_t id : candidates) {
            if (category_sets_[id] == categories) return id;
        }
        if (category_sets_.size() >= kMissingId) {
            throw std::overflow_error("count Parent fact category dictionary exceeds uint32 IDs");
        }
        const auto id = static_cast<std::uint32_t>(category_sets_.size());
        category_sets_.push_back(std::move(categories));
        candidates.push_back(id);
        return id;
    }
    ParentFact materialize(const CompactParentFact& row) const {
        if (row.identity == nullptr || row.count_gene_id >= genes_.size() ||
            row.category_set_id >= category_sets_.size() ||
            (row.novel_origin_gene_id != kMissingId && row.novel_origin_gene_id >= genes_.size())) {
            throw std::runtime_error("invalid compact count Parent fact");
        }
        ParentFact value;
        value.unique_parent = row.identity->unique_parent;
        value.count_gene = genes_[row.count_gene_id];
        value.runtime_gene = row.identity->gene_id;
        value.canonical_transcript = row.identity->canonical_transcript;
        value.sample = row.identity->sample;
        value.feature_layer = row.identity->feature_layer;
        value.raw_categories = category_sets_[row.category_set_id];
        value.strong_local_support = (row.flags & kStrongLocalSupport) != 0;
        value.protected_primary_protein_nested_host = (row.flags & kProtectedHost) != 0;
        value.novel_paralog = (row.flags & kNovelParalog) != 0;
        if (row.novel_origin_gene_id != kMissingId) {
            value.novel_origin_gene = genes_[row.novel_origin_gene_id];
        }
        return value;
    }
    Row row_by_rank(std::size_t rank) const { return {parent_key(rank), parent_by_rank(rank)}; }

    std::vector<CompactParentFact> rows_;
    std::vector<CompactIdentityText> genes_;
    std::unordered_map<CompactIdentityText, std::uint32_t,
                       CompactIdentityTextHash, CompactIdentityTextEqual> gene_ids_;
    std::vector<std::vector<std::string>> category_sets_;
    std::unordered_map<std::size_t, std::vector<std::uint32_t>> category_ids_by_hash_;
};

enum class AnnotationFormat { gff3, gtf };

struct AnnotationIndexBinding {
    std::string ledger_sha256;
    std::uint64_t ledger_size = 0;
};

struct AnnotationSourceFile {
    std::string sha256;
    std::uint64_t size_bytes = 0;
};

struct PreparedAnnotationReference;

class CountFactCatalog {
  public:
    // Output metadata for a finished node-space reference. No Parent table or
    // identity ledger is reconstructed by this factory.
    static CountFactCatalog from_prepared_genes(std::vector<GeneFact> genes,
        std::vector<ProfileGeneFact> policies, std::string content_id) {
        CountFactCatalog result;
        result.content_id_ = std::move(content_id);
        result.authority_ = "prepared-node-count-index";
        for (auto& gene : genes) {
            const auto key = gene.count_gene;
            result.genes_.emplace(key, std::move(gene));
        }
        for (auto& policy : policies) {
            const auto key = policy.count_gene;
            result.profile_genes_[policy.profile_id].emplace(key, std::move(policy));
        }
        return result;
    }
    static CountFactCatalog load(const CountFactBundle& bundle,
                                 const path_identity::PathIdentityLedger& path_ledger);

    // Build the same facts directly from a corrected annotation, with no bundle in the loop.
    // The correction chain writes every classifier verdict onto the GFF3 as a tag, and panSC's
    // own cache builder documents that its cache "must agree with direct annotation parsing",
    // so the cache is an acceleration artifact rather than an input. `reference` supplies the
    // gene tags CAT drops when it projects a model; they are inherited through source_gene.
    static CountFactCatalog from_annotation(
        const std::filesystem::path& annotation,
        const std::filesystem::path& reference,
        const path_identity::PathIdentityLedger& path_ledger,
        const std::vector<ProfileId>& profile_ids,
        AnnotationFormat annotation_format = AnnotationFormat::gff3,
        AnnotationFormat reference_format = AnnotationFormat::gff3,
        const std::string& ledger_sha256 = {});

    // Precompiled annotation facts, not graph geometry or a full reference snapshot.
    void write_annotation_index(const std::filesystem::path& destination,
                                const path_identity::PathIdentityLedger& path_ledger,
                                const AnnotationIndexBinding& binding) const;
    static CountFactCatalog load_annotation_index(
        const std::filesystem::path& filename,
        const path_identity::PathIdentityLedger& path_ledger,
        const AnnotationIndexBinding& binding,
        const std::vector<ProfileId>& required_profiles);
    void write_prepared_annotation_index(const std::filesystem::path& destination,
        const path_identity::PathIdentityLedger& path_ledger,
        const AnnotationIndexBinding& binding) const;
    static PreparedAnnotationReference load_prepared_annotation_index(
        const std::filesystem::path& filename, const std::vector<ProfileId>& required_profiles);
    // A locally trusted PCANIDX3 still receives the complete framing and bounds
    // parse, but avoids a second full-file SHA-256 pass before metadata-only
    // native compilation. Strict loaders retain checksum verification.
    static PreparedAnnotationReference load_prepared_annotation_index_trusted(
        const std::filesystem::path& filename, const std::vector<ProfileId>& required_profiles);

    // The identity of these facts, whichever factory built them: the bundle's own
    // content_id on the bundle route, and a digest over the annotation inputs on the
    // annotation route. `authority` names the route so a receipt reader never mistakes
    // one for the other, and callers never have to reach back to a route-specific object.
    const std::string& content_id() const { return content_id_; }
    const std::string& authority() const { return authority_; }
    const std::string& annotation_index_derivation_id() const { return annotation_index_derivation_id_; }
    const std::string& annotation_index_file_sha256() const { return annotation_index_file_sha256_; }
    std::uint64_t annotation_index_file_size() const { return annotation_index_file_size_; }

    // Parent facts materialize by value. They remain valid independently of the
    // catalog, but callers cannot use their address as a catalog-stable identity.
    ParentFact parent(std::string_view unique_parent) const;
    ParentFact parent_by_rank(std::size_t rank) const { return parents_.parent_by_rank(rank); }
    const CompactIdentityText& parent_key(std::size_t rank) const { return parents_.parent_key(rank); }
    const GeneFact& gene(std::string_view count_gene) const;
    const ProfileGeneFact& profile_gene(ProfileId profile_id,
                                        std::string_view count_gene) const;
    const ParentFactTable& parents() const {
        return parents_;
    }
    // Call after all Parent-dependent assignment and compatibility work. This
    // releases only Parent rows and their dictionaries; gene/profile facts and
    // catalog provenance remain available for output and UMI finalization.
    void release_parent_facts() { parents_.clear(); }
    const std::map<std::string, GeneFact, std::less<>>& genes() const {
        return genes_;
    }

    AssignmentCandidate candidate(const EffectiveProfile& profile,
                                  std::string_view unique_parent, std::int64_t score,
                                  EvidenceTier tier, EvidenceStrand strand,
                                  std::uint32_t body_sample_support = 0) const;

    // Avoid repeating the Parent-name lookup for an already materialized identity.
    // `parent_fact` may be a value returned by parent()/parent_by_rank(); only
    // this catalog must remain alive while policy is resolved.
    AssignmentCandidate candidate(const EffectiveProfile& profile,
                                  const ParentFact& parent_fact, std::int64_t score,
                                  EvidenceTier tier, EvidenceStrand strand,
                                  std::uint32_t body_sample_support = 0) const;

    AssignmentCandidate candidate_by_rank(const EffectiveProfile& profile, std::size_t rank,
                                          std::int64_t score, EvidenceTier tier,
                                          EvidenceStrand strand,
                                          std::uint32_t body_sample_support = 0) const;

    AssignmentCandidate candidate(ProfileId profile_id,
                                  std::string_view unique_parent, std::int64_t score,
                                  EvidenceTier tier, EvidenceStrand strand,
                                  std::uint32_t body_sample_support = 0) const {
        return candidate(effective_profile(profile_id), unique_parent, score, tier,
                         strand, body_sample_support);
    }

    // Rebinds policy-neutral compatibility evidence to the current corrected
    // annotation facts. The compatibility bundle owns observed tiers/scores;
    // this catalog owns gene eligibility, categories, equivalence, and other
    // profile-sensitive interpretation.
    AssignmentFacts assignment_facts(const EffectiveProfile& profile,
                                     const CompatibilityFactSet& facts) const;

    // Fail closed when replay evidence no longer describes the same
    // transcript/exon path surface. Metadata and policy may change, but D079
    // does not permit replay across altered transcript/exon structures.
    void validate_compatibility_structure(
        const CompatibilityFactSet& facts,
        const path_identity::PathIdentityLedger& path_ledger) const;

  private:
    void write_annotation_index_impl(const std::filesystem::path& destination,
        const path_identity::PathIdentityLedger& path_ledger,
        const AnnotationIndexBinding& binding, bool prepared) const;
    static CountFactCatalog load_annotation_index_impl(const std::filesystem::path& filename,
        const path_identity::PathIdentityLedger* legacy_ledger,
        const AnnotationIndexBinding* legacy_binding,
        const std::vector<ProfileId>& required_profiles,
        path_identity::PathIdentityLedger* prepared_ledger,
        AnnotationIndexBinding* prepared_binding, bool verify_checksum);
    std::string content_id_;
    std::string authority_;
    std::map<std::string, AnnotationSourceFile> annotation_sources_;
    AnnotationFormat annotation_format_ = AnnotationFormat::gff3;
    AnnotationFormat reference_format_ = AnnotationFormat::gff3;
    std::string annotation_index_derivation_id_;
    std::string annotation_index_file_sha256_;
    std::uint64_t annotation_index_file_size_ = 0;
    ParentFactTable parents_;
    std::map<std::string, GeneFact, std::less<>> genes_;
    std::map<ProfileId, std::map<std::string, ProfileGeneFact, std::less<>>>
        profile_genes_;
};

struct PreparedAnnotationReference {
    path_identity::PathIdentityLedger ledger;
    AnnotationIndexBinding binding;
    CountFactCatalog facts;
};

std::string sha256_file(const std::filesystem::path& path);

}  // namespace pancollapse::direct_count
