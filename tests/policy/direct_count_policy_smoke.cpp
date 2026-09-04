#include "direct_count.hpp"

#include <iostream>
#include <stdexcept>

using namespace pancollapse::direct_count;

static void expect(bool value, const char* message) {
  if (!value) {
    throw std::runtime_error(message);
  }
}

template<class Function>
static void expect_failure(Function function, const char* message) {
  try {
    function();
  } catch (const std::invalid_argument&) {
    return;
  }
  throw std::runtime_error(message);
}

int main() {
  try {
    auto p = effective_profile(ProfileId::cr7_v1);
    expect(p.profile.id_string == "cr7-v1" && p.effective_sha256.size() == 64,
           "profile hash");
    expect(p.effective_sha256 == effective_profile(ProfileId::cr7_v1).effective_sha256,
           "stable hash");
    const auto strand_override = parse_profile_override("cr7-v1:strand=reverse");
    auto derived = effective_profile(ProfileId::cr7_v1, {strand_override});
    expect(derived.effective_sha256 != p.effective_sha256,
           "override changes effective hash");
    expect(derived.profile.id_string.starts_with("derived-cr7-v1-"),
           "override receives derived id");
    expect(derived.profile.assignment.strand == StrandPolicy::reverse,
           "typed strand override");
    expect(profile_policy_source(p) == ProfileId::cr7_v1,
           "CR7 uses the CR7 policy universe");
    auto conjunctive = effective_profile(
        ProfileId::cr7_v1,
        {parse_profile_override("cr7-v1:membership-class=conjunctive-2024-a")});
    expect(profile_policy_source(conjunctive) == ProfileId::pansc_strict_v1,
           "membership override switches the certified policy universe");
    auto all_annotated = effective_profile(
        ProfileId::pansc_strict_v1,
        {parse_profile_override("pansc-strict-v1:membership-class=all-annotated")});
    expect(profile_policy_source(all_annotated) == ProfileId::pansc_strict_v1,
           "all-annotated retains base competition annotations");
    expect_failure(
        [] { parse_profile_override("cr7-v1:barcode-posterior-threshold=0.9"); },
        "barcode overrides must remain unavailable");
    expect_failure(
        [&] { effective_profile(ProfileId::cr7_v1, {strand_override, strand_override}); },
        "duplicate override field must fail");
    expect_failure(
        [] {
          effective_profile(ProfileId::cr7_v1,
                            {parse_profile_override(
                                "cr7-v1:exact-strand-gene-fallback=true"),
                             parse_profile_override("cr7-v1:strand=both")});
        },
        "contradictory strand fallback must fail");

    BarcodeCorrector cb({"AAAA", "AAAT"});
    for (int i=0;i<100;++i) cb.observe({"AAAA", std::nullopt}); // all-read prior
    cb.observe({"AAAT", std::nullopt});
    auto x=cb.correct({"AAAG", std::string("FFFF")});
    expect(x.barcode && *x.barcode == "AAAA" && x.corrected, "barcode correction");
    auto no_quality=cb.correct({"AAAG", std::string("F")});
    expect(no_quality.barcode && *no_quality.barcode == "AAAA" && no_quality.corrected,
           "wrong-length barcode quality is ignored");
    expect(!cb.correct({"AANN", std::nullopt}).barcode, "two N bases reject");
    BarcodeCorrector tie({"AAAA", "AAAT"});
    expect(!tie.correct({"AAAG", std::nullopt}).barcode, "pseudocount ambiguous reject");

    expect(basic_umi_valid("ACGT") && !basic_umi_valid("AAAA") &&
               !basic_umi_valid("ACNT") &&
               basic_umi_status("ACNT") == BasicUmiStatus::contains_n &&
               basic_umi_status("AAAA") == BasicUmiStatus::homopolymer,
           "umi filter");
    auto labels=collapse_1mm_cr({{"AAAA",1},{"AAAT",2},{"AATT",3}});
    expect(labels["AAAA"] == "AAAT" && labels["AAAT"] == "AATT", "non-transitive 1MM labels");

    std::vector<AssignmentObservation> o = {
        {"C", "ACAA", {"G1"}, 1, 3},
        {"C", "ACAT", {"G1"}, 1, 2},
        {"C", "ACAT", {"G2"}, 1, 1},
        {"C", "CCCC", {"G1", "G2"}, 1, 4},
    };
    auto r=count_observations(o);
    expect(r.counts.size()==1 && r.counts[0].barcode=="C" && r.counts[0].gene=="G1" && r.counts[0].molecules==1, "count record");
    expect(r.molecules.size() == 1 && r.molecules[0].supporting_reads == 2 &&
               r.molecules[0].raw_umis_collapsed == 2,
           "molecule support fields");
    std::cout << "direct_count_policy_smoke passed\n";
  } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
