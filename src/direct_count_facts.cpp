#include "direct_count_facts.hpp"

#include <openssl/evp.h>
#include <simdjson.h>

#include <algorithm>
#include <array>
#include <fstream>
#include <iomanip>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>

namespace pancollapse::direct_count {
namespace {

using Row = std::map<std::string, std::string, std::less<>>;

std::string digest_bytes(std::string_view bytes) {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),
                                                                  EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1 ||
        EVP_DigestUpdate(context.get(), bytes.data(), bytes.size()) != 1) {
        throw std::runtime_error("cannot initialize SHA-256");
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &length) != 1 || length != 32) {
        throw std::runtime_error("cannot finalize SHA-256");
    }
    std::ostringstream result;
    for (unsigned int index = 0; index < length; ++index) {
        result << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(digest[index]);
    }
    return result.str();
}

std::uint64_t uint_field(simdjson::dom::element object, std::string_view field) {
    try {
        return static_cast<std::uint64_t>(object[field]);
    } catch (const simdjson::simdjson_error&) {
        throw std::runtime_error("count-fact manifest field " + std::string(field) +
                                 " must be uint64");
    }
}

std::string string_field(simdjson::dom::element object, std::string_view field) {
    try {
        return std::string(static_cast<std::string_view>(object[field]));
    } catch (const simdjson::simdjson_error&) {
        throw std::runtime_error("count-fact manifest field " + std::string(field) +
                                 " must be a string");
    }
}

bool path_is_within(const std::filesystem::path& root,
                    const std::filesystem::path& candidate) {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    for (; root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *root_part != *candidate_part) {
            return false;
        }
    }
    return true;
}

std::vector<std::string> split_tab(const std::string& line) {
    std::vector<std::string> fields;
    size_t begin = 0;
    while (begin <= line.size()) {
        const size_t tab = line.find('\t', begin);
        if (tab == std::string::npos) {
            fields.push_back(line.substr(begin));
            break;
        }
        fields.push_back(line.substr(begin, tab - begin));
        begin = tab + 1;
    }
    return fields;
}

std::vector<Row> read_tsv(const std::filesystem::path& path,
                          const std::vector<std::string>& expected_header) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open count fact " + path.string());
    }
    std::string line;
    if (!std::getline(input, line)) {
        throw std::runtime_error("count fact is empty: " + path.string());
    }
    if (split_tab(line) != expected_header) {
        throw std::runtime_error("count fact has an unexpected header: " + path.string());
    }
    std::vector<Row> rows;
    size_t line_number = 1;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty()) {
            throw std::runtime_error("count fact has an empty line at " + path.string() + ":" +
                                     std::to_string(line_number));
        }
        std::vector<std::string> fields = split_tab(line);
        if (fields.size() != expected_header.size()) {
            throw std::runtime_error("count fact has the wrong field count at " + path.string() +
                                     ":" + std::to_string(line_number));
        }
        Row row;
        for (size_t index = 0; index < expected_header.size(); ++index) {
            if (fields[index].find_first_of("\r\n") != std::string::npos) {
                throw std::runtime_error("count fact contains a line delimiter in a field");
            }
            row.emplace(expected_header[index], std::move(fields[index]));
        }
        rows.push_back(std::move(row));
    }
    if (rows.empty()) {
        throw std::runtime_error("count fact has no data rows: " + path.string());
    }
    return rows;
}

std::vector<std::string> token_list(std::string_view text, std::string_view field) {
    if (text == ".") {
        return {};
    }
    std::vector<std::string> result;
    size_t begin = 0;
    while (begin <= text.size()) {
        const size_t comma = text.find(',', begin);
        std::string token(text.substr(begin, comma == std::string_view::npos
                                                ? text.size() - begin
                                                : comma - begin));
        if (token.empty() || token.find_first_of(" \t\r\n") != std::string::npos) {
            throw std::runtime_error("count fact has malformed " + std::string(field));
        }
        result.push_back(std::move(token));
        if (comma == std::string_view::npos) {
            break;
        }
        begin = comma + 1;
    }
    std::sort(result.begin(), result.end());
    if (std::adjacent_find(result.begin(), result.end()) != result.end()) {
        throw std::runtime_error("count fact repeats a token in " + std::string(field));
    }
    return result;
}

bool parse_boolean(std::string_view value, std::string_view field) {
    if (value == "true") {
        return true;
    }
    if (value == "false") {
        return false;
    }
    throw std::runtime_error("count fact " + std::string(field) +
                             " must be true or false");
}

CompetitionClass parse_competition(std::string_view value) {
    if (value == "PRIMARY") {
        return CompetitionClass::primary;
    }
    if (value == "FALLBACK") {
        return CompetitionClass::fallback;
    }
    if (value == "UNKNOWN") {
        return CompetitionClass::unknown;
    }
    throw std::runtime_error("gene-policy competition_class is invalid");
}

ProfileId parse_policy_profile(std::string_view value) {
    if (value == "cr7-v1") {
        return ProfileId::cr7_v1;
    }
    if (value == "pansc-strict-v1") {
        return ProfileId::pansc_strict_v1;
    }
    throw std::runtime_error("profile gene-policy profile_id is invalid");
}

std::string unqualified(std::string value) {
    const size_t first = value.find('#');
    const size_t second = first == std::string::npos ? std::string::npos
                                                     : value.find('#', first + 1);
    if (second != std::string::npos) {
        value.erase(0, second + 1);
    }
    return value;
}

struct EnsemblIdentity {
    std::string stable_gene;
    std::string discriminator;
};

std::optional<EnsemblIdentity> ensembl_identity(std::string value) {
    static const std::regex pattern(R"(^(ENSG[0-9]+)(?:\.[0-9]+)?(_[0-9]+|__LOC[0-9]+)?$)");
    value = unqualified(std::move(value));
    std::smatch match;
    if (!std::regex_match(value, match, pattern)) {
        return std::nullopt;
    }
    return EnsemblIdentity{match[1].str(), match[2].matched ? match[2].str() : ""};
}

std::string normalized_locus(const path_identity::AnnotationIdentity& identity) {
    const std::string source = identity.source_gene;
    if (!source.empty() && source != "N/A") {
        if (const auto parsed = ensembl_identity(source)) {
            return parsed->stable_gene + parsed->discriminator;
        }
        return source;
    }
    if (!identity.gene_locus.empty() && identity.gene_locus != "N/A") {
        if (const auto parsed = ensembl_identity(identity.gene_locus)) {
            return parsed->stable_gene + parsed->discriminator;
        }
        return identity.gene_locus;
    }
    return "__panSC_runtime_gene__:" + identity.gene_id;
}

const CountFactFile& required_file(const CountFactBundle& bundle,
                                   std::string_view role) {
    const auto found = bundle.files.find(std::string(role));
    if (found == bundle.files.end()) {
        throw std::runtime_error("count-fact bundle is missing required role " +
                                 std::string(role));
    }
    return found->second;
}

void require_schema(const CountFactBundle& bundle, std::string_view role,
                    std::string_view expected_schema) {
    const CountFactFile& file = required_file(bundle, role);
    if (file.schema != expected_schema) {
        throw std::runtime_error("count-fact bundle role " + std::string(role) +
                                 " has schema " + file.schema + "; expected " +
                                 std::string(expected_schema));
    }
}

}  // namespace

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open file for SHA-256: " + path.string());
    }
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context(EVP_MD_CTX_new(),
                                                                  EVP_MD_CTX_free);
    if (!context || EVP_DigestInit_ex(context.get(), EVP_sha256(), nullptr) != 1) {
        throw std::runtime_error("cannot initialize file SHA-256");
    }
    std::array<char, 1 << 20> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0 && EVP_DigestUpdate(context.get(), buffer.data(),
                                          static_cast<size_t>(count)) != 1) {
            throw std::runtime_error("cannot update file SHA-256");
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("failed while reading file for SHA-256: " + path.string());
    }
    std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
    unsigned int length = 0;
    if (EVP_DigestFinal_ex(context.get(), digest.data(), &length) != 1 || length != 32) {
        throw std::runtime_error("cannot finalize file SHA-256");
    }
    std::ostringstream result;
    for (unsigned int index = 0; index < length; ++index) {
        result << std::hex << std::setw(2) << std::setfill('0')
               << static_cast<unsigned int>(digest[index]);
    }
    return result.str();
}

CountFactBundle load_count_fact_bundle(const std::filesystem::path& requested_root) {
    const std::filesystem::path requested = std::filesystem::canonical(requested_root);
    const bool manifest_was_named = std::filesystem::is_regular_file(requested);
    const std::filesystem::path manifest_path =
        manifest_was_named ? requested : requested / "MANIFEST.json";
    if (!std::filesystem::is_regular_file(manifest_path)) {
        throw std::runtime_error("count-fact bundle lacks MANIFEST.json");
    }
    if (manifest_was_named && manifest_path.filename() != "MANIFEST.json") {
        throw std::runtime_error("count-fact bundle manifest must be named MANIFEST.json");
    }
    const std::filesystem::path root = manifest_path.parent_path();

    try {
        simdjson::dom::parser parser;
        simdjson::dom::element document = parser.load(manifest_path.string());
        if (string_field(document, "schema") != kCountFactBundleSchema) {
            throw std::runtime_error("count-fact bundle has an unsupported schema");
        }
        CountFactBundle bundle;
        bundle.root = root;
        bundle.manifest_path = manifest_path;
        bundle.content_id = string_field(document, "content_id");
        if (!bundle.content_id.starts_with("sha256:") ||
            bundle.content_id.size() != std::string("sha256:").size() + 64) {
            throw std::runtime_error("count-fact bundle has a malformed content_id");
        }
        simdjson::dom::element content = document["content"];
        const std::string canonical_content = simdjson::minify(content);
        if (bundle.content_id != "sha256:" + digest_bytes(canonical_content)) {
            throw std::runtime_error("count-fact bundle content_id does not match content");
        }

        const simdjson::dom::element assignment_contract =
            content["assignment_contract"];
        bundle.transcript_filter_mode =
            string_field(assignment_contract, "transcript_filter_mode");
        if (bundle.transcript_filter_mode != "parent-category-ledger-v3" ||
            string_field(assignment_contract, "nested_host_interpretation") !=
                "relation-owner-loses" ||
            string_field(assignment_contract, "score_window_order") !=
                "before-parent-category-filter" ||
            string_field(assignment_contract, "profile_policy_source") !=
                "profile-specific-certified-ledgers") {
            throw std::runtime_error(
                "count-fact bundle has an unsupported assignment contract");
        }

        simdjson::dom::object files = content["files"];
        for (const auto field : files) {
            const std::string role(field.key);
            const simdjson::dom::element record = field.value;
            const std::filesystem::path relative = string_field(record, "path");
            if (relative.empty() || relative.is_absolute() ||
                relative.lexically_normal() != relative) {
                throw std::runtime_error("count-fact bundle role " + role +
                                         " has a non-canonical relative path");
            }
            const std::filesystem::path resolved = std::filesystem::canonical(root / relative);
            if (!path_is_within(root, resolved) || !std::filesystem::is_regular_file(resolved)) {
                throw std::runtime_error("count-fact bundle role " + role +
                                         " escapes the bundle or is not a regular file");
            }
            CountFactFile file;
            file.path = resolved;
            file.schema = string_field(record, "schema");
            file.sha256 = string_field(record, "sha256");
            file.source_sha256 = string_field(record, "source_sha256");
            file.size_bytes = uint_field(record, "size_bytes");
            file.source_size_bytes = uint_field(record, "source_size_bytes");
            if (std::filesystem::file_size(resolved) != file.size_bytes) {
                throw std::runtime_error("count-fact bundle role " + role +
                                         " has a size mismatch");
            }
            if (sha256_file(resolved) != file.sha256) {
                throw std::runtime_error("count-fact bundle role " + role +
                                         " has a SHA-256 mismatch");
            }
            if (!bundle.files.emplace(role, std::move(file)).second) {
                throw std::runtime_error("count-fact bundle repeats role " + role);
            }
        }
        require_schema(bundle, "path_identity_ledger", "panSC-path-identity-v1");
        require_schema(bundle, "profile_gene_policy",
                       "panSC-count-profile-gene-policy-v1");
        require_schema(bundle, "profile_policy_receipt_cr7_v1",
                       "panSC-count-cr-gene-policy-only-v2");
        require_schema(bundle, "profile_policy_receipt_pansc_strict_v1",
                       "panSC-projected-nested-host-policy-v1");
        require_schema(bundle, "parent_category_ledger", "panSC-parent-category-ledger-v3");
        require_schema(bundle, "parent_category_receipt",
                       "panSC-parent-category-ledger-v3");
        require_schema(bundle, "projected_nested_policy",
                       "panSC-projected-nested-host-policy-v1");
        require_schema(bundle, "strong_support_ledger", "panSC-strong-support-audit-v1");
        require_schema(bundle, "corrected_annotation", "annotation");
        if (bundle.files.contains("gene_metadata")) {
            require_schema(bundle, "gene_metadata", "panSC-gene-metadata-v1");
        }
        return bundle;
    } catch (const simdjson::simdjson_error& error) {
        throw std::runtime_error("cannot parse count-fact MANIFEST.json: " +
                                 std::string(error.what()));
    }
}

CountFactCatalog CountFactCatalog::load(
    const CountFactBundle& bundle,
    const path_identity::PathIdentityLedger& path_ledger) {
    CountFactCatalog result;
    const auto gene_rows = read_tsv(
        required_file(bundle, "profile_gene_policy").path,
        {"profile_id", "count_gene", "gene_type", "competition_class",
         "competition_reason", "equivalence_gene", "source_annotation", "nested_host"});
    for (const Row& row : gene_rows) {
        const ProfileId profile_id = parse_policy_profile(row.at("profile_id"));
        const std::string& count_gene = row.at("count_gene");
        if (count_gene.empty()) {
            throw std::runtime_error("gene-policy count_gene must not be empty");
        }
        const std::vector<std::string> gene_types =
            token_list(row.at("gene_type"), "gene_type");
        auto [metadata, metadata_inserted] = result.genes_.try_emplace(
            count_gene, GeneFact{count_gene, gene_types, std::nullopt});
        if (!metadata_inserted && metadata->second.gene_types != gene_types) {
            throw std::runtime_error(
                "profile gene policies disagree on gene_type for " + count_gene);
        }
        ProfileGeneFact fact;
        fact.profile_id = profile_id;
        fact.count_gene = count_gene;
        fact.competition = parse_competition(row.at("competition_class"));
        fact.competition_reason = row.at("competition_reason");
        fact.equivalence_gene = row.at("equivalence_gene");
        fact.source_annotation = row.at("source_annotation");
        fact.nested_host = row.at("nested_host") == "." ? "" : row.at("nested_host");
        if (fact.equivalence_gene.empty()) {
            throw std::runtime_error("gene-policy equivalence_gene must not be empty");
        }
        if (!result.profile_genes_[profile_id]
                 .emplace(count_gene, std::move(fact)).second) {
            throw std::runtime_error("profile gene-policy repeats (profile_id,count_gene)");
        }
    }
    const std::set<ProfileId> required_profiles{
        ProfileId::cr7_v1, ProfileId::pansc_strict_v1};
    for (const ProfileId profile_id : required_profiles) {
        const auto found = result.profile_genes_.find(profile_id);
        if (found == result.profile_genes_.end() ||
            found->second.size() != result.genes_.size()) {
            throw std::runtime_error(
                "profile gene-policy does not exactly cover the shared gene universe");
        }
        for (const auto& [count_gene, metadata] : result.genes_) {
            static_cast<void>(metadata);
            if (!found->second.contains(count_gene)) {
                throw std::runtime_error(
                    "profile gene-policy is missing count_gene " + count_gene);
            }
        }
    }

    const auto metadata_file = bundle.files.find("gene_metadata");
    if (metadata_file != bundle.files.end()) {
        const auto metadata_rows = read_tsv(
            metadata_file->second.path, {"count_gene", "gene_name", "gene_type"});
        std::set<std::string> seen;
        for (const Row& row : metadata_rows) {
            const std::string& count_gene = row.at("count_gene");
            const auto gene = result.genes_.find(count_gene);
            if (count_gene.empty() || gene == result.genes_.end() ||
                row.at("gene_name").empty() || !seen.insert(count_gene).second) {
                throw std::runtime_error(
                    "gene metadata contains an empty, duplicate, or unknown count_gene");
            }
            if (token_list(row.at("gene_type"), "gene_type") != gene->second.gene_types) {
                throw std::runtime_error(
                    "gene metadata gene_type disagrees with gene policy for " + count_gene);
            }
            gene->second.gene_name = row.at("gene_name");
        }
        if (seen.size() != result.genes_.size()) {
            throw std::runtime_error(
                "gene metadata does not exactly cover gene-policy count genes");
        }
    }

    const auto category_rows = read_tsv(
        required_file(bundle, "parent_category_ledger").path,
        {"unique_parent", "categories"});
    std::map<std::string, std::vector<std::string>, std::less<>> categories;
    for (const Row& row : category_rows) {
        if (!categories.emplace(row.at("unique_parent"),
                                token_list(row.at("categories"), "categories"))
                 .second) {
            throw std::runtime_error("Parent-category ledger repeats a Parent");
        }
    }

    struct StrongFact {
        std::string count_gene;
        std::vector<std::string> raw;
        std::vector<std::string> effective;
        std::vector<std::string> exempted;
        bool strong = false;
        bool protected_host = false;
    };
    std::map<std::string, StrongFact, std::less<>> strong;
    const auto strong_rows = read_tsv(
        required_file(bundle, "strong_support_ledger").path,
        {"unique_parent", "count_gene", "raw_categories", "effective_categories",
         "strong_support_tag", "protected_primary_protein_host", "exempted_categories"});
    for (const Row& row : strong_rows) {
        StrongFact fact;
        fact.count_gene = row.at("count_gene");
        fact.raw = token_list(row.at("raw_categories"), "raw_categories");
        fact.effective = token_list(row.at("effective_categories"), "effective_categories");
        fact.exempted = token_list(row.at("exempted_categories"), "exempted_categories");
        fact.strong = parse_boolean(row.at("strong_support_tag"), "strong_support_tag");
        fact.protected_host = parse_boolean(row.at("protected_primary_protein_host"),
                                            "protected_primary_protein_host");
        std::vector<std::string> expected;
        std::set_difference(fact.raw.begin(), fact.raw.end(), fact.exempted.begin(),
                            fact.exempted.end(), std::back_inserter(expected));
        if (expected != fact.effective) {
            throw std::runtime_error("strong-support effective categories disagree with raw minus "
                                     "exempted categories");
        }
        if (!strong.emplace(row.at("unique_parent"), std::move(fact)).second) {
            throw std::runtime_error("strong-support ledger repeats a Parent");
        }
    }

    if (categories.size() != path_ledger.identities_by_parent.size() ||
        strong.size() != path_ledger.identities_by_parent.size()) {
        throw std::runtime_error("Parent fact caches do not exactly cover the path ledger");
    }
    for (const auto& [parent, identity_pointer] : path_ledger.identities_by_parent) {
        const auto category = categories.find(parent);
        const auto support = strong.find(parent);
        if (category == categories.end() || support == strong.end()) {
            throw std::runtime_error("Parent fact caches are missing " + parent);
        }
        const path_identity::AnnotationIdentity& identity = *identity_pointer;
        ParentFact fact;
        fact.unique_parent = parent;
        fact.count_gene = normalized_locus(identity);
        fact.runtime_gene = identity.gene_id;
        fact.canonical_transcript = identity.canonical_transcript;
        fact.sample = identity.sample;
        fact.feature_layer = identity.feature_layer;
        fact.raw_categories = category->second;
        fact.strong_local_support = support->second.strong;
        fact.protected_primary_protein_nested_host = support->second.protected_host;
        if (support->second.raw != fact.raw_categories) {
            throw std::runtime_error("strong-support raw categories disagree for Parent " +
                                     parent);
        }
        if (support->second.count_gene != fact.count_gene) {
            throw std::runtime_error("strong-support count_gene disagrees for Parent " + parent);
        }
        if (result.genes_.count(fact.count_gene) == 0) {
            throw std::runtime_error("gene-policy ledger lacks Parent count identity " +
                                     fact.count_gene);
        }
        if (const auto parsed = ensembl_identity(identity.source_gene);
            parsed && !parsed->discriminator.empty()) {
            fact.novel_paralog = true;
            fact.novel_origin_gene = parsed->stable_gene;
        }
        result.parents_.emplace(parent, std::move(fact));
    }
    return result;
}

const ParentFact& CountFactCatalog::parent(std::string_view unique_parent) const {
    const auto found = parents_.find(unique_parent);
    if (found == parents_.end()) {
        throw std::runtime_error("count-fact catalog lacks Parent " +
                                 std::string(unique_parent));
    }
    return found->second;
}

const GeneFact& CountFactCatalog::gene(std::string_view count_gene) const {
    const auto found = genes_.find(count_gene);
    if (found == genes_.end()) {
        throw std::runtime_error("count-fact catalog lacks gene " + std::string(count_gene));
    }
    return found->second;
}

const ProfileGeneFact& CountFactCatalog::profile_gene(
    ProfileId profile_id, std::string_view count_gene) const {
    const auto profile = profile_genes_.find(profile_id);
    if (profile == profile_genes_.end()) {
        throw std::runtime_error("count-fact catalog lacks requested profile policy");
    }
    const auto found = profile->second.find(count_gene);
    if (found == profile->second.end()) {
        throw std::runtime_error("count-fact catalog lacks profile gene " +
                                 std::string(count_gene));
    }
    return found->second;
}

AssignmentCandidate CountFactCatalog::candidate(ProfileId profile_id,
                                                std::string_view unique_parent,
                                                std::int64_t score,
                                                EvidenceTier tier,
                                                EvidenceStrand strand,
                                                std::uint32_t body_sample_support) const {
    const ParentFact& parent_fact = parent(unique_parent);
    const GeneFact& gene_fact = gene(parent_fact.count_gene);
    const ProfileGeneFact& policy_fact = profile_gene(profile_id, parent_fact.count_gene);
    AssignmentCandidate result;
    result.gene = parent_fact.count_gene;
    result.equivalence_gene = policy_fact.equivalence_gene;
    result.nested_host = policy_fact.nested_host;
    result.score = score;
    result.tier = tier;
    result.strand = strand;
    result.competition = policy_fact.competition;
    result.categories = parent_fact.raw_categories;
    result.gene_types = gene_fact.gene_types;
    result.body_sample_support = body_sample_support;
    if (tier == EvidenceTier::body && !parent_fact.sample.empty() &&
        parent_fact.sample != "." && parent_fact.sample != "NA" &&
        parent_fact.sample != "N/A" && parent_fact.sample != "None" &&
        parent_fact.sample != "nan") {
        result.body_support_units.push_back(parent_fact.sample);
    }
    result.novel_paralog = parent_fact.novel_paralog;
    result.novel_origin_gene = parent_fact.novel_origin_gene;
    result.strong_local_support = parent_fact.strong_local_support;
    result.protected_primary_protein_nested_host =
        parent_fact.protected_primary_protein_nested_host;
    return result;
}

}  // namespace pancollapse::direct_count
