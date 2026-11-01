#!/usr/bin/env python3
"""Compare a frozen executable with the memory-reduced build on the nine-read fixture.

Usage: script OLD NEW VG GENERATED_EX50_DIR SOURCE_EX50_DIR OUTPUT_PARENT
This bounded acceptance test is intentionally not a production/reference loader.
"""

import csv
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def semantic_counters(path):
    with path.open() as stream:
        return [row for row in csv.reader(stream, delimiter="\t")
                if len(row) < 3 or not row[2].startswith(("assignment_cache_", "aggregate_spill_"))]


def products(root):
    return {str(path.relative_to(root)): digest(path)
            for path in root.rglob("*") if path.is_file()
            and path.name not in {"manifest.json", "summary.tsv"}}


def main():
    old, new, vg, work, source, parent = map(lambda value: Path(value).resolve(), sys.argv[1:])
    root = Path(tempfile.mkdtemp(prefix="reference-memory-parity-", dir=parent))
    helper = Path(__file__).with_name("graph_input_fixture.py")
    env = dict(os.environ, PANCOLLAPSE_FORCE_WORKER_POOL="1")
    checks = []
    for mode in ("xg", "xg-gbwt", "gbz"):
        for sinks in ("default", "all"):
            baseline = None
            for name, binary, threads in (("old", old, 1), ("new-one", new, 1), ("new-four", new, 4)):
                output = root / f"{mode}-{sinks}-{name}"
                command = [sys.executable, str(helper), mode, str(vg), str(binary), "count",
                           "--gamp", str(work / "reads.gamp"), "--xg", str(work / "graph.xg"),
                           "--count-bundle", str(work / "count_bundle"),
                           "--barcode-whitelist", str(source / "count_whitelist.txt"),
                           "--cr7", "--pansc-strict-v1", "--threads", str(threads),
                           "--out-dir", str(output)]
                if sinks == "all":
                    command += ["--10x-mex", "--rad-out", "--read-assignments-out",
                                "diagnostics/reads.parquet"]
                completed = subprocess.run(command, env=env, capture_output=True, text=True, timeout=120)
                (root / f"{output.name}.stderr").write_text(completed.stderr)
                assert completed.returncode == 0, (output.name, completed.stderr)
                manifest = json.loads((output / "manifest.json").read_text())
                observed = {"products": products(output),
                            "counters": semantic_counters(output / "summary.tsv"),
                            "profiles": manifest["profiles"]}
                if baseline is None:
                    baseline = observed
                else:
                    assert observed == baseline, f"frozen-binary parity failed: {output.name}"
                if name != "old":
                    assert manifest["count_storage"] == "memory-only"
                    assert "reference_released_before_umi=1" in completed.stderr
                checks.append({"case": output.name, "input_reads": 9,
                               "products": observed["products"], "status": "PASS"})
    receipt = {"status": "PASS", "fixture": "nine-read-ex50-tier",
               "old_sha256": digest(old), "new_sha256": digest(new), "checks": checks}
    (root / "VALIDATION.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps({"status": "PASS", "cases": len(checks), "root": str(root)}))


if __name__ == "__main__":
    main()
