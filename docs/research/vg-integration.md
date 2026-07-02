# VG Integration Research Brief

> **Runtime input set superseded by D048.** This Phase 0 research brief was written for the
> GTF-projection design and lists GTF and a collapse manifest as runtime inputs (see
> "Minimal converter inputs" and "Recommended boundary" below). Under D048, the active
> runtime inputs are: name-grouped GAMP, the `.xg` graph carrying `vg rna` HST paths, and a
> transcript-to-gene map. GTF and collapse manifest are reference/fixture-creation tools
> only. The VG-linkage boundary, GAMP streaming API (`vg::io::for_each`), per-node scoring
> evidence (`alignment_scorer.cpp`), and XG `for_each_step_on_handle` path attribution
> sections remain current. Annotation-tag evidence (D038 raw CB/UMI comes from the GAMP
> name field, not `mpmap -C` annotation tags) was superseded by D038.

**Status: Phase 0 researched. Scope: local VG v1.75.0-68-ge82694b69
"Spike".**

## Verified facts

- Local baseline:
  - Source checkout: `/mnt/ssd/lalli/usr/local/src/vg`.
  - Installed binary: `/mnt/ssd/lalli/usr/local/bin/vg`.
  - Version: `v1.75.0-68-ge82694b69 "Spike"`, commit
    `e82694b699988205d5105231240db000f9655335`, describe
    `v1.75.0-68-ge82694b69-dirty`.
  - Built with GCC 15.2.0, libstdc++ 20250808, HTSlib headers 101990, and
    HTSlib library `1.19.1-29-g3cfe8769`.
  - The dirty dependency state is local evidence only; do not infer broader
    upstream stability from it.
- Minimal converter inputs:
  - Name-grouped GAMP.
  - GTF.
  - Collapse manifest keyed to exact source graph path names plus source transcript IDs.
  - `.xg` for the same graph with visible source-coordinate paths used by the GTF.
  - The public `vg mpmap` wiki documents `-x graph.xg -g graph.gcsa` as normal mpmap
    inputs and `-d graph.dist` for RNA/splice-variation graph mapping. These are
    upstream mapper artifacts; after GAMP exists, panCollapse needs the matching `.xg`
    for coordinate projection but not `.gcsa`, `.gcsa.lcp`, `.dist`, `.snarls`, or
    minimizer indexes.
- GAMP streaming:
  - Use `vg::io::for_each<vg::MultipathAlignment>` from
    `include/vg/io/stream.hpp`.
  - Prefer protobuf fields from `deps/libvgio/deps/vg.proto` plus
    handlegraph interfaces over VG's internal `multipath_alignment_t`.
- Coordinate lookup:
  - Local evidence supports `xg::XG` from `include/xg.hpp` as a
    `handlegraph::PathPositionHandleGraph`.
  - Use `include/handlegraph/path_position_handle_graph.hpp` for path
    position operations.
  - The VG programming wiki demonstrates GBZ access through GBWTGraph as normal
    `handlegraph::HandleGraph` access. That does not satisfy the V1 coordinate contract
    by itself, because panCollapse needs path-position operations. GBZ/GBWT remain
    future alternatives until they pass an equivalent projection and dependency-boundary
    proof.
- GAMP fields required by panCollapse:
  - `MultipathAlignment`: `annotation`, repeated `subpath`, repeated `start`.
  - `Subpath`: `path`, repeated `next`, `score`, repeated `connection`.
  - `Connection`: `next`, `score`.
  - `Mapping`, `Position`, and `Edit`: `node_id`, `offset`, `is_reverse`,
    `from_length`, `to_length`, and `sequence`.
- Multipath traversal scoring:
  - Local VG source shows `identify_start_subpaths` and topological ordering
    treat both `next` and `connection` as graph edges.
  - Optimal path score adds subpath scores.
  - Ordinary `next` edges add no score.
  - `connection.score` is additive.
  - Recommendation: implement a local DAG traversal over the protobuf
    structure instead of depending on internal VG traversal objects.
- Annotation tags:
  - `vg mpmap -C` stores FASTQ comment SAM tags under annotation key `tags`.
  - Support direct `Struct` string fields `CB`, `UB`, `CR`, and `UR`.
  - Also support SAM-tag strings such as `CB:Z:... UB:Z:...` in `tags`.
- Build discovery evidence:
  - `VGioConfig.cmake` and `libhandlegraphConfig.cmake` exist under
    `/mnt/ssd/lalli/usr/local/src/vg/lib/cmake`.
  - No exported CMake configs were found for `xg`, `gbwt`, `gbwtgraph`,
    `sdsl`, or `divsufsort`.
  - Future CMake discovery should start with
    `CMAKE_PREFIX_PATH=/mnt/ssd/lalli/usr/local/src/vg` and
    `PKG_CONFIG_PATH=/mnt/ssd/lalli/usr/local/src/vg/lib/pkgconfig`.
  - Smoke-validated link boundary: `VGio::VGio`, `libhandlegraph::handlegraph_shared`,
    local imported `/mnt/ssd/lalli/usr/local/src/vg/lib/libxg.a`,
    `PkgConfig::SDSL`, `PkgConfig::DIVSUFSORT`, `PkgConfig::DIVSUFSORT64`,
    `OpenMP::OpenMP_CXX`, `pthread`, `m`, and `atomic`.
  - Phase 1 projection smoke broadened this tracked boundary to include
    `PkgConfig::ABSL_LOG_INTERNAL_CHECK_OP` because protobuf repeated-field access in the
    local generated GAMP headers references Abseil check-op symbols.
  - `libxg.a` has no exported CMake target; panCollapse creates a local imported target
    in the Phase 1 build skeleton.
  - The Phase 0 smoke was throwaway outside this repo; Phase 1 has added a tracked build
    smoke before behavior implementation.
  - Avoid linking full `libvg.a` unless the smaller boundary proves
    insufficient.
  - The VG programming wiki uses submodule/add-subdirectory examples for general
    ecosystem-library projects, including libbdsg and GBWTGraph. panCollapse V1 should
    not vendor those libraries or expand dependencies solely from that tutorial; the
    product contract uses the existing local VG checkout/build family.

## Uncertainties

- Whether GBZ/GBWT-only loading can replace `.xg` for V1 path coordinate lookup is a
  future question. Source review suggests an overlay-based path may be possible, but it
  would add libbdsg/gbwt/gbwtgraph/zstd/crypto dependencies and requires representative
  projection fixtures before it can supersede XG.
- The standalone CMake expression for `libxg.a` transitives was first proven in a
  throwaway `/tmp` smoke and is now represented by the Phase 1 tracked build skeleton.
- The local VG checkout is dirty; version and ABI notes must be emitted in
  diagnostics but should not be treated as a portable upstream guarantee.

## Integration options

1. **Recommended: protobuf GAMP + XG path-position graph.** Stream
   `MultipathAlignment`, traverse subpath DAGs locally, extract tags from
   protobuf annotations, and query `.xg` through `PathPositionHandleGraph`.
2. **Fallback: VG internal multipath types and full VG linkage.** This may
   reuse more existing code but increases ABI and linkage exposure. It is not
   justified for V1 unless the protobuf/XG boundary fails.

## Recommended boundary

Use GAMP protobuf as the read-evidence API and `.xg` as the path coordinate API. Treat
`graph.gcsa`, `graph.gcsa.lcp`, and `graph.dist` as upstream `vg mpmap` artifacts that
do not cross the converter API boundary. V1 run metadata should identify the actual
panCollapse inputs and build/configuration, not accept mapper-index provenance flags.
Require the manifest to use exact graph path names plus source transcript IDs so source
transcript identities can be resolved deterministically before collapse.

## Primary source citations

- Version evidence: `vg version`; `git -C /mnt/ssd/lalli/usr/local/src/vg rev-parse HEAD`;
  `git -C /mnt/ssd/lalli/usr/local/src/vg describe --always --dirty --tags`.
- GAMP protobuf schema:
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libvgio/deps/vg.proto:50-53`,
  `:60-63`, `:87-91`, `:157-177`, `:181-203`.
- GAMP streaming API:
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libvgio/include/vg/io/stream.hpp:115-146`.
- VG wiki provenance:
  `https://github.com/vgteam/vg/wiki/Multipath-alignments-and-vg-mpmap`
  documents `vg mpmap -x graph.xg -g graph.gcsa` and the RNA/splice-graph
  `-d graph.dist` distance-index workflow.
  `https://github.com/vgteam/vg/wiki/Programming-with-the-vg-API`
  recommends vg-ecosystem libraries and handlegraph APIs for external programs and
  describes GBZ access through GBWTGraph as normal `HandleGraph` access.
- VG traversal/scoring reference:
  `/mnt/ssd/lalli/usr/local/src/vg/src/multipath_alignment.cpp:157-216`,
  `:497-519`, `:621-644`, `:664-682`, `:813-860`.
- `mpmap -C` tag handling:
  `/mnt/ssd/lalli/usr/local/src/vg/src/subcommand/mpmap_main.cpp:121-125`,
  `:425`, `:449-455`, `:613-614`, `:2339-2344`;
  `/mnt/ssd/lalli/usr/local/src/vg/src/alignment.cpp:159-180`;
  `/mnt/ssd/lalli/usr/local/src/vg/src/multipath_alignment.cpp:2173-2195`,
  `:2296-2303`, `:2330-2332`.
- Path-position and XG APIs:
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libhandlegraph/src/include/handlegraph/path_position_handle_graph.hpp:16-51`;
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libhandlegraph/src/path_position_handle_graph.cpp:11-15`;
  `/mnt/ssd/lalli/usr/local/src/vg/include/xg.hpp:181`, `:315-328`, `:370-375`;
  `/mnt/ssd/lalli/usr/local/src/vg/deps/xg/src/xg.cpp:2145-2185`,
  `:2275-2293`, `:2297-2308`.
- Build artifacts:
  `/mnt/ssd/lalli/usr/local/src/vg/lib/cmake/VGio/VGioConfig.cmake:6-19`;
  `/mnt/ssd/lalli/usr/local/src/vg/lib/cmake/VGio/VGioTargets.cmake:58-76`;
  `/mnt/ssd/lalli/usr/local/src/vg/lib/cmake/libhandlegraph/libhandlegraphTargets.cmake:58-70`;
  `/mnt/ssd/lalli/usr/local/src/vg/Makefile:886-893`.
- Throwaway compile/link smoke command:
  `cmake -S /tmp/pancollapse-vg-link-smoke.* -B build -G Ninja -DCMAKE_PREFIX_PATH=/mnt/ssd/lalli/usr/local/src/vg -DCMAKE_BUILD_RPATH=/mnt/ssd/lalli/usr/local/src/vg/lib`;
  the link used `libvgio.so`, `libhandlegraph.so`, `libxg.a`, `libgomp`,
  `libprotobuf.so`, `htslib`, `jansson`, `libsdsl.a`, `libdivsufsort.a`,
  `libdivsufsort64.a`, `pthread`, `m`, and `atomic`; the executable printed `0 0` after
  instantiating `vg::MultipathAlignment` and `xg::XG`.

## Risks and verification experiment

- Verify on a tiny graph that GAMP streaming preserves all subpaths, `next`
  edges, `connection` edges, scores, orientations, and annotation tags.
- Verify that `.xg` can project aligned node spans to the expected source path
  intervals for every manifest path.
- Verify that the `.xg` corresponds to the same graph/node-id space used by the GAMP and
  by the upstream mapping workflow that produced the GAMP.
- Keep the tracked Phase 1 build smoke green before production behavior broadens.
