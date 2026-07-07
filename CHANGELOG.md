# Changelog

All notable changes to panCollapse are recorded here. Versions follow the project's
`major.minor.patch` scheme.

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

### Notes

- No binary behavior changed from v0.3: the converter is byte-identical; GeneFull rides on the
  existing path-attribution mechanism. GeneFull's only marginal call over spliced is the
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
