#!/usr/bin/env python3
"""Verify an allowed sensitivity override is never mislabeled as frozen CR7."""

from __future__ import annotations

import json
import sys
from pathlib import Path


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit("usage: direct_count_override_verify.py OUTPUT")
    manifest = json.loads((Path(sys.argv[1]) / "manifest.json").read_text())
    assert manifest["analysis_scope"] == "sensitivity-analysis"
    assert len(manifest["profiles"]) == 1
    profile = manifest["profiles"][0]
    assert profile["id"].startswith("derived-cr7-v1-")
    assert profile["id"] != "cr7-v1"
    assert len(profile["effective_sha256"]) == 64
    assert profile["id"].endswith(profile["effective_sha256"][:12])
    assert profile["overrides"] == [{"field": "score-window", "value": "4"}]
    assert manifest["libraries"] == {
        "arrow": "17.0.0",
        "parquet": "17.0.0",
        "zstd": "1.5.7",
    }


if __name__ == "__main__":
    main()
