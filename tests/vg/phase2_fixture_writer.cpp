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
                        "S\t2\t" +
                            sequence +
                            "\n"
                            "S\t3\t" +
                            sequence +
                            "\n"
                            "P\tchrFixture\t1+\t400M\n"
                            "P\tchrFixtureB\t2+\t400M\n"
                            "P\tchrFixtureRev\t3-\t400M\n");

        write_text_file(work_dir / "annotation.gtf",
                        "chrFixture\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n");

        write_text_file(work_dir / "collapse.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_A\tTX_A\tGENE_A\n");

        write_text_file(work_dir / "annotation_multi.gtf",
                        "chrFixture\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n"
                        "chrFixtureB\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_B\"; transcript_id \"SRC_B\";\n");

        write_text_file(work_dir / "collapse_multi.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_A\tTX_A\tGENE_A\n"
                        "chrFixtureB\tSRC_B\tTX_B\tGENE_B\n");

        write_text_file(work_dir / "annotation_revpath.gtf",
                        "chrFixtureRev\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_REV\"; transcript_id \"SRC_REV\";\n");

        write_text_file(work_dir / "collapse_revpath.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixtureRev\tSRC_REV\tTX_REV\tGENE_REV\n");

        write_text_file(work_dir / "read.gamp.json",
                        "{\"name\":\"read000_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_reverse.gamp.json",
                        "{\"name\":\"read001_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":70,\"is_reverse\":true},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_mixed.gamp.json",
                        "{\"name\":\"read002_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":["
                        "{\"position\":{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read002_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":["
                        "{\"position\":{\"node_id\":1,\"offset\":70,\"is_reverse\":true},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_recurrent.gamp.json",
                        "{\"name\":\"read003_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read004_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read003_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_two_groups.gamp.json",
                        "{\"name\":\"read_with_under_score005_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read006_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_multi_target.gamp.json",
                        "{\"name\":\"read007_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read007_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":2,\"offset\":70,\"is_reverse\":true},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_no_compatible.gamp.json",
                        "{\"name\":\"read008_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":10,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_revpath.gamp.json",
                        "{\"name\":\"read012_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":3,\"offset\":310,\"is_reverse\":true},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read013_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":3,\"offset\":70,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_n_base.gamp.json",
                        "{\"name\":\"read014_NAACCCAAGTTTGGGA_AAAAAAAAAAAN\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_bad_molecule_malformed.gamp.json",
                        "{\"name\":\"read009_AAACCCAAGTTTGGG_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_bad_molecule_unsupported.gamp.json",
                        "{\"name\":\"read010_AAACCCAAGTTTGGGX_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_bad_molecule_missing.gamp.json",
                        "{\"name\":\"read011_AAACCCAAGTTTGGGA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_bad_molecule.gamp.json",
                        "{\"name\":\"read009_AAACCCAAGTTTGGG_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read010_AAACCCAAGTTTGGGX_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read011_AAACCCAAGTTTGGGA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
