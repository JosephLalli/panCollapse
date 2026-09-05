#!/usr/bin/env python3
"""Build a producer-side Parent allowlist from a graph-path reclassification.

The filtering order is intentional: a Parent must first be classified as countable, and its
reclassified count_gene must then occur in the frozen Cell Ranger-compatible gene set. The emitted
Parent file is consumed by PanCollapse before exact-Ex50 score-window and tier selection.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
from pathlib import Path


PATH_POLICY_SCHEMA = "panSC-graph-path-count-policy-v2"
PATH_POLICY_COLUMNS = (
    "schema_version", "vg_path_name", "source_path_or_contig", "input_parent",
    "source_parent", "canonical_transcript", "source_transcript", "transcript_class",
    "feature_layer", "ledger_gene", "parent_match_status", "matched_parent",
    "policy_basis", "source_reference_gene", "source_reference_tags",
    "source_reference_relation", "parent_structural_class", "parent_current_category",
    "count_role", "count_gene", "policy_reason",
)
COUNTABLE_ROLES = frozenset({"countable", "reassigned_countable"})
DERIVATION = "path_countable_and_reclassified_count_gene_in_allowed_count_genes"


def open_text(path: Path):
    import gzip

    return gzip.open(path, "rt", newline="") if path.suffix == ".gz" else path.open(
        newline=""
    )


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def load_one_column(path: Path, label: str) -> set[str]:
    values: set[str] = set()
    with path.open(encoding="utf-8") as handle:
        for line_number, line in enumerate(handle, 1):
            value = line.strip()
            if not value or value.startswith("#"):
                continue
            if any(character.isspace() for character in value):
                raise ValueError(f"{label} line {line_number} is not one identity")
            if value in values:
                raise ValueError(f"{label} repeats {value!r} at line {line_number}")
            values.add(value)
    if not values:
        raise ValueError(f"{label} is empty")
    return values


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("path_policy", type=Path)
    parser.add_argument("path_identity_ledger", type=Path)
    parser.add_argument("allowed_count_genes", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--summary-out", required=True, type=Path)
    parser.add_argument("--prior-parent-allowlist", type=Path)
    return parser.parse_args()


def main() -> None:
    args = parse_args()
    for path in (args.path_policy, args.path_identity_ledger, args.allowed_count_genes):
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"missing or empty input: {path}")
    if args.prior_parent_allowlist and (
        not args.prior_parent_allowlist.is_file()
        or args.prior_parent_allowlist.stat().st_size == 0
    ):
        raise ValueError(f"missing or empty prior Parent allowlist: {args.prior_parent_allowlist}")
    if args.output.exists() or args.summary_out.exists():
        raise ValueError("output and summary paths must not already exist")

    allowed_genes = load_one_column(args.allowed_count_genes, "allowed count-gene file")
    prior_parents = (
        load_one_column(args.prior_parent_allowlist, "prior Parent allowlist")
        if args.prior_parent_allowlist
        else set()
    )

    selected: set[str] = set()
    countable: set[str] = set()
    prior_countable: set[str] = set()
    seen_paths: set[str] = set()
    seen_parents: dict[str, tuple[str, str]] = {}
    role_counts: dict[str, int] = {}
    rows = 0
    with open_text(args.path_policy) as policy_handle, args.path_identity_ledger.open(
        newline=""
    ) as ledger_handle:
        policy_reader = csv.DictReader(policy_handle, delimiter="\t")
        ledger_reader = csv.DictReader(ledger_handle, delimiter="\t")
        if tuple(policy_reader.fieldnames or ()) != PATH_POLICY_COLUMNS:
            raise ValueError("path-policy header does not match the v2 schema")
        ledger_required = {"schema_version", "vg_path_name", "unique_parent"}
        if not ledger_required <= set(ledger_reader.fieldnames or ()):
            raise ValueError("path-identity ledger lacks required path/Parent columns")
        while True:
            policy = next(policy_reader, None)
            ledger = next(ledger_reader, None)
            if policy is None or ledger is None:
                if policy is not None or ledger is not None:
                    raise ValueError("path policy and identity ledger have different row counts")
                break
            rows += 1
            if policy["schema_version"] != PATH_POLICY_SCHEMA:
                raise ValueError(f"path-policy row {rows + 1} has the wrong schema version")
            path_name = policy["vg_path_name"]
            if path_name != ledger["vg_path_name"]:
                raise ValueError(f"path-policy/ledger order mismatch at data row {rows}")
            if path_name in seen_paths:
                raise ValueError(f"path policy repeats graph path {path_name!r}")
            seen_paths.add(path_name)
            parent = ledger["unique_parent"]
            role = policy["count_role"]
            count_gene = policy["count_gene"]
            role_counts[role] = role_counts.get(role, 0) + 1
            if role in COUNTABLE_ROLES:
                if count_gene in {"", ".", "N/A"}:
                    raise ValueError(f"countable path {path_name!r} lacks count_gene")
            elif role == "alignment_only":
                if count_gene != ".":
                    raise ValueError(f"alignment-only path {path_name!r} has count_gene")
            else:
                raise ValueError(f"path {path_name!r} has unsupported count_role {role!r}")
            decision = (role, count_gene)
            previous = seen_parents.setdefault(parent, decision)
            if previous != decision:
                raise ValueError(f"Parent {parent!r} has conflicting path-policy decisions")
            if role in COUNTABLE_ROLES:
                countable.add(parent)
                if parent in prior_parents:
                    prior_countable.add(parent)
                if count_gene in allowed_genes:
                    selected.add(parent)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    path_policy_sha = sha256(args.path_policy)
    ledger_sha = sha256(args.path_identity_ledger)
    allowed_genes_sha = sha256(args.allowed_count_genes)
    with args.output.open("w", encoding="utf-8") as handle:
        handle.write("# panSC-strict-allowlisted-parents-v1\n")
        handle.write(f"# derivation={DERIVATION}\n")
        handle.write(f"# path_count_policy_sha256={path_policy_sha}\n")
        handle.write(f"# path_identity_ledger_sha256={ledger_sha}\n")
        handle.write(f"# allowed_count_genes_sha256={allowed_genes_sha}\n")
        if args.prior_parent_allowlist:
            handle.write(
                f"# prior_parent_allowlist_sha256={sha256(args.prior_parent_allowlist)}\n"
            )
        handle.writelines(f"{parent}\n" for parent in sorted(selected))

    summary = {
        "schema_version": "panSC-reclassified-parent-allowlist-summary-v1",
        "derivation": DERIVATION,
        "inputs": {
            "path_count_policy": {"path": str(args.path_policy), "sha256": path_policy_sha},
            "path_identity_ledger": {
                "path": str(args.path_identity_ledger),
                "sha256": ledger_sha,
            },
            "allowed_count_genes": {
                "path": str(args.allowed_count_genes),
                "sha256": allowed_genes_sha,
                "genes": len(allowed_genes),
            },
        },
        "counts": {
            "policy_rows": rows,
            "unique_paths": len(seen_paths),
            "unique_parents": len(seen_parents),
            "role_rows": dict(sorted(role_counts.items())),
            "countable_parents": len(countable),
            "selected_parents": len(selected),
            "countable_excluded_by_gene_policy": len(countable - selected),
        },
        "output": {"path": str(args.output), "sha256": sha256(args.output)},
    }
    if args.prior_parent_allowlist:
        summary["inputs"]["prior_parent_allowlist"] = {
            "path": str(args.prior_parent_allowlist),
            "sha256": sha256(args.prior_parent_allowlist),
            "parents": len(prior_parents),
        }
        summary["prior_comparison"] = {
            "prior_countable_parents": len(prior_countable),
            "selected_also_prior": len(selected & prior_countable),
            "selected_newly_eligible": len(selected - prior_countable),
            "prior_countable_not_selected": len(prior_countable - selected),
        }
    args.summary_out.write_text(json.dumps(summary, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
