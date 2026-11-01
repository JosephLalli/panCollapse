#!/usr/bin/env python3
"""Retired count routes fail before reference or GAMP I/O."""

import subprocess
import sys
import tempfile
from pathlib import Path


def main():
    program = sys.argv[1]
    with tempfile.TemporaryDirectory(prefix="pancollapse-count-cli-") as tmp:
        root = Path(tmp)
        removed = {
            "--compatibility-in": "bundle",
            "--compatibility-out": "bundle",
            "--count-bundle": "facts.json",
            "--annotation": "annotation.gff3",
            "--assignment-scoring": "legacy",
            "--gbz": "graph.gbz",
            "--xg": "graph.xg",
            "--gbwt": "paths.gbwt",
            "--r-index": "paths.ri",
            "--r-index-manifest": "binding.json",
            "--rad-out": None,
            "--read-assignments-out": "reads.parquet",
            "--count-memory-budget": "128GiB",
            "--memory-budget": "0B",
            "--cache-limit": "unknown=1B",
        }
        for option, value in removed.items():
            arguments = [option] + ([] if value is None else [value])
            output = root / option.removeprefix("--")
            result = subprocess.run(
                [program, "count", *arguments, "--gamp", str(root / "missing.gamp"),
                 "--out-dir", str(output)], text=True, capture_output=True, timeout=30)
            assert result.returncode != 0, option
            assert option in result.stderr, result.stderr
            assert "cannot open GAMP" not in result.stderr, result.stderr
            assert not output.exists(), output


if __name__ == "__main__":
    main()
