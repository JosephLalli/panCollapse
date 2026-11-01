#include "node_tally_cache.hpp"

#include <iostream>
#include <map>
#include <random>
#include <tuple>

namespace {
struct Metadata {
    struct Reference { uint64_t path_handle; bool path_is_reverse; };
    std::vector<Reference> ref_paths;
};
struct Visit { int64_t node; bool reverse; int64_t score; int64_t bases; };
using Catalog = std::map<int64_t, Metadata>;
using Cache = pathtally::ReadNodeTallyCache<Metadata>;

std::shared_ptr<const Metadata> lease(const Catalog& catalog, int64_t id) {
    return std::shared_ptr<const Metadata>(&catalog.at(id), [](const Metadata*) {});
}

void check(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

template<class Lookup>
void original(pathtally::NumericTallyMap& out, const std::vector<Visit>& visits,
              const Lookup& lookup) {
    out.clear();
    for (const auto& v : visits) {
        for (const auto& ref : lookup(v.node).ref_paths) {
            auto& tally = out[ref.path_handle];
            tally.score += v.score;
            if (v.reverse == ref.path_is_reverse) tally.forward_bases += v.bases;
            else tally.reverse_bases += v.bases;
        }
    }
}

bool compare(Cache& cache, const Catalog& catalog, const std::vector<Visit>& visits) {
    cache.clear();
    pathtally::NumericTallyMap expected, observed;
    const auto lookup = [&](int64_t id) -> const Metadata& { return catalog.at(id); };
    original(expected, visits, lookup);
    std::map<int64_t, size_t> calls;
    const auto counted_lookup = [&](int64_t id) -> std::shared_ptr<const Metadata> {
        ++calls[id];
        return lease(catalog, id);
    };
    bool fits = true;
    for (const auto& v : visits) fits &= cache.add(v.node, v.reverse, v.score, v.bases);
    const bool expanded = fits && cache.expand(observed, counted_lookup);
    if (!expanded) {
        original(observed, visits, [&](int64_t id) -> const Metadata& {
            return cache.resolve(id, counted_lookup);
        });
    }
    check(expected.size() == observed.size(), "path-key presence differs");
    for (const auto& [key, wanted] : expected) {
        const auto& actual = observed.at(key);
        check(std::tie(actual.score, actual.forward_bases, actual.reverse_bases) ==
              std::tie(wanted.score, wanted.forward_bases, wanted.reverse_bases),
              "score/orientation differs");
    }
    // Emulate subsequent exact matching: metadata must remain stable with no
    // additional global lookup, including zero-contribution nodes.
    for (const auto& v : visits) {
        check(&cache.resolve(v.node, counted_lookup) == &catalog.at(v.node),
              "metadata address differs");
    }
    std::set<int64_t> distinct;
    for (const auto& v : visits) distinct.insert(v.node);
    check(calls.size() == distinct.size(), "metadata node set differs");
    for (const auto& [id, n] : calls) {
        static_cast<void>(id);
        check(n == 1, "metadata fetched more than once per read");
    }
    return expanded;
}
}  // namespace

int main() {
    try {
        Cache cache;
        const Catalog duplicates = {
            {1, {{{7, false}, {7, false}, {7, true}, {8, true}}}},
            {2, {{{7, false}, {9, false}}}}, {3, {}}
        };
        check(compare(cache, duplicates, {{1, false, 9, 3}, {2, true, -3, 0},
              {1, true, -9, 5}, {3, false, 0, 0}, {2, false, 0, 0}}),
              "ordinary case should aggregate");
        check(compare(cache, duplicates, {{1, false, 0, 0}}), "zero path lost");
        check(compare(cache, duplicates, {}), "empty group failed");
        check(compare(cache, {{1, {{{10, false}}}}, {2, {{{11, false}}}}},
                      {{1, false, 0, 0}, {2, false, -5, 0}}), "zero versus negative failed");

        cache.clear();
        cache.add(1, false, 0, 0);
        std::weak_ptr<const Metadata> retained;
        const auto owning_lookup = [&](int64_t id) {
            check(id == 1, "unexpected lease-test node");
            auto value = std::make_shared<Metadata>();
            value->ref_paths.push_back({77, false});
            retained = value;
            return std::shared_ptr<const Metadata>(std::move(value));
        };
        check(cache.resolve(1, owning_lookup).ref_paths.front().path_handle == 77,
              "owned metadata lookup failed");
        check(!retained.expired(), "read cache did not retain metadata lease");
        cache.clear();
        check(retained.expired(), "read cache clear did not release metadata lease");

        const int64_t max = std::numeric_limits<int64_t>::max();
        const Catalog shared = {{1, {{{1, false}}}}, {2, {{{1, false}}}}, {3, {{{1, false}}}}};
        check(!compare(cache, shared, {{1, false, max, 0}, {2, false, -max, 0},
                                       {1, false, 1, 0}}), "node overflow should replay");
        check(!compare(cache, shared, {{1, false, 1, 0}, {2, false, max - 1, 0},
              {3, false, -(max - 1), 0}, {1, false, max - 1, 0},
              {2, false, -(max - 2), 0}}), "fanout overflow should replay");

        std::mt19937_64 rng(20260916);
        for (size_t trial = 0; trial < 2000; ++trial) {
            Catalog catalog;
            const int64_t node_count = 1 + rng() % 40;
            for (int64_t node = 1; node <= node_count; ++node) {
                auto& refs = catalog[node].ref_paths;
                const size_t count = rng() % 12;
                for (size_t i = 0; i < count; ++i) {
                    refs.push_back({rng() % 20, bool(rng() % 2)});
                }
            }
            std::vector<Visit> visits;
            const size_t count = rng() % 1000;
            for (size_t i = 0; i < count; ++i) {
                visits.push_back({1 + int64_t(rng() % node_count), bool(rng() % 2),
                                  int64_t(rng() % 41) - 20, int64_t(rng() % 16)});
            }
            check(compare(cache, catalog, visits), "bounded oracle unexpectedly overflowed");
        }
        // Force several hash growths, then confirm cached metadata survives.
        Catalog large;
        std::vector<Visit> visits;
        for (int64_t node = 1; node <= 10000; ++node) {
            large[node].ref_paths.push_back({uint64_t(node), false});
            visits.push_back({node, false, 1, 1});
        }
        check(compare(cache, large, visits), "hash growth failed");
        cache.clear();
        cache.add(99999, false, 0, 0);
        bool rejected = false;
        try {
            pathtally::NumericTallyMap output;
            cache.expand(output, [&](int64_t id) { return lease(large, id); });
        } catch (const std::out_of_range&) { rejected = true; }
        check(rejected, "absent graph node was not rejected");
        std::cout << "PASS: 2000 differential cases, zero/negative/orientation/duplicates, "
                     "overflow replay, one lookup/node/read, growth, missing node\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
