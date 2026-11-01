#include "native_count_index.hpp"

#include <chrono>
#include <array>
#include <atomic>
#include <barrier>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <utility>
#include <vector>

namespace {

using pancollapse::count_reference::GeneRecord;
using pancollapse::count_reference::PolicyRecord;
using pancollapse::count_reference::missing;
using pancollapse::native_count::Data;
using pancollapse::native_count::PairRecord;
using pancollapse::native_count::PathRecord;

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<class Function> void rejects(Function&& function, const char* message) {
    try {
        function();
    } catch (const std::exception&) {
        return;
    }
    throw std::runtime_error(message);
}

std::vector<char> read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

void write_bytes(const std::filesystem::path& path, const std::vector<char>& bytes) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    require(static_cast<bool>(output), "cannot write corrupt fixture");
}

template<class Function> void concurrent_checks(Function&& function) {
    std::barrier start(8);
    std::atomic<unsigned> failed{0};
    std::vector<std::thread> workers;
    for (unsigned i = 0; i < 8; ++i) workers.emplace_back([&] {
        start.arrive_and_wait();
        try { for (unsigned repeat = 0; repeat < 32; ++repeat) function(); }
        catch (...) { failed.fetch_add(1); }
    });
    for (auto& worker : workers) worker.join();
    require(failed.load() == 0, "concurrent validation result changed");
}

// PCANBND1 has a 16-byte magic and ten (offset,size) pairs. Mutate only
// payloads, retaining valid framing so access-time checks must reject them.
std::size_t section_offset(const std::vector<char>& bytes, unsigned section) {
    std::uint64_t offset = 0;
    require(section < 10 && bytes.size() >= 176, "invalid test section header");
    std::memcpy(&offset, bytes.data() + 16 + 16 * section, sizeof(offset));
    require(offset <= bytes.size(), "invalid test section offset");
    return static_cast<std::size_t>(offset);
}

template<class Value> void patch(std::vector<char>& bytes, unsigned section,
                                std::size_t offset, Value value) {
    const auto at = section_offset(bytes, section) + offset;
    require(at <= bytes.size() && sizeof(value) <= bytes.size() - at, "invalid test patch");
    std::memcpy(bytes.data() + at, &value, sizeof(value));
}

Data fixture() {
    Data data;
    data.content_id = "native-codec";
    data.gbwt_size_bytes = 123456789;
    data.strings = {"GENE", "protein_coding", "TX_SHARED", "TX_BODY", "raw-category"};
    // Set 1 is a string dictionary set. Sets 2/3 intentionally hold numeric
    // independent-sample IDs outside strings(), proving context-sensitive checks.
    data.sets = {{}, {1, 4}, {101}, {202}};
    data.feature_catalog = {"GENE"};
    GeneRecord gene;
    gene.id_string = 0;
    gene.type_set = 1;
    gene.equivalence[0] = 0;
    gene.equivalence[1] = 0;
    data.genes = {gene};
    data.policies = {
        PolicyRecord{0, 1, missing, 0},
        PolicyRecord{0, 1, missing, 1},
    };

    // Paths 0 and 2 deliberately have the same exon metadata. They remain
    // distinct raw Parent rows by owning separate PairRecord IDs and bodies.
    data.paths = {
        PathRecord{0, 2, 0, 0, pancollapse::native_count::exon_path, 0, 1},
        PathRecord{1, 3, 0, 2, pancollapse::native_count::body_path, 0, 1},
        PathRecord{2, 2, 1, 0, pancollapse::native_count::exon_path, 0, 1},
        PathRecord{3, 3, 1, 3, pancollapse::native_count::body_path, 0, 1},
    };
    data.pairs = {PairRecord{0, 1}, PairRecord{2, 3}};
    data.model_refs = {0, 0, 1, 1};
    // GBWT path ID 0 is unannotated; raw annotated rows retain their ordinal,
    // not their background GBWT IDs.
    data.path_lookup = {missing, 2, 0, 3, 1};
    return data;
}

void validation_memoization(const std::filesystem::path& original,
                            const std::filesystem::path& directory) {
    auto valid = pancollapse::native_count::Index::load(original);
    concurrent_checks([&] {
        require(valid.path_row(0) == missing, "missing lookup changed");
        constexpr std::array<unsigned, 4> lookups{2, 4, 1, 3};
        for (unsigned row = 0; row < lookups.size(); ++row) {
            require(valid.path_row(lookups[row]) == row, "concurrent path lookup changed");
            const auto refs = valid.model_ids(valid.paths()[row]);
            require(refs.size() == 1 && refs.front() == row / 2, "concurrent model refs changed");
        }
    });
    const auto foreign = valid.paths().front();
    rejects([&] { static_cast<void>(valid.model_ids(foreign)); }, "foreign cached row accepted");
    rejects([&] { static_cast<void>(valid.path_row(valid.path_count())); }, "lookup bounds skipped");
    // Moving transfers certificates with their owning mapping, not row copies.
    auto moved = std::move(valid);
    require(moved.model_ids(moved.paths()[0]).front() == 0, "memoized index move failed");

    unsigned serial = 0;
    const auto corrupted = [&](const std::vector<char>& bytes) {
        const auto path = directory / ("row-corrupt-" + std::to_string(serial++) + ".pcn");
        write_bytes(path, bytes);
        return pancollapse::native_count::Index::load(path);
    };
    const auto rejects_refs = [&](const std::vector<char>& bytes) {
        auto invalid = corrupted(bytes);
        concurrent_checks([&] {
            rejects([&] { static_cast<void>(invalid.model_ids(invalid.paths()[0])); },
                    "failed model validation was cached as successful");
        });
    };

    auto bytes = read_bytes(original);
    patch(bytes, 7, offsetof(PathRecord, strand), std::uint32_t{2});
    auto invalid_path = corrupted(bytes);
    concurrent_checks([&] {
        rejects([&] { static_cast<void>(invalid_path.path_row(2)); }, "invalid path was memoized");
        rejects([&] { static_cast<void>(invalid_path.model_ids(invalid_path.paths()[0])); },
                "invalid path refs were memoized");
    });

    bytes = read_bytes(original);
    patch(bytes, 7, offsetof(PathRecord, policy), missing);
    rejects_refs(bytes);
    bytes = read_bytes(original);
    patch(bytes, 7, offsetof(PathRecord, model_first), std::uint64_t{UINT64_MAX});
    rejects_refs(bytes);
    bytes = read_bytes(original);
    patch(bytes, 6, 2 * sizeof(std::uint32_t), std::uint32_t{999});
    auto invalid_lookup = corrupted(bytes);
    concurrent_checks([&] {
        rejects([&] { static_cast<void>(invalid_lookup.path_row(2)); }, "invalid lookup accepted");
    });

    // The first inverse reference is valid, but the last is not. A partially
    // completed loop must never publish the model-refs-valid bit.
    bytes = read_bytes(original);
    patch(bytes, 7, offsetof(PathRecord, model_count), std::uint32_t{2});
    patch(bytes, 9, sizeof(std::uint32_t), missing);
    rejects_refs(bytes);
    bytes = read_bytes(original);
    patch(bytes, 8, offsetof(PairRecord, body), std::uint32_t{0});
    rejects_refs(bytes);
    bytes = read_bytes(original);
    patch(bytes, 8, offsetof(PairRecord, body), std::uint32_t{999});
    rejects_refs(bytes);

    auto other_gene = fixture();
    other_gene.strings.push_back("OTHER");
    other_gene.feature_catalog.push_back("OTHER");
    auto gene = other_gene.genes.front();
    gene.id_string = 5;
    gene.equivalence[0] = gene.equivalence[1] = 1;
    other_gene.genes.push_back(gene);
    const auto other = directory / "other-gene.pcn";
    pancollapse::native_count::write(other, other_gene);
    bytes = read_bytes(other);
    patch(bytes, 7, sizeof(PathRecord) + offsetof(PathRecord, policy), std::uint32_t{1});
    patch(bytes, 5, sizeof(PolicyRecord) + offsetof(PolicyRecord, gene), std::uint32_t{1});
    rejects_refs(bytes);

    auto zero = fixture();
    zero.paths.push_back(PathRecord{4, 2, 0, 0, pancollapse::native_count::exon_path, 0, 0});
    zero.path_lookup.push_back(4);
    const auto zero_path = directory / "zero-model-row.pcn";
    pancollapse::native_count::write(zero_path, zero);
    auto zero_index = pancollapse::native_count::Index::load(zero_path);
    concurrent_checks([&] {
        require(zero_index.path_row(5) == 4 && zero_index.model_ids(zero_index.paths()[4]).empty(),
                "zero-model row changed after memoization");
    });
}

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        path_ = std::filesystem::temp_directory_path() /
            ("pancollapse-native-index-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        std::filesystem::create_directories(path_);
    }
    ~TemporaryDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    const std::filesystem::path& path() const { return path_; }
  private:
    std::filesystem::path path_;
};

}  // namespace

int main() try {
    TemporaryDirectory temporary;
    const auto empty_path = temporary.path() / "empty.pcn";
    const auto first = temporary.path() / "first.pcn";
    const auto second = temporary.path() / "second.pcn";
    const auto truncated = temporary.path() / "truncated.pcn";
    const auto corrupt = temporary.path() / "corrupt.pcn";

    Data empty;
    empty.content_id = "empty";
    pancollapse::native_count::write(empty_path, empty);
    auto empty_index = pancollapse::native_count::Index::load(empty_path);
    require(empty_index.content_id() == "empty" && empty_index.path_count() == 0 &&
                empty_index.paths().empty() && empty_index.pairs().empty(),
            "empty codec roundtrip failed");

    const Data data = fixture();
    pancollapse::native_count::write(first, data);
    pancollapse::native_count::write(second, data);
    require(read_bytes(first) == read_bytes(second), "codec bytes are not deterministic");
    require(pancollapse::native_count::is_index(first), "native magic probe failed");

    auto index = pancollapse::native_count::Index::load(first);
    require(index.content_id() == data.content_id && index.gbwt_size_bytes() == data.gbwt_size_bytes &&
                index.strings() == data.strings && index.feature_catalog() == data.feature_catalog,
            "native metadata roundtrip failed");
    require(index.sets().size() == 4 && index.sets()[2] == std::vector<std::uint32_t>{101} &&
                index.sets()[3] == std::vector<std::uint32_t>{202},
            "numeric support IDs were treated as string IDs");
    require(index.path_count() == 5 && index.path_row(0) == missing && index.path_row(1) == 2 &&
                index.path_row(2) == 0 && index.path_row(3) == 3 && index.path_row(4) == 1,
            "GBWT path lookup did not preserve raw IDs");
    require(index.pairs().size() == 2 && index.pairs()[0].exon == 0 && index.pairs()[0].body == 1 &&
                index.pairs()[1].exon == 2 && index.pairs()[1].body == 3,
            "identical exon rows were incorrectly merged across Parents");
    const auto paths = index.paths();
    const auto ids0 = index.model_ids(paths[0]);
    const auto ids1 = index.model_ids(paths[1]);
    const auto ids2 = index.model_ids(paths[2]);
    const auto ids3 = index.model_ids(paths[3]);
    require(ids0.size() == 1 && ids0[0] == 0 && ids1.size() == 1 && ids1[0] == 0 &&
                ids2.size() == 1 && ids2[0] == 1 && ids3.size() == 1 && ids3[0] == 1,
            "pair CSR inverse references failed");
    require(index.take_feature_catalog() == std::vector<std::string>{"GENE"},
            "feature catalog transfer failed");
    validation_memoization(first, temporary.path());

    auto bytes = read_bytes(first);
    require(bytes.size() > 16, "native fixture unexpectedly short");
    bytes.resize(bytes.size() - 1);
    write_bytes(truncated, bytes);
    rejects([&] { static_cast<void>(pancollapse::native_count::Index::load(truncated)); },
            "truncated native index was accepted");
    bytes = read_bytes(first);
    bytes.front() ^= 1;
    write_bytes(corrupt, bytes);
    rejects([&] { static_cast<void>(pancollapse::native_count::Index::load(corrupt)); },
            "corrupt native header was accepted");

    std::cout << "native count index codec passed\n";
    return 0;
} catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
}
