#include "read_granular_input.hpp"

#include <vg/io/message_emitter.hpp>

#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
void check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}
struct Temporary {
    std::filesystem::path path;
    Temporary() {
        auto pattern = (std::filesystem::temp_directory_path() / "pancollapse-gamp-windows-XXXXXX").string();
        check(::mkdtemp(pattern.data()) != nullptr, "temporary directory");
        path = pattern;
    }
    ~Temporary() { std::error_code ignored; std::filesystem::remove_all(path, ignored); }
};
int run(const std::vector<std::string>& arguments) {
    std::vector<char*> argv;
    for (const auto& value : arguments) argv.push_back(const_cast<char*>(value.c_str()));
    argv.push_back(nullptr);
    const auto pid = ::fork();
    check(pid >= 0, "fork");
    if (pid == 0) { ::execv(argv.front(), argv.data()); ::_exit(127); }
    int status = 0;
    while (::waitpid(pid, &status, 0) < 0) check(errno == EINTR, "waitpid");
    return WIFEXITED(status) ? WEXITSTATUS(status) : 128;
}
std::vector<std::string> payloads(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    check(bool(input), "fixture input");
    std::vector<std::string> result;
    vg::io::MessageIterator iterator(input, false, 2);
    while (iterator.has_current()) {
        auto& message = *iterator;
        if (vg::io::Registry::check_protobuf_tag<vg::MultipathAlignment>(message.first) && message.second)
            result.push_back(*message.second);
        iterator.advance();
    }
    return result;
}
}

int main(int argc, char** argv) try {
    check(argc == 2, "expected fixture utility path");
    Temporary temporary;
    const auto input = temporary.path / "input.gamp";
    std::vector<std::vector<std::string>> groups(7);
    {
        std::ofstream stream(input, std::ios::binary);
        vg::io::MessageEmitter emitter(stream, false, 2);
        vg::Alignment other;
        other.set_name("wrong-type");
        emitter.write_copy(vg::io::Registry::get_protobuf_tag<vg::Alignment>(), other.SerializeAsString());
        for (std::size_t group = 0; group < groups.size(); ++group) {
            for (std::size_t member = 0; member < 1 + group % 3; ++member) {
                vg::MultipathAlignment record;
                record.set_name("read" + std::to_string(group));
                record.set_sequence("ACGT" + std::to_string(member));
                auto bytes = record.SerializeAsString();
                bytes.append("\xa0\x06\x2a", 3); // Preserve an unknown protobuf field as well.
                groups[group].push_back(bytes);
                emitter.write_copy(vg::io::Registry::get_protobuf_tag<vg::MultipathAlignment>(), bytes);
            }
        }
    }
    const auto expected = [&](std::size_t start, std::size_t end) {
        std::vector<std::string> result;
        for (auto i = start; i < end; ++i)
            result.insert(result.end(), groups[i].begin(), groups[i].end());
        return result;
    };
    const auto prefix = temporary.path / "prefix.gamp";
    check(run({argv[1], input.string(), prefix.string(), "3"}) == 0, "legacy prefix failed");
    check(payloads(prefix) == expected(0, 3), "legacy prefix changed payloads or groups");
    const auto middle = temporary.path / "middle.gamp", end = temporary.path / "end.gamp";
    check(run({argv[1], input.string(), middle.string(), "1", "2", end.string(), "5", "2"}) == 0,
          "batch windows failed");
    check(payloads(middle) == expected(1, 3) && payloads(end) == expected(5, 7),
          "windows changed payloads, split groups or lost EOF");
    const auto adjacent = temporary.path / "adjacent.gamp", last = temporary.path / "last.gamp";
    check(run({argv[1], input.string(), adjacent.string(), "0", "1", last.string(), "1", "6"}) == 0,
          "adjacent windows failed");
    check(payloads(adjacent) == expected(0, 1) && payloads(last) == expected(1, 7),
          "adjacent window boundary changed");
    unsigned serial = 0;
    const auto reject = [&](std::vector<std::string> suffix) {
        const auto output = temporary.path / ("invalid-" + std::to_string(serial++) + ".gamp");
        std::vector<std::string> args{argv[1], input.string(), output.string()};
        args.insert(args.end(), suffix.begin(), suffix.end());
        check(run(args) != 0, "invalid window accepted");
        check(!std::filesystem::exists(output), "incomplete or invalid output published");
    };
    for (const auto& count : {"0", "-1", "+1", "1x", "18446744073709551616"}) reject({count});
    reject({"18446744073709551615", "1"});
    reject({"6", "2"}); // Starts in range, ends beyond EOF.
    reject({"7", "1"});
    reject({"1", "2", (temporary.path / "overlap.gamp").string(), "2", "2"});
    reject({"1", "2", prefix.string(), "4", "1"});
    check(run({argv[1], input.string(), prefix.string(), "2"}) != 0, "existing fixture overwritten");
    check(payloads(prefix) == expected(0, 3), "existing fixture changed");
    const auto partial = temporary.path / "has-partial.gamp";
    std::ofstream(partial.string() + ".partial").put('x');
    check(run({argv[1], input.string(), partial.string(), "2"}) != 0 &&
              !std::filesystem::exists(partial), "existing partial overwritten");
    std::cout << "gamp window payload and boundary tests passed\n";
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
