# Changelog

All notable changes to panCollapse are recorded here. Versions follow the project's
`major.minor.patch` scheme.

## [0.10.0]

- Adds native `panCollapse count`, which consumes a verified `panSC-count-facts-v1` bundle
  and emits a Parquet count dataset by default. Native count uses frozen profiles, retains
  legacy `convert` unchanged, rejects mixed legacy identity/policy inputs, writes no BAM,
  and has no Python production dependency.
- Adds immutable `cr7-v1` and `pansc-strict-v1` profiles, selectable together in one GAMP
  pass. Typed assignment-policy overrides are accepted only under explicit
  `sensitivity-analysis` scope and always receive a derived profile ID plus full effective-policy
  SHA-256; barcode and UMI algorithms cannot be overridden.
- Adds all-valid-read barcode priors, frozen one-mismatch posterior correction, exact basic-UMI
  filtering, non-transitive `1MM_CR`, and cross-gene `MultiGeneUMI_CR` support/tie handling.
  Profile-specific terminal counters preserve the oracle ordering between correction, early
  evidence exits, and UMI filtering.
- Adds deterministic aggregate spill/merge under `--count-memory-budget` and a bounded sharded
  numeric-evidence assignment cache. Decoded semantic rows and per-profile logical hashes are
  invariant to thread count, scheduling, spill boundaries, and joint-versus-separate execution;
  operational manifest fields are intentionally not byte-identical.
- Adds optional 10x MEX (`--10x-mex`) and RAD (`--rad-out`) compatibility outputs to native
  count, plus opt-in ordered read-assignment Parquet (`--read-assignments-out`) for per-read
  audit. The latter is disabled by default because it is per-read-scale I/O.
- The release image now requires and stages Arrow/Parquet runtime libraries, verifies the
  staged loader closure before image build, requires Zstandard for compressed native outputs,
  and fails if `libpython` would enter the image. The image is standalone and makes no
  workflow-orchestrator runtime assumption.

## [0.9.0]

- Adds deterministic read-group parallelism through `--threads N`. One parser owns GAMP
  grouping and recurrence validation, workers use private per-group workspaces, and one ordered
  writer preserves the original RAD/BAM/debug record order and chunk boundaries.
- Parallelizes exact Parent-model construction and canonical-target splice geometry (plus the
  legacy gene-geometry compatibility path) with adaptive Parent blocks. Worker results commit in
  stable order through a reduction window bounded at twice the worker count, preserving model IDs
  and lexical failure order without retaining every temporary geometry result.
- Replaces process-wide lazy-cache miss locks with 256-way sharded node/model-candidate caches
  whose values are immutable and address-stable. Caches remain demand-driven, so threading does
  not require an eager whole-XG cache or additional disk I/O.
- Builds each exon/body path geometry once per Parent instead of once per exon/body model pair.
  Exact exon splice-edge geometry is shared by exactly equal ordered step sequences and referenced
  directly by the read DP, removing a highly contended cache lock without collapsing path/Parent
  evidence identities. Ordinary models no longer allocate repeated-body position maps.
- Keeps production count-mode tallies numeric from XG path handle through the path -> Parent ->
  target MAX reduction. A single layered flat Parent table replaces duplicated string-keyed ordered
  maps, while path and Parent names are recovered only for winning BAM provenance.
- Removes duplicated production adapter maps and annotation strings: the validated ledger owns
  stable row/Parent metadata, and compact indexes retain pointers plus lexical ranks.
- Consolidates equivalent exact dynamic-programming states by complete future-relevant geometry
  while retaining the maximum integer score, and prefilters impossible starting body paths within
  a source subpath.
- Defers debug-sidecar writes until after read-local exact DP, so requesting diagnostics no longer
  serializes the dominant worker computation. Tiny exact surfaces (fewer than 32 models) use one
  active worker automatically when thread handoff costs more than the work.
- Reports initialization and processing time, effective group throughput, and five-minute or
  one-million-group progress on stderr without changing persisted artifacts. Setting
  `PANCOLLAPSE_PROFILE_TIMING=1` adds queue, compute, ordered-wait, and ordered-region diagnostics.
- Keeps normal typed-union BAM evidence, the five-point exact score window, Parent identity,
  degradation behavior, RAD schema, and output order unchanged.

## [0.8.2]

- Exact GeneFull initialization now records the canonical targets containing an unresolvable
  transcript-body Parent while those Parents are identified. Geometry construction uses that
  constant-time index instead of rescanning every exon path for every canonical target. This
  removes an accidental quadratic pass without changing Parent degradation, clean-sibling,
  exon/body geometry, RAD, or BAM evidence semantics. On the joint chr20-22 graph, the patched
  binary reached the frozen `evaluated_target_edges=141543588` geometry boundary in 73 minutes;
  v0.8.1 remained in the same pre-GAMP pass after more than 25 hours.

## [0.8.1]

- A cyclic transcript-body path now degrades only its linked Parent to exon-layer evidence when
  no unique oriented body occurrence exists. Clean sibling Parents remain body-capable; the run
  reports the degraded-path counter and debug path names.

## [0.8.0]

### Changed

- **Exact GeneFull_Ex50pAS now score-filters compatible traversals before feature ordering.**
  Across every MultipathAlignment record in a read group, Parent-specific exact evidence is retained
  only when its best complete compatible traversal score is within five points of the global
  compatible optimum, inclusive. The dynamic program includes stored subpath and scored-connection
  values; downstream `count_cr.py` applies the six E/P/B-orientation ranks only after this filter.
  The five-point window is one mismatch-equivalent under vg's default scoring. Ordinary Gene
  evidence and RAD output are unchanged. New typed-union BAMs pin the policy with
  `@CO panCollapse-ex50-score-window:5`. (D068)

### Added

- **`--no-ex50-score-window` restores pre-D068 exact-evidence eligibility.** The v0.8 default
  remains the inclusive five-point filter; the opt-out skips score pruning entirely and sends
  every complete compatible exact traversal to downstream E/P/B-orientation ranking. It is
  accepted only for production-ledger count-mode BAMs. Opt-out BAMs carry
  `@CO panCollapse-ex50-score-window:disabled`, and `summary.tsv` records `5`, `disabled`, or
  `not_applicable`. Focused fixtures prove the disabled policy restores top-minus-six alternatives
  both within a GAMP DAG and across records while leaving RAD byte-identical. (D069)

The bounded HG002 chr20 qualification recovered 5,306 of 445,665 prior direct-multigene drops
(1.190580%). The policy is retained as a valid default improvement with an explicit sensitivity
opt-out; it is not presented as the primary explanation for the remaining graph/STAR gap.

See `docs/decisions.md` D068 and D069 for the score policy, qualification, and opt-out contract.

## [0.7.0]

### Changed

- **BREAKING: production ledger count-mode BAMs are now a versioned typed union.** A single
  `genefull_ex50pas` conversion carries both ordinary Gene and exact Ex50 evidence under the exact
  `@CO	panCollapse-evidence-schema:panCollapse-superset-v1` header marker. Parallel
  `XR/TX/GX/GD/GL/GT/XP/XU` rows use `XR=G` or `XR=X` as the family discriminator and literal `.`
  for the inactive family. Repeated transcripts remain independent evidence rows. Consumers must
  validate the complete union before projecting one family; the previous mode-specific production
  BAM layout remains available only on marker-absent legacy paths.
- **Typed-union BAM production requires complete body provenance.** Every exon Parent in the
  production path-identity ledger must have at least one linked body row. This fail-closed
  requirement applies only to production ledger count-mode BAM output; general score-mode and
  non-BAM RAD conversion remain less restrictive.

### Added

- **`--debug-evidence-out` emits the versioned `panCollapse-debug-evidence-v1` TSV sidecar.** It
  records source-order read rows, recognized splice-edge counts, and the exact-top candidate set
  before flank filtering. The sidecar is written atomically and is diagnostic only: enabling it
  does not change BAM classifications or RAD bytes.

See `docs/decisions.md` D067 for the schema rationale and cross-repository consumer contract.

## [0.6.0]

### Changed

- **BREAKING: production conversion now takes `--path-identity-ledger`, and a bare `--t2g` is a hard
  error.** The ledger is a headered, strictly validated `panSC-path-identity-v1` TSV whose 24 columns
  carry the full annotation provenance for every emitted path: `unique_parent`, separate
  `source_parent` and `input_parent`, canonical transcript and gene, source/sample/haplotype/annotation
  fields, exon-versus-body layer and selection/fallback state, an explicit body-to-exon Parent
  crosslink, source identities/class/coordinates/strand, and the exact vg path name, length, and
  haplotype origins. The historical exon/body t2gs still work, but only behind an explicit
  `--legacy-adapter hst-v1`. Neither route auto-detects the other and neither silently falls back --
  selecting exactly one identity input is required, and combining `--path-identity-ledger` with
  `--t2g`/`--body-t2g` is rejected. This replaces heuristic identity inference: production lookup is
  the exact chain `vg_path_name -> unique_parent -> canonical_transcript -> gene_id`, and identifier
  strings are opaque, including any literal `_H<n>`/`_R<n>` suffix that previously would have been
  stripped.
- **BREAKING: production `genefull_ex50pas` is now an exact BAM/count_cr evidence mode.** It requires
  `--path-identity-ledger`, `--bam-out`, and `--bam-multigene all`; `--legacy-adapter hst-v1`
  is rejected. Each exact body-contained GAMP traversal is classified as E (fully exonic with
  concordant junctions), P (strictly more than half exonic), or B. Exactly half is B and a fully
  exonic splice-discordant traversal is P. Evidence remains path/Parent-specific until count_cr
  applies STARsolo 2.7.11b's six sense/antisense ranks. The RAD compatibility path is unchanged.
- **Transcript is the classification unit end to end; no isoform inherits another's evidence.** Exon
  and body scores are keyed by canonical transcript against one shared global top-score threshold. A
  transcript is `S` from its own near-top exon score, and otherwise `U` from its own near-top *body*
  score -- another transcript of the same gene can no longer inherit a gene-body `U` call. D061 splice
  concordance gates both states. Splice ownership is precomputed per canonical transcript by comparing
  that transcript's exon paths only against its own body paths, rather than against a pooled gene body,
  and body-path traversal order (not numeric node-id order) defines an internal step, so fragmented
  bodies, nonmonotonic node ids, reverse paths, and repeated-node adjacency resolve correctly. No gene
  identity participates in the classifier at all; `count_cr` groups the emitted `TX` entries by `GX`.
- **`GD` is computed per transcript in the transcript-first path**, not as one majority orientation per
  gene -- closing the gap 0.5.0 recorded as "not implemented". A two-column body t2g continues to use
  the D060/D061 gene-body/span classifier and gene-level orientation.
- **Scores collapse by maximum, never by sum, at every level.** Paths MAX-collapse within a Parent and
  Parents MAX-collapse within a canonical transcript, with all tied winning paths and Parents retained
  as provenance. Raw graph paths are scored and selected *first*; only those tied at the single top
  score are then grouped by canonical identity. Additional CAT-projected haplotype copies of one
  transcript therefore cannot manufacture a higher score than a single-copy competitor, which summing
  before winner selection would have allowed.

### Added

- **`GT` BAM tag for exact `genefull_ex50pas` evidence.** It is parallel to
  `TX`/`GX`/`GD`/`XP`/`XU`; every slot has one exact exon path/Parent for E/P or one linked body
  path/Parent for B. Canonical `TX` values may repeat across locus Parents and alignment
  alternatives. `GL` is absent in this mode.
- **`XP` and `XU` BAM tags**, parallel to `TX`, naming the exon and body layer that supplied each
  transcript's `S`/`U` call. Groups are semicolon-separated in `TX` order; tied exact paths and Parents
  within a group are comma-sorted. Production score-mode BAM carries these tags too, while its
  `GX`/`GD` stay gene-level. The `hst-v1` adapter emits no new score-mode tags and keeps its prior
  tested layout.
- **Optional three-column t2g aliases** for both `--t2g` (`graph_path`, `gene`,
  `canonical_transcript`) and `--body-t2g` (`raw_body_path`, `gene`, `canonical_transcript`), the
  intermediate that the path identity ledger generalizes. Column 1 stays the exact raw graph path, so
  CAT-projected transcripts remain haplotype-unique while `vg rna` builds the graph. A t2g is
  consistently two- or three-column; mixing widths is a hard error, as is one raw path mapping to
  different transcripts or one canonical transcript mapping to different genes.
- **Strict ledger validation at load and at runtime.** The reader requires the exact 24-column header
  and order, nonempty TSV-safe fields, positive numeric coordinates and path lengths, `start <= end`,
  unique exact path names, one annotation identity and one feature layer per Parent, each canonical
  transcript bound to one gene, disjoint exon and body Parents, exon self-links, and body links to an
  existing exon Parent with matching canonical transcript and gene. At runtime every ledger path must
  occur in the XG with exactly the recorded length; extra non-ledger genomic paths are allowed. Comma
  and semicolon are rejected only where they would corrupt BAM provenance grouping, so general
  provenance such as comma-separated `vg_haplotype_origins` is unaffected.

### Notes

- A transcript-first production run must pass `--bam-out ... --bam-multigene all` so multi-gene ledger
  records survive into `count_cr`. RAD output policy is unchanged and continues to omit them under the
  Unique rule.
- The version is bumped to 0.6.0 rather than 0.5.1 because requiring `--path-identity-ledger` removes a
  previously valid invocation: any caller passing a bare `--t2g` must now add `--legacy-adapter hst-v1`.

See `docs/decisions.md` D062, D063, and D064 for the full mechanism, validation rules, and rationale.

## [0.5.0]

### Changed

- **The ledger spliced/unspliced classification is now per transcript, by intron touch, replacing
  the per-gene score-tie rule -- and panCollapse now emits it per transcript too, not as a per-gene
  summary.** A new `TX` BAM tag carries the read's compatible transcript ids (`;`-separated,
  sorted); `GX`/`GD`/`GL` become one entry **per `TX` entry** (positionally parallel), not one per
  gene, so a gene with more than one compatible transcript repeats in `GX`. `GL` is now a single
  `S`/`U` call per transcript, replacing the 2-field per-gene `spliced:unspliced` flag pair
  (0.4.5). Gene compatibility itself is unchanged (still the genes whose body or an exon transcript
  ties the read's single top score). Within each compatible gene, panCollapse now classifies
  **every candidate transcript**: at graph load it precomputes each exon transcript's on-body exon
  node-id span, and at read time a transcript "spans" the read if the read's gene-body node range
  falls inside that span. It is `S` iff its own exon path also ties the top score; `U` iff it spans
  the read, its gene's body ties the top score, and it is not already `S`. This fixes a Gene-mode
  over-count on the all-haplotype pangenome under the old rule: a read sitting in a reference
  transcript's intron could tie the read's top score via some *other* haplotype's or isoform's
  unrelated exon path, and the old per-gene rule ("spliced iff any exon isoform ties top") called
  the gene spliced on that coincidental tie regardless of whether any transcript actually explained
  the read exon-only.
- **Splice-junction concordance now gates the `S` call.** Node-membership alone (a transcript's
  exon path ties the read's top score) was too loose: a read can land its aligned bases on
  transcript A's exon while the splice (node-skip) it actually makes is a *different*, overlapping
  transcript B's intron -- a graph-sharing coincidence at, e.g., a tail-to-tail gene overlap. A
  transcript is now `S` only if it additionally owns *every* splice edge the read's own alignment
  crosses -- STAR's `classifyAlign` concordance rule, kept strand-blind exactly as STAR does
  (orientation is applied later by `count_cr` via `GD`). The same gate applies to `U`: a transcript
  that ties the exon score or spans the read's body range but fails concordance is emitted as
  **neither** `S` nor `U` -- because a spliced read is not also an unspliced one, and without this
  gate a concordance-failed transcript would fall through to `U` and spuriously make its gene
  ambiguous.
- **A gene with both an `S` and a `U` transcript among its compatible set is velocyto's
  "ambiguous"** (the read is spliced for one isoform, unspliced for another). panCollapse does not
  label it as such -- it emits each transcript's own call as-is and leaves gene grouping,
  ambiguity, the count-mode rule, and the sense/antisense policy to the downstream counter
  (`count_cr`: `gene` mode counts spliced-only, excluding both ambiguous and unspliced;
  `genefull`/`genefull_exonoverintron`/`genefull_ex50pas` count any compatible gene, preferring
  purely-spliced where the mode's tie-break applies).

### Not implemented (documented)

- A read's compatible transcripts are still summarized as one majority orientation per gene (the
  `GD` tag); reporting per-transcript sense/antisense sets separately is planned but not
  implemented.

See `docs/decisions.md` D060 and D061 for the full mechanism, rationale, and chr20 validation
numbers.

## [0.4.5]

### Changed

- **The per-gene ledger BAM `GL` tag is now two boolean compatibility flags, `spliced:unspliced`,
  replacing the `exonic:intronic:concordant` aligned-base counts.** Reads are scored per reference
  path (exon-layer transcripts and gene-body paths) by aligned bases; the references tied at the
  single top score form the compatible set. A gene is `spliced` iff one of its exon isoforms ties top
  (the read is fully explained by that isoform) and `unspliced` iff its gene body ties top. This
  replaces the base-count ledger and the per-gene exon UNION it relied on. `count_cr.py` reads the two
  flags directly (`gene` = spliced, `genefull` = any compatible, the `genefull_*` variants prefer
  spliced), so **v0.4.5 is required by the panSC `count_cr` ledger count modes** -- v0.4.3/v0.4.4 emit
  the old 3-field `GL`, which the current `count_cr` cannot parse.
- **The count mode is applied entirely downstream now, including on the RAD/alevin-fry path.** The RAD
  equivalence class is the full compatible gene set (still Unique: a read compatible with more than one
  gene is dropped from the RAD), no longer the `--count-mode`-filtered set, so one alignment serves
  every count mode. This changes the alevin-fry/simpleaf counts for `--count-mode gene`: the RAD now
  includes body-only (intronic) single-gene reads that the old `apply_count_mode` gene rule dropped.
  The gene-vs-GeneFull distinction for that path now lives only in a downstream `count_cr` step.

### Fixed

- **The mode-agnostic ledger BAM again honours `--bam-multigene`.** The 0.4.4 mode-agnostic BAM change
  had begun emitting every compatible read regardless of the policy; a single-gene read is once more
  always written, while a multi-gene read follows `--bam-multigene`: `omit` drops it from the BAM (as
  the RAD Unique rule drops it), `first` assigns it to the primary gene, `all` carries it with the full
  candidate set and no `XT` for `count_cr`'s MultiGeneUMI_CR rescue.

## [0.4.4]

### Changed

- **Per-gene target orientation moved out of the collapse stage into a new `GD` BAM tag (D059).**
  panCollapse now *emits* each read's per-gene orientation instead of only being able to *filter* on it,
  so a downstream counter owns the sense/antisense policy. This fixes a strand gap in the ledger
  `genefull_ex50pas`/`genefull` count modes: they kept antisense **intronic** reads that STARsolo's
  `GeneFull_Ex50pAS` excludes (`intronicAS`), inflating large (−)-strand genes that overlap antisense
  transcription. On a chr20 pangenome GEX benchmark, `count_cr.py --strand forward` (STARsolo sense-
  strand default) now reproduces STARsolo: PTPRT 26,432 → 157 UMI (STAR 36), per-gene Pearson
  0.966 → 0.995, shared-space UMI delta +12.4% → +1.55%. `--strand both` reproduces the pre-0.4.4
  numbers exactly (Ex50pAS 1,078,434; gene 740,205), so the change is behavior-preserving with strand
  the only new lever.

### Added

- **`GD` BAM tag** (`docs/bam-export.md`): per-gene target orientation, one `F` (sense/forward) / `R`
  (antisense/reverse) per `GX` gene, `;`-separated and positionally parallel to `GX`. Additive — the
  RAD path, the existing `--strand` RAD-side filter (D056), and `--count-mode score` are unchanged, and
  all 58 tests pass. A STARsolo genome BAM carries no `GD`, so a `GD`-gated counter is a no-op there and
  its counts are unchanged: Gene stays bit-identical to STARsolo (0 UMI diff) and GeneFull /
  GeneFull_Ex50pAS match to 100.000% (a pre-existing 2-UMI/~922k residual from a MultiGeneUMI_CR
  multimapper-tie edge, unrelated to this change). See `docs/decisions.md` D059.

## [0.4.3]

### Added

- **`--bam-multigene all`** (D058). For the ledger `--count-mode` modes (`gene`, `genefull`,
  `genefull_exonoverintron`, `genefull_ex50pas`) ONLY, a read compatible with more than one gene is
  still dropped from `map.rad` exactly as before (`multigene_dropped_groups`), but under `all` it is
  now also written to the optional `--bam-out` BAM, tagged with its full candidate-gene set (`GX`)
  and no `XT`. Previously such a read was hard-dropped from BOTH outputs, so a downstream UMI-level
  rescue (STARsolo/CellRanger `MultiGeneUMI_CR`: assign the UMI to its dominant gene, discard on an
  exact tie) never saw it. `--count-mode score` and `--bam-multigene omit`/`first` are byte-identical
  to v0.4.2 -- this is purely additive. See `docs/decisions.md` D058 for the full mechanism and the
  STARsolo-source tie-rule verification.

## [0.4.2]

Infrastructure release. No change to the binary, its inputs, or its output: the RAD,
`tx2gene.tsv`, `summary.tsv`, and optional BAM are byte-identical to v0.4.1 on the same
inputs (the bundled binary still reports `panCollapse 0.4.1`). Only the runtime image
changed.

### Changed

- **Runtime image installs `procps`.** The Nextflow docker executor invokes `ps` inside the
  container to collect per-task resource metrics; the v0.4.1 image (`debian:bookworm-slim`,
  which omits it) made Nextflow abort the task with "Command 'ps' required by nextflow to
  collect task metrics cannot be found". The image now adds the `procps` package
  (`/bin/ps`) in its own layer. `ps` is unrelated to the bundled binary, which runs through
  its own dynamic loader independent of the base image. Published as
  `josephlalli/pancollapse:v0.4.2`.

## [0.4.0]

### Added

- **GeneFull (intron-inclusive) counting** — a read calls a gene if it overlaps the gene body
  (exon **or** intron), matching STARsolo/CellRanger include-introns, keeping the
  purely-intronic reads the default spliced/exonic count drops. This needs **no new panCollapse
  code or flag**: it is the ordinary count run against a graph annotated with **gene-body
  paths**, selected by the t2g. panCollapse reads gene-body membership off the embedded paths
  exactly as it reads transcript membership off HST paths, so the same graph gives a spliced
  count with the HST t2g or a GeneFull count with the gene t2g. See
  [`docs/genefull.md`](docs/genefull.md). (D055)
- `scripts/make-gene-annotation.sh` — annotates a graph with one unspliced gene-body path per
  gene (projected by `vg rna --feature-type gene`, covering introns) and writes the matching
  gene t2g; pass the haplotype GBWT (`-l`) to cover non-reference (alt) nodes. Output graph keeps
  the original HST paths, so it serves both counts.
- **`--strand both|forward|reverse`** — target-relative orientation filter (default `both`, no
  filtering). `forward` keeps only sense targets, `reverse` only antisense; reads left with no
  matching target emit no record and are counted in the new `strand_filtered_groups`. Use
  `forward` for a sense-stranded library to drop antisense reads (like STARsolo
  `GeneFull_Ex50pAS`'s antisense exclusion / `--soloStrand`). Opt-in; `both` reverses nothing
  (D042 preserved as the default). (D056)
- **`--count-mode` + `--body-t2g`** — reproduce STARsolo/CellRanger `soloFeatures` exactly by
  counting per-gene exonic vs intronic aligned bases across two path->gene t2gs (exon/HST +
  gene-body). Modes: `gene` (>=50% exonic), `genefull` (any body overlap),
  `genefull_exonoverintron`, `genefull_ex50pas` (CellRanger v7 default: prefer >50%-exon genes,
  drop 100%-exonic antisense). All apply the `Unique` multimapper rule (a read compatible with >1
  gene is dropped, counted in the new `multigene_dropped_groups`). Default `--count-mode score` is
  the unchanged D048 count. See [`docs/genefull.md`](docs/genefull.md). (D057)

### Notes

- GeneFull adds no binary behavior: it rides on the existing path-attribution mechanism, and with
  the default `--strand both` the converter output is byte-identical to v0.3. The only new binary
  behavior is the opt-in `--strand` filter. GeneFull's only marginal call over spliced is the
  purely-intronic read (any read touching an exon is already called in spliced counting).
  Overlapping gene bodies resolve by the same top-score-plus-ties rule (entirely-shared ->
  multi-gene; dominant -> that gene). Requires a graph that retains intron sequence (do not build
  it with `vg rna -d/--remove-non-gene`).

## [0.3.0]

### Added

- **Optional BAM output (`--bam-out`)** for a CellRanger-style counting stack
  (`umi_tools count --per-gene --gene-tag=XT`, then DropletUtils `emptyDropsCellRanger`),
  alongside the existing RAD. The BAM carries panCollapse's graph-derived gene assignment as
  10x tags (`CB`/`CR`, `UB`/`UR`, `GX` = full compatible-gene set, `GN`, and `XT` = the single
  gene or omitted for multi-gene reads), with one mapped record per emitted read placed at a
  nominal position 1 of a synthetic per-gene contig. Because a `--per-gene --gene-tag` counter
  dedups by `(cell, UMI, gene)` and ignores position, no real coordinates are needed; genes come
  from the graph, so reads that never surject to the linear reference are kept. See
  [`docs/bam-export.md`](docs/bam-export.md). (D054)
- `--bam-multigene omit|first` — `XT` policy for multi-gene reads (default `omit`, matching
  CellRanger `soloMultiMappers Unique`).

### Notes

- The RAD path is unchanged: `map.rad`/`tx2gene.tsv`/`summary.tsv` are byte-identical with or
  without `--bam-out` (verified on the 1M-read MHC GAMP, sha `7df66891...`). No genome surjection,
  no new runtime input; htslib was already a transitive dependency.

## [0.2.0]

Performance release. Every change below is a speed or memory improvement only: the RAD,
`tx2gene.tsv`, and `summary.tsv` output is byte-for-byte identical to v0.1 on the same
inputs, and the full test suite (30 tests) passes at each step. On the 1M-read MHC GAMP
benchmark the end-to-end conversion is about **7.5x faster** than v0.1 (3:47 -> 0:30) with
peak resident memory down from ~362 MB to ~281 MB.

### Changed

- **Optimized build by default.** The build set no `CMAKE_BUILD_TYPE`, so binaries — and the
  v0.1 Docker image — were compiled at `-O0`. `CMakeLists.txt` now defaults to
  `RelWithDebInfo` (`-O2 -g`) when no configuration is chosen. `-O2` alone accounts for a
  ~3.6x speedup (2:44 -> 0:45.6); `-O3` measured within noise and drops debug symbols, so
  `RelWithDebInfo` is the default. There are no `assert()`s in production code, so the
  accompanying `-DNDEBUG` disables no runtime checks. (D052)
- **Cache per-node HST lookups.** Scanning a graph node's path steps
  (`for_each_step_on_handle`) was repeated for every read that crossed the node. Each distinct
  node is now scanned at most once and the result replayed, cutting the `-O0` run from 3:47 to
  2:44. (D051)
- **Move parsed records instead of copying them.** Each GAMP `MultipathAlignment` was
  deep-copied into its read group's buffer (~21% of the run in profiling); it is now moved.
  This also lowered peak resident memory. (D053)
- **Reuse a flat tally instead of a per-group `std::map`.** The per-group transcript tally is
  now a reused `absl::flat_hash_map`, replacing red-black-tree string comparisons (~13% of the
  run) with hashing and removing per-group node allocation. `cmake/PanCollapseVg.cmake` links
  `absl_raw_hash_set`; abseil was already a transitive dependency. (D053)
- Version bumped to 0.2.0; the runtime image is published as `josephlalli/pancollapse:v0.2`.

### Notes

- Profiling (callgrind, 1M-read MHC) showed the conversion is dominated by data movement —
  GAMP parse, record copies, and allocations — not by scoring or graph math (the scorer is
  ~1%). The converter remains single-threaded (D045): with only ~34% of the run parallelizable,
  multithreading is Amdahl-capped near 1.5x and was judged not worth the added complexity.

## [0.1.0]

Initial release: convert `vg mpmap` GAMP multipath alignments into alevin-fry RAD records
using PathTally graph-native transcript-compatibility scoring (D048), with a streaming RAD
writer (D049) and a runtime Docker image.
