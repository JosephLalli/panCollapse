# Changelog

All notable changes to panCollapse are recorded here. Versions follow the project's
`major.minor.patch` scheme.

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
