#!/usr/bin/env python3
"""Build panSC-path-identity-v1 for a reference GTF projected by ``vg rna``.

This adapter is intentionally limited to the pipeline's locally built reference/toy
indexes. Production CAT/pangenome annotations must supply their audited path-identity
ledger; identities must never be reconstructed from path-name suffixes.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import re
from dataclasses import dataclass
from pathlib import Path


SCHEMA = "panSC-path-identity-v1"
FIELDS = [
    "schema_version",
    "vg_path_name",
    "vg_path_length",
    "vg_haplotype_origins",
    "unique_parent",
    "source_parent",
    "input_parent",
    "canonical_transcript",
    "gene_id",
    "source_path_or_contig",
    "sample",
    "haplotype",
    "annotation_source",
    "feature_layer",
    "selection_status",
    "fallback_status",
    "exon_unique_parent",
    "gene_locus",
    "source_gene",
    "source_transcript",
    "transcript_class",
    "strand",
    "start",
    "end",
]


def get_attr(text: str, key: str) -> str | None:
    match = re.search(rf'(?:^|[;\s]){re.escape(key)}[ =]"?([^";]+)"?', text)
    return match.group(1).strip() if match else None


@dataclass
class Transcript:
    seqid: str
    source: str
    start: int
    end: int
    strand: str
    gene: str
    source_gene: str
    source_transcript: str
    transcript_class: str
    has_transcript_row: bool


def read_gtf(path: Path) -> dict[str, Transcript]:
    transcripts: dict[str, Transcript] = {}
    with path.open() as handle:
        for line_number, line in enumerate(handle, 1):
            if not line.strip() or line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9 or fields[2] not in {"transcript", "exon"}:
                continue
            attrs = fields[8]
            transcript = (
                get_attr(attrs, "transcript_id")
                or (get_attr(attrs, "ID") if fields[2] == "transcript" else None)
                or get_attr(attrs, "Parent")
            )
            if not transcript:
                raise ValueError(f"{path}:{line_number}: missing transcript identity")
            gene = get_attr(attrs, "gene_id") or transcript
            source_gene = get_attr(attrs, "source_gene") or gene
            source_transcript = get_attr(attrs, "source_transcript") or transcript
            transcript_class = get_attr(attrs, "transcript_class") or "reference"
            start, end = int(fields[3]), int(fields[4])
            current = Transcript(
                fields[0],
                fields[1] or "reference",
                start,
                end,
                fields[6],
                gene,
                source_gene,
                source_transcript,
                transcript_class,
                fields[2] == "transcript",
            )
            prior = transcripts.get(transcript)
            if prior is None or current.has_transcript_row:
                transcripts[transcript] = current
                continue
            if prior.has_transcript_row:
                continue
            identity = (
                prior.seqid,
                prior.source,
                prior.strand,
                prior.gene,
                prior.source_gene,
                prior.source_transcript,
                prior.transcript_class,
            )
            observed = (
                current.seqid,
                current.source,
                current.strand,
                current.gene,
                current.source_gene,
                current.source_transcript,
                current.transcript_class,
            )
            if identity != observed:
                raise ValueError(
                    f"{path}:{line_number}: transcript {transcript} has conflicting identity"
                )
            prior.start = min(prior.start, start)
            prior.end = max(prior.end, end)
    if not transcripts:
        raise ValueError(f"{path}: no transcript/exon rows")
    return transcripts


def read_body_map(path: Path) -> dict[str, tuple[str, str]]:
    result: dict[str, tuple[str, str]] = {}
    with path.open() as handle:
        for line_number, line in enumerate(handle, 1):
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 3 or not all(fields):
                raise ValueError(
                    f"{path}:{line_number}: expected composite, transcript, gene"
                )
            composite, transcript, gene = fields
            if composite in result:
                raise ValueError(f"{path}:{line_number}: duplicate {composite}")
            result[composite] = (transcript, gene)
    return result


def read_info(path: Path) -> list[dict[str, str]]:
    with path.open(newline="") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        expected = ["Name", "Length", "Transcripts", "Haplotypes"]
        if reader.fieldnames != expected:
            raise ValueError(
                f"{path}: expected exact header {expected}, observed {reader.fieldnames}"
            )
        rows = list(reader)
    if not rows:
        raise ValueError(f"{path}: no vg rna paths")
    for line_number, row in enumerate(rows, 2):
        if None in row or not all(row.values()):
            raise ValueError(f"{path}:{line_number}: malformed/empty field")
        if not row["Length"].isdigit() or int(row["Length"]) <= 0:
            raise ValueError(f"{path}:{line_number}: invalid path length")
        if "," in row["Transcripts"]:
            raise ValueError(
                f"{path}:{line_number}: path must resolve to exactly one annotation Parent"
            )
    return rows


def stable_parent(prefix: str, *identity: str) -> str:
    digest = hashlib.sha256("\0".join(identity).encode()).hexdigest()
    return f"{prefix}_{digest}"


def annotation_row(
    *,
    path_row: dict[str, str],
    transcript_id: str,
    transcript: Transcript,
    feature_layer: str,
) -> dict[str, str]:
    exon_parent = stable_parent(
        "panSCrefexon1",
        transcript_id,
        transcript.gene,
        transcript.seqid,
        transcript.strand,
        str(transcript.start),
        str(transcript.end),
    )
    unique_parent = (
        exon_parent
        if feature_layer == "exon"
        else stable_parent("panSCrefbody1", exon_parent)
    )
    return {
        "schema_version": SCHEMA,
        "vg_path_name": path_row["Name"],
        "vg_path_length": path_row["Length"],
        "vg_haplotype_origins": path_row["Haplotypes"],
        "unique_parent": unique_parent,
        "source_parent": transcript_id,
        "input_parent": transcript_id,
        "canonical_transcript": transcript_id,
        "gene_id": transcript.gene,
        "source_path_or_contig": transcript.seqid,
        "sample": "reference",
        "haplotype": "0",
        "annotation_source": transcript.source,
        "feature_layer": feature_layer,
        "selection_status": "reference_projection",
        "fallback_status": "none",
        "exon_unique_parent": exon_parent,
        "gene_locus": (
            f"{transcript.seqid}:{transcript.strand}:"
            f"{transcript.start}-{transcript.end}"
        ),
        "source_gene": transcript.source_gene,
        "source_transcript": transcript.source_transcript,
        "transcript_class": transcript.transcript_class,
        "strand": transcript.strand,
        "start": str(transcript.start),
        "end": str(transcript.end),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--gtf", required=True, type=Path)
    parser.add_argument("--exon-info", required=True, type=Path)
    parser.add_argument("--body-info", type=Path)
    parser.add_argument("--body-identity-map", type=Path)
    parser.add_argument("--out", required=True, type=Path)
    args = parser.parse_args()

    transcripts = read_gtf(args.gtf)
    if (args.body_info is None) != (args.body_identity_map is None):
        parser.error("--body-info and --body-identity-map must be supplied together")
    body_map = (
        read_body_map(args.body_identity_map)
        if args.body_identity_map is not None
        else {}
    )
    output: list[dict[str, str]] = []
    seen_paths: set[str] = set()

    info_layers = [("exon", args.exon_info)]
    if args.body_info is not None:
        info_layers.append(("body", args.body_info))
    for feature_layer, info_path in info_layers:
        for path_row in read_info(info_path):
            path_name = path_row["Name"]
            if path_name in seen_paths:
                raise ValueError(f"duplicate vg path across layers: {path_name}")
            seen_paths.add(path_name)
            parent = path_row["Transcripts"]
            if feature_layer == "body":
                if parent not in body_map:
                    raise ValueError(
                        f"{info_path}: body Parent {parent} missing from identity map"
                    )
                transcript_id, gene = body_map[parent]
            else:
                transcript_id, gene = parent, None
            transcript = transcripts.get(transcript_id)
            if transcript is None:
                raise ValueError(
                    f"{info_path}: unknown transcript {transcript_id}"
                )
            if gene is not None and gene != transcript.gene:
                raise ValueError(
                    f"{info_path}: body {parent} maps to gene {gene}, expected "
                    f"{transcript.gene}"
                )
            output.append(
                annotation_row(
                    path_row=path_row,
                    transcript_id=transcript_id,
                    transcript=transcript,
                    feature_layer=feature_layer,
                )
            )

    args.out.parent.mkdir(parents=True, exist_ok=True)
    with args.out.open("w", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=FIELDS, delimiter="\t", lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(sorted(output, key=lambda row: row["vg_path_name"]))


if __name__ == "__main__":
    main()
