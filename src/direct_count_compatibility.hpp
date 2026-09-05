#pragma once

#include "direct_count_assignment.hpp"
#include "path_identity_ledger.hpp"

#include <compare>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace pancollapse::direct_count {

inline constexpr std::string_view kCompatibilityAlgorithmId =
    "pancollapse-ex50pas-compatibility-v1";
inline constexpr std::uint64_t kMaximumCompatibilityRowsPerBatch = 1U << 20;
inline constexpr std::uint64_t kMaximumCompatibilityStringBytesPerBatch =
    1ULL << 30;

// D079's deliberately policy-neutral, complete input denominator.
enum class MoleculeStatus : std::uint8_t { valid, missing, malformed, unsupported };
enum class StructuralLayer : std::uint8_t { spliced, unspliced };

struct MoleculeStatusCounts {
    std::uint64_t valid = 0;
    std::uint64_t missing = 0;
    std::uint64_t malformed = 0;
    std::uint64_t unsupported = 0;
    auto operator<=>(const MoleculeStatusCounts&) const = default;
};

struct ExactCompatibilityFact {
    std::string canonical_transcript;
    std::string locus_parent;
    std::string path;
    std::string unique_parent;
    std::int64_t score = 0;
    EvidenceTier tier = EvidenceTier::exon;
    EvidenceStrand strand = EvidenceStrand::forward;
    auto operator<=>(const ExactCompatibilityFact&) const = default;
};

struct StructuralCompatibilityFact {
    std::string canonical_transcript;
    StructuralLayer layer = StructuralLayer::spliced;
    std::int64_t score = 0;
    EvidenceStrand strand = EvidenceStrand::forward;
    std::vector<std::string> winning_paths;
    std::vector<std::string> winning_parents;
    auto operator<=>(const StructuralCompatibilityFact&) const = default;
};

struct CompatibilityFactSet {
    // True when complete GAMP-derived provenance was available for this state.
    // It is evidence metadata, never a count-policy decision.
    bool complete_provenance = true;
    std::vector<ExactCompatibilityFact> exact;
    std::vector<StructuralCompatibilityFact> structural;
    auto operator<=>(const CompatibilityFactSet&) const = default;
};

struct ReadCompatibilityRow {
    std::uint64_t ordinal = 0;
    std::string original_name;
    MoleculeStatus molecule_status = MoleculeStatus::valid;
    std::optional<std::string> raw_barcode;
    std::optional<std::string> barcode_quality;
    std::optional<std::string> umi;
    std::optional<std::string> umi_quality;
    std::uint64_t fact_set_id = 0;  // writer input id; remapped to canonical intern id
};

struct CompatibilityInputIdentity {
    std::string role;
    std::string sha256;
    std::uint64_t size_bytes = 0;
    auto operator<=>(const CompatibilityInputIdentity&) const = default;
};
struct CompatibilityBundle {
    std::vector<ReadCompatibilityRow> reads;
    // Input IDs are indexes in this vector.  Output IDs are deterministic,
    // zero-based intern IDs after canonicalization and deduplication.
    std::vector<CompatibilityFactSet> fact_sets;
    std::vector<CompatibilityInputIdentity> inputs;
    std::string producer_version;
    // Compatibility can be replayed only by a binary that implements this
    // exact graph-to-tier algorithm. This is intentionally independent of
    // annotation metadata and count policy.
    std::string compatibility_algorithm_id;
    // Content address over the structure-bearing path-ledger fields. Metadata
    // and count-policy changes may keep this ID while changing the full
    // count-fact bundle identity.
    std::string structural_surface_id;
    std::uint64_t input_records = 0;
    std::uint64_t input_read_groups = 0;
    MoleculeStatusCounts molecule_status_counts;
    // Populated by the reader. Writers derive it from canonical manifest
    // content and reject a caller-supplied value.
    std::string content_id;
};

// The immutable identity known before the first input read is decoded. It is
// deliberately separate from CompatibilityBundle so production writers do not
// need to retain the ordered read denominator in memory.
struct CompatibilityBundleIdentity {
    std::vector<CompatibilityInputIdentity> inputs;
    std::string producer_version;
    std::string compatibility_algorithm_id;
    std::string structural_surface_id;
    std::uint64_t input_records = 0;
    // Independently accumulated by the GAMP producer and required to equal
    // the writer's ordered row count at publication.
    std::uint64_t input_read_groups = 0;
};

// Hashes only fields that define the replayable transcript/exon/path surface;
// gene categories, correction tags, competition classes, and count policy are
// intentionally excluded.
std::string compatibility_structural_surface_id(
    const path_identity::PathIdentityLedger& path_ledger);

struct CompatibilityTableIdentity {
    std::string relative_path;
    std::uint64_t rows = 0;
    std::uint64_t size_bytes = 0;
    std::string sha256;
    std::string logical_sha256;
};
struct CompatibilityWriteOptions {
    std::filesystem::path output_directory;
    std::uint64_t parquet_row_group_rows = 65536;
    // Bounds the combined UTF-8 payload used to build any one fact-table
    // RecordBatch. This remains below Arrow's 32-bit StringArray offset limit;
    // tests may lower it to exercise byte-triggered flushing cheaply.
    std::uint64_t parquet_max_string_bytes_per_batch = 256ULL << 20;
};
struct CompatibilityWriteReceipt {
    std::vector<CompatibilityTableIdentity> tables;
    std::string content_id;
    std::string manifest_sha256;
};

// Production writer. append() requires exact input order and immediately
// writes the read row to a checksummed ZSTD spool. Only unique canonical fact
// sets remain resident. finalize() remaps their provisional IDs, writes the
// four deterministic Parquet tables, validates the staged bundle, and atomically
// publishes the destination.
class CompatibilityBundleWriter {
  public:
    CompatibilityBundleWriter(CompatibilityBundleIdentity identity,
                              CompatibilityWriteOptions options);
    CompatibilityBundleWriter(const CompatibilityBundleWriter&) = delete;
    CompatibilityBundleWriter& operator=(const CompatibilityBundleWriter&) = delete;
    CompatibilityBundleWriter(CompatibilityBundleWriter&&) noexcept;
    CompatibilityBundleWriter& operator=(CompatibilityBundleWriter&&) noexcept;
    ~CompatibilityBundleWriter();

    // May be called once after streamed input hashes become available. The
    // constructor may receive an empty input list for this purpose.
    void set_inputs(std::vector<CompatibilityInputIdentity> inputs,
                    std::uint64_t input_records,
                    std::uint64_t input_read_groups);
    void append(ReadCompatibilityRow read, CompatibilityFactSet facts);
    CompatibilityWriteReceipt finalize();
    std::uint64_t rows_appended() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Production reader metadata. The fact dictionary is intentionally resident;
// read rows are delivered in bounded batches by CompatibilityBundleReader.
struct CompatibilityBundleIndex {
    std::vector<CompatibilityFactSet> fact_sets;
    std::vector<CompatibilityInputIdentity> inputs;
    std::string producer_version;
    std::string compatibility_algorithm_id;
    std::string structural_surface_id;
    std::string content_id;
    std::uint64_t input_records = 0;
    std::uint64_t input_read_groups = 0;
    MoleculeStatusCounts molecule_status_counts;
    std::uint64_t read_rows = 0;
};

class CompatibilityBundleReader {
  public:
    explicit CompatibilityBundleReader(const std::filesystem::path& directory);
    CompatibilityBundleReader(const CompatibilityBundleReader&) = delete;
    CompatibilityBundleReader& operator=(const CompatibilityBundleReader&) = delete;
    CompatibilityBundleReader(CompatibilityBundleReader&&) noexcept;
    CompatibilityBundleReader& operator=(CompatibilityBundleReader&&) noexcept;
    ~CompatibilityBundleReader();

    const CompatibilityBundleIndex& index() const;
    void for_each_read_batch(
        const std::function<void(std::span<const ReadCompatibilityRow>)>& callback,
        std::uint64_t maximum_batch_rows = 65536) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Test-scale convenience wrapper. Production call sites must use
// CompatibilityBundleWriter so the read denominator stays bounded-memory.
CompatibilityWriteReceipt write_compatibility_bundle(
    CompatibilityBundle bundle, const CompatibilityWriteOptions& options);

// Test-scale convenience reader. Production call sites must use
// CompatibilityBundleReader and consume ordered batches.
CompatibilityBundle read_compatibility_bundle(const std::filesystem::path& directory);


}  // namespace pancollapse::direct_count
