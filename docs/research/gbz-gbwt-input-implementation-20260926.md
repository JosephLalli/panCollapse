# GBZ and XG+GBWT input implementation

The approved D094 feature is implemented on `design/gbz-gbwt-count-inputs-20260926`, based
on `d655fe9`. Scientific acceptance passes on the named real subset in all three input modes.

## Interface and behavior

Both `convert` and GAMP-based `count` accept:

```text
--xg transcripts.xg
--gbz transcripts.gbz
--xg graph_only.xg --gbwt transcripts.gbwt
```

`src/graph_input.{hpp,cpp}` supplies node access, selected path names/lengths, ordered path
steps and per-node occurrences. Existing XG calls and enumeration order are forwarded.
GBWT-backed modes use standard in-memory `FastLocate::decompressSA`, retaining every repeated
visit and its physical orientation. Querying the forward node record once avoids counting the
paired reverse sequence twice. Exact modes construct selected-path prefixes, widening from
32 to 64 bits as required; membership-only modes allocate no position arrays. The node ledger
cache is reused. No new persistent index or external r-index input is introduced.

The loaders reject mixed input flags, translations, non-bidirectional GBWTs, incomplete metadata,
ambiguous selected names, absent graph nodes, missing ledger paths and wrong path lengths.
The scientific files do not contain the graph mode. Manifest and compatibility input identities
retain truthful XG/GBZ/GBWT digests. Annotation, scoring, assignment and molecule policies are
unchanged. Replay remains graph-independent and excludes all graph-input flags.

## Validation

The final XG build passes all **179 tests**. Both alternative modes pass all **167 graph-dependent
tests**, including existing native-count, exact-overlap, cyclic/repeated-body, splice, isoform,
quality-adjusted, thread-determinism and compatibility-replay fixtures. Across each alternative,
**382 scientific fixture products** match the XG results. RAD, Parquet, compressed MEX and
scientific companion tables compare directly. Native summaries use the predeclared resource
counter exclusions. Existing converter BAM command-line provenance is excluded only at `@PG CL`;
all remaining headers and exact ordered SAM records match.

The independent occurrence oracle contains five visits to node 2 across two paths, both
orientations, repeated steps, variable node lengths, excluded paths, internal sparse-ID holes
and a path-free node. It checks both backends against hand-specified positions and XG walks,
checks 32/64-bit coordinate boundaries, missing position capability, missing metadata,
non-bidirectional input and a genuinely missing GBWT node. CLI tests also check translated GBZ
and corrupt-file rejection.

Acceptance exposed and fixed an API pitfall: GBWT `contains(node)` checks alphabet bounds,
while `empty(node)` tests occurrence presence. Missing numeric IDs inside the alphabet must be
skipped; real GBWT nodes absent from the graph still fail. Tiny fixture GBZ construction also
needs unselected coverage paths to retain isolated nodes carrying unassigned-read controls.
This affects only the test builder. The audited real GBZ already retains its required nodes.

## Real-data acceptance and measurements

The approved replacement fixture is
`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/joint/inputs/filtered_first100k.gamp`:
**100,000 complete read groups, 216,700 GAMP records**, from the supplied tagged real 10x read
set and joint chr20–22 graph. The full production GAMP is not the acceptance scope claimed here.
The count bundle, whitelist and graph artifacts are reused unchanged from the design evidence.

Every timed run uses the same frozen implementation executable, `--cr7 --pansc-strict-v1`,
32 requested threads, 64 GiB count aggregation budget, 16-base barcode and 12-base UMI, and both
`--10x-mex --rad-out`. The container has 32 CPU quota and a 256 GiB limit. GNU `time -v` measures
whole-process wall/RSS, including loading, hashing, FastLocate construction and model preparation.
Host load and filesystem cache are uncontrolled. These are matched sequential observations,
not isolated causal speedups or full-GAMP throughput estimates.

The first implementation XG output matches the archived pre-feature XG baseline in every
scientific file and counter. That regression comparison permits only the executable identity
to change in addition to the documented operational exclusions; same-build comparisons do
not permit that exception. Receipt: `results/comparison_archived_xg_vs_implementation.json`.

The fresh repeated-XG determinism gate passes: all 11 scientific files, six compressed MEX
headers, and scientific summary/manifest projections match. Receipt:
`results/comparison_xg_1_vs_xg_2.json`. GBZ also passes every scientific comparison against
XG (`results/comparison_xg_1_vs_gbz.json`), as does XG+GBWT
(`results/comparison_xg_1_vs_xg_gbwt.json`). All three modes therefore pass the named scientific
acceptance scope, including compressed bytes. The acceptance runner completed with exit 0.
The comparison contract is [explicit and field-based](graph-input-scientific-comparison-20260926.md).

| Input | Whole-process wall | Peak RSS (KiB) | Scientific result |
|---|---:|---:|---|
| XG, first run | 33:52.81 | 56,711,904 | Matches archived default XG |
| XG, repeat | 25:00.07 | 56,695,940 | Matches first implementation XG |
| GBZ | 13:44.92 | 39,134,904 | Matches first implementation XG |
| XG+GBWT | 13:08.47 | 38,369,492 | Matches first implementation XG |
| Archived executable, current XG control | 15:14.99 | 56,761,504 | Matches first implementation XG |

The archived pre-feature XG times were 459.06/448.25 seconds; these fresh observations are
substantially slower and also vary between repeats. Host load/cache state were uncontrolled.
This evidence establishes scientific equivalence, not runtime nonregression against that
archive or a causal attribution of the timing difference. The additional archived-binary control
under the same current container/settings took 914.99 seconds and also matches all scientific
outputs. It is slower than its own earlier archive and faster than both fresh implementation
XG runs. Thus the scientific default is preserved, but runtime nonregression is not established;
these sequential shared-host runs do not isolate adapter overhead from changing conditions.
`commands/count_baseline_control.sh` records this single control's scope and requires completed
three-mode scientific acceptance before it runs. No performance optimization was folded into
the feature on the basis of this uncontrolled timing comparison.

## Chr2 limit

The [measured design](gbz-gbwt-count-inputs-design-20260926.md#7-chr2-projection) remains the
chr2 planning reference: 19,538,776,098 forward real steps imply a conditional 72.90 GiB upper
bound for selected 32-bit prefix payload, and an illustrative roughly 100 GiB graph-access
model including graph/index/bookkeeping allowances. Native annotation/model and read-dependent
counting memory is additional. This implementation has not been benchmarked on chr2 and does
not turn that model into a peak-RSS guarantee.

## Reproduction and evidence

Evidence root:
`/mnt/ssd/lalli/pancollapse_graph_input_implementation_v1_20260926/`.
The `commands/` directory records configure/build/test/count invocation scripts, `logs/` retains
raw GNU-time and phase logs, `results/` holds comparison receipts, and `provenance/` pins the
binary and review. Interrupted/environment-failed attempts are retained and excluded from
completed measurements. `attempts/pre_sparse_fix/` is a stopped initialization attempt, not a
count result or determinism failure.

The build uses existing `/mnt/ssd/lalli/usr/local/src/vg` libraries and GCC 15.2.0, with C++20
and `RelWithDebInfo`. This is the compatible family used by the existing panCollapse build.
The initial `vg-latest` compile encountered its generated-Protobuf/runtime mismatch and was
not used. No VG source or binary was rebuilt or changed. All VG CLI fixture steps use stock
`quay.io/vgteam/vg:v1.74.1`; the build/test runner uses the existing Ubuntu 24 development image.

Fixture mode selection is `-DPANCOLLAPSE_TEST_GRAPH_MODE=xg|gbz|xg-gbwt`, followed by
`ctest --test-dir build -L vg --output-on-failure`. `PANCOLLAPSE_TEST_VG` selects the stock VG
wrapper. `tests/vg/compare_projection_outputs.py` compares fixture trees;
`tests/vg/compare_graph_scientific.py` enforces the native scientific-output contract.
