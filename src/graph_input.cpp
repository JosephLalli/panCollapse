#include "graph_input.hpp"

#include <algorithm>
#include <chrono>
#include <climits>
#include <exception>
#include <iostream>
#include <mutex>
#include <omp.h>
#include <stdexcept>

namespace pancollapse {
namespace {
constexpr auto unselected = std::numeric_limits<std::size_t>::max();
std::uint64_t add_length(std::uint64_t position, std::size_t length) {
    if (length > std::numeric_limits<std::uint64_t>::max() - position) {
        throw std::runtime_error("GBWT path base length exceeds 64-bit coordinates");
    }
    return position + length;
}
}

void PathPositions::push(std::uint64_t position) {
    if (!wide() && position > std::numeric_limits<std::uint32_t>::max()) {
        wide_.reserve(narrow_.capacity());
        wide_.assign(narrow_.begin(), narrow_.end());
        // An empty prefix may widen on its very first value.
        wide_.push_back(position);
        std::vector<std::uint32_t>().swap(narrow_);
    } else if (wide()) {
        wide_.push_back(position);
    } else {
        narrow_.push_back(static_cast<std::uint32_t>(position));
    }
}
std::uint64_t PathPositions::at(std::size_t ordinal) const {
    return wide() ? wide_.at(ordinal) : narrow_.at(ordinal);
}
std::uint64_t GraphPathStep::base_position() const {
    if (!position) {
        throw std::runtime_error("graph input lacks path base positions required by exact overlap counting");
    }
    return *position;
}

void GraphInput::load_xg(std::istream& input) {
    xg_ = std::make_unique<xg::XG>();
    xg_->deserialize(input);
    nodes_ = xg_.get();
}
void GraphInput::load_gbz(std::istream& input) {
    gbz_ = std::make_unique<gbwtgraph::GBZ>();
    gbz_->simple_sds_load(input);
    if (gbz_->graph.has_segment_names()) {
        throw std::runtime_error("GBZ has an unsupported node-to-segment translation; GAMP node IDs must be preserved");
    }
    nodes_ = &gbz_->graph;
    index_ = &gbz_->index;
}
void GraphInput::load_gbwt(std::istream& input) {
    external_gbwt_ = std::make_unique<gbwt::GBWT>();
    external_gbwt_->load(input);
    index_ = external_gbwt_.get();
}
void GraphInput::initialize_gbwt(std::size_t threads) {
    if (!index_) return;
    if (!nodes_) throw std::logic_error("GBWT requires a node graph");
    if (!index_->bidirectional()) {
        throw std::runtime_error("GBWT input must be bidirectional to provide oriented path occurrences");
    }
    const auto& metadata = index_->metadata;
    if (!index_->hasMetadata() || !metadata.hasPathNames() ||
        !metadata.hasSampleNames() || !metadata.hasContigNames() ||
        metadata.paths() != index_->sequences() / 2 || index_->sequences() % 2 != 0) {
        throw std::runtime_error("GBWT input requires complete path, sample and contig name metadata for every sequence pair");
    }
    for (std::size_t p = 0; p < metadata.paths(); ++p) {
        const auto& name = metadata.path(p);
        if (name.sample >= metadata.samples() || name.contig >= metadata.contigs()) {
            throw std::runtime_error("GBWT path metadata has an invalid sample or contig identifier");
        }
    }
    for (auto n = index_->firstNode(); n < index_->sigma(); ++n) {
        if (!gbwt::Node::is_reverse(n) && index_->contains(n) && !index_->empty(n) &&
            !nodes_->has_node(gbwt::Node::id(n))) {
            throw std::runtime_error("GBWT/graph node-id-space mismatch: node " +
                                     std::to_string(gbwt::Node::id(n)) + " is absent from the graph");
        }
    }
    reference_samples_ = gbwtgraph::parse_reference_samples_tag(*index_);
    selected_rank_.assign(metadata.paths(), unselected);
    const auto started = std::chrono::steady_clock::now();
    // FastLocate's constructor uses OpenMP internally. Restore the caller's setting.
    const int previous_threads = omp_get_max_threads();
    omp_set_num_threads(static_cast<int>(std::min<std::size_t>(threads, INT_MAX)));
    try {
        locate_ = std::make_unique<gbwt::FastLocate>(*index_);
    } catch (...) {
        omp_set_num_threads(previous_threads);
        throw;
    }
    omp_set_num_threads(previous_threads);
    std::cerr << "panCollapse: initialization: phase=fastlocate seconds="
              << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()
              << '\n';
}
void GraphInput::for_each_path_handle(
    const std::function<void(handlegraph::path_handle_t)>& callback) const {
    if (!index_) {
        xg_->for_each_path_handle(callback);
    } else {
        for (std::size_t p = 0; p < index_->metadata.paths(); ++p) {
            callback(handlegraph::as_path_handle(p + 1));
        }
    }
}
std::string GraphInput::get_path_name(handlegraph::path_handle_t path) const {
    if (!index_) return xg_->get_path_name(path);
    const auto p = handlegraph::as_integer(path) - 1;
    if (p >= index_->metadata.paths()) throw std::out_of_range("invalid GBWT path handle");
    return gbwtgraph::compose_path_name(*index_, p,
        gbwtgraph::get_path_sense(*index_, p, reference_samples_));
}
const GraphInput::SelectedPath& GraphInput::selected(handlegraph::path_handle_t path) const {
    const auto p = handlegraph::as_integer(path) - 1;
    return selected_paths_.at(selected_rank_.at(p));
}
std::uint64_t GraphInput::get_path_length(handlegraph::path_handle_t path) const {
    return index_ ? selected(path).length : xg_->get_path_length(path);
}
void GraphInput::prepare_paths(const std::vector<handlegraph::path_handle_t>& paths,
                              bool positions, std::size_t threads) {
    if (!index_) return;
    if (!locate_) throw std::logic_error("GBWT occurrence lookup is not initialized");
    if (!selected_paths_.empty()) throw std::logic_error("GBWT paths already prepared");
    selected_paths_.resize(paths.size());
    for (std::size_t rank = 0; rank < paths.size(); ++rank) {
        const auto p = handlegraph::as_integer(paths[rank]) - 1;
        if (selected_rank_.at(p) != unselected) throw std::logic_error("duplicate selected GBWT path");
        selected_rank_[p] = rank;
        selected_paths_[rank].id = p;
    }
    std::exception_ptr error;
    std::mutex error_mutex;
    const int workers = static_cast<int>(std::min<std::size_t>(threads, INT_MAX));
    #pragma omp parallel for schedule(dynamic, 64) num_threads(workers)
    for (std::size_t rank = 0; rank < selected_paths_.size(); ++rank) {
        try {
            auto& path = selected_paths_[rank];
            const auto walk = index_->extract(gbwt::Path::encode(path.id, false));
            path.steps = walk.size();
            if (positions) path.positions.reserve(walk.size() + 1);
            for (auto node : walk) {
                if (positions) path.positions.push(path.length);
                path.length = add_length(path.length,
                    nodes_->get_length(nodes_->get_handle(gbwt::Node::id(node))));
            }
            if (positions) path.positions.push(path.length);
        } catch (...) {
            std::lock_guard lock(error_mutex);
            if (!error) error = std::current_exception();
        }
    }
    if (error) std::rethrow_exception(error);
    positions_ready_ = positions;
}
void GraphInput::for_each_path_step(handlegraph::path_handle_t path,
                                   const StepCallback& callback, bool positions) const {
    std::uint64_t ordinal = 0;
    if (!index_) {
        xg_->for_each_step_in_path(path, [&](handlegraph::step_handle_t step) {
            callback({path, xg_->get_handle_of_step(step), ordinal++, positions
                ? std::optional<std::uint64_t>(xg_->get_position_of_step(step)) : std::nullopt});
        });
    } else {
        const auto& info = selected(path);
        std::uint64_t position = 0;
        for (auto node : index_->extract(gbwt::Path::encode(info.id, false))) {
            const auto handle = nodes_->get_handle(gbwt::Node::id(node), gbwt::Node::is_reverse(node));
            callback({path, handle, ordinal++, positions
                ? std::optional<std::uint64_t>(position) : std::nullopt});
            if (positions) position = add_length(position, nodes_->get_length(handle));
        }
    }
}
void GraphInput::for_each_node_occurrence(handlegraph::nid_t node,
                                        const StepCallback& callback, bool positions) const {
    if (!index_) {
        xg_->for_each_step_on_handle(xg_->get_handle(node), [&](handlegraph::step_handle_t step) {
            callback({xg_->get_path_handle_of_step(step), xg_->get_handle_of_step(step),
                      std::nullopt, positions ? std::optional<std::uint64_t>(xg_->get_position_of_step(step))
                                   : std::nullopt});
        });
        return;
    }
    if (positions && !positions_ready_) {
        throw std::runtime_error("GBWT input lacks path base positions required by exact overlap counting");
    }
    const auto encoded = gbwt::Node::encode(node, false);
    if (!index_->contains(encoded) || index_->empty(encoded)) return;  // Graph-only nodes carry no transcript evidence.
    // Query one orientation once. Both stored sequence directions represent the
    // physical visits in that node record; querying both would double-count them.
    for (auto packed : locate_->decompressSA(encoded)) {
        const auto [sequence, back_offset] = locate_->unpack(packed);
        if (sequence >= index_->sequences()) throw std::runtime_error("invalid FastLocate sequence ID");
        const auto p = gbwt::Path::id(sequence);
        const auto rank = selected_rank_.at(p);
        if (rank == unselected) continue;
        const auto& info = selected_paths_[rank];
        if (back_offset >= info.steps) throw std::runtime_error("invalid FastLocate path step offset");
        const bool reverse = gbwt::Path::is_reverse(sequence);
        const auto ordinal = reverse ? back_offset : info.steps - 1 - back_offset;
        callback({handlegraph::as_path_handle(p + 1), nodes_->get_handle(node, reverse), ordinal,
                  positions ? std::optional<std::uint64_t>(info.positions.at(ordinal)) : std::nullopt});
    }
}
}  // namespace pancollapse
