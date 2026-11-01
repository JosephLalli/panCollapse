#include "direct_count_runtime.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

using namespace pancollapse::direct_count;

namespace {

void expect(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

void expect_memory_only(const CountRuntimeResult& result, const char* message) {
    expect(result.counters.aggregate_spill_runs == 0 &&
               result.counters.aggregate_spill_rows == 0 &&
               result.counters.aggregate_spill_bytes == 0,
           message);
}

std::string barcode_from_index(std::uint32_t value) {
    static constexpr char bases[] = "ACGT";
    std::string barcode(8, 'A');
    for (size_t index = barcode.size(); index-- > 0;) {
        barcode[index] = bases[value & 3U];
        value >>= 2U;
    }
    return barcode;
}

using CountTuple = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t,
                              std::uint64_t>;

std::vector<CountTuple> count_tuples(const CountRuntimeResult& result) {
    std::vector<CountTuple> rows;
    for (const NumericCountRecord& row : result.counts) {
        rows.emplace_back(row.profile_index, row.barcode_whitelist_index,
                          row.feature_catalog_index, row.umi_count);
    }
    return rows;
}

bool same_result(const CountRuntimeResult& left, const CountRuntimeResult& right) {
    if (count_tuples(left) != count_tuples(right) ||
        left.exact_barcode_priors != right.exact_barcode_priors ||
        left.counters.assigned_observations != right.counters.assigned_observations ||
        left.counters.direct_multigene_dropped != right.counters.direct_multigene_dropped ||
        left.counters.invalid_umi_dropped != right.counters.invalid_umi_dropped ||
        left.counters.barcode_correction_succeeded != right.counters.barcode_correction_succeeded ||
        left.counters.barcode_correction_failed != right.counters.barcode_correction_failed) {
        return false;
    }
    if (left.molecules.size() != right.molecules.size()) return false;
    for (size_t index = 0; index < left.molecules.size(); ++index) {
        const auto& a = left.molecules[index];
        const auto& b = right.molecules[index];
        if (std::tie(a.profile_index, a.barcode_whitelist_index, a.feature_catalog_index,
                     a.corrected_umi, a.supporting_reads, a.raw_umis_collapsed) !=
            std::tie(b.profile_index, b.barcode_whitelist_index, b.feature_catalog_index,
                     b.corrected_umi, b.supporting_reads, b.raw_umis_collapsed)) return false;
    }
    return true;
}

CountRuntimeResult large_run(const std::vector<std::string>& whitelist, bool reverse) {
    CountRuntimeOptions options;
    options.raw_barcode_length = 8;
    options.raw_umi_length = 8;
    CountRuntime runtime({effective_profile(ProfileId::cr7_v1)}, whitelist, {"G"}, options);
    auto worker = runtime.make_worker(101);
    if (reverse) {
        for (auto it = whitelist.rbegin(); it != whitelist.rend(); ++it) {
            worker.observe_barcode(*it);
            worker.observe_assignment(0, *it, std::nullopt, "ACGTACGT", {"G"});
        }
    } else {
        for (const std::string& barcode : whitelist) {
            worker.observe_barcode(barcode);
            worker.observe_assignment(0, barcode, std::nullopt, "ACGTACGT", {"G"});
        }
    }
    worker.flush();
    return runtime.finalize();
}

}  // namespace

int main() {
    try {
        CountRuntimeOptions options;
        options.raw_barcode_length = 4;
        options.raw_umi_length = 4;
        CountRuntime runtime(
            {effective_profile(ProfileId::pansc_strict_v1),
             effective_profile(ProfileId::cr7_v1)},
            {"AAAA", "AAAT"}, {"G1", "G2"}, options);
        auto worker = runtime.make_worker(2);
        for (int index = 0; index < 100; ++index) {
            worker.observe_barcode("AAAA");
        }
        worker.observe_barcode("AAAT");
        worker.observe_barcode("AAAG");
        worker.observe_assignment(0, "AAAG", std::string_view("FFFF"),
                                  "ACGT", {"G1"});
        worker.observe_assignment(0, "AAAA", std::nullopt, "ACGT", {"G1"}, 2);
        worker.observe_assignment(0, "AAAA", std::nullopt, "ACGT", {"G2"});
        worker.observe_assignment(1, "AAAA", std::nullopt, "TGCA", {"G1"});
        worker.observe_assignment(1, "AAAA", std::nullopt, "AAAA", {"G1"});
        worker.observe_assignment(1, "AAAA", std::nullopt, "AAAA", {"G1", "G2"});
        worker.observe_assignment(1, "AAAA", std::nullopt, "TGCA",
                                  {"G1", "G2"}, 4);
        worker.observe_assignment(0, "A", std::nullopt, "TGCA", {"G1"});
        worker.observe_assignment(1, "A", std::nullopt, "TGCA", {"G1"});
        worker.observe_assignment(0, "AAAA", std::nullopt, "ACNT", {}, 3);
        worker.observe_assignment(0, "AAAA", std::nullopt, "NNNN", {}, 2,
                                  /*reaches_umi_filter=*/false);
        worker.flush();
        const CountRuntimeResult resident = runtime.finalize();
        expect_memory_only(resident, "memory-only runtime wrote aggregate storage metadata");
        expect(resident.counters.barcode_prior_groups == 102 &&
                   resident.counters.exact_barcode_prior_groups == 101,
               "all-read barcode priors");
        expect(resident.counters.barcode_correction_succeeded == 1,
               "deferred barcode correction");
        expect(resident.counters.invalid_umi_dropped == 5 &&
                   resident.counters.umi_n_dropped == 3 &&
                   resident.counters.umi_homopolymer_dropped == 2 &&
                   resident.counters.direct_multigene_dropped == 4,
               "UMI filtering precedes direct multigene filtering");
        expect(resident.counters.off_whitelist_uncorrectable_dropped == 1,
               "short packed barcode must not alias an exact whitelist barcode");
        expect(resident.profile_counters.size() == 2 &&
                   resident.profile_counters[0].invalid_umi_dropped == 3 &&
                   resident.profile_counters[0].umi_n_dropped == 3 &&
                   resident.profile_counters[0].barcode_correction_succeeded == 1 &&
                   resident.profile_counters[0]
                           .off_whitelist_uncorrectable_dropped == 1 &&
                   resident.profile_counters[1].invalid_umi_dropped == 2 &&
                   resident.profile_counters[1].umi_homopolymer_dropped == 2 &&
                   resident.profile_counters[1].direct_multigene_dropped == 4 &&
                   resident.profile_counters[1]
                           .off_whitelist_uncorrectable_dropped == 1,
               "joint pass retains exact downstream counters per profile");
        expect(resident.counts.size() == 2 && resident.molecules.size() == 2,
               "joint profiles produce two molecules");
        expect(resident.molecules[0].profile_index == 0 &&
                   resident.molecules[0].corrected_umi == "ACGT" &&
                   resident.molecules[0].supporting_reads == 3,
               "cross-gene maximum support retains CR7 winner");

        // correct_cb precedes count_cr's UMI filter. An eligible read whose
        // ambiguous Hamming-1 barcode fails the posterior must not increment
        // either UMI-N or assigned-observation counters.
        CountRuntimeOptions posterior_options;
        posterior_options.raw_barcode_length = 4;
        posterior_options.raw_umi_length = 4;
        CountRuntime posterior_runtime(
            {effective_profile(ProfileId::cr7_v1),
             effective_profile(ProfileId::pansc_strict_v1)},
            {"AAAA", "AAAT"}, {"G"}, posterior_options);
        auto posterior_worker = posterior_runtime.make_worker();
        posterior_worker.observe_barcode("AAAG");
        posterior_worker.observe_assignment(0, "AAAG", std::nullopt, "NNNN", {"G"});
        posterior_worker.observe_assignment(1, "AAAG", std::nullopt, "NNNN", {"G"});
        posterior_worker.flush();
        const CountRuntimeResult posterior_result = posterior_runtime.finalize();
        expect_memory_only(posterior_result,
                           "barcode-correction runtime must remain memory-only");
        expect(posterior_result.counters.barcode_correction_failed == 1 &&
                   posterior_result.counters.invalid_umi_dropped == 0 &&
                   posterior_result.counters.assigned_observations == 0 &&
                   posterior_result.profile_counters[0].barcode_correction_failed == 1 &&
                   posterior_result.profile_counters[1].barcode_correction_failed == 1 &&
                   posterior_result.profile_counters[0].invalid_umi_dropped == 0 &&
                   posterior_result.profile_counters[1].invalid_umi_dropped == 0,
               "global barcode failure is counted once before joint-profile UMI filtering");

        std::vector<std::string> whitelist;
        whitelist.reserve(10000);
        for (std::uint32_t index = 0; index < 10000; ++index) {
            whitelist.push_back(barcode_from_index(index));
        }
        const CountRuntimeResult forward = large_run(whitelist, false);
        const CountRuntimeResult reverse = large_run(whitelist, true);
        expect_memory_only(forward, "large forward aggregation must remain memory-only");
        expect_memory_only(reverse, "large reverse aggregation must remain memory-only");
        expect(forward.counts.size() == whitelist.size() &&
                   forward.molecules.size() == whitelist.size(),
               "large resident row count");
        expect(count_tuples(forward) == count_tuples(reverse) &&
                   forward.exact_barcode_priors == reverse.exact_barcode_priors,
               "resident output must be insertion-order invariant");

        // Keep the formerly pathological single-barcode case wholly resident:
        // 60,000 equal cross-gene candidates must still be tie-discarded.
        CountRuntimeOptions single_options;
        single_options.raw_barcode_length = 8;
        single_options.raw_umi_length = 8;
        std::vector<std::string> many_features;
        constexpr size_t kSingleBarcodeFeatureCount = 60000;
        many_features.reserve(kSingleBarcodeFeatureCount);
        for (size_t index = 0; index < kSingleBarcodeFeatureCount; ++index) {
            many_features.push_back("G" + std::to_string(index));
        }
        CountRuntime single_runtime(
            {effective_profile(ProfileId::cr7_v1)}, {"AAAAAAAA"},
            many_features, single_options);
        auto single_worker = single_runtime.make_worker(8192);
        single_worker.observe_barcode("AAAAAAAA");
        for (const std::string& feature : many_features) {
            single_worker.observe_assignment(
                0, "AAAAAAAA", std::nullopt, "ACGTACGT", {feature});
        }
        single_worker.flush();
        const CountRuntimeResult single_result = single_runtime.finalize();
        expect_memory_only(single_result,
                           "single-barcode UMI finalization must remain in memory");
        expect(single_result.counts.empty() && single_result.molecules.empty(),
               "equal cross-gene support remains a tie-discard in memory");

        // MultiGeneUMI_CR raw guard: a raw UMI sequence that 1MM-collapses into a
        // neighbor inside one gene still counts as that gene's pre-correction
        // support. G2 has more raw ACGT reads than G1, so G1's ACGT molecule is
        // discarded even though G2 carries ACGT only inside its ACGA label.
        CountRuntimeOptions raw_guard_options;
        raw_guard_options.raw_barcode_length = 4;
        raw_guard_options.raw_umi_length = 4;
        CountRuntime raw_guard_runtime({effective_profile(ProfileId::cr7_v1)},
                                       {"AAAA"}, {"G1", "G2"}, raw_guard_options);
        auto raw_guard_worker = raw_guard_runtime.make_worker();
        raw_guard_worker.observe_barcode("AAAA");
        raw_guard_worker.observe_assignment(0, "AAAA", std::nullopt, "ACGT", {"G1"}, 2);
        raw_guard_worker.observe_assignment(0, "AAAA", std::nullopt, "ACGT", {"G2"}, 3);
        raw_guard_worker.observe_assignment(0, "AAAA", std::nullopt, "ACGA", {"G2"}, 4);
        raw_guard_worker.flush();
        const CountRuntimeResult raw_guard_result = raw_guard_runtime.finalize();
        expect_memory_only(raw_guard_result, "raw guard runtime must remain memory-only");
        expect(raw_guard_result.molecules.size() == 1 &&
                   raw_guard_result.molecules[0].feature_catalog_index == 1 &&
                   raw_guard_result.molecules[0].corrected_umi == "ACGA" &&
                   raw_guard_result.molecules[0].supporting_reads == 7,
               "raw guard must see pre-correction reads of a collapsed-away sequence");

        // Packing is lossless, not another UMI filter. Variable lengths, leading
        // A's, 31/32-base boundaries and unusual historically accepted symbols
        // must retain the preexisting runtime API semantics.
        CountRuntime packed_runtime({effective_profile(ProfileId::cr7_v1)},
                                    {"AAAA"}, {"G"}, raw_guard_options);
        auto packed_worker = packed_runtime.make_worker(2);
        std::vector<std::string> packed_inputs{
            "AACGT", "ACGT", "XCGY",
            "ACGTACGTACGTACGTACGTACGTACGTACG",
            "ACGTACGTACGTACGTACGTACGTACGTACGT"};
        for (const auto& umi : packed_inputs) {
            packed_worker.observe_barcode("AAAA");
            packed_worker.observe_assignment(0, "AAAA", std::nullopt, umi, {"G"}, 2);
            packed_worker.observe_assignment(0, "AAAA", std::nullopt, umi, {"G"}, 3);
        }
        packed_worker.flush();
        const auto packed_result = packed_runtime.finalize();
        std::sort(packed_inputs.begin(), packed_inputs.end());
        std::vector<std::string> packed_actual;
        for (const auto& molecule : packed_result.molecules) {
            packed_actual.push_back(molecule.corrected_umi);
            expect(molecule.supporting_reads == 5, "packed UMI read support changed");
        }
        expect(packed_actual == packed_inputs, "packed UMI boundary/fallback changed identity");
        // A writer-certified lexical feature catalog is queried in place: the
        // native route must not reconstruct a reference-wide string map.
        for (const bool prepared : {false, true}) {
            auto ready_options = raw_guard_options;
            ready_options.feature_catalog_prepared = prepared;
            CountRuntime ready({effective_profile(ProfileId::cr7_v1)},
                               {"AAAA"}, {"G1", "G2"}, ready_options);
            auto ready_worker = ready.make_worker();
            ready_worker.observe_barcode("AAAA");
            ready_worker.observe_assignment(0, "AAAA", std::nullopt, "ACGT", {"G2"}, 3);
            bool rejected = false;
            try { ready_worker.observe_assignment(0, "AAAA", std::nullopt, "TGCA", {"absent"}); }
            catch (const std::runtime_error&) { rejected = true; }
            expect(rejected, "unknown feature must fail in both dictionary modes");
            ready_worker.flush();
            const auto ready_result = ready.finalize();
            expect(ready_result.counts.size() == 1 &&
                   ready_result.counts[0].feature_catalog_index == 1 &&
                   ready_result.molecules[0].supporting_reads == 3,
                   "prepared feature dictionary changed count identity");
        }

        // The native-ID entry point must retain the exact string-adapter
        // semantics, while never reading a non-singleton ID value.
        CountRuntime string_runtime({effective_profile(ProfileId::cr7_v1)},
                                    {"AAAA", "AAAT"}, {"G1", "G2"}, raw_guard_options);
        CountRuntime numeric_runtime({effective_profile(ProfileId::cr7_v1)},
                                     {"AAAA", "AAAT"}, {"G1", "G2"}, raw_guard_options);
        auto string_worker = string_runtime.make_worker();
        auto numeric_worker = numeric_runtime.make_worker();
        for (int index = 0; index < 20; ++index) {
            string_worker.observe_barcode("AAAA");
            numeric_worker.observe_barcode("AAAA");
        }
        expect(numeric_worker.potentially_countable("AAAA", "ACGT") &&
                   numeric_worker.potentially_countable("AAAG", "ACGT") &&
                   numeric_worker.potentially_countable("AAAN", "ACGT") &&
                   !numeric_worker.potentially_countable("AANN", "ACGT") &&
                   !numeric_worker.potentially_countable("CCCC", "ACGT") &&
                   !numeric_worker.potentially_countable("AAAA", "ACNT") &&
                   !numeric_worker.potentially_countable("AAAA", "AAAA"),
               "numeric prefilter must be pure CB/UMI eligibility");
        const auto both = [&](std::string_view barcode, std::string_view umi,
                              const std::vector<std::string>& names,
                              std::span<const std::uint32_t> ids, std::uint64_t reads,
                              std::optional<std::string_view> quality = std::nullopt) {
            string_worker.observe_assignment(0, barcode, quality, umi, names, reads);
            numeric_worker.observe_assignment_ids(0, barcode, quality, umi, ids, reads);
        };
        const std::array<std::uint32_t, 1> id_g1{0}, id_g2{1}, bad_id{2};
        const std::array<std::uint32_t, 2> ignored_multigene{999, 998};
        both("AAAA", "ACGT", {"G1"}, id_g1, 2);
        both("AAAA", "ACGT", {"G2"}, id_g2, 3);
        both("AAAA", "ACGA", {"G2"}, id_g2, 4);
        both("AAAG", "TATA", {"G1"}, id_g1, 1, std::string_view("FFFF"));
        both("AAAA", "ACGT", {"G1", "G2"}, ignored_multigene, 2);
        bool bad_id_rejected = false;
        try { numeric_worker.observe_assignment_ids(0, "AAAA", std::nullopt, "TGCA", bad_id, 1); }
        catch (const std::out_of_range&) { bad_id_rejected = true; }
        expect(bad_id_rejected, "out-of-catalog singleton numeric feature must fail");
        string_worker.flush();
        numeric_worker.flush();
        const auto string_result = string_runtime.finalize();
        const auto numeric_result = numeric_runtime.finalize();
        expect(same_result(string_result, numeric_result),
               "numeric IDs must preserve strings, priors, 1MM and raw-guard semantics");
        std::cout << "direct_count_runtime_smoke passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
