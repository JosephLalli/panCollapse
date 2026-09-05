#!/usr/bin/env python3
"""Project an existing CAT Parent report onto a vg path identity ledger.

This report-only utility deliberately does not re-run CAT classification.  It reuses the
detailed classifier's reference parsing and graph-path projection contract, then writes a
new, terminal policy artifact set atomically.
"""

from __future__ import annotations

import argparse
import csv
import datetime as dt
import json
import os
import shutil
import sys
import tempfile
from pathlib import Path
from typing import Sequence

from assign_cat_denovo_transcripts_detailed import (
    build_reference_index,
    load_eligible_count_genes,
    open_text_auto,
    project_parent_roles_to_graph_paths,
    sha256_file,
    write_sha256s,
)


SCHEMA_VERSION = "panSC-cat-parent-count-policy-projection-v1"
OUTPUT_NAMES = (
    "path_count_policy.tsv.gz",
    "projection_summary.tsv",
    "RUN_MANIFEST.json",
    "SHA256SUMS",
    "STATUS",
)
LEDGER_REQUIRED = frozenset(
    (
        "vg_path_name",
        "source_path_or_contig",
        "input_parent",
        "source_parent",
        "canonical_transcript",
        "source_transcript",
        "gene_id",
        "transcript_class",
        "feature_layer",
    )
)
DETAILED_REQUIRED = frozenset(
    (
        "contig",
        "parent",
        "structural_class",
        "current_category",
        "count_role",
        "count_gene",
        "category_reason",
    )
)


def require_nonempty_file(path: str, label: str) -> Path:
    resolved = Path(path).resolve()
    if not resolved.is_file() or resolved.stat().st_size == 0:
        raise ValueError(f"{label} must be a non-empty regular file: {resolved}")
    return resolved


def require_tabular_rows(path: Path, label: str, required: frozenset[str]) -> int:
    with open_text_auto(str(path)) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        missing = required - set(reader.fieldnames or ())
        if missing:
            raise ValueError(
                f"{label} missing columns: {', '.join(sorted(missing))}"
            )
        rows = sum(1 for _ in reader)
    if rows == 0:
        raise ValueError(f"{label} contains no data rows: {path}")
    return rows


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference-gff", required=True)
    parser.add_argument("--detailed-assignment", required=True)
    parser.add_argument("--path-identity-ledger", required=True)
    parser.add_argument(
        "--eligible-count-genes",
        help="optional one-column stable-gene count allowlist",
    )
    parser.add_argument("--output-dir", required=True)
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    reference_path = require_nonempty_file(args.reference_gff, "--reference-gff")
    detailed_path = require_nonempty_file(
        args.detailed_assignment, "--detailed-assignment"
    )
    ledger_path = require_nonempty_file(
        args.path_identity_ledger, "--path-identity-ledger"
    )
    detailed_rows = require_tabular_rows(
        detailed_path, "detailed assignment", DETAILED_REQUIRED
    )
    ledger_rows = require_tabular_rows(ledger_path, "path identity ledger", LEDGER_REQUIRED)
    reference = build_reference_index(str(reference_path), 1)
    if reference.transcript_count == 0:
        raise ValueError(f"reference GFF contains no usable exon transcripts: {reference_path}")

    eligible_count_genes = (
        load_eligible_count_genes(args.eligible_count_genes)
        if args.eligible_count_genes
        else None
    )
    out_dir = Path(args.output_dir).resolve()
    if out_dir.exists():
        raise FileExistsError(f"refusing to overwrite output directory: {out_dir}")
    if not out_dir.parent.is_dir():
        raise FileNotFoundError(f"output parent does not exist: {out_dir.parent}")
    staging = Path(tempfile.mkdtemp(prefix=f".{out_dir.name}.partial.", dir=out_dir.parent))
    try:
        policy_path = staging / "path_count_policy.tsv.gz"
        results = project_parent_roles_to_graph_paths(
            str(ledger_path), detailed_path, policy_path, reference, eligible_count_genes
        )
        if results["ledger_rows"] != ledger_rows:
            raise RuntimeError(
                f"ledger row count changed during projection: expected {ledger_rows}, "
                f"observed {results['ledger_rows']}"
            )
        with open(staging / "projection_summary.tsv", "w", newline="") as handle:
            writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
            writer.writerow(("metric", "value"))
            for key in ("ledger_rows", "unique_paths", "referenced_parent_keys", "matched_parent_keys"):
                writer.writerow((key, results[key]))
            for group in ("parent_match_statuses", "count_roles", "categories"):
                for value, count in results[group].items():
                    writer.writerow((f"{group}:{value}", count))
        manifest = {
            "schema_version": SCHEMA_VERSION,
            "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
            "inputs": {
                label: {
                    "path": str(path),
                    "bytes": path.stat().st_size,
                    "file_sha256": sha256_file(str(path)),
                }
                for label, path in (
                    ("reference_gff", reference_path),
                    ("detailed_assignment", detailed_path),
                    ("path_identity_ledger", ledger_path),
                )
            },
            "reuse": {
                "projector": "assign_cat_denovo_transcripts_detailed.project_parent_roles_to_graph_paths",
                "reference_index": "assign_cat_denovo_transcripts_detailed.build_reference_index",
                "classification": "not_rerun",
            },
            "reference": {"transcripts": reference.transcript_count},
            "eligible_count_genes": (
                {
                    "path": str(Path(args.eligible_count_genes).resolve()),
                    "bytes": Path(args.eligible_count_genes).stat().st_size,
                    "file_sha256": sha256_file(args.eligible_count_genes),
                    "genes": len(eligible_count_genes),
                }
                if args.eligible_count_genes
                else None
            ),
            "validated_input_rows": {
                "detailed_assignment": detailed_rows,
                "path_identity_ledger": ledger_rows,
            },
            "results": results,
            "outputs": {"path_count_policy.tsv.gz": sha256_file(str(policy_path))},
        }
        (staging / "RUN_MANIFEST.json").write_text(
            json.dumps(manifest, indent=2, sort_keys=True) + "\n"
        )
        write_sha256s(
            staging,
            ("path_count_policy.tsv.gz", "projection_summary.tsv", "RUN_MANIFEST.json"),
        )
        (staging / "STATUS").write_text(
            f"cat_parent_count_policy_projection_complete\tpaths={results['unique_paths']}"
            f"\tdetailed_rows={detailed_rows}\n"
        )
        os.rename(staging, out_dir)
    except BaseException:
        shutil.rmtree(staging, ignore_errors=True)
        raise
    print(f"complete: {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
