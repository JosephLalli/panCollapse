#include "direct_count_runtime.hpp"

#include <algorithm>
#include <array>
#include <filesystem>
#include <iostream>
#include <map>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

using namespace pancollapse::direct_count;

namespace {

constexpr std::uint64_t kNoSpillBudget = 64ULL << 20;
constexpr std::uint64_t kForcedSpillBudget = 1ULL << 20;
constexpr size_t kBulkBarcodeCount = 40000;

void expect(bool value, const std::string& message) {
    if (!value) {
        throw std::runtime_error(message);
    }
}

std::filesystem::path scratch(std::string_view name) {
    return std::filesystem::temp_directory_path() /
           ("pancollapse-direct-count-determinism-" + std::string(name));
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

struct Assignment {
    std::string umi;
    std::vector<std::string> genes;
    std::uint64_t read_count = 1;
    std::optional<std::string> barcode_quality;
};

struct Group {
    std::string barcode;
    std::vector<Assignment> assignments;
};

struct Fixture {
    std::vector<std::string> whitelist;
    std::vector<std::string> features{"G1", "G2", "G3"};
    std::vector<Group> groups;
};

Fixture make_fixture() {
    Fixture fixture;
    std::set<std::string> whitelist{"AAAAAAAA", "AAAAAAAT"};
    // The corrected barcode below must be off-whitelist.  The remaining dense
    // barcode set makes the posterior calculation exercise all Hamming-1 candidates.
    constexpr std::string_view deferred_barcode = "AAAAAAAC";
    for (std::uint32_t value = 0; whitelist.size() < kBulkBarcodeCount + 2; ++value) {
        const std::string barcode = barcode_from_index(value);
        if (barcode != deferred_barcode) {
            whitelist.insert(barcode);
        }
    }
    fixture.whitelist.assign(whitelist.begin(), whitelist.end());

    // Featureless groups contribute only to the all-read barcode prior.  Their
    // large number makes the intended Hamming-1 candidate dominate at EOF.
    for (size_t index = 0; index < 10000; ++index) {
        fixture.groups.push_back({"AAAAAAAA", {}});
    }

    for (const std::string& barcode : fixture.whitelist) {
        fixture.groups.push_back({barcode, {{"ACGTACGT", {"G3"}, 1, std::nullopt}}});
    }

    fixture.groups.push_back({
        "AAAAAAAA",
        {
            {"ACGTACGT", {"G1"}, 5, std::nullopt},
            {"ACGTACGT", {"G2"}, 2, std::nullopt},
            {"CACACACA", {"G1"}, 4, std::nullopt},
            {"CACACACC", {"G1"}, 1, std::nullopt},
            {"GCGCGCGC", {"G1"}, 3, std::nullopt},
            {"GCGCGCGC", {"G2"}, 3, std::nullopt},
            // UMI filtering precedes the no-gene terminal in the frozen oracle.
            {"NNNNNNNN", {}, 2, std::nullopt},
            {"GATCGATC", {"G1", "G2"}, 2, std::nullopt},
        }});
    fixture.groups.push_back(
        {std::string(deferred_barcode), {{"TATATATA", {"G1"}, 1, "IIIIIIII"}}});
    // Q40 and Q33 are the same frozen correction signature because the
    // posterior caps base quality at 33. They must therefore aggregate before
    // correction without changing molecule support.
    fixture.groups.push_back(
        {std::string(deferred_barcode), {{"TATATATA", {"G1"}, 1, "BBBBBBBB"}}});
    return fixture;
}

struct CanonicalResult {
    std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t, std::uint64_t>> counts;
    std::vector<std::tuple<std::string, std::uint32_t, std::uint32_t, std::string,
                           std::uint64_t, std::uint64_t>> molecules;
    std::vector<std::uint64_t> exact_priors;
    CountRuntimeCounters counters;
    std::map<std::string, ProfileCountRuntimeCounters> profile_counters;
};

std::string profile_id(const CountRuntime& runtime, std::uint32_t profile_index) {
    return runtime.profiles().at(profile_index).profile.id_string;
}

CanonicalResult canonicalize(const CountRuntime& runtime, CountRuntimeResult result) {
    CanonicalResult canonical;
    for (const NumericCountRecord& row : result.counts) {
        canonical.counts.emplace_back(profile_id(runtime, row.profile_index),
                                      row.barcode_whitelist_index,
                                      row.feature_catalog_index, row.umi_count);
    }
    for (const NumericMoleculeRecord& row : result.molecules) {
        canonical.molecules.emplace_back(profile_id(runtime, row.profile_index),
                                         row.barcode_whitelist_index,
                                         row.feature_catalog_index, row.corrected_umi,
                                         row.supporting_reads, row.raw_umis_collapsed);
    }
    std::sort(canonical.counts.begin(), canonical.counts.end());
    std::sort(canonical.molecules.begin(), canonical.molecules.end());
    canonical.exact_priors = std::move(result.exact_barcode_priors);
    canonical.counters = result.counters;
    for (size_t profile_index = 0; profile_index < result.profile_counters.size();
         ++profile_index) {
        canonical.profile_counters.emplace(
            runtime.profiles().at(profile_index).profile.id_string,
            result.profile_counters.at(profile_index));
    }
    return canonical;
}

bool same_semantic_counters(const CountRuntimeCounters& left,
                            const CountRuntimeCounters& right) {
    return left.barcode_prior_groups == right.barcode_prior_groups &&
           left.exact_barcode_prior_groups == right.exact_barcode_prior_groups &&
           left.assigned_observations == right.assigned_observations &&
           left.direct_multigene_dropped == right.direct_multigene_dropped &&
           left.invalid_umi_dropped == right.invalid_umi_dropped &&
           left.umi_n_dropped == right.umi_n_dropped &&
           left.umi_homopolymer_dropped == right.umi_homopolymer_dropped &&
           left.off_whitelist_uncorrectable_dropped ==
               right.off_whitelist_uncorrectable_dropped &&
           left.barcode_correction_succeeded == right.barcode_correction_succeeded &&
           left.barcode_correction_failed == right.barcode_correction_failed;
}

void expect_same_semantics(const CanonicalResult& expected, const CanonicalResult& observed,
                           const std::string& context) {
    expect(expected.counts == observed.counts, context + ": count matrix differs");
    expect(expected.molecules == observed.molecules, context + ": molecule table differs");
    expect(expected.exact_priors == observed.exact_priors,
           context + ": exact barcode priors differ");
    expect(same_semantic_counters(expected.counters, observed.counters),
           context + ": semantic counters differ");
    expect(expected.profile_counters == observed.profile_counters,
           context + ": per-profile counters differ");
}

CanonicalResult run(const Fixture& fixture, std::vector<EffectiveProfile> profiles,
                    size_t thread_count, std::uint64_t budget, std::string_view label) {
    const auto spill = scratch(label);
    std::filesystem::remove_all(spill);
    CountRuntimeOptions options;
    options.memory_budget_bytes = budget;
    options.spill_directory = spill;
    options.raw_barcode_length = 8;
    options.raw_umi_length = 8;
    CountRuntime runtime(std::move(profiles), fixture.whitelist, fixture.features, options);

    std::vector<std::thread> threads;
    threads.reserve(thread_count);
    for (size_t worker_index = 0; worker_index < thread_count; ++worker_index) {
        threads.emplace_back([&, worker_index] {
            auto worker = runtime.make_worker(31);
            for (size_t group_index = worker_index; group_index < fixture.groups.size();
                 group_index += thread_count) {
                const Group& group = fixture.groups[group_index];
                worker.observe_barcode(group.barcode);
                for (std::uint32_t profile_index = 0;
                     profile_index < runtime.profiles().size(); ++profile_index) {
                    for (const Assignment& assignment : group.assignments) {
                        const std::optional<std::string_view> quality =
                            assignment.barcode_quality
                                ? std::optional<std::string_view>(*assignment.barcode_quality)
                                : std::nullopt;
                        worker.observe_assignment(profile_index, group.barcode, quality,
                                                  assignment.umi, assignment.genes,
                                                  assignment.read_count);
                    }
                }
            }
            worker.flush();
        });
    }
    for (std::thread& thread : threads) {
        thread.join();
    }
    return canonicalize(runtime, runtime.finalize());
}

CanonicalResult extract_profile(const CanonicalResult& joint, std::string_view profile) {
    CanonicalResult extracted;
    for (const auto& row : joint.counts) {
        if (std::get<0>(row) == profile) {
            extracted.counts.push_back(row);
        }
    }
    for (const auto& row : joint.molecules) {
        if (std::get<0>(row) == profile) {
            extracted.molecules.push_back(row);
        }
    }
    extracted.exact_priors = joint.exact_priors;
    extracted.profile_counters.emplace(
        std::string(profile), joint.profile_counters.at(std::string(profile)));
    return extracted;
}

CountRuntimeCounters combine_separate_counters(const CountRuntimeCounters& cr7,
                                               const CountRuntimeCounters& pansc) {
    CountRuntimeCounters total;
    // Priors and barcode-stage correction outcomes are pass-global: they are
    // observed once in a joint pass. Downstream assignment/UMI counters sum.
    total.barcode_prior_groups = cr7.barcode_prior_groups;
    total.exact_barcode_prior_groups = cr7.exact_barcode_prior_groups;
    total.assigned_observations = cr7.assigned_observations + pansc.assigned_observations;
    total.direct_multigene_dropped =
        cr7.direct_multigene_dropped + pansc.direct_multigene_dropped;
    total.invalid_umi_dropped = cr7.invalid_umi_dropped + pansc.invalid_umi_dropped;
    total.umi_n_dropped = cr7.umi_n_dropped + pansc.umi_n_dropped;
    total.umi_homopolymer_dropped =
        cr7.umi_homopolymer_dropped + pansc.umi_homopolymer_dropped;
    total.off_whitelist_uncorrectable_dropped =
        cr7.off_whitelist_uncorrectable_dropped;
    total.barcode_correction_succeeded = cr7.barcode_correction_succeeded;
    total.barcode_correction_failed = cr7.barcode_correction_failed;
    return total;
}

void expect_fixture_semantics(const CanonicalResult& result) {
    const auto one_mm = std::find_if(
        result.molecules.begin(), result.molecules.end(), [](const auto& row) {
            return std::get<1>(row) == 0 && std::get<2>(row) == 0 &&
                   std::get<3>(row) == "CACACACA" && std::get<4>(row) == 5 &&
                   std::get<5>(row) == 2;
        });
    expect(one_mm != result.molecules.end(), "fixture must retain the 1MM_CR molecule");
    const auto winner = std::find_if(
        result.molecules.begin(), result.molecules.end(), [](const auto& row) {
            return std::get<1>(row) == 0 && std::get<2>(row) == 0 &&
                   std::get<3>(row) == "ACGTACGT" && std::get<4>(row) == 5;
        });
    expect(winner != result.molecules.end(), "fixture must retain the MultiGeneUMI_CR winner");
    const auto tie = std::find_if(
        result.molecules.begin(), result.molecules.end(), [](const auto& row) {
            return std::get<3>(row) == "GCGCGCGC";
        });
    expect(tie == result.molecules.end(), "fixture must discard the MultiGeneUMI_CR tie");
    const auto corrected = std::find_if(
        result.molecules.begin(), result.molecules.end(), [](const auto& row) {
            return std::get<1>(row) == 0 && std::get<2>(row) == 0 &&
                   std::get<3>(row) == "TATATATA" && std::get<4>(row) == 2;
        });
    expect(corrected != result.molecules.end(),
           "capped-equivalent quality signatures must aggregate supporting reads");
    expect(result.counters.barcode_correction_succeeded == 2,
           "joint fixture must count each corrected read once globally");
    expect(result.counters.invalid_umi_dropped == 4 &&
               result.counters.umi_n_dropped == 4 &&
               result.counters.umi_homopolymer_dropped == 0 &&
               result.counters.direct_multigene_dropped == 4,
           "joint fixture must exercise invalid-UMI and direct-multigene drops");
    expect(result.profile_counters.size() == 2 &&
               result.profile_counters.at("cr7-v1").invalid_umi_dropped == 2 &&
               result.profile_counters.at("pansc-strict-v1").invalid_umi_dropped == 2 &&
               result.profile_counters.at("cr7-v1").barcode_correction_succeeded == 2 &&
               result.profile_counters.at("pansc-strict-v1")
                       .barcode_correction_succeeded == 2,
           "joint fixture must retain correction and invalid-UMI counters per profile");
}

}  // namespace

int main() {
    try {
        const Fixture fixture = make_fixture();
        const std::vector<EffectiveProfile> joint_profiles{
            effective_profile(ProfileId::cr7_v1),
            effective_profile(ProfileId::pansc_strict_v1),
        };

        const CanonicalResult reference =
            run(fixture, joint_profiles, 1, kNoSpillBudget, "joint-reference");
        expect(reference.counters.aggregate_spill_runs == 0,
               "large-budget reference must remain resident");
        expect_fixture_semantics(reference);

        for (const size_t thread_count : {size_t{1}, size_t{2}, size_t{4}, size_t{8},
                                          size_t{16}, size_t{128}}) {
            const std::string prefix = "joint-" + std::to_string(thread_count);
            const CanonicalResult resident =
                run(fixture, joint_profiles, thread_count, kNoSpillBudget, prefix + "-resident");
            const CanonicalResult spilled =
                run(fixture, joint_profiles, thread_count, kForcedSpillBudget, prefix + "-spill");
            expect(resident.counters.aggregate_spill_runs == 0,
                   prefix + ": large-memory run spilled unexpectedly");
            expect(spilled.counters.aggregate_spill_runs > 2,
                   prefix + ": forced run did not make repeated spill runs");
            expect_same_semantics(reference, resident, prefix + " resident");
            expect_same_semantics(reference, spilled, prefix + " spilled");
        }

        const CanonicalResult cr7 = run(fixture, {effective_profile(ProfileId::cr7_v1)}, 16,
                                        kForcedSpillBudget, "cr7-alone");
        const CanonicalResult pansc =
            run(fixture, {effective_profile(ProfileId::pansc_strict_v1)}, 16,
                kForcedSpillBudget, "pansc-alone");
        expect(cr7.counters.aggregate_spill_runs > 2 && pansc.counters.aggregate_spill_runs > 2,
               "separate-profile runs must exercise repeated spills");
        const CanonicalResult joint_cr7 = extract_profile(reference, "cr7-v1");
        const CanonicalResult joint_pansc = extract_profile(reference, "pansc-strict-v1");
        expect(joint_cr7.counts == cr7.counts && joint_cr7.molecules == cr7.molecules &&
                   joint_cr7.exact_priors == cr7.exact_priors &&
                   joint_cr7.profile_counters == cr7.profile_counters,
               "CR7 separate run differs from the joint pass");
        expect(joint_pansc.counts == pansc.counts && joint_pansc.molecules == pansc.molecules &&
                   joint_pansc.exact_priors == pansc.exact_priors &&
                   joint_pansc.profile_counters == pansc.profile_counters,
               "panSC separate run differs from the joint pass");
        const CountRuntimeCounters combined =
            combine_separate_counters(cr7.counters, pansc.counters);
        expect(same_semantic_counters(reference.counters, combined),
               "joint semantic counters differ from the two separate profile runs");

        std::cout << "direct_count_runtime_determinism passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
