#include "direct_count_diagnostics.hpp"
#include "direct_count_assignment.hpp"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <openssl/evp.h>
#include <parquet/arrow/writer.h>
#include <parquet/properties.h>
#include <zstd.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace pancollapse::direct_count {
namespace {

constexpr std::array<char, 8> kMagic{'P', 'C', 'D', 'I', 'A', 'G', '0', '3'};
constexpr std::uint32_t kVersion = 3;
constexpr char kRecordMarker = 'R';
constexpr char kTrailerMarker = 'T';
constexpr size_t kBufferBytes = 1 << 16;

[[noreturn]] void fail(std::string_view message) {
    throw std::runtime_error(std::string(message));
}

void check_zstd(size_t value, std::string_view operation) {
    if (ZSTD_isError(value)) {
        throw std::runtime_error(std::string(operation) + ": " +
                                 ZSTD_getErrorName(value));
    }
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

class Sha256Digest {
  public:
    Sha256Digest() : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
        if (!context_ || EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1) {
            fail("cannot initialize diagnostics SHA-256");
        }
    }

    void update(const void* data, size_t size) {
        if (finished_) {
            fail("cannot update a finalized diagnostics SHA-256");
        }
        if (size != 0 && EVP_DigestUpdate(context_.get(), data, size) != 1) {
            fail("cannot update diagnostics SHA-256");
        }
    }

    void update(std::string_view value) { update(value.data(), value.size()); }

    std::string finish() {
        if (finished_) {
            fail("diagnostics SHA-256 was finalized twice");
        }
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int length = 0;
        if (EVP_DigestFinal_ex(context_.get(), digest.data(), &length) != 1 ||
            length != 32) {
            fail("cannot finalize diagnostics SHA-256");
        }
        static constexpr char hexadecimal[] = "0123456789abcdef";
        std::string result;
        result.reserve(length * 2);
        for (size_t index = 0; index < length; ++index) {
            result.push_back(hexadecimal[digest[index] >> 4U]);
            result.push_back(hexadecimal[digest[index] & 15U]);
        }
        finished_ = true;
        return result;
    }

  private:
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_;
    bool finished_ = false;
};

void append_u32(std::string& output, std::uint32_t value) {
    for (unsigned shift = 0; shift != 32; shift += 8) {
        output.push_back(static_cast<char>(value >> shift));
    }
}

void append_u64(std::string& output, std::uint64_t value) {
    for (unsigned shift = 0; shift != 64; shift += 8) {
        output.push_back(static_cast<char>(value >> shift));
    }
}

void append_text(std::string& output, std::string_view value) {
    if (value.size() > std::numeric_limits<std::uint32_t>::max()) {
        fail("diagnostic string exceeds uint32 length");
    }
    append_u32(output, static_cast<std::uint32_t>(value.size()));
    output.append(value);
}

void append_optional(std::string& output,
                     const std::optional<std::string>& value) {
    if (value && value->size() >= std::numeric_limits<std::uint32_t>::max()) {
        fail("optional diagnostic string exceeds uint32 length");
    }
    append_u32(output, value ? static_cast<std::uint32_t>(value->size() + 1) : 0);
    if (value) {
        output.append(*value);
    }
}

std::string encode_record(const DirectCountDiagnosticRecord& record) {
    std::string output;
    append_u64(output, record.ordinal);
    append_text(output, record.qname);
    append_u32(output, record.profile_index);
    append_optional(output, record.raw_barcode);
    append_optional(output, record.raw_barcode_quality);
    append_optional(output, record.umi);
    append_text(output, record.terminal_class);
    append_optional(output, record.selected_gene);
    append_optional(output, record.selected_tier);
    append_u64(output, record.reason_bits);
    output.push_back(record.barcode_correction_eligible ? '\1' : '\0');
    return output;
}

class ZstdWriter {
  public:
    explicit ZstdWriter(const std::filesystem::path& path)
        : output_(path, std::ios::binary | std::ios::trunc),
          context_(ZSTD_createCCtx()) {
        if (!output_) {
            fail("cannot open diagnostics spool");
        }
        if (context_ == nullptr) {
            fail("cannot allocate diagnostics compressor");
        }
        check_zstd(ZSTD_CCtx_setParameter(context_, ZSTD_c_compressionLevel, 3),
                   "cannot set diagnostics ZSTD level");
        check_zstd(ZSTD_CCtx_setParameter(context_, ZSTD_c_checksumFlag, 1),
                   "cannot enable diagnostics ZSTD checksum");
    }

    ZstdWriter(const ZstdWriter&) = delete;
    ZstdWriter& operator=(const ZstdWriter&) = delete;

    ~ZstdWriter() {
        if (context_ != nullptr) {
            ZSTD_freeCCtx(context_);
        }
    }

    void write(std::string_view value) {
        ZSTD_inBuffer input{value.data(), value.size(), 0};
        while (input.pos < input.size) {
            ZSTD_outBuffer output{buffer_.data(), buffer_.size(), 0};
            check_zstd(ZSTD_compressStream2(context_, &output, &input,
                                            ZSTD_e_continue),
                       "cannot write diagnostics spool");
            output_.write(buffer_.data(), static_cast<std::streamsize>(output.pos));
            if (!output_) {
                fail("cannot write diagnostics spool");
            }
        }
    }

    void finish() {
        size_t remaining = 1;
        while (remaining != 0) {
            ZSTD_inBuffer input{nullptr, 0, 0};
            ZSTD_outBuffer output{buffer_.data(), buffer_.size(), 0};
            remaining = ZSTD_compressStream2(context_, &output, &input, ZSTD_e_end);
            check_zstd(remaining, "cannot finish diagnostics spool");
            output_.write(buffer_.data(), static_cast<std::streamsize>(output.pos));
            if (!output_) {
                fail("cannot write diagnostics spool trailer");
            }
        }
        output_.close();
        if (!output_) {
            fail("cannot close diagnostics spool");
        }
    }

  private:
    std::ofstream output_;
    ZSTD_CCtx* context_ = nullptr;
    std::array<char, kBufferBytes> buffer_{};
};

class ZstdReader {
  public:
    explicit ZstdReader(const std::filesystem::path& path)
        : input_(path, std::ios::binary), context_(ZSTD_createDCtx()) {
        if (!input_) {
            fail("cannot open diagnostics spool");
        }
        if (context_ == nullptr) {
            fail("cannot allocate diagnostics decompressor");
        }
    }

    ZstdReader(const ZstdReader&) = delete;
    ZstdReader& operator=(const ZstdReader&) = delete;

    ~ZstdReader() {
        if (context_ != nullptr) {
            ZSTD_freeDCtx(context_);
        }
    }

    bool read_byte(char& value) { return read(&value, 1, true); }

    void read_exact(void* destination, size_t size) {
        if (!read(destination, size, false)) {
            fail("diagnostics spool is truncated");
        }
    }

    void require_eof() {
        char extra = 0;
        if (read_byte(extra)) {
            fail("diagnostics spool has trailing logical bytes");
        }
        if (frame_count_ != 1) {
            fail("diagnostics spool is truncated or has multiple ZSTD frames");
        }
    }

  private:
    bool read(void* destination, size_t size, bool allow_clean_eof) {
        char* target = static_cast<char*>(destination);
        size_t copied = 0;
        while (copied < size) {
            if (decoded_at_ == decoded_size_ && !refill()) {
                if (allow_clean_eof && copied == 0) {
                    return false;
                }
                fail("diagnostics spool is truncated");
            }
            const size_t available = decoded_size_ - decoded_at_;
            const size_t amount = std::min(size - copied, available);
            std::copy_n(decoded_.data() + decoded_at_, amount, target + copied);
            decoded_at_ += amount;
            copied += amount;
        }
        return true;
    }

    bool refill() {
        decoded_at_ = 0;
        decoded_size_ = 0;
        while (decoded_size_ == 0) {
            if (encoded_at_ == encoded_size_) {
                input_.read(encoded_.data(),
                            static_cast<std::streamsize>(encoded_.size()));
                encoded_size_ = static_cast<size_t>(input_.gcount());
                encoded_at_ = 0;
                if (encoded_size_ == 0) {
                    if (!input_.eof()) {
                        fail("cannot read diagnostics spool");
                    }
                    return false;
                }
            }
            ZSTD_inBuffer input{encoded_.data(), encoded_size_, encoded_at_};
            ZSTD_outBuffer output{decoded_.data(), decoded_.size(), 0};
            const size_t remaining = ZSTD_decompressStream(context_, &output, &input);
            check_zstd(remaining, "cannot decode diagnostics spool");
            encoded_at_ = input.pos;
            decoded_size_ = output.pos;
            if (remaining == 0) {
                ++frame_count_;
                if (frame_count_ > 1) {
                    fail("diagnostics spool has multiple ZSTD frames");
                }
            }
        }
        return true;
    }

    std::ifstream input_;
    ZSTD_DCtx* context_ = nullptr;
    std::array<char, kBufferBytes> encoded_{};
    std::array<char, kBufferBytes> decoded_{};
    size_t encoded_at_ = 0;
    size_t encoded_size_ = 0;
    size_t decoded_at_ = 0;
    size_t decoded_size_ = 0;
    size_t frame_count_ = 0;
};

std::uint32_t read_u32(ZstdReader& input) {
    std::array<unsigned char, 4> bytes{};
    input.read_exact(bytes.data(), bytes.size());
    std::uint32_t result = 0;
    for (size_t index = 0; index < bytes.size(); ++index) {
        result |= static_cast<std::uint32_t>(bytes[index]) << (index * 8);
    }
    return result;
}

std::uint64_t read_u64(ZstdReader& input) {
    std::array<unsigned char, 8> bytes{};
    input.read_exact(bytes.data(), bytes.size());
    std::uint64_t result = 0;
    for (size_t index = 0; index < bytes.size(); ++index) {
        result |= static_cast<std::uint64_t>(bytes[index]) << (index * 8);
    }
    return result;
}

std::string read_text(ZstdReader& input) {
    std::string result(read_u32(input), '\0');
    if (!result.empty()) {
        input.read_exact(result.data(), result.size());
    }
    return result;
}

std::optional<std::string> read_optional(ZstdReader& input) {
    const std::uint32_t encoded_size = read_u32(input);
    if (encoded_size == 0) {
        return std::nullopt;
    }
    std::string result(static_cast<size_t>(encoded_size) - 1, '\0');
    if (!result.empty()) {
        input.read_exact(result.data(), result.size());
    }
    return result;
}

DirectCountDiagnosticRecord decode_record(ZstdReader& input) {
    DirectCountDiagnosticRecord result;
    result.ordinal = read_u64(input);
    result.qname = read_text(input);
    result.profile_index = read_u32(input);
    result.raw_barcode = read_optional(input);
    result.raw_barcode_quality = read_optional(input);
    result.umi = read_optional(input);
    result.terminal_class = read_text(input);
    result.selected_gene = read_optional(input);
    result.selected_tier = read_optional(input);
    result.reason_bits = read_u64(input);
    char barcode_correction_eligible = 0;
    input.read_exact(&barcode_correction_eligible, 1);
    if (barcode_correction_eligible != 0 && barcode_correction_eligible != 1) {
        fail("diagnostics spool has an invalid barcode-eligibility flag");
    }
    result.barcode_correction_eligible = barcode_correction_eligible != 0;
    return result;
}

std::string file_sha256(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        fail("cannot open diagnostics Parquet for hashing");
    }
    Sha256Digest digest;
    std::array<char, kBufferBytes> buffer{};
    while (input) {
        input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        if (input.gcount() > 0) {
            digest.update(buffer.data(), static_cast<size_t>(input.gcount()));
        }
    }
    if (!input.eof()) {
        fail("cannot read diagnostics Parquet for hashing");
    }
    return digest.finish();
}

}  // namespace

struct DirectCountDiagnosticsSpool::Impl {
    explicit Impl(const std::filesystem::path& path) : writer(path) {
        std::string header(kMagic.begin(), kMagic.end());
        append_u32(header, kVersion);
        writer.write(header);
    }

    ZstdWriter writer;
    Sha256Digest record_digest;
    std::uint64_t count = 0;
    std::uint64_t last_ordinal = 0;
    bool have_ordinal = false;
    bool finished = false;
};

DirectCountDiagnosticsSpool::DirectCountDiagnosticsSpool(
    const std::filesystem::path& path)
    : impl_(std::make_unique<Impl>(path)) {}

DirectCountDiagnosticsSpool::DirectCountDiagnosticsSpool(
    DirectCountDiagnosticsSpool&&) noexcept = default;

DirectCountDiagnosticsSpool& DirectCountDiagnosticsSpool::operator=(
    DirectCountDiagnosticsSpool&&) noexcept = default;

DirectCountDiagnosticsSpool::~DirectCountDiagnosticsSpool() = default;

void DirectCountDiagnosticsSpool::append(
    const DirectCountDiagnosticRecord& record) {
    if (!impl_ || impl_->finished) {
        fail("cannot append a finished diagnostics spool");
    }
    if (impl_->have_ordinal && record.ordinal < impl_->last_ordinal) {
        fail("diagnostic spool ordinals must be nondecreasing");
    }
    if (record.terminal_class.empty()) {
        fail("diagnostic terminal class must not be empty");
    }
    if (impl_->count == std::numeric_limits<std::uint64_t>::max()) {
        fail("diagnostic row count overflow");
    }
    const std::string encoded = encode_record(record);
    impl_->writer.write(std::string_view(&kRecordMarker, 1));
    impl_->writer.write(encoded);
    impl_->record_digest.update(encoded);
    ++impl_->count;
    impl_->last_ordinal = record.ordinal;
    impl_->have_ordinal = true;
}

void DirectCountDiagnosticsSpool::finish() {
    if (!impl_ || impl_->finished) {
        return;
    }
    std::string trailer(1, kTrailerMarker);
    append_u64(trailer, impl_->count);
    append_text(trailer, impl_->record_digest.finish());
    impl_->writer.write(trailer);
    impl_->writer.finish();
    impl_->finished = true;
}

DirectCountDiagnosticsParquetResult write_direct_count_diagnostics_parquet(
    const std::filesystem::path& spool_path, const CountRuntime& runtime,
    const DirectCountDiagnosticsParquetOptions& options) {
    if (options.output_path.empty() || options.row_group_rows == 0 ||
        options.row_group_rows >
            static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
        fail("diagnostics Parquet output and a valid row-group size are required");
    }
    if (!options.output_path.parent_path().empty()) {
        std::filesystem::create_directories(options.output_path.parent_path());
    }

    const auto schema = arrow::schema({
        arrow::field("ordinal", arrow::uint64(), false),
        arrow::field("qname", arrow::utf8(), false),
        arrow::field("profile_id", arrow::utf8(), false),
        arrow::field("raw_barcode", arrow::utf8(), true),
        arrow::field("corrected_barcode", arrow::utf8(), true),
        arrow::field("umi", arrow::utf8(), true),
        arrow::field("terminal_class", arrow::utf8(), false),
        arrow::field("selected_gene", arrow::utf8(), true),
        arrow::field("selected_tier", arrow::utf8(), true),
        arrow::field("reason_bits", arrow::uint64(), false),
    });
    auto output = arrow_value(
        arrow::io::FileOutputStream::Open(options.output_path.string()),
        "cannot open diagnostics Parquet");
    parquet::WriterProperties::Builder parquet_properties;
    parquet_properties.version(parquet::ParquetVersion::PARQUET_2_6)
        ->compression(parquet::Compression::ZSTD)
        ->compression_level(3)
        ->created_by("panCollapse-direct-diagnostics-v1");
    parquet::ArrowWriterProperties::Builder arrow_properties;
    arrow_properties.store_schema();
    auto writer = arrow_value(
        parquet::arrow::FileWriter::Open(
            *schema, arrow::default_memory_pool(), output,
            parquet_properties.build(), arrow_properties.build()),
        "cannot open diagnostics Parquet writer");

    ZstdReader spool(spool_path);
    std::array<char, kMagic.size()> magic{};
    spool.read_exact(magic.data(), magic.size());
    if (magic != kMagic || read_u32(spool) != kVersion) {
        fail("diagnostics spool has invalid magic or version");
    }

    Sha256Digest raw_digest;
    Sha256Digest logical_digest;
    std::uint64_t row_count = 0;
    std::uint64_t last_ordinal = 0;
    bool have_ordinal = false;
    bool done = false;
    const auto& profiles = runtime.profiles();
    const auto& whitelist = runtime.whitelist();

    while (!done) {
        arrow::UInt64Builder ordinal_builder;
        arrow::StringBuilder qname_builder;
        arrow::StringBuilder profile_builder;
        arrow::StringBuilder raw_barcode_builder;
        arrow::StringBuilder corrected_barcode_builder;
        arrow::StringBuilder umi_builder;
        arrow::StringBuilder terminal_builder;
        arrow::StringBuilder gene_builder;
        arrow::StringBuilder tier_builder;
        arrow::UInt64Builder reason_builder;
        std::int64_t rows_in_group = 0;

        while (rows_in_group < static_cast<std::int64_t>(options.row_group_rows)) {
            char marker = 0;
            spool.read_exact(&marker, 1);
            if (marker == kTrailerMarker) {
                const std::uint64_t declared_rows = read_u64(spool);
                const std::string declared_digest = read_text(spool);
                if (declared_rows != row_count ||
                    declared_digest != raw_digest.finish()) {
                    fail("diagnostics spool trailer validation failed");
                }
                done = true;
                break;
            }
            if (marker != kRecordMarker) {
                fail("diagnostics spool has an invalid record marker");
            }

            DirectCountDiagnosticRecord record = decode_record(spool);
            if (have_ordinal && record.ordinal < last_ordinal) {
                fail("diagnostics spool ordinal order is not deterministic");
            }
            have_ordinal = true;
            last_ordinal = record.ordinal;
            raw_digest.update(encode_record(record));
            if (record.profile_index >= profiles.size()) {
                fail("diagnostics spool profile index is outside runtime dictionary");
            }

            std::optional<std::string> corrected_barcode;
            if (record.raw_barcode) {
                const std::optional<std::string_view> quality =
                    record.raw_barcode_quality
                        ? std::optional<std::string_view>(*record.raw_barcode_quality)
                        : std::nullopt;
                const auto corrected_index = runtime.corrected_barcode_index(
                    *record.raw_barcode, quality);
                if (corrected_index) {
                    if (*corrected_index >= whitelist.size()) {
                        fail("runtime returned an invalid corrected barcode index");
                    }
                    corrected_barcode = whitelist[*corrected_index];
                }
            }
            if (record.barcode_correction_eligible && record.raw_barcode &&
                !corrected_barcode) {
                record.terminal_class = "barcode_uncorrectable";
                record.reason_bits |= reason_barcode_uncorrectable;
            }
            const std::string& profile_id =
                profiles[record.profile_index].profile.id_string;

            std::string logical_row;
            append_u64(logical_row, record.ordinal);
            append_text(logical_row, record.qname);
            append_text(logical_row, profile_id);
            append_optional(logical_row, record.raw_barcode);
            append_optional(logical_row, corrected_barcode);
            append_optional(logical_row, record.umi);
            append_text(logical_row, record.terminal_class);
            append_optional(logical_row, record.selected_gene);
            append_optional(logical_row, record.selected_tier);
            append_u64(logical_row, record.reason_bits);
            logical_digest.update(logical_row);

            check_arrow(ordinal_builder.Append(record.ordinal),
                        "append diagnostic ordinal");
            check_arrow(qname_builder.Append(record.qname),
                        "append diagnostic QNAME");
            check_arrow(profile_builder.Append(profile_id),
                        "append diagnostic profile");
            check_arrow(record.raw_barcode
                            ? raw_barcode_builder.Append(*record.raw_barcode)
                            : raw_barcode_builder.AppendNull(),
                        "append diagnostic raw barcode");
            check_arrow(corrected_barcode
                            ? corrected_barcode_builder.Append(*corrected_barcode)
                            : corrected_barcode_builder.AppendNull(),
                        "append diagnostic corrected barcode");
            check_arrow(record.umi ? umi_builder.Append(*record.umi)
                                   : umi_builder.AppendNull(),
                        "append diagnostic UMI");
            check_arrow(terminal_builder.Append(record.terminal_class),
                        "append diagnostic terminal class");
            check_arrow(record.selected_gene
                            ? gene_builder.Append(*record.selected_gene)
                            : gene_builder.AppendNull(),
                        "append diagnostic gene");
            check_arrow(record.selected_tier
                            ? tier_builder.Append(*record.selected_tier)
                            : tier_builder.AppendNull(),
                        "append diagnostic tier");
            check_arrow(reason_builder.Append(record.reason_bits),
                        "append diagnostic reason bits");
            ++row_count;
            ++rows_in_group;
        }

        if (rows_in_group != 0) {
            std::shared_ptr<arrow::Array> ordinal_array;
            std::shared_ptr<arrow::Array> qname_array;
            std::shared_ptr<arrow::Array> profile_array;
            std::shared_ptr<arrow::Array> raw_barcode_array;
            std::shared_ptr<arrow::Array> corrected_barcode_array;
            std::shared_ptr<arrow::Array> umi_array;
            std::shared_ptr<arrow::Array> terminal_array;
            std::shared_ptr<arrow::Array> gene_array;
            std::shared_ptr<arrow::Array> tier_array;
            std::shared_ptr<arrow::Array> reason_array;
            check_arrow(ordinal_builder.Finish(&ordinal_array),
                        "finish diagnostic ordinal");
            check_arrow(qname_builder.Finish(&qname_array),
                        "finish diagnostic QNAME");
            check_arrow(profile_builder.Finish(&profile_array),
                        "finish diagnostic profile");
            check_arrow(raw_barcode_builder.Finish(&raw_barcode_array),
                        "finish diagnostic raw barcode");
            check_arrow(corrected_barcode_builder.Finish(&corrected_barcode_array),
                        "finish diagnostic corrected barcode");
            check_arrow(umi_builder.Finish(&umi_array), "finish diagnostic UMI");
            check_arrow(terminal_builder.Finish(&terminal_array),
                        "finish diagnostic terminal class");
            check_arrow(gene_builder.Finish(&gene_array), "finish diagnostic gene");
            check_arrow(tier_builder.Finish(&tier_array), "finish diagnostic tier");
            check_arrow(reason_builder.Finish(&reason_array),
                        "finish diagnostic reasons");
            const auto table = arrow::Table::Make(
                schema,
                {ordinal_array, qname_array, profile_array, raw_barcode_array,
                 corrected_barcode_array, umi_array, terminal_array, gene_array,
                 tier_array, reason_array},
                rows_in_group);
            check_arrow(writer->WriteTable(
                            *table,
                            static_cast<std::int64_t>(options.row_group_rows)),
                        "write diagnostics Parquet row group");
        }
    }

    spool.require_eof();
    check_arrow(writer->Close(), "close diagnostics Parquet writer");
    check_arrow(output->Close(), "close diagnostics Parquet stream");
    return {row_count, logical_digest.finish(), file_sha256(options.output_path)};
}

}  // namespace pancollapse::direct_count
