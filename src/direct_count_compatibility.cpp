#include "direct_count_compatibility.hpp"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/util/config.h>
#include <openssl/evp.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/parquet_version.h>
#include <parquet/properties.h>
#include <simdjson.h>
#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <system_error>
#include <unistd.h>

namespace pancollapse::direct_count {
namespace {

static_assert(ARROW_VERSION_MAJOR == 17 && ARROW_VERSION_MINOR == 0 &&
              ARROW_VERSION_PATCH == 0);
static_assert(PARQUET_VERSION_MAJOR == 17 && PARQUET_VERSION_MINOR == 0 &&
              PARQUET_VERSION_PATCH == 0);

constexpr std::string_view kSchema = "pancollapse-read-compatibility-v1";
constexpr std::string_view kCreatedBy = "panCollapse-read-compatibility-v1";
constexpr std::array<std::string_view, 4> kTablePaths{
    "parquet/exact_facts.parquet",
    "parquet/fact_sets.parquet",
    "parquet/read_rows.parquet",
    "parquet/structural_facts.parquet",
};
constexpr std::array<char, 8> kSpoolMagic{'P', 'C', 'C', 'S', 'P', '0', '0', '1'};
constexpr std::array<char, 8> kSpoolFooter{'P', 'C', 'C', 'E', 'N', 'D', '0', '1'};
constexpr size_t kSpoolBufferBytes = 1 << 20;
constexpr std::uint64_t kMaximumSpoolStringBytes = 1ULL << 30;

[[noreturn]] void fail(std::string_view message) {
    throw std::runtime_error(std::string(message));
}

void check_arrow(const arrow::Status& status, std::string_view operation) {
    if (!status.ok()) {
        throw std::runtime_error(std::string(operation) + ": " + status.ToString());
    }
}

template<class T>
T arrow_value(arrow::Result<T> result, std::string_view operation) {
    if (!result.ok()) {
        throw std::runtime_error(std::string(operation) + ": " +
                                 result.status().ToString());
    }
    return std::move(*result);
}

std::string hex_digest(const unsigned char* digest, size_t length) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string output;
    output.reserve(2 * length);
    for (size_t index = 0; index < length; ++index) {
        output.push_back(digits[digest[index] >> 4U]);
        output.push_back(digits[digest[index] & 0xFU]);
    }
    return output;
}

class Sha256Digest {
  public:
    Sha256Digest() : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
        if (!context_ || EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1) {
            fail("cannot initialize compatibility SHA-256");
        }
    }

    explicit Sha256Digest(std::string_view domain) : Sha256Digest() {
        append_string(domain);
    }

    void update(const void* data, size_t size) {
        if (finished_) {
            fail("cannot update a finalized compatibility SHA-256");
        }
        if (size != 0 && EVP_DigestUpdate(context_.get(), data, size) != 1) {
            fail("cannot update compatibility SHA-256");
        }
    }

    void append_u8(std::uint8_t value) { update(&value, sizeof(value)); }

    void append_u64(std::uint64_t value) {
        std::array<unsigned char, 8> bytes{};
        for (size_t index = 0; index < bytes.size(); ++index) {
            bytes[index] = static_cast<unsigned char>(value >> (8U * index));
        }
        update(bytes.data(), bytes.size());
    }

    void append_i64(std::int64_t value) {
        append_u64(static_cast<std::uint64_t>(value));
    }

    void append_string(std::string_view value) {
        append_u64(value.size());
        update(value.data(), value.size());
    }

    void append_optional(const std::optional<std::string>& value) {
        append_u8(value.has_value() ? 1 : 0);
        if (value) {
            append_string(*value);
        }
    }

    std::string finish() {
        if (finished_) {
            fail("compatibility SHA-256 was finalized twice");
        }
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int length = 0;
        if (EVP_DigestFinal_ex(context_.get(), digest.data(), &length) != 1 ||
            length != 32) {
            fail("cannot finalize compatibility SHA-256");
        }
        finished_ = true;
        return hex_digest(digest.data(), length);
    }

  private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_;
    bool finished_ = false;
};

void check_zstd(size_t value, std::string_view operation) {
    if (ZSTD_isError(value)) {
        throw std::runtime_error(std::string(operation) + ": " +
                                 ZSTD_getErrorName(value));
    }
}

class ZstdSpoolOutput {
  public:
    explicit ZstdSpoolOutput(const std::filesystem::path& path)
        : output_(path, std::ios::binary | std::ios::trunc),
          context_(ZSTD_createCCtx()) {
        if (!output_) fail("cannot open compatibility read spool");
        if (context_ == nullptr) fail("cannot allocate compatibility spool compressor");
        check_zstd(ZSTD_CCtx_setParameter(context_, ZSTD_c_compressionLevel, 3),
                   "cannot set compatibility spool ZSTD level");
        check_zstd(ZSTD_CCtx_setParameter(context_, ZSTD_c_checksumFlag, 1),
                   "cannot enable compatibility spool ZSTD checksum");
    }

    ZstdSpoolOutput(const ZstdSpoolOutput&) = delete;
    ZstdSpoolOutput& operator=(const ZstdSpoolOutput&) = delete;

    ~ZstdSpoolOutput() {
        if (context_ != nullptr) ZSTD_freeCCtx(context_);
    }

    void write(const void* data, size_t size) {
        if (finished_) fail("cannot append to a finished compatibility spool");
        ZSTD_inBuffer input{data, size, 0};
        while (input.pos < input.size) {
            ZSTD_outBuffer output{buffer_.data(), buffer_.size(), 0};
            check_zstd(ZSTD_compressStream2(context_, &output, &input,
                                            ZSTD_e_continue),
                       "cannot compress compatibility read spool");
            output_.write(buffer_.data(), static_cast<std::streamsize>(output.pos));
            if (!output_) fail("cannot write compatibility read spool");
        }
    }

    void write(std::string_view value) { write(value.data(), value.size()); }

    void finish() {
        if (finished_) fail("compatibility read spool was finalized twice");
        size_t remaining = 1;
        while (remaining != 0) {
            ZSTD_inBuffer input{nullptr, 0, 0};
            ZSTD_outBuffer output{buffer_.data(), buffer_.size(), 0};
            remaining = ZSTD_compressStream2(context_, &output, &input, ZSTD_e_end);
            check_zstd(remaining, "cannot finalize compatibility read spool");
            output_.write(buffer_.data(), static_cast<std::streamsize>(output.pos));
            if (!output_) fail("cannot write compatibility read spool trailer");
        }
        output_.close();
        if (!output_) fail("cannot close compatibility read spool");
        finished_ = true;
    }

  private:
    std::ofstream output_;
    ZSTD_CCtx* context_ = nullptr;
    std::array<char, kSpoolBufferBytes> buffer_{};
    bool finished_ = false;
};

class ZstdSpoolInput {
  public:
    explicit ZstdSpoolInput(const std::filesystem::path& path)
        : input_(path, std::ios::binary), context_(ZSTD_createDCtx()) {
        if (!input_) fail("cannot open compatibility read spool");
        if (context_ == nullptr) fail("cannot allocate compatibility spool decompressor");
    }

    ZstdSpoolInput(const ZstdSpoolInput&) = delete;
    ZstdSpoolInput& operator=(const ZstdSpoolInput&) = delete;

    ~ZstdSpoolInput() {
        if (context_ != nullptr) ZSTD_freeDCtx(context_);
    }

    void read_exact(void* destination, size_t size) {
        char* target = static_cast<char*>(destination);
        size_t copied = 0;
        while (copied < size) {
            if (decoded_at_ == decoded_size_) refill();
            if (decoded_at_ == decoded_size_) {
                fail("compatibility read spool ended before its footer");
            }
            const size_t available = decoded_size_ - decoded_at_;
            const size_t amount = std::min(available, size - copied);
            std::copy_n(decoded_.data() + decoded_at_, amount, target + copied);
            decoded_at_ += amount;
            copied += amount;
        }
    }

    void finish() {
        if (finished_) return;
        if (decoded_at_ != decoded_size_) {
            fail("compatibility read spool has trailing logical bytes");
        }
        while (!frame_complete_) {
            if (!load_encoded()) {
                fail("compatibility read spool is truncated before its ZSTD checksum");
            }
            ZSTD_inBuffer input{encoded_.data(), encoded_size_, encoded_at_};
            ZSTD_outBuffer output{decoded_.data(), decoded_.size(), 0};
            const size_t remaining = ZSTD_decompressStream(context_, &output, &input);
            check_zstd(remaining, "cannot decode compatibility read spool trailer");
            encoded_at_ = input.pos;
            if (output.pos != 0) {
                fail("compatibility read spool has trailing logical bytes");
            }
            frame_complete_ = remaining == 0;
        }
        if (encoded_at_ != encoded_size_) {
            fail("compatibility read spool has trailing bytes or multiple ZSTD frames");
        }
        char trailing = 0;
        input_.read(&trailing, 1);
        if (input_.gcount() != 0) {
            fail("compatibility read spool has trailing bytes or multiple ZSTD frames");
        }
        if (input_.bad()) fail("cannot finish reading compatibility read spool");
        finished_ = true;
    }

  private:
    bool load_encoded() {
        if (encoded_at_ != encoded_size_) return true;
        input_.read(encoded_.data(), static_cast<std::streamsize>(encoded_.size()));
        encoded_size_ = static_cast<size_t>(input_.gcount());
        encoded_at_ = 0;
        if (input_.bad()) fail("cannot read compatibility read spool");
        return encoded_size_ != 0;
    }

    void refill() {
        decoded_at_ = 0;
        decoded_size_ = 0;
        while (decoded_size_ == 0 && !frame_complete_) {
            if (!load_encoded()) return;
            ZSTD_inBuffer input{encoded_.data(), encoded_size_, encoded_at_};
            ZSTD_outBuffer output{decoded_.data(), decoded_.size(), 0};
            const size_t remaining = ZSTD_decompressStream(context_, &output, &input);
            check_zstd(remaining, "cannot decode compatibility read spool");
            encoded_at_ = input.pos;
            decoded_size_ = output.pos;
            frame_complete_ = remaining == 0;
        }
    }

    std::ifstream input_;
    ZSTD_DCtx* context_ = nullptr;
    std::array<char, kSpoolBufferBytes> encoded_{};
    std::array<char, kSpoolBufferBytes> decoded_{};
    size_t encoded_at_ = 0;
    size_t encoded_size_ = 0;
    size_t decoded_at_ = 0;
    size_t decoded_size_ = 0;
    bool frame_complete_ = false;
    bool finished_ = false;
};

std::string digest_bytes(std::string_view bytes) {
    Sha256Digest digest;
    digest.update(bytes.data(), bytes.size());
    return digest.finish();
}

std::string sha256_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("cannot open compatibility file for SHA-256: " +
                                 path.string());
    }
    Sha256Digest digest;
    std::array<char, 1 << 20> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            digest.update(buffer.data(), static_cast<size_t>(count));
        }
    }
    if (!input.eof()) {
        throw std::runtime_error("failed while hashing compatibility file: " +
                                 path.string());
    }
    return digest.finish();
}

void append_u8(std::string& output, std::uint8_t value) {
    output.push_back(static_cast<char>(value));
}

void append_u64(std::string& output, std::uint64_t value) {
    for (size_t index = 0; index < 8; ++index) {
        output.push_back(static_cast<char>(value >> (8U * index)));
    }
}

void append_text(std::string& output, std::string_view value) {
    append_u64(output, value.size());
    output.append(value);
}

void append_optional_text(std::string& output,
                          const std::optional<std::string>& value) {
    append_u8(output, value ? 1 : 0);
    if (value) append_text(output, *value);
}

std::string encode_spool_read(const ReadCompatibilityRow& read) {
    std::string output;
    output.reserve(read.original_name.size() +
                   (read.raw_barcode ? read.raw_barcode->size() : 0) +
                   (read.barcode_quality ? read.barcode_quality->size() : 0) +
                   (read.umi ? read.umi->size() : 0) +
                   (read.umi_quality ? read.umi_quality->size() : 0) + 64);
    append_u64(output, read.ordinal);
    append_text(output, read.original_name);
    append_u8(output, static_cast<std::uint8_t>(read.molecule_status));
    append_optional_text(output, read.raw_barcode);
    append_optional_text(output, read.barcode_quality);
    append_optional_text(output, read.umi);
    append_optional_text(output, read.umi_quality);
    append_u64(output, read.fact_set_id);
    return output;
}

std::uint8_t read_u8(ZstdSpoolInput& input) {
    std::uint8_t value = 0;
    input.read_exact(&value, sizeof(value));
    return value;
}

std::uint64_t read_u64(ZstdSpoolInput& input) {
    std::array<unsigned char, 8> bytes{};
    input.read_exact(bytes.data(), bytes.size());
    std::uint64_t value = 0;
    for (size_t index = 0; index < bytes.size(); ++index) {
        value |= static_cast<std::uint64_t>(bytes[index]) << (8U * index);
    }
    return value;
}

std::string read_text(ZstdSpoolInput& input) {
    const std::uint64_t size = read_u64(input);
    if (size > kMaximumSpoolStringBytes ||
        size > static_cast<std::uint64_t>(std::numeric_limits<size_t>::max())) {
        fail("compatibility read spool string is too large");
    }
    std::string value(static_cast<size_t>(size), '\0');
    input.read_exact(value.data(), value.size());
    return value;
}

std::optional<std::string> read_optional_text(ZstdSpoolInput& input) {
    const std::uint8_t marker = read_u8(input);
    if (marker == 0) return std::nullopt;
    if (marker != 1) fail("compatibility read spool optional marker is invalid");
    return read_text(input);
}

ReadCompatibilityRow decode_spool_read(ZstdSpoolInput& input,
                                       Sha256Digest& spool_digest) {
    ReadCompatibilityRow result;
    result.ordinal = read_u64(input);
    result.original_name = read_text(input);
    const std::uint8_t status = read_u8(input);
    if (status > static_cast<std::uint8_t>(MoleculeStatus::unsupported)) {
        fail("compatibility read spool molecule status is invalid");
    }
    result.molecule_status = static_cast<MoleculeStatus>(status);
    result.raw_barcode = read_optional_text(input);
    result.barcode_quality = read_optional_text(input);
    result.umi = read_optional_text(input);
    result.umi_quality = read_optional_text(input);
    result.fact_set_id = read_u64(input);
    const std::string canonical = encode_spool_read(result);
    spool_digest.update(canonical.data(), canonical.size());
    return result;
}

bool is_sha256(std::string_view value) {
    return value.size() == 64 &&
           std::all_of(value.begin(), value.end(), [](char character) {
               return (character >= '0' && character <= '9') ||
                      (character >= 'a' && character <= 'f');
           });
}

bool is_content_id(std::string_view value) {
    return value.starts_with("sha256:") && is_sha256(value.substr(7));
}

void append_unicode_escape(std::string& output, std::uint32_t unit) {
    static constexpr char digits[] = "0123456789abcdef";
    output += "\\u";
    output.push_back(digits[(unit >> 12U) & 0xFU]);
    output.push_back(digits[(unit >> 8U) & 0xFU]);
    output.push_back(digits[(unit >> 4U) & 0xFU]);
    output.push_back(digits[unit & 0xFU]);
}

void append_canonical_string(std::string& output, std::string_view value) {
    output.push_back('"');
    size_t index = 0;
    while (index < value.size()) {
        const unsigned char byte = static_cast<unsigned char>(value[index]);
        if (byte < 0x80U) {
            switch (byte) {
                case '"': output += "\\\""; break;
                case '\\': output += "\\\\"; break;
                case '\n': output += "\\n"; break;
                case '\r': output += "\\r"; break;
                case '\t': output += "\\t"; break;
                case '\b': output += "\\b"; break;
                case '\f': output += "\\f"; break;
                default:
                    if (byte >= 0x20U && byte < 0x7FU) {
                        output.push_back(static_cast<char>(byte));
                    } else {
                        append_unicode_escape(output, byte);
                    }
            }
            ++index;
            continue;
        }
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
            append_unicode_escape(output, 0xD800U + (code_point >> 10U));
            append_unicode_escape(output, 0xDC00U + (code_point & 0x3FFU));
        } else {
            append_unicode_escape(output, code_point);
        }
    }
    output.push_back('"');
}

void append_canonical_json(std::string& output,
                           const simdjson::dom::element& element) {
    switch (element.type()) {
        case simdjson::dom::element_type::ARRAY: {
            output.push_back('[');
            bool first = true;
            const simdjson::dom::array array = element.get_array().value();
            for (const simdjson::dom::element child : array) {
                if (!first) output.push_back(',');
                first = false;
                append_canonical_json(output, child);
            }
            output.push_back(']');
            return;
        }
        case simdjson::dom::element_type::OBJECT: {
            std::vector<std::pair<std::string_view, simdjson::dom::element>> members;
            const simdjson::dom::object object = element.get_object().value();
            for (const auto field : object) {
                members.emplace_back(field.key, field.value);
            }
            std::sort(members.begin(), members.end(),
                      [](const auto& left, const auto& right) {
                          return left.first < right.first;
                      });
            output.push_back('{');
            bool first = true;
            for (const auto& [key, value] : members) {
                if (!first) output.push_back(',');
                first = false;
                append_canonical_string(output, key);
                output.push_back(':');
                append_canonical_json(output, value);
            }
            output.push_back('}');
            return;
        }
        case simdjson::dom::element_type::INT64:
            output += std::to_string(element.get_int64().value());
            return;
        case simdjson::dom::element_type::UINT64:
            output += std::to_string(element.get_uint64().value());
            return;
        case simdjson::dom::element_type::STRING:
            append_canonical_string(output, element.get_string().value());
            return;
        case simdjson::dom::element_type::BOOL:
            output += element.get_bool().value() ? "true" : "false";
            return;
        case simdjson::dom::element_type::NULL_VALUE:
            output += "null";
            return;
        case simdjson::dom::element_type::DOUBLE:
        case simdjson::dom::element_type::BIGINT:
            fail("compatibility manifest contains an unsupported numeric value");
    }
    fail("compatibility manifest contains an unsupported JSON value");
}

std::string canonical_json(const simdjson::dom::element& element) {
    std::string output;
    append_canonical_json(output, element);
    return output;
}

std::string molecule_status_name(MoleculeStatus value) {
    switch (value) {
        case MoleculeStatus::valid: return "valid";
        case MoleculeStatus::missing: return "missing";
        case MoleculeStatus::malformed: return "malformed";
        case MoleculeStatus::unsupported: return "unsupported";
    }
    fail("invalid molecule status");
}

MoleculeStatus parse_molecule_status(std::string_view value) {
    if (value == "valid") return MoleculeStatus::valid;
    if (value == "missing") return MoleculeStatus::missing;
    if (value == "malformed") return MoleculeStatus::malformed;
    if (value == "unsupported") return MoleculeStatus::unsupported;
    fail("invalid compatibility molecule status");
}

void increment_status(MoleculeStatusCounts& counts, MoleculeStatus status) {
    std::uint64_t* value = nullptr;
    switch (status) {
        case MoleculeStatus::valid: value = &counts.valid; break;
        case MoleculeStatus::missing: value = &counts.missing; break;
        case MoleculeStatus::malformed: value = &counts.malformed; break;
        case MoleculeStatus::unsupported: value = &counts.unsupported; break;
    }
    if (value == nullptr) fail("invalid compatibility molecule status");
    if (*value == std::numeric_limits<std::uint64_t>::max()) {
        fail("compatibility molecule-status count overflow");
    }
    ++*value;
}

std::uint64_t status_total(const MoleculeStatusCounts& counts) {
    std::uint64_t total = 0;
    for (const std::uint64_t value :
         {counts.valid, counts.missing, counts.malformed, counts.unsupported}) {
        if (value > std::numeric_limits<std::uint64_t>::max() - total) {
            fail("compatibility molecule-status counts overflow");
        }
        total += value;
    }
    return total;
}

std::string tier_name(EvidenceTier value) {
    switch (value) {
        case EvidenceTier::exon: return "E";
        case EvidenceTier::partial_exon: return "P";
        case EvidenceTier::body: return "B";
        case EvidenceTier::gene:
            fail("gene tier is not exact compatibility evidence");
    }
    fail("invalid exact compatibility tier");
}

EvidenceTier parse_tier(std::string_view value) {
    if (value == "E") return EvidenceTier::exon;
    if (value == "P") return EvidenceTier::partial_exon;
    if (value == "B") return EvidenceTier::body;
    fail("invalid exact compatibility tier");
}

std::string strand_name(EvidenceStrand value) {
    switch (value) {
        case EvidenceStrand::forward: return "F";
        case EvidenceStrand::reverse: return "R";
    }
    fail("invalid compatibility evidence strand");
}

EvidenceStrand parse_strand(std::string_view value) {
    if (value == "F") return EvidenceStrand::forward;
    if (value == "R") return EvidenceStrand::reverse;
    fail("invalid compatibility evidence strand");
}

std::string layer_name(StructuralLayer value) {
    switch (value) {
        case StructuralLayer::spliced: return "S";
        case StructuralLayer::unspliced: return "U";
    }
    fail("invalid structural compatibility layer");
}

StructuralLayer parse_layer(std::string_view value) {
    if (value == "S") return StructuralLayer::spliced;
    if (value == "U") return StructuralLayer::unspliced;
    fail("invalid structural compatibility layer");
}

template<class T>
void sort_unique(std::vector<T>& values) {
    std::sort(values.begin(), values.end());
    values.erase(std::unique(values.begin(), values.end()), values.end());
}

void canonicalize(CompatibilityFactSet& facts) {
    for (StructuralCompatibilityFact& fact : facts.structural) {
        sort_unique(fact.winning_paths);
        sort_unique(fact.winning_parents);
    }
    sort_unique(facts.exact);
    sort_unique(facts.structural);
}

void validate_exact_fact(const ExactCompatibilityFact& fact) {
    if (fact.canonical_transcript.empty() || fact.locus_parent.empty() ||
        fact.path.empty() || fact.unique_parent.empty()) {
        fail("exact compatibility provenance fields must not be empty");
    }
    static_cast<void>(tier_name(fact.tier));
    static_cast<void>(strand_name(fact.strand));
}

void validate_structural_fact(const StructuralCompatibilityFact& fact) {
    if (fact.canonical_transcript.empty() || fact.winning_paths.empty() ||
        fact.winning_parents.empty()) {
        fail("structural compatibility provenance must not be empty");
    }
    if (std::any_of(fact.winning_paths.begin(), fact.winning_paths.end(),
                    [](const std::string& value) { return value.empty(); }) ||
        std::any_of(fact.winning_parents.begin(), fact.winning_parents.end(),
                    [](const std::string& value) { return value.empty(); })) {
        fail("structural compatibility provenance contains an empty identity");
    }
    static_cast<void>(layer_name(fact.layer));
    static_cast<void>(strand_name(fact.strand));
}

void validate_fact_set(const CompatibilityFactSet& facts) {
    for (const ExactCompatibilityFact& fact : facts.exact) validate_exact_fact(fact);
    for (const StructuralCompatibilityFact& fact : facts.structural) {
        validate_structural_fact(fact);
    }
}

void validate_read(const ReadCompatibilityRow& read) {
    if (read.original_name.empty()) fail("compatibility read name must not be empty");
    auto empty = [](const std::optional<std::string>& value) {
        return value && value->empty();
    };
    if (empty(read.raw_barcode) || empty(read.barcode_quality) || empty(read.umi) ||
        empty(read.umi_quality)) {
        fail("compatibility molecule fields must not contain empty strings");
    }
    if (read.molecule_status == MoleculeStatus::valid) {
        if (!read.raw_barcode || !read.umi) {
            fail("valid compatibility reads require raw barcode and UMI");
        }
    } else if (read.raw_barcode || read.barcode_quality || read.umi || read.umi_quality) {
        fail("invalid compatibility reads must not carry parsed molecule fields");
    }
}

void validate_inputs(std::vector<CompatibilityInputIdentity>& inputs) {
    std::sort(inputs.begin(), inputs.end(), [](const auto& left, const auto& right) {
        return left.role < right.role;
    });
    bool have_gamp = false;
    bool have_xg = false;
    std::string previous;
    for (const CompatibilityInputIdentity& input : inputs) {
        if (input.role.empty() || !is_sha256(input.sha256)) {
            fail("compatibility input identity is malformed");
        }
        if (!previous.empty() && input.role == previous) {
            fail("compatibility input roles must be unique");
        }
        previous = input.role;
        have_gamp = have_gamp || input.role == "gamp";
        have_xg = have_xg || input.role == "xg";
    }
    if (!have_gamp || !have_xg) {
        fail("compatibility provenance requires gamp and xg input identities");
    }
}

void append_logical_read(Sha256Digest& digest,
                         const ReadCompatibilityRow& read) {
    digest.append_u64(read.ordinal);
    digest.append_string(read.original_name);
    digest.append_u8(static_cast<std::uint8_t>(read.molecule_status));
    digest.append_optional(read.raw_barcode);
    digest.append_optional(read.barcode_quality);
    digest.append_optional(read.umi);
    digest.append_optional(read.umi_quality);
    digest.append_u64(read.fact_set_id);
}

std::map<std::string, std::string> logical_hashes(const CompatibilityBundle& bundle) {
    std::map<std::string, std::string> result;
    {
        Sha256Digest digest("parquet/read_rows.parquet");
        for (const ReadCompatibilityRow& read : bundle.reads) {
            append_logical_read(digest, read);
        }
        result.emplace("parquet/read_rows.parquet", digest.finish());
    }
    {
        Sha256Digest digest("parquet/fact_sets.parquet");
        for (size_t index = 0; index < bundle.fact_sets.size(); ++index) {
            digest.append_u64(index);
            digest.append_u8(bundle.fact_sets[index].complete_provenance ? 1 : 0);
        }
        result.emplace("parquet/fact_sets.parquet", digest.finish());
    }
    {
        Sha256Digest digest("parquet/exact_facts.parquet");
        for (size_t index = 0; index < bundle.fact_sets.size(); ++index) {
            for (const ExactCompatibilityFact& fact : bundle.fact_sets[index].exact) {
                digest.append_u64(index);
                digest.append_string(fact.canonical_transcript);
                digest.append_string(fact.locus_parent);
                digest.append_string(fact.path);
                digest.append_string(fact.unique_parent);
                digest.append_i64(fact.score);
                digest.append_u8(static_cast<std::uint8_t>(fact.tier));
                digest.append_u8(static_cast<std::uint8_t>(fact.strand));
            }
        }
        result.emplace("parquet/exact_facts.parquet", digest.finish());
    }
    {
        Sha256Digest digest("parquet/structural_facts.parquet");
        for (size_t index = 0; index < bundle.fact_sets.size(); ++index) {
            for (const StructuralCompatibilityFact& fact : bundle.fact_sets[index].structural) {
                digest.append_u64(index);
                digest.append_string(fact.canonical_transcript);
                digest.append_u8(static_cast<std::uint8_t>(fact.layer));
                digest.append_i64(fact.score);
                digest.append_u8(static_cast<std::uint8_t>(fact.strand));
                digest.append_u64(fact.winning_paths.size());
                for (const std::string& path : fact.winning_paths) digest.append_string(path);
                digest.append_u64(fact.winning_parents.size());
                for (const std::string& parent : fact.winning_parents) {
                    digest.append_string(parent);
                }
            }
        }
        result.emplace("parquet/structural_facts.parquet", digest.finish());
    }
    return result;
}

std::shared_ptr<arrow::Schema> read_schema() {
    return arrow::schema({
        arrow::field("ordinal", arrow::uint64(), false),
        arrow::field("original_name", arrow::utf8(), false),
        arrow::field("molecule_status", arrow::utf8(), false),
        arrow::field("raw_barcode", arrow::utf8(), true),
        arrow::field("barcode_quality", arrow::utf8(), true),
        arrow::field("umi", arrow::utf8(), true),
        arrow::field("umi_quality", arrow::utf8(), true),
        arrow::field("fact_set_id", arrow::uint64(), false),
    });
}

std::shared_ptr<arrow::Schema> fact_set_schema() {
    return arrow::schema({
        arrow::field("fact_set_id", arrow::uint64(), false),
        arrow::field("complete_provenance", arrow::boolean(), false),
    });
}

std::shared_ptr<arrow::Schema> exact_schema() {
    return arrow::schema({
        arrow::field("fact_set_id", arrow::uint64(), false),
        arrow::field("canonical_transcript", arrow::utf8(), false),
        arrow::field("locus_parent", arrow::utf8(), false),
        arrow::field("path", arrow::utf8(), false),
        arrow::field("unique_parent", arrow::utf8(), false),
        arrow::field("score", arrow::int64(), false),
        arrow::field("tier", arrow::utf8(), false),
        arrow::field("strand", arrow::utf8(), false),
    });
}

std::shared_ptr<arrow::Schema> structural_schema() {
    return arrow::schema({
        arrow::field("fact_set_id", arrow::uint64(), false),
        arrow::field("canonical_transcript", arrow::utf8(), false),
        arrow::field("layer", arrow::utf8(), false),
        arrow::field("score", arrow::int64(), false),
        arrow::field("strand", arrow::utf8(), false),
        arrow::field("winning_paths", arrow::list(arrow::utf8()), false),
        arrow::field("winning_parents", arrow::list(arrow::utf8()), false),
    });
}

template<class Builder, class Value>
void append_value(Builder& builder, const Value& value, std::string_view operation) {
    check_arrow(builder.Append(value), operation);
}

void append_optional(arrow::StringBuilder& builder,
                     const std::optional<std::string>& value,
                     std::string_view operation) {
    check_arrow(value ? builder.Append(*value) : builder.AppendNull(), operation);
}

std::shared_ptr<arrow::Array> finish(arrow::ArrayBuilder& builder,
                                     std::string_view operation) {
    std::shared_ptr<arrow::Array> output;
    check_arrow(builder.Finish(&output), operation);
    return output;
}

CompatibilityTableIdentity write_table(
    const std::filesystem::path& path, std::string relative_path,
    const std::shared_ptr<arrow::Table>& table, std::uint64_t row_group_rows,
    std::string logical_sha256) {
    if (row_group_rows == 0 ||
        row_group_rows > kMaximumCompatibilityRowsPerBatch) {
        fail("compatibility Parquet row-group size is invalid");
    }
    auto output = arrow_value(arrow::io::FileOutputStream::Open(path.string()),
                              "cannot open compatibility Parquet output");
    parquet::WriterProperties::Builder properties;
    properties.version(parquet::ParquetVersion::PARQUET_2_6)
        ->compression(parquet::Compression::ZSTD)
        ->compression_level(3)
        ->created_by(std::string(kCreatedBy));
    parquet::ArrowWriterProperties::Builder arrow_properties;
    arrow_properties.store_schema();
    auto writer = arrow_value(
        parquet::arrow::FileWriter::Open(*table->schema(), arrow::default_memory_pool(), output,
                                         properties.build(), arrow_properties.build()),
        "cannot create compatibility Parquet writer");
    check_arrow(writer->WriteTable(*table, static_cast<std::int64_t>(row_group_rows)),
                "cannot write compatibility Parquet table");
    check_arrow(writer->Close(), "cannot close compatibility Parquet writer");
    check_arrow(output->Close(), "cannot close compatibility Parquet output");
    return {std::move(relative_path), static_cast<std::uint64_t>(table->num_rows()),
            std::filesystem::file_size(path), sha256_file(path),
            std::move(logical_sha256)};
}

class CompatibilityParquetBatchWriter {
  public:
    CompatibilityParquetBatchWriter(const std::filesystem::path& path,
                                    std::string relative_path,
                                    std::shared_ptr<arrow::Schema> schema)
        : path_(path), relative_path_(std::move(relative_path)),
          schema_(std::move(schema)) {
        output_ = arrow_value(arrow::io::FileOutputStream::Open(path_.string()),
                              "cannot open compatibility Parquet output");
        parquet::WriterProperties::Builder properties;
        properties.version(parquet::ParquetVersion::PARQUET_2_6)
            ->compression(parquet::Compression::ZSTD)
            ->compression_level(3)
            ->created_by(std::string(kCreatedBy));
        parquet::ArrowWriterProperties::Builder arrow_properties;
        arrow_properties.store_schema();
        writer_ = arrow_value(
            parquet::arrow::FileWriter::Open(
                *schema_, arrow::default_memory_pool(), output_, properties.build(),
                arrow_properties.build()),
            "cannot create compatibility Parquet batch writer");
    }

    void write(const std::shared_ptr<arrow::RecordBatch>& batch) {
        if (!batch || batch->num_rows() <= 0 ||
            !batch->schema()->Equals(*schema_)) {
            fail("compatibility Parquet batch is empty or has the wrong schema");
        }
        check_arrow(writer_->NewRowGroup(batch->num_rows()),
                    "cannot start compatibility Parquet row group");
        for (int column = 0; column < batch->num_columns(); ++column) {
            check_arrow(writer_->WriteColumnChunk(*batch->column(column)),
                        "cannot write compatibility Parquet column chunk");
        }
        rows_ += static_cast<std::uint64_t>(batch->num_rows());
    }

    CompatibilityTableIdentity finish(std::string logical_sha256) {
        check_arrow(writer_->Close(), "cannot close compatibility Parquet batch writer");
        writer_.reset();
        check_arrow(output_->Close(), "cannot close compatibility Parquet output");
        output_.reset();
        return {std::move(relative_path_), rows_, std::filesystem::file_size(path_),
                sha256_file(path_), std::move(logical_sha256)};
    }

  private:
    std::filesystem::path path_;
    std::string relative_path_;
    std::shared_ptr<arrow::Schema> schema_;
    std::shared_ptr<arrow::io::FileOutputStream> output_;
    std::unique_ptr<parquet::arrow::FileWriter> writer_;
    std::uint64_t rows_ = 0;
};

struct ExactBatchRow {
    std::uint64_t fact_set_id = 0;
    const ExactCompatibilityFact* fact = nullptr;
};

std::shared_ptr<arrow::RecordBatch> make_exact_batch(
    const std::vector<ExactBatchRow>& rows) {
    arrow::UInt64Builder id_builder;
    arrow::StringBuilder transcript_builder;
    arrow::StringBuilder locus_parent_builder;
    arrow::StringBuilder path_builder;
    arrow::StringBuilder parent_builder;
    arrow::Int64Builder score_builder;
    arrow::StringBuilder tier_builder;
    arrow::StringBuilder strand_builder;
    for (const ExactBatchRow& row : rows) {
        if (row.fact == nullptr) fail("exact compatibility batch contains a null fact");
        const ExactCompatibilityFact& fact = *row.fact;
        append_value(id_builder, row.fact_set_id, "append exact fact-set ID");
        append_value(transcript_builder, fact.canonical_transcript,
                     "append exact transcript");
        append_value(locus_parent_builder, fact.locus_parent,
                     "append exact locus Parent");
        append_value(path_builder, fact.path, "append exact path");
        append_value(parent_builder, fact.unique_parent,
                     "append exact unique Parent");
        append_value(score_builder, fact.score, "append exact score");
        append_value(tier_builder, tier_name(fact.tier), "append exact tier");
        append_value(strand_builder, strand_name(fact.strand),
                     "append exact strand");
    }
    return arrow::RecordBatch::Make(
        exact_schema(), static_cast<std::int64_t>(rows.size()),
        {finish(id_builder, "finish exact fact-set IDs"),
         finish(transcript_builder, "finish exact transcripts"),
         finish(locus_parent_builder, "finish exact locus Parents"),
         finish(path_builder, "finish exact paths"),
         finish(parent_builder, "finish exact unique Parents"),
         finish(score_builder, "finish exact scores"),
         finish(tier_builder, "finish exact tiers"),
         finish(strand_builder, "finish exact strands")});
}

struct StructuralBatchRow {
    std::uint64_t fact_set_id = 0;
    const StructuralCompatibilityFact* fact = nullptr;
};

std::shared_ptr<arrow::RecordBatch> make_structural_batch(
    const std::vector<StructuralBatchRow>& rows) {
    arrow::UInt64Builder id_builder;
    arrow::StringBuilder transcript_builder;
    arrow::StringBuilder layer_builder;
    arrow::Int64Builder score_builder;
    arrow::StringBuilder strand_builder;
    arrow::ListBuilder paths_builder(
        arrow::default_memory_pool(), std::make_shared<arrow::StringBuilder>());
    arrow::ListBuilder parents_builder(
        arrow::default_memory_pool(), std::make_shared<arrow::StringBuilder>());
    auto* path_value_builder =
        static_cast<arrow::StringBuilder*>(paths_builder.value_builder());
    auto* parent_value_builder =
        static_cast<arrow::StringBuilder*>(parents_builder.value_builder());
    for (const StructuralBatchRow& row : rows) {
        if (row.fact == nullptr) {
            fail("structural compatibility batch contains a null fact");
        }
        const StructuralCompatibilityFact& fact = *row.fact;
        append_value(id_builder, row.fact_set_id,
                     "append structural fact-set ID");
        append_value(transcript_builder, fact.canonical_transcript,
                     "append structural transcript");
        append_value(layer_builder, layer_name(fact.layer),
                     "append structural layer");
        append_value(score_builder, fact.score, "append structural score");
        append_value(strand_builder, strand_name(fact.strand),
                     "append structural strand");
        check_arrow(paths_builder.Append(), "append structural path list");
        for (const std::string& value : fact.winning_paths) {
            append_value(*path_value_builder, value, "append structural path");
        }
        check_arrow(parents_builder.Append(), "append structural Parent list");
        for (const std::string& value : fact.winning_parents) {
            append_value(*parent_value_builder, value, "append structural Parent");
        }
    }
    return arrow::RecordBatch::Make(
        structural_schema(), static_cast<std::int64_t>(rows.size()),
        {finish(id_builder, "finish structural fact-set IDs"),
         finish(transcript_builder, "finish structural transcripts"),
         finish(layer_builder, "finish structural layers"),
         finish(score_builder, "finish structural scores"),
         finish(strand_builder, "finish structural strands"),
         finish(paths_builder, "finish structural path lists"),
         finish(parents_builder, "finish structural Parent lists")});
}

void add_string_bytes(std::uint64_t& total, size_t bytes) {
    if (bytes > std::numeric_limits<std::uint64_t>::max() - total) {
        fail("compatibility fact string-byte count overflows uint64");
    }
    total += static_cast<std::uint64_t>(bytes);
}

std::uint64_t exact_string_bytes(const ExactCompatibilityFact& fact) {
    std::uint64_t result = 0;
    add_string_bytes(result, fact.canonical_transcript.size());
    add_string_bytes(result, fact.locus_parent.size());
    add_string_bytes(result, fact.path.size());
    add_string_bytes(result, fact.unique_parent.size());
    add_string_bytes(result, tier_name(fact.tier).size());
    add_string_bytes(result, strand_name(fact.strand).size());
    return result;
}

std::uint64_t structural_string_bytes(const StructuralCompatibilityFact& fact) {
    std::uint64_t result = 0;
    add_string_bytes(result, fact.canonical_transcript.size());
    add_string_bytes(result, layer_name(fact.layer).size());
    add_string_bytes(result, strand_name(fact.strand).size());
    for (const std::string& value : fact.winning_paths) {
        add_string_bytes(result, value.size());
    }
    for (const std::string& value : fact.winning_parents) {
        add_string_bytes(result, value.size());
    }
    return result;
}

std::uint64_t read_string_bytes(const ReadCompatibilityRow& read) {
    std::uint64_t result = 0;
    add_string_bytes(result, read.original_name.size());
    add_string_bytes(result, molecule_status_name(read.molecule_status).size());
    for (const auto* value : {std::addressof(read.raw_barcode),
                              std::addressof(read.barcode_quality),
                              std::addressof(read.umi),
                              std::addressof(read.umi_quality)}) {
        if (*value) add_string_bytes(result, (*value)->size());
    }
    return result;
}

std::shared_ptr<arrow::RecordBatch> make_read_batch(
    const std::vector<ReadCompatibilityRow>& rows) {
    arrow::UInt64Builder ordinal_builder;
    arrow::StringBuilder name_builder;
    arrow::StringBuilder status_builder;
    arrow::StringBuilder barcode_builder;
    arrow::StringBuilder barcode_quality_builder;
    arrow::StringBuilder umi_builder;
    arrow::StringBuilder umi_quality_builder;
    arrow::UInt64Builder fact_set_builder;
    for (const ReadCompatibilityRow& read : rows) {
        append_value(ordinal_builder, read.ordinal,
                     "append compatibility ordinal");
        append_value(name_builder, read.original_name,
                     "append compatibility name");
        append_value(status_builder, molecule_status_name(read.molecule_status),
                     "append compatibility molecule status");
        append_optional(barcode_builder, read.raw_barcode,
                        "append compatibility barcode");
        append_optional(barcode_quality_builder, read.barcode_quality,
                        "append compatibility barcode quality");
        append_optional(umi_builder, read.umi, "append compatibility UMI");
        append_optional(umi_quality_builder, read.umi_quality,
                        "append compatibility UMI quality");
        append_value(fact_set_builder, read.fact_set_id,
                     "append compatibility fact-set ID");
    }
    return arrow::RecordBatch::Make(
        read_schema(), static_cast<std::int64_t>(rows.size()),
        {finish(ordinal_builder, "finish compatibility ordinals"),
         finish(name_builder, "finish compatibility names"),
         finish(status_builder, "finish compatibility statuses"),
         finish(barcode_builder, "finish compatibility barcodes"),
         finish(barcode_quality_builder, "finish compatibility barcode qualities"),
         finish(umi_builder, "finish compatibility UMIs"),
         finish(umi_quality_builder, "finish compatibility UMI qualities"),
         finish(fact_set_builder, "finish compatibility fact-set IDs")});
}

CompatibilityTableIdentity write_spooled_read_table(
    const std::filesystem::path& spool_path,
    const std::filesystem::path& parquet_path,
    std::uint64_t row_count,
    std::uint64_t row_group_rows,
    std::uint64_t max_string_bytes_per_batch,
    const std::vector<std::uint64_t>& provisional_to_canonical) {
    CompatibilityParquetBatchWriter writer(
        parquet_path, "parquet/read_rows.parquet", read_schema());

    ZstdSpoolInput input(spool_path);
    std::array<char, kSpoolMagic.size()> magic{};
    input.read_exact(magic.data(), magic.size());
    if (magic != kSpoolMagic) fail("compatibility read spool magic is invalid");
    Sha256Digest spool_digest("compatibility-read-spool-v1");
    Sha256Digest logical_digest("parquet/read_rows.parquet");
    std::vector<ReadCompatibilityRow> batch;
    batch.reserve(static_cast<size_t>(std::min<std::uint64_t>(
        row_group_rows, std::numeric_limits<size_t>::max())));
    std::uint64_t batch_string_bytes = 0;
    auto flush = [&]() {
        if (batch.empty()) return;
        writer.write(make_read_batch(batch));
        batch.clear();
        batch_string_bytes = 0;
    };
    for (std::uint64_t ordinal = 0; ordinal < row_count; ++ordinal) {
        ReadCompatibilityRow read = decode_spool_read(input, spool_digest);
        if (read.ordinal != ordinal ||
            read.fact_set_id >= provisional_to_canonical.size()) {
            fail("compatibility read spool ordinal/fact-set integrity failure");
        }
        validate_read(read);
        read.fact_set_id = provisional_to_canonical[read.fact_set_id];
        append_logical_read(logical_digest, read);
        const std::uint64_t row_string_bytes = read_string_bytes(read);
        if (row_string_bytes > max_string_bytes_per_batch) {
            fail("one compatibility read row exceeds the Parquet batch string budget");
        }
        if (!batch.empty() &&
            (batch.size() == row_group_rows ||
             row_string_bytes >
                 max_string_bytes_per_batch - batch_string_bytes)) {
            flush();
        }
        batch.push_back(std::move(read));
        batch_string_bytes += row_string_bytes;
    }
    flush();

    std::array<char, kSpoolFooter.size()> footer{};
    input.read_exact(footer.data(), footer.size());
    if (footer != kSpoolFooter || read_u64(input) != row_count) {
        fail("compatibility read spool footer/count is invalid");
    }
    const std::string expected_spool_digest = read_text(input);
    if (!is_sha256(expected_spool_digest) ||
        spool_digest.finish() != expected_spool_digest) {
        fail("compatibility read spool logical checksum mismatch");
    }
    input.finish();

    CompatibilityTableIdentity result = writer.finish(logical_digest.finish());
    if (result.rows != row_count) {
        fail("compatibility read Parquet writer dropped an input row");
    }
    return result;
}

std::shared_ptr<arrow::Table> read_table(
    const std::filesystem::path& path,
    const std::shared_ptr<arrow::Schema>& expected_schema) {
    auto input = arrow_value(arrow::io::ReadableFile::Open(path.string()),
                             "cannot open compatibility Parquet input");
    std::unique_ptr<parquet::arrow::FileReader> reader;
    check_arrow(parquet::arrow::OpenFile(input, arrow::default_memory_pool(), &reader),
                "cannot initialize compatibility Parquet reader");
    std::shared_ptr<arrow::Table> table;
    check_arrow(reader->ReadTable(&table), "cannot read compatibility Parquet table");
    if (!table->schema()->Equals(*expected_schema)) {
        throw std::runtime_error("unexpected compatibility Parquet schema: " + path.string());
    }
    return table;
}

void for_each_fact_table_batch(
    const std::filesystem::path& path,
    const std::shared_ptr<arrow::Schema>& expected_schema,
    std::uint64_t expected_rows,
    const std::function<void(const std::shared_ptr<arrow::RecordBatch>&)>& callback) {
    auto input = arrow_value(arrow::io::ReadableFile::Open(path.string()),
                             "cannot open compatibility Parquet input");
    std::unique_ptr<parquet::arrow::FileReader> reader;
    check_arrow(parquet::arrow::OpenFile(input, arrow::default_memory_pool(), &reader),
                "cannot initialize compatibility Parquet batch reader");
    std::shared_ptr<arrow::Schema> schema;
    check_arrow(reader->GetSchema(&schema),
                "cannot read compatibility Parquet batch schema");
    if (!schema->Equals(*expected_schema)) {
        throw std::runtime_error("unexpected compatibility Parquet schema: " +
                                 path.string());
    }
    const auto metadata = reader->parquet_reader()->metadata();
    if (metadata->num_rows() < 0 ||
        static_cast<std::uint64_t>(metadata->num_rows()) != expected_rows) {
        throw std::runtime_error("compatibility table row-count mismatch: " +
                                 path.string());
    }
    std::uint64_t rows = 0;
    for (int row_group = 0; row_group < metadata->num_row_groups(); ++row_group) {
        std::unique_ptr<arrow::RecordBatchReader> batches;
        check_arrow(reader->GetRecordBatchReader({row_group}, &batches),
                    "cannot initialize compatibility row-group reader");
        while (true) {
            std::shared_ptr<arrow::RecordBatch> batch;
            check_arrow(batches->ReadNext(&batch),
                        "cannot read compatibility fact-table batch");
            if (!batch) break;
            if (!batch->schema()->Equals(*expected_schema) || batch->num_rows() <= 0) {
                fail("compatibility fact-table batch is empty or has the wrong schema");
            }
            rows += static_cast<std::uint64_t>(batch->num_rows());
            callback(batch);
        }
    }
    if (rows != expected_rows) {
        throw std::runtime_error("compatibility fact-table batch count mismatch: " +
                                 path.string());
    }
}

std::shared_ptr<arrow::Array> combine_column(const std::shared_ptr<arrow::Table>& table,
                                             int index) {
    const auto& chunks = table->column(index)->chunks();
    if (chunks.empty()) {
        return arrow_value(arrow::MakeArrayOfNull(table->field(index)->type(), 0),
                           "cannot create empty compatibility column");
    }
    return arrow_value(arrow::Concatenate(chunks, arrow::default_memory_pool()),
                       "cannot combine compatibility Parquet chunks");
}

std::string string_value(const std::shared_ptr<arrow::StringArray>& array,
                         std::int64_t index) {
    if (array->IsNull(index)) fail("non-null compatibility string is null");
    return array->GetString(index);
}

std::optional<std::string> optional_string(
    const std::shared_ptr<arrow::StringArray>& array, std::int64_t index) {
    return array->IsNull(index) ? std::nullopt
                                : std::optional<std::string>(array->GetString(index));
}

bool path_is_within(const std::filesystem::path& root,
                    const std::filesystem::path& candidate) {
    auto root_part = root.begin();
    auto candidate_part = candidate.begin();
    for (; root_part != root.end(); ++root_part, ++candidate_part) {
        if (candidate_part == candidate.end() || *root_part != *candidate_part) return false;
    }
    return true;
}

void validate_relative_path(const std::filesystem::path& path) {
    if (path.empty() || path.is_absolute() || path.lexically_normal() != path) {
        fail("compatibility manifest contains a non-canonical relative path");
    }
    for (const auto& component : path) {
        if (component == "." || component == "..") {
            fail("compatibility manifest table path escapes the bundle");
        }
    }
}

void require_keys(simdjson::dom::object object,
                  std::initializer_list<std::string_view> expected,
                  std::string_view context) {
    std::vector<std::string> actual;
    for (const auto field : object) actual.emplace_back(field.key);
    std::vector<std::string> wanted;
    for (std::string_view key : expected) wanted.emplace_back(key);
    std::sort(actual.begin(), actual.end());
    std::sort(wanted.begin(), wanted.end());
    if (actual != wanted) {
        throw std::runtime_error("compatibility manifest " + std::string(context) +
                                 " has missing, duplicate, or unexpected fields");
    }
}

std::string json_string(simdjson::dom::element object, std::string_view field) {
    try {
        return std::string(static_cast<std::string_view>(object[field]));
    } catch (const simdjson::simdjson_error&) {
        throw std::runtime_error("compatibility manifest field " + std::string(field) +
                                 " must be a string");
    }
}

std::uint64_t json_uint(simdjson::dom::element object, std::string_view field) {
    try {
        return static_cast<std::uint64_t>(object[field]);
    } catch (const simdjson::simdjson_error&) {
        throw std::runtime_error("compatibility manifest field " + std::string(field) +
                                 " must be uint64");
    }
}

struct ParsedManifest {
    std::string producer_version;
    std::string compatibility_algorithm_id;
    std::string structural_surface_id;
    std::string content_id;
    std::uint64_t input_records = 0;
    std::uint64_t input_read_groups = 0;
    MoleculeStatusCounts molecule_status_counts;
    std::vector<CompatibilityInputIdentity> inputs;
    std::map<std::string, CompatibilityTableIdentity> tables;
};

ParsedManifest read_manifest(const std::filesystem::path& manifest_path) {
    try {
        simdjson::dom::parser parser;
        const simdjson::dom::element document = parser.load(manifest_path.string());
        require_keys(document.get_object().value(), {"content", "content_id", "schema"},
                     "root");
        if (json_string(document, "schema") != kSchema) {
            fail("unsupported compatibility manifest schema");
        }
        ParsedManifest result;
        result.content_id = json_string(document, "content_id");
        if (!is_content_id(result.content_id)) {
            fail("compatibility manifest content_id is malformed");
        }
        const simdjson::dom::element content = document["content"];
        require_keys(content.get_object().value(),
                     {"compatibility_algorithm_id", "input_read_groups",
                      "input_records", "inputs", "molecule_status_counts",
                      "producer_version", "structural_surface_id", "tables"},
                     "content");
        if (result.content_id != "sha256:" + digest_bytes(canonical_json(content))) {
            fail("compatibility manifest content_id does not match content");
        }
        result.producer_version = json_string(content, "producer_version");
        result.compatibility_algorithm_id =
            json_string(content, "compatibility_algorithm_id");
        result.structural_surface_id = json_string(content, "structural_surface_id");
        result.input_read_groups = json_uint(content, "input_read_groups");
        result.input_records = json_uint(content, "input_records");
        const simdjson::dom::element status_counts =
            content["molecule_status_counts"];
        require_keys(status_counts.get_object().value(),
                     {"malformed", "missing", "unsupported", "valid"},
                     "molecule status counts");
        result.molecule_status_counts = {
            json_uint(status_counts, "valid"),
            json_uint(status_counts, "missing"),
            json_uint(status_counts, "malformed"),
            json_uint(status_counts, "unsupported")};
        if (status_total(result.molecule_status_counts) !=
            result.input_read_groups) {
            fail("compatibility manifest molecule-status denominator is inconsistent");
        }
        if (result.producer_version.empty() || result.compatibility_algorithm_id.empty() ||
            !is_content_id(result.structural_surface_id)) {
            fail("compatibility manifest contract identity is malformed");
        }
        const simdjson::dom::array inputs = content["inputs"].get_array().value();
        for (const simdjson::dom::element row : inputs) {
            require_keys(row.get_object().value(), {"role", "sha256", "size_bytes"},
                         "input row");
            result.inputs.push_back({json_string(row, "role"),
                                     json_string(row, "sha256"),
                                     json_uint(row, "size_bytes")});
        }
        const auto inputs_before_sort = result.inputs;
        validate_inputs(result.inputs);
        if (result.inputs != inputs_before_sort) {
            fail("compatibility manifest input identities are not canonically sorted");
        }
        std::string previous_path;
        const simdjson::dom::array tables = content["tables"].get_array().value();
        for (const simdjson::dom::element row : tables) {
            require_keys(row.get_object().value(),
                         {"logical_sha256", "path", "rows", "sha256", "size_bytes"},
                         "table row");
            CompatibilityTableIdentity table;
            table.logical_sha256 = json_string(row, "logical_sha256");
            table.relative_path = json_string(row, "path");
            table.rows = json_uint(row, "rows");
            table.sha256 = json_string(row, "sha256");
            table.size_bytes = json_uint(row, "size_bytes");
            validate_relative_path(table.relative_path);
            if (!is_sha256(table.sha256) || !is_sha256(table.logical_sha256)) {
                fail("compatibility manifest table digest is malformed");
            }
            if (!previous_path.empty() && table.relative_path <= previous_path) {
                fail("compatibility manifest table rows are not canonically sorted");
            }
            previous_path = table.relative_path;
            if (!result.tables.emplace(table.relative_path, std::move(table)).second) {
                fail("compatibility manifest repeats a table path");
            }
        }
        std::set<std::string> actual;
        for (const auto& [path, table] : result.tables) {
            static_cast<void>(table);
            actual.insert(path);
        }
        const std::set<std::string> expected(kTablePaths.begin(), kTablePaths.end());
        if (actual != expected) {
            fail("compatibility manifest table set is incomplete or unexpected");
        }
        return result;
    } catch (const simdjson::simdjson_error& error) {
        throw std::runtime_error("cannot parse compatibility manifest: " +
                                 std::string(error.what()));
    }
}

std::string build_manifest_content(
    const CompatibilityBundle& bundle,
    const std::vector<CompatibilityTableIdentity>& tables) {
    std::string output = "{\"compatibility_algorithm_id\":";
    append_canonical_string(output, bundle.compatibility_algorithm_id);
    output += ",\"input_read_groups\":" +
              std::to_string(bundle.input_read_groups);
    output += ",\"input_records\":" + std::to_string(bundle.input_records);
    output += ",\"inputs\":[";
    for (size_t index = 0; index < bundle.inputs.size(); ++index) {
        if (index != 0) output.push_back(',');
        const CompatibilityInputIdentity& input = bundle.inputs[index];
        output += "{\"role\":";
        append_canonical_string(output, input.role);
        output += ",\"sha256\":";
        append_canonical_string(output, input.sha256);
        output += ",\"size_bytes\":" + std::to_string(input.size_bytes) + "}";
    }
    output += "],\"molecule_status_counts\":{\"malformed\":" +
              std::to_string(bundle.molecule_status_counts.malformed) +
              ",\"missing\":" +
              std::to_string(bundle.molecule_status_counts.missing) +
              ",\"unsupported\":" +
              std::to_string(bundle.molecule_status_counts.unsupported) +
              ",\"valid\":" +
              std::to_string(bundle.molecule_status_counts.valid) + "}";
    output += ",\"producer_version\":";
    append_canonical_string(output, bundle.producer_version);
    output += ",\"structural_surface_id\":";
    append_canonical_string(output, bundle.structural_surface_id);
    output += ",\"tables\":[";
    for (size_t index = 0; index < tables.size(); ++index) {
        if (index != 0) output.push_back(',');
        const CompatibilityTableIdentity& table = tables[index];
        output += "{\"logical_sha256\":";
        append_canonical_string(output, table.logical_sha256);
        output += ",\"path\":";
        append_canonical_string(output, table.relative_path);
        output += ",\"rows\":" + std::to_string(table.rows) + ",\"sha256\":";
        append_canonical_string(output, table.sha256);
        output += ",\"size_bytes\":" + std::to_string(table.size_bytes) + "}";
    }
    output += "]}";
    return output;
}

void write_fact_tables(const std::filesystem::path& staging,
                       const CompatibilityBundle& bundle,
                       std::uint64_t row_group_rows,
                       std::uint64_t max_string_bytes_per_batch,
                       const std::map<std::string, std::string>& logical,
                       CompatibilityWriteReceipt& receipt) {
    {
        arrow::UInt64Builder id_builder;
        arrow::BooleanBuilder complete_builder;
        for (size_t index = 0; index < bundle.fact_sets.size(); ++index) {
            append_value(id_builder, static_cast<std::uint64_t>(index),
                         "append compatibility fact-set ID");
            append_value(complete_builder,
                         bundle.fact_sets[index].complete_provenance,
                         "append compatibility provenance state");
        }
        const std::string path = "parquet/fact_sets.parquet";
        receipt.tables.push_back(write_table(
            staging / path, path,
            arrow::Table::Make(
                fact_set_schema(),
                {finish(id_builder, "finish compatibility fact-set IDs"),
                 finish(complete_builder,
                        "finish compatibility provenance states")}),
            row_group_rows, logical.at(path)));
    }
    {
        const std::string path = "parquet/exact_facts.parquet";
        CompatibilityParquetBatchWriter writer(staging / path, path,
                                               exact_schema());
        std::vector<ExactBatchRow> batch;
        batch.reserve(static_cast<size_t>(row_group_rows));
        std::uint64_t batch_string_bytes = 0;
        auto flush = [&]() {
            if (batch.empty()) return;
            writer.write(make_exact_batch(batch));
            batch.clear();
            batch_string_bytes = 0;
        };
        for (size_t index = 0; index < bundle.fact_sets.size(); ++index) {
            for (const ExactCompatibilityFact& fact : bundle.fact_sets[index].exact) {
                const std::uint64_t row_string_bytes = exact_string_bytes(fact);
                if (row_string_bytes > max_string_bytes_per_batch) {
                    fail("one exact compatibility fact exceeds the Parquet batch string budget");
                }
                if (!batch.empty() &&
                    (batch.size() == row_group_rows ||
                     row_string_bytes >
                         max_string_bytes_per_batch - batch_string_bytes)) {
                    flush();
                }
                batch.push_back(
                    {static_cast<std::uint64_t>(index), std::addressof(fact)});
                batch_string_bytes += row_string_bytes;
            }
        }
        flush();
        receipt.tables.push_back(writer.finish(logical.at(path)));
    }
    {
        const std::string path = "parquet/structural_facts.parquet";
        CompatibilityParquetBatchWriter writer(staging / path, path,
                                               structural_schema());
        std::vector<StructuralBatchRow> batch;
        batch.reserve(static_cast<size_t>(row_group_rows));
        std::uint64_t batch_string_bytes = 0;
        auto flush = [&]() {
            if (batch.empty()) return;
            writer.write(make_structural_batch(batch));
            batch.clear();
            batch_string_bytes = 0;
        };
        for (size_t index = 0; index < bundle.fact_sets.size(); ++index) {
            for (const StructuralCompatibilityFact& fact :
                 bundle.fact_sets[index].structural) {
                const std::uint64_t row_string_bytes =
                    structural_string_bytes(fact);
                if (row_string_bytes > max_string_bytes_per_batch) {
                    fail("one structural compatibility fact exceeds the Parquet batch string budget");
                }
                if (!batch.empty() &&
                    (batch.size() == row_group_rows ||
                     row_string_bytes >
                         max_string_bytes_per_batch - batch_string_bytes)) {
                    flush();
                }
                batch.push_back(
                    {static_cast<std::uint64_t>(index), std::addressof(fact)});
                batch_string_bytes += row_string_bytes;
            }
        }
        flush();
        receipt.tables.push_back(writer.finish(logical.at(path)));
    }
}

}  // namespace

std::string compatibility_structural_surface_id(
    const path_identity::PathIdentityLedger& path_ledger) {
    Sha256Digest digest("pancollapse-compatibility-structural-surface-v1");
    digest.append_u64(path_ledger.rows_by_path.size());
    for (const auto& [path_name, row] : path_ledger.rows_by_path) {
        const auto& annotation = row.annotation;
        digest.append_string(path_name);
        digest.append_u64(row.vg_path_length);
        digest.append_string(annotation.unique_parent);
        digest.append_string(annotation.canonical_transcript);
        digest.append_string(annotation.feature_layer);
        digest.append_string(annotation.exon_unique_parent);
        digest.append_string(annotation.source_path_or_contig);
        digest.append_string(annotation.strand);
        digest.append_string(annotation.start);
        digest.append_string(annotation.end);
    }
    return "sha256:" + digest.finish();
}

struct CompatibilityBundleWriter::Impl {
    CompatibilityBundleIdentity identity;
    CompatibilityWriteOptions options;
    std::filesystem::path output;
    std::filesystem::path staging;
    std::filesystem::path spool_path;
    std::unique_ptr<ZstdSpoolOutput> spool;
    Sha256Digest spool_digest{"compatibility-read-spool-v1"};
    std::map<CompatibilityFactSet, std::uint64_t> fact_ids;
    std::uint64_t next_fact_id = 0;
    std::uint64_t rows = 0;
    MoleculeStatusCounts molecule_status_counts;
    bool input_closed = false;
    bool published = false;

    Impl(CompatibilityBundleIdentity identity_value,
         CompatibilityWriteOptions options_value)
        : identity(std::move(identity_value)), options(std::move(options_value)) {
        if (options.output_directory.empty() || options.parquet_row_group_rows == 0 ||
            options.parquet_row_group_rows > kMaximumCompatibilityRowsPerBatch ||
            options.parquet_max_string_bytes_per_batch == 0 ||
            options.parquet_max_string_bytes_per_batch >
                kMaximumCompatibilityStringBytesPerBatch) {
            throw std::invalid_argument("compatibility output options are incomplete");
        }
        if (identity.producer_version.empty() ||
            identity.compatibility_algorithm_id.empty() ||
            !is_content_id(identity.structural_surface_id)) {
            throw std::invalid_argument(
                "compatibility producer/contract identity is incomplete");
        }
        if (!identity.inputs.empty()) validate_inputs(identity.inputs);
        output = std::filesystem::absolute(options.output_directory).lexically_normal();
        if (std::filesystem::exists(output)) {
            throw std::invalid_argument("compatibility destination already exists");
        }
        std::filesystem::path parent = output.parent_path();
        if (parent.empty()) parent = ".";
        std::filesystem::create_directories(parent);
        staging = parent /
                  ("." + output.filename().string() + ".compatibility-staging-" +
                   std::to_string(static_cast<unsigned long long>(getpid())));
        if (std::filesystem::exists(staging)) {
            fail("compatibility staging path already exists");
        }
        try {
            std::filesystem::create_directories(staging / "parquet");
            spool_path = staging / "read_rows.spool.zst";
            spool = std::make_unique<ZstdSpoolOutput>(spool_path);
            spool->write(kSpoolMagic.data(), kSpoolMagic.size());
        } catch (...) {
            std::error_code ignored;
            std::filesystem::remove_all(staging, ignored);
            throw;
        }
    }

    ~Impl() {
        if (!published && !staging.empty()) {
            spool.reset();
            std::error_code ignored;
            std::filesystem::remove_all(staging, ignored);
        }
    }
};

CompatibilityBundleWriter::CompatibilityBundleWriter(
    CompatibilityBundleIdentity identity, CompatibilityWriteOptions options)
    : impl_(std::make_unique<Impl>(std::move(identity), std::move(options))) {}

CompatibilityBundleWriter::CompatibilityBundleWriter(
    CompatibilityBundleWriter&&) noexcept = default;
CompatibilityBundleWriter& CompatibilityBundleWriter::operator=(
    CompatibilityBundleWriter&&) noexcept = default;
CompatibilityBundleWriter::~CompatibilityBundleWriter() = default;

void CompatibilityBundleWriter::set_inputs(
    std::vector<CompatibilityInputIdentity> inputs,
    std::uint64_t input_records,
    std::uint64_t input_read_groups) {
    if (!impl_ || impl_->input_closed) {
        throw std::logic_error("cannot set inputs on a closed compatibility writer");
    }
    if (!impl_->identity.inputs.empty()) {
        throw std::logic_error("compatibility writer inputs were already set");
    }
    validate_inputs(inputs);
    impl_->identity.inputs = std::move(inputs);
    impl_->identity.input_records = input_records;
    impl_->identity.input_read_groups = input_read_groups;
}

void CompatibilityBundleWriter::append(ReadCompatibilityRow read,
                                       CompatibilityFactSet facts) {
    if (!impl_ || impl_->input_closed) {
        throw std::logic_error("cannot append to a closed compatibility writer");
    }
    if (read.ordinal != impl_->rows) {
        throw std::invalid_argument(
            "compatibility reads must be appended in contiguous input order");
    }
    if (impl_->rows == std::numeric_limits<std::uint64_t>::max()) {
        fail("compatibility read-row count overflow");
    }
    validate_read(read);
    canonicalize(facts);
    validate_fact_set(facts);
    auto [found, inserted] = impl_->fact_ids.emplace(std::move(facts), 0);
    if (inserted) {
        if (impl_->next_fact_id == std::numeric_limits<std::uint64_t>::max()) {
            fail("compatibility fact-set count overflow");
        }
        found->second = impl_->next_fact_id;
        ++impl_->next_fact_id;
    }
    read.fact_set_id = found->second;
    const std::string encoded = encode_spool_read(read);
    impl_->spool_digest.update(encoded.data(), encoded.size());
    impl_->spool->write(encoded);
    increment_status(impl_->molecule_status_counts, read.molecule_status);
    ++impl_->rows;
}

std::uint64_t CompatibilityBundleWriter::rows_appended() const {
    if (!impl_) throw std::logic_error("compatibility writer was moved from");
    return impl_->rows;
}

CompatibilityWriteReceipt CompatibilityBundleWriter::finalize() {
    if (!impl_ || impl_->input_closed) {
        throw std::logic_error("compatibility writer was already closed");
    }
    impl_->input_closed = true;
    if (impl_->identity.inputs.empty()) {
        throw std::logic_error(
            "compatibility writer inputs were not set before finalize");
    }
    if (impl_->identity.input_read_groups != impl_->rows ||
        status_total(impl_->molecule_status_counts) != impl_->rows ||
        impl_->identity.input_records < impl_->identity.input_read_groups) {
        throw std::runtime_error(
            "compatibility writer input record/read-group denominator is inconsistent");
    }
    std::string footer(kSpoolFooter.begin(), kSpoolFooter.end());
    append_u64(footer, impl_->rows);
    append_text(footer, impl_->spool_digest.finish());
    impl_->spool->write(footer);
    impl_->spool->finish();
    impl_->spool.reset();

    CompatibilityBundle canonical;
    canonical.inputs = impl_->identity.inputs;
    canonical.producer_version = impl_->identity.producer_version;
    canonical.compatibility_algorithm_id =
        impl_->identity.compatibility_algorithm_id;
    canonical.structural_surface_id = impl_->identity.structural_surface_id;
    canonical.input_records = impl_->identity.input_records;
    canonical.input_read_groups = impl_->identity.input_read_groups;
    canonical.molecule_status_counts = impl_->molecule_status_counts;
    canonical.fact_sets.reserve(impl_->fact_ids.size());
    std::vector<std::uint64_t> remap(impl_->fact_ids.size());
    for (const auto& [facts, provisional_id] : impl_->fact_ids) {
        if (provisional_id >= remap.size()) {
            fail("compatibility provisional fact-set ID is invalid");
        }
        remap[provisional_id] = canonical.fact_sets.size();
        canonical.fact_sets.push_back(facts);
    }
    const std::map<std::string, std::string> logical = logical_hashes(canonical);

    CompatibilityWriteReceipt receipt;
    try {
        receipt.tables.push_back(write_spooled_read_table(
            impl_->spool_path, impl_->staging / "parquet/read_rows.parquet",
            impl_->rows, impl_->options.parquet_row_group_rows,
            impl_->options.parquet_max_string_bytes_per_batch, remap));
        write_fact_tables(
            impl_->staging, canonical, impl_->options.parquet_row_group_rows,
            impl_->options.parquet_max_string_bytes_per_batch, logical, receipt);
        if (!std::filesystem::remove(impl_->spool_path)) {
            fail("compatibility read spool disappeared before staged publication");
        }

        std::sort(receipt.tables.begin(), receipt.tables.end(),
                  [](const auto& left, const auto& right) {
                      return left.relative_path < right.relative_path;
                  });
        const std::string content = build_manifest_content(canonical, receipt.tables);
        receipt.content_id = "sha256:" + digest_bytes(content);
        std::string manifest = "{\"content\":" + content + ",\"content_id\":";
        append_canonical_string(manifest, receipt.content_id);
        manifest += ",\"schema\":";
        append_canonical_string(manifest, kSchema);
        manifest += "}\n";
        {
            std::ofstream manifest_output(impl_->staging / "manifest.json",
                                          std::ios::binary | std::ios::trunc);
            if (!manifest_output) fail("cannot open compatibility manifest output");
            manifest_output << manifest;
            manifest_output.close();
            if (!manifest_output) fail("cannot close compatibility manifest output");
        }
        receipt.manifest_sha256 = sha256_file(impl_->staging / "manifest.json");

        CompatibilityBundleReader staged(impl_->staging);
        staged.for_each_read_batch([](std::span<const ReadCompatibilityRow>) {});
        if (staged.index().content_id != receipt.content_id ||
            staged.index().read_rows != impl_->rows) {
            fail("staged compatibility identity changed during validation");
        }
        if (std::filesystem::exists(impl_->output)) {
            fail("compatibility destination appeared before atomic publish");
        }
        std::filesystem::rename(impl_->staging, impl_->output);
        impl_->published = true;
        return receipt;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(impl_->staging, ignored);
        throw;
    }
}

CompatibilityWriteReceipt write_compatibility_bundle(
    CompatibilityBundle bundle, const CompatibilityWriteOptions& options) {
    if (!bundle.content_id.empty()) {
        throw std::invalid_argument("compatibility writer derives content_id");
    }
    std::sort(bundle.reads.begin(), bundle.reads.end(),
              [](const auto& left, const auto& right) {
                  return left.ordinal < right.ordinal;
              });
    for (size_t index = 0; index < bundle.reads.size(); ++index) {
        if (bundle.reads[index].ordinal != index ||
            bundle.reads[index].fact_set_id >= bundle.fact_sets.size()) {
            throw std::invalid_argument(
                "compatibility read ordinals or fact-set IDs are invalid");
        }
    }
    CompatibilityBundleWriter writer(
        {std::move(bundle.inputs), std::move(bundle.producer_version),
         std::move(bundle.compatibility_algorithm_id),
         std::move(bundle.structural_surface_id), bundle.input_records,
         bundle.input_read_groups},
        options);
    for (ReadCompatibilityRow& read : bundle.reads) {
        const std::uint64_t fact_set_id = read.fact_set_id;
        writer.append(std::move(read), bundle.fact_sets[fact_set_id]);
    }
    return writer.finalize();
}

struct CompatibilityBundleReader::Impl {
    std::filesystem::path root;
    ParsedManifest manifest;
    CompatibilityBundleIndex index;
};

CompatibilityBundleReader::CompatibilityBundleReader(
    const std::filesystem::path& requested_directory)
    : impl_(std::make_unique<Impl>()) {
    impl_->root = std::filesystem::canonical(requested_directory);
    if (!std::filesystem::is_directory(impl_->root)) {
        fail("compatibility input must be a directory");
    }
    const std::filesystem::path manifest_path = impl_->root / "manifest.json";
    if (!std::filesystem::is_regular_file(manifest_path)) {
        fail("compatibility input lacks manifest.json");
    }
    impl_->manifest = read_manifest(manifest_path);
    for (const auto& [relative_name, table] : impl_->manifest.tables) {
        const std::filesystem::path relative(relative_name);
        validate_relative_path(relative);
        const std::filesystem::path resolved =
            std::filesystem::canonical(impl_->root / relative);
        if (!path_is_within(impl_->root, resolved) ||
            !std::filesystem::is_regular_file(resolved)) {
            fail("compatibility table escapes the bundle or is not a regular file");
        }
        if (std::filesystem::file_size(resolved) != table.size_bytes ||
            sha256_file(resolved) != table.sha256) {
            throw std::runtime_error("compatibility table hash/size mismatch: " +
                                     relative_name);
        }
    }

    const auto fact_sets =
        read_table(impl_->root / "parquet/fact_sets.parquet", fact_set_schema());
    auto validate_rows = [&](const std::shared_ptr<arrow::Table>& table,
                             std::string_view path) {
        if (table->num_rows() < 0 ||
            static_cast<std::uint64_t>(table->num_rows()) !=
                impl_->manifest.tables.at(std::string(path)).rows) {
            throw std::runtime_error("compatibility table row-count mismatch: " +
                                     std::string(path));
        }
    };
    validate_rows(fact_sets, "parquet/fact_sets.parquet");
    if (static_cast<std::uint64_t>(fact_sets->num_rows()) >
        static_cast<std::uint64_t>(std::numeric_limits<size_t>::max())) {
        fail("compatibility fact-set dictionary is too large");
    }

    impl_->index.inputs = impl_->manifest.inputs;
    impl_->index.producer_version = impl_->manifest.producer_version;
    impl_->index.compatibility_algorithm_id =
        impl_->manifest.compatibility_algorithm_id;
    impl_->index.structural_surface_id = impl_->manifest.structural_surface_id;
    impl_->index.content_id = impl_->manifest.content_id;
    impl_->index.input_records = impl_->manifest.input_records;
    impl_->index.input_read_groups = impl_->manifest.input_read_groups;
    impl_->index.molecule_status_counts =
        impl_->manifest.molecule_status_counts;
    impl_->index.read_rows =
        impl_->manifest.tables.at("parquet/read_rows.parquet").rows;
    if (impl_->index.read_rows != impl_->index.input_read_groups) {
        fail("compatibility read table does not match the input-group denominator");
    }
    if (impl_->index.input_records < impl_->index.input_read_groups) {
        fail("compatibility input record/read-group denominator is inconsistent");
    }

    const auto set_ids = std::static_pointer_cast<arrow::UInt64Array>(
        combine_column(fact_sets, 0));
    const auto complete = std::static_pointer_cast<arrow::BooleanArray>(
        combine_column(fact_sets, 1));
    impl_->index.fact_sets.resize(static_cast<size_t>(fact_sets->num_rows()));
    for (std::int64_t row = 0; row < fact_sets->num_rows(); ++row) {
        if (set_ids->IsNull(row) || complete->IsNull(row) ||
            set_ids->Value(row) != static_cast<std::uint64_t>(row)) {
            fail("compatibility fact-set IDs are not contiguous");
        }
        impl_->index.fact_sets[static_cast<size_t>(row)].complete_provenance =
            complete->Value(row);
    }

    std::uint64_t previous_exact_set = 0;
    bool have_exact = false;
    for_each_fact_table_batch(
        impl_->root / "parquet/exact_facts.parquet", exact_schema(),
        impl_->manifest.tables.at("parquet/exact_facts.parquet").rows,
        [&](const std::shared_ptr<arrow::RecordBatch>& batch) {
            const auto exact_set_ids =
                std::static_pointer_cast<arrow::UInt64Array>(batch->column(0));
            const auto exact_transcripts =
                std::static_pointer_cast<arrow::StringArray>(batch->column(1));
            const auto exact_locus_parents =
                std::static_pointer_cast<arrow::StringArray>(batch->column(2));
            const auto exact_paths =
                std::static_pointer_cast<arrow::StringArray>(batch->column(3));
            const auto exact_parents =
                std::static_pointer_cast<arrow::StringArray>(batch->column(4));
            const auto exact_scores =
                std::static_pointer_cast<arrow::Int64Array>(batch->column(5));
            const auto exact_tiers =
                std::static_pointer_cast<arrow::StringArray>(batch->column(6));
            const auto exact_strands =
                std::static_pointer_cast<arrow::StringArray>(batch->column(7));
            for (std::int64_t row = 0; row < batch->num_rows(); ++row) {
                if (exact_set_ids->IsNull(row) || exact_scores->IsNull(row)) {
                    fail("exact compatibility fact contains an unexpected null");
                }
                const std::uint64_t set_id = exact_set_ids->Value(row);
                if (set_id >= impl_->index.fact_sets.size() ||
                    (have_exact && set_id < previous_exact_set)) {
                    fail("exact compatibility fact-set IDs are invalid or unsorted");
                }
                have_exact = true;
                previous_exact_set = set_id;
                ExactCompatibilityFact fact{
                    string_value(exact_transcripts, row),
                    string_value(exact_locus_parents, row),
                    string_value(exact_paths, row),
                    string_value(exact_parents, row),
                    exact_scores->Value(row),
                    parse_tier(string_value(exact_tiers, row)),
                    parse_strand(string_value(exact_strands, row)),
                };
                validate_exact_fact(fact);
                impl_->index.fact_sets[set_id].exact.push_back(std::move(fact));
            }
        });

    std::uint64_t previous_structural_set = 0;
    bool have_structural = false;
    for_each_fact_table_batch(
        impl_->root / "parquet/structural_facts.parquet", structural_schema(),
        impl_->manifest.tables.at("parquet/structural_facts.parquet").rows,
        [&](const std::shared_ptr<arrow::RecordBatch>& batch) {
            const auto structural_set_ids =
                std::static_pointer_cast<arrow::UInt64Array>(batch->column(0));
            const auto structural_transcripts =
                std::static_pointer_cast<arrow::StringArray>(batch->column(1));
            const auto structural_layers =
                std::static_pointer_cast<arrow::StringArray>(batch->column(2));
            const auto structural_scores =
                std::static_pointer_cast<arrow::Int64Array>(batch->column(3));
            const auto structural_strands =
                std::static_pointer_cast<arrow::StringArray>(batch->column(4));
            const auto path_lists =
                std::static_pointer_cast<arrow::ListArray>(batch->column(5));
            const auto parent_lists =
                std::static_pointer_cast<arrow::ListArray>(batch->column(6));
            const auto path_values = std::static_pointer_cast<arrow::StringArray>(
                path_lists->values());
            const auto parent_values = std::static_pointer_cast<arrow::StringArray>(
                parent_lists->values());
            for (std::int64_t row = 0; row < batch->num_rows(); ++row) {
                if (structural_set_ids->IsNull(row) ||
                    structural_scores->IsNull(row) || path_lists->IsNull(row) ||
                    parent_lists->IsNull(row)) {
                    fail("structural compatibility fact contains an unexpected null");
                }
                const std::uint64_t set_id = structural_set_ids->Value(row);
                if (set_id >= impl_->index.fact_sets.size() ||
                    (have_structural && set_id < previous_structural_set)) {
                    fail("structural compatibility fact-set IDs are invalid or unsorted");
                }
                have_structural = true;
                previous_structural_set = set_id;
                StructuralCompatibilityFact fact{
                    string_value(structural_transcripts, row),
                    parse_layer(string_value(structural_layers, row)),
                    structural_scores->Value(row),
                    parse_strand(string_value(structural_strands, row)),
                    {},
                    {},
                };
                for (std::int64_t index = path_lists->value_offset(row);
                     index < path_lists->value_offset(row + 1); ++index) {
                    if (path_values->IsNull(index)) {
                        fail("structural compatibility path list contains null");
                    }
                    fact.winning_paths.push_back(path_values->GetString(index));
                }
                for (std::int64_t index = parent_lists->value_offset(row);
                     index < parent_lists->value_offset(row + 1); ++index) {
                    if (parent_values->IsNull(index)) {
                        fail("structural compatibility Parent list contains null");
                    }
                    fact.winning_parents.push_back(parent_values->GetString(index));
                }
                validate_structural_fact(fact);
                impl_->index.fact_sets[set_id].structural.push_back(std::move(fact));
            }
        });

    for (size_t index = 0; index < impl_->index.fact_sets.size(); ++index) {
        CompatibilityFactSet canonical = impl_->index.fact_sets[index];
        canonicalize(canonical);
        if (canonical != impl_->index.fact_sets[index]) {
            fail("compatibility facts are not canonical or contain duplicates");
        }
        if (index != 0 &&
            !(impl_->index.fact_sets[index - 1] < impl_->index.fact_sets[index])) {
            fail("compatibility fact-set dictionary is not canonical and unique");
        }
    }
    CompatibilityBundle logical_bundle;
    logical_bundle.fact_sets = impl_->index.fact_sets;
    const auto actual_logical = logical_hashes(logical_bundle);
    for (const std::string_view path : {"parquet/fact_sets.parquet",
                                        "parquet/exact_facts.parquet",
                                        "parquet/structural_facts.parquet"}) {
        if (actual_logical.at(std::string(path)) !=
            impl_->manifest.tables.at(std::string(path)).logical_sha256) {
            throw std::runtime_error("compatibility logical hash mismatch: " +
                                     std::string(path));
        }
    }

    auto input = arrow_value(arrow::io::ReadableFile::Open(
                                 (impl_->root / "parquet/read_rows.parquet").string()),
                             "cannot open compatibility read Parquet input");
    std::unique_ptr<parquet::arrow::FileReader> reader;
    check_arrow(parquet::arrow::OpenFile(input, arrow::default_memory_pool(), &reader),
                "cannot initialize compatibility read Parquet reader");
    std::shared_ptr<arrow::Schema> schema;
    check_arrow(reader->GetSchema(&schema),
                "cannot read compatibility read Parquet schema");
    if (!schema->Equals(*read_schema())) {
        fail("unexpected compatibility read Parquet schema");
    }
    const auto metadata = reader->parquet_reader()->metadata();
    if (metadata->num_rows() < 0 ||
        static_cast<std::uint64_t>(metadata->num_rows()) != impl_->index.read_rows) {
        fail("compatibility read Parquet row-count mismatch");
    }
}

CompatibilityBundleReader::CompatibilityBundleReader(
    CompatibilityBundleReader&&) noexcept = default;
CompatibilityBundleReader& CompatibilityBundleReader::operator=(
    CompatibilityBundleReader&&) noexcept = default;
CompatibilityBundleReader::~CompatibilityBundleReader() = default;

const CompatibilityBundleIndex& CompatibilityBundleReader::index() const {
    if (!impl_) throw std::logic_error("compatibility reader was moved from");
    return impl_->index;
}

void CompatibilityBundleReader::for_each_read_batch(
    const std::function<void(std::span<const ReadCompatibilityRow>)>& callback,
    std::uint64_t maximum_batch_rows) const {
    if (!impl_) throw std::logic_error("compatibility reader was moved from");
    if (!callback || maximum_batch_rows == 0 ||
        maximum_batch_rows > kMaximumCompatibilityRowsPerBatch) {
        throw std::invalid_argument("compatibility read batch options are invalid");
    }
    auto input = arrow_value(arrow::io::ReadableFile::Open(
                                 (impl_->root / "parquet/read_rows.parquet").string()),
                             "cannot reopen compatibility read Parquet input");
    std::unique_ptr<parquet::arrow::FileReader> reader;
    check_arrow(parquet::arrow::OpenFile(input, arrow::default_memory_pool(), &reader),
                "cannot initialize compatibility streaming reader");
    reader->set_batch_size(static_cast<std::int64_t>(maximum_batch_rows));
    std::unique_ptr<arrow::RecordBatchReader> batches;
    check_arrow(reader->GetRecordBatchReader(&batches),
                "cannot initialize compatibility record-batch reader");

    Sha256Digest logical_digest("parquet/read_rows.parquet");
    std::uint64_t next_ordinal = 0;
    MoleculeStatusCounts observed_status_counts;
    while (true) {
        std::shared_ptr<arrow::RecordBatch> batch;
        check_arrow(batches->ReadNext(&batch),
                    "cannot stream compatibility read batch");
        if (!batch) break;
        if (!batch->schema()->Equals(*read_schema())) {
            fail("unexpected compatibility streaming batch schema");
        }
        const auto ordinals =
            std::static_pointer_cast<arrow::UInt64Array>(batch->column(0));
        const auto names =
            std::static_pointer_cast<arrow::StringArray>(batch->column(1));
        const auto statuses =
            std::static_pointer_cast<arrow::StringArray>(batch->column(2));
        const auto barcodes =
            std::static_pointer_cast<arrow::StringArray>(batch->column(3));
        const auto barcode_qualities =
            std::static_pointer_cast<arrow::StringArray>(batch->column(4));
        const auto umis =
            std::static_pointer_cast<arrow::StringArray>(batch->column(5));
        const auto umi_qualities =
            std::static_pointer_cast<arrow::StringArray>(batch->column(6));
        const auto fact_sets =
            std::static_pointer_cast<arrow::UInt64Array>(batch->column(7));
        std::vector<ReadCompatibilityRow> rows;
        rows.reserve(static_cast<size_t>(batch->num_rows()));
        for (std::int64_t row = 0; row < batch->num_rows(); ++row) {
            if (ordinals->IsNull(row) || fact_sets->IsNull(row) ||
                ordinals->Value(row) != next_ordinal ||
                fact_sets->Value(row) >= impl_->index.fact_sets.size()) {
                fail("compatibility read ordinal/fact-set referential integrity failure");
            }
            ReadCompatibilityRow decoded{
                ordinals->Value(row),
                string_value(names, row),
                parse_molecule_status(string_value(statuses, row)),
                optional_string(barcodes, row),
                optional_string(barcode_qualities, row),
                optional_string(umis, row),
                optional_string(umi_qualities, row),
                fact_sets->Value(row),
            };
            validate_read(decoded);
            append_logical_read(logical_digest, decoded);
            increment_status(observed_status_counts, decoded.molecule_status);
            rows.push_back(std::move(decoded));
            ++next_ordinal;
        }
        if (!rows.empty()) callback(rows);
    }
    if (next_ordinal != impl_->index.read_rows) {
        fail("compatibility streaming read count is incomplete");
    }
    if (observed_status_counts != impl_->index.molecule_status_counts) {
        fail("compatibility read statuses do not match the manifest denominator");
    }
    if (logical_digest.finish() !=
        impl_->manifest.tables.at("parquet/read_rows.parquet").logical_sha256) {
        fail("compatibility logical hash mismatch: parquet/read_rows.parquet");
    }
}

CompatibilityBundle read_compatibility_bundle(
    const std::filesystem::path& directory) {
    CompatibilityBundleReader reader(directory);
    CompatibilityBundle output;
    output.fact_sets = reader.index().fact_sets;
    output.inputs = reader.index().inputs;
    output.producer_version = reader.index().producer_version;
    output.compatibility_algorithm_id = reader.index().compatibility_algorithm_id;
    output.structural_surface_id = reader.index().structural_surface_id;
    output.content_id = reader.index().content_id;
    output.input_records = reader.index().input_records;
    output.input_read_groups = reader.index().input_read_groups;
    output.molecule_status_counts = reader.index().molecule_status_counts;
    if (reader.index().read_rows >
        static_cast<std::uint64_t>(std::numeric_limits<size_t>::max())) {
        fail("compatibility read table is too large for the test-scale reader");
    }
    output.reads.reserve(static_cast<size_t>(reader.index().read_rows));
    reader.for_each_read_batch([&](std::span<const ReadCompatibilityRow> rows) {
        output.reads.insert(output.reads.end(), rows.begin(), rows.end());
    });
    return output;
}

}  // namespace pancollapse::direct_count
