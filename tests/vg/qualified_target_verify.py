#!/usr/bin/env python3
"""Compare split targets to an explicitly disambiguated, valid tiny reference."""
import csv
import json
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

import pyarrow.parquet as pq


def run(command, *, succeeds=True):
    result = subprocess.run([str(value) for value in command], capture_output=True, text=True)
    if (result.returncode == 0) != succeeds:
        raise AssertionError(f"unexpected exit {result.returncode}: {command}\n{result.stderr}")
    return result


def main():
    program, fixture, inputs = Path(sys.argv[1]), Path(sys.argv[2]), Path(sys.argv[3])
    root = Path(tempfile.mkdtemp(prefix="qualified-targets-", dir=inputs))
    print(f"retained regression outputs: {root}", flush=True)
    bundles = {}
    for variant in ("explicit", "qualified"):
        source = root / f"{variant}-inputs"
        shutil.copytree(fixture, source)
        ledger = source / "path_identity_ledger.tsv"
        with ledger.open() as handle:
            reader = csv.DictReader(handle, delimiter="\t")
            fields, rows = reader.fieldnames, list(reader)
        for row in rows:
            second = row["unique_parent"].endswith("_2")
            row["source_gene"] = row["gene_id"] = "GENE2" if second else "GENE"
            if variant == "explicit":
                row["canonical_transcript"] = "CANON2" if second else "CANON1"
        with ledger.open("w") as handle:
            writer = csv.DictWriter(handle, fields, delimiter="\t", lineterminator="\n")
            writer.writeheader()
            writer.writerows(rows)
        metadata = source / "count_gene_metadata.tsv"
        metadata.write_text(metadata.read_text() + "GENE2\tSECOND_GENE\tprotein_coding\n")
        policy = source / "count_profile_gene_policy.tsv"
        lines = policy.read_text().splitlines()
        policy.write_text("\n".join(lines + [line.replace("\tGENE\t", "\tGENE2\t")
                                               for line in lines[1:]]) + "\n")
        strong = source / "count_strong_support.tsv"
        lines = strong.read_text().splitlines()
        strong.write_text("\n".join(line.replace("\tGENE\t", "\tGENE2\t")
                                    if line.split("\t")[0].endswith("_2") else line
                                    for line in lines) + "\n")
        bundles[variant] = root / f"{variant}-bundle"
        run([sys.executable, Path(__file__).with_name("make_count_bundle_fixture.py"),
             source, bundles[variant]])

    common = [program, "count", "--barcode-whitelist", fixture / "count_whitelist.txt",
              "--cr7", "--pansc-strict-v1",
              "--max-alignments-per-read", "0",
              "--read-assignments-out", "diagnostics/reads.parquet"]
    outputs = []
    compatibility = root / "compatibility"
    for variant, threads in (("explicit", 1), ("qualified", 1), ("qualified", 4)):
        output = root / f"{variant}-{threads}"
        command = common + ["--gamp", inputs / "reads.gamp", "--xg", inputs / "graph.xg",
                            "--count-bundle", bundles[variant], "--threads", threads,
                            "--out-dir", output]
        if variant == "qualified" and threads == 1:
            command += ["--compatibility-out", compatibility, "--rad-out"]
        run(command)
        outputs.append(output)
    replay = root / "replay"
    run(common + ["--compatibility-in", compatibility, "--count-bundle", bundles["qualified"],
                  "--out-dir", replay])
    outputs.append(replay)
    tables = ["parquet/barcodes.parquet", "parquet/features.parquet", "parquet/counts.parquet",
              "parquet/molecules.parquet", "diagnostics/reads.parquet"]
    for table in tables:
        expected = pq.read_table(outputs[0] / table).to_pylist()
        for output in outputs[1:]:
            if pq.read_table(output / table).to_pylist() != expected:
                raise AssertionError(f"split/replay/threads changed {table}: {output}")
    def semantic_counters(output):
        with (output / "summary.tsv").open() as handle:
            rows = csv.reader(handle, delimiter="\t")
            next(rows)
            return {tuple(row[:3]): row[3] for row in rows
                    if not row[2].startswith(("assignment_cache_", "aggregate_spill_"))}
    for output in outputs[1:]:
        if semantic_counters(output) != semantic_counters(outputs[0]):
            raise AssertionError(f"split/replay/threads changed semantic counters: {output}")
    features = pq.read_table(outputs[1] / "parquet/features.parquet").to_pylist()
    if {row["gene_id"] for row in features} != {"GENE", "GENE2"}:
        raise AssertionError("fixture did not exercise both source-gene identities")
    for output in outputs[1:]:
        manifest = json.loads((output / "manifest.json").read_text())
        if (manifest["qualified_transcript_targets"] != 2 or
                manifest["transcript_target_scheme"] != "source-gene-qualified-v1"):
            raise AssertionError("missing qualified-target provenance")
        if manifest["transcript_target_mapping_id"] != json.loads(
                (outputs[1] / "manifest.json").read_text())["transcript_target_mapping_id"]:
            raise AssertionError("target mapping identity changed across threads/replay")
    if (outputs[1] / "map.rad").read_bytes().count(b"panSCtxg1:Q") != 2:
        raise AssertionError("RAD dictionary did not retain distinct qualified targets")
    raw = common + ["--gamp", inputs / "reads.gamp", "--xg", inputs / "graph.xg",
                    "--count-bundle", bundles["qualified"], "--out-dir", root / "assertion"]
    failure = run(raw + ["--t2g", fixture / "count_t2g.tsv"], succeeds=False)
    if "legacy t2g assertions cannot represent" not in failure.stderr:
        raise AssertionError("unrepresentable legacy t2g assertion was not rejected explicitly")


if __name__ == "__main__":
    main()
