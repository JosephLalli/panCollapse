#!/usr/bin/env python3
"""Exercise cache hits with distinct molecules and compare both fact authorities."""
import copy
import itertools
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

import pyarrow.parquet as pq


def main():
    binary, vg, fixture, work, bundle = map(Path, sys.argv[1:])
    env = dict(os.environ, PANCOLLAPSE_FORCE_WORKER_POOL="1", PANCOLLAPSE_PROFILE_TIMING="1")
    records = [json.loads(line) for line in (fixture / "reads.gamp.json").read_text().splitlines()]
    groups = [list(values) for _, values in itertools.groupby(records, key=lambda row: row["name"])]
    assert len(groups) == 9
    with tempfile.TemporaryDirectory(prefix="cache-cli-", dir=work) as temporary:
        root = Path(temporary)
        repeated = []
        for serial, group in enumerate(groups * 4):
            barcode = group[0]["name"].rsplit("_", 2)[1]
            # Two copies of every symbol keep distinct molecules at Hamming
            # distance >=2, outside the one-substitution UMI correction radius.
            umi = "AC" + "".join("ACGT"[(serial >> shift) & 3] * 2 for shift in range(8, -1, -2))
            for record in group:
                row = copy.deepcopy(record)
                row["name"] = f"cache{serial}_{barcode}_{umi}"
                repeated.append(row)
        gamp = root / "reads.gamp"
        with gamp.open("wb") as output:
            subprocess.run([str(vg), "view", "-J", "-K", "-k", "-"],
                           input="".join(json.dumps(row) + "\n" for row in repeated).encode(),
                           stdout=output, stderr=subprocess.PIPE, check=True, env=env)
        annotation, reference = root / "annotation.gff3", root / "reference.gff3"
        reference.write_text("##gff-version 3\n")
        annotation.write_text(
            "##gff-version 3\nchr1\tfixture\tgene\t1\t40\t.\t+\t.\tID=GENE;gene_type=protein_coding\n"
            + "".join(
                f"chr1\tfixture\ttranscript\t1\t40\t.\t+\t.\tID={parent};Parent={owner};"
                "source_gene=GENE;gene_type=protein_coding;transcript_type=protein_coding\n"
                for parent, owner in (("EXON_PARENT_1", "GENE"), ("BODY_PARENT_1", "EXON_PARENT_1"),
                                      ("EXON_PARENT_2", "GENE"), ("BODY_PARENT_2", "EXON_PARENT_2"))
            )
        )
        authorities = {
            "bundle": ["--count-bundle", str(bundle)],
            "annotation": ["--annotation", str(annotation), "--annotation-reference", str(reference),
                           "--path-identity-ledger", str(fixture / "path_identity_ledger.tsv")],
        }
        manifests = {}
        for name, authority, threads, capture, sidecars in (
            ("bundle-one", "bundle", 1, False, "all"), ("bundle-four", "bundle", 4, False, "all"),
            ("bundle-capture", "bundle", 4, True, "all"),
            ("bundle-diagnostics", "bundle", 4, False, "diagnostics"),
            ("bundle-count-only", "bundle", 4, False, "none"),
            ("annotation-one", "annotation", 1, False, "all"),
            ("annotation-four", "annotation", 4, False, "all"),
            ("annotation-diagnostics", "annotation", 4, False, "diagnostics"),
            ("annotation-count-only", "annotation", 4, False, "none"),
        ):
            output = root / name
            argv = [str(binary), "count", "--gamp", str(gamp), "--xg", str(work / "graph.xg"),
                    "--barcode-whitelist", str(fixture / "count_whitelist.txt"), "--cr7",
                    "--pansc-strict-v1", "--threads", str(threads), "--count-memory-budget", "1MiB",
                    "--10x-mex",
                    "--out-dir", str(output)] + authorities[authority]
            if sidecars == "all":
                argv += ["--rad-out"]
            if sidecars != "none":
                argv += ["--read-assignments-out", "diagnostics/reads.parquet"]
            if capture:
                argv += ["--compatibility-out", str(root / "compatibility")]
            result = subprocess.run(argv, capture_output=True, text=True, env=env, timeout=60)
            assert result.returncode == 0, result.stderr
            regions = re.search(r"region_sum_seconds=([\deE.+-]+) compute_total_seconds=([\deE.+-]+)",
                                result.stderr)
            assert regions, result.stderr
            region_sum, compute_total = map(float, regions.groups())
            assert region_sum <= compute_total * 1.00002 + 1e-8, (name, regions.group())
            manifest = json.loads((output / "manifest.json").read_text())
            manifests[name] = manifest
            timings = manifest["timings"]
            phase_keys = ("read_processing_seconds", "compatibility_finalize_seconds",
                          "count_finalize_seconds", "diagnostics_merge_export_seconds")
            assert all(timings[key] >= 0 for key in phase_keys)
            assert sum(timings[key] for key in phase_keys) <= timings["processing_seconds"] + 1e-8
            assert timings["read_processing_seconds"] > 0
            assert timings["count_finalize_seconds"] > 0
            assert (timings["diagnostics_merge_export_seconds"] > 0) == (sidecars != "none")
            assert manifest["assignment_cache"]["hits"] > 0, "fixture did not exercise reuse"
            assert manifest["counters"]["global"]["input_read_groups"] == 36
            counts = pq.read_table(output / "parquet/counts.parquet").to_pylist()
            for profile in ("cr7-v1", "pansc-strict-v1"):
                assert sum(row["umi_count"] for row in counts if row["profile_id"] == profile) == 32
            one_name = authority + "-one"
            reference_manifest = manifests[one_name]
            assert manifest["counters"] == reference_manifest["counters"]
            if name != one_name:
                for section in ("tables", "outputs"):
                    omitted = set()
                    if section == "outputs":
                        if sidecars != "all":
                            omitted.add("rad")
                        if sidecars == "none":
                            omitted.add("read_assignments")
                    assert manifest[section].keys() == reference_manifest[section].keys() - omitted
                    for role, table in manifest[section].items():
                        # The summary reports RAD record counts, which differ by
                        # requested artifact mode. All biological tables, MEX and
                        # diagnostics must still be byte-identical.
                        if section == "outputs" and role == "summary" and sidecars != "all":
                            continue
                        expected = root / one_name / reference_manifest[section][role]["path"]
                        assert (output / table["path"]).read_bytes() == expected.read_bytes(), (name, role)
            assert [profile["logical_tables"] for profile in manifest["profiles"]] == [
                profile["logical_tables"] for profile in manifests["bundle-one"]["profiles"]
            ]
        print("cache hits, 32 molecules/profile, one/four workers, capture, artifact modes, phases and both authorities: PASS")


if __name__ == "__main__":
    main()
