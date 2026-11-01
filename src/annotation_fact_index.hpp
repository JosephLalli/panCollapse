#pragma once

#include "direct_count_facts.hpp"

namespace pancollapse::direct_count {

// Read and validate the ordinary ledger while hashing its exact bytes once.
// A buffered digest stream avoids per-character SHA calls from std::getline.
struct IndexedLedger {
    path_identity::PathIdentityLedger ledger;
    AnnotationIndexBinding binding;
};
IndexedLedger read_index_ledger(const std::filesystem::path& filename,
    const std::function<void(size_t, size_t, uint64_t)>& progress = {});

} // namespace pancollapse::direct_count
