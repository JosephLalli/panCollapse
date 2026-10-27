# Native-count processing performance checkpoint — 2026-09-15

## CURRENT: superseded by the full-input cliff investigation (2026-09-21)

The frontier moved off this record on 2026-09-21. Everything below was measured on the real
first-1M-read prefix, and the prefix is now known to sit entirely inside a cheap regime that
covers 85.0% of reads but only 14.2% of read time on the full input. Prefix-measured optimization
estimates — including the 17.13% glibc allocator share and the roughly 1.2x ceiling implied by it
— therefore understate the expensive regime, where allocation is 38% of all CPU. The prefix
results remain valid *for the prefix regime*; they are not a full-input claim.

The current record is
[`docs/research/full-input-throughput-cliff-20260921.md`](full-input-throughput-cliff-20260921.md),
which carries the full narrative, tables, evidence roots and limits. In brief:

- The full-input run `/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z` is
  **terminal and validated, seal pending** (STATUS `VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`,
  finished 2026-09-18 13:23 UTC per `REPORT_RECEIPT.json`): 25,755,749 reads, 124,261,698 alignments, read phase
  210,645.357663 s = 817.857627 s/100k reads, peak RSS 137.476540 GiB. It ran binary
  `3d13e082…d7d0f958`, not the frozen `6eb3e7ca…d6843a`, so absolute prefix-versus-full
  comparisons are confounded by binary version; within-run behaviour is clean.
- Throughput collapses from 300–2,000 reads/s to about 21 reads/s inside one roughly 12,288-read
  interval at read ~22,046,000. Cold 100,000-read windows reproduce it, excluding accumulated
  state, and post-cliff reads carry **fewer** alignments each (5.41 against 9.99), excluding
  multimapping. The cause is graph structure per alignment: 326x subpaths and 596x mappings.
- **Why the input changes character is not established** and is an open question for the mapping
  pipeline owner; retention into the GAMP shifts at the same point (~69% locally in the tail
  against ~6.6% cumulative early), which points upstream rather than at biology.
- The stage table in `/mnt/ssd/lalli/pancollapse_stage_attribution_v1_20260918T192832Z` is
  **superseded** by `/mnt/ssd/lalli/pancollapse_profile_tools_v1_20260919T052434Z`: substring
  matching over demangled C++ signatures misfiled a tally map into node-ledger build (15.72 →
  13.13; per-read tally maps 6.91 → 9.40).
- One fix is accepted and validated: sampling `SpaceUsedLong()` once per 4 MiB of serialized bytes
  per decoder thread cut decoder CPU 14.7% and raised post-cliff throughput 102.8 → 114.3 reads/s,
  with byte-identical published tables and 77/77 focused tests. It captured 68% of the predicted
  saving; the residual ~148 CPU-s is unexplained.

Terminology note carried into all new text: a "read group" in this codebase's counters
(`input_read_groups`, `completed_read_groups`) is one read's alignments — all GAMP records sharing
one QNAME (`src/main.cpp:6321`, `struct Group` at `:2004`) — and not a SAM/BAM `@RG` read group.

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
per-read `std::map` target below came from the 1M-read prefix; see the CURRENT section above. As
originally written: Reduce the allocation rate at its
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
`frozen/manifest.json`. Source and tests are `src/ordered_gamp_reader.hpp`, `src/main.cpp`,
`tests/vg/ordered_gamp_reader_test.cpp`, and `tests/vg/direct_count_verify.py`; trial commands
are in the authority root's `commands/`.

The historical original full-data `3d13…f958` run below is a separate snapshot and remains
active, untouched. The bounded implementation-and-measurement phase is complete; future tuning
is unexecuted and requires a new checkpoint.

## Real-input native-count measurement (TERMINAL, validated 2026-09-18; seal pending)

**Terminal outcome (2026-09-21 reconciliation).** This run completed 2026-09-18 13:23 UTC
(`REPORT_RECEIPT.json` `created_utc`), every
stage exit 0, STATUS `VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`; the completion seal is still
pending. Terminal figures: 25,755,749 reads, 124,261,698 alignment records, read phase
210,645.357663 s (**817.857627 s/100k reads**, 817.985728 s/100k with finalization), peak RSS
137.476540 GiB; both runtime targets missed. The text below is the historical in-flight record of
the same run, and its live checkpoints are superseded by these figures. See
`docs/research/full-input-throughput-cliff-20260921.md`.

The prior fallback phase is complete and sealed: the 3M authority
`/mnt/ssd/lalli/pancollapse_native_larger_3m_v1_20260915T231536Z` records 75 files
verified with exit 0 in its completion seal, and the measured goal is complete. Its
100k/1M/3M outcomes and limits remain prior evidence below. User authorization starts
a separate bounded measurement phase: run the frozen resident binary on an existing,
genuine biological real-read GAMP and measure native read plus count-finalization per
100k, initialization, diagnostic merge, publication, whole wall time, and peak RSS.

The selected existing input is `site1_donor2_asian`: the sealed genuine 10x ARC GEX
GAMP `/mnt/ssd/lalli/hprc_v2_vg_rna/chr20_compare/readsets/real_scrna_benchmark_v1/pangenome_mapping_reuse_v15r2_20260908T150525-0500/site1_donor2_asian/d10k_cap20k/mapping/reads.gamp`
(127,202,588,396 bytes, 25,755,749 groups, SHA-256
`000858121cdcbbc09364e423586ca57324e19c8ef0ad968a3308c91b7e7e87fe`). Its matching
chr20--22 XG, GCSA, distance index, and ledger are under
`/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500`.
The ledger hash is `73c7a7a03abcbb9eb795c3ac2b1141cca82333c73134b190ee1a30be8657f10c`,
matching corrected-bundle content `sha256:f1ffba4d4fe821e6bda6d0b88bba3cfe5df9df5fb263efe099e27fe14146c99e`.

The input has 6,211 eligible cells. Its 25,755,749 pre-mpmap groups combine A=chr20--22,
B=unmapped, and C=elsewhere MAPQ<10 (D excluded); it is neither whole-pangenome nor
chromosome-only throughput. Names embed raw GEX CR, CB16/UMI12. The mechanically pinned
ARC GEX whitelist is `inputs/737K-arc-v1.gex.txt` in
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z` (736,320 barcodes,
SHA-256 `fde6ac9705089a87cb078993500af6bcac89176953fac46f2173c6dd1b613108`); its
official chemistry provenance and PASS preflight are adjacent. It was not used by the
original frozen producer.

Parent preflight passed (`provenance/preflight.json`; `logs/preflight.exit.tsv=0`; available
bytes 414005080064). The run was launched `COUNT_RUNNING` (it is now terminal; see the
section header) in
`pancollapse-real-native-20260916T022706Z` (ID
`e465ff51a2d64dda303c0ad61c45725ce65a2a34a8c2ab9e3569599ddd8d429b`), started
2026-09-16T02:41:19.314463216Z. Monitor `logs/count.stderr`, `logs/count.time`, and
`logs/count.host.samples` under `/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z`.
`logs/count.exit.tsv`, `logs/coordinator.exit.tsv`, `METRICS.json`, and terminal validation remain
pending; output is `counts/`; it runs 32 workers, CR7+strict, a 64 GiB count aggregation budget,
and a 256 GiB no-swap Docker limit, with diagnostics off and fine timing unset; do not duplicate
or restart. The original mpmap completed but
its post-mapping summarizer failed on QNAME ordering, so no terminal mapping-rate claim exists.
At 2026-09-16T05:58:12Z, the latest recorded live checkpoint reported 7,290,880 completed groups in
11,292.7 s (645.625 groups/s; 154.88802 s/100k cumulative read-only partial). The 16.5 s/100k
full-input target is already missed: its entire read-plus-finalize budget is 4,249.698585 s,
below the read time already elapsed. This is not a terminal benchmark or a change to the prior
synthetic-fixture result. RSS was 100,498,312 KiB (95.84266 GiB), not terminal peak.
Initialization/finalization are excluded; throughput is not
stable and neither value estimates whole-run wall time. The follow-up-only worker snapshot
`logs/worker_snapshot.20260916T0321Z.txt` found most workers futex-waiting and a few running.
Shard-cache builders use a unique lock, while aggregation-lock and queue paths can wait; this
does not establish an actual cause or deadlock.
No mapping, converter, replay, annotation, or biological-policy change is authorized, and
real-data time/RSS does not establish truth-based accuracy. The frozen executable is
`/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z/provenance/panCollapse.resident-finalize`
(SHA-256 `3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`); the
worktree build is older. HEAD `258f22e` remains 23 ahead/1 behind main `d4a60ae`.

Within the same measurement phase, the run root's `commands/report_result.py` and
`commands/report_after_count.sh` now prepare the terminal report automatically after successful
count and coordinator receipts. Syntax checks passed; separate container
`pancollapse-real-report-20260916T022706Z` has a 512 MiB no-swap limit and waits without reading
the GAMP. It writes `RESULTS.md`, `REPORT_RECEIPT.json`, and `logs/report.exit.tsv`; a failed or
interrupted count is never promoted. Documentation reconciliation and sealing remain pending
after report creation. `provenance/source_selection/` preserves existing upstream A/B/C totals:
7,417,531 / 4,964,736 / 13,373,482 reads (28.80% / 19.28% / 51.92%). These pre-pangenome-mapping
categories describe input selection, not native assignment or accuracy.

The current gate remains the later machine-verifiable increment after Vertical Slice
Proven. Next action: monitor this single count, then perform terminal validation and report its
measured results; no further confirmation is required.

### Authorized bounded cache trial (documentation checkpoint, 2026-09-16)

The user has newly authorized one bounded cache experiment. It supersedes the prior
no-new-experiment boundary only for this trial. Required evidence is real-input runtime
and peak RAM plus same-authority output-equivalence validation. It authorizes no F1,
mapping, converter, replay, annotation, or biological-policy claim or change.

The original full real count remains active and must be preserved, not stopped,
restarted, or replaced. Its latest recorded checkpoint is **22,159,360 groups in
35,443.3 s**. Preserve its output root and all previous outputs and receipts.

The clarified trial is **per-read node-to-tally caching/aggregation**: obtain a node's
tally metadata only once per read, so repeated node occurrences do not repeat metadata-cache
lookups or member-reference-path hash updates. Current tally is a
`flat_hash_map<raw path handle, HstTally>` that records a path-to-Parent maximum using
exon/body evidence followed by transcript resolution. Equivalence must retain numerical scores,
orientation, duplicate path steps, and the raw-path-to-Parent maximum; exact matching is
separate. Existing identity/order/filtering, score-window/final comparator, cross-record
aggregation, structural tally, fallback inputs, and bounded worker-local memory remain scope
for equivalence evidence.

The prior assignment-cache-capacity interpretation was wrong. Preserve
`/mnt/ssd/lalli/pancollapse_cache_trial_v1_20260916T124251Z`: it holds a first-1M-complete-group
subset extracted with the official VG C++ reader/writer. Its frozen-native pair exited 143 after
intentional SIGTERM during initialization in `pancollapse-cache-pair-20260916`; no wrong
candidate count launched. The 8x-capacity candidate passed 74/74 tests, but its build wrapper
exited 129 at Git-provenance collection; preserve its binary and receipt. The temporary capacity
edit and test assertion were reverted before corrected implementation, restoring `src/main.cpp` to SHA-256
`dc74cccad9de07e92bc6c77870465dfb11d0158f88692ff5a7e0bacfa79ca6bd`.

The corrected trial root is `/mnt/ssd/lalli/pancollapse_node_tally_cache_v1_20260916T125145Z`
with `README.md` and `commands/count_one.sh`. Frozen 1M baseline container
`pancollapse-node-cache-baseline-20260916` ran using original binary `3d13…f958` and
the same inputs, profiles, and settings. The preserved input is
`/mnt/ssd/lalli/pancollapse_cache_trial_v1_20260916T124251Z/inputs/reads.first1m.gamp`
(SHA-256 `39d4238d9c55d18d114b554665a6d3b5ad8ba97a418a11ae122b6afff2e0aa8c`; 1,000,000
groups and 2,621,222 records).

The corrected candidate is implemented in `src/node_tally_cache.hpp` and wired through
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

The matched trial is terminal PASS at
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

## Input/dispatch discussion checkpoint (read-only, 2026-09-16)

This session completed a **read-only design/explanation** of parallel native GAMP input and dispatch.
An implementation must retain contiguous read-group formation, validation and error behavior, observable input
order, bounded memory, and the existing core budget. No production code was changed and no new
benchmark was run; implementation remains unexecuted.

The installed VG `MessageIterator` supports multithreaded BGZF decompression, and
`for_each_parallel` performs protobuf parsing and callbacks in OpenMP tasks. Those callbacks are
not ordered, so existing adjacent-record GAMP grouping is not a drop-in callback workload.

The original baseline's 60-second worker sample is recorded at
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/worker-utilization-v1/`.
Its 32 workers averaged 3.955 cores (12.3603% of combined capacity), 87.65% of sampled worker
states reported `futex`, and the main thread used 92.65% of one core. The main thread currently
decodes native GAMP, forms and validates groups, parses molecules, hashes input, and dispatches
completed groups. That makes producer delivery plausible as a limit, but the sample cannot
separate empty-job-queue waits from cache or aggregation contention. The sealed node-tally-cache
candidate (`e189…4dba`) observed 55.226 versus 63.018 s/100k on one shared-host pair at 72.110
GiB peak RSS; its utilization was not measured, so it supplies no evidence for parallel-input
throughput. The current gate remains the later machine-verifiable increment after Vertical Slice
Proven.

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

The user has authorized implementation and bounded validation of the reviewed ordered parallel
native-GAMP reader. The current phase is now **authorized implementation plus bounded
validation**, replacing the preceding discussion-only boundary. The original frozen full real
count remains untouched and cannot be stopped, restarted, replaced, or used as the benchmark.
No Parent or biological policy, replay, mapping, or converter change is authorized.

Implement an adapter around VG `MessageIterator` threaded BGZF input: serialize batches bounded
by bytes and logical record count, decode them through a small pool, then deliver records in
input order to the existing adjacent-record grouping callback. Preserve groups spanning batches,
file and stdin behavior, validation behavior, and observable error order, including corrupt
input, wrong tag, large group, and a slow oldest batch. Bound added memory and account for the
32-CPU limit across BGZF work, decoders, and existing count workers.

The sealed comparison baseline is
`/mnt/ssd/lalli/pancollapse_node_tally_cache_v1_20260916T125145Z`; its candidate binary SHA-256 is
`e189ba54c69d4a33c73364313e860a437585f93e0519c4fb7978bb2444ab4dba`. Later matched validation
uses the preserved first-1M GAMP
`/mnt/ssd/lalli/pancollapse_cache_trial_v1_20260916T124251Z/inputs/reads.first1m.gamp`, SHA-256
`39d4238d9c55d18d114b554665a6d3b5ad8ba97a418a11ae122b6afff2e0aa8c` (1,000,000 groups and
2,621,222 records). Acceptance requires byte/logical-count agreement, grouping across batches,
file/stdin and stated error-order checks, CPU-budget accounting, bounded added memory, and a
matched real-1M time/RSS/worker-utilization observation. No existing speed result is evidence
for this adapter.

### Ordered parallel native-input trial (complete bounded implementation and measurement, 2026-09-16)

The trial authority is `/mnt/ssd/lalli/pancollapse_parallel_input_v1_20260916T164124Z/`.
`RESULTS.md`, `METRICS.json`, `UTILIZATION.json`, `OUTPUT_COMPARISON.json`, and coordinator exit
0 are complete; the first report failure is retained in `logs/report-attempt-1/` and
`report_only.sh` resumed reporting without a count rerun. The frozen baseline is
`e189…4dba`; the candidate is `06e189…67f84`; both used the preserved real first-1M GAMP.

`src/ordered_gamp_reader.hpp` and `src/main.cpp` integration are written. Native runs with at
least eight threads select ordered input; converter and sub-eight-thread behavior are unchanged.
At 32 threads, the budget is 24 count workers, four decoders, two decompression threads, one
BGZF I/O thread, and the main thread. The 64 MiB buffer is an admission watermark; protobuf
expansion and in-flight buffers can raise resident use above it.

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

Supplemental sanitizer evidence is unavailable: `sanitizer-test/RECEIPT.json` records Abseil
constexpr compilation and ASan runtime failures before the oracle, so it is neither a candidate
defect nor a sanitizer pass. Completion integrity is recorded in `FINAL_AUDIT.json`,
`COMPLETION.SHA256SUMS`, and `logs/final_audit.exit.tsv`. The next unexecuted work is read-stage attribution of
remaining cost; it requires the next phase's documentation checkpoint and no further experiment
has launched.

Keep frozen resident executable SHA-256
`3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`, dirty integration
HEAD `258f22e889b94129024ebe8cdf0c295dfd0d12a2`, and main
`d4a60aed001bc03b00baf7ed54e39176d2266b55` alongside the structural sample record. The original
full real count remains active and untouched, last recorded at 22,216,704 groups in 38,209.5 s;
do not restart it.

### Interim explanation of the fixture-versus-real throughput gap

See the active run's `provenance/FIXTURE_COMPARISON_INTERIM.md` for exact sources
and code boundaries. At approximately 06:11 UTC the latest real checkpoint was
7,880,704 groups in 11,900.4 s (about 151.0 s/100k read-only partial), compared with
9.907194 s/100k read plus finalization for the completed simulated 3M fixture.
The frozen binary and main graph/profile settings match; the input workloads differ.

The 3M fixture had 96.5469% assignment-cache hits, with all 103,593 distinct results
retained. Real cache counters remain pending. Its full GAMP averages 41.596 times
more encoded bytes/group, but its processed prefix differs from that average; neither
number establishes processing cost or an I/O bottleneck. Upstream low-MAPQ/unmapped
selection describes input composition, not measured native alignment complexity.

Assignment-cache misses materialize candidates and resolve assignments under a shard
lock; full shards compute new signatures without retention. Hits do not avoid all
evidence extraction. This behavior and the observed worker waits motivate later
profiling, but are not a causal attribution. `uncached` is a subset of `misses`:
cache hit share is `hits / (hits + misses)`. No new profiling or implementation phase
has started; continue the existing count and terminal reporting.

### Real-versus-simulated workload attribution checkpoint (read-only, 2026-09-16)

The user's challenge to describe concrete reasons beyond “different data” is valid:
the current record contains leads, not a measured decomposition. A later nonterminal
observation of the same active count is approximately 9.81 million groups in 14,029 s
with diagnostics off. It confirms the performance gap, but neither the 41.596-fold
whole-file bytes/group comparison, upstream A/B/C selection, the fixture's 96.5469%
cache-hit rate, nor futex waits attributes any portion of it to I/O, cache misses,
candidate work, resolution, tally, or contention. The frozen binary, graph, corrected
bundle, profiles, worker count, and aggregation budget remain pinned; executable SHA-256
is `3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`.

**Authorized bounded next task:** without waiting for, modifying, stopping, or
restarting the active full count, inspect small existing representative samples from
the sealed real GAMP and completed simulated 3M fixture and trace the frozen source's
per-read processing path: grouped GAMP decode, evidence extraction, assignment-cache
lookup/build, candidate materialization/resolution, and tally/aggregation. Report
observable sample features and which implementation work can consume them. A bounded
non-invasive attribution observation is permitted only if this inspection supports a
specific measurable question. No native-count rerun, new production profiler,
production-code change, converter/replay/policy change, or biological conclusion is
authorized. Preserve all active-run and completed-fixture evidence.

### Prefix-sample structural attribution (complete read-only observation, 2026-09-16)

The resulting sampler exited 0. Its artifacts are
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/attribution/`:
`sample_structure.py`, `fixture.summary.json`, `real.summary.json`, per-group TSVs,
and `concentration.json`. It used existing `vg view -Kj` to decode the **first**
20,000 complete contiguous QNAME groups of each GAMP without invoking native count.
This is a prefix sample, not a random or whole-run-representative sample.

All 20,000 fixture groups aligned; the real prefix has 7,534 aligned and 12,466
unaligned groups. Including every group, real/fixture totals are 2,494,196/130,161
node-mapping visits (19.16x mean visits/group) and 508,359/68,997 subpaths (7.37x).
For aligned groups only, mean node-mapping visits are 331.059/6.508 (50.87x). The
real prefix's top 1% of groups hold 69.05% of mapping visits, compared with 22.20%
for the fixture. The most extreme real group is 90 bp with six records, 42,311
subpaths, 298,198 mapping visits, and 1,338 distinct nodes; the fixture maximum is
2,398 mapping visits.

These are measured structural reasons the real prefix creates more upstream work
in the current implementation: `src/main.cpp:4651-4676` visits each mapping and then
each matching reference path in the numeric tally, and `src/main.cpp:4398-4592`
processes subpath states, mappings, and links in exact DP. Assignment-cache reuse is
downstream and cannot remove those loops. This supersedes the generic “harder data”
explanation. It does **not** attribute a fraction of full wall time, prove the real
cache has poorer reuse, or assign a wait cause; the active full count, diagnostics
setting, terminal evidence, and all prior results remain unchanged. No CPU profiling
or production edit was performed. A targeted reread of extreme group 7,501 confirmed
the six records share one repetitive 90-base sequence; all 42,311 subpaths are
reachable, and one node appears 2,235 times in the first record. See
`real.extreme_group.json`. `validation.json` is PASS for row sums, means, top-1%
concentration, complete-group boundaries, and extreme-record totals. The concise
authority is `attribution/RESULTS.md` in the active run root. The small inspection
containers shared the host with the count; their decoding times are not native
throughput measurements.

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

Read processing remains 96.04–96.78% of combined time. The paired resident 1M receipts
place evidence (53.76–56.57%), resolution (17.67–18.98%), and tally (14.63–15.11%)
at the largest labeled worker-wall shares; these are neither CPU nor additive
whole-command times, and queue wait denotes queue residence. The recovered matching
3M GAMP is now the authorized single scaling point. No converter, replay,
annotation-policy, or accuracy/F1 claim changed.

## Historical goal and phase checkpoints

The completed bounded validation targeted a dramatic reduction in native `panCollapse count`'s
**non-initialization per-read processing** runtime. The intended eventual workload is a whole
pangenome run with at least 20 million reads. The current one-million-read chr20--22 fixture is
an attribution and determinism fixture only: it cannot establish a whole-pangenome peak-RAM
claim.

## Historical optimization phase record (terminal receipt above)

The completed validation below is past evidence. A new native-count evidence-optimization phase
is **BENCHMARK_IN_PROGRESS**. The bounded implementation changes
`src/main.cpp::exact_ex50_evidence_for` to use `src/score_dedup.hpp`'s alignment-scoped,
contiguous open-address scratch for in-place stable maximum-score state deduplication; all existing
`StateKey` fields and filter/window order remain in `src/main.cpp`. `src/direct_count_runtime.cpp`
adds optional finalization profiling. The implementation aims to reduce temporary allocation and
deduplication cost while preserving comparator-equivalent evidence identity, maximum-score
retention, Parent allowlist filtering, final comparator/score-window order, and aggregation over
records. Structural tally and its fallback assignment inputs remain required. Reused state must be
worker-local and bounded, and RAM must be measured.

Report `read_processing` and `count_finalize` separately, plus their combined native-count
processing time, and investigate count-finalization scaling: the completed 100k debug measurement
was 0.484642656 s finalization while the completed 1M production measurement was 17.857453257 s.
Do not extrapolate between them. The phase finishes with combined native counting under 5 s/100k,
excluding initialization, or evidence-backed combined performance of about 16.5 s/100k after
optimization. Work cannot be shifted into finalization to satisfy that target; debug merge/export
remains separately reported. Same-authority biological determinism and RAM validation remain
required; cross-authority strict parity is not required. The approximately 52.1 GiB 1M chr20--22
peak RSS is already high and does not support a whole-pangenome claim. The later applicable
acceptance gate is machine-verifiable behavior after Vertical Slice Proven. The results workspace
`/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z` is
`BENCHMARK_IN_PROGRESS`. `tests/policy/score_dedup_smoke.cpp` passes 2,000 seeded collision,
order, reuse, and split-record cases against its map oracle, and `logs/focused-tests.log` records
74/74 focused tests PASS. Candidate binary `provenance/panCollapse.exact-dedup` SHA-256 is
`e7434ac59a2fb3c5acf688ba9ca1a28827ceac14f49b2f5c78109561004809ae`; its source snapshot is
`provenance/source.exact-dedup.tar.gz`, while the worktree build is untouched. Both 100k arms are terminal exit-0 PASS for exact
biology and input/execution identity. Baseline measured 18.784944554 s read processing,
0.542118419 s count finalization, and 19.327062973 s combined; candidate measured 21.794378933 s,
0.929468246 s, and 22.723847179 s, respectively. Candidate is 17.6% slower in this initial pair.
Preserve this no-speedup result; it does not promote the candidate or complete the goal. See
`RESULTS.100k.md` in the results workspace. Sol review
is ACCEPT in `provenance/REVIEW.md`; its helper
map-oracle ASan/UBSan check passed, but it is not full native-runtime sanitizer coverage. The 1M
candidate is terminal PASS for all four biological tables, summary, non-cache counters, and
input/execution identity: 567.421721609 s initialization, 119.747233383 s read processing,
13.392732078 s finalization, 133.139965461 s combined (13.3139965461 s/100k), 1.48518 s
publication, 730.67 s wall, and 51.9642486572 GiB RSS. The first 1M pair is complete: exact-
correct baseline measured 592.363492914 s initialization, 141.787332656 s read, 13.141190729 s
finalization, 154.928523385 s combined (15.4928523385 s/100k), 1.37301 s publication, 785.16 s
wall, and 51.9636306763 GiB RSS. The original candidate is about 14.1% lower combined in this
pair only; the 100k candidate remains 17.6% slower. No established repeated speedup or goal
completion exists.

`provenance/FINALIZATION_INVESTIGATION.md` establishes that completed 1M diagnostics alone cannot
exactly reconstruct `CountRuntime` inputs; no harness or parser change was made. Existing
`--compatibility-in` can measure finalization without graph initialization, but that is separate
compatibility-replay performance and was not run. The candidate's 100k UMI-collapse time is
0.0459925 s, so a quadratic UMI cause is not presumed. Docker
`pancollapse-exact-1m-pair-20260915T205001Z`, container
`7e296abb2d881b0c11fd90ab163caccf84a612dfcafbbc668d91fbf9ca430e3b`, is the terminal original
candidate/baseline pair; preserve its receipts. The resident-finalization change is implemented
in `src/direct_count_runtime.cpp`: checked one-time reserve per resident vector and no redundant
first exact sort; final exact sort/combine, deferred sort, and spill behavior remain unchanged.
`logs/focused-tests.resident.log` is 74/74 PASS. Frozen binary
`provenance/panCollapse.resident-finalize` SHA-256 is
`3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`; source snapshot and complete
checksums are retained. Build/test succeeded, but wrapper exit 129 during Docker Git metadata
collection is preserved; host metadata recovery passed. First resident 1M is terminal PASS for all
four tables, summary, non-cache counters, and identity: 113.148701764 s read, 3.769589644 s
finalization, 116.918291408 s combined (11.6918291408 s/100k), 1.42122 s publication, 768.52 s
wall, and 51.9621620178 GiB RSS. Resident prepare is 0.515448 s versus 10.2197 s in the original
candidate; correction is 1.30683 s, molecule stage 1.89924 s, UMI collapse 0.69825 s. This one
observation numerically meets the fallback, but does not establish reliable gain or completion.
First resident 100k is also terminal exact PASS: 20.449478765 s read, 0.664120164 s finalization,
21.113598929 s combined, and 48.4330329895 GiB RSS, slower than its 19.327062973 s baseline and
not meeting fallback at 100k. Resident 100k-r2 is terminal exact PASS at 21.146822184 s read,
0.595278286 s finalization, and 21.74210047 s combined, with 48.4485282898 GiB RSS. The two
resident 100k results span 21.113598929--21.74210047 s (median 21.4278496995), above standalone
fallback. Baseline 100k-r2 is terminal exact PASS at 21.384829 s combined; the baseline 100k
median is 20.355946 s versus resident 21.427850 s (5.3% higher observed). Resident 1M-r2 is terminal exact PASS: 96.659688 s
read, 3.986118 s finalization, 100.645807 s combined (10.064581 s/100k), 1.29438 s publication,
640.02 s wall, and 51.885620 GiB RSS, with input identity PASS. The two 1M values span
10.064581--11.691829 s/100k (median 10.878205); the two 100k values remain
21.113599--21.742100 s (median 21.427850). All same-authority four tables, summary bytes, and
non-cache counters are exact. This is not a causal speedup or full-scale claim. Baseline 1M-r2
is terminal exact PASS at 145.251348 s combined and 52.036316 GiB RSS. Baseline 1M median
is 15.008994 s/100k versus resident 10.878205 (27.5% lower observed); standalone 100k remains
5.3% higher. Six of seven scheduled arms pass; only complete debug 100k remains live in Docker
`pancollapse-resident-repeats-20260915T205001Z`, container
`7aebee643a0e5058cf66ce28de73a8070570f9bf09a4503a634a045930181507`; debug 100k began
22:53:56Z. Poll it, do not duplicate it. `ACCEPTANCE.md` audit remains pending; its
`REPEATS_REPORT.md` separates PASS/PENDING/FAILED and whole-command host load. Baseline CPU
sampling ended before counting, so CPU window comparison is unavailable; host-load samples remain.
`provenance/LARGER_FIXTURE_AVAILABILITY.md` finds only a mismatched old 1.998M chr20 asset (graph
`1f49...`, ledger `6e40...`) against current graph `1568...`/ledger `73c7...`; bounded search found
no ready matching authority above 1M, not global absence. No whole-pangenome claim follows.
`provenance/REMAINING_PROCESSING_COSTS.md` records first-1M summed-worker-wall attribution:
evidence 53.76%, resolution 18.98%, and tally 15.11%; historical `worker_queue_wait` is job-queue
residence, not worker idle.
`provenance/PERFORMANCE_ACCEPTANCE_SCOPE.md` resolves the acceptance wording: the original 1M
authority is 18.011 s/100k, and normalized 1M throughput may satisfy the fallback only after
remaining repeats, exact outputs, RSS, and complete debug evidence. The failed 100k observation
stays adjacent; no blanket <=16.5 s/100k claim follows. Larger-input validation is conditional on
suitable existing inputs, with no new mapping or mandatory 20M gate; scaling is unproven.

The prior corrected-bundle/annotation production result is terminal for its stated accuracy and
route-diagnostic questions. The user removed strict annotation-versus-bundle parity as an
acceptance requirement. That does not permit any performance change to alter the output semantics
of a run using the same fact authority. Converter performance, compatibility-replay performance,
and counting-accuracy work remain separate.

## Pinned source, executable, inputs, and observations

- Source worktree: `/mnt/ssd/lalli/panCollapse/.codex/worktrees/compat-replay-integration-b8210`,
  dirty HEAD `258f22e889b94129024ebe8cdf0c295dfd0d12a2`.
- Pinned baseline executable: SHA-256
  `251bfa0c948735fdf0ca33c346ac2ca63684679d6ccf13241d90792d0eaab763`, copied into the recovered
  result root below from this worktree. Do not overwrite this baseline with a future build.
- Current native-count code: `src/direct_count_facts.cpp`, `src/direct_count_assignment.cpp`,
  `src/direct_count_output.cpp`, and their headers; CLI wiring is `src/main.cpp`.
- Recovered result root and receipts:
  `/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z`, especially
  `logs/annotation.time`, `logs/bundle.time`, and the matching native-count stderr logs. Its
  `INPUTS.SHA256SUMS` pins the GAMP, XG, whitelist, authorities, and other run inputs.

Both same-binary 1M invocations used 32 requested threads and a 64 GiB count budget:

| Fact authority | Initialization | Processing | Wall | Peak RSS |
| --- | ---: | ---: | ---: | ---: |
| Annotation | 577.057 s | 1,044.36 s | 1,658.51 s | 52.114975 GiB |
| Bundle | 551.372 s | 970.332 s | 1,549.16 s | 52.041714 GiB |

The run recorded whole-command CPU of 524% (annotation) and 541% (bundle). Those values do not
isolate processing parallelism. The two observations ran on a shared host and do not establish a
controlled authority or speed comparison. Their roughly 52 GiB peak is high-end acceptable for
the current run; no whole-pangenome RAM conclusion follows.

## Existing performance evidence

D086 removed measured splice-geometry initialization cost without changing computed values.
D087 reduced identified memory costs while retaining byte-identical outputs. The recovered
resumption record separately notes an unconfirmed 1.46x worker-CPU improvement from streaming an
assignment signature; it lacks an interleaved A/B and is not an established speedup. None of that
evidence attributes the present 970--1,044 s per-read processing phase.

## Authorized bounded continuation

1. Attribute the processing bottleneck from the existing receipts and current code, then obtain a
   bounded profile that separates processing from initialization and output.
2. Optimize only a cost demonstrated by that attribution.
3. Validate identical canonical/logical outputs for repeat runs using the same fact authority,
   plus RAM behavior. Retain operational timing/thread fields as operational evidence where the
   established contract allows them to differ.

The user prefers under 5 s processing per 100k reads, with 16.5 s per 100k an acceptable plausible
fallback. Linear same-fixture estimates are 16m40 and 55m per 20M reads, respectively; they
exclude initialization and final output and are not whole-pangenome predictions. Neither value is
a mandatory acceptance gate. No experiment, build, or large input inspection was performed during
this documentation reconciliation.

## Live 100k attribution (2026-09-15)

The bundle-authority baseline completed with exit 0 at
`/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z/baseline-100k`. Its GAMP is the
systematic every-tenth-ordinal sample across the complete 1M input: 100,000 read groups and
106,671 records, with JSON round-trip input SHA-256
`eadee0b7d0c00735ca043617ea831d1d8d1402941198e502f63b87bea2214f98`. The baseline binary remains
preserved. `logs/baseline-100k.stderr` records 461.991 s initialization and 124.485 s processing
at 32 requested workers.

The profile attributes worker computation before output to 480.908 worker-seconds. Its main
measured regions are evidence 173.835 s, facts 139.517 s, signature 81.3797 s, and tally 71.2295
s. Optional `--read-assignments-out` creates the larger utilization constraint: ordered-output
wait totals 3,420.45 worker-seconds, producer admission wait is 121.52 of 123.475 wall seconds,
and producer active decoding is 1.95515 s. `worker_ordered_region_sum_seconds` is only 42.8633 s.

Do not interpret `observe_seconds=3422.09` or `region_sum_seconds=3944.21` as CPU work: source
`acquire_output` does not adjust the region clock while waiting for ordered output. The resulting
3,422.09 s excess over `compute_total_seconds=480.908` includes ordered wait. Therefore the data
does not establish `observe` as a computational bottleneck, and producer decode is not dominant.

The user accepts slower explicit debug diagnostics and prioritizes actual-count mode. The terminal
production baseline at
`/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z/baseline-production-100k` disables
`--read-assignments-out` and exits 0. `logs/baseline-production-100k.stderr` records 425.592 s
initialization, 16.523 s processing, 503.273 worker-seconds compute, zero ordered wait, 1.97841 s
producer active decode, and 6,052.17 groups/s. GNU time records 7:44.92 wall and 50,742,376 KiB
(48.391701 GiB) maximum RSS.

`logs/baseline-modes.validation.json` is populated and `PASS`: biological counters and barcodes,
counts, features, molecules, and summary are identical to the debug baseline; only the optional
read-assignment sidecar is unpaired. The 124.485 s debug versus 16.523 s no-diagnostics processing
difference is 7.53x. It measures diagnostics mode, not cache code. The compact-cache
production-100k result is recorded below. Diagnostics remain available for debug use.

## Compact-cache production checkpoint

`/mnt/ssd/lalli/pancollapse_processing_perf_v1_20260915T193226Z/input-cache-production-100k`
completed with exit 0. Its receipt records 422.692 s initialization, 14.7644 s processing, and
7:46.84 wall; GNU time reports 50,721,736 KiB (48.372017 GiB) maximum RSS. The matched baseline
processing value is 16.523 s. This is a same-fixture runtime observation, not an accuracy/F1
result or a whole-pangenome prediction.

`logs/production-pair.validation.v2.json` is `PASS`: barcodes, counts, features, and molecules
are exactly equal and all non-cache biological counters agree. `summary.tsv` differs only in the
four named operational cache counters (`assignment_cache_entries`, `assignment_cache_hits`,
`assignment_cache_misses`, `assignment_cache_uncached`). The original `compare_outputs.py`
failed because those same counters are duplicated in the global counters and summary; retain the
failed script and log as evidence. The v2 comparator makes that operational exception explicit.

The input-cache review is **ACCEPTED**. The initial 64/65 focused-test result is retained: its
sole new fixture failed because Hamming-neighbor synthetic UMIs deduplicated. Replacing them with
>=2-distance UMIs yields 65/65 focused tests for candidate SHA-256
`8ca5c2c1a35d6bef9dae53c0d46f019ccd983873d32bf01f55eb9fa108b504d4`. The baseline binary and
existing output/source files remain untouched.

Final diagnostic-sidecar order is an output contract; count semantics do not depend on it. Sol
accepts worker-private diagnostic streams plus a cap-32 bounded final merge preserving exact order
and profile coverage as the next bounded debug-performance improvement. Review is accepted and
`logs/focused-tests.merge.log` passes 65/65 focused tests. The provenance binary is
`provenance/panCollapse.worker-spools` SHA-256
`e48c7ddefebda46998b24e56b22473738cb1c8e185ba3df440e1de825182ced2`; its source snapshot is
`provenance/source.worker-spools.tar.gz`.
Timing boundaries are planned or implemented as `read_processing`, `count_finalize`, `diagnostics_merge_export`, and
`compatibility_finalize` when capture applies; existing aggregation is unchanged. The verified
implementation result and remaining 1M validation are recorded below.

## Verified worker-spool debug checkpoint and final 1M validation

`logs/worker-spools-debug.validation.json` is `PASS`: all same-authority biological outputs and
complete diagnostic Parquet bytes are exact; summary differences are only operational cache
counters. The debug run records 453.462356205 s initialization, 17.281165981 s read processing,
0.484642656 s count finalization, 0.630234947 s diagnostics merge/export, and 18.396179319 s total
processing. Whole command is 501.31 s with 48.453689575 GiB peak RSS. Against the old ordered
debug processing time 124.48512534 s, this is 6.77x; diagnostic-only ordered wait is zero. This
does not meet the preferred <5 s/100k target and does not establish an accuracy/F1 change.

Exact Parent and scientific policy are unchanged. The production path continues to block RAD
capture as before. The final diagnostics-disabled 1M run is terminal:
`logs/production-1m.validation.json` and `logs/production-1m.inputs.validation.json` are `PASS`.
Against `/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z/native-bundle`,
all four biological tables are byte-identical and all non-cache counters agree; diagnostics are
explicitly unpaired. It used the unchanged original GAMP, fixed corrected bundle authority, both
profiles, 32 threads, and 64 GiB. It records 162.249680264 s read processing (16.225 s/100k),
17.857453257 s count finalization, 180.107408508 s total processing (18.011 s/100k), zero
diagnostic merge/export, 603.204376707 s initialization, 842.62 s wall, and 52.07076644897 GiB
peak RSS. This validates output semantics and does not change accuracy or F1.

The validated binary is `provenance/panCollapse.worker-spools`, SHA-256
`e48c7ddefebda46998b24e56b22473738cb1c8e185ba3df440e1de825182ced2`; the worktree's original
build remains the preserved baseline. Topology remains dirty HEAD
`258f22e889b94129024ebe8cdf0c295dfd0d12a2`, 23 ahead / 1 behind main
`d4a60aed001bc03b00baf7ed54e39176d2266b55`. Status is
`COMPLETE_BOUNDED_PERFORMANCE_VALIDATION`. This is prior completed evidence; the current
optimization phase is in progress. The preferred <5 s/100k target remains open.

Historical target (partly superseded): temporary allocation and deduplication in
`src/main.cpp::exact_ex50_evidence_for`, as scoped in `provenance/NEXT_BOTTLENECK.md`. The
deduplication half was implemented and measured in the fused exact-DP state trial above and
rejected for performance, with only 0.479% duplicate emissions avoided. The allocation half is not
addressed by that result; the symbol-resolved re-read at the top of this record attributes 17.13%
of user CPU to glibc allocator machinery, and the prepared Allocator Substitution measurement tests
how much of it a load-time allocator substitution recovers without any source change. Structural
tally cannot be skipped: it supplies fallback assignment. Any follow-up must preserve evidence
identity, comparator/score-window order, Parent filtering, cross-record aggregation, and bounded
worker-local memory; it must measure RAM. Count finalization rose from 0.484642656 s at 100k to
17.857453257 s at 1M, so do not linearly promise a 20M finalization/runtime.

## Related records

- `docs/research/compat-replay-resumption-20260914.md` — recovered inputs, executable identity,
  and boundaries.
- `docs/research/parent-policy-accuracy-1m-20260915.md` — terminal production accuracy,
  native-count timing/RSS receipts, and exact inputs.
- `docs/research/full-input-throughput-cliff-20260921.md` — the full-input throughput cliff, the
  corrected stage attribution, the accepted decoder fix, and the open upstream question.
- `docs/decisions.md` D086/D087 — completed native initialization/memory work; D088 — fact
  authorities and the superseded strict-parity acceptance clause; D089 — this phase boundary.
