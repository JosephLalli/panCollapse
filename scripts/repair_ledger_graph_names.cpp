// Reference preparation only. No aliases or policy changes enter the counter.
#include <gbwt/gbwt.h>
#include <gbwtgraph/utils.h>
#include <sdsl/simple_sds.hpp>
#include <openssl/evp.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <parallel/algorithm>
#include <omp.h>
#include <algorithm>
#include <array>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;
namespace {
void require(bool ok, std::string_view why) {
    if (!ok) throw std::runtime_error(std::string(why));
}
class Digest {
    std::unique_ptr<EVP_MD_CTX, decltype(&EVP_MD_CTX_free)> ctx{EVP_MD_CTX_new(), EVP_MD_CTX_free};
    std::string pending;
public:
    Digest() { require(ctx && EVP_DigestInit_ex(ctx.get(), EVP_sha256(), nullptr) == 1, "SHA init"); }
    void add(std::string_view s) {
        pending.append(s);
        if (pending.size() >= (1u << 20)) flush();
    }
    void flush() {
        require(EVP_DigestUpdate(ctx.get(), pending.data(), pending.size()) == 1, "SHA update");
        pending.clear();
    }
    std::string finish() {
        flush();
        std::array<unsigned char, 32> bytes{}; unsigned length = 0;
        require(EVP_DigestFinal_ex(ctx.get(), bytes.data(), &length) == 1 && length == 32, "SHA finish");
        constexpr char hex[] = "0123456789abcdef";
        std::string out;
        for (auto b : bytes) { out += hex[b >> 4]; out += hex[b & 15]; }
        return out;
    }
};
std::string sha(std::string_view s) { Digest d; d.add(s); return d.finish(); }
uint64_t number(std::string_view s) {
    uint64_t value = 0;
    auto [end, ec] = std::from_chars(s.data(), s.data() + s.size(), value);
    require(ec == std::errc() && end == s.data() + s.size(), "invalid unsigned integer");
    return value;
}
struct CheckedFile {
    std::string path, expected;
    int descriptor;
    std::ifstream stream;
    static std::string snapshot(const struct stat& value) {
        std::ostringstream out;
        out << value.st_dev << ':' << value.st_ino << ':' << value.st_size << ':'
            << value.st_mtim.tv_sec << ':' << std::setw(9) << std::setfill('0') << value.st_mtim.tv_nsec;
        return out.str();
    }
    CheckedFile(std::string filename, std::string binding)
        : path(std::move(filename)), expected(std::move(binding)),
          descriptor(open(path.c_str(), O_RDONLY | O_CLOEXEC)) {
        require(descriptor >= 0, "cannot open bound input");
        check();
        stream.open("/proc/self/fd/" + std::to_string(descriptor), std::ios::binary);
        require(bool(stream), "cannot read bound descriptor");
    }
    ~CheckedFile() { if (descriptor >= 0) close(descriptor); }
    void check() const {
        struct stat opened{}, named{};
        require(fstat(descriptor, &opened) == 0 && stat(path.c_str(), &named) == 0,
                "cannot stat bound input");
        require(snapshot(opened) == expected && snapshot(named) == expected,
                "bound input device/inode/size/mtime changed");
    }
    uint64_t size() const {
        struct stat value{}; require(fstat(descriptor, &value) == 0, "cannot stat input size");
        return static_cast<uint64_t>(value.st_size);
    }
};
struct Range { std::string chrom; uint64_t end, expected_generic; };
struct Entry {
    uint64_t hash, path;
    bool operator<(const Entry& other) const {
        return hash < other.hash || (hash == other.hash && path < other.path);
    }
};
class Resolver {
    const gbwt::Metadata& metadata;
    gbwtgraph::sample_name_set references;
    std::vector<Range> ranges;
    std::vector<Entry> entries;
    std::vector<uint64_t> used;
    uint64_t generic;
    uint64_t generic_paths = 0;
    uint64_t legacy_paths = 0;
    static constexpr uint64_t legacy_bit = uint64_t{1} << 63;
    bool collisions;
    uint64_t hash(std::string_view s) const {
        if (collisions) return 0; // Adversarial self-test: equality is never hash-only.
        uint64_t result = 14695981039346656037ULL;
        for (unsigned char c : s) { result ^= c; result *= 1099511628211ULL; }
        return result;
    }
    template<class Action> auto with_name(uint64_t entry, Action action) const {
        const uint64_t id = entry & ~legacy_bit;
        const auto& path = metadata.path(id);
        if (path.sample == generic)
            return action(metadata.contig_names.strings.view(path.contig));
        const auto name = gbwtgraph::compose_path_name(metadata, path,
            entry & legacy_bit ? handlegraph::PathSense::REFERENCE :
            gbwtgraph::get_path_sense(metadata, path, references));
        return action(std::string_view(name));
    }
    bool has_legacy_name(uint64_t id) const {
        const auto& path = metadata.path(id);
        return path.sample != generic && path.count == 0 &&
            gbwtgraph::get_path_sense(metadata, path, references) == handlegraph::PathSense::HAPLOTYPE;
    }
    struct Hit { uint64_t id; bool legacy; };
    std::optional<Hit> lookup(std::string_view name, bool include_legacy = false) const {
        const auto key = hash(name);
        auto found = std::lower_bound(entries.begin(), entries.end(), Entry{key, 0});
        std::optional<Hit> result;
        for (; found != entries.end() && found->hash == key; ++found) {
            const bool legacy = found->path & legacy_bit;
            if (legacy && !include_legacy) continue;
            if (with_name(found->path, [&](auto actual) { return actual == name; })) {
                const uint64_t id = found->path & ~legacy_bit;
                require(!result || result->id == id, "ambiguous GBWT native/legacy path name");
                if (!result || !legacy) result = Hit{id, legacy};
            }
        }
        return result;
    }
public:
    Resolver(const gbwt::Metadata& m, gbwtgraph::sample_name_set refs,
             std::vector<Range> owners, bool forced_collisions = false)
        : metadata(m), references(std::move(refs)), ranges(std::move(owners)),
          used((m.paths() + 63) / 64, 0), generic(m.sample("_gbwt_ref")),
          collisions(forced_collisions) {
        require(m.hasPathNames() && m.hasSampleNames() && m.hasContigNames(), "incomplete GBWT metadata");
        require(generic < m.samples(), "generic GBWT sample is absent");
        require(m.paths() < legacy_bit, "too many metadata paths");
        require(!ranges.empty() && ranges.back().end == m.paths(), "range/metadata count mismatch");
        for (size_t i = 0; i < ranges.size(); ++i)
            require(ranges[i].end > (i ? ranges[i-1].end : 0), "invalid range order");
        std::vector<uint64_t> observed(ranges.size(), 0);
        size_t range = 0;
        for (uint64_t id = 0; id < m.paths(); ++id) {
            while (id >= ranges[range].end) ++range;
            const auto& path = m.path(id);
            require(path.sample < m.samples() && path.contig < m.contigs(), "invalid metadata name ID");
            if (path.sample == generic) {
                require(path.phase == std::numeric_limits<uint32_t>::max() && path.count == 0,
                        "generic path carries unsupported phase/subrange metadata");
                ++observed[range]; ++generic_paths;
            }
            if (has_legacy_name(id)) ++legacy_paths;
        }
        for (size_t i = 0; i < ranges.size(); ++i)
            require(observed[i] == ranges[i].expected_generic, "chromosome generic path count mismatch");
        entries.resize(m.paths() + legacy_paths);
        // Each slot is independent; metadata strings are immutable.
        #pragma omp parallel for schedule(static)
        for (uint64_t id = 0; id < m.paths(); ++id)
            entries[id] = {with_name(id, [&](auto name) { return hash(name); }), id};
        size_t next = m.paths();
        for (uint64_t id = 0; id < m.paths(); ++id) if (has_legacy_name(id)) {
            const uint64_t encoded = id | legacy_bit;
            entries[next++] = {with_name(encoded, [&](auto name) { return hash(name); }), encoded};
        }
        std::cerr << "repair: metadata_names=" << entries.size() << " sorting\n";
        __gnu_parallel::sort(entries.begin(), entries.end());
        // Equal spellings must have equal lookup hashes. Compare full strings
        // within each equal-hash group, including native/legacy collisions.
        for (size_t begin = 0; begin < entries.size();) {
            size_t end = begin + 1;
            while (end < entries.size() && entries[end].hash == entries[begin].hash) ++end;
            for (size_t a = begin; a < end; ++a) for (size_t b = a + 1; b < end; ++b)
                with_name(entries[a].path, [&](auto left) {
                    return with_name(entries[b].path, [&](auto right) {
                        require(left != right, "duplicate native/legacy GBWT spelling");
                        return true;
                    });
                });
            begin = end;
        }
        std::cerr << "repair: legacy_bridges_validated=" << legacy_paths << '\n';
    }
    struct Match { uint64_t id; bool changed, legacy; };
    uint64_t non_generic_count() const { return metadata.paths() - generic_paths; }
    uint64_t legacy_count() const { return legacy_paths; }
    std::string canonical_name(uint64_t id) const {
        return with_name(id, [](auto name) { return std::string(name); });
    }
    Match resolve(std::string_view old_name) {
        auto exact = lookup(old_name);
        const auto split = old_name.find("::");
        std::optional<Hit> candidate;
        std::string_view short_name;
        if (!exact && split != std::string_view::npos) {
            short_name = old_name.substr(split + 2);
            candidate = lookup(short_name, true);
        }
        uint64_t id = 0;
        bool changed = false;
        if (exact) id = exact->id;
        else {
            require(candidate.has_value(), "ledger path has no exact GBWT counterpart");
            id = candidate->id;
            require(metadata.path(id).sample != generic, "cannot dequalify a generic GBWT path");
            const auto owner = std::upper_bound(ranges.begin(), ranges.end(), id,
                [](uint64_t path, const Range& range) { return path < range.end; });
            require(owner != ranges.end() && owner->chrom == old_name.substr(0, split),
                    "dequalified name belongs to a different chromosome");
            changed = true;
        }
        auto& word = used[id / 64];
        const uint64_t mask = uint64_t{1} << (id % 64);
        require(!(word & mask), "duplicate ledger name or reused GBWT path");
        word |= mask;
        return {id, changed, changed && candidate->legacy};
    }
};

struct Binding { std::string hash; uint64_t bytes, rows; };
uint64_t take_number(const std::string& data, size_t& pos, size_t width) {
    require(pos <= data.size() && width <= data.size() - pos, "truncated PCI header");
    uint64_t result = 0;
    for (size_t i = 0; i < width; ++i)
        result |= uint64_t(static_cast<unsigned char>(data[pos++])) << (8*i);
    return result;
}
std::string take_string(const std::string& data, size_t& pos) {
    const auto length = take_number(data, pos, 4);
    require(length <= (1u << 20) && pos <= data.size() && length <= data.size() - pos,
            "invalid PCI string");
    auto result = data.substr(pos, length); pos += length; return result;
}
void put_number(std::string& data, size_t pos, uint64_t value) {
    require(pos + 8 <= data.size(), "PCI numeric patch outside header");
    for (size_t i = 0; i < 8; ++i) data[pos+i] = static_cast<char>((value >> (8*i)) & 255);
}
std::string rebind(std::string data, const Binding& old, const Binding& next,
                   const std::string& expected_payload) {
    require(data.size() >= 100, "short PCI");
    const size_t end = data.size() - 64;
    const auto original_digest = sha(std::string_view(data).substr(0, end));
    require(original_digest == data.substr(end) && original_digest == expected_payload,
            "source PCI checksum/expected identity mismatch");
    size_t pos = 0;
    require(data.substr(pos, 8) == "PCANIDX2", "unsupported PCI magic"); pos += 8;
    require(take_number(data, pos, 4) == 2, "unsupported PCI version");
    require(take_string(data, pos) == "pancollapse-annotation-facts-v2", "unsupported PCI semantics");
    require(take_number(data, pos, 1) <= 1 && take_number(data, pos, 1) <= 1, "invalid annotation formats");
    const size_t content_pos = pos + 4;
    const auto content = take_string(data, pos);
    require(content.size() == 71 && take_string(data, pos) == "annotation", "invalid PCI authority");
    const auto count = take_number(data, pos, 8);
    require(count == 3, "unexpected PCI source roles");
    std::map<std::string, std::string> sources;
    bool bound = false;
    for (uint64_t i = 0; i < count; ++i) {
        const auto role = take_string(data, pos);
        const size_t hash_pos = pos + 4;
        auto digest = take_string(data, pos);
        require(digest.size() == 64 && sources.emplace(role, digest).second, "invalid PCI source digest");
        const size_t size_pos = pos;
        const auto bytes = take_number(data, pos, 8);
        if (role == "path_identity_ledger") {
            require(digest == old.hash && bytes == old.bytes, "source PCI does not bind the input ledger");
            data.replace(hash_pos, 64, next.hash);
            put_number(data, size_pos, next.bytes);
            bound = true;
        }
    }
    require(bound && take_number(data, pos, 8) == old.rows && old.rows == next.rows,
            "PCI ledger row count mismatch");
    Digest before, after;
    for (const char* role : {"annotation", "annotation_reference", "path_identity_ledger"}) {
        require(sources.contains(role), "missing PCI source role");
        before.add(sources.at(role)); before.add("\n");
        after.add(std::string_view(role) == "path_identity_ledger" ? next.hash : sources.at(role));
        after.add("\n");
    }
    require(content == "sha256:" + before.finish(), "invalid PCI source content identity");
    data.replace(content_pos, 71, "sha256:" + after.finish());
    // No byte at or beyond pos (gene/profile/category/Parent facts) was patched.
    data.replace(end, 64, sha(std::string_view(data).substr(0, end)));
    return data;
}
void self_test() {
    omp_set_num_threads(2);
    gbwtgraph::MetadataBuilder builder;
    builder.add_generic_path("chr1::exact");
    builder.add_haplotype("HG", "tx", 1, 0);
    auto metadata = builder.get_metadata();
    const auto hap_name = gbwtgraph::compose_path_name(metadata, metadata.path(1),
        gbwtgraph::get_path_sense(metadata, metadata.path(1), {}));
    auto rejects = [](auto action) {
        try { action(); } catch (const std::exception&) { return; }
        throw std::runtime_error("self-test expected rejection");
    };
    for (bool collision : {false, true}) {
        Resolver resolver(metadata, {}, {{"chr1", 1, 1}, {"chr2", 2, 0}}, collision);
        require(!resolver.resolve("chr1::exact").changed, "exact name changed");
        const auto actual_match = resolver.resolve("chr2::" + hap_name);
        require(actual_match.changed && !actual_match.legacy &&
                resolver.canonical_name(actual_match.id) == hap_name, "haplotype not dequalified");
        rejects([&] { resolver.resolve("chr2::" + hap_name); });
        rejects([&] { resolver.resolve("chr1::missing"); });
        Resolver wrong(metadata, {}, {{"chr1", 1, 1}, {"chr2", 2, 0}}, collision);
        rejects([&] { wrong.resolve("chr1::" + hap_name); });
        rejects([&] { wrong.resolve("chr2::chr1::exact"); });
        Resolver legacy(metadata, {}, {{"chr1", 1, 1}, {"chr2", 2, 0}}, collision);
        const auto legacy_match = legacy.resolve("chr2::HG#1#tx");
        require(legacy_match.changed && legacy_match.legacy &&
                legacy.canonical_name(legacy_match.id) == hap_name, "legacy haplotype bridge failed");
        rejects([&] { legacy.resolve("chr2::" + hap_name); });
        Resolver bare_legacy(metadata, {}, {{"chr1", 1, 1}, {"chr2", 2, 0}}, collision);
        rejects([&] { bare_legacy.resolve("HG#1#tx"); });
        rejects([&] { bare_legacy.resolve("chr1::HG#1#tx"); });
    }
    gbwtgraph::MetadataBuilder both_builder;
    both_builder.add_generic_path("chr2::" + hap_name); both_builder.add_haplotype("HG", "tx", 1, 0);
    auto both = both_builder.get_metadata();
    Resolver ambiguous(both, {}, {{"chr2", 2, 1}});
    require(!ambiguous.resolve("chr2::" + hap_name).changed, "exact native name lost precedence");
    Resolver reference_paths(metadata, {"HG"}, {{"chr1", 1, 1}, {"chr2", 2, 0}});
    const auto reference_match = reference_paths.resolve("chr2::HG#1#tx");
    require(!reference_match.legacy && reference_paths.canonical_name(reference_match.id) == "HG#1#tx",
            "reference-sense spelling failed");
    rejects([&] { Resolver bad_ranges(metadata, {}, {{"chr1", 1, 0}, {"chr2", 2, 0}}); });
    auto duplicate = metadata;
    duplicate.path_names.push_back(metadata.path(0));
    rejects([&] { Resolver duplicates(duplicate, {}, {{"chr1", 1, 1}, {"chr2", 2, 0}, {"chr3", 3, 1}}); });
    auto duplicate_legacy = metadata;
    duplicate_legacy.path_names.push_back(metadata.path(1));
    rejects([&] { Resolver duplicates(duplicate_legacy, {}, {{"chr1", 1, 1}, {"chr2", 3, 0}}); });
    gbwtgraph::MetadataBuilder alias_collision_builder;
    alias_collision_builder.add_generic_path("HG#1#tx");
    alias_collision_builder.add_haplotype("HG", "tx", 1, 0);
    auto alias_collision = alias_collision_builder.get_metadata();
    rejects([&] { Resolver ambiguous_alias(alias_collision, {}, {{"chr2", 2, 1}}); });
    gbwtgraph::MetadataBuilder phases_builder;
    phases_builder.add_generic_path("chr1::exact");
    phases_builder.add_haplotype("HG", "tx", 1, 0);
    phases_builder.add_haplotype("HG", "tx", 2, 0);
    phases_builder.add_haplotype("HG", "nonzero", 1, 7);
    auto phases = phases_builder.get_metadata();
    Resolver distinct_phases(phases, {}, {{"chr1", 1, 1}, {"chr2", 4, 0}}, true);
    require(distinct_phases.legacy_count() == 2, "nonzero count received legacy alias");
    require(distinct_phases.canonical_name(distinct_phases.resolve("chr2::HG#1#tx").id) == "HG#1#tx#0",
            "phase one lost");
    require(distinct_phases.canonical_name(distinct_phases.resolve("chr2::HG#2#tx").id) == "HG#2#tx#0",
            "phase two lost");
    rejects([&] { distinct_phases.resolve("chr2::HG#1#nonzero"); });
    gbwtgraph::MetadataBuilder no_generic_builder;
    no_generic_builder.add_haplotype("HG", "tx", 1, 0);
    auto no_generic = no_generic_builder.get_metadata();
    rejects([&] { Resolver missing_generic(no_generic, {}, {{"chr2", 1, 0}}); });
    // Synthetic envelope: the opaque fact section must survive byte-identically.
    Binding old{std::string(64, 'a'), 1000, 2}, next{std::string(64, 'b'), 994, 2};
    std::string pci = "PCANIDX2";
    auto num = [&](uint64_t v, unsigned n) { for (unsigned i=0; i<n; ++i) pci += char((v>>(8*i))&255); };
    auto str = [&](std::string_view s) { num(s.size(), 4); pci += s; };
    num(2, 4); str("pancollapse-annotation-facts-v2"); num(0,1); num(0,1);
    const auto annotation = std::string(64, 'c'), reference = std::string(64, 'd');
    str("sha256:" + sha(annotation+"\n"+reference+"\n"+old.hash+"\n")); str("annotation"); num(3,8);
    str("annotation"); str(annotation); num(12,8);
    str("annotation_reference"); str(reference); num(13,8);
    str("path_identity_ledger"); str(old.hash); num(old.bytes,8); num(old.rows,8);
    const auto fact_start = pci.size(); pci += "opaque immutable gene/profile/category/Parent facts";
    const auto digest = sha(pci); pci += digest;
    auto rebound = rebind(pci, old, next, digest);
    require(rebound.substr(fact_start, pci.size()-64-fact_start) == pci.substr(fact_start, pci.size()-64-fact_start),
            "PCI fact payload changed");
    require(rebound.substr(rebound.size()-64) == sha(std::string_view(rebound).substr(0,rebound.size()-64)),
            "PCI output digest invalid");
    rejects([&] { auto corrupt=pci; corrupt[fact_start]^=1; rebind(corrupt,old,next,digest); });
    rejects([&] { auto changed=old; changed.hash[0]='f'; rebind(pci,changed,next,digest); });
    rejects([&] { auto changed=next; changed.rows=3; rebind(pci,old,changed,digest); });
    std::cout << "repair self-test PASS\n";
}
} // namespace

int main(int argc, char** argv) try {
    if (argc == 2 && std::string_view(argv[1]) == "--self-test") { self_test(); return 0; }
    if (argc == 7 && std::string_view(argv[1]) == "--inspect") {
        CheckedFile bound(argv[2], argv[4]);
        gbwt::GBWTHeader header; header.load(bound.stream); header.check();
        gbwt::Tags tags; tags.simple_sds_load(bound.stream);
        const auto refs = gbwtgraph::parse_reference_samples_tag(tags.get(gbwtgraph::REFERENCE_SAMPLE_LIST_GBWT_TAG));
        const auto offset = number(argv[3]); require(offset >= 8, "invalid metadata offset");
        bound.stream.seekg(static_cast<std::streamoff>(offset-8));
        gbwt::Metadata metadata;
        require(sdsl::simple_sds::load_option(metadata,bound.stream),"missing metadata");
        bound.check();
        const auto sample = metadata.sample(argv[5]), locus = metadata.contig(argv[6]);
        const auto generic = metadata.sample("_gbwt_ref");
        uint64_t nongeneric = 0, haplotype_zero = 0;
        for (uint64_t id = 0; id < metadata.paths(); ++id) {
            const auto& path = metadata.path(id);
            if (path.sample != generic) {
                ++nongeneric;
                if (path.count == 0 && gbwtgraph::get_path_sense(metadata,path,refs) == handlegraph::PathSense::HAPLOTYPE)
                    ++haplotype_zero;
            }
            if (path.sample == sample && path.contig == locus) {
                std::cout << "path=" << id << " sample=" << metadata.sample(path.sample)
                          << " locus=" << metadata.contig(path.contig) << " phase=" << path.phase
                          << " count=" << path.count << " native="
                          << gbwtgraph::compose_path_name(metadata,path,gbwtgraph::get_path_sense(metadata,path,refs))
                          << " reference=" << gbwtgraph::compose_path_name(metadata,path,handlegraph::PathSense::REFERENCE) << '\n';
            }
        }
        std::cout << "nongeneric=" << nongeneric << " haplotype_zero=" << haplotype_zero << '\n';
        return 0;
    }
    std::map<std::string, std::string> args;
    require(argc % 2 == 1, "expected --option value pairs");
    for (int i=1; i<argc; i+=2) require(args.emplace(argv[i],argv[i+1]).second,"duplicate option");
    require(args.size() == 15, "unexpected option count");
    auto value = [&](const char* key) -> const std::string& { require(args.contains(key),key); return args.at(key); };
    const auto ledger_in=value("--ledger"), ledger_out=value("--out-ledger");
    const auto index_in=value("--annotation-index"), index_out=value("--out-index");
    const auto stats_out=value("--stats-out");
    require(!fs::exists(ledger_out) && !fs::exists(index_out) && !fs::exists(stats_out),"output exists");
    omp_set_num_threads(static_cast<int>(number(value("--threads"))));
    const auto expected_rows=number(value("--expected-rows"));
    const auto expected_changes=number(value("--expected-changes"));
    const auto expected_bytes=number(value("--expected-new-bytes"));
    std::vector<Range> ranges;
    { CheckedFile bound_ranges(value("--ranges"), value("--ranges-stat"));
      auto& input = bound_ranges.stream;
      std::string chrom; uint64_t end, generic;
      while(input >> chrom >> end >> generic) ranges.push_back({chrom,end,generic});
      require(input.eof(),"invalid range input"); bound_ranges.check(); }
    CheckedFile bound_gbwt(value("--gbwt"), value("--gbwt-stat"));
    auto& gbwt_file = bound_gbwt.stream;
    gbwt::GBWTHeader header; header.load(gbwt_file); header.check();
    require(header.get(gbwt::GBWTHeader::FLAG_SIMPLE_SDS),"metadata offset requires simple-sds GBWT");
    gbwt::Tags tags; tags.simple_sds_load(gbwt_file);
    auto refs=gbwtgraph::parse_reference_samples_tag(tags.get(gbwtgraph::REFERENCE_SAMPLE_LIST_GBWT_TAG));
    const auto offset=number(value("--metadata-offset")); require(offset>=8,"invalid metadata offset");
    gbwt_file.seekg(static_cast<std::streamoff>(offset-8));
    gbwt::Metadata metadata;
    require(sdsl::simple_sds::load_option(metadata,gbwt_file),"missing metadata");
    require(gbwt_file && static_cast<uint64_t>(gbwt_file.tellg())==bound_gbwt.size(),"metadata framing mismatch");
    bound_gbwt.check();
    require(metadata.paths()==header.sequences/2,"GBWT metadata/header mismatch");
    Resolver resolver(metadata,std::move(refs),std::move(ranges));
    require(resolver.non_generic_count() == expected_changes, "non-generic/rewrite partition mismatch");
    require(resolver.legacy_count() == expected_changes, "legacy-haplotype/rewrite partition mismatch");
    std::cerr << "repair: names_ready streaming_ledger\n";
    const auto staged=ledger_out+".partial";
    require(!fs::exists(staged),"staged ledger exists");
    std::vector<char> in_buffer(8u<<20),out_buffer(8u<<20);
    std::ifstream input; input.rdbuf()->pubsetbuf(in_buffer.data(),in_buffer.size()); input.open(ledger_in,std::ios::binary);
    std::ofstream output; output.rdbuf()->pubsetbuf(out_buffer.data(),out_buffer.size()); output.open(staged,std::ios::binary);
    require(input && output,"open ledger streams");
    // These two digests are required by PCANIDX2's source binding, not extra
    // audit scans. They consume bytes already passing through the repair.
    Digest old_hash,new_hash;
    std::map<std::string, uint64_t> changed_by_chrom;
    uint64_t rows=0,changed=0,old_bytes=0,new_bytes=0;
    std::string line;
    while(std::getline(input,line)) {
        require(!input.eof(),"ledger must end with newline");
        require(line.size() <= (16u<<20),"ledger line exceeds limit");
        old_hash.add(line); old_hash.add("\n"); old_bytes+=line.size()+1;
        if (!rows && line.starts_with("schema_version\t")) {
            require(line.starts_with("schema_version\tvg_path_name\tvg_path_length\t"),"wrong ledger header");
            output << line << '\n'; new_hash.add(line);new_hash.add("\n");new_bytes+=line.size()+1;
            // Header only once; data rows are tracked independently.
            line.clear(); break;
        }
        throw std::runtime_error("missing ledger header");
    }
    require(!line.size() && input,"empty ledger");
    while(std::getline(input,line)) {
        require(!input.eof() && line.size() <= (16u<<20),"malformed/truncated ledger line");
        require(std::count(line.begin(),line.end(),'\t')==23,"wrong ledger column count");
        const auto first=line.find('\t'),second=line.find('\t',first+1);
        require(line.substr(0,first)=="panSC-path-identity-v1","wrong ledger schema");
        const std::string_view name(line.data()+first+1,second-first-1);
        const auto match=resolver.resolve(name);
        require(!match.changed || match.legacy, "unexpected actual-suffix rewrite route");
        const std::string replacement = match.changed ? resolver.canonical_name(match.id) : std::string();
        const std::string_view canonical = match.changed ? std::string_view(replacement) : name;
        const std::string_view prefix(line.data(),first+1),suffix(line.data()+second,line.size()-second);
        old_hash.add(line);old_hash.add("\n");old_bytes+=line.size()+1;
        new_hash.add(prefix);new_hash.add(canonical);new_hash.add(suffix);new_hash.add("\n");
        output.write(prefix.data(),prefix.size());output.write(canonical.data(),canonical.size());
        output.write(suffix.data(),suffix.size());output.put('\n');
        new_bytes+=prefix.size()+canonical.size()+suffix.size()+1;
        ++rows;changed+=match.changed;
        if (match.changed) ++changed_by_chrom[std::string(name.substr(0,name.find("::")))];
        if(rows%1000000==0) std::cerr << "repair: rows="<<rows<<" rewritten="<<changed<<'\n';
    }
    require(input.eof() && !input.bad() && output,"ledger IO failed");
    output.close();require(bool(output),"ledger close failed");
    require(rows==expected_rows && changed==expected_changes,"ledger row/rewrite count mismatch");
    require(new_bytes==expected_bytes,"corrected ledger size mismatch");
    bound_gbwt.check();
    const Binding old{old_hash.finish(),old_bytes,rows}, next{new_hash.finish(),new_bytes,rows};
    std::ifstream pci_file(index_in,std::ios::binary);require(bool(pci_file),"open annotation index");
    std::string pci(fs::file_size(index_in),'\0');pci_file.read(pci.data(),pci.size());require(bool(pci_file),"read annotation index");
    pci=rebind(std::move(pci),old,next,value("--expected-index-payload"));
    require(!fs::exists(index_out+".partial"),"staged index exists");
    std::ofstream pci_out(index_out+".partial",std::ios::binary);pci_out.write(pci.data(),pci.size());pci_out.close();require(bool(pci_out),"write annotation index");
    require(!fs::exists(ledger_out) && !fs::exists(index_out),"output appeared before publish");
    fs::rename(staged,ledger_out);fs::rename(index_out+".partial",index_out);
    std::ofstream stats(stats_out);
    stats << "{\n  \"status\": \"complete\",\n  \"rows\": "<<rows<<",\n  \"rewritten\": "<<changed
          <<",\n  \"unchanged\": "<<rows-changed<<",\n  \"old_ledger_sha256\": \""<<old.hash
          <<"\",\n  \"ledger_sha256\": \""<<next.hash<<"\",\n  \"ledger_bytes\": "<<next.bytes
          <<",\n  \"rewrites_by_chromosome\": {";
    bool first_chrom = true;
    for (const auto& [chrom, count] : changed_by_chrom) {
        if (!first_chrom) stats << ',';
        stats << '"' << chrom << "\":" << count;
        first_chrom = false;
    }
    stats << "},\n  \"non_name_bytes_preserved\": true,\n  \"annotation_index_payload_sha256\": \""
          << pci.substr(pci.size()-64)<<"\"\n}\n";
    stats.close();require(bool(stats),"write stats");
    std::cerr << "repair: complete rows="<<rows<<" rewritten="<<changed<<" ledger_bytes="<<next.bytes<<'\n';
    return 0;
} catch(const std::exception& error) { std::cerr<<"repair: "<<error.what()<<'\n';return 1; }
