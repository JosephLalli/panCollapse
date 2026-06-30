# panCollapse V1 Product Specification

This document is the canonical product contract. Implementation choices may evolve, but
changes to the behavior defined here require an explicit decision-log entry and human
approval.

## 1. Purpose

`panCollapse` converts VG multipath alignments from 10x 3′ single-cell or single-nucleus
RNA sequencing into transcript-target compatibility records in mapper-style, uncollated
RAD format. The RAD output is consumed by the ordinary alevin-fry permit-list, collation,
and quantification workflow, which ultimately emits gene-level expression values.

The project exists to preserve pangenome-aware alignment evidence while presenting common
single-cell quantifiers with the transcript-level target sets they expect.

## 2. V1 scope

V1 supports:

- 10x 3′ v2/v3-style experiments where the cDNA read is represented in GAMP;
- raw cell barcode and UMI retrieval from the GAMP name field;
- transcript compatibility inferred from both annotated exons and annotated introns;
- an explicit transcript/path-copy collapse manifest;
- score-based eligibility with tied-best behavior by default;
- preservation of multimapping transcript equivalence classes;
- mapper-style, uncollated RAD output for alevin-fry;
- preservation of target-relative read orientation in RAD `dirs`;
- selectable assignment policies: all, unique transcript, unique gene, and a mode intended
  to mirror STARsolo's default unique-gene behavior.

## 3. Explicit non-goals

V1 does not:

- classify molecules as spliced, unspliced, or ambiguous;
- create synthetic unspliced targets or a splici/USA reference;
- surject GAMP through a genomic or transcriptomic BAM intermediate;
- emit transcriptome BAM, BUS, or TCC output;
- automatically infer which copy in a CNV gene family is closest to GRCh38, T2T, or an
  annotated canonical copy;
- build a custom transcript-compatibility index;
- fetch, build, or vendor VG itself;
- define a general library-geometry framework beyond the initial 10x 3′ scope.

Potential V1.1–V3 extensions belong in `docs/roadmap.md` and must not leak into V1
acceptance criteria.

## 4. Inputs

The final V1 CLI must accept:

1. **Name-grouped GAMP** containing one or more multipath alignments per cDNA read.
2. **GTF annotation** defining genes, transcripts, exons, and implied introns.
3. **Existing `.xg` coordinate graph/index file** for the same graph used to create the
   GAMP, sufficient to interpret GAMP graph walks and relate them to the source coordinate
   paths needed for annotation lookup.
4. **Collapse manifest** mapping graph-/haplotype-/copy-specific source transcript
   identities to canonical transcript IDs and genes. Phase 0 resolves the source
   identity as a source coordinate path plus source transcript ID.
5. **Output destination** for mapper-style uncollated RAD and associated metadata/logs.
6. **Raw molecule-identity lengths** for the cell barcode and UMI. The CLI provides
   `--raw-cb-length` and `--raw-umi-length`; Phase 2 defaults are 16 and 12.

GCSA/LCP and distance indexes may be required upstream to produce the GAMP with
`vg mpmap`, commonly alongside the same `.xg` used by panCollapse. Once GAMP exists,
they are not V1 panCollapse inputs. The implementation must not require GFF3, GBZ as a
substitute for `.xg`, or a newly generated custom lookup index in V1.

## 5. Barcode and UMI source

Upstream FASTQ preparation writes the observed raw cell barcode and UMI into the
biological read name before alignment. panCollapse reads those uncorrected values from
the GAMP name field and writes them to RAD:

- RNA read-name convention: `<original_read_name>_<raw_CB>_<raw_UMI>`.
- The barcode and UMI are parsed from the right side of the name so the original read
  name may contain underscores.
- Parsed raw barcode and UMI values must match the configured barcode and UMI lengths.
- panCollapse does not correct cell barcodes or UMIs and does not build a permit list.
- alevin-fry performs permit-list construction and cell-barcode correction, followed by
  UMI deduplication/resolution during quantification.
- missing, malformed, or unsupported raw barcode/UMI fields are reported in diagnostics;
- `--molecule-identity-failures skip|fail` controls whether those conditions are skipped
  and counted or treated as hard failures; the default is `skip`.

## 6. Read grouping

All serialized GAMP records belonging to one read name must be adjacent. The V1 tool must
stream groups, validate that a completed read name does not recur later, and fail clearly
when the grouping contract is violated.

Whether `vg mpmap` already guarantees this grouping, and the supported remedy when it
does not, must be established during Phase 0. General external sorting is not silently
included in the converter's scope.

## 7. Transcript compatibility

Compatibility is an annotation-aware property of a candidate GAMP traversal and a
transcript. It is not limited to sequence compatibility with a mature spliced transcript
path.

A transcript may be compatible when the read evidence is:

- entirely exonic for that transcript;
- entirely within an annotated intron of that transcript;
- unspliced across an exon–intron or intron–exon boundary;
- exonic for one isoform and intronic for another, in which case both may be compatible.

If the GAMP traversal contains an observed splice junction, that junction must be present
in the candidate transcript's annotated exon structure for the transcript to remain
compatible.

A transcript requires at least one aligned reference-consuming base overlapping one of
its annotated exons or implied introns. Do not reject that transcript solely because other
aligned sequence extends beyond its first-to-last-exon span. Dedicated fixtures must
prevent the implementation from silently substituting either strict containment or mere
parent-gene-locus overlap.

panCollapse does not filter transcript compatibility by library strand and does not infer
library chemistry. For every emitted target, it preserves the read alignment orientation
relative to that target/transcript in RAD `dirs`. Downstream alevin-fry expected-
orientation settings handle library-orientation filtering.

If one read group contributes mixed target-relative orientations for the same emitted
target in the current implementation scope, panCollapse drops that read group and reports
the condition instead of assigning a synthetic direction.

## 8. Path and copy collapse

V1 uses an explicit deterministic manifest. At minimum it must establish:

- source graph/haplotype/copy coordinate path identity;
- source transcript identity on that path;
- canonical transcript target identity;
- canonical gene identity.

Identity mappings are valid when explicitly listed. Multiple source transcript identities
may collapse to one canonical transcript. The best score for a collapsed target is the
maximum score observed among its compatible source members; scores are not summed across
copies or haplotypes.

Every compatible source identity must have an explicit manifest row. Missing coverage is
a hard error; there is no implicit identity fallback. The manifest is applied before
transcript- or gene-level uniqueness is evaluated.

For V1 compatibility evaluation, the source path is a visible source-coordinate path used
by the GTF, not a mature spliced transcript path by itself. The transcript model is
selected by the source transcript identity on that path.

## 9. Alignment eligibility and scoring

For each read group, the implementation must derive a best score for each canonical
transcript target from complete compatible candidate traversals.

Default eligibility is tied-best:

```text
retain target when read_best_score - target_best_score == 0
```

An advanced nonnegative score-window option must allow:

```text
retain target when read_best_score - target_best_score <= N
```

Phase 0 defines complete GAMP traversal scores as the sum of selected subpath scores plus
selected connection scores, with ordinary `next` edges adding no transition score. No
probabilistic weighting is required in V1, and RAD output is an unweighted target set.

## 10. Assignment policies

Eligibility filtering and copy collapse happen before assignment-policy evaluation.

- `all` — default. Emit every eligible canonical transcript target.
- `unique-transcript` — emit only when exactly one eligible canonical transcript remains.
- `unique-gene` — emit when all eligible canonical transcripts map to exactly one gene;
  preserve all eligible transcripts from that gene in the RAD record.
- `starsolo-default` — exact alias for `unique-gene` in V1. After score filtering and
  copy collapse, retain the read when all eligible canonical transcripts belong to one
  gene, preserving all eligible transcripts from that gene in the RAD record.

A read that has many graph paths, haplotype paths, or transcript isoforms but only one
canonical gene is gene-unique. It must not be discarded merely because the pangenome
represents equivalent evidence multiple ways.

## 11. Output

V1 emits mapper-style, uncollated RAD that can enter the standard sequence:

```text
alevin-fry generate-permit-list
alevin-fry collate
alevin-fry quant
```

Target IDs in RAD are canonical transcript IDs from the collapse manifest. A standard
two-column transcript-to-gene map allows alevin-fry to produce the final cell-by-gene
matrix. V1 does not attach splicing-state labels.

Each emitted read record carries the raw cell barcode, raw UMI, compatible target IDs,
and one orientation value per target. Target IDs are indices into the RAD header target
dictionary, not genomic coordinates. Orientation values are target-level RAD metadata
consumed by alevin-fry's expected-orientation filtering.

The exact RAD header, record fields, orientation encoding, chunking, and metadata are
external contracts to verify from current primary sources during Phase 0. For identical
inputs and configuration, RAD and companion artifacts must be byte-identical regardless
of thread count.

## 12. Diagnostics

The final tool must provide machine-readable or clearly parseable summary counts for at
least:

- input records and read groups;
- read groups skipped for missing, malformed, or unsupported raw CB/UMI values parsed
  from the GAMP name field;
- `raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
  `raw_molecule_unsupported_groups`, and `raw_molecule_skipped_groups`;
- grouping violations;
- groups with no compatible transcript;
- groups dropped because one emitted target has mixed target-relative orientations;
- compatible targets removed by score-window filtering (`score_removed_targets`);
- groups removed by each uniqueness policy;
- groups emitted and number of targets per emitted group;
- manifest misses and annotation/index consistency failures.

Errors affecting input interpretation must fail loudly rather than silently changing the
assignment model.

## 13. Performance constraints

V1 should be stream-oriented over name-grouped GAMP. It may build ordinary in-memory GTF
structures and use existing VG indexes, but it must not introduce a new persistent custom
index.

If direct lookup proves too slow, record profiling evidence and a proposed custom-index
design as a future development item. Implementation of that index requires a separate
human-approved decision.

## 14. Acceptance boundary

The V1 product is complete only when:

- all behavioral fixtures in `docs/validation-contract.md` pass;
- a tiny generated RAD file is accepted by the current supported alevin-fry workflow;
- the output gene matrix matches the exact expected result;
- name-grouping, raw molecule-identity parsing, configured-length mismatch, collapse,
  score, target-relative orientation, and assignment-policy failures are tested;
- performance is characterized on a bounded pilot;
- deterministic-output tests prove byte-identical artifacts across supported thread
  counts;
- no custom index or deferred splicing-state behavior has entered the implementation.
