# Progress

## CURRENT: Measured graph-input design ready for approval (2026-09-26)

**Current deliverable:** [design memo](docs/research/gbz-gbwt-count-inputs-design-20260926.md).
Recommendation: a projection-fact interface, standard FastLocate constructed from the loaded
GBWT, and eager in-memory prefixes restricted to selected paths, with checked 32/64-bit width.
No external r-index flag initially: FastLocate files do not bind themselves to a source GBWT.
No production source or default XG behavior has changed. **Gate Architecture Approved is pending.**

**Measured evidence:** repeated native XG counts on `filtered_first100k.gamp` (100,000 complete
read groups, 216,700 records) have identical Parquet, MEX, RAD and scientific counters. Wall:
459.06 / 448.25 s; RSS: 54.063 / 54.063 GiB. Compressed MEX bytes and headers are deterministic.
Every one of 2,461,354 joint ledger paths matches the supplied GBWT by exact name, length and
complete oriented walk; extra paths require no rebuild. Stock VG 1.74.1 produced the test GBZ,
graph-only XG and standard r-index without modifying source inputs or the existing count bundle.

Chr21 graph-access harnesses, including in-memory FastLocate construction where needed:
XG 71.35 s / 52.663 GiB; GBZ 229.60 s / 21.572 GiB; graph-only XG+GBWT
169.77 s / 20.260 GiB. These are not native-count backend results. All 2,804,175 selected
chr21 geometry paths match XG names/lengths/walks, and all 581,735 sampled selected occurrences
match positions/orientation. Node-ID/sequence hashes agree, no GBZ translation is present, and
the omitted 25,251 graph edges are unused by counting. A repeated/reverse-step oracle passed
37 assertions. Lazy positions finished at 733.92 s / 13.349 GiB; denser DA resampling was stopped
at 922.63 s while still building, not misreported as a completed alternative.

The chr2 guide header gives 19,538,776,098 forward real steps. The memo distinguishes the
72.90 GiB conditional 32-bit prefix-payload upper bound, an illustrative roughly 100 GiB
graph-access model, and still-unmeasured native-count/model/read memory.

**Evidence and next task:**
`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/`
contains commands, hashes, raw GNU-time receipts, source/binary pins and
`results/measurement_summary.{json,md}`. Scientific exclusions are explicitly listed in the
[comparison contract](docs/research/graph-input-scientific-comparison-20260926.md).
Worktree: `design/gbz-gbwt-count-inputs-20260926`, based on `f511306`.
After architecture approval: implement the fact interface and both input loaders; run all
projection fixtures and matched three-mode native counts on the frozen named subset; update
usage and the input/output contract. Cross-mode native-count equivalence is not yet claimed.

### Earlier intake checkpoint (superseded by the measured memo above)

**Intake gate: joint chr20–22 equivalence fixture; no policy-input blocker.** Exact chr21
corrected/projected annotations do not exist, so chr21 count-bundle construction stops; raw
projection is not substituted. The prior 2,804,175-row audit is an exact-ID intersection, not
biological transcript coverage: it comprises 1,006,635 transcripts plus 1,797,540
`panSCbody1` body-segment paths linked by `exact/annotation/body_parent_aliases.tsv`.
The joint fixture instead uses
`/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500/`
with its final XG, PG, and pre-strip GBWT, plus the production GAMP/bundle/whitelist from
`/mnt/ssd/lalli/pancollapse_filtered_native_count_v1_20260924T105938Z/commands/run_count.sh`.
No annotation-policy change is authorized or needed for this fixture.

**Authorized scope.** Produce a design memo with chr21 capability/performance measurements for
GBZ and graph-only XG+GBWT inputs, compared with the default XG route. The memo must distinguish
what requires graph positions from path/membership facts; enumerate metadata names, identity and
translation requirements, edge and targeted-position access, GBWT locate options, and measured
wall/RSS with a clearly labelled chr2 projection. Build fixture GBZ/path-free XG with stock
`quay.io/vgteam/vg:v1.74.1` for every VG CLI step.

**Boundary and gate.** This authorizes measurement and design only. Do not implement a production
input route or create a persistent panCollapse index. Chr21 bundle construction is stopped.
Validate every ledger name, node ID, and length against the joint XG; if required paths disagree,
rebuilding it from XG embedded paths with `vg gbwt -E` is authorized. Extra unselected paths alone
do not require a rebuild. Record exact
commands and input hashes. A full GAMP or an explicitly named read-contiguous GAMP subset may be
used. The first deliverable stops for review at **Gate
Architecture Approved**. The default XG contract remains operative; no GBZ substitution has been
accepted. D094 is the current checkpoint, not a product decision.

**Accepted scientific identity scope.** Result tables, MEX, RAD, and scientific counters must be
identical. Timing, memory, host, timestamps, input mode, paths, and hashes are explicit excluded
operational-provenance fields and remain truthful; input mode must not appear in scientific files.
Compression determinism passed in the repeated baseline-XG gate on
`joint/inputs/filtered_first100k.gamp` (100,000 read groups; 216,700 records): scientific
inventory, Parquet, MEX, RAD, summary projection, and manifest projection were identical;
all gzip headers had `mtime=0`, flags 0, and no filename. `xg_1`/`xg_2` were 459.06/448.25 s
and 56,688,948/56,689,116 KiB peak RSS. Receipt:
`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/joint/results/comparison_xg_1_vs_xg_2.json`.
Joint fixture probes may continue; this does not approve a production backend or architecture.

**Worktree.** `design/gbz-gbwt-count-inputs-20260926`, based on `f511306`.

**Intake completed; joint probes ongoing.** [Source/input intake](docs/research/gbz-gbwt-count-inputs-intake-20260926.md)
records verified files, mode capabilities, and source locations. The chr21 annotation route is
superseded by the joint equivalence fixture; chr21 remains useful only for graph-scale
microbenchmarks. No production graph-input route has been implemented; architecture approval remains pending.

**Existing evidence and next task.** The retained chr21 GBZ fixture is
`/mnt/ssd/lalli/hprc_v2_vg_rna/notes/evidence/chr21_gbz_indexing_fixture_20260920/README.md`:
`chr21.xg` build was 54:28.44/68.52 GiB and GBZ build 3:30.59/46.35 GiB, but its five-read mpmap
smoke test does not measure native-count capability, throughput, accuracy, or output identity.
The current production-count command and XG/bundle inputs are preserved at
`/mnt/ssd/lalli/pancollapse_filtered_native_count_v1_20260924T105938Z/commands/run_count.sh`.
Next: the parent prepares the bounded memo and measurements; do not launch a benchmark from this
documentation checkpoint.

## CURRENT: rDNA host genes as rRNA; count 2.7x faster at fixed CPU (2026-09-25)

**rDNA hosts (D092).** On the fastp-filtered real reads, 46.6% of `cr7-v1` UMIs sat in lncRNA
ENSG00000280441, whose GRCh38 model spans rDNA units; relabelling it moved them to its twin
ENSG00000278996. Both are now rRNA/FALLBACK: `cr7-v1` 1,384,692 UMIs against STAR GRCh38
1,161,119 on 6,211 cells (STAR/`cr7-v1` 0.886 on shared genes, Spearman 0.859). Next: 41 genes
(111k STAR UMIs, led by ENSG00000293050) that `cr7-v1` never counts but strict does.

**Count speed (D093).** Unlocked cache misses, 5x cache, FALLBACK-only skip: full filtered count
1:40:27 wall at 32 threads (was 4:20:47), outputs byte-identical. CPU-bound now.

**Reads.** The GAMP tail (reads 22.0M+, 95.7% STAR-unmapped) holds ~44% of subpaths; 26% of those
reads are low-complexity and hold 87% of the tail's subpaths. The fixture re-inserts the TSO and
poly-A bases Cell Ranger trims (`ts`/`pa` tags; Cell Ranger 10.1.0 trims both, score >= 20); no arm
trims them. Re-applied on user decision (2026-09-26): STAR/`cr7-v1` on shared genes 0.886 -> 0.989,
Spearman 0.859 -> 0.873; `cr7-v1` lost 54,751 UMIs mostly in genes STAR never counted (TSO/poly-A
tails placed by the graph); count 1:21:11 at 32 threads. The GAMP tail still holds 44% of subpaths
(low-complexity content). Evidence: `/mnt/ssd/lalli/star_vs_pancollapse_filtered_20260924/crtrim/`.
Evidence: `/mnt/ssd/lalli/star_vs_pancollapse_filtered_20260924/` (`README.md`,
`count_speed/TAIL_READS.md`).

## CURRENT: Read-Granular Decode, arena decode and ordered-output reorder measured for native `count` (2026-09-23)

**Read-Granular Decode (D090, accepted).** The producer groups serialized GAMP records into reads
from each record's `name` and subpath presence, and each count worker decodes, counts and
discards one whole read (`src/read_granular_input.hpp`, `src/main.cpp`). The previous reader
stays selectable with `PANCOLLAPSE_INPUT_MODE=ordered_batch`. On `w_cliff` the read phase fell
from 1,145.6 and 890.6 s to 222.8 and 207.7 s, count-worker idle from about 58% to 0.02%, and
total CPU from 10,973 and 9,084 to 6,000 and 5,611 CPU-seconds; on `w_precliff` from 23.06 to
10.96 s. Count tables byte-identical to the sealed baseline; all global counters equal. The
design's 2.5x ceiling (CPU unchanged) was wrong; why CPU fell is not profiled. Evidence:
`/mnt/ssd/lalli/pancollapse_read_granular_v1_20260923T160346Z/RESULTS.md`; design:
`docs/research/read-granular-decode-design-20260923.md`.

**Arena decode (done).** Each worker decodes into its own protobuf arena, reset per read. At
matched load on `w_cliff`: read phase 193.8 to 160.6 s, worker CPU 5,186 to 4,240 CPU-seconds,
byte-identical tables. `/mnt/ssd/lalli/pancollapse_decode_arena_v1_20260923T173617Z/RESULTS.md`.

**Ordered output (done for `count`).** A bounded reorder stage (`src/ordered_output_reorder.hpp`)
replaced the output turn for `--rad-out` and the compatibility writer. `count --rad-out` on
`w_cliff`: 1,319.8 and 1,510.5 s under the turn, 155.3 and 154.7 s under reorder, `map.rad`
byte-identical. `/mnt/ssd/lalli/pancollapse_rad_reorder_v1_20260923T182610Z/RESULTS.md`.
`convert` still decodes on its producer and uses the turn.

**Next candidates.** Count-side compute is now the whole critical path (headroom 1.05): the three
traversals per mapping and per-read ordered containers. A full-input run would confirm the
window projections (about 620 to 645 reads/s post-cliff).

**Throughput cliff cause and read filter (2026-09-23/24).** The dense GAMP tail is the block of
STAR-unmapped (bucket B) reads of the site1_donor2_asian fixture, rich in poly-G and low-complexity
reads (19.6% of a dense sample against 0.05% before the cliff, carrying 82% of subpaths; count
assigned many to two G-rich genes). By the user's decision the tier was filtered once with fastp:
23,789,806 of 25,755,749 reads kept, B bucket 35.9% removed, A 0.61%, C 1.05%; 96.1% of the dense
sample's subpaths removed. Filtered FASTQs:
`/mnt/ssd/lalli/hprc_v2_vg_rna/chr20_compare/readsets/real_scrna_benchmark_v1/fastp_filtered_v1_20260924T014617Z/tiers/site1_donor2_asian/d10k_cap20k/`
(README.md). Mapping started 2026-09-24 with the unfiltered arm's exact vg binary, indexes and flags
(`vg mpmap -n rna -t 32 -F GAMP -M 10`), detached:
`.../fastp_filtered_v1_20260924T014617Z/arms/site1_donor2_asian/d10k_cap20k/pangenome/` (STATUS reads
MAPPED_SEALED when done; `mapping/reads.gamp`, `logs/vg_mpmap.time.txt`). The user switched to the
vg v1.74.1 release image (v1.77.0 cannot load the v4 distance index): mapped all 23,789,806 reads in
7 h 20 min, GAMP 9.0 GB against 127.2 GB. Native count on it: read phase 3.39 h against 58.5 h;
cr7-v1 UMIs 2,856,170 to 2,597,568, per-gene Spearman 0.991, losses concentrated in a few G-rich
genes (ENSG00000231133 -99.2%). `/mnt/ssd/lalli/pancollapse_filtered_native_count_v1_20260924T105938Z/RESULTS.md`. The packed-key speedup of the route
tally is built and tested but unmeasured; that work was stopped in favor of filtering.

**D091 route tally (accepted, measured).** Each alignment path is scored separately and each HST
keeps its best path (user's rule: argmax over (alignment path, transcript) with ties). Implemented
in `pathtally::RouteTally` for both tally producers; `PANCOLLAPSE_STRUCTURAL_TALLY=summed` restores
D048. `cr7-v1` unchanged; `pansc-strict-v1` +60 TP / -2 FP on the 1M truth benchmark; +35% read
phase on `w_cliff` (unoptimized). Open: RAD's one-gene rule drops nearly every read because ties
span per-assembly gene identities. `/mnt/ssd/lalli/pancollapse_route_tally_v1_20260923T204025Z`.

**Branch summing (D090 fork 3, decided by D091).** The summed per-node tally decides no `cr7-v1`
assignment and 9 of 962 and 8 of 968 `pansc-strict-v1` assignments in two dense windows (via
the exact-strand Gene fallback), and it selects RAD targets. Write-up:
https://claude.ai/artifact/JNas3Gek3XzpPFn5D3LS2m ; evidence:
`/mnt/ssd/lalli/pancollapse_branch_summing_v1_20260923T160346Z`.

**Commits.** `2ce20c9` (ordered GAMP reader and test) and `2b5e466` (sampled `SpaceUsedLong`).
The D090 code sits in `src/main.cpp`, whose uncommitted diff also carries the earlier annotation,
cache and reader-wiring work, so it cannot be committed on its own without a decision about that
work.

## Completed (superseded as the frontier): Full-input throughput cliff characterized; sampled-`SpaceUsedLong` decoder fix accepted (2026-09-21)

Full narrative, evidence tables and limits:
[`docs/research/full-input-throughput-cliff-20260921.md`](docs/research/full-input-throughput-cliff-20260921.md).
The allocator-substitution result below is **no longer the frontier**; its identified next step
(reducing per-read `std::map` allocation) was measured on the 1M-read prefix, and the prefix is
now known to sit entirely inside a cheap regime that covers 85.0% of reads but only 14.2% of
read time.

**Terminology.** The counters `input_read_groups` and `completed_read_groups` name what this
codebase calls a read group: all GAMP records sharing one QNAME, i.e. one read's alignments
(`src/main.cpp:6321`, `struct Group` at `:2004`). That collides with the SAM/BAM `@RG` meaning
and is unrelated to it. New text below says **read** and **alignments per read**; historical
sections keep their original wording.

**The full-input run is terminal, validated, and not yet sealed.**
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z` finished 2026-09-18 13:23 UTC
(`REPORT_RECEIPT.json` `created_utc`) with every stage exit 0 and STATUS `VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`; sealing is
pending and is not this checkpoint's to perform. It processed 25,755,749 reads and 124,261,698
alignments in a 210,645.357663 s read phase — **817.857627 s/100k reads**, 817.985728 s/100k
including finalization — at peak RSS 137.476540 GiB. Both targets are missed by roughly fifty
times the fallback. It ran binary `3d13e082…d7d0f958` (2026-09-15 exact-evidence/resident-finalize),
**not** the frozen `6eb3e7ca…d6843a`, and predates ordered parallel input and Parent metadata
reuse, so **any absolute prefix-versus-full comparison is confounded by binary version and is not
a scale effect**. Its within-run behaviour is clean.

**The cliff.** Interval throughput holds 300–2,000 reads/s to read about 22,046,000, then
collapses inside a single roughly 12,288-read interval to about 21 reads/s and never recovers:
85.0% of reads take 14.2% of read time, the remaining 15.0% take 85.8%, mean per-read ratio 34x.
Not aggregation spill (`aggregate_spill_bytes`/`rows`/`runs` all zero), not host load (it fell),
and unlogged (11 non-progress lines in 58.5 hours). `pancollapse_cliff_characterization_v1_20260919T194550Z`
(sealed, 101 files) cut four 100,000-read windows from one pass over the source GAMP and counted
each from a **cold** process on the current binary, which excludes accumulated state. It also
excludes multimapping: post-cliff reads carry **fewer** alignments each (5.41 against 9.99
pre-cliff). The cause is graph structure per alignment — 326x the subpaths and 596x the mappings
of the pre-cliff window, with the whole distribution shifted (subpaths p50 1→6, p90 1→261,
p99 7→15,295). Measured 56,950 candidates/read confirmed a 55,700 prediction to within 2%.
Nothing crosses a software threshold: decode CPU per alignment rises 408x against 596x more
mappings, and the glibc slow-path/fast-path ratio *falls* from 0.98 to 0.33. The cost function is
smooth and the input steps.

**Open question, upstream, unresolved and prominent.** Source ordinals rise monotonically with
file position, so this GAMP is one continuous run in sequencing order, not concatenated shards.
Across ~725,000 source reads at the cliff both alignment character (median subpaths 1→34) and
retention into the GAMP (~69% locally in the tail against ~6.6% cumulative early) change
together. **Why the tail carries ~600x more graph structure per alignment is not established**;
candidates are flowcell/run-position effects, the upstream `d10k_cap20k` capping, and a
permissive fallback re-mapping pass in `pangenome_mapping_reuse_v15r2`. The retention shift
points upstream rather than at biology. If these alignments are a mapper artifact, optimizing
panCollapse to consume them faster treats a symptom, so asking the mapping pipeline owner is the
cheapest next action in the programme.

**Stage attribution corrected; one sealed root superseded.**
`pancollapse_profile_tools_v1_20260919T052434Z` (sealed, 10 files) supersedes
`pancollapse_stage_attribution_v1_20260918T192832Z` (sealed, 9 files, retained; its stage table
is wrong). Substring matching over demangled C++ signatures is unsound because a demangled
signature names every type in its template arguments: 40 symbols contain the count-worker
lambda's needle and only 20 are that lambda. Corrected shares of worker-attributable CPU: node
ledger build 15.72 → **13.13**, per-read tally maps 6.91 → **9.40**, everything else under 0.1
point. Corrected node-ledger build is 10.71% of all CPU, now equal to perf's independent
inclusive 10.712% for `ReadNodeTallyCache::expand`, a check the old number failed.

**Two hypotheses dead, one producer partition, one profile.** Raising `max_inflight_batches`
8→64→256 is **rejected** (`pancollapse_inflight_depth_v1_20260920T181428Z`, sealed, 61 files):
idle fell only 65.1→58.6→60.8% (non-monotone), headroom flat 3.55–3.72x, throughput worse than
both controls, and peak buffered bytes saturated 632/676/678/608 MB across a 32x slot increase —
admission is byte-gated, not slot-gated. Two identical controls differed 14.8% in reads/s but
0.02 points in idle, so **idle% is the trustworthy statistic here and raw rate is not**. The
producer partition (`pancollapse_producer_partition_v1_20260920T220538Z`, sealed, 27 files)
partitions a 973.1 s read phase into 910.3 s (93.5%) blocked in `wait_until_complete` on the
oldest batch, 53.9 s (5.5%) reading and framing, and 7.9 s (0.8%) grouping and submitting; the
grouping hypothesis is dead and dequeue is FIFO. The first profile ever taken on the expensive
regime (`pancollapse_cliff_profile_v1_20260920T232129Z`, sealed, 21 files; 895,383 samples,
`cpu-clock:u` at 99 Hz, no call graph, threads separated by name) finds decoder CPU as allocator
59.3%, `SpaceUsedLong()` 21.7%, parse 10.1%, and count CPU as other 61.5%, allocator 30.7%.
**Allocation is 3,569 of 9,382 CPU-s, 38% of all CPU post-cliff, against 17.13% on the prefix.**
Lock contention is dead at 1.1% and 0.6%.

**Accepted fix.** `SpaceUsedLong()` walked the whole nested object graph per record and fed only
diagnostics; it is now measured exactly once per 4 MiB of serialized bytes per decoder thread
with the rest scaled by serialized size (`src/ordered_gamp_reader.hpp:47-48`, `:355-366`).
On `w_cliff`: decoder CPU **2,123.5 → 1,810.9 s (-14.7%)**, read phase **973.1 → 874.6 s**,
throughput **102.8 → 114.3 reads/s (+11.3%)**, with the v2 arm at host load 67.6 against the
original's ~9 and still fastest. All four published tables are byte-identical to the sealed
`w_cliff` arm, non-cache counters are equal, and 77/77 focused tests pass
(`pancollapse_spaceused_fix_v2_20260921T043810Z`, sealed, 28 files; first iteration retained at
`…_v1_20260921T002651Z`, sealed, 26 files). What it did **not** achieve: the profile predicted
~461 CPU-s available and 313 were captured (68%), leaving ~148 CPU-s unexplained that needs a
decoder re-profile rather than another guess; and two diagnostics are now estimates —
`budget_oversize_records` 1,859 → 1,666 and `decoded_bytes` about 9% lower. Neither gates
anything and no published output moved.

**Where the budget stands (post-cliff, per 100,000 reads).** Decode 1,811 CPU-s over 4 threads =
452.7 s floor; counting 6,755 CPU-s over 24 workers = 281.4 s floor; read phase 874.6 s. Decode
is the long pole at 1.6x counting's floor, with 1.93x of in-order-commit penalty on top.
`decoder_threads` is hard-capped at 4 (`src/main.cpp:4967-4968`) but decoders run only ~52% busy,
so thread count is not obviously the limit.

**Next steps — none started, each requiring approval.** (a) Ask the mapping pipeline owner about
the upstream question above; it is free and could moot the rest. (b) Arena allocation for decoded
records, targeting the 38% and hitting both pools (`_int_free` alone is 19.7% of counting). Arena
support is already compiled into the vg protobuf, but it is not a drop-in: a cross-arena move
deep-copies, so `Group.records` (`src/main.cpp:2010`, moved at `:6334`) must become pointers plus
a `shared_ptr<Arena>` whose lifetime spans every group drawing from that batch. It needs a design
and human approval. (c) Re-profile the decoder for the ~148 CPU-s residual. (d) Re-check, before
acting on it, the working note that `max_buffer_bytes` never blocks and `exceeds_raw_budget` is
dead: in the current worktree `options.max_buffer_bytes` gates producer admission at
`src/ordered_gamp_reader.hpp:564`/`:581`/`:585` by draining through `commit_front()` (`:511`),
which blocks in `wait_until_complete()` (`:514`), and `exceeds_raw_budget` (`:73`) is assigned
positionally at `:600`. The 632–678 MB peak buffered against a 64 MiB default budget is itself
unexplained and is recorded as a limit.

**Uncommitted source state.** Three changes sit uncommitted alongside the pre-existing
uncommitted work on `codex/compat-replay-integration-b8210`: the validated sampled-`SpaceUsedLong`
fix and the producer-partition timers, both in the untracked `src/ordered_gamp_reader.hpp`, and
the `PANCOLLAPSE_INPUT_INFLIGHT_BATCHES` knob plus producer-partition receipt fields in
`src/main.cpp` (`:4977`), which sits inside a larger uncommitted diff containing earlier
unrelated work. The latter two are instrumentation only, inert when unset. **None has been
committed.** The gate remains the later machine-verifiable increment after Vertical Slice Proven.
Nothing here touches biological counting policy, converter performance, replay performance, or
any accuracy or F1 claim.

## Completed (superseded as the frontier): Allocator Substitution measurement — tcmalloc rejected, jemalloc not established (2026-09-18)

Authority: `/mnt/ssd/lalli/pancollapse_allocator_substitution_v1_20260918T035718Z`, STATUS
`COMPLETE_VALIDATED`, with `SHA256SUMS` sealing 379 files. Read `RESULTS.md` and
`INTERPRETATION.md` first, then `METRICS.json`, `provenance/time-receipts.md`,
`provenance/profile-reattribution/REATTRIBUTION.json` and `CALLERS.md`,
`provenance/preflight.json`, `provenance/tcmalloc-defaults.md`, and
`provenance/jemalloc-defaults.md`. Active source, the frozen binary `6eb3e7ca…d6843a`, the count
policy, and every earlier sealed evidence root are unchanged: the run made zero source, build, or
link change, and it authorizes none.

**Why the measurement was needed (attribution; re-analysis of already-sealed samples, no new run).**
The sealed root `/mnt/ssd/lalli/pancollapse_parent_metadata_profile_v1_20260917T152700Z` is
untouched; the same `profile/perf.data` was re-reported against host debug symbols, so all of its
prior conclusions stand, including exact compatibility at 29.76% inclusive CPU. The profiling
container did not mount host debug symbols, so `libc.so.6` appears in the sealed
`profile/perf.self.txt` as 15 visible rows totalling 18.68% of self CPU. That 18.68% is the total of
all fifteen visible rows, not of the unresolved ones: 13 unresolved hexadecimal address rows are
13.79% together, and the already-named `cfree` (2.45%) plus `malloc` (2.44%) are 4.89% combined. The
host carries `libc6-dbg` for glibc 2.39-0ubuntu8.9 at
`/usr/lib/debug/.build-id/a4/a7992a8e66555c8141ab2a08a8465ff6e0ea65.debug`, build-id
`a4a7992a8e66555c8141ab2a08a8465ff6e0ea65`. With those symbols the block resolves to
`__memcmp_avx2_movbe` 7.40%, `__memmove_avx_unaligned_erms` 4.92%, `_int_malloc` 3.87%,
`malloc_consolidate` 3.62%, `unlink_chunk.isra.0` 2.57%, `cfree` 2.45%, `malloc` 2.44%, and
`_int_free` 2.18%. The eight named rows sum to 29.45%, more than the 18.68% previously visible,
because the sealed reports used `--percent-limit 0.5` (`commands/run_profile.py`): sub-threshold
per-address buckets were invisible and are now aggregated into their functions. Self shares may be
summed; inclusive shares may not. The symbolized report is now persisted under the allocator root
as `provenance/profile-reattribution/perf.self.symbolized.txt`, `perf.self.libc-only.txt`,
`sealed-report.libc-rows.tsv`, and `REATTRIBUTION.json`, so the premise is citable evidence rather
than a session record. Allocator machinery (`_int_malloc`, `malloc_consolidate`,
`unlink_chunk.isra.0`, `cfree`, `malloc`, `_int_free`) is **17.13% of user CPU**, larger than the
12.01% self share of the exact Ex50 evidence vector, and only 4.89% of it was previously visible as
allocator work. The `malloc_consolidate` plus `unlink_chunk.isra.0` pair, 6.19%, is glibc-specific
free-list bin maintenance with no equivalent in a thread-caching allocator, which made a load-time
substitution the cheapest available test of whether that share was recoverable without touching
source. These shares are of user-CPU samples only (event `cpu-clock:u`), so kernel-side allocation
cost such as page faults and `mmap` is excluded, and a CPU share is not recoverable time.

**What was run.** Six sequential arms of the identical frozen binary `6eb3e7ca…d6843a` at
`/mnt/ssd/lalli/pancollapse_parent_metadata_v1_20260917T105800Z/provenance/panCollapse.parent-metadata`,
on the same real first-1M-group GAMP `39d4238d…e0aa8c`, same chr20–22 XG, corrected count-fact
bundle, whitelist, both profiles, 32 requested threads / 24 count workers, 64 GiB aggregation
budget, diagnostics and fine runtime timing off — the same input and configuration as the rejected
fused-state trial. Arms differed only in `LD_PRELOAD` scoped to the native process, in the recorded
`arm_order` `glibc_a`, `tcmalloc_a`, `jemalloc_a`, `glibc_b`, `tcmalloc_b`, `jemalloc_b`: two
interleaved replicates each of glibc 2.39 (no preload), gperftools 2.18 `libtcmalloc_minimal.so.4`
(`f3ab9ee1…385131`) and system jemalloc 5.3.0 `libjemalloc.so.2` (`567efa52…3baf12`), with `glibc_a`
the reference arm. No `TCMALLOC_*` or `MALLOC_CONF` setting was applied; the effective defaults are
recorded in `provenance/tcmalloc-defaults.md` (32 MiB process-wide thread-cache bound, quoted from
the installed gperftools documentation) and `provenance/jemalloc-defaults.md` (1024 arenas on this
256-CPU host and a per-thread cache, queried through `mallctl`). Validation: all six arms PASS, the
five `OUTPUT_COMPARISON.glibc_a.*.json` files against `glibc_a` PASS, every arm published
byte-identical four tables and equal non-cache counters, the validated denominator is 1,000,000
complete read groups and 2,621,222 alignment records, and the container exited 0 with no OOM. The
fail-closed validator smoke test (`harness-smoke/VALIDATION.json`) is PASS, and the seven-check
pre-flight receipt `provenance/preflight.json` — re-run after the arms launched so it is evidence
rather than assertion — confirms one binary, one libstdc++, no pre-existing allocator override, and
each allocator loadable inside the container's bind mounts. The host carried unrelated concurrent
work throughout, including a 142 GiB independent count deliberately left untouched; per-arm host
load1 medians in `RESULTS.md` range 56.98 to 60.36 over the whole command; the sealed
`INTERPRETATION.md` quotes 63.9 to 67.0 because it takes the read phase only, after the
initialization samples, so the two ranges are the same host observed over different windows.

**The result.** Read time per 100k groups is read-phase wall time divided by the validated
1,000,000 groups; count-worker CPU seconds is the CPU consumed by the 24 count workers during the
read phase. Per arm (a, b): read **31.487 / 29.429** s/100k for glibc, **89.335 / 77.414** for
tcmalloc, **25.910 / 31.642** for jemalloc; count-worker CPU **6327.10 / 5909.62** s for glibc,
**15010.20 / 13195.20** for tcmalloc, **5077.91 / 5865.88** for jemalloc; peak RSS **72.248 /
72.151** GiB for glibc, **72.665 / 72.733** for tcmalloc, **74.104 / 74.014** for jemalloc;
initialization **447.7 / 395.0** s for glibc, **467.1 / 509.8** for tcmalloc, **360.0 / 363.5** for
jemalloc. The two glibc arms are identical in every respect, so their spread is the within-session
noise floor: **2.059 s/100k (6.76%)** on read time and **417.48 s (6.82%)** on worker CPU.

**tcmalloc-minimal at its defaults is rejected.** Against the baseline mean, read time rose
**+173.74%** and count-worker CPU **+130.50%**, roughly twenty times the floor, and both replicates
agree (+183.72% and +163.06% on read time). The mechanism is lock contention, not merely more
expensive allocation: the GNU time receipts (`provenance/time-receipts.md`) record 134.6M and 140.9M
voluntary context switches for the tcmalloc arms against 3.4M for glibc, and 1808 and 1572 s of
system time against 214 and 193 s, which is futex waiting with a spin phase before each sleep.
Workers were not starved — empty-input-queue wait stayed under 1.3% of capacity in every arm. The
documented default 32 MiB process-wide thread-cache bound, divided among 32 pipeline threads, makes
the central free list the identifiable lock; *which* lock is a hypothesis, the contention is
observed.

**jemalloc 5.3.0 at its defaults is not established as a read-time improvement at two replicates.**
The replicates disagree: round a fell 17.71% on read time and 19.74% on worker CPU, round b rose
7.52% on read time and fell 0.74% on worker CPU. Against the baseline mean, read time fell 5.52%,
which does **not** clear the 2.059 s/100k floor, and worker CPU fell 10.57%, which does clear the
417.48 s floor — but the jemalloc replicate spread is itself 19.92% on read time and 14.40% on
worker CPU, two to three times the glibc floor, so the worker-CPU effect is suggestive only and the
condition mean is not a stable estimate. Three secondary effects did replicate: initialization fell
to 360.0 and 363.5 s against 447.7 and 395.0 s, decoder CPU fell to 36.5 and 39.8 s against 62.6 and 57.8 s, and peak RSS rose 1.86
GiB to about **74.06 GiB**, which the sealed interpretation records as above the roughly 72 GiB
previously described as the high end of acceptable.

**The critique, and what it changed.** The strongest objection is that two replicates on a loaded
shared host cannot separate a 5 to 10% allocator effect from drift, and the jemalloc result shows
exactly that: `jemalloc_b` ran 3207 groups/s for its first two thirds and 2301 groups/s for the
last third, the same tail slowdown `glibc_a` showed, while `jemalloc_a` and `glibc_b` held 3860 and
3398 groups/s throughout. The receipts rule out two candidate causes: `jemalloc_b` took fewer minor
page faults than `jemalloc_a` (79.0M against 85.5M), so decay purging and re-faulting is not it, and
fewer involuntary context switches than either glibc arm (17,753 against 20,413 and 19,324), so
preemption is not it either. What remains is more user CPU per group (8234.6 s against 7520.9 s)
concentrated in the final third, consistent with concurrent host memory traffic; this run cannot
attribute it further. The consequence is that jemalloc's read-time effect is recorded as unresolved
rather than as a 5.5% gain. The tcmalloc conclusion survives unchanged: a 2.6 to 2.8 times
regression is not a drift artifact at any plausible floor.

**What it means for the wider claims.** The hypothesis that a drop-in allocator recovers the 17.13%
allocator share at zero source cost is **not supported on this host**: one candidate is
catastrophically worse and the other is unresolved at this replication and carries a 1.86 GiB RSS
cost. The 17.13% attribution itself stands and is strengthened, because the tcmalloc collapse is
only explicable if the workload really is churn-heavy and cross-thread in its allocation, which is
the same mechanism the rejected fused-state trial met from the other direction. The 28.952 s/100k
baseline from that trial is confirmed host-dependent: the same binary measured 31.487 and 29.429
s/100k here. The under-5-second aspiration and the 16.5-second practical endpoint remain unmet.
Nothing here touches biological counting policy, converter performance, replay performance, or any
accuracy or F1 claim, and the first-1M prefix is not representative full-input throughput.

**Caller attribution (where the allocator time comes from).** Over all 350,676 sealed samples,
`provenance/profile-reattribution/CALLERS.md` resolves the callers: `_int_malloc` is reached 100%
through `operator new`, so the churn is C++ object allocation rather than C buffers;
`malloc_consolidate` is triggered 84.3% from `_int_free_maybe_consolidate`; `free` is called 57.5%
from the destructor of
`std::_Rb_tree<unsigned int, std::pair<const unsigned int, pathtally::CompactCollapsedIdentityTally>>`,
13.2% from `AssignmentCandidate::~AssignmentCandidate`, 11.4% from
`std::vector<AssignmentFacts>::~vector`, and the rest from two further `std::_Rb_tree<unsigned int,
...>` destructors; `_int_free` adds protobuf `RepeatedPtrFieldBase::DestroyProtos` and
`vg::Mapping::~Mapping`. Every element of a `std::map` is a separately heap-allocated node, and
these maps are built and destroyed once per read. The sites are `src/pathtally.hpp:196-197` (the
`exon` and `body` `std::map<uint32_t, CompactCollapsedIdentityTally>` members that
`collapse_ranked_identity_tallies` at `src/pathtally.hpp:201` returns per read), `src/main.cpp:5276`
to `:5287` (six per-read `std::map<uint32_t, ...>` instances, beside optional maps at `:5235` and
`:5236`), and `src/direct_count_assignment.hpp:93-96` (the two `std::vector<AssignmentCandidate>`
inside `AssignmentFacts`). `__memcmp_avx2_movbe` (7.40% self) was attempted with the same method and
**not obtained**: 99.9% of its 25,993 samples have no unwindable caller, because the hand-written
assembly leaf carries no frame information the DWARF unwinder can use. It remains an open target
requiring a different capture.

**Next bounded step — identified here, and since superseded as the frontier (2026-09-21).** The
per-read `std::map` allocation target below was identified from the 1M-read prefix, which is now
known to sit inside the cheap regime covering 85.0% of reads but only 14.2% of read time; the
current frontier is the post-cliff decoder cost recorded in the CURRENT section above and in
`docs/research/full-input-throughput-cliff-20260921.md`. As originally written: Reduce the allocation rate at its
source rather than change the allocator: replace the per-read `uint32_t`-keyed `std::map` instances
in the identity-collapse and resolve path with a representation that does not allocate per element
(a sorted flat vector or a worker-owned reusable container), and reuse the `AssignmentFacts` vectors
across reads, preserving iteration order wherever published output depends on it. It is a source
change and needs its own design, review, and matched measurement with byte-identical tables. It is
not another allocator swap, and nothing in this run authorizes it.

**Open user decision.** Whether to pursue jemalloc further is open, not decided. Doing so would
need at least four replicates on a quieter host, `MALLOC_CONF` tuning to recover the 1.86 GiB RSS,
and instrumentation of the round-b slowdown. Promoting any allocator into the build is a dependency
change under `.claude/rules/languages.md` and requires a decision entry plus user approval; no such
approval exists. Out of scope as before: any CMake or link change, full-data restart, and any
converter, replay, full-input, or biological accuracy claim. The gate remains the later
machine-verifiable increment after Vertical Slice Proven.

## Completed: fused exact-DP state insertion/deduplication trial rejected (2026-09-17)

The fused candidate passed build, 77/77 focused tests, and 1/8/32-thread artifacts. It also produced byte-identical four tables, equal logical profile tables, and equal non-cache counters. The sequential unprofiled pair nevertheless rejected it: frozen/candidate read time was 289.521504/348.267754 s (28.952150/34.826775 s/100k; candidate 20.29% higher), worker CPU 5802.09/7073.95 s (21.92% higher), RSS 72.292023/93.584633 GiB, initialization 394.595157/400.072956 s, and finalization 0.240441/0.272832 s. This shared-host pair measures no gain; it does not identify a causal reason beyond the measured result.

Authority is `/mnt/ssd/lalli/pancollapse_fused_state_v1_20260917T164000Z/RESULTS.md` and `METRICS.json`; candidate binary/source/outputs are preserved there. `STATE_SCRATCH.json` reports 12,270,941,652 emitted and 12,212,154,984 retained states: only 0.479% duplicate emissions avoided. One worker transiently reached 9,268,864,400 bytes (8.63 GiB); the 8 MiB cap applies only between groups, yielding 18,045 releases and 73,330,760 final retained bytes. Near-unique states undermine the proposed fused-deduplication mechanism. `provenance/SOURCE_RESTORATION.json` is PASS: only the three trial files were restored to exact pretrial bytes, so active code and baseline remain Parent-metadata binary `6eb3e7ca…d6843a`. The prior 27.755 s/100k observation used different conditions and is historical, not this pair's baseline. The trial is complete; no subsequent optimization had started at that checkpoint (one has since been accepted; see the current section). The gate remains the later machine-verifiable increment after Vertical Slice Proven.

## Completed: Optimized Parent-metadata profile validated (2026-09-17)

Authority: `/mnt/ssd/lalli/pancollapse_parent_metadata_profile_v1_20260917T152700Z/`;
start with `INTERPRETATION.md`, `RESULTS.md`, `METRICS.json`, and `OUTPUT_COMPARISON.json`.
Native count, perf capture, same-UID observer, and coordinator all exited 0. The profile captured
350,676 CPU samples with zero lost samples; one out-of-order event warning is preserved.
All four tables are byte-identical to the same-binary unprofiled reference, and logical profiles
and non-cache counters agree. Only operational cache hit/miss/uncached counts differ by one.

The fixed binary is `6eb3e7ca044f70aa40eac9f472819625d5448cff2f236a00242cabe181d6843a`,
at `/mnt/ssd/lalli/pancollapse_parent_metadata_v1_20260917T105800Z/provenance/panCollapse.parent-metadata`.
The input remains the 261,652,666-byte genuine ARC first-1M-complete-group GAMP, 2,621,222
alignment records, SHA `39d4238d…e0aa8c`, matched chr20–22 XG/count bundle/whitelist, both
profiles, 32 requested threads/24 count workers, and 64 GiB aggregation budget. Exact inputs,
commands, and hashes are in `provenance/run_config.json`. The documentation-agent checkpoint
was reviewed before launch. CPU stacks were sampled at 49 Hz only after initialization;
fine region timers were on and per-read output diagnostics off. No production code changed.

Profiled native read time was 362.208603 s (36.220860 s/100k groups), finalization 0.323943 s,
read plus finalization 36.253255 s/100k, initialization 444.555873 s, output publication
0.182191 s, whole native wall 844.77 s, and peak native RSS 72.161621 GiB. Perf trace
finalization and report generation are additional profiler work, outside native command time.
These instrumented timings are not an unprofiled speed comparison or full-input/F1 result.

Current CPU evidence identifies exact Ex50 compatibility at 29.76% inclusive / 12.01% self.
Its mapping-projection helper is 8.01% inclusive / 6.01% self; state deduplication is 4.20%
inclusive / 4.19% self. Parent/target collapse is 8.00% inclusive / 5.75% self, candidate
materialization 7.25% inclusive / 3.23% self, and assignment resolution 5.69% inclusive.
Inclusive shares overlap and must not be summed. The old Parent lookup hotspot is absent
from the >=0.5% reports. The 4.09% unsigned-path-handle-to-model-vector hash lookup is
`exact_ex50_models_by_body_path`, not a node metadata cache; source/callgraph attribution
is in the interpretation. Compact full-symbol evidence is `profile/compact/PERF_SYMBOLS.json`.

Worker elapsed regions (a different denominator) are evidence 41.968%, resolution/cache
23.207%, and tally 22.045%. Count workers average 19.737 cores, empty-input wait is 1.626%
of read-worker capacity, producer queue-space wait is 87.802%, and decoders average 0.212
core. Input starvation is not the dominant limit on this subset. Cache-lock and scheduling
costs are not separately isolated. Use `read_resources` for read-phase input wait; the broader
runtime queue-wait sum includes earlier time and is not a valid read-phase denominator.

Historical profile proposal: a worker-owned fused logical-state index was selected for trial.
The terminal trial above rejected it for performance and restored the active source; this proposal
is superseded. Body-path model lookup and broader container changes were not part of that trial.

Source and worktree state: branch `codex/compat-replay-integration-b8210`, HEAD `258f22e`,
23 commits ahead and one behind main `d4a60ae`; dirty edits are preserved. The gate remains
the later machine-verifiable increment after Vertical Slice Proven. Preserve the user-stopped
full comparison and independent older full count. This profile does not reopen biological,
converter, replay, or full-workload claims.

## Completed: immutable Parent metadata reuse and unprofiled validation (2026-09-17)

The sealed previous root `/mnt/ssd/lalli/pancollapse_parent_metadata_v1_20260917T105800Z/`
remains the unprofiled performance authority. Its fresh sequential pair observed 393.060940 →
277.545923 s read time (39.306094 → 27.754592 s/100k), 29.39% lower wall and 26.62% lower
worker CPU, with peak RSS 72.248035 → 72.155113 GiB. Shared-host load differed and build/test
work overlapped part of the baseline. All 77 focused tests, 1/8/32-thread artifact comparisons,
four-table bytes, and non-cache counters passed. Initialization binds immutable Parent facts
to validated dense ranks; fallback retains names/ranks; dynamic fields and policy are unchanged.
The <5 s/100k aspiration and 16.5 s/100k plausible endpoint remain unmet by that accepted
unprofiled result. The current profile identifies a next target without changing that result.

## Historical context: detailed real-GAMP profile (COMPLETE_PROFILE_VALIDATED, 2026-09-17)

The preceding sealed profile at `/mnt/ssd/lalli/pancollapse_small_gamp_profile_v1_20260917T051400Z/`
recorded 397,598 CPU samples with no loss: `CountFactCatalog::parent` was 16% self CPU and
`candidate` 22.02% inclusive. It selected immutable Parent reuse as the bounded target. Its
instrumented 1M-prefix timing (42.433851 s/100k read; 72.066700 GiB peak RSS) is diagnostic
context, not a speed comparison with the unprofiled terminal pair. The stopped full comparison
remains preserved and excluded from this result.

## Historical preparation: small real-GAMP detailed profiling (2026-09-17)

The user stopped the full real-input comparison and authorized a smaller chr21-compatible GAMP
with detailed profiling. The full candidate root
`/mnt/ssd/lalli/pancollapse_parallel_input_full_real_v1_20260917T011600Z` is
`STOPPED_BY_USER_CANDIDATE_EXIT_143_BASELINE_CANCELLED`. Its candidate and coordinator receipts
record exit 143, the container exit is `oom=false`, and `STOPPED.md` is the exact stop receipt.
GNU time records SIGTERM after 3:43:05 wall time and 117,540,436 KiB maximum RSS; the last stderr
checkpoint is 11,845,632 completed groups in 12,615.0 s. The queued frozen baseline did not
start. Preserve preflight, command, samples, partial output, and stop receipts; these are not a
terminal matched comparison or full-input performance result.

Historical stopped full-run configuration: the complete genuine 10x ARC GEX `site1_donor2_asian`
`reads.gamp`: 25,755,749 complete groups, 127,202,588,396 bytes, SHA-256
`000858121cdcbbc09364e423586ca57324e19c8ef0ad968a3308c91b7e7e87fe`, using the already
matched chr20--22 XG, corrected bundle, path ledger, and ARC GEX whitelist. The requested
configuration is CR7+strict, 32 threads, a 64 GiB count-aggregation budget, diagnostics off,
and fine profiling off. It compares frozen node-cache binary
`e189ba54c69d4a33c73364313e860a437585f93e0519c4fb7978bb2444ab4dba` with ordered-input
candidate `06e1892ea613d7d7e4212f43f27a015d1b8ebbc6291e41c6340aa3e0cc467f84` on this same input.

Preflight is PASS (`provenance/preflight.json`; available memory 703,372,435,456 bytes; 256 GiB
no-swap container limit). Candidate container
`pancollapse-parallel-input-full-real-20260917T011600Z` (ID
`2829a05bea4bac0ca1993b3db5b5000b6877e939b73c92057350e9ccd402901a`) started
2026-09-17T01:26:45.690675015Z. `provenance/LAUNCH.SHA256SUMS` pins command/config and launch
artifacts. Reporter fixture-v2 smoke is PASS, including deliberate rejection of a wrong worker
configuration. The frozen arm was cancelled and never started. No code, policy, converter,
replay, mapping, annotation, or biological change is authorized.

The independent original full `3d13…f958` count at
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z` remains active and must be
preserved. Its supplied latest checkpoint is 23,097,344 groups in 80,200.6 s. It cannot isolate
the ordered-reader effect and must not be stopped, restarted, replaced, or used as either matched
arm. Its incomplete rate cannot be extrapolated to terminal full-data performance. This full
sample still uses a chr20--22 graph and preselection, so it is neither whole-pangenome throughput
nor F1/accuracy evidence.

The smaller diagnostic input is now selected at
`/mnt/ssd/lalli/pancollapse_small_gamp_profile_v1_20260917T051400Z`.
Bounded inventory found only simulated HG002 for the existing small chr21-only GAMP. The
parent asked an optional real-subset versus simulated-chr21 preference; with no reply,
proceeded with the stated real-data assumption. The existing genuine ARC GEX first-1M
complete-group subset has 2,621,222 records and 261,652,666 bytes (SHA `39d4238d…e0aa8c`).
It retains the proven chr20–22 graph/bundle/whitelist route, not a chr21-only claim. This is
local hotspot attribution; the prefix remains inadequate as a full-workload benchmark.

Exact native configuration, binary `06e1892e…7f84`, and same-input output reference are in
`provenance/run_config.json`. `PANCOLLAPSE_PROFILE_TIMING=1` reports read-path worker-wall
regions, producer/admission timing, worker CPU and empty-queue time, and finalize subdivisions.
User-space CPU stacks use perf at 49 Hz with DWARF unwind, enabled after initialization and
disabled after worker join via external markers; `profile/WINDOWS.json` records acknowledgement
boundaries. Region wall timings are not CPU percentages. The profiler uses Docker CAP_PERFMON;
native count drops to the workspace UID/GID. No host sysctl change is needed. Per-read output
diagnostics remain off. The 32-thread/64-GiB-budget native configuration and 256-GiB no-swap
container are unchanged; a 40-minute bound preserves interrupted evidence if exceeded.

The documentation checkpoint was reviewed before preparation. `harness-smoke/` passed native,
perf, and coordinator exits, enable/disable acknowledgements, and symbolized 140-sample reports.
Preflight passed with 421,780,467,712 available bytes; container
`pancollapse-small-gamp-profile-20260917T051400Z` started 2026-09-17T05:19:59.208859207Z,
ID `435c57fd9f30c02c04a8b7383aafe2e98461261c2e56eb97d386e07e14dbef9f`.
Initialization is live with samples disabled. A reporting-only positive sample-header regex
fix during initialization is documented in `provenance/REPORTER_UPDATE.md` and pinned by
`REPORTER.SHA256SUMS`; counting/profiler scripts remain launch-frozen. Next: finish the profile,
validate outputs against the same-input candidate, and interpret CPU samples alongside
worker-wall timers. Do not
rerun the large input. No production optimization, biological change, or F1 claim is included.
The gate remains the later machine-verifiable increment after Vertical Slice Proven.

## Completed: ordered native-input real-1M trial (COMPLETE_BOUNDED_MEASUREMENT, 2026-09-16)

Authority and navigation: `/mnt/ssd/lalli/pancollapse_parallel_input_v1_20260916T164124Z/`
(`README.md`, `STATUS`, `METRICS.json`, `UTILIZATION.json`, `OUTPUT_COMPARISON.json`,
`frozen/manifest.json`, `commands/`, `logs/`, and `provenance/`). Coordinator exit is 0 and the
state is `COMPLETE_MEASURED_NO_SPEEDUP`. Final audit exited 0 and verified 678 files;
`FINAL_AUDIT.json`, `COMPLETION.SHA256SUMS`, and `logs/final_audit.exit.tsv` close this trial.
Candidate `06e1892ea613d7d7e4212f43f27a015d1b8ebbc6291e41c6340aa3e0cc467f84` completed the real
1M pair. Read time was 558.912 s (55.891 s/100k), versus frozen 436.828 s (43.683 s/100k);
read-plus-finalize was 55.933 versus 43.715 s/100k. Both the <5 and 16.5 s/100k targets are
unmet. This one shared-host pair is an observed regression, not a general performance claim.

All 77 focused tests, small artifacts at 1/8/32 threads, native file/stdin/cross-batch/error and
empty-input 8/32 checks pass. `OUTPUT_COMPARISON.json` is PASS: four published tables are byte
identical, non-cache counters and logical profiles agree, and summary differences are limited to
operational cache counters. This is output-equivalence evidence, not F1 or accuracy evidence.

The prior header's admission-triggered error-ordering defect and its correction are preserved at
`trial/admission-regression/README.md`: attempt 1 has slot-admission exit 1 and the corrected
case exit 0, with exact header/test hashes. The earlier `329c8…f3c8` candidate was stopped during
initialization before read processing; retained attempt-1 evidence remains intact. The fresh
frozen baseline remains terminal PASS.

The fresh matched baseline exited 0: read 436.828 s, finalization 0.325313 s, initialization
497.297 s, peak RSS 75,598,436 KiB, and wall 967.47 s. Its exact manifest is
`frozen/manifest.json`. Implementation evidence is `src/ordered_gamp_reader.hpp` and
`src/main.cpp`; acceptance tests are `tests/vg/ordered_gamp_reader_test.cpp` and
`tests/vg/direct_count_verify.py`; trial commands are under `commands/` in the authority root.

The historical original full-data `3d13…f958` run discussed below is a separate snapshot and
remains active, untouched. The bounded implementation-and-measurement phase is complete; future
tuning is unexecuted and requires a new checkpoint.

## Real-input native-count measurement (TERMINAL, validated 2026-09-18; seal pending)

**Terminal outcome (2026-09-21 reconciliation).** This run completed on 2026-09-18 at 13:23 UTC (08:23 local, -0500), per
`REPORT_RECEIPT.json` `created_utc`,
with every stage exit 0. Its STATUS is `VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`: measurement
and `RESULTS.md` are validated, and the completion seal has not been applied. Terminal figures are
25,755,749 reads and 124,261,698 alignment records, read phase 210,645.357663 s
(**817.857627 s/100k reads**; 817.985728 s/100k including count finalization), initialization
516.531992 s, count finalization 32.993320 s, publication 5.993920 s, whole-command wall
211,323.00 s, peak process RSS 137.476540 GiB. Both the preferred <5 s/100k and the 16.5 s/100k
fallback are missed. The text below is the historical in-flight record of the same run and its
live checkpoints are superseded by these terminal figures. Why the run is slow, and what has since
been measured and fixed, is in the CURRENT section at the top of this file and in
`docs/research/full-input-throughput-cliff-20260921.md`.

The prior fallback phase is complete and sealed: the 3M authority
`/mnt/ssd/lalli/pancollapse_native_larger_3m_v1_20260915T231536Z` records 75 files
verified with exit 0 in its completion seal, and the measured goal is complete. Its
100k/1M/3M outcomes and limits remain prior evidence below. User authorization starts
a separate bounded measurement phase: run the frozen resident binary on an existing,
genuine biological real-read GAMP and measure native read plus count-finalization per
100k, initialization, diagnostic merge, publication, whole wall time, and peak RSS.

The selected input is the sealed, genuine 10x ARC GEX `site1_donor2_asian` GAMP
`/mnt/ssd/lalli/hprc_v2_vg_rna/chr20_compare/readsets/real_scrna_benchmark_v1/pangenome_mapping_reuse_v15r2_20260908T150525-0500/site1_donor2_asian/d10k_cap20k/mapping/reads.gamp`
(127,202,588,396 bytes; 25,755,749 groups; SHA-256
`000858121cdcbbc09364e423586ca57324e19c8ef0ad968a3308c91b7e7e87fe`). It uses the
matching chr20--22 XG and path ledger in
`/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500`;
the ledger is SHA-256 `73c7a7a03abcbb9eb795c3ac2b1141cca82333c73134b190ee1a30be8657f10c`
and both graph and ledger match corrected-bundle content
`sha256:f1ffba4d4fe821e6bda6d0b88bba3cfe5df9df5fb263efe099e27fe14146c99e`.

The selected tier has 6,211 eligible called cells. Its pre-mpmap throughput mix is
A=chr20--22, B=unmapped, and C=elsewhere MAPQ<10 (D excluded), so it is not a
whole-pangenome graph or chromosome-only throughput claim. GAMP names encode raw GEX
CR (CB16/UMI12). The new checksum-pinned ARC GEX whitelist is
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/inputs/737K-arc-v1.gex.txt`
(736,320 barcodes; SHA-256 `fde6ac9705089a87cb078993500af6bcac89176953fac46f2173c6dd1b613108`),
with provenance in `provenance/10x_chemistry_source.json`; it was not part of the
original frozen producer. The `whitelist_preflight.json` there is PASS.

No mapping, converter, replay, annotation, or biological-policy change is authorized,
and real-data time/RSS does not establish truth-based accuracy. The frozen executable is
`/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z/provenance/panCollapse.resident-finalize`
(SHA-256 `3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`); the
worktree build is older. HEAD `258f22e` remains 23 ahead/1 behind main `d4a60ae`.

The parent preflight passed (`provenance/preflight.json`; `logs/preflight.exit.tsv=0`;
available bytes 414005080064). Native count is running in container
`pancollapse-real-native-20260916T022706Z` (ID
`e465ff51a2d64dda303c0ad61c45725ce65a2a34a8c2ab9e3569599ddd8d429b`), started
2026-09-16T02:41:19.314463216Z. Monitor
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/logs/count.stderr`,
`count.time`, and `count.host.samples`; `logs/count.exit.tsv`, `logs/coordinator.exit.tsv`,
and terminal validation remain pending. It runs 32 workers, CR7+strict, a 64 GiB count
aggregation budget, and a 256 GiB Docker total limit with no swap; diagnostics are off
and fine timing is unset. Output is `counts/`. Do not duplicate or restart.
The run root now also contains `commands/report_result.py` and `commands/report_after_count.sh`.
A separate 512 MiB no-swap observer, `pancollapse-real-report-20260916T022706Z`, waits for
successful count and coordinator receipts, then writes `RESULTS.md`, `REPORT_RECEIPT.json`,
and `logs/report.exit.tsv`. Python and shell syntax checks passed. It never promotes a failed
or interrupted count; successful reporting still leaves documentation reconciliation and the
completion seal pending. Existing upstream selection receipts were copied to
`provenance/source_selection/`: A/B/C = 7,417,531 / 4,964,736 / 13,373,482 reads;
these are pre-pangenome-mapping categories, not native outcomes or accuracy labels.
At 2026-09-16T05:58:12Z, the latest recorded live checkpoint reported 7,290,880 completed
groups in 11,292.7 s (645.625 groups/s; 154.88802 s/100k cumulative read-only partial).
The 16.5 s/100k target for this full input is already impossible: its entire read-plus-finalize
budget is 4,249.698585 s, below the read time already elapsed. This does not change the prior
synthetic-fixture result; the real run remains active and is not a completed benchmark.
RSS at that observation was 100,498,312 KiB (95.84266 GiB), not terminal peak. Initialization and finalization
are excluded; throughput is not stable and neither value estimates whole-run wall time. The
worker snapshot `logs/worker_snapshot.20260916T0321Z.txt` is follow-up evidence only: most
workers were futex-waiting and a few running; wait-capable shard-cache, aggregation-lock, and
queue paths do not establish a cause or deadlock.
The current gate remains the later machine-verifiable increment after Vertical Slice Proven.

### Authorized bounded cache trial (documentation checkpoint, 2026-09-16)

The user has authorized one bounded cache experiment, superseding the earlier
no-new-experiment boundary only for that trial. Required evidence is real-input
runtime and peak-RAM measurement plus same-authority output-equivalence validation.
At trial authorization, the original full real count's recorded checkpoint was
**22,159,360 groups in 35,443.3 s**; do not stop, restart, or replace it, and preserve
all earlier outputs and receipts.

The clarified trial is **per-read node-to-tally caching/aggregation**: obtain a node's
tally metadata once per read, so repeated node occurrences do not repeat metadata-cache
lookups or member-reference-path hash updates. Current tally is a
`flat_hash_map<raw path handle, HstTally>` and maps each path to the maximum Parent
score by exon/body evidence and then transcript. Equivalence must retain numerical
scores, orientation, duplicate path steps, and the raw-path-to-Parent maximum; exact
matching is separate. No score, Parent, filter, comparator/window, or biological-policy
change is approved, and this creates no F1 claim.

The earlier assignment-cache-capacity interpretation was wrong. Preserve
`/mnt/ssd/lalli/pancollapse_cache_trial_v1_20260916T124251Z`: its first-1M-complete-group
subset was extracted with the official VG C++ reader/writer. The frozen-native pair exited
143 after intentional SIGTERM during initialization in `pancollapse-cache-pair-20260916`;
no wrong candidate count launched. The 8x-capacity candidate passed 74/74 tests, but its build
wrapper exited 129 at Git-provenance collection; preserve its binary and receipt. The temporary
capacity edit and test assertion were reverted before corrected implementation, restoring `src/main.cpp` to SHA-256
`dc74cccad9de07e92bc6c77870465dfb11d0158f88692ff5a7e0bacfa79ca6bd`.

The corrected trial root is `/mnt/ssd/lalli/pancollapse_node_tally_cache_v1_20260916T125145Z`
with `README.md` and `commands/count_one.sh`. Its frozen 1M baseline ran in
`pancollapse-node-cache-baseline-20260916`, using original binary `3d13…f958` with the same
inputs, profiles, and settings. It uses the preserved first-1M-complete-group subset
`/mnt/ssd/lalli/pancollapse_cache_trial_v1_20260916T124251Z/inputs/reads.first1m.gamp`
(SHA-256 `39d4238d9c55d18d114b554665a6d3b5ad8ba97a418a11ae122b6afff2e0aa8c`; 1,000,000
groups and 2,621,222 records).

The corrected candidate is now implemented in `src/node_tally_cache.hpp` and wired through
`src/main.cpp`: worker-local node totals and memoized `NodeLedger` data perform one fanout per
distinct node, with checked-overflow fallback to the original order. SolMax review found no
concrete design or implementation defect, subject to tests and artifact parity. Candidate binary
`provenance/panCollapse.node-tally-cache` is frozen at SHA-256
`e189ba54c69d4a33c73364313e860a437585f93e0519c4fb7978bb2444ab4dba`; 76/76 focused tests pass
in `logs/focused-tests.log`. Small-artifact validation v2 is PASS at one and four workers:
count, compatibility Parquet, RAD, diagnostics, summary, and tx2gene are byte-identical. BAM
records, tags, and remaining headers are exact after normalizing only native `@PG CL` command
provenance. The matched first-1M baseline and queued candidate coordinator subsequently reached
the terminal pair reported below.

The matched trial is now terminal PASS at
`/mnt/ssd/lalli/pancollapse_node_tally_cache_v1_20260916T125145Z`:
`METRICS.json`, `RESULTS.md`, `OUTPUT_COMPARISON.json`, and `logs/coordinator.exit.tsv` record
the result. On one sequential shared-host pair, frozen/candidate read time was 630.179/552.257 s
(63.018/55.226 s per 100k), and read plus finalization was 63.120/55.321 s per 100k: an observed
12.37% lower candidate read time. Initialization was 516.849/593.711 s, finalization
1.017/0.953 s, publication 0.340/0.297 s, wall time 1201.76/1197.19 s, and peak RSS
72.161/72.110 GiB. All four published tables are byte-identical; non-cache counters and logical
profile tables agree. Summary differs only in four operational assignment-cache counters (hits
824,101/824,105); exact search candidates are identical at 626,965,346 before and 340,444,150
after prefilter. This does not establish whole-pangenome throughput or biological accuracy, and
both the <5 and 16.5 s/100k targets remain unmet.

Supplemental sanitizer evidence is unavailable: `sanitizer-test/RECEIPT.json` records Abseil
constexpr compilation and ASan runtime failures before the oracle, so it is neither a candidate
defect nor a sanitizer pass. Completion integrity is recorded in `FINAL_AUDIT.json`,
`COMPLETION.SHA256SUMS`, and `logs/final_audit.exit.tsv`. The next unexecuted work is read-stage attribution of
remaining cost; it requires the next phase's documentation checkpoint and no further experiment
has launched.

Keep frozen resident binary `3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`,
dirty integration HEAD `258f22e889b94129024ebe8cdf0c295dfd0d12a2`, and main
`d4a60aed001bc03b00baf7ed54e39176d2266b55` discoverable with the structural sample
results. The original full real count remains active and untouched, last recorded at 22,216,704
groups in 38,209.5 s; do not restart it.

The user's fixture-versus-real throughput question is documented in the active run's
`provenance/FIXTURE_COMPARISON_INTERIM.md` (2026-09-16, approximately 06:11 UTC).
The simulated 3M fixture had 96.5469% assignment-cache hits; the real hit rate is
still unknown. Full-input GAMP bytes/group differ 41.596-fold, but this is not a
processing-cost ratio and does not describe the processed real prefix. Cache misses
build under a shard lock; hits skip candidate materialization/resolution, not all
evidence extraction. Worker waits do not establish which path is responsible.
The synthetic result cannot be treated as a production throughput estimate.
Latest checkpoint for that comparison: 7,880,704 groups / 11,900.4 s, about
151.0 s/100k read-only partial. The count and terminal-report observer remain active.

### Input/dispatch discussion checkpoint (read-only, 2026-09-16)

This session completed a **read-only design/explanation** of parallel native GAMP input and completed
group dispatch. An implementation must preserve contiguous read-group formation, validation and error behavior,
observable input order, bounded memory, and the existing core budget. No production code was
changed and no new benchmark was run; implementation remains unexecuted.

The installed VG `MessageIterator` supports multithreaded BGZF decompression, and
`for_each_parallel` performs protobuf parsing and callbacks in OpenMP tasks. Those callbacks are
not ordered, so the existing adjacent-record GAMP grouping is not a drop-in callback workload.

The only current utilization observation is the original baseline's one-minute worker sample at
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/worker-utilization-v1/`:
32 workers averaged 3.955 cores (12.3603% of their combined capacity), with 87.65% of sampled
worker states in `futex`; its main thread used 92.65% of one core. Source inspection places
native GAMP decode, grouping, validation, molecule parsing, hashing, and dispatch in the main
thread before completed groups reach workers. This makes input delivery a plausible limit but
does not distinguish empty-queue waits from cache or aggregation contention. The sealed
node-tally-cache pair (`e189…4dba`) observed 55.226 versus 63.018 s/100k on one shared-host
pair, at 72.110 GiB peak RSS, but cached-candidate utilization was not measured and this record
does not establish a parallel-input benefit. The current gate remains the later
machine-verifiable increment after Vertical Slice Proven.

### Worker-utilization observation (complete read-only, 2026-09-16)

The original frozen baseline `3d13…f958` remained active in
`pancollapse-real-native-20260916T022706Z` (host PID 4125739). Read-only sampling from
2026-09-16T15:32:35.519Z through 15:33:35.519Z recorded 121 snapshots over 60.000186 s
for its 32 workers (TIDs 340--371). The record is
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/worker-utilization-v1/`:
`SUMMARY.json`, `CONDITIONS.json`, `samples.jsonl`, and `sample.py`.

Workers accumulated 237.318824 CPU seconds: 3.95530 core-equivalents, or 12.3603% of
32-worker capacity. All process threads accumulated 4.88183 core-equivalents; the main
thread was about 0.9265 core. Across worker snapshots, state counts were sleeping 3401
and runnable 471 of 3872; 3394/3872 (about 87.65%) reported `futex` in `wchan`.
This establishes that the counting workers were far below their combined CPU capacity in this interval.

It does not establish why workers slept. Kernel `sched_schedstats` was disabled, so the
raw runqueue delta is not a valid scheduler-delay fraction; `cpu.max=max` and
`nr_throttled=0` exclude observed CPU-quota throttling. Source inspection is consistent
with a serial main-thread producer (`vg::io::for_each` at `src/main.cpp:6219`) that
decodes, validates/groups, parses molecules, and hashes input before workers process
completed groups. Its approximately 93% CPU use makes a producer limit plausible, but
does not distinguish it from cache-lock waits. Native-only mode has no ordered-output
blocking; the existing `worker_queue_wait` counter is job-queue residence, not worker
idle time. The 1466% whole-command figure belongs to the completed cache candidate and
must not be attributed to this original full run.

The sealed node-tally-cache candidate (`e189…4dba`; 76/76 tests; 63.018 to 55.226
s/100k observed pair) is terminal, so this baseline observation cannot establish
post-cache utilization. The original full count remains active, last observed at
22,396,928 groups in 45,808.3 s. No further measurement or production-source change was made.
The current gate remains the later machine-verifiable increment after Vertical Slice Proven.

### Ordered parallel native-input implementation authorized (documentation checkpoint, 2026-09-16)

The user has authorized implementation and bounded validation of the previously reviewed
ordered parallel native-GAMP input adapter. This changes the current phase from discussion to
**authorized implementation plus bounded validation**. It does not authorize a biological,
Parent-policy, replay, mapping, converter, or full-run change. The original frozen full real
count continues untouched; do not stop, restart, replace, or use it as this implementation's
benchmark.

The bounded design is: use VG `MessageIterator` threaded BGZF input, serialize byte- and
logical-count-bounded record batches, decode with a small pool, and deliver decoded records in
source order to the existing contiguous-grouping callback. The adapter must retain group
formation across batch boundaries, stdin and file behavior, current validation semantics, and
the order in which corrupt-input, wrong-tag, and slow-oldest-batch errors become observable.
Its additional memory must remain bounded. It must account for the 32-CPU budget across
decompression, decoder pool, and existing workers rather than oversubscribing it.

The sealed matched baseline for later comparison is
`/mnt/ssd/lalli/pancollapse_node_tally_cache_v1_20260916T125145Z`, candidate binary SHA-256
`e189ba54c69d4a33c73364313e860a437585f93e0519c4fb7978bb2444ab4dba`. The preserved test input is
`/mnt/ssd/lalli/pancollapse_cache_trial_v1_20260916T124251Z/inputs/reads.first1m.gamp`, SHA-256
`39d4238d9c55d18d114b554665a6d3b5ad8ba97a418a11ae122b6afff2e0aa8c` (1,000,000 groups;
2,621,222 records). Required acceptance evidence is grouping across batches; file/stdin and
error-order behavior for corrupt, wrong-tag, large-group, and slow-oldest cases; byte and
logical-count agreement; 32-CPU accounting; bounded additional memory; and a matched real-1M
time/RSS/worker-utilization observation. No previous speed observation applies to this adapter.

### Ordered parallel native-input trial (complete bounded implementation and measurement, 2026-09-16)

The trial authority is `/mnt/ssd/lalli/pancollapse_parallel_input_v1_20260916T164124Z/`.
`RESULTS.md`, `METRICS.json`, `UTILIZATION.json`, `OUTPUT_COMPARISON.json`, and coordinator exit
0 are complete; the first report failure is retained in `logs/report-attempt-1/` and
`report_only.sh` resumed reporting without a count rerun. The frozen baseline is
`e189…4dba`; the candidate is `06e189…67f84`; both used the preserved real first-1M GAMP.

`src/ordered_gamp_reader.hpp` and its `src/main.cpp` integration are now written. Native runs
with at least eight threads use ordered input; converter and sub-eight-thread behavior are
unchanged. At 32 threads the implementation budgets 24 count workers, four decoders, two
decompression threads, one BGZF I/O thread, and the main thread. The 64 MiB buffer is an
admission watermark: protobuf expansion and in-flight buffers can make resident use higher.

The candidate read time was 558.912 s (55.891 s/100k), 27.9478% higher observed than frozen
436.828 s (43.683 s/100k); read plus finalization was 55.933 versus 43.715 s/100k. Peak RSS was
72.2187 versus 72.0963 GiB. Candidate/frozen load-1 medians were 122.625/97.52 (ranges
107.85--128.22/79.32--115.35), so this shared-host pair cannot support a causal percentage
claim. A performance improvement was not demonstrated; both <5 and 16.5 s/100k targets are
unmet.

The candidate sampled 19.7851 count-worker cores across 24 workers (82.44% capacity), with
19.7566 whole-count cores; decoder CPU was 74.2963 s (0.133 core), main CPU 39.183 s, and empty
queue wait 0.668797% of count-worker capacity. These values do not establish a delivery/main
bottleneck, and the old full-run sample is another binary and workload. Peak input buffering was
160,702,112 bytes, above the 64 MiB admission watermark as documented. No tuning has started;
any follow-up requires a new checkpoint.

### Real-versus-simulated workload attribution checkpoint (read-only, 2026-09-16)

This documentation checkpoint authorizes one bounded attribution task while the
existing count continues untouched. A later live observation is nonterminal:
approximately 9.81 million completed groups in 14,029 s, with diagnostics still off.
It confirms a large throughput difference but does not establish its cause. In
particular, different full-file encoded bytes/group, upstream A/B/C selection, the
simulated fixture's cache-hit rate, and futex waits are not causal proof for the
real workload; real cache counters remain unavailable until terminal reporting.

**Exact next task:** read only small, existing representative input samples from the
sealed real GAMP and completed 3M fixture, then trace the frozen implementation's
per-read work from grouped GAMP decoding through evidence extraction, cache lookup or
build, candidate materialization/resolution, and tally/aggregation. Record only
observable workload features and the code paths that can consume them; if those facts
support it, perform a bounded non-invasive attribution observation. Do not launch a
new native count or profiling run, alter production code, restart/stop/inspectively
interfere with the active full count, or turn the listed leads into causal claims.

### Prefix-sample structural attribution (complete read-only observation, 2026-09-16)

The authorized sampler completed with exit 0 and wrote
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/attribution/`
(`sample_structure.py`, both summaries, per-group TSVs, and `concentration.json`).
It decoded the **first** 20,000 complete contiguous QNAME groups from each GAMP using
existing `vg view -Kj`; this is a prefix, not a random or whole-run-representative
sample, and it did not run native count. The fixture had 20,000 aligned groups;
the real prefix had 7,534 aligned and 12,466 unaligned groups. Across all groups,
real versus fixture had 2,494,196 versus 130,161 node-mapping visits (19.16x mean
visits/group) and 508,359 versus 68,997 subpaths (7.37x). Among aligned groups,
mean node-mapping visits were 331.059 versus 6.508 (50.87x).

The real prefix is also strongly concentrated: its top 1% of groups carry 69.05% of
node-mapping visits versus 22.20% for the fixture. Its largest group (90 bp, six
records) has 42,311 subpaths, 298,198 mapping visits, and 1,338 distinct nodes;
the fixture maximum is 2,398 mapping visits. These are concrete structural workload
differences that the frozen implementation consumes: the numeric tally loops every
mapping and then every matching reference path (`src/main.cpp:4651-4676`), while the
exact DP traverses subpath states, mappings, and links (`src/main.cpp:4398-4592`).
An assignment-cache hit is downstream and cannot remove that work.

This supersedes the generic “harder data” explanation. The observation does not
allocate a causal share of wall time, prove a real-cache disadvantage, or identify a
wait cause. The active full count and its pending terminal evidence remain untouched;
no further profiling or production change was performed here. The independent
extreme-group reread also completed: `real.extreme_group.json` records identical
90-base sequences in all six records, all 42,311 subpaths reachable, and one node
occurring 2,235 times in the first record. `validation.json` is PASS for row sums,
means, concentration, group boundaries, and extreme-record totals. The concise
authority is the attribution directory's `RESULTS.md`; scripts and exact inputs
are adjacent. One-CPU inspection containers briefly shared the host with the count.

## Full-goal native-count fallback validation (COMPLETE_FALLBACK_VALIDATED, 2026-09-15)

The authorized measured-fallback implementation and validation are complete.
Completion integrity is recorded by `COMPLETION.SHA256SUMS` and
`logs/completion_seal.exit.tsv` in the final 3M root. The final
3M authority is [RESULTS.md](/mnt/ssd/lalli/pancollapse_native_larger_3m_v1_20260915T231536Z/RESULTS.md)
and `GOAL_ACCEPTANCE.md` in that root; they supersede the earlier 3M-pending
checkpoint in the 100k/1M evidence root.

The recovered 3M chr20--22 GAMP is
`tmp/chr202122_subset/reads.gamp` (SHA-256
`3d857f957b5c585c97784e5e583dc166b63f59f8cf35045055b15e1aea708a67`), with XG
`1568ff5d682c5d11cdffaec34def02d3322d35d9e001b5fdd6bbbb35fc9849d6` and ledger
`73c7a7a03abcbb9eb795c3ac2b1141cca82333c73134b190ee1a30be8657f10c`. The pair
used the fixed corrected bundle `sha256:f1ffba4d4fe821e6bda6d0b88bba3cfe5df9df5fb263efe099e27fe14146c99e`,
32 workers, 64 GiB, CR7, and strict profiles. Frozen baseline
`/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z/provenance/panCollapse.worker-spools`
(`e48c7ddefebda46998b24e56b22473738cb1c8e185ba3df440e1de825182ced2`) and resident
`/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z/provenance/panCollapse.resident-finalize`
(`3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`) both passed
fresh identity checks. The resume exited 0 at 2026-09-15T23:52:51.781Z; final audit
passed (`final-audit/engineering-validation.json`, `logs/final_audit.exit.tsv=0`).
All four Parquet tables and the summary are byte-identical; profile tables,
non-cache counters, and cache counters agree. This is engineering performance
parity, not independent F1/accuracy evidence. The original exit-1 verifier defect
and zero-byte identity receipt remain preserved.

| 3M arm | Init s | Read s | Final s | Combined s/100k | Wall s | RSS GiB |
|---|---:|---:|---:|---:|---:|---:|
| Baseline | 523.720685 | 272.125138 | 7.801599 | 9.330891 | 834.62 | 53.473888 |
| Resident | 604.282338 | 292.485916 | 4.729907 | 9.907194 | 939.89 | 53.598610 |

At 3M, resident combined time is **6.176290% higher observed**, so no 3M speedup is
claimed. Whole-command load-1 median/range was 157.855 (143.77--171.78) baseline
and 183.04 (150.68--201.62) resident; shared-host conditions preclude causal
attribution. The 3M resident RSS is only 0.124722 GiB above baseline, consistent
with bounded scratch/cache and no material RAM growth for this pair.

The fallback remains supported by repeated 1M resident **10.878205 s/100k** median
(range 10.064581--11.691829), 27.52% lower observed than repeated baseline. The
standalone 100k result misses: 21.427850 s versus 20.355946 baseline (5.27% higher).
Preferred **<5 s/100k** remains unmet, and whole-pangenome/20M performance remains
unproven. Focused tests remain 74/74 PASS. The next unexecuted work is read-path
cost per 100k and matched whole-pangenome/20M validation, with the required
current-state documentation checkpoint before a new phase. No converter, replay,
or F1 behavior changed.

## Native-count 100k/1M validation — completed (2026-09-15)

The fallback is validated on the original **1M throughput authority**. The
subsequent 3M comparison and final assessment are complete as recorded above.
Read plus finalization: **10.878205 s/100k median** (10.064581–11.691829), versus
repeated baseline 15.008994, or 27.52% lower observed. Standalone **100k** remains
**21.427850 s** versus baseline 20.355946, or 5.27% higher observed.
**Preferred <5 s/100k, standalone-100k fallback, and whole-pangenome/20M scaling
are not demonstrated.** Shared-host variability and every run are reported.

All seven scheduled runs and nine fresh audits pass, including byte-identical
count tables, summary and complete diagnostics; input/profile identities match.
Focused tests: 74/74 PASS. Final 1M RSS: 51.8856–51.9622 GiB, similar to baseline.
Debug 100k: read 20.497832 s, finalization 0.512393 s, merge/export 0.732247 s,
publication 0.254814 s, whole wall 570.75 s, peak 48.505100 GiB.

Authoritative receipt:
/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z/RESULTS.md.
Adjacent ACCEPTANCE.md audits requirements; METRICS.json and REPEATS_REPORT.md
record all timings/host conditions; final-audit/ holds fresh comparison receipts.
The queue ended exit 0 at 2026-09-15T23:03:27Z; no prior 100k/1M or debug arm remains pending.
Final executable: provenance/panCollapse.resident-finalize in that result root,
SHA256 3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958.
The worktree build is older. Dirty HEAD 258f22e remains 23 ahead/1 behind main
d4a60ae on codex/compat-replay-integration-b8210.

Implementation: stable contiguous max-score deduplication in src/score_dedup.hpp
and src/main.cpp; checked resident reservation and omission of the redundant
first exact sort in src/direct_count_runtime.cpp. Required post-correction
sort/combine and spill behavior remain. Frozen source, tests, reviews, inputs and
commands are in the result provenance; completed/failed evidence is preserved.

Read processing remains 96.04–96.78% of combined time; the two resident 1M receipts
attribute the largest labeled worker-wall regions to evidence (53.76–56.57%),
resolution (17.67–18.98%), and tally (14.63–15.11%). Those are summed worker wall
timers, not CPU or additive whole-command times; queue wait is queue residence. The
recovered matching 3M input is now the authorized scaling point above. No converter,
replay, annotation-policy, or accuracy/F1 claim changed.

## Historical optimization checkpoints (superseded by completed result above)

The preceding bounded performance validation is complete and remains the evidence authority
below. A new authorized native-count optimization phase is now in progress. Its bounded optimizer
has exact-correct 100k data but no validated speedup: `src/score_dedup.hpp` performs in-place,
stable maximum-score state deduplication with contiguous open-address scratch scoped to one
alignment; `src/main.cpp` retains every `StateKey` field and the existing filter/window order;
and `src/direct_count_runtime.cpp` adds optional finalization profiling. The exact optimization is
in
`src/main.cpp::exact_ex50_evidence_for`, targeting temporary allocation and deduplication while
preserving evidence identity, maximum-score retention, Parent filtering, comparator/score-window
order, cross-record aggregation, structural tally, and fallback assignment. Structural tally
cannot be removed because fallback resolution consumes it. Measure read processing and count
finalization separately and also report their combined native-count processing time, including
finalization scaling; retain worker-local bounded memory and report RAM. The phase finishes with
combined native counting under 5 s/100k excluding initialization, or evidence-backed combined
performance of about 16.5 s/100k after optimization. Work cannot be shifted into count
finalization to satisfy the target; debug merge/export remains separately reported.

Same-authority biological determinism and RAM validation remain required. Strict
annotation-versus-bundle parity is not required. The current chr20--22 fixture's roughly 52.1 GiB
peak RSS is already high and cannot support a whole-pangenome RAM claim. Converter, compatibility
replay, and accuracy work remain separate. The applicable acceptance gate is later
machine-verifiable behavior after Vertical Slice Proven; this checkpoint neither advances nor
replaces that gate.

The new results workspace is
`/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z`; its status is
`BENCHMARK_IN_PROGRESS`. `tests/policy/score_dedup_smoke.cpp` passes 2,000 seeded collision,
order, reuse, and split-record cases against a map oracle; the focused suite is 74/74 PASS in
`logs/focused-tests.log`. Candidate provenance is `provenance/panCollapse.exact-dedup`, SHA-256
`e7434ac59a2fb3c5acf688ba9ca1a28827ceac14f49b2f5c78109561004809ae`, with
`provenance/source.exact-dedup.tar.gz`; the worktree build is untouched. Docker run
`pancollapse-exact-initial-pair-20260915T205001Z` is terminal: both 100k arms exit 0 and pass
exact biology plus input/execution identity. Baseline recorded 18.784944554 s read processing,
0.542118419 s finalization, and 19.327062973 s combined; candidate recorded 21.794378933 s,
0.929468246 s, and 22.723847179 s, respectively. Candidate is 17.6% slower in this initial pair;
preserve the result, but make no promotion or goal-completion claim. It does not establish speedup.
See `RESULTS.100k.md` in this results workspace.
Sol's invariant review is ACCEPT in `provenance/REVIEW.md`; the helper map-oracle ASan/UBSan check
passed, not a full native-runtime sanitizer run. The 1M candidate is terminal PASS: all four
biological tables, summary, non-cache counters, and input/execution identity are exact. It measured
567.421721609 s initialization, 119.747233383 s read processing, 13.392732078 s finalization,
133.139965461 s combined (13.3139965461 s/100k), 1.48518 s publication, 730.67 s wall, and
51.9642486572 GiB RSS. The first 1M pair is complete: baseline also passes exact biology and
identity at 15.4928523385 s/100k combined (141.787332656 s read, 13.141190729 s finalization,
51.9636306763 GiB RSS). The original candidate is about 14.1% lower combined in this pair only;
the earlier 100k candidate is 17.6% slower, so there is no established repeated speedup or goal
completion.

`provenance/FINALIZATION_INVESTIGATION.md` records that diagnostics alone cannot reconstruct
`CountRuntime` inputs exactly; no parser extraction or harness was created. Existing
`--compatibility-in` supports finalization profiling without graph initialization, but that is
separate compatibility-replay performance and was not executed. The candidate's 100k UMI collapse
was 0.0459925 s, so do not presume a quadratic UMI cause. The original candidate-then-baseline
1M pair is terminal; see `RESULTS.1m.initial.md` in the results workspace. The resident-finalization change is
implemented in `src/direct_count_runtime.cpp`: checked one-time reserve per resident vector and
omission of the redundant first exact sort, with final exact sort/combine, deferred sort, and spill
unchanged. Focused tests are 74/74 PASS in `logs/focused-tests.resident.log`; frozen binary
`provenance/panCollapse.resident-finalize` is SHA-256
`3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`, with source snapshot and
complete checksums. Build/test succeeded; wrapper exit 129 during Docker Git metadata collection is
preserved and host metadata recovery passed. First resident 1M is terminal PASS: all four tables,
summary, non-cache counters, and identity are exact at 113.148701764 s read, 3.769589644 s
finalization, and 116.918291408 s combined (11.6918291408 s/100k), with 51.9621620178 GiB RSS.
Resident prepare is 0.515448 s versus the original candidate's 10.2197 s; correction is 1.30683 s,
molecule stage 1.89924 s, and UMI collapse 0.69825 s. This is one observation: the fallback is
numerically met, but no completion or reliable gain claim precedes repeats/debug and acceptance
audit. First resident 100k is also terminal exact PASS: 20.449478765 s read, 0.664120164 s
finalization, 21.113598929 s combined, and 48.4330329895 GiB RSS, slower than the 19.327062973 s
baseline and not meeting fallback at 100k. The size-mixed evidence does not support completion or
a reliable gain claim. Resident 100k-r2 is terminal exact PASS at 21.146822184 s read,
0.595278286 s finalization, and 21.74210047 s combined, with 48.4485282898 GiB RSS. The two
resident 100k results span 21.113598929--21.74210047 s (median 21.4278496995), so neither meets
the standalone fallback. Baseline 100k-r2 is terminal exact PASS at 21.384829 s combined;
the baseline 100k median is 20.355946 s versus resident 21.427850 s (5.3% higher observed).
Resident 1M-r2 is terminal exact PASS: 96.659688 s read, 3.986118 s finalization,
100.645807 s combined (10.064581 s/100k), 51.885620 GiB RSS, and input identity PASS. The two 1M
combined values span 10.064581--11.691829 s/100k (median 10.878205), while the two 100k values
remain 21.113599--21.742100 s (median 21.427850). All same-authority four tables, summary bytes,
and non-cache counters are exact. This remains noncausal, non-full-scale evidence; acceptance is
pending. Baseline 1M-r2 is terminal exact PASS at 132.429317 s read, 12.822031 s finalization,
145.251348 s combined, and 52.036316 GiB RSS. The repeated 1M baseline spans
14.525135--15.492852 s/100k (median 15.008994); resident median 10.878205 is 27.5% lower
observed, while standalone 100k remains 5.3% higher. Six of seven scheduled arms pass;
only complete debug 100k remains in Docker
`pancollapse-resident-repeats-20260915T205001Z`
`7aebee643a0e5058cf66ce28de73a8070570f9bf09a4503a634a045930181507`; debug 100k began
22:53:56Z. Poll it; do not duplicate it. `ACCEPTANCE.md` audit is pending; `REPEATS_REPORT.md`
separates PASS/PENDING/FAILED and whole-command host load. Baseline CPU sampler ended before counting, so CPU
window comparison is unavailable; host-load samples remain. No F1 change or whole-pangenome claim
exists. `provenance/LARGER_FIXTURE_AVAILABILITY.md` found only a mismatched old 1.998M chr20 asset
(graph `1f49...`, ledger `6e40...`) versus current (`1568...`, `73c7...`); bounded search found no
ready matching authority above 1M, not global absence.
`provenance/REMAINING_PROCESSING_COSTS.md` records first-1M summed-worker-wall attribution:
evidence 53.76%, resolution 18.98%, and tally 15.11%; historical `worker_queue_wait` is job-queue
residence, not worker idle.
`provenance/PERFORMANCE_ACCEPTANCE_SCOPE.md` clarifies the full goal: the original 1M authority is
18.011 s/100k, and normalized 1M throughput may meet the fallback only if remaining repeats,
exact outputs, RSS, and complete debug evidence pass. The 100k failure remains adjacent evidence;
do not claim a blanket <=16.5 s/100k result. Larger-input validation is conditional on suitable
existing inputs, with no new mapping or mandatory 20M gate; scaling remains unproven.

## Native-count processing performance (COMPLETE_BOUNDED_PERFORMANCE_VALIDATION, 2026-09-15; prior evidence)

The bounded performance phase is complete. Use the validated final binary
`/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z/provenance/panCollapse.worker-spools`
(SHA-256 `e48c7ddefebda46998b24e56b22473738cb1c8e185ba3df440e1de825182ced2`) and its authoritative
result record [`RESULTS.md`](/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z/RESULTS.md).
On the unchanged 1M input it passed byte-identical biological count tables against `native-bundle`:
162.249680264 s read processing plus 17.857453257 s finalization equals 180.107408508 s total,
with 52.07076644897 GiB peak RSS. Diagnostics were disabled; no F1 or accuracy claim changed.

The preferred <5 s/100k processing target remains open. The next bounded work is allocation and
deduplication profiling in `src/main.cpp::exact_ex50_evidence_for`, retaining structural tally and
fallback assignment semantics and measuring RAM. Do not extrapolate finalization from 100k to 20M.

## Historical baseline and accuracy context

This completed phase targeted non-initialization per-read native `count` runtime while preserving
output semantics for each fact authority. It follows the completed
corrected-bundle/annotation 1M validation at
`/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z`; its results remain
the authority for counting accuracy and route diagnostics. Strict annotation-versus-bundle parity
is no longer an acceptance requirement. It does not permit a performance change to alter outputs
within either authority.

That recovered run used the preserved baseline binary
`251bfa0c948735fdf0ca33c346ac2ca63684679d6ccf13241d90792d0eaab763`, sourced from dirty HEAD
`258f22e889b94129024ebe8cdf0c295dfd0d12a2`, with 32 requested threads. Its same-executable
observations are annotation: initialization 577.057 s, processing 1,044.36 s, wall 1,658.51 s,
peak 52.114975 GiB; bundle: 551.372 s, 970.332 s, wall 1,549.16 s, peak 52.041714 GiB. Whole-command
CPU of 524%/541% does not isolate processing parallelism. These shared-host observations are not
a controlled speed comparison. The current chr20--22 1M fixture cannot establish whole-pangenome
peak RAM for the future >=20M-read target; its roughly 52 GiB peak is high-end acceptable now.

The 100k bundle-authority baseline at
`/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z` completed with exit 0 on the
systematic every-tenth-ordinal sample of the 1M GAMP: 100,000 groups / 106,671 records, input
SHA-256 `eadee0b7d0c00735ca043617ea831d1d8d1402941198e502f63b87bea2214f98`. It measured 461.991 s
initialization and 124.485 s processing. Optional per-read diagnostics force workers to wait for
ordered output and leave producer decoding non-dominant. The `observe_seconds=3422.09` region is
not a CPU bottleneck: `source.acquire_output` leaves its region clock running across ordered wait.

The terminal no-diagnostics production baseline at
`/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z/baseline-production-100k` exited
0: initialization 425.592 s, processing 16.523 s, 6,052.17 groups/s, 7:44.92 wall, and 48.391701
GiB maximum RSS. Worker compute was 503.273 worker-seconds, ordered wait 0, and producer active
decode 1.97841 s. Its biological comparison with the debug baseline passes: counters and all
non-diagnostic tables/summary are identical. The debug 124.485 s versus no-diagnostics 16.523 s
processing difference is 7.53x and is a diagnostics-mode effect, **not** a cache-code speedup.

The compact-cache candidate production-100k arm also exited 0: initialization 422.692 s,
processing 14.7644 s, 7:46.84 wall, and 50,721,736 KiB (48.372017 GiB) maximum RSS. Its v2
comparison passes exact equality of all four tables and biological counters. `summary.tsv` differs
only in the four operational assignment-cache counters (`entries`, `hits`, `misses`, `uncached`),
which appear both in the manifest global counters and summary; the original comparator incorrectly
failed by treating those duplicated operational values as biological. Its failed script/log remain
preserved. This is a same-fixture result, not an accuracy/F1 result or a whole-pangenome claim.

The earlier 64/65 focused-cache result is preserved as fixture evidence: synthetic
Hamming-neighbor UMIs deduplicated. After replacement with >=2-distance UMIs, all 65/65 focused
tests pass for candidate SHA-256 `8ca5c2c1a35d6bef9dae53c0d46f019ccd983873d32bf01f55eb9fa108b504d4`.

Sol accepts worker-private diagnostic streams with a cap-32 bounded final merge as the next
bounded debug-performance improvement. Final diagnostic-sidecar order remains contractual while
counting is independent of it. The spool code/review is accepted; 65/65 focused tests pass in
`logs/focused-tests.merge.log`. Its provenance binary is
`provenance/panCollapse.worker-spools` SHA-256
`e48c7ddefebda46998b24e56b22473738cb1c8e185ba3df440e1de825182ced2`, with source snapshot
`provenance/source.worker-spools.tar.gz`. Timing boundaries are `read_processing`,
`count_finalize`, `diagnostics_merge_export`, and `compatibility_finalize` when capture applies.
Existing aggregation is unchanged. The completed debug-100k spool run has 453.462356205 s
initialization, 17.281165981 s read processing, 0.484642656 s count finalization,
0.630234947 s diagnostics merge/export, and 18.396179319 s total processing; whole command is
501.31 s at 48.453689575 GiB. It is 6.77x faster than the old 124.48512534 s ordered-debug
processing run, with zero diagnostic-only ordered wait. All same-authority biological outputs are
exact and complete diagnostic Parquet bytes are identical; summary differs only in operational
cache counters. This does not meet the preferred <5 s target. Exact Parent and scientific policy,
including the existing RAD-capture block, are unchanged.

The final-binary diagnostics-disabled original-1M run is complete and passes exact biological
tables and all non-cache counters against the corrected-bundle accuracy `native-bundle`; input
identities also pass. It recorded 162.249680264 s read processing (16.225 s/100k), 17.857453257 s
count finalization, 180.107408508 s total processing (18.011 s/100k), 603.204376707 s
initialization, 842.62 s wall, and 52.07076644897 GiB peak RSS. Diagnostics were disabled.
No accuracy/F1 claim changes. The next unimplemented profiling target is
`exact_ex50_evidence_for` temporary allocation/deduplication, documented in
`provenance/NEXT_BOTTLENECK.md`. Structural tally cannot be skipped because it supports fallback
assignment; count finalization cannot be linearly projected from 100k (0.485 s) to 1M (17.857 s).
Any replacement must retain its identity, ordering, filtering, and bounded worker-local memory
rules. The user prefers under 5 s processing per 100k reads; 16.5 s per
100k is an acceptable plausible fallback. Linear same-fixture estimates are 16m40 and 55m per
20M reads, respectively; they exclude initialization/final output and are not whole-pangenome
predictions. Neither value is a mandatory acceptance gate. Same-authority biological-output
determinism and RAM validation remain required. Keep converter, compatibility replay, and accuracy
work separate. The validated binary is `provenance/panCollapse.worker-spools`
`e48c7ddefebda46998b24e56b22473738cb1c8e185ba3df440e1de825182ced2`; the worktree's original
build remains the preserved baseline. Live topology is dirty HEAD
`258f22e889b94129024ebe8cdf0c295dfd0d12a2`, 23 ahead / 1 behind main
`d4a60aed001bc03b00baf7ed54e39176d2266b55`. Full phase record and final receipts:
[`docs/research/native-count-processing-performance-20260915.md`](docs/research/native-count-processing-performance-20260915.md).

## Current production validation (completed 2026-09-15; annotation-accuracy investigation next)

The user authorized the fresh corrected bundle and subsequent 1M accuracy comparison.
Execution is at `/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z`;
see [`docs/research/parent-policy-accuracy-1m-20260915.md`](docs/research/parent-policy-accuracy-1m-20260915.md)
and the run-local README/status/exit receipts. All eleven pinned input checks passed.
Current-binary annotation counting completed with exit 0 on all 1M reads per profile.
The first policy generation exposed a
second generator defect: all configured Parent exclusion tags voted as readthrough. D088
requires exact readthrough tags. That repair passes 11 focused tests and both counting
integration suites. Attempt2 policy/support derivation is complete: 273 genes return to
PRIMARY, with Parent exclusions unchanged. Fresh category derivation reproduced all 2,461,354
retained Parent rows byte-for-byte, and the unchanged compiler completed with exit 0 using
the new source-bound receipt. Corrected bundle content ID:
`sha256:f1ffba4d4fe821e6bda6d0b88bba3cfe5df9df5fb263efe099e27fe14146c99e`.
Both native arms, decoded comparison, and both truth-scoring runs completed with exit 0.
The run's `STATUS` is `COMPLETE_WITH_STRICT_DISCREPANCY`; `RESULTS.md` and `VALIDATION.json`
record the terminal evidence. CR7 is exact across all 1M diagnostics, counts, and 699,260
molecules. Strict parity fails: 322 diagnostic assignment changes, 209 differing matrix
coordinates, and 709,623 annotation versus 709,532 bundle molecules. The strict-only cause
is projected nested-host direction and per-Parent host protection absent from annotation
loading: 199 cleared-edge effects, 86 reversed-edge effects, and 37 protection effects.

Frozen all-mapped-origin scoring covers all 1M reads and 1,024 truth genes in each of six
arms and exactly reproduces the historical scores. Corrected bundle read F1 is 0.962336
(CR7) and 0.969883 (strict), versus historical 0.957309 and 0.963628; executable and bundle
changes are both included. The separate corrected-policy open-set guard passes unchanged:
999,085 eligible and 915 background reads. False assignments on 819 CR7 / 820 strict
background reads remain a material limitation. These are read-assignment truth metrics;
matrix checks establish internal consistency and route agreement, not independent matrix
accuracy. Converter and replay performance remain separate.

The authorized production comparison is complete. The 2026-09-15 user decision supersedes
D088's strict bit-exact parity acceptance requirement: retain its 322-assignment and
209-coordinate discrepancy as diagnostic evidence, but it no longer blocks acceptance. The
projected policy receipt remains `method_search_only` / `benchmark_eligible: false`; the decision
does not promote it into biological authority, choose a biological rule, authorize reruns, or
alter sealed bundle receipts. Preserve every existing edit, successful output, and failed
preparation attempt.

**Exact next task:** an annotation-accuracy investigation under the revised gate. Evaluate
truth-based precision and recall as percentages and F1 on its conventional 0--1 scale using a
fixed, clearly stated denominator. More assignments or relaxed parity alone do not establish an
improvement. The current annotation strict result is slightly lower than the corrected bundle:
read F1 `0.96986068839009076` versus `0.96988313775162238`; gene-balanced F1
`0.94202475192394564` versus `0.94356879517102621`. No concrete implementation experiment is
specified. Before any new scientific phase, automatically run a
documentation-reconciliation agent to update the local AI-facing current state, decisions, and
research record with the intellectual state/goals, generated files and experiments, terminal
results, settled decisions, existing code, exact source/input locations, and the next gated task.
This supersedes prior future `/reload` offers; historical offers below remain as dated records.

## Comparison gene-policy generator repaired (2026-09-15)

Completed the next user-authorized bounded D088 engineering task in the isolated panSC
worktree `/mnt/ssd/lalli/panSC/.codex/worktrees/gene-policy-parent-identity`, branch
`codex/gene-policy-parent-identity`, base `61b1eaea741d86b8e3e883168b2c681c6e60cbd4`.
That base's generator is byte-identical to the frozen comparison generator (`4d9707ef...`).
The corrected `count_cr.py` is `604faf4aee96f90180b0bb5ec9af91072df2173ea63b3bb0fc3c9013a8ee0f64`.

Exact Parent IDs now preserve observations before gene aggregation; parent features take
precedence over child exons regardless of ordering, and local-ID aliases cannot transfer
a Parent's exclusion to a clean sibling. Canonical transcript/gene grouping is unchanged.
The nine new policy tests pass, both gene-competition/readthrough integration suites pass,
and eleven bundle compiler tests pass. Source snapshots, before/after logs, and checksums
are in that worktree's `results/gene-policy-parent-identity.BMr8qj/`; the full record is
`docs/gene_policy_parent_identity_repair.md` in that worktree.

At this checkpoint the generator repair was closed and D088 production acceptance remained
open. The existing comparison bundle still contained the old gene policy. The production
phase described above supersedes this checkpoint's next action; it additionally discovered
and repaired the exact-readthrough-tag predicate before building the corrected authority.
No PanCollapse production code, binary, benchmark output, or scientific score changed in
this increment. Converter performance, replay performance, and counting accuracy remain
separate. The prior resumption record below supplies the unchanged exact benchmark paths.

## Current integration state (2026-09-14; supersedes older next-action text)

Branch `codex/compat-replay-integration-b8210`, HEAD `258f22e889b94129024ebe8cdf0c295dfd0d12a2`,
has the uncommitted D088 annotation authority and additional allocation changes. It is 23 commits
ahead of the common ancestor with main; main has one separate documentation commit. Existing
edits and production benchmark outputs remain in place. Exact recovery evidence is in
[`docs/research/compat-replay-resumption-20260914.md`](docs/research/compat-replay-resumption-20260914.md).

The latest unfinished scientific validation is D088. The September 13 annotation/bundle A/B
finished and differed; it is not running. Its frozen `490643ca...db6aa` executable predates the
current `251bfa0c...ab763` executable and later readthrough/body-parent fixes. The identifier-fixed
bundle repairs Parent categories but retains the gene-policy defect documented in D088. None of
these retained production outputs proves current annotation-route parity or biological accuracy.

Completed the next bounded machine check: a graph-backed `direct_count_annotation_cli` test now
exercises the actual CLI through output publication, both profiles with eight expected UMIs,
gzip inputs, the three input digests, and six parse-time refusals. Focused CTest passes **8/8**,
including the existing annotation factory test and fixture dependencies. The pre-session log
records **166/166** passes; it was preserved before running the new checks. There are now 167
registered tests; a full 167-test run was not performed. No production C++ changed this session.

Gate: D088 accepted, under the post-Vertical-Slice rule of one machine-verifiable increment.
Large scientific acceptance remained open. Its next-action text is superseded by the completed
2026-09-15 production validation above and by the automatic documentation-reconciliation
checkpoint required before any new scientific phase.
The signature optimization's interleaved performance A/B also remains unperformed; it is a
separate task from counting-rule validation. Converter, replay, and biological results remain
separate in the recovery record.

## v0.10 reusable compatibility intermediate (validated branch, 2026-09-05)

The isolated branch `codex/read-compatibility-bundle` implements D079 without modifying the
shared PanCollapse or panSC worktrees. `panCollapse count --compatibility-out` now captures a
policy-neutral, non-BAM `pancollapse-read-compatibility-v1` bundle during the ordinary GAMP/XG
pass. `count --compatibility-in` rebinds those facts through a current corrected count-facts
bundle and emits normal count matrices, molecules, optional MEX, and per-read diagnostics without
opening GAMP or XG. No transcript or exon structure is changed.

- The intermediate has four deterministic Parquet tables plus a checksum-bound manifest. It
  retains exact E/P/B facts, structural S/U facts, raw molecule state, invalid and featureless
  rows, original order, source GAMP/XG digests, independent record/read-group denominators, and
  valid/missing/malformed/unsupported totals. It contains no alignment payload.
- Read rows stream through a checksummed ZSTD spool; repeated fact sets are interned and canonical
  final IDs make one-worker and forced four-worker bundles byte-identical. Unique fact sets remain
  resident and are the current unbounded dimension.
- The focused graph-backed gate passes 17/17 fixture stages. Direct and replay outputs match for
  biological counters, count/molecule/barcode/feature Parquet, MEX, and per-read diagnostics. A
  replayed score-window override matches a fresh GAMP/XG override. A separate two-read denominator
  case retains one malformed aligned read and one valid unaligned read. A separately authenticated
  Parent-tag variant changes CR7 counts without changing compatibility bytes; its replay matches a
  fresh direct count, while panSC-strict's configured tag relaxation remains unchanged.
- Count manifests identify the compatibility bundle as `produced` or `replayed` and bind both its
  logical content ID and exact manifest SHA-256. Replay validates the current ledger structural
  surface and compatibility algorithm. It does not reopen XG; graph changes require regeneration.

The complete graph-enabled repository suite passes 165/165 tests. Next: benchmark compatibility
size, production overhead, replay runtime, and unique-fact cardinality on the checksum-validated
fixed authority. Replay is currently serial and declares one active worker; do not claim a
production speed or memory bound until that benchmark is recorded.

## v0.9.0 deterministic performance release (verified 2026-09-03)

This section supersedes the v0.8.2 next action below. The user explicitly reopened PanCollapse
performance work as a 0.9.0 feature release. The final implementation is committed as
`b128919293f11f5d1528d9bd8feadde806717582` and promoted to `main`; it descends from the v0.8.2
provenance commit `f5b5cae74bd69ca44e39dbdba80d7f86c90cdf33`. The running v0.8.2 producer and partial outputs
remain untouched.

The release combines sparse repeated-body geometry, exact-state consolidation/prefiltering,
adaptive Parent/target initialization workers, 256-way sharded lazy node/model-candidate caches,
numeric production tally/collapse, immutable Parent-local exon-edge sharing, and bounded
ordinal-preserving read-group workers behind `--threads N`. Normal typed-union BAM and the
five-point exact score window remain the production evidence surface; no alignment, index, Parent,
or evidence semantics changed.

- The optimized build passes 128/128 CTests. The exact-mode gate requires byte-identical RAD,
  BAM, summary, tx2gene, and debug artifacts at one, two, four, and eight workers, including forced
  one-byte RAD chunks. An independent score-mode gate proves default/one/eight-worker artifact
  identity. A generated 64-Parent/64-target gate with three exon and three body aliases per Parent
  proves production Parent/target and legacy multi-gene initialization actually use eight workers,
  numeric XG handle order cannot alter lexical output, shared geometry retains distinct exon/body
  evidence identities, repeated runs are byte-identical, and lexical-first parallel failures create
  no partial output. Zero and non-integer thread counts fail, and a recurrent closed read name still
  fails without committed partial output under eight workers.
- Three repeat Helgrind runs of the generated exact eight-worker fixture completed with zero error
  contexts. One earlier run exposed a condition-mutex warning whose stack originated entirely in
  glibc 2.39's internal `pthread_cond_wait` broadcast; a focused source audit and a recheck with
  Helgrind's condition-signal heuristic disabled found no PanCollapse race or API misuse. The local
  ThreadSanitizer runtime still cannot start because it rejects the host address layout, so TSan is
  not counted as a passing code check.
- On the bounded adversarial 2,000-model, 5,000-group exact fixture, median wall time fell from
  24.86 s in v0.8.2 to 3.76 s with one final v0.9.0 worker, 0.52 s with eight, 0.33 s with sixteen,
  and 0.25 s with thirty-two: 6.61x, 47.81x, 75.33x, and 99.44x faster than v0.8.2, respectively.
  Sixty-four and 128 workers regressed from the 32-worker optimum on this fixture. RAD and summary
  hashes match v0.8.2. A separate clean `5769f11` baseline matched the final one/eight-worker
  generated-alias, cyclic-degradation, and repeated-resolvable artifacts recursively. Full inputs,
  replicates, hashes, and interpretation limits are recorded in
  [`docs/research/v090-deterministic-parallelism.md`](docs/research/v090-deterministic-parallelism.md).
- A cheap two-model/900,000-group fixture automatically reduced eight requested processing workers
  to one active worker. It took 9.40 s with one requested and 9.34 s with eight requested, avoiding
  the earlier queue-overhead regression while leaving initialization under the requested ceiling.
- On a separate 512-Parent startup fixture with 32 exon and 32 body paths per Parent (524,288
  exact models), final one-worker wall time was 1.12 s and median peak RSS was 131.1 MiB, versus
  6.51 s and 463.0 MiB for v0.8.2. The final Parent phase was 0.291 s at one and 0.120 s at 64; total
  initialization was already dominated by ledger/XG loading and ranged from 0.881 to 1.068 s at
  8-64 workers. The 16,384 exon aliases share 512 exact edge geometries. RAD, summary, and tx2gene
  bytes match v0.8.2; the ordered result window remains capped at twice the effective worker count
  and adds no persistent or spill I/O.
- The local runtime image `josephlalli/pancollapse:v0.9.0` has immutable image ID
  `sha256:3886de3476a9ed0553ac49147bccf61740f68dacd8603d4c5c283a0185baa211`, OCI version label
  `0.9.0`, and size 156,933,811 bytes. It reports `panCollapse 0.9.0`. An exact eight-worker
  container smoke matched host RAD, summary, tx2gene, debug evidence, and BAM records; a second run
  with forced one-byte RAD chunks reproduced the baseline differential `map.rad` SHA-256
  `cc1b3fa2ececf7de5c1a67defdd4c715f83531a39572652ea48dc7dfd83dc19b`. The corresponding
  unstripped optimized executable SHA-256 is
  `d91f2bfaddb65d137fc766a3e003fa81e9a670b931e7a7ad3592ef4b49b1b227`.

These bounded results establish the targeted algorithmic and parallel gains, not chr20-22 or
whole-pangenome wall time, peak RAM, or the best worker count on a production graph. The next
performance gate is an explicitly authorized bounded real-graph/read slice, starting at eight and
sixteen workers. Do not stop or restart the active v0.8.2 producer from this evidence alone.

## v0.8.2 geometry-initialization correction (verified 2026-09-03)

This section supersedes the older reconciliation snapshot below as the current authority.
The implementation is committed as `8d3ba5800b6a6de7e2ceac46467e406a10f67dd9` on isolated
branch `worktree-codex-v081-geometry-precompute`, based on committed v0.8.1 revision
`8fc0c0e85212d92b2203f7ca6106760547bc5724`.

- v0.8.1 correctly made ambiguous cyclic transcript-body degradation Parent-scoped, but its
  geometry worker rediscovered whether a canonical target contained a degraded Parent by scanning
  all 1,230,107 exon-path mappings once per target. The joint chr20-22 producer remained CPU-active
  in that pre-GAMP pass for more than 25 hours with no output writer open.
- v0.8.2 caches the affected canonical target IDs at the moment an unresolvable Parent is found.
  The geometry worker now performs one set lookup per target. No transcript/exon structure,
  occurrence resolution, evidence tier, score, Parent identity, or output contract changes.
- The exact source passes all 119 CTests, including the degraded-Parent/clean-sibling byte-parity,
  gene-change fail-closed, and resolvable repeated-body fixtures. A bounded full-graph preload
  reached `evaluated_target_edges=141543588`, `owned_target_edges=5937500`,
  `fragment_only_target_edges=207`, and `adjacent_vetoed_target_edges=135606081` in 73 minutes,
  then was deliberately terminated before its output was used.
  Exact inputs, command surface, binary digest, and the geometry receipt are recorded in
  [`docs/research/v082-joint-geometry-preload.md`](docs/research/v082-joint-geometry-preload.md).
  The full-graph process used the corrected logic before its version-only 0.8.2 rebuild; the rebuilt
  0.8.2 binary passed the complete fixture suite and the graph was not rerun for a string change.
- The independent graph scan reports 13 directed-cyclic body paths; exact occurrence/orientation
  resolution degrades 12 and retains one resolvable repeated path. These are intentionally
  different quantities. A producer must validate `body_paths_degraded=12`, not equate degradation
  with the scan's 13 directed-cycle candidates.
- The original joint producer and its partial output remain untouched pending explicit user
  authorization for a graceful service stop and checksum-pinned restart. No final-test assignment
  result has been opened, and this performance-only correction does not revise the frozen
  annotation/counting method.
- The rebuilt binary SHA-256 is
  `f1bb013cb6ac3671f43d66a550b927327d73cdaa20a182c0bc153bad58acff85`. The local runtime
  `josephlalli/pancollapse:v0.8.2` has immutable image ID
  `sha256:2ab89665e39a4be8a87ce13766f413090783034a52a3eac97abb7ec1eff0c02e`, carries OCI version
  label `0.8.2`, and reports `panCollapse 0.8.2` from inside the container.

Next: restart the joint producer only after explicit authorization. Preserve the failed v0.8.1
attempt and use a corrected producer receipt that records both the 13 scan candidates and 12
degraded Parents.

## Historical reconciliation snapshot (verified 2026-08-21)

This section was the current authority on 2026-08-21. The development narrative below is retained as
history through D069 and must not be used to blur committed, validated-uncommitted, and
later experimental state.

### Committed baseline

- The branch HEAD is `7ada24463dda`, release 0.5.0. This is the only committed
  implementation baseline.
- The repository's current source and documentation are materially dirty; the working-tree
  version string of 0.8.0 does not make the current tree a release.

### Validated but uncommitted v0.8.0 snapshot

- D067-D069 define the normal information-complete typed-union/superset BAM path and the
  five-point exact-Ex50 score window, with `--no-ex50-score-window` as the explicit
  sensitivity route.
- That earlier uncommitted snapshot passed 109/109 CTests. Its validated executable SHA-256
  is `693ce49924982793b837a772da4aa84e13d49dda89f0ad3b62d56d67ba687cd3`;
  the local image SHA-256 is
  `16613165973e5587bcc3ef078f5b8859500d655e2faff4d481f2bcbbc1bdfa44`.
  The image is not registry-published.
- Those hashes and the retained test/fixture evidence identify the validated snapshot. They
  do not validate later edits merely because those edits still report version 0.8.0.

### Experimental-only current delta

- The later `--compact-exact-count-bam` path, strict allowlisted-Parent input, producer-side
  single-winner projection, and ledger/provenance hashes are research additions. Compact
  output is intentionally lossy and must not replace the normal typed-union/superset BAM in
  the production or biological identity ledger.
- Binding policy from 2026-08-21: compact mode remains default-off. Every proposed use needs
  a recorded case-specific reason, and every actual use must be prominently reported as
  compact, lossy, and experimental; it may not enter a default config or ordinary recipe.
  Only the user may determine that compact output is necessary and authorize it. The current
  decision is that it is not necessary.
- The current build reports 0.8.0 but has executable SHA-256
  `aa4b6e00d056b712f8d70047d087c6636aaf03d2820564a787917098efc0db9a`,
  which differs from the validated D069 executable.
- The current build configures 111 CTests, adding compact/strict-allowlist coverage. No
  durable 111/111 result exists for the exact present tree, so neither the compact path nor
  the whole current working tree is a validated release.
- The user nevertheless accepts the current software as valid for the next scientific
  experiment. That closes an additional pre-run test requirement without creating a 111/111
  record, a commit, or a release claim.

### Next bounded action

1. Preserve the normal v0.8.0 evidence and the compact experiment as separate provenance
   classes. Compact is not authorized for the next experiment and must not be used unless
   the user later determines it is necessary.
2. Treat the current code as accepted for the next experiment; retain the missing 111/111
   result as an honest provenance limitation rather than a pre-run blocker.
3. Review and promote the normal v0.8.0 production seam separately from the
   compact/strict-allowlist experiment when doing release engineering. A version label alone
   is not a promotion, but promotion is not a prerequisite for the accepted experiment.
4. Follow `/mnt/ssd/lalli/panSC/PROGRESS.md` for the chr20 k32 loss-diagnostic phase, using
   the normal typed-union output.

## Historical implementation record through D069

**Phase 3, redesigned around D048 (graph-native transcript feature counting).**

The GAMP-to-RAD core is now specified in `docs/conversion-algorithm.md`: score each aligned
node under vg's own scoring scheme, add each node's score to every HST path crossing it, take
the top HST score across all of a read's alignments plus ties, collapse to unique transcript
IDs for RAD `refs` through the t2g's implicit suffix rule or optional explicit alias column,
and record orientation from the read's direction along the HST path
(majority of aligned bases on disagreement). Runtime inputs are the name-grouped GAMP, the
graph with `vg rna` HST paths, and a transcript-to-gene map; the GTF and `vg rna` are
reference/fixture-creation only.

D064 now supersedes that runtime identity surface for production: one strict
`panSC-path-identity-v1` ledger supplies exact path, unique Parent, canonical transcript, gene,
exon/body, and provenance identity. Historical t2gs remain available only through the explicit
`--legacy-adapter hst-v1`.

D065 closes the barcode-correction population seam for the optional BAM. New RNA carry-along
names preserve hex-encoded raw barcode and UMI qualities, feature records emit `CY`/`UY`, and every valid input
group gets exactly one BAM record. Groups without count features use unmapped
`XB:Z:barcode_only` records so downstream correction can build an all-read exact-barcode prior
without admitting those reads to feature counting. Legacy names remain accepted without `CY`.

D066 replaces the production `genefull_ex50pas` BAM approximation with exact STARsolo 2.7.11b
evidence. Parent-specific GAMP/body/exon comparisons emit parallel
`TX`/`GX`/`GD`/`GT`/`XP`/`XU` E/P/B slots without canonical pre-collapse; count_cr owns the global
six-rank and UMI reductions. This mode requires the production ledger, BAM output, and
`--bam-multigene all`; `hst-v1` now hard-fails. Its established RAD path is unchanged.

D067 is implemented for 0.7.0 in the current uncommitted tree and passed the
full cross-repository chr20 evidence chain on 2026-07-29. One normal conversion
emits an information-complete, header-marked typed-union BAM carrying ordinary
Gene `GL` and exact-Ex50 `GT`/`XP`/`XU` evidence with common
`TX`/`GX`/`GD` and `CB`/`UB`/`CY`/`UY`. `--debug-evidence-out` writes
normalized `panCollapse-debug-evidence-v1` read/candidate rows. Focused tests
prove that debug mode leaves production BAM records and RAD bytes unchanged.
Typed-union BAM production fails early unless every exon Parent has at least
one linked body row. The complete PanCollapse suite passes 105/105 tests, and
the real one-BAM benchmark, ordered audit, and reproducible two-mode report are
recorded in
`/mnt/ssd/lalli/panSC/docs/pancollapse_countcr_superset_plan.md`. The validated
binary reports 0.7.0 and has SHA-256
`4d78a650993a54ec997c5e75df35311f37121423c832c60e8519131203662a08`;
the local `josephlalli/pancollapse:v0.7.0` image is not registry-published.

D068 is implemented in the current uncommitted tree. Exact Ex50 evidence now
retains only complete transcript-compatible traversals within five score points
of the read group's global compatible optimum before `count_cr.py` applies the
six GT/GD ranks. Five points is one mismatch-equivalent under vg's default
`+1` match / `-4` mismatch scoring. Focused fixtures pass for inclusive
top-minus-five and exclusive top-minus-six alternatives both within one
MultipathAlignment DAG and across separate records in one read group; ordinary
Gene evidence and RAD bytes remain unchanged. The bounded real HG002 chr20
qualification is complete: among the 445,665 reads STAR assigned but the prior
graph run rejected as direct multigene, D068 uniquely recovers 5,306 (1.190580%),
leaves 440,305 (98.797303%) multigene, and correctly strand-vetoes 54 (0.012117%).
The residual two-gene audit finds zero shared curated HGNC identities and zero
identical complete retained-exon sequence sets. This validates D068 mechanics
but is not a replacement full-production recount; see the exact qualification
root linked under Next action.

D069 promotes this behavior as the v0.8.0 default and adds the explicit
`--no-ex50-score-window` sensitivity flag. The opt-out skips score pruning
entirely, restoring the pre-D068 all-compatible exact-evidence set before Ex50
ordering. BAM headers distinguish `panCollapse-ex50-score-window:5` from
`panCollapse-ex50-score-window:disabled`; `summary.tsv` records the same state.
Focused default/disabled fixtures restore both within-DAG and cross-record
top-minus-six alternatives only when disabled, reject invalid-scope use, and
prove byte-identical RAD. The full suite passes 109/109 as v0.8.0. The validated
binary SHA-256 is
`693ce49924982793b837a772da4aa84e13d49dda89f0ad3b62d56d67ba687cd3`.
The local `josephlalli/pancollapse:v0.8.0` image
(`sha256:16613165973e5587bcc3ef078f5b8859500d655e2faff4d481f2bcbbc1bdfa44`)
reports version/label 0.8.0 and passes containerized default/disabled exact-tier
verification plus byte-identical RAD. The image is local and not registry-published.

The prior traversal-enumeration and GTF-projection implementation in `src/main.cpp`
(D026/D027) is superseded by D048 and is being replaced increment by increment; it removes
the complete-traversal enumeration whose cap hard-failed on real MHC GAMP. RAD wire format,
streaming writer, raw CB/UMI sourcing, name-grouping, unaligned counting, and the
human-pangenome fixture plan (D047) are unchanged. Generated graph/RAD artifacts remain
confined to ignored build directories.

## Settled product decisions

D048 supersedes the assignment-surface, collapse-manifest, traversal-cap, and GTF-projection
items below for active GAMP-to-RAD conversion; see `docs/conversion-algorithm.md`. The
bullets are retained as cumulative history.

- Project name: `panCollapse`.
- C++20, GCC 15 environment, CMake + Ninja.
- Standalone project linking to an existing VG installation or checkout.
- GTF annotation; no GFF3 requirement in V1.
- V1 uses preexisting VG/index files and does not build a custom lookup index.
- Input GAMP must be grouped by read name; the tool validates this contract.
- CB/UMI come from the GAMP name field as observed raw values prepared upstream.
  panCollapse does not correct cell barcodes or UMIs; alevin-fry performs permit-list
  construction/cell-barcode correction and UMI deduplication/resolution downstream.
- Raw CB and UMI lengths are CLI-controlled values. Phase 2 defaults are
  `--raw-cb-length 16` and `--raw-umi-length 12`.
- Intronic and exon–intron-boundary evidence can make a transcript compatible.
- panCollapse does not filter compatibility by library strand. It preserves target-
  relative orientation in RAD `dirs`, and downstream alevin-fry expected-orientation
  settings handle library-orientation filtering.
- Copy/path collapse uses an explicit deterministic manifest.
- Phase 0 resolved the manifest source key as
  `(source_path_name, source_transcript_id)`.
- Phase 0 recentered V1 around mapper-side availability of `graph.xg`, `graph.gcsa`, and
  `graph.dist`: panCollapse consumes GAMP plus the matching `.xg`; `.gcsa`/`.dist` remain
  upstream `vg mpmap` artifacts and are not converter inputs or V1 provenance flags.
- Tied-best target score is the default; a nonzero score window is an advanced option.
- Full transcript equivalence classes are retained by default.
- V1 emits mapper-style uncollated RAD for standard alevin-fry processing.
- V1 does not classify spliced versus unspliced molecules.
- Transcript overhang is allowed only when at least one aligned base overlaps that
  transcript's exon/intron model.
- Missing, malformed, or unsupported raw CB/UMI values parsed from the GAMP name field
  are skipped and counted by default; `--molecule-identity-failures fail` converts those
  conditions into hard failures.
- Collapse-manifest coverage is mandatory and explicit.
- GAMP-to-RAD output uses all-compatible-target assignment. `all` is the default and only
  active RAD assignment behavior; `unique-transcript`, `unique-gene`, and
  `starsolo-default` are deferred to-be-implemented modes outside active RAD conversion.
- Historical D045 kept the active converter single-threaded. D073 supersedes that deferral for
  v0.9.0 with deterministic complete-read-group workers behind `--threads N`; stdin streaming
  remains supported and parsing/grouping remain single-owner.
- RAD output should be written to disk with a streaming writer: `num_chunks = 0`, file
  tag values, then complete chunks emitted incrementally.
- License: Apache-2.0.
- D062 permits optional t2g column 3 (`raw_graph_path`, `gene`, `canonical_transcript`) for
  CAT-style path names. Raw-path top scores are selected before alias collapse; ledger mode
  max-collapses scores across raw paths mapped to the same canonical transcript. Two-column
  behavior is unchanged.
- D063 extends the optional alias to body t2gs. A consistently three-column body file keeps exon
  and body scores, splice ownership, S/U calls, and orientation keyed by canonical transcript;
  count_cr performs the only transcript-to-gene pooling. Two-column body behavior is unchanged.
- D064 makes the headered `panSC-path-identity-v1` ledger the production input. Lookup and scoring
  are exact path -> Parent -> canonical MAX; paths/Parents tied at either boundary are retained in
  BAM `XP`/`XU`. Every ledger path and length must match the XG. `hst-v1` is an explicit adapter,
  never auto-detected.
- D065 makes the optional BAM's barcode-evidence population independent of feature construction:
  one record per valid input group, `CY`/`UY` from optional hex quality suffixes, and an unmapped
  `XB:Z:barcode_only` record whenever no feature record exists.
- D066 makes production `genefull_ex50pas` exact and Parent-preserving: `GT` E/P/B plus one exact
  `XP`/`XU` path/Parent per evidence slot, with repeated canonical `TX` values allowed until
  count_cr applies E-sense, E-AS, P-sense, P-AS, B-sense, B-AS priority. Legacy Ex50pAS is rejected.

## Superseded next action

The earlier instruction to launch a marker-pinned HG002 chr20 replay is historical: HG002
simulation and multiple downstream runs now exist. Use the reconciliation snapshot above
for the current software gate. The bounded score-window qualification report remains at
`/mnt/ssd/lalli/hg002_10x5p_q100_chr20_v1/ex50_read_loss_crosstab_v1_20260802T110935-0500/pair_analysis_v1/score_window5_v1/REPORT.md`,
but it does not validate the later compact/strict-allowlist source delta.

## Required stop

Do not broaden production code into multithreading without a later explicit approval.
Stdin GAMP streaming is implemented (D049, `--gamp -`). Do not add stdout RAD output unless
separately approved (the seek-and-backpatch writer needs a seekable file). Phase 3 work must
proceed one
independently testable behavior at a time, following D048 and
`docs/conversion-algorithm.md`. Do not reintroduce traversal enumeration, runtime GTF
projection, or a custom index.

## Phase 0 artifacts

- `docs/research/vg-integration.md` — local VG v1.75.0-68-ge82694b69 evidence, GAMP API,
  upstream `mpmap` artifact context, XG/path-position boundary, annotation access, VG
  API wiki review, and smoke-validated link boundary.
- `docs/research/gamp-grouping-and-sorting.md` — version-scoped single-end GAMP grouping
  verdict and no-`gamsort` remedy.
- `docs/research/rad-format.md` — alevin-fry v0.15.0/libradicl v0.13.0 RAD wire schema,
  native writer strategy, and conformance plan.
- `docs/research/annotation-lookup-performance.md` — source transcript identity, GTF/XG
  projection design, strand/splice rules, and no-custom-index thresholds.
- `docs/architecture-proposal.md` — proposed V1 inputs, CLI, data flow, dependency
  boundary, deterministic writer model, diagnostics, alternatives, and Phase 1 plan.
- `docs/decisions.md` — proposed D023-D034 resolving Phase 0 forks and superseding broad
  VG/path-only wording.
- `docs/testing_fixture_creation.md` — medium-scale known-truth RAD fixture plan for about
  50,000 BEERS2-generated reads from a pangenome fixture, with independent expected RAD
  records for panCollapse output comparison.

## Phase 1 artifacts

- `docs/phase1/README.md` — routing index for future agents and reviewers.
- `docs/phase1/cli-run-contract.md` — focused CLI, option-default, exit, traversal-cap,
  and stable diagnostic-counter contract.
- `docs/phase1/compatibility-fixtures.md` — VG-dependent compatibility and projection
  fixture expectations.
- `docs/phase1/policy-fixtures.md` — pure score, collapse, assignment, target ordering,
  and summary fixtures.
- `docs/phase1/input-diagnostics-fixtures.md` — barcode/UMI, grouping, manifest, and
  diagnostic fixtures.
- `docs/phase1/rad-interop-fixture.md` — tiny RAD/alevin-fry interoperability fixture
  contract and exact expected cell-by-gene matrix.
- `docs/phase1/build-test-plan.md` — CMake/Ninja/CTest skeleton contract and verification
  commands.
- `CMakeLists.txt`, `cmake/PanCollapseVg.cmake`, and `tests/` — Phase 1 build/test
  skeleton with one pure policy smoke, one VG link smoke, and one build-dir-only
  GAMP/XG/GTF projection smoke.

## Phase 2 planning artifacts

- `docs/phase2/implementation-plan.md` — focused vertical-slice plan using GAMP plus the
  matching `.xg`, build-dir-only fixtures, raw read-name CB/UMI extraction, single-thread
  execution, configured raw CB/UMI lengths, two-column `tx2gene.tsv`, and a 1-cell x
  1-gene alevin-fry assertion. Its historical Phase 2 `--strand sense` scope is now
  superseded by D042 for active V1 work.
- RAD docs now explicitly state that production writing is native C++, while libradicl
  and alevin-fry are validation oracles. They also document per-record `bc`, `umi`,
  `refs`, and `dirs` semantics.

## Phase 2 implementation artifacts

- `src/main.cpp` and `src/CMakeLists.txt` — minimal VG-backed `panCollapse convert`
  executable for the approved one-read vertical slice.
- `tests/vg/phase2_fixture_writer.cpp` — build-dir-only GFA, GTF, collapse manifest,
  forward/reverse/mixed-orientation JSON GAMP fixture generator, plus generated-source
  RPVG multipath, connection-score, and vg tiny splice/intron fixtures.
- `tests/vg/phase2_quant_verify.py` — strict Phase 2 verifier for `tx2gene.tsv`, RAD
  header/file tags, packed raw CB/UMI, `refs=[TX_A_ID]`, configurable forward/reverse
  `dirs`, and the final 1-cell x 1-gene alevin-fry matrix.
- `tests/vg/phase3_mixed_orientation_verify.py` — checks that mixed target-relative
  orientation evidence is dropped and counted, and that the resulting diagnostic fixture
  writes a header-only RAD with no chunks.
- `tests/vg/CMakeLists.txt` — CTest chain for fixture generation, local `vg` conversion
  to `.xg` and binary GAMP, primary and repeat single-thread `panCollapse convert` runs,
  byte-for-byte output comparison, forward and reverse alevin-fry permit-list/collate/
  quant flows, mixed-orientation drop verification, and strict output verification.

## Phase 0 verification

- Original read-only research agents completed for VG integration, GAMP grouping, RAD
  format, and annotation semantics.
- Imported `.claude/agents/oracle.md` from the adjacent reviewer pattern and used it for
  skeptical review. Its first pass found blocking gaps; follow-up research resolved them.
- Throwaway VG link smoke outside the repo succeeded with `VGio::VGio`,
  `libhandlegraph::handlegraph_shared`, imported `libxg.a`, SDSL,
  divsufsort/divsufsort64, OpenMP, `pthread`, `m`, and `atomic`.
- Reviewed the VG `mpmap` wiki and VG API programming wiki. The docs now treat
  `graph.xg`, `graph.gcsa`, and `graph.dist` as expected upstream mapper artifacts, keep
  `.xg` as the V1 coordinate input, and keep GBZ/GBWT as a future alternate requiring
  separate projection proof.
- At Phase 0 close, checked no production/build/fixture/generated files existed with:
  `find . -maxdepth 3 \( -name CMakeLists.txt -o -path './src/*' -o -path './include/*' -o -path './tests/*' -o -name '*.rad' -o -name map.rad -o -name '*.gamp' -o -name '*.xg' -o -name '*.gcsa' -o -name '*.dist' -o -name '*.gbz' \)`.
- Direct stale-term audit found no remaining `not started`, path-only manifest, source
  order tag-precedence, or unresolved Phase 0 placeholder language outside historical
  decision text explicitly superseded by D032-D034.

## Phase 1 verification

- Delegated focused read-only reviews for the synthetic fixture matrix, RAD
  interoperability fixture, and build/test skeleton.
- Created focused Phase 1 contract documents instead of one monolithic contract, so future
  agents can load only the relevant task bundle.
- Oracle review found Phase 1 blockers in score-removal diagnostics, traversal-cap
  fixture coverage, and CLI routing; the contracts were revised to resolve those issues.
- Final oracle cleanup found an ambiguous parent-gene-only fixture and stale entry-point
  verification text; `CMP-10`, `README.md`, `.gitignore`, and
  `scripts/verify-workspace.sh` were revised.
- Pure build/test passed:
  `cmake -S . -B build-pure -G Ninja -DPANCOLLAPSE_ENABLE_VG_TESTS=OFF`;
  `cmake --build build-pure`;
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- Full local VG smoke passed:
  `PKG_CONFIG_PATH=/mnt/ssd/lalli/usr/local/src/vg/lib/pkgconfig cmake -S . -B build -G Ninja -DPANCOLLAPSE_VG_ROOT=/mnt/ssd/lalli/usr/local/src/vg -DCMAKE_PREFIX_PATH=/mnt/ssd/lalli/usr/local/src/vg -DCMAKE_BUILD_RPATH=/mnt/ssd/lalli/usr/local/src/vg/lib`;
  `cmake --build build`;
  `ctest --test-dir build --output-on-failure`.
  This includes the VG link smoke and the build-dir-only GAMP/XG/GTF projection smoke.
- Workspace verification passed: `./scripts/verify-workspace.sh`.
- Oracle re-review passed with no findings, and the verified slice was committed as
  `7396cca Implement Phase 3 orientation slice`.

## Phase 2 verification

- Low-cost explorer agents completed the CMake/CTest fixture-wiring check and the
  alevin-fry output-file/path check before implementation integration.
- Oracle review found and the implementation resolved blockers in RAD/companion verifier
  coverage, exact projection evidence, source-vs-canonical gene handling, deferred strand
  modes, decision/progress bookkeeping, and single-thread reproducibility.
- Pure build/test passed:
  `cmake --build build-pure`;
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- Full local VG/alevin-fry test suite passed:
  `cmake --build build`;
  `ctest --test-dir build --output-on-failure`.
  The historical Phase 2 suite covered primary and repeat `panCollapse convert` runs,
  byte-identical comparison of `map.rad`, `tx2gene.tsv`, and `summary.tsv`, and alevin-
  fry `generate-permit-list`, `collate`, and `quant`.
- Direct RAD/quant evidence is covered by `phase2_quant_verify.py`: `cblen=16`,
  `ulen=12`, raw CB `AAACCCAAGTTTGGGA`, raw UMI `AAAAAAAAAAAA`, one target `TX_A`,
  forward direction, `tx2gene.tsv` equal to `TX_A<TAB>GENE_A`, and a final
  `GENE_A=1` matrix for one cell.

## Phase 3 orientation verification

- VG orientation semantics were checked against the local VG/handlegraph sources by an
  explorer agent before implementation and rechecked after oracle review. Projection
  computes the node-forward span with `node_length - offset - from_length` for reverse
  GAMP mappings, mirrors coordinates when the source path step itself is reverse, and
  records target-relative RAD `dirs`.
- Current full local VG/alevin-fry suite passed:
  `cmake --build build`;
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 55 tests, including forward and reverse orientation RAD
  verification, reverse `alevin-fry generate-permit-list -d rc`, adjacent-read grouped
  mixed-orientation drop/count behavior, recurrent completed-name failure, two successful
  adjacent read groups, and a two-target record with `TX_A` forward and `TX_B` reverse
  accepted by alevin-fry with `-d either`, exact multi-target quant verification, and a
  no-compatible-read summary/header check. It also covers source paths that traverse a
  node in reverse orientation, original read names containing underscores,
  `N`-containing raw CB/UMI values, default `--molecule-identity-failures skip` for
  missing, malformed, and unsupported raw CB/UMI values, and
  `--molecule-identity-failures fail` as a strict hard failure with `counter=1` in the
  diagnostic for each failure class.
- Pure build/test passed:
  `cmake --build build-pure`;
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- Workspace verification passed: `./scripts/verify-workspace.sh`.

## Phase 3 traversal and compatibility verification

- Low-cost worker agents inspected and materialized scratch conversions from
  `jonassibbesen/rpvg/src/tests/alignment_path_finder_test.cpp` and the local VG
  `test/tiny` fixtures. The generated `.vg`, `.xg`, and `.gamp` scratch artifacts were
  not kept in the repo because D4/D036 require generated graph/alignment/RAD artifacts to
  remain build-dir-only.
- The implementation now enumerates complete `MultipathAlignment` DAG traversals through
  both ordinary `next` edges and scored `connection` arcs, applies
  `--max-traversals-per-read`, sums subpath plus connection scores, projects every
  reference-consuming edit, keeps tied-best targets by default, and applies
  `--score-window`.
- Compatibility now includes exon and implied-intron anchors, exon/intron-boundary
  evidence, annotated splice-junction checks for `connection` edges, and long same-path
  gap checks controlled by `--min-splice-jump`.
- The full local VG/alevin-fry suite passed:
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 95 tests. New Phase 3 coverage includes RPVG-derived
  multipath traversal over a six-node branching graph, RPVG traversal-cap failure,
  connection-score aggregation with score-window removal and score-window retention,
  same-path short and long gap handling with a `--min-splice-jump` override,
  connection-through-insertion splice preservation, anchored outside-transcript overhang,
  parent-gene-only negative evidence, and vg tiny exon/intron/boundary/splice-junction
  compatibility. It also covers strict numeric CLI parsing for the new traversal/scoring
  options and the existing raw CB/UMI length options.
- Pure build/test passed:
  `ctest --test-dir build-pure -L pure --output-on-failure`.
- High-cost oracle review initially found permissive numeric CLI parsing for the new
  Phase 3 options and existing raw CB/UMI length options. The parser now rejects signed
  values, trailing junk, empty values, and overflow; the added CLI failure tests cover the
  reported cases. Oracle re-review passed with no blocking or non-blocking findings.

## Phase 3 assignment-surface verification

- D044 supersedes the historical uniqueness-policy plan for active GAMP-to-RAD behavior:
  RAD output preserves all compatible retained targets. `--assignment all` is accepted
  and equivalent to the default; `unique-transcript`, `unique-gene`, and
  `starsolo-default` fail with a to-be-implemented diagnostic.
- The full local VG/alevin-fry suite passed:
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 101 tests, including explicit `--assignment all` coverage
  on the multi-target RAD/alevin-fry fixture, a default-vs-explicit-`all` byte comparison,
  unknown-assignment usage-error coverage, and deferred-mode failure tests for all three
  inactive uniqueness modes.
- The first high-cost oracle review found stale removal-counter/uniqueness wording and
  missing assignment-surface evidence. The stale wording was corrected, the missing CTest
  coverage was added, and high-cost oracle re-review passed with no blocking findings.
- Pure build/test passed:
  `ctest --test-dir build-pure -L pure --output-on-failure`.
  Workspace verification passed: `./scripts/verify-workspace.sh`.

## Phase 3 manifest/collapse verification

- The implementation already enforced the manifest source key and mapped compatible source
  identities to canonical RAD targets. The new validation slice adds direct fixtures for
  Section 5 manifest/collapse requirements.
- The local manifest-focused suite passed:
  `ctest --test-dir build -L manifest --output-on-failure`.
  It covers two source transcript identities collapsing to one canonical target using the
  maximum source score, graph/source multiplicity not inflating a collapsed target score by
  summing, missing source identity hard failure, contradictory manifest row hard failure,
  and canonical transcript mapped to multiple genes hard failure.
- The full local VG/alevin-fry suite passed:
  `ctest --test-dir build --output-on-failure`.
  The suite currently contains 110 tests.
- High-cost oracle review initially found that the max-score fixture could pass a
  last-compatible-source-wins bug. The fixture now covers both low-to-high and high-to-low
  collapsed-source score order, and oracle re-review passed with no blocking findings.

## Gate checklist snapshot

- Gate Architecture Approved: passed in Phase 0.
- Gate Behavior Specified: passed in Phase 1.
- Gate Vertical Slice Proven: passed locally by the current Phase 2 implementation,
  final oracle review, and committed test evidence.
- Phase 2 exact happy path: implemented and verified for one name-grouped GAMP record,
  one source transcript, one canonical target, raw read-name CB/UMI extraction, RAD
  emission, and alevin-fry quantification.
- Phase 2 single-thread reproducibility: represented by repeat conversion and
  byte-for-byte comparison of `map.rad`, `tx2gene.tsv`, and `summary.tsv`.
- Production source has been broadened from the Phase 2 vertical slice for Phase 3:
  multi-row manifests, multiple GTF transcript models, grouped GAMP records,
  multi-target RAD records, target-relative `dirs`, complete GAMP traversal enumeration,
  score-window filtering, and exon/intron/splice compatibility are implemented.
  Checked-in generated graph/RAD artifacts remain absent.
- Manifest/collapse validation now directly verifies max-score collapse, no score summing
  across collapsed source identities, missing source identity failure, contradictory row
  failure, and canonical-gene conflict failure.
- D044 supersedes the old assignment-policy plan for active GAMP-to-RAD behavior:
  preserve all compatible retained targets in RAD, accept/default to `all`, and treat
  `unique-transcript`, `unique-gene`, and `starsolo-default` as to-be-implemented future
  modes rather than RAD filters.
- Phase 3 orientation work supersedes the old strand-mode plan under D042: remove
  `--strand`, preserve target-relative RAD `dirs`, and drop/count mixed-orientation
  evidence for one emitted target.
- Medium artificial-GAMP RAD semantic coverage now generates 50,000 read groups (77,500
  input records) across a four-node/four-path graph, FASTQ, GFA/XG, GTF, collapse
  manifest, JSON GAMP converted to binary GAMP, expected semantic RAD rows, expected
  non-emitted rows, and panCollapse RAD. Its scaled case mix now exercises multi-gene and
  multi-isoform overlap, unique single-target emission, two source paths collapsing to one
  canonical target `TX_H` with an equal-score companion `TX_B` that proves no score
  summing, collapse to a single canonical target, top-score retention with score-window
  removal, target-relative forward/reverse `dirs`, mixed-orientation drops, no-compatible
  drops, unaligned read groups, and a malformed raw molecule-identity skip. It verifies
  exact target sets and per-target orientation, the alphabetical RAD target dictionary,
  every Section 12 counter, `tx2gene.tsv`, and RAD record semantics, and a coarse
  `TIMEOUT` guards the convert against catastrophic throughput regressions. It remains an
  artificial-GAMP regression and does not exercise the FASTQ -> `vg mpmap` -> GAMP mapper
  path.
- D043 adds the active raw molecule-identity failure policy:
  `--molecule-identity-failures skip|fail`, default `skip`, with
  `raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
  `raw_molecule_unsupported_groups`, and `raw_molecule_skipped_groups`.
- D046 adds the `unaligned_reads` counter: read groups whose GAMP records all have zero
  subpaths emit no RAD record and increment `unaligned_reads` additively alongside
  `no_compatible_transcript_groups`, without being treated as molecule-identity failures.
- The all-dropped mixed-orientation diagnostic fixture is not an alevin-fry
  interoperability fixture. Local alevin-fry rejects no-chunk RAD files, so nonempty
  forward and reverse orientation fixtures carry the current alevin-fry proof.
- A local implementation experiment confirmed the same conflict for populated RAD:
  libradicl can represent unknown chunk count as `num_chunks = 0`, but local alevin-fry
  v0.15.0 `generate-permit-list` rejects populated `num_chunks = 0` input as no chunks.
  The current production writer remains exact-count/backpatched-style until the user
  approves how to reconcile D045 with the supported alevin-fry path.

## Session log

- 2026-06-27: Contract and tracked agent workspace created from the workplace-engineering
  principles. No implementation attempted.
- 2026-06-28: All seven pending product questions resolved with option A and incorporated
  into the product, interface, compatibility, and validation contracts.
- 2026-06-28: Phase 0 delegated research completed and integrated. Oracle review found
  blockers in evidence citations, source identity, strand/splice semantics, RAD wire
  details, tag precedence, and progress state; docs were revised to resolve them.
- 2026-06-28: Phase 1 behavior contract completed with focused fixture documents, RAD
  interoperability fixture contract, CMake/CTest skeleton, and passing pure/VG smoke
  tests. Stopped at Gate Behavior Specified for human review.
- 2026-06-28: Phase 2 planning started. User approved raw read-name CB/UMI extraction,
  build-dir-only fixtures, single-thread Phase 2 execution, no Phase 2 USA output, and
  active-doc cleanup away from corrected-tag barcode sourcing.
- 2026-06-29: User clarified that raw CB and UMI lengths are CLI-controlled values and
  approved `--raw-cb-length 16`, `--raw-umi-length 12`, and fixture read
  `read000_AAACCCAAGTTTGGGA_AAAAAAAAAAAA`. The user also asked for RAD `refs`/`dirs`
  semantics to be included in the contracts.
- 2026-06-29: Phase 2 implementation completed for the approved vertical slice. The
  build-dir-only fixture creates GFA/GTF/manifest/JSON GAMP inputs, local `vg` produces
  `.xg` and binary GAMP, `panCollapse convert` emits `map.rad`, `tx2gene.tsv`, and
  `summary.tsv`, a repeat single-thread run is byte-identical, and alevin-fry produces
  the expected 1-cell x 1-gene matrix. User approved deferring `antisense` and `both`
  strand modes to Phase 3 with to-be-implemented errors in Phase 2.
- 2026-06-29: User superseded the panCollapse strand-mode plan with target-relative RAD
  orientation preservation: remove `--strand`, do not filter by strand, write real
  forward/reverse `dirs`, and drop/count mixed orientations for one emitted target.
- 2026-06-29: User approved the raw molecule-identity failure surface
  `--molecule-identity-failures skip|fail`, default `skip`, with stable
  `raw_molecule_*` counters. The implementation now skips/counts missing, malformed,
  and unsupported raw CB/UMI groups by default and fails strictly under `fail`.
- 2026-06-30: Phase 3 orientation and raw molecule-identity slice was oracle-reviewed,
  verified locally, and committed as `7396cca`.
- 2026-06-30: Phase 3 traversal/scoring plus intron/splice compatibility was implemented
  with build-dir generated RPVG-derived and vg tiny fixtures. Full local VG/alevin-fry
  tests passed with 95 tests after strict numeric CLI parser coverage was added, and
  pure tests passed. High-cost oracle re-review passed.
- 2026-06-30: User superseded historical assignment-policy scope for active GAMP-to-RAD
  behavior: RAD output preserves all compatible retained targets; uniqueness modes are
  deferred outside active RAD conversion. Local full suite passed with 101 tests after
  addressing first-pass oracle findings; high-cost oracle re-review passed.
- 2026-06-30: Added direct Phase 3 manifest/collapse validation fixtures. Local manifest
  label passed and full local VG/alevin-fry suite passed with 110 tests after addressing
  first-pass oracle findings; high-cost oracle re-review passed.
- 2026-06-30: Added `docs/testing_fixture_creation.md` to the local documentation index as
  the medium-scale known-truth RAD fixture plan: about 50,000 BEERS2 reads from a
  pangenome fixture with independently computed expected RAD records.
- 2026-07-01: User deferred panCollapse-side multithreading for now and identified direct
  stdin GAMP streaming from `vg mpmap` into panCollapse as a future interface direction to
  research. User also approved streaming RAD-to-disk output with `num_chunks = 0` and
  complete chunks emitted incrementally.
- 2026-07-01: Added the medium artificial-GAMP RAD semantic fixture. The local full suite
  passed with `ctest --test-dir build --output-on-failure -j 8` at 115/115 tests.
  `git diff --check` and `./scripts/verify-workspace.sh` also passed before commit.
  High-cost oracle review returned PASS WITH RISKS: no blocking findings, with the
  remaining BEERS2 plus `vg mpmap` path and RAD chunk-count policy conflict called out
  for human review.
- 2026-07-01: Landed the unaligned read-group diagnostic (D046) that a prior interrupted
  session left uncommitted. A GAMP read group whose records all have zero subpaths emits
  no RAD record and increments the additive `unaligned_reads` counter alongside
  `no_compatible_transcript_groups`. Added a name-only GAMP fixture, the
  `phase3_unaligned_*` builder/convert/verify CTest chain, and the `verify_unaligned`
  check. The local full suite passed at 118/118 tests and the pure suite passed.
- 2026-07-01: Reviewed the artificial-GAMP medium fixture for throughput, multimapping,
  and required-representation coverage, then extended it (no aligner) per user direction.
  The scaled mix now adds many-source->one-canonical collapse with a no-score-summing
  companion, collapse-to-single-target, unique single-target emission, unaligned groups,
  and a malformed molecule-identity skip, growing the graph to four nodes/paths and
  77,500 input records. A first oracle version mispredicted the malformed case as
  unsupported (`N` is a supported molecule base, `main.cpp:410`); corrected to a
  wrong-length UMI. Added a coarse `TIMEOUT` throughput guard on the convert. Full local
  suite passed at 118/118 and pure passed. The fixture remains artificial GAMP and does
  not prove the FASTQ -> `vg mpmap` -> GAMP mapper interface, which still needs a real-
  mpmap fixture (direct template slicing recommended over full BEERS2 for the strict
  oracle).
- 2026-07-01: User rescoped panCollapse as a GAMP -> RAD bridge (D047): FASTQ -> `vg mpmap`
  is out of scope, the production-applicability fixture must come from the human pangenome
  (pinned MHC `sampleA` spliced bundle), and the RAD oracle is GAMP-driven and independent
  of panCollapse output. Rewrote `docs/testing_fixture_creation.md` from the ~1500-line
  BEERS2-plus-`vg mpmap` plan to a 208-line human-pangenome GAMP-to-RAD plan, retiring the
  "artificial GAMP must never be primary" and mapping-stability rules. Verified all eight
  pinned MHC source files exist with sizes matching the recorded `size_bytes`. Docs-only
  change; no production code touched. Committed as `281aae1`.
- 2026-07-01: Session-start verification for the D047 human-pangenome smoke slice.
  Re-verified all eight pinned MHC source files by sha256 (all `OK`) and confirmed the local
  `vg` is the pinned `v1.75.0-68-ge82694b69 "Spike"`. Characterized the bundle from primary
  sources: `sampleA.spliced.xg` has 353,766 nodes (id range 1:353766), 488,650 edges, and
  27,013 paths, of which 27,011 are HST transcript-haplotype paths (`<transcript>_H<n>` /
  `_R<n>`) and 2 are genomic backbones (`GRCh38#0#chr6[28510118]`, `CHM13#0#chr6[28381547]`).
  `mhc.refpath.gtf` annotates a single seqname `GRCh38#0#chr6[28510118]` (1,022 transcripts,
  11,033 exons) that exactly matches a visible `.xg` path, satisfying the D023 seqname/path
  match on the reference path. Surfaced the smoke-slice source-path/collapse-model fork now
  recorded under Next action. No production code, fixtures, or generated artifacts created.
- 2026-07-01: Ran scoped read-only research (librarian + vg-integration-researcher +
  Explore) on building a production-representative human-pangenome GAMP-to-RAD fixture from
  the full HPRC pangenome, and verified the load-bearing claims locally. Findings: (a) the
  full HPRC v1.1 MC GRCh38 pangenome is already on disk at
  `/mnt/ssd/lalli/nf_stage/genome_refs/pangenome/GRCh38/` (`hprc-v1.1-mc-grch38.d9.gbz`
  4.4G, `.full.gbz` 7.1G, `.xg`, `.dist`), HPRC S3 provenance recorded in `to_download.txt`;
  no download needed. (b) The pinned `vg` provides the whole recipe (confirmed flags): `vg
  chunk` (`-p`/`--gbz`/`--contig`/`-c`/`-T`/`-G`), `vg rna` (`-z`/`-l`/`-e`/`-r`/`-a`/`-i`),
  `vg autoindex --workflow mpmap`, `vg sim` (`-P`/`-a`/`--multi-position`, truth in GAM
  refpos not read name), `vg mpmap -F GAMP` (default), and `vg gampcompare`. (c) The
  existing MHC bundle was NOT sliced from HPRC; its source is a toy 466-haplotype
  `1kg_hg38-MHC.gbz` (panresolve example, unrecorded upstream URL) built with native vg
  1.51.0, so a fresh HPRC-derived pinned-vg build is a provenance and version upgrade. (d)
  Prior scratch in `build/real_mhc_scratch/` (today) already tagged real 10x reads
  (`SRR10587809` subsample), produced a 228 MB real `mhcA.gamp` against the toy bundle,
  built a reference-path collapse manifest (`mhc_identity_collapse.tsv`, exact D033 format,
  `source_path_name=GRCh38#0#chr6[28510118]`, source==canonical, 1022 transcripts), and ran
  panCollapse -> HARD FAIL `traversal_cap_exceeded_groups=1`: real MHC GAMP exceeds the
  default `--max-traversals-per-read 100000` on at least one read group. Two build-blocking
  points recorded: a `vg chunk -p` region slice of a GBZ drops the GBWT haplotypes `vg rna`
  needs (use `-T`/embedded paths or `--gbz --contig`), and the D037 traversal-cap hard-fail
  policy needs a production-graph decision. Research only; no production code, fixtures, or
  committed artifacts created.
- 2026-07-01: Adopted D048 and rewrote the planning docs around it. The GAMP-to-RAD core is
  now graph-native transcript feature counting: score aligned nodes under vg's own scheme,
  attribute to the HST paths crossing each node, take the top HST score across all of a
  read's alignments plus ties, collapse HST copies to unique transcript IDs, and record
  orientation by majority of aligned bases. This removes complete-traversal enumeration (and
  the cap that hard-failed on real MHC GAMP), runtime GTF projection and exon/intron/junction
  compatibility, and the explicit collapse manifest, and it dissolves the earlier
  reference-path-vs-HST fork. Confirmed from vg source that per-node scoring reproduces
  `Subpath.score` (`alignment_scorer.cpp`) and that HST paths are stored transcript 5'->3'
  (`transcriptome.cpp`). Wrote `docs/conversion-algorithm.md`, added D048, rewrote
  `docs/compatibility-semantics.md`, and updated `docs/product-spec.md`, `TASKS.md`,
  `PROGRESS.md`, and the `CLAUDE.md` index and principles. Remaining doc-ecosystem updates
  (input/output contract, validation contract, fixture oracle, glossary, historical
  pointers) in progress. No production code changed yet.
- 2026-07-02: PathTally increment 1 implemented and verified. Added `src/pathtally_score.hpp`
  (per-node reproduction of vg's `score_partial_alignment`: match +1, mismatch -4, affine
  gaps 6/1, +5 read-end bonus, soft-clip and affine-carry handling, plus a subpath read-offset
  propagator) and `tests/vg/pathtally_score_test.cpp`. Synthetic unit asserts hold exactly;
  the committed CTest (`pathtally_score_test`) runs those. Against the real MHC GAMP
  (`build/real_mhc_scratch/mhcA.gamp`, 13.1M subpaths) the flat scorer reproduces
  `Subpath.score` exactly for 93.14%; the remainder are base-quality-adjusted lower-quality
  bases (confirmed: deviating subpaths mean base quality 22.4 vs 34.4 for exact matches, and
  single-base mismatches score -3 at q25 / -2 at q11 instead of -4). Adopted the flat per-node
  score as the ranking weight; exact quality-adjusted reproduction deferred as unnecessary.
- 2026-07-02: PathTally increments 2-5 implemented and verified, plus the optional
  quality-adjusted scorer. Added `src/pathtally.hpp` (node-to-HST attribution via injected
  lookup, transcript-id collapse, top-score-plus-ties winner selection across a read's
  alignments, majority-of-bases orientation) with `tests/vg/pathtally_pipeline_test.cpp`
  (synthetic, all cases PASS). Rewrote `src/main.cpp` on PathTally: inputs are now
  `--gamp/--xg/--t2g` (no GTF/manifest), with `--score flat|qualadj`; it converts the real
  1M-read MHC GAMP (against `hst_index`) to a valid `map.rad` (39,103 records),
  `tx2gene.tsv` (1022 transcripts), and summary in ~6 min (per-node path lookup is the perf
  hotspot). Reimplemented vg's QualAdjAligner in `src/pathtally_qualadj.hpp` (recovered
  log-base, quality-indexed log-odds matrix, per-base scoring) and validated it reproduces
  `Subpath.score` for 99.2% of subpaths (residual +/-1 is score-matrix rounding). alevin-fry
  0.15.0 consumed the real PathTally RAD through generate-permit-list (46 corrected barcodes),
  collate (257 chunks), and quant to a 257-cell x 317-gene count matrix, completing increment
  5's interop on real data. Increment 6 is also verified: an independent Python oracle
  (`tests/vg/pathtally_oracle.py`, reimplementing steps 1-4 from the graph P-lines without
  using PathTally code) matched panCollapse's RAD exactly for all 420 emitted records across a
  10,551-read-group real MHC-GAMP sample. All six PathTally increments are implemented and
  verified. Remaining (hygiene, not algorithm): retiring the old traversal-era CTest fixtures
  (`phase2_*`/`phase3_*`/projection, which invoke the removed CLI), self-contained CTests for
  the real-data score and oracle checks (they depend on build-dir scratch), and the
  per-node-lookup performance work. New code (`src/pathtally*.hpp`, rewritten `src/main.cpp`,
  `tests/vg/pathtally_*`) is uncommitted.
- 2026-07-02: Verified PathTally on a deterministic `vg sim` fixture. `vg sim -x spliced.xg
  -P <four HST paths> -n 400 -l 90 -s 1 -q` -> `tests/vg/fixture_tag_reads.py` (synthetic
  CB/UMI) -> `vg mpmap -n rna -F GAMP` -> panCollapse emitted RAD for all 400 read groups,
  byte-identical on re-run, with the four source transcripts in the dictionary; the
  independent oracle matched all 400 records exactly. Committed the PathTally implementation
  as `07000a0` and the vg sim fixture verification as `6bc16eb`.
- 2026-07-02: Implemented streaming GAMP input and a seek-and-backpatch streaming RAD writer
  (D049). `--gamp -` reads GAMP from stdin (`vg mpmap ... | panCollapse convert --gamp -`); the
  writer emits the header, target dictionary, and tags with a placeholder `num_chunks`, streams
  each record as its group flushes, and seeks back at finalize to patch the exact chunk
  byte/record counts and `num_chunks`. Verified byte-identical to the buffered writer on the
  sim fixture, byte-identical between file and stdin input, the oracle still exact, and a
  no-emit run leaves a header-only `num_chunks = 0` file. This supersedes D045's
  `num_chunks = 0` output and resolves the chunk-count-versus-alevin-fry conflict.
- 2026-07-02: Cleaned up the test suite and optimized the lookup. Retired the old traversal-era
  CTest fixtures (`phase2_*`, `phase3_*`, GTF-projection smoke) that invoked the removed CLI,
  and added a self-contained end-to-end CTest: a committed tiny graph + multipath GAMP
  (`tests/vg/fixtures/pathtally_smoke/`) is converted to xg/GAMP by vg, run through panCollapse,
  and checked exactly by the independent oracle (`pathtally_smoke_*`; 3 emitted records
  covering haplotype collapse, a multi-gene record, and reverse orientation). Optimized the
  per-node HST lookup by precomputing a path-handle -> HST-name map (removing per-step
  `get_path_name` + string-set lookup); output stays byte-identical and the 1M-read MHC run
  dropped from ~5m55s to ~3m57s. The pathtally + pure suite passes 9/9.
- 2026-07-04: Cleared the pre-release review (`docs/v0.1-review-findings.md`) for a full v0.1.
  Blockers: multi-chunk RAD writer with per-chunk and file-level seek-and-backpatch removing the
  single-chunk u32 ceiling (B3); hermetic alevin-fry interop CTest asserting the exact
  cell-by-gene matrix (B1); scoring-fidelity CTest that simulates reads from the HST paths, maps
  them with mpmap, and checks the flat and quality-adjusted per-node scorers against vg's own
  Subpath.score, plus stdin byte-identity and molecule/recurrence failure-mode tests (B2). Then
  every non-blocking finding N1-N11: spec reconciliation to D048/D049 (N1); documented
  completed_names memory cost (N2); GAMP/xg node-id-space hard failure with a regression fixture
  (N4); quality-adjusted scorer bounds checks (N5); qualadj cross-platform determinism caveat
  (N6); parameterized and documented build paths (N7); -Wall -Wextra on project targets (N8);
  --version/--help success paths (N9); atomic map.rad via temp-path-and-rename (N10); and the
  per-emitted-group target-size histogram in summary.tsv (N11). N3 was resolved earlier as
  working-as-intended. Recorded as D050. Full CTest suite: 30/30. Commits c4df275 (blockers),
  e7e2156 (N1), then 29b6794, a3a38e9, bf31524, 9fd1e3a, 92bc8a1, a8e5bea, c675cc8, 90b9a21,
  1bcc288 (N-tasks), 1aa2567 (findings closeout).
- 2026-07-05: Performance session. Optimized the per-node HST lookup by memoizing each
  distinct node's HST crossings on first visit (lazy cache in `src/main.cpp`), so a node's
  path steps are scanned at most once per run instead of once per node-visit. Measured on the
  real 1M-read MHC GAMP against `sampleA` `spliced.xg`: wall clock 3:47 -> 2:44 (about 28%,
  1.39x), `map.rad`/`tx2gene.tsv`/`summary.tsv` byte-identical (sha256 `7df66891...` on
  map.rad), peak RSS +~16 MB, full CTest 30/30. Recorded as D051, which also captures two
  requested analyses: the run is ~99% single-core CPU-bound so per-read-group work is the
  natural parallel unit (producer/worker/writer with an order-preserving reorder buffer for
  byte-identical output), identification only under D045; and a sorted/indexed GAMP is judged
  not worthwhile because the tool is a single linear streaming pass with no random access.
  Committed on branch `perf/node-hst-cache` as `f3bda22`.
- 2026-07-05: Found the dominant lever while profiling: the build set no `CMAKE_BUILD_TYPE`,
  so the shipped binary (and the v0.1 Docker image, which packages `build/src/panCollapse`)
  compiled at `-O0`. Building `-O2` (RelWithDebInfo) cut the 1M-read MHC run from 2:44 to
  0:45.6 (about 3.6x, ~5x over the original `-O0`), byte-identical, CTest 30/30; `-O3` was
  within noise (0:46.1) and loses symbols, so RelWithDebInfo is the default. Made the
  optimized build the default in `CMakeLists.txt`, bumped the version to 0.2.0, and built the
  `josephlalli/pancollapse:v0.2` runtime image from the `-O2` binary. Verified the image
  reports `panCollapse 0.2.0` and that a containerized smoke conversion is byte-identical to
  the host `-O2` run. Recorded as D052.
- 2026-07-06: Added an opt-in BAM output (`--bam-out`, D054) for a CellRanger-style counting
  stack (`umi_tools count --per-gene --gene-tag=XT` -> DropletUtils `emptyDropsCellRanger`),
  designed via plan-mode approval since a new output backend returns to a gate. One mapped
  record per emitted read on a synthetic per-gene contig, carrying 10x tags (`CB`/`CR`,
  `UB`/`UR`, full-set `GX`, `GN`, and single-gene-or-omitted `XT`); genes come from the same
  t2g the RAD uses, so no surjection and no new runtime input. New `BamWriter` (htslib, already
  a transitive dep) in `src/main.cpp`; `cmake/PanCollapseVg.cmake` links `htslib`. Ten hermetic
  `bam`-label CTests (quickcheck, header, RAD byte-identity with/without `--bam-out`, oracle
  `GX` parity, `umi_tools count` vs a naive tag recount, position independence); full suite
  40/40. Real 1M-read MHC: `map.rad` byte-identical (sha `7df66891...`), BAM 39,103 records over
  317 gene contigs, and the sort/index + `umi_tools count` pipeline yields 30,105 molecules over
  9,701 cells x 203 genes (2,755 multi-gene reads skipped for lacking `XT`). Bumped to 0.3.0.
  Documented in `docs/bam-export.md`, README, and `docs/input-output-contract.md`. umi_tools
  1.1.6 installed to `~/.local`; the BAM tests need a python with pysam (the pysam+umi_tools
  interpreter is `python@3.11`, passed via `-DPython3_EXECUTABLE`).
- 2026-07-07: Added GeneFull (intron-inclusive) counting (D055) so intronic reads call their gene,
  closing the exon-only gap vs STARsolo `--soloFeatures GeneFull` (surfaced while explaining a
  counter comparison: the STAR-CR arm counted introns via GeneFull; panCollapse is spliced-only,
  an ~6-7% MHC GEX confound). Final design uses **no panCollapse code or flag**: GeneFull is the
  ordinary spliced count run against a graph annotated with gene-body paths, selected by the t2g.
  panCollapse reads gene-body membership off embedded paths exactly as it reads transcript
  membership off HST paths, so the same graph gives a spliced count (HST t2g) or a GeneFull count
  (gene t2g). `scripts/make-gene-annotation.sh` embeds one unspliced gene-body path per gene
  (`vg rna --feature-type gene`, `-l` for alt coverage) and writes the gene t2g. An interim
  attempt (a `--gene-mode full` flag + `--gene-loci` node->gene TSV + parallel lookup) was removed
  before merge: it duplicated the graph's own path index (a custom node->gene map is the very
  "custom index" the transcript path avoids), and spliced mode already did GeneFull with a
  gene-body t2g. Verified edge cases: exon-intron boundary and multi-exon+spanned-intron reads are
  called either way (the exon node is on the HST), so GeneFull's only marginal call is the
  purely-intronic read; overlapping bodies give multi-gene on full overlap and the dominant gene
  on partial overlap; a single t2g must not list both layers. Hermetic `genefull` CTests annotate
  the fixture graph and show the SAME graph drops the intron with the HST t2g and calls it with
  the gene t2g. Full suite 45/45; the converter is byte-identical to v0.3 (no binary change).
  Docs in `docs/genefull.md` + README.
- 2026-07-07: Added an opt-in target-relative orientation filter `--strand {both,forward,reverse}`
  (D056), applied after winner selection: `forward` keeps sense targets, `reverse` antisense,
  `both` (default) filters nothing. Reads whose targets are all filtered emit no record and are
  counted in a new `strand_filtered_groups` summary counter. This reintroduces (as opt-in) the
  strand filtering D042 had pushed downstream, matching STARsolo `GeneFull_Ex50pAS`/`--soloStrand`;
  the default `both` is byte-identical to before and still writes `dirs`. Hermetic `strand` CTests
  on the smoke fixture (readfwd/readmulti forward, readrev reverse): `forward` emits 2
  (`strand_filtered_groups=1`, no readrev), `reverse` emits 1 (`=2`). Full suite 47/47.
- 2026-07-07: Added ledger count modes reproducing STARsolo/CellRanger exactly (D057):
  `--count-mode {score,gene,genefull,genefull_exonoverintron,genefull_ex50pas}` + `--body-t2g`.
  The ledger counts per read per gene the aligned match bases on exon vs intron nodes, then
  applies the mode rule (gene = >=50% exonic; genefull = any body overlap; the variants add
  exon-overlap tie-breaks and Ex50pAS's 100%-exon-antisense exclusion), then CellRanger `Unique`
  (a read kept for >1 gene is dropped, counted in `multigene_dropped_groups`). Membership is two
  ordinary path->gene t2gs (exon/HST via `--t2g`, gene-body via `--body-t2g`) read off the graph's
  embedded paths -- no custom node->gene map (memory: graph-native-feature-annotation). Stays
  O(nodes) like D048 (base counts instead of scores, no traversal). Default `score` is the
  unchanged D048 count. Pure rule in `pathtally_ledger.hpp` with a 14-assertion unit test; graph
  integration tests (`genefull_ledger_gene`/`genefull`/`ex50pas_antisense`, plus a manual
  multi-gene Unique-drop check). Full suite 51/51; converter byte-identical without the flag.
- 2026-07-05: Profiled the `-O2` binary (callgrind, 1M-read MHC) and acted on it (D053). The
  scorer is ~1%; the run is dominated by GAMP ingestion (~60%) and the per-group tally (~34%),
  with malloc/free (~27%) and string-compare memcmp (~13%) the top categories. Multithreading
  judged low value (only ~34% parallelizable, Amdahl-capped near 1.5x without BGZF-stream
  sharding). Applied two byte-identical single-threaded wins: move each parsed record into the
  group buffer instead of deep-copying (`main.cpp`), and key the per-group tally with a reused
  `absl::flat_hash_map` workspace instead of a per-group `std::map` (`pathtally.hpp`,
  `main.cpp`, plus `absl_raw_hash_set` in `cmake/PanCollapseVg.cmake`). Measured 45.6s -> 41.2s
  (move) -> 30.4s (flat tally), byte-identical each step, RSS 393 -> 281 MB, CTest 30/30. End to
  end the v0.2 build is ~7.5x faster than the original `-O0` v0.1 (227s -> 30s), identical
  output. Rebuilt the `josephlalli/pancollapse:v0.2` image on the fully-optimized binary.
- 2026-07-20: Implemented D062's optional third t2g column for CAT-aware path-to-canonical
  transcript collapse. Score mode selects the tied top raw graph paths before resolving aliases;
  ledger mode maps every raw exon path to the canonical target id and retains MAX (not sum)
  score collapse across copies. Two-column `_H<n>` / `_R<n>` behavior and ledger bare-t2g
  fallback remain unchanged. Added a raw-selection unit regression, an independent RAD oracle
  alias fixture, both conflict-validation cases, and a ledger integration fixture with two raw
  paths collapsing to one canonical transcript, including a body-dominant MAX-vs-sum guard.
  Full CTest passes 75/75.
- 2026-07-20: Implemented D063's optional three-column transcript-body t2g. Raw body copies and
  fragments MAX-collapse to canonical transcripts; S/U classification, splice ownership, and GD
  remain transcript-specific until count_cr pools `TX` into genes. Fragmented d46 body paths join
  endpoint evidence for splice ownership unless any body path contains the endpoints adjacently.
  Added focused pure/integration
  coverage for cross-isoform isolation, raw-body MAX collapse, transcript-specific geometry, and
  validation failures. Legacy two-column body behavior remains covered by the pre-existing suite;
  full CTest passes 86/86.
- 2026-07-20: Implemented D064's production path-identity interface. Added the strict
  `panSC-path-identity-v1` reader, exact XG name/length binding, path -> Parent -> canonical MAX
  collapse, opaque literal suffix handling, and production BAM `TX`/`XP`/`XU` provenance in score
  and count modes. The synthetic fixture covers multiple paths per Parent, multiple Parents per
  canonical transcript, sequence-identical paths with distinct canonical identities, exon/body
  crosslinks, literal `_R1` plus hashed `panSCup1_`/`panSCbody1_` Parents preserved in `XU`, parser
  conflicts/delimiters/fallbacks, and missing/mismatched XG identity. All legacy
  integration calls now select `--legacy-adapter hst-v1` explicitly. Full CTest passes 92/92.
- 2026-07-26: Implemented D066 exact STARsolo 2.7.11b `GeneFull_Ex50pAS` producer evidence.
  A body-path-bound dynamic program propagates through GAMP DAGs and emits Parent-preserving E/P/B
  slots in parallel `TX`/`GX`/`GD`/`GT`/`XP`/`XU`, without traversal enumeration or canonical
  pre-collapse. The fixture covers E, P, exact-half B, splice-discordant P, reverse E-AS, two locus
  Parents on one canonical transcript, branching alternatives, and a count_cr-ready nine-UMI seam
  oracle (eight expected `GENE` UMIs). Legacy Ex50pAS and missing BAM/all requirements hard-fail;
  exact and ordinary conversions write byte-identical RAD. Focused CTest passes 14/14; full CTest
  passes 101/101.
- 2026-09-04: Implemented D076/D077 native direct gene counting for v0.10.0 on the isolated
  PanCollapse branch `feature/v0.10.0-native-count`, based on `main` at `5f4daff`. Added the
  additive `panCollapse count` command while preserving `convert`: immutable `cr7-v1` and
  `pansc-strict-v1` profiles (joint or separate execution), sensitivity-only typed derived
  profiles, authenticated `panSC-count-facts-v1` loading, exact-whitelist barcode priors and
  posterior correction, oracle-ordered UMI filtering, non-transitive `1MM_CR`,
  `MultiGeneUMI_CR`, sharded numeric assignment caching, deterministic aggregate/spill merging,
  and explicit pre-UMI gate state. Default output is atomically published Parquet barcode,
  feature, count, and molecule tables with decoded per-profile logical hashes; raw 10x MEX, byte-identical
  pre-count RAD, and ordered per-read diagnostic Parquet are opt-in. Native count has no BAM sink
  and no production Python dependency. The CMake/Docker release surface is 0.10.0 with exact
  Arrow/Parquet 17.0.0 and Zstandard 1.5.7 pins. The standalone release image gate ran both
  profiles and every count format successfully as an unprivileged UID and verified that no Python
  executable or `libpython` enters the image. Full PanCollapse CTest passes 150/150; the separate
  panSC annotation-fact compiler passes 9/9 pytest cases; `git diff --check` and release-script
  syntax checks pass.
- 2026-09-04: Applied the fifteen code-review findings against the v0.10.0 native count commit
  `88d963f` (D078). The runtime MultiGeneUMI_CR raw guard now matches the frozen rule for
  sequences relabeled inside one gene, `content_id` has a documented canonical form the loader
  implements, novel-paralog lump/separate are effective, strand and representative selection
  are evidence-order independent, and `count` parses identity, folds case, canonicalizes
  `--out-dir` and profile selectors, and rejects convert-only options like `convert` would.
  The bundle loader validates equivalence and paralog-origin closure, spill cleanup is RAII, and
  the CLI thread tests force the worker pool (new
  `direct_count_threads_4_spool` covers the spool-only ordered path). 151/151 CTests pass;
  every CLI-level finding was re-run empirically on the ex50 fixture. The D076 scientific gates
  (one-million-read subset, chr20, chr21, joint chr20-22) must be rerun before release because
  the raw-guard correction changes molecule tables wherever a UMI collapses away in a gene with
  more raw reads, and because the restored post-resolution tagged-last-resort retry (D078 item
  6) changes `pansc-strict-v1` gene assignments, so `counts.parquet` and reason bits can differ
  on reads with no UMI collapse at all.
- 2026-09-04: The large scientific release gate is intentionally still pending, not silently
  inferred from fixtures: frozen Python differential on the canonical one-million-read subset,
  full chr20 and chr21, then the immutable joint chr20–22 GAMP without remapping. The historical
  `/reload` instruction is superseded by the automatic documentation-reconciliation checkpoint
  recorded in the current production state. Required joint outcomes remain exact oracle matrices/counters,
  default persisted output at most 8.4 GB, at least twofold wall-time improvement over the matched
  7 h 48 m v0.9 producer, completion under one day, and peak RSS below 600 GB. Failures may change
  implementation/resource behavior, not the frozen biological rules.
- 2026-09-11: Folded the four annotation-chain commits (`f021280`, `22ef3b2`, `73c5609`,
  `8080f49`) onto the compatibility-replay integration branch as `2535fc7`, `c48a563`,
  `28e4870`, `02f717e`, without rewriting the checksum-pinned branch. The fold changed 21
  files and none under `src/`, `tests/`, or any CMake file, so it cannot alter the binary,
  the suite, or any checksum. 165/165 CTests pass and the provenance guard passes through
  its new `scripts/tooling/` path.
- 2026-09-11: Revalidated the accepted one-million-read replay receipt on the folded branch
  rather than assuming the review fixes in `b8210ab` changed it. Six of the seven
  replay-count receipt entries are byte-identical and `manifest.json` differs in 4 of its
  126 leaf fields, all provenance or timing. Neither predicted divergence occurred: the
  fixture holds no equivalence aggregate with a support and gene tie differing in
  `nested_host`, and no malformed-quality read. The `b8210ab` fixes are therefore
  output-neutral on this fixture, measured rather than argued. Replay ran 767.80 s at
  63,040,692 KiB against the accepted 775.92 s and 63,048,652 KiB; both are single runs with
  no noise floor, so neither delta is claimed.
- 2026-09-11: Instrumented replay initialization and retargeted the speed work (D080). The
  streaming read loop the parallel-replay proposal aimed at is 14.06 s, 1.9 percent of wall.
  `fact_set_resolve` is 519.57 s and `compatibility_open` is 127.93 s, together 85.6 percent.
  Still unmeasured: whether the fact-set resolve loop scales with threads, and the noise floor
  on any wall-time comparison.
- 2026-09-11: Parallelized the replay fact-set resolve loop over ordered blocks (D081). The
  phase fell from 519.57 s to 24.59 s at 128 workers and total replay wall from the accepted
  775.92 s to 304.85 s, a 2.545x speedup, with peak RSS up 1.6 percent to 64,078,328 KiB. The
  accepted receipt is reproduced: `summary.tsv` and all five Parquet tables, including
  `read_assignments.parquet`, are byte-identical. 165/165 CTests pass. The parallel run ran
  under heavier system load than the serial baseline, so the speedup is a lower bound. With
  this phase reduced, `compatibility_open` at 127.93 s uncontended is now the largest single
  phase and the next target; it performs two full traversals with copies for integrity checks
  that need none.
- 2026-09-11: Removed both copies from the compatibility open integrity checks (D082). The
  canonical check now uses an in-place predicate and the logical hash reads the reader's fact
  index directly instead of copying the whole collection into a temporary bundle. Replay wall
  fell from 304.85 s to 251.10 s and peak RSS from 64,078,328 KiB to 37,347,844 KiB; against
  the accepted 775.92 s and 63,048,652 KiB that is 3.09x faster at 59 percent of the memory,
  with the accepted receipt reproduced byte for byte and 165/165 CTests passing. Because no
  test covered the non-canonical rejection branch, the new predicate was checked against the
  original on 400,000 randomized fact sets, 241,899 of them non-canonical, with no
  disagreement. Remaining phases are the Parquet fact decodes at roughly 37 s and 29 s under
  load, the count fact catalog and identity ledger reads at roughly 29 s each, and the
  streaming read loop at 14 s.
- 2026-09-11: Confirmed the replay speedup with an interleaved A/B rather than single runs taken
  under different machine states. The pre-optimization commit `02f717e` was built in its own
  worktree and the two binaries were run alternately, twice each, re-verifying the accepted
  receipt every time; all four runs reproduced it. Baseline wall 834.59 s and 729.33 s, mean
  781.96 s, spread 105.26 s; optimized 246.71 s and 194.53 s, mean 220.62 s, spread 52.18 s.
  That is 3.544x on means, 3.170x across the three runs sharing a load of 28 to 31, and 2.956x
  on the most conservative pairing; the 482.62 s gap between the fastest baseline and the
  slowest optimized run is 4.6 times the largest within-arm spread, so it clears the noise
  floor. Peak RSS fell from a mean 63,039,050 KiB to 37,335,760 KiB, 40.8 percent, against a
  largest within-arm spread of 35,888 KiB. The machine was never idle: unrelated `zstd` and `vg`
  jobs held the load average near 29, so these are shared-machine figures, and the quietest run
  (load 12.56) was also the fastest at 194.53 s.
- 2026-09-11: Brought `.claude/rules/annotation-identifiers.md` onto this branch and fixed the
  four identifier defects the audit confirmed in the folded chain (D083): transcript keying in
  `filter_cellranger_annotation.py` and `gff3_to_genebody.py`, feature-level gene resolution in
  `local_gene_for`, and a hash-seed-dependent tie in `chain_verdict`. Nothing in the repository
  invokes these scripts, so the suite could not have caught any of it; each fix was checked
  against its pre-fix version instead. Reference GTF output is byte-identical; on colliding CAT
  input the old code emitted one transcript body instead of two and lost both genes of a
  haplotype pair to the other haplotype's `readthrough_transcript` tag. Left open: the body
  gene in `gff3_to_genebody.py` still comes from bare `gene_id`, which is a t2g question rather
  than an identifier bug.
- 2026-09-11: Acted on an adversarial review of the branch (D084). Confirmed and fixed the
  writer-side duplication of the whole fact index, bounded the spill decompression window and
  buffers, and repaired two defects in `filter_cellranger_annotation.py`: child rows were being
  resolved in the bare `gene_id` namespace while the allowlist held qualified identities, which
  dropped every exon of a retained gene (a regression introduced by D083), and the GTF parser
  rejected GENCODE's unquoted attribute values so no real reference GTF could be read. One fact
  index is about 25 GiB by three agreeing estimates, and finalize held three copies; the spill
  merge measured 254,588 KiB peak at a 1 MiB budget against 220 MB for the same job that never
  spilled, improving 2.45x once the window was bounded. 165/165 CTests pass and the accepted
  receipt reproduces byte for byte at 221.01 s and 37,351,368 KiB. The writer saving still needs
  a producer run to confirm end to end. A bounded fan-in merge was prototyped and shown
  output-identical but not adopted; it needs the run row count moved out of the compressed frame.
- 2026-09-11: Narrowed the count-fact bundle and ledger to what the runtime actually reads
  (D085), after establishing that five of nine bundle roles are never opened and nine of
  twenty-four ledger columns are never read outside the ledger module. A narrowed bundle of
  four roles and a fifteen-column ledger both load and parse identically; a ledger missing a
  genuinely required column is still refused. Also replaced the per-row XG path lookup, two
  FM-index locates per ledger row, with a single graph enumeration, now timed as
  `ledger_path_handles`. 165/165 CTests pass. This is an input-contract change, not a speed
  one: on a three-million-read production subset the ledger read is 5.1 percent of wall and
  the bundle hash pass about six seconds of 965. It also does not achieve portability, since
  the annotation in the bundle is itself a panSC product carrying 1,231,247 synthesized
  transcript-body rows. Next runtime target is `splice_geometry` at 317.45 s, 32.9 percent,
  which evaluates 141,543,588 candidate edges to keep 5,937,500.
- 2026-09-11: Cut `splice_geometry`, the largest phase of a three-million-read production run at
  317.45 s and 32.9 percent, by batching its work units and probing the small side (D086). The
  phase called the ordered-block helper with a block size of one, so 1,164,040 transcripts each
  took a scheduler handshake and a commit ring slot, and its body loop walked roughly 1,578 path
  nodes against an index of about 115 candidate edges rather than the reverse. Interleaved
  against `4afb084` on a 20,000-read GAMP, since the phase is read-count independent, two
  repetitions per arm: 468.98 s mean before against 86.05 s mean after, 5.45x on means and 4.36x
  on the most conservative pairing, with a 351.86 s gap against a 37.53 s largest within-arm
  spread, so it clears the noise floor by 9.4 times. All four phase counters are identical across
  all four runs, including `fragment_only_target_edges=207`, which the inverted loop computes.
  165/165 CTests pass. Take 4.36x as the floor; the machine carried unrelated jobs at load 150 to
  209 throughout. The 95.8 percent veto rate that first
  drew attention to this phase was a red herring: the discarded candidates are cache-resident
  hash lookups and cannot be identified without building the body index that dominates the cost.
  Re-measure on a quiet machine before quoting the speedup.
- 2026-09-12: Halved peak memory on the three-million-read production subset, 122.1 GiB to
  59.0 GiB, with byte-identical output and 165/165 CTests passing (D087). A first-principles
  budget for this job is about 750 MiB, so the run was using 160 times what the workload needs.
  Attribution found `assignment_cache` holding 53.36 GiB because its key is the full
  concatenated text of every candidate's evidence, averaging over four hundred kilobytes per
  entry with the entry count bounded but the entry size not; it now stores a SHA-256 digest.
  `exact_ex50_models` held 22.65 GiB in 1.23 million `std::set<ExactEdge>` geometries, about
  eighty bytes of red-black node per thirty-two byte edge; they are now sorted unique vectors
  queried by binary search. Three earlier hypotheses were measured and rejected first: glibc
  arena count, heap fragmentation, and the node ledger cache. Still open: the remaining 59.0 GiB
  is dominated by the XG at about 23 GiB, the path identity ledger at 3.41 GiB of `std::string`
  fields that interning would reduce to tens of megabytes, and the never-evicting
  `node_ledger_cache` at 6.26 GiB.
- 2026-09-12: Extended `PANCOLLAPSE_PROFILE_TIMING` to attribute the whole per-read path, not
  just the parts that were suspected. The first attempt bracketed from candidate assembly and
  covered 47.8 percent of worker compute, which would have sent optimization at the
  second-largest region while the largest stayed unmeasured; the reconciliation line now prints
  `region_sum_seconds` beside `compute_total_seconds` so a shortfall is visible in the output.
  With full coverage on the three-million-read subset, 24,744.2 of 24,745.5 worker-seconds are
  attributed: `evidence` 38.6 percent, `facts` 27.9, `signature` 14.8, `tail` 8.9, `tally` 7.0,
  `digest` 1.8, `resolve` 0.8, `observe` 0.1, `prelude` 0.0. That is 8.25 ms of worker CPU per
  read, of which the counting itself is 0.1 percent: everything expensive is deciding what to
  count. Regions are per-worker on their own cache lines and gated behind the environment
  variable, and output is byte-identical. Next targets in order: the assignment signature and
  its digest at 16.6 percent combined, then `evidence` and `facts` at 66.5 percent, which are
  both bounded by the roughly 121 candidates evaluated per read against a prefilter that removes
  only 6.1 percent of 1.43 billion.
- 2026-09-12: Streamed the assignment signature into its digest instead of materializing it.
  The old path built the text three times per read group: the per-candidate rows, their
  concatenation into roughly four hundred kilobytes, and an outer copy per profile. A buffered
  `SignatureDigest` now feeds SHA-256 from an eight-kilobyte stack buffer, candidate rows still
  being built and sorted because that sort is what makes the key independent of candidate
  order. `body_support_units` also sorts pointers rather than copying the sample-name vector
  once per candidate. Worker CPU on the three-million-read subset fell from 24,745.5 to
  16,912.8 worker-seconds, 8.25 ms to 5.64 ms per read, and `digest` alone fell from 446.7 to
  10.1 worker-seconds for the same hash over the same logical content: almost all of it was
  materializing the input, not hashing. Output is byte-identical and 165/165 CTests pass. The
  regions I did not touch also fell 25 to 57 percent, which is consistent with reduced
  allocator contention across 128 threads but was not load-controlled, so the 1.46x total is a
  single-run figure pending an interleaved A/B. The cache key encoding changed, which is safe
  because it is an in-process key rather than a persisted content address; it remains
  self-delimiting and therefore injective.

- 2026-09-13: Added a second count-fact authority, the `--annotation` route, and recorded it as
  D088, accepted the same day. It had been raised as a proposal because it appeared to
  contradict `docs/product-spec.md`'s no-GFF3-at-runtime clause; on approval the user resolved
  that the clause was only ever meant to bar runtime GTF projection, which this route does not
  do. It does amend D076's "the bundle is the only assignment authority". D085 had
  already established the aim and explicitly deferred this decision. `panCollapse count` now
  takes exactly one of `--count-bundle` or `--annotation`, the latter with
  `--annotation-reference` and `--path-identity-ledger`, and
  `CountFactCatalog::from_annotation` builds the same `parents_`, `genes_` and `profile_genes_`
  structures the bundle loader builds, parsing gzip through zlib and writing nothing to disk.
  The derivation rule was established first in Python, `scripts/derive_count_facts_from_annotation.py`,
  at 100.00% agreement against the identifier-fixed bundle; that harness is a validation
  artifact, not a pipeline step. Two derivation details each cost a defect and are recorded in
  D088: reference gene tags must be inherited through `source_gene`, because CAT drops them on
  projection and omitting the step under-tags silently rather than failing; and `count_gene`
  must come from the loader's own `normalized_locus()`, since an independent reimplementation
  that kept the Ensembl version suffix disagreed with the bundle. The receipt gains
  `fact_authority` beside `fact_bundle_content_id`, both now validated non-empty;
  `fact_bundle_content_id` was deliberately not renamed so existing readers keep working, and on
  the annotation route it holds a digest over the three input files' SHA-256s rather than a
  published bundle id. A dereference of the `nullopt` `count_bundle` at two sites was undefined
  behavior that produced an empty string rather than crashing, and aborted a completed
  47-minute chr19 production run at the output-writing step with `direct-count output metadata
  is incomplete`. Open and unresolved: no test drives `--annotation` through
  `write_count_outputs`; a bundle-versus-annotation bit-exactness A/B on the one-million-read
  chr20/21/22 fixture is running and its result is not known, so the two routes must not yet be
  described as agreeing; `--compatibility-in` replay still loads a bundle unconditionally and
  does not accept `--annotation`, and does not currently refuse the combination either;
  `protected_primary_protein_nested_host` is left false on the derived catalog because it is a
  spatial relation the annotation does not state; the factory reads `nested_host_gene` only from
  a gene row while `tag_nested_reference_hosts.py` writes it onto every row carrying a matching
  `source_gene`, so whether nested-host policy is reachable at all on this route is unconfirmed;
  and the recognized category and gene-type
  vocabularies are compiled into the binary on this route where the bundle route reads them as
  data. Separately, chr19 shows 514 unorientable body/exon pairs across 330 parents, 0.061
  percent, which is fixture generation skew rather than a counting defect; it is handled by
  degradation recording `body_path_degrade_reason=repeated_exonic_node`, and the planned fix is
  rebuilding the body layer from the corrected annotation with `scripts/gff3_to_genebody.py`.
  Also in the working tree, the per-read `transcript_gd` map became a `std::vector<char>`
  parallel to `calls`, valid because `src/pathtally_ledger.hpp` builds `calls` by draining a
  `std::map` keyed on transcript target id, so the ids are unique and ascending and a call's own
  position is a complete key. Documentation was brought into line in the same pass:
  `docs/product-spec.md`, `CLAUDE.md`, `docs/input-output-contract.md`,
  `docs/validation-contract.md`, `README.md`, and a new `scripts/README.md` describing the
  annotation correction chain.

- 2026-09-21: Documentation reconciliation for the full-input throughput-cliff investigation. The
  full real-input native count is recorded as terminal and validated with its seal pending
  (817.857627 s/100k reads over 25,755,749 reads, peak RSS 137.476540 GiB, binary
  `3d13e082…d7d0f958`), and its within-run cliff at read ~22,046,000 is characterized from four
  cold 100,000-read windows: graph structure per alignment, not accumulated state and not
  multimapping. The allocator-substitution result was demoted from the current frontier and marked
  valid for the 1M-read prefix only, since that prefix sits inside the regime covering 85.0% of
  reads but 14.2% of read time. The earlier stage-attribution table was marked superseded by the
  mangled-prefix attribution tool. One accepted decoder fix (sampled `SpaceUsedLong`) and two
  instrumentation changes are recorded as uncommitted. A new record
  `docs/research/full-input-throughput-cliff-20260921.md` carries the full narrative; README,
  `PROGRESS.md`, `docs/decisions.md` D089, and the native-count performance record were reconciled
  to it. The unresolved upstream question — why the GAMP tail carries ~600x more graph structure
  per alignment — is recorded for the mapping pipeline owner. No source, test, build, or sealed
  evidence file was changed.
