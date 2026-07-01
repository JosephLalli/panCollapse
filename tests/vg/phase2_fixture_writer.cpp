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

        write_text_file(work_dir / "collapse_multi_missing.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_A\tTX_A\tGENE_A\n");

        write_text_file(work_dir / "collapse_contradictory.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_A\tTX_A\tGENE_A\n"
                        "chrFixture\tSRC_A\tTX_CONFLICT\tGENE_A\n");

        write_text_file(work_dir / "collapse_gene_conflict.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_A\tTX_SHARED\tGENE_A\n"
                        "chrFixtureB\tSRC_B\tTX_SHARED\tGENE_B\n");

        write_text_file(work_dir / "annotation_collapse_shared.gtf",
                        "chrFixture\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_SHARED\"; transcript_id \"SRC_SHARED_A\";\n"
                        "chrFixtureB\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_SHARED\"; transcript_id \"SRC_SHARED_B\";\n"
                        "chrFixtureRev\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_OTHER\"; transcript_id \"SRC_OTHER\";\n");

        write_text_file(work_dir / "collapse_shared.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_SHARED_A\tTX_SHARED\tGENE_SHARED\n"
                        "chrFixtureB\tSRC_SHARED_B\tTX_SHARED\tGENE_SHARED\n"
                        "chrFixtureRev\tSRC_OTHER\tTX_OTHER\tGENE_OTHER\n");

        write_text_file(work_dir / "annotation_revpath.gtf",
                        "chrFixtureRev\tpanCollapse\texon\t301\t350\t.\t+\t.\t"
                        "gene_id \"GENE_REV\"; transcript_id \"SRC_REV\";\n");

        write_text_file(work_dir / "collapse_revpath.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixtureRev\tSRC_REV\tTX_REV\tGENE_REV\n");

        write_text_file(work_dir / "annotation_connection_score.gtf",
                        "chrFixture\tpanCollapse\texon\t311\t320\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n"
                        "chrFixture\tpanCollapse\texon\t331\t340\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n"
                        "chrFixtureB\tpanCollapse\texon\t311\t320\t.\t+\t.\t"
                        "gene_id \"GENE_B\"; transcript_id \"SRC_B\";\n"
                        "chrFixtureB\tpanCollapse\texon\t331\t340\t.\t+\t.\t"
                        "gene_id \"GENE_B\"; transcript_id \"SRC_B\";\n");

        write_text_file(work_dir / "annotation_gap.gtf",
                        "chrFixture\tpanCollapse\texon\t311\t320\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n"
                        "chrFixture\tpanCollapse\texon\t351\t360\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n"
                        "chrFixture\tpanCollapse\texon\t311\t320\t.\t+\t.\t"
                        "gene_id \"GENE_B\"; transcript_id \"SRC_B\";\n"
                        "chrFixture\tpanCollapse\texon\t371\t380\t.\t+\t.\t"
                        "gene_id \"GENE_B\"; transcript_id \"SRC_B\";\n");

        write_text_file(work_dir / "collapse_gap.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_A\tTX_A\tGENE_A\n"
                        "chrFixture\tSRC_B\tTX_B\tGENE_B\n");

        write_text_file(work_dir / "annotation_connection_insert.gtf",
                        "chrFixture\tpanCollapse\texon\t311\t320\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n"
                        "chrFixture\tpanCollapse\texon\t331\t340\t.\t+\t.\t"
                        "gene_id \"GENE_A\"; transcript_id \"SRC_A\";\n"
                        "chrFixture\tpanCollapse\texon\t311\t320\t.\t+\t.\t"
                        "gene_id \"GENE_B\"; transcript_id \"SRC_B\";\n"
                        "chrFixture\tpanCollapse\texon\t351\t360\t.\t+\t.\t"
                        "gene_id \"GENE_B\"; transcript_id \"SRC_B\";\n");

        write_text_file(work_dir / "annotation_edge_cases.gtf",
                        "chrFixture\tpanCollapse\tgene\t1\t400\t.\t+\t.\t"
                        "gene_id \"GENE_EDGE\";\n"
                        "chrFixture\tpanCollapse\texon\t101\t150\t.\t+\t.\t"
                        "gene_id \"GENE_EDGE\"; transcript_id \"SRC_EDGE\";\n");

        write_text_file(work_dir / "collapse_edge_cases.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "chrFixture\tSRC_EDGE\tTX_EDGE\tGENE_EDGE\n");

        write_text_file(work_dir / "rpvg_graph.json",
                        R"json({"node":[{"id":1,"sequence":"A"},{"id":2,"sequence":"A"},{"id":3,"sequence":"AAA"},{"id":4,"sequence":"AA"},{"id":5,"sequence":"AAA"},{"id":6,"sequence":"AAA"}],"edge":[{"from":1,"to":3},{"from":2,"to":3},{"from":3,"to":4},{"from":4,"to":5},{"from":4,"to":6}],"path":[{"name":"tx1","mapping":[{"position":{"node_id":1},"edit":[{"from_length":1,"to_length":1}]},{"position":{"node_id":3},"edit":[{"from_length":3,"to_length":3}]},{"position":{"node_id":4},"edit":[{"from_length":2,"to_length":2}]},{"position":{"node_id":5},"edit":[{"from_length":3,"to_length":3}]}]},{"name":"tx2","mapping":[{"position":{"node_id":6,"is_reverse":true},"edit":[{"from_length":3,"to_length":3}]},{"position":{"node_id":4,"is_reverse":true},"edit":[{"from_length":2,"to_length":2}]},{"position":{"node_id":3,"is_reverse":true},"edit":[{"from_length":3,"to_length":3}]},{"position":{"node_id":1,"is_reverse":true},"edit":[{"from_length":1,"to_length":1}]}]}]}
)json");

        write_text_file(work_dir / "rpvg_annotation.gtf",
                        "tx1\tpanCollapse\texon\t1\t9\t.\t+\t.\tgene_id \"gene1\"; transcript_id \"tx1\";\n"
                        "tx2\tpanCollapse\texon\t1\t9\t.\t-\t.\tgene_id \"gene2\"; transcript_id \"tx2\";\n");

        write_text_file(work_dir / "rpvg_collapse.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "tx1\ttx1\ttx1\tgene1\n"
                        "tx2\ttx2\ttx2\tgene2\n");

        write_text_file(work_dir / "tiny_reference.fa",
                        ">x\n"
                        "CAAATAAGGCTTGGAAATTTTCTGGAGTTCTATTATATTCCAACTCTCTG\n");

        write_text_file(work_dir / "tiny_annotation.gtf",
                        "##description: \n"
                        "##format: gtf\n"
                        "x\tSOURCE\tgene\t2\t40\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\";\n"
                        "x\tSOURCE\ttranscript\t2\t40\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\"; transcript_id \"transcript1\";\n"
                        "x\tSOURCE\texon\t2\t10\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\"; transcript_id \"transcript1\"; exon_number 1;\n"
                        "x\tSOURCE\texon\t20\t25\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\"; transcript_id \"transcript1\"; exon_number 2;\n"
                        "x\tSOURCE\texon\t29\t40\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\"; transcript_id \"transcript1\"; exon_number 3;\n"
                        "x\tSOURCE\ttranscript\t2\t40\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\"; transcript_id \"transcript2\";\n"
                        "x\tSOURCE\texon\t2\t10\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\"; transcript_id \"transcript2\"; exon_number 1;\n"
                        "x\tSOURCE\texon\t29\t40\t.\t+\t.\t"
                        "gene_id \"gene\"; gene_type \"protein_coding\"; gene_name \"GENE1\"; transcript_id \"transcript2\"; exon_number 2;\n");

        write_text_file(work_dir / "tiny_collapse.tsv",
                        "source_path_name\tsource_transcript_id\tcanonical_transcript_id\tcanonical_gene_id\n"
                        "x\ttranscript1\ttranscript1\tgene\n"
                        "x\ttranscript2\ttranscript2\tgene\n");

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

        write_text_file(work_dir / "read_collapse_shared_max.gamp.json",
                        "{\"name\":\"read021_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":30}]}\n"
                        "{\"name\":\"read021_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":2,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read021_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":3,\"offset\":310,\"is_reverse\":true},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":38}]}\n"
                        "{\"name\":\"read023_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n"
                        "{\"name\":\"read023_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":2,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":30}]}\n"
                        "{\"name\":\"read023_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":3,\"offset\":310,\"is_reverse\":true},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":38}]}\n");

        write_text_file(work_dir / "read_collapse_shared_no_sum.gamp.json",
                        "{\"name\":\"read022_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":20}]}\n"
                        "{\"name\":\"read022_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":2,\"offset\":310,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":20}]}\n"
                        "{\"name\":\"read022_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":3,\"offset\":310,\"is_reverse\":true},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":30}]}\n");

        write_text_file(work_dir / "read_no_compatible.gamp.json",
                        "{\"name\":\"read008_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":10,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":20,\"to_length\":20}],\"rank\":1}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_unaligned.gamp.json",
                        "{\"name\":\"read024_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\"}\n");

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

        write_text_file(work_dir / "read_connection_score.gamp.json",
                        "{\"name\":\"read015_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0,2],"
                        "\"subpath\":["
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":310,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1}]},\"connection\":[{\"next\":1,\"score\":5}],\"score\":20},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":330,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1}]},\"score\":20},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":2,\"offset\":310,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1}]},\"next\":[3],\"score\":20},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":2,\"offset\":330,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1}]},\"score\":20}]}\n");

        write_text_file(work_dir / "read_short_gap.gamp.json",
                        "{\"name\":\"read016_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":["
                        "{\"position\":{\"node_id\":1,\"offset\":310,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1},"
                        "{\"position\":{\"node_id\":1,\"offset\":330,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":2}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_long_gap.gamp.json",
                        "{\"name\":\"read017_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":["
                        "{\"position\":{\"node_id\":1,\"offset\":310,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1},"
                        "{\"position\":{\"node_id\":1,\"offset\":350,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":2}]},\"score\":40}]}\n");

        write_text_file(work_dir / "read_connection_insert.gamp.json",
                        "{\"name\":\"read020_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":["
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":310,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1}]},"
                        "\"connection\":[{\"next\":1,\"score\":0}],\"score\":10},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":320,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":0,\"to_length\":5,\"sequence\":\"CCCCC\"}],\"rank\":1}]},"
                        "\"next\":[2],\"score\":0},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":330,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":10,\"to_length\":10}],\"rank\":1}]},\"score\":10}]}\n");

        write_text_file(work_dir / "read_overhang.gamp.json",
                        "{\"name\":\"read018_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":90,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":25,\"to_length\":25}],\"rank\":1}]},\"score\":25}]}\n");

        write_text_file(work_dir / "read_parent_only.gamp.json",
                        "{\"name\":\"read019_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":"
                        "{\"node_id\":1,\"offset\":360,\"is_reverse\":false},\"edit\":"
                        "[{\"from_length\":10,\"to_length\":10}],\"rank\":1}]},\"score\":10}]}\n");

        write_text_file(work_dir / "rpvg_read.gamp.json",
                        "{\"name\":\"rpvg001_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0,1],"
                        "\"subpath\":["
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1},\"edit\":[{\"from_length\":1,\"to_length\":1}]}]},\"next\":[2],\"score\":1},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":2},\"edit\":[{\"from_length\":1,\"to_length\":1,\"sequence\":\"A\"}]}]},\"next\":[2],\"score\":-1},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":3},\"edit\":[{\"from_length\":3,\"to_length\":3}]},{\"position\":{\"node_id\":4},\"edit\":[{\"from_length\":2,\"to_length\":2}]}]},\"next\":[3,4],\"score\":5},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":5},\"edit\":[{\"from_length\":2,\"to_length\":2}]}]},\"score\":2},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":6},\"edit\":[{\"from_length\":1,\"to_length\":1,\"sequence\":\"A\"},{\"from_length\":1,\"to_length\":1}]}]},\"score\":0}],"
                        "\"sequence\":\"AAAAAAAA\",\"mapping_quality\":10}\n");

        write_text_file(work_dir / "tiny_reads.gamp.json",
                        "{\"name\":\"tiny_exonic_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":1,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":9,\"to_length\":9}],\"rank\":1}]},\"score\":9}]}\n"
                        "{\"name\":\"tiny_intronic_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":10,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":9,\"to_length\":9}],\"rank\":1}]},\"score\":9}]}\n"
                        "{\"name\":\"tiny_boundary_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":7,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":6,\"to_length\":6}],\"rank\":1}]},\"score\":6}]}\n"
                        "{\"name\":\"tiny_intron_to_exon_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":16,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":6,\"to_length\":6}],\"rank\":1}]},\"score\":6}]}\n"
                        "{\"name\":\"tiny_splice_t1_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":6,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":4,\"to_length\":4}],\"rank\":1}]},\"connection\":[{\"next\":1,\"score\":0}],\"score\":4},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":19,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":6,\"to_length\":6}],\"rank\":1}]},\"score\":6}]}\n"
                        "{\"name\":\"tiny_splice_t2_AAACCCAAGTTTGGGA_AAAAAAAAAAAA\",\"start\":[0],"
                        "\"subpath\":[{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":6,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":4,\"to_length\":4}],\"rank\":1}]},\"connection\":[{\"next\":1,\"score\":0}],\"score\":4},"
                        "{\"path\":{\"mapping\":[{\"position\":{\"node_id\":1,\"offset\":28,\"is_reverse\":false},"
                        "\"edit\":[{\"from_length\":12,\"to_length\":12}],\"rank\":1}]},\"score\":12}]}\n");

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
