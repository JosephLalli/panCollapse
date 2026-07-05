# Changelog

All notable changes to panCollapse are recorded here. Versions follow the project's
`major.minor.patch` scheme.

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
