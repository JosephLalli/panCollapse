#pragma once

#include <gbwt/fast_locate.h>
#include <gbwtgraph/gbz.h>
#include <xg.hpp>

#include <cstdint>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace pancollapse {

// Process-local coordinates, never serialized. Paths normally fit 32 bits; widening
// is checked while accumulating lengths, including when a ledger length is wrong.
class PathPositions {
  public:
    void reserve(std::size_t size) { narrow_.reserve(size); }
    void push(std::uint64_t position);
    std::uint64_t at(std::size_t ordinal) const;
    bool wide() const { return !wide_.empty(); }
  private:
    std::vector<std::uint32_t> narrow_;
    std::vector<std::uint64_t> wide_;
};

struct GraphPathStep {
    handlegraph::path_handle_t path;
    handlegraph::handle_t handle;
    // XG node-occurrence queries do not expose a portable step ordinal. Their
    // native step handle supplies the base coordinate directly instead.
    std::optional<std::uint64_t> ordinal;
    std::optional<std::uint64_t> position;
    std::uint64_t base_position() const;
};

// Only the projection facts used by panCollapse cross this boundary. In particular,
// GBWTGraph is not presented as a PathPositionHandleGraph. XG retains its own path
// handles, iteration order, and positional queries.
class GraphInput {
  public:
    using StepCallback = std::function<void(const GraphPathStep&)>;
    void load_xg(std::istream& input);
    void load_gbz(std::istream& input);
    void load_gbwt(std::istream& input);
    void initialize_gbwt(std::size_t threads);
    bool uses_gbwt() const { return index_ != nullptr; }
    const handlegraph::HandleGraph& nodes() const { return *nodes_; }
    void for_each_path_handle(
        const std::function<void(handlegraph::path_handle_t)>& callback) const;
    std::string get_path_name(handlegraph::path_handle_t path) const;
    std::uint64_t get_path_length(handlegraph::path_handle_t path) const;
    void prepare_paths(const std::vector<handlegraph::path_handle_t>& paths,
                       bool positions, std::size_t threads);
    void for_each_path_step(handlegraph::path_handle_t path,
                            const StepCallback& callback, bool positions = false) const;
    void for_each_node_occurrence(handlegraph::nid_t node,
                                 const StepCallback& callback, bool positions = false) const;
  private:
    struct SelectedPath {
        std::uint64_t id = 0;
        std::uint64_t length = 0;
        std::uint64_t steps = 0;
        PathPositions positions;
    };
    const SelectedPath& selected(handlegraph::path_handle_t path) const;
    std::unique_ptr<xg::XG> xg_;
    std::unique_ptr<gbwtgraph::GBZ> gbz_;
    std::unique_ptr<gbwt::GBWT> external_gbwt_;
    const handlegraph::HandleGraph* nodes_ = nullptr;
    const gbwt::GBWT* index_ = nullptr;
    std::unique_ptr<gbwt::FastLocate> locate_;
    gbwtgraph::sample_name_set reference_samples_;
    std::vector<std::size_t> selected_rank_;
    std::vector<SelectedPath> selected_paths_;
    bool positions_ready_ = false;
};

}  // namespace pancollapse
