#!/usr/bin/env python3
"""Verify the alevin-fry count matrix for the PathTally interoperability fixture.

The fixture emits one cell (AAACCCAAGTTTGGGA) with two reads carrying distinct UMIs
that map to GENE1 (count 2) and one read mapping to GENE2 (count 1).

Usage: pathtally_af_verify.py <af_quant>/alevin
"""
import os
import sys


def main():
    d = sys.argv[1]
    cols = open(os.path.join(d, "quants_mat_cols.txt")).read().split()
    rows = open(os.path.join(d, "quants_mat_rows.txt")).read().split()
    entries = {}
    with open(os.path.join(d, "quants_mat.mtx")) as fh:
        header_seen = False
        for line in fh:
            if line.startswith("%") or not line.strip():
                continue
            p = line.split()
            if not header_seen:
                header_seen = True  # nrows ncols nnz
                continue
            r, c, v = int(p[0]), int(p[1]), float(p[2])
            entries[(rows[r - 1], cols[c - 1])] = v

    def check(ok, msg):
        if not ok:
            print(f"FAIL: {msg}")
            sys.exit(1)

    bc = "AAACCCAAGTTTGGGA"
    check(rows == [bc], f"expected one barcode {bc}, got rows={rows}")
    check("GENE1" in cols and "GENE2" in cols, f"expected GENE1,GENE2 in cols={cols}")
    check(entries.get((bc, "GENE1")) == 2, f"GENE1 count={entries.get((bc, 'GENE1'))} (expected 2)")
    check(entries.get((bc, "GENE2")) == 1, f"GENE2 count={entries.get((bc, 'GENE2'))} (expected 1)")
    print("alevin-fry matrix: PASS (GENE1=2, GENE2=1)")


if __name__ == "__main__":
    main()
