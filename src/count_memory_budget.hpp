#pragma once

#include <array>
#include <algorithm>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pancollapse::count_memory {

enum class CacheKind { node = 0, edge = 1, assignment = 2, splice = 3 };
struct Request { std::optional<std::size_t> target_bytes; std::array<std::optional<std::size_t>, 4> overrides; };
struct Plan { std::size_t target_bytes, resident_bytes, reserve_bytes, pool_bytes; std::array<std::size_t, 4> limits; };
enum class RssPressureAction { none, release_cache, stop };

inline RssPressureAction rss_pressure_action(std::size_t resident, const Plan& plan,
                                             bool cache_retaining) {
    if (plan.reserve_bytes > plan.target_bytes)
        throw std::logic_error("memory reserve exceeds target");
    if (resident < plan.target_bytes - plan.reserve_bytes)
        return RssPressureAction::none;
    return cache_retaining ? RssPressureAction::release_cache : RssPressureAction::stop;
}

inline std::size_t parse_bytes(std::string_view text) {
    static constexpr std::array<std::pair<std::string_view, std::size_t>, 9> units{{
        {"KiB", 1024}, {"MiB", std::size_t{1} << 20}, {"GiB", std::size_t{1} << 30}, {"TiB", std::size_t{1} << 40},
        {"KB", 1000}, {"MB", 1000000}, {"GB", 1000000000}, {"TB", std::size_t{1000000000000ULL}}, {"B", 1}}};
    std::size_t multiplier = 1;
    for (const auto& [suffix, value] : units) if (text.size() > suffix.size() && text.ends_with(suffix)) {
        text.remove_suffix(suffix.size()); multiplier = value; break;
    }
    if (text.empty()) throw std::runtime_error("memory size must be a positive integer with a supported byte suffix");
    std::size_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') throw std::runtime_error("memory size must be a positive integer with a supported byte suffix");
        const auto digit = static_cast<std::size_t>(c - '0');
        if (value > (std::numeric_limits<std::size_t>::max() - digit) / 10) throw std::runtime_error("memory size overflows");
        value = value * 10 + digit;
    }
    if (value == 0 || value > std::numeric_limits<std::size_t>::max() / multiplier) throw std::runtime_error("memory size must be positive and in range");
    return value * multiplier;
}

inline void set_cache_override(Request& request, std::string_view kind, std::string_view bytes) {
    static constexpr std::array<std::string_view, 4> names{"node", "edge", "assignment", "splice"};
    auto found = std::find(names.begin(), names.end(), kind);
    if (found == names.end()) throw std::runtime_error("unknown cache kind");
    auto& slot = request.overrides[static_cast<std::size_t>(found - names.begin())];
    if (slot) throw std::runtime_error("duplicate cache limit");
    slot = parse_bytes(bytes);
}

// Direct reference facts are immutable mapped arrays, not evictable caches.
// Only repeated read-assignment results consume a configurable cache pool.
inline Plan plan_direct(std::size_t target, std::size_t resident, const Request& request) {
    for (const auto kind : {CacheKind::node, CacheKind::edge, CacheKind::splice}) {
        if (request.overrides[static_cast<std::size_t>(kind)])
            throw std::runtime_error("direct count only supports --cache-limit assignment=SIZE");
    }
    const auto reserve = target / 10;
    if (target <= resident || reserve >= target - resident)
        throw std::runtime_error("memory budget leaves insufficient headroom for direct reference and counting");
    const auto pool = target - resident - reserve;
    const auto assignment = request.overrides[2].value_or(pool);
    if (assignment == 0 || assignment > pool)
        throw std::runtime_error("assignment cache limit exceeds memory pool");
    return {target, resident, reserve, pool, {0, 0, assignment, 0}};
}

inline Plan plan(std::size_t target, std::size_t resident, const Request& request) {
    if (target <= resident) throw std::runtime_error("memory budget leaves no cache headroom");
    const std::size_t reserve = target / 10;
    if (reserve > target - resident) throw std::runtime_error("memory budget leaves insufficient cache headroom");
    const std::size_t pool = target - reserve - resident;
    std::array<std::size_t, 4> limits{};
    std::size_t explicit_sum = 0; std::size_t remaining_weight = 0, remaining_caches = 0;
    constexpr std::array<std::size_t, 4> weights{60, 25, 10, 5};
    for (std::size_t i = 0; i < 4; ++i) if (request.overrides[i]) {
        if (*request.overrides[i] == 0) throw std::runtime_error("cache limits must be positive");
        if (*request.overrides[i] > pool - explicit_sum) throw std::runtime_error("cache limits exceed memory pool");
        limits[i] = *request.overrides[i]; explicit_sum += limits[i];
    } else { remaining_weight += weights[i]; ++remaining_caches; }
    if (pool - explicit_sum < remaining_caches) throw std::runtime_error("memory budget leaves insufficient cache headroom");
    const std::size_t remainder = pool - explicit_sum - remaining_caches;
    std::size_t assigned = 0;
    for (std::size_t i = 0; i < 4; ++i) if (!request.overrides[i]) {
        limits[i] = 1 + remainder / remaining_weight * weights[i] + remainder % remaining_weight * weights[i] / remaining_weight;
        assigned += limits[i] - 1;
    }
    for (std::size_t i = 0; assigned < remainder && i < 4; ++i) if (!request.overrides[i]) { ++limits[i]; ++assigned; }
    for (const auto limit : limits) if (limit == 0) throw std::runtime_error("memory budget leaves insufficient cache headroom");
    return {target, resident, reserve, pool, limits};
}
}  // namespace pancollapse::count_memory
