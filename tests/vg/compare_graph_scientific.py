#!/usr/bin/env python3
"""Compare native-count scientific outputs under graph-input experiment rules."""

from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import shlex
import sys
from pathlib import Path
from typing import Any


OPERATIONAL_SUMMARY_METRICS = {
    "aggregate_spill_runs",
    "aggregate_spill_rows",
    "aggregate_spill_bytes",
    "assignment_cache_capacity",
    "assignment_cache_entries",
    "assignment_cache_hits",
    "assignment_cache_misses",
    "assignment_cache_uncached",
}
OPERATIONAL_MANIFEST_PATHS = [
    ("assignment_cache", "capacity"),
    ("assignment_cache", "entries"),
    ("assignment_cache", "hits"),
    ("assignment_cache", "misses"),
    ("assignment_cache", "uncached"),
    ("memory_budget_bytes",),
    ("count_storage",),
    ("threads",),
    ("timings", "initialization_seconds"),
    ("timings", "processing_seconds"),
    ("timings", "read_processing_seconds"),
    ("timings", "compatibility_finalize_seconds"),
    ("timings", "count_finalize_seconds"),
    ("timings", "diagnostics_merge_export_seconds"),
    ("outputs", "summary", "logical_sha256"),
    ("outputs", "summary", "output_sha256"),
    ("outputs", "summary", "size_bytes"),
]


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def files_under(root: Path) -> set[str]:
    return {str(path.relative_to(root)) for path in root.rglob("*") if path.is_file()}


def remove_path(value: dict[str, Any], parts: tuple[str, ...]) -> None:
    target: Any = value
    for key in parts[:-1]:
        if not isinstance(target, dict) or key not in target:
            return
        target = target[key]
    if isinstance(target, dict):
        target.pop(parts[-1], None)


def scientific_summary(path: Path) -> tuple[list[str], list[str]]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or lines[0] != "scope\tprofile_id\tmetric\tvalue":
        raise ValueError(f"unexpected summary schema: {path}")
    kept = [lines[0]]
    excluded: list[str] = []
    for line in lines[1:]:
        fields = line.split("\t")
        if len(fields) != 4:
            raise ValueError(f"malformed summary row: {path}: {line!r}")
        if fields[0] == "global" and fields[1] == "." and fields[2] in OPERATIONAL_SUMMARY_METRICS:
            excluded.append(fields[2])
        else:
            kept.append(line)
    return kept, excluded


def scientific_manifest(path: Path, cross_graph: bool) -> tuple[dict[str, Any], list[str]]:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"manifest is not an object: {path}")
    value.pop("command_line", None)
    removed = ["/command_line"]
    for parts in OPERATIONAL_MANIFEST_PATHS:
        remove_path(value, parts)
        removed.append("/" + "/".join(parts))
    global_counters = value.get("counters", {}).get("global", {})
    if isinstance(global_counters, dict):
        for metric in OPERATIONAL_SUMMARY_METRICS:
            global_counters.pop(metric, None)
            removed.append(f"/counters/global/{metric}")
    if cross_graph:
        inputs = value["inputs"]
        graph_roles = set(inputs) & {"xg", "gbz", "gbwt"}
        if graph_roles not in ({"xg"}, {"gbz"}, {"xg", "gbwt"}):
            raise ValueError(f"invalid graph provenance roles: {graph_roles}")
        for role in sorted(graph_roles):
            if set(inputs[role]) != {"path", "sha256", "size_bytes"}:
                raise ValueError(f"unknown graph provenance fields: {role}")
            for key in ("path", "sha256", "size_bytes"):
                removed.append(f"/inputs/{role}/{key}")
            del inputs[role]
    return value, removed


def normalized_command(command: str, cross_graph: bool) -> list[str]:
    tokens = shlex.split(command)
    result: list[str] = []
    skip_next = False
    allowed = {"--out-dir"}
    graph_flags = {"--xg", "--gbz", "--gbwt"}
    for index, token in enumerate(tokens):
        if skip_next:
            skip_next = False
            continue
        if cross_graph and token in graph_flags:
            skip_next = True
            continue
        result.append(token)
        if token in allowed:
            result.append("<operational-path>")
            skip_next = True
    return result


def gzip_header(path: Path) -> dict[str, Any]:
    raw = path.read_bytes()
    if len(raw) < 10 or raw[:2] != b"\x1f\x8b":
        raise ValueError(f"not gzip: {path}")
    flags = raw[3]
    position = 10
    filename = None
    if flags & 0x04:
        extra_len = int.from_bytes(raw[position:position + 2], "little")
        position += 2 + extra_len
    if flags & 0x08:
        end = raw.index(b"\0", position)
        filename = raw[position:end].decode("latin-1")
        position = end + 1
    if flags & 0x10:
        position = raw.index(b"\0", position) + 1
    if flags & 0x02:
        position += 2
    with gzip.open(path, "rb") as handle:
        decompressed_sha256 = hashlib.file_digest(handle, "sha256").hexdigest()
    return {
        "mtime": int.from_bytes(raw[4:8], "little"),
        "flags": flags,
        "filename": filename,
        "header_bytes": position,
        "compressed_sha256": sha256(path),
        "decompressed_sha256": decompressed_sha256,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("left", type=Path)
    parser.add_argument("right", type=Path)
    parser.add_argument("--receipt", type=Path, required=True)
    parser.add_argument("--cross-graph", action="store_true")
    parser.add_argument("--allow-executable-change", action="store_true",
                        help="Only for comparing the archived XG baseline to a new build")
    args = parser.parse_args()
    left, right = args.left.resolve(), args.right.resolve()
    receipt: dict[str, Any] = {
        "schema": "pancollapse-scientific-comparison-v1",
        "left": str(left),
        "right": str(right),
        "cross_graph": args.cross_graph,
        "operational_summary_metrics_excluded": sorted(OPERATIONAL_SUMMARY_METRICS),
        "manifest_exclusions": [],
        "inventory": {},
        "scientific_files": {},
        "summary": {},
        "manifest": {},
        "gzip": {},
        "pass": False,
    }
    failures: list[str] = []
    try:
        left_files, right_files = files_under(left), files_under(right)
        receipt["inventory"] = {
            "left": sorted(left_files), "right": sorted(right_files),
            "only_left": sorted(left_files - right_files),
            "only_right": sorted(right_files - left_files),
        }
        if left_files != right_files:
            failures.append("scientific output inventories differ")

        for relative in sorted((left_files & right_files) - {"summary.tsv", "manifest.json"}):
            lhs, rhs = left / relative, right / relative
            equal = lhs.read_bytes() == rhs.read_bytes()
            receipt["scientific_files"][relative] = {
                "equal": equal, "left_sha256": sha256(lhs), "right_sha256": sha256(rhs),
            }
            if not equal:
                failures.append(f"byte difference: {relative}")
            if relative.endswith(".gz"):
                left_header, right_header = gzip_header(lhs), gzip_header(rhs)
                receipt["gzip"][relative] = {"left": left_header, "right": right_header,
                                            "headers_equal": left_header == right_header}
                if left_header != right_header:
                    failures.append(f"gzip header difference: {relative}")

        left_summary, left_excluded = scientific_summary(left / "summary.tsv")
        right_summary, right_excluded = scientific_summary(right / "summary.tsv")
        receipt["summary"] = {"scientific_equal": left_summary == right_summary,
                              "left_excluded": left_excluded, "right_excluded": right_excluded,
                              "left_scientific_sha256": hashlib.sha256(("\n".join(left_summary) + "\n").encode()).hexdigest(),
                              "right_scientific_sha256": hashlib.sha256(("\n".join(right_summary) + "\n").encode()).hexdigest()}
        if left_summary != right_summary:
            failures.append("scientific summary difference")

        left_raw = json.loads((left / "manifest.json").read_text(encoding="utf-8"))
        right_raw = json.loads((right / "manifest.json").read_text(encoding="utf-8"))
        left_command = normalized_command(left_raw["command_line"], args.cross_graph)
        right_command = normalized_command(right_raw["command_line"], args.cross_graph)
        if args.allow_executable_change:
            receipt["executable_comparison"] = {
                "left": left_command[0], "right": right_command[0],
                "left_sha256": sha256(Path(left_command[0])),
                "right_sha256": sha256(Path(right_command[0]))}
            left_command[0] = right_command[0] = "<recorded-executable>"
        receipt["manifest"]["command_line_equal_after_allowed_paths"] = left_command == right_command
        if not receipt["manifest"]["command_line_equal_after_allowed_paths"]:
            failures.append("manifest command lines differ outside allowed graph/output paths")
        left_manifest, excluded = scientific_manifest(left / "manifest.json", args.cross_graph)
        right_manifest, _ = scientific_manifest(right / "manifest.json", args.cross_graph)
        receipt["manifest"].update({"scientific_equal": left_manifest == right_manifest,
                                    "scientific_sha256_left": hashlib.sha256(json.dumps(left_manifest, sort_keys=True, separators=(",", ":")).encode()).hexdigest(),
                                    "scientific_sha256_right": hashlib.sha256(json.dumps(right_manifest, sort_keys=True, separators=(",", ":")).encode()).hexdigest()})
        receipt["manifest_exclusions"] = excluded
        if left_manifest != right_manifest:
            failures.append("scientific manifest difference")
    except Exception as exc:
        failures.append(f"comparator error: {type(exc).__name__}: {exc}")
    receipt["failures"] = failures
    receipt["pass"] = not failures
    args.receipt.parent.mkdir(parents=True, exist_ok=True)
    args.receipt.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps({"pass": receipt["pass"], "receipt": str(args.receipt), "failures": failures}, sort_keys=True))
    return 0 if receipt["pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
