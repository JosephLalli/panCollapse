#!/usr/bin/env python3
"""Classify CAT Parents with independent structure and footprint evidence.

This is the current report-only attribution implementation. It does not rewrite an annotation or
a path-identity ledger. The archived earlier classifier asks the local
overlap question only when its intron-length vote is uninformative, so a match to one gene
can terminate classification before terminal exons from a second gene are examined.  This
classifier always scans the complete Parent first and keeps three answers separate:

1. IDENTITY STRUCTURE: which same-contig CAT ortholog transcript paths share the Parent's
   ordered splice junctions?  Exact and contained chains are recorded separately.  Exact
   reference intron chains and reference intron-length n-gram votes are retained as fallback
   evidence, not silently promoted to same-contig evidence.
2. ASSEMBLY-CONTIG FOOTPRINT: which same-strand CAT ortholog gene models share exonic bases
   with the complete Parent, and how are those gene models arranged on this exact assembly
   contig?  Exon-overlapping, span-overlapping, and span-disjoint pairs are distinguished.
3. COUNT ROLE: only after the whole-Parent scan can a Parent nominate one count gene.
   Ordered gene-specific components from exonically independent local loci are a structural
   readthrough.  Inseparable local multigene evidence is ``multigene_unresolved``.  Both are
   alignment-only.  Global reference-chain signatures are descriptive fallback evidence and
   never create a local gene assignment, readthrough, or paralog competitor.

Coordinates are never compared across assembly contigs.  A seqid such as
``HG00097#1#CM094071.1`` is an indivisible coordinate system; it is not normalized to chr20.
This is essential for a pangenomic annotation containing many assemblies.

The input CAT GFF3 must be plain, seekable, and grouped by seqid for parallel execution.
The script first indexes byte ranges, then forked workers parse independent contigs.  Results
are emitted in input-contig order, making output deterministic across worker counts.

Outputs in OUT_DIR:

* ``detailed_assignment.tsv.gz``: one row per selected CAT Parent, including orthologs.
* ``focus_evidence.tsv.gz``: the same rows whose assigned, prior, structure, or footprint
  genes intersect any ``--focus-gene``.
* ``focus_gene_summary.tsv``: counts by focus gene, prior verdict, current category, action.
* ``category_summary.tsv``: global current-category counts.
* ``path_count_policy.tsv.gz`` (with ``--path-identity-ledger``): one count role per graph
  path, using assembly-local Parent structure when available and exact reference identity
  only for ledger-only paths.
* ``RUN_MANIFEST.json``, ``SHA256SUMS``, and terminal ``STATUS``.

SELECTED BASELINE SETTINGS
--------------------------
The annotation lineage used by the best-performing chr20/chr21 counting preset was classified
with ``--relation-aware-policy v3 --ngram 5 --min-ngram-support 1 --max-evidence-genes 20``.
Worker count changes performance only (the sealed chr20 preparation used 16). Feed
``detailed_assignment.tsv.gz`` to ``apply_cat_denovo_corrections.py`` with its defaults, then run
``tag_nested_reference_hosts.py``. Do not use a truth-selected pair allowlist, rewrite ``gene_id``,
or treat the report's ``alignment_only`` summary as the selected counting filter.

The current correction step consumes the detailed category/action pair. Only
``single_gene_reassignment + reassign_countable`` and
``structural_readthrough + tag_readthrough_alignment_only`` rewrite annotation semantics; every
category remains an auditable tag and the count-time selected category set stays explicit.
"""

from __future__ import annotations

import argparse
import collections
import csv
import datetime as dt
import gzip
import hashlib
import io
import json
import multiprocessing as mp
import os
import re
import sys
from dataclasses import dataclass
from pathlib import Path
from typing import BinaryIO, Iterable, Iterator, Mapping, Sequence


SCHEMA_VERSION = "panSC-cat-parent-detailed-v4"
PATH_POLICY_SCHEMA_VERSION = "panSC-graph-path-count-policy-v2"
VERSION_SUFFIX = re.compile(r"\.\d+$")
# `paralog` is Liftoff's spelling; CAT writes `possible_paralog`. Omitting it meant every
# Liftoff-projected transcript exited the row filter below unjudged and reached the annotation
# with no verdict tag at all -- 71 transcripts on chr20, and an unjudged transcript is treated as
# clean by a tag-based filter, so a bridging one keeps bridging. The two spellings mean the same
# thing and both belong here.
DENOVO_CLASSES = frozenset(
    (
        "putative_novel_isoform",
        "putative_novel",
        "possible_paralog",
        "paralog",
        "poor_alignment",
    )
)
STRONG_IDENTITY_BASES = frozenset(
    ("local_exact_chain", "local_query_subchain", "local_gene_junction_set")
)
HOSTED_SMALL_RNA_GENE_TYPES = frozenset(("miRNA", "snoRNA", "scaRNA"))
BIN_SIZE = 16_384


OUTPUT_COLUMNS = (
    "schema_version",
    "contig",
    "sample",
    "haplotype",
    "parent",
    "parent_scope",
    "transcript_class",
    "assigned_gene",
    "strand",
    "query_start_1based",
    "query_end_1based_inclusive",
    "query_exons",
    "query_junctions",
    "query_exonic_bases",
    "source_transcript",
    "source_reference_gene",
    "source_reference_tags",
    "source_reference_relation",
    "alignment_mode",
    "pacbio_isoform_supported",
    "reference_support",
    "rna_support",
    "exon_annotation_support",
    "intron_annotation_support",
    "exon_rna_support",
    "intron_rna_support",
    "novel_5p_cap",
    "novel_poly_a",
    "valid_start",
    "valid_stop",
    "proper_orf",
    "transcript_score",
    "prior_verdict",
    "prior_resolved_by",
    "prior_proposed_gene",
    "prior_chain_verdict",
    "prior_evidence",
    "local_exact_chain_genes",
    "local_query_subchain_genes",
    "local_ortholog_subchain_genes",
    "local_all_query_junction_genes",
    "local_gene_unique_junction_counts",
    "reference_exact_chain_genes",
    "reference_ngram_votes",
    "identity_basis",
    "identity_candidates",
    "identity_gene",
    "footprint_gene_count",
    "footprint_genes",
    "footprint_evidence",
    "same_strand_span_context",
    "antisense_span_context",
    "complete_ortholog_model_genes",
    "gene_pair_topology",
    "intergene_exon_bridges",
    "local_component_genes",
    "local_component_order",
    "local_ambiguous_component_genes",
    "local_independent_component_pairs",
    "eligible_footprint_genes",
    "ineligible_footprint_genes",
    "eligible_decisive_genes",
    "ineligible_decisive_genes",
    "structural_class",
    "count_role",
    "count_gene",
    "current_category",
    "current_proposed_gene",
    "current_action",
    "category_reason",
    "prior_current_relation",
)

PATH_POLICY_COLUMNS = (
    "schema_version",
    "vg_path_name",
    "source_path_or_contig",
    "input_parent",
    "source_parent",
    "canonical_transcript",
    "source_transcript",
    "transcript_class",
    "feature_layer",
    "ledger_gene",
    "parent_match_status",
    "matched_parent",
    "policy_basis",
    "source_reference_gene",
    "source_reference_tags",
    "source_reference_relation",
    "parent_structural_class",
    "parent_current_category",
    "count_role",
    "count_gene",
    "policy_reason",
)


@dataclass(frozen=True)
class Segment:
    ordinal: int
    contig: str
    ranges: tuple[tuple[int, int], ...]


@dataclass(frozen=True)
class ReferenceIndex:
    exact_chain_genes: Mapping[tuple[int, ...], frozenset[str]]
    ngram_genes: Mapping[tuple[int, ...], frozenset[str]]
    gene_strand: Mapping[str, str]
    transcript_gene: Mapping[str, str]
    transcript_tags: Mapping[str, frozenset[str]]
    gene_tags: Mapping[str, frozenset[str]]
    gene_types: Mapping[str, str]
    transcript_count: int


@dataclass
class TxBuilder:
    gene: str
    transcript_class: str
    strand: str
    exons: list[tuple[int, int]]
    metadata: dict[str, str]


@dataclass(frozen=True)
class Transcript:
    parent: str
    gene: str
    transcript_class: str
    strand: str
    exons: tuple[tuple[int, int], ...]
    metadata: Mapping[str, str]


@dataclass(frozen=True)
class ContigResult:
    ordinal: int
    rows: str
    focus_rows: str
    category_counts: Mapping[str, int]
    focus_counts: Mapping[tuple[str, str, str, str, str], int]
    scope_counts: Mapping[str, int]
    count_role_counts: Mapping[str, int]
    row_count: int
    focus_row_count: int


_REFERENCE: ReferenceIndex | None = None
_PRIOR: Mapping[tuple[str, str], Mapping[str, str]] = {}
_CAT_PATH = ""
_FOCUS_GENES: frozenset[str] = frozenset()
_NGRAM = 5
_MIN_NGRAM_SUPPORT = 1
_MAX_EVIDENCE_GENES = 20
_RELATION_AWARE_POLICY = "v3"
_ELIGIBLE_COUNT_GENES: frozenset[str] | None = None
_CAT_HANDLE: BinaryIO | None = None


def canonical_gene(value: str | None) -> str:
    if not value:
        return ""
    return VERSION_SUFFIX.sub("", value.split("#")[-1])


def gene_type_memberships(value: str) -> frozenset[str]:
    """Return the simultaneous categories encoded by a comma-separated gene_type."""
    return frozenset(
        member.strip() for member in value.split(",") if member.strip()
    )


def load_eligible_count_genes(path: str) -> frozenset[str]:
    """Read a strict, one-column stable-gene count universe."""
    genes: set[str] = set()
    with open(path) as handle:
        for line_number, raw_line in enumerate(handle, start=1):
            value = raw_line.strip()
            if not value or value.startswith("#"):
                continue
            fields = value.split()
            if len(fields) != 1:
                raise ValueError(
                    f"eligible count genes must have one column: {path}:{line_number}"
                )
            gene = canonical_gene(fields[0])
            if not gene:
                raise ValueError(
                    f"eligible count genes contains an empty gene: {path}:{line_number}"
                )
            genes.add(gene)
    if not genes:
        raise ValueError(f"eligible count genes contains no usable genes: {path}")
    return frozenset(genes)


def is_count_eligible(gene: str) -> bool:
    return _ELIGIBLE_COUNT_GENES is None or gene in _ELIGIBLE_COUNT_GENES


def attr_text(attributes: str, key: str) -> str:
    marker = key + "="
    start = attributes.find(marker)
    while start >= 0 and start > 0 and attributes[start - 1] != ";":
        start = attributes.find(marker, start + 1)
    if start >= 0:
        start += len(marker)
        end = attributes.find(";", start)
        return attributes[start:] if end < 0 else attributes[start:end]
    # Reference input may be GTF even though the production input is GFF3.  CAT input is
    # deliberately stricter and uses attr_bytes() because CAT emits GFF3 key=value fields.
    marker = key + ' "'
    start = attributes.find(marker)
    if start < 0:
        return ""
    start += len(marker)
    end = attributes.find('"', start)
    return attributes[start:] if end < 0 else attributes[start:end]


def attr_bytes(attributes: bytes, key: bytes) -> bytes:
    marker = key + b"="
    start = attributes.find(marker)
    while start >= 0 and start > 0 and attributes[start - 1] != 59:  # ';'
        start = attributes.find(marker, start + 1)
    if start < 0:
        return b""
    start += len(marker)
    end = attributes.find(b";", start)
    return attributes[start:] if end < 0 else attributes[start:end]


def decode(value: bytes) -> str:
    return value.decode("utf-8", errors="strict")


def open_text_auto(path: str):
    return gzip.open(path, "rt") if path.endswith(".gz") else open(path, "rt")


def sha256_file(path: str) -> str:
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(8 * 1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def merge_intervals(
    intervals: Iterable[tuple[int, int]],
) -> tuple[tuple[int, int], ...]:
    merged: list[list[int]] = []
    for start, end in sorted(set(intervals)):
        if start >= end:
            continue
        if merged and start <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], end)
        else:
            merged.append([start, end])
    return tuple((start, end) for start, end in merged)


def transcript_exons(
    intervals: Iterable[tuple[int, int]],
) -> tuple[tuple[int, int], ...]:
    """Return ordered, unique exon intervals without joining adjacent exons."""
    return tuple(sorted(set((start, end) for start, end in intervals if start < end)))


def junctions(
    exons: Sequence[tuple[int, int]], strand: str
) -> tuple[tuple[int, int], ...]:
    if len(exons) < 2:
        return ()
    ordered = list(exons if strand != "-" else reversed(exons))
    result: list[tuple[int, int]] = []
    for left, right in zip(ordered, ordered[1:]):
        if strand == "-":
            result.append((left[0], right[1]))
        else:
            result.append((left[1], right[0]))
    return tuple(result)


def intron_lengths(exons: Sequence[tuple[int, int]], strand: str) -> tuple[int, ...]:
    return tuple(abs(acceptor - donor) for donor, acceptor in junctions(exons, strand))


def ngrams(values: Sequence[int], n: int) -> Iterator[tuple[int, ...]]:
    for index in range(len(values) - n + 1):
        yield tuple(values[index : index + n])


def is_contiguous_subsequence(small: Sequence[object], large: Sequence[object]) -> bool:
    if len(small) > len(large):
        return False
    if not small:
        return False
    return any(
        tuple(large[i : i + len(small)]) == tuple(small)
        for i in range(len(large) - len(small) + 1)
    )


def overlap_bp(a: Sequence[tuple[int, int]], b: Sequence[tuple[int, int]]) -> int:
    i = j = total = 0
    while i < len(a) and j < len(b):
        start = max(a[i][0], b[j][0])
        end = min(a[i][1], b[j][1])
        if start < end:
            total += end - start
        if a[i][1] <= b[j][1]:
            i += 1
        else:
            j += 1
    return total


def intervals_fully_covered(
    targets: Sequence[tuple[int, int]], containers: Sequence[tuple[int, int]]
) -> bool:
    if not targets:
        return False
    j = 0
    for start, end in targets:
        while j < len(containers) and containers[j][1] <= start:
            j += 1
        if j == len(containers) or containers[j][0] > start or containers[j][1] < end:
            return False
    return True


def build_reference_index(path: str, ngram_size: int) -> ReferenceIndex:
    exons_by_tx: dict[str, list[tuple[int, int]]] = collections.defaultdict(list)
    tx_gene: dict[str, str] = {}
    tx_strand: dict[str, str] = {}
    gene_strand: dict[str, str] = {}
    transcript_gene: dict[str, str] = {}
    transcript_tags: dict[str, set[str]] = collections.defaultdict(set)
    gene_tags: dict[str, set[str]] = collections.defaultdict(set)
    gene_types: dict[str, str] = {}
    with open_text_auto(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t", 8)
            if len(fields) != 9:
                continue
            if fields[2] == "gene":
                gene = canonical_gene(attr_text(fields[8], "gene_id"))
                if gene:
                    gene_tags[gene].update(
                        tag for tag in attr_text(fields[8], "tag").split(",") if tag
                    )
                    gene_type = attr_text(fields[8], "gene_type") or attr_text(
                        fields[8], "gene_biotype"
                    )
                    if gene_type:
                        previous = gene_types.setdefault(gene, gene_type)
                        if gene_type_memberships(previous) != gene_type_memberships(
                            gene_type
                        ):
                            raise ValueError(
                                f"reference gene has conflicting gene_type: {gene}"
                            )
                continue
            if fields[2] != "exon":
                continue
            transcript = attr_text(fields[8], "transcript_id")
            gene = canonical_gene(attr_text(fields[8], "gene_id"))
            if not transcript or not gene:
                continue
            start = int(fields[3]) - 1
            end = int(fields[4])
            exons_by_tx[transcript].append((start, end))
            tx_gene[transcript] = gene
            tx_strand[transcript] = fields[6]
            gene_strand[gene] = fields[6]
            stable_transcript = canonical_gene(transcript)
            previous_gene = transcript_gene.setdefault(stable_transcript, gene)
            if previous_gene != gene:
                raise ValueError(
                    f"reference transcript maps to multiple genes: {stable_transcript}"
                )
            transcript_tags[stable_transcript].update(
                tag for tag in attr_text(fields[8], "tag").split(",") if tag
            )

    exact: dict[tuple[int, ...], set[str]] = collections.defaultdict(set)
    grams: dict[tuple[int, ...], set[str]] = collections.defaultdict(set)
    for transcript, raw_exons in exons_by_tx.items():
        exons = transcript_exons(raw_exons)
        chain = intron_lengths(exons, tx_strand[transcript])
        if chain:
            exact[chain].add(tx_gene[transcript])
        for gram in ngrams(chain, ngram_size):
            grams[gram].add(tx_gene[transcript])
    return ReferenceIndex(
        exact_chain_genes={key: frozenset(value) for key, value in exact.items()},
        ngram_genes={key: frozenset(value) for key, value in grams.items()},
        gene_strand=gene_strand,
        transcript_gene=transcript_gene,
        transcript_tags={
            key: frozenset(value) for key, value in transcript_tags.items()
        },
        gene_tags={key: frozenset(value) for key, value in gene_tags.items()},
        gene_types=gene_types,
        transcript_count=len(exons_by_tx),
    )


def load_prior_report(path: str) -> dict[tuple[str, str], dict[str, str]]:
    result: dict[tuple[str, str], dict[str, str]] = {}
    with open_text_auto(path) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required = {
            "contig",
            "parent",
            "verdict",
            "resolved_by",
            "proposed_gene",
            "chain_verdict",
            "evidence",
        }
        missing = required - set(reader.fieldnames or ())
        if missing:
            raise ValueError(
                f"prior report missing columns: {', '.join(sorted(missing))}"
            )
        for row in reader:
            key = (row["contig"], row["parent"])
            if key in result:
                raise ValueError(f"duplicate prior report key: {key[0]} {key[1]}")
            result[key] = {
                "verdict": row["verdict"],
                "resolved_by": row["resolved_by"],
                "proposed_gene": canonical_gene(row["proposed_gene"]),
                "chain_verdict": row["chain_verdict"],
                "evidence": row["evidence"],
            }
    return result


def index_cat_contigs(path: str) -> tuple[list[Segment], str]:
    if path.endswith(".gz"):
        raise ValueError(
            "parallel detailed classification requires a plain, seekable CAT GFF3"
        )
    ranges_by_contig: dict[str, list[tuple[int, int]]] = {}
    current = ""
    current_start = 0
    offset = 0
    digest = hashlib.sha256()
    with open(path, "rb", buffering=8 * 1024 * 1024) as handle:
        for raw in handle:
            digest.update(raw)
            line_start = offset
            offset += len(raw)
            if raw.startswith(b"#") or b"\t" not in raw:
                continue
            contig = decode(raw.split(b"\t", 1)[0])
            if not current:
                current = contig
                current_start = line_start
            elif contig != current:
                ranges_by_contig.setdefault(current, []).append(
                    (current_start, line_start)
                )
                current = contig
                current_start = line_start
    if current:
        ranges_by_contig.setdefault(current, []).append((current_start, offset))
    segments = [
        Segment(ordinal, contig, tuple(ranges))
        for ordinal, (contig, ranges) in enumerate(ranges_by_contig.items())
    ]
    return segments, digest.hexdigest()


def _metadata(attributes: bytes) -> dict[str, str]:
    keys = (
        b"source_transcript",
        b"alignment_mode",
        b"pacbio_isoform_supported",
        b"reference_support",
        b"rna_support",
        b"exon_annotation_support",
        b"intron_annotation_support",
        b"exon_rna_support",
        b"intron_rna_support",
        b"novel_5p_cap",
        b"novel_poly_a",
        b"valid_start",
        b"valid_stop",
        b"proper_orf",
        b"transcript_score",
    )
    return {decode(key): decode(attr_bytes(attributes, key)) for key in keys}


def _parse_segment(
    segment: Segment,
) -> tuple[dict[str, Transcript], dict[str, Transcript]]:
    global _CAT_HANDLE
    if _CAT_HANDLE is None:
        _CAT_HANDLE = open(_CAT_PATH, "rb", buffering=8 * 1024 * 1024)
    handle = _CAT_HANDLE
    denovo: dict[str, TxBuilder] = {}
    ortholog: dict[str, TxBuilder] = {}
    for start_offset, end_offset in segment.ranges:
        handle.seek(start_offset)
        while handle.tell() < end_offset:
            raw = handle.readline()
            if not raw:
                break
            if raw.startswith(b"#"):
                continue
            fields = raw.rstrip(b"\r\n").split(b"\t", 8)
            if len(fields) != 9 or fields[2] != b"exon":
                continue
            attributes = fields[8]
            cls_b = attr_bytes(attributes, b"transcript_class")
            if not cls_b:
                continue
            cls = decode(cls_b)
            if cls not in DENOVO_CLASSES and cls != "ortholog":
                continue
            parent_b = attr_bytes(attributes, b"Parent") or attr_bytes(
                attributes, b"transcript_id"
            )
            gene_b = attr_bytes(attributes, b"source_gene")
            if not parent_b or not gene_b:
                continue
            parent = decode(parent_b)
            gene = canonical_gene(decode(gene_b))
            strand = decode(fields[6])
            start = int(fields[3]) - 1
            end = int(fields[4])
            target = ortholog if cls == "ortholog" else denovo
            entry = target.get(parent)
            if entry is None:
                entry = TxBuilder(
                    gene=gene,
                    transcript_class=cls,
                    strand=strand,
                    exons=[],
                    metadata=_metadata(attributes),
                )
                target[parent] = entry
            elif (entry.gene, entry.strand, entry.transcript_class) != (
                gene,
                strand,
                cls,
            ):
                raise ValueError(
                    f"inconsistent CAT Parent metadata on {segment.contig}: {parent}"
                )
            entry.exons.append((start, end))

    def freeze(builders: Mapping[str, TxBuilder]) -> dict[str, Transcript]:
        return {
            parent: Transcript(
                parent=parent,
                gene=builder.gene,
                transcript_class=builder.transcript_class,
                strand=builder.strand,
                exons=transcript_exons(builder.exons),
                metadata=builder.metadata,
            )
            for parent, builder in builders.items()
            if builder.exons
        }

    return freeze(denovo), freeze(ortholog)


def _add_to_bins(
    index: dict[tuple[str, int], list[int]],
    record_id: int,
    strand: str,
    start: int,
    end: int,
) -> None:
    for bin_id in range(start // BIN_SIZE, (max(start, end - 1) // BIN_SIZE) + 1):
        index[(strand, bin_id)].append(record_id)


def _candidate_record_ids(
    index: Mapping[tuple[str, int], Sequence[int]], strand: str, start: int, end: int
) -> set[int]:
    result: set[int] = set()
    for bin_id in range(start // BIN_SIZE, (max(start, end - 1) // BIN_SIZE) + 1):
        result.update(index.get((strand, bin_id), ()))
    return result


def _serialize_genes(genes: Iterable[str]) -> str:
    return ",".join(sorted(set(g for g in genes if g))) or "."


def _serialize_counts(counts: Mapping[str, int], limit: int | None = None) -> str:
    items = sorted(counts.items(), key=lambda item: (-item[1], item[0]))
    if limit is not None:
        items = items[:limit]
    return ",".join(f"{gene}:{value}" for gene, value in items) or "."


def _tsv_line(row: Mapping[str, object]) -> str:
    values = []
    for column in OUTPUT_COLUMNS:
        value = str(row.get(column, ""))
        values.append(value.replace("\t", " ").replace("\r", " ").replace("\n", " "))
    return "\t".join(values) + "\n"


def _selected_tsv_line(row: Mapping[str, object], columns: Sequence[str]) -> str:
    values = []
    for column in columns:
        value = str(row.get(column, ""))
        values.append(value.replace("\t", " ").replace("\r", " ").replace("\n", " "))
    return "\t".join(values) + "\n"


def _span_context(
    span_records: Sequence[tuple[int, int, str, str]],
    span_bins: Mapping[tuple[str, int], Sequence[int]],
    strand: str,
    start: int,
    end: int,
) -> set[str]:
    genes: set[str] = set()
    for record_id in _candidate_record_ids(span_bins, strand, start, end):
        a, b, gene, _ = span_records[record_id]
        if a < end and start < b:
            genes.add(gene)
    return genes


def _prior_relation(prior: Mapping[str, str], category: str, proposed: str) -> str:
    verdict = prior.get("verdict", "")
    prior_gene = canonical_gene(prior.get("proposed_gene", ""))
    if not verdict:
        return "prior_missing"
    if verdict == "chain_misassignment":
        if category == "single_gene_reassignment" and prior_gene == proposed:
            return "actionable_agreement"
        if category == "identity_candidate_reassignment" and prior_gene == proposed:
            return "direction_agreement_confidence_changed"
        return "prior_reassignment_not_reproduced"
    if verdict in {
        "chain_readthrough",
        "overlap_readthrough",
        "overlap_readthrough_partial",
    }:
        if category == "structural_readthrough":
            return "readthrough_agreement"
        return "prior_readthrough_not_reproduced"
    if verdict in {"chain_consistent", "overlap_consistent"}:
        return (
            "consistent_agreement"
            if category.startswith("single_gene_")
            else "prior_consistent_changed"
        )
    if verdict == "overlap_label_conflict":
        return (
            "label_conflict_agreement"
            if category == "label_conflict"
            else "prior_label_conflict_changed"
        )
    return "category_changed" if verdict != category else "category_label_agreement"


def _gene_has_relation(gene: str, relation: str) -> bool:
    """Return a trusted GENCODE gene-row relation, never an exon-level inference."""
    if _REFERENCE is None:
        return False
    return (
        relation in _REFERENCE.gene_tags.get(gene, frozenset())
        or relation in gene_type_memberships(_REFERENCE.gene_types.get(gene, ""))
    )


def _analyze_segment(segment: Segment) -> ContigResult:
    if _REFERENCE is None:
        raise RuntimeError("reference index not initialized")
    denovo, ortholog = _parse_segment(segment)
    duplicate_parents = set(denovo) & set(ortholog)
    if duplicate_parents:
        raise ValueError(
            f"CAT Parents occur in both scopes on {segment.contig}: "
            f"{', '.join(sorted(duplicate_parents)[:5])}"
        )
    subjects = {**ortholog, **denovo}

    orth_junctions: dict[str, tuple[tuple[int, int], ...]] = {}
    junction_to_parents: dict[tuple[int, int], set[str]] = collections.defaultdict(set)
    gene_exons_raw: dict[tuple[str, str], list[tuple[int, int]]] = (
        collections.defaultdict(list)
    )
    gene_exon_counts: dict[tuple[str, str], collections.Counter[tuple[int, int]]] = (
        collections.defaultdict(collections.Counter)
    )
    orth_by_gene: dict[tuple[str, str], list[str]] = collections.defaultdict(list)
    for parent, tx in ortholog.items():
        js = junctions(tx.exons, tx.strand)
        orth_junctions[parent] = js
        orth_by_gene[(tx.gene, tx.strand)].append(parent)
        gene_exons_raw[(tx.gene, tx.strand)].extend(tx.exons)
        gene_exon_counts[(tx.gene, tx.strand)].update(tx.exons)
        for junction in js:
            junction_to_parents[junction].add(parent)

    gene_unions = {key: merge_intervals(value) for key, value in gene_exons_raw.items()}
    gene_spans = {
        key: (min(start for start, _ in value), max(end for _, end in value))
        for key, value in gene_unions.items()
        if value
    }

    exon_records: list[tuple[int, int, str, str, str]] = []
    exon_bins: dict[tuple[str, int], list[int]] = collections.defaultdict(list)
    span_records: list[tuple[int, int, str, str]] = []
    span_bins: dict[tuple[str, int], list[int]] = collections.defaultdict(list)
    for parent, tx in sorted(ortholog.items()):
        for start, end in tx.exons:
            record_id = len(exon_records)
            exon_records.append((start, end, tx.gene, tx.strand, parent))
            _add_to_bins(exon_bins, record_id, tx.strand, start, end)
    for (gene, strand), intervals in sorted(gene_unions.items()):
        span_start, span_end = gene_spans[(gene, strand)]
        record_id = len(span_records)
        span_records.append((span_start, span_end, gene, strand))
        _add_to_bins(span_bins, record_id, strand, span_start, span_end)

    category_counts: collections.Counter[str] = collections.Counter()
    focus_counts: collections.Counter[tuple[str, str, str, str, str]] = (
        collections.Counter()
    )
    scope_counts: collections.Counter[str] = collections.Counter()
    count_role_counts: collections.Counter[str] = collections.Counter()
    row_lines: list[str] = []
    focus_lines: list[str] = []
    sample_parts = segment.contig.split("#")
    sample = sample_parts[0] if sample_parts else segment.contig
    haplotype = sample_parts[1] if len(sample_parts) > 1 else "."

    def anchor_intervals(
        gene: str, strand: str, excluded_parent: str
    ) -> tuple[tuple[int, int], ...]:
        counts = gene_exon_counts.get((gene, strand), collections.Counter())
        excluded = (
            set(ortholog[excluded_parent].exons)
            if excluded_parent in ortholog
            and ortholog[excluded_parent].gene == gene
            and ortholog[excluded_parent].strand == strand
            else set()
        )
        return merge_intervals(
            interval
            for interval, count in counts.items()
            if count - int(interval in excluded) > 0
        )

    def anchor_exons(
        gene: str, strand: str, excluded_parent: str
    ) -> tuple[tuple[int, int], ...]:
        counts = gene_exon_counts.get((gene, strand), collections.Counter())
        excluded = (
            set(ortholog[excluded_parent].exons)
            if excluded_parent in ortholog
            and ortholog[excluded_parent].gene == gene
            and ortholog[excluded_parent].strand == strand
            else set()
        )
        return tuple(
            sorted(
                interval
                for interval, count in counts.items()
                if count - int(interval in excluded) > 0
            )
        )

    for parent in sorted(subjects):
        tx = subjects[parent]
        parent_scope = "ortholog" if parent in ortholog else "denovo"
        query_union = merge_intervals(tx.exons)
        query_total = sum(end - start for start, end in query_union)
        query_junctions = junctions(tx.exons, tx.strand)
        # Source-label consistency is deliberately computed before the policy tree.  The
        # relation-aware modes may use trusted GENCODE gene rows, but source provenance is
        # only a guardrail and never substitutes for assembly-local structure.
        source_transcript = canonical_gene(tx.metadata.get("source_transcript", ""))
        source_reference_gene = _REFERENCE.transcript_gene.get(source_transcript, "")
        source_reference_tags = _REFERENCE.transcript_tags.get(
            source_transcript, frozenset()
        )
        if not source_transcript or source_transcript == "N/A":
            source_reference_relation = "not_available"
        elif not source_reference_gene:
            source_reference_relation = "source_transcript_not_in_reference"
        elif source_reference_gene == tx.gene:
            source_reference_relation = "label_consistent"
        else:
            source_reference_relation = "label_conflict_descriptive_only"
        source_is_true_readthrough = bool(source_reference_gene) and (
            "readthrough_transcript" in source_reference_tags
            or _gene_has_relation(source_reference_gene, "readthrough_gene")
        )

        candidate_parents: set[str] = set()
        for junction in query_junctions:
            candidate_parents.update(junction_to_parents.get(junction, ()))
        relation_genes: dict[str, set[str]] = collections.defaultdict(set)
        for orth_parent in candidate_parents:
            if orth_parent == parent:
                continue
            other = ortholog[orth_parent]
            if other.strand != tx.strand:
                continue
            other_junctions = orth_junctions[orth_parent]
            if query_junctions == other_junctions and query_junctions:
                relation = "exact"
            elif is_contiguous_subsequence(query_junctions, other_junctions):
                relation = "query_subchain"
            elif is_contiguous_subsequence(other_junctions, query_junctions):
                relation = "ortholog_subchain"
            else:
                relation = "partial"
            relation_genes[relation].add(other.gene)

        local_all_query: set[str] = set()
        if query_junctions:
            genes_by_junction = [
                {
                    ortholog[orth_parent].gene
                    for orth_parent in junction_to_parents.get(junction, ())
                    if orth_parent != parent
                }
                for junction in query_junctions
            ]
            observed = set().union(*genes_by_junction)
            for gene in observed:
                if all(gene in genes for genes in genes_by_junction):
                    local_all_query.add(gene)
        unique_junction_counts: collections.Counter[str] = collections.Counter()
        for junction in query_junctions:
            genes = {
                ortholog[orth_parent].gene
                for orth_parent in junction_to_parents.get(junction, ())
                if orth_parent != parent
            }
            if len(genes) == 1:
                unique_junction_counts[next(iter(genes))] += 1

        chain = intron_lengths(tx.exons, tx.strand)
        reference_exact = {
            gene
            for gene in _REFERENCE.exact_chain_genes.get(chain, frozenset())
            if _REFERENCE.gene_strand.get(gene, tx.strand) == tx.strand
        }
        ngram_votes: collections.Counter[str] = collections.Counter()
        for gram in ngrams(chain, _NGRAM):
            for gene in _REFERENCE.ngram_genes.get(gram, frozenset()):
                if _REFERENCE.gene_strand.get(gene, tx.strand) == tx.strand:
                    ngram_votes[gene] += 1
        ngram_votes = collections.Counter(
            {
                gene: value
                for gene, value in ngram_votes.items()
                if value >= _MIN_NGRAM_SUPPORT
            }
        )

        local_exact = relation_genes.get("exact", set())
        local_query_subchain = relation_genes.get("query_subchain", set())
        local_ortholog_subchain = relation_genes.get("ortholog_subchain", set())
        if local_exact:
            identity_basis = "local_exact_chain"
            identity_candidates = set(local_exact)
        elif local_query_subchain:
            identity_basis = "local_query_subchain"
            identity_candidates = set(local_query_subchain)
        elif local_all_query:
            identity_basis = "local_gene_junction_set"
            identity_candidates = set(local_all_query)
        elif local_ortholog_subchain:
            identity_basis = "local_ortholog_subchain"
            identity_candidates = set(local_ortholog_subchain)
        elif query_junctions:
            identity_basis = "no_local_structure_match"
            identity_candidates = set()
        else:
            identity_basis = "single_exon_untestable"
            identity_candidates = set()
        identity_gene = (
            next(iter(identity_candidates)) if len(identity_candidates) == 1 else ""
        )

        candidate_genes: set[str] = set()
        for start, end in query_union:
            record_ids = _candidate_record_ids(exon_bins, tx.strand, start, end)
            for record_id in record_ids:
                a, b, gene, _, orth_parent = exon_records[record_id]
                if orth_parent != parent and max(start, a) < min(end, b):
                    candidate_genes.add(gene)

        anchor_unions = {
            gene: anchor_intervals(gene, tx.strand, parent) for gene in candidate_genes
        }
        anchor_exons_by_gene = {
            gene: anchor_exons(gene, tx.strand, parent) for gene in candidate_genes
        }
        shared_by_gene: collections.Counter[str] = collections.Counter(
            {
                gene: overlap_bp(query_union, intervals)
                for gene, intervals in anchor_unions.items()
                if overlap_bp(query_union, intervals) > 0
            }
        )
        footprint_genes = set(shared_by_gene)

        ordered_exons = list(tx.exons if tx.strand != "-" else reversed(tx.exons))
        component_exon_gene_sets: list[set[str]] = []
        for start, end in ordered_exons:
            component_genes = {
                gene
                for gene, intervals in anchor_exons_by_gene.items()
                if any(
                    (a <= start and end <= b) or (start <= a and b <= end)
                    for a, b in intervals
                )
            }
            component_exon_gene_sets.append(component_genes)

        junction_gene_sets = [
            {
                ortholog[orth_parent].gene
                for orth_parent in junction_to_parents.get(junction, ())
                if orth_parent != parent
            }
            for junction in query_junctions
        ]
        component_order: list[str] = []
        ambiguous_component_genes: set[str] = set()
        for index, exon_genes in enumerate(component_exon_gene_sets):
            event_sets = [exon_genes]
            if index < len(junction_gene_sets):
                event_sets.append(junction_gene_sets[index])
            for genes in event_sets:
                if len(genes) == 1:
                    gene = next(iter(genes))
                    if not component_order or component_order[-1] != gene:
                        component_order.append(gene)
                elif len(genes) > 1:
                    ambiguous_component_genes.update(genes)
        component_genes = set(component_order)
        same_span = _span_context(
            span_records, span_bins, tx.strand, query_union[0][0], query_union[-1][1]
        )
        opposite = "+" if tx.strand == "-" else "-"
        antisense_span = _span_context(
            span_records, span_bins, opposite, query_union[0][0], query_union[-1][1]
        )

        footprint_items: list[tuple[str, int, float, float, str]] = []
        for gene, shared in shared_by_gene.items():
            query_fraction = shared / query_total if query_total else 0.0
            anchor_total = sum(end - start for start, end in anchor_unions[gene])
            anchor_fraction = shared / anchor_total if anchor_total else 0.0
            if query_fraction == 1.0 and anchor_fraction == 1.0:
                relation = "equal_exon_union"
            elif query_fraction == 1.0:
                relation = "query_within_gene_exons"
            elif anchor_fraction == 1.0:
                relation = "gene_exons_within_query"
            else:
                relation = "partial_exonic_overlap"
            footprint_items.append(
                (gene, shared, query_fraction, anchor_fraction, relation)
            )
        footprint_items.sort(key=lambda item: (-item[1], item[0]))

        complete_model_genes: set[str] = set()
        for gene in footprint_genes:
            for orth_parent in orth_by_gene.get((gene, tx.strand), ()):
                if orth_parent == parent:
                    continue
                if intervals_fully_covered(ortholog[orth_parent].exons, query_union):
                    complete_model_genes.add(gene)
                    break

        pair_topology: list[tuple[str, str, str]] = []
        exon_disjoint_pairs: set[tuple[str, str]] = set()
        for i, gene_a in enumerate(sorted(footprint_genes)):
            for gene_b in sorted(footprint_genes)[i + 1 :]:
                union_a = anchor_unions[gene_a]
                union_b = anchor_unions[gene_b]
                exon_shared = overlap_bp(union_a, union_b)
                a_start, a_end = union_a[0][0], union_a[-1][1]
                b_start, b_end = union_b[0][0], union_b[-1][1]
                pair = (gene_a, gene_b)
                if exon_shared:
                    relation = f"exon_overlap:{exon_shared}"
                elif a_end <= b_start or b_end <= a_start:
                    gap = max(a_start, b_start) - min(a_end, b_end)
                    relation = f"span_disjoint:gap={gap}"
                    exon_disjoint_pairs.add(pair)
                else:
                    relation = "span_overlap_exon_disjoint"
                    exon_disjoint_pairs.add(pair)
                pair_topology.append((gene_a, gene_b, relation))

        directed_bridges = [
            (left, right)
            for left, right in zip(component_order, component_order[1:])
            if left != right
        ]
        bridge_pairs = {tuple(sorted(pair)) for pair in directed_bridges}
        independent_pairs = bridge_pairs & exon_disjoint_pairs
        # Any overlap is useful context, but only gene-specific components, a fully enclosed
        # ortholog model, or splice-chain identity can veto a single-gene decision.  This
        # keeps a 139-bp shared exon from converting two otherwise independent transcripts
        # into readthroughs, while still catching a Parent that incorporates a second gene.
        decisive_genes = (
            component_genes
            | ambiguous_component_genes
            | complete_model_genes
            | identity_candidates
        )
        strong_identity = identity_basis in STRONG_IDENTITY_BASES
        competitor_genes = (footprint_genes | decisive_genes) - {tx.gene}
        decisive_competitor_genes = decisive_genes - {tx.gene}
        eligible_footprint_genes = {
            gene for gene in footprint_genes if is_count_eligible(gene)
        }
        ineligible_footprint_genes = footprint_genes - eligible_footprint_genes
        eligible_decisive_genes = {
            gene for gene in decisive_genes if is_count_eligible(gene)
        }
        ineligible_decisive_genes = decisive_genes - eligible_decisive_genes
        # The full structural footprint remains reported above.  A selected count universe
        # narrows only inseparable count-ambiguity vetoes; an independent bridge remains a
        # structural readthrough regardless of either gene's count eligibility.
        count_decisive_genes = eligible_decisive_genes
        count_identity_candidates = {
            gene for gene in identity_candidates if is_count_eligible(gene)
        }
        count_identity_gene = (
            identity_gene if identity_gene and is_count_eligible(identity_gene) else ""
        )
        count_clean_identity_components = bool(count_identity_gene) and (
            count_decisive_genes <= {count_identity_gene}
        )
        competitor_junction_evidence = any(
            set(genes) - {tx.gene} for genes in junction_gene_sets
        )
        assigned_is_true_readthrough = _gene_has_relation(
            tx.gene, "readthrough_gene"
        )

        # A source-labelled component can be independently verified by a single local
        # same-gene ortholog whose junctions exactly match and whose exons cover every query
        # base.  This deliberately rejects query subchains, junction evidence assembled
        # across several Parents, and terminal extensions that splice-length signatures
        # alone cannot distinguish.
        exact_geometry_same_gene_parents = tuple(
            orth_parent
            for orth_parent in orth_by_gene.get((tx.gene, tx.strand), ())
            if orth_parent != parent
            and bool(query_junctions)
            and orth_junctions[orth_parent] == query_junctions
            and intervals_fully_covered(
                query_union, merge_intervals(ortholog[orth_parent].exons)
            )
        )
        uniquely_explained_by_same_gene_parent = (
            len(exact_geometry_same_gene_parents) == 1
        )

        hosted_small_rna_component_guard = (
            source_reference_relation == "label_consistent"
            and _gene_has_relation(tx.gene, "ncRNA_host")
            and not assigned_is_true_readthrough
            and bool(competitor_genes)
            and all(
                bool(
                    gene_type_memberships(_REFERENCE.gene_types.get(gene, ""))
                    & HOSTED_SMALL_RNA_GENE_TYPES
                )
                for gene in competitor_genes
            )
            and competitor_genes <= complete_model_genes
            and not independent_pairs
            and not competitor_junction_evidence
        )
        readthrough_component_guard = (
            source_reference_relation == "label_consistent"
            and not assigned_is_true_readthrough
            and bool(decisive_competitor_genes)
            and all(
                _gene_has_relation(gene, "readthrough_gene")
                for gene in decisive_competitor_genes
            )
            and uniquely_explained_by_same_gene_parent
            and not independent_pairs
        )

        # Arm B is intentionally narrower than the older splice-length heuristics.  A small
        # competitor exon embedded within an otherwise fully explained query exon is overlap
        # context, but a competitor that supplies a complete query exon or junction is a
        # competing structural explanation and keeps the Parent alignment-only.
        competitor_specific_component_or_junction = any(
            gene != tx.gene
            and any(
                any(a <= start and end <= b for a, b in intervals)
                for start, end in ordered_exons
            )
            for gene, intervals in anchor_exons_by_gene.items()
        ) or competitor_junction_evidence
        exact_geometry_overlap_guard = (
            _RELATION_AWARE_POLICY == "host_readthrough_components_strong_chain"
            and source_reference_relation == "label_consistent"
            and uniquely_explained_by_same_gene_parent
            and bool(competitor_genes)
            and not independent_pairs
            and not assigned_is_true_readthrough
            and not source_is_true_readthrough
            and not competitor_specific_component_or_junction
        )
        current_proposed = ""
        count_gene = ""
        if _RELATION_AWARE_POLICY != "v3" and (
            assigned_is_true_readthrough or source_is_true_readthrough
        ):
            structural_class = "readthrough"
            category = "protected_true_readthrough_alignment_only"
            action = "tag_readthrough_alignment_only"
            count_role = "alignment_only"
            reason = "trusted_assigned_or_source_reference_readthrough_cannot_be_countable"
        elif _RELATION_AWARE_POLICY != "v3" and hosted_small_rna_component_guard:
            structural_class = "single_gene"
            category = "protected_ncrna_host_component"
            action = "retain_countable"
            count_role = "countable"
            count_gene = tx.gene
            reason = "trusted_ncrna_host_contains_only_small_rna_without_competing_junction"
        elif _RELATION_AWARE_POLICY != "v3" and readthrough_component_guard:
            structural_class = "single_gene"
            category = "protected_readthrough_component"
            action = "retain_countable"
            count_role = "countable"
            count_gene = tx.gene
            reason = "exact_same_gene_geometry_with_only_tagged_readthrough_competitors"
        elif exact_geometry_overlap_guard:
            structural_class = "single_gene"
            category = "protected_exact_geometry_genuine_overlap"
            action = "retain_countable"
            count_role = "countable"
            count_gene = tx.gene
            reason = "single_exact_same_gene_parent_covers_query_without_competitor_component_or_junction"
        elif independent_pairs:
            structural_class = "readthrough"
            category = "structural_readthrough"
            action = "tag_readthrough_alignment_only"
            count_role = "alignment_only"
            reason = "ordered_gene_specific_components_from_exonically_independent_local_loci"
        elif parent_scope == "ortholog" and count_decisive_genes - {tx.gene}:
            structural_class = "multigene_unresolved"
            category = "multigene_unresolved"
            action = "alignment_only"
            count_role = "alignment_only"
            reason = "ortholog_parent_has_additional_inseparable_local_gene_footprint"
        elif len(count_decisive_genes) >= 2 or len(count_identity_candidates) > 1:
            structural_class = "multigene_unresolved"
            category = "multigene_unresolved"
            action = "alignment_only"
            count_role = "alignment_only"
            reason = "multiple_local_genes_without_ordered_independent_components"
        elif count_identity_gene and count_decisive_genes - {count_identity_gene}:
            structural_class = "multigene_unresolved"
            category = "multigene_unresolved"
            action = "alignment_only"
            count_role = "alignment_only"
            reason = "local_structure_identity_disagrees_with_complete_parent_footprint"
        elif parent_scope == "ortholog":
            structural_class = "single_gene"
            category = "ortholog_input_consistent"
            action = "retain_countable"
            count_role = "countable"
            count_gene = tx.gene
            reason = "ortholog_parent_has_no_additional_local_gene_component"
        elif count_identity_gene == tx.gene:
            structural_class = "single_gene"
            category = "single_gene_structure_consistent"
            action = "retain_countable"
            count_role = "countable"
            count_gene = tx.gene
            reason = f"assigned_gene_matches_{identity_basis}"
        elif count_identity_gene and count_clean_identity_components and strong_identity:
            structural_class = "single_gene"
            category = "single_gene_reassignment"
            current_proposed = count_identity_gene
            action = "reassign_countable"
            count_role = "reassigned_countable"
            count_gene = count_identity_gene
            reason = f"unique_{identity_basis}_after_complete_parent_veto"
        elif count_identity_gene and count_clean_identity_components:
            structural_class = "identity_unresolved"
            category = "identity_candidate_reassignment"
            current_proposed = count_identity_gene
            action = "alignment_only"
            count_role = "alignment_only"
            reason = f"weak_{identity_basis}_cannot_authorize_reassignment"
        elif eligible_footprint_genes == {tx.gene}:
            structural_class = "single_gene"
            category = "single_gene_footprint_consistent"
            action = "retain_countable"
            count_role = "countable"
            count_gene = tx.gene
            reason = "only_assigned_gene_has_same_contig_exonic_footprint"
        elif len(eligible_footprint_genes) == 1:
            structural_class = "identity_unresolved"
            category = "label_conflict"
            action = "alignment_only"
            count_role = "alignment_only"
            reason = "single_local_footprint_disagrees_without_reassignment_structure"
        elif not footprint_genes and tx.gene in same_span:
            structural_class = "identity_unresolved"
            category = "nested_same_strand"
            action = "alignment_only"
            count_role = "alignment_only"
            reason = "no_exonic_anchor_but_assigned_gene_span_contains_parent"
        elif not footprint_genes:
            structural_class = "identity_unresolved"
            category = "candidate_novel_locus"
            action = "alignment_only"
            count_role = "alignment_only"
            reason = "no_same_strand_ortholog_exonic_anchor_on_this_assembly_contig"
        else:
            structural_class = "identity_unresolved"
            category = "identity_unresolved"
            action = "alignment_only"
            count_role = "alignment_only"
            reason = f"footprint_present_but_{identity_basis}"

        if count_role in {"countable", "reassigned_countable"} and not is_count_eligible(
            count_gene
        ):
            count_role = "alignment_only"
            count_gene = ""
            action = "alignment_only"
            reason = "assigned_or_reassigned_count_gene_outside_eligible_count_universe"

        prior = _PRIOR.get((segment.contig, parent), {})
        prior_relation = _prior_relation(prior, category, current_proposed)
        evidence_limit = _MAX_EVIDENCE_GENES
        footprint_serialized = (
            ",".join(
                f"{gene}:{shared}:{qfrac:.6f}:{afrac:.6f}:{relation}"
                for gene, shared, qfrac, afrac, relation in footprint_items[
                    :evidence_limit
                ]
            )
            or "."
        )
        pair_serialized = (
            ",".join(
                f"{a}|{b}|{relation}"
                for a, b, relation in pair_topology[:evidence_limit]
            )
            or "."
        )
        row = {
            "schema_version": SCHEMA_VERSION,
            "contig": segment.contig,
            "sample": sample,
            "haplotype": haplotype,
            "parent": parent,
            "parent_scope": parent_scope,
            "transcript_class": tx.transcript_class,
            "assigned_gene": tx.gene or ".",
            "strand": tx.strand,
            "query_start_1based": query_union[0][0] + 1,
            "query_end_1based_inclusive": query_union[-1][1],
            "query_exons": len(tx.exons),
            "query_junctions": len(query_junctions),
            "query_exonic_bases": query_total,
            "source_reference_gene": source_reference_gene or ".",
            "source_reference_tags": _serialize_genes(source_reference_tags),
            "source_reference_relation": source_reference_relation,
            "prior_verdict": prior.get("verdict", "."),
            "prior_resolved_by": prior.get("resolved_by", "."),
            "prior_proposed_gene": prior.get("proposed_gene", ".") or ".",
            "prior_chain_verdict": prior.get("chain_verdict", "."),
            "prior_evidence": prior.get("evidence", ".") or ".",
            "local_exact_chain_genes": _serialize_genes(local_exact),
            "local_query_subchain_genes": _serialize_genes(local_query_subchain),
            "local_ortholog_subchain_genes": _serialize_genes(local_ortholog_subchain),
            "local_all_query_junction_genes": _serialize_genes(local_all_query),
            "local_gene_unique_junction_counts": _serialize_counts(
                unique_junction_counts
            ),
            "reference_exact_chain_genes": _serialize_genes(reference_exact),
            "reference_ngram_votes": _serialize_counts(ngram_votes, evidence_limit),
            "identity_basis": identity_basis,
            "identity_candidates": _serialize_genes(identity_candidates),
            "identity_gene": identity_gene or ".",
            "footprint_gene_count": len(footprint_genes),
            "footprint_genes": _serialize_genes(footprint_genes),
            "footprint_evidence": footprint_serialized,
            "same_strand_span_context": _serialize_genes(same_span),
            "antisense_span_context": _serialize_genes(antisense_span),
            "complete_ortholog_model_genes": _serialize_genes(complete_model_genes),
            "gene_pair_topology": pair_serialized,
            "intergene_exon_bridges": ",".join(
                f"{a}|{b}" for a, b in sorted(bridge_pairs)
            )
            or ".",
            "local_component_genes": _serialize_genes(component_genes),
            "local_component_order": ">".join(component_order) or ".",
            "local_ambiguous_component_genes": _serialize_genes(
                ambiguous_component_genes
            ),
            "local_independent_component_pairs": ",".join(
                f"{a}|{b}" for a, b in sorted(independent_pairs)
            )
            or ".",
            "eligible_footprint_genes": _serialize_genes(eligible_footprint_genes),
            "ineligible_footprint_genes": _serialize_genes(ineligible_footprint_genes),
            "eligible_decisive_genes": _serialize_genes(eligible_decisive_genes),
            "ineligible_decisive_genes": _serialize_genes(ineligible_decisive_genes),
            "structural_class": structural_class,
            "count_role": count_role,
            "count_gene": count_gene or ".",
            "current_category": category,
            "current_proposed_gene": current_proposed or ".",
            "current_action": action,
            "category_reason": reason,
            "prior_current_relation": prior_relation,
        }
        for key, value in tx.metadata.items():
            row[key] = value or "."
        line = _tsv_line(row)
        row_lines.append(line)
        category_counts[category] += 1
        scope_counts[parent_scope] += 1
        count_role_counts[count_role] += 1

        involved_genes = (
            {tx.gene, canonical_gene(prior.get("proposed_gene", "")), identity_gene}
            | identity_candidates
            | footprint_genes
            | component_genes
        )
        matched_focus = sorted((_FOCUS_GENES & involved_genes) - {""})
        if matched_focus:
            focus_lines.append(line)
            for gene in matched_focus:
                focus_counts[
                    (gene, prior.get("verdict", "."), category, action, count_role)
                ] += 1

    return ContigResult(
        ordinal=segment.ordinal,
        rows="".join(row_lines),
        focus_rows="".join(focus_lines),
        category_counts=dict(category_counts),
        focus_counts=dict(focus_counts),
        scope_counts=dict(scope_counts),
        count_role_counts=dict(count_role_counts),
        row_count=len(row_lines),
        focus_row_count=len(focus_lines),
    )


def deterministic_gzip_writer(path: Path):
    raw = open(path, "wb")
    compressed = gzip.GzipFile(filename="", mode="wb", fileobj=raw, mtime=0)
    text = io.TextIOWrapper(compressed, encoding="utf-8", newline="")
    return raw, compressed, text


def project_parent_roles_to_graph_paths(
    ledger_path: str,
    detailed_path: Path,
    output_path: Path,
    reference: ReferenceIndex,
    eligible_count_genes: frozenset[str] | None = None,
) -> dict[str, object]:
    """Project Parent decisions onto graph paths without collapsing coordinate systems.

    CAT-derived paths inherit the decision made from their actual assembly-contig Parent,
    not from the Parent's reference source transcript.  Ledger-only paths (notably direct
    CHM13 paths) have no CAT Parent to inspect; only those paths use their exact reference
    transcript identity as a fail-closed fallback.
    """

    required = {
        "vg_path_name",
        "source_path_or_contig",
        "input_parent",
        "source_parent",
        "canonical_transcript",
        "source_transcript",
        "gene_id",
        "transcript_class",
        "feature_layer",
    }
    ledger_parent_keys: set[tuple[str, str]] = set()
    ledger_rows = 0
    with open_text_auto(ledger_path) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        missing = required - set(reader.fieldnames or ())
        if missing:
            raise ValueError(
                f"path identity ledger missing columns: {', '.join(sorted(missing))}"
            )
        for row in reader:
            ledger_rows += 1
            contig = row["source_path_or_contig"]
            for parent_column in ("input_parent", "source_parent"):
                parent = row[parent_column]
                if contig and parent:
                    ledger_parent_keys.add((contig, parent))

    parent_policy: dict[tuple[str, str], dict[str, str]] = {}
    with gzip.open(detailed_path, "rt") as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        required_parent = {
            "contig",
            "parent",
            "structural_class",
            "current_category",
            "count_role",
            "count_gene",
            "category_reason",
        }
        missing = required_parent - set(reader.fieldnames or ())
        if missing:
            raise ValueError(
                f"detailed Parent report missing columns: {', '.join(sorted(missing))}"
            )
        for row in reader:
            key = (row["contig"], row["parent"])
            if key not in ledger_parent_keys:
                continue
            if key in parent_policy:
                raise ValueError(f"duplicate detailed Parent key: {key[0]} {key[1]}")
            parent_policy[key] = {
                column: row[column]
                for column in (
                    "parent",
                    "structural_class",
                    "current_category",
                    "count_role",
                    "count_gene",
                    "category_reason",
                )
            }

    raw, compressed, text = deterministic_gzip_writer(output_path)
    role_counts: collections.Counter[str] = collections.Counter()
    category_counts: collections.Counter[str] = collections.Counter()
    match_counts: collections.Counter[str] = collections.Counter()
    seen_paths: set[str] = set()
    try:
        text.write("\t".join(PATH_POLICY_COLUMNS) + "\n")
        with open_text_auto(ledger_path) as handle:
            for ledger in csv.DictReader(handle, delimiter="\t"):
                path_name = ledger["vg_path_name"]
                if path_name in seen_paths:
                    raise ValueError(
                        f"duplicate graph path in identity ledger: {path_name}"
                    )
                seen_paths.add(path_name)
                contig = ledger["source_path_or_contig"]
                matches: list[tuple[str, dict[str, str]]] = []
                for match_label, parent_column in (
                    ("input_parent", "input_parent"),
                    ("source_parent", "source_parent"),
                ):
                    parent = ledger[parent_column]
                    policy = parent_policy.get((contig, parent))
                    if policy is not None and all(
                        policy is not existing for _, existing in matches
                    ):
                        matches.append((match_label, policy))
                if len(matches) > 1:
                    distinct = {
                        (
                            policy["parent"],
                            policy["current_category"],
                            policy["count_role"],
                            policy["count_gene"],
                        )
                        for _, policy in matches
                    }
                    if len(distinct) > 1:
                        raise ValueError(
                            f"conflicting Parent policies for graph path: {path_name}"
                        )

                ledger_gene = canonical_gene(ledger["gene_id"])
                source_transcript = canonical_gene(ledger["source_transcript"])
                source_gene = reference.transcript_gene.get(source_transcript, "")
                source_tags = reference.transcript_tags.get(
                    source_transcript, frozenset()
                )
                if not source_transcript or source_transcript == "N/A":
                    source_relation = "not_available"
                elif not source_gene:
                    source_relation = "source_transcript_not_in_reference"
                elif source_gene == ledger_gene:
                    source_relation = "label_consistent"
                else:
                    source_relation = "label_conflict"

                if matches:
                    match_label, policy = matches[0]
                    parent_match_status = f"matched_{match_label}"
                    matched_parent = policy["parent"]
                    policy_basis = "assembly_contig_parent_structure"
                    structural_class = policy["structural_class"]
                    category = policy["current_category"]
                    count_role = policy["count_role"]
                    count_gene = policy["count_gene"]
                    reason = f"parent_policy:{policy['category_reason']}"
                    if (
                        eligible_count_genes is not None
                        and count_role in {"countable", "reassigned_countable"}
                        and count_gene not in eligible_count_genes
                    ):
                        count_role = "alignment_only"
                        count_gene = "."
                        reason = "parent_count_gene_outside_eligible_count_universe"
                else:
                    parent_match_status = "ledger_only"
                    matched_parent = "."
                    policy_basis = "direct_reference_source_fallback"
                    structural_class = "reference_identity"
                    if "readthrough_transcript" in source_tags:
                        category = "reference_readthrough_source"
                        count_role = "alignment_only"
                        count_gene = "."
                        reason = "exact_reference_source_transcript_has_readthrough_tag"
                    elif source_gene and source_gene != ledger_gene:
                        category = "reference_source_label_conflict"
                        count_role = "alignment_only"
                        count_gene = "."
                        reason = "ledger_gene_disagrees_with_exact_reference_source_transcript"
                    elif source_gene and (
                        eligible_count_genes is None
                        or ledger_gene in eligible_count_genes
                    ):
                        category = "reference_source_consistent"
                        count_role = "countable"
                        count_gene = ledger_gene
                        reason = "ledger_gene_matches_exact_reference_source_transcript"
                    elif source_gene:
                        category = "reference_source_outside_eligible_count_universe"
                        count_role = "alignment_only"
                        count_gene = "."
                        reason = "ledger_gene_outside_eligible_count_universe"
                    else:
                        structural_class = "identity_unresolved"
                        category = "unresolved_source_identity"
                        count_role = "alignment_only"
                        count_gene = "."
                        reason = "no_CAT_parent_or_exact_reference_source_identity"

                output = {
                    "schema_version": PATH_POLICY_SCHEMA_VERSION,
                    "vg_path_name": path_name,
                    "source_path_or_contig": contig,
                    "input_parent": ledger["input_parent"],
                    "source_parent": ledger["source_parent"],
                    "canonical_transcript": ledger["canonical_transcript"] or ".",
                    "source_transcript": ledger["source_transcript"] or ".",
                    "transcript_class": ledger["transcript_class"] or ".",
                    "feature_layer": ledger["feature_layer"] or ".",
                    "ledger_gene": ledger_gene or ".",
                    "parent_match_status": parent_match_status,
                    "matched_parent": matched_parent,
                    "policy_basis": policy_basis,
                    "source_reference_gene": source_gene or ".",
                    "source_reference_tags": _serialize_genes(source_tags),
                    "source_reference_relation": source_relation,
                    "parent_structural_class": structural_class,
                    "parent_current_category": category,
                    "count_role": count_role,
                    "count_gene": count_gene or ".",
                    "policy_reason": reason,
                }
                text.write(_selected_tsv_line(output, PATH_POLICY_COLUMNS))
                role_counts[count_role] += 1
                category_counts[category] += 1
                match_counts[parent_match_status] += 1
    finally:
        text.close()
        if not compressed.closed:
            compressed.close()
        if not raw.closed:
            raw.close()

    if len(seen_paths) != ledger_rows:
        raise RuntimeError(
            f"path policy row count changed: expected {ledger_rows}, observed {len(seen_paths)}"
        )
    return {
        "ledger_rows": ledger_rows,
        "unique_paths": len(seen_paths),
        "referenced_parent_keys": len(ledger_parent_keys),
        "matched_parent_keys": len(parent_policy),
        "parent_match_statuses": dict(sorted(match_counts.items())),
        "count_roles": dict(sorted(role_counts.items())),
        "categories": dict(sorted(category_counts.items())),
    }


def write_sha256s(out_dir: Path, names: Sequence[str]) -> None:
    lines = [f"{sha256_file(str(out_dir / name))}  {name}\n" for name in names]
    (out_dir / "SHA256SUMS").write_text("".join(lines))


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("reference", help="reference GFF3/GTF used by CAT")
    parser.add_argument("cat", help="plain, seekable, seqid-grouped CAT exon GFF3")
    parser.add_argument("out_dir", help="new output directory")
    parser.add_argument(
        "--prior-report",
        help="optional archived-classifier report used only for comparison columns, never decisions",
    )
    parser.add_argument(
        "--path-identity-ledger",
        help=(
            "optional graph path identity ledger; emits one path_count_policy.tsv.gz row "
            "per ledger path after Parent classification"
        ),
    )
    parser.add_argument(
        "--eligible-count-genes",
        help=(
            "optional one-column stable-gene allowlist; comments are ignored and only "
            "eligible genes can receive countable roles"
        ),
    )
    parser.add_argument("--workers", type=int, default=min(8, os.cpu_count() or 1))
    parser.add_argument(
        "--ngram",
        type=int,
        default=5,
        help="reference intron lengths per fallback signature",
    )
    parser.add_argument("--min-ngram-support", type=int, default=1)
    parser.add_argument("--max-evidence-genes", type=int, default=20)
    parser.add_argument(
        "--relation-aware-policy",
        "--relation-policy",
        dest="relation_aware_policy",
        default="v3",
        choices=(
            "v3",
            "host_readthrough_components",
            "host_readthrough_components_strong_chain",
        ),
        help=(
            "opt-in relation-aware protection: v3 preserves existing behavior; "
            "host_readthrough_components protects separately validated ncRNA-host and "
            "readthrough-component Parents; the strong_chain mode additionally requires "
            "one exact-junction same-gene Parent covering every query exon base before "
            "protecting a dense genuine overlap"
        ),
    )
    parser.add_argument(
        "--focus-gene", action="append", default=[], help="stable gene ID; repeatable"
    )
    args = parser.parse_args(argv)
    if args.workers < 1:
        parser.error("--workers must be at least 1")
    if args.ngram < 1:
        parser.error("--ngram must be at least 1")
    if args.min_ngram_support < 1:
        parser.error("--min-ngram-support must be at least 1")
    if args.max_evidence_genes < 1:
        parser.error("--max-evidence-genes must be at least 1")
    return args


def main(argv: Sequence[str] | None = None) -> int:
    global \
        _REFERENCE, \
        _PRIOR, \
        _CAT_PATH, \
        _FOCUS_GENES, \
        _NGRAM, \
        _MIN_NGRAM_SUPPORT, \
        _MAX_EVIDENCE_GENES, \
        _RELATION_AWARE_POLICY, \
        _ELIGIBLE_COUNT_GENES
    args = parse_args(argv)
    out_dir = Path(args.out_dir).resolve()
    out_dir.mkdir(parents=True, exist_ok=True)
    expected = [
        "detailed_assignment.tsv.gz",
        "focus_evidence.tsv.gz",
        "focus_gene_summary.tsv",
        "category_summary.tsv",
        "RUN_MANIFEST.json",
        "SHA256SUMS",
        "STATUS",
    ]
    if args.path_identity_ledger:
        expected.append("path_count_policy.tsv.gz")
    present = [name for name in expected if (out_dir / name).exists()]
    if present:
        raise FileExistsError(
            f"refusing to overwrite existing outputs in {out_dir}: {', '.join(present)}"
        )

    print(f"indexing reference: {args.reference}", file=sys.stderr)
    _REFERENCE = build_reference_index(args.reference, args.ngram)
    print(
        f"reference transcripts={_REFERENCE.transcript_count:,} exact_chains={len(_REFERENCE.exact_chain_genes):,} "
        f"{args.ngram}-intron_signatures={len(_REFERENCE.ngram_genes):,}",
        file=sys.stderr,
    )
    if args.prior_report:
        print(f"loading optional prior report: {args.prior_report}", file=sys.stderr)
        _PRIOR = load_prior_report(args.prior_report)
        print(f"prior rows={len(_PRIOR):,}", file=sys.stderr)
    else:
        _PRIOR = {}
        print("prior report omitted; comparison columns will be '.'", file=sys.stderr)
    print(f"indexing CAT contig byte ranges and hashing: {args.cat}", file=sys.stderr)
    segments, cat_content_sha256 = index_cat_contigs(args.cat)
    seqid_blocks = sum(len(segment.ranges) for segment in segments)
    repeated_seqids = sum(len(segment.ranges) > 1 for segment in segments)
    print(
        f"CAT contigs={len(segments):,} seqid_blocks={seqid_blocks:,} "
        f"repeated_seqids={repeated_seqids:,}",
        file=sys.stderr,
    )

    _CAT_PATH = str(Path(args.cat).resolve())
    _FOCUS_GENES = frozenset(
        canonical_gene(gene) for gene in args.focus_gene if canonical_gene(gene)
    )
    _NGRAM = args.ngram
    _MIN_NGRAM_SUPPORT = args.min_ngram_support
    _MAX_EVIDENCE_GENES = args.max_evidence_genes
    _RELATION_AWARE_POLICY = args.relation_aware_policy
    _ELIGIBLE_COUNT_GENES = (
        load_eligible_count_genes(args.eligible_count_genes)
        if args.eligible_count_genes
        else None
    )

    detailed_tmp = out_dir / ".detailed_assignment.tsv.gz.partial"
    focus_tmp = out_dir / ".focus_evidence.tsv.gz.partial"
    detailed_raw, detailed_gz, detailed_text = deterministic_gzip_writer(detailed_tmp)
    focus_raw, focus_gz, focus_text = deterministic_gzip_writer(focus_tmp)
    category_counts: collections.Counter[str] = collections.Counter()
    focus_counts: collections.Counter[tuple[str, str, str, str, str]] = (
        collections.Counter()
    )
    scope_counts: collections.Counter[str] = collections.Counter()
    count_role_counts: collections.Counter[str] = collections.Counter()
    total_rows = focus_rows = 0
    try:
        header = "\t".join(OUTPUT_COLUMNS) + "\n"
        detailed_text.write(header)
        focus_text.write(header)
        if sys.platform != "linux" and args.workers > 1:
            raise RuntimeError("multi-worker execution requires Linux fork semantics")
        if args.workers == 1:
            results = map(_analyze_segment, segments)
            pool = None
        else:
            context = mp.get_context("fork")
            pool = context.Pool(processes=args.workers)
            results = pool.imap(_analyze_segment, segments, chunksize=1)
        failed = False
        try:
            for completed, result in enumerate(results, start=1):
                detailed_text.write(result.rows)
                focus_text.write(result.focus_rows)
                category_counts.update(result.category_counts)
                focus_counts.update(result.focus_counts)
                scope_counts.update(result.scope_counts)
                count_role_counts.update(result.count_role_counts)
                total_rows += result.row_count
                focus_rows += result.focus_row_count
                if completed % 25 == 0 or completed == len(segments):
                    print(
                        f"completed_contigs={completed:,}/{len(segments):,} rows={total_rows:,} focus_rows={focus_rows:,}",
                        file=sys.stderr,
                    )
        except BaseException:
            failed = True
            if pool is not None:
                pool.terminate()
            raise
        finally:
            if pool is not None:
                if not failed:
                    pool.close()
                pool.join()
    finally:
        detailed_text.close()
        focus_text.close()
        # TextIOWrapper closes the GzipFile and underlying raw stream.  These close calls are
        # intentionally idempotent and keep partially written files inspectable after failure.
        if not detailed_gz.closed:
            detailed_gz.close()
        if not detailed_raw.closed:
            detailed_raw.close()
        if not focus_gz.closed:
            focus_gz.close()
        if not focus_raw.closed:
            focus_raw.close()

    detailed_path = out_dir / "detailed_assignment.tsv.gz"
    focus_path = out_dir / "focus_evidence.tsv.gz"
    os.replace(detailed_tmp, detailed_path)
    os.replace(focus_tmp, focus_path)

    path_policy_results: dict[str, object] | None = None
    path_policy_path = out_dir / "path_count_policy.tsv.gz"
    if args.path_identity_ledger:
        print(
            f"projecting Parent roles to graph paths: {args.path_identity_ledger}",
            file=sys.stderr,
        )
        path_policy_results = project_parent_roles_to_graph_paths(
            args.path_identity_ledger,
            detailed_path,
            path_policy_path,
            _REFERENCE,
            _ELIGIBLE_COUNT_GENES,
        )
        print(
            f"path policy rows={path_policy_results['ledger_rows']:,}",
            file=sys.stderr,
        )

    with open(out_dir / "category_summary.tsv", "w", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(("current_category", "transcripts"))
        for category, count in sorted(
            category_counts.items(), key=lambda item: (-item[1], item[0])
        ):
            writer.writerow((category, count))
    with open(out_dir / "focus_gene_summary.tsv", "w", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(
            (
                "focus_gene",
                "prior_verdict",
                "current_category",
                "current_action",
                "count_role",
                "transcripts",
            )
        )
        for key, count in sorted(focus_counts.items()):
            writer.writerow((*key, count))

    manifest = {
        "schema_version": SCHEMA_VERSION,
        "created_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "inputs": {
            "reference": {
                "path": str(Path(args.reference).resolve()),
                "bytes": os.path.getsize(args.reference),
                "file_sha256": sha256_file(args.reference),
            },
            "cat": {
                "path": _CAT_PATH,
                "bytes": os.path.getsize(args.cat),
                "content_sha256": cat_content_sha256,
                "content_sha256_scope": "raw_file_bytes_plain_gff3",
            },
            "prior_report": (
                {
                    "path": str(Path(args.prior_report).resolve()),
                    "bytes": os.path.getsize(args.prior_report),
                    "file_sha256": sha256_file(args.prior_report),
                    "role": "comparison_only",
                }
                if args.prior_report
                else None
            ),
        },
        "parameters": {
            "workers": args.workers,
            "ngram": args.ngram,
            "min_ngram_support": args.min_ngram_support,
            "max_evidence_genes": args.max_evidence_genes,
            "relation_aware_policy": args.relation_aware_policy,
            "focus_genes": sorted(_FOCUS_GENES),
            "coordinate_policy": "assembly-qualified seqid local only; no cross-contig comparisons",
        },
        "reference": {
            "transcripts": _REFERENCE.transcript_count,
            "stable_transcript_gene_mappings": len(_REFERENCE.transcript_gene),
            "exact_chain_signatures": len(_REFERENCE.exact_chain_genes),
            "ngram_signatures": len(_REFERENCE.ngram_genes),
        },
        "results": {
            "contigs": len(segments),
            "seqid_blocks": seqid_blocks,
            "seqids_with_multiple_blocks": repeated_seqids,
            "detailed_rows": total_rows,
            "focus_rows": focus_rows,
            "parent_scopes": dict(sorted(scope_counts.items())),
            "count_roles": dict(sorted(count_role_counts.items())),
            "categories": dict(sorted(category_counts.items())),
        },
        "outputs": {
            "detailed_assignment.tsv.gz": sha256_file(str(detailed_path)),
            "focus_evidence.tsv.gz": sha256_file(str(focus_path)),
        },
    }
    if args.path_identity_ledger:
        manifest["inputs"]["path_identity_ledger"] = {
            "path": str(Path(args.path_identity_ledger).resolve()),
            "bytes": os.path.getsize(args.path_identity_ledger),
            "file_sha256": sha256_file(args.path_identity_ledger),
        }
        manifest["results"]["path_policy"] = path_policy_results
        manifest["outputs"]["path_count_policy.tsv.gz"] = sha256_file(
            str(path_policy_path)
        )
    if args.eligible_count_genes:
        eligible_path = Path(args.eligible_count_genes).resolve()
        manifest["inputs"]["eligible_count_genes"] = {
            "path": str(eligible_path),
            "bytes": eligible_path.stat().st_size,
            "file_sha256": sha256_file(str(eligible_path)),
            "genes": len(_ELIGIBLE_COUNT_GENES),
        }
    (out_dir / "RUN_MANIFEST.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n"
    )
    checksum_names = [
        "detailed_assignment.tsv.gz",
        "focus_evidence.tsv.gz",
        "focus_gene_summary.tsv",
        "category_summary.tsv",
        "RUN_MANIFEST.json",
    ]
    if args.path_identity_ledger:
        checksum_names.append("path_count_policy.tsv.gz")
    write_sha256s(out_dir, checksum_names)
    (out_dir / "STATUS").write_text(
        f"detailed_annotation_classification_complete\trows={total_rows}\tfocus_rows={focus_rows}\n"
    )
    print(f"complete: {out_dir}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
