#!/usr/bin/env python3
"""Prove strict Parent filtering precedes score-window and Ex50 rank selection."""

import sys

import pysam


bam_path, allowlist_sha256 = sys.argv[1:]
records = {}
with pysam.AlignmentFile(bam_path, "rb", check_sq=False) as bam:
    text = bam.text
    assert "@CO\tpanCollapse-evidence-schema:panCollapse-exact-count-v1" in text
    assert "@CO\tpanCollapse-compatible-parent-policy:strict-allowlisted-v1" in text
    assert (
        f"@CO\tpanCollapse-compatible-parent-allowlist-sha256:{allowlist_sha256}" in text
    )
    for record in bam:
        records[record.query_name] = dict(record.get_tags())

# concord has a higher E/F EXON_PARENT_1 candidate and a lower B/F BODY_PARENT_2 candidate.
# Keeping BODY_PARENT_2 must reselect B/F. A downstream winner filter could not recover it.
tags = records["concord"]
assert tags["GT"] == "B", tags
assert tags["GD"] == "F", tags
assert tags["XU"] == "BODY_PARENT_2", tags

# Every group is still represented for barcode correction; groups with no allowlisted exact
# evidence become explicit barcode-only records rather than acquiring fabricated features.
discord = records["discord"]
assert discord["XB"] == "barcode_only", discord
print("PASS: strict Parent allowlist runs before Ex50 score/tier projection")
