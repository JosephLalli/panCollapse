#!/usr/bin/env python3
"""Independent schema/content/determinism checks for `panCollapse count`."""

from __future__ import annotations

import gzip
import hashlib
import json
import sys
from pathlib import Path

import pyarrow as pa
import pyarrow.parquet as pq


EXPECTED_UMIS = {
    "AACCGGTTAACC",
    "ACGTACGTACGT",
    "CCCCAAAAGGGG",
    "GATCGATCGATC",
    "GGGGTTTTAAAA",
    "TCGATCGATCGA",
    "TGCATGCATGCA",
    "TTGGCCAATTGG",
}


def sha256(path: Path) -> str:
    value = hashlib.sha256()
    value.update(path.read_bytes())
    return value.hexdigest()


def digest_string(value: hashlib._Hash, text: str) -> None:
    encoded = text.encode()
    value.update(len(encoded).to_bytes(8, "big"))
    value.update(encoded)


def profile_semantics(root: Path, profile_id: str) -> tuple[list[tuple], list[tuple], dict]:
    barcodes = {
        row["barcode_index"]: row["barcode"]
        for row in pq.read_table(root / "parquet/barcodes.parquet").to_pylist()
    }
    features = {
        row["feature_index"]: row["gene_id"]
        for row in pq.read_table(root / "parquet/features.parquet").to_pylist()
    }
    counts = [
        (barcodes[row["barcode_index"]], features[row["feature_index"]], row["umi_count"])
        for row in pq.read_table(root / "parquet/counts.parquet").to_pylist()
        if row["profile_id"] == profile_id
    ]
    molecules = [
        (
            barcodes[row["barcode_index"]],
            features[row["feature_index"]],
            row["corrected_umi"],
            row["supporting_read_count"],
            row["raw_umis_collapsed"],
        )
        for row in pq.read_table(root / "parquet/molecules.parquet").to_pylist()
        if row["profile_id"] == profile_id
    ]

    count_digest = hashlib.sha256()
    digest_string(count_digest, "pancollapse-logical-table-v1")
    digest_string(count_digest, f"decoded-counts/{profile_id}")
    for barcode, gene, count in counts:
        digest_string(count_digest, barcode)
        digest_string(count_digest, gene)
        count_digest.update(count.to_bytes(8, "big"))

    molecule_digest = hashlib.sha256()
    digest_string(molecule_digest, "pancollapse-logical-table-v1")
    digest_string(molecule_digest, f"decoded-molecules/{profile_id}")
    for barcode, gene, umi, supporting, collapsed in molecules:
        digest_string(molecule_digest, barcode)
        digest_string(molecule_digest, gene)
        digest_string(molecule_digest, umi)
        molecule_digest.update(supporting.to_bytes(8, "big"))
        molecule_digest.update(collapsed.to_bytes(8, "big"))
    return counts, molecules, {
        "counts": {"rows": len(counts), "sha256": count_digest.hexdigest()},
        "molecules": {"rows": len(molecules), "sha256": molecule_digest.hexdigest()},
    }


def load_manifest(root: Path) -> dict:
    manifest = json.loads((root / "manifest.json").read_text())
    assert manifest["schema"] == "pancollapse-count-output-v1"
    assert manifest["pancollapse_version"] == "0.10.0"
    assert manifest["analysis_scope"] == "frozen-profiles"
    assert manifest["libraries"] == {
        "arrow": "17.0.0",
        "parquet": "17.0.0",
        "zstd": "1.5.7",
    }
    assert [profile["id"] for profile in manifest["profiles"]] == [
        "cr7-v1",
        "pansc-strict-v1",
    ]
    for profile in manifest["profiles"]:
        assert profile["logical_tables"] == profile_semantics(root, profile["id"])[2]
    assert manifest["assignment_cache"]["capacity"] == 131072
    assert manifest["assignment_cache"]["entries"] > 0
    assert (
        manifest["assignment_cache"]["hits"]
        + manifest["assignment_cache"]["misses"]
        == 9
    )
    assert manifest["assignment_cache"]["uncached"] == 0
    assert manifest["counters"]["global"]["input_records"] == 11
    assert manifest["counters"]["global"]["input_read_groups"] == 9
    for profile in ("cr7-v1", "pansc-strict-v1"):
        profile_counters = manifest["counters"]["profiles"][profile]
        assert profile_counters["assigned_unique"] == 8
        assert profile_counters["unassigned_opposite_strand"] == 1
        assert profile_counters["assigned_observations"] == 8
        assert profile_counters["direct_multigene_dropped"] == 0
        assert profile_counters["invalid_umi_dropped"] == 0
        assert profile_counters["umi_n_dropped"] == 0
        assert profile_counters["umi_homopolymer_dropped"] == 0
        assert profile_counters["off_whitelist_uncorrectable_dropped"] == 0
        assert profile_counters["barcode_correction_succeeded"] == 0
        assert profile_counters["barcode_correction_failed"] == 0
    for table in manifest["tables"].values():
        assert sha256(root / table["path"]) == table["output_sha256"]
    for table in manifest["outputs"].values():
        assert sha256(root / table["path"]) == table["output_sha256"]
    return manifest


def assert_schema(table: pa.Table, fields: list[tuple[str, pa.DataType, bool]]) -> None:
    assert [(field.name, field.type, field.nullable) for field in table.schema] == fields


def validate(root: Path) -> dict:
    manifest = load_manifest(root)
    barcodes = pq.read_table(root / "parquet/barcodes.parquet")
    features = pq.read_table(root / "parquet/features.parquet")
    counts = pq.read_table(root / "parquet/counts.parquet")
    molecules = pq.read_table(root / "parquet/molecules.parquet")
    assert_schema(
        barcodes,
        [
            ("barcode_index", pa.uint32(), False),
            ("barcode", pa.string(), False),
            ("exact_read_prior", pa.uint64(), False),
        ],
    )
    assert_schema(
        features,
        [
            ("feature_index", pa.uint32(), False),
            ("gene_id", pa.string(), False),
            ("gene_name", pa.string(), True),
            ("gene_type", pa.string(), True),
        ],
    )
    assert barcodes.to_pylist() == [
        {"barcode_index": 0, "barcode": "AAACCCAAGTTTGGGA", "exact_read_prior": 9}
    ]
    assert features.to_pylist() == [
        {
            "feature_index": 0,
            "gene_id": "GENE",
            "gene_name": "FIXTURE_GENE",
            "gene_type": "protein_coding",
        }
    ]
    assert counts.to_pylist() == [
        {"profile_id": profile, "barcode_index": 0, "feature_index": 0, "umi_count": 8}
        for profile in ("cr7-v1", "pansc-strict-v1")
    ]
    rows = molecules.to_pylist()
    assert len(rows) == 16
    for profile in ("cr7-v1", "pansc-strict-v1"):
        selected = [row for row in rows if row["profile_id"] == profile]
        assert {row["corrected_umi"] for row in selected} == EXPECTED_UMIS
        assert all(row["supporting_read_count"] == 1 for row in selected)
        assert all(row["raw_umis_collapsed"] == 1 for row in selected)

    if "read_assignments" in manifest["outputs"]:
        diagnostics = pq.read_table(root / "diagnostics/read_assignments.parquet")
        assert_schema(
            diagnostics,
            [
                ("ordinal", pa.uint64(), False),
                ("qname", pa.string(), False),
                ("profile_id", pa.string(), False),
                ("raw_barcode", pa.string(), True),
                ("corrected_barcode", pa.string(), True),
                ("umi", pa.string(), True),
                ("terminal_class", pa.string(), False),
                ("selected_gene", pa.string(), True),
                ("selected_tier", pa.string(), True),
                ("reason_bits", pa.uint64(), False),
            ],
        )
        diagnostic_rows = diagnostics.to_pylist()
        assert len(diagnostic_rows) == 18
        assert [row["ordinal"] for row in diagnostic_rows] == [
            ordinal for ordinal in range(9) for _ in range(2)
        ]
        assert {row["profile_id"] for row in diagnostic_rows} == {
            "cr7-v1",
            "pansc-strict-v1",
        }
        assert all(
            row["corrected_barcode"] == "AAACCCAAGTTTGGGA"
            for row in diagnostic_rows
        )
        assert (
            sum(row["terminal_class"] == "assigned_unique" for row in diagnostic_rows)
            == 16
        )
        assert (
            sum(
                row["terminal_class"] == "unassigned_opposite_strand"
                for row in diagnostic_rows
            )
            == 2
        )

    for profile in ("cr7-v1", "pansc-strict-v1"):
        mex = root / "mex" / profile / "raw_feature_bc_matrix"
        assert gzip.open(mex / "barcodes.tsv.gz", "rt").read() == "AAACCCAAGTTTGGGA\n"
        assert gzip.open(mex / "features.tsv.gz", "rt").read() == (
            "GENE\tFIXTURE_GENE\tGene Expression\n"
        )
        matrix_lines = [
            line
            for line in gzip.open(mex / "matrix.mtx.gz", "rt")
            if not line.startswith("%")
        ]
        assert matrix_lines == ["1 1 1\n", "1 1 8\n"]
    return manifest


def main() -> None:
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: direct_count_verify.py THREADS1 THREADS4 CR7_ONLY PANSC_ONLY"
        )
    one = Path(sys.argv[1])
    four = Path(sys.argv[2])
    separate = {
        "cr7-v1": Path(sys.argv[3]),
        "pansc-strict-v1": Path(sys.argv[4]),
    }
    one_manifest = validate(one)
    four_manifest = validate(four)
    assert {
        name: table["logical_sha256"] for name, table in one_manifest["tables"].items()
    } == {
        name: table["logical_sha256"] for name, table in four_manifest["tables"].items()
    }
    for name in ("barcodes", "features", "counts", "molecules"):
        assert (one / "parquet" / f"{name}.parquet").read_bytes() == (
            four / "parquet" / f"{name}.parquet"
        ).read_bytes()
    joint_profiles = {profile["id"]: profile for profile in one_manifest["profiles"]}
    for profile_id, root in separate.items():
        manifest = json.loads((root / "manifest.json").read_text())
        assert manifest["schema"] == "pancollapse-count-output-v1"
        assert [profile["id"] for profile in manifest["profiles"]] == [profile_id]
        separate_profile = manifest["profiles"][0]
        joint_counts, joint_molecules, joint_hashes = profile_semantics(one, profile_id)
        separate_counts, separate_molecules, separate_hashes = profile_semantics(
            root, profile_id
        )
        assert joint_counts == separate_counts
        assert joint_molecules == separate_molecules
        assert joint_hashes == separate_hashes
        assert joint_profiles[profile_id]["logical_tables"] == separate_profile[
            "logical_tables"
        ] == separate_hashes


if __name__ == "__main__":
    main()
