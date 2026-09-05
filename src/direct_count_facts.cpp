#include "direct_count_facts.hpp"

#include <openssl/evp.h>
#include <simdjson.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <memory>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <utility>
#include <string_view>

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

void append_unicode_escape(std::string& out, std::uint32_t unit) {
    static constexpr char digits[] = "0123456789abcdef";
    out += "\\u";
    out.push_back(digits[(unit >> 12U) & 0xFU]);
    out.push_back(digits[(unit >> 8U) & 0xFU]);
    out.push_back(digits[(unit >> 4U) & 0xFU]);
    out.push_back(digits[unit & 0xFU]);
}

// Python json.dumps(ensure_ascii=True) string escaping: two-character forms for
// \" \\ \n \r \t \b \f, and \uXXXX for every other control character and every
// non-ASCII code point (UTF-16 surrogate pairs above the BMP).
void append_canonical_string(std::string& out, std::string_view value) {
    out.push_back('"');
    size_t index = 0;
    while (index < value.size()) {
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        if (byte < 0x80U) {
            switch (byte) {
                case '"': out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                default:
                    if (byte >= 0x20U && byte < 0x7FU) {
                        out.push_back(static_cast<char>(byte));
                    } else {
                        append_unicode_escape(out, byte);
                    }
            }
            ++index;
            continue;
        }
        // simdjson validated the UTF-8, so the lead byte fixes the sequence length.
        size_t length = 2;
        std::uint32_t code_point = byte & 0x1FU;
        if ((byte & 0xF0U) == 0xE0U) {
            length = 3;
            code_point = byte & 0x0FU;
        } else if ((byte & 0xF8U) == 0xF0U) {
            length = 4;
            code_point = byte & 0x07U;
        }
        for (size_t offset = 1; offset < length && index + offset < value.size(); ++offset) {
            code_point = (code_point << 6U) |
                         (static_cast<unsigned char>(value[index + offset]) & 0x3FU);
        }
        index += length;
        if (code_point >= 0x10000U) {
            code_point -= 0x10000U;
            append_unicode_escape(out, 0xD800U + (code_point >> 10U));
            append_unicode_escape(out, 0xDC00U + (code_point & 0x3FFU));
        } else {
            append_unicode_escape(out, code_point);
        }
    }
    out.push_back('"');
}

// Python float.__repr__: shortest round-trip digits, fixed notation while the
// decimal point position is in [-3, 16], otherwise d.ddde[+-]XX with at least
// two exponent digits.
void append_python_float_repr(std::string& out, double value) {
    if (std::isnan(value) || std::isinf(value)) {
        throw std::runtime_error("count-fact bundle content contains a non-finite number");
    }
    std::array<char, 64> buffer{};
    const auto converted = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value,
                                         std::chars_format::scientific);
    std::string_view text(buffer.data(), static_cast<size_t>(converted.ptr - buffer.data()));
    if (!text.empty() && text.front() == '-') {
        out.push_back('-');
        text.remove_prefix(1);
    }
    const size_t exponent_at = text.find('e');
    std::string digits;
    for (const char character : text.substr(0, exponent_at)) {
        if (character != '.') {
            digits.push_back(character);
        }
    }
    const char* exponent_begin = text.data() + exponent_at + 1;
    if (*exponent_begin == '+') {
        ++exponent_begin;
    }
    int exponent = 0;
    std::from_chars(exponent_begin, text.data() + text.size(), exponent);
    const int point = exponent + 1;
    const int count = static_cast<int>(digits.size());
    if (point <= -4 || point > 16) {
        out.push_back(digits[0]);
        if (count > 1) {
            out.push_back('.');
            out.append(digits, 1, std::string::npos);
        }
        const int shown = point - 1;
        out.push_back('e');
        out.push_back(shown < 0 ? '-' : '+');
        const int magnitude = shown < 0 ? -shown : shown;
        if (magnitude < 10) {
            out.push_back('0');
        }
        out += std::to_string(magnitude);
    } else if (point <= 0) {
        out += "0.";
        out.append(static_cast<size_t>(-point), '0');
        out += digits;
    } else if (point < count) {
        out.append(digits, 0, static_cast<size_t>(point));
        out.push_back('.');
        out.append(digits, static_cast<size_t>(point), std::string::npos);
    } else {
        out += digits;
        out.append(static_cast<size_t>(point - count), '0');
        out += ".0";
    }
}

void append_canonical_json(std::string& out, const simdjson::dom::element& element) {
    switch (element.type()) {
        case simdjson::dom::element_type::ARRAY: {
            out.push_back('[');
            bool first = true;
            const simdjson::dom::array array = element.get_array().value();
            for (const simdjson::dom::element child : array) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;
                append_canonical_json(out, child);
            }
            out.push_back(']');
            return;
        }
        case simdjson::dom::element_type::OBJECT: {
            std::vector<std::pair<std::string_view, simdjson::dom::element>> members;
            const simdjson::dom::object object = element.get_object().value();
            for (const auto field : object) {
                members.emplace_back(field.key, field.value);
            }
            std::sort(members.begin(), members.end(),
                      [](const auto& left, const auto& right) { return left.first < right.first; });
            out.push_back('{');
            bool first = true;
            for (const auto& [key, value] : members) {
                if (!first) {
                    out.push_back(',');
                }
                first = false;
                append_canonical_string(out, key);
                out.push_back(':');
                append_canonical_json(out, value);
            }
            out.push_back('}');
            return;
        }
        case simdjson::dom::element_type::INT64:
            out += std::to_string(element.get_int64().value());
            return;
        case simdjson::dom::element_type::UINT64:
            out += std::to_string(element.get_uint64().value());
            return;
        case simdjson::dom::element_type::DOUBLE:
            append_python_float_repr(out, element.get_double().value());
            return;
        case simdjson::dom::element_type::STRING:
            append_canonical_string(out, element.get_string().value());
            return;
        case simdjson::dom::element_type::BOOL:
            out += element.get_bool().value() ? "true" : "false";
            return;
        case simdjson::dom::element_type::NULL_VALUE:
            out += "null";
            return;
        case simdjson::dom::element_type::BIGINT:
            throw std::runtime_error(
                "count-fact bundle content contains an integer beyond 64 bits");
    }
    throw std::runtime_error("count-fact bundle content has an unsupported JSON value");
}

// Canonical form of the manifest `content` object: keys sorted by code point, no
// whitespace, ASCII-only strings, shortest round-trip floats. Byte-identical to
// Python's json.dumps(content, sort_keys=True, separators=(",", ":")), so a
// producer's key order and whitespace never affect content_id.
std::string canonical_json(const simdjson::dom::element& element) {
    std::string out;
    append_canonical_json(out, element);
    return out;
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
        const std::string canonical_content = canonical_json(content);
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
    // Assignment resolves reads to equivalence_gene, so every target must be a count
    // gene now rather than failing on the first read that reaches it.
    for (const auto& [profile_id, facts] : result.profile_genes_) {
        for (const auto& [count_gene, fact] : facts) {
            const auto canonical = facts.find(fact.equivalence_gene);
            if (canonical == facts.end()) {
                throw std::runtime_error("gene-policy equivalence_gene " +
                                         fact.equivalence_gene + " for " + count_gene +
                                         " is not a count gene");
            }
            if (canonical->second.equivalence_gene != fact.equivalence_gene) {
                throw std::runtime_error("gene-policy equivalence_gene " +
                                         fact.equivalence_gene + " for " + count_gene +
                                         " is not self-canonical");
            }
            if (canonical->second.competition != fact.competition) {
                throw std::runtime_error("equivalent genes " + count_gene + " and " +
                                         fact.equivalence_gene +
                                         " have conflicting competition classes");
            }
            // A nested-host identifier may name an annotation gene outside the
            // countable policy universe.  The frozen Python resolver retains that
            // edge as provenance but it is inert unless the host is also retained
            // for the read; do not manufacture a policy row or reject the bundle.
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
    const auto ensure_unknown_policy_gene = [&result](const std::string& count_gene) {
        if (result.genes_.contains(count_gene)) {
            return;
        }
        result.genes_.emplace(count_gene,
                              GeneFact{count_gene, {}, std::nullopt});
        for (auto& [profile_id, profile_facts] : result.profile_genes_) {
            ProfileGeneFact missing;
            missing.profile_id = profile_id;
            missing.count_gene = count_gene;
            missing.competition = CompetitionClass::unknown;
            missing.competition_reason = "missing_from_ledger";
            missing.equivalence_gene = count_gene;
            missing.source_annotation = "missing_from_ledger";
            profile_facts.emplace(count_gene, std::move(missing));
        }
    };
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
        std::string strong_support_gene = fact.count_gene;
        if (const auto parsed = ensembl_identity(identity.source_gene);
            parsed && !parsed->discriminator.empty()) {
            fact.novel_paralog = true;
            fact.novel_origin_gene = parsed->stable_gene;
            strong_support_gene = fact.novel_origin_gene;
            // Frozen profiles look up a lumped copy through its origin. A
            // sensitivity profile that separates the copy sees the same
            // missing-ledger UNKNOWN row that count_cr.py constructs.
            ensure_unknown_policy_gene(fact.novel_origin_gene);
            ensure_unknown_policy_gene(fact.count_gene);
        } else {
            ensure_unknown_policy_gene(fact.count_gene);
        }
        if (support->second.count_gene != strong_support_gene) {
            throw std::runtime_error("strong-support count_gene disagrees for Parent " + parent);
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

AssignmentCandidate CountFactCatalog::candidate(const EffectiveProfile& effective,
                                                std::string_view unique_parent,
                                                std::int64_t score,
                                                EvidenceTier tier,
                                                EvidenceStrand strand,
                                                std::uint32_t body_sample_support) const {
    const ParentFact& parent_fact = parent(unique_parent);
    const ProfileId profile_id = profile_policy_source(effective);

    // count_cr applies novel-paralog identity policy before consulting the gene-policy
    // ledger. Mirror that order here so a derived lump/separate profile reads policy
    // from the same final identity as the Python oracle.
    std::string resolved_gene = parent_fact.count_gene;
    bool force_self_equivalence = false;
    if (parent_fact.novel_paralog) {
        switch (effective.profile.assignment.novel_paralog) {
            case NovelParalogPolicy::lump:
                resolved_gene = parent_fact.novel_origin_gene;
                force_self_equivalence = true;
                break;
            case NovelParalogPolicy::separate:
                force_self_equivalence = true;
                break;
            case NovelParalogPolicy::ignore:
                break;
        }
    }

    const ProfileGeneFact& resolved_policy = profile_gene(profile_id, resolved_gene);
    const std::string& equivalence_gene = force_self_equivalence
                                              ? resolved_gene
                                              : resolved_policy.equivalence_gene;
    // The frozen oracle performs competition, nesting, and body-type tests on the
    // canonical equivalence row, even when only an alias occurs on this read.
    const ProfileGeneFact& canonical_policy = profile_gene(profile_id, equivalence_gene);
    const GeneFact& canonical_gene = gene(equivalence_gene);
    AssignmentCandidate result;
    result.gene = resolved_gene;
    result.equivalence_gene = equivalence_gene;
    result.nested_host = canonical_policy.nested_host;
    result.score = score;
    result.tier = tier;
    result.strand = strand;
    result.competition = canonical_policy.competition;
    result.categories = parent_fact.raw_categories;
    result.gene_types = canonical_gene.gene_types;
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
