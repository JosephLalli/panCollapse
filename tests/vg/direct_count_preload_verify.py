#!/usr/bin/env python3
"""Preload must validate production state without opening reads or publishing counts."""

from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile


program, fixture_directory, source_directory, bundle = sys.argv[1:]
fixture = Path(fixture_directory)
source = Path(source_directory)


with tempfile.TemporaryDirectory(prefix="preload-", dir=fixture) as temporary:
    root = Path(temporary)
    unread = root / "unopened.gamp"
    os.mkfifo(unread)
    output = root / "counts"
    arguments = [
        program, "count", "--preload-only", "--gamp", str(unread),
        "--xg", str(fixture / "graph.xg"), "--count-bundle", bundle,
        "--barcode-whitelist", str(source / "count_whitelist.txt"),
        "--cr7", "--pansc-strict-v1", "--count-memory-budget", "1MiB",
        "--out-dir", str(output),
    ]
    completed = subprocess.run(arguments, capture_output=True, text=True, timeout=30)
    if completed.returncode:
        raise AssertionError(completed.stderr)
    receipt = json.loads(completed.stdout)
    assert receipt["schema"] == "pancollapse-count-preload-v1", receipt
    assert receipt["status"] == "preload_complete", receipt
    assert receipt["input_read_groups"] == 0, receipt
    assert receipt["path_rows"] > 0 and receipt["parents"] > 0, receipt
    assert receipt["exact_models"] > 0 and receipt["whitelist_barcodes"] > 0, receipt
    assert receipt["peak_rss_bytes"] > 0, receipt
    assert receipt["worker_state_measured"] is False, receipt
    assert not output.exists(), "preload published a count directory"
    assert not list(root.glob(".counts.staging-*")), "preload leaked staging files"

    # An absent read path also succeeds: the flag does not even test/open GAMP.
    missing = list(arguments)
    missing[missing.index("--gamp") + 1] = str(root / "missing.gamp")
    completed = subprocess.run(missing, capture_output=True, text=True, timeout=30)
    assert completed.returncode == 0, completed.stderr
    assert not output.exists() and not list(root.glob(".counts.staging-*"))

    for option, value, expected in (
        ("--barcode-whitelist", str(root / "missing-whitelist"), "whitelist"),
        ("--count-bundle", str(root / "missing-bundle"), "missing-bundle"),
    ):
        invalid = list(arguments)
        invalid[invalid.index(option) + 1] = value
        completed = subprocess.run(invalid, capture_output=True, text=True, timeout=30)
        assert completed.returncode != 0, f"accepted invalid {option}"
        assert expected in completed.stderr.lower(), completed.stderr
        assert not output.exists() and not list(root.glob(".counts.staging-*"))

    invalid = list(arguments)
    invalid.extend(["--compatibility-in", str(root / "missing-compatibility")])
    completed = subprocess.run(invalid, capture_output=True, text=True, timeout=30)
    assert completed.returncode != 0 and "--preload-only" in completed.stderr

print("direct count preload verified: unopened GAMP, production validation, no publication")
