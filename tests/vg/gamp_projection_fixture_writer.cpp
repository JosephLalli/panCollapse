#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void write_text_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("cannot write " + path.string());
    }
    out << contents;
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: gamp_projection_fixture_writer WORK_DIR\n";
        return 2;
    }

    try {
        const std::filesystem::path work_dir(argv[1]);
        std::filesystem::create_directories(work_dir);

        write_text_file(work_dir / "graph.gfa",
                        "H\tVN:Z:1.0\n"
                        "S\t1\tACGTACGTAC\n"
                        "P\tchrFixture\t1+\t10M\n");

        write_text_file(work_dir / "annotation.gtf",
                        "chrFixture\tpanCollapse\texon\t3\t8\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n");

        write_text_file(work_dir / "read.gamp.json",
                        "{\"start\":[0],\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":2,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":4,\"to_length\":4}],\"rank\":1}]},\"score\":42}]}\n");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
