#include "count_memory_budget.hpp"

#include <stdexcept>

int main() {
    using namespace pancollapse::count_memory;
    if (parse_bytes("1GB") != 1000000000 || parse_bytes("1GiB") != (std::size_t{1} << 30)) return 1;
    for (const char* text : {"0B", "1.5GB", "GB", "999999999999999999999TB"}) {
        try { static_cast<void>(parse_bytes(text)); return 2; } catch (const std::runtime_error&) {}
    }
    Request request;
    set_cache_override(request, "node", "100B");
    try { set_cache_override(request, "node", "1B"); return 3; } catch (const std::runtime_error&) {}
    try { set_cache_override(request, "unknown", "1B"); return 4; } catch (const std::runtime_error&) {}
    const auto first = plan(10000, 1000, request);
    const auto second = plan(10000, 1000, request);
    if (first.limits != second.limits || first.reserve_bytes != 1000 || first.pool_bytes != 8000 || first.limits[0] != 100) return 5;
    Request all; for (const char* kind : {"node", "edge", "assignment", "splice"}) set_cache_override(all, kind, "1B");
    if (plan(10000, 1000, all).limits[0] != 1) return 6;
    try { static_cast<void>(plan(1000, 1000, Request{})); return 7; } catch (const std::runtime_error&) {}
    try { static_cast<void>(plan(1000, 950, Request{})); return 8; } catch (const std::runtime_error&) {}
    const auto rounded = plan(19, 0, Request{});
    std::size_t rounded_sum = 0;
    for (const auto bytes : rounded.limits) {
        if (bytes == 0) return 9;
        rounded_sum += bytes;
    }
    if (rounded_sum != rounded.pool_bytes || rounded.limits != plan(19, 0, Request{}).limits) return 10;
    try { static_cast<void>(plan(2, 0, Request{})); return 11; } catch (const std::runtime_error&) {}
    Request zero; zero.overrides[0] = 0;
    try { static_cast<void>(plan(100, 0, zero)); return 12; } catch (const std::runtime_error&) {}
    Request excess; excess.overrides[0] = 91;
    try { static_cast<void>(plan(100, 0, excess)); return 13; } catch (const std::runtime_error&) {}
    if (parse_bytes("1TB") != 1000000000000ULL || parse_bytes("1TiB") != (std::size_t{1} << 40)) return 14;
    for (const char* text : {"-1", "1e9", "1XB", "18446744073709551616B", "18446744073709551615TiB"}) {
        try { static_cast<void>(parse_bytes(text)); return 15; } catch (const std::runtime_error&) {}
    }
    const auto direct = plan_direct(10000, 1000, Request{});
    if (direct.limits != std::array<std::size_t, 4>{0, 0, 8000, 0}) return 16;
    Request direct_override;
    set_cache_override(direct_override, "assignment", "100B");
    if (plan_direct(10000, 1000, direct_override).limits[2] != 100) return 17;
    for (const auto kind : {CacheKind::node, CacheKind::edge, CacheKind::splice}) {
        Request legacy;
        legacy.overrides[static_cast<std::size_t>(kind)] = 1;
        try { static_cast<void>(plan_direct(10000, 1000, legacy)); return 18; }
        catch (const std::runtime_error&) {}
    }
    try { static_cast<void>(plan_direct(10000, 9500, Request{})); return 19; }
    catch (const std::runtime_error&) {}
    direct_override.overrides[2] = 8001;
    try { static_cast<void>(plan_direct(10000, 1000, direct_override)); return 20; }
    catch (const std::runtime_error&) {}
    if (rss_pressure_action(8999, direct, true) != RssPressureAction::none) return 21;
    if (rss_pressure_action(9000, direct, true) != RssPressureAction::release_cache) return 22;
    if (rss_pressure_action(9000, direct, false) != RssPressureAction::stop) return 23;
    Plan invalid = direct; invalid.reserve_bytes = invalid.target_bytes + 1;
    try { static_cast<void>(rss_pressure_action(0, invalid, true)); return 24; }
    catch (const std::logic_error&) {}
    return 0;
}
