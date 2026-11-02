#pragma once
#include "direct_count_index.hpp"

namespace pancollapse::test {
// Exact bijection for the fixture experiment: adjacent IDs share a gene,
// while every original policy, transcript, strand and support remains distinct.
inline std::vector<std::uint32_t> gene_local_ids(direct_index::Metadata& metadata) {
    using count_reference::missing;
    if (metadata.models.size() >= missing) throw std::runtime_error("fixture model ID width");
    const auto gene = [&](const auto& model) -> std::size_t {
        const auto policy = model.exon_policy != missing ? model.exon_policy : model.body_policy;
        if (policy == missing) return metadata.genes.size();
        const auto id = metadata.policies.at(policy).gene;
        if (id >= metadata.genes.size()) throw std::runtime_error("fixture gene ID range");
        return id;
    };
    std::vector<std::size_t> next(metadata.genes.size() + 1);
    for (const auto& model : metadata.models) ++next[gene(model)];
    std::size_t offset = 0;
    for (auto& count : next) { const auto size = count; count = offset; offset += size; }
    std::vector<std::uint32_t> order(metadata.models.size());
    std::vector<count_reference::ModelRecord> reordered(metadata.models.size());
    for (std::size_t id = 0; id < metadata.models.size(); ++id) {
        const auto target = next[gene(metadata.models[id])]++;
        order[id] = static_cast<std::uint32_t>(target);
        reordered[target] = metadata.models[id];
    }
    metadata.models.swap(reordered);
    return order;
}
} // namespace pancollapse::test
