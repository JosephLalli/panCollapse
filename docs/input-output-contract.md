# Input and Output Contract

This document translates the product specification into interface obligations without
prescribing the code architecture.

## Input obligations

### GAMP

- Binary VG multipath alignment stream.
- Records for a read name are contiguous.
- The converter validates recurrence of a previously completed name.
- For RNA inputs in V1, the GAMP name field carries the observed raw molecule identity as
  `<original_read_name>_<raw_CB>_<raw_UMI>`.
- The raw barcode and UMI are parsed from the right side of the name; panCollapse does
  not correct either value.
- Parsed raw barcode and UMI values must match the configured barcode and UMI lengths.
  Phase 2 defaults are `--raw-cb-length 16` and `--raw-umi-length 12`.
- All alternatives for the read are considered before target eligibility is finalized.
- Complete traversal uses both ordinary `next` edges and scored `connection` arcs.
  Traversal score is the sum of selected subpath scores plus selected connection scores;
  ordinary `next` edges add no transition score.
- Complete traversal enumeration is bounded by `--max-traversals-per-read`; overflow is a
  hard failure, not a lossy skip.

### GTF

- GTF is the required annotation format.
- Gene, transcript, and exon identifiers must be configurable or mapped from standard GTF
  attributes.
- Introns are implied by ordered exon structures.
- Annotation inconsistencies must be detectable and reported.

### Existing VG artifacts

- Producing GAMP with `vg mpmap` is an upstream step. For RNA/splice-variation graphs,
  the expected mapper-side artifacts commonly include `graph.xg`, `graph.gcsa`/GCSA-LCP,
  and `graph.dist`, plus the emitted GAMP.
- V1 panCollapse consumes the name-grouped GAMP and an existing `.xg` coordinate graph for
  the same graph/node-id space.
- The `.xg` must expose visible source-coordinate paths through
  `PathPositionHandleGraph`.
- After GAMP exists, `.gcsa`, `.gcsa.lcp`, `.dist`, `.snarls`, minimizer indexes, GBZ,
  and GBWT are not panCollapse converter inputs in V1.
- GBZ/GBWT are not a V1 replacement for `.xg` unless a later decision proves equivalent
  path-position behavior and passes the same projection fixtures.
- V1 may not require building a panCollapse-specific persistent index.

### Collapse manifest

The manifest is a headered TSV with these fields:

```text
source_path_name    source_transcript_id    canonical_transcript_id    canonical_gene_id
```

Requirements:

- deterministic and one-to-one from `(source_path_name, source_transcript_id)` to
  canonical transcript;
- `source_path_name` must exactly match the relevant visible source-coordinate path in
  the graph/index and the GTF seqname used for annotation lookup;
- source paths may represent reference, haplotype, or copy coordinates, but V1 does not
  use GBZ/GBWT-only compressed paths or mature spliced transcript paths as the annotation
  coordinate basis;
- `source_transcript_id` must match the configured GTF transcript ID attribute;
- many source transcript identities may map to one canonical transcript;
- every emitted canonical transcript must map to exactly one gene;
- duplicate contradictory source rows are errors;
- one canonical transcript mapped to multiple genes is an error;
- identity mappings are supported only when explicitly listed;
- every compatible source identity must be covered; a missing row is a hard error.

### Barcode and UMI

The raw cell barcode and raw UMI come from the GAMP name field, not from GAMP annotation
tags. RNA names follow `<original_read_name>_<raw_CB>_<raw_UMI>`, parsed from the right.
Values are written to RAD as observed. panCollapse does not correct, permit-list, repair,
or mix barcode/UMI values.

Missing, malformed, or unsupported raw barcode/UMI values are skipped per read group by
default and counted. Length mismatch against the configured raw barcode or UMI length is
malformed. `--molecule-identity-failures skip|fail` controls this behavior; `skip` is
the default, while `fail` makes those conditions fatal.

## Output obligations

### RAD

- mapper-style, uncollated RAD;
- canonical transcript target dictionary;
- raw cell barcode and raw UMI encoded in the schema expected by alevin-fry;
- complete eligible transcript target set after collapse and score filtering, encoded as
  zero-based target IDs into the RAD header;
- one orientation value for every emitted target ID;
- no transcript likelihood weights and no splicing-state target labels in V1;
- byte-identical output for identical inputs/configuration across supported execution
  modes. The active converter is single-threaded under D045; multithreaded execution is
  deferred.
- streaming RAD-to-disk writing: `num_chunks = 0`, file-tag values, and complete chunks
  emitted incrementally to `map.rad` without retaining the whole RAD file in memory.
- GAMP read groups with only empty-subpath records are treated as unaligned input: they
  emit no RAD record and increment `unaligned_reads`.

Conceptually, each emitted read record contains `bc`, `umi`, `refs`, and `dirs`. `refs`
is the read's target compatibility set, not genomic coordinates. `dirs` is parallel to
`refs` and is consumed by alevin-fry's expected-orientation filtering. panCollapse
preserves the actual read alignment orientation relative to each emitted target; it does
not synthesize all-forward directions after a panCollapse-side strand filter. A read group
with mixed orientations for the same emitted target is dropped and counted in the current
implementation scope.

RAD output uses the all-compatible-target assignment behavior. `--assignment all`, when
present, is equivalent to the default. Transcript- or gene-uniqueness modes are deferred
outside the active GAMP-to-RAD converter and must not filter RAD records in V1.

### Companion artifacts

The final CLI should write or expose:

- configured raw barcode and UMI lengths, which determine RAD `cblen` and `ulen`
  (`16` and `12` by default in Phase 2);
- target dictionary and transcript-to-gene mapping provenance;
- a run summary with counts from Section 12 of the product spec, including groups dropped
  for mixed target-relative orientations and read groups skipped as unaligned;
- version/build information;
- explicit input file identities or checksums when practical;
- warnings and rejected-read reasons suitable for tests.

## Exit behavior

Hard failure is expected for:

- unreadable or incompatible inputs;
- GAMP name-grouping violations;
- contradictory collapse manifest entries or missing rows for compatible source
  identities;
- annotation/index identity mismatches that invalidate assignments;
- inability to encode standards-conformant RAD;
- traversal cap overflow.

Future stdin support, if approved, must accept the same binary GAMP stream semantics as
file input and preserve the same grouping and recurrence validation. A future stdout RAD
mode is not part of the active CLI; if approved later, stdout must contain only binary RAD
data and logs/progress must go to stderr.

Skippable per-read conditions are missing, malformed, or unsupported raw CB/UMI values in
the GAMP name field. They must be counted and become fatal under strict molecule-identity
handling. The stable counters are `raw_molecule_missing_groups`,
`raw_molecule_malformed_groups`, `raw_molecule_unsupported_groups`, and
`raw_molecule_skipped_groups`.

Unaligned GAMP read groups are not molecule-identity failures. If every record in the
group has zero subpaths, panCollapse emits no RAD record, counts the group under both
`no_compatible_transcript_groups` and `unaligned_reads`, and continues.
