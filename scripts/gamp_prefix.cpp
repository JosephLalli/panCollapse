#include "read_granular_input.hpp"

#include <vg/io/message_emitter.hpp>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

// Diagnostic fixture utility: preserve protobuf payload bytes and whole QNAME
// groups, without decoding/re-encoding alignments or visiting a reference.
int main(int argc, char** argv) {
    try {
        if (argc != 4) throw std::runtime_error("usage: pancollapse_gamp_prefix INPUT OUTPUT GROUPS");
        std::size_t parsed = 0;
        const auto limit = std::stoull(argv[3], &parsed);
        if (parsed != std::string(argv[3]).size() || limit == 0 || argv[3][0] == '-') {
            throw std::runtime_error("GROUPS must be a positive integer");
        }
        const std::filesystem::path output(argv[2]);
        const std::filesystem::path staged(output.string() + ".partial");
        if (std::filesystem::exists(output) || std::filesystem::exists(staged)) {
            throw std::runtime_error("refusing to overwrite fixture output");
        }
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) throw std::runtime_error("cannot open GAMP input");
        std::ofstream stream(staged, std::ios::binary);
        if (!stream) throw std::runtime_error("cannot open staged fixture output");
        std::uint64_t groups = 0, records = 0;
        std::string previous;
        {
            vg::io::MessageIterator iterator(input, false, 2);
            vg::io::MessageEmitter emitter(stream, true);
            const auto tag = vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>();
            while (iterator.has_current()) {
                auto& message = *iterator;
                if (message.first == tag && message.second) {
                    pancollapse::input::MultipathRecordPeek peek;
                    std::string fallback;
                    if (!pancollapse::input::peek_multipath_record(*message.second, peek)) {
                        vg::MultipathAlignment decoded;
                        if (!decoded.ParseFromString(*message.second)) {
                            throw std::runtime_error("invalid GAMP protobuf");
                        }
                        fallback = decoded.name();
                        peek.name = fallback;
                    }
                    if (groups == 0 || peek.name != previous) {
                        if (groups == limit) break;
                        previous = peek.name;
                        ++groups;
                    }
                    emitter.write(tag, std::move(*message.second));
                    ++records;
                }
                iterator.advance();
            }
        }
        stream.close();
        if (!stream || groups != limit) throw std::runtime_error("incomplete prefix fixture");
        std::filesystem::rename(staged, output);
        std::cout << "{\"schema\":\"pancollapse-gamp-prefix-v1\",\"groups\":" << groups
                  << ",\"records\":" << records << ",\"size_bytes\":"
                  << std::filesystem::file_size(output)
                  << ",\"protobuf_payloads\":\"unchanged\"}\n";
    } catch (const std::exception& error) {
        std::cerr << "pancollapse_gamp_prefix: " << error.what() << '\n';
        return 1;
    }
}
