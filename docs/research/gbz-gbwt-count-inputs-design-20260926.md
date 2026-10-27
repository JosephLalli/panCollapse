# Design: GBZ and graph-only XG + GBWT counting inputs

Status: measured design, architecture approval pending. Base `f51130675e23434b24919f558f305dad60340dec`,
branch `design/gbz-gbwt-count-inputs-20260926`. No production backend has been implemented.
This memo supersedes the input-discovery stage of [D094](../decisions.md#d094--checkpoint-evaluate-gbz-and-graph-only-xggbwt-count-inputs).

## Recommendation and scope

Add a graph-access seam at the facts consumed by projection: node sequence/length, selected
path identity/length, ordered path steps, and oriented occurrences of a node on selected paths.
Keep the existing XG path as the default. For both new inputs, use the existing GBWT r-index
(`gbwt::FastLocate`) for occurrence enumeration and immutable, in-memory prefix sums restricted
to selected paths for base positions. Store prefixes as 32-bit values when the entire path
fits, with checked 64-bit fallback. Do not build a whole-GBZ position overlay or a persistent
panCollapse-specific index.

This is an engineering input-equivalence change. The existing count bundle and annotation
policies are immutable inputs. The real-read fixture is the user-supplied joint chr20–22
setup, using its first **100,000 complete read groups / 216,700 GAMP records**. Chr21 remains
a separate graph-access and scale experiment, using the existing biological-info table to
select geometry. That table is not a runtime count ledger or a new annotation-policy artifact.

The scientific self-determinism gate has passed. Full projection-fixture and cross-input count
equivalence are implementation acceptance gates, not claims established by these graph probes.
End-to-end times for the two unimplemented backends are likewise not available yet.

Evidence root throughout this memo:

`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z`

It contains exact commands, standalone C++ probes, input hashes, raw GNU `time -v` logs, output
comparison receipts, and the frozen baseline executable. The probes are not production code.
All VG CLI construction used stock `quay.io/vgteam/vg:v1.74.1`; standalone API probes linked
the existing vg-latest libraries without rebuilding or modifying VG. Those are distinct
software roles: stock VG produced the fixtures, the pinned local libraries establish the API
implementation being considered. Record both revisions in implementation acceptance.

## 1. What the current code actually needs

Source references are relative to `f511306`, principally `src/main.cpp`. There is one current
concrete XG deserialization site (3066–3077). Ledger enumeration and length validation are at
3096–3121; score-mode node membership is at 3367–3397; non-score membership and exact positions
are at 4211–4302. These findings replace the older line references in the request.

| Projection/count route | Membership and orientation | Ordered path walk / repeated occurrences | Base positions | Length / sequence |
|---|---|---|---|---|
| Score projection | Yes | Preserve occurrence semantics; no ordered walk for scoring | No | Ledger lengths if a production ledger is supplied; sequence for quality-adjusted scoring |
| Legacy Gene / GeneFull / exon-over-intron projection, without production exact BAM | Yes | Yes, for body membership and splice geometry | No | Node lengths; sequence if quality-adjusted |
| Production exact-overlap projection / typed-union BAM | Yes | Yes, including repeated nodes | Yes | Ledger and node lengths; sequence if quality-adjusted |
| Native `count` | Yes | Yes | Yes: CLI fixes `GeneFull_Ex50pAS` | Ledger and node lengths; sequence if quality-adjusted |
| Replay from captured facts | No graph | No graph | No graph | No graph |

Native-count mode selection is at 760–777; `exact_ex50` at 3182–3185. Exact parent preparation
walks exon and body paths at 3632–3698. Splice geometry also walks paths at 3949–4059.
Quality-adjusted scoring reads node sequence in `src/pathtally_qualadj.hpp:88`; looking only
at `.cpp` files misses this requirement. The graph API inventory must therefore include
`for_each_step_in_path`, as well as the operations listed in the initial brief.

Counting does not use `follow_edges`, edge existence, graph degree, or graph traversal.
The exon adjacency edges at 3662–3675 come from consecutive steps of a path. Read adjacency
comes from GAMP mappings and multipath links, not the graph's edge set. Missing aligned nodes
still cause a hard failure (`has_node` at 3376 and 4213).

## 2. Fixed inputs and what was checked

The joint baseline XG, source PG, and guide GBWT are under:

`/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500/`

Files: `chr20_chr21_chr22.final.genic.xg`, `.final.genic.pg`, and
`chr20_chr21_chr22.final.prune_paths.gbwt`. The existing GBWT is usable: every one of the
**2,461,354 ledger paths** matches XG by exact name, length, and the complete ordered
node-ID/orientation walk. All GBWT nodes exist in XG. Rebuilding the GBWT was unnecessary.
Extra retention paths are excluded by the ledger selection, not treated as count evidence.

The bundle is the unmodified
`/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z/count-facts`.
The source GAMP and whitelist are recorded in `commands/joint_baseline.sh`; no remapping ran.
The existing read-group-aware extractor produced `joint/inputs/filtered_first100k.gamp`.
It stops at a QNAME boundary and retains all records of each selected group. Its source,
binary hash, command, group/record counts, and output hash are retained.

New standard artifacts are `joint/graphs/genic.gbz`, `genic.pathfree.pg`,
`genic.pathfree.xg`, and `paths.ri`. Their producing scripts are
`build_joint_gbz.sh`, `build_joint_pathfree.sh`, and `build_joint_rindex.sh`.
Every command and GNU-time receipt is retained. Hashes are in
`provenance/GRAPH_INPUTS.SHA256SUMS`, `joint/inputs/fixture.SHA256SUMS`, and
`joint/graphs/*.SHA256SUMS`; the bundle's own manifest retains its policy-file hashes.

On chr21, the complete audit validates **2,804,175 selected path names and walks**, containing
**4,238,280,959 steps**. The selected rows include 1,006,635 transcripts and 1,797,540 body
segments; they must not be described as 2.8 million biological transcripts. No chr21 count
bundle was created or substituted.

## 3. GBZ path sense, names, node identity, and edges

The actual metadata name composition matters even when the graph and GBWT walks are unchanged:

| chr21 artifact | GENERIC | REFERENCE | HAPLOTYPE | Selected names matching directly |
|---|---:|---:|---:|---:|
| `chr21.gbz` | 4,209,052 | 1,398,636 | 0 | 2,804,175 / 2,804,175 |
| `chr21.ref.gbz` | 4,209,052 | 0 | 1,398,636 | 1,405,539 / 2,804,175 |

Use `chr21.gbz`. Both have 5,607,688 metadata paths; the named-path count alone is not the
total metadata-path count. The reference-sample tag changes path sense and exposed names.
The research probe can recover the remaining 1,398,636 names of `chr21.ref.gbz` using VG's
short canonical naming rule, with no selected-name collisions. That fallback is recorded
as a probe transformation, not silently accepted production behavior. The first implementation
should require exact metadata-composed ledger names and fail clearly on unmatched names;
it does not need a naming rewrite for either chosen fixture. A later naming extension must
test fragments, phase blocks, collisions, and complete walks, rather than dropping suffixes.

The chr21 full XG, graph-only XG, and GBZ all have **2,056,621 nodes** and **34,459,835 bases**.
Their canonical SHA-256 over sorted node IDs and forward sequences is identical:

`41a16248306b6e85962fde00b8414601cbf01080bd8d2d2eff5d2e327d8685c9`

Both GBZs report no node-to-segment translation (`has_segment_names() == false`). The two
XGs have 2,726,485 edges, versus 2,701,234 in GBZ: **25,251 fewer, 0.926%**. The operations
in section 1 do not consult those edges. Complete path-walk validation separately protects
the path adjacency that counting does use.

Production must reject a GBZ carrying a translation before counting, with a message identifying
the unsupported node-ID translation. It must preserve the existing missing-aligned-node
failure for all modes. GAMP has no graph fingerprint proving identity by itself: equal node
counts alone are insufficient. Our fixture assertion is stronger—complete ID/sequence
fingerprints and selected path walks agree with the graph used to produce the fixed GAMP.

## 4. Membership and positions: avoid the expensive API path

`GBWTGraph::for_each_step_on_handle` is supported. The installed implementation examines both
node orientations, locates each occurrence, and discards reverse GBWT sequences. It can emit
HAPLOTYPE steps too, even though named-path iteration emphasizes GENERIC/REFERENCE paths.
Calling `get_path_handle_of_step` adds another locate. This is a valid oracle, but an expensive
production enumeration strategy on high-multiplicity transcript nodes.

Use `FastLocate::decompressSA(node)` for exact counting. Unlike `FastLocate::locate(state)`,
it preserves repeated occurrences. `decompressDA(node)` also preserves duplicates but returns
only sequence IDs, so it cannot alone recover path positions or distinguish step ordinals.

For a bidirectional index, query the forward encoding of a node exactly once. Decode each
packed SA value as `(sequence_id, back_offset)`:

```text
path_id = Path::id(sequence_id)
reverse_sequence = Path::is_reverse(sequence_id)
step_ordinal = reverse_sequence ? back_offset : path_steps[path_id] - 1 - back_offset
step_is_reverse = Node::is_reverse(query_node) XOR reverse_sequence
base_position = selected_prefix[path_id][step_ordinal]
```

Querying both orientations while retaining both sequence directions double-counts physical
occurrences. Endmarkers are not path steps. `fastlocate_occurrence_oracle.cpp` passed **37
assertions** on repeated-node and reverse-step paths, including the reverse-sequence and
endmarker cases. On real graphs, sorted `(selected path, strand, base position)` multisets
match XG for all sampled nodes: **581,735 selected occurrences on chr21**, **109,911 on the
joint fixture**, each over 256 fixed-seed sampled nodes. Complete selected walks were also
compared, not just these samples.

Build prefix sums during the mandatory selected-path length-validation pass. Use the existing
GBWT forward extraction to stream a path's ordered node steps. All chr21 selected path lengths
fit 32 bits; global array offsets and step totals require 64 bits. A 64-bit per-path fallback
must remain available. Prefix storage is ordinary process memory, discarded at exit.

A stock `PackedPositionOverlay` with `extra_path_names` is not a ledger-only overlay: extra
names are additive to its default named paths. It does not solve this selection problem.
The probe instead materializes precisely the selected paths. It stores no node-to-path table
and writes no custom lookup index.

## 5. Measurements and their limits

All receipts use GNU `time -v`. Graph probes use 24 threads, fixed node seed `20260926`, and
immutable inputs. Jobs ran on the shared host with uncontrolled filesystem-cache state;
single-run wall times are receipts, not isolated throughput estimates. Sampling is uniform
over GBWT-present graph nodes, not weighted by this read set. Matched query comparisons use
the same first 16 nodes and occurrence counts. Complete audit runs hold both the full XG
oracle and alternative structures, so their peak RSS is not an alternative-backend footprint.

Detailed machine-readable values and a compact table are in `results/measurement_summary.json`
and `.md`. The finalized table below is filled from those terminal receipts.

The self-contained design, including construction of FastLocate in memory:

| chr21 graph-access harness | Wall seconds | Peak RSS GiB | r-index construction seconds |
|---|---:|---:|---:|
| XG with embedded paths, using its existing positions | 71.35 | 52.663 | Not needed |
| GBZ, selected 32-bit prefixes | 229.60 | 21.572 | 92.48 |
| Graph-only XG + GBWT, selected 32-bit prefixes | 169.77 | 20.260 | 89.50 |

These processes load inputs, validate selected paths, and query 256 nodes. GBWT-backed runs
also fingerprint graph nodes and run diagnostic locate comparisons; XG already stores its
positions and uses constant-time path-length access. They do not prepare panCollapse parent
models or process GAMP. The complete-XG/GBWT oracle audit peaks of 70.222 GiB (chr21) and
38.032 GiB (joint) are intentionally excluded from this backend-footprint comparison.

The joint equivalents measured 41.29 s / 28.343 GiB for XG alone and 104.21 s / 13.879 GiB
for GBZ with in-memory FastLocate construction (38.38 s). Diagnostic runs loading a previously
built, provenance-matched r-index measured 65.90 s / 11.541 GiB for joint GBZ and
40.72 s / 10.352 GiB for joint graph-only XG+GBWT. Those latter times exclude index construction.

Targeted-position alternatives on chr21, with the existing r-index loaded:

| Position strategy | Wall seconds | Peak RSS GiB | Additional prefix payload |
|---|---:|---:|---:|
| None, GBZ membership probe | 116.41 | 3.592 | 0 |
| Eager 32-bit, ordinary GBZ | 121.45 | 19.400 | 16,964,340,536 bytes |
| Eager 64-bit, reference-tag GBZ | 121.02 | 34.974 | 33,928,681,072 bytes |
| Eager 32-bit, graph-only XG+GBWT | 74.57 | 18.079 | 16,964,340,536 bytes |

The 64-bit experiment explicitly uses the name mapping described in section 3. It measures
position storage, not acceptance of that naming contract. Prefix payload is calculated as
width times `(selected steps + selected paths)`; RSS additionally includes graph/index data,
names, mappings and allocation overhead. Eager chr21 path scanning/prefix construction took
40.17–53.69 s at 24 threads in these runs. No positions are needed for the membership-only
route, but its mandatory length validation still scans the selected GBWT paths.

Lazy positions on graph-only XG+GBWT completed in **733.92 s / 13.349 GiB**. After the same
mandatory length-validation scan (47.10 s), querying 256 nodes materialized **429,880 paths /
2,586,484,614 steps**, using 10,347,657,976 bytes of logical prefix payload. The query phase
alone took **663.46 s**, versus subsecond eager SA query phases. This prototype materializes
lazy paths in the single-threaded query loop, while eager preparation uses 24 threads; it is
not a claim about every possible concurrent lazy implementation. Nevertheless, only 256 nodes
touched 61% of selected steps, and lazy storage repeats work already done for mandatory length
validation. Eager construction is the supported first native-count choice. Membership-only
projection can avoid prefixes entirely.

On the same 16 chr21 nodes (31,737 physical occurrences), the self-contained GBZ run measured:

| Membership API | Mean milliseconds/node | Supplies exact step ordinal? |
|---|---:|---|
| GBWTGraph step enumeration + path-handle lookup | 423.86 | Not a base position |
| Ordinary per-occurrence GBWT locate, existing DA samples | 140.45 | No |
| FastLocate bulk DA | 0.813 | No |
| FastLocate bulk SA | 1.266 | Yes |

This compares identical node samples, not the differently sized samples from prior mapper
experiments. Native GBWTGraph step handles identify BWT edges; recovering a base coordinate
still needs position support. Existing chr21 DA sampling has 17,987,378 samples occupying
79,054,227 serialized bytes. In-memory `GBWT::resample(256)` was deliberately stopped after
**922.63 s total wall**, still in resampling, at **3.048 GiB observed peak**. No dense-sample
queries ran. This is a censored measurement, not a final time/memory result; see
`results/chr21_dense256_stop.md`. It does not support choosing dense DA sampling over the
completed occurrence-preserving r-index path.

Standard joint artifact preparation with stock VG 1.74.1 cost: drop paths 787.87 s / 24.14 GiB;
build graph-only XG 113.72 s / 2.02 GiB; build GBZ 129.65 s / 26.80 GiB; build a standard
r-index 95.25 s / 3.64 GiB. The stock r-index CLI used 16 threads, whereas the in-memory
constructor probes used 24 threads and the pinned local libraries; their times are not a
controlled binary-to-binary speed comparison.

The existing XG native-count baseline, with `--cr7 --pansc-strict-v1`, 32 requested threads,
64 GiB count aggregation budget, `--raw-cb-length 16 --raw-umi-length 12 --10x-mex --rad-out`,
ran twice on the named subset:

| Repeat | Wall seconds | Peak RSS GiB | Scientific comparison |
|---|---:|---:|---|
| XG 1 | 459.06 | 54.06 | Identical |
| XG 2 | 448.25 | 54.06 | Identical |

This is not the earlier full-GAMP production measurement of 3:32:49 / 137.80 GiB. Read volume,
binary state and optional outputs differ, so those numbers must not be presented as a matched
speed comparison. New-backend native-count wall/RSS will be measured after implementation.
There is no valid end-to-end chr21 count baseline without its missing policy inputs; the
approved joint replacement supplies the scientific equivalence fixture.

## 6. Proposed code boundary and CLI

Extract only graph fact collection from `main.cpp`, leaving annotation interpretation, exact
parent construction, splice ownership, scoring, molecule aggregation and output serialization
unchanged. The boundary exposes:

- Node existence, length, orientation and sequence through the existing HandleGraph API.
- Selected path enumeration/name/length with opaque, backend-local handles.
- `for_each_path_step(path, callback)` supplying ordered node, orientation, ordinal and,
  when required, base position. GBWT streaming supplies positions with a running sum.
- `for_each_node_occurrence(node, callback)` supplying the same occurrence facts via FastLocate
  and the immutable selected-prefix arrays. Repeated visits remain distinct.

Do not pretend that bare GBWTGraph implements PathPositionHandleGraph. Test equivalence of
the operations consumed by projection, using the fact boundary. A callback fact object avoids
forcing arbitrary `(path, ordinal)` into GBWTGraph's native BWT-edge step-handle representation
or retaining a second complete array of path node IDs just to support random step access.
The XG adapter forwards existing path/step operations and preserves its iteration order and
numeric handles. Existing path-name ranks remain the scientific tie-break authority; handles
must not enter exported identities. Alternate handles/order remain a differential-test risk,
not something established by this memo's microbenchmarks alone.

Proposed graph selection follows the existing long-option style:

```text
--xg transcripts.xg                         # current default, unchanged
--gbz transcripts.gbz                       # graph + transcript GBWT
--xg graph_only.xg --gbwt transcripts.gbwt    # separate graph and paths
```

Exactly one graph source is required. Reject `--gbz` together with `--xg` or `--gbwt`, and GBWT
without XG. Apply the seam to the common projection
path so the existing projection fixtures can exercise all three inputs as well as native
`count`; replay remains graph-independent.

Construct standard FastLocate once in memory from the loaded GBWT and report its initialization
cost. Do not create an implicit persistent file. The first implementation should not accept
an external r-index: its serialized header has no GBWT fingerprint, and `setGBWT()` merely
assigns a pointer. Dimension checks and sampled tuples cannot prove complete pairing; a wrong
but structurally compatible index can return wrong locations silently. Existing `.ri` artifacts
were useful for these controlled probes, with producer provenance retained. External reuse can
be added when there is an exact binding contract; it is not needed to deliver the requested
three graph inputs. Construction failure is fatal. This adds startup cost, measured separately
below, while avoiding another required input or a custom persistent index.

Select capabilities before processing: ordinary membership scoring need not allocate prefix
sums; native exact counting does. If a required capability is unavailable, report both the
input and the required capability, e.g. `GBWT input cannot provide path base positions required
by GeneFull_Ex50pAS`. Missing/ambiguous selected names, wrong path lengths, invalid step
offsets, non-bidirectional indexes, malformed metadata, absent graph nodes, and translations
are explicit errors. Do not silently skip a path or downgrade to membership-only counting.

Threading: metadata maps and eager prefixes are finalized before workers start; GBWT and
FastLocate queries are read-only and query vectors are local. Reuse the existing node ledger
cache rather than maintaining another unbounded node-occurrence cache. Lazy prefixes would
require synchronized one-time publication and duplicate-work prevention; prefer eager storage
for the first native-count implementation. Construct FastLocate before materializing the large
prefix arrays where possible, so its temporary allocations need not coexist with them. The
reported self-contained probe peaks conservatively include construction after prefix allocation;
the integrated count peak still requires measurement.

## 7. Chr2 projection

The actual chr2 guide header was measured, rather than scaling from GBZ file size. It contains
29,902,979 forward metadata paths, 59,805,958 stored sequences, and 39,137,358,154 symbols
including endmarkers. Thus the exact total of forward real steps is:

`(39,137,358,154 - 59,805,958) / 2 = 19,538,776,098`.

`results/chr2_header_stats_receipt.md` records the input path/hash, 16.23 s GBWT load and
5,691,012 KiB peak RSS. This total includes retention paths. For any subset of those paths,
32-bit prefix payload is bounded by `4 * (19,538,776,098 + 29,902,979)` bytes, about **72.90 GiB**.
The 64-bit equivalent is about **145.80 GiB**. These are prefix-payload bounds only, conditional
on per-path lengths fitting the chosen width; they exclude names, graph, r-index, allocation
overhead, count metadata, parent models and read-dependent counting state.

The existing partition receipt lists 14,952,173 biological paths and 14,950,806 retention
paths. The total-step upper bound is more conservative than assuming identical step counts
per path. Chr21 selected paths contain 95.305% of all forward real steps; carrying that fraction
to chr2 would predict 18.62 billion selected steps and about 69.4 GiB of 32-bit prefix payload.
That fraction is an assumption, not a measured chr2 subset.

An illustrative graph-only-XG+GBWT memory model using the all-step bound is roughly **100 GiB**:
72.9 GiB prefix payload, 5.43 GiB measured GBWT-load peak, about 5 GiB selected-name bookkeeping
(scaling chr21 by biological-path count), roughly 12 GiB r-index/transient allowance (scaling
the chr21 construction increment by step count), plus graph and allocation overhead. This
deliberately adds component peaks that need not coincide. It is neither a measured RSS nor
a guaranteed cap, and assumes paths fit the 32-bit case. Widening all prefixes approximately
doubles the payload component alone.

A similarly explicit linear-work model scales 40.17–53.69 s of chr21 prefix construction by
`19.539 / 4.238 = 4.61`, giving about **3.1–4.1 minutes** for that pass. Scaling the 92.48 s
r-index construction by total GBWT symbols gives about **6.8 minutes**. These are planning
estimates before graph/name loading, not runtime bounds: compression, path-length imbalance,
cache locality, library version and host contention can change them substantially. Full native
count wall/RSS also includes ledger/annotation/model and read-dependent state; it cannot be
estimated from graph loading alone. Measure the integrated backend before making a chr2
operational memory or completion-time guarantee.

## 8. Acceptance after architecture approval

1. Introduce the fact seam with the existing XG backend and require unchanged existing tests
   and XG scientific bytes. Add the two loaders and occurrence provider with explicit errors.
2. Run all projection fixtures through all three graph inputs, including Ex50 tiers, cyclic
   bodies, repeated-body resolution, isoforms, splice concordance, reverse steps and
   quality-adjusted scoring. Add missing-path/length, translated-node, malformed-index, naming
   collision, repeated-node and numeric-width boundary cases appropriate to this change.
3. Repeat the now-proven baseline on the implementation binary, then run the two new inputs
   on exactly `filtered_first100k.gamp`, the frozen bundle, whitelist and science flags.
   Require direct bytes for every scientific file and exact projected scientific counters.
   If a repeat stops being deterministic, stop before cross-mode acceptance.
4. Report GNU-time wall/RSS for each native-count mode with matched settings. Identify the
   100,000-group scope in the report; make no full-GAMP equivalence claim unless that larger
   run is actually performed. Audit retained caches and initialization peaks before a chr2 run.
5. Update README usage, `docs/input-output-contract.md`, decisions and PROGRESS when the
   interface is implemented. No new production flags are documented as available before then.

The exact scientific comparison contract is
[graph-input-scientific-comparison-20260926.md](graph-input-scientific-comparison-20260926.md).
Parquet, MEX, RAD, file inventories, table hashes and every non-excluded counter already match
in the baseline repeats. All six MEX gzip headers have mtime 0, flags 0 and no filename;
compressed bytes match directly. Raw operational manifest/summary data remain preserved.
For cross-mode manifests, extend the explicit graph-provenance exclusion list only to the
known `xg`, `gbz` and `gbwt` roles and their path/hash/size fields; validate
the permitted role combinations and retain all non-graph input checks. Do not ignore unknown
fields. Input mode belongs in provenance, never in a scientific result file.

Approval requested: the fact boundary, FastLocate occurrence enumeration, selected in-memory
prefix positions, exact-name contract and three-input CLI above. F001's custom persistent-index
restriction remains in force. Approval authorizes production implementation and its stated
acceptance work; it does not approve annotation or counting-policy changes.
