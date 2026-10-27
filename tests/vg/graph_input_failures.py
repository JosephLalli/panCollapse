#!/usr/bin/env python3
"""Input combinations and translated-node rejection at the real CLI boundary."""
from pathlib import Path
import subprocess
import sys

binary, vg, root_arg = sys.argv[1:]
root = Path(root_arg)
root.mkdir(parents=True, exist_ok=True)
(root / "t2g.tsv").write_text("A\tGENE_A\tA\n")
base = [binary, "convert", "--gamp", str(root / "unused.gamp"), "--out-dir", str(root / "out"),
        "--legacy-adapter", "hst-v1", "--t2g", str(root / "t2g.tsv")]

def reject(extra, expected):
    result = subprocess.run(base + extra, text=True, capture_output=True)
    if result.returncode == 0 or expected not in result.stderr:
        raise AssertionError(f"{extra}: expected {expected!r}: {result.returncode}\n{result.stderr}")

for flags in (["--gbwt", "paths.gbwt"], ["--gbz", "graph.gbz", "--xg", "graph.xg"],
              ["--gbz", "graph.gbz", "--gbwt", "paths.gbwt"]):
    reject(flags, "exactly one graph source")
(root / "translated.gfa").write_text("H\tVN:Z:1.0\nS\tsegmentA\tACGT\nP\tA\tsegmentA+\t*\n")
subprocess.run([vg, "gbwt", "--num-jobs", "2", "-G", "--gbz-format", "-g", str(root / "translated.gbz"), str(root / "translated.gfa")], check=True)
reject(["--gbz", str(root / "translated.gbz")], "unsupported node-to-segment translation")
(root / "invalid.gbwt").write_bytes(b"not a GBWT index")
# The production loader must reject a corrupt graph input rather than publishing.
reject(["--gbz", str(root / "invalid.gbwt")], "GBZ: Invalid tag")
print("graph input CLI and translation rejections passed")
