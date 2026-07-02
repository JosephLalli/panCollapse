#!/usr/bin/env python3
"""Append a synthetic 10x raw CB/UMI to each read name for a PathTally fixture.

vg sim emits reads without 10x barcodes; panCollapse parses
`<original_name>_<raw_CB>_<raw_UMI>` from the GAMP name. This appends a fixed
16bp CB and a deterministic per-read 12bp UMI (so records stay distinct).

Usage: fixture_tag_reads.py in.fastq > out.fastq
"""
import sys

CB = "AAACCCAAGTTTGGGA"  # 16bp
_B = "ACGT"


def umi(i):
    s = ""
    for _ in range(12):
        s = _B[i & 3] + s
        i >>= 2
    return s


def main():
    with open(sys.argv[1]) as fh:
        lines = [ln.rstrip("\n") for ln in fh]
    out = []
    i = 0
    k = 0
    while k + 3 < len(lines) + 1 and k < len(lines) and lines[k].startswith("@"):
        name = lines[k][1:].split()[0]
        out.append(f"@{name}_{CB}_{umi(i)}")
        out.append(lines[k + 1])
        out.append(lines[k + 2])
        out.append(lines[k + 3])
        i += 1
        k += 4
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
