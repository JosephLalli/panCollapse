# Full-input throughput cliff and decoder cost — 2026-09-21

This record covers native `count` processing performance on the full real input only. It says
nothing about converter performance, compatibility-replay performance, or biological counting
accuracy, and it authorizes no change to counting policy.

## Terminology: a "read group" here is one read

The runtime counters are named `input_read_groups` and `completed_read_groups`. That name
collides with the SAM/BAM `@RG` read group, which it does not mean. In panCollapse a group is
all GAMP records sharing one QNAME — that is, one read's alignments. The grouping boundary is
`alignment.name() != current_group.name` at `src/main.cpp:6321`, accumulating into
`struct Group` (`src/main.cpp:2004`, whose `std::vector<vg::MultipathAlignment> records` is at
`:2010`). This record therefore writes **read** and **alignments per read**; the counter names
are unchanged in code and in historical sections of other documents.

## Evidence roots

| Root | Contents | State |
|---|---|---|
| `/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z` | terminal full-input run | STATUS `VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`; not sealed |
| `/mnt/ssd/lalli/pancollapse_cliff_characterization_v1_20260919T194550Z` | four cold windows across the cliff | sealed, 101 files |
| `/mnt/ssd/lalli/pancollapse_profile_tools_v1_20260919T052434Z` | corrected symbol-attribution tool and re-attribution | sealed, 10 files |
| `/mnt/ssd/lalli/pancollapse_stage_attribution_v1_20260918T192832Z` | earlier stage table | sealed, 9 files; **superseded**, retained |
| `/mnt/ssd/lalli/pancollapse_inflight_depth_v1_20260920T181428Z` | in-flight batch depth sweep | sealed, 61 files; hypothesis rejected |
| `/mnt/ssd/lalli/pancollapse_producer_partition_v1_20260920T220538Z` | producer-side time partition | sealed, 27 files |
| `/mnt/ssd/lalli/pancollapse_cliff_profile_v1_20260920T232129Z` | first profile of the expensive regime | sealed, 21 files |
| `/mnt/ssd/lalli/pancollapse_spaceused_fix_v1_20260921T002651Z` | first iteration of the accepted fix | sealed, 26 files |
| `/mnt/ssd/lalli/pancollapse_spaceused_fix_v2_20260921T043810Z` | accepted fix, validated | sealed, 28 files |

## Why the investigation was needed

Every optimization measured before this programme — node-to-tally aggregation, ordered parallel
input, immutable Parent metadata reuse, the rejected fused exact-DP state trial, and the
allocator substitution arms — was measured on the real first-1M-read prefix. The prefix was
chosen because it is cheap enough to repeat. What was open was whether it is representative.
The full-input run answers that directly, and the answer is no: the prefix sits entirely inside
a cheap regime that ends partway through the input, so optimization estimates taken on it,
including the 17.13% allocator share and the roughly 1.2x ceiling implied by it, understate the
regime where the run actually spends its time.

## The completed full-input run

The run at `/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z` finished on
2026-09-18 at 13:23 UTC (08:23 local, -0500), per
`REPORT_RECEIPT.json` `created_utc`, with every stage exit 0. Its `STATUS` is
`VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`: the measurement and report are validated, and
the completion seal has not been applied. It processed 25,755,749 reads and 124,261,698
alignment records in a 210,645.357663 s read phase, which is 817.857627 s per 100,000 reads
(817.985728 s/100k including count finalization), at a peak process RSS of 137.476540 GiB.
Both runtime targets are missed by a wide margin: the preferred under-5 s/100k and the 16.5
s/100k fallback would have permitted 1,287.8 s and 4,249.7 s of read time respectively against
210,645 s observed.

Two limits bound every cross-run comparison drawn from it. First, it ran the 2026-09-15
exact-evidence/resident-finalize binary
`3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`, not the currently frozen
`6eb3e7ca…d6843a`, and therefore predates both ordered parallel native input and immutable
Parent metadata reuse. **Any absolute prefix-versus-full throughput comparison is confounded by
binary version and must not be quoted as a scale effect.** What is clean is the run's internal
behaviour, because every read inside it met the same binary and the same configuration. Second,
it is one observational run on a shared host against a chr20–22 reference; it establishes no
accuracy, parity, or whole-pangenome claim.

## The throughput cliff, measured within the run

Interval throughput holds between 300 and 2,000 reads/s from the start of the read phase to
read about 22,046,000. Inside a single interval of roughly 12,288 reads it collapses to about
21 reads/s and never recovers. Partitioned at that point, the first 85.0% of reads consume
14.2% of read time and the remaining 15.0% consume 85.8%; the mean per-read cost ratio between
the two regimes is 34x.

Three candidate explanations are excluded by the run's own receipts. Aggregation spill is not
involved: `aggregate_spill_bytes`, `aggregate_spill_rows`, and `aggregate_spill_runs` are all
zero for the whole run. Host contention is not involved: host load fell during the collapse.
And the binary reported nothing about it — 11 non-progress lines in 58.5 hours of stderr — so
the collapse was invisible to anyone reading the log rather than the interval timings.

## Cause: graph structure per alignment, not accumulated state

The characterization run cut four windows of 100,000 reads each from one pass over the
127.2 GB source GAMP, at reads 500,000, 12,000,000, 21,800,000 and 22,300,000, and counted each
window from a **cold** process on the current binary `6eb3e7ca…d6843a`. Because each window
starts a fresh process, no state accumulated from earlier reads can be present in the
post-cliff window.

| window | first read | alignments/read | subpaths/alignment | mappings/alignment | bytes/alignment | reads/s | candidates/read |
|---|---:|---:|---:|---:|---:|---:|---:|
| `w_early` | 500,000 | 2.57 | 8.34 | 34.10 | 111 | 2,641 | 542 |
| `w_mid` | 12,000,000 | 1.41 | 4.05 | 9.13 | 52 | 4,466 | 272 |
| `w_precliff` | 21,800,000 | 9.99 | 2.12 | 8.67 | 22 | 6,731 | 568 |
| `w_cliff` | 22,300,000 | 5.41 | 690.64 | 5,164.95 | 5,972 | 101 | 56,950 |

The cold windows reproduce the cliff, so accumulated in-process state is excluded as the cause.
Multimapping is excluded as well, and in the opposite direction from the intuition: post-cliff
reads carry **fewer** alignments each than the immediately preceding window, 5.41 against 9.99.
What changes is the graph structure inside each alignment — 326x the subpaths and 596x the
mappings of `w_precliff` — and the change is a shift of the whole distribution rather than a
handful of outliers: subpaths per alignment move from p50 1, p90 1, p99 7 to p50 6, p90 261,
p99 15,295. Measured post-cliff candidates per read of 56,950 confirmed, to within 2%, a 55,700
prediction made beforehand from the full run's own counters, so the full run's post-cliff
regime and the cold `w_cliff` window are the same workload.

## Why the costs appear only after the cliff

They do not appear; they scale into visibility. No software threshold is crossed anywhere in
this transition. Decode CPU per alignment rises 408x against 596x more mappings, slightly
sublinear rather than discontinuous, and decode's share of CPU moves from 3.4% to 25.6%. The
glibc allocator is not degrading under the larger objects: the ratio of slow-path work
(`malloc_consolidate` plus `unlink_chunk`) to fast-path work (`_int_malloc` plus `malloc`) is
0.98 in the prefix profile and 0.33 post-cliff, so the allocator is doing proportionally *less*
bin maintenance in the expensive regime, not more. The cost function is smooth and the input
steps.

Per read, only decode tracks structure, at 219x. Counting scales just 23.1x, and the reason is
the exact-Ex50 prefilter at `src/main.cpp:4488-4545`: a necessary-only set intersection of
per-node candidate model lists across a subpath's reference-consuming mappings, guarded to
subpaths with at least two such mappings. It cuts candidates 10.1x post-cliff against only
1.36x pre-cliff, because dense subpaths are exactly the case it was built for. Wall time scales
73.8x while total CPU scales 29.9x; the 2.5x gap between them is starvation, not work.

## Open question for the mapping pipeline owner (unresolved)

Source ordinals embedded in read names rise monotonically with file position, so this GAMP is
one continuous run in original sequencing order and not a concatenation of shards. Across the
roughly 725,000 source reads spanning the cliff, two things change together: alignment character
(median subpaths per alignment 1 to 34) and retention into the GAMP (about 69% locally in the
tail against about 6.6% cumulative early in the file).

**Why the tail of this GAMP contains alignments carrying roughly 600x more graph structure is
not established.** The candidates are flowcell or run-position effects, the upstream
`d10k_cap20k` capping scheme, and a permissive fallback re-mapping pass inside
`pangenome_mapping_reuse_v15r2`. The simultaneous shift in retention points upstream, at the
mapping stage, rather than at biology in the reads. This matters strategically and not only
descriptively: if these alignments are a mapper artifact, then optimizing panCollapse to consume
them faster treats a symptom, and the cheapest next action in the whole programme is to ask the
mapping pipeline owner. This record cannot settle it; it is an open question, not a finding.

## Stage-attribution correction (supersedes the earlier stage table)

`/mnt/ssd/lalli/pancollapse_profile_tools_v1_20260919T052434Z` (sealed, 10 files) **supersedes**
`/mnt/ssd/lalli/pancollapse_stage_attribution_v1_20260918T192832Z` (sealed, 9 files). The
earlier root is retained, and its stage table is wrong.

Classifying profile rows by substring match over demangled C++ signatures is unsound, because a
demangled signature names every type appearing in its template arguments. Forty symbols contain
the count-worker lambda's needle and only twenty are that lambda's own `operator()`. The error
misfiled `std::_Rb_tree<unsigned int, pair<const unsigned int, {lambda}>>` — a per-read tally
map whose *value type* mentions `ReadNodeTallyCache` — into node-ledger build. Corrected, as a
percentage of worker-attributable CPU, node ledger build falls from 15.72% to **13.13%** and
per-read tally maps rise from 6.91% to **9.40%**; every other stage moves by less than 0.1
point. The corrected node-ledger figure is 10.71% of all CPU, which now equals perf's
independent inclusive figure for `ReadNodeTallyCache::expand` of 10.712% — a consistency check
the superseded number failed. The replacement tool matches anchored **mangled** name prefixes
against a closed, conflict-checked table and fails loudly on unknown symbols, and it makes one
perf pass into an aggregate instead of four.

## Rejected and falsified hypotheses

**In-flight batch depth (rejected).** `/mnt/ssd/lalli/pancollapse_inflight_depth_v1_20260920T181428Z`
(sealed, 61 files) raised `max_inflight_batches` from 8 to 64 to 256. Decoder idle fell only
from 65.1% to 58.6% to 60.8%, which is non-monotone; headroom stayed flat between 3.55x and
3.72x; and throughput was worse than both bracketing controls. Peak buffered bytes saturated at
632, 676, 678 and 608 MB across a 32x increase in slots, which shows admission is byte-gated
rather than slot-gated. The run also established a measurement rule worth carrying forward: two
identical control arms differed by 14.8% in reads/s but by 0.02 points in idle, so **idle
percentage is the trustworthy statistic on this host and raw rate is not**. The trial added an
environment knob `PANCOLLAPSE_INPUT_INFLIGHT_BATCHES` (`src/main.cpp:4977`), inert when unset,
with byte-identical output.

**`max_buffer_bytes` as the admission gate — the working note is contradicted by the current
code and needs re-checking before it is acted on.** The note recorded during the investigation
was that `max_buffer_bytes` appears only to increment `budget_oversize_records` and never
blocks, and that `exceeds_raw_budget` is dead. In the worktree as it now stands, neither holds
as stated:

- The decoder-side member `max_buffer_bytes_` is indeed only compared at
  `src/ordered_gamp_reader.hpp:381-383` to increment `oversized_records`.
- But `options.max_buffer_bytes` is also the producer-side byte budget in `append_record`
  (`src/ordered_gamp_reader.hpp:564`, `:581`, `:585`): exceeding it calls `seal_building()` and
  then drains through `commit_front()` (`:511`), which calls `pool.wait_until_complete()`
  (`:514`). That is a blocking gate, and it is consistent with the byte-gated admission the
  depth sweep observed and with the producer partition below.
- `exceeds_raw_budget` (`src/ordered_gamp_reader.hpp:73`) is not unassigned: it is set
  positionally by the aggregate initializer `building->raw.push_back({std::move(data),
  oversized});` at `:600`, and read at `:381`. A name-based grep misses it.

Separately unexplained, and recorded as a limit rather than reconciled: peak buffered bytes of
632–678 MB sit far above the 64 MiB default `max_buffer_bytes` (`:40`), so the accounting the
budget applies to is not the same quantity as the peak that was measured. Nothing here should be
cleaned up or re-tested until that is resolved.

## Producer partition: the wait is downstream of the producer

`/mnt/ssd/lalli/pancollapse_producer_partition_v1_20260920T220538Z` (sealed, 27 files) added
timing-only instrumentation to the producer thread. Of a 973.1 s read phase, the producer spent
**910.3 s (93.5%) blocked in `wait_until_complete` on the oldest batch**, 53.9 s (5.5%) reading
and framing raw input, and 7.9 s (0.8%) grouping records and submitting jobs, summing to 99.9%.
The hypothesis that grouping is a producer-side bottleneck is dead. Decoder dequeue is FIFO, so
the wait is not an ordering defect either: the producer waits because the oldest batch is not
yet decoded.

## First profile of the expensive regime

`/mnt/ssd/lalli/pancollapse_cliff_profile_v1_20260920T232129Z` (sealed, 21 files) is the first
profile in this programme taken on the expensive regime; every earlier profile sampled the cheap
1M-read prefix. It captured 895,383 samples of `cpu-clock:u` at 99 Hz with no call graph, using
thread names to separate the pools — a deliberate choice, since the DWARF unwinding needed for
call graphs left 18% of the prefix profile unattributable. Threads split `pc-count` 74.05% and
`pc-decode` 25.36%.

Within the decoder pool: glibc allocator **59.3%** (1,423 CPU-s), `SpaceUsedLong()` accounting
**21.7%** (521 CPU-s), protobuf parse 10.1% (242 CPU-s), constructors and destructors 5.9%, and
lock/futex 1.1%. Within the count pool: other work, meaning the exact DP, tally and resolve
paths, 61.5%; allocator **30.7%** (2,146 CPU-s); parse 2.8%; constructors and destructors 2.3%;
`memcpy` 2.1%.

Allocation therefore totals 3,569 of 9,382 CPU-s, **38% of all CPU in the expensive regime**,
against the 17.13% measured on the prefix. That comparison is between two different regimes of
the same workload rather than between two binaries, and it is the clearest single statement of
why prefix-measured optimization estimates understate what is available here. Lock contention is
dead as a hypothesis at 1.1% in the decoder pool and 0.6% in the count pool.

## Accepted fix: sample `SpaceUsedLong()` instead of calling it per record

`SpaceUsedLong()` walks the whole nested protobuf object graph, costing as much as parsing on a
dense alignment, and it feeds only diagnostics. It is now called once per 4 MiB of serialized
bytes per decoder thread (`kExactSpaceSampleIntervalBytes`,
`src/ordered_gamp_reader.hpp:47-48`, applied at `:355-366`), and the remaining records are
scaled by serialized size. The first iteration
(`/mnt/ssd/lalli/pancollapse_spaceused_fix_v1_20260921T002651Z`, sealed, 26 files) sampled per
batch and recovered 247 CPU-s; because batches seal on bytes, a large record arrives alone in
its own batch and so was always measured exactly, which is precisely the record whose traversal
costs most. The accepted iteration
(`/mnt/ssd/lalli/pancollapse_spaceused_fix_v2_20260921T043810Z`, sealed, 28 files) moved the
budget to a running counter carried across batches and recovered 66 CPU-s more.

On `w_cliff`, decoder CPU fell **2,123.5 to 1,810.9 s (-14.7%)**, the read phase fell **973.1 to
874.6 s**, and throughput rose **102.8 to 114.3 reads/s (+11.3%)**. The v2 arm ran at host load
67.6 against the original arm's roughly 9 and was still the fastest, so the improvement is not a
quiet-host artifact. Validation: all four published tables are byte-identical to the sealed
`w_cliff` arm, non-cache counters are equal, and 77/77 focused tests pass.

Two things this fix did not achieve, recorded beside the win rather than below it. The profile
predicted roughly 461 CPU-s available and 313 were captured, **68%**; the residual of about 148
CPU-s is unexplained and needs a decoder re-profile rather than another guess at the mechanism.
And two diagnostics are now estimates rather than exact: `budget_oversize_records` reads 1,666
where the exact path read 1,859, and `decoded_bytes` is about 9% lower. Neither gates any
behaviour and no published output moved, but neither is an exact measurement any more.

## Where the budget stands now

Post-cliff, per 100,000 reads: decode consumes 1,811 CPU-s across 4 decoder threads, a 452.7 s
floor; counting consumes 6,755 CPU-s across 24 workers, a 281.4 s floor; the read phase takes
874.6 s. Decode is now the long pole at 1.6x counting's floor, with a further 1.93x of
in-order-commit penalty on top of it. `decoder_threads` is hard-capped at 4
(`std::min<size_t>(4, requested_processing_threads / 4)`, `src/main.cpp:4967-4968`), but the
decoders run only about 52% busy, so thread count is not obviously the binding constraint and
raising the cap is not an evidenced next step.

## Next steps — none started, each requiring approval

1. **Ask the mapping pipeline owner about the tail** (the open question above). It is free, and
   it could moot everything below it.
2. **Arena allocation for decoded records.** It targets the 38% allocation share and hits both
   pools, since count workers pay to destroy what decoders build (`_int_free` alone is 19.7% of
   counting). Arena support is already compiled into the vg protobuf (the generated
   `/mnt/ssd/lalli/vg/include/vg/vg.pb.h` carries the arena-aware API). It is **not** a drop-in:
   a cross-arena move deep-copies, so `Group.records` (`src/main.cpp:2010`, moved at `:6334`)
   would have to become pointers alongside a `shared_ptr<Arena>` whose lifetime spans every
   group drawing from that batch. This needs a design and human approval before any code.
3. **The residual roughly 148 CPU-s of `SpaceUsedLong` cost**, which needs a decoder re-profile.
4. **Re-check the `exceeds_raw_budget` and `max_buffer_bytes` notes** against the code as
   recorded above before treating either as a cleanup item.

## Uncommitted source state in this worktree

Three source changes sit uncommitted alongside the pre-existing uncommitted work on branch
`codex/compat-replay-integration-b8210`:

- The **sampled `SpaceUsedLong` fix** in `src/ordered_gamp_reader.hpp` — validated by the v2
  root above, with byte-identical published tables.
- **Producer-partition timers**, also in `src/ordered_gamp_reader.hpp` — instrumentation only.
- The **`PANCOLLAPSE_INPUT_INFLIGHT_BATCHES` environment knob and producer-partition receipt
  fields** in `src/main.cpp` (`:4977`) — instrumentation only, inert when unset.

`src/ordered_gamp_reader.hpp` is untracked in this worktree, so its changes do not appear in a
`git diff` against HEAD; `src/main.cpp` carries these changes inside a larger uncommitted diff
that also contains earlier unrelated work. **None of the three has been committed.**

## Limits on interpretation

This record establishes the existence, location and proximate cause of the cliff, and the
validated effect of one decoder fix on one post-cliff window. It does not establish why the
input changes character, does not license any prefix-versus-full absolute throughput comparison
across binary versions, does not measure the fix on the cheap regime or on the whole input, and
makes no accuracy, parity, whole-pangenome, converter or replay claim. Runtime targets of under
5 s/100k and 16.5 s/100k both remain unmet, and the gap on the full input is roughly fifty times
the fallback target rather than a tuning margin.

## Related records

- `docs/research/native-count-processing-performance-20260915.md` — the phase record this
  continues, including the superseded allocator-substitution frontier.
- `docs/decisions.md` D089 — the decision chain that governs this work.
- `/mnt/ssd/lalli/pancollapse_allocator_substitution_v1_20260918T035718Z` — the prefix-measured
  allocator arms, valid for the prefix regime and not for the post-cliff regime.
