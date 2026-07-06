#!/usr/bin/env python3
"""Parity checker for the optional BAM output.

Confirms every BAM record carries the 10x tags (CB/CR/UB/UR/GX/GN, and XT per the
multi-gene policy), is flagged mapped, and is placed on its primary gene's contig; and
that each read's GX gene set equals the gene set the independent oracle derives for that
read from the GAMP. Reuses pathtally_oracle for the graph-native gene derivation, so the
BAM's gene assignment is checked against an implementation that never touches panCollapse's
C++.

Usage: pathtally_bam_verify.py reads.bam subset.json paths.gfa t2g.tsv expected_count omit|first
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pathtally_oracle as oracle  # noqa: E402
import pysam  # noqa: E402


def original_name(name):
    i = name.rfind("_")
    j = name.rfind("_", 0, i)
    return name[:j]


def main():
    bam_path, subset_json, gfa, t2g_path, want, policy = sys.argv[1:7]
    want = int(want)

    hst_gene = {}
    with open(t2g_path) as fh:
        for line in fh:
            p = line.split()
            if len(p) >= 2:
                hst_gene[p[0]] = p[1]
    hst_set = set(hst_gene)
    tx_gene = {oracle.transcript_id(h): g for h, g in hst_gene.items()}

    groups, cur, cur_name, touched = [], [], None, set()
    with open(subset_json) as fh:
        for line in fh:
            mp = json.loads(line)
            name = mp.get("name", "")
            if name != cur_name:
                if cur:
                    groups.append((cur_name, cur))
                cur, cur_name = [], name
            cur.append(mp)
            for sp in mp.get("subpath", []):
                for m in sp.get("path", {}).get("mapping", []):
                    touched.add(str(m.get("position", {}).get("node_id")))
        if cur:
            groups.append((cur_name, cur))

    node_paths = oracle.load_node_paths(gfa, hst_set, touched)

    expected = {}  # original read name -> sorted unique gene list
    for name, group in groups:
        if oracle.parse_molecule(name) is None:
            continue
        targets = oracle.predict(group, node_paths)
        if not targets:
            continue
        expected[original_name(name)] = sorted({tx_gene[tx] for tx, _ in targets})

    seen = set()
    bam = pysam.AlignmentFile(bam_path, "rb")
    for rec in bam:
        q = rec.query_name
        if q not in expected:
            print(f"FAIL: BAM has record {q} the oracle did not emit")
            return 1
        seen.add(q)
        genes = expected[q]
        if rec.is_unmapped:
            print(f"FAIL: {q} is flagged unmapped")
            return 1
        for tag in ("CB", "CR", "UB", "UR", "GX", "GN"):
            if not rec.has_tag(tag):
                print(f"FAIL: {q} missing {tag}")
                return 1
        if rec.get_tag("GX").split(";") != genes:
            print(f"FAIL: {q} GX={rec.get_tag('GX')} expected {genes}")
            return 1
        if rec.get_tag("GN").split(";") != genes:
            print(f"FAIL: {q} GN != GX")
            return 1
        if rec.get_tag("CR") != rec.get_tag("CB") or rec.get_tag("UR") != rec.get_tag("UB"):
            print(f"FAIL: {q} raw CR/UR != CB/UB")
            return 1
        if rec.reference_name != genes[0]:
            print(f"FAIL: {q} placed on {rec.reference_name}, expected primary gene {genes[0]}")
            return 1
        want_xt = len(genes) == 1 or policy == "first"
        have_xt = rec.has_tag("XT")
        if want_xt and (not have_xt or rec.get_tag("XT") != genes[0]):
            print(f"FAIL: {q} expected XT={genes[0]}, got {rec.get_tag('XT') if have_xt else None}")
            return 1
        if not want_xt and have_xt:
            print(f"FAIL: {q} has XT={rec.get_tag('XT')} but is multi-gene under omit policy")
            return 1

    missing = sorted(set(expected) - seen)
    if missing:
        print(f"FAIL: BAM missing records for {missing}")
        return 1
    if len(seen) != want:
        print(f"FAIL: expected {want} BAM records, got {len(seen)}")
        return 1
    print(f"bam parity: PASS ({len(seen)} records; tags + GX gene sets match the oracle)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
