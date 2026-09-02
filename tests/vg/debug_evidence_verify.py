#!/usr/bin/env python3
"""Verify the normalized audit sidecar and debug-neutral production BAM records."""

import csv
import sys

import pysam


normal_bam, debug_bam, debug_tsv = sys.argv[1:4]


def record_projection(path):
    projected = []
    with pysam.AlignmentFile(path, "rb", check_sq=False) as bam:
        for record in bam:
            projected.append(
                (
                    record.query_name,
                    record.flag,
                    record.reference_name,
                    record.reference_start,
                    record.cigarstring,
                    record.query_sequence,
                    record.qual,
                    tuple(record.get_tags()),
                )
            )
    return projected


normal_records = record_projection(normal_bam)
assert normal_records == record_projection(debug_bam)

with open(debug_tsv, newline="") as handle:
    rows = list(csv.DictReader(handle, delimiter="\t"))

expected_qnames = [record[0] for record in normal_records]
read_rows = [row for row in rows if row["row_type"] == "read"]
candidate_rows = [row for row in rows if row["row_type"] == "candidate"]

assert len(read_rows) == len(expected_qnames), read_rows
assert [row["qname"] for row in read_rows] == expected_qnames, read_rows
assert [int(row["input_group_ordinal"]) for row in read_rows] == list(
    range(1, len(expected_qnames) + 1)
)
assert all(row["schema_version"] == "panCollapse-debug-evidence-v1" for row in rows)
assert all(row["input_name"].startswith(row["qname"] + "_") for row in rows)
assert all(row["splice_edge_count"].isdigit() for row in read_rows)
assert all(
    row[field] == "."
    for row in read_rows
    for field in (
        "candidate_layer",
        "canonical_transcript",
        "gene_id",
        "score",
        "splice_concordant",
    )
)

assert candidate_rows, rows
assert all(row["splice_edge_count"] == "." for row in candidate_rows)
assert all(row["candidate_layer"] in {"exon", "body"} for row in candidate_rows)
assert all(row["canonical_transcript"] == "CANON" for row in candidate_rows)
assert all(row["gene_id"] == "GENE" for row in candidate_rows)
assert all(row["splice_concordant"] in {"true", "false"} for row in candidate_rows)

# Candidate rows are exact global-top rows, before the flank admits near-top rows. Multiple rows
# are allowed only when layers/transcripts tie at that same score.
by_ordinal = {}
for row in candidate_rows:
    by_ordinal.setdefault(int(row["input_group_ordinal"]), []).append(row)
for ordinal, group in by_ordinal.items():
    assert len({int(row["score"]) for row in group}) == 1, (ordinal, group)
    observed_order = [
        (0 if row["candidate_layer"] == "exon" else 1, row["canonical_transcript"])
        for row in group
    ]
    assert observed_order == sorted(observed_order), (ordinal, group)

print("PASS: normalized debug evidence and production-record neutrality")
