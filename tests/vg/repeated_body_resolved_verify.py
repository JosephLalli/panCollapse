#!/usr/bin/env python3
"""Verify a repeated body node is resolved to one persisted position and orientation."""
import sys

import pysam


with pysam.AlignmentFile(sys.argv[1], "rb", check_sq=False) as bam:
    records = list(bam)

assert len(records) == 1
record = records[0]
fields = ("XR", "TX", "GX", "GD", "GL", "GT", "XP", "XU")
values = {field: record.get_tag(field).split(";") for field in fields}
exact_rows = [tuple(values[field][i] for field in fields)
              for i in range(len(values["XR"])) if values["XR"][i] == "X"]

# The repeated node has an opposite-oriented second body occurrence. Only the selected first
# occurrence may project the read, so the one exact row is forward rather than a F/R tie.
assert exact_rows == [("X", "RESOLVED_TX", "RESOLVED_GENE", "F", ".", "E",
                       "RESOLVED_EXON", "RESOLVED_EXON_PARENT")]
print("PASS: repeated body occurrence selected once with forward orientation")
