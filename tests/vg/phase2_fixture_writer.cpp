#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
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
        std::cerr << "usage: phase2_fixture_writer WORK_DIR\n";
        return 2;
    }

    try {
        const std::filesystem::path work_dir(argv[1]);
        std::filesystem::remove_all(work_dir);
        std::filesystem::create_directories(work_dir);

        const std::string sequence(400, 'A');
        write_text_file(work_dir / "graph.gfa",
                        "H\tVN:Z:1.0\n"
                        "S\t1\t" +
                            sequence +
                            "\n"
                            "P\tchrFixture\t1+\t400M\n");

        write_text_file(work_dir / "annotation.gtf",
                        "chrFixture\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n");

        write_text_file(work_dir / "collapse.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_A\tTX_A\tGENE_A\n");

        write_text_file(work_dir / "read.gamp.json",
                        "{\"name\":\"read000_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
