#include "read_granular_input.hpp"

#include <vg/io/message_emitter.hpp>

#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::uint64_t number(std::string_view text) {
    std::uint64_t value = 0;
    const auto result = std::from_chars(text.data(), text.data() + text.size(), value);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size())
        throw std::runtime_error("group offsets and counts must be nonnegative integers");
    return value;
}
struct Window {
    std::filesystem::path output, staged;
    std::uint64_t start = 0, count = 0, groups = 0, records = 0;
};
}

// Diagnostic fixture utility: preserve protobuf payload bytes and whole QNAME
// groups, without decoding/re-encoding alignments or visiting a reference.
int main(int argc, char** argv) {
    try {
        const bool prefix = argc == 4;
        if (!prefix && (argc < 5 || (argc - 2) % 3 != 0)) {
            throw std::runtime_error("usage: pancollapse_gamp_prefix INPUT OUTPUT GROUPS\n"
                "   or: pancollapse_gamp_prefix INPUT OUTPUT START_GROUP GROUPS [OUTPUT START_GROUP GROUPS...]");
        }
        std::vector<Window> windows;
        for (int i = 2; i < argc; i += prefix ? 2 : 3) {
            Window window;
            window.output = std::filesystem::absolute(argv[i]).lexically_normal();
            window.staged = window.output.string() + ".partial";
            window.start = prefix ? 0 : number(argv[i + 1]);
            window.count = number(argv[i + (prefix ? 1 : 2)]);
            if (window.count == 0 || window.start > std::numeric_limits<std::uint64_t>::max() - window.count)
                throw std::runtime_error("GROUPS must be positive and window end must not overflow");
            if (!windows.empty() && window.start < windows.back().start + windows.back().count)
                throw std::runtime_error("windows must be ordered and nonoverlapping");
            for (const auto& previous : windows)
                if (window.output == previous.output || window.output == previous.staged ||
                    window.staged == previous.output)
                    throw std::runtime_error("duplicate fixture output");
            if (std::filesystem::exists(window.output) || std::filesystem::exists(window.staged))
                throw std::runtime_error("refusing to overwrite fixture output");
            windows.push_back(std::move(window));
        }
        std::ifstream input(argv[1], std::ios::binary);
        if (!input) throw std::runtime_error("cannot open GAMP input");
        std::ofstream stream;
        std::unique_ptr<vg::io::MessageEmitter> emitter;
        std::size_t selected = 0;
        std::uint64_t groups = 0;
        std::string previous;
        const auto finish = [&] {
            auto& window = windows[selected];
            emitter.reset();
            stream.close();
            if (!stream || window.groups != window.count)
                throw std::runtime_error("incomplete fixture window");
            std::filesystem::rename(window.staged, window.output);
            std::cout << "{\"schema\":\"pancollapse-gamp-prefix-v1\",\"start_group\":" << window.start
                      << ",\"groups\":" << window.groups << ",\"records\":" << window.records
                      << ",\"size_bytes\":" << std::filesystem::file_size(window.output)
                      << ",\"protobuf_payloads\":\"unchanged\"}" << std::endl;
            ++selected;
        };
        {
            vg::io::MessageIterator iterator(input, false, 8);
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
                        if (emitter && groups == windows[selected].start + windows[selected].count) finish();
                        if (selected == windows.size()) break;
                        previous = peek.name;
                        if (groups == windows[selected].start) {
                            stream.open(windows[selected].staged, std::ios::binary);
                            if (!stream) throw std::runtime_error("cannot open staged fixture output");
                            emitter = std::make_unique<vg::io::MessageEmitter>(stream, true);
                        }
                        if (emitter) ++windows[selected].groups;
                        ++groups;
                    }
                    if (emitter) {
                        emitter->write(tag, std::move(*message.second));
                        ++windows[selected].records;
                    }
                }
                iterator.advance();
            }
        }
        if (emitter) finish();
        if (selected != windows.size()) throw std::runtime_error("input ended before all fixture windows");
    } catch (const std::exception& error) {
        std::cerr << "pancollapse_gamp_prefix: " << error.what() << '\n';
        return 1;
    }
}
