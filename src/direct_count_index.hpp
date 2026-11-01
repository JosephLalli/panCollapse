#pragma once

#include "count_reference.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace pancollapse::direct_index {

using GeneRecord = count_reference::GeneRecord;
using PolicyRecord = count_reference::PolicyRecord;
using ModelRecord = count_reference::ModelRecord;
using Posting = count_reference::Posting;
using JunctionOwner = count_reference::JunctionOwner;

struct Metadata {
    std::vector<std::string> strings, feature_catalog;
    std::vector<std::vector<std::uint32_t>> sets;
    std::vector<GeneRecord> genes;
    std::vector<PolicyRecord> policies;
    std::vector<ModelRecord> models;
    std::string content_id, provenance_json;
};

// Counts describe the compact, published direct-facts representation.  They
// are deliberately independent of graph indexes and read-time scoring state.
struct Stats {
    std::uint64_t nodes = 0;
    std::uint64_t unique_lists = 0;
    std::uint64_t stored_postings = 0;
    std::uint64_t models = 0;
    std::uint64_t splice_pairs = 0;
    std::uint64_t splice_targets = 0;
    std::uint64_t junction_rows = 0;
    std::uint64_t junction_owners = 0;
    std::uint64_t index_bytes = 0;
};

// A complete direct-facts index.  It deliberately contains no graph paths,
// positions, or scoring data.
class Writer {
  public:
    Writer(std::filesystem::path output, Metadata metadata, std::uint64_t max_node_id,
           std::size_t memory_limit_bytes, unsigned testing_digest_bits = 64);
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;
    void append_node(std::uint64_t id, std::uint64_t length, std::span<const Posting> postings);
    void append_splice(std::uint64_t physical_a, std::uint64_t physical_b,
                       std::span<const std::uint32_t> targets);
    void append_junction(std::uint64_t oriented_from, std::uint64_t oriented_to,
                         std::span<const JunctionOwner> owners);
    // Freezes node facts while retaining their mapped/owned compact spans for
    // the compiler's derived-fact pass.  Appending node facts afterwards fails.
    void seal_nodes();
    std::optional<count_reference::NodeView> node_view(std::uint64_t id) const;
    Stats stats() const;
    void finish();
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

bool is_index(const std::filesystem::path& path) noexcept;

class Source final : public count_reference::Source {
  public:
    ~Source() override;
    Source(Source&&) noexcept;
    Source& operator=(Source&&) noexcept;
    Source(const Source&) = delete;
    Source& operator=(const Source&) = delete;
    static std::unique_ptr<Source> load(const std::filesystem::path& path);
    const std::vector<std::string>& strings() const override;
    const std::vector<std::vector<std::uint32_t>>& sets() const override;
    std::span<const GeneRecord> genes() const override;
    std::span<const PolicyRecord> policies() const override;
    const std::string& content_id() const override;
    std::size_t model_count() const override;
    ModelRecord model(std::uint32_t id) const override;
    std::size_t scoring_class_count() const override { return 0; }
    count_reference::ScoringClassRecord scoring_class(std::uint32_t) const override;
    std::optional<count_reference::NodeView> node_view(std::uint64_t id) const override;
    std::span<const std::uint32_t> splice_targets(std::uint64_t, std::uint64_t) const override;
    std::span<const JunctionOwner> junction_owners(std::uint64_t, std::uint64_t) const override;
    Stats stats() const;
    std::vector<std::string> take_feature_catalog();
    const std::string& provenance_json() const;
    // Direct indexes cannot answer legacy graph/scoring queries.
    const count_reference::NodeRecord* node(std::uint64_t) const override;
    std::span<const Posting> node_postings(const count_reference::NodeRecord&) const override;
    std::span<const count_reference::ScoringPosting> node_scoring(const count_reference::NodeRecord&) const override;
    std::span<const count_reference::ScoringBinding> scoring_bindings(const count_reference::ScoringClassRecord&) const override;
    std::span<const count_reference::EdgePosting> edge(std::uint64_t, std::uint64_t) const override;
  private:
    Source();
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace pancollapse::direct_index
