#include "direct_count_runtime.hpp"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace pancollapse::direct_count;

namespace {

void expect(bool value, const char* message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path scratch(std::string_view name) {
    return std::filesystem::temp_directory_path() /
           ("pancollapse-direct-count-" + std::string(name));
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

CountRuntimeResult large_run(const std::vector<std::string>& whitelist,
                             bool reverse, std::string_view suffix) {
    const auto spill = scratch(suffix);
    std::filesystem::remove_all(spill);
    CountRuntimeOptions options;
    options.memory_budget_bytes = 1ULL << 20;
    options.spill_directory = spill;
    options.raw_barcode_length = 8;
    options.raw_umi_length = 8;
    CountRuntime runtime({effective_profile(ProfileId::cr7_v1)}, whitelist,
                         {"G"}, options);
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
        const auto spill = scratch("resident");
        std::filesystem::remove_all(spill);
        CountRuntimeOptions options;
        options.memory_budget_bytes = 1ULL << 20;
        options.spill_directory = spill;
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
        expect(resident.counters.aggregate_spill_runs == 0,
               "small aggregation must remain resident");
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
        const auto posterior_spill = scratch("posterior-before-umi");
        std::filesystem::remove_all(posterior_spill);
        CountRuntimeOptions posterior_options;
        posterior_options.memory_budget_bytes = 1ULL << 20;
        posterior_options.spill_directory = posterior_spill;
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
        const CountRuntimeResult forward = large_run(whitelist, false, "spill-forward");
        const CountRuntimeResult reverse = large_run(whitelist, true, "spill-reverse");
        expect(forward.counters.aggregate_spill_runs > 0 &&
                   reverse.counters.aggregate_spill_runs > 0,
               "forced-spill fixture must exercise external merge");
        expect(forward.counts.size() == whitelist.size() &&
                   forward.molecules.size() == whitelist.size(),
               "forced-spill row count");
        expect(count_tuples(forward) == count_tuples(reverse) &&
                   forward.exact_barcode_priors == reverse.exact_barcode_priors,
               "spill output must be insertion-order invariant");

        // A single corrected barcode can remain below the global aggregate
        // threshold yet still create a large post-1MM candidate set. Exercise
        // the secondary per-barcode spill independently of aggregate spilling.
        const auto single_barcode_spill = scratch("single-barcode-spill");
        std::filesystem::remove_all(single_barcode_spill);
        CountRuntimeOptions single_options;
        single_options.memory_budget_bytes = 1ULL << 20;
        single_options.spill_directory = single_barcode_spill;
        single_options.raw_barcode_length = 8;
        single_options.raw_umi_length = 8;
        std::vector<std::string> many_features;
        many_features.reserve(6000);
        for (size_t index = 0; index < 6000; ++index) {
            many_features.push_back("G" + std::to_string(index));
        }
        CountRuntime single_runtime(
            {effective_profile(ProfileId::cr7_v1)}, {"AAAAAAAA"},
            many_features, single_options);
        auto single_worker = single_runtime.make_worker(6000);
        single_worker.observe_barcode("AAAAAAAA");
        for (const std::string& feature : many_features) {
            single_worker.observe_assignment(
                0, "AAAAAAAA", std::nullopt, "ACGTACGT", {feature});
        }
        single_worker.flush();
        const CountRuntimeResult single_result = single_runtime.finalize();
        expect(single_result.counters.aggregate_spill_runs > 0,
               "pathological single barcode must exercise secondary spill");
        expect(single_result.counts.empty() && single_result.molecules.empty(),
               "equal cross-gene support remains a tie-discard after secondary spill");

        // The ZSTD checksum follows the final decoded record. Truncating only
        // that trailer used to leave every declared row readable, so finalization
        // must explicitly authenticate the completed frame.
        const auto corrupt_spill = scratch("corrupt-spill");
        std::filesystem::remove_all(corrupt_spill);
        CountRuntimeOptions corrupt_options;
        corrupt_options.memory_budget_bytes = 1ULL << 20;
        corrupt_options.spill_directory = corrupt_spill;
        corrupt_options.raw_barcode_length = 8;
        corrupt_options.raw_umi_length = 8;
        CountRuntime corrupt_runtime(
            {effective_profile(ProfileId::cr7_v1)}, whitelist, {"G"},
            corrupt_options);
        auto corrupt_worker = corrupt_runtime.make_worker(20000);
        for (size_t index = 0; index < whitelist.size(); ++index) {
            corrupt_worker.observe_barcode(whitelist[index]);
            corrupt_worker.observe_assignment(
                0, whitelist[index], std::nullopt, "ACGTACGT", {"G"});
        }
        corrupt_worker.flush();
        std::vector<std::filesystem::path> spill_runs;
        for (const auto& entry : std::filesystem::directory_iterator(corrupt_spill)) {
            if (entry.is_regular_file()) {
                spill_runs.push_back(entry.path());
            }
        }
        expect(!spill_runs.empty(), "corruption fixture must create a spill run");
        const auto corrupt_path = spill_runs.front();
        const auto corrupt_size = std::filesystem::file_size(corrupt_path);
        expect(corrupt_size > 1, "corruption fixture spill must be nonempty");
        std::filesystem::resize_file(corrupt_path, corrupt_size - 1);
        bool corrupt_rejected = false;
        try {
            static_cast<void>(corrupt_runtime.finalize());
        } catch (const std::runtime_error&) {
            corrupt_rejected = true;
        }
        expect(corrupt_rejected, "truncated aggregate spill must be rejected");
        std::filesystem::remove_all(corrupt_spill);

        // MultiGeneUMI_CR raw guard: a raw UMI sequence that 1MM-collapses into a
        // neighbor inside one gene still counts as that gene's pre-correction
        // support. G2 has more raw ACGT reads than G1, so G1's ACGT molecule is
        // discarded even though G2 carries ACGT only inside its ACGA label.
        const auto raw_guard_spill = scratch("raw-guard");
        std::filesystem::remove_all(raw_guard_spill);
        CountRuntimeOptions raw_guard_options;
        raw_guard_options.memory_budget_bytes = 1ULL << 20;
        raw_guard_options.spill_directory = raw_guard_spill;
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
        expect(raw_guard_result.molecules.size() == 1 &&
                   raw_guard_result.molecules[0].feature_catalog_index == 1 &&
                   raw_guard_result.molecules[0].corrected_umi == "ACGA" &&
                   raw_guard_result.molecules[0].supporting_reads == 7,
               "raw guard must see pre-correction reads of a collapsed-away sequence");
        expect(!std::filesystem::exists(raw_guard_spill),
               "a runtime-created spill directory is removed after finalize");

        // A caller-created spill directory is left in place.
        const auto owned_spill = scratch("caller-owned-spill");
        std::filesystem::remove_all(owned_spill);
        std::filesystem::create_directories(owned_spill);
        {
            CountRuntimeOptions owned_options = raw_guard_options;
            owned_options.spill_directory = owned_spill;
            CountRuntime owned_runtime({effective_profile(ProfileId::cr7_v1)}, {"AAAA"},
                                       {"G"}, owned_options);
            static_cast<void>(owned_runtime.finalize());
        }
        expect(std::filesystem::is_directory(owned_spill),
               "a caller-created spill directory survives finalize");
        std::filesystem::remove_all(owned_spill);

        std::cout << "direct_count_runtime_smoke passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
