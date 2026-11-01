#!/usr/bin/env python3
"""Shared native-count table, profile, and input-cutoff test checks."""

from __future__ import annotations

import gzip
import copy
import csv
import json
import subprocess
import sys
from pathlib import Path

import pyarrow.parquet as pq


TABLES = ("barcodes", "features", "counts", "molecules")
PROFILES = ("cr7-v1", "pansc-strict-v1")


def run(command: list[object], succeeds: bool = True) -> subprocess.CompletedProcess[str]:
    result = subprocess.run([str(item) for item in command], text=True, capture_output=True)
    if (result.returncode == 0) != succeeds:
        raise AssertionError(
            f"unexpected exit {result.returncode}: {' '.join(map(str, command))}\n{result.stderr}"
        )
    return result


def logical_tables(output: Path) -> dict[str, object]:
    manifest = json.loads((output / "manifest.json").read_text())
    return {entry["id"]: entry["logical_tables"] for entry in manifest["profiles"]}


def profile_rows(output: Path, profile: str) -> tuple[list[dict], list[dict]]:
    counts = [row for row in pq.read_table(output / "parquet/counts.parquet").to_pylist()
              if row["profile_id"] == profile]
    molecules = [row for row in pq.read_table(output / "parquet/molecules.parquet").to_pylist()
                 if row["profile_id"] == profile]
    return counts, molecules


def assert_count_outputs(reference: Path, candidate: Path) -> None:
    for name in TABLES:
        expected = pq.read_table(reference / "parquet" / f"{name}.parquet").to_pylist()
        actual = pq.read_table(candidate / "parquet" / f"{name}.parquet").to_pylist()
        if actual != expected:
            raise AssertionError(f"prepared table differs: {name}")
    if logical_tables(candidate) != logical_tables(reference):
        raise AssertionError("prepared logical table digests differ")
    for expected in (reference / "mex").rglob("*.gz"):
        actual = candidate / expected.relative_to(reference)
        if not actual.is_file() or gzip.decompress(actual.read_bytes()) != gzip.decompress(expected.read_bytes()):
            raise AssertionError(f"prepared MEX differs: {expected.relative_to(reference)}")


def assert_profile_output(reference: Path, candidate: Path, profile: str) -> None:
    expected = logical_tables(reference)[profile]
    actual = logical_tables(candidate)[profile]
    if actual != expected or profile_rows(reference, profile) != profile_rows(candidate, profile):
        raise AssertionError(f"prepared {profile} output differs")


def verify_alignment_cutoff(common: list[object], vg: Path, template: dict,
                            root: Path, native: bool = False) -> None:
    """The guard counts a whole QNAME before any reference query, retaining priors."""
    root.mkdir()
    common = list(common)
    gamp_arg = common.index("--gamp")
    del common[gamp_arg:gamp_arg + 2]

    def encode(records: list[dict], name: str) -> Path:
        source = root / f"{name}.json"
        source.write_text("".join(json.dumps(record) + "\n" for record in records))
        destination = root / f"{name}.gamp"
        with destination.open("wb") as stream:
            subprocess.run([str(vg), "view", "-J", "-K", "-k", str(source)],
                           stdout=stream, check=True)
        return destination

    records = []
    for count, umi in ((1, "ACGTACGTACGT"), (2, "TGCATGCATGCA"),
                       (3, "CAGTCAGTCAGT")):
        record = copy.deepcopy(template)
        record["name"] = f"cutoff{count}_AAACCCAAGTTTGGGA_{umi}"
        records.extend(copy.deepcopy(record) for _ in range(count))
    reads = encode(records, "boundary")

    def count(path: Path, label: str, limit: int | None, threads: int = 1):
        output = root / label
        command = common + ["--gamp", path, "--cr7", "--pansc-strict-v1",
                            "--threads", threads, "--out-dir", output]
        if limit is not None:
            command += ["--max-alignments-per-read", limit]
        result = run(command)
        manifest = json.loads((output / "manifest.json").read_text())
        with (output / "summary.tsv").open() as stream:
            summary = {(row["scope"], row["profile_id"], row["metric"]): int(row["value"])
                       for row in csv.DictReader(stream, delimiter="\t")}
        return output, manifest, summary, result

    outputs = []
    for threads in (1, 4):
        output, manifest, summary, _ = count(reads, f"limit2-t{threads}", 2, threads)
        assert manifest["alignment_complexity_filter"] == {
            "algorithm": "gamp-total-routes-v1", "max_alignments_per_read": 2,
            "high_complexity_read_groups": 1, "enabled": True}
        assert summary[("global", ".", "input_read_groups")] == 3
        assert summary[("global", ".", "barcode_prior_groups")] == 3
        assert summary[("global", ".", "exact_barcode_prior_groups")] == 3
        assert summary[("global", ".", "high_complexity_read_groups")] == 1
        for profile in PROFILES:
            assert summary[("profile", profile, "unassigned_high_complexity")] == 1
        outputs.append(output)
    assert_count_outputs(*outputs)
    default, manifest, summary, _ = count(reads, "default", None)
    assert manifest["alignment_complexity_filter"]["max_alignments_per_read"] == 64000000
    assert summary[("global", ".", "high_complexity_read_groups")] == 0
    unlimited, manifest, _, _ = count(reads, "disabled", 0)
    assert manifest["alignment_complexity_filter"]["enabled"] is False
    assert_count_outputs(default, unlimited)

    # Exceeding the route cap must skip a deliberately invalid reference node.
    bad = copy.deepcopy(records[-1])
    bad["subpath"][0]["path"]["mapping"][0]["position"]["node_id"] = "999999999"
    bad_reads = encode([bad, bad, bad], "no-reference-lookup")
    _, _, summary, result = count(bad_reads, "skipped-invalid-node", 2)
    assert summary[("global", ".", "high_complexity_read_groups")] == 1
    assert summary[("global", ".", "barcode_prior_groups")] == 1
    if native:
        assert "native_queries=0" in result.stderr, result.stderr
    failure = run(common + ["--gamp", bad_reads, "--cr7",
                           "--max-alignments-per-read", 3,
                           "--out-dir", root / "retained-invalid-node"], succeeds=False)
    assert "node" in failure.stderr.lower(), failure.stderr
