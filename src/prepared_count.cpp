#include "prepared_count.hpp"
#include "node_score_resolver.hpp"

#include "bounded_sharded_cache.hpp"
#include "count_assignment_signature.hpp"
#include "count_reference.hpp"
#include "direct_count_index.hpp"
#include "direct_count_output.hpp"
#include "direct_count_runtime.hpp"
#include "read_granular_input.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cctype>
#include <condition_variable>
#include <deque>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <stop_token>
#include <thread>
#include <syncstream>
#include <unordered_set>
#include <utility>
#ifdef __linux__
#include <unistd.h>
#endif
#if defined(__GLIBC__)
#include <malloc.h>
#endif

namespace pancollapse::prepared_count {
namespace {
std::optional<std::size_t> read_memory_number(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::size_t value = 0;
    if (input >> value) return value;
    return std::nullopt; // cgroup "max", missing, or unsupported accounting.
}

std::size_t resident_memory_bytes() {
#ifdef __linux__
    std::ifstream statm("/proc/self/statm");
    std::size_t pages = 0, resident_pages = 0;
    const long page_size = sysconf(_SC_PAGESIZE);
    if (!(statm >> pages >> resident_pages) || page_size <= 0 ||
        resident_pages > std::numeric_limits<std::size_t>::max() /
            static_cast<std::size_t>(page_size)) {
        throw std::runtime_error("cannot determine resident memory");
    }
    return resident_pages * static_cast<std::size_t>(page_size);
#else
    throw std::runtime_error("resident memory accounting requires Linux");
#endif
}

void release_unused_heap() {
#if defined(__GLIBC__)
    static_cast<void>(malloc_trim(0));
#endif
}

count_memory::Plan plan_memory(const count_memory::Request& request,
                               std::uint64_t reference_file_bytes) {
#ifdef __linux__
    const std::size_t resident = resident_memory_bytes();
    std::ifstream meminfo("/proc/meminfo");
    std::string line;
    std::optional<std::size_t> available;
    while (std::getline(meminfo, line)) {
        std::istringstream fields(line);
        std::string name;
        std::size_t kib = 0;
        if (fields >> name >> kib; name == "MemAvailable:" &&
            kib <= (std::numeric_limits<std::size_t>::max() - resident) / 1024) {
            available = resident + kib * 1024;
            break;
        }
    }
    if (!available) throw std::runtime_error("cannot determine available memory for cache planning");
    std::size_t target = std::min(request.target_bytes.value_or(*available), *available);
    const auto limit = read_memory_number("/sys/fs/cgroup/memory.max");
    const auto current = read_memory_number("/sys/fs/cgroup/memory.current");
    if (limit) {
        target = std::min(target, *limit);
        if (current) {
            const std::size_t free = *current < *limit ? *limit - *current : 0;
            const std::size_t shared_target = free > std::numeric_limits<std::size_t>::max() - resident
                ? std::numeric_limits<std::size_t>::max() : free + resident;
            target = std::min(target, shared_target);
        }
    }
    // Reserve for the complete mapped payload as well as current resident
    // metadata. mmap alone does not make not-yet-faulted reference pages free.
    if (reference_file_bytes > std::numeric_limits<std::size_t>::max() - resident)
        throw std::runtime_error("direct reference memory reservation overflows");
    return count_memory::plan_direct(target, resident + reference_file_bytes, request);
#else
    static_cast<void>(request);
    static_cast<void>(reference_file_bytes);
    throw std::runtime_error("automatic native-count cache planning requires Linux memory accounting");
#endif
}

struct Molecule {
    std::string barcode, umi;
    std::optional<std::string> barcode_quality;
};
struct Job {
    std::string name;
    std::vector<std::unique_ptr<std::string>> records;
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
std::optional<std::string> take_quality(std::string& name, std::string_view tag,
                                        std::size_t expected) {
    const auto separator = name.rfind('_');
    if (separator == std::string::npos || !name.substr(separator + 1).starts_with(tag)) return std::nullopt;
    const std::string encoded = name.substr(separator + 1 + tag.size());
    if (encoded.size() != expected * 2) throw std::runtime_error("malformed raw molecule quality suffix");
    std::string result; result.reserve(expected);
    for (std::size_t i = 0; i < encoded.size(); i += 2) {
        const int high = hex(encoded[i]), low = hex(encoded[i + 1]);
        if (high < 0 || low < 0) throw std::runtime_error("non-hexadecimal raw molecule quality suffix");
        const char quality = static_cast<char>((high << 4) | low);
        if (static_cast<unsigned char>(quality) < 33 || static_cast<unsigned char>(quality) > 126) {
            throw std::runtime_error("non-printable raw molecule quality suffix");
        }
        result.push_back(quality);
    }
    name.resize(separator);
    return result;
}
Molecule parse_molecule(std::string name, std::size_t cb_length, std::size_t umi_length) {
    const auto umi_quality = take_quality(name, "uy", umi_length);
    const auto barcode_quality = take_quality(name, "cy", cb_length);
    if (umi_quality && !barcode_quality) throw std::runtime_error("raw UMI quality requires barcode quality");
    static_cast<void>(umi_quality);  // UMI quality is validated but CountRuntime has no UMI-quality input.
    const auto umi_separator = name.rfind('_');
    if (umi_separator == std::string::npos) throw std::runtime_error("GAMP name does not contain raw UMI");
    const auto barcode_separator = name.rfind('_', umi_separator == 0 ? 0 : umi_separator - 1);
    if (barcode_separator == std::string::npos || barcode_separator == 0) throw std::runtime_error("GAMP name does not contain raw CB");
    Molecule result{name.substr(barcode_separator + 1, umi_separator - barcode_separator - 1),
                    name.substr(umi_separator + 1), barcode_quality};
    if (result.barcode.size() != cb_length || result.umi.size() != umi_length) throw std::runtime_error("raw CB/UMI lengths do not match configuration");
    for (std::string* field : {&result.barcode, &result.umi}) {
        for (char& base : *field) {
            if (!supported_base(base)) throw std::runtime_error("raw CB/UMI contains unsupported base");
            base = static_cast<char>(std::toupper(static_cast<unsigned char>(base)));
        }
    }
    return result;
}
direct_count::GeneFact gene_metadata(const count_reference::Source& reference,
                                     std::string_view id) {
    const auto& strings = reference.strings();
    const auto genes = reference.genes();
    const auto found = std::lower_bound(genes.begin(), genes.end(), id,
        [&](const auto& record, std::string_view target) {
            return strings.at(record.id_string) < target;
        });
    if (found == genes.end() || strings.at(found->id_string) != id)
        throw std::runtime_error("output gene absent from native count index");
    direct_count::GeneFact result{strings.at(found->id_string), {}, std::nullopt};
    if (found->name_string != count_reference::missing)
        result.gene_name = strings.at(found->name_string);
    for (const auto type : reference.sets().at(found->type_set))
        result.gene_types.push_back(strings.at(type));
    std::sort(result.gene_types.begin(), result.gene_types.end());
    return result;
}
}  // namespace

int run(const Config& config) {
    if (config.threads == 0 || config.profiles.empty()) throw std::runtime_error("prepared count requires profiles and positive threads");
    if(std::filesystem::exists(config.outdir))throw std::runtime_error("count destination already exists");
    const auto initialized = std::chrono::steady_clock::now();
    auto reference = direct_index::Source::load(config.index);
    auto features = reference->take_feature_catalog();
    const auto memory = plan_memory(config.memory, std::filesystem::file_size(config.index));
    std::cerr << "panCollapse: memory-plan: target_bytes=" << memory.target_bytes
              << " resident_bytes=" << memory.resident_bytes
              << " reserve_bytes=" << memory.reserve_bytes << " cache_pool_bytes=" << memory.pool_bytes
              << " node_bytes=" << memory.limits[0] << " edge_bytes=" << memory.limits[1]
              << " assignment_bytes=" << memory.limits[2] << " splice_bytes=" << memory.limits[3] << '\n';
    std::cerr << "panCollapse: initialization: phase=prepared_count_index_load seconds="
              << std::chrono::duration<double>(std::chrono::steady_clock::now()-initialized).count()
              << " models=" << reference->model_count()
              << " reference_mode=direct-facts" << '\n';
    NodeScoreResolver node_resolver(*reference);
    std::vector<NumericAssignmentProfile> numeric_profiles;
    numeric_profiles.reserve(config.profiles.size());
    for (const auto& profile : config.profiles)
        numeric_profiles.push_back(compile_numeric_profile(*reference, profile));
    std::cerr << "panCollapse: assignment-algorithm: node-score-v1\n";
    using AssignmentResults = std::vector<direct_count::NumericAssignmentResult>;
    using AssignmentCache = BoundedShardedCache<std::string, AssignmentResults>;
    // Retention is bounded by bytes, not an unrelated number of read signatures.
    constexpr std::size_t assignment_cache_capacity = std::numeric_limits<std::size_t>::max();
    const std::size_t assignment_cache_bytes = memory.limits[2];
    // The reference and ordered profile vector are immutable for this run.
    // Cache only final assignments, never sample-dependent CB/UMI tallies.
    auto assignment_cache = std::make_unique<AssignmentCache>(
        assignment_cache_capacity, assignment_cache_bytes,
        [](const AssignmentResults& results) {
            std::size_t bytes = sizeof(results) +
                results.capacity() * sizeof(direct_count::NumericAssignmentResult);
            for (const auto& result : results) {
                bytes += result.genes.capacity() * sizeof(std::uint32_t);
            }
            return bytes;
        });
    direct_count::CountRuntime runtime(config.profiles,
        direct_count::read_barcode_whitelist(config.whitelist, config.cb_length),
        std::move(features), {config.cb_length, config.umi_length, true});
    const auto processing_started = std::chrono::steady_clock::now();
    const auto report_native_queries = [&] {
        const auto node_stats = node_resolver.stats();
        std::osyncstream(std::cerr) << "panCollapse: node-score-stats: represented_alignments="
            << node_stats.complete_alignments << " subpath_visits=" << node_stats.subpath_visits
            << " model_visits=" << node_stats.model_visits
            << " dp_states=" << node_stats.dp_states << " dp_transitions=" << node_stats.dp_transitions
            << " max_frontier_states=" << node_stats.max_frontier_states
            << " raw_models=" << node_stats.raw_models << " model_groups=" << node_stats.model_groups
            << " read_offset_conflicts=" << node_stats.read_offset_conflicts
            << " membership_groups=" << node_stats.membership_groups
            << " raw_model_transfers=" << node_stats.raw_model_transfers
            << " frontier_compactions=" << node_stats.frontier_compactions
            << " balance_saturated_groups=" << node_stats.balance_saturated_groups
            << " balance_unsaturated_groups=" << node_stats.balance_unsaturated_groups << '\n';
        std::osyncstream(std::cerr) << "panCollapse: assignment-cache-stats: entries="
                  << assignment_cache->entries() << " hits=" << assignment_cache->hits()
                  << " misses=" << assignment_cache->misses()
                  << " uncached=" << assignment_cache->uncached()
                  << " charged_bytes=" << assignment_cache->charged_bytes() << '\n';
    };
    report_native_queries();
    std::cerr << "panCollapse: initialization: prepared counting ready seconds="
              << std::chrono::duration<double>(processing_started-initialized).count() << '\n';
    if (config.preload_only) {
        std::cout << "{\"schema\":\"pancollapse-prepared-count-v1\",\"status\":\"preloaded\"}\n";
        return 0;
    }

    std::ifstream input_file;
    std::istream* input = &std::cin;
    if (config.gamp != "-") { input_file.open(config.gamp, std::ios::binary); if (!input_file) throw std::runtime_error("cannot open GAMP"); input = &input_file; }
    const std::size_t workers = std::max<std::size_t>(1, config.threads);
    const std::size_t capacity = std::max<std::size_t>(2, workers * 2);
    std::deque<Job> jobs; std::mutex mutex; std::condition_variable ready, space;
    bool closed = false, stopped = false; std::exception_ptr failure;
    std::uint64_t groups = 0, input_records = 0;
    std::atomic<std::uint64_t> completed_groups{0};
    std::atomic<std::uint64_t> missing_molecules{0}, malformed_molecules{0}, unsupported_molecules{0};
    std::atomic<std::uint64_t> high_complexity_groups{0};
    std::atomic<std::uint64_t> noncountable_molecule_groups{0};
    const auto report_progress = [&](std::uint64_t completed) {
        std::osyncstream(std::cerr)
            << "panCollapse: counting: completed_groups=" << completed
            << " seconds=" << std::chrono::duration<double>(
                std::chrono::steady_clock::now() - processing_started).count() << '\n';
        report_native_queries();
    };
    const auto stop_with_failure = [&](std::exception_ptr error) {
        std::lock_guard lock(mutex);
        if (!failure) failure = std::move(error);
        stopped = true;
        ready.notify_all();
        space.notify_all();
    };
    // A completion milestone alone hides a few expensive groups still running
    // after the input queue has drained. This timer observes counters and RSS;
    // it neither queries the reference nor records per-read evidence.
    std::jthread periodic_progress([&](std::stop_token stop) {
            std::mutex progress_mutex;
            std::condition_variable_any wake;
            std::unique_lock lock(progress_mutex);
            while (!stop.stop_requested()) {
                wake.wait_for(lock, stop, std::chrono::seconds(30), [] { return false; });
                if (stop.stop_requested()) break;
                try {
                    const auto resident = resident_memory_bytes();
                    const auto action = count_memory::rss_pressure_action(
                        resident, memory, assignment_cache->retention_enabled());
                    if (action == count_memory::RssPressureAction::release_cache) {
                        const auto cleared = assignment_cache->disable_retention_and_clear();
                        release_unused_heap();
                        std::osyncstream(std::cerr)
                            << "panCollapse: assignment-cache-release: phase=counting"
                            << " resident_bytes=" << resident
                            << " entries=" << cleared.entries
                            << " charged_bytes=" << cleared.charged_bytes << '\n';
                    } else if (action == count_memory::RssPressureAction::stop) {
                        stop_with_failure(std::make_exception_ptr(std::runtime_error(
                            "mandatory count state reached the protected memory boundary")));
                        break;
                    }
                    report_progress(completed_groups.load(std::memory_order_relaxed));
                } catch (...) {
                    stop_with_failure(std::current_exception());
                    break;
                }
            }
    });
    const auto process = [&](Job job, direct_count::CountWorker& worker,
                             std::vector<std::array<std::uint64_t, direct_count::kAssignmentTerminalCount>>& terminals) {
        // Even a molecule excluded before scoring releases the preceding read's leases.
        reference->begin_read_group();
        Molecule molecule;
        try {
            molecule = parse_molecule(job.name, config.cb_length, config.umi_length);
        } catch (const std::runtime_error& error) {
            if (config.fail_molecule_identity) throw;
            const std::string_view message(error.what());
            if (message.find("unsupported") != std::string_view::npos) ++unsupported_molecules;
            else if (message.find("does not contain") != std::string_view::npos) ++missing_molecules;
            else ++malformed_molecules;
            return;
        }
        worker.observe_barcode(molecule.barcode);
        // This exclusion does not depend on gene assignment or sample priors.
        // All valid groups still contributed to the exact-whitelist CB prior.
        if (!worker.potentially_countable(molecule.barcode, molecule.umi)) {
            noncountable_molecule_groups.fetch_add(1, std::memory_order_relaxed);
            return;
        }
        std::vector<vg::MultipathAlignment> decoded(job.records.size());
        std::vector<const vg::MultipathAlignment*> records; records.reserve(decoded.size());
        for (std::size_t i = 0; i < job.records.size(); ++i) { input::decode_multipath_record(*job.records[i], decoded[i]); records.push_back(&decoded[i]); }
        thread_local input::GampAlignmentComplexity alignment_complexity;
        if (alignment_complexity.exceeds(records, config.max_alignments_per_read)) {
            high_complexity_groups.fetch_add(1, std::memory_order_relaxed);
            for (auto& profile : terminals) {
                ++profile[static_cast<std::size_t>(
                    direct_count::AssignmentTerminal::unassigned_high_complexity)];
            }
            return;
        }
        const auto resolve_results = [&] {
            auto resolved = node_resolver.resolve(records, numeric_profiles);
            if (resolved.size() != config.profiles.size())
                throw std::runtime_error("prepared resolver profile cardinality mismatch");
            AssignmentResults results;
            results.reserve(resolved.size());
            for (auto& assignment : resolved)
                results.push_back(std::move(assignment.result));
            return results;
        };
        AssignmentResults uncached_results;
        std::shared_ptr<const AssignmentResults> cached_results;
        std::span<const direct_count::NumericAssignmentResult> results;
        if (assignment_cache) {
            cached_results = assignment_cache->get_or_build(
                assignment_graph_signature(records), resolve_results);
            results = *cached_results;
        } else {
            uncached_results = resolve_results();
            results = uncached_results;
        }
        if (results.size() != config.profiles.size()) throw std::runtime_error("prepared resolver profile cardinality mismatch");
        for (std::size_t i = 0; i < results.size(); ++i) {
            const auto& result = results[i];
            ++terminals[i][static_cast<std::size_t>(result.terminal)];
            if (result.barcode_correction_eligible) worker.observe_assignment_ids(static_cast<std::uint32_t>(i), molecule.barcode, molecule.barcode_quality, molecule.umi, result.genes, 1, result.reaches_umi_filter);
        }
    };
    std::vector<std::array<std::uint64_t, direct_count::kAssignmentTerminalCount>> profile_terminals(config.profiles.size());
    std::vector<std::thread> pool;
    try { for (std::size_t i = 0; i < workers; ++i) pool.emplace_back([&] {
        try {
            auto worker = runtime.make_worker();
            std::vector<std::array<std::uint64_t, direct_count::kAssignmentTerminalCount>> local_terminals(config.profiles.size());
            for (;;) {
                Job job;
                {
                    std::unique_lock lock(mutex);
                    ready.wait(lock, [&] { return stopped || !jobs.empty() || closed; });
                    if (stopped || (closed && jobs.empty())) break;
                    job = std::move(jobs.front()); jobs.pop_front();
                    space.notify_one();
                }
                process(std::move(job), worker, local_terminals);
                const auto completed=completed_groups.fetch_add(1,std::memory_order_relaxed)+1;
                if (completed == 1000 || completed % 10000 == 0) {
                    report_progress(completed);
                }
            }
            worker.flush();
            std::lock_guard terminal_lock(mutex);
            for (std::size_t p = 0; p < local_terminals.size(); ++p)
                for (std::size_t t = 0; t < local_terminals[p].size(); ++t)
                    profile_terminals[p][t] += local_terminals[p][t];
        }
        catch (...) { std::lock_guard lock(mutex); if (!failure) failure = std::current_exception(); stopped = true; ready.notify_all(); space.notify_all(); }
    }); } catch (...) { { std::lock_guard lock(mutex); failure = std::current_exception(); stopped = true; closed = true; } ready.notify_all(); space.notify_all(); for (auto& thread : pool) thread.join(); std::rethrow_exception(failure); }
    try {
        Job current;
        bool has_current=false;
        auto submit = [&] { if (!has_current) return; std::unique_lock lock(mutex); space.wait(lock, [&] { return stopped || jobs.size() < capacity; }); if (stopped) std::rethrow_exception(failure); jobs.push_back(std::move(current)); ++groups; lock.unlock(); ready.notify_one(); };
        std::unordered_set<std::string> completed_names;
        const auto stream_stats = input::raw_gamp_for_each(*input, [&](std::unique_ptr<std::string> bytes) {
            input::MultipathRecordPeek peek; std::string fallback;
            if (!input::peek_multipath_record(*bytes, peek)) { vg::MultipathAlignment decoded; input::decode_multipath_record(*bytes, decoded); fallback = decoded.name(); peek.name = fallback; peek.has_subpath = decoded.subpath_size() != 0; }
            if (!has_current) { current.name = peek.name; has_current=true; }
            else if (current.name != peek.name) { completed_names.insert(current.name); if (completed_names.contains(std::string(peek.name))) throw std::runtime_error("GAMP read name recurred after its group closed"); submit(); current.name = peek.name; }
            if (peek.has_subpath) current.records.push_back(std::move(bytes));
        }, 2);
        submit(); { std::lock_guard lock(mutex); closed = true; } ready.notify_all();
        input_records = stream_stats.records;
    } catch (...) { std::lock_guard lock(mutex); if (!failure) failure = std::current_exception(); stopped = true; closed = true; ready.notify_all(); space.notify_all(); }
    for (auto& thread : pool) thread.join();
    if (periodic_progress.joinable()) {
        periodic_progress.request_stop();
        periodic_progress.join();
    }
    if (failure) std::rethrow_exception(failure);
    report_native_queries();
    const auto assignment_cache_entries = assignment_cache->entries();
    const auto assignment_cache_hits = assignment_cache->hits();
    const auto assignment_cache_misses = assignment_cache->misses();
    const auto assignment_cache_uncached = assignment_cache->uncached();
    const auto cleared = assignment_cache->disable_retention_and_clear();
    assignment_cache.reset();
    release_unused_heap();
    const auto resident_after_cache = resident_memory_bytes();
    std::cerr << "panCollapse: assignment-cache-release: phase=eof"
              << " resident_bytes=" << resident_after_cache
              << " entries=" << cleared.entries
              << " charged_bytes=" << cleared.charged_bytes << '\n';
    if (count_memory::rss_pressure_action(resident_after_cache, memory, false) ==
        count_memory::RssPressureAction::stop) {
        throw std::runtime_error(
            "mandatory count state reached the protected memory boundary before finalization");
    }
    const auto processed = std::chrono::steady_clock::now();
    std::cerr << "panCollapse: counting: EOF records=" << input_records << " groups=" << groups
              << "; reducing barcode/UMI tallies\n";
    std::cerr << "panCollapse: complexity-filter: max_alignments_per_read="
              << config.max_alignments_per_read << " dropped_read_groups="
              << high_complexity_groups.load() << '\n';
    const auto result = runtime.finalize();
    std::cerr << "panCollapse: counting: count_rows=" << result.counts.size()
              << " molecules=" << result.molecules.size() << "; writing compact outputs\n";
    direct_count::CountOutputOptions output; output.output_directory = config.outdir;
    output.pancollapse_version = config.version; output.command_line = config.command_line;
    output.analysis_scope = config.analysis_scope.empty() ? "frozen-profiles" : config.analysis_scope;
    output.assignment_algorithm = "node-score-v1";
    output.fact_bundle_content_id = reference->content_id();
    output.fact_authority = "direct-count-index";
    output.reference_provenance_json = reference->provenance_json();
    output.threads = workers; output.input_records = input_records; output.input_read_groups = groups; output.write_10x_mex = config.write_10x_mex;
    output.max_alignments_per_read = config.max_alignments_per_read;
    output.high_complexity_read_groups = high_complexity_groups.load();
    output.noncountable_molecule_groups = noncountable_molecule_groups.load();
    output.assignment_cache_capacity = assignment_cache_capacity;
    output.memory_plan = memory;
    output.assignment_cache_entries = assignment_cache_entries;
    output.assignment_cache_hits = assignment_cache_hits;
    output.assignment_cache_misses = assignment_cache_misses;
    output.assignment_cache_uncached = assignment_cache_uncached;
    output.initialization_seconds = std::chrono::duration<double>(processing_started - initialized).count();
    output.processing_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - processing_started).count();
    output.processing_phases = direct_count::CountProcessingPhases{};
    output.processing_phases->read_processing_seconds = std::chrono::duration<double>(processed - processing_started).count();
    output.processing_phases->count_finalize_seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - processed).count();
    output.raw_molecule_missing_groups = missing_molecules.load();
    output.raw_molecule_malformed_groups = malformed_molecules.load();
    output.raw_molecule_unsupported_groups = unsupported_molecules.load();
    output.raw_molecule_skipped_groups = output.raw_molecule_missing_groups + output.raw_molecule_malformed_groups + output.raw_molecule_unsupported_groups;
    output.profile_assignment_terminals = std::move(profile_terminals);
    const auto input_size = [](const std::filesystem::path& path) -> std::uint64_t {
        return path == "-" ? 0 : std::filesystem::file_size(path);
    };
    output.inputs = {{"native_count_reference",
                      config.index, {}, input_size(config.index), std::nullopt, std::nullopt},
                     {"gamp", config.gamp, {}, input_size(config.gamp), std::nullopt, std::nullopt},
                     {"barcode_whitelist", config.whitelist, {}, input_size(config.whitelist), std::nullopt, std::nullopt}};
    direct_count::write_count_outputs(runtime, result,
        direct_count::GeneMetadataLookup([&](std::string_view id) {
            return gene_metadata(*reference, id);
        }), output);
    return 0;
}

}  // namespace pancollapse::prepared_count
