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
std::uint64_t add_path_length(std::uint64_t length, std::size_t node_length) {
    if (node_length > std::numeric_limits<std::uint64_t>::max() - length) {
        throw std::runtime_error("GBWT path length exceeds 64-bit range");
    }
    return length + node_length;
}
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
void GraphInput::load_r_index(std::istream& input) {
    if (!index_) {
        throw std::logic_error("r-index requires a loaded GBWT");
    }
    auto loaded = std::make_unique<gbwt::FastLocate>();
    loaded->load(input);
    loaded->setGBWT(*index_);
    if (loaded->header.max_length == 0 || loaded->comp_to_run.size() != index_->effective() ||
        loaded->samples.empty() || loaded->samples.size() != loaded->last_to_run.size() ||
        loaded->last.ones() != loaded->samples.size()) {
        throw std::runtime_error("r-index has invalid FastLocate structures for the GBWT");
    }
    locate_ = std::move(loaded);
}
void GraphInput::adopt_gbwt_input(GraphInput&& donor) {
    if (this == &donor || !xg_ || nodes_ != xg_.get() || gbz_ || external_gbwt_ || index_ ||
        locate_ || !selected_rank_.empty() || !selected_paths_.empty()) {
        throw std::logic_error("GBWT adoption requires an uninitialized XG-only receiver");
    }
    if (donor.xg_ || donor.gbz_ || donor.nodes_ || !donor.external_gbwt_ ||
        donor.index_ != donor.external_gbwt_.get() || !donor.selected_rank_.empty() ||
        !donor.selected_paths_.empty() ||
        (donor.locate_ && donor.locate_->index != donor.external_gbwt_.get())) {
        throw std::logic_error("GBWT adoption requires an uninitialized GBWT-only donor");
    }
    // Move the allocation, not its contents: FastLocate::index continues to point
    // at this exact GBWT object after ownership changes hands.
    external_gbwt_ = std::move(donor.external_gbwt_);
    locate_ = std::move(donor.locate_);
    index_ = external_gbwt_.get();
    donor.index_ = nullptr;
}
void GraphInput::initialize_gbwt(std::size_t threads, bool occurrence_lookup) {
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
    if (!occurrence_lookup) return; // The compiler streams paths once; no RI is needed.
    selected_rank_.assign(metadata.paths(), unselected);
    const auto started = std::chrono::steady_clock::now();
    if (locate_) {
        // This happens only after main has accepted the sidecar identity (or its
        // explicitly requested trusted receipt), so malformed bytes are never
        // queried before their selected validation policy completes.
        for (auto node = index_->firstNode(); node < index_->sigma(); ++node) {
            if (index_->contains(node) && !index_->empty(node)) {
                const auto positions = locate_->decompressSA(node);
                if (positions.empty()) {
                    throw std::runtime_error("r-index FastLocate sanity query returned no occurrences");
                }
                break;
            }
        }
        std::cerr << "panCollapse: initialization: phase=fastlocate_load seconds="
                  << std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count()
                  << '\n';
        return;
    }
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
                               std::size_t threads) {
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
    std::uint64_t selected_steps = 0;
    const int workers = static_cast<int>(std::min<std::size_t>(threads, INT_MAX));
    #pragma omp parallel for schedule(dynamic, 64) num_threads(workers) \
        reduction(+:selected_steps)
    for (std::size_t rank = 0; rank < selected_paths_.size(); ++rank) {
        try {
            auto& path = selected_paths_[rank];
            const auto walk = index_->extract(gbwt::Path::encode(path.id, false));
            path.steps = walk.size();
            for (auto node : walk) {
                path.length = add_path_length(path.length,
                    nodes_->get_length(nodes_->get_handle(gbwt::Node::id(node))));
            }
            selected_steps += path.steps;
        } catch (...) {
            std::lock_guard lock(error_mutex);
            if (!error) error = std::current_exception();
        }
    }
    if (error) std::rethrow_exception(error);
    path_preparation_stats_ = {
        static_cast<std::uint64_t>(selected_paths_.size()), selected_steps, 0
    };
    std::cerr << "panCollapse: initialization: phase=selected_path_nodes paths="
              << path_preparation_stats_.selected_paths
              << " steps=" << path_preparation_stats_.selected_steps
              << " coordinate_bytes=0\n";
}
void GraphInput::for_each_path_step(handlegraph::path_handle_t path,
                                    const StepCallback& callback) const {
    std::uint64_t ordinal = 0;
    if (!index_) {
        xg_->for_each_step_in_path(path, [&](handlegraph::step_handle_t step) {
            callback({path, xg_->get_handle_of_step(step), ordinal++});
        });
    } else {
        const auto id = handlegraph::as_integer(path) - 1;
        for (auto node : index_->extract(gbwt::Path::encode(id, false))) {
            const auto handle = nodes_->get_handle(gbwt::Node::id(node), gbwt::Node::is_reverse(node));
            callback({path, handle, ordinal++});
        }
    }
}
void GraphInput::for_each_node_occurrence(handlegraph::nid_t node,
                                          const StepCallback& callback) const {
    if (!index_) {
        xg_->for_each_step_on_handle(xg_->get_handle(node), [&](handlegraph::step_handle_t step) {
            callback({xg_->get_path_handle_of_step(step), xg_->get_handle_of_step(step), std::nullopt});
        });
        return;
    }
    const auto encoded = gbwt::Node::encode(node, false);
    if (!index_->contains(encoded) || index_->empty(encoded)) return;  // Graph-only nodes carry no transcript evidence.
    // Query one orientation once. Both stored sequence directions represent the
    // physical visits in that node record; querying both would double-count them.
    const auto occurrences = locate_->decompressSA(encoded);
    for (const auto packed : occurrences) {
        const auto [sequence, back_offset] = locate_->unpack(packed);
        if (sequence >= index_->sequences()) throw std::runtime_error("invalid FastLocate sequence ID");
        const auto p = gbwt::Path::id(sequence);
        const auto rank = selected_rank_.at(p);
        if (rank == unselected) continue;
        const bool reverse = gbwt::Path::is_reverse(sequence);
        static_cast<void>(back_offset);
        callback({handlegraph::as_path_handle(p + 1), nodes_->get_handle(node, reverse), std::nullopt});
    }
}
}  // namespace pancollapse
