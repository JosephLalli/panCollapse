#include "direct_count_runtime.hpp"

#include <zstd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <condition_variable>
#include <fstream>
#include <limits>
#include <map>
#include <mutex>
#include <queue>
#include <set>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace pancollapse::direct_count {
namespace {

constexpr size_t kShardCount = 256;
constexpr std::uint64_t kExactEntryBytes = 96;
constexpr std::uint64_t kDeferredEntryBytes = 192;
constexpr std::array<char, 8> kSpillMagic{'P', 'C', 'S', 'P', 'I', 'L', 'L', '1'};

void require_zstd(size_t result, std::string_view operation) {
    if (ZSTD_isError(result)) {
        throw std::runtime_error(std::string(operation) + ": " + ZSTD_getErrorName(result));
    }
}

class ZstdOutput {
  public:
    explicit ZstdOutput(const std::filesystem::path& path) : output_(path, std::ios::binary) {
        if (!output_) {
            throw std::runtime_error("cannot open aggregate spill output " + path.string());
        }
        context_ = ZSTD_createCCtx();
        if (context_ == nullptr) {
            throw std::runtime_error("cannot allocate ZSTD compression context");
        }
        require_zstd(ZSTD_CCtx_setParameter(context_, ZSTD_c_compressionLevel, 3),
                     "cannot set ZSTD compression level");
        require_zstd(ZSTD_CCtx_setParameter(context_, ZSTD_c_checksumFlag, 1),
                     "cannot enable ZSTD checksum");
    }

    ZstdOutput(const ZstdOutput&) = delete;
    ZstdOutput& operator=(const ZstdOutput&) = delete;

    ~ZstdOutput() {
        if (context_ != nullptr) {
            ZSTD_freeCCtx(context_);
        }
    }

    void write(const void* data, size_t size) {
        ZSTD_inBuffer input{data, size, 0};
        while (input.pos < input.size) {
            ZSTD_outBuffer output{buffer_.data(), buffer_.size(), 0};
            require_zstd(ZSTD_compressStream2(context_, &output, &input, ZSTD_e_continue),
                         "cannot compress aggregate spill");
            output_.write(buffer_.data(), static_cast<std::streamsize>(output.pos));
            if (!output_) {
                throw std::runtime_error("cannot write aggregate spill");
            }
        }
    }

    void finish() {
        size_t remaining = 1;
        while (remaining != 0) {
            ZSTD_inBuffer input{nullptr, 0, 0};
            ZSTD_outBuffer output{buffer_.data(), buffer_.size(), 0};
            remaining = ZSTD_compressStream2(context_, &output, &input, ZSTD_e_end);
            require_zstd(remaining, "cannot finalize aggregate spill");
            output_.write(buffer_.data(), static_cast<std::streamsize>(output.pos));
        }
        output_.close();
        if (!output_) {
            throw std::runtime_error("cannot close aggregate spill");
        }
        finished_ = true;
    }

  private:
    std::ofstream output_;
    ZSTD_CCtx* context_ = nullptr;
    std::array<char, 1 << 20> buffer_{};
    bool finished_ = false;
};

class ZstdInput {
  public:
    explicit ZstdInput(const std::filesystem::path& path) : input_(path, std::ios::binary) {
        if (!input_) {
            throw std::runtime_error("cannot open aggregate spill input " + path.string());
        }
        context_ = ZSTD_createDCtx();
        if (context_ == nullptr) {
            throw std::runtime_error("cannot allocate ZSTD decompression context");
        }
    }

    ZstdInput(const ZstdInput&) = delete;
    ZstdInput& operator=(const ZstdInput&) = delete;

    ~ZstdInput() {
        if (context_ != nullptr) {
            ZSTD_freeDCtx(context_);
        }
    }

    void read(void* destination, size_t size) {
        char* target = static_cast<char*>(destination);
        size_t copied = 0;
        while (copied < size) {
            if (decoded_at_ == decoded_size_) {
                refill();
            }
            if (decoded_size_ == 0) {
                throw std::runtime_error("aggregate spill ended before its declared row count");
            }
            const size_t available = decoded_size_ - decoded_at_;
            const size_t take = std::min(available, size - copied);
            std::copy_n(decoded_.data() + decoded_at_, take, target + copied);
            decoded_at_ += take;
            copied += take;
        }
    }

    // A spill run is an internal transactional artifact: accepting all of its
    // declared rows is not sufficient unless the enclosing ZSTD frame also
    // reaches its authenticated end.  In particular, the frame checksum lives
    // after the final decoded byte and would otherwise never be examined.
    void finish() {
        if (finished_) {
            return;
        }
        if (decoded_at_ != decoded_size_) {
            throw std::runtime_error(
                "aggregate spill contains data beyond its declared row count");
        }
        while (!frame_complete_) {
            if (!load_encoded()) {
                throw std::runtime_error(
                    "aggregate spill is truncated before the ZSTD frame end");
            }
            ZSTD_inBuffer input{encoded_.data(), encoded_size_, encoded_at_};
            ZSTD_outBuffer output{decoded_.data(), decoded_.size(), 0};
            const size_t remaining = ZSTD_decompressStream(context_, &output, &input);
            require_zstd(remaining, "cannot decompress aggregate spill");
            encoded_at_ = input.pos;
            if (output.pos != 0) {
                throw std::runtime_error(
                    "aggregate spill contains data beyond its declared row count");
            }
            frame_complete_ = remaining == 0;
        }
        if (encoded_at_ != encoded_size_) {
            throw std::runtime_error(
                "aggregate spill has trailing bytes or multiple ZSTD frames");
        }
        char trailing = 0;
        input_.read(&trailing, 1);
        if (input_.gcount() != 0) {
            throw std::runtime_error(
                "aggregate spill has trailing bytes or multiple ZSTD frames");
        }
        if (input_.bad()) {
            throw std::runtime_error("cannot finish reading aggregate spill");
        }
        finished_ = true;
    }

  private:
    bool load_encoded() {
        if (encoded_at_ != encoded_size_) {
            return true;
        }
        input_.read(encoded_.data(), static_cast<std::streamsize>(encoded_.size()));
        encoded_size_ = static_cast<size_t>(input_.gcount());
        encoded_at_ = 0;
        if (input_.bad()) {
            throw std::runtime_error("cannot read aggregate spill");
        }
        return encoded_size_ != 0;
    }

    void refill() {
        decoded_at_ = 0;
        decoded_size_ = 0;
        while (decoded_size_ == 0 && !frame_complete_) {
            if (!load_encoded()) {
                return;
            }
            ZSTD_inBuffer input{encoded_.data(), encoded_size_, encoded_at_};
            ZSTD_outBuffer output{decoded_.data(), decoded_.size(), 0};
            const size_t remaining = ZSTD_decompressStream(context_, &output, &input);
            require_zstd(remaining, "cannot decompress aggregate spill");
            encoded_at_ = input.pos;
            decoded_size_ = output.pos;
            frame_complete_ = remaining == 0;
        }
    }

    std::ifstream input_;
    ZSTD_DCtx* context_ = nullptr;
    std::array<char, 1 << 20> encoded_{};
    std::array<char, 1 << 20> decoded_{};
    size_t encoded_at_ = 0;
    size_t encoded_size_ = 0;
    size_t decoded_at_ = 0;
    size_t decoded_size_ = 0;
    bool frame_complete_ = false;
    bool finished_ = false;
};

void write_u8(ZstdOutput& output, std::uint8_t value) {
    output.write(&value, sizeof(value));
}

void write_u32(ZstdOutput& output, std::uint32_t value) {
    std::array<unsigned char, 4> bytes{};
    for (size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<unsigned char>(value >> (8 * index));
    }
    output.write(bytes.data(), bytes.size());
}

void write_u64(ZstdOutput& output, std::uint64_t value) {
    std::array<unsigned char, 8> bytes{};
    for (size_t index = 0; index < bytes.size(); ++index) {
        bytes[index] = static_cast<unsigned char>(value >> (8 * index));
    }
    output.write(bytes.data(), bytes.size());
}

std::uint8_t read_u8(ZstdInput& input) {
    std::uint8_t result = 0;
    input.read(&result, sizeof(result));
    return result;
}

std::uint32_t read_u32(ZstdInput& input) {
    std::array<unsigned char, 4> bytes{};
    input.read(bytes.data(), bytes.size());
    std::uint32_t result = 0;
    for (size_t index = 0; index < bytes.size(); ++index) {
        result |= static_cast<std::uint32_t>(bytes[index]) << (8 * index);
    }
    return result;
}

std::uint64_t read_u64(ZstdInput& input) {
    std::array<unsigned char, 8> bytes{};
    input.read(bytes.data(), bytes.size());
    std::uint64_t result = 0;
    for (size_t index = 0; index < bytes.size(); ++index) {
        result |= static_cast<std::uint64_t>(bytes[index]) << (8 * index);
    }
    return result;
}

void write_string(ZstdOutput& output, const std::string& value) {
    if (value.size() > std::numeric_limits<std::uint8_t>::max()) {
        throw std::runtime_error("aggregate spill string exceeds uint8 length");
    }
    write_u8(output, static_cast<std::uint8_t>(value.size()));
    output.write(value.data(), value.size());
}

std::string read_string(ZstdInput& input) {
    std::string result(read_u8(input), '\0');
    if (!result.empty()) {
        input.read(result.data(), result.size());
    }
    return result;
}

std::string canonical_barcode_quality(std::string_view quality) {
    std::string result;
    result.reserve(quality.size());
    for (const unsigned char encoded : quality) {
        const int phred = std::clamp(static_cast<int>(encoded) - 33, 0, 33);
        result.push_back(static_cast<char>(phred + 33));
    }
    return result;
}

struct ExactKey {
    std::uint32_t profile = 0;
    std::uint32_t barcode = 0;
    std::uint32_t feature = 0;
    std::string umi;

    auto tuple() const { return std::tie(profile, barcode, feature, umi); }
    bool operator==(const ExactKey& other) const { return tuple() == other.tuple(); }
    bool operator<(const ExactKey& other) const { return tuple() < other.tuple(); }
};

enum class PostBarcodeDisposition : std::uint8_t {
    unique = 0,
    early_exit = 1,
    no_gene = 2,
    direct_multigene = 3,
    umi_n = 4,
    umi_homopolymer = 5,
    invalid_umi = 6,
};

struct DeferredKey {
    std::uint32_t profile = 0;
    std::string raw_barcode;
    std::string barcode_quality;
    PostBarcodeDisposition disposition = PostBarcodeDisposition::unique;
    std::uint32_t feature = 0;
    std::string umi;

    auto tuple() const {
        return std::tie(profile, raw_barcode, barcode_quality, disposition, feature, umi);
    }
    bool operator==(const DeferredKey& other) const { return tuple() == other.tuple(); }
    bool operator<(const DeferredKey& other) const { return tuple() < other.tuple(); }
};

template<class Key>
struct KeyHash;

template<>
struct KeyHash<ExactKey> {
    size_t operator()(const ExactKey& key) const {
        size_t hash = key.profile;
        auto combine = [&](size_t value) {
            hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        };
        combine(key.barcode);
        combine(key.feature);
        combine(std::hash<std::string>{}(key.umi));
        return hash;
    }
};

template<>
struct KeyHash<DeferredKey> {
    size_t operator()(const DeferredKey& key) const {
        size_t hash = key.profile;
        auto combine = [&](size_t value) {
            hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
        };
        combine(std::hash<std::string>{}(key.raw_barcode));
        combine(std::hash<std::string>{}(key.barcode_quality));
        combine(static_cast<std::uint8_t>(key.disposition));
        combine(key.feature);
        combine(std::hash<std::string>{}(key.umi));
        return hash;
    }
};

template<class Key>
struct AggregateShard {
    std::mutex mutex;
    std::unordered_map<Key, std::uint64_t, KeyHash<Key>> values;
};

struct ExactRow {
    ExactKey key;
    std::uint64_t count = 0;
};

struct DeferredRow {
    DeferredKey key;
    std::uint64_t count = 0;
};

struct CollapsedCandidate {
    std::string umi;
    std::uint32_t feature = 0;
    std::uint64_t supporting_reads = 0;
    std::uint64_t raw_umis = 0;
    std::uint64_t original_label_reads = 0;

    auto tuple() const { return std::tie(umi, feature); }
};

bool same_key(const ExactRow& left, const ExactRow& right) {
    return left.key == right.key;
}

bool same_key(const DeferredRow& left, const DeferredRow& right) {
    return left.key == right.key;
}

template<class Row>
void sort_and_combine(std::vector<Row>& rows) {
    std::sort(rows.begin(), rows.end(),
              [](const Row& left, const Row& right) { return left.key < right.key; });
    size_t output = 0;
    for (Row& row : rows) {
        if (output != 0 && same_key(rows[output - 1], row)) {
            if (std::numeric_limits<std::uint64_t>::max() - rows[output - 1].count < row.count) {
                throw std::runtime_error("aggregate read count overflow");
            }
            rows[output - 1].count += row.count;
        } else {
            if (&rows[output] != &row) {
                rows[output] = std::move(row);
            }
            ++output;
        }
    }
    rows.resize(output);
}

void write_header(ZstdOutput& output, char type, std::uint64_t row_count) {
    output.write(kSpillMagic.data(), kSpillMagic.size());
    output.write(&type, 1);
    write_u64(output, row_count);
}

void require_header(ZstdInput& input, char type, std::uint64_t& row_count) {
    std::array<char, 8> magic{};
    input.read(magic.data(), magic.size());
    char actual_type = 0;
    input.read(&actual_type, 1);
    if (magic != kSpillMagic || actual_type != type) {
        throw std::runtime_error("aggregate spill has an invalid header");
    }
    row_count = read_u64(input);
}

void write_exact_run(const std::filesystem::path& path, std::vector<ExactRow> rows) {
    sort_and_combine(rows);
    ZstdOutput output(path);
    write_header(output, 'E', rows.size());
    for (const ExactRow& row : rows) {
        write_u32(output, row.key.profile);
        write_u32(output, row.key.barcode);
        write_u32(output, row.key.feature);
        write_string(output, row.key.umi);
        write_u64(output, row.count);
    }
    output.finish();
}

void write_deferred_run(const std::filesystem::path& path,
                        std::vector<DeferredRow> rows) {
    sort_and_combine(rows);
    ZstdOutput output(path);
    write_header(output, 'D', rows.size());
    for (const DeferredRow& row : rows) {
        write_u32(output, row.key.profile);
        write_string(output, row.key.raw_barcode);
        write_string(output, row.key.barcode_quality);
        write_u8(output, static_cast<std::uint8_t>(row.key.disposition));
        write_u32(output, row.key.feature);
        write_string(output, row.key.umi);
        write_u64(output, row.count);
    }
    output.finish();
}

void write_collapsed_run(const std::filesystem::path& path,
                         std::vector<CollapsedCandidate> rows) {
    std::sort(rows.begin(), rows.end(), [](const auto& left, const auto& right) {
        return left.tuple() < right.tuple();
    });
    ZstdOutput output(path);
    write_header(output, 'C', rows.size());
    for (const CollapsedCandidate& row : rows) {
        write_string(output, row.umi);
        write_u32(output, row.feature);
        write_u64(output, row.supporting_reads);
        write_u64(output, row.raw_umis);
        write_u64(output, row.original_label_reads);
    }
    output.finish();
}

class ExactRunReader {
  public:
    explicit ExactRunReader(const std::filesystem::path& path) : input_(path) {
        require_header(input_, 'E', remaining_);
    }
    bool next(ExactRow& row) {
        if (remaining_ == 0) {
            input_.finish();
            return false;
        }
        row.key.profile = read_u32(input_);
        row.key.barcode = read_u32(input_);
        row.key.feature = read_u32(input_);
        row.key.umi = read_string(input_);
        row.count = read_u64(input_);
        --remaining_;
        return true;
    }

  private:
    ZstdInput input_;
    std::uint64_t remaining_ = 0;
};

class DeferredRunReader {
  public:
    explicit DeferredRunReader(const std::filesystem::path& path) : input_(path) {
        require_header(input_, 'D', remaining_);
    }
    bool next(DeferredRow& row) {
        if (remaining_ == 0) {
            input_.finish();
            return false;
        }
        row.key.profile = read_u32(input_);
        row.key.raw_barcode = read_string(input_);
        row.key.barcode_quality = read_string(input_);
        const std::uint8_t disposition = read_u8(input_);
        if (disposition >
            static_cast<std::uint8_t>(PostBarcodeDisposition::invalid_umi)) {
            throw std::runtime_error(
                "aggregate spill has an invalid post-barcode disposition");
        }
        row.key.disposition = static_cast<PostBarcodeDisposition>(disposition);
        row.key.feature = read_u32(input_);
        row.key.umi = read_string(input_);
        row.count = read_u64(input_);
        --remaining_;
        return true;
    }

  private:
    ZstdInput input_;
    std::uint64_t remaining_ = 0;
};

class CollapsedRunReader {
  public:
    explicit CollapsedRunReader(const std::filesystem::path& path) : input_(path) {
        require_header(input_, 'C', remaining_);
    }
    bool next(CollapsedCandidate& row) {
        if (remaining_ == 0) {
            input_.finish();
            return false;
        }
        row.umi = read_string(input_);
        row.feature = read_u32(input_);
        row.supporting_reads = read_u64(input_);
        row.raw_umis = read_u64(input_);
        row.original_label_reads = read_u64(input_);
        --remaining_;
        return true;
    }

  private:
    ZstdInput input_;
    std::uint64_t remaining_ = 0;
};

template<class Consumer>
void merge_collapsed_runs(const std::vector<std::filesystem::path>& paths,
                          Consumer consume) {
    struct Cursor {
        std::unique_ptr<CollapsedRunReader> reader;
        CollapsedCandidate row;
    };
    std::vector<Cursor> cursors;
    cursors.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        Cursor cursor;
        cursor.reader = std::make_unique<CollapsedRunReader>(path);
        if (cursor.reader->next(cursor.row)) {
            cursors.push_back(std::move(cursor));
        }
    }
    auto later = [&](size_t left, size_t right) {
        return cursors[right].row.tuple() < cursors[left].row.tuple();
    };
    std::priority_queue<size_t, std::vector<size_t>, decltype(later)> queue(later);
    for (size_t index = 0; index < cursors.size(); ++index) {
        queue.push(index);
    }
    while (!queue.empty()) {
        const size_t index = queue.top();
        queue.pop();
        consume(std::move(cursors[index].row));
        if (cursors[index].reader->next(cursors[index].row)) {
            queue.push(index);
        }
    }
}

template<class Reader, class Row, class Consumer>
void merge_runs(const std::vector<std::filesystem::path>& paths, Consumer consume) {
    struct Cursor {
        std::unique_ptr<Reader> reader;
        Row row;
    };
    std::vector<Cursor> cursors;
    cursors.reserve(paths.size());
    for (const std::filesystem::path& path : paths) {
        Cursor cursor;
        cursor.reader = std::make_unique<Reader>(path);
        if (cursor.reader->next(cursor.row)) {
            cursors.push_back(std::move(cursor));
        }
    }
    auto later = [&](size_t left, size_t right) {
        return cursors[right].row.key < cursors[left].row.key;
    };
    std::priority_queue<size_t, std::vector<size_t>, decltype(later)> queue(later);
    for (size_t index = 0; index < cursors.size(); ++index) {
        queue.push(index);
    }
    while (!queue.empty()) {
        const size_t index = queue.top();
        queue.pop();
        Row combined = cursors[index].row;
        if (cursors[index].reader->next(cursors[index].row)) {
            queue.push(index);
        }
        while (!queue.empty() && same_key(combined, cursors[queue.top()].row)) {
            const size_t duplicate = queue.top();
            queue.pop();
            if (std::numeric_limits<std::uint64_t>::max() - combined.count <
                cursors[duplicate].row.count) {
                throw std::runtime_error("aggregate read count overflow while merging spills");
            }
            combined.count += cursors[duplicate].row.count;
            if (cursors[duplicate].reader->next(cursors[duplicate].row)) {
                queue.push(duplicate);
            }
        }
        consume(std::move(combined));
    }
}

std::optional<std::uint64_t> encode_barcode(std::string_view barcode) {
    if (barcode.size() > 32) {
        return std::nullopt;
    }
    std::uint64_t result = 0;
    for (char base : barcode) {
        result <<= 2;
        switch (base) {
            case 'A':
                break;
            case 'C':
                result |= 1;
                break;
            case 'G':
                result |= 2;
                break;
            case 'T':
                result |= 3;
                break;
            default:
                return std::nullopt;
        }
    }
    return result;
}

}  // namespace

struct CountWorker::Pending {
    bool exact = false;
    ExactKey exact_key;
    DeferredKey deferred_key;
    std::uint64_t count = 0;
};

struct AtomicProfileCountRuntimeCounters {
    std::atomic<std::uint64_t> assigned_observations{0};
    std::atomic<std::uint64_t> direct_multigene_dropped{0};
    std::atomic<std::uint64_t> invalid_umi_dropped{0};
    std::atomic<std::uint64_t> umi_n_dropped{0};
    std::atomic<std::uint64_t> umi_homopolymer_dropped{0};
    std::atomic<std::uint64_t> off_whitelist_uncorrectable_dropped{0};
    std::atomic<std::uint64_t> barcode_correction_succeeded{0};
    std::atomic<std::uint64_t> barcode_correction_failed{0};

    ProfileCountRuntimeCounters snapshot() const {
        return {
            assigned_observations.load(std::memory_order_relaxed),
            direct_multigene_dropped.load(std::memory_order_relaxed),
            invalid_umi_dropped.load(std::memory_order_relaxed),
            umi_n_dropped.load(std::memory_order_relaxed),
            umi_homopolymer_dropped.load(std::memory_order_relaxed),
            off_whitelist_uncorrectable_dropped.load(std::memory_order_relaxed),
            barcode_correction_succeeded.load(std::memory_order_relaxed),
            barcode_correction_failed.load(std::memory_order_relaxed),
        };
    }
};

struct CountRuntime::Impl {
    std::vector<EffectiveProfile> profiles;
    std::vector<std::string> whitelist;
    std::vector<std::string> features;
    CountRuntimeOptions options;
    std::unordered_map<std::uint64_t, std::uint32_t> barcode_index;
    std::unordered_map<std::string, std::uint32_t> feature_index;
    std::unique_ptr<std::atomic<std::uint64_t>[]> exact_priors;
    std::vector<std::unique_ptr<AtomicProfileCountRuntimeCounters>> profile_counters;
    std::array<AggregateShard<ExactKey>, kShardCount> exact_shards;
    std::array<AggregateShard<DeferredKey>, kShardCount> deferred_shards;
    std::atomic<std::uint64_t> approximate_bytes{0};
    std::mutex spill_mutex;
    std::vector<std::filesystem::path> exact_runs;
    std::vector<std::filesystem::path> deferred_runs;
    std::uint64_t next_run = 0;
    bool finalized = false;
    std::vector<std::filesystem::path> collapsed_runs;
    bool owns_spill_directory = false;

    ~Impl() {
        // Best-effort cleanup for a runtime that never reached the end of
        // finalize(): remove every spill run this instance wrote and the spill
        // directory when this instance created it.
        std::error_code ignored;
        for (const std::filesystem::path& path : exact_runs) {
            std::filesystem::remove(path, ignored);
        }
        for (const std::filesystem::path& path : deferred_runs) {
            std::filesystem::remove(path, ignored);
        }
        for (const std::filesystem::path& path : collapsed_runs) {
            std::filesystem::remove(path, ignored);
        }
        if (owns_spill_directory) {
            std::filesystem::remove(options.spill_directory, ignored);
        }
    }

    std::atomic<std::uint64_t> barcode_prior_groups{0};
    std::atomic<std::uint64_t> exact_barcode_prior_groups{0};
    std::atomic<std::uint64_t> assigned_observations{0};
    std::atomic<std::uint64_t> direct_multigene_dropped{0};
    std::atomic<std::uint64_t> invalid_umi_dropped{0};
    std::atomic<std::uint64_t> umi_n_dropped{0};
    std::atomic<std::uint64_t> umi_homopolymer_dropped{0};
    std::atomic<std::uint64_t> off_whitelist_uncorrectable_dropped{0};
    std::atomic<std::uint64_t> barcode_correction_succeeded{0};
    std::atomic<std::uint64_t> barcode_correction_failed{0};
    std::atomic<std::uint64_t> aggregate_spill_runs{0};
    std::atomic<std::uint64_t> aggregate_spill_rows{0};
    std::atomic<std::uint64_t> aggregate_spill_bytes{0};

    void record_post_barcode(std::uint32_t profile,
                             PostBarcodeDisposition disposition,
                             std::uint64_t count) {
        AtomicProfileCountRuntimeCounters& profile_values =
            *profile_counters.at(profile);
        switch (disposition) {
            case PostBarcodeDisposition::unique:
                assigned_observations.fetch_add(count, std::memory_order_relaxed);
                profile_values.assigned_observations.fetch_add(
                    count, std::memory_order_relaxed);
                break;
            case PostBarcodeDisposition::early_exit:
            case PostBarcodeDisposition::no_gene:
                break;
            case PostBarcodeDisposition::direct_multigene:
                direct_multigene_dropped.fetch_add(count, std::memory_order_relaxed);
                profile_values.direct_multigene_dropped.fetch_add(
                    count, std::memory_order_relaxed);
                break;
            case PostBarcodeDisposition::umi_n:
            case PostBarcodeDisposition::umi_homopolymer:
            case PostBarcodeDisposition::invalid_umi:
                invalid_umi_dropped.fetch_add(count, std::memory_order_relaxed);
                profile_values.invalid_umi_dropped.fetch_add(
                    count, std::memory_order_relaxed);
                if (disposition == PostBarcodeDisposition::umi_n) {
                    umi_n_dropped.fetch_add(count, std::memory_order_relaxed);
                    profile_values.umi_n_dropped.fetch_add(
                        count, std::memory_order_relaxed);
                } else if (disposition ==
                           PostBarcodeDisposition::umi_homopolymer) {
                    umi_homopolymer_dropped.fetch_add(
                        count, std::memory_order_relaxed);
                    profile_values.umi_homopolymer_dropped.fetch_add(
                        count, std::memory_order_relaxed);
                }
                break;
        }
    }

    std::optional<std::uint32_t> exact_barcode(std::string_view barcode) const {
        if (barcode.size() != options.raw_barcode_length) {
            return std::nullopt;
        }
        const auto encoded = encode_barcode(barcode);
        if (!encoded) {
            return std::nullopt;
        }
        const auto found = barcode_index.find(*encoded);
        return found == barcode_index.end() ? std::nullopt
                                             : std::optional<std::uint32_t>(found->second);
    }

    std::vector<std::pair<std::uint32_t, size_t>> barcode_candidates(
        std::string_view barcode) const {
        if (barcode.size() != options.raw_barcode_length) {
            return {};
        }
        static constexpr std::array<char, 4> bases{'A', 'C', 'G', 'T'};
        size_t invalid_count = 0;
        size_t invalid_at = 0;
        for (size_t index = 0; index < barcode.size(); ++index) {
            if (std::find(bases.begin(), bases.end(), barcode[index]) == bases.end()) {
                ++invalid_count;
                invalid_at = index;
            }
        }
        if (invalid_count > 1) {
            return {};
        }
        std::vector<std::pair<std::uint32_t, size_t>> result;
        std::string candidate(barcode);
        auto try_candidate = [&](size_t mismatch, char base) {
            candidate.assign(barcode);
            candidate[mismatch] = base;
            const auto encoded = encode_barcode(candidate);
            if (!encoded) {
                return;
            }
            const auto found = barcode_index.find(*encoded);
            if (found != barcode_index.end()) {
                result.emplace_back(found->second, mismatch);
            }
        };
        if (invalid_count == 1) {
            for (char base : bases) {
                try_candidate(invalid_at, base);
            }
        } else {
            for (size_t index = 0; index < barcode.size(); ++index) {
                for (char base : bases) {
                    if (base != barcode[index]) {
                        try_candidate(index, base);
                    }
                }
            }
        }
        std::sort(result.begin(), result.end());
        result.erase(std::unique(result.begin(), result.end()), result.end());
        return result;
    }

    std::optional<std::uint32_t> correct_barcode(std::string_view barcode,
                                                 std::string_view quality) const {
        if (const auto exact = exact_barcode(barcode)) {
            return exact;
        }
        const auto candidates = barcode_candidates(barcode);
        if (candidates.empty()) {
            return std::nullopt;
        }
        double total = 0.0;
        double maximum = -1.0;
        std::uint32_t winner = 0;
        for (const auto& [index, mismatch] : candidates) {
            double weight = static_cast<double>(exact_priors[index].load(
                                std::memory_order_relaxed) + 1);
            if (!quality.empty()) {
                int q = static_cast<unsigned char>(quality[mismatch]) - 33;
                q = std::clamp(q, 0, 33);
                weight *= std::pow(10.0, -static_cast<double>(q) / 10.0);
            }
            total += weight;
            if (weight > maximum) {
                maximum = weight;
                winner = index;
            }
        }
        return maximum >= 0.975 * total ? std::optional<std::uint32_t>(winner)
                                        : std::nullopt;
    }

    void record_spill(const std::filesystem::path& path, size_t rows) {
        ++aggregate_spill_runs;
        aggregate_spill_rows.fetch_add(rows, std::memory_order_relaxed);
        aggregate_spill_bytes.fetch_add(std::filesystem::file_size(path),
                                        std::memory_order_relaxed);
    }

    void spill_locked() {
        std::vector<ExactRow> exact_rows;
        std::vector<DeferredRow> deferred_rows;
        for (auto& shard : exact_shards) {
            std::lock_guard<std::mutex> lock(shard.mutex);
            exact_rows.reserve(exact_rows.size() + shard.values.size());
            for (auto& [key, count] : shard.values) {
                exact_rows.push_back({std::move(key), count});
            }
            shard.values.clear();
        }
        for (auto& shard : deferred_shards) {
            std::lock_guard<std::mutex> lock(shard.mutex);
            deferred_rows.reserve(deferred_rows.size() + shard.values.size());
            for (auto& [key, count] : shard.values) {
                deferred_rows.push_back({std::move(key), count});
            }
            shard.values.clear();
        }
        const std::uint64_t released = exact_rows.size() * kExactEntryBytes +
                                       deferred_rows.size() * kDeferredEntryBytes;
        // Workers can insert into a shard that has already been drained while
        // this spill is walking later shards. Subtract only the entries this
        // spill actually removed so those concurrent insertions remain charged.
        std::uint64_t prior = approximate_bytes.load(std::memory_order_relaxed);
        while (!approximate_bytes.compare_exchange_weak(
            prior, prior > released ? prior - released : 0,
            std::memory_order_relaxed, std::memory_order_relaxed)) {
        }
        if (!exact_rows.empty()) {
            const std::filesystem::path path =
                options.spill_directory /
                ("aggregate-" + std::to_string(next_run++) + ".exact.zst");
            const size_t rows = exact_rows.size();
            exact_runs.push_back(path);
            write_exact_run(path, std::move(exact_rows));
            record_spill(path, rows);
        }
        if (!deferred_rows.empty()) {
            const std::filesystem::path path =
                options.spill_directory /
                ("aggregate-" + std::to_string(next_run++) + ".deferred.zst");
            const size_t rows = deferred_rows.size();
            deferred_runs.push_back(path);
            write_deferred_run(path, std::move(deferred_rows));
            record_spill(path, rows);
        }
    }

    void spill_if_needed(bool force = false) {
        if (!force && approximate_bytes.load(std::memory_order_relaxed) <=
                          options.memory_budget_bytes * 3 / 4) {
            return;
        }
        std::lock_guard<std::mutex> lock(spill_mutex);
        if (force || approximate_bytes.load(std::memory_order_relaxed) >
                         options.memory_budget_bytes * 3 / 4) {
            spill_locked();
        }
    }

    CountRuntimeCounters counters() const {
        return {
            barcode_prior_groups.load(),
            exact_barcode_prior_groups.load(),
            assigned_observations.load(),
            direct_multigene_dropped.load(),
            invalid_umi_dropped.load(),
            umi_n_dropped.load(),
            umi_homopolymer_dropped.load(),
            off_whitelist_uncorrectable_dropped.load(),
            barcode_correction_succeeded.load(),
            barcode_correction_failed.load(),
            aggregate_spill_runs.load(),
            aggregate_spill_rows.load(),
            aggregate_spill_bytes.load(),
        };
    }
};

CountWorker::CountWorker(CountRuntime& runtime, size_t batch_capacity)
    : runtime_(&runtime), batch_capacity_(std::max<size_t>(1, batch_capacity)) {
    pending_.reserve(batch_capacity_);
}

CountWorker::CountWorker(CountWorker&& other) noexcept
    : runtime_(std::exchange(other.runtime_, nullptr)),
      batch_capacity_(other.batch_capacity_), pending_(std::move(other.pending_)) {}

CountWorker& CountWorker::operator=(CountWorker&& other) noexcept {
    if (this != &other) {
        try {
            flush();
        } catch (...) {
        }
        runtime_ = std::exchange(other.runtime_, nullptr);
        batch_capacity_ = other.batch_capacity_;
        pending_ = std::move(other.pending_);
    }
    return *this;
}

CountWorker::~CountWorker() {
    try {
        flush();
    } catch (...) {
    }
}

void CountWorker::observe_barcode(std::string_view raw_barcode) {
    if (runtime_ == nullptr) {
        throw std::logic_error("count worker is not attached to a runtime");
    }
    runtime_->observe_barcode(raw_barcode);
}

void CountWorker::observe_assignment(std::uint32_t profile_index,
                                     std::string_view raw_barcode,
                                     std::optional<std::string_view> barcode_quality,
                                     std::string_view umi,
                                     const std::vector<std::string>& genes,
                                     std::uint64_t read_count,
                                     bool reaches_umi_filter) {
    if (runtime_ == nullptr) {
        throw std::logic_error("count worker is not attached to a runtime");
    }
    auto pending = runtime_->prepare_assignment(profile_index, raw_barcode,
                                                barcode_quality, umi, genes, read_count,
                                                reaches_umi_filter);
    if (!pending) {
        return;
    }
    pending_.push_back(std::move(*pending));
    if (pending_.size() >= batch_capacity_) {
        flush();
    }
}

void CountWorker::flush() {
    if (runtime_ != nullptr && !pending_.empty()) {
        runtime_->merge_worker_batch(pending_);
    }
}

CountRuntime::CountRuntime(std::vector<EffectiveProfile> profiles,
                           std::vector<std::string> whitelist,
                           std::vector<std::string> feature_catalog,
                           CountRuntimeOptions options)
    : impl_(std::make_unique<Impl>()) {
    if (profiles.empty()) {
        throw std::invalid_argument("direct count requires at least one profile");
    }
    if (options.memory_budget_bytes < (1ULL << 20)) {
        throw std::invalid_argument("count memory budget must be at least 1 MiB");
    }
    if (options.raw_barcode_length == 0 || options.raw_barcode_length > 32 ||
        options.raw_umi_length == 0 || options.raw_umi_length > 32) {
        throw std::invalid_argument("raw barcode and UMI lengths must be in 1..32");
    }
    std::sort(profiles.begin(), profiles.end(), [](const EffectiveProfile& left,
                                                   const EffectiveProfile& right) {
        return left.profile.id_string < right.profile.id_string;
    });
    if (std::adjacent_find(profiles.begin(), profiles.end(),
                           [](const EffectiveProfile& left,
                              const EffectiveProfile& right) {
                               return left.profile.id_string == right.profile.id_string;
                           }) != profiles.end()) {
        throw std::invalid_argument("direct count profile IDs must be unique");
    }
    impl_->profiles = std::move(profiles);
    impl_->profile_counters.reserve(impl_->profiles.size());
    for (size_t index = 0; index < impl_->profiles.size(); ++index) {
        impl_->profile_counters.push_back(
            std::make_unique<AtomicProfileCountRuntimeCounters>());
    }
    impl_->options = std::move(options);

    std::sort(whitelist.begin(), whitelist.end());
    if (whitelist.empty() ||
        std::adjacent_find(whitelist.begin(), whitelist.end()) != whitelist.end()) {
        throw std::invalid_argument("barcode whitelist must be nonempty and unique");
    }
    impl_->whitelist = std::move(whitelist);
    impl_->exact_priors =
        std::make_unique<std::atomic<std::uint64_t>[]>(impl_->whitelist.size());
    impl_->barcode_index.reserve(impl_->whitelist.size());
    for (size_t index = 0; index < impl_->whitelist.size(); ++index) {
        const std::string& barcode = impl_->whitelist[index];
        if (barcode.size() != impl_->options.raw_barcode_length) {
            throw std::invalid_argument("barcode whitelist length does not match raw barcode length");
        }
        const auto encoded = encode_barcode(barcode);
        if (!encoded) {
            throw std::invalid_argument("barcode whitelist contains a non-ACGT sequence");
        }
        impl_->exact_priors[index].store(0, std::memory_order_relaxed);
        impl_->barcode_index.emplace(*encoded, static_cast<std::uint32_t>(index));
    }

    std::sort(feature_catalog.begin(), feature_catalog.end());
    if (feature_catalog.empty() ||
        std::adjacent_find(feature_catalog.begin(), feature_catalog.end()) !=
            feature_catalog.end()) {
        throw std::invalid_argument("feature catalog must be nonempty and unique");
    }
    if (feature_catalog.size() > std::numeric_limits<std::uint32_t>::max() ||
        impl_->whitelist.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("direct count dictionaries exceed uint32 indexes");
    }
    impl_->features = std::move(feature_catalog);
    impl_->feature_index.reserve(impl_->features.size());
    for (size_t index = 0; index < impl_->features.size(); ++index) {
        impl_->feature_index.emplace(impl_->features[index], static_cast<std::uint32_t>(index));
    }

    if (impl_->options.spill_directory.empty()) {
        throw std::invalid_argument("direct count requires a spill directory");
    }
    if (std::filesystem::exists(impl_->options.spill_directory) &&
        !std::filesystem::is_empty(impl_->options.spill_directory)) {
        throw std::invalid_argument("direct count spill directory must be absent or empty");
    }
    impl_->owns_spill_directory = !std::filesystem::exists(impl_->options.spill_directory);
    std::filesystem::create_directories(impl_->options.spill_directory);
}

CountRuntime::~CountRuntime() = default;

CountWorker CountRuntime::make_worker(size_t batch_capacity) {
    if (impl_->finalized) {
        throw std::logic_error("cannot create a worker after direct-count finalization");
    }
    return CountWorker(*this, batch_capacity);
}

void CountRuntime::observe_barcode(std::string_view raw_barcode) {
    if (impl_->finalized) {
        throw std::logic_error("cannot observe a barcode after direct-count finalization");
    }
    ++impl_->barcode_prior_groups;
    if (const auto index = impl_->exact_barcode(raw_barcode)) {
        impl_->exact_priors[*index].fetch_add(1, std::memory_order_relaxed);
        ++impl_->exact_barcode_prior_groups;
    }
}

std::optional<CountWorker::Pending> CountRuntime::prepare_assignment(
    std::uint32_t profile_index, std::string_view raw_barcode,
    std::optional<std::string_view> barcode_quality, std::string_view umi,
    const std::vector<std::string>& genes, std::uint64_t read_count,
    bool reaches_umi_filter) {
    if (impl_->finalized) {
        throw std::logic_error("cannot observe an assignment after finalization");
    }
    if (profile_index >= impl_->profiles.size()) {
        throw std::invalid_argument("assignment profile index is outside the selected profiles");
    }
    if (read_count == 0) {
        throw std::invalid_argument("assignment read_count must be positive");
    }
    PostBarcodeDisposition disposition = PostBarcodeDisposition::early_exit;
    if (reaches_umi_filter) {
        switch (basic_umi_status(umi)) {
            case BasicUmiStatus::contains_n:
                disposition = PostBarcodeDisposition::umi_n;
                break;
            case BasicUmiStatus::homopolymer:
                disposition = PostBarcodeDisposition::umi_homopolymer;
                break;
            case BasicUmiStatus::empty:
                disposition = PostBarcodeDisposition::invalid_umi;
                break;
            case BasicUmiStatus::valid:
                disposition = genes.empty()
                    ? PostBarcodeDisposition::no_gene
                    : genes.size() == 1
                        ? PostBarcodeDisposition::unique
                        : PostBarcodeDisposition::direct_multigene;
                break;
        }
    }
    std::uint32_t feature_index = 0;
    if (disposition == PostBarcodeDisposition::unique) {
        const auto feature = impl_->feature_index.find(genes.front());
        if (feature == impl_->feature_index.end()) {
            throw std::runtime_error(
                "assignment resolved a gene outside the count-fact catalog: " +
                genes.front());
        }
        feature_index = feature->second;
    }
    const bool usable_barcode_quality =
        barcode_quality && barcode_quality->size() == raw_barcode.size();

    if (const auto barcode = impl_->exact_barcode(raw_barcode)) {
        impl_->record_post_barcode(profile_index, disposition, read_count);
        if (disposition != PostBarcodeDisposition::unique) {
            return std::nullopt;
        }
        CountWorker::Pending result;
        result.count = read_count;
        result.exact = true;
        result.exact_key = {profile_index, *barcode, feature_index, std::string(umi)};
        return result;
    }
    AtomicProfileCountRuntimeCounters& profile_counters =
        *impl_->profile_counters.at(profile_index);
    if (impl_->barcode_candidates(raw_barcode).empty()) {
        // Barcode correction precedes profile evaluation in the frozen Python
        // pipeline. Profile zero owns the one global barcode-stage count; every
        // profile retains its own downstream audit counter.
        if (profile_index == 0) {
            impl_->off_whitelist_uncorrectable_dropped.fetch_add(
                read_count, std::memory_order_relaxed);
        }
        profile_counters.off_whitelist_uncorrectable_dropped.fetch_add(
            read_count, std::memory_order_relaxed);
        return std::nullopt;
    }
    CountWorker::Pending result;
    result.count = read_count;
    result.deferred_key = {
        profile_index,
        std::string(raw_barcode),
        usable_barcode_quality ? canonical_barcode_quality(*barcode_quality)
                               : std::string{},
        disposition,
        feature_index,
        disposition == PostBarcodeDisposition::unique ? std::string(umi)
                                                       : std::string{},
    };
    return result;
}

void CountRuntime::merge_worker_batch(std::vector<CountWorker::Pending>& pending) {
    for (CountWorker::Pending& observation : pending) {
        if (observation.exact) {
            const size_t shard_index = KeyHash<ExactKey>{}(observation.exact_key) % kShardCount;
            auto& shard = impl_->exact_shards[shard_index];
            std::lock_guard<std::mutex> lock(shard.mutex);
            auto [found, inserted] = shard.values.try_emplace(
                std::move(observation.exact_key), observation.count);
            if (!inserted) {
                if (std::numeric_limits<std::uint64_t>::max() - found->second <
                    observation.count) {
                    throw std::runtime_error("aggregate read count overflow");
                }
                found->second += observation.count;
            } else {
                impl_->approximate_bytes.fetch_add(kExactEntryBytes,
                                                   std::memory_order_relaxed);
            }
        } else {
            const size_t shard_index =
                KeyHash<DeferredKey>{}(observation.deferred_key) % kShardCount;
            auto& shard = impl_->deferred_shards[shard_index];
            std::lock_guard<std::mutex> lock(shard.mutex);
            auto [found, inserted] = shard.values.try_emplace(
                std::move(observation.deferred_key), observation.count);
            if (!inserted) {
                if (std::numeric_limits<std::uint64_t>::max() - found->second <
                    observation.count) {
                    throw std::runtime_error("aggregate read count overflow");
                }
                found->second += observation.count;
            } else {
                impl_->approximate_bytes.fetch_add(kDeferredEntryBytes,
                                                   std::memory_order_relaxed);
            }
        }
    }
    pending.clear();
    impl_->spill_if_needed();
}

CountRuntimeResult CountRuntime::finalize() {
    if (impl_->finalized) {
        throw std::logic_error("direct count runtime can be finalized only once");
    }
    impl_->finalized = true;
    const bool external_merge = !impl_->exact_runs.empty() || !impl_->deferred_runs.empty();
    std::vector<ExactRow> resident_exact;
    std::vector<DeferredRow> resident_deferred;
    if (external_merge) {
        // Once a budget crossing has created external runs, materialize the final
        // resident shard contents as another run and use one deterministic merge.
        impl_->spill_if_needed(true);
    } else {
        // The normal no-spill path remains entirely in memory. Finalization must not
        // turn every successful run into an evidence spool merely for convenience.
        for (auto& shard : impl_->exact_shards) {
            std::lock_guard<std::mutex> lock(shard.mutex);
            resident_exact.reserve(resident_exact.size() + shard.values.size());
            for (auto& [key, count] : shard.values) {
                resident_exact.push_back({std::move(key), count});
            }
            shard.values.clear();
        }
        for (auto& shard : impl_->deferred_shards) {
            std::lock_guard<std::mutex> lock(shard.mutex);
            resident_deferred.reserve(resident_deferred.size() + shard.values.size());
            for (auto& [key, count] : shard.values) {
                resident_deferred.push_back({std::move(key), count});
            }
            shard.values.clear();
        }
        impl_->approximate_bytes.store(0, std::memory_order_relaxed);
        sort_and_combine(resident_exact);
        sort_and_combine(resident_deferred);
    }

    std::vector<ExactRow> corrected_buffer;
    const size_t corrected_limit = static_cast<size_t>(std::max<std::uint64_t>(
        1, impl_->options.memory_budget_bytes / (4 * kExactEntryBytes)));
    auto spill_corrected = [&]() {
        if (corrected_buffer.empty()) {
            return;
        }
        const std::filesystem::path path =
            impl_->options.spill_directory /
            ("corrected-" + std::to_string(impl_->next_run++) + ".exact.zst");
        const size_t rows = corrected_buffer.size();
        impl_->exact_runs.push_back(path);
        write_exact_run(path, std::move(corrected_buffer));
        corrected_buffer.clear();
        corrected_buffer.reserve(std::min<size_t>(corrected_limit, 1 << 20));
        impl_->record_spill(path, rows);
    };
    auto consume_deferred = [&](DeferredRow row) {
            const auto corrected = impl_->correct_barcode(row.key.raw_barcode,
                                                          row.key.barcode_quality);
            if (corrected) {
                if (row.key.profile == 0) {
                    impl_->barcode_correction_succeeded.fetch_add(
                        row.count, std::memory_order_relaxed);
                }
                impl_->profile_counters.at(row.key.profile)
                    ->barcode_correction_succeeded.fetch_add(
                        row.count, std::memory_order_relaxed);
                impl_->record_post_barcode(row.key.profile, row.key.disposition,
                                           row.count);
                if (row.key.disposition == PostBarcodeDisposition::unique) {
                    corrected_buffer.push_back(
                        {{row.key.profile, *corrected, row.key.feature,
                          std::move(row.key.umi)},
                         row.count});
                    if (external_merge && corrected_buffer.size() >= corrected_limit) {
                        spill_corrected();
                    }
                }
            } else {
                if (row.key.profile == 0) {
                    impl_->barcode_correction_failed.fetch_add(
                        row.count, std::memory_order_relaxed);
                }
                impl_->profile_counters.at(row.key.profile)
                    ->barcode_correction_failed.fetch_add(
                        row.count, std::memory_order_relaxed);
            }
        };
    if (external_merge) {
        merge_runs<DeferredRunReader, DeferredRow>(impl_->deferred_runs,
                                                   consume_deferred);
        spill_corrected();
    } else {
        for (DeferredRow& row : resident_deferred) {
            consume_deferred(std::move(row));
        }
        resident_exact.insert(resident_exact.end(),
                              std::make_move_iterator(corrected_buffer.begin()),
                              std::make_move_iterator(corrected_buffer.end()));
        corrected_buffer.clear();
        sort_and_combine(resident_exact);
    }

    CountRuntimeResult result;
    std::optional<std::pair<std::uint32_t, std::uint32_t>> current_barcode;
    std::optional<std::uint32_t> current_feature;
    std::map<std::string, std::uint64_t> feature_umis;
    std::vector<CollapsedCandidate> collapsed;
    std::vector<std::filesystem::path>& collapsed_runs = impl_->collapsed_runs;
    const size_t collapsed_limit = static_cast<size_t>(
        std::max<std::uint64_t>(1, impl_->options.memory_budget_bytes /
                                      (4 * kExactEntryBytes)));
    auto spill_collapsed = [&]() {
        if (collapsed.empty()) {
            return;
        }
        const std::filesystem::path path =
            impl_->options.spill_directory /
            ("barcode-" + std::to_string(impl_->next_run++) + ".collapsed.zst");
        const size_t rows = collapsed.size();
        collapsed_runs.push_back(path);
        write_collapsed_run(path, std::move(collapsed));
        collapsed.clear();
        collapsed.reserve(std::min<size_t>(collapsed_limit, 1 << 20));
        impl_->record_spill(path, rows);
    };

    auto flush_feature = [&]() {
        if (!current_feature || feature_umis.empty()) {
            feature_umis.clear();
            return;
        }
        const auto labels = collapse_1mm_cr(feature_umis);
        struct Aggregate {
            std::uint64_t supporting_reads = 0;
            std::uint64_t raw_umis = 0;
        };
        std::map<std::string, Aggregate> aggregates;
        for (const auto& [umi, count] : feature_umis) {
            Aggregate& aggregate = aggregates[labels.at(umi)];
            aggregate.supporting_reads += count;
            ++aggregate.raw_umis;
        }
        for (const auto& [umi, aggregate] : aggregates) {
            collapsed.push_back({umi, *current_feature, aggregate.supporting_reads,
                                 aggregate.raw_umis, feature_umis.at(umi)});
        }
        // A raw UMI relabeled into a neighbor leaves no label of its own, yet the
        // frozen MultiGeneUMI_CR raw guard compares every feature's pre-correction
        // read count for the winning sequence. Carry those sequences as
        // zero-support shadow rows so the per-barcode merge can still see them.
        for (const auto& [umi, count] : feature_umis) {
            if (labels.at(umi) != umi) {
                collapsed.push_back({umi, *current_feature, 0, 0, count});
            }
        }
        feature_umis.clear();
        if (collapsed.size() >= collapsed_limit) {
            spill_collapsed();
        }
    };

    auto flush_barcode = [&]() {
        flush_feature();
        if (!current_barcode) {
            collapsed.clear();
            std::error_code ignored_removal;
            for (const std::filesystem::path& path : collapsed_runs) {
                std::filesystem::remove(path, ignored_removal);
            }
            collapsed_runs.clear();
            return;
        }
        const size_t molecule_begin = result.molecules.size();
        std::vector<CollapsedCandidate> same_umi;
        auto resolve_umi = [&]() {
            if (same_umi.empty()) {
                return;
            }
            // Shadow rows (zero supporting reads) never compete for the label;
            // they only feed the raw guard below.
            size_t winner = same_umi.size();
            bool tied = false;
            for (size_t index = 0; index < same_umi.size(); ++index) {
                if (same_umi[index].supporting_reads == 0) {
                    continue;
                }
                if (winner == same_umi.size() ||
                    same_umi[index].supporting_reads >
                        same_umi[winner].supporting_reads) {
                    winner = index;
                    tied = false;
                } else if (same_umi[index].supporting_reads ==
                           same_umi[winner].supporting_reads) {
                    tied = true;
                }
            }
            if (winner == same_umi.size()) {
                same_umi.clear();
                return;
            }
            const bool raw_guard = std::any_of(
                same_umi.begin(), same_umi.end(),
                [&](const CollapsedCandidate& candidate) {
                    return candidate.original_label_reads >
                           same_umi[winner].original_label_reads;
                });
            if (!tied && !raw_guard) {
                result.molecules.push_back(
                    {current_barcode->first, current_barcode->second,
                     same_umi[winner].feature, same_umi[winner].umi,
                     same_umi[winner].supporting_reads,
                     same_umi[winner].raw_umis});
            }
            same_umi.clear();
        };
        auto consume_collapsed = [&](CollapsedCandidate candidate) {
            if (!same_umi.empty() && candidate.umi != same_umi.front().umi) {
                resolve_umi();
            }
            same_umi.push_back(std::move(candidate));
        };
        if (collapsed_runs.empty()) {
            std::sort(collapsed.begin(), collapsed.end(),
                      [](const auto& left, const auto& right) {
                          return left.tuple() < right.tuple();
                      });
            for (CollapsedCandidate& candidate : collapsed) {
                consume_collapsed(std::move(candidate));
            }
        } else {
            spill_collapsed();
            merge_collapsed_runs(collapsed_runs, consume_collapsed);
        }
        resolve_umi();
        auto first_molecule = result.molecules.begin() +
                              static_cast<std::ptrdiff_t>(molecule_begin);
        std::sort(first_molecule, result.molecules.end(),
                  [](const NumericMoleculeRecord& left,
                     const NumericMoleculeRecord& right) {
                      return std::tie(left.feature_catalog_index, left.corrected_umi) <
                             std::tie(right.feature_catalog_index, right.corrected_umi);
                  });
        size_t at = molecule_begin;
        while (at < result.molecules.size()) {
            const std::uint32_t feature =
                result.molecules[at].feature_catalog_index;
            size_t end = at + 1;
            while (end < result.molecules.size() &&
                   result.molecules[end].feature_catalog_index == feature) {
                ++end;
            }
            result.counts.push_back({current_barcode->first,
                                     current_barcode->second, feature,
                                     static_cast<std::uint64_t>(end - at)});
            at = end;
        }
        collapsed.clear();
        std::error_code ignored_removal;
        for (const std::filesystem::path& path : collapsed_runs) {
            std::filesystem::remove(path, ignored_removal);
        }
        collapsed_runs.clear();
    };

    auto consume_exact = [&](ExactRow row) {
        const std::pair<std::uint32_t, std::uint32_t> barcode{row.key.profile,
                                                             row.key.barcode};
        if (current_barcode && barcode != *current_barcode) {
            flush_barcode();
            current_feature.reset();
        }
        current_barcode = barcode;
        if (current_feature && row.key.feature != *current_feature) {
            flush_feature();
        }
        current_feature = row.key.feature;
        feature_umis.emplace(std::move(row.key.umi), row.count);
    };
    if (external_merge) {
        merge_runs<ExactRunReader, ExactRow>(impl_->exact_runs, consume_exact);
    } else {
        for (ExactRow& row : resident_exact) {
            consume_exact(std::move(row));
        }
    }
    flush_barcode();

    result.exact_barcode_priors.reserve(impl_->whitelist.size());
    for (size_t index = 0; index < impl_->whitelist.size(); ++index) {
        result.exact_barcode_priors.push_back(
            impl_->exact_priors[index].load(std::memory_order_relaxed));
    }
    result.counters = impl_->counters();
    result.profile_counters.reserve(impl_->profile_counters.size());
    for (const auto& profile_counters : impl_->profile_counters) {
        result.profile_counters.push_back(profile_counters->snapshot());
    }

    // Cleanup must not discard a completed result: use the non-throwing overloads
    // and let the destructor retry anything that could not be removed here.
    std::error_code ignored;
    for (const std::filesystem::path& path : impl_->exact_runs) {
        std::filesystem::remove(path, ignored);
    }
    for (const std::filesystem::path& path : impl_->deferred_runs) {
        std::filesystem::remove(path, ignored);
    }
    impl_->exact_runs.clear();
    impl_->deferred_runs.clear();
    // Leave a caller-supplied spill directory in place; only remove one this
    // runtime created itself.
    if (impl_->owns_spill_directory) {
        std::filesystem::remove(impl_->options.spill_directory, ignored);
        // Ownership ends here so the destructor cannot remove a directory that a
        // caller recreates at the same path after finalize.
        impl_->owns_spill_directory = false;
    }
    return result;
}

const std::vector<EffectiveProfile>& CountRuntime::profiles() const {
    return impl_->profiles;
}

const std::vector<std::string>& CountRuntime::whitelist() const {
    return impl_->whitelist;
}

const std::vector<std::string>& CountRuntime::feature_catalog() const {
    return impl_->features;
}

std::optional<std::uint32_t> CountRuntime::corrected_barcode_index(
    std::string_view raw_barcode,
    std::optional<std::string_view> barcode_quality) const {
    const std::string_view usable_quality =
        barcode_quality && barcode_quality->size() == raw_barcode.size()
            ? *barcode_quality
            : std::string_view{};
    return impl_->correct_barcode(raw_barcode, usable_quality);
}

std::vector<std::string> read_barcode_whitelist(const std::filesystem::path& path,
                                                size_t expected_length) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("cannot open barcode whitelist " + path.string());
    }
    std::vector<std::string> result;
    std::string line;
    size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.size() != expected_length ||
            std::any_of(line.begin(), line.end(), [](char value) {
                return value != 'A' && value != 'C' && value != 'G' && value != 'T';
            })) {
            throw std::runtime_error("barcode whitelist line " +
                                     std::to_string(line_number) +
                                     " is not an uppercase A/C/G/T barcode of the expected length");
        }
        result.push_back(std::move(line));
    }
    std::sort(result.begin(), result.end());
    if (result.empty() || std::adjacent_find(result.begin(), result.end()) != result.end()) {
        throw std::runtime_error("barcode whitelist is empty or contains duplicates");
    }
    return result;
}

}  // namespace pancollapse::direct_count
