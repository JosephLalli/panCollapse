#!/usr/bin/env python3
"""Derive a count-facts bundle from a corrected annotation and a path-identity ledger.

The bundle is a cache, not a policy decision: panSC's own cache builder documents that it
"must agree with direct annotation parsing", and the correction chain writes every classifier
verdict into the annotation as a ``tag=`` so that "the counter needs no new code". This tool
performs that direct parsing, so a run needs only the annotation and the ledger.

Emits the four tables ``CountFactCatalog::load`` reads, plus a MANIFEST.json whose content_id
is computed the way the loader verifies it.
"""
from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import os
import sys
import urllib.parse
from pathlib import Path

# The categories a count profile may act on. Sourced from the selected_annotation_tags list in
# panSC's parent_category_receipt.json; tags outside it (CCDS, basic, MANE_Select, ...) are
# annotation metadata rather than count policy and are deliberately dropped.
SELECTED_TAGS = (
    "candidate_novel_locus",
    "identity_candidate_reassignment",
    "identity_unresolved",
    "independent_single_gene_support_strong_local_structure",
    "label_conflict",
    "nested_same_strand",
    "no_source_provenance",
    "overlaps_pseudogene",
    "readthrough_gene",
    "readthrough_transcript",
    "source_gene_label_conflict",
    "structural_readthrough",
)
STRONG_SUPPORT_TAG = "independent_single_gene_support_strong_local_structure"
MISSING = {"", ".", "N/A", "NA", "nan", "None"}

# The public 10x 2024-A gene_type allowlist, and the demotion vocabulary count_cr.py applies to
# a gene whose single biotype is outside it. Ported verbatim so a derived policy classifies a
# gene the way the cached policy does.
ALLOWED_TYPES_2024_A = frozenset({
    "protein_coding", "protein_coding_LoF", "lncRNA",
    "IG_C_gene", "IG_D_gene", "IG_J_gene", "IG_LV_gene", "IG_V_gene",
    "IG_V_pseudogene", "IG_J_pseudogene", "IG_C_pseudogene",
    "TR_C_gene", "TR_D_gene", "TR_J_gene", "TR_V_gene",
    "TR_V_pseudogene", "TR_J_pseudogene",
})
SMALL_STRUCTURAL_RNA_TYPES = frozenset({
    "miRNA", "misc_RNA", "rRNA", "ribozyme", "sRNA", "scaRNA", "scRNA",
    "snRNA", "snoRNA", "tRNA", "vault_RNA",
})
MITOCHONDRIAL_STRUCTURAL_RNA_TYPES = frozenset({"Mt_rRNA", "Mt_tRNA"})
# The transcript_type allowlist is a strict superset of the gene_type one: isoform-quality
# categories are legitimate for one alternative transcript of an otherwise countable gene.
ALLOWED_TRANSCRIPT_TYPES = ALLOWED_TYPES_2024_A | frozenset({
    "retained_intron", "protein_coding_CDS_not_defined", "nonsense_mediated_decay",
    "non_stop_decay", "TEC",
})
ENSEMBL_GENE = __import__("re").compile(r"^(ENSG\d+)(?:\.\d+)?(_\d+|__LOC\d+)?$")


def unqualified(value: str) -> str:
    return value.rpartition("#")[2] if "#" in value else value


def normalized_locus(source_gene: str, gene_locus: str, gene_id: str) -> str:
    """The count gene, matching CountFactCatalog's own derivation.

    The loader cross-checks this against the strong-support table and refuses a bundle that
    disagrees, so the version suffix must be stripped while any _N / __LOC discriminator is
    kept: that discriminator is what separates novel paralogs.
    """
    for candidate in (source_gene, gene_locus):
        if candidate and candidate not in MISSING:
            match = ENSEMBL_GENE.match(unqualified(candidate))
            if match:
                return match.group(1) + (match.group(2) or "")
            return candidate
    return "__panSC_runtime_gene__:" + gene_id


def fallback_reason_for_gene_type(gene_type: str):
    if gene_type in SMALL_STRUCTURAL_RNA_TYPES:
        return "small_rna"
    if gene_type in MITOCHONDRIAL_STRUCTURAL_RNA_TYPES:
        return "mt_structural"
    if gene_type == "TEC":
        return "TEC"
    if gene_type == "artifact":
        return "artifact"
    if "pseudogene" in gene_type.lower():
        return "pseudogene"
    return None


def classify_annotation_aggregate(gene_row_types, gene_types, transcript_types,
                                  qualifying_transcripts, source_less, readthrough, evidence):
    """Port of count_cr.py classify_annotation_aggregate.

    UNKNOWN is not a safe default here: gene competition keeps only PRIMARY candidates unless
    the profile's membership is all_annotated, so a universally UNKNOWN policy empties the
    matrix under cr7-v1's cellranger_2024_a membership.
    """
    types = gene_row_types or gene_types
    if source_less:
        return "UNKNOWN", "source_less"
    if not types:
        return "UNKNOWN", "missing_gene_type"
    all_allowlisted = types <= ALLOWED_TYPES_2024_A
    if len(types) > 1 and not all_allowlisted:
        return "UNKNOWN", "gene_type_membership_veto"
    if readthrough:
        return "FALLBACK", evidence
    if all_allowlisted:
        if transcript_types and not qualifying_transcripts:
            return "FALLBACK", "no_qualifying_transcript_type"
        return "PRIMARY", ("all_gene_type_members_allowlisted" if len(types) > 1
                           else "allowlisted")
    reason = fallback_reason_for_gene_type(next(iter(types)))
    if reason is not None:
        return "FALLBACK", reason
    return "UNKNOWN", "unrecognized_gene_type"


def open_text(path: str):
    return gzip.open(path, "rt") if str(path).endswith(".gz") else open(path)


def parse_attributes(column: str) -> dict[str, list[str]]:
    out: dict[str, list[str]] = {}
    for part in column.rstrip().split(";"):
        if "=" not in part:
            continue
        key, value = part.split("=", 1)
        out.setdefault(key.strip(), []).append(urllib.parse.unquote(value.strip()))
    return out


def tags_of(attrs: dict[str, list[str]]) -> set[str]:
    found: set[str] = set()
    for value in attrs.get("tag", []):
        found.update(token for token in value.split(",") if token)
    return found


def first(attrs: dict[str, list[str]], key: str) -> str:
    for value in attrs.get(key, []):
        if value not in MISSING:
            return value
    return ""


def strip_version(value: str) -> str:
    head, _, tail = value.rpartition(".")
    return head if head and tail.isdigit() else value


def scan_annotation(path: str):
    """Gene-row and transcript-row facts, keyed the way the identifier rule requires."""
    gene_tags: dict[str, set[str]] = {}
    gene_biotype: dict[str, str] = {}
    gene_nested_host: dict[str, str] = {}
    tx: dict[str, dict] = {}
    with open_text(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 9:
                continue
            feature = fields[2]
            if feature not in ("gene", "transcript", "mRNA"):
                continue
            attrs = parse_attributes(fields[8])
            # Only ID is a unique key; transcript_id and gene_id are bare and assembly-local.
            identifier = first(attrs, "ID")
            if not identifier:
                continue
            if feature == "gene":
                gene_tags[identifier] = tags_of(attrs)
                gene_biotype[identifier] = first(attrs, "gene_biotype") or first(attrs, "gene_type")
                host = first(attrs, "nested_host_gene")
                if host:
                    gene_nested_host[identifier] = host
                continue
            # On a transcript row Parent is the gene.
            tx[identifier] = {
                "gene": first(attrs, "Parent"),
                "source_gene": first(attrs, "source_gene"),
                "tags": tags_of(attrs),
                "biotype": first(attrs, "gene_biotype") or first(attrs, "gene_type"),
                "transcript_biotype": (first(attrs, "transcript_biotype")
                                       or first(attrs, "transcript_type")),
            }
    return gene_tags, gene_biotype, gene_nested_host, tx


def reference_gene_tags(path: str) -> dict[str, set[str]]:
    """Reference gene ID (version stripped) -> its tags.

    CAT drops reference gene tags when it projects a model, so a CAT transcript carries none of
    them. panSC restores them through source_gene, and overlaps_pseudogene reaches 234,932
    Parents that way on the joint chr20-22 surface.
    """
    tags: dict[str, set[str]] = {}
    with open_text(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) < 9 or fields[2] != "gene":
                continue
            attrs = parse_attributes(fields[8])
            identifier = first(attrs, "ID") or first(attrs, "gene_id")
            if identifier:
                tags[strip_version(identifier)] = tags_of(attrs)
    return tags


def ledger_parents(path: str) -> dict[str, str]:
    """unique_parent -> count gene, from the path-identity ledger."""
    parents: dict[str, str] = {}
    with open_text(path) as handle:
        header = handle.readline().rstrip("\n").split("\t")
        index = {name: position for position, name in enumerate(header)}
        for line in handle:
            row = line.rstrip("\n").split("\t")
            if len(row) < len(header):
                continue
            parent = row[index["unique_parent"]]
            parents.setdefault(parent, normalized_locus(row[index["source_gene"]],
                                                        row[index["gene_locus"]],
                                                        row[index["gene_id"]]))
    return parents


def write_tsv(path: Path, header: list[str], rows) -> None:
    with path.open("w", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(header)
        for row in rows:
            writer.writerow(row)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--annotation", required=True, help="corrected/selected GFF3")
    parser.add_argument("--path-identity-ledger", required=True)
    parser.add_argument("--reference", required=True,
                        help="reference GFF3 the CAT annotation was projected from; its gene "
                             "tags are inherited through source_gene because CAT drops them "
                             "on projection")
    parser.add_argument("--out-dir", required=True, help="new bundle directory")
    parser.add_argument("--profile", action="append", default=None,
                        help="profile id to emit gene policy for (repeatable)")
    args = parser.parse_args()

    profiles = args.profile or ["cr7-v1", "pansc-strict-v1"]
    out = Path(args.out_dir)
    if out.exists():
        print(f"refusing to overwrite {out}", file=sys.stderr)
        return 2
    (out / "files").mkdir(parents=True)

    print("scanning annotation ...", file=sys.stderr)
    gene_tags, gene_biotype, gene_nested_host, tx = scan_annotation(args.annotation)
    print(f"  genes={len(gene_tags):,} transcripts={len(tx):,}", file=sys.stderr)

    print("scanning reference annotation ...", file=sys.stderr)
    ref_tags = reference_gene_tags(args.reference)
    print(f"  reference genes={len(ref_tags):,}", file=sys.stderr)

    print("reading path-identity ledger ...", file=sys.stderr)
    parents = ledger_parents(args.path_identity_ledger)
    print(f"  unique_parents={len(parents):,}", file=sys.stderr)

    selected = set(SELECTED_TAGS)
    categories: dict[str, list[str]] = {}
    for parent in parents:
        row = tx.get(parent)
        if row is None:
            categories[parent] = []
            continue
        # A gene's tags apply to its transcripts: the chain writes gene-level verdicts on the
        # gene row, and the cache builder inherits them to every Parent resolving to that gene.
        merged = (row["tags"]
                  | gene_tags.get(row["gene"], set())
                  | ref_tags.get(strip_version(row["source_gene"]), set())) & selected
        categories[parent] = sorted(merged)

    write_tsv(out / "files/parent_category_ledger.tsv", ["unique_parent", "categories"],
              ((p, ",".join(c) if c else ".") for p, c in sorted(categories.items())))

    write_tsv(out / "files/strong_support_ledger.tsv",
              ["unique_parent", "count_gene", "raw_categories", "effective_categories",
               "strong_support_tag", "protected_primary_protein_host", "exempted_categories"],
              ((p, parents[p],
                ",".join(categories[p]) if categories[p] else ".",
                ",".join(categories[p]) if categories[p] else ".",
                "true" if STRONG_SUPPORT_TAG in categories[p] else "false",
                "false", ".")
               for p in sorted(categories)))

    # Gene policy. equivalence_gene equals count_gene in every row of the reference bundle
    # (0 of 40,243 differ), and nested_host is the named host the nested-host pass wrote.
    # competition_class is the one table that is a computation rather than a cached parse: it
    # aggregates every annotation row of a count gene before classifying it.
    print("aggregating gene policy ...", file=sys.stderr)
    agg: dict[str, dict] = {}
    for parent, gene in parents.items():
        row = tx.get(parent)
        bucket = agg.setdefault(gene, {"gene_row_types": set(), "gene_types": set(),
                                       "transcript_types": set(), "qualifying": set(),
                                       "source_less": True, "readthrough": False,
                                       "evidence": "", "host": ""})
        if row is None:
            continue
        local_gene = row["gene"]
        gene_row_type = gene_biotype.get(local_gene, "")
        if gene_row_type and gene_row_type not in MISSING:
            bucket["gene_row_types"].add(gene_row_type)
        if row["biotype"] and row["biotype"] not in MISSING:
            bucket["gene_types"].add(row["biotype"])
        if row["transcript_biotype"] and row["transcript_biotype"] not in MISSING:
            bucket["transcript_types"].add(row["transcript_biotype"])
            if row["transcript_biotype"] in ALLOWED_TRANSCRIPT_TYPES:
                bucket["qualifying"].add(row["transcript_biotype"])
        # source_less is an OR over the gene's rows: any row with real provenance clears it.
        if row["source_gene"] and row["source_gene"] not in MISSING:
            bucket["source_less"] = False
        tags = row["tags"] | gene_tags.get(local_gene, set()) | \
            ref_tags.get(strip_version(row["source_gene"]), set())
        if "readthrough_gene" in tags or "readthrough_transcript" in tags:
            bucket["readthrough"] = True
            bucket["evidence"] = ("readthrough_gene" if "readthrough_gene" in tags
                                  else "readthrough_transcript")
        if not bucket["host"]:
            bucket["host"] = gene_nested_host.get(local_gene, "")

    rows = []
    from collections import Counter
    reasons = Counter()
    for gene, b in sorted(agg.items()):
        klass, reason = classify_annotation_aggregate(
            b["gene_row_types"], b["gene_types"], b["transcript_types"],
            b["qualifying"], b["source_less"], b["readthrough"], b["evidence"])
        reasons[reason] += 1
        types = b["gene_row_types"] or b["gene_types"]
        gene_type = next(iter(sorted(types))) if types else "UNKNOWN"
        for profile in profiles:
            rows.append((profile, gene, gene_type, klass, reason, gene,
                         f"annotation:{Path(args.annotation).name}", b["host"] or ""))
    print(f"  genes={len(agg):,} competition reasons: " +
          ", ".join(f"{k}={v:,}" for k, v in reasons.most_common(6)), file=sys.stderr)
    write_tsv(out / "files/profile_gene_policy.tsv",
              ["profile_id", "count_gene", "gene_type", "competition_class",
               "competition_reason", "equivalence_gene", "source_annotation", "nested_host"],
              rows)

    ledger_copy = out / "files/path_identity_ledger.tsv"
    try:
        os.link(args.path_identity_ledger, ledger_copy)
    except OSError:
        import shutil
        shutil.copy2(args.path_identity_ledger, ledger_copy)

    roles = {
        "path_identity_ledger": ("panSC-path-identity-v1", "files/path_identity_ledger.tsv"),
        "profile_gene_policy": ("panSC-count-profile-gene-policy-v1", "files/profile_gene_policy.tsv"),
        "parent_category_ledger": ("panSC-parent-category-ledger-v3", "files/parent_category_ledger.tsv"),
        "strong_support_ledger": ("panSC-strong-support-audit-v1", "files/strong_support_ledger.tsv"),
    }
    files = {}
    for role, (schema, relative) in roles.items():
        target = out / relative
        digest = sha256_file(target)
        files[role] = {"path": relative, "schema": schema, "sha256": digest,
                       "source_sha256": digest, "size_bytes": target.stat().st_size,
                       "source_size_bytes": target.stat().st_size}
    content = {
        "assignment_contract": {
            "nested_host_interpretation": "relation-owner-loses",
            "profile_policy_source": "profile-specific-certified-ledgers",
            "score_window_order": "before-parent-category-filter",
            "transcript_filter_mode": "parent-category-ledger-v3",
        },
        "files": files,
        "profile_policy_sources": {profile: f"annotation:{Path(args.annotation).name}"
                                   for profile in profiles},
        "validation": {"derived_from_annotation": True},
    }
    encoded = json.dumps(content, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    manifest = {"content": content,
                "content_id": "sha256:" + hashlib.sha256(encoded.encode()).hexdigest(),
                "created_by": "derive_count_facts_from_annotation.py",
                "schema": "panSC-count-facts-v1"}
    with (out / "MANIFEST.json").open("w") as handle:
        json.dump(manifest, handle, sort_keys=True, separators=(",", ":"), ensure_ascii=False)
    print(f"  wrote {out}", file=sys.stderr)
    print(f"  content_id {manifest['content_id']}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
