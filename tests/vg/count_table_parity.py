#!/usr/bin/env python3
"""Compare a native numeric count output to its frozen-G reference."""

from __future__ import annotations

import filecmp
import json
import sys
from pathlib import Path

from count_test_support import TABLES, assert_count_outputs


def manifest(path: Path) -> dict:
    return json.loads((path / "manifest.json").read_text())


def profile_contract(value: dict) -> list[dict]:
    return sorted(({key: profile[key] for key in ("id", "effective_sha256", "logical_tables")}
                   for profile in value["profiles"]), key=lambda profile: profile["id"])


def require_equal(left: object, right: object, label: str) -> None:
    if left != right:
        raise AssertionError(f"{label} differs")


def counter_changes(left: dict, right: dict) -> dict[str, dict[str, object]]:
    changes: dict[str, dict[str, object]] = {}
    for scope in ("global", "profiles"):
        old, new = left["counters"][scope], right["counters"][scope]
        if scope == "global":
            for key in sorted(set(old) | set(new)):
                if old.get(key) != new.get(key):
                    changes[f"global.{key}"] = {"old": old.get(key), "new": new.get(key)}
        else:
            for profile in sorted(set(old) | set(new)):
                for key in sorted(set(old.get(profile, {})) | set(new.get(profile, {}))):
                    if old.get(profile, {}).get(key) != new.get(profile, {}).get(key):
                        changes[f"profile.{profile}.{key}"] = {
                            "old": old.get(profile, {}).get(key),
                            "new": new.get(profile, {}).get(key)}
    return changes


def check_bytes(left: Path, right: Path) -> None:
    for table in TABLES:
        if not filecmp.cmp(left / "parquet" / f"{table}.parquet",
                           right / "parquet" / f"{table}.parquet", shallow=False):
            raise AssertionError(f"Parquet bytes differ: {table}")
    mex = sorted(path.relative_to(left / "mex") for path in (left / "mex").rglob("*.gz"))
    if len(mex) != 6 or mex != sorted(path.relative_to(right / "mex")
                                      for path in (right / "mex").rglob("*.gz")):
        raise AssertionError("expected exactly the same six MEX gzip files")
    for relative in mex:
        if not filecmp.cmp(left / "mex" / relative, right / "mex" / relative,
                           shallow=False):
            raise AssertionError(f"MEX gzip bytes differ: {relative}")


def main() -> None:
    if len(sys.argv) != 4:
        raise ValueError("usage: count_table_parity.py LEFT_COUNTS RIGHT_COUNTS EXPECTED_GROUPS")
    left, right = Path(sys.argv[1]), Path(sys.argv[2])
    expected_groups = int(sys.argv[3])
    old, new = manifest(left), manifest(right)
    for value in (old, new):
        if value["counters"]["global"]["input_read_groups"] != expected_groups:
            raise AssertionError("manifest input_read_groups does not match EXPECTED_GROUPS")
    require_equal(old["inputs"]["gamp"]["path"], new["inputs"]["gamp"]["path"], "GAMP path")
    require_equal(old["inputs"]["gamp"]["size_bytes"], new["inputs"]["gamp"]["size_bytes"], "GAMP size")
    for key in ("fact_bundle_content_id", "analysis_scope", "assignment_algorithm",
                "assignment_score_window"):
        require_equal(old[key], new[key], key)
    for key in ("algorithm", "max_alignments_per_read", "enabled"):
        require_equal(old["alignment_complexity_filter"][key],
                      new["alignment_complexity_filter"][key], f"alignment filter {key}")
    require_equal(old["libraries"]["arrow"], new["libraries"]["arrow"], "Arrow version")
    require_equal(profile_contract(old), profile_contract(new), "profile contracts")
    for key in ("barcode_prior_groups", "exact_barcode_prior_groups"):
        require_equal(old["counters"]["global"][key], new["counters"]["global"][key], key)
    assert_count_outputs(left, right)
    check_bytes(left, right)
    print(json.dumps({"status": "pass", "expected_groups": expected_groups,
                      "arrow": old["libraries"]["arrow"],
                      "changed_counters": counter_changes(old, new)}, sort_keys=True))


if __name__ == "__main__":
    try:
        main()
    except Exception as error:
        print(json.dumps({"status": "fail", "error": str(error)}, sort_keys=True))
        sys.exit(1)
