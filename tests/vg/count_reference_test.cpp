#include "count_reference.hpp"

#include <filesystem>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace pancollapse::count_reference;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
template<class F> void rejects(F&& action, const char* message) {
    try { action(); } catch (const std::exception&) { return; }
    throw std::runtime_error(message);
}
std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}
template<class T> EncodedSpan append_raw(Data& data, const std::vector<T>& values) {
    constexpr std::uint32_t raw = std::uint32_t{1} << 31;
    const auto first = static_cast<std::uint64_t>(data.encoded.size());
    const auto bytes = values.size() * sizeof(T);
    data.encoded.resize(data.encoded.size() + bytes);
    if (bytes != 0) std::memcpy(data.encoded.data() + first, values.data(), bytes);
    return {first, raw | static_cast<std::uint32_t>(bytes), static_cast<std::uint32_t>(values.size())};
}

int main() try {
    const auto root = std::filesystem::temp_directory_path() / "pancollapse-count-reference-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    const auto first = root / "first.pcr";
    const auto second = root / "second.pcr";

    Data empty;
    empty.content_id = "empty";
    write(first, empty);
    auto empty_reference = Reference::load(first);
    require(empty_reference.content_id() == "empty" && empty_reference.genes().empty() &&
            empty_reference.node(1) == nullptr, "empty roundtrip");

    Data data;
    data.content_id = "stable-content";
    data.strings = {"gene-id", "GENE", "origin", "transcript", "splice-target"};
    data.sets = {{1, 4}, {7}};
    GeneRecord gene; gene.id_string = 0; gene.name_string = 1; gene.type_set = 0;
    gene.equivalence[0] = 0; gene.competition[0] = 0; gene.competition[1] = 1;
    data.genes = {gene};
    data.policies = {{0, 1, 0, 3}};
    data.models = {{3, 0, 0, 1}};
    data.nodes = {{5, 12, 0, 2, 0, 0, 1, 0},
                  {(std::uint64_t{1} << 55), 9, 2, 1, 0, 1, 1, 0}};
    data.postings = {{0, 0, 1}, {0, 1, 10}, {0, 0, 4}};
    data.scoring_postings = {{0, 7, 3}, {1, 2, 9}};
    data.scoring_classes = {{0, 1, 0}, {1, 0, 1}};
    data.scoring_bindings = {{3, 0, 2, 0}};
    data.edges = {{10, 11, 0, 1, 0}};
    data.edge_postings = {{0, 3}};
    data.splice_records = {{5, (std::uint64_t{1} << 55), 0, 1, 0}};
    data.splice_targets = {4};
    data.junction_records = {{10, 11, 0, 1, 0}};
    data.junction_owners = {{3, 3}};
    write(first, data); write(second, data);
    require(bytes(first) == bytes(second), "deterministic bytes");
    require(is_count_reference(first), "cheap magic probe");

    auto reference = Reference::load(first);
    require(reference.strings() == data.strings && reference.sets() == data.sets &&
            reference.content_id() == data.content_id, "metadata roundtrip");
    require(reference.genes().size() == 1 && reference.policies().size() == 1 &&
            reference.models().size() == 1, "fixed metadata roundtrip");
    const NodeRecord* small = reference.node(5);
    const NodeRecord* large = reference.node(std::uint64_t{1} << 55);
    require(small != nullptr && large != nullptr && reference.node(6) == nullptr, "node lookup");
    const auto small_postings = reference.node_postings(*small);
    require(small_postings.size() == 2 && small_postings[1].flags == 10, "repeated node postings");
    require(reference.node_postings(*large).size() == 1, "large node id");
    const auto small_scoring = reference.node_scoring(*small);
    require(small_scoring.size() == 1 && small_scoring.front().class_id == 0 &&
            small_scoring.front().forward == 7 && small_scoring.front().reverse == 3,
            "node scoring roundtrip");
    const auto classes = reference.scoring_classes();
    require(classes.size() == 2 && reference.scoring_bindings(classes.front()).size() == 1 &&
            reference.scoring_bindings(classes[1]).empty(), "scoring class bindings roundtrip");
    const auto edge_postings = reference.edge(10, 11);
    require(edge_postings.size() == 1 && edge_postings.front().flags == 3 &&
            reference.edge(10, 12).empty(), "edge lookup and flags");
    const auto splice_forward = reference.splice_targets(5, std::uint64_t{1} << 55);
    const auto splice_reverse = reference.splice_targets(std::uint64_t{1} << 55, 5);
    require(splice_forward.size() == 1 && splice_forward.front() == 4 &&
            splice_reverse.size() == 1 && splice_reverse.front() == 4 &&
            reference.splice_targets(5, 5).empty(), "canonical splice target lookup");
    const auto junction = reference.junction_owners(10, 11);
    require(junction.size() == 1 && junction.front().transcript_string == 3 &&
            junction.front().flags == 3 && reference.junction_owners(11, 10).empty(),
            "oriented junction owner lookup");

    Data colored = data;
    colored.postings.clear(); colored.scoring_postings.clear(); colored.edges.clear();
    colored.edge_postings.clear();
    for (auto& node : colored.nodes) {
        node.first = node.count = node.score_first = node.score_count = 0;
    }
    const auto node_a = append_raw(colored, std::vector<Posting>{{0, 0, 1}});
    const auto node_b = append_raw(colored, std::vector<Posting>{{0, 0, 4}});
    const auto node_dense = append_raw(colored, std::vector<Posting>{{0, 1, 2}});
    const auto score = append_raw(colored, std::vector<ScoringPosting>{{0, 7, 3}});
    const auto encoded_edge = append_raw(colored, std::vector<EdgePosting>{{0, 3}});
    colored.node_blocks = {{0, {1, 0, 0, 0}, node_a}, {0, {1, 0, 0, 0}, node_b},
                           {0, {2, 0, 0, 0}, node_dense}};
    colored.scoring_blocks = {{0, {3, 0, 0, 0}, score}};
    colored.edge_keys = {{10, 11, 131}};
    colored.edge_blocks = {{2, std::uint64_t{1} << 3, encoded_edge}};
    const auto colored_path = root / "colored.pcr";
    write(colored_path, colored);
    auto colored_reference = Reference::load(colored_path);
    const auto colored_small = colored_reference.node_postings(*colored_reference.node(5));
    const auto colored_large = colored_reference.node_postings(*colored_reference.node(std::uint64_t{1} << 55));
    require(colored_small.size() == 1 && colored_small.front().flags == 5 &&
            colored_large.size() == 1 && colored_large.front().support_set == 1,
            "sparse/dense colored blocks merge sorted postings");
    require(colored_reference.node_scoring(*colored_reference.node(5)).size() == 1 &&
            colored_reference.node_scoring(*colored_reference.node(std::uint64_t{1} << 55)).size() == 1,
            "colored scoring blocks cover multiple rank bits");
    require(colored_reference.edge(10, 11).size() == 1 &&
            colored_reference.edge(10, 11).front().flags == 3,
            "encoded edge payload lookup");

    // The file format sees a single contiguous encoded section even when the
    // builder retained the payload in fixed append-only slabs.
    Data slabbed = colored;
    EncodedBuffer small_slabs(7);
    const std::size_t split = slabbed.encoded.size() < 3 ? slabbed.encoded.size() : 3;
    small_slabs.append(std::span<const std::byte>(slabbed.encoded.data(), split));
    small_slabs.append(std::span<const std::byte>(slabbed.encoded.data() + split,
                                                  slabbed.encoded.size() - split));
    slabbed.encoded.clear();
    slabbed.encoded_slabs = std::move(small_slabs);
    std::uint64_t slab_capacity = 0;
    for (const auto& slab : slabbed.encoded_slabs.slabs()) {
        require(slab.size() <= 7 && slab.size() <= slab.capacity(), "slab capacity bound");
        slab_capacity += slab.capacity();
    }
    require(slabbed.encoded_slabs.size() == colored.encoded.size() &&
            slabbed.encoded_slabs.slabs().size() > 1 &&
            slabbed.encoded_slabs.allocated_bytes() == slab_capacity &&
            encoded_size(slabbed) == colored.encoded.size(),
            "slab multi-append crosses fixed boundaries");
    EncodedBuffer copied_slabs = slabbed.encoded_slabs;
    const std::byte copied_tail{0};
    copied_slabs.append(std::span<const std::byte>(&copied_tail, 1));
    require(copied_slabs.slabs().back().size() <= copied_slabs.slab_capacity() &&
            copied_slabs.slabs().back().capacity() >= copied_slabs.slab_capacity(),
            "copied partial slab restores fixed capacity before append");
    EncodedBuffer moved_slabs = std::move(copied_slabs);
    copied_slabs.append(std::span<const std::byte>(&copied_tail, 1));
    std::uint64_t moved_from_used = 0;
    for (const auto& slab : copied_slabs.slabs()) moved_from_used += slab.size();
    require(!moved_slabs.slabs().empty() && copied_slabs.size() == moved_from_used &&
            copied_slabs.slab_capacity() == 7,
            "moved-from buffer resets logical bytes before reuse");
    const auto slabbed_path = root / "colored-slabs.pcr";
    write(slabbed_path, slabbed);
    require(bytes(colored_path) == bytes(slabbed_path), "slabbed payload preserves file bytes");
    auto slabbed_reference = Reference::load(slabbed_path);
    require(slabbed_reference.node_postings(*slabbed_reference.node(5)).size() == 1 &&
            slabbed_reference.node_scoring(*slabbed_reference.node(5)).size() == 1 &&
            slabbed_reference.edge(10, 11).size() == 1,
            "slabbed payload mmap lookup");
    EncodedBuffer empty_slabs(7);
    require(empty_slabs.size() == 0 && empty_slabs.allocated_bytes() == 0,
            "empty slabs allocate nothing");
    rejects([] { static_cast<void>(EncodedBuffer(0)); }, "zero slab capacity rejected");

    Data invalid = data;
    invalid.scoring_classes[0].count = 0;
    rejects([&] { write(root / "invalid-scoring.pcr", invalid); }, "unbound non-body scoring class rejected");
    invalid = data;
    invalid.scoring_bindings[0].multiplicity = 0;
    rejects([&] { write(root / "invalid-binding.pcr", invalid); }, "zero scoring multiplicity rejected");
    invalid = data;
    invalid.splice_records[0].from = invalid.splice_records[0].to;
    rejects([&] { write(root / "invalid-splice.pcr", invalid); }, "invalid physical splice rejected");
    invalid = data;
    invalid.junction_owners[0].flags = 4;
    rejects([&] { write(root / "invalid-junction-owner.pcr", invalid); }, "invalid junction owner rejected");
    invalid = colored;
    invalid.node_blocks[0].values.first = invalid.encoded.size();
    rejects([&] { write(root / "invalid-encoded-bounds.pcr", invalid); }, "invalid encoded bounds rejected");
    invalid = colored;
    invalid.node_blocks[0].values.bytes &= ~(std::uint32_t{1} << 31);
    const auto invalid_payload = root / "invalid-encoded-payload.pcr";
    write(invalid_payload, invalid);
    auto invalid_reference = Reference::load(invalid_payload);
    rejects([&] { static_cast<void>(invalid_reference.node_postings(*invalid_reference.node(5))); },
            "truncated compressed payload rejected on touch");

    const auto bad_header = root / "bad-header.pcr";
    { std::ofstream out(bad_header, std::ios::binary); out << "not-a-reference"; }
    require(!is_count_reference(bad_header), "invalid header probe");
    rejects([&] { static_cast<void>(Reference::load(bad_header)); }, "invalid header rejected");
    const auto truncated = root / "truncated.pcr";
    std::filesystem::copy_file(first, truncated);
    std::filesystem::resize_file(truncated, 20);
    rejects([&] { static_cast<void>(Reference::load(truncated)); }, "truncation rejected");

    std::filesystem::remove_all(root);
    std::cout << "count reference codec checks passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
