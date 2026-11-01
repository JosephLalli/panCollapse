#include "direct_count_index.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace pancollapse::direct_index;
using pancollapse::count_reference::missing;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void fails(F&& operation, const char* message) {
    bool failed = false;
    try { operation(); } catch (const std::exception&) { failed = true; }
    require(failed, message);
}
struct Temp {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("pancollapse-direct-storage-" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()));
    Temp() { std::filesystem::create_directory(path); }
    ~Temp() { std::error_code error; std::filesystem::remove_all(path, error); }
};
Metadata metadata() {
    Metadata result;
    result.strings = {"A", "B", "TX_A", "TX_B", "protein_coding"};
    result.feature_catalog = {"A", "B"};
    result.sets = {{}, {4}};
    result.genes = {{0, missing, 1, {0, 0}, {missing, missing}, {0, 0}},
                    {1, missing, 1, {1, 1}, {missing, missing}, {0, 0}}};
    result.policies = {{0, 0, missing, 0}, {1, 0, missing, 0}};
    result.models = {{2, 0, 0, 0}, {3, 1, 1, 1}};
    result.content_id = "fixed-storage-fixture";
    result.provenance_json = "{\"source\":\"fixed-fixture\"}";
    return result;
}
std::string bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
void write_bytes(const std::filesystem::path& path, std::string_view value) {
    std::ofstream output(path, std::ios::binary);
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    require(bool(output), "test file write");
}
void put32(std::string& output, std::uint32_t value) {
    for (unsigned shift = 0; shift < 32; shift += 8)
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
}
void set32(std::string& output, std::size_t offset, std::uint32_t value) {
    require(offset <= output.size() && output.size() - offset >= 4, "test u32 range");
    for (unsigned index = 0; index < 4; ++index)
        output[offset + index] = static_cast<char>((value >> (8 * index)) & 0xffU);
}
void put64(std::string& output, std::uint64_t value) {
    for (unsigned shift = 0; shift < 64; shift += 8)
        output.push_back(static_cast<char>((value >> shift) & 0xffU));
}
std::uint64_t get64(std::string_view input, std::size_t offset) {
    require(offset <= input.size() && input.size() - offset >= 8, "test u64 range");
    std::uint64_t value = 0;
    for (unsigned index = 0; index < 8; ++index)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(input[offset + index])) << (8 * index);
    return value;
}
template<class T> void append_rows(std::string& output, std::span<const T> values) {
    if (!values.empty())
        output.append(reinterpret_cast<const char*>(values.data()), values.size_bytes());
}
void append_strings(std::string& output, const std::vector<std::string>& values) {
    for (const auto& value : values) {
        put32(output, static_cast<std::uint32_t>(value.size()));
        output.append(value);
    }
}
void append_sets(std::string& output, const std::vector<std::vector<std::uint32_t>>& values) {
    for (const auto& value : values) {
        put64(output, value.size());
        append_rows(output, std::span<const std::uint32_t>(value));
    }
}
struct LegacyNodeSlot { std::uint32_t length = 0, list = missing; };
struct LegacyPairRow {
    std::uint64_t from = 0, to = 0, first = 0;
    std::uint32_t count = 0, reserved = 0;
};
static_assert(std::endian::native == std::endian::little &&
              sizeof(LegacyNodeSlot) == 8 && sizeof(LegacyPairRow) == 32);

std::string legacy_fixture() {
    constexpr std::size_t section_count = 13;
    constexpr std::uint64_t header_bytes = 32 + section_count * 16;
    const auto data = metadata();
    std::array<std::string, section_count> sections;
    auto& meta = sections[0];
    for (const auto value : std::array<std::uint64_t, 14>{
             8, data.strings.size(), data.feature_catalog.size(), data.sets.size(),
             data.genes.size(), data.policies.size(), data.models.size(), 3,
             data.content_id.size(), 1, 2, 2, 1, 2}) put64(meta, value);
    meta.append(data.content_id);
    append_strings(sections[1], data.strings);
    append_strings(sections[2], data.feature_catalog);
    append_sets(sections[3], data.sets);
    append_rows(sections[4], std::span<const GeneRecord>(data.genes));
    append_rows(sections[5], std::span<const PolicyRecord>(data.policies));
    append_rows(sections[6], std::span<const ModelRecord>(data.models));
    std::vector<LegacyNodeSlot> slots(9);
    slots[1] = {4, 1}; slots[2] = {4, 1}; slots[3] = {5, 2};
    slots[4] = {0, 0}; slots[6] = {9, 0};
    append_rows(sections[7], std::span<const LegacyNodeSlot>(slots));
    for (const auto offset : {0ULL, 0ULL, 1ULL, 2ULL}) put64(sections[8], offset);
    const std::vector<Posting> postings{{0, 1, 5}, {1, 0, 10}};
    append_rows(sections[9], std::span<const Posting>(postings));
    const LegacyPairRow splice{1, 3, 0, 1, 0};
    const std::uint32_t target = 2;
    append_rows(sections[10], std::span<const LegacyPairRow>(&splice, 1));
    append_rows(sections[10], std::span<const std::uint32_t>(&target, 1));
    const std::array<LegacyPairRow, 2> junctions{{{2, 7, 0, 1, 0}, {6, 3, 1, 1, 0}}};
    const std::array<JunctionOwner, 2> owners{{{2, 1}, {2, 2}}};
    append_rows(sections[11], std::span<const LegacyPairRow>(junctions));
    append_rows(sections[11], std::span<const JunctionOwner>(owners));
    sections[12] = data.provenance_json;

    std::array<std::pair<std::uint64_t, std::uint64_t>, section_count> ranges{};
    std::uint64_t offset = header_bytes;
    for (std::size_t index = 0; index < sections.size(); ++index) {
        ranges[index] = {offset, sections[index].size()};
        offset = (offset + sections[index].size() + 7) & ~std::uint64_t{7};
    }
    const auto final_size = ((offset + 7) & ~std::uint64_t{7}) + 16;
    std::string output = "PCDIRECT";
    put32(output, 1); put32(output, 1);
    put64(output, 0x0102030405060708ULL); put64(output, section_count);
    for (const auto [start, size] : ranges) { put64(output, start); put64(output, size); }
    for (std::size_t index = 0; index < sections.size(); ++index) {
        output.resize(static_cast<std::size_t>(ranges[index].first), '\0');
        output.append(sections[index]);
    }
    output.resize(static_cast<std::size_t>(final_size - 16), '\0');
    output.append("PCDEND01", 8);
    put64(output, final_size);
    require(output.size() == final_size, "legacy fixture framing");
    return output;
}

void build(const std::filesystem::path& output) {
    // Deliberately collide EVERY annotation-list digest. Equality must compare
    // the full stored rows rather than trusting the digest as an identity.
    Writer writer(output, metadata(), 8, 8U << 20, 0);
    const std::vector<Posting> forward{{0, 1, 5}}, reverse{{1, 0, 10}};
    writer.append_node(1, 4, forward);
    writer.append_node(2, 4, forward);
    writer.append_node(3, 5, reverse);
    writer.append_node(4, 0, {});
    writer.append_node(6, 9, {});
    writer.seal_nodes();
    const auto one = writer.node_view(1), two = writer.node_view(2);
    require(one && two && one->postings.data() == two->postings.data(),
            "writer did not intern identical lists");
    require(writer.node_view(4) && writer.node_view(4)->length == 0,
            "zero-length node lost before publication");
    require(!writer.node_view(5), "missing node became unannotated");
    const std::vector<std::uint32_t> targets{2};
    const std::vector<JunctionOwner> along{{2, 1}}, reverse_owner{{2, 2}};
    writer.append_splice(1, 3, targets);
    writer.append_junction(2, 7, along);
    writer.append_junction(6, 3, reverse_owner);
    writer.finish();
    fails([&] { writer.finish(); }, "double finish accepted");
}
}

int main() try {
    Temp temp;
    const auto path = temp.path / "direct.pci";
    build(path);
    require(is_index(path), "completed index not detected");
    auto source = Source::load(path);
    require(source->strings() == metadata().strings && source->sets() == metadata().sets,
            "dictionary round trip");
    require(source->content_id() == metadata().content_id &&
            source->provenance_json() == metadata().provenance_json,
            "source identity round trip");
    require(source->model_count() == 2 && source->model(1).transcript_string == 3,
            "ready model round trip");
    const auto one = source->node_view(1), two = source->node_view(2), three = source->node_view(3);
    require(one && two && three && one->length == 4 && three->length == 5,
            "node lengths");
    require(one->postings.size() == 1 && one->postings[0].model == 0 &&
            one->postings[0].support_set == 1 && one->postings[0].flags == 5 &&
            three->postings[0].model == 1 &&
            three->postings[0].flags == 10, "digest collision changed memberships");
    require(one->postings.data() == two->postings.data(), "reader expanded shared lists");
    require(source->node_view(4) && source->node_view(4)->length == 0 &&
            source->node_view(4)->postings.empty(), "zero-length node became absent");
    require(source->node_view(6) && source->node_view(6)->postings.empty(),
            "valid unannotated node lost");
    require(!source->node_view(0) && !source->node_view(5) && !source->node_view(9),
            "absent-node sentinel");
    for (const auto [a, b] : {std::pair{1ULL, 3ULL}, std::pair{3ULL, 1ULL}}) {
        const auto targets = source->splice_targets(a, b);
        require(targets.size() == 1 && targets[0] == 2, "physical splice target");
    }
    const auto along = source->junction_owners(2, 7), reverse = source->junction_owners(6, 3);
    require(along.size() == 1 && along[0].transcript_string == 2 && along[0].flags == 1 &&
            reverse.size() == 1 && reverse[0].flags == 2, "directional ownership");
    require(source->junction_owners(2, 6).empty() && !source->splice_targets(1, 3).empty(),
            "known target without compatible owner became unknown");
    require(source->splice_targets(1, 2).empty(), "unknown splice acquired a target");
    fails([&] { static_cast<void>(source->model(2)); }, "invalid model accepted");
    fails([&] { static_cast<void>(source->node(1)); }, "legacy lookup silently accepted");
    require(source->take_feature_catalog() == metadata().feature_catalog,
            "feature dictionary round trip");

    const auto second = temp.path / "identical.pci";
    build(second);
    require(bytes(path) == bytes(second), "output contains volatile identity or ordering");
    const auto copied = temp.path / "copied-v2.pci";
    repack(path, copied, 8U << 20);
    require(bytes(path) == bytes(copied), "V2 canonical repack changed bytes");

    const auto legacy = temp.path / "legacy-v1.pci";
    write_bytes(legacy, legacy_fixture());
    require(is_index(legacy), "legacy V1 index not detected");
    auto legacy_source = Source::load(legacy);
    const auto legacy_one = legacy_source->node_view(1);
    require(legacy_one && legacy_one->postings.size() == 1 &&
            legacy_one->postings[0].model == 0 && legacy_one->postings[0].support_set == 1 &&
            legacy_one->postings[0].flags == 5, "legacy V1 facts changed");
    const auto repacked = temp.path / "repacked-v2.pci";
    repack(legacy, repacked, 8U << 20);
    auto repacked_source = Source::load(repacked);
    const auto retained = repacked_source->node_view(1);
    require(retained && retained->postings.size() == 1, "repacked node missing");
    static_cast<void>(repacked_source->node_view(3));
    const auto retained_posting = *retained->postings.begin();
    require(retained_posting.model == 0 && retained_posting.support_set == 1 &&
            retained_posting.flags == 5, "compact iterator lifetime or support changed");
    require(repacked_source->splice_targets(3, 1).size() == 1 &&
            repacked_source->junction_owners(6, 3).size() == 1,
            "repack changed splice or junction facts");

    const auto legacy_posting_offset = static_cast<std::size_t>(
        get64(legacy_fixture(), 32 + 9 * 16));
    auto noncanonical_support = legacy_fixture();
    set32(noncanonical_support, legacy_posting_offset + 8, 1);  // exon-only flags
    const auto noncanonical_support_path = temp.path / "legacy-nonbody-support-v1.pci";
    write_bytes(noncanonical_support_path, noncanonical_support);
    const auto rejected_noncanonical = temp.path / "rejected-nonbody-support-v2.pci";
    fails([&] { repack(noncanonical_support_path, rejected_noncanonical, 8U << 20); },
          "V1 non-body support was silently changed");
    require(!std::filesystem::exists(rejected_noncanonical),
            "failed non-body support repack published output");

    auto conflicting_support = legacy_fixture();
    set32(conflicting_support, legacy_posting_offset + sizeof(Posting), 0);      // model
    set32(conflicting_support, legacy_posting_offset + sizeof(Posting) + 8, 5); // body flags
    const auto conflicting_support_path = temp.path / "legacy-conflicting-support-v1.pci";
    write_bytes(conflicting_support_path, conflicting_support);
    const auto rejected_conflict = temp.path / "rejected-conflicting-support-v2.pci";
    fails([&] { repack(conflicting_support_path, rejected_conflict, 8U << 20); },
          "V1 conflicting body supports were silently changed");
    require(!std::filesystem::exists(rejected_conflict),
            "failed conflicting support repack published output");

    const auto original = bytes(path);
    fails([&] { Writer writer(path, metadata(), 8, 8U << 20); }, "existing output accepted");
    require(bytes(path) == original, "existing output modified");
    const auto truncated = temp.path / "truncated.pci";
    write_bytes(truncated, std::string_view(original).substr(0, 128));
    fails([&] { Source::load(truncated); }, "truncated file accepted");
    const auto corrupt = temp.path / "corrupt.pci";
    auto broken = original;
    broken[0] ^= 1;
    write_bytes(corrupt, broken);
    require(!is_index(corrupt), "corrupt magic accepted");
    fails([&] { Source::load(corrupt); }, "corrupt file loaded");

    unsigned counter = 0;
    const auto invalid = [&](auto operation) {
        const auto output = temp.path / ("invalid-" + std::to_string(counter++));
        fails([&] { Writer writer(output, metadata(), 8, 8U << 20); operation(writer); },
              "invalid index construction accepted");
        require(!std::filesystem::exists(output), "failed build published an output");
    };
    invalid([](Writer& w) { w.append_node(1, std::uint64_t{UINT32_MAX} + 1, {}); });
    invalid([](Writer& w) { const std::vector<Posting> p{{2, 0, 1}}; w.append_node(1, 1, p); });
    invalid([](Writer& w) { const std::vector<Posting> p{{0, 9, 4}}; w.append_node(1, 1, p); });
    invalid([](Writer& w) { const std::vector<Posting> p{{0, 1, 1}}; w.append_node(1, 1, p); });
    invalid([](Writer& w) {
        const std::vector<Posting> first{{0, 1, 4}}, second{{0, 0, 4}};
        w.append_node(1, 1, first); w.append_node(2, 1, second);
    });
    invalid([](Writer& w) { const std::vector<Posting> p{{1, 0, 1}, {0, 0, 1}}; w.append_node(1, 1, p); });
    invalid([](Writer& w) { const std::vector<Posting> p{{0, 0, 1}, {0, 0, 2}}; w.append_node(1, 1, p); });
    invalid([](Writer& w) { w.append_node(1, 1, {}); w.append_node(1, 1, {}); });
    invalid([](Writer& w) { w.seal_nodes(); w.append_node(1, 1, {}); });
    invalid([](Writer& w) { w.seal_nodes(); const std::vector<std::uint32_t> p{99}; w.append_splice(1, 3, p); });
    fails([&] { Writer writer(temp.path / "small", metadata(), 8, 1); }, "build budget ignored");
    fails([&] { Writer writer(temp.path / "overflow", metadata(), UINT64_MAX, 1U << 20); },
          "node-directory overflow accepted");

    auto wide = metadata();
    wide.models.assign(8201, ModelRecord{2, 0, 0, 0});
    wide.content_id = "varint-widths";
    const auto wide_path = temp.path / "varint-widths.pci";
    {
        Writer writer(wide_path, std::move(wide), 1, 8U << 20);
        const std::vector<Posting> values{{0, 0, 1}, {8, 0, 1}, {8200, 0, 1}};
        writer.append_node(1, 1, values);
        writer.finish();
    }
    auto wide_source = Source::load(wide_path);
    const auto wide_view = wide_source->node_view(1);
    std::vector<std::uint32_t> wide_models;
    for (const auto posting : wide_view->postings) wide_models.push_back(posting.model);
    require(wide_models == std::vector<std::uint32_t>{0, 8, 8200},
            "one/two/three-byte model deltas changed");
    const auto wide_bytes = bytes(wide_path);
    const auto posting_offset = get64(wide_bytes, 32 + 9 * 16);
    const auto posting_bytes = get64(wide_bytes, 32 + 9 * 16 + 8);
    require(posting_bytes >= 6 && posting_offset <= wide_bytes.size() &&
            posting_bytes <= wide_bytes.size() - posting_offset, "test posting range");
    auto overflow_varint = wide_bytes;
    for (std::size_t index = 0; index < 6; ++index)
        overflow_varint[static_cast<std::size_t>(posting_offset) + index] = static_cast<char>(0xff);
    const auto overflow_varint_path = temp.path / "overflow-varint.pci";
    write_bytes(overflow_varint_path, overflow_varint);
    auto overflow_source = Source::load(overflow_varint_path);
    fails([&] {
        const auto view = overflow_source->node_view(1);
        static_cast<void>(*view->postings.begin());
    }, "overflowing posting varint accepted");
    auto truncated_varint = wide_bytes;
    const auto final_posting = static_cast<std::size_t>(posting_offset + posting_bytes - 1);
    truncated_varint[final_posting] = static_cast<char>(
        static_cast<unsigned char>(truncated_varint[final_posting]) | 0x80U);
    const auto truncated_varint_path = temp.path / "truncated-varint.pci";
    write_bytes(truncated_varint_path, truncated_varint);
    auto truncated_source = Source::load(truncated_varint_path);
    fails([&] {
        const auto view = truncated_source->node_view(1);
        for (const auto posting : view->postings) static_cast<void>(posting);
    }, "truncated posting varint accepted");

    Metadata empty;
    empty.sets = {{}};
    empty.content_id = "empty";
    const auto empty_path = temp.path / "empty.pci";
    { Writer writer(empty_path, std::move(empty), 0, 1U << 20); writer.finish(); }
    auto empty_source = Source::load(empty_path);
    require(empty_source->model_count() == 0 && !empty_source->node_view(0) &&
            empty_source->take_feature_catalog().empty(), "empty index round trip");
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
