# Phase 1 Build And Test Skeleton

Phase 1 creates the build/test skeleton required by **Gate Behavior Specified**. The
skeleton proves separation between pure policy tests and local VG-dependent tests without
creating production panCollapse implementation files.

## Created Files

```text
CMakeLists.txt
cmake/PanCollapseVg.cmake
tests/CMakeLists.txt
tests/policy/CMakeLists.txt
tests/policy/target_set_policy_smoke.cpp
tests/vg/CMakeLists.txt
tests/vg/vg_link_smoke.cpp
tests/vg/gamp_projection_fixture_writer.cpp
tests/vg/gamp_xg_gtf_projection_smoke.cpp
```

No `src/`, `include/panCollapse/`, checked-in fixture data, checked-in `.gamp`, `.xg`,
`.rad`, or panCollapse executable belongs in this phase.

## Test Labels

| Label | Meaning |
|---|---|
| `pure` | Standard-library-only tests; no VG headers or libraries |
| `policy` | Policy behavior tests |
| `vg` | Tests that use local VG headers/libraries |
| `link` | Compile/link boundary smoke tests |
| `projection` | Build-dir-only GAMP/XG/GTF projection smoke tests |

## Pure Build Verification

```bash
cmake -S . -B build-pure -G Ninja -DPANCOLLAPSE_ENABLE_VG_TESTS=OFF
cmake --build build-pure
ctest --test-dir build-pure -L pure --output-on-failure
```

## Full Local VG Verification

```bash
PKG_CONFIG_PATH=/mnt/ssd/lalli/usr/local/src/vg/lib/pkgconfig \
cmake -S . -B build -G Ninja \
  -DPANCOLLAPSE_VG_ROOT=/mnt/ssd/lalli/usr/local/src/vg \
  -DCMAKE_PREFIX_PATH=/mnt/ssd/lalli/usr/local/src/vg \
  -DCMAKE_BUILD_RPATH=/mnt/ssd/lalli/usr/local/src/vg/lib

cmake --build build
ctest --test-dir build -L pure --output-on-failure
ctest --test-dir build -L vg --output-on-failure
ctest --test-dir build --output-on-failure
```

## Boundary Assertions

- The VG smoke links `VGio::VGio`, `libhandlegraph::handlegraph_shared`, imported
  `libxg.a`, SDSL, divsufsort, divsufsort64, Abseil protobuf check-op transitives,
  OpenMP, Threads, `m`, and `atomic`.
- The VG smoke includes `vg::MultipathAlignment`, `vg::io::for_each`, `xg::XG`, and
  `handlegraph::PathPositionHandleGraph`.
- The projection smoke creates temporary GFA, GTF, GAMP, and XG files only under the
  CTest build directory; none are checked in.
- The projection smoke verifies GTF 1-based-inclusive to 0-based-half-open conversion,
  XG visible path lookup, `PathPositionHandleGraph::for_each_step_position_on_handle`,
  and VGio streaming of one generated `MultipathAlignment`.
- The pure smoke does not include VG headers.
- Build products remain ignored under `build*/`.
