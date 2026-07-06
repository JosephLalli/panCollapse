#!/usr/bin/env python3
"""Verify umi_tools count output against a naive recount of the BAM tags.

Independently recomputes, per (gene, cell), the number of unique UMIs over BAM records that
carry an XT gene tag -- ignoring alignment position, exactly as a --per-gene --gene-tag
counter should -- and asserts umi_tools count --method=unique produced the same non-empty
table. Because the recount ignores position, agreement also demonstrates position
independence: identical (cell, UMI, gene) records at different positions collapse to one.

Usage: bam_count_verify.py sorted.bam counts.tsv
"""
import collections
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import pysam  # noqa: E402


def main():
    bam_path, counts_path = sys.argv[1:3]

    umis = collections.defaultdict(set)  # (gene, cell) -> {umi}
    bam = pysam.AlignmentFile(bam_path, "rb")
    for rec in bam:
        if rec.is_unmapped or not rec.has_tag("XT"):
            continue
        umis[(rec.get_tag("XT"), rec.get_tag("CB"))].add(rec.get_tag("UB"))
    expected = {key: len(u) for key, u in umis.items()}

    actual = {}
    with open(counts_path) as fh:
        for line in fh:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if fields[:3] == ["gene", "cell", "count"]:
                continue
            gene, cell, count = fields[0], fields[1], int(fields[2])
            actual[(gene, cell)] = count

    if not expected:
        print("FAIL: no XT-tagged records in the BAM to count")
        return 1
    if expected != actual:
        print(f"FAIL: umi_tools counts != naive recount\n  bam   ={sorted(expected.items())}\n"
              f"  umitools={sorted(actual.items())}")
        return 1
    print(f"bam count: PASS ({sorted(actual.items())})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
