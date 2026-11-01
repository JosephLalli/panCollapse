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

struct GraphPathPreparationStats {
    std::uint64_t selected_paths = 0;
    std::uint64_t selected_steps = 0;
    // Node-space preparation retains no base-coordinate payload.
    std::uint64_t allocated_coordinate_bytes = 0;
};

struct GraphPathStep {
    handlegraph::path_handle_t path;
    handlegraph::handle_t handle;
    // Present for ordered path iteration only; node-occurrence queries do not
    // need an ordinal for node-evidence counting.
    std::optional<std::uint64_t> ordinal;
};

// Only the projection facts used by panCollapse cross this boundary. In particular,
// GBWTGraph is not presented as a PathPositionHandleGraph. XG retains its own path
// handles and iteration order.
class GraphInput {
  public:
    using StepCallback = std::function<void(const GraphPathStep&)>;
    void load_xg(std::istream& input);
    void load_gbz(std::istream& input);
    void load_gbwt(std::istream& input);
    // Load a precomputed FastLocate (.ri) only after its GBWT has been loaded.
    // The caller owns artifact identity verification; this binds and validates it.
    void load_r_index(std::istream& input);
    // Join independently loaded XG and GBWT/RI inputs. The donor's native GBWT
    // allocation remains in place so a preloaded FastLocate stays valid.
    void adopt_gbwt_input(GraphInput&& donor);
    void initialize_gbwt(std::size_t threads, bool occurrence_lookup = true);
    bool uses_gbwt() const { return index_ != nullptr; }
    std::size_t gbwt_path_count() const { return index_ ? index_->metadata.paths() : 0; }
    const handlegraph::HandleGraph& nodes() const { return *nodes_; }
    void for_each_path_handle(
        const std::function<void(handlegraph::path_handle_t)>& callback) const;
    std::string get_path_name(handlegraph::path_handle_t path) const;
    std::uint64_t get_path_length(handlegraph::path_handle_t path) const;
    GraphPathPreparationStats path_preparation_stats() const { return path_preparation_stats_; }
    void prepare_paths(const std::vector<handlegraph::path_handle_t>& paths, std::size_t threads);
    void for_each_path_step(handlegraph::path_handle_t path, const StepCallback& callback) const;
    void for_each_node_occurrence(handlegraph::nid_t node, const StepCallback& callback) const;
  private:
    struct SelectedPath {
        std::uint64_t id = 0;
        std::uint64_t length = 0;
        std::uint64_t steps = 0;
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
    GraphPathPreparationStats path_preparation_stats_;
};

}  // namespace pancollapse
