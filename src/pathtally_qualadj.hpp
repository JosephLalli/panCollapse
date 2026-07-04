#pragma once

// PathTally optional scorer: a faithful reimplementation of vg's QualAdjAligner
// base-quality-adjusted scoring (vg src/alignment_scorer.cpp:
// QualAdjAlignmentScorer). Reads mapped with quality-adjusted scoring store
// subpath scores under this scheme; using it makes the per-node sum reproduce
// Subpath.score exactly. It needs the read sequence + qualities and the graph
// (for reference bases), so it is heavier than the flat scorer and is opt-in.

#include <handlegraph/handle_graph.hpp>
#include <vg/vg.pb.h>

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace pathtally {

// A=0, C=1, G=2, T=3, anything else (incl. N)=4, matching the 5-wide score
// matrix indexing.
inline int nt_index(char c) {
    switch (c) {
    case 'A':
    case 'a':
        return 0;
    case 'C':
    case 'c':
        return 1;
    case 'G':
    case 'g':
        return 2;
    case 'T':
    case 't':
        return 3;
    default:
        return 4;
    }
}

class QualAdjScorer {
public:
    explicit QualAdjScorer(int match = 1, int mismatch = 4, int gap_open = 6, int gap_extension = 1,
                           int full_length_bonus = 5, double gc_content = 0.5, int max_qual = 255)
        : gap_open_(gap_open), gap_extension_(gap_extension) {
        double m[16];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m[i * 4 + j] = (i == j) ? static_cast<double>(match) : static_cast<double>(-mismatch);
            }
        }
        const double nt_freqs[4] = {0.5 * (1 - gc_content), 0.5 * gc_content, 0.5 * gc_content,
                                    0.5 * (1 - gc_content)};
        const double log_base = recover_log_base(m, nt_freqs);
        build_matrix(m, nt_freqs, log_base, max_qual);
        build_bonuses(full_length_bonus, log_base, max_qual);
    }

    // Per-node scores for one subpath under the quality-adjusted scheme.
    int64_t score_subpath(const handlegraph::HandleGraph& graph, const vg::Subpath& subpath,
                          const std::string& read_seq, const std::string& read_qual,
                          size_t read_start_offset, bool no_read_end_scoring,
                          std::vector<int64_t>* per_node) const {
        const vg::Path& path = subpath.path();
        if (per_node != nullptr) {
            per_node->assign(static_cast<size_t>(path.mapping_size()), 0);
        }
        const size_t read_len = read_seq.size();
        if (read_start_offset > read_len) {
            throw std::runtime_error(
                "qualadj scorer: read_start_offset " + std::to_string(read_start_offset) +
                " exceeds read_seq length " + std::to_string(read_len));
        }
        if (read_start_offset > read_qual.size()) {
            throw std::runtime_error(
                "qualadj scorer: read_start_offset " + std::to_string(read_start_offset) +
                " exceeds read_qual length " + std::to_string(read_qual.size()));
        }
        int64_t total = 0;
        size_t read_pos = read_start_offset;
        bool in_deletion = false;

        for (int i = 0; i < path.mapping_size(); ++i) {
            const vg::Mapping& mapping = path.mapping(i);
            int64_t node_score = 0;
            const std::string node_seq =
                graph.get_sequence(graph.get_handle(mapping.position().node_id(), mapping.position().is_reverse()));
            size_t ref_pos = static_cast<size_t>(mapping.position().offset());

            for (int j = 0; j < mapping.edit_size(); ++j) {
                const vg::Edit& edit = mapping.edit(j);
                const int64_t from = edit.from_length();
                const int64_t to = edit.to_length();
                if (from > 0) {
                    if (to > 0) {
                        const size_t to_sz = static_cast<size_t>(to);
                        if (read_pos + to_sz > read_seq.size()) {
                            throw std::runtime_error(
                                "qualadj scorer: read_seq index out of range: end " +
                                std::to_string(read_pos + to_sz) + " > size " +
                                std::to_string(read_seq.size()));
                        }
                        if (read_pos + to_sz > read_qual.size()) {
                            throw std::runtime_error(
                                "qualadj scorer: read_qual index out of range: end " +
                                std::to_string(read_pos + to_sz) + " > size " +
                                std::to_string(read_qual.size()));
                        }
                        if (ref_pos + to_sz > node_seq.size()) {
                            throw std::runtime_error(
                                "qualadj scorer: node_seq index out of range: end " +
                                std::to_string(ref_pos + to_sz) + " > size " +
                                std::to_string(node_seq.size()) + " (node " +
                                std::to_string(mapping.position().node_id()) + ")");
                        }
                        for (int64_t k = 0; k < to; ++k) {
                            const int q = static_cast<uint8_t>(read_qual[read_pos + static_cast<size_t>(k)]);
                            const int refn = nt_index(node_seq[ref_pos + static_cast<size_t>(k)]);
                            const int readn = nt_index(read_seq[read_pos + static_cast<size_t>(k)]);
                            node_score += matrix_[25 * static_cast<size_t>(q) + 5 * refn + readn];
                        }
                        if (!no_read_end_scoring) {
                            if (read_pos == 0) {
                                node_score += bonus_[static_cast<uint8_t>(read_qual[read_pos])];
                            }
                            if (read_pos + static_cast<size_t>(to) == read_len) {
                                node_score += bonus_[static_cast<uint8_t>(read_qual[read_pos + static_cast<size_t>(to) - 1])];
                            }
                        }
                        in_deletion = false;
                    } else if (in_deletion) {
                        node_score -= from * gap_extension_;
                    } else {
                        node_score -= gap_open_ + (from - 1) * gap_extension_;
                        in_deletion = true;
                    }
                } else if (to > 0) {
                    const bool at_read_end = (read_pos == 0) || (read_pos + static_cast<size_t>(to) == read_len);
                    if (no_read_end_scoring || !at_read_end) {
                        node_score -= gap_open_ + (to - 1) * gap_extension_;
                    }
                    in_deletion = false;
                }
                read_pos += static_cast<size_t>(to);
                ref_pos += static_cast<size_t>(from);
            }
            total += node_score;
            if (per_node != nullptr) {
                (*per_node)[static_cast<size_t>(i)] = node_score;
            }
        }
        return total;
    }

private:
    static double partition(double lambda, const double m[16], const double nt_freqs[4]) {
        double sum = 0.0;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                sum += nt_freqs[i] * nt_freqs[j] * std::exp(lambda * m[i * 4 + j]);
            }
        }
        return sum;
    }

    static double recover_log_base(const double m[16], const double nt_freqs[4], double tol = 1e-12) {
        double lambda = 1.0;
        double lower_bound = 0.0;
        double upper_bound = 0.0;
        double p = partition(lambda, m, nt_freqs);
        if (p < 1.0) {
            lower_bound = lambda;
            while (p <= 1.0) {
                lower_bound = lambda;
                lambda *= 2.0;
                p = partition(lambda, m, nt_freqs);
            }
            upper_bound = lambda;
        } else {
            upper_bound = lambda;
            while (p >= 1.0) {
                upper_bound = lambda;
                lambda /= 2.0;
                p = partition(lambda, m, nt_freqs);
            }
            lower_bound = lambda;
        }
        while (upper_bound / lower_bound - 1.0 > tol) {
            lambda = 0.5 * (lower_bound + upper_bound);
            if (partition(lambda, m, nt_freqs) < 1.0) {
                lower_bound = lambda;
            } else {
                upper_bound = lambda;
            }
        }
        return 0.5 * (lower_bound + upper_bound);
    }

    void build_matrix(const double m[16], const double nt_freqs[4], double log_base, int max_qual) {
        double align_prob[16];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                align_prob[i * 4 + j] = std::exp(log_base * m[i * 4 + j]) * nt_freqs[i] * nt_freqs[j];
            }
        }
        double align_complement_prob[16];
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                double sum = 0.0;
                for (int k = 0; k < 4; ++k) {
                    if (k != j) sum += align_prob[i * 4 + k];
                }
                align_complement_prob[i * 4 + j] = sum;
            }
        }
        const int lowest_meaningful_qual = static_cast<int>(std::ceil(-10.0 * std::log10(0.75)));
        matrix_.assign(static_cast<size_t>(25) * (static_cast<size_t>(max_qual) + 1), 0);
        for (int q = 0; q <= max_qual; ++q) {
            const double err = std::pow(10.0, -static_cast<double>(q) / 10.0);
            for (int i = 0; i < 5; ++i) {
                for (int j = 0; j < 5; ++j) {
                    int score = 0;
                    if (i != 4 && j != 4 && q >= lowest_meaningful_qual) {
                        score = static_cast<int>(std::round(
                            std::log(((1.0 - err) * align_prob[i * 4 + j] + (err / 3.0) * align_complement_prob[i * 4 + j]) /
                                     (nt_freqs[i] * ((1.0 - err) * nt_freqs[j] + (err / 3.0) * (1.0 - nt_freqs[j])))) /
                            log_base));
                    }
                    matrix_[static_cast<size_t>(q) * 25 + static_cast<size_t>(i) * 5 + static_cast<size_t>(j)] = score;
                }
            }
        }
    }

    void build_bonuses(int full_length_bonus, double log_base, int max_qual) {
        const double p_full_len = std::exp(log_base * full_length_bonus) / (1.0 + std::exp(log_base * full_length_bonus));
        bonus_.assign(static_cast<size_t>(max_qual) + 1, 0);
        int lowest_meaningful_qual = static_cast<int>(std::ceil(-10.0 * std::log10(0.75)));
        ++lowest_meaningful_qual;
        for (int q = lowest_meaningful_qual; q <= max_qual; ++q) {
            const double err = std::pow(10.0, -static_cast<double>(q) / 10.0);
            const double score =
                std::log(((1.0 - err * 4.0 / 3.0) * p_full_len + (err * 4.0 / 3.0) * (1.0 - p_full_len)) /
                         (1.0 - p_full_len)) /
                log_base;
            bonus_[static_cast<size_t>(q)] = static_cast<int>(std::round(score));
        }
    }

    int gap_open_;
    int gap_extension_;
    std::vector<int> matrix_;
    std::vector<int> bonus_;
};

}  // namespace pathtally
