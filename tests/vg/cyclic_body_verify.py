#!/usr/bin/env python3
"""Verify Parent-scoped cyclic-body degradation preserves a shared clean sibling."""
import sys

import pysam

cyclic_bam, clean_bam = sys.argv[1:3]

def rows(path):
    with pysam.AlignmentFile(path, "rb", check_sq=False) as bam:
        return list(bam)

cyclic = rows(cyclic_bam)
clean = rows(clean_bam)
assert len(cyclic) == len(clean) == 1

FIELDS = ("XR", "TX", "GX", "GD", "GL", "GT", "XP", "XU")


def exact_for(record, parent):
    values = {tag: record.get_tag(tag).split(";")
              for tag in FIELDS}
    return [tuple(values[tag][i] for tag in FIELDS)
            for i in range(len(values["XR"]))
            if values["XR"][i] == "X" and values["XU"][i] == parent]

# The ambiguous Parent has no evidence for this clean-sibling read. Both Parents resolve to the
# same canonical transcript, so a target-scoped degradation would still contaminate this record.
all_cyclic_parents = {parent for slot in cyclic[0].get_tag("XU").split(";")
                       for parent in slot.split(",")}
assert "CYCLIC_BODY_PARENT" not in all_cyclic_parents
assert exact_for(cyclic[0], "CYCLIC_EXON_PARENT") == []
clean_cyclic = exact_for(cyclic[0], "CLEAN_EXON_PARENT")
clean_only = exact_for(clean[0], "CLEAN_EXON_PARENT")
assert len(clean_cyclic) == len(clean_only) == 1
assert clean_cyclic == clean_only
assert cyclic[0].to_string() == clean[0].to_string()
print("PASS: one cyclic Parent degrades; clean shared-canonical record is identical")
