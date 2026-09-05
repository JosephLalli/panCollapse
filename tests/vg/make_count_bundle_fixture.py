#!/usr/bin/env python3
"""Build the tiny authenticated count-fact bundle used by the CLI smoke test."""

from __future__ import annotations

import hashlib
import json
import shutil
import sys
from pathlib import Path


def canonical(value: object) -> bytes:
    return json.dumps(value, sort_keys=True, separators=(",", ":")).encode()


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            value.update(block)
    return value.hexdigest()


def main() -> None:
    if len(sys.argv) not in (3, 4):
        raise SystemExit(
            "usage: make_count_bundle_fixture.py SOURCE_DIR OUTPUT_DIR [tagged]"
        )
    source = Path(sys.argv[1])
    output = Path(sys.argv[2])
    variant = sys.argv[3] if len(sys.argv) == 4 else None
    if variant not in (None, "tagged"):
        raise SystemExit(f"unsupported fixture variant: {variant}")
    if output.exists():
        raise SystemExit(f"refusing existing output directory: {output}")

    role_inputs = {
        "corrected_annotation": (source / "count_corrected_annotation.gff3", "annotation"),
        "gene_metadata": (source / "count_gene_metadata.tsv", "panSC-gene-metadata-v1"),
        "profile_gene_policy": (
            source / "count_profile_gene_policy.tsv",
            "panSC-count-profile-gene-policy-v1",
        ),
        "parent_category_ledger": (
            source
            / (
                "count_parent_categories_tagged.tsv"
                if variant == "tagged"
                else "count_parent_categories.tsv"
            ),
            "panSC-parent-category-ledger-v3",
        ),
        "parent_category_receipt": (
            source / "count_parent_categories_receipt.json",
            "panSC-parent-category-ledger-v3",
        ),
        "path_identity_ledger": (
            source / "path_identity_ledger.tsv",
            "panSC-path-identity-v1",
        ),
        "projected_nested_policy": (
            source / "count_projected_nested.json",
            "panSC-projected-nested-host-policy-v1",
        ),
        "strong_support_ledger": (
            source
            / (
                "count_strong_support_tagged.tsv"
                if variant == "tagged"
                else "count_strong_support.tsv"
            ),
            "panSC-strong-support-audit-v1",
        ),
        "profile_policy_receipt_cr7_v1": (
            source / "count_policy_receipt_cr7_v1.json",
            "panSC-count-cr-gene-policy-only-v2",
        ),
        "profile_policy_receipt_pansc_strict_v1": (
            source / "count_policy_receipt_pansc_strict_v1.json",
            "panSC-projected-nested-host-policy-v1",
        ),
    }
    output.mkdir(parents=True)
    files: dict[str, dict[str, object]] = {}
    for role, (input_path, schema) in sorted(role_inputs.items()):
        suffix = input_path.suffix
        relative = Path("files") / f"{role}{suffix}"
        target = output / relative
        target.parent.mkdir(exist_ok=True)
        shutil.copyfile(input_path, target)
        files[role] = {
            "path": relative.as_posix(),
            "schema": schema,
            "sha256": digest(target),
            "size_bytes": target.stat().st_size,
            "source_sha256": digest(input_path),
            "source_size_bytes": input_path.stat().st_size,
        }

    content = {
        "assignment_contract": {
            "nested_host_interpretation": "relation-owner-loses",
            "profile_policy_source": "profile-specific-certified-ledgers",
            "score_window_order": "before-parent-category-filter",
            "transcript_filter_mode": "parent-category-ledger-v3",
        },
        "files": files,
        "validation": {
            "gene_metadata_coverage": "exact",
            "gene_policy_genes": 1,
            "parent_category_coverage": "exact",
            "path_identity_parents": 4,
            "strong_support_coverage": "exact",
        },
    }
    manifest = {
        "content": content,
        "content_id": "sha256:" + hashlib.sha256(canonical(content)).hexdigest(),
        "created_by": {"program": "make_count_bundle_fixture.py", "version": "1"},
        "schema": "panSC-count-facts-v1",
    }
    (output / "MANIFEST.json").write_bytes(canonical(manifest) + b"\n")


if __name__ == "__main__":
    main()
