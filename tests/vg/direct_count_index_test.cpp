#include "direct_count_index.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <initializer_list>
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
using pancollapse::count_reference::PostingView;

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
void set32(std::string& output, std::size_t offset, std::uint32_t value) {
    require(offset <= output.size() && output.size() - offset >= 4, "test u32 range");
    for (unsigned index = 0; index < 4; ++index)
        output[offset + index] = static_cast<char>((value >> (8 * index)) & 0xffU);
}
std::uint64_t get64(std::string_view input, std::size_t offset) {
    require(offset <= input.size() && input.size() - offset >= 8, "test u64 range");
    std::uint64_t value = 0;
    for (unsigned index = 0; index < 8; ++index)
        value |= static_cast<std::uint64_t>(static_cast<unsigned char>(input[offset + index])) << (8 * index);
    return value;
}
void posting_view_boundaries() {
    std::vector<std::uint32_t> supports(9, 1);
    for (const std::uint32_t delta : {0U, 7U, 8U}) {
        for (std::uint32_t flags = 1; flags < 16; ++flags) {
            const auto code = (delta << 4) | flags;
            std::vector<std::byte> encoded;
            if (code < 128) encoded.push_back(static_cast<std::byte>(code));
            else {
                encoded.push_back(static_cast<std::byte>((code & 127U) | 128U));
                encoded.push_back(static_cast<std::byte>(code >> 7));
            }
            const auto check = [&](std::size_t count) {
                const auto view = PostingView::encoded(encoded, count, supports, 2);
                auto cursor = view.begin();
                if (count == 2) {
                    require((*cursor).model == 0 && (*cursor).flags == 1,
                            "first posting changed before boundary delta");
                    ++cursor;
                }
                const auto posting = *cursor;
                require(posting.model == delta && posting.flags == flags &&
                            posting.support_set == ((flags & 12U) ? 1U : 0U),
                        "compact first/nonfirst one-byte/two-byte boundary changed");
                require(++cursor == view.end(), "compact boundary iterator did not end");
            };
            check(1);
            if (delta != 0) { encoded.insert(encoded.begin(), std::byte{1}); check(2); }
        }
    }
    const auto rejects = [&](std::initializer_list<unsigned char> input, std::size_t count) {
        std::vector<std::byte> encoded;
        for (const auto byte : input) encoded.push_back(static_cast<std::byte>(byte));
        fails([&] {
            const auto view = PostingView::encoded(encoded, count, supports, 2);
            for (const auto posting : view) static_cast<void>(posting);
        }, "malformed compact fast-path input accepted");
    };
    rejects({0}, 1);             // zero flags, first model 0
    rejects({0x70}, 1);          // zero flags, first model 7
    rejects({1, 1}, 2);          // zero nonfirst delta
    rejects({0x81, 0}, 1);       // noncanonical multibyte code
    rejects({0x80}, 1);          // truncated multibyte code
    rejects({1, 1}, 1);          // trailing byte after terminal posting
    const std::array<std::byte, 1> model7{std::byte{0x7f}};
    fails([&] { static_cast<void>(*PostingView::encoded(
        std::span<const std::byte>(model7).first(0), 1, supports, 2).begin()); },
        "empty compact first posting accepted");
    fails([&] { static_cast<void>(*PostingView::encoded(model7, 1,
        std::span<const std::uint32_t>(supports).first(7), 2).begin()); },
        "one-byte out-of-range model accepted");
    supports[7] = 2;
    fails([&] { static_cast<void>(*PostingView::encoded(model7, 1, supports, 2).begin()); },
        "one-byte out-of-range support accepted");
}
void build(const std::filesystem::path& output) {
    // Deliberately collide EVERY annotation-list digest. Equality must compare
    // the full stored rows rather than trusting the digest as an identity.
    Writer writer(output, metadata(), 8, 8U << 20, 0);
    const std::vector<Posting> forward{{0, 1, 5}}, reverse{{1, 0, 10}};
    writer.append_node(1, 4, forward);
    writer.append_node(2, 7, forward);
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

void build_buffered(const std::filesystem::path& output) {
    constexpr std::uint32_t posting_count = 65535;
    auto data = metadata();
    data.models.assign(posting_count, ModelRecord{0, 0, 0, 0});
    data.content_id = "buffered-postings";
    Writer writer(output, std::move(data), 3, 8U << 20);
    std::vector<Posting> values;
    values.reserve(posting_count);
    for (std::uint32_t model = 0; model < posting_count; ++model)
        values.push_back({model, 0, 1});
    writer.append_node(1, 1, values);
    // This list begins one byte before the staging buffer boundary. Its duplicate
    // forces the interner to compare one flushed byte and one pending byte.
    const std::vector<Posting> boundary{{0, 0, 1}, {1, 0, 1}};
    writer.append_node(2, 1, boundary);
    writer.append_node(3, 1, boundary);
    writer.seal_nodes();
    const auto first = writer.node_view(1), second = writer.node_view(2), third = writer.node_view(3);
    require(first && second && third && second->postings.data() == third->postings.data() &&
                first->postings.size() == posting_count &&
                first->postings[posting_count - 1].model == posting_count - 1 &&
                second->postings.size() == 2 && second->postings[1].model == 1,
            "buffered posting list lost pending or mapped values");
    writer.finish();
}
}

int main() try {
    posting_view_boundaries();
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
    const auto ready_models = source->ready_models();
    require(ready_models.size() == source->model_count(), "ready model table size");
    for (std::uint32_t i = 0; i < ready_models.size(); ++i) {
        const auto expected = source->model(i);
        const auto actual = ready_models[i];
        require(actual.transcript_string == expected.transcript_string &&
                    actual.exon_policy == expected.exon_policy &&
                    actual.body_policy == expected.body_policy && actual.strand == expected.strand,
                "ready model table changed mapped values");
    }
    const auto one = source->node_view(1), two = source->node_view(2), three = source->node_view(3);
    require(one && two && three && one->length == 4 && two->length == 7 && three->length == 5,
            "node lengths");
    require(one->postings.size() == 1 && one->postings[0].model == 0 &&
            one->postings[0].support_set == 1 && one->postings[0].flags == 5 &&
            three->postings[0].model == 1 &&
            three->postings[0].flags == 10, "digest collision changed memberships");
    require(one->postings.data() == two->postings.data() &&
                one->shared_membership_id != missing &&
                one->shared_membership_id == two->shared_membership_id &&
                one->shared_membership_id != three->shared_membership_id,
            "reader must expose stable identities for equal and distinct posting lists");
    require(source->node_view(4) && source->node_view(4)->length == 0 &&
            source->node_view(4)->postings.empty(), "zero-length node became absent");
    require(source->node_view(6) && source->node_view(6)->postings.empty(),
            "valid unannotated node lost");
    require(source->node_view(4)->shared_membership_id == 0 &&
                source->node_view(4)->shared_membership_id == source->node_view(6)->shared_membership_id,
            "empty posting list identity must use valid list zero");
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

    const auto buffered = temp.path / "buffered.pci";
    const auto buffered_second = temp.path / "buffered-identical.pci";
    build_buffered(buffered);
    build_buffered(buffered_second);
    auto buffered_source = Source::load(buffered);
    const auto buffered_first = buffered_source->node_view(1);
    const auto buffered_second_view = buffered_source->node_view(2);
    const auto buffered_third_view = buffered_source->node_view(3);
    require(buffered_first && buffered_second_view &&
                buffered_third_view && buffered_first->postings.size() == 65535 &&
                buffered_first->postings[65534].model == 65534 &&
                buffered_second_view->postings.size() == 2 &&
                buffered_second_view->postings[1].model == 1 &&
                buffered_second_view->postings.data() == buffered_third_view->postings.data(),
            "buffered publication changed large posting lists");
    require(bytes(buffered) == bytes(buffered_second),
            "buffered output changed canonical bytes");

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
    const auto old_version = temp.path / "old-version.pci";
    broken = original;
    set32(broken, 8, 1);
    write_bytes(old_version, broken);
    require(!is_index(old_version), "old direct-index version accepted");
    fails([&] { Source::load(old_version); }, "old direct-index version loaded");

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
