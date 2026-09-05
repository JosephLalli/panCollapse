#include "direct_count_output.hpp"

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <arrow/util/config.h>
#include <openssl/evp.h>
#include <parquet/arrow/writer.h>
#include <parquet/file_reader.h>
#include <parquet/parquet_version.h>
#include <parquet/properties.h>
#include <zstd.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <unistd.h>

namespace pancollapse::direct_count {
namespace {

static_assert(ARROW_VERSION_MAJOR == 17 && ARROW_VERSION_MINOR == 0 &&
                  ARROW_VERSION_PATCH == 0,
              "PanCollapse v0.10.0 requires pinned Arrow 17.0.0");
static_assert(PARQUET_VERSION_MAJOR == 17 && PARQUET_VERSION_MINOR == 0 &&
                  PARQUET_VERSION_PATCH == 0,
              "PanCollapse v0.10.0 requires pinned Parquet 17.0.0");

constexpr std::uint32_t kMissingIndex = std::numeric_limits<std::uint32_t>::max();
constexpr size_t kProfileRuntimeMetricCount = 8;

std::array<std::pair<std::string_view, std::uint64_t>, kProfileRuntimeMetricCount>
profile_runtime_metrics(const ProfileCountRuntimeCounters& counters) {
    return {{{"assigned_observations", counters.assigned_observations},
             {"direct_multigene_dropped", counters.direct_multigene_dropped},
             {"invalid_umi_dropped", counters.invalid_umi_dropped},
             {"umi_n_dropped", counters.umi_n_dropped},
             {"umi_homopolymer_dropped", counters.umi_homopolymer_dropped},
             {"off_whitelist_uncorrectable_dropped",
              counters.off_whitelist_uncorrectable_dropped},
             {"barcode_correction_succeeded", counters.barcode_correction_succeeded},
             {"barcode_correction_failed", counters.barcode_correction_failed}}};
}

std::string hex(const unsigned char* bytes, size_t size) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result(size * 2, '0');
    for (size_t index = 0; index < size; ++index) {
        result[index * 2] = digits[bytes[index] >> 4U];
        result[index * 2 + 1] = digits[bytes[index] & 15U];
    }
    return result;
}

class LogicalDigest {
  public:
    explicit LogicalDigest(std::string_view table) : context_(EVP_MD_CTX_new(), EVP_MD_CTX_free) {
        if (!context_ || EVP_DigestInit_ex(context_.get(), EVP_sha256(), nullptr) != 1) {
            throw std::runtime_error("cannot initialize logical SHA-256");
        }
        string("pancollapse-logical-table-v1");
        string(table);
    }

    void byte(std::uint8_t value) { update(&value, sizeof(value)); }

    template<class Integer>
    void integer(Integer value) {
        static_assert(std::is_unsigned_v<Integer>);
        std::array<unsigned char, sizeof(Integer)> encoded{};
        for (size_t index = 0; index < sizeof(Integer); ++index) {
            encoded[sizeof(Integer) - index - 1] = static_cast<unsigned char>(value & 0xffU);
            value >>= 8U;
        }
        update(encoded.data(), encoded.size());
    }

    void string(std::string_view value) {
        integer<std::uint64_t>(value.size());
        update(value.data(), value.size());
    }

    void nullable(const std::optional<std::string>& value) {
        byte(value ? 1 : 0);
        if (value) {
            string(*value);
        }
    }

    std::string finish() {
        if (finished_) {
            throw std::logic_error("logical digest finalized twice");
        }
        std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
        unsigned int length = 0;
        if (EVP_DigestFinal_ex(context_.get(), digest.data(), &length) != 1 || length != 32) {
            throw std::runtime_error("cannot finalize logical SHA-256");
        }
        finished_ = true;
        return hex(digest.data(), length);
    }

  private:
    void update(const void* data, size_t size) {
        if (size != 0 && EVP_DigestUpdate(context_.get(), data, size) != 1) {
            throw std::runtime_error("cannot update logical SHA-256");
        }
    }

    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> context_;
    bool finished_ = false;
};

void check_arrow(const arrow::Status& status, std::string_view operation) {
    if (!status.ok()) {
        throw std::runtime_error(std::string(operation) + ": " + status.ToString());
    }
}

template<class T>
T arrow_value(arrow::Result<T> result, std::string_view operation) {
    if (!result.ok()) {
        throw std::runtime_error(std::string(operation) + ": " + result.status().ToString());
    }
    return std::move(result).ValueUnsafe();
}

template<class Builder>
std::shared_ptr<arrow::Array> finish_builder(Builder& builder) {
    std::shared_ptr<arrow::Array> result;
    check_arrow(builder.Finish(&result), "cannot finish Arrow array");
    return result;
}

class ParquetSink {
  public:
    ParquetSink(const std::filesystem::path& path, std::shared_ptr<arrow::Schema> schema,
                std::uint64_t row_group_rows)
        : path_(path), schema_(std::move(schema)), row_group_rows_(row_group_rows) {
        if (row_group_rows_ == 0 ||
            row_group_rows_ > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            throw std::invalid_argument("Parquet row-group size must be positive");
        }
        stream_ = arrow_value(arrow::io::FileOutputStream::Open(path_.string()),
                              "cannot open Parquet output");
        parquet::WriterProperties::Builder properties;
        properties.version(parquet::ParquetVersion::PARQUET_2_6)
            ->compression(parquet::Compression::ZSTD)
            ->compression_level(3)
            ->max_row_group_length(static_cast<std::int64_t>(row_group_rows_))
            ->created_by("panCollapse-native-count");
        parquet::ArrowWriterProperties::Builder arrow_properties;
        arrow_properties.store_schema();
        writer_ = arrow_value(
            parquet::arrow::FileWriter::Open(*schema_, arrow::default_memory_pool(), stream_,
                                             properties.build(), arrow_properties.build()),
            "cannot create Parquet writer");
    }

    void write(const std::vector<std::shared_ptr<arrow::Array>>& arrays,
               std::int64_t rows) {
        if (rows == 0) {
            return;
        }
        const auto table = arrow::Table::Make(schema_, arrays, rows);
        check_arrow(writer_->WriteTable(*table, static_cast<std::int64_t>(row_group_rows_)),
                    "cannot write Parquet row group");
        rows_ += static_cast<std::uint64_t>(rows);
    }

    void close() {
        check_arrow(writer_->Close(), "cannot close Parquet writer");
        writer_.reset();
        check_arrow(stream_->Close(), "cannot close Parquet output");
        stream_.reset();
        auto reader = parquet::ParquetFileReader::OpenFile(path_.string(), false);
        if (reader->metadata()->num_rows() != static_cast<std::int64_t>(rows_) ||
            reader->metadata()->num_columns() != schema_->num_fields()) {
            throw std::runtime_error("Parquet validation disagrees with emitted schema/row count: " +
                                     path_.string());
        }
    }

    std::uint64_t rows() const { return rows_; }

  private:
    std::filesystem::path path_;
    std::shared_ptr<arrow::Schema> schema_;
    std::uint64_t row_group_rows_;
    std::uint64_t rows_ = 0;
    std::shared_ptr<arrow::io::FileOutputStream> stream_;
    std::unique_ptr<parquet::arrow::FileWriter> writer_;
};

std::string join(const std::vector<std::string>& values, std::string_view separator) {
    std::string result;
    for (const std::string& value : values) {
        if (!result.empty()) {
            result += separator;
        }
        result += value;
    }
    return result;
}

std::string json_string(std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20) {
                    output << "\\u" << std::hex << std::setw(4) << std::setfill('0')
                           << static_cast<unsigned int>(character) << std::dec;
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
    return output.str();
}

void write_file(const std::filesystem::path& path, std::string_view contents) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("cannot open output " + path.string());
    }
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output) {
        throw std::runtime_error("cannot write output " + path.string());
    }
}

class GzipOutput {
  public:
    explicit GzipOutput(const std::filesystem::path& path)
        : path_(path), digest_(path.filename().string()) {
        file_ = gzopen(path.string().c_str(), "wb6");
        if (file_ == nullptr) {
            throw std::runtime_error("cannot open gzip output " + path.string());
        }
    }
    GzipOutput(const GzipOutput&) = delete;
    GzipOutput& operator=(const GzipOutput&) = delete;
    ~GzipOutput() {
        if (file_ != nullptr) {
            gzclose(file_);
        }
    }
    void write(std::string_view value) {
        digest_.string(value);
        size_t offset = 0;
        while (offset < value.size()) {
            const unsigned int amount = static_cast<unsigned int>(std::min<size_t>(
                value.size() - offset, std::numeric_limits<unsigned int>::max()));
            const int written = gzwrite(file_, value.data() + offset, amount);
            if (written <= 0 || static_cast<unsigned int>(written) != amount) {
                int error_number = Z_OK;
                const char* message = gzerror(file_, &error_number);
                throw std::runtime_error("cannot write gzip output " + path_.string() +
                                         ": " + (message == nullptr ? "unknown" : message));
            }
            offset += amount;
        }
    }
    std::string close() {
        if (file_ == nullptr) {
            throw std::logic_error("gzip output closed twice");
        }
        const int status = gzclose(file_);
        file_ = nullptr;
        if (status != Z_OK) {
            throw std::runtime_error("cannot close gzip output " + path_.string());
        }
        return digest_.finish();
    }

  private:
    std::filesystem::path path_;
    gzFile file_ = nullptr;
    LogicalDigest digest_;
};

CountTableIdentity table_identity(const std::filesystem::path& root,
                                  std::string relative, std::uint64_t rows,
                                  std::string logical_sha256) {
    const std::filesystem::path path = root / relative;
    return {std::move(relative), rows, std::filesystem::file_size(path),
            std::move(logical_sha256), sha256_file(path)};
}

void append_identity_json(std::ostringstream& out, const CountTableIdentity& identity) {
    out << "{\"logical_sha256\":" << json_string(identity.logical_sha256)
        << ",\"output_sha256\":" << json_string(identity.output_sha256)
        << ",\"path\":" << json_string(identity.relative_path)
        << ",\"rows\":" << identity.rows
        << ",\"size_bytes\":" << identity.size_bytes << '}';
}

}  // namespace

CountOutputReceipt write_count_outputs(
    const CountRuntime& runtime, const CountRuntimeResult& result,
    const CountFactCatalog& facts, const CountOutputOptions& options) {
    if (options.output_directory.empty() || options.pancollapse_version.empty() ||
        options.analysis_scope.empty() || options.fact_bundle_content_id.empty() ||
        options.parquet_row_group_rows == 0) {
        throw std::invalid_argument("direct-count output metadata is incomplete");
    }
    if (options.compatibility_bundle &&
        ((options.compatibility_bundle->mode != "produced" &&
          options.compatibility_bundle->mode != "replayed") ||
         options.compatibility_bundle->manifest_path.empty() ||
         options.compatibility_bundle->content_id.rfind("sha256:", 0) != 0 ||
         options.compatibility_bundle->manifest_sha256.size() != 64)) {
        throw std::invalid_argument(
            "direct-count compatibility provenance is incomplete");
    }
    if (!options.profile_assignment_terminals.empty() &&
        options.profile_assignment_terminals.size() != runtime.profiles().size()) {
        throw std::invalid_argument(
            "profile assignment-terminal counters do not match selected profiles");
    }
    if (result.profile_counters.size() != runtime.profiles().size()) {
        throw std::invalid_argument(
            "profile runtime counters do not match selected profiles");
    }
    if (std::filesystem::exists(options.output_directory)) {
        throw std::invalid_argument("direct-count destination already exists: " +
                                    options.output_directory.string());
    }
    std::filesystem::path parent = options.output_directory.parent_path();
    if (parent.empty()) {
        parent = ".";
    }
    std::filesystem::create_directories(parent);
    std::filesystem::path staging = options.staging_directory;
    if (staging.empty()) {
        staging = parent / ("." + options.output_directory.filename().string() +
                            ".staging-" +
                            std::to_string(static_cast<unsigned long long>(getpid())));
        if (std::filesystem::exists(staging)) {
            throw std::runtime_error("direct-count staging directory already exists: " +
                                     staging.string());
        }
    } else {
        const std::filesystem::path absolute_parent =
            std::filesystem::absolute(parent).lexically_normal();
        const std::filesystem::path staging_parent =
            std::filesystem::absolute(staging).parent_path().lexically_normal();
        if (!std::filesystem::is_directory(staging) ||
            staging_parent != absolute_parent) {
            throw std::invalid_argument(
                "direct-count staging directory must be an existing sibling of destination");
        }
    }

    CountOutputReceipt receipt;
    try {
        std::filesystem::create_directories(staging / "parquet");
        const auto& whitelist = runtime.whitelist();
        const auto& features = runtime.feature_catalog();
        const auto& profiles = runtime.profiles();
        if (result.exact_barcode_priors.size() != whitelist.size()) {
            throw std::logic_error("barcode-prior dictionary size mismatch");
        }

        std::set<std::uint32_t> active_barcodes;
        std::set<std::uint32_t> active_features;
        for (const NumericCountRecord& row : result.counts) {
            active_barcodes.insert(row.barcode_whitelist_index);
            active_features.insert(row.feature_catalog_index);
        }
        for (const NumericMoleculeRecord& row : result.molecules) {
            active_barcodes.insert(row.barcode_whitelist_index);
            active_features.insert(row.feature_catalog_index);
        }
        std::vector<std::uint32_t> barcode_remap(whitelist.size(), kMissingIndex);
        std::vector<std::uint32_t> feature_remap(features.size(), kMissingIndex);
        std::vector<std::uint32_t> barcode_source;
        std::vector<std::uint32_t> feature_source;
        for (const std::uint32_t source : active_barcodes) {
            if (source >= whitelist.size()) {
                throw std::logic_error("count row barcode index is outside whitelist");
            }
            barcode_remap[source] = static_cast<std::uint32_t>(barcode_source.size());
            barcode_source.push_back(source);
        }
        for (const std::uint32_t source : active_features) {
            if (source >= features.size()) {
                throw std::logic_error("count row feature index is outside catalog");
            }
            feature_remap[source] = static_cast<std::uint32_t>(feature_source.size());
            feature_source.push_back(source);
        }

        std::vector<std::unique_ptr<LogicalDigest>> profile_count_digests;
        std::vector<std::unique_ptr<LogicalDigest>> profile_molecule_digests;
        std::vector<std::uint64_t> profile_count_rows(profiles.size(), 0);
        std::vector<std::uint64_t> profile_molecule_rows(profiles.size(), 0);
        profile_count_digests.reserve(profiles.size());
        profile_molecule_digests.reserve(profiles.size());
        for (const EffectiveProfile& profile : profiles) {
            profile_count_digests.push_back(std::make_unique<LogicalDigest>(
                "decoded-counts/" + profile.profile.id_string));
            profile_molecule_digests.push_back(std::make_unique<LogicalDigest>(
                "decoded-molecules/" + profile.profile.id_string));
        }

        const auto barcode_schema = arrow::schema({
            arrow::field("barcode_index", arrow::uint32(), false),
            arrow::field("barcode", arrow::utf8(), false),
            arrow::field("exact_read_prior", arrow::uint64(), false),
        });
        LogicalDigest barcode_digest("barcodes.parquet");
        ParquetSink barcode_sink(staging / "parquet/barcodes.parquet", barcode_schema,
                                 options.parquet_row_group_rows);
        for (size_t begin = 0; begin < barcode_source.size();
             begin += options.parquet_row_group_rows) {
            const size_t end = std::min<size_t>(barcode_source.size(),
                                                begin + options.parquet_row_group_rows);
            arrow::UInt32Builder index_builder;
            arrow::StringBuilder barcode_builder;
            arrow::UInt64Builder prior_builder;
            for (size_t index = begin; index < end; ++index) {
                const std::uint32_t source = barcode_source[index];
                check_arrow(index_builder.Append(static_cast<std::uint32_t>(index)),
                            "cannot append barcode index");
                check_arrow(barcode_builder.Append(whitelist[source]),
                            "cannot append barcode");
                check_arrow(prior_builder.Append(result.exact_barcode_priors[source]),
                            "cannot append barcode prior");
                barcode_digest.integer<std::uint32_t>(static_cast<std::uint32_t>(index));
                barcode_digest.string(whitelist[source]);
                barcode_digest.integer<std::uint64_t>(result.exact_barcode_priors[source]);
            }
            barcode_sink.write({finish_builder(index_builder), finish_builder(barcode_builder),
                                finish_builder(prior_builder)},
                               static_cast<std::int64_t>(end - begin));
        }
        barcode_sink.close();
        receipt.tables.emplace(
            "barcodes", table_identity(staging, "parquet/barcodes.parquet",
                                        barcode_sink.rows(), barcode_digest.finish()));

        const auto feature_schema = arrow::schema({
            arrow::field("feature_index", arrow::uint32(), false),
            arrow::field("gene_id", arrow::utf8(), false),
            arrow::field("gene_name", arrow::utf8(), true),
            arrow::field("gene_type", arrow::utf8(), true),
        });
        LogicalDigest feature_digest("features.parquet");
        ParquetSink feature_sink(staging / "parquet/features.parquet", feature_schema,
                                 options.parquet_row_group_rows);
        for (size_t begin = 0; begin < feature_source.size();
             begin += options.parquet_row_group_rows) {
            const size_t end = std::min<size_t>(feature_source.size(),
                                                begin + options.parquet_row_group_rows);
            arrow::UInt32Builder index_builder;
            arrow::StringBuilder id_builder;
            arrow::StringBuilder name_builder;
            arrow::StringBuilder type_builder;
            for (size_t index = begin; index < end; ++index) {
                const std::string& gene_id = features[feature_source[index]];
                const GeneFact& fact = facts.gene(gene_id);
                const std::optional<std::string> gene_type =
                    fact.gene_types.empty() ? std::nullopt
                                            : std::optional<std::string>(join(fact.gene_types, ","));
                check_arrow(index_builder.Append(static_cast<std::uint32_t>(index)),
                            "cannot append feature index");
                check_arrow(id_builder.Append(gene_id), "cannot append gene id");
                check_arrow(fact.gene_name ? name_builder.Append(*fact.gene_name)
                                           : name_builder.AppendNull(),
                            "cannot append gene name");
                check_arrow(gene_type ? type_builder.Append(*gene_type)
                                      : type_builder.AppendNull(),
                            "cannot append gene type");
                feature_digest.integer<std::uint32_t>(static_cast<std::uint32_t>(index));
                feature_digest.string(gene_id);
                feature_digest.nullable(fact.gene_name);
                feature_digest.nullable(gene_type);
            }
            feature_sink.write({finish_builder(index_builder), finish_builder(id_builder),
                                finish_builder(name_builder), finish_builder(type_builder)},
                               static_cast<std::int64_t>(end - begin));
        }
        feature_sink.close();
        receipt.tables.emplace(
            "features", table_identity(staging, "parquet/features.parquet",
                                        feature_sink.rows(), feature_digest.finish()));

        const auto count_schema = arrow::schema({
            arrow::field("profile_id", arrow::utf8(), false),
            arrow::field("barcode_index", arrow::uint32(), false),
            arrow::field("feature_index", arrow::uint32(), false),
            arrow::field("umi_count", arrow::uint64(), false),
        });
        LogicalDigest count_digest("counts.parquet-decoded-v1");
        ParquetSink count_sink(staging / "parquet/counts.parquet", count_schema,
                               options.parquet_row_group_rows);
        for (size_t begin = 0; begin < result.counts.size();
             begin += options.parquet_row_group_rows) {
            const size_t end = std::min<size_t>(result.counts.size(),
                                                begin + options.parquet_row_group_rows);
            arrow::StringBuilder profile_builder;
            arrow::UInt32Builder barcode_builder;
            arrow::UInt32Builder feature_builder;
            arrow::UInt64Builder count_builder;
            for (size_t index = begin; index < end; ++index) {
                const NumericCountRecord& row = result.counts[index];
                if (row.profile_index >= profiles.size()) {
                    throw std::logic_error("count row profile index is outside selected profiles");
                }
                const std::string& profile_id = profiles[row.profile_index].profile.id_string;
                const std::string& barcode_id =
                    whitelist.at(row.barcode_whitelist_index);
                const std::string& gene_id = features.at(row.feature_catalog_index);
                const std::uint32_t barcode = barcode_remap.at(row.barcode_whitelist_index);
                const std::uint32_t feature = feature_remap.at(row.feature_catalog_index);
                check_arrow(profile_builder.Append(profile_id), "cannot append count profile");
                check_arrow(barcode_builder.Append(barcode), "cannot append count barcode");
                check_arrow(feature_builder.Append(feature), "cannot append count feature");
                check_arrow(count_builder.Append(row.umi_count), "cannot append UMI count");
                count_digest.string(profile_id);
                count_digest.string(barcode_id);
                count_digest.string(gene_id);
                count_digest.integer<std::uint64_t>(row.umi_count);
                profile_count_digests[row.profile_index]->string(barcode_id);
                profile_count_digests[row.profile_index]->string(gene_id);
                profile_count_digests[row.profile_index]->integer<std::uint64_t>(
                    row.umi_count);
                ++profile_count_rows[row.profile_index];
            }
            count_sink.write({finish_builder(profile_builder), finish_builder(barcode_builder),
                              finish_builder(feature_builder), finish_builder(count_builder)},
                             static_cast<std::int64_t>(end - begin));
        }
        count_sink.close();
        receipt.tables.emplace(
            "counts", table_identity(staging, "parquet/counts.parquet", count_sink.rows(),
                                      count_digest.finish()));
        std::vector<std::string> profile_count_hashes;
        profile_count_hashes.reserve(profiles.size());
        for (auto& digest : profile_count_digests) {
            profile_count_hashes.push_back(digest->finish());
        }

        const auto molecule_schema = arrow::schema({
            arrow::field("profile_id", arrow::utf8(), false),
            arrow::field("barcode_index", arrow::uint32(), false),
            arrow::field("feature_index", arrow::uint32(), false),
            arrow::field("corrected_umi", arrow::utf8(), false),
            arrow::field("supporting_read_count", arrow::uint64(), false),
            arrow::field("raw_umis_collapsed", arrow::uint64(), false),
        });
        LogicalDigest molecule_digest("molecules.parquet-decoded-v1");
        ParquetSink molecule_sink(staging / "parquet/molecules.parquet", molecule_schema,
                                  options.parquet_row_group_rows);
        for (size_t begin = 0; begin < result.molecules.size();
             begin += options.parquet_row_group_rows) {
            const size_t end = std::min<size_t>(result.molecules.size(),
                                                begin + options.parquet_row_group_rows);
            arrow::StringBuilder profile_builder;
            arrow::UInt32Builder barcode_builder;
            arrow::UInt32Builder feature_builder;
            arrow::StringBuilder umi_builder;
            arrow::UInt64Builder support_builder;
            arrow::UInt64Builder raw_builder;
            for (size_t index = begin; index < end; ++index) {
                const NumericMoleculeRecord& row = result.molecules[index];
                if (row.profile_index >= profiles.size()) {
                    throw std::logic_error("molecule row profile index is outside selected profiles");
                }
                const std::string& profile_id = profiles[row.profile_index].profile.id_string;
                const std::string& barcode_id =
                    whitelist.at(row.barcode_whitelist_index);
                const std::string& gene_id = features.at(row.feature_catalog_index);
                const std::uint32_t barcode = barcode_remap.at(row.barcode_whitelist_index);
                const std::uint32_t feature = feature_remap.at(row.feature_catalog_index);
                check_arrow(profile_builder.Append(profile_id), "cannot append molecule profile");
                check_arrow(barcode_builder.Append(barcode), "cannot append molecule barcode");
                check_arrow(feature_builder.Append(feature), "cannot append molecule feature");
                check_arrow(umi_builder.Append(row.corrected_umi), "cannot append corrected UMI");
                check_arrow(support_builder.Append(row.supporting_reads),
                            "cannot append supporting reads");
                check_arrow(raw_builder.Append(row.raw_umis_collapsed),
                            "cannot append raw UMI count");
                molecule_digest.string(profile_id);
                molecule_digest.string(barcode_id);
                molecule_digest.string(gene_id);
                molecule_digest.string(row.corrected_umi);
                molecule_digest.integer<std::uint64_t>(row.supporting_reads);
                molecule_digest.integer<std::uint64_t>(row.raw_umis_collapsed);
                profile_molecule_digests[row.profile_index]->string(barcode_id);
                profile_molecule_digests[row.profile_index]->string(gene_id);
                profile_molecule_digests[row.profile_index]->string(row.corrected_umi);
                profile_molecule_digests[row.profile_index]->integer<std::uint64_t>(
                    row.supporting_reads);
                profile_molecule_digests[row.profile_index]->integer<std::uint64_t>(
                    row.raw_umis_collapsed);
                ++profile_molecule_rows[row.profile_index];
            }
            molecule_sink.write({finish_builder(profile_builder), finish_builder(barcode_builder),
                                 finish_builder(feature_builder), finish_builder(umi_builder),
                                 finish_builder(support_builder), finish_builder(raw_builder)},
                                static_cast<std::int64_t>(end - begin));
        }
        molecule_sink.close();
        receipt.tables.emplace(
            "molecules", table_identity(staging, "parquet/molecules.parquet",
                                         molecule_sink.rows(), molecule_digest.finish()));
        for (size_t profile_index = 0; profile_index < profiles.size(); ++profile_index) {
            receipt.profile_logical_tables.emplace(
                profiles[profile_index].profile.id_string,
                CountProfileLogicalIdentity{
                    profile_count_rows[profile_index],
                    profile_count_hashes[profile_index],
                    profile_molecule_rows[profile_index],
                    profile_molecule_digests[profile_index]->finish(),
                });
        }

        if (options.write_10x_mex) {
            for (size_t profile_index = 0; profile_index < profiles.size(); ++profile_index) {
                const std::string& profile_id = profiles[profile_index].profile.id_string;
                const std::filesystem::path relative =
                    std::filesystem::path("mex") / profile_id / "raw_feature_bc_matrix";
                const std::filesystem::path root = staging / relative;
                std::filesystem::create_directories(root);

                GzipOutput barcodes(root / "barcodes.tsv.gz");
                for (const std::uint32_t source : barcode_source) {
                    barcodes.write(whitelist[source] + "\n");
                }
                receipt.additional_outputs.emplace(
                    profile_id + "/barcodes",
                    table_identity(staging, (relative / "barcodes.tsv.gz").string(),
                                   barcode_source.size(), barcodes.close()));

                GzipOutput feature_file(root / "features.tsv.gz");
                for (const std::uint32_t source : feature_source) {
                    const std::string& gene_id = features[source];
                    const GeneFact& fact = facts.gene(gene_id);
                    feature_file.write(gene_id + "\t" + fact.gene_name.value_or(gene_id) +
                                       "\tGene Expression\n");
                }
                receipt.additional_outputs.emplace(
                    profile_id + "/features",
                    table_identity(staging, (relative / "features.tsv.gz").string(),
                                   feature_source.size(), feature_file.close()));

                const std::uint64_t nonzero = static_cast<std::uint64_t>(std::count_if(
                    result.counts.begin(), result.counts.end(),
                    [&](const NumericCountRecord& row) {
                        return row.profile_index == profile_index;
                    }));
                GzipOutput matrix(root / "matrix.mtx.gz");
                matrix.write("%%MatrixMarket matrix coordinate integer general\n%\n");
                matrix.write(std::to_string(feature_source.size()) + " " +
                             std::to_string(barcode_source.size()) + " " +
                             std::to_string(nonzero) + "\n");
                for (const NumericCountRecord& row : result.counts) {
                    if (row.profile_index == profile_index) {
                        matrix.write(std::to_string(feature_remap.at(row.feature_catalog_index) + 1) +
                                     " " +
                                     std::to_string(barcode_remap.at(row.barcode_whitelist_index) + 1) +
                                     " " + std::to_string(row.umi_count) + "\n");
                    }
                }
                receipt.additional_outputs.emplace(
                    profile_id + "/matrix",
                    table_identity(staging, (relative / "matrix.mtx.gz").string(),
                                   nonzero, matrix.close()));
            }
        }

        if (std::filesystem::is_regular_file(staging / "map.rad")) {
            const std::string digest = sha256_file(staging / "map.rad");
            receipt.additional_outputs.emplace(
                "rad", table_identity(staging, "map.rad", options.rad_records, digest));
        } else if (options.rad_records != 0) {
            throw std::logic_error("RAD record count was supplied without a staged RAD file");
        }
        if (options.read_assignments) {
            const CountTableIdentity& identity = *options.read_assignments;
            const std::filesystem::path relative(identity.relative_path);
            if (relative.empty() || relative.is_absolute() ||
                relative.lexically_normal() != relative ||
                std::any_of(relative.begin(), relative.end(), [](const auto& component) {
                    return component == "." || component == "..";
                })) {
                throw std::invalid_argument(
                    "read-assignment identity must name a normalized staged relative path");
            }
            const std::filesystem::path path = staging / identity.relative_path;
            if (!std::filesystem::is_regular_file(path) ||
                std::filesystem::file_size(path) != identity.size_bytes ||
                sha256_file(path) != identity.output_sha256) {
                throw std::runtime_error(
                    "staged read-assignment Parquet failed output identity validation");
            }
            receipt.additional_outputs.emplace("read_assignments", identity);
        }

        std::ostringstream summary;
        summary << "scope\tprofile_id\tmetric\tvalue\n";
        const CountRuntimeCounters& counters = result.counters;
        const std::array<std::pair<std::string_view, std::uint64_t>, 25> global{{
            {"input_records", options.input_records},
            {"input_read_groups", options.input_read_groups},
            {"raw_molecule_missing_groups", options.raw_molecule_missing_groups},
            {"raw_molecule_malformed_groups", options.raw_molecule_malformed_groups},
            {"raw_molecule_unsupported_groups", options.raw_molecule_unsupported_groups},
            {"raw_molecule_skipped_groups", options.raw_molecule_skipped_groups},
            {"barcode_prior_groups", counters.barcode_prior_groups},
            {"exact_barcode_prior_groups", counters.exact_barcode_prior_groups},
            {"assigned_observations", counters.assigned_observations},
            {"direct_multigene_dropped", counters.direct_multigene_dropped},
            {"invalid_umi_dropped", counters.invalid_umi_dropped},
            {"umi_n_dropped", counters.umi_n_dropped},
            {"umi_homopolymer_dropped", counters.umi_homopolymer_dropped},
            {"off_whitelist_uncorrectable_dropped", counters.off_whitelist_uncorrectable_dropped},
            {"barcode_correction_succeeded", counters.barcode_correction_succeeded},
            {"barcode_correction_failed", counters.barcode_correction_failed},
            {"aggregate_spill_runs", counters.aggregate_spill_runs},
            {"aggregate_spill_rows", counters.aggregate_spill_rows},
            {"aggregate_spill_bytes", counters.aggregate_spill_bytes},
            {"assignment_cache_capacity", options.assignment_cache_capacity},
            {"assignment_cache_entries", options.assignment_cache_entries},
            {"assignment_cache_hits", options.assignment_cache_hits},
            {"assignment_cache_misses", options.assignment_cache_misses},
            {"assignment_cache_uncached", options.assignment_cache_uncached},
            {"active_barcodes", barcode_source.size()},
        }};
        for (const auto& [name, value] : global) {
            summary << "global\t.\t" << name << '\t' << value << '\n';
        }
        for (size_t profile_index = 0; profile_index < profiles.size(); ++profile_index) {
            const std::uint64_t count_rows = std::count_if(
                result.counts.begin(), result.counts.end(), [&](const NumericCountRecord& row) {
                    return row.profile_index == profile_index;
                });
            const std::uint64_t molecule_rows = std::count_if(
                result.molecules.begin(), result.molecules.end(),
                [&](const NumericMoleculeRecord& row) {
                    return row.profile_index == profile_index;
                });
            summary << "profile\t" << profiles[profile_index].profile.id_string
                    << "\tcount_rows\t" << count_rows << '\n'
                    << "profile\t" << profiles[profile_index].profile.id_string
                    << "\tmolecule_rows\t" << molecule_rows << '\n';
            for (const auto& [name, value] :
                 profile_runtime_metrics(result.profile_counters.at(profile_index))) {
                summary << "profile\t" << profiles[profile_index].profile.id_string
                        << '\t' << name << '\t' << value << '\n';
            }
            for (size_t terminal = 0; terminal < kAssignmentTerminalCount; ++terminal) {
                const std::uint64_t value = options.profile_assignment_terminals.empty()
                    ? 0
                    : options.profile_assignment_terminals.at(profile_index).at(terminal);
                summary << "profile\t" << profiles[profile_index].profile.id_string << '\t'
                        << assignment_terminal_name(
                               static_cast<AssignmentTerminal>(terminal))
                        << '\t' << value << '\n';
            }
        }
        write_file(staging / "summary.tsv", summary.str());
        receipt.additional_outputs.emplace(
            "summary", table_identity(staging, "summary.tsv",
                                      global.size() + profiles.size() *
                                          (2 + kProfileRuntimeMetricCount +
                                           kAssignmentTerminalCount),
                                      sha256_file(staging / "summary.tsv")));

        std::vector<CountInputIdentity> inputs = options.inputs;
        std::sort(inputs.begin(), inputs.end(), [](const CountInputIdentity& left,
                                                   const CountInputIdentity& right) {
            return left.role < right.role;
        });
        if (std::adjacent_find(inputs.begin(), inputs.end(),
                               [](const CountInputIdentity& left,
                                  const CountInputIdentity& right) {
                                   return left.role == right.role;
                               }) != inputs.end()) {
            throw std::invalid_argument("direct-count input roles must be unique");
        }
        std::ostringstream manifest;
        manifest << "{\"analysis_scope\":" << json_string(options.analysis_scope)
                 << ",\"assignment_cache\":{\"capacity\":"
                 << options.assignment_cache_capacity << ",\"entries\":"
                 << options.assignment_cache_entries << ",\"hits\":"
                 << options.assignment_cache_hits << ",\"misses\":"
                 << options.assignment_cache_misses << ",\"uncached\":"
                 << options.assignment_cache_uncached << "},\"command_line\":"
                 << json_string(options.command_line)
                 << ",\"compatibility_bundle\":";
        if (options.compatibility_bundle) {
            const CountCompatibilityIdentity& compatibility =
                *options.compatibility_bundle;
            manifest << "{\"content_id\":"
                     << json_string(compatibility.content_id)
                     << ",\"manifest_path\":"
                     << json_string(compatibility.manifest_path.string())
                     << ",\"manifest_sha256\":"
                     << json_string(compatibility.manifest_sha256)
                     << ",\"manifest_size_bytes\":"
                     << compatibility.manifest_size_bytes << ",\"mode\":"
                     << json_string(compatibility.mode) << '}';
        } else {
            manifest << "null";
        }
        manifest << ",\"counters\":{\"global\":{";
        for (size_t index = 0; index < global.size(); ++index) {
            if (index != 0) manifest << ',';
            manifest << json_string(global[index].first) << ':' << global[index].second;
        }
        manifest << "},\"profiles\":{";
        for (size_t profile_index = 0; profile_index < profiles.size(); ++profile_index) {
            if (profile_index != 0) manifest << ',';
            manifest << json_string(profiles[profile_index].profile.id_string) << ":{";
            for (size_t terminal = 0; terminal < kAssignmentTerminalCount; ++terminal) {
                if (terminal != 0) manifest << ',';
                const std::uint64_t value = options.profile_assignment_terminals.empty()
                    ? 0
                    : options.profile_assignment_terminals.at(profile_index).at(terminal);
                manifest << json_string(assignment_terminal_name(
                                static_cast<AssignmentTerminal>(terminal)))
                         << ':' << value;
            }
            for (const auto& [name, value] :
                 profile_runtime_metrics(result.profile_counters.at(profile_index))) {
                manifest << ',' << json_string(name) << ':' << value;
            }
            manifest << '}';
        }
        manifest << "}}"
                 << ",\"fact_bundle_content_id\":"
                 << json_string(options.fact_bundle_content_id)
                 << ",\"libraries\":{\"arrow\":\"" ARROW_VERSION_STRING
                    "\",\"parquet\":\"17.0.0\",\"zstd\":"
                 << json_string(ZSTD_versionString()) << "},\"inputs\":{";
        for (size_t index = 0; index < inputs.size(); ++index) {
            if (index != 0) manifest << ',';
            manifest << json_string(inputs[index].role) << ":{\"path\":"
                     << json_string(inputs[index].path.string()) << ",\"sha256\":"
                     << json_string(inputs[index].sha256) << ",\"size_bytes\":"
                     << inputs[index].size_bytes << '}';
        }
        manifest << "},\"memory_budget_bytes\":" << options.memory_budget_bytes
                 << ",\"outputs\":{";
        bool first = true;
        for (const auto& [name, identity] : receipt.additional_outputs) {
            if (!first) manifest << ',';
            first = false;
            manifest << json_string(name) << ':';
            append_identity_json(manifest, identity);
        }
        manifest << "},\"pancollapse_version\":" << json_string(options.pancollapse_version)
                 << ",\"profiles\":[";
        for (size_t index = 0; index < profiles.size(); ++index) {
            if (index != 0) manifest << ',';
            const EffectiveProfile& profile = profiles[index];
            const CountProfileLogicalIdentity& logical =
                receipt.profile_logical_tables.at(profile.profile.id_string);
            manifest << "{\"effective_sha256\":" << json_string(profile.effective_sha256)
                     << ",\"id\":" << json_string(profile.profile.id_string)
                     << ",\"logical_tables\":{\"counts\":{\"rows\":"
                     << logical.count_rows << ",\"sha256\":"
                     << json_string(logical.counts_sha256)
                     << "},\"molecules\":{\"rows\":" << logical.molecule_rows
                     << ",\"sha256\":" << json_string(logical.molecules_sha256)
                     << "}}"
                     << ",\"overrides\":[";
            for (size_t override_index = 0; override_index < profile.overrides.size();
                 ++override_index) {
                if (override_index != 0) manifest << ',';
                manifest << "{\"field\":"
                         << json_string(profile.overrides[override_index].canonical_field)
                         << ",\"value\":"
                         << json_string(profile.overrides[override_index].canonical_value) << '}';
            }
            manifest << "]}";
        }
        manifest << "],\"schema\":\"pancollapse-count-output-v1\",\"tables\":{";
        first = true;
        for (const auto& [name, identity] : receipt.tables) {
            if (!first) manifest << ',';
            first = false;
            manifest << json_string(name) << ':';
            append_identity_json(manifest, identity);
        }
        manifest << "},\"threads\":" << options.threads << ",\"timings\":{"
                 << "\"initialization_seconds\":" << std::fixed << std::setprecision(9)
                 << options.initialization_seconds << ",\"processing_seconds\":"
                 << options.processing_seconds << "}}\n";
        write_file(staging / "manifest.json", manifest.str());

        std::filesystem::rename(staging, options.output_directory);
        return receipt;
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove_all(staging, ignored);
        throw;
    }
}

}  // namespace pancollapse::direct_count
