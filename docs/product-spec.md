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

Version 0.10 additionally defines a native count surface. It consumes a verified
`panSC-count-facts-v1` bundle rather than independent annotation/policy flags and emits
Parquet count tables by default; 10x MEX and RAD are optional compatibility outputs. The
legacy conversion contract below remains supported independently. An opt-in
`--read-assignments-out` emits ordered compressed Parquet diagnostics for audit; it is not
enabled by default because it is per-read-scale I/O.

`count --compatibility-out <dir>` may instead publish a reusable, non-BAM tiered
read-compatibility intermediate. It is policy-neutral evidence before
`CountFactCatalog::candidate`, not final `AssignmentFacts`, an assignment result, or a count
output. `count --compatibility-in <dir>` replays that evidence with the current count-facts
bundle and whitelist. One invocation accepts exactly one source: GAMP plus its matching XG, or
the compatibility intermediate. This avoids duplicating alignments while retaining per-read
molecule status, interned fact-set identity, exact E/P/B and structural S/U
provenance. It supports lossless metadata, tagging, whitelist, and count-policy re-evaluation
while the ledger transcript/exon surface and the compatibility algorithm are unchanged. Source
GAMP/XG digests are frozen in its manifest; replay intentionally does not reopen XG, so any graph
change requires GAMP-derived regeneration.

The project exists to preserve pangenome-aware alignment evidence while either presenting common
single-cell quantifiers with the transcript-level target sets they expect (`convert`) or reducing
that evidence directly to gene/UMI counts without a BAM boundary (`count`).

## 2. V1 scope

V1 supports:

- 10x 3′ v2/v3-style experiments where the cDNA read is represented in GAMP;
- raw cell barcode and UMI retrieval from the GAMP name field;
- transcript compatibility from HST-path membership: a read is compatible with a transcript
  whose `vg rna` HST path crosses the read's aligned nodes;
- per-node scoring under vg's own alignment scheme, with the top HST score across all of a
  read's alignments plus ties selecting the winners;
- transcript-copy collapse either implicit in conventional HST path naming or explicit in
  optional t2g column 3 (haplotype paths collapse to one canonical transcript ID);
- preservation of multimapping transcript equivalence classes;
- mapper-style, uncollated RAD output for alevin-fry;
- preservation of target-relative read orientation in RAD `dirs`;
- the full compatible transcript target set per read;
- native one-pass execution of one or more immutable count profiles, initially `cr7-v1` and
  `pansc-strict-v1`;
- default compact Parquet gene-count and molecule outputs, with optional raw 10x MEX, RAD, and
  per-read diagnostic Parquet.

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

Native v0.10 count specifically does not call cells, model ambient RNA, normalize counts, write a
BAM, or use Python at production runtime. Its `cr7-v1` result is “CR7 emulation on PanCollapse
alignments,” not a claim of complete Cell Ranger equivalence: mapper and graph-reference choices
remain different.

## 4. Inputs

The final V1 CLI must accept:

1. **Name-grouped GAMP** containing one or more multipath alignments per cDNA read.
2. **Existing `.xg` graph** for the same graph/node-id space that produced the GAMP,
   exposing the `vg rna` HST paths used to read transcript compatibility.
3. **Path identity ledger** (`--path-identity-ledger`, schema `panSC-path-identity-v1`)
   projecting exact graph paths through unique annotation Parents to canonical transcripts and
   genes, and driving `tx2gene.tsv`.
4. **Output destination** for mapper-style uncollated RAD and associated metadata/logs.
5. **Raw molecule-identity lengths** for the cell barcode and UMI (`--raw-cb-length`,
   `--raw-umi-length`; Phase 2 defaults 16 and 12).

A GTF and `vg rna` build the annotated graph during reference/fixture creation only; they
are not runtime inputs, and there is no separate collapse manifest. GCSA/LCP and distance
indexes may be needed upstream to produce the GAMP with `vg mpmap`; once GAMP exists they are
not panCollapse inputs. The implementation must not require GFF3, GBZ as a substitute for
`.xg`, or a custom lookup index in V1.

Historical two- and three-column exon/body t2gs are accepted only through the explicit
`--legacy-adapter hst-v1`; they are never inferred from file shape. The production ledger carries
both exon and body rows. Ordinary ledger modes keep S/U evidence transcript-specific.
`genefull_ex50pas` instead emits exact Parent-preserving E/P/B evidence and requires
`--bam-out --bam-multigene all`; it rejects `hst-v1`. See `docs/genefull.md`.

For native `count`, the runtime identity inputs are instead a checksum-bound
`panSC-count-facts-v1` bundle, a barcode whitelist, and one or more profile selectors. The bundle
binds the path-identity ledger, per-profile gene policy, Parent categories, projected-nesting
relations, independent-support facts, gene metadata, producer receipts, and source hashes.
Optional `--t2g` and `--body-t2g` are assertions against that bundle and never participate in
assignment. Incompatible or incomplete facts fail before GAMP consumption.

## 5. Barcode and UMI source

Upstream FASTQ preparation writes the observed raw cell barcode and UMI into the
biological read name before alignment. panCollapse reads those uncorrected values from
the GAMP name field. `convert` writes them to RAD or optional BAM; `count` consumes them in its
native barcode/UMI engine:

- RNA read-name convention:
  `<original_read_name>_<raw_CB>_<raw_UMI>_cy<hex(raw_barcode_quality)>_uy<hex(raw_UMI_quality)>`.
- Barcode, UMI, and optional qualities are parsed from the right side of the name so the original read
  name may contain underscores.
- Legacy quality-free and CY-only names remain accepted; absent qualities cannot be emitted.
- Parsed raw barcode and UMI values must match the configured barcode and UMI lengths.
- `convert` does not correct cell barcodes or UMIs and does not build a permit list; alevin-fry or
  a legacy BAM counter owns those operations downstream.
- `count` builds the exact-whitelist read prior from every valid group, including featureless
  groups, applies the frozen one-mismatch quality posterior, and then runs the frozen UMI filters,
  non-transitive one-mismatch collapse, and cross-gene maximum-support/tie-discard rule.
- missing, malformed, or unsupported raw barcode/UMI/quality fields are reported in diagnostics;
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

Production transcript-copy collapse follows the exact ledger chain
`vg_path_name -> unique_parent -> canonical_transcript -> gene_id`. Paths MAX-collapse within a
Parent, then Parents MAX-collapse within a canonical transcript; tied winning paths and Parents
are retained as provenance, never summed. Path names are opaque: a literal `_R1` is not stripped.
The XG must contain every ledger path exactly once at the recorded `vg_path_length`. The explicit
`hst-v1` adapter preserves D062/D063's historical suffix and t2g behavior.

## 9. Alignment eligibility and scoring

Each aligned node is scored under vg's own alignment scheme, reproduced per node from the
`Mapping` edits (see `docs/conversion-algorithm.md`). A transcript's evidence is the score of
its best HST. The read's targets are the HSTs tied at the single top score pooled across all
of the read's alignments; lower-scoring HSTs are not emitted. RAD output is an unweighted
target set with no probabilistic weighting.

Exact `genefull_ex50pas` BAM evidence has a D068-specific eligibility rule. Its model-bound
dynamic program accumulates the stored GAMP subpath scores and scored connections for each
complete transcript-compatible traversal. Across every MultipathAlignment record in the read
group, retain evidence whose best traversal score is at least the global compatible top minus
five, inclusive; only then may the downstream consumer apply the six E/P/B-orientation ranks.
This five-point window is one mismatch-equivalent under vg's default `+1` match and `-4`
mismatch scoring. It does not change the ordinary score-mode target set or RAD output.
In v0.8 this is the default. `--no-ex50-score-window` skips exact-evidence score pruning entirely
and restores all complete compatible traversals before the same six-rank consumer logic. The
opt-out is a sensitivity mode and likewise leaves ordinary score/RAD output unchanged.

## 10. Emitted target set

panCollapse emits the full compatible transcript set per read — the winning HSTs collapsed to
unique transcript IDs. It does not prefilter to transcript- or gene-unique subsets, and the
active converter has no assignment-mode CLI.

## 11. Output

Native v0.10 `count` always publishes `parquet/barcodes.parquet`,
`parquet/features.parquet`, `parquet/counts.parquet`, `parquet/molecules.parquet`,
`manifest.json`, and `summary.tsv` through a staged directory rename. The Parquet tables use the
exact schemas and sort order in `docs/input-output-contract.md`; barcode and feature dictionaries
are the lexical union actually used across requested profiles. The manifest records immutable
profile hashes, source identities, canonical logical table hashes, current output hashes,
counters, timings, threads, memory limit, cache statistics, and spill statistics. Optional raw
10x MEX is emitted per profile, while optional RAD is one pre-count compatibility artifact rather
than a rendering of either profile. Per-read assignments remain opt-in because they restore
per-read-scale I/O.

The remaining section defines legacy `convert` output.

V1 emits mapper-style, uncollated RAD that can enter the standard sequence:

```text
alevin-fry generate-permit-list
alevin-fry collate
alevin-fry quant
```

Target IDs in RAD are the canonical transcript IDs a read's winning HSTs collapse to.
panCollapse writes their ordinary two-column canonical `tx2gene.tsv`, which allows
alevin-fry to produce the final cell-by-gene matrix. V1 does not attach splicing-state labels.

Each emitted read record carries the raw cell barcode, raw UMI, compatible target IDs,
and one orientation value per target. Target IDs are indices into the RAD header target
dictionary, not genomic coordinates. Orientation values are target-level RAD metadata
consumed by alevin-fry's expected-orientation filtering.

With optional BAM output, production identity is auditable per target through `TX`, exact-path
`XP`, and unique-Parent `XU` tags. `XP`/`XU` use semicolon-separated groups parallel to `TX` and
comma-sorted tied winners inside each group in ordinary modes. Exact `genefull_ex50pas` uses
parallel `TX`/`GX`/`GD`/`GT`/`XP`/`XU` slots with exactly one path/Parent each (exon for E/P,
linked body for B); canonical `TX` values may repeat across distinct evidence slots. The existing
RAD remains byte-identical. By default these exact slots have passed D068's inclusive five-point
complete-traversal score window; under `--no-ex50-score-window`, they retain all compatible exact
evidence. Their ordering in the BAM is not a score ranking. Typed-union BAMs pin the selected policy
with `@CO panCollapse-ex50-score-window:5` or `:disabled` in addition to the evidence-schema marker.

The normal information-complete typed-union BAM is the production default. The
`--compact-exact-count-bam` projection is an explicit, lossy research opt-in and must never
be enabled by a default CLI value, pipeline configuration, or example recipe. Only the user
may decide that it is necessary and authorize a use; the current decision is that it is not
necessary. A later authorized plan must record the concrete operational reason, why the
normal BAM is unsuitable for that run, and which downstream-reselectable evidence is
discarded; run updates and results must highlight that compact mode was used.

The optional BAM also preserves the barcode-correction population independently of feature
construction. Every valid raw-molecule group emits exactly one BAM record. Featureless groups are
unmapped `XB:Z:barcode_only` records with `CB`/`CR`, `UB`/`UR`, and optional raw qualities
`CY`/`UY`; they carry no gene/transcript tags. The supported upstream RNA name is
`<name>_<CB>_<UMI>_cy<hex(CY)>_uy<hex(UY)>`, with qualities encoded as ASCII hex.
Legacy quality-free and CY-only names remain readable.

The exact RAD header, record fields, orientation encoding, chunking, and metadata are
defined by the supported alevin-fry/libradicl baseline. Starting in v0.9, `--threads N`
also uses exact Parents and canonical targets (or legacy genes) as independent initialization work
units before using complete read-name groups as processing work units. Initialization results are
reduced in stable ordinal order; one parser owns grouping and one ordered writer owns every
artifact. `map.rad` is streamed to disk incrementally: records roll into complete,
self-describing chunks, and the writer seeks back to backpatch each chunk header and the file-level
`num_chunks` with their final values once known (D049). RAD, BAM, summary, tx2gene, and debug
artifacts from `convert` must remain byte-identical across supported thread counts for identical
non-operational inputs and configuration. Native `count` must instead preserve decoded semantic
rows and corresponding per-profile logical hashes across thread counts, scheduling, spill
boundaries, and joint-versus-separate profile execution. Its operational manifest and summary fields may legitimately report different
thread, timing, cache, or spill values, and Parquet bytes may change after a future pinned Arrow
upgrade without changing logical identity.

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
- total optional-BAM records and barcode-only prior-evidence records;
- manifest misses and annotation/index consistency failures;
- native-count barcode-prior, correction, UMI-filter, assignment, molecule, cache, memory, and
  spill counters globally and per profile where the event is profile-specific.

Errors affecting input interpretation must fail loudly rather than silently changing the
assignment model.

## 13. Performance constraints

V1 should be stream-oriented over name-grouped GAMP. It may build ordinary in-memory GTF
structures and use existing VG indexes, but it must not introduce a new persistent custom
index.

Parallel execution must share graph and annotation state within one process, bound queued and
out-of-order work, preserve exact completed-name validation, and serialize output by input-group
ordinal. Initialization workers must read immutable graph/annotation state, return private
Parent/target/gene results, and commit through an ordinal window bounded at twice the effective
worker count. Lazy node and exon-edge caches may be sharded but must remain demand-driven; no eager
whole-XG cache or cache spill is implied. Worker count is operational provenance and must not
change scientific output bytes. Initialization phase time, processing time, group throughput,
cache-entry counts, and periodic progress are stderr-only diagnostics so performance regressions
can be separated into startup and per-group phases.

If direct lookup proves too slow, record profiling evidence and a proposed custom-index
design as a future development item. Implementation of that index requires a separate
human-approved decision.

Native `count` is a single GAMP pass with no complete per-read evidence spool. Workers resolve
CB-independent evidence into thread-local aggregate batches; fixed hash shards combine integer
counts. Only potentially correctable off-whitelist observations are deferred. Deterministic,
Zstandard-compressed sorted runs spill when `--count-memory-budget` is crossed, including a
secondary path for one pathological barcode. Barcode correction partitions by raw barcode and
UMI reduction by corrected barcode. Ordered coordination is reserved for explicitly ordered RAD
or diagnostic sinks.

## 14. Acceptance boundary

The V1 product is complete only when:

- all behavioral fixtures in `docs/validation-contract.md` pass;
- a tiny generated RAD file is accepted by the current supported alevin-fry workflow;
- the output gene matrix matches the exact expected result;
- name-grouping, raw molecule-identity parsing, configured-length mismatch, collapse,
  score, target-relative orientation, all-mode RAD assignment, and deferred assignment
  option failures are tested;
- performance is characterized on a bounded pilot;
- deterministic-output tests prove byte-identical `convert` artifacts across supported execution
  modes;
- no custom index or deferred splicing-state behavior has entered the implementation.

Native v0.10 additionally requires exact frozen-oracle agreement for terminal classes, corrected
barcodes, retained molecules, matrices, and counters; logical-hash equality for no-spill and
repeated-spill runs at supported thread counts; joint-profile equality with separate-profile runs;
independent Parquet/MEX validation; optional RAD byte identity; and a release image that exercises
all count formats without Python. The staged scientific release gate then uses the canonical
one-million-read subset, full chr20 and chr21, and the immutable joint chr20–22 GAMP. On the joint
slice, default persisted output must be at most 8.4 GB versus the 167.7 GB v0.9 BAM, wall time at
least twofold faster than the matched 7 h 48 m producer, total time below one day, and peak RSS
below 600 GB. Biological profile rules stay frozen if an implementation or resource gate fails.
