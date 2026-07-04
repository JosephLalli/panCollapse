# panCollapse V1 Product Specification

This document is the canonical product contract. Implementation choices may evolve, but
changes to the behavior defined here require an explicit decision-log entry and human
approval.

The GAMP-to-RAD algorithm is defined by decision D048 and `docs/conversion-algorithm.md`.
Where mechanism text below predates D048, the algorithm doc governs.

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
- transcript compatibility from HST-path membership: a read is compatible with a transcript
  whose `vg rna` HST path crosses the read's aligned nodes;
- per-node scoring under vg's own alignment scheme, with the top HST score across all of a
  read's alignments plus ties selecting the winners;
- transcript-copy collapse implicit in HST path naming (haplotype copies collapse to one
  transcript ID);
- preservation of multimapping transcript equivalence classes;
- mapper-style, uncollated RAD output for alevin-fry;
- preservation of target-relative read orientation in RAD `dirs`;
- the full compatible transcript target set per read.

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
2. **Existing `.xg` graph** for the same graph/node-id space that produced the GAMP,
   exposing the `vg rna` HST paths used to read transcript compatibility.
3. **Transcript-to-gene map (t2g)** projecting transcript targets to genes and driving
   `tx2gene.tsv`.
4. **Output destination** for mapper-style uncollated RAD and associated metadata/logs.
5. **Raw molecule-identity lengths** for the cell barcode and UMI (`--raw-cb-length`,
   `--raw-umi-length`; Phase 2 defaults 16 and 12).

A GTF and `vg rna` build the annotated graph during reference/fixture creation only; they
are not runtime inputs, and there is no separate collapse manifest. GCSA/LCP and distance
indexes may be needed upstream to produce the GAMP with `vg mpmap`; once GAMP exists they are
not panCollapse inputs. The implementation must not require GFF3, GBZ as a substitute for
`.xg`, or a custom lookup index in V1.

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

Compatibility is HST-path membership: a read is compatible with a transcript when one of
that transcript's `vg rna` HST paths crosses a node the read aligns to. The HST path encodes
the transcript's spliced structure, so there is no runtime exon/intron or splice-junction
test.

panCollapse does not filter compatibility by library strand. It preserves the read alignment
orientation relative to each emitted target in RAD `dirs` (forward if the read runs along the
HST path, reverse if opposite), and alevin-fry applies expected-orientation filtering. If a
transcript's winning HSTs disagree on orientation, the majority of aligned bases decides.

## 8. Path and copy collapse

Transcript-copy collapse is implicit in HST naming: a read's winning HST haplotype copies of
one transcript collapse to one transcript ID. A transcript's score is the score of its best
HST and is never inflated by summing across copies. There is no runtime collapse manifest.

## 9. Alignment eligibility and scoring

Each aligned node is scored under vg's own alignment scheme, reproduced per node from the
`Mapping` edits (see `docs/conversion-algorithm.md`). A transcript's evidence is the score of
its best HST. The read's targets are the HSTs tied at the single top score pooled across all
of the read's alignments; lower-scoring HSTs are not emitted. RAD output is an unweighted
target set with no probabilistic weighting.

## 10. Emitted target set

panCollapse emits the full compatible transcript set per read — the winning HSTs collapsed to
unique transcript IDs. It does not prefilter to transcript- or gene-unique subsets, and the
active converter has no assignment-mode CLI.

## 11. Output

V1 emits mapper-style, uncollated RAD that can enter the standard sequence:

```text
alevin-fry generate-permit-list
alevin-fry collate
alevin-fry quant
```

Target IDs in RAD are the transcript IDs a read's winning HSTs collapse to. A standard
two-column transcript-to-gene map allows alevin-fry to produce the final cell-by-gene
matrix. V1 does not attach splicing-state labels.

Each emitted read record carries the raw cell barcode, raw UMI, compatible target IDs,
and one orientation value per target. Target IDs are indices into the RAD header target
dictionary, not genomic coordinates. Orientation values are target-level RAD metadata
consumed by alevin-fry's expected-orientation filtering.

The exact RAD header, record fields, orientation encoding, chunking, and metadata are
defined by the supported alevin-fry/libradicl baseline. Current active V1 execution is
single-threaded under D045, and `map.rad` is streamed to disk incrementally: records roll
into complete, self-describing chunks, and the writer seeks back to backpatch each chunk
header and the file-level `num_chunks` with their final values once known (D049). If future
supported execution modes include multiple worker threads or direct `vg mpmap` stdin
streaming, RAD and companion artifacts must remain byte-identical for identical inputs and
configuration.

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
  score, target-relative orientation, all-mode RAD assignment, and deferred assignment
  option failures are tested;
- performance is characterized on a bounded pilot;
- deterministic-output tests prove byte-identical artifacts across supported execution
  modes;
- no custom index or deferred splicing-state behavior has entered the implementation.
