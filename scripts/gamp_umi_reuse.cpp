#include "count_assignment_signature.hpp"
#include "gamp_alignment_complexity.hpp"
#include "read_granular_input.hpp"

#include <cctype>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

// This deliberately mirrors prepared_count.cpp's private parse_molecule(): the
// diagnostic must use the same raw-name contract without loading a reference.
struct Molecule {
    std::string barcode;
    std::string umi;
};

bool supported_base(char value) {
    switch (std::toupper(static_cast<unsigned char>(value))) {
    case 'A': case 'C': case 'G': case 'T': case 'N': return true;
    default: return false;
    }
}

int hex(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    value = static_cast<char>(std::tolower(static_cast<unsigned char>(value)));
    return value >= 'a' && value <= 'f' ? value - 'a' + 10 : -1;
}

void remove_quality(std::string& name, std::string_view tag, std::size_t expected) {
    const auto separator = name.rfind('_');
    if (separator == std::string::npos || !name.substr(separator + 1).starts_with(tag)) return;
    const std::string encoded = name.substr(separator + 1 + tag.size());
    if (encoded.size() != expected * 2) throw std::runtime_error("malformed raw molecule quality suffix");
    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        const int high = hex(encoded[i]), low = hex(encoded[i + 1]);
        if (high < 0 || low < 0) throw std::runtime_error("non-hexadecimal raw molecule quality suffix");
        const unsigned char quality = static_cast<unsigned char>((high << 4) | low);
        if (quality < 33 || quality > 126) throw std::runtime_error("non-printable raw molecule quality suffix");
    }
    name.resize(separator);
}

Molecule parse_molecule(std::string name, std::size_t cb_length, std::size_t umi_length) {
    const std::string before_umi_quality = name;
    remove_quality(name, "uy", umi_length);
    const bool has_umi_quality = name != before_umi_quality;
    const std::string before_barcode_quality = name;
    remove_quality(name, "cy", cb_length);
    if (has_umi_quality && name == before_barcode_quality) {
        throw std::runtime_error("raw UMI quality requires barcode quality");
    }
    const auto umi_separator = name.rfind('_');
    if (umi_separator == std::string::npos) throw std::runtime_error("GAMP name does not contain raw UMI");
    const auto barcode_separator = name.rfind('_', umi_separator == 0 ? 0 : umi_separator - 1);
    if (barcode_separator == std::string::npos || barcode_separator == 0) {
        throw std::runtime_error("GAMP name does not contain raw CB");
    }
    Molecule result{name.substr(barcode_separator + 1, umi_separator - barcode_separator - 1),
                    name.substr(umi_separator + 1)};
    if (result.barcode.size() != cb_length || result.umi.size() != umi_length) {
        throw std::runtime_error("raw CB/UMI lengths do not match configuration");
    }
    for (std::string* field : {&result.barcode, &result.umi}) {
        for (char& base : *field) {
            if (!supported_base(base)) throw std::runtime_error("raw CB/UMI contains unsupported base");
            base = static_cast<char>(std::toupper(static_cast<unsigned char>(base)));
        }
    }
    return result;
}

std::string exact_key(std::string_view first, std::string_view second) {
    std::string result;
    result.reserve(first.size() + second.size() + 1);
    result.append(first);
    result.push_back('\0');
    result.append(second);
    return result;
}

void append_length_prefixed(std::string& output, std::string_view value) {
    const std::uint64_t length = value.size();
    for (int shift = 56; shift >= 0; shift -= 8) output.push_back(static_cast<char>((length >> shift) & 0xffU));
    output.append(value);
}

void append_signature_id(std::string& output, std::uint32_t value) {
    for (int shift = 24; shift >= 0; shift -= 8) output.push_back(static_cast<char>((value >> shift) & 0xffU));
}

struct StoredRecord {
    std::unique_ptr<std::string> bytes;
    std::optional<vg::MultipathAlignment> decoded;
};

struct Group {
    std::string name;
    std::vector<StoredRecord> records;
};

struct Family {
    std::uint64_t groups = 0;
    std::uint32_t first_signature = std::numeric_limits<std::uint32_t>::max();
    bool mixed_signatures = false;
};

struct Summary {
    std::uint64_t input_groups = 0;
    std::uint64_t input_records = 0;
    std::uint64_t valid_molecule_groups = 0;
    std::uint64_t valid_molecule_groups_featureless = 0;
    std::uint64_t cutoff_drops = 0;
    std::uint64_t eligible_valid_groups = 0;
    std::uint64_t sequence_family_signature_mismatches = 0;
    std::unordered_map<std::string, Family> families;
    std::unordered_map<std::string, Family> sequence_families;
    std::unordered_map<std::string, std::uint32_t> signature_ids;
    std::unordered_set<std::string> eligible_family_signatures;
};

std::uint32_t intern_signature(Summary& summary, std::string signature) {
    if (summary.signature_ids.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error("too many distinct graph signatures for uint32 interning");
    }
    const auto [it, inserted] = summary.signature_ids.emplace(std::move(signature),
                                                               static_cast<std::uint32_t>(summary.signature_ids.size()));
    static_cast<void>(inserted);
    return it->second;
}

Family& observe_family_group(std::unordered_map<std::string, Family>& families, std::string key) {
    auto [it, inserted] = families.try_emplace(std::move(key));
    static_cast<void>(inserted);
    ++it->second.groups;
    return it->second;
}

void observe_family_signature(Family& family, std::uint32_t signature,
                              std::uint64_t* mismatch_count) {
    if (family.first_signature == std::numeric_limits<std::uint32_t>::max()) {
        family.first_signature = signature;
        return;
    }
    if (family.first_signature != signature && !family.mixed_signatures) {
        family.mixed_signatures = true;
        if (mismatch_count != nullptr) ++*mismatch_count;
    }
}

void process_group(Group& group, Summary& summary, pancollapse::input::GampAlignmentComplexity& complexity) {
    ++summary.input_groups;
    Molecule molecule;
    try {
        molecule = parse_molecule(group.name, 16, 12);
    } catch (const std::runtime_error&) {
        return;
    }
    ++summary.valid_molecule_groups;
    Family& raw_family = observe_family_group(summary.families,
                                               exact_key(molecule.barcode, molecule.umi));
    if (group.records.empty()) ++summary.valid_molecule_groups_featureless;
    std::vector<const vg::MultipathAlignment*> records;
    records.reserve(group.records.size());
    for (StoredRecord& stored : group.records) {
        if (!stored.decoded) {
            stored.decoded.emplace();
            pancollapse::input::decode_multipath_record(*stored.bytes, *stored.decoded);
        }
        records.push_back(&*stored.decoded);
    }
    if (complexity.exceeds(records, pancollapse::input::kDefaultMaxAlignmentsPerRead)) {
        ++summary.cutoff_drops;
        return;
    }
    ++summary.eligible_valid_groups;
    const std::uint32_t signature = intern_signature(
        summary, pancollapse::prepared_count::assignment_graph_signature(records));
    std::string family_signature_key;
    append_length_prefixed(family_signature_key, molecule.barcode);
    append_length_prefixed(family_signature_key, molecule.umi);
    append_signature_id(family_signature_key, signature);
    summary.eligible_family_signatures.insert(std::move(family_signature_key));
    observe_family_signature(raw_family, signature, nullptr);
    // Production retains only records with subpaths. The first retained record's
    // sequence is the raw-sequence component of this supplementary bucket; an
    // empty retained-record list has the corresponding empty sequence key.
    const std::string_view raw_sequence = records.empty() ? std::string_view{} : records.front()->sequence();
    Family& sequence_family = observe_family_group(
        summary.sequence_families,
        exact_key(exact_key(molecule.barcode, molecule.umi), raw_sequence));
    observe_family_signature(sequence_family, signature, &summary.sequence_family_signature_mismatches);
}

std::uint64_t parse_limit(const char* text) {
    std::size_t parsed = 0;
    const auto value = std::stoull(text, &parsed);
    if (parsed != std::string(text).size() || text[0] == '-') {
        throw std::runtime_error("GROUPS must be a non-negative integer");
    }
    return value;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 2 && argc != 3) {
            throw std::runtime_error("usage: pancollapse_gamp_umi_reuse INPUT [GROUPS]");
        }
        const std::uint64_t group_limit = argc == 3 ? parse_limit(argv[2]) : std::numeric_limits<std::uint64_t>::max();
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) throw std::runtime_error("cannot open GAMP input");

        Summary summary;
        Group current;
        bool have_group = false;
        bool limit_reached = group_limit == 0;
        pancollapse::input::GampAlignmentComplexity complexity;
        const auto finish_current = [&] {
            if (!have_group || limit_reached) return;
            process_group(current, summary, complexity);
            if (summary.input_groups == group_limit) limit_reached = true;
            current = {};
            have_group = false;
        };
        pancollapse::input::raw_gamp_for_each(input, [&](std::unique_ptr<std::string> bytes) {
            if (limit_reached) return;
            pancollapse::input::MultipathRecordPeek peek;
            std::optional<vg::MultipathAlignment> decoded;
            if (!pancollapse::input::peek_multipath_record(*bytes, peek)) {
                decoded.emplace();
                pancollapse::input::decode_multipath_record(*bytes, *decoded);
                peek.name = decoded->name();
                peek.has_subpath = decoded->subpath_size() != 0;
            }
            if (have_group && peek.name != current.name) finish_current();
            if (limit_reached) return;
            ++summary.input_records;
            if (!have_group) {
                current.name.assign(peek.name);
                have_group = true;
            }
            // This matches prepared_count's producer: featureless records establish
            // the QNAME group but never enter complexity or graph resolution.
            if (peek.has_subpath) {
                current.records.push_back({std::move(bytes), std::move(decoded)});
            }
        }, 2);
        finish_current();

        std::uint64_t repeated_family_reads = 0;
        std::uint64_t mixed_signatures = 0;
        std::uint64_t histogram_1 = 0, histogram_2 = 0, histogram_3_5 = 0;
        std::uint64_t histogram_6_10 = 0, histogram_11_100 = 0, histogram_over_100 = 0;
        for (const auto& [key, family] : summary.families) {
            static_cast<void>(key);
            repeated_family_reads += family.groups - 1;
            mixed_signatures += family.mixed_signatures;
            if (family.groups == 1) ++histogram_1;
            else if (family.groups == 2) ++histogram_2;
            else if (family.groups <= 5) ++histogram_3_5;
            else if (family.groups <= 10) ++histogram_6_10;
            else if (family.groups <= 100) ++histogram_11_100;
            else ++histogram_over_100;
        }
        std::cout << "{\"schema\":\"pancollapse-gamp-umi-reuse-v1\""
                  << ",\"input_groups\":" << summary.input_groups
                  << ",\"input_records\":" << summary.input_records
                  << ",\"valid_molecule_groups\":" << summary.valid_molecule_groups
                  << ",\"valid_molecule_groups_featureless_before_cutoff\":" << summary.valid_molecule_groups_featureless
                  << ",\"cutoff_drops\":" << summary.cutoff_drops
                  << ",\"eligible_valid_groups\":" << summary.eligible_valid_groups
                  << ",\"raw_cb_umi_families\":" << summary.families.size()
                  << ",\"family_size_histogram\":{\"1\":" << histogram_1
                  << ",\"2\":" << histogram_2 << ",\"3-5\":" << histogram_3_5
                  << ",\"6-10\":" << histogram_6_10 << ",\"11-100\":" << histogram_11_100
                  << ",\">100\":" << histogram_over_100 << "}"
                  << ",\"repeated_family_reads\":" << repeated_family_reads
                  << ",\"mixed_full_graph_signatures_per_family\":" << mixed_signatures
                  << ",\"same_sequence_cb_umi_buckets_with_mixed_full_graph_signatures\":" << summary.sequence_family_signature_mismatches
                  << ",\"distinct_graphs\":" << summary.signature_ids.size()
                  << ",\"ideal_graph_resolver_redundancy\":"
                  << (summary.eligible_valid_groups - summary.signature_ids.size())
                  << ",\"distinct_eligible_raw_cb_umi_full_signatures\":" << summary.eligible_family_signatures.size()
                  << ",\"eligible_groups_minus_distinct_raw_cb_umi_full_signatures\":"
                  << (summary.eligible_valid_groups - summary.eligible_family_signatures.size())
                  << ",\"metric_denominators\":{\"family_metrics\":\"valid molecule groups before cutoff, grouped by exact normalized raw CB and UMI\",\"mixed_signature_metrics\":\"eligible valid groups within each raw CB and UMI family\",\"graph_metrics\":\"valid molecule groups after the production producer retained subpath records and 64000000-route cutoff\",\"featureless\":\"valid molecule groups before cutoff with no producer-retained subpath records\",\"within_umi_reuse_ceiling\":\"eligible groups minus exact (raw CB, raw UMI, full graph signature) tuples\"}"
                  << "}\n";
    } catch (const std::exception& error) {
        std::cerr << "pancollapse_gamp_umi_reuse: " << error.what() << '\n';
        return 1;
    }
}
