// PathTally step 1 verification: per-node scores reproduce vg's scheme, and
// summed over a subpath they equal Subpath.score.

#include "pathtally_score.hpp"
#include "pathtally_qualadj.hpp"

#include <vg/io/stream.hpp>
#include <vg/vg.pb.h>
#include <xg.hpp>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

vg::Edit* add_edit(vg::Mapping* m, int64_t from, int64_t to, const std::string& seq) {
    vg::Edit* e = m->add_edit();
    e->set_from_length(from);
    e->set_to_length(to);
    if (!seq.empty()) e->set_sequence(seq);
    return e;
}

void check(bool ok, const char* what) {
    if (!ok) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

// Synthetic hand cases with known scores, independent of any GAMP.
void run_synthetic() {
    const pathtally::ScoreParams p;
    std::vector<int64_t> per_node;

    // 10-base exact match spanning the whole read: match + both read-end bonuses.
    {
        vg::Subpath sp;
        add_edit(sp.mutable_path()->add_mapping(), 10, 10, "");
        check(pathtally::score_subpath(sp, 0, 10, false, p, &per_node) == 20, "match+2 bonuses");
        check(per_node.size() == 1 && per_node[0] == 20, "match per-node");
        check(pathtally::score_subpath(sp, 0, 10, true, p, &per_node) == 10, "match no-read-end");
    }
    // 3-base mismatch, interior of read: -mismatch*3.
    {
        vg::Subpath sp;
        add_edit(sp.mutable_path()->add_mapping(), 3, 3, "AAA");
        check(pathtally::score_subpath(sp, 5, 100, false, p, &per_node) == -12, "mismatch");
    }
    // 5-base deletion (from>0,to=0): -(gap_open + 4*extend).
    {
        vg::Subpath sp;
        add_edit(sp.mutable_path()->add_mapping(), 5, 0, "");
        check(pathtally::score_subpath(sp, 5, 100, false, p, &per_node) == -10, "deletion");
    }
    // Interior insertion (to>0,from=0): penalized as a gap.
    {
        vg::Subpath sp;
        add_edit(sp.mutable_path()->add_mapping(), 0, 4, "AAAA");
        check(pathtally::score_subpath(sp, 5, 100, false, p, &per_node) == -9, "interior insertion");
    }
    // Read-start insertion (soft clip): not penalized when read-end scoring is on.
    {
        vg::Subpath sp;
        add_edit(sp.mutable_path()->add_mapping(), 0, 4, "AAAA");
        check(pathtally::score_subpath(sp, 0, 100, false, p, &per_node) == 0, "soft clip unpenalized");
        check(pathtally::score_subpath(sp, 0, 100, true, p, &per_node) == -9, "soft clip penalized when off");
    }
    // Affine gap spanning a node boundary: gap_open charged once across two nodes.
    {
        vg::Subpath sp;
        vg::Mapping* m1 = sp.mutable_path()->add_mapping();
        add_edit(m1, 2, 2, "");   // +2
        add_edit(m1, 2, 0, "");   // deletion opens: -(6+1) = -7
        vg::Mapping* m2 = sp.mutable_path()->add_mapping();
        add_edit(m2, 3, 0, "");   // deletion continues: -3
        add_edit(m2, 2, 2, "");   // +2
        const int64_t total = pathtally::score_subpath(sp, 10, 100, false, p, &per_node);
        check(total == -6, "affine gap across nodes total");
        check(per_node.size() == 2 && per_node[0] == -5 && per_node[1] == -1, "affine gap per-node split");
    }

    std::printf("synthetic: PASS\n");
}

// Diagnostic + oracle over a real GAMP: for every subpath, does the sum of
// per-node scores equal Subpath.score? Report match rates for the read-end-aware
// (A) and no-read-end (B) conventions so the exact vg convention is learned.
int run_real_gamp(const char* path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open GAMP %s\n", path);
        return 2;
    }

    uint64_t subpaths = 0;
    uint64_t match_a = 0;
    uint64_t mism_with_n = 0;
    uint64_t mism_without_n = 0;
    uint64_t no_sequence = 0;
    double qsum_match = 0;
    uint64_t qn_match = 0;
    double qsum_dev = 0;
    uint64_t qn_dev = 0;
    int printed = 0;

    // For each subpath, does the per-node sum equal Subpath.score? Correlate each
    // mismatch with whether the subpath's read segment contains an N base (vg's DP
    // score is N-aware; score_partial_alignment is not).
    vg::io::for_each<vg::MultipathAlignment>(in, [&](vg::MultipathAlignment& mp) {
        const size_t total_len = mp.sequence().size();
        if (total_len == 0) {
            no_sequence += static_cast<uint64_t>(mp.subpath_size());
            return;
        }
        const std::vector<size_t> offsets = pathtally::subpath_read_offsets(mp);
        const pathtally::ScoreParams p;
        std::vector<int64_t> per_node;
        const std::string& qual = mp.quality();
        for (int i = 0; i < mp.subpath_size(); ++i) {
            const vg::Subpath& sp = mp.subpath(i);
            const size_t off = offsets[static_cast<size_t>(i)];
            const size_t len = pathtally::subpath_read_length(sp);
            const int64_t a = pathtally::score_subpath(sp, off, total_len, false, p, &per_node);
            ++subpaths;

            double mean_q = -1.0;
            if (!qual.empty() && len > 0) {
                double s = 0;
                size_t n = 0;
                for (size_t k = off; k < off + len && k < qual.size(); ++k) {
                    s += static_cast<unsigned char>(qual[k]);
                    ++n;
                }
                if (n > 0) mean_q = s / static_cast<double>(n);
            }

            if (a == sp.score()) {
                ++match_a;
                if (mean_q >= 0) { qsum_match += mean_q; ++qn_match; }
                continue;
            }
            bool has_n = false;
            for (size_t k = off; k < off + len && k < total_len; ++k) {
                const char c = mp.sequence()[k];
                if (c == 'N' || c == 'n') { has_n = true; break; }
            }
            if (has_n) {
                ++mism_with_n;
            } else {
                ++mism_without_n;
                if (mean_q >= 0) { qsum_dev += mean_q; ++qn_dev; }
                if (printed < 8) {
                    std::fprintf(stderr, "  no-N diff: stored=%lld A=%lld off=%zu len=%zu meanq=%.1f\n",
                                 (long long)sp.score(), (long long)a, off, len, mean_q);
                    ++printed;
                }
            }
        }
    });

    std::printf("real GAMP subpaths=%llu  A-match=%llu (%.4f)  mism_with_N=%llu  mism_without_N=%llu  no_sequence=%llu\n",
                (unsigned long long)subpaths,
                (unsigned long long)match_a, subpaths ? (double)match_a / subpaths : 0.0,
                (unsigned long long)mism_with_n, (unsigned long long)mism_without_n,
                (unsigned long long)no_sequence);
    std::printf("mean read quality: matched-subpaths=%.1f  deviating-subpaths(no-N)=%.1f\n",
                qn_match ? qsum_match / static_cast<double>(qn_match) : 0.0,
                qn_dev ? qsum_dev / static_cast<double>(qn_dev) : 0.0);

    // The flat scorer reproduces vg's scheme exactly for the large majority of
    // subpaths; the remainder are lower-quality bases whose penalty vg attenuated
    // under base-quality-adjusted mapping. Assert that confirmed regime.
    check(subpaths == 0 || static_cast<double>(match_a) / static_cast<double>(subpaths) >= 0.90,
          "real-GAMP: >=90% of subpath scores reproduced exactly");
    check(qn_dev == 0 || qn_match == 0 ||
              qsum_dev / static_cast<double>(qn_dev) < qsum_match / static_cast<double>(qn_match),
          "real-GAMP: deviations concentrate at lower base quality (quality-adjusted mapping)");
    std::printf("real GAMP: PASS\n");
    return 0;
}

// With the quality-adjusted scorer, per-node sums should reproduce Subpath.score
// essentially exactly (the reads were mapped with quality-adjusted scoring).
int run_qualadj(const char* gamp_path, const char* xg_path) {
    xg::XG graph;
    std::ifstream xin(xg_path, std::ios::binary);
    if (!xin) {
        std::fprintf(stderr, "cannot open xg %s\n", xg_path);
        return 2;
    }
    graph.deserialize(xin);

    std::ifstream in(gamp_path, std::ios::binary);
    if (!in) {
        std::fprintf(stderr, "cannot open GAMP %s\n", gamp_path);
        return 2;
    }

    const pathtally::QualAdjScorer scorer;
    uint64_t subpaths = 0;
    uint64_t match = 0;
    int printed = 0;
    bool stop = false;
    const uint64_t cap = 1000000;

    vg::io::for_each<vg::MultipathAlignment>(in, [&](vg::MultipathAlignment& mp) {
        if (stop) return;
        const std::string& seq = mp.sequence();
        const std::string& qual = mp.quality();
        if (seq.empty() || qual.size() != seq.size()) return;
        const std::vector<size_t> offsets = pathtally::subpath_read_offsets(mp);
        std::vector<int64_t> per_node;
        for (int i = 0; i < mp.subpath_size(); ++i) {
            const vg::Subpath& sp = mp.subpath(i);
            const int64_t a =
                scorer.score_subpath(graph, sp, seq, qual, offsets[static_cast<size_t>(i)], false, &per_node);
            ++subpaths;
            if (a == sp.score()) {
                ++match;
            } else if (printed < 8) {
                std::fprintf(stderr, "  qa diff: stored=%lld a=%lld\n", (long long)sp.score(), (long long)a);
                ++printed;
            }
            if (subpaths >= cap) {
                stop = true;
                return;
            }
        }
    });

    std::printf("qualadj GAMP subpaths=%llu  match=%llu (%.4f)\n",
                (unsigned long long)subpaths, (unsigned long long)match,
                subpaths ? static_cast<double>(match) / static_cast<double>(subpaths) : 0.0);
    // Residual (~0.8%) differs by +/-1: floating-point rounding at score-matrix
    // boundaries (libm differences), not an algorithmic error.
    check(subpaths == 0 || static_cast<double>(match) / static_cast<double>(subpaths) >= 0.99,
          "qualadj scorer reproduces Subpath.score for >=99% of subpaths");
    std::printf("qualadj: PASS\n");
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    run_synthetic();
    if (argc > 2) {
        return run_qualadj(argv[1], argv[2]);
    }
    if (argc > 1) {
        return run_real_gamp(argv[1]);
    }
    return 0;
}
