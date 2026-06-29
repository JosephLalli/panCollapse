# Input and Output Contract

This document translates the product specification into interface obligations without
prescribing the code architecture.

## Input obligations

### GAMP

- Binary VG multipath alignment stream.
- Records for a read name are contiguous.
- The converter validates recurrence of a previously completed name.
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

Accepted coherent pairs:

```text
CB + UB   corrected values, preferred
CR + UR   raw values, fallback
```

Missing, malformed, or non-coherent pairs are skipped per read group by default and
counted. A strict CLI option makes those conditions fatal. If records sharing one read
name disagree on the selected CB or UMI, the run fails. Values are never silently
repaired or mixed.

## Output obligations

### RAD

- mapper-style, uncollated RAD;
- canonical transcript target dictionary;
- cell barcode and UMI tags in the schema expected by alevin-fry;
- complete eligible transcript target set after collapse and policy filtering;
- no transcript likelihood weights and no splicing-state target labels in V1;
- byte-identical output for identical inputs/configuration across supported thread counts.

### Companion artifacts

The final CLI should write or expose:

- target dictionary and transcript-to-gene mapping provenance;
- a run summary with counts from Section 12 of the product spec;
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
- conflicting selected CB or UMI values among records in one read group;
- unsupported global tag configuration when no explicit override can identify a coherent
  pair;
- traversal cap overflow.

Skippable per-read conditions are missing, malformed, or non-coherent CB/UMI annotations.
They must be counted and become fatal under strict tag handling.
