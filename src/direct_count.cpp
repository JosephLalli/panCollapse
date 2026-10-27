#include "direct_count.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace pancollapse::direct_count {
namespace {

constexpr std::array<char, 4> kBases{'A', 'C', 'G', 'T'};

bool is_base(char value) {
    return std::find(kBases.begin(), kBases.end(), value) != kBases.end();
}

std::string join(const std::vector<std::string>& values) {
    std::string result;
    for (const std::string& value : values) {
        if (!result.empty()) {
            result.push_back(',');
        }
        result += value;
    }
    return result;
}

std::vector<std::string> sorted_unique(std::vector<std::string> values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
    return values;
}

std::vector<std::string> parse_list(std::string_view text) {
    if (text == "." || text == "none") {
        return {};
    }
    if (text.empty()) {
        throw std::invalid_argument("profile override list must not be empty; use none");
    }
    std::vector<std::string> values;
    size_t begin = 0;
    while (begin <= text.size()) {
        const size_t end = text.find(',', begin);
        const std::string value(text.substr(begin, end == std::string_view::npos
                                                       ? text.size() - begin
                                                       : end - begin));
        if (value.empty() || value.find_first_of(" \t\r\n=:") != std::string::npos) {
            throw std::invalid_argument("profile override contains a malformed list value");
        }
        values.push_back(value);
        if (end == std::string_view::npos) {
            break;
        }
        begin = end + 1;
    }
    const size_t original_size = values.size();
    values = sorted_unique(std::move(values));
    if (values.size() != original_size) {
        throw std::invalid_argument("profile override list repeats a value");
    }
    return values;
}

bool parse_bool(std::string_view text) {
    if (text == "true") {
        return true;
    }
    if (text == "false") {
        return false;
    }
    throw std::invalid_argument("boolean profile override must be true or false");
}

std::int64_t parse_nonnegative_integer(std::string_view text) {
    std::int64_t result = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), result);
    if (error != std::errc{} || end != text.data() + text.size() || result < 0) {
        throw std::invalid_argument("integer profile override must be a nonnegative integer");
    }
    return result;
}

double parse_ratio(std::string_view text) {
    std::string owned(text);
    char* end = nullptr;
    const double result = std::strtod(owned.c_str(), &end);
    if (end != owned.c_str() + owned.size() || !std::isfinite(result) || result <= 1.0) {
        throw std::invalid_argument("body-support-ratio must be finite and greater than one");
    }
    return result;
}

std::string strand_name(StrandPolicy value) {
    switch (value) {
        case StrandPolicy::forward:
            return "forward";
        case StrandPolicy::reverse:
            return "reverse";
        case StrandPolicy::both:
            return "both";
    }
    throw std::logic_error("unhandled strand policy");
}

std::string membership_name(MembershipClass value) {
    switch (value) {
        case MembershipClass::cellranger_2024_a:
            return "cellranger-2024-a";
        case MembershipClass::conjunctive_2024_a:
            return "conjunctive-2024-a";
        case MembershipClass::all_annotated:
            return "all-annotated";
    }
    throw std::logic_error("unhandled membership class");
}

std::string novel_paralog_name(NovelParalogPolicy value) {
    switch (value) {
        case NovelParalogPolicy::lump:
            return "lump";
        case NovelParalogPolicy::ignore:
            return "ignore";
        case NovelParalogPolicy::separate:
            return "separate";
    }
    throw std::logic_error("unhandled novel-paralog policy");
}

std::string nested_rule_name(NestedHostRule value) {
    return value == NestedHostRule::none ? "none" : "contradiction-only";
}

std::string nested_action_name(NestedHostAction value) {
    return value == NestedHostAction::none ? "none" : "prefer-independent-protein-child";
}

// A small self-contained SHA-256 keeps policy hashing available to pure tests and
// avoids coupling this file to the Parquet/output dependency closure.
std::string sha256(std::string_view input) {
    static constexpr std::array<std::uint32_t, 64> constants{
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
        0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
        0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
        0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
        0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    auto rotate = [](std::uint32_t value, int bits) {
        return (value >> bits) | (value << (32 - bits));
    };
    std::vector<unsigned char> bytes(input.begin(), input.end());
    const std::uint64_t bit_count = static_cast<std::uint64_t>(bytes.size()) * 8;
    bytes.push_back(0x80);
    while (bytes.size() % 64 != 56) {
        bytes.push_back(0);
    }
    for (int index = 7; index >= 0; --index) {
        bytes.push_back(static_cast<unsigned char>(bit_count >> (index * 8)));
    }
    std::array<std::uint32_t, 8> hash{
        0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    for (size_t offset = 0; offset < bytes.size(); offset += 64) {
        std::array<std::uint32_t, 64> words{};
        for (size_t index = 0; index < 16; ++index) {
            const size_t at = offset + 4 * index;
            words[index] = (static_cast<std::uint32_t>(bytes[at]) << 24) |
                           (static_cast<std::uint32_t>(bytes[at + 1]) << 16) |
                           (static_cast<std::uint32_t>(bytes[at + 2]) << 8) |
                           static_cast<std::uint32_t>(bytes[at + 3]);
        }
        for (size_t index = 16; index < words.size(); ++index) {
            const std::uint32_t s0 = rotate(words[index - 15], 7) ^
                                     rotate(words[index - 15], 18) ^
                                     (words[index - 15] >> 3);
            const std::uint32_t s1 = rotate(words[index - 2], 17) ^
                                     rotate(words[index - 2], 19) ^
                                     (words[index - 2] >> 10);
            words[index] = words[index - 16] + s0 + words[index - 7] + s1;
        }
        std::array<std::uint32_t, 8> state = hash;
        for (size_t index = 0; index < words.size(); ++index) {
            const std::uint32_t s1 = rotate(state[4], 6) ^ rotate(state[4], 11) ^
                                     rotate(state[4], 25);
            const std::uint32_t choose = (state[4] & state[5]) ^ (~state[4] & state[6]);
            const std::uint32_t temp1 =
                state[7] + s1 + choose + constants[index] + words[index];
            const std::uint32_t s0 = rotate(state[0], 2) ^ rotate(state[0], 13) ^
                                     rotate(state[0], 22);
            const std::uint32_t majority = (state[0] & state[1]) ^
                                           (state[0] & state[2]) ^
                                           (state[1] & state[2]);
            const std::uint32_t temp2 = s0 + majority;
            state = {temp1 + temp2, state[0], state[1], state[2],
                     state[3] + temp1, state[4], state[5], state[6]};
        }
        for (size_t index = 0; index < hash.size(); ++index) {
            hash[index] += state[index];
        }
    }
    std::ostringstream result;
    for (std::uint32_t word : hash) {
        result << std::hex << std::setw(8) << std::setfill('0') << word;
    }
    return result.str();
}

Profile make_cr7_profile() {
    return {
        ProfileId::cr7_v1,
        "cr7-v1",
        AssignmentPolicy{
            StrandPolicy::forward,
            5,
            MembershipClass::cellranger_2024_a,
            NovelParalogPolicy::lump,
            sorted_unique({"candidate_novel_locus", "identity_candidate_reassignment",
                           "identity_unresolved", "label_conflict", "nested_same_strand",
                           "no_source_provenance", "overlaps_pseudogene", "readthrough_gene",
                           "readthrough_transcript", "source_gene_label_conflict",
                           "structural_readthrough"}),
            {},
            false,
            false,
            NestedHostRule::none,
            NestedHostAction::none,
            std::nullopt,
            {},
            false,
            {},
            false,
        },
        "exact-whitelist-prior+hamming1-q33-posterior-0.975-v1",
        "basic-filter+nontransitive-1MM_CR-v1",
        "MultiGeneUMI_CR-max-support-tie-discard-raw-guard-v1",
    };
}

Profile make_pansc_profile() {
    return {
        ProfileId::pansc_strict_v1,
        "pansc-strict-v1",
        AssignmentPolicy{
            StrandPolicy::forward,
            5,
            MembershipClass::conjunctive_2024_a,
            NovelParalogPolicy::lump,
            sorted_unique({"candidate_novel_locus",
                           "foreign_component_only_except_host_concordant_small_rna",
                           "no_source_provenance", "overlaps_pseudogene",
                           "readthrough_transcript", "source_gene_label_conflict",
                           "structural_readthrough"}),
            {"no_source_provenance"},
            true,
            true,
            NestedHostRule::contradiction_only,
            NestedHostAction::prefer_independent_protein_child,
            4.0,
            {"protein_coding"},
            true,
            {"no_source_provenance", "source_gene_label_conflict"},
            true,
        },
        "exact-whitelist-prior+hamming1-q33-posterior-0.975-v1",
        "basic-filter+nontransitive-1MM_CR-v1",
        "MultiGeneUMI_CR-max-support-tie-discard-raw-guard-v1",
    };
}

std::string canonical_profile(const Profile& value) {
    const AssignmentPolicy& policy = value.assignment;
    std::ostringstream out;
    out << "profile_schema=panCollapse-count-profile-v1\n"
        << "base=" << (value.id == ProfileId::cr7_v1 ? "cr7-v1" : "pansc-strict-v1") << '\n'
        << "strand=" << strand_name(policy.strand) << '\n'
        << "score_window=" << policy.score_window << '\n'
        << "membership_class=" << membership_name(policy.membership) << '\n'
        << "novel_paralog_policy=" << novel_paralog_name(policy.novel_paralog) << '\n'
        << "excluded_categories=" << join(sorted_unique(policy.excluded_categories)) << '\n'
        << "post_resolution_fallback_categories="
        << join(sorted_unique(policy.post_resolution_fallback_categories)) << '\n'
        << "tagged_last_resort=" << (policy.tagged_last_resort ? "true" : "false") << '\n'
        << "exact_strand_gene_fallback="
        << (policy.exact_strand_gene_fallback ? "true" : "false") << '\n'
        << "nested_host_rule=" << nested_rule_name(policy.nested_host_rule) << '\n'
        << "nested_host_action=" << nested_action_name(policy.nested_host_action) << '\n'
        << "body_support_ratio=";
    if (policy.body_support_ratio) {
        out << std::setprecision(17) << *policy.body_support_ratio;
    } else {
        out << "none";
    }
    out << '\n'
        << "body_support_gene_types=" << join(sorted_unique(policy.body_support_gene_types)) << '\n'
        << "strong_local_support_exemption="
        << (policy.strong_local_support_exemption ? "true" : "false") << '\n'
        << "strong_support_clearable_categories="
        << join(sorted_unique(policy.strong_support_clearable_categories)) << '\n'
        << "protect_primary_protein_nested_hosts="
        << (policy.protect_primary_protein_nested_hosts ? "true" : "false") << '\n'
        << "barcode_algorithm=" << value.barcode_algorithm << '\n'
        << "umi_algorithm=" << value.umi_algorithm << '\n'
        << "multigene_umi_algorithm=" << value.multigene_umi_algorithm << '\n';
    return out.str();
}

void check_profile_consistency(const Profile& value) {
    const AssignmentPolicy& policy = value.assignment;
    const bool has_nested_rule = policy.nested_host_rule != NestedHostRule::none;
    const bool has_nested_action = policy.nested_host_action != NestedHostAction::none;
    if (has_nested_rule != has_nested_action) {
        throw std::invalid_argument("nested-host rule and action must both be none or both be active");
    }
    if (policy.exact_strand_gene_fallback && policy.strand == StrandPolicy::both) {
        throw std::invalid_argument("exact-strand-gene-fallback requires forward or reverse strand");
    }
    if (policy.body_support_ratio.has_value() != !policy.body_support_gene_types.empty()) {
        throw std::invalid_argument(
            "body-support-ratio and body-support-gene-types must be enabled together");
    }
    if (policy.strong_local_support_exemption &&
        policy.strong_support_clearable_categories.empty()) {
        throw std::invalid_argument(
            "strong-local-support-exemption requires clearable categories");
    }
    const std::set<std::string> excluded(policy.excluded_categories.begin(),
                                         policy.excluded_categories.end());
    for (const std::string& category : policy.post_resolution_fallback_categories) {
        if (!excluded.contains(category)) {
            throw std::invalid_argument(
                "post-resolution fallback categories must also be excluded categories");
        }
    }
}

bool hamming_one(std::string_view left, std::string_view right) {
    if (left.size() != right.size()) {
        return false;
    }
    size_t distance = 0;
    for (size_t index = 0; index < left.size(); ++index) {
        if (left[index] != right[index] && ++distance > 1) {
            return false;
        }
    }
    return distance == 1;
}

}  // namespace

const Profile& profile(ProfileId id) {
    static const Profile cr7 = make_cr7_profile();
    static const Profile pansc = make_pansc_profile();
    switch (id) {
        case ProfileId::cr7_v1:
            return cr7;
        case ProfileId::pansc_strict_v1:
            return pansc;
    }
    throw std::invalid_argument("unknown count profile");
}

ProfileId parse_profile_id(std::string_view value) {
    if (value == "cr7-v1" || value == "cr7") {
        return ProfileId::cr7_v1;
    }
    if (value == "pansc-strict-v1") {
        return ProfileId::pansc_strict_v1;
    }
    throw std::invalid_argument("unknown count profile: " + std::string(value));
}

ProfileOverride parse_profile_override(std::string_view expression) {
    const size_t colon = expression.find(':');
    const size_t equals = expression.find('=', colon == std::string_view::npos ? 0 : colon + 1);
    if (colon == std::string_view::npos || equals == std::string_view::npos || colon == 0 ||
        equals == colon + 1 || equals + 1 >= expression.size()) {
        throw std::invalid_argument(
            "profile override must have the form <base>:<field>=<value>");
    }
    ProfileOverride result;
    result.base = parse_profile_id(expression.substr(0, colon));
    result.canonical_field = std::string(expression.substr(colon + 1, equals - colon - 1));
    const std::string_view value = expression.substr(equals + 1);
    result.canonical_value = std::string(value);

    const std::string& field = result.canonical_field;
    if (field == "strand") {
        result.key = OverrideKey::strand;
        if (value != "forward" && value != "reverse" && value != "both") {
            throw std::invalid_argument("strand must be forward, reverse, or both");
        }
        result.value = std::string(value);
    } else if (field == "score-window") {
        result.key = OverrideKey::score_window;
        result.value = parse_nonnegative_integer(value);
    } else if (field == "membership-class") {
        result.key = OverrideKey::membership_class;
        if (value != "cellranger-2024-a" && value != "conjunctive-2024-a" &&
            value != "all-annotated") {
            throw std::invalid_argument("unknown membership-class");
        }
        result.value = std::string(value);
    } else if (field == "novel-paralog-policy") {
        result.key = OverrideKey::novel_paralog_policy;
        if (value != "lump" && value != "ignore" && value != "separate") {
            throw std::invalid_argument("novel-paralog-policy must be lump, ignore, or separate");
        }
        result.value = std::string(value);
    } else if (field == "excluded-categories") {
        result.key = OverrideKey::excluded_categories;
        result.value = parse_list(value);
        result.canonical_value = join(std::get<std::vector<std::string>>(result.value));
    } else if (field == "post-resolution-fallback-categories") {
        result.key = OverrideKey::post_resolution_fallback_categories;
        result.value = parse_list(value);
        result.canonical_value = join(std::get<std::vector<std::string>>(result.value));
    } else if (field == "tagged-last-resort") {
        result.key = OverrideKey::tagged_last_resort;
        result.value = parse_bool(value);
    } else if (field == "exact-strand-gene-fallback") {
        result.key = OverrideKey::exact_strand_gene_fallback;
        result.value = parse_bool(value);
    } else if (field == "nested-host-rule") {
        result.key = OverrideKey::nested_host_rule;
        if (value != "none" && value != "contradiction-only") {
            throw std::invalid_argument("nested-host-rule must be none or contradiction-only");
        }
        result.value = std::string(value);
    } else if (field == "nested-host-action") {
        result.key = OverrideKey::nested_host_action;
        if (value != "none" && value != "prefer-independent-protein-child") {
            throw std::invalid_argument(
                "nested-host-action must be none or prefer-independent-protein-child");
        }
        result.value = std::string(value);
    } else if (field == "body-support-ratio") {
        result.key = OverrideKey::body_support_ratio;
        if (value == "none") {
            result.value = std::string("none");
        } else {
            result.value = parse_ratio(value);
        }
    } else if (field == "body-support-gene-types") {
        result.key = OverrideKey::body_support_gene_types;
        result.value = parse_list(value);
        result.canonical_value = join(std::get<std::vector<std::string>>(result.value));
    } else if (field == "strong-local-support-exemption") {
        result.key = OverrideKey::strong_local_support_exemption;
        result.value = parse_bool(value);
    } else if (field == "strong-support-clearable-categories") {
        result.key = OverrideKey::strong_support_clearable_categories;
        result.value = parse_list(value);
        result.canonical_value = join(std::get<std::vector<std::string>>(result.value));
    } else if (field == "protect-primary-protein-nested-hosts") {
        result.key = OverrideKey::protect_primary_protein_nested_hosts;
        result.value = parse_bool(value);
    } else {
        throw std::invalid_argument("unknown profile override field: " + field);
    }
    return result;
}

EffectiveProfile effective_profile(ProfileId id,
                                   const std::vector<ProfileOverride>& overrides) {
    EffectiveProfile result{profile(id), overrides, {}};
    std::set<OverrideKey> seen;
    for (const ProfileOverride& override : overrides) {
        if (override.base != id) {
            throw std::invalid_argument("profile override base does not match selected profile");
        }
        if (!seen.insert(override.key).second) {
            throw std::invalid_argument("profile override repeats a field");
        }
        AssignmentPolicy& policy = result.profile.assignment;
        switch (override.key) {
            case OverrideKey::strand: {
                const std::string& value = std::get<std::string>(override.value);
                policy.strand = value == "forward" ? StrandPolicy::forward
                                : value == "reverse" ? StrandPolicy::reverse
                                                     : StrandPolicy::both;
                break;
            }
            case OverrideKey::score_window:
                policy.score_window = std::get<std::int64_t>(override.value);
                break;
            case OverrideKey::membership_class: {
                const std::string& value = std::get<std::string>(override.value);
                policy.membership = value == "cellranger-2024-a"
                                        ? MembershipClass::cellranger_2024_a
                                    : value == "conjunctive-2024-a"
                                        ? MembershipClass::conjunctive_2024_a
                                        : MembershipClass::all_annotated;
                break;
            }
            case OverrideKey::novel_paralog_policy: {
                const std::string& value = std::get<std::string>(override.value);
                policy.novel_paralog = value == "lump" ? NovelParalogPolicy::lump
                                         : value == "ignore" ? NovelParalogPolicy::ignore
                                                             : NovelParalogPolicy::separate;
                break;
            }
            case OverrideKey::excluded_categories:
                policy.excluded_categories =
                    std::get<std::vector<std::string>>(override.value);
                break;
            case OverrideKey::post_resolution_fallback_categories:
                policy.post_resolution_fallback_categories =
                    std::get<std::vector<std::string>>(override.value);
                break;
            case OverrideKey::tagged_last_resort:
                policy.tagged_last_resort = std::get<bool>(override.value);
                break;
            case OverrideKey::exact_strand_gene_fallback:
                policy.exact_strand_gene_fallback = std::get<bool>(override.value);
                break;
            case OverrideKey::nested_host_rule:
                policy.nested_host_rule = std::get<std::string>(override.value) == "none"
                                              ? NestedHostRule::none
                                              : NestedHostRule::contradiction_only;
                break;
            case OverrideKey::nested_host_action:
                policy.nested_host_action = std::get<std::string>(override.value) == "none"
                                                ? NestedHostAction::none
                                                : NestedHostAction::prefer_independent_protein_child;
                break;
            case OverrideKey::body_support_ratio:
                if (std::holds_alternative<std::string>(override.value)) {
                    policy.body_support_ratio.reset();
                } else {
                    policy.body_support_ratio = std::get<double>(override.value);
                }
                break;
            case OverrideKey::body_support_gene_types:
                policy.body_support_gene_types =
                    std::get<std::vector<std::string>>(override.value);
                break;
            case OverrideKey::strong_local_support_exemption:
                policy.strong_local_support_exemption = std::get<bool>(override.value);
                break;
            case OverrideKey::strong_support_clearable_categories:
                policy.strong_support_clearable_categories =
                    std::get<std::vector<std::string>>(override.value);
                break;
            case OverrideKey::protect_primary_protein_nested_hosts:
                policy.protect_primary_protein_nested_hosts = std::get<bool>(override.value);
                break;
        }
    }
    check_profile_consistency(result.profile);
    result.effective_sha256 = sha256(canonical_profile(result.profile));
    if (!overrides.empty()) {
        result.profile.id_string = "derived-" + profile(id).id_string + "-" +
                                   result.effective_sha256.substr(0, 12);
        std::sort(result.overrides.begin(), result.overrides.end(),
                  [](const ProfileOverride& left, const ProfileOverride& right) {
                      return left.canonical_field < right.canonical_field;
                  });
    }
    return result;
}

ProfileId profile_policy_source(const EffectiveProfile& value) {
    switch (value.profile.assignment.membership) {
        case MembershipClass::cellranger_2024_a:
            return ProfileId::cr7_v1;
        case MembershipClass::conjunctive_2024_a:
            return ProfileId::pansc_strict_v1;
        case MembershipClass::all_annotated:
            return value.profile.id;
    }
    throw std::logic_error("unhandled membership class");
}

BarcodeCorrector::BarcodeCorrector(std::vector<std::string> whitelist, double threshold)
    : whitelist_(std::move(whitelist)), threshold_(threshold) {
    if (!std::isfinite(threshold_) || threshold_ < 0.0 || threshold_ > 1.0) {
        throw std::invalid_argument("barcode posterior threshold must be in [0,1]");
    }
    if (whitelist_.empty()) {
        throw std::invalid_argument("barcode whitelist must not be empty");
    }
    const size_t length = whitelist_.front().size();
    for (const std::string& barcode : whitelist_) {
        if (barcode.size() != length ||
            !std::all_of(barcode.begin(), barcode.end(), is_base)) {
            throw std::invalid_argument(
                "barcode whitelist entries must be equal-length uppercase A/C/G/T strings");
        }
    }
    std::sort(whitelist_.begin(), whitelist_.end());
    if (std::adjacent_find(whitelist_.begin(), whitelist_.end()) != whitelist_.end()) {
        throw std::invalid_argument("barcode whitelist repeats an entry");
    }
}

void BarcodeCorrector::observe(const BarcodeRead& read) {
    if (read.primary && std::binary_search(whitelist_.begin(), whitelist_.end(),
                                          read.raw_barcode)) {
        ++exact_counts_[read.raw_barcode];
    }
}

std::uint64_t BarcodeCorrector::exact_count(std::string_view barcode) const {
    const auto found = exact_counts_.find(barcode);
    return found == exact_counts_.end() ? 0 : found->second;
}

BarcodeCorrection BarcodeCorrector::correct(const BarcodeRead& read) const {
    if (std::binary_search(whitelist_.begin(), whitelist_.end(), read.raw_barcode)) {
        return {read.raw_barcode, false};
    }
    if (read.raw_barcode.size() != whitelist_.front().size()) {
        return {};
    }
    const bool usable_quality =
        read.quality && read.quality->size() == read.raw_barcode.size();

    size_t invalid_count = 0;
    size_t invalid_at = 0;
    for (size_t index = 0; index < read.raw_barcode.size(); ++index) {
        if (!is_base(read.raw_barcode[index])) {
            ++invalid_count;
            invalid_at = index;
        }
    }
    if (invalid_count > 1) {
        return {};
    }

    std::vector<std::pair<std::string, size_t>> candidates;
    std::string candidate = read.raw_barcode;
    auto try_candidate = [&](size_t mismatch, char base) {
        candidate = read.raw_barcode;
        candidate[mismatch] = base;
        if (std::binary_search(whitelist_.begin(), whitelist_.end(), candidate)) {
            candidates.emplace_back(candidate, mismatch);
        }
    };
    if (invalid_count == 1) {
        for (char base : kBases) {
            try_candidate(invalid_at, base);
        }
    } else {
        for (size_t index = 0; index < read.raw_barcode.size(); ++index) {
            for (char base : kBases) {
                if (base != read.raw_barcode[index]) {
                    try_candidate(index, base);
                }
            }
        }
    }
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()), candidates.end());
    if (candidates.empty()) {
        return {};
    }

    double total = 0.0;
    double maximum = -1.0;
    std::string winner;
    for (const auto& [barcode, mismatch] : candidates) {
        double weight = static_cast<double>(exact_count(barcode) + 1);
        if (usable_quality) {
            int quality = static_cast<unsigned char>((*read.quality)[mismatch]) - 33;
            quality = std::clamp(quality, 0, 33);
            weight *= std::pow(10.0, -static_cast<double>(quality) / 10.0);
        }
        total += weight;
        if (weight > maximum) {
            maximum = weight;
            winner = barcode;
        }
    }
    if (maximum >= threshold_ * total) {
        return {winner, true};
    }
    return {};
}

BasicUmiStatus basic_umi_status(std::string_view umi) {
    if (umi.empty()) {
        return BasicUmiStatus::empty;
    }
    bool homopolymer = true;
    for (char base : umi) {
        if (base == 'N') {
            return BasicUmiStatus::contains_n;
        }
        if (base != umi.front()) {
            homopolymer = false;
        }
    }
    return homopolymer ? BasicUmiStatus::homopolymer : BasicUmiStatus::valid;
}

bool basic_umi_valid(std::string_view umi) {
    return basic_umi_status(umi) == BasicUmiStatus::valid;
}

std::map<std::string, std::string>
collapse_1mm_cr(const std::map<std::string, std::uint64_t>& counts) {
    std::vector<std::string> umis;
    umis.reserve(counts.size());
    for (const auto& [umi, count] : counts) {
        static_cast<void>(count);
        umis.push_back(umi);
    }
    std::sort(umis.begin(), umis.end(), [&](const std::string& left, const std::string& right) {
        return counts.at(left) != counts.at(right) ? counts.at(left) < counts.at(right)
                                                   : left < right;
    });
    std::map<std::string, std::string> labels;
    for (size_t index = 0; index < umis.size(); ++index) {
        labels[umis[index]] = umis[index];
        for (size_t higher = umis.size(); higher-- > index + 1;) {
            if (hamming_one(umis[index], umis[higher])) {
                labels[umis[index]] = umis[higher];
                break;
            }
        }
    }
    return labels;
}

CountResult count_observations(std::vector<AssignmentObservation> observations) {
    using UmiCounts = std::map<std::string, std::uint64_t>;
    using GeneUmis = std::map<std::string, UmiCounts>;
    std::map<std::string, GeneUmis> raw;

    for (AssignmentObservation& observation : observations) {
        observation.genes = sorted_unique(std::move(observation.genes));
        if (observation.genes.size() == 1 && basic_umi_valid(observation.umi) &&
            observation.read_count > 0) {
            raw[observation.barcode][observation.genes.front()][observation.umi] +=
                observation.read_count;
        }
    }

    CountResult result;
    for (const auto& [barcode, genes] : raw) {
        std::map<std::string, std::map<std::string, std::uint64_t>> corrected;
        std::map<std::string, std::map<std::string, std::uint64_t>> original;
        std::map<std::string, std::map<std::string, std::uint64_t>> raw_umi_counts;
        for (const auto& [gene, umi_counts] : genes) {
            const auto labels = collapse_1mm_cr(umi_counts);
            for (const auto& [umi, count] : umi_counts) {
                original[umi][gene] += count;
                corrected[labels.at(umi)][gene] += count;
                ++raw_umi_counts[labels.at(umi)][gene];
            }
        }
        for (const auto& [umi, per_gene] : corrected) {
            std::uint64_t maximum = 0;
            std::string winner;
            bool tied = false;
            for (const auto& [gene, count] : per_gene) {
                if (count > maximum) {
                    maximum = count;
                    winner = gene;
                    tied = false;
                } else if (count == maximum) {
                    tied = true;
                }
            }
            if (winner.empty() || tied) {
                continue;
            }
            const std::uint64_t winner_raw = original[umi][winner];
            const bool raw_guard = std::any_of(
                original[umi].begin(), original[umi].end(),
                [&](const auto& item) { return item.second > winner_raw; });
            if (raw_guard) {
                continue;
            }
            result.molecules.push_back(
                {barcode, umi, winner, maximum, raw_umi_counts[umi][winner]});
        }
    }

    std::sort(result.molecules.begin(), result.molecules.end(),
              [](const MoleculeRecord& left, const MoleculeRecord& right) {
                  return std::tie(left.barcode, left.gene, left.umi) <
                         std::tie(right.barcode, right.gene, right.umi);
              });
    std::map<std::pair<std::string, std::string>, std::uint64_t> totals;
    for (const MoleculeRecord& molecule : result.molecules) {
        ++totals[{molecule.barcode, molecule.gene}];
    }
    for (const auto& [key, count] : totals) {
        result.counts.push_back({key.first, key.second, count});
    }
    return result;
}

}  // namespace pancollapse::direct_count
