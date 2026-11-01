#!/usr/bin/env python3
"""Parallel input and ordered-output modes must produce the one-worker result byte for byte.

Both parallel input modes need --threads >= 8. Each run spools read-assignment diagnostics and
writes RAD, so the spool's per-worker ordinal order and the RAD writer's ordinal order are both
exercised: through the reorder stage by default, and through the output turn when
PANCOLLAPSE_ORDERED_OUTPUT=turn.
"""

from __future__ import annotations

import os
import shutil
import subprocess
import sys
from pathlib import Path

TABLES = ("barcodes", "features", "counts", "molecules")


def run(binary: str, ex50_dir: Path, ex50_fix: Path, bundle: Path, out_dir: Path,
        mode: str, ordered_output: str) -> str:
    shutil.rmtree(out_dir, ignore_errors=True)
    env = dict(os.environ, PANCOLLAPSE_FORCE_WORKER_POOL="1", PANCOLLAPSE_INPUT_MODE=mode,
               PANCOLLAPSE_ORDERED_OUTPUT=ordered_output)
    result = subprocess.run(
        [binary, "count",
         "--gamp", str(ex50_dir / "reads.gamp"), "--xg", str(ex50_dir / "graph.xg"),
         "--count-bundle", str(bundle),
         "--barcode-whitelist", str(ex50_fix / "count_whitelist.txt"),
         "--t2g", str(ex50_fix / "count_t2g.tsv"),
         "--body-t2g", str(ex50_fix / "count_body_t2g.tsv"),
         "--cr7", "--pansc-strict-v1", "--threads", "8",
         "--read-assignments-out", "diagnostics/read_assignments.parquet",
         "--rad-out", "--out-dir", str(out_dir)],
        env=env, capture_output=True, text=True, check=False)
    if result.returncode != 0:
        raise SystemExit(f"{mode} run failed ({result.returncode}):\n{result.stderr}")
    return result.stderr


def main() -> None:
    if len(sys.argv) != 7:
        raise SystemExit("usage: direct_count_input_mode_verify.py PANCOLLAPSE EX50_DIR "
                         "EX50_FIX COUNT_BUNDLE THREADS1_OUT WORK_DIR")
    binary = sys.argv[1]
    ex50_dir, ex50_fix, bundle, one, work = (Path(value) for value in sys.argv[2:])
    work.mkdir(parents=True, exist_ok=True)
    runs = [("read_granular", "reorder", "mode=read_granular"),
            ("read_granular", "turn", "mode=read_granular"),
            ("ordered_batch", "reorder", "mode=ordered_parallel")]
    for mode, ordered_output, marker in runs:
        out_dir = work / f"{mode}_{ordered_output}"
        stderr = run(binary, ex50_dir, ex50_fix, bundle, out_dir, mode, ordered_output)
        pipeline = [line for line in stderr.splitlines() if "input pipeline:" in line]
        assert pipeline and marker in pipeline[0], (mode, pipeline)
        for name in TABLES:
            assert (one / "parquet" / f"{name}.parquet").read_bytes() == (
                out_dir / "parquet" / f"{name}.parquet").read_bytes(), (mode, name)
        assert (one / "diagnostics/read_assignments.parquet").read_bytes() == (
            out_dir / "diagnostics/read_assignments.parquet").read_bytes(), mode
        assert (one / "map.rad").read_bytes() == (out_dir / "map.rad").read_bytes(), (
            mode, ordered_output)
        reorder = [line for line in stderr.splitlines() if "ordered output: mode=reorder" in line]
        assert bool(reorder) == (ordered_output == "reorder"), (ordered_output, reorder)
        if mode == "read_granular" and ordered_output == "reorder":
            reader = [line for line in stderr.splitlines() if "input reader: mode=read_granular" in line]
            assert reader, stderr
            fields = dict(item.split("=", 1) for item in reader[0].split()[3:] if "=" in item)
            # Unaligned and molecule-skipped records are grouped but never decoded.
            assert 0 < int(fields["decoded_records"]) <= int(fields["records"]), fields
            assert fields["peek_fallback_records"] == "0", fields
    print("direct_count_input_mode_verify: ok")


if __name__ == "__main__":
    main()
