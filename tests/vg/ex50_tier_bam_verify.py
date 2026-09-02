#!/usr/bin/env python3
"""Verify exact, Parent-preserving GeneFull_Ex50pAS BAM evidence."""

import csv
import itertools
import sys

import pysam


bam_path, ledger_path, score_policy = sys.argv[1:4]
assert score_policy in {"window5", "disabled"}, score_policy

rows_by_path = {}
with open(ledger_path, newline="") as handle:
    for row in csv.DictReader(handle, delimiter="\t"):
        rows_by_path[row["vg_path_name"]] = row

expected = {
    "concord": [
        ("CANON", "GENE", "F", "E", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "B", "P2_BODY", "BODY_PARENT_2"),
    ],
    "majority": [
        ("CANON", "GENE", "F", "P", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "P", "P2_EXON", "EXON_PARENT_2"),
    ],
    "half": [
        ("CANON", "GENE", "F", "B", "P1_BODY", "BODY_PARENT_1"),
        ("CANON", "GENE", "F", "B", "P2_BODY", "BODY_PARENT_2"),
    ],
    "discord": [
        ("CANON", "GENE", "F", "P", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "E", "P2_EXON", "EXON_PARENT_2"),
    ],
    "discordrev": [
        ("CANON", "GENE", "R", "P", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "R", "E", "P2_EXON", "EXON_PARENT_2"),
    ],
    "branch": [
        ("CANON", "GENE", "F", "E", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "P", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "E", "P2_EXON", "EXON_PARENT_2"),
        ("CANON", "GENE", "F", "B", "P2_BODY", "BODY_PARENT_2"),
    ],
    "branch6": [
        ("CANON", "GENE", "F", "E", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "B", "P2_BODY", "BODY_PARENT_2"),
    ],
    "multirecord5": [
        ("CANON", "GENE", "F", "E", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "P", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "E", "P2_EXON", "EXON_PARENT_2"),
        ("CANON", "GENE", "F", "B", "P2_BODY", "BODY_PARENT_2"),
    ],
    "multirecord6": [
        ("CANON", "GENE", "F", "E", "P1_EXON", "EXON_PARENT_1"),
        ("CANON", "GENE", "F", "B", "P2_BODY", "BODY_PARENT_2"),
    ],
}
if score_policy == "disabled":
    expected["branch6"] = list(expected["branch"])
    expected["multirecord6"] = list(expected["multirecord5"])
expected_umis = {
    "concord": "ACGTACGTACGT",
    "majority": "TGCATGCATGCA",
    "half": "AACCGGTTAACC",
    "discord": "TTGGCCAATTGG",
    "discordrev": "AGCTAGCTAGCT",
    "branch": "TCGATCGATCGA",
    "branch6": "GATCGATCGATC",
    "multirecord5": "CCCCAAAAGGGG",
    "multirecord6": "GGGGTTTTAAAA",
}
for first, second in itertools.combinations(expected_umis.values(), 2):
    assert len(first) == len(second) == 12
    assert len(set(first)) > 1 and len(set(second)) > 1
    assert sum(a != b for a, b in zip(first, second)) >= 6

observed = {}
with pysam.AlignmentFile(bam_path, "rb", check_sq=False) as bam:
    assert "@CO\tpanCollapse-evidence-schema:panCollapse-superset-v1" in bam.text, bam.text
    expected_marker = f"@CO\tpanCollapse-ex50-score-window:{'5' if score_policy == 'window5' else 'disabled'}"
    rejected_marker = f"@CO\tpanCollapse-ex50-score-window:{'disabled' if score_policy == 'window5' else '5'}"
    assert expected_marker in bam.text, bam.text
    assert rejected_marker not in bam.text, bam.text
    for record in bam:
        assert not record.is_unmapped, record.query_name
        assert not record.has_tag("XB"), record.query_name
        assert record.get_tag("UB") == expected_umis[record.query_name], record.query_name
        tag_values = {
            tag: record.get_tag(tag).split(";")
            for tag in ("XR", "TX", "GX", "GD", "GL", "GT", "XP", "XU")
        }
        lengths = {len(values) for values in tag_values.values()}
        assert lengths == {len(tag_values["TX"])}, (record.query_name, tag_values)

        exact_entries = []
        for xr, tx, gx, _gd, gl, gt, xp_group, xu_group in zip(
            tag_values["XR"], tag_values["TX"], tag_values["GX"], tag_values["GD"],
            tag_values["GL"], tag_values["GT"], tag_values["XP"], tag_values["XU"],
        ):
            assert xr in {"G", "X"}, (record.query_name, xr)
            if xr == "G":
                assert gl in {"S", "U"} and gt == ".", (record.query_name, gl, gt)
                paths = xp_group.split(",")
                parents = xu_group.split(",")
                wanted_layer = "exon" if gl == "S" else "body"
            else:
                assert gl == "." and gt in {"E", "P", "B"}, (record.query_name, gl, gt)
                paths = xp_group.split(",")
                parents = xu_group.split(",")
                wanted_layer = "body" if gt == "B" else "exon"
                exact_entries.append((tx, gx, _gd, gt, xp_group, xu_group))
            paths = xp_group.split(",")
            parents = xu_group.split(",")
            derived_parents = {rows_by_path[path]["unique_parent"] for path in paths}
            assert derived_parents == set(parents), (
                record.query_name,
                xp_group,
                xu_group,
            )
            for path in paths:
                row = rows_by_path[path]
                assert row["feature_layer"] == wanted_layer, (record.query_name, gt, path)
                assert row["canonical_transcript"] == tx, (record.query_name, tx, path)
                assert row["gene_id"] == gx, (record.query_name, gx, path)
        observed[record.query_name] = exact_entries

assert observed == expected, (observed, expected)
print("PASS: exact Ex50 tiers and Parent/path provenance")
