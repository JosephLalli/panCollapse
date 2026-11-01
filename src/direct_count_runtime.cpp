#include "direct_count_runtime.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <map>
#include <mutex>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <utility>

namespace pancollapse::direct_count {
namespace {

constexpr size_t kShardCount = 256;

// The common A/C/G/T UMI fits in one tagged integer. Preserve the complete
// historical API for longer/noncanonical strings through a rare owned fallback;
// storage optimization must not add a new biological filter.
class PackedUmi {
  public:
    PackedUmi() = default;
    explicit PackedUmi(std::string_view text) {
        if (text.size() <= 31) {
            std::uint64_t encoded = 1; // Length sentinel, including leading A's.
            bool canonical = true;
            for (char base : text) {
                std::uint64_t digit = 0;
                switch (base) {
                    case 'A': break;
                    case 'C': digit = 1; break;
                    case 'G': digit = 2; break;
                    case 'T': digit = 3; break;
                    default: canonical = false; break;
                }
                if (!canonical) break;
                encoded = (encoded << 2) | digit;
            }
            if (canonical) { bits_ = encoded; return; }
        }
        bits_ = 0;
        exceptional_ = std::make_unique<const std::string>(text);
    }
    PackedUmi(const PackedUmi& other) : bits_(other.bits_) {
        if (other.exceptional_) exceptional_ =
            std::make_unique<const std::string>(*other.exceptional_);
    }
    PackedUmi& operator=(const PackedUmi& other) {
        if (this != &other) { PackedUmi copy(other); swap(copy); }
        return *this;
    }
    PackedUmi(PackedUmi&& other) noexcept
        : bits_(std::exchange(other.bits_, 1)), exceptional_(std::move(other.exceptional_)) {}
    PackedUmi& operator=(PackedUmi&& other) noexcept {
        if (this != &other) {
            bits_ = std::exchange(other.bits_, 1);
            exceptional_ = std::move(other.exceptional_);
        }
        return *this;
    }
    std::string str() const {
        if (exceptional_) return *exceptional_;
        static constexpr char bases[] = "ACGT";
        std::string result;
        for (auto encoded = bits_; encoded > 1; encoded >>= 2)
            result.push_back(bases[encoded & 3]);
        std::reverse(result.begin(), result.end());
        return result;
    }
    std::size_t hash() const {
        return exceptional_ ? std::hash<std::string>{}(*exceptional_)
                            : std::hash<std::uint64_t>{}(bits_);
    }
    bool operator==(const PackedUmi& other) const {
        return bits_ == other.bits_ && (bits_ != 0 || *exceptional_ == *other.exceptional_);
    }
    bool operator<(const PackedUmi& other) const {
        return bits_ != other.bits_ ? bits_ < other.bits_
                                   : bits_ == 0 && *exceptional_ < *other.exceptional_;
    }
  private:
    void swap(PackedUmi& other) noexcept {
        std::swap(bits_, other.bits_); exceptional_.swap(other.exceptional_);
    }
    std::uint64_t bits_ = 1;
    std::unique_ptr<const std::string> exceptional_;
};
static_assert(sizeof(PackedUmi) <= 16);

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
    PackedUmi umi;

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

PostBarcodeDisposition assignment_disposition(std::string_view umi,
                                              size_t gene_count,
                                              bool reaches_umi_filter) {
    if (!reaches_umi_filter) return PostBarcodeDisposition::early_exit;
    switch (basic_umi_status(umi)) {
        case BasicUmiStatus::contains_n: return PostBarcodeDisposition::umi_n;
        case BasicUmiStatus::homopolymer: return PostBarcodeDisposition::umi_homopolymer;
        case BasicUmiStatus::empty: return PostBarcodeDisposition::invalid_umi;
        case BasicUmiStatus::valid:
            return gene_count == 0 ? PostBarcodeDisposition::no_gene
                 : gene_count == 1 ? PostBarcodeDisposition::unique
                                   : PostBarcodeDisposition::direct_multigene;
    }
    throw std::logic_error("unrecognized UMI status");
}

struct DeferredKey {
    std::uint32_t profile = 0;
    std::string raw_barcode;
    std::string barcode_quality;
    PostBarcodeDisposition disposition = PostBarcodeDisposition::unique;
    std::uint32_t feature = 0;
    PackedUmi umi;

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
        combine(key.umi.hash());
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
        combine(key.umi.hash());
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
static_assert(sizeof(ExactKey) <= 32);
static_assert(sizeof(DeferredKey) <= 96);

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
    bool finalized = false;

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
            0,
            0,
            0,
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
    auto pending = runtime_->prepare_string_assignment(
        profile_index, raw_barcode, barcode_quality, umi, genes, read_count,
        reaches_umi_filter);
    if (!pending) {
        return;
    }
    pending_.push_back(std::move(*pending));
    if (pending_.size() >= batch_capacity_) {
        flush();
    }
}

void CountWorker::observe_assignment_ids(
    std::uint32_t profile_index, std::string_view raw_barcode,
    std::optional<std::string_view> barcode_quality, std::string_view umi,
    std::span<const std::uint32_t> gene_ids, std::uint64_t read_count,
    bool reaches_umi_filter) {
    if (runtime_ == nullptr) {
        throw std::logic_error("count worker is not attached to a runtime");
    }
    auto pending = runtime_->prepare_id_assignment(
        profile_index, raw_barcode, barcode_quality, umi, gene_ids, read_count,
        reaches_umi_filter);
    if (!pending) {
        return;
    }
    pending_.push_back(std::move(*pending));
    if (pending_.size() >= batch_capacity_) {
        flush();
    }
}

bool CountWorker::potentially_countable(std::string_view raw_barcode,
                                        std::string_view umi) const {
    if (runtime_ == nullptr) {
        throw std::logic_error("count worker is not attached to a runtime");
    }
    return runtime_->potentially_countable(raw_barcode, umi);
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

    if (!impl_->options.feature_catalog_prepared) {
        std::sort(feature_catalog.begin(), feature_catalog.end());
    }
    if (feature_catalog.empty() ||
        (!impl_->options.feature_catalog_prepared &&
         std::adjacent_find(feature_catalog.begin(), feature_catalog.end()) !=
            feature_catalog.end())) {
        throw std::invalid_argument("feature catalog must be nonempty and unique");
    }
    if (feature_catalog.size() > std::numeric_limits<std::uint32_t>::max() ||
        impl_->whitelist.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::invalid_argument("direct count dictionaries exceed uint32 indexes");
    }
    impl_->features = std::move(feature_catalog);
    if (!impl_->options.feature_catalog_prepared) {
        impl_->feature_index.reserve(impl_->features.size());
        for (size_t index = 0; index < impl_->features.size(); ++index) {
            impl_->feature_index.emplace(impl_->features[index], static_cast<std::uint32_t>(index));
        }
    }
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

bool CountRuntime::potentially_countable(std::string_view raw_barcode,
                                         std::string_view umi) const {
    return basic_umi_status(umi) == BasicUmiStatus::valid &&
           (impl_->exact_barcode(raw_barcode).has_value() ||
            !impl_->barcode_candidates(raw_barcode).empty());
}

std::optional<CountWorker::Pending> CountRuntime::prepare_string_assignment(
    std::uint32_t profile_index, std::string_view raw_barcode,
    std::optional<std::string_view> barcode_quality, std::string_view umi,
    const std::vector<std::string>& genes, std::uint64_t read_count,
    bool reaches_umi_filter) {
    const PostBarcodeDisposition disposition =
        assignment_disposition(umi, genes.size(), reaches_umi_filter);
    std::optional<std::uint32_t> feature_index;
    if (disposition == PostBarcodeDisposition::unique) {
        if (impl_->options.feature_catalog_prepared) {
            const auto feature = std::lower_bound(impl_->features.begin(),
                impl_->features.end(), genes.front());
            if (feature == impl_->features.end() || *feature != genes.front()) {
                throw std::runtime_error(
                    "assignment resolved a gene outside the count-fact catalog: " +
                    genes.front());
            }
            feature_index = static_cast<std::uint32_t>(feature - impl_->features.begin());
        } else {
            const auto feature = impl_->feature_index.find(genes.front());
            if (feature == impl_->feature_index.end()) {
                throw std::runtime_error(
                    "assignment resolved a gene outside the count-fact catalog: " +
                    genes.front());
            }
            feature_index = feature->second;
        }
    }
    return prepare_assignment(profile_index, raw_barcode, barcode_quality, umi,
                              static_cast<std::uint8_t>(disposition), feature_index,
                              read_count);
}

std::optional<CountWorker::Pending> CountRuntime::prepare_id_assignment(
    std::uint32_t profile_index, std::string_view raw_barcode,
    std::optional<std::string_view> barcode_quality, std::string_view umi,
    std::span<const std::uint32_t> gene_ids, std::uint64_t read_count,
    bool reaches_umi_filter) {
    const PostBarcodeDisposition disposition =
        assignment_disposition(umi, gene_ids.size(), reaches_umi_filter);
    std::optional<std::uint32_t> feature_index;
    if (disposition == PostBarcodeDisposition::unique) {
        feature_index = gene_ids.front();
    }
    return prepare_assignment(profile_index, raw_barcode, barcode_quality, umi,
                              static_cast<std::uint8_t>(disposition), feature_index,
                              read_count);
}

std::optional<CountWorker::Pending> CountRuntime::prepare_assignment(
    std::uint32_t profile_index, std::string_view raw_barcode,
    std::optional<std::string_view> barcode_quality, std::string_view umi,
    std::uint8_t disposition_value, std::optional<std::uint32_t> feature_index,
    std::uint64_t read_count) {
    if (impl_->finalized) {
        throw std::logic_error("cannot observe an assignment after finalization");
    }
    if (profile_index >= impl_->profiles.size()) {
        throw std::invalid_argument("assignment profile index is outside the selected profiles");
    }
    if (read_count == 0) {
        throw std::invalid_argument("assignment read_count must be positive");
    }
    const PostBarcodeDisposition disposition =
        static_cast<PostBarcodeDisposition>(disposition_value);
    std::uint32_t resolved_feature_index = 0;
    if (disposition == PostBarcodeDisposition::unique) {
        if (!feature_index || *feature_index >= impl_->features.size()) {
            throw std::out_of_range("assignment feature index is outside the count-fact catalog");
        }
        resolved_feature_index = *feature_index;
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
        result.exact_key = {profile_index, *barcode, resolved_feature_index, PackedUmi(umi)};
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
        resolved_feature_index,
        PackedUmi(disposition == PostBarcodeDisposition::unique ? umi : std::string_view{}),
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
            }
        }
    }
    pending.clear();
}

CountRuntimeResult CountRuntime::finalize() {
    if (impl_->finalized) {
        throw std::logic_error("direct count runtime can be finalized only once");
    }
    impl_->finalized = true;
    using Clock = std::chrono::steady_clock;
    const bool profile_timing = std::getenv("PANCOLLAPSE_PROFILE_TIMING") != nullptr;
    const auto begin = profile_timing ? Clock::now() : Clock::time_point{};
    double umi_collapse_seconds = 0;
    std::uint64_t feature_groups = 0, distinct_umis = 0, max_feature_umis = 0;
    std::vector<ExactRow> resident_exact;
    std::vector<DeferredRow> resident_deferred;
    size_t resident_exact_size = 0;
    for (auto& shard : impl_->exact_shards) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        if (shard.values.size() > resident_exact.max_size() - resident_exact_size) {
            throw std::length_error("resident exact aggregate exceeds vector capacity");
        }
        resident_exact_size += shard.values.size();
    }
    resident_exact.reserve(resident_exact_size);
    for (auto& shard : impl_->exact_shards) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        for (auto& [key, count] : shard.values) {
            resident_exact.push_back({std::move(key), count});
        }
        shard.values.clear();
        shard.values.rehash(0);
    }

    size_t resident_deferred_size = 0;
    for (auto& shard : impl_->deferred_shards) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        if (shard.values.size() > resident_deferred.max_size() - resident_deferred_size) {
            throw std::length_error("resident deferred aggregate exceeds vector capacity");
        }
        resident_deferred_size += shard.values.size();
    }
    resident_deferred.reserve(resident_deferred_size);
    for (auto& shard : impl_->deferred_shards) {
        std::lock_guard<std::mutex> lock(shard.mutex);
        for (auto& [key, count] : shard.values) {
            resident_deferred.push_back({std::move(key), count});
        }
        shard.values.clear();
        shard.values.rehash(0);
    }
    sort_and_combine(resident_deferred);

    const auto resident_done = profile_timing ? Clock::now() : Clock::time_point{};
    std::vector<ExactRow> corrected_buffer;
    corrected_buffer.reserve(resident_deferred.size());
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
    for (DeferredRow& row : resident_deferred) {
        consume_deferred(std::move(row));
    }
    std::vector<DeferredRow>().swap(resident_deferred);
    if (corrected_buffer.size() > resident_exact.max_size() - resident_exact.size())
        throw std::length_error("corrected aggregate exceeds vector capacity");
    resident_exact.reserve(resident_exact.size() + corrected_buffer.size());
    resident_exact.insert(resident_exact.end(),
                          std::make_move_iterator(corrected_buffer.begin()),
                          std::make_move_iterator(corrected_buffer.end()));
    std::vector<ExactRow>().swap(corrected_buffer);
    sort_and_combine(resident_exact);

    const auto correction_done = profile_timing ? Clock::now() : Clock::time_point{};
    CountRuntimeResult result;
    std::optional<std::pair<std::uint32_t, std::uint32_t>> current_barcode;
    std::optional<std::uint32_t> current_feature;
    std::map<std::string, std::uint64_t> feature_umis;
    std::vector<CollapsedCandidate> collapsed;

    auto flush_feature = [&]() {
        if (!current_feature || feature_umis.empty()) {
            feature_umis.clear();
            return;
        }
        const auto collapse_begin = profile_timing ? Clock::now() : Clock::time_point{};
        const auto labels = collapse_1mm_cr(feature_umis);
        if (profile_timing) {
            umi_collapse_seconds += std::chrono::duration<double>(Clock::now() - collapse_begin).count();
            ++feature_groups;
            distinct_umis += feature_umis.size();
            max_feature_umis = std::max(max_feature_umis,
                                      static_cast<std::uint64_t>(feature_umis.size()));
        }
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
    };

    auto flush_barcode = [&]() {
        flush_feature();
        if (!current_barcode) {
            collapsed.clear();
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
        std::sort(collapsed.begin(), collapsed.end(),
                  [](const auto& left, const auto& right) {
                      return left.tuple() < right.tuple();
                  });
        for (CollapsedCandidate& candidate : collapsed) {
            consume_collapsed(std::move(candidate));
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
        feature_umis.emplace(row.key.umi.str(), row.count);
    };
    for (ExactRow& row : resident_exact) {
        consume_exact(std::move(row));
    }
    std::vector<ExactRow>().swap(resident_exact);
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

    if (profile_timing) {
        std::cerr << "panCollapse: count_finalize_profile"
                  << " resident_prepare_seconds="
                  << std::chrono::duration<double>(resident_done - begin).count()
                  << " correction_seconds="
                  << std::chrono::duration<double>(correction_done - resident_done).count()
                  << " molecule_stage_seconds="
                  << std::chrono::duration<double>(Clock::now() - correction_done).count()
                  << " umi_collapse_seconds=" << umi_collapse_seconds
                  << " feature_groups=" << feature_groups
                  << " distinct_umis=" << distinct_umis
                  << " max_feature_umis=" << max_feature_umis
                  << " storage=memory_only\n";
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
