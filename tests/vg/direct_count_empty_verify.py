#!/usr/bin/env python3
"""Verify that a zero-record direct count still publishes valid empty tables."""

from __future__ import annotations

import json
import sys
from pathlib import Path

import pyarrow.parquet as pq


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: direct_count_empty_verify.py OUTPUT")
    root = Path(sys.argv[1])
    manifest = json.loads((root / "manifest.json").read_text())
    assert manifest["counters"]["global"]["input_records"] == 0
    assert manifest["counters"]["global"]["input_read_groups"] == 0
    assert pq.read_table(root / "parquet/barcodes.parquet").num_rows == 0
    # Dictionaries are the lexical union of nonempty output identities, so an
    # empty observation set publishes empty barcode and feature dictionaries.
    assert pq.read_table(root / "parquet/features.parquet").num_rows == 0
    assert pq.read_table(root / "parquet/counts.parquet").num_rows == 0
    assert pq.read_table(root / "parquet/molecules.parquet").num_rows == 0


if __name__ == "__main__":
    main()
