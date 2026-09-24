#include "read_granular_input.hpp"

#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>

#include <cstdint>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void check(bool value, const std::string& message) {
    if (!value) throw std::runtime_error(message);
}

// The oracle is the full protobuf decoder: whenever both succeed they must agree exactly.
void expect_agrees(const std::string& bytes, const std::string& label) {
    pancollapse::input::MultipathRecordPeek peek;
    const bool peeked = pancollapse::input::peek_multipath_record(bytes, peek);
    vg::MultipathAlignment decoded;
    const bool parsed = decoded.ParseFromString(bytes);
    if (peeked && parsed) {
        check(peek.name == decoded.name(), label + ": name differs from decoder");
        check(peek.has_subpath == (decoded.subpath_size() > 0),
              label + ": subpath presence differs from decoder");
    }
}

void append_varint(std::string& out, std::uint64_t value) {
    while (value >= 0x80) {
        out.push_back(static_cast<char>((value & 0x7F) | 0x80));
        value >>= 7;
    }
    out.push_back(static_cast<char>(value));
}

void append_length_delimited(std::string& out, std::uint64_t field, const std::string& body) {
    append_varint(out, (field << 3) | 2);
    append_varint(out, body.size());
    out += body;
}

vg::MultipathAlignment dense_record(const std::string& name, int subpaths) {
    vg::MultipathAlignment record;
    record.set_sequence("ACGTACGT");
    record.set_quality("IIIIIIII");
    record.set_name(name);
    record.set_mapping_quality(60);
    for (int i = 0; i < subpaths; ++i) {
        vg::Subpath* subpath = record.add_subpath();
        subpath->set_score(i);
        for (int m = 0; m < 3; ++m) {
            vg::Mapping* mapping = subpath->mutable_path()->add_mapping();
            mapping->mutable_position()->set_node_id(1000 + i * 3 + m);
            vg::Edit* edit = mapping->add_edit();
            edit->set_from_length(2);
            edit->set_to_length(2);
        }
        if (i + 1 < subpaths) subpath->add_next(i + 1);
    }
    record.add_start(0);
    return record;
}

void test_constructed_cases() {
    expect_agrees(std::string(), "empty message");

    vg::MultipathAlignment name_only;
    name_only.set_name("CB:UMI:read1");
    expect_agrees(name_only.SerializeAsString(), "name only");

    vg::MultipathAlignment unnamed = dense_record("", 4);
    unnamed.clear_name();
    expect_agrees(unnamed.SerializeAsString(), "subpaths without name");

    expect_agrees(dense_record("dense", 700).SerializeAsString(), "dense record");

    // An empty embedded subpath still adds one element.
    std::string empty_subpath;
    append_length_delimited(empty_subpath, 3, "read-empty-subpath");
    append_length_delimited(empty_subpath, 6, std::string());
    expect_agrees(empty_subpath, "empty embedded subpath");
    pancollapse::input::MultipathRecordPeek peek;
    check(pancollapse::input::peek_multipath_record(empty_subpath, peek) && peek.has_subpath,
          "empty embedded subpath must count as a subpath");

    // Out of canonical order, and a repeated name: proto3 keeps the last one.
    std::string reordered;
    append_length_delimited(reordered, 6, dense_record("x", 1).subpath(0).SerializeAsString());
    append_length_delimited(reordered, 3, "first-name");
    append_length_delimited(reordered, 1, "ACGT");
    append_length_delimited(reordered, 3, "last-name");
    expect_agrees(reordered, "reordered with repeated name");
    check(pancollapse::input::peek_multipath_record(reordered, peek) && peek.name == "last-name",
          "the last name must win");

    // Concatenated serializations merge, as the decoder does.
    const std::string merged = dense_record("a", 2).SerializeAsString() +
        dense_record("b", 0).SerializeAsString();
    expect_agrees(merged, "concatenated messages");

    // Unknown fields of every scalar wire type are skipped.
    std::string unknown = name_only.SerializeAsString();
    append_varint(unknown, (50ULL << 3) | 0);
    append_varint(unknown, 123456789);
    append_varint(unknown, (51ULL << 3) | 1);
    unknown.append(8, '\x01');
    append_varint(unknown, (52ULL << 3) | 5);
    unknown.append(4, '\x02');
    append_length_delimited(unknown, 100, "annotation-bytes");
    expect_agrees(unknown, "unknown fields");
    check(pancollapse::input::peek_multipath_record(unknown, peek) && peek.name == "CB:UMI:read1",
          "unknown fields must be skipped");

    // Encodings the scan does not model are declined, never guessed.
    // End the message inside its last subpath, not at a field boundary.
    vg::MultipathAlignment ends_in_subpath = dense_record("t", 3);
    ends_in_subpath.set_mapping_quality(0);
    ends_in_subpath.clear_start();
    std::string truncated = ends_in_subpath.SerializeAsString();
    truncated.resize(truncated.size() - 1);
    check(!pancollapse::input::peek_multipath_record(truncated, peek), "truncated must decline");
    std::string group;
    append_varint(group, (40ULL << 3) | 3);
    append_varint(group, (40ULL << 3) | 4);
    check(!pancollapse::input::peek_multipath_record(group, peek), "groups must decline");
    std::string wrong_wire;
    append_varint(wrong_wire, (3ULL << 3) | 0);
    append_varint(wrong_wire, 7);
    check(!pancollapse::input::peek_multipath_record(wrong_wire, peek),
          "name under a varint wire type must decline");
    check(!pancollapse::input::peek_multipath_record(std::string("\x80", 1), peek),
          "unterminated tag must decline");
}

void test_randomized_against_decoder() {
    std::mt19937_64 rng(20260923);
    for (int trial = 0; trial < 2000; ++trial) {
        vg::MultipathAlignment record = dense_record(
            "r" + std::to_string(rng() % 1000), static_cast<int>(rng() % 12));
        if (rng() % 3 == 0) record.clear_name();
        if (rng() % 4 == 0) record.set_paired_read_name("mate");
        if (rng() % 5 == 0) record.set_read_group("rg");
        std::string bytes = record.SerializeAsString();
        if (rng() % 3 == 0) bytes += dense_record("tail", static_cast<int>(rng() % 3))
                                        .SerializeAsString();
        pancollapse::input::MultipathRecordPeek peek;
        check(pancollapse::input::peek_multipath_record(bytes, peek),
              "a valid serialization must be peekable (trial " + std::to_string(trial) + ")");
        expect_agrees(bytes, "randomized trial " + std::to_string(trial));
    }
}

void test_raw_framing_matches_serial_reader() {
    std::vector<vg::MultipathAlignment> records;
    for (int i = 0; i < 40; ++i) records.push_back(dense_record("read" + std::to_string(i / 3), i % 5));
    for (bool compressed : {false, true}) {
        std::ostringstream out(std::ios::binary);
        std::vector<vg::Alignment> wrong_type(1);
        wrong_type.front().set_name("not-a-gamp");
        check(vg::io::write<vg::Alignment>(out, wrong_type.size(),
              [&wrong_type](size_t i) { return wrong_type.at(i); }, compressed),
              "wrong-type write failed");
        check(vg::io::write<vg::MultipathAlignment>(out, records.size(),
              [&records](size_t i) { return records.at(i); }, compressed), "GAMP write failed");
        vg::io::finish(out, compressed);
        const std::string stream = out.str();

        std::istringstream input(stream, std::ios::binary);
        std::vector<std::string> names;
        std::vector<bool> has_subpath;
        const auto stats = pancollapse::input::raw_gamp_for_each(
            input, [&](std::unique_ptr<std::string> data) {
                pancollapse::input::MultipathRecordPeek peek;
                check(pancollapse::input::peek_multipath_record(*data, peek), "peek failed");
                names.emplace_back(peek.name);
                has_subpath.push_back(peek.has_subpath);
            }, 2);
        check(stats.records == records.size(), "raw framing record count");
        check(names.size() == records.size(), "raw framing callback count");
        for (size_t i = 0; i < records.size(); ++i) {
            check(names[i] == records[i].name(), "raw framing name order");
            check(has_subpath[i] == (records[i].subpath_size() > 0), "raw framing subpath flag");
        }
    }
}

}  // namespace

int main() {
    try {
        test_constructed_cases();
        test_randomized_against_decoder();
        test_raw_framing_matches_serial_reader();
    } catch (const std::exception& error) {
        std::cerr << "read_granular_input_test: " << error.what() << '\n';
        return 1;
    }
    std::cout << "read_granular_input_test: ok\n";
    return 0;
}
