#include "read_granular_input.hpp"

#include <vg/io/message_emitter.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <vector>

// Local transport adapter, not a biological GAM conversion. vg gamsort sorts
// one envelope per read. Its bytes-valued quality field carries the original
// GAMP payloads; no route, edit, annotation, or alternate record is rewritten.
namespace {
constexpr std::string_view marker = "pancollapse-gamp-sort-envelope-v1";
// Stay below MessageEmitter's 1-GB message ceiling. Keep total serialized
// envelopes below 512 GiB so vg's 512-MiB runs fit its single-pass 2048-way
// merge (the launcher must provide RLIMIT_NOFILE >= 2058).
constexpr std::uint64_t max_envelope_bytes = 900000000;
constexpr std::uint64_t max_sort_bytes = std::uint64_t{512} << 30;
using Anchor = std::tuple<std::int64_t, bool, std::int64_t>;
struct Stats { std::uint64_t groups = 0, records = 0, payload_bytes = 0; };

void append_length(std::string& output, std::uint64_t length) {
    for (unsigned shift = 0; shift < 64; shift += 8)
        output.push_back(static_cast<char>((length >> shift) & 255U));
}

std::uint64_t take_integer(std::string_view& input) {
    if (input.size() < 8) throw std::runtime_error("truncated envelope length");
    std::uint64_t length = 0;
    for (unsigned i = 0; i < 8; ++i)
        length |= static_cast<std::uint64_t>(static_cast<unsigned char>(input[i])) << (8U * i);
    input.remove_prefix(8);
    return length;
}

Anchor anchor(const vg::Position& position) {
    return {position.node_id(), position.is_reverse(), position.offset()};
}

void include_positions(const vg::MultipathAlignment& record, vg::Alignment& envelope) {
    for (const auto& subpath : record.subpath()) {
        for (const auto& mapping : subpath.path().mapping()) {
            if (!mapping.has_position() || mapping.position().node_id() <= 0) continue;
            if (envelope.path().mapping_size() == 0) {
                *envelope.mutable_path()->add_mapping()->mutable_position() = mapping.position();
            } else if (anchor(mapping.position()) < anchor(envelope.path().mapping(0).position())) {
                *envelope.mutable_path()->mutable_mapping(0)->mutable_position() = mapping.position();
            }
        }
    }
}

Anchor envelope_anchor(const vg::Alignment& envelope) {
    return envelope.path().mapping_size() == 0 ? Anchor{0, false, 0}
                                              : anchor(envelope.path().mapping(0).position());
}

void report(const char* operation, const Stats& stats) {
    std::cerr << "{\"schema\":\"pancollapse-gamp-sort-envelope-v1\",\"operation\":\""
              << operation << "\",\"groups\":" << stats.groups
              << ",\"records\":" << stats.records << ",\"payload_bytes\":"
              << stats.payload_bytes << "}\n";
}

void pack(std::istream& input, std::ostream& output) {
    vg::io::MessageIterator iterator(input, false, 2);
    const auto tag = vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>();
    const auto gam_tag = vg::io::Registry::get_protobuf_tag<vg::Alignment>();
    Stats stats;
    std::uint64_t serialized_bytes = 0, envelope_records = 0;
    {
        vg::io::MessageEmitter emitter(output, true);
        vg::Alignment envelope;
        const auto flush = [&] {
            if (envelope.name().empty()) return;
            envelope.set_sample_name(std::string(marker));
            std::string count;
            append_length(count, envelope_records);
            envelope.mutable_quality()->insert(0, count);
            const auto size = envelope.ByteSizeLong();
            if (size > max_envelope_bytes || size > max_sort_bytes - serialized_bytes)
                throw std::runtime_error("sort envelope/run fan-in safety bound exceeded");
            serialized_bytes += size;
            emitter.write(gam_tag, envelope.SerializeAsString());
            ++stats.groups;
            if (stats.groups % 1000000 == 0) report("pack-progress", stats);
            envelope.Clear();
            envelope_records = 0;
        };
        while (iterator.has_current()) {
            const auto& message = *iterator;
            if (message.first != tag) throw std::runtime_error("expected GAMP input tag");
            if (message.second) {
                vg::MultipathAlignment record;
                pancollapse::input::decode_multipath_record(*message.second, record);
                if (record.name().empty()) throw std::runtime_error("empty GAMP read name");
                if (envelope.name() != record.name()) {
                    flush();
                    envelope.set_name(record.name());
                }
                include_positions(record, envelope);
                if (message.second->size() > max_envelope_bytes - 16 ||
                    envelope.quality().size() > max_envelope_bytes - 16 - message.second->size())
                    throw std::runtime_error("read group exceeds safe envelope size");
                append_length(*envelope.mutable_quality(), message.second->size());
                envelope.mutable_quality()->append(*message.second);
                ++envelope_records;
                ++stats.records;
                stats.payload_bytes += message.second->size();
            }
            iterator.advance();
        }
        flush();
    }
    if (!output) throw std::runtime_error("envelope output failed");
    report("pack", stats);
}

void unpack(std::istream& input, std::ostream& output) {
    vg::io::MessageIterator iterator(input, false, 2);
    const auto tag = vg::io::Registry::get_protobuf_tag<vg::Alignment>();
    const auto gamp_tag = vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>();
    Stats stats;
    Anchor previous{};
    {
        vg::io::MessageEmitter emitter(output, true);
        while (iterator.has_current()) {
            const auto& message = *iterator;
            if (message.first != tag) throw std::runtime_error("expected GAM envelope tag");
            if (message.second) {
                vg::Alignment envelope;
                if (!envelope.ParseFromString(*message.second) || envelope.sample_name() != marker)
                    throw std::runtime_error("invalid sort envelope marker");
                if (envelope.path().mapping_size() > 1 ||
                    (envelope.path().mapping_size() == 1 &&
                     (!envelope.path().mapping(0).has_position() ||
                      envelope.path().mapping(0).position().node_id() <= 0)))
                    throw std::runtime_error("invalid sort envelope anchor");
                const auto current = envelope_anchor(envelope);
                if (stats.groups != 0 && current < previous)
                    throw std::runtime_error("vg output is not graph-position sorted");
                previous = current;
                std::string_view payload = envelope.quality();
                const auto record_count = take_integer(payload);
                if (record_count == 0 || record_count > payload.size() / 8)
                    throw std::runtime_error("invalid envelope record count");
                for (std::uint64_t record = 0; record < record_count; ++record) {
                    const auto length = take_integer(payload);
                    if (length > payload.size()) throw std::runtime_error("truncated envelope record");
                    std::string bytes(payload.substr(0, static_cast<std::size_t>(length)));
                    payload.remove_prefix(static_cast<std::size_t>(length));
                    pancollapse::input::MultipathRecordPeek peek;
                    if (!pancollapse::input::peek_multipath_record(bytes, peek) || peek.name != envelope.name())
                        throw std::runtime_error("envelope read-name mismatch");
                    stats.payload_bytes += length;
                    ++stats.records;
                    emitter.write(gamp_tag, std::move(bytes));
                }
                if (!payload.empty()) throw std::runtime_error("trailing envelope bytes");
                ++stats.groups;
                if (stats.groups % 1000000 == 0) report("unpack-progress", stats);
            }
            iterator.advance();
        }
    }
    if (!output) throw std::runtime_error("GAMP output failed");
    report("unpack", stats);
}

// Small-fixture-only exact check: no hashes and no normalization of protobufs.
using Groups = std::map<std::string, std::vector<std::string>>;
Groups read_groups(const char* filename, bool check_sorted) {
    std::ifstream stream(filename, std::ios::binary);
    if (!stream) throw std::runtime_error("cannot open verification input");
    vg::io::MessageIterator iterator(stream, false, 2);
    const auto tag = vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>();
    Groups groups;
    std::string previous_name;
    vg::Alignment envelope;
    Anchor previous{};
    bool have_previous = false;
    const auto close_group = [&] {
        if (envelope.name().empty()) return;
        const auto current = envelope_anchor(envelope);
        if (check_sorted && have_previous && current < previous)
            throw std::runtime_error("GAMP anchor order mismatch");
        previous = current;
        have_previous = true;
        envelope.Clear();
    };
    while (iterator.has_current()) {
        const auto& message = *iterator;
        if (message.first != tag) throw std::runtime_error("unexpected verification tag");
        if (message.second) {
            vg::MultipathAlignment record;
            pancollapse::input::decode_multipath_record(*message.second, record);
            if (record.name() != previous_name) {
                close_group();
                if (groups.contains(record.name())) throw std::runtime_error("split read group");
                previous_name = record.name();
                envelope.set_name(previous_name);
            }
            include_positions(record, envelope);
            groups[record.name()].push_back(*message.second);
        }
        iterator.advance();
    }
    close_group();
    return groups;
}

void fixture(bool malformed) {
    vg::io::MessageEmitter emitter(std::cout, true);
    if (malformed) {
        vg::Alignment envelope;
        envelope.set_sample_name(std::string(marker));
        envelope.set_quality("truncated");
        emitter.write(vg::io::Registry::get_protobuf_tag<vg::Alignment>(), envelope.SerializeAsString());
        return;
    }
    const auto tag = vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>();
    for (const auto& [name, node, reverse, offset, copies] :
         std::vector<std::tuple<std::string, int, bool, int, int>>{
             {"high", 100, false, 0, 1}, {"reverse", 10, true, 2, 2},
             {"tie-a", 10, false, 3, 1}, {"unmapped", 0, false, 0, 1},
             {"tie-b", 10, false, 3, 1}, {"low", 10, false, 1, 1}}) {
        for (int copy = 0; copy < copies; ++copy) {
            vg::MultipathAlignment record;
            record.set_name(name);
            record.set_sequence("ACGT");
            record.set_quality(std::string("\0\xff\0\xff", 4));
            if (node != 0) {
                auto* position = record.add_subpath()->mutable_path()->add_mapping()->mutable_position();
                position->set_node_id(node);
                position->set_is_reverse(reverse);
                position->set_offset(offset);
            }
            auto bytes = record.SerializeAsString();
            // Unknown field 101 with binary payload must also survive unchanged.
            bytes.append("\xaa\x06\x03\0\xff\0", 6);
            emitter.write(tag, std::move(bytes));
        }
    }
}
}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc == 2 && (std::string_view(argv[1]) == "fixture" ||
                          std::string_view(argv[1]) == "malformed-fixture")) {
            fixture(std::string_view(argv[1]) == "malformed-fixture");
        } else if (argc == 4 && std::string_view(argv[1]) == "verify") {
            if (read_groups(argv[2], false) != read_groups(argv[3], true))
                throw std::runtime_error("GAMP payload/group parity failed");
            std::cout << "{\"payload_and_group_parity\":true,\"graph_position_sorted\":true}\n";
        } else if (argc == 2 && std::string_view(argv[1]) == "pack") {
            pack(std::cin, std::cout);
        } else if (argc == 2 && std::string_view(argv[1]) == "unpack") {
            unpack(std::cin, std::cout);
        } else {
            throw std::runtime_error("usage: gamp_sort_envelope pack|unpack OR verify INPUT SORTED");
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "gamp_sort_envelope: " << error.what() << '\n';
        return 1;
    }
}
