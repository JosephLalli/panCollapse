#!/usr/bin/env python3
"""Build a Cell Ranger reference-style annotation and graph parent allowlist.

The public 10x reference recipe derives a gene allowlist from transcript rows,
then retains all rows belonging to a gene when at least one transcript has an
accepted gene type, an accepted transcript type, and no
``readthrough_transcript`` tag. CAT graph fixtures may contain exon-only
annotations; there, the first row for each transcript supplies the same fields.
An optional panSC path-identity ledger projects retained genes to the exact
exon/body ``unique_parent`` values seen by PanCollapse.
"""

from __future__ import annotations

import argparse
import csv
import gzip
import hashlib
import json
import os
import re
import tempfile
import urllib.parse
from collections import Counter
from pathlib import Path


SCHEMA = "panSC-cellranger-reference-filter-v1"
PARENT_SCHEMA = "panSC-strict-allowlisted-parents-v1"
PATH_IDENTITY_SCHEMA = "panSC-path-identity-v1"

# Public 10x 2024-A human reference-build list. Unlike 2020-A, 2024-A adds
# protein_coding_LoF and does not apply the older transcript tag PAR exclusion.
ALLOWED_TYPES_2024_A = frozenset(
    {
        "protein_coding",
        "protein_coding_LoF",
        "lncRNA",
        "IG_C_gene",
        "IG_D_gene",
        "IG_J_gene",
        "IG_LV_gene",
        "IG_V_gene",
        "IG_V_pseudogene",
        "IG_J_pseudogene",
        "IG_C_pseudogene",
        "TR_C_gene",
        "TR_D_gene",
        "TR_J_gene",
        "TR_V_gene",
        "TR_V_pseudogene",
        "TR_J_pseudogene",
    }
)
MISSING = frozenset({"", ".", "N/A", "NA", "nan", "None"})
ENSEMBL_GENE = re.compile(r"^(ENSG\d+)(?:\.\d+)?$")
PATH_COLUMNS = (
    "schema_version", "vg_path_name", "vg_path_length", "vg_haplotype_origins",
    "unique_parent", "source_parent", "input_parent", "canonical_transcript", "gene_id",
    "source_path_or_contig", "sample", "haplotype", "annotation_source", "feature_layer",
    "selection_status", "fallback_status", "exon_unique_parent", "gene_locus", "source_gene",
    "source_transcript", "transcript_class", "strand", "start", "end",
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("annotation", help="Input GTF/GFF3 annotation")
    parser.add_argument("--filtered-annotation", help="Write all rows of retained genes")
    parser.add_argument("--allowlisted-local-genes-out")
    parser.add_argument("--allowlisted-count-genes-out")
    parser.add_argument(
        "--count-reference-annotation",
        help=(
            "Trusted GTF/GFF3 used to suppress observed stable count genes with no "
            "qualifying non-readthrough transcript"
        ),
    )
    parser.add_argument("--path-identity-ledger")
    parser.add_argument("--allowlisted-parents-out")
    parser.add_argument("--summary-out", required=True)
    parser.add_argument("--reference-release", choices=("2024-A",), default="2024-A")
    args = parser.parse_args()
    if bool(args.path_identity_ledger) != bool(args.allowlisted_parents_out):
        parser.error(
            "--path-identity-ledger and --allowlisted-parents-out must be supplied together"
        )
    if not any(
        (
            args.filtered_annotation,
            args.allowlisted_local_genes_out,
            args.allowlisted_count_genes_out,
            args.allowlisted_parents_out,
        )
    ):
        parser.error("request at least one annotation or allowlist output")
    return args


def sha256(path: str | os.PathLike[str]) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(8 * 1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def open_annotation_text(path: str | os.PathLike[str]):
    """Open a plain-text or gzip-compressed annotation for streaming reads."""
    if str(path).endswith(".gz"):
        return gzip.open(path, "rt", encoding="utf-8")
    return open(path, encoding="utf-8")


def parse_attributes(text: str) -> dict[str, list[str]]:
    """Parse GFF3 key=value or GTF key "value" attributes without losing tags."""
    result: dict[str, list[str]] = {}
    for raw in text.strip().strip(";").split(";"):
        raw = raw.strip()
        if not raw:
            continue
        if "=" in raw:
            key, value = raw.split("=", 1)
            value = urllib.parse.unquote(value.strip())
        else:
            match = re.fullmatch(r'(\S+)\s+"(.*)"', raw)
            if match is None:
                raise ValueError(f"malformed GTF attribute {raw!r}")
            key, value = match.groups()
        result.setdefault(key.strip(), []).append(value)
    return result


def first(attrs: dict[str, list[str]], *keys: str) -> str | None:
    for key in keys:
        for value in attrs.get(key, []):
            if value not in MISSING:
                return value
    return None


def split_values(values: list[str]) -> set[str]:
    result: set[str] = set()
    for value in values:
        for component in value.split(","):
            component = component.strip()
            if component and component not in MISSING:
                result.add(component)
    return result


def canonical_count_gene(value: str | None, fallback: str) -> str:
    if value is None or value in MISSING:
        return fallback
    match = ENSEMBL_GENE.fullmatch(value)
    return match.group(1) if match else value


def transcript_ids(feature: str, attrs: dict[str, list[str]]) -> set[str]:
    if feature in {"transcript", "mRNA"}:
        value = first(attrs, "transcript_id", "ID")
        return {value} if value else set()
    if feature == "exon":
        # CAT emits a haplotype-qualified GFF3 Parent but may reuse the unqualified
        # transcript_id on several samples/haplotypes. The graph identity ledger keys the
        # qualified Parent. GTFs have no Parent and fall back to transcript_id.
        parents = split_values(attrs.get("Parent", []))
        return parents or split_values(attrs.get("transcript_id", []))
    return set()


def local_gene_for(feature: str, attrs: dict[str, list[str]]) -> str | None:
    gene = first(attrs, "gene_id")
    if gene:
        return gene
    if feature == "gene":
        return first(attrs, "ID")
    if feature in {"transcript", "mRNA"}:
        return first(attrs, "Parent")
    return None


def observation(feature: str, attrs: dict[str, list[str]]) -> dict[str, object] | None:
    gene = local_gene_for(feature, attrs)
    if gene is None:
        return None
    return {
        "local_gene": gene,
        "gene_type": first(attrs, "gene_type", "gene_biotype"),
        "transcript_type": first(attrs, "transcript_type", "transcript_biotype"),
        "tags": split_values(attrs.get("tag", [])),
        "count_gene": canonical_count_gene(first(attrs, "source_gene"), gene),
    }


def merge_observation(
    table: dict[str, dict[str, object]], transcript: str, candidate: dict[str, object]
) -> None:
    prior = table.get(transcript)
    if prior is None:
        table[transcript] = candidate
        return
    for key in ("local_gene", "gene_type", "transcript_type", "count_gene"):
        if prior[key] != candidate[key]:
            raise ValueError(
                f"transcript {transcript!r} has inconsistent {key}: "
                f"{prior[key]!r} versus {candidate[key]!r}"
            )
    prior["tags"].update(candidate["tags"])


def scan_transcripts(annotation: str) -> tuple[dict[str, dict[str, object]], str, int]:
    primary: dict[str, dict[str, object]] = {}
    exon_fallback: dict[str, dict[str, object]] = {}
    records = 0
    with open_annotation_text(annotation) as handle:
        for line_number, line in enumerate(handle, start=1):
            if not line.strip() or line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9:
                raise ValueError(
                    f"{annotation}:{line_number}: expected 9 columns, observed {len(fields)}"
                )
            records += 1
            feature = fields[2]
            if feature not in {"transcript", "mRNA", "exon"}:
                continue
            attrs = parse_attributes(fields[8])
            # The truebody fixture annotation appends synthetic transcript-body paths as
            # exon-shaped rows. They are retention geometry, not biological transcript
            # annotations, and inherit no biotype contract of their own. Eligibility comes from
            # the biological exon linked by exon_unique_parent in the identity ledger.
            if feature == "exon" and first(attrs, "feature_layer") in {
                "transcript_body", "body"
            }:
                continue
            candidate = observation(feature, attrs)
            if candidate is None:
                continue
            target = primary if feature in {"transcript", "mRNA"} else exon_fallback
            for transcript in transcript_ids(feature, attrs):
                merge_observation(target, transcript, dict(candidate, tags=set(candidate["tags"])))
    if primary:
        return primary, "transcript_or_mRNA", records
    if exon_fallback:
        return exon_fallback, "exon_fallback", records
    raise ValueError(f"{annotation!r} contains no usable transcript or exon evidence")


def qualifying_transcript_ids(
    transcripts: dict[str, dict[str, object]], allowed_types: frozenset[str]
) -> tuple[set[str], Counter[str]]:
    qualifying: set[str] = set()
    failure_reasons: Counter[str] = Counter()
    for transcript, row in transcripts.items():
        reasons = []
        if row["gene_type"] not in allowed_types:
            reasons.append("gene_type")
        if row["transcript_type"] not in allowed_types:
            reasons.append("transcript_type")
        if "readthrough_transcript" in row["tags"]:
            reasons.append("readthrough_transcript")
        if reasons:
            failure_reasons.update(reasons)
        else:
            qualifying.add(transcript)
    return qualifying, failure_reasons


def atomic_text(path: str, text: str) -> None:
    destination = Path(path)
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=destination.parent,
        prefix=destination.name + ".", delete=False
    ) as handle:
        handle.write(text)
        temporary = handle.name
    os.replace(temporary, destination)


def write_list(path: str | None, schema: str, values: set[str]) -> None:
    if path is not None:
        atomic_text(path, f"# {schema}\n" + "".join(f"{value}\n" for value in sorted(values)))


def filter_annotation(
    source: str,
    destination: str,
    allowed_genes: set[str],
    transcript_gene: dict[str, str],
) -> tuple[int, int, int]:
    output = Path(destination)
    output.parent.mkdir(parents=True, exist_ok=True)
    kept = dropped = unresolved = 0
    with tempfile.NamedTemporaryFile(
        "w", encoding="utf-8", dir=output.parent,
        prefix=output.name + ".", delete=False
    ) as target, open_annotation_text(source) as handle:
        temporary = target.name
        for line_number, line in enumerate(handle, start=1):
            if not line.strip() or line.startswith("#"):
                target.write(line)
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9:
                raise ValueError(f"{source}:{line_number}: expected 9 columns")
            attrs = parse_attributes(fields[8])
            gene = local_gene_for(fields[2], attrs)
            if gene is None:
                genes = {
                    transcript_gene[transcript]
                    for transcript in transcript_ids(fields[2], attrs)
                    if transcript in transcript_gene
                }
                if len(genes) == 1:
                    gene = next(iter(genes))
            if gene is None:
                unresolved += 1
                dropped += 1
            elif gene in allowed_genes:
                target.write(line)
                kept += 1
            else:
                dropped += 1
    os.replace(temporary, output)
    return kept, dropped, unresolved


def project_parent_allowlist(
    ledger: str,
    transcript_gene: dict[str, str],
    allowed_genes: set[str],
) -> tuple[set[str], dict[str, int]]:
    parents: set[str] = set()
    total = matched = retained = 0
    with open(ledger, newline="", encoding="utf-8") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        if tuple(reader.fieldnames or ()) != PATH_COLUMNS:
            raise ValueError(
                f"{ledger!r} does not have the exact {PATH_IDENTITY_SCHEMA} column contract"
            )
        for row_number, row in enumerate(reader, start=2):
            total += 1
            if row["schema_version"] != PATH_IDENTITY_SCHEMA:
                raise ValueError(
                    f"{ledger}:{row_number}: unexpected schema {row['schema_version']!r}"
                )
            aliases = {
                row["unique_parent"], row["input_parent"],
                row["exon_unique_parent"], row["source_parent"],
            }
            genes = {transcript_gene[alias] for alias in aliases if alias in transcript_gene}
            if not genes:
                continue
            matched += 1
            if len(genes) != 1:
                raise ValueError(
                    f"{ledger}:{row_number}: parent aliases map to multiple local genes {sorted(genes)!r}"
                )
            if next(iter(genes)) in allowed_genes:
                parents.add(row["unique_parent"])
                retained += 1
    if matched != total:
        raise ValueError(
            f"path-identity projection matched {matched}/{total} rows; "
            "the annotation and ledger are not the same fixture generation"
        )
    return parents, {"total_rows": total, "matched_rows": matched, "retained_rows": retained}


def main() -> None:
    args = parse_args()
    transcripts, evidence_source, annotation_records = scan_transcripts(args.annotation)
    allowed_types = ALLOWED_TYPES_2024_A

    qualifying_transcripts, failure_reasons = qualifying_transcript_ids(
        transcripts, allowed_types
    )

    allowed_local_genes = {
        str(transcripts[transcript]["local_gene"]) for transcript in qualifying_transcripts
    }
    retained_transcripts = {
        transcript for transcript, row in transcripts.items()
        if row["local_gene"] in allowed_local_genes
    }
    transcript_gene = {
        transcript: str(row["local_gene"]) for transcript, row in transcripts.items()
    }
    # Count identities follow only the observations that established eligibility.
    # Retaining every row of an eligible local gene remains the public 10x behavior,
    # but an ineligible readthrough isoform must not add its source gene to the
    # stable counting allowlist.
    allowed_count_genes = {
        str(transcripts[transcript]["count_gene"])
        for transcript in qualifying_transcripts
    }
    count_reference_metrics = None
    if args.count_reference_annotation:
        reference_transcripts, reference_evidence_source, reference_records = scan_transcripts(
            args.count_reference_annotation
        )
        qualifying_reference_transcripts, _ = qualifying_transcript_ids(
            reference_transcripts, allowed_types
        )
        observed_reference_count_genes = {
            str(row["count_gene"]) for row in reference_transcripts.values()
        }
        qualifying_reference_count_genes = {
            str(reference_transcripts[transcript]["count_gene"])
            for transcript in qualifying_reference_transcripts
        }
        allowed_count_genes -= (
            observed_reference_count_genes - qualifying_reference_count_genes
        )
        count_reference_metrics = {
            "path": os.path.abspath(args.count_reference_annotation),
            "sha256": sha256(args.count_reference_annotation),
            "records": reference_records,
            "transcript_evidence_source": reference_evidence_source,
            "transcripts_observed": len(reference_transcripts),
            "observed_count_genes": len(observed_reference_count_genes),
            "qualifying_count_genes": len(qualifying_reference_count_genes),
        }

    filtered_metrics = None
    if args.filtered_annotation:
        filtered_metrics = filter_annotation(
            args.annotation, args.filtered_annotation, allowed_local_genes, transcript_gene
        )
    write_list(
        args.allowlisted_local_genes_out,
        f"{SCHEMA} local_gene reference_release={args.reference_release}",
        allowed_local_genes,
    )
    write_list(
        args.allowlisted_count_genes_out,
        f"{SCHEMA} count_gene reference_release={args.reference_release}",
        allowed_count_genes,
    )

    parent_metrics = None
    if args.path_identity_ledger:
        parents, parent_metrics = project_parent_allowlist(
            args.path_identity_ledger, transcript_gene, allowed_local_genes
        )
        write_list(args.allowlisted_parents_out, PARENT_SCHEMA, parents)

    outputs = {}
    for name, path in (
        ("filtered_annotation", args.filtered_annotation),
        ("allowlisted_local_genes", args.allowlisted_local_genes_out),
        ("allowlisted_count_genes", args.allowlisted_count_genes_out),
        ("allowlisted_parents", args.allowlisted_parents_out),
    ):
        if path:
            outputs[name] = {"path": os.path.abspath(path), "sha256": sha256(path)}

    summary = {
        "schema_version": SCHEMA,
        "reference_release": args.reference_release,
        "filter_semantics": (
            "retain a gene and all of its rows iff at least one transcript has allowed "
            "gene_type and transcript_type and lacks tag=readthrough_transcript"
        ),
        "annotation": {
            "path": os.path.abspath(args.annotation), "sha256": sha256(args.annotation),
            "records": annotation_records, "transcript_evidence_source": evidence_source,
        },
        "allowed_types": sorted(allowed_types),
        "transcripts_observed": len(transcripts),
        "qualifying_transcripts": len(qualifying_transcripts),
        "retained_transcripts": len(retained_transcripts),
        "retained_local_genes": len(allowed_local_genes),
        "retained_count_genes": len(allowed_count_genes),
        "transcript_failure_reasons": dict(sorted(failure_reasons.items())),
        "filtered_annotation": (
            None if filtered_metrics is None else {
                "retained_records": filtered_metrics[0],
                "dropped_records": filtered_metrics[1],
                "unresolved_records": filtered_metrics[2],
            }
        ),
        "path_identity_projection": parent_metrics,
        "inputs": {
            "count_reference_annotation": count_reference_metrics,
            "path_identity_ledger": (
                None if not args.path_identity_ledger else {
                    "path": os.path.abspath(args.path_identity_ledger),
                    "sha256": sha256(args.path_identity_ledger),
                }
            )
        },
        "outputs": outputs,
    }
    atomic_text(args.summary_out, json.dumps(summary, indent=2, sort_keys=True) + "\n")


if __name__ == "__main__":
    main()
