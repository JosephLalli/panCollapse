#include <handlegraph/path_position_handle_graph.hpp>
#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

namespace {

struct GtfExon {
    std::string seqname;
    size_t start = 0;
    size_t end = 0;
    std::string transcript_id;
};

GtfExon read_single_exon_gtf(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) {
        throw std::runtime_error("cannot open GTF");
    }

    std::string seqname;
    std::string source;
    std::string feature;
    size_t start_1_based = 0;
    size_t end_inclusive = 0;
    std::string score;
    std::string strand;
    std::string frame;
    std::string attributes;
    in >> seqname >> source >> feature >> start_1_based >> end_inclusive >> score >> strand >> frame;
    std::getline(in, attributes);

    if (feature != "exon" || attributes.find("transcript_id \"SRC_A\"") == std::string::npos) {
        throw std::runtime_error("unexpected GTF contents");
    }

    return {seqname, start_1_based - 1, end_inclusive, "SRC_A"};
}

} // namespace

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: gamp_xg_gtf_projection_smoke GRAPH_XG ANNOTATION_GTF READS_GAMP\n";
        return 2;
    }

    try {
        const std::string xg_filename = argv[1];
        const std::string gtf_filename = argv[2];
        const std::string gamp_filename = argv[3];

        const GtfExon exon = read_single_exon_gtf(gtf_filename);
        if (exon.seqname != "chrFixture" || exon.start != 2 || exon.end != 8) {
            throw std::runtime_error("GTF coordinate conversion failed");
        }

        xg::XG graph;
        std::ifstream xg_in(xg_filename, std::ios::binary);
        if (!xg_in) {
            throw std::runtime_error("cannot open XG");
        }
        graph.deserialize(xg_in);

        if (!graph.has_path("chrFixture")) {
            throw std::runtime_error("XG is missing chrFixture path");
        }

        handlegraph::PathPositionHandleGraph* positioned_graph = &graph;
        const handlegraph::path_handle_t path = graph.get_path_handle("chrFixture");
        if (graph.get_path_length(path) != 10) {
            throw std::runtime_error("unexpected chrFixture path length");
        }

        std::ifstream gamp_in(gamp_filename, std::ios::binary);
        if (!gamp_in) {
            throw std::runtime_error("cannot open GAMP");
        }

        size_t records = 0;
        bool saw_expected_projection = false;

        vg::io::for_each<vg::MultipathAlignment>(gamp_in, [&](vg::MultipathAlignment& alignment) {
            ++records;
            if (alignment.subpath_size() != 1) {
                throw std::runtime_error("unexpected GAMP record");
            }

            const vg::Mapping& mapping = alignment.subpath(0).path().mapping(0);
            const auto& position = mapping.position();
            const handlegraph::handle_t handle = graph.get_handle(position.node_id(), position.is_reverse());

            positioned_graph->for_each_step_position_on_handle(
                handle,
                [&](const handlegraph::step_handle_t& step, const bool& step_is_reverse, const size_t& step_position) {
                    const std::string path_name = graph.get_path_name(graph.get_path_handle_of_step(step));
                    if (path_name == "chrFixture" && !step_is_reverse && step_position == 0) {
                        const size_t projected_start = step_position + static_cast<size_t>(position.offset());
                        const size_t projected_end = projected_start + static_cast<size_t>(mapping.edit(0).from_length());
                        saw_expected_projection = (projected_start == 2 && projected_end == 6 &&
                                                   projected_start < exon.end && projected_end > exon.start);
                    }
                    return true;
                });
        });

        if (records != 1 || !saw_expected_projection) {
            throw std::runtime_error("GAMP/XG/GTF projection smoke failed");
        }
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }

    return 0;
}
