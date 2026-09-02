#!/usr/bin/env python3
"""Parity checker for the optional BAM output.

Confirms every valid input group has exactly one BAM record. Feature records carry the
10x tags (CB/CR/UB/UR/GX/GN, and XT per the multi-gene policy), are mapped nominally,
and have the gene set the independent oracle derives. Groups for which the oracle finds
no feature must instead be unmapped XB:Z:barcode_only records with molecule tags and no
feature tags.

Usage: pathtally_bam_verify.py reads.bam subset.json paths.gfa t2g.tsv expected_count omit|first
"""
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pathtally_oracle as oracle  # noqa: E402
import pysam  # noqa: E402


def original_name(name):
    molecule_name = name
    quality_sep = molecule_name.rfind("_")
    if quality_sep >= 0 and molecule_name[quality_sep + 1:].startswith("uy"):
        molecule_name = molecule_name[:quality_sep]
    quality_sep = molecule_name.rfind("_")
    if quality_sep >= 0 and molecule_name[quality_sep + 1:].startswith("cy"):
        molecule_name = molecule_name[:quality_sep]
    i = molecule_name.rfind("_")
    j = molecule_name.rfind("_", 0, i)
    return molecule_name[:j]


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
    valid = set()
    for name, group in groups:
        if oracle.parse_molecule(name) is None:
            continue
        valid.add(original_name(name))
        targets = oracle.predict(group, node_paths)
        if not targets:
            continue
        expected[original_name(name)] = sorted({tx_gene[tx] for tx, _ in targets})

    seen = set()
    feature_seen = set()
    barcode_only_seen = set()
    bam = pysam.AlignmentFile(bam_path, "rb")
    for rec in bam:
        q = rec.query_name
        if q in seen:
            print(f"FAIL: BAM has more than one record for {q}")
            return 1
        seen.add(q)
        if rec.has_tag("XB") and rec.get_tag("XB") == "barcode_only":
            barcode_only_seen.add(q)
            if q not in valid or q in expected:
                print(f"FAIL: unexpected barcode-only record {q}")
                return 1
            if not rec.is_unmapped:
                print(f"FAIL: barcode-only record {q} is mapped")
                return 1
            for tag in ("CB", "CR", "UB", "UR"):
                if not rec.has_tag(tag):
                    print(f"FAIL: barcode-only record {q} missing {tag}")
                    return 1
            for tag in ("GX", "GN", "GD", "TX", "GL", "XT"):
                if rec.has_tag(tag):
                    print(f"FAIL: barcode-only record {q} has feature tag {tag}")
                    return 1
            continue
        if q not in expected:
            print(f"FAIL: BAM has record {q} the oracle did not emit")
            return 1
        feature_seen.add(q)
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

    missing = sorted(valid - seen)
    if missing:
        print(f"FAIL: BAM missing records for {missing}")
        return 1
    if len(feature_seen) != want:
        print(f"FAIL: expected {want} BAM feature records, got {len(feature_seen)}")
        return 1
    if feature_seen != set(expected):
        print(f"FAIL: feature-record names {sorted(feature_seen)} != oracle {sorted(expected)}")
        return 1
    print(
        f"bam parity: PASS ({len(feature_seen)} feature + {len(barcode_only_seen)} barcode-only "
        "records; feature tags/GX match the oracle)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
