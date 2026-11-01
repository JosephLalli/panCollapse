#pragma once

#include "count_reference.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace pancollapse::native_count {

inline constexpr std::uint32_t exon_path = 1U;
inline constexpr std::uint32_t body_path = 2U;

// One annotated GBWT path, not one occurrence or one graph node. model_first
// indexes incident PairRecord IDs in model_refs. Each row retains its own raw
// Parent policy/support/strand/target; PairRecord alone determines exon/body role.
struct PathRecord {
    std::uint64_t model_first = 0;
    std::uint32_t transcript_string = count_reference::missing;
    std::uint32_t policy = count_reference::missing;
    std::uint32_t support_set = 0, flags = 0, strand = 0, model_count = 0;
};
static_assert(sizeof(PathRecord) == 32);
struct PairRecord { std::uint32_t exon = 0, body = 0; };
static_assert(sizeof(PairRecord) == 8);

struct Data {
    std::vector<std::string> strings;
    std::vector<std::string> feature_catalog;
    std::vector<std::vector<std::uint32_t>> sets;
    std::vector<count_reference::GeneRecord> genes;
    std::vector<count_reference::PolicyRecord> policies;
    std::vector<std::uint32_t> path_lookup;
    std::vector<PathRecord> paths;
    std::vector<PairRecord> pairs;
    std::vector<std::uint32_t> model_refs;
    std::uint64_t gbwt_size_bytes = 0;
    std::string content_id;
};

void write(const std::filesystem::path& path, const Data& data);
bool is_index(const std::filesystem::path& path) noexcept;

// The mapped file must remain immutable for the loaded instance's lifetime.
// Successful deep row checks are memoized; access bounds remain checked.
class Index {
  public:
    Index();
    ~Index();
    Index(Index&&) noexcept;
    Index& operator=(Index&&) noexcept;
    Index(const Index&) = delete;
    Index& operator=(const Index&) = delete;
    static Index load(const std::filesystem::path& path);
    const std::vector<std::string>& strings() const;
    const std::vector<std::string>& feature_catalog() const;
    std::vector<std::string> take_feature_catalog();
    const std::vector<std::vector<std::uint32_t>>& sets() const;
    std::span<const count_reference::GeneRecord> genes() const;
    std::span<const count_reference::PolicyRecord> policies() const;
    const std::string& content_id() const;
    std::size_t path_count() const;
    std::uint64_t gbwt_size_bytes() const;
    std::span<const PathRecord> paths() const;
    std::span<const PairRecord> pairs() const;
    std::uint32_t path_row(std::uint64_t gbwt_path_id) const;
    std::span<const std::uint32_t> model_ids(const PathRecord& path) const;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

struct BuildOptions {
    std::filesystem::path source_index, count_bundle, gbwt, output;
    std::size_t threads = 1;
};
// Metadata binding only: never extract a path or inspect graph-node records.
void build(const BuildOptions& options);

} // namespace pancollapse::native_count
