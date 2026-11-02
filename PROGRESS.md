# Progress

## CURRENT: D116 support guard rejected; restored E verification complete (2026-11-02)

**D116 state:** The validated E source was committed as
`fba7ebad9f8ca07bdaa73eb8b4422299de9a8078`. At
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_e_hotpath_20261102_Qpl24N`,
the first profile capture (6,086 samples/46.985 s) failed quality and is retained
but not selection evidence; the second observer failed because `rg` was absent
in the container and its late manual capture ended status 255. The corrected
third capture passed: 11,274 samples over 60.057764 s, zero lost/empty, capture
exit 0 after 100k groups, and the existing quality checks. It attributes 95.875%
of caller samples to the resolver (splice 0.03548%, FastLocate 0); reliable
ELF-self attribution is decode lambda 15.31%, `PostingView` 9.62%, `malloc`
4.95%, `free` 3.18%, and `unique<uint>` 1.04%. Do not rank source-inline reports:
their addr2line evidence is erroneous. Profiled3 native wall was 120.18 s
(6,204.78 + 76.30 CPU-s) and is instrumentation context, not a comparison;
120.14 s belongs to failed-observer profiled2. All E native counts passed exact
R7 parity.

The selected smallest change, the adjacent duplicate body-support push guard
(+5 net production lines; final sort/unique retained), passed proof review and
175 tests in 30.00 s without build warnings. Terminal driver results in the D116
root were exact R7 parity for all four first-1M cases (four Parquet/six MEX plus
decoded/logical/profile/prior/counter evidence), same verified image, zero
max/OOM/swap and zero major faults: E-control1/2 wall 117.58/127.11 s, CPU
6,139.38/6,763.52 s, HWM 68,175,273,984/68,506,148,864 B; guard support1/2 wall
118.39/124.94 s, CPU 6,242.49/6,727.88 s, HWM 67,495,948,288/68,155,641,856 B.
Mean guard wall was -0.5558% and CPU +0.5229%, noise without gain. The guard is
rejected and removed; source is restored to committed E `fba7eba`, with no added
production complexity. Frozen candidate artifacts remain preserved and the new
27-line generic regression remains. Restored-E rebuild/full-175 passed in
29.58 s without warnings, and `build/src/panCollapse` is byte-identical to the
frozen E binary used by both fresh controls; no extra 1M run is needed. The D116
gate is complete. No automatic next run, release, whole-GAMP count, or index
work is authorized.

Preserve the existing inputs and resource envelope: no whole-GAMP count, index
rebuild, GAMP/XG/RI or input-hash work, biology/CB/UMI/profile/scorer/cap change,
spill, release, or other job manipulation.

The D115 R7/E observation supplies the original-R7-versus-E comparison: a sequential,
shared-host `n=1` comparison showing E 15.702% lower wall and 17.172% lower CPU
than original R7 under its stated confounders. It is neither a causal E speedup
nor a latest-E-versus-F comparison: R7 is not asserted to be later intermediate
F, cache warmth differed, major faults were 87/0, and an unrelated 128-thread
job remained busy. D116's current E controls are separate, host-conditioned
iteration evidence and do not convert D115 into E-versus-F, whole-GAMP, or <2h
evidence.

**Matched benchmark terminal:** The two sequential real-first-1M cases completed
with terminal driver exit 0 at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_r7_e_current_host_20261102_hgbDBv`
(`RESULTS.json`, `RUN.json`, and per-case validation/parity receipts). They used
the frozen original R7 binary at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_posting_reuse_20261101_nf6RG9/panCollapse`
and immutable E at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_buffer_dense_reuse_20261102_93jnXf/candidate-e/panCollapse`.
Both used 1,000,000 groups/2,502,433 records, both profiles, Parquet plus MEX,
the same 167,968,148,664-B direct `.pci`, 64 threads/64-CPU quota, 300e9-B
no-swap envelope, and verified identical Docker image IDs. Both passed exact
frozen-R7 parity for four Parquet and six MEX bytes plus decoded, logical,
profile, prior, and counter comparisons; both had zero OOM/swap. R7/E wall was
299.70/252.64s; CPU 17,395.35/14,408.16s (4.83204/4.00227h); HWM
69,774,120/66,551,840KiB. The observed E reduction is 15.702% wall and 17.172%
CPU (1.18627x throughput).

This is a single sequential R7→E shared-host observation (n=1), not an isolated
causal claim, a whole-GAMP <2h prediction, or an isolated latest-E-versus-F
effect: major faults were 87/0, cache warmth was neither identical nor flushed,
and a separate 128-thread job was busy before both cases and after all work;
that job was preserved. Production code was unchanged in this phase; no index
or hash work, full run, or release occurred. The bounded comparison gate is
complete; no automatic next launch is authorized.

User explicitly reactivated the 66,601,887-group full 10/30 whole-GAMP per-gene
objective: <2h wall and <300e9 B. R7 authoritative terminal audit at 17:03:05Z
achieved it: native 6,372.41s (827.59s headroom), kernel HWM 245,153,902,592B
(54,846,097,408-B headroom), count-memcg peak 128,493,842,432B, and zero
max/OOM/oomkill/swap/spill. Container/native both exited 0 after full original
66,601,887-group/166,824,266-record GAMP EOF. Independent Parquet/MEX validation
and exact R6 four-Parquet/six-MEX byte/decoded/logical/profile/prior parity pass.
Counts: [R7/full/counts](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_posting_reuse_20261101_nf6RG9/full/counts);
authority: [full-terminal.json](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_posting_reuse_20261101_nf6RG9/full-terminal.json).
Tool goal state is complete after root receipt/documentation review. This is not F1,
biological truth, external CR7 workflow equivalence, release, or causal-speedup proof.

**Full64 terminal:** Frozen candidate C completed original 66,601,887 groups/
166,824,266 records, container/native exit 0, in 8,770.10s (2h26m10.1): the
64-core <2h target failed by 1,570.10s. Memory passed—HWM 225,357,627,392B,
memcg 184,388,919,296B, zero max/OOM/swap/spill—and independent Parquet/MEX plus
exact R7 four-Parquet/six-MEX byte/decoded/logical/profile/prior parity passed
with unchanged counters. CPU was 534,792.51s vs R7 760,250.94s (~29.66% lower),
but differing threads/shared host prevent causal inference. Authority:
[candidate-c64-full-terminal.json](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_remaining_optimizations_20261101_gGNqCQ/candidate-c64-full-terminal.json).
The prior 128-core objective remains historically achieved; do not mark 64-core
achieved. Targeted profiling, remaining scratch reuse, or a 96-core comparison
were evidence-gated and unlaunched. User now authorizes the bounded next phase
below; no automatic full rerun follows.

**D113 current-build optimization gate:** Latest “Do it” supersedes the prior
read-only restriction. First take representative early/middle/late profiles of
current C build membership/scoring work (the older R7 profile predates C), then
implement only the most promising exact-count candidate. Each candidate needs
tests, exact 1M parity and representative windows; consolidate to full only if
gain justifies it. C64 needs 17.9% less wall time (1.218x); ideal 64*7200 CPU
budget is 460,800s versus 534,792.51s, at least 13.8% CPU reduction plus
overhead. No index build, XG/GAMP/RI input change, policy/complexity change,
representative UMI-family shortcut, or spill. R7/C64 and STAR scope limitations
remain controls, not causal comparisons.

**D113 profiles complete; D/E1 rejected, F timing deferred:** Fresh current-C early/
middle/late profiles (`profile-c-{early,middle,late}-profile`) passed quality:
11,229/11,199/11,563 samples over 59.96/59.96/61.36s, zero lost/empty. About
96% are resolver callers; flat self decode lambda ~9%, `PostingView` decode
8.3–9.7%, `Source::model` 6.1–6.4%, malloc+free 7.8–8.5%, with model-map
insert/destruction extra. Caller and self scopes overlap and are not additive.
D passed all parity but is rejected/restored to C: early/middle/late C/D CPU-s
7479.97/6876.14 (-8.07%), 7158.10/7339.67 (+2.54%), 6773.12/6758.63 (-0.21%);
total only -2.04%, inconsistent shared-host. E1 also passed exact early parity
but regressed (139.75s/7,665.11 CPU-s vs C 135.62/7,479.97; HWM 66,387,428KiB),
so it is restored to C with no middle/late/full trial. Candidate F now quotients per-record
model incidences by immutable decoded `LocalPostingList` identity: assign a
64-bit read-local list ID in the existing deque, retain every actual node's
bases/score/orientation/endpoints/edges, and use ID only for class lookup.
No index/input/score/cap/policy/UMI/cache change. Require shared/missing,
unequal flags/support, subset/zero-base/empty/mixed-support oracle differentials,
full 175, then early 64-core exact unprofiled C comparison; middle/late only on
material gain. F now passed focused 5/5/full 175 (30.51s): early exact parity
improved C→F 135.62/7479.97 to 123.07/6630.32 wall/CPU-s (-9.25/-11.36%) and
rows 25,868,945,316→16,969,675,586; middle exact parity regressed
131.83/7158.10→137.90/7562.25 (+5.65% CPU). All three F windows passed exact
four-Parquet/six-MEX parity, but late (149.12s/8,212.56 CPU-s; HWM
66,279,268KiB) is confounded by an independent 128-thread count. A second,
whole-GAMP 128-thread count then confounded C(S1); its benchmark driver was
terminated before F(S1), F(S2), or C(S2) could launch. C(S1) finished native
work and all parity checks: 275.63s/14,172.86 CPU-s, HWM 65,513,268KiB, zero
major faults. Thus no F retention/speed/full-throughput
claim follows. Defer the four-run physical-core comparison to a quiet window or
coordinated CPU allocation; C64's 2h26m result remains authoritative. A separate
offline, existing-early-profile line-attribution analysis in `R/line-attribution`
(1 CPU, 8GB, 180s) exited 0 but failed attribution quality: addr2line protocol
errors invalidate source-line rankings. Existing caller/flat profiles remain
the reliable evidence. No new live profiler, full count, index or input change.
F remains a count-validated optimization-branch candidate, not a promoted
performance result. The next speed gate needs a quiet/coordinated 64-core window.

**D113 bounded-iteration clarification:** The user accepts exact one-million-
read validation for current optimization iterations. This does not establish the
64-core target, release F, request an automatic whole-GAMP run, or replace the
pending controlled-performance gate. No implementation or experiment was
performed in this read-only Q&A; the additional suggestions were reported in
chat and are not newly accepted design decisions.

**D114 exact-count optimization gate:** The user now authorizes narrowly scoped
implementation of worker-buffer reuse; dense list-ID class lookup; batch posting/
model access; and conditional omission of threshold preparation only for a
single-start, no-join DAG, while retaining every alignment score. Membership
preparation reuse across alternatives is authorized only after measuring repeat
record-list sets. Each independent patch requires proof/feasibility review before
subtle algorithm or concurrency edits, full CTest, then exact 1M R7 parity
(four Parquet, six MEX, decoded/logical/profile/prior/counters) and resource
statistics at the same 64-core quota, 300GB, and no-swap envelope. No full run
or <2h/release claim is authorized. Do not change GAMP/XG/RI, CB/UMI, profiles,
scorer semantics, or caps; do not hash inputs, spill, or build an index. The
earlier F timing remains inconclusive because of later-window and affinity-control
confounding; it is not adopted as a speed estimate. The current gate is this
per-patch exact 1M validation sequence.

**D114 A--D archived; E is the active gate:** In
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_buffer_dense_reuse_20261102_93jnXf`,
A (workspace plus dense lookup) and B (ready models plus temporary recurrence)
each passed 175 CTest and exact 1M validation. Their observed baseline-F/A/B
wall/CPU-s were 210.42/11,991.94, 265.46/15,282.72, and 262.28/15,233.19.
D passed 175 CTest and exact 1M validation at 236.30s/13,561.54 CPU-s; the final
F control passed at 223.44s/12,905.47 CPU-s. All current timing occurred under
heavy competition and is provisional: it establishes neither a causal regression
nor speedup, nor full/under-two-hour evidence. The membership-preparation result
(4,001/1,574,890 repeats; 0.254%, and 104,542,815/16,966,500,631 repeat/list
rows; 0.616%) rejects a plan cache; its telemetry is deleted.

Following the user's simplicity preference, parent selected E to remove the entire `NodeScoreWorkspace` API,
implementation, guard/thread ownership/budget/logging, and unused `Delta.model`.
It retains only record-local vector reuse across piece/group loops, dense-ID
lookup, immutable `ready_models`, and the no-join bypass; the production diff is
now 28 lines. A--D artifacts/results remain preserved but are superseded by E,
not active parallel engines. E's full 175-test suite passed in 34.97s; its
native 1M run completed in 229.69s with 13,097.69 CPU-s and 66,792,380KiB HWM.
The Docker driver exited 0, validation/MEX-validation/parity JSONs are present,
and `PARITY_PASSED`/`VALIDATED` establish exact 1M output parity. Source/binary
snapshots at `R/candidate-e` exactly match current uncommitted code on base
`a4138e0`; this is neither a commit nor release. F's 210.42s then 223.44s
controls and E's 229.69s observation are all contended/provisional and do not
demonstrate a speedup or under-two-hour result. The next safe gate is quiet, controlled 1M
performance validation only; no full run, release, index, UMI, or policy change
is authorized.

**64-core feasibility/comparator question (read-only):** At that comparison, HEAD was
`5003be0` over source `3b4c76f`. C64 consumed 534,792.51 CPU-s = 148.553475
CPU-hours for both count profiles including MEX, and is 1,570.10s/21.8% over
target: it needs 17.9% less wall time (1.218x throughput). Its fresh C profile
is unavailable; prior fresh profile predates A/C, so it cannot attribute the
remaining gap or establish feasibility.
Both completed STARsolo 2.7.11b arms processed the same named PBMC sample and
66,601,887 read pairs, including linear alignment and three raw count modes,
without BAM. GRCh38/T2T native walls are 455.30/469.75s at 32-core quotas:
CPU user/system were not captured, so ~4.05/4.18 core-hours are quota-based
upper bounds, not measured CPU. C64 counting alone therefore used at least
~144.5/144.4 extra CPU-hours (at least ~36.7/35.6x); it excludes mpmap and uses
different reference/assignment machinery. Receipts are in
`/mnt/ssd/lalli/pancollapse-wg-analysis-20261101/data/linear/starsolo-{grch38,t2t}/run/count-resource.txt`;
commands and `Log.final.out` in those directories establish scope/read counts.
Attempt5's separate mapping-container terminal CPU counter is 446.2702 hours
including its 10k pilot/helpers, not a full-mapping-only delta; adding C64 gives
594.8236 recorded core-hours, not a same-container end-to-end benchmark.
No code or run follows from this question alone; a current-build profile remains
necessary before attributing or promising the remaining optimization gap.

**D111 implementation gate:** Clean 119-file baseline commit `c408857` was made
after source archive comparison and binary `cmp` to frozen R7. User authorizes
only count-preserving measured optimization: fresh bounded R7 profile plus
aggregate cache/ingest telemetry; read-local scratch allocation removal; independent
buffered index writer; then conditional cache key canonicalization after the
original cap, measured duplicate work, and per-key singleflight. Metadata mapped
views/accounting, compact exact QNAME guard, observation multiplicity, bounded
locality, and structural/SIMD/grammar ideas require their stated evidence gates.
Per patch oracle/CTest; then consolidated 1M exact R7 parity and one full original
GAMP under <7200s/<300e9/no OOM/swap/spill. Counts must remain identical;
diagnostic counters may change. Revert unhelpful code; no biology/scoring/UMI,
input XG/GAMP/RI, spill, or release change.

**D111 64-core evidence checkpoint:** New target is <2h on 64 cores (96
intermediate), <300GB and exact R7 counts. R7 used 760,250.94 CPU seconds, so
64*7200 permits 460,800: at least 39.4% less CPU work before overhead is needed,
not a promised speedup. Fresh R7 profile baseline passed exact 1M parity
(107.48s native/HWM 80,584,964KiB); profile timing is not a causal A/B. Its
23,036-sample/61.562883s zero-loss caller profile is 96.4273% resolver-other,
0.05209% splice; no-children self is resolve 35.44%, posting decode 9.41%, model
4.99%, malloc/free 3.89/3.29%. Do not sum caller/self fractions. The buffered
64KiB writer, Delta scratch and aggregate cache/ingest metrics are validated
below; the fresh profile motivated these changes rather than proving speedup.

**D111 A/C validated on prefix; B rejected:** A (buffered writer, Delta scratch and
metrics) passed full 174 (32.45s), exact 1M parity, and 64-core prefix: 141.03s/
7,690.11 CPU-s/HWM 68,703,408KiB versus frozen R7 197.86s/11,367.23 CPU-s
(~29% wall/~32% CPU). Shared-host prefix comparison is not full proof. Duplicate
builds were 10,469/685,400 (1.53%, count not CPU cost); ingest had zero fallbacks
and 854,778 full-queue submissions, so it is not priority. B added only exact
post-cap sorted/deduplicated length-framed cache records but was rejected:
1M/64 153.54s/8,524.55 CPU-s versus A 141.03s/7,690.11, only 765 keys saved
(674,166 vs 674,931); root restored baseline key header/test hunks. No rejected
B implementation or feature flag remains active.
C now passed its direct/shared-vs-missing contract/oracles and 64-core exact 1M
gate: 123.46s/6,596.85 CPU-s/HWM 66,413,420KiB, zero memory max/OOM/swap and
four-Parquet/six-MEX/logical/profile/prior R7 parity. It reuses 3,327,984 lists
and 8,837,666,038 posting rows versus 8,662,357/16,616,919,983 decoded. This is
still single-prefix engineering evidence, not a full 64-core proof.

**D112 V1 cleanup integrated and validated:** User authorized manual integration of
uncommitted isolated `remove-v1-direct-index.patch` from
`direct-index-v2-only-20261101`. Remove only PCDIRECTV1 reading/decoding,
repack/upgrader API/CLI and obsolete V1/repack fixtures/tests; retain compact V2
counting, PCANIDX2/3 and native annotation preparation. Root compared the actual
uncommitted six-file diff with the supplied patch, then manually retained D111's
buffered writer/boundary tests and C's V2 shared-list identity while removing
only the obsolete compatibility code. Active-tree build, full 174 CTest (31.25s)
and consolidated C64 1M exact R7 parity pass, independent of the isolated proof.
No 168GB rebuild or cleanup-only whole run; jobs/frozen A/B/R7/indexes untouched.
Receipt: [v1-cleanup-integration.json](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_remaining_optimizations_20261101_gGNqCQ/v1-cleanup-integration.json).

**Consolidated gate completed:** D112 retained PCANIDX2/3 with no index-byte
change or 168GB rebuild. After root's prelaunch checkpoint review, the original
66,601,887-group 64-core optimization run completed with full R7 parity and
memory acceptance, but missed <7200s as recorded above. No cleanup-only run.

**R4 compact empirical gate (terminal):** Repack exited 0 at 12:20:20Z
(5,822.268 s real) with 167,968,148,664-B final index. Its queued 1M count and
validators exited 0 at 12:24:11Z: native wall 200.040151 s (init/read/finalize
6.694304950/178.824724895/2.478044831), 1M groups/2,502,433 records/982,842
counts/1,302,814 molecules. Four Parquet/six MEX bytes, logical tables,
profiles and priors exactly match N (`PARITY_PASSED`). Peak memcg
126,853,758,976 B, zero OOM/max/swap/spill; 5-s RSS samples are not kernel HWM.
Final cache was 674,931 entries/296,609 hits/1,009,589,118 charged B (diagnostic
counter change only). Valid profile: 22,474 samples/60.435917 s/zero loss or
empty, 0 explicit FastLocate, 97.477% resolver-other/0.049% splice. R4's
~5,592 groups/s prefix rate is a warning, not a full-run falsifier: warm-cache
asymptote is unknown and hits include duplicate builder losers.

**R6 prefix passed / full native EOF near-miss:** R6 prefix exited 0/no OOM
12:38:01.593650971–12:40:55.325512877Z: GNU time 143.57 s (native 143.580467 s;
init/read/finalize 6.190571497/126.821282846/2.826345579), 7,885.111848 groups/s,
kernel HWM 77,308,772KiB/79,164,182,528 B, memcg peak 28,207,353,856 B, zero
max/OOM/swap/spill. Exact N 4-Parquet/6-MEX/logical/profile/prior parity passed
(982,842 rows/1,302,814 molecules); receipt `R6/first1000000-terminal.json` is
validated. Root launched the same frozen original-GAMP full at 12:42:36Z:
`pancollapse-memory-guard-full-20261101-o70tyk` (ID `a3daffbffed697ad2b274767241592c5bc6d832305b5b06a25a031af4f08a04c`, host PID 1071618), 300e9/128 quota, no profiler/input/rule change. At ~220k/28.0s it is live with zero OOM/swap.
Native completed 0: GNU time 7,305.22s/native 7,305.224792s (2h1m45.22), kernel
HWM 240,016,211,968B, memcg peak 166,879,653,888B, zero max/OOM/swap/spill.
EOF has 66,601,887 groups/166,824,266 records. Container exited 0 at 14:48:07Z;
both independent full Parquet/MEX validations passed and `R6/full-terminal.json`
is valid. Counts are at `R6/full/counts` (four Parquet 295,286,773 B).
It misses <2h by 105.22s (~1.46%) while RAM passes; goal is not achieved.

**R7 frozen pre-prefix gate:** R7 implements only read-local `LocalPosting`
descriptors and exact `has_annotation`, replacing repeated membership varint
decompression while retaining IDs/support/flags/order (8B/touched posting versus
old 4B local ID). Annotation-free/repeated-node/body-support two-record/two-
profile oracle passed; focused 2/2 (.24s), full 174/174 (32.09s), independent
oracle/eager baseline/goldens unchanged. Frozen
`pancollapse_posting_reuse_20261101_nf6RG9` holds 93,112,944-B binary and
495,380-B archive; comparisons and `run-count.sh -n` passed. R7 prefix exited
0/no OOM (14:59:01–15:01:35Z): GNU time 123.64s/native 123.646394s,
init/read/finalize 6.025417827/107.214604235/2.642745386, 9,327.087547 groups/s;
kernel HWM 81,771,417,600B/memcg peak 30,953,394,176B, zero max/OOM/swap/spill.
Exact N 4-Parquet/6-MEX/logical/profile/prior parity passed at 1M/2,502,433/
982,842/1,302,814 group/record/count/molecule totals (`PARITY_PASSED`); terminal
receipt is being written. After root review, launch same original GAMP/R4 index/
profiles/filters at 300GB/128/no swap/spill, unprofiled GNU time; validators then
exact full comparison to R6. Prefix rate is not goal proof. No other optimization.

**R6 memory-guard candidate:** Sol proof review accepted an exactness-
preserving guard for new R6, combining the late-winner changes and guard, not R4:
retain mapped-reference
admission; periodic RSS reader disables cache retention permanently and clears
one shard lock at a time at target minus the existing 10% reserve; leases remain
valid, then best-effort `malloc_trim`; later pressure clean-stops. Snapshot/
destroy cache before finalize and guard EOF resident use. No LRU/refill/ceilings,
model dropping, or policy/profile/count change. It is not an OOM guarantee:
whole-run RSS/HWM+cgroup peak <300e9, zero OOM/swap/no spill remain required;
`memory.events:max` may occur through reclaim. R6 focused 8/8 passed 7.22 s and
full 174/174 passed 31.92 s (session 89139 exit 0). Frozen
[`R6/RUN.json`](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_memory_guard_20261101_O70tyk/RUN.json)
records 93,141,152-B binary and 495,153-B source archive; binary/source compares,
Docker RUN validation, and `run-count.sh -n` passed. Its authorized same-index
1M job passed above; full measurement is live. R5 remains frozen late-winner-only.

**Read-only performance-review gate:** User authorized twelve distinct reviews
of direct-index speed/memory, alternate performance approaches, cache hits, and
empirical comparison with the old same-1M control. All twelve completed, with no
more than two concurrent children; they authorize neither biology/policy change nor
review-driven implementation before root synthesis. R4 stays untouched with its
queued 1M gate; R6 remains under its separately recorded authorization.

**Completed direct-index review synthesis (11:55Z):** Twelve read-only reviews
are consolidated in [direct-index-performance-review-20261101.md](docs/research/direct-index-performance-review-20261101.md).
Root reviewed the synthesis: unchanged R4 benchmarking first; if needed, read-local reuse plus
allocation removal, then measured cache-key work, with structural sharing only
as fallback. No source/policy change is adopted. R4 remained live without final
`direct.pci` or count (161,711,487,246-B postings unchanged since 11:49Z; phase
unknown), so final fit/speed are unmeasured. R6 is now tested/frozen but unlaunched;
R4/R5 are untouched.

**Pause snapshot (04:29:05Z):** R2 PID 1265651 was live without OOM at
42M/116,844,907 slots with 397,631,746,048-B partial postings; waiter
1381666/1381670 and observer 1602210 were recently confirmed. R5 remains
tested/frozen and unlaunched. The <2h/<300e9-B objective and all gates persist.
At 04:34:50Z R2 was 44M/116,844,907/no OOM; waiter 1381666/1381670 and observer
1602210 were live.

**Live wait / admission evidence (04:06:42Z):** R2 remains running at PID
1265651, 28M/116,844,907 slots, zero OOM. Partial raw postings alone are
271,496,812,248 B, exceeding the 270e9-B maximum after the unchanged 30e9-B
count safety reserve, before other reference/resident data. Thus raw V1 cannot
be admitted under 300GB; queued exact V2 repack is necessary, with final V2
size still unknown. Detached R4 waiter 1381666/1381670 remained live for five
minutes. A retained 37-s resource observation in
`pancollapse_direct_facts_batchfix_20261101_1REM6g/resource-observation-20261101T040409Z.json`
shows 67.2 CPU cores, 100.5 MB/s writes, 78.1GB anon and clean-file reclaim;
not a count-speed measurement or evidence of storage-bandwidth saturation.
N exited 0/no
OOM at 2026-11-01T01:10:43.120947404Z as the accepted numeric-control evidence:
four Parquet and six MEX files are byte-identical to G, including both profiles'
count/molecule tables and identical all-read barcode priors. Full CTest passed
172/172; peak memory was 149,023,064,064 B. Read processing took 715.320 s
(~1,398 groups/s), 17.73% slower than G under different concurrent workload;
this is not an isolated performance comparison. Terminal authority is
[N/terminal-receipt.json](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/numeric-count-first1000000-t128.vWzYte/terminal-receipt.json).
Historical receipts are controls; do not infer current job state from prior
checkpoints. Reactivation changes no policy, remapping, or input contract.

**Historical retry launch:** `pancollapse-direct-index-20261101-batchfix` started at
2026-11-01T03:19:24.28115287Z, constructor entered `phase=load`; 128 CPUs,
300,000,000,000-B hard cap and no swap verified. Frozen source/binary, final
174/174 log, scripts and `RUN.json` are in
[`pancollapse_direct_facts_batchfix_20261101_1REM6g`](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_direct_facts_batchfix_20261101_1REM6g).
The host `after-index.sh` continuation waits for success, then runs only the
frozen 1M count and independent Parquet/MEX plus exact N byte/logical checks.
No complete direct index, empirical count parity, or speed result yet.

**Historical early retry snapshot:** At 03:33:47Z, verified Docker PID 1265651 remained running. After 194.834-s
load, index log had reached 8M/116,844,907 graph-node slots; partial posting file
was about 68 GiB (rounded `ls`). Cgroup current/peak were
151,776,280,576/151,778,562,048 B, with max/oom zero. This is construction
progress only. Next authorized sequence: completed direct reference fitting
<=300GB, exact 1M N parity/resources, optimize if needed, then full same-GAMP
counts and full-scope validation against the active objective.

**Historical separate-pilot reconciliation (2026-11-01):** R2 remains the unmodified
frozen batch-fix builder; at 03:47:30Z it was 17M/116,844,907 slots, partial
postings 157,543,858,176 B, cgroup current/peak 240,284,389,376/
240,287,301,632 B, max/oom zero; count has not started. The missing turn-scoped
waiter was restored without restarting index: detached bash PID 1297730/PPID 1,
SID 1297730 and live `docker wait` 1297737; R2 `RUN` records recovery. New R3
candidate `pancollapse_compact_assignment_key_20261101_TN29f2` is frozen/prepared
only: compact typed field visitor/size pass/canonical unsigned varint key omits
unused fields, no per-record/Subpath serialization, and preserves scorer read
fields/profiles/score/UMI. Full 174/174 passed 30.79 s; real parity/speed pending.
Sorted background output completed but is opaque-payload receipts/totals only, not
multiset proof or adopted count path. The old separate R2/R3 1M-pilot wording is
superseded by the consolidated R4 gate below.

**Exact-storage feasibility checkpoint (2026-11-01):** At 03:51:32Z R2 remained
live, frozen binary/source unchanged, `phase=nodes` 19M/116,844,907, no counts;
restored detached waiter PIDs 1297730/1297737 had remained live 14 minutes. R3
compact varint signature is bounded-test accepted (174/174, 30.79 s), real data
pending. Next authorized task is exact storage-size feasibility and, if needed,
compact read-only representation—engineering only, not a biological/policy
decision or relaxed planner. Existing 16MiB-prefix compression receipt and
`storage-feasibility.json` are indicative prefix evidence only; do not rerun
experiments. Active objective remains unmet.

**Compact V2/repack empirical prelaunch checkpoint (2026-11-01):** Exact V2
codec/repack is bounded-test accepted: PostingView, direct index V1/V2 body
factoring and resolver range iteration; nonbody nonzero/conflicting body support
fails publication. Profiles/scoring/UMI unchanged. Full 174/174 passed 31.46 s
(`build/compact-direct-index-ctest.log` 351/410), focused C++ 3/3 2.00 s. Root
CLI/repack and native verification cover canonical/no-overwrite/invalid/count
equality. Frozen R4 `pancollapse_compact_direct_facts_20261101_FAuuqd` has
93,109,696-B binary, 566,500-B archive, logs/RUN/repack/count scripts, unlaunched.

R2 at 24M/116,844,907 has 237,611,110,572-B partial postings, not final size.
Its memory limit now includes file cache: last max 20/oom 0/oomkill 0, anon
77.9GB/file 215.5GB—not all heap. After review replace only host waiter
1297730/1297737, then R2-success -> exact R4 repack <=300GB -> one combined 1M
exact-N parity/resources gate, consolidating R2/R3 pilots. Only then assess rate
and full original-GAMP run under active target. No whole launch/fit/speed claim.

**Current queued continuation (2026-11-01):** After checkpoint review only the
old host waiter 1297730/1297737 was terminated; native R2 PID 1265651 was not
stopped. Detached R4 continuation bash 1381666 (PPID 1, SID 1381666) and docker
wait 1381670 were verified live. R2/R3/R4 RUN records now route automatically:
R2 success -> V2 repack -> one frozen 1M exact-N gate, with no automatic full
launch. Final index/count remain pending; active objective is not met.

**R4 caller-profile prelaunch checkpoint (2026-11-01):** At 04:07:45Z R2 was
live at 29M slots; queued R4 waiter 1381666/1381670 remained confirmed. Existing
P is the terminal exact-N 1M control with valid 60-s/3-Hz DWARF, 22,090 zero-
loss/empty samples and RI-node/splice/resolver-other 59.31/5.81/31.64%; it is
retained, not duplicated. To diagnose remaining direct-counter cost only if R4
is slow, root may prepare one fail-open external 60-s, low-frequency caller
capture on R4 first-1M using existing setup, capped profiler 8GB/4CPU (the
accepted P configuration), no host
sysctl/binary change/extra read run. Full objective timing stays unprofiled;
captured prefix rates are profiled, nonisolated evidence only. Profiles, biology,
inputs, and counter matrices remain frozen. No full raw-count admission: 271GB
exceeds 270GB proof. Root review precedes installation.

**Late-winner hotpath candidate checkpoint (2026-11-01):** R4 remains the frozen
174/174 control; its queued repack/1M exact-N gate and optional caller capture
are unchanged. Optional profiling helpers (8 tests), mock fixes, and pinned-image
perf/Python preflight passed; observer 1602210 (PPID 1/SID 1602210, 04:17:35Z)
waits only for R4 count, separately from 1381666/1381670 and R2 PID 1265651; no
capture occurred. Sol completed the proof and root reviewed minimal edits to
`src/node_score_resolver.cpp` and `tests/vg/node_score_dp_test.cpp`: lazy
WinnerKey creation omits only no-score keys, retaining scored missing-policy
winners and exact ties/negative scores/support. Build exited 0; focused 2/2
passed 0.20 s and full 174/174 passed 31.22 s (`build/late-winner-{build,
focused-ctest,ctest}.log`). Separate frozen R5
`pancollapse_late_winners_20261101_wc5G9s` holds its 93,081,344-B binary,
566,996-B source archive, RUN/logs; eager oracles and R4 remain unchanged. Real
R5 parity/speed are unmeasured and no R5 job/script exists: use it only after R4
rate/profile establishes need. R2 is now 40M/116,844,907 nodes/no OOM. Objective
unmet.

Direct-facts custom-index implementation is complete and bounded-test accepted:
graph-specific node length/membership, numeric ready-model metadata, physical
splice targets and directed ownership persisted in an immutable read-ready count
source. XG/GBWT/RI are compiler inputs and remain external/unchanged; no path
positions/steps/global occurrence arrays. Full-reference compilation and empirical
1M parity remain pending.
Numeric engine's bounded 1M parity gate is accepted; this is not whole-performance
achievement; its earlier paused-goal state is historical and does not meet the
reactivated whole objective.

**A2 full-reference build failure / retry gate (2026-11-01):** A2 exited 1/no
OOM at 2026-11-01T03:12:31.792628943Z after 314.924-s wall/198.368-s load, at
last 1M graph-node slots (not reads): `worker ordered-batch payload exceeds
memory budget`. Peak was 132,390,883,328 B under 300e9 B, with all memory max/
oom/oom_kill zero and swap 0. RAII removed incomplete regenerable temporary
posting payloads; no index published, input/control changed, or count started;
`after-index.sh` correctly exited 1. Evidence is
`R/pancollapse_direct_facts_20261101_UKcX1N/{index.log,index.time.txt,index-resources.log,continuation.log}`.
Sol's fix is bounded adaptive/flush batching replacing fixed 256 items, preserving
targets/facts; no cap raise/spill/hash/policy change. Next: regression/full suite,
freeze retry, full compile at 300e9 B/no swap/128 cores, then 1M N parity only on
success. This is not whole production promotion.

**Batch-fix retry acceptance checkpoint (2026-11-01):** Fixed compiler full
174/174 passed 30.92 s (`build/direct-facts-batch-fix-ctest.log`, lines 351/410);
`direct_count_compile_test` regression passed 3.11 s (line 110); build log
`build/direct-facts-batch-fix-build.log`. Fixed 256-result queue is replaced by
ordered waves: at most one completed result per actual OpenMP worker, stable
ascending serial commit; Writer/candidate budgets unchanged, remaining memory
rebalanced to one-result scratch/retention. The 48-B cap regression fits a node
but not queued dense results and yields byte-identical serial artifact. CLI,
profiles, biological facts unchanged; barrier overhead is residual measured risk.
Frozen retry root is `pancollapse_direct_facts_batchfix_20261101_1REM6g`; A2
failure root remains preserved. No job is launched. After root review, run same
full compile at 300e9 B/no swap/128 CPUs; `after-index.sh` launches frozen 1M
count and N byte/logical Parquet/MEX validation only on success.

**Historical direct-reference build launch:** container
`pancollapse-direct-index-20261101-ukcx1n-a2` started at
2026-11-01T03:07:16.644025201Z with 128 CPUs and a hard 300,000,000,000-B
cap/no swap. Constructor completed loading in 198.368 s and entered
`phase=nodes`; this is not a completed index.
Frozen binary/source and `index.log`, `index.time.txt`, `RUN.json` are in the
direct-facts result root linked below. First wrapper attempt exited 127 before
construction because the image lacks `/usr/bin/time`; retry uses Bash timing,
no new dependency. Next: quantify complete index size/resources, then frozen
1M count and exact N comparison. `after-index.sh` is waiting on successful
compiler exit and will launch the frozen 1M count (same cap/threads), independent
Parquet/MEX validation, and byte/logical N comparison automatically. Its
`continuation.log` and `RUN.json` record this engineering gate. No whole-count
restart is part of that script.
At 03:12:15Z the builder had completed 1,000,000/116,844,907 graph-node ID
slots (999,999 present nodes), with 5,500,393,992 B of partial posting payload.
Cgroup current/peak were 84,096,843,776/132,390,883,328 B, zero OOM events.
These are reference-construction metrics, not read throughput or an estimate
of complete-index size.

**Historical direct implementation-in-progress checkpoint:** `direct_count_compile.{hpp,cpp}` then
contains bounded ordered parallel construction, ready model/node facts, and
physical/directed splice facts. `Source::node_view` and the scorer's immutable
posting spans/local-ID sidecars are implemented. Count CLI/runtime now target
the direct reader only; native queries are a test-only oracle. Storage
`direct_count_index.cpp` was being replaced after review rejected the initial
in-memory payload copy; the integrated build was then not yet accepted. Focused
resolver/DP tests passed, and the assignment-only memory planner's standalone
test passed. New storage/compiler tests and migrated count fixtures were pending
the completed reader/writer. No full-reference build or new 1M count had started.
Artifacts/continuation inputs are recorded in
[`RUN.json`](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_direct_facts_20261101_UKcX1N/RUN.json).

One external caller-attributed profile of unchanged frozen N is terminal and
accepted for measurement; it is not a speedup, redesign, or whole-goal result.

**P terminal caller-attribution checkpoint (2026-11-01):** P passed profile and
exact N parity: exit 0/no OOM; 1M/2,502,433 records; 982,842 count and 1,302,814
molecule rows; four Parquet/six MEX byte plus decoded/logical/CB-prior parity,
with no changed manifest counters. Its nonisolated wall/read time was
654.467462/425.698263286 s. Valid 60.370597-s, 3-Hz DWARF capture had 22,090
samples, zero lost/empty, 99.9894% RI-like caller attribution: inclusive splice
5.8126%, node-only 59.3119%, resolver-other 31.6388%, other 3.2368%. This does
not include inlined masks/DAG state effects, infer counterfactual speedup, or
authorize production/index change. Next is reporting measured cost only.

**Direct-facts index implementation checkpoint (2026-11-01):** This phase
authorizes bounded implementation and acceptance testing only. Preserve count
algorithm, profiles, barcode/UMI behavior, and output; no input hashes or count
spill. N/P remain 1M controls; speed and <2h/<300GB whole performance are
unproven. Sol accepted semantics but warned raw FastLocate occurrence cost and
unknown complete-index size/build time. Build direct index only after bounded
acceptance; no whole launch until parity/resources support it.

**Direct-facts bounded acceptance / full-reference prebuild checkpoint
(2026-11-01):** `direct_count_index.{hpp,cpp}` provides streamed numeric shared
lists/mmap reader; `direct_count_compile.{hpp,cpp}` bounded ordered parallel
compile. Runtime is direct-only: no XG/GBWT/RI/receipt loading; immutable scorer
spans/local-ID sidecars and assignment-only cache planning. Final 174/174 passed
31.83 s (`build/direct-facts-ctest.log`, total line 410), focused 12/12 8.18 s
(`build/direct-facts-focused.log`), build log `build/direct-facts-build.log`.
The initial four old UMI PYTHONPATH misses were rerun inside Docker with existing
PYTHONPATH, no install; final all passed. Frozen count hashes, thread/profile
parity 1--128, serial/4 byte-identical compile, fact-oracle tests and hidden
graph-input count test pass. Root `pancollapse_direct_facts_20261101_UKcX1N`
records inputs; no empirical job yet. Next: full compile <=128 threads under
300e9-B/no-swap cap, quantify size/resources, then frozen-1M exact N comparison.
No whole launch/speed/F1/full-genome claim.

**Historical N ready/profile snapshot checkpoint (2026-11-01):** N native ready in
238.36 s (index 10.0791, XG 14.3376, RI 77.9083, GBWT 227.785; combined index
load 232.472). At 00:58:34Z it was live at ~90,771 groups/90.005 read seconds,
with 144,729,862,144-B peak; parity was then pending. External flat profile
`N/cpu-live-flat.jdqg24` passed: helper `56571969...` exited 0/no OOM,
30-s/49-Hz `cpu-clock:u`, no stacks, ~165K samples, zero loss, record/report 0,
report stderr 0. Self snapshot: select 20.51%, locateNext 12.16%, packed-get
primary 11.46%, resolver 10.18%, node-add-path 9.22%, path-row 5.20%, model
2.32%, model-ids 2.06%. Do not sum shared helpers into exclusive stages.
Profile overhead plus W/sorter concurrency prevents isolated speedup claims.

**Historical N matched-window performance checkpoint (2026-11-01):**
`N/cpu-live-flat.jdqg24/runtime-cost-checkpoint.json` records 165,127 all-symbol
samples; 2,741 named ZSTD samples, including compression 1.6599%. On matched
first 300k groups, G was 204.721 s (~1,465.409/s) and N 249.291 s
(~1,203.413/s), 17.8787% lower—not halved. Old complete-run versus new cold-
prefix averaging was misleading. Approximate 100k--300k 5-s samples show G
125.726 mean CPUs/N 113.117; CPU seconds G 15,087.106/N 17,080.740. Host has
128 physical/256 logical CPUs; live Docker snapshot N 10800.69%, W 12031.13%,
sorter 123.10%. Do not attribute this only to contention or exclude regression.
Rank/select, FastLocate and packed access remain distinct from generic ZSTD; no
index/cache choice follows. N was live at 400k/320.04 read s with parity pending
at that snapshot; the accepted terminal result above supersedes its live state.

The completed reviews support only the implementation boundary recorded in the
final D110 entry; root review is required before edits.

### Analysis handoff: bounded independent whole-genome comparison design (2026-11-01)

The whole-genome native-count objective is complete, not a new biological
conclusion: R7 counted the real PBMC1k v3 GAMP to EOF (66,601,887 read groups,
166,824,266 records) in 6,372.41 s with kernel HWM 245,153,902,592 B. The
terminal receipt records exit 0, no OOM/swap/spill, independent Parquet and MEX
readers passed, and exact R6 four-Parquet/six-MEX byte, decoded/logical-table,
profile-contract, and barcode-prior parity. The frozen `cr7-v1` and
`pansc-strict-v1` outputs, source archive, binary, receipts, and profiles are
the comparison inputs; this is not read-level F1, biological truth, external
Cell Ranger whole-workflow equivalence, or a linear-arm result.

User authorized a separate analysis environment at
`/mnt/ssd/lalli/pancollapse-wg-analysis-20261101` (not yet created), containing
only a roughly 335-MB copy of the validated count outputs plus receipts, with
its own Docker environment. Do not copy GAMPs or indexes, change PanCollapse
source/optimizations, tune annotation or counting policy, make cell-filtering
decisions, run heavy analysis, or begin a new linear alignment. The older HG002
simulations remain distinct evidence from these current real PBMC1k v3 counts.

The next authorized action is a bounded initializer and comparison-design
record: establish the independent directory and provenance-bound count/receipt
copies, then specify a conservative comparison against a linear result only
when an equivalently frozen input and counter contract exist. The unresolved
question is what matched linear dataset/workflow, feature namespace, cell set,
and comparison metrics can support that design; no current record establishes
their match. Preserve the distinction between `cr7-v1` profile semantics and
external Cell Ranger equivalence.

**Subsequent authorization (2026-11-01):** the independent root now contains
the frozen R7 count/receipt snapshot at
`/mnt/ssd/lalli/pancollapse-wg-analysis-20261101/data/pancollapse/r7-20261101/`
and records its live gate in
[`PROGRESS.md`](/mnt/ssd/lalli/pancollapse-wg-analysis-20261101/PROGRESS.md).
User additionally authorizes a reference/command audit followed by two new
whole-genome, same-FASTQ STARsolo runs (GRCh38 and T2T), not a claim that a
matched linear comparison already exists. The raw STAR proposal is 2.7.11b,
`GeneFull_Ex50pAS` primary with `Gene` and `GeneFull` secondary, and later
separate common cell calling; exact reference/annotation namespaces remain an
open freeze gate. The comparison plan is root-owned at
`/mnt/ssd/lalli/pancollapse-wg-analysis-20261101/docs/comparison-plan.md`.
No Nextflow/BAM workflow, GAMP or index copy, PanCollapse source/count-policy
tuning, heavy analysis, or automatic goal transition is authorized.

**Subsequent run status (2026-11-01):** the independent analysis root now exists
with byte-verified, read-only copied R7 counts/receipts, locked analysis image,
and vendor raw/filtered H5 context; no GAMP or index was copied and no C++ source
or count policy changed. GRCh38 indexing finished exit 0 at 17:57:09Z and its
full raw STARsolo count is live; the saved command passed the explicit CR7 bundle
checker. T2T native-local indexing is live with the same 32-CPU/96-GiB/no-swap,
no-BAM envelope and its count queued; T2T command validation awaits count start.
Neither raw matrix is ready for analysis until full EOF and matrix checks pass.
Next work is monitor/validate both arms, then execute the existing comparison
plan under its stated annotation, coverage, and scientific limits—not a claim of
linear equivalence or biological superiority.

**Subsequent GRCh38 terminal status (2026-11-01):** GRCh38 now has
`STATUS=complete`; Docker records exit 0/OOM=false at 18:04:46Z, `Log.final.out`
records all 66,601,887 input reads, and the saved command passed the CR7-preset
checker. This is an operational terminal receipt, not independent matrix
validation or a scientific comparison result. T2T remains indexing with its
count queued. The next work is matrix validation for GRCh38 and terminal/matrix
validation for T2T, then the existing comparison plan.

**Subsequent T2T terminal status (2026-11-01):** T2T now has `STATUS=complete`;
Docker records exit 0/OOM=false at 18:21:39Z, `Log.final.out` records all
66,601,887 reads, and the actual command passed the CR7-preset checker. Its
independent raw-MEX validation is in progress. Both linear executions are
terminal, but common-caller, ID/barcode crosswalk, QC, and comparison work have
not started; no scientific result follows from completion receipts.

**Subsequent T2T validation status (2026-11-01):** T2T's actual command and all
three raw MEX matrices now independently pass their recorded checks. Both linear
arms are terminal and command/matrix validated. The remaining work is the
shared-core ID/barcode crosswalk and common metadata, followed by the planned
QC/calling and descriptive comparison; no biological or cross-method conclusion
follows from structural validation.

## Historical checkpoint: approved Stage 6 performance implementation; whole count stopped (2026-10-31)

The user amends the goal to full same-GAMP per-gene counts in under two hours
wall time and under 300 GB RAM, and directs stopping the current run to diagnose
its performance. Root sent SIGTERM to whole count PID 7 at 16:33:45 UTC, then
stopped its remaining wrapper container. Docker confirms exit 137/no OOM at
`2026-10-31T16:34:24.640829452Z` (user stop, not a memory failure), recorded in
`RUN.json`. Last measured whole milestone was
1,236,704 groups in 4890.05 s, 406,490,640,384-byte peak and
391,276,007,424-byte pre-stop current cgroup memory, with max/oom zero. No
final EOF or counts exist. The observed rate is about 253 groups/s versus about
9,250 groups/s required for two hours (~37x gap). These are measurements, not a
diagnosis. The subsequent user approval authorizes semantics-preserving
implementation and bounded tests; a whole restart still requires supporting
performance/correctness evidence. No extra large-input hashing is authorized.
The Stage 3 control independently reached genuine EOF (25,000
records/10,000 groups), exit 0/no OOM at `2026-10-31T16:37:50.15274514Z`.
Processing/wall/user/system were 13672.3/13842.887/43214.267/306.462 s, with
14,004 count and 14,095 molecule rows. Its resolver route/model/state totals
match Stage 4 expectation. Existing-file three-way comparison passed
(`first10000.three-way-byte-parity.json`): Stage 3 versus Stage 4 and Stage 3
versus Stage 5 have byte-identical four Parquet tables and six compressed MEX
files, equal both-profile definitions/logical tables/global table logical hashes,
and equal scientific summary counters except `assignment_cache_` and
`aggregate_spill_`. Stage 3's independent reader was not rerun; Stage 4/5 were
already independently validated. This proves only the genuine-EOF 10k existing
outputs, not full genome, F1, or release. All containers remain stopped. The
user approved a new semantics-preserving performance phase at 16:49 UTC:
consequential design/proof review of alias pre-deduplication, shared immutable
graph facts, and DP factorization, then bounded implementation/tests. This
changes no biology (D105 no ordered containment; D107 wrong-way splice veto;
raw Parent exon/body separation), frozen profile/barcode/UMI/output invariant,
or no-BAM/no-spill/no-hash/<=128-core boundary. Existing three-way 10k parity
is the baseline; a whole count may follow only if bounded synthetic/real
heavy-tail evidence supports <2-h wall and <300-GB RAM. No 37x promise is made.
New production runs are capped at 300,000,000,000 bytes/no swap; existing frozen
artifacts and 500-GB historical receipts are preserved.

Stage 6 alias/cache work is implemented after parent review and Sol bounded
architecture/proof acceptance, with no biological-policy change. Focused 3/3
native-query/resolver/cache tests passed in 1.02 s and full 198/198 passed in
31.92 s (`build/ctest-alias-cache-full.log`); frozen binary/source/receipt are
`panCollapse-alias-cache`, `source-alias-cache.tar.gz`, and
`ctest-alias-cache-full.log`. Conservative keys (policy plus optional fixed
support ID) preserve malformed-source faults; per-sink E/P/B vectors cache once
with logical multiplicity retained. Shared immutable leaf singleflight has
8+8-GiB source bounds and checked lease/charge/cleanup diagnostics. The
authorized same-10k gate is live in `pancollapse-native-alias-cache-first10000-20261031`
(`310171b9a5f62a5d12d3f3cf2f7873a7795ff751bf3aa6492e24308d8ef654a8`),
started 17:11:14.932 UTC under 300 GB/no swap/128 CPUs; native ready was
164.257 s and at 600 s it was 9,997/10,000 groups, 8.8315B states, and
150,791,712,768-byte peak. It is a gate, not a performance claim. The frozen
1M-QNAME source prefix exists (2,502,433 records, 465,121,668 bytes) but has not
run; no whole launch is authorized before 10k and larger-fixture evidence. The
mapper command used `-M10`, which caps records rather than represented GAMP DAG
routes; counting traverses existing alignments without remapping or
full-reference-walk containment. Next authorized work is read-only review of
minimal exact GAMP-branch computation before container-only optimization; no
top-K/hard cap/score pruning, biological/profile change, or unsupported speedup
claim is authorized. Sealed source/statistics record 167.66-s ready time,

**Route-scoring clarification:** The user describes wrong-way-splice exclusion,
per-alternative node-score transcript exon/body tally, and argmax; D091 agrees
selection is max over (route, transcript), never a sum across mutually exclusive
alternatives. Compact `RouteTally` max-prefix DP is distinct from richer exact
Ex50/support-state DP; the latter was previously overstated as scientifically
necessary. Frozen CR7/custom parity remains unchanged. No cap, pruning, or new
counting mode is authorized. The pending user choice is whether whole counts use
a separately named node-score argmax mode with frozen profiles retained
separately, or exact frozen CR7/custom parity. Stage 6 binary/gate is unchanged.
`first10000.route-complexity.json` reports 25k records/10k groups/max 10 records,
340 records with >1000 structural routes, maximum 14,875,194,015,048,794,112
routes and 44,989 subpaths; structural routes are not distinct biological
alignments. `inspect_gamp_routes.py` passed four toy assertions; no production
change or hash was made.

**CountOutcome performance API:** Sol conditionally approves, under the existing
performance authorization, a typed CountOutcome-only API (terminal, barcode
correction eligibility, UMI-filter reachability, genes) consumed only by
prepared-count/cache; full `resolve()` facts/reasons/oracles remain unchanged.
Prepared count currently discards facts/reasons and runtime counters do not use
them. Outcome DP may merge only the specified reduced state while retaining exact
body-support sets, anchor, last-node/end, splice/wrong flags, alias, and policy;
same-key higher score dominates only when final count outcomes agree for both
frozen profiles. No score-window/strand/tier/body/nesting rule changes. A
conservative checked no-graph score certificate otherwise falls back unchanged
to full resolve. Reasons/facts/coordinates are not exposed by the new API, so
their internal work/order metrics are not preserved claims; consumed outcomes,
final counters, and tables must match. Focused/full, adversarial outcome parity,
and same-prefix EOF gates are required. Stage 6 benchmark binary/source remains
immutable and live (~1380 s, 9,997 groups, 150,791,712,768-byte peak); no full
launch or speedup claim is authorized.

**Authorized scoring replacement:** User supersedes the CountOutcome-only
proposal for default native scoring. For each candidate: correct library strand,
veto known wrong-direction splice alternatives, credit existing VG per-node
scores once to transcript exon/body memberships, never add complete GAMP
alternatives, classify each transcript/branch E=100%, P=50--<100%, B=<50% with
gene-body support, then retain per-class best scores/ties across routes/records
for downstream assignment and CB/UMI. D091 remains max over (route, transcript),
with no ordered-reference-walk containment or per-route body-support-subset
enumeration. Default native output must identify `node-score-v1`; frozen profile
definitions are not edited in place and legacy full resolve remains a frozen
oracle, not a claimed scientific-parity baseline. Whole-count goal is paused.

Known design-review pitfalls: official CR7 behavior uses STAR near-best/window
and per-splice-segment >=50% semantics; STARsolo Ex50 uses strict >50% and can
let antisense E veto outrank sense P/B. The user-selected whole-branch >=50%
and early strand discard can differ. Do not silently import those extras or
recalibrate windows. Three prefix maxima alone are unsound at merges; compact
margin/best-score envelopes are required. Parent review must settle evidence/
version identity before implementation; no new full test or run is authorized.

**Node-score clarifications accepted before coding:** Retain argmax and admit
within one additional mismatch inclusively: D068's +1/-4 means a pool-local
five-VG-point window, not literal one STAR point. Score both sense and antisense;
antisense exon evidence is a veto, superseding early sense discard. The separate
NodeScoreResolver has six independent E/P/B-by-orientation pools, per-transcript
max/ties, then the inclusive five-point window within each pool only (never the
old global cross-pool score window). Complete alignment routes, not exclusive
branch sums, are scored; P remains whole-route >=50%. Per-splice-segment CR7 is
requested expansion only, not replacement.

Sparse envelope keys are endpoint, exon margin/all-exonic, body seen, strand
margin, valid-orientation mask, and aligned state--not support sets. Exact ties
merge Parent provenance/support units from retained winning aliases. Node scores
credit actual mapped segments once, not reference-path multiplicity; placement
score remains separate and alone includes connections. Legacy full resolve and
`node-score-v1` manifest identity remain. New gene assignments do not claim old
parity. Complete potential path node sets are the unit; prefixes share work only.
No production fatal biological-validity checks are authorized; flat per-node
scorer certification is a reported pitfall, not default read rejection.

**Raw alignment-alternative cutoff:** User overrides deduplication: count all
complete alignment alternatives encoded in GAMP, duplicates included and summed
across records per read; use it as the cheap first filter. The concluded 10k
dedup fixture (166/10,000 or 1.66%, zero unknown) is retained as evidence but
is not the target metric. Prior raw structural-route 167/10,000 (1.67%) is the
relevant prior exact-10k receipt. Native simple scanner
`R/inspect_gamp_alignment_counts.cpp` is requested to produce exact `cpp_int`
histogram percentiles and >1000 drop fraction, with no rescoring, node/gene/index
work. Stage 6 pause receipt persists; development is running only for this
diagnostic. Whole goal remains paused; no biological policy, cap implementation,
or count restart is authorized.

**Raw-alternative diagnostic complete:** `total-alignment-diagnostic.validation.json`
independently validates raw complete routes per QNAME (duplicates included, no
rescoring/dedup): toy real GAMP 7 records/6 groups histogram {0:1,1:1,2:3,4:1};
10k 25k records has 167 >1000 (1.67%); first 1M has 2,502,433 records,
17,617 >1000 (1.7617%), and 19,787 zero. Nearest-rank P99/P99.9/P99.99 are
6912/59,719,688/334,430,208,000; literal low P1/P0.1/P0.01 are zero. Root's
independent Python recomputation agrees; `cpp_int` avoided overflow (maximum
15,303,008,032,705,444,785). The 1M scan took 60.2414 s; 333,632-KiB RSS is a
snapshot, not peak. No full 66.6M rescan or whole count occurred. Recommended
cheap guard is now authorized for bounded implementation: raw total complete
GAMP routes (duplicates included, summed records/QNAME) capped at default
64,000,000; strict greater drops the whole read before graph/annotation queries,
with no enumeration/dedup/rescoring. Proposed
`--max-alignments-per-read N` defaults to 64,000,000 and 0 disables, pending
parent CLI validation. Preserve global barcode prior over all valid groups.
This does not resume whole count or change a scientific profile. Required tests
cover capped overflow arithmetic, branches/connections/parallel/sources/
multirecords, equality N retained/N+1 dropped, no graph query on reject,
barcode-prior retention, and CLI/default/manifest/counters.

**Completed tunable raw-alignment cutoff checkpoint (2026-10-31):** The bounded
implementation and focused verification are complete. `src/gamp_alignment_complexity.hpp`
uses O(subpaths+edges) saturating capped arithmetic to count complete GAMP DAG
routes, including duplicates and all records with the same QNAME. Default
`--max-alignments-per-read 64000000` drops a read only when its total is strictly
greater; explicit `0` disables the cutoff. After the valid-barcode prior, the
guard runs before graph lookup, caches, or assignment in both prepared/native
and legacy direct-count paths. Each retained complete alignment remains
separately scored and classified; shared piece/prefix work may be reused, but
mutually exclusive alternatives are never summed. Rejections are terminal
`unassigned_high_complexity` with reason bit `1<<18`; summary and manifest record
the parameter and dropped counters. `convert` is unchanged. Legacy
compatibility-in/out requires explicit `0`, because its schema cannot represent
a dropped read; no schema expansion was made. The simplified node-score scorer
is not implemented, and this checkpoint claims neither improved performance nor
scientific parity.

Build succeeded (`build/build-alignment-cutoff.log`); focused CTest passed 10/10
(`build/ctest-alignment-cutoff-focused.log`); and full CTest passed 200/200 in
31.14 s (`build/ctest-alignment-cutoff-full.log`, exit 0). The three
mixed-configuration parity fixes set both comparison arms to explicit `0`
(cutoff disabled), retaining all semantic/output comparisons. The first-1M helper receipt
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/cutoff-guard-first1000000.validation.json`
reports 2,502,433 records/1,000,000 read groups, 982 drops above 64,000,000
(0.0982%), 17,617 above 1,000, and 57.3979 s wall time. It is a non-random
prefix, not full-genome evidence. Its exact histogram records 990,090 reads and
35,251,126 alternatives at <=7,000; 8,928 reads and 21,981,204,583 alternatives
at 7,001..64,000,000; and 22,016,455,709 retained alternatives. Thus the
0.8928% tail accounts for 99.839887% of retained alternatives, not measured
runtime. Earlier pending cutoff wording is superseded by this completed
implementation checkpoint. The whole-count goal remains paused; no production
launch is authorized.

**Superseded scientific-phase checkpoint (2026-10-31):** The prior
current-binary legacy-resolver 10k authorization is superseded by the user's
explicit simplified per-alignment scorer requirement; no run launched.

**Scientific-phase checkpoint correction — simplified scorer first
(2026-10-31):** The requested one bounded 10k count must use the enabled 64M
cutoff and simplified per-alignment scorer. The current tested binary has the
cutoff only and cannot fulfill the test. Implement and correctness-review that
scorer first; no old-resolver test is authorized. Then one count may use existing
`R/first10000.gamp` (25,000 records/10,000 QNAME groups/8,506,683 bytes),
unchanged native.pci/XG/GBWT/RI, `cr7-v1` plus `pansc-strict-v1`, 128 threads,
300,000,000,000-byte cap/no swap, and no BAM/spill/hash/remap/index build.
Fresh empty `R/cutoff-first10000-t128.gVysNL` may be reused after implementation.
Whole counts remain paused.

**Simplified scorer implementation and bounded-test gate (2026-10-31):**
`NodeScoreResolver` is implemented as complete-route explicit-stack DFS with
read-local node facts, per-piece sparse tallies, and push/pop prefix reuse;
there is no sequence realignment, ordered reference walk, or subset-state DP.
Node scores use existing flat VG edit scoring; E/P/B uses consumed reference
bases, including deletions. Maxima are per transcript/pool with only exact-tied
alias policy/support payloads, then inclusive five-point independent windows.
Known wrong-direction splices veto candidates without an endpoint-containment
gate. Six-pool ranking and the existing downstream profile-specific Gene
fallback are separate; CR7 has no such fallback. All-vetoed shadow evaluation
preserves molecule-stage eligibility without restoring gene calls.
`--assignment-scoring node-score-v1` explicitly selects this engine; the legacy
default remains available as the frozen oracle during validation. Output records
the algorithm/window. Final build passed (`build/build-node-score-final.log`),
and full CTest passed 201/201 in 31.43 s (`build/ctest-node-score-full.log`).

**Authorized bounded structural diagnostic (2026-10-31):** The user authorizes
read-only inspection of existing first-10k and first-1M GAMP only, not a
production filter/rescue/argmax change. A standalone degree/route-count group
diagnostic will report outer-record histograms conditional on per-record
complexity >7,000, >1M, and >64M; nonempty remaining-record counts after
removal; and totals checked against 25,000 records/10,000 groups and 2,502,433
records/1,000,000 groups. No remap, hash, graph load, or full scan is allowed.
Validated raw-sum drops remain 14 (10k) and 982 (1M). Preserve the current 10k
container and frozen binary; 9,989/10,000 at 510.007 read seconds is not EOF.
Smaller surviving records do not prove winning-gene safety because a discarded
record can contain the best or competitive gene. Whole count remains paused;
parent review precedes execution.

**Authorized minimum-record-complexity rescan (2026-10-31):** The per-read
sum histogram cannot answer minimum record-route-count percentiles. The user
authorizes `R/inspect_gamp_record_complexity.cpp` to rescan existing first-10k
and first-1M GAMP only, reporting per-read minimum nonempty record complexity
(zero when all records are zero), literal minimum over all records to expose
zero-sibling effects, weighted nearest-rank percentiles on the same 1M
denominator, and zero/mixed-zero counts. Validate input records/groups and
known summed-drop counts (14/982). No graph/remap/hash/filter/production change
is authorized. Root may compile standalone in the existing dev image, without
shared Ninja, in isolated 1--2-core/2-GiB diagnostic containers; the production
10k test remains untouched.

**Minimum-record-complexity diagnostic complete (2026-10-31):** Standalone
compilation reused known CMake link flags inside DEV (no Ninja); DEV is stopped
and the count producer is unchanged. Toy 9-record/7-group checks passed:
min-any {0:2,1:2,2:3}, min-nonempty {0:1,1:2,2:4}, zero-only 1, mixed 1.
Receipts `R/first10000.min-record-routes.json` and
`R/first1000000.min-record-routes.json` validate 25,000/10,000 and
2,502,433/1,000,000 records/groups plus raw-sum >64M/>1000 counts 14/167 and
982/17,617. The 1M scan took 57.1735 s in an isolated 2-CPU/2-GiB/no-swap
container. Both minimum histograms: zero-only 19,787, mixed 0; weighted
nearest-rank P50/P90/P95/P99/P99.5/P99.9/P99.99/P99.999/max =
1/4/8/64/160/9216/1990656/2388787200/149593924085760; strict-above counts =
447441/85651/42006/8659/4978/980/98/10/0. Min >64M is 39/1M. Of 982 raw-sum
failures, 943 have a <=64M nonempty record and 39 do not. Records >7k affect
9,447 reads (6,055 with ten records, 1,352 with 1--2; 8,332 with a <=7k
sibling); >64M records affect 960 (711 with ten), while 22 additional raw-sum
failures lack an individual >64M record. This supersedes the older failed-attempt
wording. Minimum record complexity does not establish a safe winning or
competing gene and authorizes no policy change.

**Authorized first-1M sibling-winner diagnostic plan (2026-10-31):** Before
execution, analyze only
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/first1000000.gamp`.
For reads whose least-complex GAMP record has 9,000--10,000 complete routes
inclusive, report the fraction whose argmax-scored alignment occurs in a
more-complex sibling record. Keep two results distinct: cheaply exact per-record
maximum VG alignment score via DAG DP, and transcript-aware `node-score-v1`
argmax. For each, report ties separately from complex-only winners; neither
proves a safe rescue or policy change. This is a bounded structural/scoring
diagnostic, not a production count. The current 10k node-score/cutoff gate stays
untouched and running; whole count remains paused.

**First-1M sibling VG-score diagnostic complete (2026-10-31):**
`R/sibling-vg-score.validation.json` PASS validates the bounded global
source-to-sink VG-score diagnostic. The 9,000--10,000 minimum-record cohort has
43 reads/45 records: 42 single-record and one three-record read. The latter's
records are 9,140 -> 51, 121,440 -> 46, and 922,200 -> 60, yielding one
complex-only winner (1/43 = 2.32558%; 1/1 with a complex sibling), gap 9, and
zero ties. All 43 pass the 64M summed-route guard; inferred starts are zero.
The 1M scan took 57.925 s in a 2-CPU/2-GiB container. Annotation-aware
`node-score-v1` comparison was not implemented or launched and remains pending.
This diagnostic intentionally uses global source-to-sink score; vg's default
`optimal_alignment_score` is local subpath-to-subpath DP. Normal VG ranking can
use DAG DP without route enumeration, whereas current `node-score-v1` enumerates
routes. The existing 10k benchmark is untouched and whole count remains paused.

**Authorized exact sparse node-score max-DP implementation phase (2026-10-31):**
Reconciled on `fix/whole-genome-count-repair-20261028` at `327b048`; existing
dirty state is preserved. User authorizes replacing `node-score-v1` complete-route
DFS with sparse exact max-DP. Preserve every E/P/B-by-strand pool and its
inclusive five-point window, raw-model coherence, directed-splice masks and last
covered endpoint, projected alias/evidence identity, body-support metadata and
separate body-fallback score, then the same assignment/CB/UMI/output behavior.
No new biology, ordered containment, spill, remap, or production launch is
authorized. Existing exact counters/oracle and live 10k gate remain historical/
live verification, not altered by this authorization. Required next evidence is
bounded DP tests followed by the same 10k gate before any larger test; whole
count remains paused.

**Pre-real-10k sparse-DP checkpoint (2026-10-31):** Final sparse DP is 439
lines with a frozen 354-line oracle. `NodeScoreExecution` test controls cover
automatic, force-sparse, and enumerate-oracle modes; exact keys retain
independent A/G maxima, same-TX operation grouping, support/alias restoration,
fixed native-body support skipper state/payload, and metrics for represented
routes, DP states/transitions, max frontier, groups, and offset conflicts. Sol
conditionally accepted the design without a blocking flaw. Expanded tests pass:
256 seeded small DAG full-facts/results three-mode comparisons, adverse cases,
and a >=2^32-route DP-only case with <1,000 states. Full CTest passed 202/202
in 31.05 s with `PYTHONPATH=/mnt/ssd/lalli/.local/lib/python3.11/site-packages`
(`build/ctest-node-score-dp-full.log`); the first full attempt's `umi_tools`
import environment failure required no production-code change.

Frozen binary/source archive, copied CTest receipt, and runner are ready at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-dp-first10000-t128.UxhZc8`.
The then-next authorized phase was pending launch, not live: same
`first10000.gamp` (25,000 records), both profiles, 64M cutoff, 128 threads,
300,000,000,000-byte/no-swap, no BAM/spill/hash, Parquet+MEX, independent
validation, and baseline table/logical/byte/scientific-counter comparison only
after `cutoff-first10000-t128.gVysNL` reaches EOF. That original remains at
9,999 groups after >3,000 s. Whole launch remains paused.

**Sparse-DP same-10k terminal result (2026-10-31):**
`pancollapse-node-score-dp-first10000-20261031` launched at
`2026-10-31T19:43:41.682094185Z` (container
`6599e6ed92b8b5439b8d19b541e9f31e97263331eee3019e7cf22e89d1686211`) with
300,000,000,000-byte memory and memory+swap limits (no swap) and 128,000,000,000
NanoCPUs. It reached EOF: 25,000 records/10,000 groups, 14 cutoff drops,
13,904 count rows, and 13,995 molecules; `validation.json` passed. Native count
exit was 0. Manifest initialization/read/processing were 154.724650670/
202.183747006/202.281111332 s; shell wall was 377.946674 s including cleanup,
or about 49.46 read groups/s on the inclusive 10k denominator. Cgroup peak was
166,680,592,384 bytes with no OOM; sampled RSS was 176,101,588,992 bytes.
The wrapper exited 127 because post-count validation invoked bare `python3` not
on PATH; outputs were already published, frozen runner is unchanged, and the
independent validator reran via explicit
`/mnt/ssd/lalli/.linuxbrew/opt/python@3.11/bin/python3.11` in a separate
2-CPU/2-GiB/no-swap Docker container with exit 0. The original explicit
enumerator baseline remains live at 9,999 groups after >3,600 s, so table/logical/
byte/scientific-counter equality is pending. Full CTest remains 202/202 and DP
differential facts/results match the oracle on 256 seeded small DAGs plus adverse
fixtures. No CR7/legacy F1, two-hour-genome, or promotion claim follows. Pending
cross-TX grouping status is superseded by the next checkpoint. Whole count
remains paused.

**Pre-real-10k final cross-TX quotient checkpoint (2026-10-31):** The exact
quotient in `src/node_score_resolver.cpp` is implemented and Sol final review
passed without correction. It preserves complete-operation/fixed-support/full
sparse per-local-edge two-bit-mask grouping, raw TX/policy/support/winner
restoration per alias, force-sparse singleton, and original enumerator. A pure
performance guard skips cross-edge closure when all piece deltas are empty,
preserving route/offset telemetry and outputs. Focused passed 1/1 in 0.20 s and
full passed 202/202 in 32.57 s (`build/ctest-node-score-behavior-full.log`),
including 64 cross-TX seeded cases atop 256 existing: absent-vs-mask3,
fixed-vs-variable support, bridge veto, exact different policy/support
restoration, and group reduction. Frozen binary/source/logs/runner/receipt are
at `/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-behavior-first10000-t128.gISKSl`.
Launch is pending, not live: same 10k, profiles, 64M cutoff, 128 threads, 300 GB
no swap, existing indexes, no BAM/spill/hash/remap/index build; independent
validation and automatic ten-file byte/logical/scientific-counter comparison
against completed first-DP output are required. The old enumerator remains live.
Comparator receipts are future `R/node-score-enumerator-independent-validation.json`
and `R/node-score-dp-vs-enumerator.parity.json`; neither is passing evidence.
Whole genome remains paused.

**Cross-TX quotient same-10k terminal gate (2026-10-31):**
`pancollapse-node-score-behavior-first10000-20261031` started at
`2026-10-31T20:00:51.074088999Z` (container
`416becaaf21aa5160bd02c0d48a54194513d1e382e05ca5c6f9a4c93c4563eae`) with
verified 300,000,000,000-byte memory and memory+swap limits and
128,000,000,000 NanoCPUs. It exited 0/no OOM at `2026-10-31T20:05:57.313310042Z`
after EOF on 25,000 records/10,000 groups: 14 drops, 13,904 count rows, and
13,995 molecules. Independent reader passed; all ten outputs are byte-identical
to completed first-DP output with equal profile definitions, logical identities,
and scientific counters (cache work excluded). No F1, CR7/legacy, or production
claim follows. Init/read/processing/wall were 150.430731581/128.025707080/
128.138611337/303.977775 s; read rate 78.1093 groups/s. Sampled RSS/cgroup peak
were 167,081,734,144/157,636,481,024 bytes, no OOM or spill. Model groups and DP
states were 158,583 and 1,258,500,451 versus first-DP 8,784,273 and
4,376,860,251 (55.39x/3.48x fewer); count phase was 1.58x faster, while ~150-s
initialization is unchanged. Raw-model transfer work remains 629,485,930, so a
two-hour genome target is unproven. Full 202/202 and 320 seeded small-DAG
differentials/goldens pass. The original enumerator remains live at 9,999 groups
after >4,530 s; the background comparator awaits EOF, not full-real oracle
parity. Next safe work is larger-fixture/performance evidence after its own
checkpoint; whole genome remains stopped.

User redirected to per-read percentile cutoffs: reuse
`R/first1000000.total-alignments.json`, whose metric already sums all raw DAG
routes across each QNAME's records. No new scan is needed. Weighted nearest-rank
P50/P90/P95/P99/P99.5/P99.9/P99.99/P99.999/max are
3/24/67/6912/118672/59719688/334430208000/681661459881772/
15303008032705444785, over exactly 1M groups and 2,502,433 records.
The separate conditional-record diagnostic did not run (isolated compile
environment failures before input open); its source exists but no result receipt
does. Do not claim a conditional complex-read record distribution is measured.
The authorized one-10k test is now launch-ready at
`R/cutoff-first10000-t128.gVysNL/run.sh`, explicitly selecting node-score-v1,
64,000,000 cutoff, both profiles, 128 threads and 300 GB/no swap. This is not a
whole-count resumption or an old-assignment parity claim.

The bounded count launched at `2026-10-31T18:47:25.587391629Z` in
`pancollapse-node-score-cutoff-first10000-20261031`
(`b75cf2019b406bcf588daef45127c1be35edd0d391a9c80ce59c207e4b557b69`).
The test root contains its copied executable, exact `run.sh`, terminal CTest
log, `run-receipt.json`, count log and five-second resource samples. Sol's final
bounded review found no remaining correctness blocker; it explicitly did not
approve whole-genome production or old-assignment parity. The build container
was stopped once idle. Counting/output validation and throughput remain pending.

Latest bounded-test milestone: 9,987/10,000 QNAME groups completed at 300.004 s
of read processing (~33.3 input groups/s, provisional; 13 high-complexity reads
remain). Initialization was 155.803 s; observed cgroup peak is 132,202,708,992
bytes, no OOM. The early 9,118/30-s rate (~304/s) is not final throughput.
Existing exact fixture histogram predicts 14 above-cap reads and 239,882,441
retained alternatives; the filter does not make the retained tail cheap.
There is no per-alignment/per-read sink: count/resource logs total 30,908 bytes
at this milestone (118 count-log lines/17,817 bytes and 97 resource lines/13,091
bytes). Large log numbers are aggregate counters, not serialized alignments.
Continue this same frozen 10k run to EOF and independent output validation;
do not change its cutoff, expand the fixture, or launch whole counts.
about 124 busy cores and >99.5% CPU user, 105.08M native queries/95.43B
occurrences, 5.684B raw-model visits, and 537.51B DP state cells in per-model/
behavior `std::map<StateKey,State>` traversal. Actual collector visits are
431,860,084,428 (=1,995,995,637,646 raw logical minus
1,564,135,553,218 counter-only avoided), comprising 428,975,326,656 memo hits
plus 2,884,757,772 materializations: 99.332% rediscover existing candidates
after alias/profile expansion. Whole-signature cache hit rate was 18.9% at its
512-MiB bound. Near-ready cgroup memory was 73--76 GB versus 406.49-GB peak;
exact heap/CPU attribution is unmeasured. No remedy, semantic change, or
implementation is authorized by this diagnosis.

Stage 5 A/B/C is implemented and parent-reviewed against frozen Stage 4. The
cache now charges its full string key plus approximate map/control/bucket and
result payload globally within 512 MiB, rather than charging values alone;
legacy insertion-only default, outside-lock builders, full equality, and
observations are unchanged. Focused `native_count_query_test` passed 1/1 in
1.00 s and full 198/198 passed in 32.40 s
(`build/ctest-cached-inline-full.log`, copied as `ctest-cached-inline-full.log`
to the run root). Native CLI legacy parity passed finite duplicate-evidence,
different-CB/UMI-quality, featureless, wrong-strand, and uncorrectable cases;
headers cover full-key mutations, support semantics, concurrency, collision,
oversize, and failures. Frozen artifacts are `panCollapse-cached-inline`
(114,174,464 bytes) and `source-cached-inline.tar.gz`.

The same-prefix 128-thread Stage 5 real-10k gate began at 14:28 UTC in
`pancollapse-native-cached-inline-first10000-20261031`
(`173f144717edcef1ff01a227163f9ca4a8936ab5a37fccbc13b29eabfbf4f30c`),
with existing native.pci/XG/GBWT/RI, same profiles, 500 GB/no swap; index load
took 4.28836 s. It reached genuine EOF (25,000 records/10,000 groups), exit
0/no OOM at `2026-10-31T15:24:07.078856015Z`; independent reader passed
(`cached-inline-first10000-t128.validation.json`). All four Parquet tables,
logical hashes, MEX, and scientific counters exactly match Stage 4
(`first10000.cached-inline-vs-transfer-parity.json`): 14,004 count and 14,095
molecule rows; ready/read/final 159.204247955/3141.699891474/0.465537699 s,
wall 3318.905 s, CPU 13121.952 s, system 247.774 s, peak 132,201,291,776 bytes.
Cache recorded 9,382 entries, 618 hits, zero uncached, and 159,451,941 charged
bytes; resulting work-counter reduction is actual permitted hit work. Stage 3
remains live at 9,999 groups after about 9300 s without EOF. Stage 4 is now a genuine 10k EOF: 25,000 records/10,000 groups, exit 0/no
OOM at `2026-10-31T14:51:30.834648311Z`; independent reader validation passed
(`transfer-first10000-t128.validation.json`) with 14,004 count and 14,095
molecule rows, ready/read/final 154.653820861/4997.695011896/0.810682403 s,
wall 5171.601 s, CPU 18636.597 s, and 132,201,127,936-byte peak. It is evidence
only until Stage 3/4/5 exact comparison. No whole count, F1, or release result
is established.

After parent review and fresh preflight, frozen Stage 5 full count launched at
`2026-10-31T15:09:26.107895372Z` in
`pancollapse-native-whole-cached-inline-20261031`
(`7648d97ab46b21c9cbd18c68b5b4830550a0ae8b29d40db4c2d37355311251a6`),
host init PID 1230154. It uses frozen `panCollapse-cached-inline`/source,
unchanged `run-count-only.sh`, native.pci/XG/GBWT/RI/full GAMP, 128 threads,
500-GB memory and memory-plus-swap/no-swap limits, default Parquet/memory-only
output, and no BAM/spill/hash. Preflight observed MemAvailable 691,667,886,080
bytes (>=660 GB), 256 CPUs/no swap, 11,916,846,014,464-byte disk, and absent
destination/staging; control PID/executables were unchanged. Ordinary index/XG
loads took 4.87111/6.76435 s; RI/GBWT took 38.0166/158.238 s and pre-read
native queries were zero. It has streamed 230,095 groups at 750.008 s
(`2026-10-31T15:24:53Z`), with about 279-GB cgroup peak/no OOM and host
MemAvailable about 503 GB after the Stage 5 control freed. Resource sampling is
active (`whole-cached-inline.resources.tsv`).
**Historical live-monitor milestone:** At `2026-10-31T16:31:10Z`, the full container was genuinely live at
1,200,000/66,601,887 groups after 4734.65 s (PID 7, 128 threads, 500 GB/no
swap). Latest cgroup memory values are 390,104,215,552 bytes current and
406,490,640,384 bytes peak; all `memory.events` max/oom/oom_kill are zero, and
host MemAvailable is 394,150,000 KiB. This is not EOF, finalization, or
scientific acceptance. Stage 3 control remains live at 9,999 groups after
13260.1 s on one active CPU core, with no EOF. The Stage 4/5 exact 10k receipt
is therefore only partial of the three-way gate. The batch-streaming 65,536-row
`validate_counts.py` is reviewed ready for full EOF. The only stopped unused
container was idle developer `3d21f8a16391` at 15:33:50 UTC (tail process only;
no files deleted); the required full and old-parity controls remain active.
Restart that developer container only for validation, then stop it again. Only
the full count and required Stage 3 control are active. The user update was sent
at 16:31 UTC (the earlier 16:11 marker was prepared, not sent); next update is
not before 17:01 UTC unless an exception occurs.

`counts/` is PROVISIONAL_UNACCEPTED: do not publish, interpret, or use it until
the exact Stage 3/4/5 10k and independent full-output gates pass. Stop first on
parity mismatch, source fix, OOM/limit pressure, or MemAvailable approaching
100 GB. A read-only memory audit finds NativeSource TLS caches clear per group
and fixed references; read-proportional completed-name plus exact/deferred-UMI
aggregates and DP/allocator high-water are plausible RSS sources, not proof of
a leak. No source fix, semantics, or new experiment is authorized. No scientific
acceptance or whole-genome completion is claimed.

**Historical Stage 4 implementation/early-progress note:** Stage 4 read-local transfer and terminal projection passed focused 1/1 in
0.91 s and full 198/198 in 31.06 s; frozen binary/source/receipt are
`panCollapse-transfer`, `source-transfer.tar.gz`, and `ctest-transfer-full.log`.
Its 128-thread 10k control is at 9,997 groups after 1320 s versus frozen Stage
3's 9,997 at 3630 s, with the same completed-model/route/state-cell/logical-
emission/retained totals at that milestone and 132-GB peak. Neither has EOF or
output parity, and whole count is not restarted. Source review confirms the
original fixed-reference assignment cache is bypassed by
`prepared_count.cpp:258` (`resolver.resolve`) and `main.cpp:3160`; its legacy
256-shard/655,360-entry cache remains unused, native cache metrics are zero,
and TLS node/edge caches clear per group.

Root authorizes bounded A/B/C implementation, not gate or whole-launch
acceptance. A restores the fixed-run/profile successful-final-assignment cache
before Source queries, with a full serialized read-evidence key (record bounds,
starts/sequence lengths, subpath Position/Edit/score/next/connections; no
QNAME, contents, or quality), the reusable sharded cache capped at 512 MiB and
655,360 entries, full equality, outside-lock builds, and no disk/CLI. B changes
numeric support storage to inline `small_vector<uint32,1>` flat set and inlines
numeric-memo vectors while preserving exact ordered values. C uses compact
automatic-map `try_emplace` and successor `lower_bound` with existing full-key
and strict score-only maximum behavior; checked-add remains first. Observations
always execute on hits and `begin_read_group` remains first; all-raw,
forced-generic, and raw-serial remain unchanged. Source/binary, focused/full
tests, exact complete-10k output gates, and the no-EOF Stage 3/4 controls remain
distinct; cache work metrics may decrease only for actual saved work. A bounded
detached GDB snapshot confirms map find/iteration and set-clone allocation, not
percentages.

Stage 3 implements full-literal, per-profile four-pool candidate interning and
success-only numeric memoization in the automatic native route
(`stable_unique_memoized`); legacy `all_raw` is unchanged. Sol gives conditional
source acceptance. The final all-15-override and novel-profile suite passed
198/198 in 30.80 s (`build/ctest-native-interned-final.log`). The 16-thread real-100
control exited 0/no OOM at `2026-10-31T12:46:32.549458304Z` and exactly matches
frozen-100 in Parquet, MEX, scientific counters, and logical hashes
(`first100.interned-parity.json`): read 1.545567871 s, ready 144.149925717 s,
final 0.043175297 s, 156 count and 156 molecule rows. Its independent reader
passed (`interned-first100-t16.validation.json`), with sampled max RSS
131,736,555,520 bytes and cgroup peak 132,202,790,912 bytes. The 128-thread interned 10k control
`pancollapse-native-interned-first10000-20261031`
(`ff854b5a7742bb5972f2edb331ad04dca3da37851555a9879268db5a046cf655`) is live:
at 90 s, 9,981/10,000 groups and 128-GB RSS, with no EOF yet. A compact
raw-model-serial 10k 16-thread diagnostic ran as
`pancollapse-native-interned-raw-serial-first10000-20261031`
(`66fd5937483da7e0c7dea4638993fe6b5368a7999ff14fa47d1d0c9ed97d6a8b`),
and was stopped incomplete after the explicit internal-count oracle amendment:
SIGTERM at 13:12:56 UTC, exit 143/no OOM at 13:13:05.029 UTC, wall 1319.282 s,
sampled peak RSS 131,596,333,056 bytes. It produced no count outputs. The
prior grouped-jemalloc 10k attempt exited 143/not OOM at
`2026-10-31T12:20:36.087636348Z` after 307.687 s and RSS 377,611,792,384 bytes.
No whole-genome count, raw-serial 10k oracle parity, Python/F1, or release
result is established. For a provisional internal both-profile count only, the
accepted oracle is complete frozen Stage 3 automatic 10k EOF, frozen with its
binary/input-prefix manifest and payload validation/index identity/profiles/
config. Stage 4 must independently reach the same true 10k EOF and match all
four Parquet tables, logical hashes, MEX, scientific terminal/CB/UMI counters,
and route/model/state-cell totals while staying below 500 GB and operationally
viable; only query/memo/materialization and timing/RSS metrics may differ. This
does not waive raw 10k, Python/F1, or release certification, and accepts a
residual latent Stage-3 grouping-risk anchored only by frozen real-100 and
all-raw unit tests. No other large pilot is required. After these gates,
`run-count-only.sh` may launch a provisional internal count using the reusable
9.77-GB index and XG/GBWT/RI under 500 GB/no swap/up-to-128 threads,
memory-only default-Parquet output, no spill, BAM, or hash.

The strict 10k interned control fits at about 120--132 GB but remains
computationally tail-bound (9,987 groups at 390 s, about 1.4 billion state
cells, 4.54 billion raw terminal emissions, and 24.4 million retained). Sol has
accepted a source proof for a minimal read-local subpath-transfer delta cache
and sink-local stable-unique terminal projection. Root reviewed this checkpoint
and authorized bounded implementation at 13:09 UTC; code and new gates are
not yet accepted. Both retain all-raw/forced-generic/raw-serial traversal as
oracle, raw pairs/facts/emission multiplicity and biological/profile policy.
The required tests cover orientation, node-end/same-node/body-edge, empty/
insertion/repeat/support-union, multi-sink/score/unknown-edge/source-fault,
and collision paths, with raw facts, compact facts/full results, and state-cell
counts compared to frozen Stage 3. Full 198 and complete 10k output comparison
remain mandatory before any whole-genome launch.

Stage 2 now executes one independent read-local behavior group at a time,
preserving every raw-model candidate and original sink/model/StateKey order.
The old all-model map remains the forced small-fixture oracle; a separate
raw-model-serial control disables grouping and the chain fast path. Complete
ordered facts/results match for automatic, raw-serial and forced hash-collision
execution, including fixed/mixed support, D107, multiple records/sinks/sources,
bridges, scores and single-fault errors. GBWT metadata-clear API equivalence
also passes. Consequential source inspection found no semantic blocker.
The complete Docker suite passed 198/198 in 30.24 s
(`build/ctest-native-serial-group-full.log`), and the restored automatic build
passed the focused seven-test closure in 5.72 s.

The run root freezes `panCollapse-serial-groups` and
`panCollapse-raw-model-serial` (112,283,952 bytes each), their distinct source
archives, and `ctest-serial-group-full.log`. The raw-serial binary was made by
temporarily selecting that resolver in the prepared counter constructor;
production source was restored to automatic and rebuilt. No public test flag
or default policy was added. A frozen 100-group actual-EOF baseline has
completed and passed independent Parquet/counter validation (209 records,
156 count rows and 156 molecule rows across both profiles, no OOM).
Its 100 groups retain exactly the source protobuf payloads. Optimized and
raw-serial 100-group controls finished in independent output paths; their
parity passed: all four Parquet tables, logical hashes, MEX content
and scientific counters match the frozen baseline exactly
(`first100.stage2-parity.json`). Both new controls exited 0 without OOM and
passed independent reader checks. Their sampled peak RSS was about 131.9 GB;
the optimized generic state-cell count was 23,323 versus 7,218,810 for the
ungrouped raw-serial control. This is a small-real-prefix observation, not a
sustained whole-genome speed claim. The strict 10k raw-serial control was
SIGTERM-stopped at 12:14:11 UTC as RSS reached about 409 GB and completion
stalled at 3863 groups. It exited 143/OOM false at 12:14:59.802 UTC after
530.260 s; sampled peak RSS was 443,453,325,312 bytes and cgroup peak
435,976,323,072 bytes. No EOF or 10k oracle output exists. The grouped
16-thread jemalloc diagnostic was also SIGTERM-stopped at 12:19:45 UTC before
the cap; that pending-exit wording is historical. Its sampled heap estimates
255,375 MB total, with `emit_exact_candidate`/`resolve_one_serial_group`
167,655 MB (~66%), `candidate_for` 27,632 MB, `flush` 5,568 MB, and node
membership below 250 MB. These are not exact RSS, but locate pressure in
terminal candidate buffering rather than DP node cache. The exact frozen-100
and full unit/integration passes stand, but the intended 10k oracle-parity gate
is not satisfied. There are still no whole-genome outputs.

The next technical direction, now accepted by consequential review and root
after this documentation checkpoint, is early stable interning of structurally literal-identical
`AssignmentCandidate` values in four separate allowed/vetoed exact/fallback
pools per profile. It must retain a minimum canonical
(record,sink,raw-model,StateKey-emission) coordinate and final-sort order because
generation is noncanonical. Default retention remains `all_raw`; compact facts
are native-production opt-in only. Raw pair IDs and source facts remain unchanged;
`all_raw` retains literal fact multiplicity, while compact returned facts must
equal the canonical stable-unique projection. Every result/reason, CB/UMI,
profile result and output remains exact: no Parent geometry or
policy merge, state-key quotient, lower-score pruning, or hit cap. Required
tests compare full results plus stable unique-fact projection across old
all-model/raw-serial/group modes, typed overrides, hash collisions, pools, and
ordering, followed by full CTest, frozen-100, and real-10k resource gates.
Original 10k oracle parity remains unmet.

The same review accepts a resolve-local successful-materialization memo keyed
by profile/pool, policy ID, score, tier, strand and body-support ID set only for
body tier. A new key must first materialize/validate the full candidate and
enter the literal interner; a repeated identical numeric key can reuse that
entry and update its minimum canonical coordinate. Different policy IDs must
materialize independently before any full-literal merge. Source and effective
profiles stay immutable; no memo is shared across reads. Both basic interning
and memoized interning require exact stable-unique facts and complete-result
comparison against all-raw oracles. StateKey count-quotient/max-score collapse
was rejected because it can erase a score-window-pruned reason.

The final native-query binary passed all 198/198 CTest checks in 31.03 s
(`build/ctest-native-full-final.log`). The repaired focused gate passed 12/12
in 6.40 s (`build/ctest-native-focused-repaired.log`), including exact biological
tables, MEX, logical hashes, terminal and barcode-stage counters at
1/2/4/8/16/128 threads, joint/separate profiles and a reverse-strand derived
profile. The 1000-alias fixture retained 1001 raw models and matched legacy;
its native four-group read phase was 0.00711 s (toy-only, not a whole-genome
throughput estimate). No native query runs during preload. The source-level
D107 bridge and terminal/barcode provenance issues are fixed without changing
frozen assignment policy.

New run root:
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/`.
It freezes the accepted 111,622,752-byte binary/source/test receipts and builds PCANBND1
from the existing PCI3 plus GBWT metadata only, then attempted to count the unchanged
66,601,887-group GAMP with XG/GBWT/existing RI, both profiles, 128 threads,
500,000,000,000-byte/no-swap limit, no count spill or BAM, and no extra input
hash pass. Whole-genome fit, reader EOF and final counts remain unproven.

The metadata index completed at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/index.receipt.json`:
9,765,357,968 bytes, 190,240,849 paths, 95,130,819 raw pairs, 1749.71 s
reported/1771.748 s process wall, 183.38-GB cgroup peak, and no path steps.
The first count loaded ordinary-parallel index/XG/RI/GBWT in 4.37089/14.6753/
95.8872/176.179 s, was ready in 181.525 s with zero query preload, then took
7.81606 s for its first 1k groups and 70.7681 s for 10k. It was stopped by
SIGTERM at 11:08:59 UTC because cgroup use reached 476,584,603,648 bytes
(RSS 460,924,900 kB) approaching the 500-GB cap; Docker exited 143 at
11:10:07 UTC, OOM false. `count.time.txt` records 627.091 s overall. There is no
EOF or output. The safe index is reusable. This observation does not prove a
memory leak: WorkerCache cleanup, transient models/state/candidates, and 128
glibc arenas remain plausible pressure sources. An allocator A/B is diagnostic
only if needed; a copied host jemalloc 5.3 passed image load/profiling-option
smokes and is now being used only for the bounded allocator diagnostic below.

The frozen 128-thread strict `first10000.gamp` control—not the earlier
unordered 10k completion milestone—also reached the cap and was SIGTERM-stopped
at 11:21:11 UTC (last anonymous RSS 453,770,848 kB, cgroup 498,153,754,624,
peak 499,999,997,952, OOM 0, `memory.events:max` 3223, 39 threads remaining);
it exited 143 at 11:22:08.815 UTC after 592.294 s. There is no EOF or output.
The native index is unchanged and safe. This does not establish a root cause.
A frozen plain 16-thread strict control, container
`pancollapse-native-prefix-control16-20261031`, began about 11:25:14 UTC but
also failed before EOF: exit 137/OOM true at 11:35:51.869 UTC, cgroup peak
499,999,997,952 bytes, sampled process RSS 507,377,152,000 bytes (including
shared file mappings), and 637.201 s overall. This is still not a root-cause
diagnosis. Frozen binary plus jemalloc 5.3 at 16 threads, with bounded sampled
heap profiles, started at 11:36:02 UTC as
`pancollapse-native-prefix-jemalloc16-20261031`; its label/receipts are
`frozen-jemalloc-first10000-t16`. It also failed before EOF, exit 137/OOM true
at 11:45:19.492 UTC, with cgroup peak 499,999,997,952 bytes. The allocator is
diagnostic, not promoted. Its sampled live heap identifies resolver state
propagation as major pressure (41,456.2 MB cumulative in the first analyzed
sample, versus 912 MB candidate construction), and redundant GBWT metadata
as 42,240 MB. These are sampled allocation estimates, not exact RSS. The
profiles and `heap-frozen-i1.txt` remain in the run root.

Stage 1 (unique-chain fast path, flat model sort/unique, binary D107 lookups,
model hoisting, and stats) has focused 1/1 and 3/3 passes and 198/198 complete
CTest passes in 60.57 s (`build/ctest-native-linear-full.log`). Its frozen
binary/source are `panCollapse-linear`/`source-linear.tar.gz` in the run root;
Stage 1 is not real-data resource qualified. The approved next computational
work groups per-alignment DP only
where full effective node membership/presence/flags, reachable effective
oriented transitions, D107 behavior, and fixed support status agree; all raw
pairs/facts/emission order/profile rules remain and generic DP is forced oracle
for mixed support labels. Exact oracle comparisons must cover specified
fixed/overlap/mixed support, policy/transcript/edge/D107, branch/merge,
empty/insertion, raw-duplicate, and hash-collision cases. Require full CTest,
exact frozen 16-thread prefix outputs/counters, and a resource gate before any
full restart. jemalloc 5.3 is an optional smoke-tested diagnostic, not adopted.
The follow-up consequential review accepts processing independent raw-model
or behavior-group DPs one at a time, retaining the original State transition
and strict-greater score dominance. Per-sink/profile allowed and vetoed
buffers must restore sink/raw-model/StateKey fact order. Keep the old all-model
map as the small-fixture oracle and add a raw-model-serial control for the
real prefix. This removes simultaneous model-state storage without deleting
raw facts or changing biological rules; multiply-corrupt-input exception
ordering is not promised. Native loading now also releases redundant GBWT
name metadata after numeric identity checks; an API before/after query test
and full rebuild remain required. No whole-genome restart is authorized by
unit passes alone.

**Stopped predecessor (historical):** after the seven-byte boundary/raw-ZSTD and complete fixture
byte comparison passed, the 104,928,704-byte slab binary passed 195/195 CTest
checks in 27.04 s. Run root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_node_slabs_count_20261031_iVn7rR/`
launched its index container at `2026-10-31T06:12:28.764761604Z` but was stopped
at `2026-10-31T08:15:15.87979019Z`, exit 143 and not OOM; PID 157285 is gone.
Terminal scan state was 104,448/16,469,090 targets, 18,972,832/95,110,030
Parents, 45,633,563,883/224,984,793,940 streamed steps, 22,589,770,903 encoded
bytes, 22,817,013,760 allocated bytes, and 184,861,290,496-byte peak RSS. No
`counting.pci`, GAMP processing, or counts exist. The 195/195 and fixture-byte
receipts are implementation-only historical evidence, not whole-genome
completion.

The user approves a load-ready, prebound numeric annotation object used with
XG, transcript GBWT, and the existing RI. It is custom annotation/metadata
only, not the 225-billion-step path index. Count must load or mmap it, attach
the native references, and begin GAMP processing without string joins,
metadata/path/node prepasses, `prepare_paths`, FastLocate construction,
geometry building, or global warmup. Persist ready raw exon/body pairs and
policy/support facts; native GBWT/RI are limited to touched-node and local-edge
queries. D105 and D107, frozen profile/barcode/UMI/output contracts, and the
per-gene goal remain unchanged. The unproven PCANCOUNT2 cross-Parent geometry
merge is excluded from this route. Implementation and its parity-corner/direct
native-query tests are next; then a new full suite and performance fanout gate.
No whole-genome launch occurs before those gates.

**D110 implementation checkpoint:** branch
`fix/whole-genome-count-repair-20261028` is at `327b048ce874` with preserved
dirty state. New native index/reference, CLI/runtime, resolver/shared-source,
and focused verifier work implements PCANBND1 metadata-only compilation from
raw Parent exon-by-body pairs with CSR and a feature catalog; it does not walk
paths/nodes or use the PCANCOUNT2 cross-Parent geometry merge. The prepared
counter loads the native object with XG/GBWT/RI and no GFF/ledger parse,
reference joins, or count prepass. Trusted PCI3 reading skips the whole-payload
SHA while strict legacy validation remains default; the current native route
trusts existing RI receipt sizes/prior hashes. The FastLocate sequence-ID versus
packed-SA issue is fixed. The D107 empty-subpath Gene-fallback leak has a
source-level fix: carry the last reference-consuming endpoint through empty or
insertion-only subpaths over `next` and `connection` links. A combined Docker
build passed; the six focused codec/query/direction/runtime/index tests passed
in 0.91 s (`build/ctest-native-basics.log`). The follow-up rebuild also passed.
The expanded focused gate passed 11/12 tests but failed native CLI terminal
counter parity (`build/ctest-native-focused.log`): all four Parquet tables,
logical hashes and MEX matched the nine-read legacy fixture; one antisense
read became `unassigned_no_evidence` instead of `unassigned_opposite_strand`.
Removing a wrong-way candidate also erased its barcode-stage eligibility.
A resolver-local pre-veto diagnostic evaluation is being implemented, without
allowing rejected candidates into returned facts or counts and without changing
the frozen assignment policy. The consequential review accepts that mechanism
and the empty-subpath fix. Integration/full-suite gates remain pending. Before an immutable new native index/count launch, run
focused codec/query/direct-parity checks at 1,2,4,8,16,128 threads, the
1000-alias fanout case (1001 expected), and the complete existing CTest suite.
The active goal is both-profile outputs for 66,601,887 GAMP groups, not a
scientific parity or performance claim. No new run root exists; production is
stopped.

**Read-only architecture evidence (2026-10-31):** Native FastLocate nodeDA
exposes numeric oriented path IDs with visit multiplicity, and its two-node
find/locate exposes oriented adjacency IDs; neither API encodes gene, exon, or
body-policy semantics. The existing RI (44,433,079,158 bytes), GBWT
(44,562,796,448 bytes), and XG (5,560,818,652 bytes) are present; no hashes
were taken. Local GBZ load invokes named-path LF-walk caching, whereas normal
XG+GBWT+RI deserialize/attach does not; this is observed behavior only, not a
selected route. The current PCANCOUNT2 model equality (`same_model`) can merge
different raw exon Parents with the same full exon geometry and then combine
body fragments/supports. Legacy D105 code retains raw exon-by-body pairs within
Parent. Their observational equivalence is unproven, and the 195 small
fixtures do not cover that counterexample; a regression fixture is required
before any future promotion. No whole-genome output exists, so this does not
establish a wrong count. Production remains stopped pending validation gates.
Continuing progress updates are every 30 minutes unless directly answering or
stopping a job.

**Historical compact-construction milestone, 2026-10-31T05:55:12Z:** source loading,
path binding, and temporary edge ranking completed. The source has 16,469,090
canonical targets and 224,984,793,940 path steps. The compact scanner has
processed 1,745,056 linked Parent groups / 4,164,805,763 steps, retaining
1,946,002,334 encoded payload bytes. RSS is 155,355,648,000 bytes, below the
176,450,789,376-byte initialization peak. No index publication or GAMP counting
yet; final fit and elapsed time remain unproven. The old expanded attempt had
already reached 339 GB by 4.75 billion steps. Independent streaming output
validator `validate_counts.py` in the current run root passes on the existing
nine-read fixture; it has not validated whole-genome counts that do not yet exist.

The prior production attempt remains stopped. Container
`pancollapse-intronic-fragment-count-20261031-jFlfrm-r1` stopped at
`2026-10-31T01:38:02.955258791Z`, exit 143 and not OOM. A live Docker check
confirmed that attempt stopped. Its final log remained in path-catalog
initialization, with no GAMP processing or final counts. Preserve the existing
inputs, compiled index, frozen binary and run files. D105's containment removal
remains active. The user now authorizes implementation and tests for a
graph-specific prepared numeric node-counting index, directly queried while
streaming reads. It moves reference parsing, path binding, catalog, model, and
junction preparation offline; prepared counting loads no GFF, ledger, XG,
GBWT, RI, DNA sequence, or full graph walk. Preserve transcript-specific
exon/intron evidence, strand, scores, splice support, and profile/barcode/UMI
rules. D105 has no ordered-containment requirement, and D107 remains a hard
wrong-way splice-candidate exclusion.

**Historical v1 prepared-path acceptance (2026-10-31):** the 102,781,736-byte binary
passed all 195/195 CTest checks in 26.39 s; receipt:
`build/ctest-prepared-final.log`. Focused codec, scoring, and directed-junction
checks passed, as did the nine-read Parquet/MEX logical-count and molecule
parity fixture at 1/4 threads and joint/separate profiles. This is validation
of the prepared path, not whole-genome fit, reader EOF, final counts, or
production completion. Source remains branch
`fix/whole-genome-count-repair-20261028` at `327b048...`, with 77 preserved
dirty paths.

**Resource redesign gate (observed first attempt):** run root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_node_count_20261031_5gyam1/`
contains the frozen binary, source archive, 195-test receipt, and index-then-count
script. Container `pancollapse-node-count-20261031-5gyam1`
(`80e9ad2d80a5e01062556f285bc110338c5739c85df7cd0b10666e6ffcf4a059`)
was stopped at `2026-10-31T05:01:53.714Z`, exit 143 and not OOM, during
whole-genome PCANCOUNT1 construction. It reached 339 GB peak RSS by 2,400,000
of 95,110,030 linked groups. No completed index or read count exists.

The accepted engineering repair is construction-time compaction, not a
biological-policy change: target-local compilation, deterministic per-target
sample-support dictionary pre-encoding, shared 256-node/64-edge bitmap blocks
with ZSTD rows, and at most 32 live geometry workers even when `--threads 128`
is passed. Only selected annotation-edge keys are persisted; graph-edge ranks
are temporary compiler state, and GBWT-only transitions receive deterministic
lexical overflow ranks before publication.
It preserves model and distinct scoring IDs plus D105/D107 semantics.

**Historical PCANCOUNT2 acceptance and first launch:** the final 104,884,760-byte binary
passed all 195 CTest checks in 26.72 s; receipt
`build/ctest-prepared-blocks-release.log` is copied to
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_node_blocks_count_20261031_rywaxM/ctest.log`.
This includes count/molecule/MEX parity, the missing-edge fixture, and byte-identical
1/4-thread index construction. It validates the prepared implementation, not
whole-genome fit, reader EOF, or output counts. Container
`pancollapse-node-blocks-count-20261031-rywaxm`
(`82dafdc8202706d92f80220ce7446420fdeff5eb85ac82501a10062df71fdbc3`)
launched at `2026-10-31T05:19:29.175716381Z` but was stopped at
`2026-10-31T06:06:23.480629105Z`, exit 143 and not OOM. Terminal construction
statistics were 9,205,174,225 steps/225 billion, 3,936,680 Parents, and
4,613,383,674 encoded payload bytes; peak RSS was 176,450,789,376 bytes
(~176 GB). Count never started. This is not an observed final-fit failure:
review found a credible late `std::vector` payload reallocation that can double
memory beyond the cap despite projected final fit.

The accepted allocator-only repair is `EncodedBuffer`: append once into 256-MiB
RAM slabs, then stream used regions into the same flat PCANCOUNT2 encoded
section. It changes neither reader, on-disk format, policy, nor I/O/spill
contract. The seven-byte-boundary raw/ZSTD checks passed; the complete fixture
byte comparison passed between
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_node_blocks_count_20261031_rywaxM/fixture-pre-slab.pci` and
`build/tests/vg/xg/prepared_count_work/run-5csmbeu8/prepared-node-count.pci`.
The final 104,928,704-byte slab binary passed 195/195 CTest checks in 27.04 s
(`build/ctest-prepared-slabs-final.log`).

**Historical slab launch:** frozen binary, source, receipt, validator, RUN.json,
and script are in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_node_slabs_count_20261031_iVn7rR/`.
Container `pancollapse-node-slabs-count-20261031-ivn7rr`
(`a2a1822eb7103bead95a2cefc029af7c5d6b40d90b3b75d13ce96cea241ce83a`)
started `2026-10-31T06:12:28.764761604Z`; index PID 157285 was live at about
07:25:15Z. The 32-worker scan reached 50,176/16,469,090 targets,
9,106,292/95,110,030 Parents, 1,086,946 models, 9,989,293 scoring classes, and
22,604,198,100/224,984,793,940 streamed source steps (about 10% of source
steps, not the whole job). It retained 11,158,835,735 encoded bytes in
11,274,289,152 allocated slab bytes; RSS was 166,767,640 KiB (~170.8 GB), with
all memory.events counters zero at 07:24:19Z. The frozen
500,000,000,000-byte/no-swap/128-CPU/32-geometry-worker,
no-count-spill index-to-count script retains both profiles and the expected
66,601,887 GAMP groups. This container was later stopped at 08:15:15Z before
index publication or count. No index is published and no count has started; final
fit, reader EOF, and output results are unproven. The stopped attempt was
compiling from PCANIDX3
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_prepared_reference_count_20261030_7jj7q9/reference.pci`,
`joint_prepared_24_20261017/joint/joint.xg`, and
`joint_transcript_gbz_20261028_repair/joint/joint.transcripts.gbwt` under
500,000,000,000 bytes/no swap/128 threads. Then count the existing
`mpmap_pbmc1k_v3_20261027T122300Z/attempt5/full/alignments.gamp` (expected
66,601,887 groups) with `/mnt/ssd/lalli/3M-february-2018.txt` and both
`cr7-v1`/`pansc-strict-v1` profiles under the same cap, 128 threads, and no
disk spill. No RI compiler is used. No whole-genome index publication, count,
reader EOF, fit, or output result is yet established.

Before a whole-genome index build, small fixtures and read-lookup goldens must
pass. The whole-genome build may then use existing inputs under a 500-GB no-swap
cap in a named, recorded container, with operational updates retained. D108 now
authorizes the conditional next stages: after the prepared index works and the
specified validation gate passes, build the whole-genome index at 500 GB with no
swap, then run the whole-genome count at 500 GB/no swap/128 threads and no disk
spill. The production goal is active; the first node-index attempt exposed a
construction-memory blocker that is being repaired. The
following run milestones are historical, not current-running status.

**Milestone, 2026-10-31T01:13Z:** reference loading and sparse path coordinates
completed. `selected_path_positions` took 1525.94 s for 190,240,849 paths and
224,984,793,940 path steps, with 3,801,283,990 checkpoints and 15,205,135,960
coordinate payload bytes. `ledger_path_handles` then passed in 34.9733 s.
The run is now preparing the path catalog; PID 3586664 had RSS 233,843,048 KiB
(~223.0 GiB) at 01:13:03Z. This is startup progress, not GAMP processing,
successful exact-parent geometry, whole-genome fit, or final counts.

PCANIDX3 embeds the repaired ledger and fact catalog and loads reference/XG/GBWT+RI
in three owned branches. The published PCI remains
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_prepared_reference_count_20261030_7jj7q9/reference.pci`
(40,929,379,277 bytes) and the existing RI is reused. These establish prepared
startup only, not a completed count.

**Terminal failure checkpoint:** at about `2026-10-30T21:39:50Z`,
`pancollapse-prepared-count-20261030-7jj7q9` exited 1, not OOM, before GAMP or
output. Terminal error: `genefull_ex50pas cannot establish body/exon orientation`
for linked paths
`chr19::panSCbody1_c9d010286f5911382668f6fe055ae707550636e675a81168114a9c1ac8764113_R1`
and `chr19::HG00128_ha_T0269130_R1`. Index loading, bindings, lengths, and path
catalog completed; failure is in exact-parent geometry before an
`exact_parent_models` completion receipt. `count.time.txt` records exit 1, wall
1:14:46, peak RSS 289,479,060 KiB (~276.1 GiB), and zero swaps.

**Diagnosis (observed):** the retained read-only root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_orientation_diagnosis_20261030_030HFr/`
contains `walks.tsv`, `original-gff-parent.tsv`, `original-gff-query.log`, and
`source-fragments.*`. The failing body fragment has 68 unique nodes and shares
zero with the exon’s 62 nodes; its sibling body fragments share 30 and 32 nodes,
respectively, in the same orientation. The original CAT GFF3 reports three
`HG00128_ha_T0269130` exons totaling 712 bp; the failing 642-bp body interval is
entirely intronic. This does not prove bad linkage. It does establish that the
runtime’s shared-node-only orientation inference cannot handle this valid
fragmented-body case.

**Implemented repair:** `ExactOrientationBasis` now uses `IntrinsicBody` only when
the linked body/exon pair has zero shared nodes. Such a candidate is B-only and
its `GD` is computed from the emitted body walk. Existing `SharedSame`,
`SharedOpposite`, repeated-occurrence, and ambiguous mixed-parity behavior remain
unchanged. This is a software correction to the recovered v0.8 source contract
(`e047b07`), not a new biological overlap requirement, eligibility, or filter.

**Provenance accepted by review:** archived `004333prepare_chromosome.sh` passed
chr19 `final/retained_exons_and_bodies.gff3` from `004277` to
`candidate/mpmap_index_chromosome_v2.sh`; both layers are GFF `exon` features,
retain their strand, and use the same pinned `vg rna`. Archived `004314` guide and
`004317` ledger evidence were restored. No sequence reconstruction or full-file
hash was performed.

**Acceptance:** `build/disjoint-body-regression-ctest.log` records 190/190 CTest
passes in 25.78 s. `disjoint_body_exact_orientation` dynamically checks disjoint
plus/minus body walks, both profiles at 1/4 threads, shared-opposite preservation,
and mixed shared-node failure; the frozen prior binary fails the intended
`cannot establish body/exon orientation` error. The test is registered in
`tests/vg/CMakeLists.txt` and can be run as `ctest --test-dir build -R
disjoint_body_exact_orientation --output-on-failure`.

**Live launch:** the repaired launcher started
`pancollapse-intronic-fragment-count-20261031-jFlfrm-r1` at
`2026-10-31T00:37:41.339941051Z` (17:37 PDT) in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_intronic_fragment_count_20261031_jFlfrm/`.
Docker verifies Memory and MemorySwap at 500,000,000,000 bytes and NanoCpus at
128,000,000,000. It reuses PCI/RI with both profiles; no BAM, spill, remap, or
full-file hash is authorized. `count.log` exists and reference initialization is
ongoing. Zero reads have been processed, and neither fit nor final RSS/count/output
is established; `count.time.txt` and reader EOF 66,601,887 remain future evidence.

The initial non-`r1` container exited 126 before panCollapse because its launcher
passed an extraneous `/bin/bash` argument; it is retained and did not open GAMP.
The repaired launcher uses the image and script with bash as entrypoint. Final full
CTest is 190/190 in 25.78 s at `build/disjoint-body-regression-ctest.log`, copied
to the run root as `ctest.log`; the 25.37-s initial receipt is retained as
`ctest-initial.log`. Monitor reference initialization, then the reader through
completion under the existing authority.

All dated operational updates below are retained historical milestones, not
current-running status.

The conditionally approved/refined architecture is PCANIDX3: a self-contained
runtime ledger plus fact catalog, lexical shared dictionary, 64-byte identity
rows with Parent rank, 8-byte Parent representative records, and serialized
canonical/qualified/normalized/escaped/original maps. It uses linear Parent
validation, direct `FrozenFlatMapSortedBuilder`, bulk fixed I/O, and inline
checksums only (no V3 verification reread). Three owned-result
`std::launch::async` branches load reference, XG, and GBWT then RI; join all
before existing crosschecks, with `--threads < 3` retaining the serial baseline.
Legacy V2 APIs remain; count uses V3 without TSV parsing, normal indexing writes
V3, and `--from-annotation-index` upgrades V2 once from the corrected ledger.

Final implementation review accepted PCANIDX3 and the post-release embedded
provenance-lifetime fix. `ctest` records 188/188 passing in 23.56 s. New V3
parity covers all three load branches at 1/4 threads, files/counters/read
assignments/RAD, TSV removal, and legacy V2 byte identity. Two earlier test
failures (fixture baseline authority and truncation expectation) were corrected
without weakening exact comparison. This remains bounded validation, not a
whole-genome performance/fit claim.

The frozen pre-production root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_prepared_reference_count_20261030_7jj7q9/`;
it retains the tested binary, build/CTest logs, RI binding, and
`index.sh`/`launch-index.sh`/`count.sh`/`launch-count.sh`/`restart-after-index.sh`.
The pre-production checkpoint was reviewed and the authorized index container
`pancollapse-prepared-index-20261030-7jj7q9` started at
`2026-10-30T18:27:46.588311157Z` (11:27 PDT). It runs with verified
500,000,000,000-byte memory and matching memory-plus-swap limits (no swap), 128
cores, and read-only mounted GNU `/usr/bin/time -v`, which records peak RSS in
`index.time.txt` (and later `count.time.txt`). It upgrades prior
`DkAAtB/reference/annotation.pci` with its corrected ledger using
`index --from-annotation-index OLD_V2 --path-identity-ledger TSV --threads 128
--out reference.pci`. It performs no GFF reparse, RI rebuild, or extra SHA
verification pass; TSV is a one-time input parse, and the V3 writer atomically
publishes with inline checksum. `index.log` is active; no new index is published
and no count has started. Only after index exit 0 will host supervisor session
59300 launch
`pancollapse-prepared-count-20261030-7jj7q9`, using V3 without TSV and the same
GAMP/XG/GBWT/RI/whitelist paths. Both stages retain 500,000,000,000 bytes/no
swap/128 cores/both profiles; mounted native `/usr/bin/time` records peak RSS.
Expected reader EOF remains 66,601,887 input read groups. Index construction and
monitoring are active; count is running. The corrected prior DkAAtB
`RUN.json` now records its stop exit 143. The next authorized task is monitoring
the index and conditional count through completion, diagnosing failures as needed.

**Index-build milestone, 2026-10-30T19:49Z:** the one-time TSV read, cross-row
validation, and compaction completed for all 190,240,849 rows; existing PCANIDX2
facts also loaded and validated successfully. `index.log` is in `binary_write`
(190,240,849 Parents), collecting the shared string dictionary before parallel
lexical sorting and fixed-record serialization. Native PID 3037317 was active
with VmRSS 170,644,716 KiB (~162.7 GiB) and VmHWM 170,698,396 KiB; the 500-GB
no-swap cap is unchanged. These are live observations, not final timing/RSS
receipts: native GNU time output is pending index exit. No V3 had then been
published, no count had started, and GAMP remained untouched at that checkpoint.

**Live update, 2026-10-30T19:57:39Z:** container
`pancollapse-prepared-index-20261030-7jj7q9` remains running and not OOM. Shared
dictionary collection finished with 489,252,881 string references; the log stage
is `identity_dictionary_sort`, with parallel sort launched (129 total threads),
and the writer is canonicalizing. Do not infer sort completion from a sleeping
worker pool. VmRSS is 175,577,128 KiB (~167.4 GiB), VmHWM 179,215,400 KiB
(~170.9 GiB), and the 500,000,000,000-byte cap is unchanged. No V3 index had
then been published and no count had started.

**Publication milestone:** PCANIDX3 was atomically published at
`2026-10-30T20:22:32.302287029Z` as
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_prepared_reference_count_20261030_7jj7q9/reference.pci`
(40,929,379,277 bytes; `stat` verified at 20:22:50Z). The index container remains
running in native destructor cleanup, so it has no exit/time receipt or flushed
stdout yet; supervisor 59300 is still waiting for exit 0. PID 3037317 RSS was
161,159,892 KiB at 20:22:50Z, falling after final write. At that publication
checkpoint, terminal build success and count start were still unconfirmed.

**Terminal index / current count gate:** at 2026-10-30T20:25Z the index container
exited 0, not OOM. `index.stdout` JSON reports 190,240,849 Parents, 265,064 genes,
and 40,929,379,277 bytes; `index.time.txt` records wall 1:57:11, peak RSS
179,215,400 KiB (~170.9 GiB), zero swaps, exit 0. Supervisor 59300 then completed
0 and started `pancollapse-prepared-count-20261030-7jj7q9` at
`2026-10-30T20:24:58.75885233Z`. Docker limits are verified: Memory and
MemorySwap 500,000,000,000, NanoCpus 128,000,000,000. `count.log` records
`reference_load_parallel branches=3`, `prepared_annotation_index_begin`,
`xg_deserialize_begin`, and `gbwt_deserialize_begin`; XG completed in 13.4924 s.
By 20:25:20 the compiled dictionary had loaded 90M entries, with no TSV flag.
PID 3170470 remains in reference initialization. No GAMP processing, whole-genome
fit, final output, or count completion is established. The next authorized task
is monitor this count through completion and diagnose any failure without new
scientific claims.

**Prepared-count startup milestone, 2026-10-30T20:32:27Z:** prepared annotation
index deserialization completed in 442.225 s, loading 190,240,849 Parents and
190,240,849 embedded ledger rows. Parallel owners also completed XG in 13.4924 s,
GBWT in 222.931 s, then RI in 98.9993 s; their overlap is intentional and all
three joined successfully. No TSV parse occurred. PID 3170470 was running and
not OOM at VmRSS 193,390,372 KiB (~184.4 GiB). Graph/path compatibility
initialization, GAMP/read processing, and terminal count/output validation remain
pending; `RUN.json` retains the measured timings.

**Reference-initialization update, 2026-10-30T20:57:55Z:** serial name binding
transitioned to 128-core position preparation by 20:46:40 (129 threads, about
12,800% CPU). PID 3170470 remains not OOM at VmRSS 232,359,584 KiB (~221.6 GiB)
under the unchanged 500-GB cap. GAMP processing has not started.

**Read-only architecture clarification:** PCANIDX3 is annotation-prepared, not a
complete graph-prepared reference: repeated graph binding/geometry remains the
startup gap. A graph-bound numeric projection is possible while preserving exact
semantics only with occurrence coordinates, model links, and junction ownership;
it is proposed future work, not authorization for implementation, a stop, or a
biological-policy change. Existing native RI receipts/header give a maximum
physical node ID of 116,844,906 (not actual node count), bidirectional GBWT size
474,167,531,502 including 760,920,122 endmarkers, and 380,460,061 metadata paths.
The ledger selects 190,240,849 paths, but selected steps cannot be inferred by
ratio. Conditional 16-byte-posting CSR offsets at 1/4/10 associations per
addressable ID are 2.80/8.41/19.63 GiB for a deduplicated category layer only,
not a full-index forecast; all-forward occurrence expansion is
236,703,305,690 × 16 = 3.787 TB. Selected volume and deduplication remain
unknown, so no tens-of-GB full-size promise is supported. Continue current count
monitoring unchanged.

**Selected-path geometry milestone, 2026-10-30T20:59:11Z:** `count.log` records
`selected_path_coordinates` for 190,240,849 paths: 224,984,793,940 steps,
3,801,283,990 checkpoints, 15,205,135,960 coordinate bytes, and zero wide paths;
it completed in 1446.72 s. `ledger_path_handles` completed all 190,240,849 paths
in 29.7063 s. Thus selected bindings and length checks passed with no mismatch.
RSS is 234,185,140 KiB (~223.3 GiB), running and not OOM. Path catalog and exact
geometry remain before GAMP; no read counting is claimed.

The 224.985-billion selected-step count is now measured, not inferred from a path
ratio. A 16-byte-per-occurrence expansion of that measured volume is
3.59975670304 TB. This does not make a shared gene/category layer terabyte-scale:
its deduplication is unmeasured. The 15.205-GB coordinate payload is checkpoints
only, not the complete reference or all path descriptors. `RUN.json` retains the
measured metrics.

**Path-catalog milestone, 2026-10-30T21:29:15Z:** production-1 path catalog
completed all 190,240,849 relevant paths in 1793.76 s (29m54s), including
fallback classification in 245.137 s; it records 4,122,279 fallback-only Parents
and 23,439 fallback genes. All catalog paths are ready. The run is now in exact
geometry setup, with no `exact_parent_models` milestone and no GAMP processing
yet. RSS is 257,874,236 KiB (~245.9 GiB), running and not OOM. `RUN.json`
retains path-catalog metrics. This quantifies the remaining graph-prepared cache
gap only; it authorizes no new implementation.

The first prefix-only preflight, container
`pancollapse-ledger-name-repair-20261030-Dfwj5y`, exited 1 without OOM at the
first `HG00097` chr2 row (about 101 million rows processed). It did not publish
a reference or start counting. Retain its original run-root evidence at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_name_repair_count_20261030_Dfwj5y/`.

The succeeding metadata-only diagnostic (`metadata-inspection.txt` in that run
root) establishes two distinct metadata cases: ID 47621561 phase 1 has native
`HG00097#1#HG00097_ha_T0037310_R1#0`, while its forced `REFERENCE` spelling lacks
`#0`; ID 52589685 is a distinct phase-2 case. All 7,458,390 non-generic records
are actual `HAP` records with count 0 and lie in chr2 by the recorded ranges.
This explains the prefix-only preflight failure; it is not repair success.

The revised metadata-derived legacy-alias utility passed bounded review with its
collision, owner, and reuse guards. `ctest.log` records 188/188 passing in
23.76 s, and `git diff --check` passed. The expected repair output is
190,240,849 rows, 7,458,390 legacy rewrites, and 87,702,873,062 bytes.

The active repair root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_name_count_20261030_DkAAtB/`.
Its initial same-root container exited 126 from an entrypoint-argument error
before work; it is retained, and the wrapper was corrected. Replacement container
`pancollapse-native-name-repair-20261030-DkAAtB-r1` exited 0 without OOM.
Published `reference/repair.json` confirms 190,240,849 rows, 7,458,390 rewritten
(all chr2), 182,782,459 unchanged, 87,702,873,062 bytes, and
`non_name_bytes_preserved: true`; Docker wall time was 1068.03 s. Its required
inline old-ledger SHA receipt matches the original PCI; new ledger SHA-256 is
`44c4c62d8fd7f79d51ba7d18d1f0349df962cd38d596d3f463b387c79f5faa7d` and PCI
SHA-256 is `736f928c2084d5c6923f019aa5d587a566eb441e79a3776449a74c8b28a8043a`.

**Historical stopped attempt:** supervisor exec session 51043 finished 0 and launched
`pancollapse-native-name-count-20261030-DkAAtB` at
`2026-10-30T17:53:19.121568821Z` (10:53 PDT), under verified
500,000,000,000-byte memory and matching memory-plus-swap limits (no swap), with
128 cores. It did not reach GAMP processing or final counts; the user-directed
stop above supersedes continued monitoring of this attempt.

**Historical live checkpoints:** at 2026-10-30T17:58Z, `identity_ledger_load`
reached 23 million rows in 298.7 s with process RSS about 16 GiB at 20 million
rows; before the stop request it reached 34 million rows in 446.7 s at about
25.8 GiB RSS. These are reference-initialization observations only, not GAMP
processing, counted-read progress, resource-fit evidence, or output completion.

**Reader-EOF denominator:** existing mapper receipts at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/mpmap_pbmc1k_v3_20261027T122300Z/attempt5/full/`
give 66,601,887 input reads/pairs (`input_stats.json`: L001 33,436,697 + L002
33,165,190). `COMPLETE.json` records complete status, both FASTQ EOFs verified,
zero exit, and a 30,326,279,277-byte GAMP; `process_receipt.json` records mapper
and producer exit 0, while `stderr.log` ends `Mapping finished. Mapped 66601887 reads`.
The native `input_read_groups` target is therefore 66,601,887 pending reader EOF.
Serialized alignment-record count is not independently known and must not be
conflated with this denominator. Stale `full/.progress.json` cutoffs are superseded.

The completed RI remains valid with the unchanged GBWT and is used through its
trusted-receipt flag; no RI regeneration or extra SHA passes occurred. The
188/188 CTest result in 23.76 s remains the bounded acceptance receipt.
The active root retains `ctest.log`, `CI_ACCEPTED.json`, scripts, `repair.log`,
and current `r-index-binding.json`. Avoid extra file hashes; use only the
PCANIDX2-required inline bindings and fast lookup hash. Repair and any eventual
count retain both profiles, 128 cores, and the 500,000,000,000-byte hard memory
and memory-plus-swap limits (no swap). The first prefix-only failed root remains
distinct and retained. The next authorized task is monitoring and diagnosing any
failure without biological-policy change; this establishes neither whole-genome
fit, parity, count completion, nor published counts.

**Documentation checkpoint — next performance phase (2026-10-31):** Reconciled
on dirty `fix/whole-genome-count-repair-20261028` at
`327b048ce8747c83fe20c9f75074cc4ee77b1312`. The active goal is unchanged:
same-GAMP whole-genome per-gene counts in under two hours wall time and under
300 GB RAM. The completed cross-TX quotient 10k gate is evidence of exact
node-score behavior and a bounded benchmark, not goal completion: its terminal
receipt, validation, and parity records are in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-behavior-first10000-t128.gISKSl/`.
It reached EOF on all 25,000 records/10,000 groups with 14 cutoff drops;
`validation.json` passed and `parity.json` records ten files byte-identical to
the first-DP result. Count read time was 128.025707080 s (78.1093 groups/s),
initialization 150.430731581 s, and runner wall 303.977775 s; sampled RSS/cgroup
peak were 167,081,734,144/157,636,481,024 bytes. It used 158,583 model groups,
1,258,500,451 DP states, and 629,485,930 raw-model transfers. The 55.39x
model-group and 3.48x state reductions and 1.58x count-phase improvement versus
first-DP do not establish a two-hour full-input rate or a <300-GB whole-input
fit. It claims neither legacy/CR7 truth parity nor altered biology.

Accepted invariants remain exact node-score semantics, D105/D107, frozen
profiles/barcode/UMI/output behavior, 64M raw-alternative cutoff, no spill,
hash, remap, or index expansion, and the 128-core/300-GB/no-swap boundary.
The whole count remains stopped. The old explicit enumerator
`pancollapse-node-score-cutoff-first10000-20261031` and its parity watcher were
confirmed live at this checkpoint; they are observational only and do not turn
the new result into full-real oracle parity. Current code implements the exact
DAG scorer and cross-TX quotient. The next proposal, not yet approved, is an
exact semantics-preserving optimization of raw-model/node-transfer construction
and/or further exact DAG factorization; parallel review will settle its mechanism.
Only focused/full tests and then larger-fixture performance evidence are next;
there is no production or whole-count launch authorization.

**Pre-O3-benchmark authority checkpoint (2026-10-31):** Sol approved only the
bounded implementation mechanism: immutable edge-only `NativeQueryMode` (with
no lazy node scoring), fixed-support specialization, and early read-local
node-membership/splice-mask quotienting before piece-by-raw-model expansion.
Mixed-support cases remain singletons and raw aliases restore before downstream
handling. This changes no policy, cutoff, profile, output, or biological
semantics. The worker is editing the native edge-only source and root is
implementing the early quotient; these structural changes are not in the O3
snapshot and are not benchmark authority yet.

The user additionally authorized an O3-only target CMake change if measurement
helps: the count target's final flags are O3, with no fast-math, while Debug is
unchanged. The O3-only binary, source archive, and full-test log are frozen at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-o3-first10000-t128.A5MNAe/`.
Full CTest passed 202/202 in 31.58 s. The authorized next gate is one same-10k
A/B benchmark using the existing GAMP fixture, both profiles, 300-GB/no-swap
cap, 128 threads, and existing indexes, with an independent reader and ten-file
byte/logical/scientific-counter comparison against the O2 behavior root before
interpreting performance. It has not launched. Keep O3 only on measured
improvement; it establishes neither the two-hour target nor a whole-count
launch.

**Pre-structural-10k checkpoint and O3 terminal result (2026-10-31):** The
O3-only control in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-o3-first10000-t128.A5MNAe/`
reached terminal PASS: its independent reader passed and all ten files are
byte-identical to O2. `terminal-receipt.json`, timing, CPU, and parity receipts
record 129.682-s read time and 2720.94-s CPU, versus O2's 128.026 s and
2758.87 s; this is no clear improvement. O2 is restored as the default CMake
configuration and O3 artifacts remain preserved.

The structural candidate is frozen and tested, but unlaunched, at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-early-membership-first10000-t128.JwNFph/`.
It uses the control's O3 compiler flags and passed full CTest 202/202 in 31.41 s
and focused 3/3, including 320 seeded cases plus new mixed/multi-record,
second-quotient, and cache-lifetime fixtures; Sol final review found no critical
bug. It constructs full read-local node-incidence/splice-mask classes before
Delta/Op, omits fixed-support vectors, leaves mixed support singleton, restores
raw aliases, retains the second Op quotient, eagerly builds node scoring, and
uses edge-only `NativeQueryMode` to skip unused edge postings. Legacy `lazy_edge`
remains exact and lease-safe. Metrics distinguish actual model visits from raw
model transfers and report membership groups and edge-model builds. The next
authorized gate is the same 10k at 128 threads/300 GB/no swap with unchanged
indexes/profiles/64M cutoff/output and no hash/spill/remap, then independent
reading and ten-file byte/logical/scientific-counter comparison to the O3
control. No structural benchmark has launched; the whole goal and larger-fixture
evidence remain pending. A prospective fixed/none vector-frontier score-storage
experiment (exact sort+max; mixed generic) is approved for later investigation,
not implemented and not a speed claim.

**Early-membership terminal benchmark; CompactScoreFrontier pending
(2026-10-31):** The structural benchmark at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-early-membership-first10000-t128.JwNFph/`
exited 0/no OOM at `2026-10-31T20:39:52.465535056Z`. `validation.json` passed
10,000 groups with 13,904 count and 13,995 molecule rows; `parity.json` reports
all ten files, scientific counters, and logical definitions byte-equal to the
O3 control. Manifest init/read/processing were 146.854756294/78.422407757/
78.486712665 s; runner wall was 237.103126 s, CPU 1589.931948 s, cgroup peak
132,202,778,624 bytes, and sampled RSS maximum 132,463,898,624 bytes. It reports
29,791,873 actual model visits versus 629,606,801 raw-model transfers, zero
edge-model builds, and 1,258,506,306 DP states. Compared with O3's 129.68213154-s
read and 2720.935242-s CPU, this is 1.65x read-wall and 1.71x CPU on 10k only;
it does not prove whole-genome two-hour or <300-GB performance.

`CompactScoreFrontier` is prelaunch-gate ready. For fixed/zero support it uses
vectors with exact sorting and componentwise maxima over independent
annotation/Gene objectives, preserving the full `StateKey` and generic step;
mixed support retains the map and `force_map` is a test control. The initial
new-test build failed only on a mixed-type initializer; that historical receipt
is retained. The corrected retry build exited 0, focused CTest passed 4/4 in
1.36 s (`build/ctest-node-score-compact-frontier-focused.log`), and full CTest
passed 203/203 in 32.18 s (`build/ctest-node-score-compact-frontier-full.log`).
The O2 binary, source archive (`source-vector.tar.gz`), and both logs are frozen
at `R/node-score-vector-first10000-t128.jfIkiW/`; `build.ninja:655-659` verifies
default O2 flags. `terminal-receipt.json` now exists for E.

The next gate is unlaunched: same 10k, 128 threads, 300 GB/no swap, both
profiles, 64M cutoff, unchanged inputs/indexes/MEX, no spill/hash/remap, then
independent reading and ten-file byte/logical/scientific-counter parity against
both E and the O2 control. No profile, biology, or approximate-pruning change is
authorized. After that, require 1M performance evidence; whole count requires
supported fit and throughput. Reverse-DAG suffix-bound balancing remains
unimplemented: Sol's proof is conditional, and a final bound alone cannot retain
the checked-overflow contract, so defer it pending measured need.

**Vector same-10k gate live (2026-10-31):** This supersedes only the preceding
unlaunched status. `pancollapse-node-score-vector-first10000-20261031`
(`59fe443bc40227e93585062d59b99db984c9453c847481dbed1eea4a8dd3dc19`) started
at `2026-10-31T20:41:58.387715404Z` from
`R/node-score-vector-first10000-t128.jfIkiW/`. `launch-receipt.json` records
300,000,000,000-byte memory and memory+swap limits, 128,000,000,000 NanoCPUs,
and no network. `count.log` has begun four concurrent load branches only; there
is no ready/count/parity result. No whole launch is authorized. Next is EOF,
reader/parity/resource/performance validation, then a larger 1M gate only if
appropriate; the <2-hour/<300-GB whole goal remains unmet.

**Vector 10k terminal; prepared 1M profiled gate (2026-10-31):** This
supersedes only the preceding live status. The vector 10k run exited 0/no OOM at
`2026-10-31T20:45:29.460668780Z`; its reader passed, `terminal-receipt.json` is
persisted, and both `parity-early-membership.json` and `parity-o2-control.json`
report all ten files, scientific counters, and definitions byte-equal. Init/read/
processing were 148.532392104/48.117434958/48.181194519 s; CPU was 1515.247936 s,
cgroup peak 132,201,652,224 bytes, and sampled RSS 132,021,186,560 bytes. This
is about 1.63x read-wall faster than E but only 4.7% lower CPU, so it does not
support a two-hour full-input inference.

The prepared root is `R/node-score-vector-first1000000-t128.TzO6UH/`, with the
frozen binary and source referenced relative to the 10k source archive, run
receipt, and bash-syntax-checked runner for
the existing 465,121,668-byte first-1M GAMP (2,502,433 records/1,000,000 groups;
expected 982 cutoff drops). It retains both profiles, 64M cutoff, 128 threads,
300 GB/no swap, MEX/indexes, and no hash/spool/remap. Sol approved external
gprofng 2.45 clock profiling across all target worker threads only (10-ms
sampling, 10,800-s recording cap, no I/O/heap/sync/per-read tracing);
collector/dispatcher source confirms the
cap closes profiling rather than counting. Correct relocation environment and a
collector/symbolized-display smoke receipt are recorded; count PID identity is
verified before RSS sampling and all processes are cgroup-bounded. Profiled
timing is not a clean benchmark. After review, launch this 1M gate, monitor EOF/
memory/performance, and independently validate counts plus profile header/sample
warnings/symbols. No full launch is authorized until feasibility is supported.

**Vector 1M profiled gate live (2026-10-31):** This supersedes only the prepared
unlaunched status. `pancollapse-node-score-vector-first1000000-20261031`
(`9e3398436ff6317580c8c155170cd7cbcf4fed74771b98cb06c61fbec7188556`) started
at `2026-10-31T20:47:43.684753337Z` from
`R/node-score-vector-first1000000-t128.TzO6UH/`. Inspection confirms
300,000,000,000-byte memory and memory+swap limits, 128,000,000,000 NanoCPUs,
and no network; `launch-receipt.json` records them. Native PID 7 command and
executable were checked; collector profile/log XML exist and four load branches
have started, but no counts are ready. Next is 1M EOF, profile attribution,
output/MEX independent validation, and resource/performance evidence. No profile
or biological change and no whole launch are authorized; the 66.6M <2-hour/
<300-GB goal remains unmet.

**Pre-exact-cache/index-validation implementation checkpoint (2026-10-31):**
The frozen live 1M run is unaffected and at 100,000 groups reached 195.179 s
(about 512 groups/s). Its 8-GiB node cache records 1,100,159 builds/985,911
evictions/367,691 hits; its 8-GiB edge cache 4,325,307/2,750,543/1,944,385;
active leases are 2.26 GB. Early open-profile snapshots
`R/early-cpu-functions.txt` and `R/early-cpu-calltree.txt` total 2835.1 sampled
CPU-s, with self time: `sdsl::select` 18.77%, `FastLocate::locateNext` 9.85%,
`Index::path_row` 8.25%, and `Index::model_ids` 8.01%. They include startup and
are partial, not final or phase-additive attribution. `profiling-notes.json`
records an expected SIGPROF mask warning and no initial errors; closed-profile
quality remains pending. The golden 10k MEX validator passed; 1M MEX validation
waits for EOF.

Sol approved two exact, test-gated mechanisms, with no implementation started:
(A) a 512-MiB shared immutable canonical physical-pair `SpliceEvidence` cache,
including negative/same-node cases, leases/charges, unchanged directional owners,
and lock order splice -> node/edge -> index (the existing bounded reference
cache does hold its shard factory lock); (B) one atomic byte per actual
`Impl.path_count` row with separate path-valid/model-refs-valid bits, acquire
load/release fetch-or only after complete successful checks, while concurrent
first checks may duplicate. Bounds/provenance/span checks remain per call; no
scan, hash, format/profiling-policy, or budget increase is authorized. Required
evidence is malformed/retry/concurrency/lease/eviction coverage plus existing
320-oracle/full-203 tests, then exact 10k parity. Flat builder vectors, DP
saturation, larger caches, and reverse-DAG work remain deferred. The whole goal
is still unmet and no whole launch is authorized.

**Exact-cache/index-validation prelaunch readiness (2026-10-31):** Both approved
mechanisms are implemented; Sol's final read-only review found no blocker subject
to the existing gates. Combined build exited 0; focused CTest passed 5/5 in
1.23 s (`build/ctest-static-splice-validation-focused.log`) and full passed
203/203 in 33.11 s (`build/ctest-static-splice-validation-full.log`). Coverage
includes concurrent validation, malformed/retry/late-bad-model-ref and
foreign/move/zero index cases, plus canonical negative/same-node splice cache,
singleflight leased spans, failed retry/no partial publish, reset/direction, and
tiny generic-cache eviction. No source budget API was added.

Frozen binary/source/logs and runner are at
`R/node-score-static-cache-first10000-t128.pLa3n0/`; `run-native-count-gate.sh`
passed `bash -n`. The same 10k correctness/resource gate is ready but unlaunched:
128 threads, 300 GB/no swap, both profiles, 64M cutoff, existing indexes, no
hash/spill/remap, independent reader and new MEX validation, plus ten-file parity
against vector 10k and O2. The live 1M profiled control remains unmodified at
320k groups/609.241 s (about 525/s), peak about 132 GB. Concurrent-gate results
will not be a clean speed comparison; atomic-byte allocation/RSS must be
measured. Node/edge warm-cache caps remain 8 GiB. No biological/profile/API
change or whole-launch authority follows.

**Static-cache same-10k gate live (2026-10-31):** This supersedes only the
preceding unlaunched status. `pancollapse-node-score-static-cache-first10000-20261031`
(`897ead8beab5428f67741f67a67f0f886a3c1375a5f3d94a4493a25bc3389cf3`) started
at `2026-10-31T21:02:24.658520064Z` from
`R/node-score-static-cache-first10000-t128.pLa3n0/`. Inspection confirms
300,000,000,000-byte memory and memory+swap limits, 128,000,000,000 NanoCPUs,
and no network; `launch-receipt.json` records them. Four load branches started.
The frozen 1M control remains live at 390k groups/736.37 s (about 530/s), about
132-GB peak, and no OOM. Next evidence is 10k EOF, reader/MEX validation and
two ten-file parities, plus 1M EOF, closed-profile quality, and format validation.
No full count is authorized.

A future node-64-GiB/edge-16-GiB/assignment-8-GiB configuration under the same
300-GB cap is read-only feasibility review with Sol only—not implemented or
authorized as default. Current node/edge 8-GiB and assignment 512-MiB caps are
unchanged. No scientific or policy change; the <2-hour/<300-GB goal is unmet.

**Static-cache 10k terminal and next exact phase checkpoint (2026-10-31):**
`node-score-static-cache-first10000-t128.pLa3n0` exited 0/no OOM at
`2026-10-31T21:06:16.673302741Z`. It completed 25,000 records/10,000 groups
with 14 drops, 13,904 counts, and 13,995 molecules; independent counts/MEX
validation and both ten-file byte parities versus vector `jfIkiW` and O2
`gISKSl` passed. Receipt: `terminal-receipt.json`. Manifest init/read/processing
were 156.313917758/58.028528258/58.082092723 s; CPU usage was 2,151,175,835 us
and cgroup peak 132,392,271,872 bytes, no spill. Concurrent frozen vector 1M
`node-score-vector-first1000000-t128.TzO6UH` remains live (582,995 at 1110 s), so
this is not a clean speed comparison. Full tests passed 203/203. The accepted
next exact technical phase is node-score-only NodeEvidence slimming: retain exact
metadata score_count, postings, and sorted-unique body transcript IDs; discard
raw hits/scoring vectors; reconstruct node_scoring lazily only on explicit
request; leave full mode unchanged. Sol conditionally requires metadata/
multiplicity/span/lifetime/concurrency/retry tests; production node-score never
requests node_scoring. Diagnostic query/cache counts may change, science may not.
Experimental 64/16/4/0.5-GiB node/edge/assignment/splice caps are feasibility
only under the same 300-GB hard cap, not defaults or proven fit; retain the
655,360-entry assignment cap. Freeze a candidate binary after slim tests, restore
normal source defaults, then run one isolated 1M parity/resource gate after M
EOF. No extra mandatory 10k pilot, full launch, or <2h/<300GB claim is authorized.

**Prelaunch 1M slim-wide checkpoint (2026-10-31):** Frozen, unlaunched candidate
root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-slim-wide-first1000000-t128.GwhDPE`
with `panCollapse`, `source-slim-wide.tar.gz`, `run-receipt.json`, `run.sh`, and
`ctest-slim-node-evidence-{focused,full}.log`; new scorer sources are
`src/node_score_resolver.{cpp,hpp}` and `src/node_score_oracle.cpp`. Normal slim
tests passed focused 5/5 (1.22 s) and full 203/203 (32.14 s); experimental caps
passed focused 5/5 and full 203/203 (32.87 s). Normal defaults were restored and
rebuilt at node/edge/assignment 8/8/0.5 GiB. The frozen candidate alone uses
64/16/4/0.5 GiB node/edge/assignment/splice caches, 655,360 assignment entries,
300 GB/no swap/128 cores. Slimming retains exact `score_count`, postings, and
sorted-unique body IDs; it reconstructs `node_scoring` lazily only on explicit
request, which production node-score never makes. Unique body capacity and the
original interleaved overflow-check order are preserved.

Sol found no blocker (including the stated failure-test limitation) and accepts
one isolated 1M gate, not another 10k pilot. It cannot launch yet: control
`node-score-vector-first1000000-t128.TzO6UH` is live at 870k/1645.92 s, no OOM,
about 132-GB cgroup/137-GB RSS; its native EOF, independent MEX/record
(2,502,433)/982-drop validation, and closed-profile quality are pending. On
terminal pass, launch one isolated candidate gate, record resources, and require
all-ten-file/scientific parity. The old enumerator 10k control is not whole
production. No 66,601,887-group count, <2h/<300GB proof, or whole launch exists.

**Vector 1M terminal and slim-wide launch readiness (2026-10-31):**
`node-score-vector-first1000000-t128.TzO6UH` exited 0/no OOM at
`2026-10-31T21:23:02.08623378Z`: native EOF 2,502,433 records/1,000,000 groups,
982 drops, 982,842 count rows, and 1,302,814 molecules. Both validation and MEX
validation passed (29,788 barcodes/20,621 features; CR7/custom NNZ
480,803/502,039). Init/read/processing/finalize/native-wall were
149.469227426/1918.133442001/1920.488174038/2.354732067/2090.459025 s; CPU use
131,680,755,620 us, cgroup peak 133,257,510,912 bytes, sampled RSS max
141,840,748,544 bytes across 418 samples, all OOM/max counters zero, no spill.
`terminal-receipt.json` records the result. Profile header reports no errors and
only the previously explained SIGPROF-mask warning; no timer/dispatcher/lost-
packet/unwind warnings. Exclusive functions prioritize native reference/cache
work (select 18.96%, locateNext 9.74%, path_row 8.54%, model_ids 8.32%), not a
clean unprofiled speed, F1, or full-goal claim.

Prerequisites now pass to launch exactly one isolated frozen slim-wide 1M gate
at `node-score-slim-wide-first1000000-t128.GwhDPE`, under 300 GB/no swap/128
cores, then monitor EOF, byte/scientific parity, and resources. Root must verify
actual container limits and record launch receipt. This does not authorize a
whole count; the 66,601,887-group <2h/<300GB goal remains unmet.

**Slim-wide isolated 1M gate live (2026-10-31):** Root launched
`pancollapse-node-score-slim-wide-first1000000-20261031` at
`2026-10-31T21:24:33.681372597Z` (container
`a81d3879ea37f0ffaf575b09e5983beece73a3e472008fb1cfc2dccd15d5ecfe`). Its
`launch-receipt.json` records actual 300,000,000,000-byte memory and
memory+swap limits, 128,000,000,000 NanoCPUs, network none, and exact image ID;
Docker verified native PID 8/host 3320043 command and executable. Four parallel
reference branches started; index/XG loaded in 5.04916/6.91632 s and GBWT/RI was
pending at last inspection. No EOF, counts, parity, or resource gate is yet
established; whole count is not launched. Main next work is monitoring C
throughput/cache churn/EOF/exact 1M parity under 300 GB. A separate EdgeEvidence
slimming proposal is read-only and unaccepted: production could retain only
canonical transcript exon-direction/body presences while a legacy getter lazily
requeries raw Parent hits. C is unchanged; no full-speed or F1 claim follows.

**Pre-implementation EdgeEvidence checkpoint (2026-10-31):** C remains live and
unchanged (92,823 groups/150.002 s, about 619/s). Partial cold observations are
node cache 24,145,165,576 B/812,909 builds/0 evictions; edge cache cap
17,179,869,184 B/resident 17,161,571,192 B/3,672,024 builds/10,476 hits/
551,738 evictions; splice 115,985,252 B/921,362 builds/536,920 hits/0 evictions;
sampled RSS 135,713,783,808 B. They are not whole-feasibility evidence; eviction
does not prove repeated FastLocate because splice caching bypasses most leaf-query
reuse, though raw edge Parent IDs create capacity pressure.

Sol conditionally approves, and root accepts, exact node-score-only EdgeEvidence
slimming before a second-level raw cache. Retain sorted-unique
`{canonical_transcript,uint32_flags}` with exon-forward, exon-reverse, and body
presence; splice/junction must reduce exactly those bits. Full mode keeps raw
hits/eager postings. Legacy node-score `Source::edge` first uses canonical
evidence, then requeries raw hits outside the cache lock, builds identical
checked model postings locally, and leases/publishes. Query diagnostics may
change only for the explicit getter; production does not call it. Required tests
cover full/slim order/flags, duplicate Parents, both directions, body veto,
negative/all orientations, lifetime/reset/concurrency, and failed-model-ref
retries. No index format, biology, score, or worker budget changes. Only
`native_count_reference.cpp/.hpp` and `native_count_query_test` are authorized;
root alone runs Ninja. No extra 10k pilot; a later frozen 1M candidate requires
exact C completion and new 203-test success plus a fresh checkpoint. Whole count
and <2h/<300GB feasibility remain absent.

**Node-compression/locality reconciliation (2026-10-31):** Fresh Docker poll
finds C and the old enumerator 10k control both still running with verified
300,000,000,000-byte memory/memory+swap and 128,000,000,000 NanoCPU limits; no
terminal claim is made. C's latest count milestone is 790,000 groups at 985.452
read seconds. Latest C resource sample is 199,106,256,896-byte RSS,
189,827,350,528-byte cgroup peak; no OOM is observed. C remains immutable and
EOF/parity pending.

The root-resolved worker helper is low-level POD codec only; prepared wrappers,
file validation, 31-bit limit/errors, and level-3 writer behavior remain unchanged.
The NodeCompression authorization stands. Sol separately approves only a later
read-only locality experiment: opt-in, no position sort/default, anchored at
32,768 complete QNAME groups/256 MiB window and globally capped at 65,536/512 MiB
including workers; no code is authorized. Current per-UMI counts are integer
aggregates by (profile, CB, gene, UMI) after per-QNAME assignment. Same-CB/UMI
reads cannot be discarded absent gene evidence/support; the exact graph-signature
cache is already safe. Whole count and <2h/<300GB feasibility remain absent.

**C live/provenance checkpoint (2026-10-31):** One fresh poll finds C still
running (no exit/EOF/validator/parity receipt). Latest sampled RSS/cgroup peak
are 200,757,465,088/191,563,079,680 bytes; no OOM is observed. Do not infer a
terminal result from the near-EOF milestone. NodeCompression native code and
4,096-fanout tests are written, but TerraMax repairs, robust-test work, and a
4-MiB encoder trim remain pre-build; no new build started. Continue only the
approved same-fixture optimization path. Provenance caveat: tracked same-GAMP
path attempt 5/full directory records Oct-27 stat mtime and
`process_receipt.completed_utc=2026-10-27T17:35:27`, despite later 10/30 goal
wording; it must not substitute for this input, and root will flag the mismatch
before any whole launch. This is frozen node-score baseline evidence only, not a
biological or Cell Ranger parity claim.

**C slim-wide 1M terminal gate (2026-10-31):** Docker exited 0/no OOM at
`2026-10-31T21:49:11.934653515Z`. Native EOF completed 2,502,433 records/
1,000,000 groups with 982 drops, 982,842 counts, and 1,302,814 molecules.
Independent count and MEX validations passed; `parity.json` independently
confirms all ten output files byte-identical with equal scientific counters.
Manifest init/read/processing/finalize were 154.522569225/1264.504138590/
1266.863654962/2.359516433 s. `resources.tsv`/`cpu.stat` give cgroup peak
192,385,966,080 bytes and CPU use 82,515,471,681 us. Assignment cache was
655,360 entries, 286,517 hits, 712,501 misses, 57,141 uncached, and
2,875,590,906 charged bytes. `terminal-receipt.json` records this frozen
node-score baseline result only—not Cell Ranger parity, F1, or whole feasibility.
NodeCompression helper/robust tests are still pre-build. Sol accepts raw typed
fallback for small/no improvement and ZSTD only for worker decode; the 4-MiB
encoder trim remains. A new candidate requires those tests and a fresh checkpoint.
Preserve the 10/30-goal versus Oct-27-recorded-input provenance caveat.

**Prelaunch D node-compression 1M checkpoint (2026-10-31):** Frozen, unlaunched
D is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-compressed-first1000000-t128.zvlqnP`,
containing immutable `panCollapse` (120,690,672 B), `source-compressed-wide.tar.gz`
(575,880 B), `run.sh`, `run-receipt.json`, and
`ctest-{normal,wide}-{focused,full}.log`. Sol approved implementation/storage/
concurrency including typed raw fallback. Normal focused/full passed 10/10 and
204/204 in 31.89 s; same-C wide focused/full passed 10/10 and 204/204 in 31.38 s.
Source defaults are restored to node/edge/assignment 8/8/0.5 GiB; root's
restored-default build/focused check is still running and must pass before launch.

D alone uses C's 64/16/4/0.5-GiB node/edge/assignment/splice caps, 655,360
assignment entries, 128 threads, 300 GB/no swap, network none, both profiles,
`node-score-v1`, and 64M cutoff. It adds canonical edge summary and lossless
node POD ZSTD-1/raw fallback with once-per-worker/group decode, strong publish,
and 4-MiB encoder trim; full legacy getters and prepared level-3 byte compatibility
remain unchanged. No biology, per-alignment scoring, CB/UMI, policy, spill,
spool, or input-hash change exists. Its script independently validates counts/MEX
and all-ten-file/scientific parity against C and profiled M. After root reviews
the restored build, exactly one isolated D 1M gate is authorized—then limits,
native PID, EOF, parity, and resources must be recorded. The future 8M-entry
assignment-cap experiment and locality opt-in are not in D. Preserve the same
Oct-27-recorded-input versus 10/30-goal caveat; whole count and <2h/<300GB proof
remain absent.

**Pre-implementation node-posting compression checkpoint (2026-10-31):** C is
live and immutable at 550,000 groups/705.927 s (about 779/s): node 64-GiB
resident 68,698,509,904 B/3.524M builds/1,229,290 evictions; edge 16-GiB
11.925M evictions; splice 473.6 MB/0 evictions; sampled RSS 194,051,293,184 B,
no OOM. These are partial observations, not whole feasibility; C EOF/parity is
pending. EdgeSlim is implemented only in the worktree
`src/native_count_reference.cpp` and `tests/vg/native_count_query_test.cpp`;
after comparator/corrupt-copy fixture repair before load, focused 5/5 (1.27 s)
and full 203/203 (32.27 s) passed (`build/ctest-edge-summary-{focused,full}.log`)
and Sol found no blocker. Full and node-score preserve science; canonical edges
retain only E-forward/E-reverse/body presence and the legacy getter exactly
reconstructs checked raw postings. It is not in C.

Accepted next technical phase: lossless in-memory raw-or-ZSTD-level-1 numeric
posting payload in node-score, retaining uncompressed NodeRecord count/body IDs;
decode worker-local once per node/group outside cache lock and publish only after
exact count/size/sorted-field validation plus lease accounting; reset clears it.
Full mode remains eager; no model-ID/gene/Parent collapse, 2-GiB restriction,
format/CLI change, or cap increase. Reuse `decode_rows` in `count_reference.cpp`
and raw/ZSTD selection in `count_reference_builder.cpp`; retain wrapper, 31-bit/
raw-payload flags, and prepared writer level-3 exactly; native uses level 1 and
fresh selected payload bytes avoid compressBound capacity. Authorized worker
files are `src/pod_row_codec.hpp`, `src/count_reference.cpp`,
`src/count_reference_builder.cpp`, `src/native_count_reference.cpp`,
`tests/vg/native_count_query_test.cpp`, `tests/vg/pod_row_codec_test.cpp`, and
`tests/vg/CMakeLists.txt`; root alone runs Ninja. Required tests cover raw/
compressed/empty/size/truncation/corruption/high IDs/sorted posts/getter reuse/
reset decode without FastLocate/lifetime/concurrency/rollback. Node compression
ratio/CPU benefit is unmeasured. Producer emission is read-group, not position,
ordered; locality batching is read-only architecture evaluation only. Future
candidate launch still requires C terminal exact parity, new full tests, and a
fresh checkpoint. Whole count and <2h/<300GB proof remain absent.

**D launch and old enumerator terminal reconciliation (2026-10-31):** Root
launched D at `2026-10-31T21:55:51.499119795Z` as
`pancollapse-node-score-compressed-first1000000-20261031` (container
`1f3e71b622a195c0750430abcb42ce62cc4eee9f10c8273a57a95c0ce24a4073`).
`launch-receipt.json` records actual 300,000,000,000-byte memory/memory+swap
(no swap), 128 CPUs (128,000,000,000 NanoCPUs), network none, image ID, and verified native PID 8/host
3461347 command/executable. Restored-default build session 55946 exited 0 and
focused passed 10/10 in 1.35 s (`ctest-default-restored-focused.log`); branch
head remains `327b048` with normal 8/8/0.5-GiB defaults. Index/XG took
5.0188/7.21022 s; GBWT/RI was pending at last poll. D has no EOF claim.

Old B is terminal, not live: Docker exited 127/no OOM at
`2026-10-31T21:45:50.925503247Z` because frozen wrapper post-validation used
bare Python, while `timing.tsv` records native count exit 0 after 25,000
records/10,000 groups, 14 drops, 13,904 counts, and 13,995 molecules. Existing
watcher exited 0; explicit-Python B count/MEX validations and
enumerator-versus-DP `node-score-dp-vs-enumerator.parity.json` passed all ten
files/scientific counters. B `terminal-receipt.json` records the native/wrapper
distinction and 132,202,708,992-byte cgroup peak. This is 10k output parity only,
not F1 or CR7/Cell Ranger parity. D monitoring remains the safe next task; whole
66.6M <2h/<300GB goal remains unmet.

**Pre-implementation per-key singleflight checkpoint (2026-10-31):** Frozen D
remains live after about 90k groups/148.093 read seconds, no OOM; its EOF/parity
is still required before any future candidate. Read-only receipts
`cache-wait-diagnostic-receipt.json`, `thread-wait-snapshot-{1,2}.tsv`,
`cache-lock-layout-check.tsv`, and `cache-wait-attribution.json` attribute 86/134
and 67/134 advancing-read-phase threads to exact node/edge/splice shard mutex
words (six other futexes each). Object base was read only for the three known
cache budgets (24 bytes); all waits are offsets 8/12 in actual shard mutexes.
Observer was network-none, PID-namespace/SYSPTRACE, with no attach, signal,
pause, profiler, or large hash. Current `get_or_build` holds shard exclusivity
through FastLocate/nested splice/node/edge builds; this satisfies Sol's acceptance
condition, but speedup and unrelated-versus-same-key fraction are unmeasured.

Approved scope is only `src/bounded_reference_cache.hpp` and
`tests/vg/bounded_reference_cache_test.cpp`: brief shard-lock flight install/join;
owner build and waiter waits outside all shard locks; atomic ready/FIFO/accounting
publication under shard lock before wake; one immutable same-key builder; error
fanout/retry; and oversized one-flight uncached behavior. Preserve the three-cache
acyclic dependency, capacity/hash/biology/scoring/profile/CB/UMI/locality, and
cumulative stats; define an explicit post-join production contract. Required
tests are same-shard different keys, same key, shared exception/clean retry,
oversized joins, and existing FIFO/lease/byte bounds. Root alone runs Ninja;
implementation has not started. The future 8M-entry cap experiment is separate
and must not combine here. Whole goal remains unmet.

**Ongoing per-key phase update (2026-10-31):** Root reviewed the checkpoint and
assigned Sol only header/focused-test implementation; no code result is claimed.
Accepted `hits` semantics are diagnostic-only: increment for ready returns and
in-flight joins at join time, including failed flights, with no scientific-counter
or new public-counter change. Deterministic tests require N-1 wait hits before
owner release plus bounded timeout cleanup. D remains immutable/live at 250,000
groups/350.581 s, sampled RSS 126,236,172,288 B, no OOM. Matched 220k evidence:
C/D read 313.674/313.449 s, node 49,247,789,088/13,335,578,885 B, edge
17,157,766,328/5,455,473,800 B, and both about 1.636M node builds. C/D edge
evictions are 4,305,615/0 but both edge queries about 7.45M: memory savings only,
not speed or repeated-query proof. D parity remains pending. Native init was
already complete after loading; no extra hash. The future 8M entry-cap experiment
remains separate after D/E, neither implemented. Whole goal remains unmet.

**Background sort and UMI-design checkpoint (2026-10-31):** The user authorizes
two bounded next tasks. First, inspect and, only after feasibility confirmation,
run a background position-sorted GAMP preparation on the preserved original
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/mpmap_pbmc1k_v3_20261027T122300Z/attempt5/full/alignments.gamp`.
The sort must preserve complete QNAME groups—every alternate record remains
together; it cannot treat records independently. Local `vg gamsort` source
supports GAM/GAF, not GAMP, so actual combined-vg support remains under
inspection and no sort is launched or implied. Second, analyze potential early
UMI-deduplication speed savings only. It does not authorize a frozen counting
policy, biology, scoring, CB/UMI, or output change. Compressed D remains live;
keyflight wide focused/full tests have terminal 0 and root is reviewing/staging.
Whole-goal feasibility remains unmet.

**D terminal / E prelaunch checkpoint (2026-10-31):** D exited 0/no OOM at
`2026-10-31T22:18:39.626954194Z` under actual 300-GB memory/no-swap limits.
It reached native EOF with 2,502,433 records/1M groups, 982 drops, 982,842
counts, and 1,302,814 molecules. Independent count/MEX validation and both
all-ten-byte/scientific parities (`parity.json` versus C and
`parity-vs-vector.json`) passed. Read/init/native-wall were 1087.0224/164.0927/
1348.4599 s; cgroup peak was 158,858,080,256 B. `terminal-receipt.json` records
the frozen node-score baseline result only, not F1, Cell Ranger, or whole-goal
evidence.

Frozen unlaunched E is `node-score-keyflight-first1000000-t128.KrM8LH`, differing
from D only by per-key flight. Its binary/source archive and runner are frozen;
focused/full tests passed 6/6 and 204/204, 100-cache repeats and TSAN/ASLR-off
passed, and normal 8/8-GiB plus 0.5-GiB assignment defaults were restored with
focused pass. E uses D parameters and must establish expected byte/scientific
parity, but is not launched. The 10k sort adapter round trip passed exactly in
`build/gamp-sort-check.C3cWqR`; final guard/toy tests continue and no full sort
is launched. UMI grouping is already required and implemented as exact
(profile, CB, feature, PackedUMI) aggregation after per-QNAME assignment, final
CB correction/1MM/cross-gene handling; analysis concerns upstream computational
reuse, not a missing count stage or policy change.

**E/sort/UMI live-state correction (2026-10-31):** The prior E wording is
superseded: at that checkpoint only E binary/source/logs were frozen; its
`run.sh` and `run-receipt.json` were created and frozen afterward. E is now live
as `pancollapse-node-score-keyflight-first1000000-20261031` (container
`042b057de24529656193368c2fa27280e45bdd7edbea6aba1cadaf4a28a44a37`), started
22:23:52.566 UTC, with Docker 300,000,000,000-B/no-swap/128-CPU limits and
cgroup `memory.max=299999997952`, `memory.swap.max=0`, `cpu.max=12800000 100000`.
It is loading indexes; no EOF/parity claim exists.

Background sort attempt 2 is live after the first attempt exited 126 from an
ENTRYPOINT double-bash before input use. It uses explicit entrypoint,
4 OMP threads/6-CPU quota/32-GiB no-swap cap; pack passed 1M reads/2,502,433
records/4,370,496,823 payload bytes. Output
`R/position-sorted-gamp.KhGLHP/position-sorted.gamp` is not published until
pipeline counters pass; final toy/empty/10k guards passed and malformed input is
rejected while unknown field 101 is preserved. Frozen UMI diagnostic
`R/umi-reuse-first1000000.NAojV1` is live in a 16-GiB/2-CPU container: 10k
groups/records/drop counters passed; 1M is pending. Exact raw UMI families are
not safe representative-read assignment. E timing is confounded by shared sort
storage and the 2-CPU diagnostic: establish parity first, not isolated A/B speed.

**UMI diagnostic terminal / E-sort live update (2026-10-31):**
`R/umi-reuse-first1000000.NAojV1` exited 0/no OOM at 22:25:09.307 UTC with
`memory.peak=3466788864`; `validation.json` passed 10k and 1M. Its
`first1000000.json` has 1M valid reads, 899,427 raw families, 100,573 repeated-
family reads, 999,018 eligible after 982 drops, 89,896 multi-read families,
37,856 mixed full-graph signatures, 5,447 same-sequence CB/UMI mixed families,
709,598 distinct graphs, and 938,970 distinct family+signature pairs. Identity
redundancy is 60,048 (6.0107%) within UMI family versus 289,420 (28.9704%)
globally. Sol's exact D-cache algebra finds 54,238 distinct unretained plus
2,903 repeated uncached entries: capacity-only additional-reuse ceiling 0.2906%
eligible, with actual reuse unknown because hits can be loser builds. This is
fixture-only theoretical redundancy, not speedup, safe representative-read
assignment, or biology/policy evidence. Diagnostic-only helpers are
`scripts/gamp_sort_envelope.cpp`, `scripts/gamp_umi_reuse.cpp`, and their CMake
targets; runtime count UMI handling is unchanged.

E completed initial loading in 152.925 s (native GBWT/RI/XG/index
146.852/32.157/6.851/4.815 s) and its latest count log has no EOF. Sort pack is
live at 3M groups/7,508,995 records/12,961,427,041 bytes, unpublished. Exact next
task is monitoring E terminal parity and sort completion/counters; no full-genome
count or goal claim exists.

**Reference-query hot-path analysis checkpoint (2026-10-31):** The unchanged
whole goal is 66,601,887 groups in <2 h and <300 GB memory; after initialization
it needs about 9,500 groups/s and is not met. E is fixed running to EOF/parity,
not restarted for a poll timeout: verified 50,000 groups/49.3897 read seconds
(about 1,012/s), initialization 152.925 s, live/no OOM. Sort remains live at 3M
groups. The authorized next phase is bounded analysis of existing M gprofng
evidence: `FastLocate::locateNext` 36.22% inclusive/9.74% self,
`Index::path_row` 8.54% self, `model_ids` 8.32% self, and `build_node` 34.23%
inclusive/4.94% self. Investigate exact direct numeric model/path-row lookup and
duplicate path-model work only. No annotation biology, containment rule, startup
reference preprocessing, heuristic cap, or patch is authorized before Sol and
root review. No new experiment or hash is authorized.

**Pre-implementation distinct-document node-builder checkpoint (2026-10-31):**
Sol approved one coherent node-score-only change, accepted by root but not yet
implemented. `path_row` is already direct mmap `uint32[gbwt_path]` with
380,460,061 entries/190,240,849 rows and model IDs CSR. New node-score query
would use `FastLocate::find/locate` unique sequence IDs while retaining
state-size raw-occurrence statistics; sort/reduce annotated row+orientation,
call `model_ids` once per row, then flat-vector sort/reduce raw
pair/record-ID/flags/support conflict and score count by distinct row. Full legacy
mode and lazy `node_scoring` raw requery remain unchanged. No index format,
startup join, biology, approximation cutoff, or avoidance of per-occurrence
FastLocate cost is authorized. Required evidence: repeated node/path/both
orientations, exon+body pair, unannotated paths/supports/malformed bounds,
full-vs-node-score getter parity, full 204 suite, all-ten-file/scientific parity
versus E, and 300-GB cap. It is not expected to provide 9x speed alone: old
profile bounds metadata plus builder are about 22% self, so the goal remains
unmet. E stays immutable to EOF and sort stays background.

**Read-only legacy-work audit checkpoint (2026-10-31):** User requests a
simplification audit, beginning with a legacy visit-count map that node-score
scoring may not use. Root may scan the active count/node-score path for obsolete
executed work and retained structures versus legacy-only/convert code, recording
exact callers, consumers, and cost bounds. This is not authorization to remove
counting rules or code: CR7/custom scientific branches are not presumed legacy,
and no broader implementation follows. The next authorized action is an audit
report, not a purge. The already-approved distinct-document worker continues only
in `native_count_reference.cpp` and `native_count_query_test.cpp`; its work must
not be reverted. E and the background sort continue unchanged.

**Production-surface retirement checkpoint (2026-10-31):** User explicitly
overrides prior legacy preservation: production is `index` plus native `count`;
everything else is default-off or deleted, recoverable through Git if needed.
Plan retirement of convert, BAM/reference-walk assignment, compatibility replay,
raw-GFF count, and old Ex50 resolver routes rather than retaining new legacy
flags or active branches. This does not retire frozen biological profile, CB/UMI,
node-score, or directed-splice rules. Safeguard dirty/untracked work outside the
production surface before deletion (existing R snapshots assist); no commit or
reset. Active index/count tests and parity goldens remain, with frozen binaries
as external comparators. The bounded node-builder worker finishes its patch and
full-mode parity first; root then coordinates deletion. E/sort runners remain
unchanged until a tested replacement. Next authorized task is Sol's bounded
extraction/removal architecture with caller/dependency checks—not a mass purge.
No release/restart is authorized; whole-goal feasibility remains unmet.

**Converter-preservation correction (2026-10-31):** User supersedes converter
deletion only. Supported production commands are `index` and native `count`;
`convert` remains as a third, explicitly **deprecated** BAM-conversion command.
It is not a new legacy flag or a supported production count path. Retirement
continues to target obsolete count engines, compatibility replay, and
count-startup reference-walk work, without biological-rule change. The node-builder
worker has finished its two owned files pending root review/build; E and sort
continue. Next architecture review is minimal actual count-legacy deletion while
preserving deprecated `convert`.

**E terminal / F-freeze and purge sequencing checkpoint (2026-10-31):** E
native count exited 0: `timing.tsv` spans 950.123016 s, manifest initialization/
read are 156.876415759/705.031969301 s, and cgroup peak is 158,946,189,312 B.
Independent count/MEX validation and both all-ten-file/scientific parities passed.
`terminal-receipt.json` records one-million-read frozen node-score evidence only,
not a whole-genome/F1/Cell Ranger timing claim. Background sort remains live at
9M packed groups.

The distinct-document node patch finished; root removed temporary reduce-vector/
dead checks. A stale reverse+body fixture expectation was repaired to exact
F2/R1 and F1/R0; focused repair passed 5/5 and full 205 is running in root's
session. Deprecated-convert help/warning/README and `convert_deprecation.cmake`
test pass; no obsolete count code is removed yet. Pre-purge safeguard
`R/pre-count-legacy-purge.DfQFWi` holds HEAD/status JSON, tracked patch, and
complete 4.7-MB source/tests/docs/scripts archive without a new hash. After F
freeze, Sol's accepted plan is to extract shared projection/ResolvedAssignment,
then remove legacy CountReferenceResolver/rawcount/compatibility/expanded-index
production routes while retaining deprecated BAM `convert`, active profiles,
CB/UMI/splice rules, and goldens. Do not mix the hot-path F comparison with this
later purge until freeze and normal restore. Whole <2h/<300GB goal remains unmet.

**F distinct-document 1M prelaunch checkpoint (2026-10-31):** Frozen F is
`R/node-score-distinct-doc-first1000000-t128.GEL63B` with binary and
`source-distinct-doc-wide.tar.gz`. Normal and wide focused tests passed 5/5 and
full suites 205/205. Logs retain the initial stale fixture assertion and missing
`umi_tools` Python-path failures; corrected complete runs passed with no disabled
tests. Source defaults are restored to node/edge 8/8 GiB and assignment 512 MiB;
root rebuild/focused session 69481 remains pending. F is unlaunched and changes
only distinct-document node construction; convert-label changes are non-counting.

F uses E's 64/16/4-GiB experimental capacities, 128 threads, 300e9-byte/no-swap
cap, same 1M input, two profiles, node-score, 64M cutoff, and independent output/
ten-file parity gates. E terminal parity passed. Background sort remains live
(last 12M groups), so F timing is not isolated A/B evidence. After restored-
default success and review, the next authorized action is F launch. Broader count
purge stays separate after this freeze; third `convert` stays deprecated and kept.

**F launch / native-only cleanup checkpoint (2026-10-31):** Restored-default
focused tests passed 5/5. F launched from its frozen binary/source as container
`pancollapse-node-score-distinct-doc-first1000000-20261031`
(`4be03058dab99db2a14c3771efa3000d06285a23a89feaad806dbd5ae0a4ad17`).
Verified cgroup limits: memory 299,999,997,952 B, swap 0, CPU quota 128 cores.
Native PID 8 is loading references; no count result or F parity yet. The separate
background sort remains live. Native-only cleanup now begins in disjoint owned
files, after the freeze: shared policy projection/prepared runner and CLI route
retirement. The third `convert` command and its BAM behavior remain deprecated,
not deleted. No frozen profile or biological rules change.

**Current cleanup/research gate (2026-10-31):** The whole-GAMP goal remains
66,601,887 read groups in <2 h and <300e9 B; it is unmet and no whole count is
launched. E is terminal one-million-read evidence only:
`R/node-score-keyflight-first1000000-t128.KrM8LH/terminal-receipt.json` records
native exit 0, 950.123016-s wall, 705.031969301-s read time, 158,946,189,312-B
peak, and passed independent count/MEX plus both ten-file/scientific-counter
parities. F remains a separate frozen distinct-document comparison at
`R/node-score-distinct-doc-first1000000-t128.GEL63B`, live under
299,999,997,952 B/no swap/128 cores; the latest read-only observation is
320,000 completed groups at 228.906 s, with terminal completion/parity pending.
The
position-sort container remains live; `R/position-sorted-gamp.KhGLHP/pack.jsonl`
last records 17M packed groups, with no final output.

Pre-cleanup means code removal only, never data or biological filtering. The
implemented cleanup is NativeSource+NodeScore-only prepared count with verbatim
shared metadata projection extraction; the main worker is physically deleting
entangled unreachable count/replay branches while preserving converter behavior.
Root migrated integration tests to fixed pre-refactor node-score fixture
expectations and retained converter tests. No build/test has yet covered this
refactor; its component build exposed only a missing explicit vg protobuf include
(fixed) and a stale native bool in `prepared_count` (root fixing). Root must run
the full suite after source freeze. `convert`
remains deprecated, while obsolete count engines are deleted rather than hidden
behind flags. Dirty/untracked pre-purge material is safeguarded outside
production at `R/pre-count-legacy-purge.DfQFWi`, with no hash/reset/commit.

After refactor verification and F parity, a bounded read-only native-lookup-cost
profile may be reviewed for exact runtime optimization; it authorizes neither an
algorithm nor biology change. Before any UMI decision, research/review
evidence-aware pangenome UMI-family inference, consensus/representative
selection, locus+CB+UMI grouping versus post-gene counting, and current
implementation. Do not frame UMI dedup as randomly selecting a read. This is a
research gate only, authorizing no UMI algorithm, grouping, or biological-policy
change pending root review.

**Component validation / UMI timing evidence (2026-10-31):** Shared projection,
native query, node-score resolver and DP differential tests passed 4/4 after
fixing the explicit protobuf include and stale native-only progress guard.
The pruned native-query test then passed again after restoring its shared
PathRecord fixture helper. Obsolete expanded builder/resolver source files and
resolver-only tests are removed, recoverable from the pre-purge archive; the
converter-only main extraction is still in progress and full-suite validation
remains pending.

The user asks whether family inference, deduplication/correction and consensus
belong before counting or alignment, not whether to choose a random read.
Verified `mpmap_pbmc1k_v3_20261027T122300Z/attempt5/full/command.json`,
`input_stats.json` and `code/prepare_10x_whole_genome_mpmap.py:186`: preparation
copied raw 16-base CB, 12-base UMI and quality bytes into QNAME and emitted every
R2 unchanged. All 66,601,887 pairs entered mpmap individually; no prealignment
family grouping, deduplication or consensus occurred. Current native runtime
does molecular aggregation/error correction after graph assignment. A research
agent consulted primary Cell Ranger 7.2, UMI-tools, Alevin/alevin-fry and BUS
sources; timing/design interpretation is ongoing, with no new UMI rule adopted.

**Native-only refactor verification / next implementation gate (2026-10-31):**
Root's full refactor build and remaining 168 tests passed in 27.12 s
(`build/ctest-native-only-full.log`); `native_count_verify`,
`convert_deprecation`, and `count_cli_surface` also passed. Immutable snapshot
`R/native-only-refactor-check.SYZ2ls` contains `panCollapse`,
`source-native-only.tar.gz`, logs, and `receipt.json`, which records 168/168,
unchanged node-score/frozen profiles, and no recomputed hashes. Main is about
4,880 lines with no active old count/replay branch; expanded builder/resolver
sources are physically deleted, while RAD/BAM `convert` remains. The new binary
has not had a 1M benchmark; the whole goal remains unmet.

Sol/root accepted the exact ordinary-body witness accelerator as the next
authorized implementation, distinct from CLI removal and UMI science. For valid
trusted native inputs, current splice semantics are `E intersect C minus B`:
empty `C`, or witnessed `C` subset `B`, proves an empty splice-target set;
otherwise use the complete existing fallback. A raw-SA scan receives a global
target-scaled budget `max(256, 4*C)` capped at 4096; it is only an accelerator,
never a read/evidence drop. Resume prefix/full RI cursor behavior must avoid
rescanning on fallback and preserve thread-local synchronous edge-cache building
and splice per-key singleflight. Local GBWT `FastLocate` find/unpack/locateNext
APIs were checked. Incidental malformed-RI tail detection may be skipped only in
the bounded shortcut; no new full-scan validation, hash, CLI mode, biological
profile, or UMI resolver is authorized. Test-only probe limits 0/1 may force
fallback. Worker scope is native cpp/hpp/query tests; root owns prepared-count
stat printing and assignment-cache width. Keep 8/8-GiB source caches and 512-MiB
assignment cache until patch tests pass; then use tested F 64/16/4-GiB capacities
for 1M/whole operations under the unchanged external 300-GB cap. Required gate:
full 168 plus forced-probe cases before the changed-binary 1M benchmark;
1M ten-file/scientific parity versus F and resource/throughput evidence before
a whole-GAMP launch.

The Alevin clarification remains independent: molecule-level resolution is
appropriate, but PUG+EM versus family argmax is an unresolved scientific choice;
no resolver is adopted.

**Ongoing witness implementation reconciliation (2026-10-31):** Exact
ordinary-body witness cpp/hpp cursor resumption is now implemented; its worker
tests remain pending, and root will build. The authorized sequence is witness
tests/full suite, then changed-binary same-1M parity against frozen F (all ten
files/scientific counters) plus resources/throughput, then the unchanged whole
66,601,887-group <2h/<300GB gate—no smaller-goal substitution. No whole run or
new speed measurement exists.

The user-proposed lazy wrong-way-splice check for only competitive alignments is
not adopted. Root/Sol find it sound only after complete certification across
ties, score window, support, antisense, and rejected-diagnostic behavior; it is
therefore a review condition, not implementation or measured speedup. Defaults
and biological choices stay unchanged. Native `count` is not PUG: `convert`
already provides `map.rad`, and the full-168 interoperability coverage is
cr-like rather than replication of original Alevin parsimony. PUG+EM versus
family argmax remains unresolved and independent of this exact optimization.

**G body-witness prelaunch checkpoint (2026-10-31):** Default witness validation
is terminal: all 168 tests passed in 27.63 s
(`build/ctest-body-witness-full.log`). Root corrected common-empty success,
immediate complete-body proof, the cursor unordered-map comparator, and sorted
common-body intersection. The scoped source change is native cpp/hpp/query test
plus prepared-count operational statistics; Sol found the RI cursor/prefix/
synchronous-cache invariants sound in principle with no blocker. Matched-F-cache
wide (64/16/4 GiB) build is complete. Root verified its terminal 168/168 pass in
27.25 s at `build/ctest-body-witness-wide-full.log` before launch.

G is prepared, not live, at `R/node-score-body-witness-first1000000-t128.FCVKgz`
with `run.sh`, F/counts comparator, immutable matched-cache binary/source/logs and
`run-receipt.json`. Worktree cache defaults are restored to 8/8 GiB and 512 MiB;
the frozen executable and archive retain the tested F-matched 64/16/4 GiB values.
The next gate is the same
1M input at 128 CPUs and 300e9 B/no swap, followed by independent validators and
all-ten-file/scientific parity. No Docker launch, whole run, performance claim,
or change to profiles/biology/UMI model is recorded. Whole <2h/<300GB remains
pending.

**G body-witness 1M live (2026-10-31):** Root launched
`pancollapse-node-score-body-witness-first1000000-20261031`, ID
`4f2744cde4538aaa84111088af90e24e59522a6f909d37c41092f9a0706a455d`,
at `2026-10-31T23:19:45.976220066Z`. Native PID 119155 was live; cgroup confirms
`memory.max=299999997952`, swap 0, and 128-CPU quota. Frozen G contains source,
binary, receipt and both passing full-suite logs. Initial log shows all four
reference branches loading in parallel. No performance or parity result yet;
whole input remains unlaunched and the whole goal unmet.

**Endpoint-free DP specialization pre-edit checkpoint (2026-10-31):** Sol/root
reviewed the bounded proof. For a coalesced group whose unchanged complete
reachable-edge prepass yields an empty final `SpliceOp` vector, every internal,
cross-piece, and empty-bridge transition permits both final strands. The group is
therefore certified endpoint-free: `StateKey.last` remains null and `valid`
remains mask 3, so last tracking and allowed/internal-mask work may be skipped
while retaining scores, exon/strand balance, body, bases, support, and normal
merge. Restrictive groups remain unchanged. No fallback, biology, molecule
policy, preliminary-winner certificate, or cross-record cache is introduced.

The next worker owns only `node_score_resolver.cpp` and
`tests/vg/node_score_dp_test.cpp`; root builds. Required acceptance is independent
oracle parity; empty/insertion bridges; explicit mask 3; restrictive internal/
bridge refusal; variable-support tie union; record-local masks; both strands and
Gene objective; reduced targeted-toy DP states; full 168; and existing 320 seeded
cases. Frozen G is a running reference witness benchmark (latest 10k 12.7004 s),
not a performance conclusion; new source does not alter frozen G. No adaptive
cache work is implemented from the cache question. The whole <2h/<300GB goal
remains the only production goal.

**G live CPU-profile / cache requirements checkpoint (2026-10-31):** Endpoint-
free remains unpromoted: its unlaunched cpp/tests patch passed focused 2/2
(`build/ctest-endpoint-free-focused.log`), but the user directs keeping current
code running and profiling CPU because the certified case may be rare. No
endpoint-free performance conclusion, new algorithm, or further implementation
is authorized before profile interpretation.

Frozen G is live at 420,000 groups/271.038 read seconds. Separate profiler
container `pancollapse-live-cpu-profile-G-20261031`
(`13cf850f726f0d41a92436cf6abd89d846ce64ee71cee844e91bcc28654d4c08`) attaches
to shared target PID namespace process 8/host 119155; artifacts are
`G/cpu-live-profile.5WMEZo`. It uses perf 6.8.12, `cpu-clock:u` at 19 Hz,
DWARF 4 KiB, 30 s, and a separate 4-GB/4-CPU PERFMON+SYS_PTRACE container, with
no host sysctl change or target restart. `perf.data` was confirmed growing; its
report is pending. Do not use post-attach G timing as an unprofiled baseline.

The user requires future cache limits to be configurable, scale to the overall
target, and be generous, removing arbitrary fixed ceilings. That implementation
has not started and establishes neither an optimal allocation nor a new default.
The whole <2h/<300GB goal remains pending.

**G terminal live-profiling evidence (2026-10-31):** G remains live at 700,000
groups/412.740 read seconds; current RSS is 149.7e9 B, cgroup current 140.5e9 B,
and cgroup peak 140.7e9 B. Initial stack profile
`G/cpu-live-profile.5WMEZo` is excluded from percentages: its 336.979-MB
`perf.data` lost 198,970 samples (71.96%), and its inclusive analysis stopped
the report container only, never count.

Valid repeat `G/cpu-live-flat.BMof18` completed record/report status 0 with
49-Hz `cpu-clock:u`, no callgraph, 30 s, 8.433-MB data, 186K samples, zero loss,
and empty report stderr. Its self samples identify
`select_support_mcl::select` 21.64%, `FastLocate::locateNext` 10.90%,
`NodeScoreResolver::resolve` 8.60%, `int_vector::get_int` 7.49%, node-build
add-path lambda 5.14%, `Index::path_row` 4.46%, and `model` 2.66%; remaining DP
helpers account for other self samples. These are self, not inclusive shares and
do not establish that scoring is only 8.6% total. First-profile hardware stats
over 10.00975 s observed 124.906 CPUs, 0.72 IPC, 31.94% cache misses/reference,
and 75% multiplex coverage. Profiler overhead contaminates G wall timing; no
full parity, endpoint-free promotion, or cache implementation follows. The whole
<2h/<300GB goal remains unchanged.

**Configurable generous cache-limits preimplementation checkpoint (2026-10-31):**
To implement the user's configurable/scaled/generous requirement, root selects a
startup-only planner rather than a live adaptive controller. Pending
endpoint-free work remains out of the next binary; root will preserve/undo that
experimental patch. `count --memory-budget <bytes>` sets planning target
(default effective cgroup limit/available RAM); repeatable
`--cache-limit node|edge|assignment|splice=<bytes>` is positive-unit strict, with
duplicates rejected before GAMP. After reference load, measure RSS; reserve 10%
of target for workers/counts (heuristic headroom, not a hard aggregate cap or
spill), set cache pool `max(0, 90% target - RSS)`, and split unoverridden pool by
renormalized heuristic weights node/edge/assignment/splice 60/25/10/5. Explicit
overrides must sum at most pool. This is no optimum or upfront-allocation claim.

Remove the practical assignment-entry ceiling; it is byte-only. Configure limits
only at startup with empty caches/no flights, holding all shards and validating
before a budget change; hot resize is forbidden. Logs/manifest report target,
postload RSS, reserve, pool, and actual caps. Limits do not guarantee RSS; the
external 300-GB cap remains authoritative, and defaults account for shared
cgroup/MemAvailable. Required acceptance: pure parser/planner overflow,
duplicate, partial/all override, rounding, and insufficient-headroom cases;
configure-after-query rejection; tiny/default/huge count scientific parity; and
full suite. Frozen G remains running (last 999,998 groups/570 s), with full
parity pending; no wait is needed for this documentation gate.

**G terminal / cache-planner integration checkpoint (2026-10-31):** G exited
0/no OOM. `G/terminal-receipt.json` records 801.679596-s wall (13m21.68),
140.956499312-s initialization, 588.488848619-s read, 2.920263331-s UMI final,
154,255,171,584-B peak, and EOF at 2,502,433 records/1M groups with 982 drops,
982,842 count rows, and 1,302,814 molecules. Count/MEX validation passed;
`parity-0.json` passed all ten byte comparisons and scientific counters,
summaries, and logical tables against F. Profiling and concurrent sort
contaminate timing, so this is not isolated A/B performance evidence, nor
whole-genome, F1, or CR full-parity evidence.

Endpoint-free remains unpromoted and preserved externally at
`R/unpromoted-endpoint-free.KrSF4h/source.tar.gz` with focused logs; root undid
its cpp/test changes, so the next binary uses the same G scorer. Configurable,
generous memory-budget/cache-limit CLI/helper/runtime integration is implemented
by root/worker; 169-test build/full suite logs
`build/build-memory-scaled-caches.log` and
`build/ctest-memory-scaled-caches-full.log` remain pending. No full-GAMP
production launch or whole-goal claim is authorized; <2h/<300GB remains unmet.

**Whole memory-scaled live launch checkpoint (2026-10-31):**
`R/whole-genome-memory-scaled-t128.XOrxaC` is frozen: `source-memory-scaled.tar.gz`
was created; binary matches `build/src/panCollapse` by `cmp`; binary/archive/run
scripts/prepared receipt are read-only. Container
`pancollapse-whole-genome-memory-scaled-20261031`
(`68825f7852326af087266ebb1abec3114a564efeceb81f85376bb52ce0d656a4`) started
at 2026-10-31T23:43:46.622153755Z; host init PID 236004, native host PID 236044.
Cgroup has memory.max 299,999,997,952 B, swap 0, 128 CPUs, and all
`memory.events` zero. Parallel native.pci/XG/GBWT/RI load has begun; it is not
yet streaming. Input is the same tracked 66,601,887-read/30,326,279,277-B full
GAMP, retaining the 27-Oct/30-Oct provenance caveat; no profiler is attached.
G 1M remains a control only, not full proof. Next verify ready, memory plan,
read rate, EOF/group count, validators, and resources; launch does not meet the
target.

**Live whole performance-investigation checkpoint (2026-10-31):** Whole frozen
container `68825f7852326af087266ebb1abec3114a564efeceb81f85376bb52ce0d656a4`
remains unchanged and was still loading GBWT at 23:45:07Z; no new streaming rate
exists. It is not close to demonstrated <2h feasibility: recent G 1M read rate
was about 1,699/s versus about 9,250/s minimum (about 5.5x gap). An ~11-h linear
projection is not a full-run ETA. The live whole attempt continues unchanged.

Next authorized work is read-only: investigate exact neighboring-node membership
reuse, using an occurrence-bijection certificate, to avoid redundant RI occurrence
decoding and measure prevalence before any code promotion. This follows valid
profile self samples (`select` 21.64%, `locateNext` 10.90%, `get_int` 7.49%);
main scorer 8.60% self is not inclusive. Root must obtain consequential proof
review before implementation and retain frozen G 1M outputs/full 169 tests.
Measure live cache-warm throughput first. No shortcut, endpoint-free promotion,
biology change, memory-target relaxation, or whole restart is authorized here.

**Neighbor-membership pre-diagnostic-code checkpoint (2026-10-31):** Sol/root
accepted the universal physical-adjacency empty-splice certificate under the
existing trusted valid bidirectional GBWT/RI contract: if either orientation of
either endpoint has nonempty outgoing successors and all successors are the
physical other endpoint, with no endmarker, every body target in `C` has ordinary
adjacency `B`, so `E intersect C minus B` is empty. Starts, ends, repeats, mixed
orientation, and missing annotation rows are covered; `NativeSource` load
enforces bidirectionality. G 1M previously had 8.92M edge queries and 3.855B
probe-SA steps, but this and neighbor reuse alone cannot close the ~5.5x gap;
45.17% profiled time gives 1.82x ideal Amdahl speedup.

Authorize only a standalone no-RI 10k prevalence diagnostic: mirror resolver
endpoint propagation, report unique physical pairs/query-weighted prevalence,
cross-check record predicate against exact GBWT find counts, potential avoided
edge occurrences, and bounded aggregate logging. No production scorer/cache
edit, promotion, full restart, or hash is authorized. Whole stays live above
260k at ~1,820 recent reads/s under 300GB/no swap/128 threads. Read-only NUMA
observation (anon N0 ~34.9 GiB, N1 ~76.4 GiB; ~69% N1) is not remote-cost proof;
no affinity change. Suffix saturation needs separate proof; endpoint-free held.

**Sample-dependent DP threshold-saturation implementation checkpoint
(2026-10-31):** Supported by new Sol proof, root authorizes semantics-preserving
automatic saturation under existing performance authorization; frozen live-whole
binary remains unchanged. Per coalesced group/piece use exon margin
`2*exon-bases` and forward-minus-reverse strand balance; reverse-DAG suffix
bounds `[L_j,U_j]` include successor `j` across all completion routes. Before
every start seed/successor merge, canonicalize entering balance `b`: `b>=-L_j`
to `-L_j`, `b<-U_j` to `-U_j-1`, otherwise exact. Exon/strand are independent;
last, valid, all-exonic, body, bases/objectives/support and other flags remain
unchanged. Sol's sequential-quotient proof is exact.

Wide-128 preflight requires original signed bases/margins representable, exon/
strand sum-absolute piece deltas <= `(INT64_MAX-1)/2`, and score/gene sumabs <=
`INT64_MAX`;
unsafe groups use unchanged DP and preserve original invalid errors. Automatic
only: force-map/sparse/enumerator controls unchanged. A compact private helper
is permitted for boundary tests, no public CLI. Required: 320 seeded-oracle
parity; threshold/overflow/multistart/mask/support tests; full suite; real
same-10k/1M parity before promotion; then state/time measurement without a 5.5x
assumption. Adjacency worker separately runs prevalence diagnostic before any
production shortcut. This does not endorse endpoint-free.

**Threshold/adjacency real-gate prelaunch checkpoint (2026-10-31):** Threshold
balance helper is implemented in `src/threshold_balance.hpp` with resolver and
prepared logging; pure tests plus variable-length positive-balance fixture retain
enumerated counts and reduce states >3x versus force-map. Focused DP/helper,
including 320 seeded cases, and final full 171/171 passed in 30.07 s
(`build/ctest-threshold-full.log`). Initial adjacency diagnostic API/fixture
defects were corrected (edge pair, histogram/count sums, separated reversed
fixture, GBWT dynamic include); its selftest now passes and full 171 includes it.

Frozen unlaunched A is `R/adjacency-prevalence-first10000.yxZIo8`: helper/cpp/
run script, GBWT-only/no RI, no GAMP count, 10k structural-pair upper bound,
128GB/no swap/4 CPU, aggregate JSON, and no speed claim. Frozen unlaunched T is
`R/node-score-threshold-first10000-t128.bDX1Cb`: binary/archive/logs/run script
read-only; same 10k, both profiles, MEX, 128 threads/300GB/no swap/memory target;
independent validators plus ten-file/scientific comparison against frozen static
cache result. Production adjacency shortcut is not implemented. Root review and
capacity check precede A/T launches. Whole W remains unchanged (>1.4M reads,
~2,100 recent/s, ~180GB); concurrent whole/sort makes T timing nonisolated. T
parity is not 1M/F1/CR proof; 1M parity is required before promotion/restart.

**Threshold/adjacency real-gate live checkpoint (2026-11-01):** A is live as
`pancollapse-adjacency-prevalence-first10000-20261101`
(`ac4edd70cc1f91192e7519ce85b0d0c0122852eae600c78947c95306f8f91fa5`), started
00:00:45.322173614Z; host init 313521/helper 313557; 128GB/no swap/4 CPUs,
GBWT loading. T is live as `pancollapse-node-score-threshold-first10000-20261101`
(`b456b6f2d1615859163dee3d39779ae3bd0cfb9cb20dff844b0d20d8860ab1f0`), started
00:01:03.719884679Z; host init 317333/native 317369; memory.max
299,999,997,952 B, swap 0, CPU quota 128. Source/binary/scripts are frozen.
Sol accepted actual-code proof conformance without blocker: unsafe fallback keeps
original invalid errors; potential linear-prepass overhead is unproven and added
resolver exception testing is optional. Await A JSON prevalence and T EOF,
validators/parity/state/timing. No promotion/restart/goal claim. W remains
unchanged >1.52M/~2,300 recent/s/~188GB peak; W/T share reference loads and
background sort, so timing is not isolated.

**A/T terminal / adjacency implementation checkpoint (2026-11-01):** A exited
0/no OOM: 179.341119-s wall, 81,317,462,016-B peak; 10k groups/25k records/14
drops; 131,436 unique pairs/111,135 certified and 170,183 weighted/144,613
certified (~85%); four edge occurrences 239,596,894/204,572,195 certified
(~85.4%). Direct-record versus GBWT-find cross-check passed; load/scan/certify
were 170.278/5.56907/0.524194 s. This is a structural superset, not actual count
work or CPU speed evidence.

T exited 0/no OOM: 219.087795-s wall, 175.339366509-s init,
29.650317813-s read, 0.039151048-s finalize, 133,459,050,496-B peak, 13,904
counts/13,995 molecules. Validation/MEX and `parity-0` all-ten files, scientific
counters, summary and logical tables against static control passed. DP states
were 49,948,641 versus old 10k 1,258,500,869 (~25x), max frontier 69 versus
1411; all 158,551 groups saturated, zero unsafe. Cache races slightly change raw
resolver call work; concurrent timing is not a 2x claim. Whole W unchanged at
2.175M/~2k/s/no OOM.

Root authorizes node-score-only universal adjacency fast return after same-node
early return and both node-evidence validations, before common intersection/SA
probe: either anchor orientation has nonempty all-non-ENDMARKER successors equal
to physical other endpoint. Facts/profiles/strand rules remain unchanged. Add
operational checks/success stats and test-only disable bool after existing load
max-probe argument; no public CLI; full mode remains control. Root owns
production files; bounded query tests follow. Require adversarial/random
predicate-native-full parity and all 171, then one combined threshold+adjacency
1M ten-file parity against G/F before promotion. No separate T 1M/10k, restart,
version, policy, or memory change.

**Complexity/SIMD read-only review checkpoint (2026-11-01):** User requests
overall-complexity and SIMD assessment; root pauses further production edits/new
fast paths. Combined threshold+adjacency build passed 171/171 in 34.07 s
(`build/ctest-threshold-adjacency-full.log`). Root fixed a query-fixture
assertion: ordinary small pairs already have old probe edge 0; a 512-unannotated-
path-heavy fixture now forces probe exhaustion and proves certificate zero SA/edge
versus baseline >0. Full parity covers 440 ordered fixture pairs, 32 random
predicate-FM checks, endpoint-invalid case, and cold four-client case.

Combined 1M `R/node-score-threshold-adjacency-first1000000-t128.zLDSex` is
empty/prepared only: no binary frozen or launch. The requested review will assess
algorithm cost, maintainability/minimal data structures, and vectorization fit
from actual profile on AMD EPYC 7713 (AVX2, no AVX-512). No SIMD implementation,
scientific/UMI policy change, promotion, whole restart, or review experiment is
authorized. Whole W continues unchanged (3.27M, no OOM); A/T terminal evidence
remains applicable.

**Whole memory-scaled prelaunch checkpoint (2026-10-31):** Cache configuration
full suite passed 169/169 in 27.67 s
(`build/ctest-memory-scaled-caches-full-rounding.log`), including pure planner
rounding/minimum-1-byte correction, invalid-option diagnostics, and
`native_count_verify` tiny-1-B/default/generous-64-MiB fixture scientific parity.
The startup memory target/caches are configurable and the old 655,360-entry
assignment ceiling is removed. Root's engineering decision requires no new 1M
run for this cache-retention-only change: scorer is restored G and G already
passed 1M ten-file parity.

Whole attempt is prepared, not launched, at
`R/whole-genome-memory-scaled-t128.XOrxaC`, which contains binary, build/test
logs, `run.sh`, `run-count.sh`, and `run-receipt.json`. It will use current
tracked full/pilot `alignments.gamp` only (30,326,279,277 B; 66,601,887 reads),
pending the asynchronous 30-Oct versus recorded-27-Oct provenance question; it
does not introduce a dataset. Plan: 128 threads, 300e9 B/no swap,
`--memory-budget 300GB` with automatic generous post-load caps, both profiles,
MEX+Parquet, 64M complexity guard, and no profiler. Background sort remains
original-envelope transport. Following root review, freeze archive then launch;
actual Docker/PID/cgroup, EOF/group count, validators, and wall/RSS/cgroup
evidence are required before any throughput/goal claim. No more pilot runs; the
<2h target remains unproven.

**F terminal / lookup-design gate (2026-10-31):** Frozen F terminal receipt is
`R/node-score-distinct-doc-first1000000-t128.GEL63B/terminal-receipt.json`:
exit 0/no OOM; 884.117372-s wall (166.691148893-s initialization,
617.838627167-s read, 2.999921962-s finalize); 159,724,564,480-B peak; EOF at
2,502,433 records/1M groups; 982 complexity drops; 982,842 count rows; and
1,302,814 molecules. Validation/MEX validation and both all-ten-byte/scientific-
counter parities passed. This is one-million-read node-score evidence only, not
whole-genome, F1, or CR full-parity evidence. Sort remains live at 22M packed
groups, so F is not isolated speed A/B.

The 66,601,887-group <2h/<300GB goal remains unmet and no whole count launched.
Native components passed focused 4/4 and retired query 1/1; converter-only
extraction has since passed full 168/168 (the verification checkpoint above
supersedes this checkpoint's earlier pending status). UMI research
is complete as source findings plus verified preprocessing, with no policy change.
In parallel with converter-only cleanup, root authorizes read-only native-hot-path
correctness/performance design from frozen F source/tests/parity; no changed
binary launch precedes full build/tests. Review-only candidate:
`build_splice_evidence` exact ordinary-body witness short-circuit. It must prove
that all possible common body targets having ordinary adjacency in any orientation
implies an empty splice-target set under existing semantics. A bounded RI witness
scan plus fallback is an accelerator only; budget exhaustion cannot drop reads or
evidence. It is separate from CLI removal/UMI research and changes no biology or
eligibility rule.

On native successful exit and atomic output publication, validate completion from
existing receipts only: required `manifest.json`, `summary.tsv`, and the four
required Parquet tables (`barcodes`, `features`, `counts`, `molecules`); manifest
count-storage/profile/input/provenance fields and table schemas/sort contract;
and stable global/per-profile summary counters. These lightweight presence,
schema, and counter checks do not add full-file hash scans. The next authorized
task remains monitoring and diagnosing/fixing/restarting any failure without a
biological-policy change.

## Historical authorization checkpoint (2026-10-30)

The user now authorizes diagnosis, repair, preflight, and a restart without
further intervention, subject to a bounded technical review of the implementation
approach. This authorizes no biological-policy change. The original GAMP and GBWT
are preferred unchanged, and all immutable inputs must be preserved. The repair,
preflight, and restart have **not** yet been performed; no new container, count,
or published output exists.

The terminal count failed on a missing path, not OOM: its fatal ledger row
`chr2::HG00097#1#HG00097_ha_T0037310_R1` is byte-identical in the original and
later ledger. The diagnosed cause is the Oct 28 asymmetric qualification:
`qualify_ledgers.py:40-43` prefixes every ledger path, whereas
`repair_metadata.cpp:115` skips non-generic GBWT paths. The binding records
7,458,390 non-generic chr2 paths, exactly the difference between 190,240,849
ledger rows and 182,782,459 matched paths. The completed `.ri` artifact and its
source/hash receipts remain recorded below; it does not itself repair the
ledger/graph binding or complete a count.

Any restart must retain both profiles, 128 threads, and the 500,000,000,000-byte
hard memory and memory-plus-swap limits (no swap). It is not evidence of a
successful repair, fit, GAMP processing, or published count until its own native
receipts are complete. Do not record a specific repair design as accepted before
the bounded technical review concludes.

## Historical: standard FastLocate `.ri` built; earlier namespace-repair mismatch diagnosed (2026-10-30)

The `.ri` container exited 0 at `2026-10-30T16:03:46.588726829Z` (09:03 PDT).
Published artifact: `pancollapse_transcript_ri_20261030_eAma1l/joint.transcripts.ri`
under the result root below, 44,433,079,158 bytes, SHA-256
`7c6f0526b35ccf6ce744f026da89e9203a2f3e7d8664691054dd2dcaaab711bd`.
Source GBWT SHA-256 is
`165d0a427a82eec08310b7f2306d6cb2dbdadc89a60bbbcab67c2f3224086961`.
Builder reports 3,610,110,443 logical runs and 1,812.16 s indexing; native
FastLocate construction was 1,560.69 s. Full query verification and a PanCollapse
`.ri` loader remain absent; artifact generation is not count completion.

**Read-only cause audit:** binary-search lookup in both the original graph-side
ledger and the later count-repair ledger finds the fatal row byte-identical,
including `vg_path_name`. Both ledgers retain 190,240,849 rows. The earlier
Oct 28 graph namespace repair is inconsistent: `qualify_ledgers.py:40-43`
prefixes every ledger path with `chromosome::`, while
`repair_metadata.cpp:115` skips non-generic GBWT paths. Source and binding are
under `/mnt/ssd/lalli/hprc_v2_vg_rna/notes/joint_transcript_gbz_20261028_repair/`.
`BINDING.json` records 380,460,061 total paths and 373,001,671 generic paths;
the difference, 7,458,390, exactly equals 190,240,849 ledger rows minus the
182,782,459 matched paths. The ranges put all non-generic paths in chr2.
This identifies the earlier name-qualification asymmetry, not recent string
interning/flat-storage changes or biological/row removal, as the cause mechanism.
A complete per-path counterpart audit has not run. No ledger/GBWT/index rewrite
or count restart was performed or authorized by this diagnostic question.

Container `pancollapse-memory-reduced-count-20261030-4sQJXD` finished at
`2026-10-30T15:17:19.33273047Z` with exit 1 and `OOMKilled=false`. It completed
`FastLocate` in 1592.66 s and `selected_path_positions` in 1511.04 s
(182,782,459 paths; 224,363,518,048 steps; 3,780,279,798 checkpoints;
15,121,119,192 coordinate bytes), then failed because ledger path
`chr2::HG00097#1#HG00097_ha_T0037310_R1` was absent. No GAMP processing,
completed count, or published output exists; no restart is authorized.

The authorized independent standard FastLocate `.ri` build launched at
`2026-10-30T15:32:41.725382404Z` (08:32 PDT) as container
`pancollapse-transcript-ri-20261030-eAma1l` in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_transcript_ri_20261030_eAma1l/`.
It uses frozen `build_ri` 1.5.0 SHA-256
`3b1e02dff44c9973379c0e598b81267bb2cd93c319c22d5b284ef50e39023a0f`,
128 threads/CPUs, and 500,000,000,000-byte memory and memory-plus-swap limits
(no swap), with `/mnt/ssd/lalli/usr/local/src/vg/deps/gbwt/bin/build_ri` for the unchanged
44,562,796,448-byte
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/joint_transcript_gbz_20261028_repair/joint/joint.transcripts.gbwt`.
The root retains `launch.sh`, `build.sh`, `RUN.json`, `container.id`,
`build.stdout`, and `build.log`. The `.staging/joint.transcripts.gbwt` symlink
is the unchanged input; `joint.transcripts.ri` publishes only after builder exit
0, nonempty output, stable source, and recorded completed source/output SHA-256
binding. At 15:33 UTC it was running and loading GBWT (Docker displayed 23.4
GiB); this is not artifact completion or query verification. This `.ri` is a
reusable GBWT-side artifact only: current
panCollapse has no `.ri` CLI option or loader, so it is not usable by the
current count CLI and does not replace the ledger, XG/GBWT, PCANIDX2,
selected-path positions, or exact model geometry. The GBWT must remain
unchanged. Host preflight reported `MemAvailable` 896,347,656,192 bytes and
about 12.15 TB free disk. The failed count remains unrestarted.

## Historical: memory-reduced whole-genome count launched; reference initialization active (2026-10-30)

The latest user instruction, **"Run it,"** authorized exactly one retry, now
launched at `2026-10-30T12:54:14.737507532Z` (05:54 PDT). It uses the dirty, tested memory-reduction
worktree on `fix/whole-genome-count-repair-20261028` at
`327b048ce8747c83fe20c9f75074cc4ee77b1312`, using
`build/src/panCollapse` SHA-256
`ddb57632d7c70b6d7fb080a52bbbcd76cc11d32d6ae0abb41ffcae1a2305dc82`.
The bounded receipts remain `build/reference-memory-ctest-final.log`
(186/186) and `build/reference-memory-parity.log` (18/18 on the nine-read
fixture); neither establishes real-data fit or completion.

The one run is both `cr7` and `pansc-strict-v1`, with 128 threads, a
500,000,000,000-byte hard container memory cap and equal memory-swap cap (no
swap), memory-only aggregation (no spills), and the same existing GAMP, XG,
GBWT, PCI, ledger, and whitelist inputs recorded by
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_indexed_count_20261030_uSVnCB/RUN.json`.
It does not authorize remapping, reindexing, preloading, an old-binary retry,
or a cap change. Preserve the old 600-GB OOM evidence. At admission check,
`MemAvailable` was 895063134208 bytes; the target is 12.15 TB available, with no
active PanCollapse count at that preflight. The launch passed the documentation
checkpoint before execution. Container
`pancollapse-memory-reduced-count-20261030-4sQJXD` is running with Docker limits
verified at 500,000,000,000 bytes memory/memory-plus-swap and 128 CPUs. The kernel
page-aligns `memory.max` to 499999997952 bytes; `memory.swap.max` is zero.
The first native milestone is `identity_ledger_begin`; GAMP processing has not
started and no counts are published. This does not establish full-reference fit.

**Read-only update, 14:46 UTC / 07:46 PDT:** ledger validation, annotation-index
load, metadata parse, XG and GBWT load have completed. FastLocate construction
is still the uncompleted native phase; no GAMP progress is logged. Anonymous
memory was 413.4 GB at 14:43 and 347.5 GB at 14:46; the latter snapshot also has
64.8 GB file cache. The cgroup previously reached its 500-GB cap, with no OOM
or OOM kill. These are process/container observations, not per-container-object
heap attribution. The user asks why this work is not indexed: FastLocate has
native `.ri` serialize/load support, but `GraphInput::initialize_gbwt()` builds
it unconditionally. Current PCANIDX2 stores annotation-policy facts only, not
the ledger identities, graph lookups or FastLocate. A future `.ri` loader must
bind the exact source GBWT; the native `.ri` header does not verify that identity.
No implementation or job change is authorized by this diagnostic question.

Frozen executable, `launch.sh`, prepared `RUN.json`, `container.id`, `count.log`
and `count.stdout` are in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_memory_reduced_count_20261030_4sQJXD/`.
Final output target is that root's `counts/`, published atomically on success.
The next authorized task is observing this one run through reference loading,
GAMP processing and output validation; do not silently restart it if it fails.

## Historical implementation checkpoint: reference-memory reductions; 186/186 tests and frozen-binary parity pass (2026-10-30)

The user authorized the preceding audit's storage reductions. Implemented on
dirty branch `fix/whole-genome-count-repair-20261028` (base `327b048`), preserving
preexisting D097/D099 and memory-only changes:

- Exact GBWT coordinates retain 64-step/terminal checkpoints, recovering an
  occurrence through at most 63 LF steps with its BWT-record offset. Only one
  coordinate width is live. Repeated visits, mixed orientation and >32-bit
  positions remain supported; statistics report actual selected steps,
  checkpoints, coordinate-capacity bytes and wide paths.
- Validated ledger trees compact to lexical arrays, with representative Parent
  pointers rebound before catalog construction. Main reuses lexical Parent
  ranks rather than a second hash table, sorted name vector or persistent
  materialized-fact pointer vector. Selection reuses row binding flags instead
  of copying every selected GBWT name. Legacy-only path strings allocate only
  on legacy paths; unused production gene-geometry duplicates are skipped.
- Parent facts retain 24-byte records with shared gene/category dictionaries
  and lexical rank/key APIs. The existing 13-byte PCANIDX2 disk representation,
  source identity and deterministic bytes are unchanged; existing indexes need
  no regeneration.
- Native node/model caches account 1 GiB/256 MiB of resident values with FIFO
  eviction/oversize bypass. Shared-pointer read leases preserve values during
  eviction. Actual process memory also includes allocator overhead, active
  leases and uncached work; these cache bounds are not a whole-process limit.
- After workers and compatibility capture finish, cache/reference/model/Parent
  state is released before UMI finalization; required transcript provenance is
  retained separately. Common UMIs use lossless packed keys with an owned-string
  fallback; historical filtering/UMI semantics remain unchanged. Consumed
  buffers and hash buckets are released rather than retained through output.

**Acceptance:** Docker build succeeds and
`build/reference-memory-ctest-final.log` records **186/186 PASS in 47.20 s**.
Coverage includes LF checkpoint/terminal boundaries, repeated/mixed-orientation
paths, 1,025 steps -> 18 checkpoints, wide coordinates, flat catalog movement
and pointer rebinding, 24-byte policies, shared categories, byte-exact index
roundtrips, evicted active leases, concurrent single-flight cache construction,
UMI packing/fallback edges and outputs after Parent release. Earlier integrated
pass was 185/185 in 45.95 s before the cache test was added. A transient round3
build failed because a parallel-owned release helper had not yet landed; final
build resolves it (`build/reference-memory-build-final.log`).

Frozen-executable comparison **18/18 PASS** via
`tests/vg/reference_memory_parity_verify.py`: old/new 1 thread and new 4 threads,
XG/XG+GBWT/GBZ, default/all-format sinks, both profiles, nine-read fixture.
All produced table/diagnostic/MEX/RAD files are byte-identical; scientific
counters and profile logical hashes agree. Receipt root:
`build/reference-memory-parity-wvzakxpv/VALIDATION.json`; summary log:
`build/reference-memory-parity.log`.
Tested new binary SHA-256:
`ddb57632d7c70b6d7fb080a52bbbcd76cc11d32d6ae0abb41ffcae1a2305dc82`
at `build/src/panCollapse`; old frozen SHA remains
`952b993fac47d773e36a3d258770d476da91f52dda85ead840f2b101c88b10ed`.
Final DWARF sizes: Parent record 24 B, SelectedPath 56 B, exact aggregate key
32 B and deferred key 96 B. These are layout facts, not measured whole-genome
RSS reductions.

**Next gate at this implementation checkpoint:** user-authorized resource qualification/retry, not automatically
launched then (the later authorized launch is recorded above). No production GAMP, reference rebuild, release image or cap
increase was started. The user's requested 500-GB process cap remains unchanged;
the older-binary/450-GB count spill request is deferred and the working build
remains memory-only. No claim of 500-GB fit, throughput, real-data F1 or full
whole-genome count completion follows from bounded fixtures. The sampled
coordinates introduce a lookup-time tradeoff that remains unmeasured at scale.

## Historical: diagnostic/source/type-layout audit after indexed-count OOM (2026-10-30)

The prior read-only audit asked why reference initialization used so much memory
and which smaller structures could be retained while streaming reads. It did
not then authorize implementation, relaunch, a new scientific policy, or a new
resource limit. The implementation authorization above supersedes only that
read-only boundary; the prior 500-GB-cap/450-GB-spill older-binary request
remains deferred after the failure.

Observed: container `pancollapse-indexed-count-20261030-uSVnCB` exited 137
with `OOMKilled=true` at `2026-10-30T06:36:08.346768207Z`, under its
600,000,000,000-byte no-swap cap; frozen binary SHA-256 is
`952b993fac47d773e36a3d258770d476da91f52dda85ead840f2b101c88b10ed`.
It completed identity-ledger validation (190,240,849 rows), annotation-index
load, metadata parse, XG, GBWT, and FastLocate. It did not complete
selected-path positions or GAMP processing. No GAMP completion or published
counts exist. Receipts: `RUN.json`, `launch.sh`, and `count.log` in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_indexed_count_20261030_uSVnCB/`.

**Read-only audit complete.** The failed interval is after FastLocate and
before `selected_path_positions` (`src/main.cpp:3185-3205`). It both duplicates
selected names in a string hash set and eagerly prepares per-step coordinates;
there is no intermediate milestone identifying which allocation exhausted RAM.
No heap trace or per-structure measured RSS survives. Do not call the following
layout arithmetic an observed allocation breakdown or a proven memory-fit fix.

Bounded GBWT-header inspection reports 760,920,122 stored sequences and
474,167,531,502 symbols. Its bidirectional representation therefore contains
380,460,061 forward metadata paths and 236,703,305,690 forward real steps.
The format is pinned by
`/mnt/ssd/lalli/usr/local/src/vg/deps/gbwt/include/gbwt/files.h:44`;
`od -An -tx4 -N48` and `od -An -tu8 -j8 -N40` on the supplied GBWT read
only its header (tag `0x6b376b37`, version 5, offset 1, alphabet size
233,689,814, flags 7). Real forward steps are `(size - sequences) / 2`,
excluding endmarkers and the stored reverse walks.
`src/graph_input.cpp:136-158` extracts each selected walk and reserves a base
coordinate for every step plus its terminal. If all forward paths were selected
and stayed 32-bit, the packed coordinate payload alone would be
948,335,063,004 bytes. The actual selected-step count was not recorded and was
not measured by scanning the reference; that scenario is not the actual RSS.
DWARF gives 72 bytes per `SelectedPath` object, or 13.70 GB if all 190,240,849
ledger paths are selected, before coordinate buffers. FastLocate's constructor
clears its large temporary construction arrays before logging completion;
retained r-index arrays, XG and GBWT remain resident.

Frozen-binary type inspection without running the inferior also establishes:
176 bytes per ledger row tree node and 48 bytes per Parent index tree node,
giving a combined 42,613,950,176-byte floor at the observed row/Parent count,
excluding shared text atoms, character buffers and allocator overhead.
Strings are already interned (8-byte handles; 40-byte unique atoms before text
heap); audit-only provenance is already released after validation. Loaded
Parent facts are flat 96-byte rows, not another tree: 18,263,121,504 bytes
before category buffers, versus 13-byte numeric disk records. Do not propose
already implemented string interning or provenance release as new work.

**Opportunities, not implemented or approved designs:** retain numeric
row/Parent/model IDs and packed policy records; remove startup-only copied
selected names and redundant handle storage; avoid dense coordinates for every
path occurrence through compact/shared or sampled/lazy geometry; combine
duplicated node-reference/exact postings and make read-time caches safely
discardable. A node-to-gene union remains insufficient: repeated occurrences,
orientation, transcript coherence, exact overlap and junction semantics must
survive. A flat node-posting expansion is not automatically smaller than GBWT.

`NodeLedger`, exact-model candidate caches, exact Parent models, GAMP queues and
count tallies were not populated at this failure. They cannot explain this OOM,
but are additional whole-genome peaks still unqualified. The lazy reference
caches never evict; safe eviction would require lifetime-safe access rather
than deleting objects still referenced by workers. During later counting,
exact/deferred aggregate keys also
retain string objects (DWARF 48/112 bytes), and finalization retains multiple
resident buffers. Compact sequence encoding, per-barcode batch output and
releasing graph/model caches after workers join could reduce later peaks, not
this startup failure. No dataset was rerun, production code changed, or new
fit/performance claim established. Existing memory-only source and 185/185
bounded acceptance remain untouched.

## Historical snapshot: memory-only native count implemented and bounded tests pass; active run untouched (2026-10-30)

The user has explicitly directed that **all count-path disk fallback be
removed**.  The accepted follow-up requirement is therefore memory-only raw
read/count aggregation and UMI collapse, with no spill, merge, or other
count-intermediate disk escape path.  This is an implementation and parity
gate, not a change to annotation, compatibility, assignment, or biological
policy. The implementation now removes aggregate/deferred/corrected and
single-barcode spill I/O, external merges, directory management and the
independent runtime/output memory budget. The old CLI option fails explicitly;
manifests/preload receipts report memory-only storage and compatibility spill
counters remain zero. Optional diagnostic staging and final compressed outputs
are unchanged. The frozen live run was neither stopped nor restarted; a restart
remains an explicit user decision.

**Acceptance:** coordinated Docker build plus the full existing CTest suite
pass 185/185 in 45.84 s (`build/memory-only-build-final.log`,
`build/memory-only-ctest-final.log`). An earlier pass was 185/185 in 47.54 s.
Coverage includes frozen barcode/UMI goldens, insertion/thread/joint-profile
invariance, a 60,000-feature single-barcode resident case, removed-option
rejection, absence of aggregate files and raw/index output equivalence.
The release-image smoke command was updated and passes `bash -n`; a new release
image was not built. Runtime static inspection confirms no aggregate file
writers or compressed-run machinery remain. These bounded checks do not prove
whole-genome peak RAM or end-to-end performance. D100 and
`docs/conversion-algorithm.md` record the actual design and remaining boundary.

At that historical checkpoint the container was observed `running` (not then
OOM-killed) with Memory=MemorySwap=600,000,000,000 bytes. Its latest native log milestone
is 90,000,000 / 190,240,849 identity-ledger rows at 1438.44 s; this is still
initialization only.  There is no observed GAMP processing, published count
output, or count spill file.  These facts do not establish end-to-end memory
fit or count correctness.

**What was indexed.**  D099's completed `annotation.pci` is a 2,539,421,338-
byte compiled annotation-fact artifact: the two frozen profile fact tables,
source identities/formats, canonical Parent records and small gene/category
dictionaries.  It avoids annotation decompression, parsing, and
classification at count start.  It is deliberately not a graph/HST-node
association index, nor a full prepared reference: the checksum-bound path
identity ledger, XG/GBWT, FastLocate, graph/path positions, and exact
Parent/exon/splice geometry still initialize for each count.

**Architecture boundary.**  D048's simple active algorithm scores aligned
nodes and attributes that evidence to every traversing HST path, then preserves
the best-score/tie, transcript-collapse, orientation, and downstream
assignment semantics.  A proposed node-to-association dictionary would have
to represent that many-to-many path membership and all required identity and
geometry links; a single node-to-gene mapping is not equivalent.  It may be a
future performance candidate, but no simplification or replacement is
authorized from this observation. The source-backed architecture audit and
memory-only implementation/bounded validation are complete. The next decision
is the user's choice about restarting the frozen run; a full prepared reference
remains a recommendation, not an implemented or launched change. Do not reindex,
launch a new pilot, alter policy, or infer a production restart.

## Historical snapshot: direct indexed whole-genome count launched at 600 GB (2026-10-30)

After the documentation checkpoint, actual native `count` (not preload) started
at 2026-10-30T04:15:28.260611550Z (21:15:28 Pacific, Oct 29). Container
`pancollapse-indexed-count-20261030-uSVnCB`, ID
`c68ae03ffb6faed56e397051ac6b9d0179a07fa67aab2510395f8213e4458bc5`,
was confirmed running at that time with Memory=MemorySwap=600,000,000,000 bytes, no swap,
NanoCpus=128,000,000,000 and frozen binary
`952b993fac47d773e36a3d258770d476da91f52dda85ead840f2b101c88b10ed`.
Run root:
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_indexed_count_20261030_uSVnCB/`.
Exact command/inputs/limits are in `launch.sh`/`RUN.json`; native progress is
`count.log`. Default Parquet counts for both profiles will publish atomically
under `counts/`. No BAM, optional diagnostic/RAD/MEX sink, remapping, index
rebuild, GFF parse or separate `--preload-only` pass is requested. Initialization
will proceed directly into GAMP in this same process if it succeeds.

The first native observation is 5M ledger rows after 58.38 s, before GAMP.
This is running initialization, not read processing, count completion or
whole-genome fit. Admission `MemAvailable` was 878,532,720 kB (~900 GB), with
~12 TB free. The compiled annotation index is complete and reused; remaining
ledger/graph/FastLocate/model startup is still required.

`--count-memory-budget 128GiB` was carried from the original plan; it is not a
second hard process cap or preallocated reservation. It controls approximate
aggregation spilling (currently triggers at 3/4, ~96 GiB) and merge buffers,
not reference initialization. The user questioned this independent setting;
no runtime change/restart or new policy was authorized by that question.

## Prelaunch checkpoint: annotation index complete; higher-cap count authorized (2026-10-30)

The former annotation-index build is **COMPLETE**, not running. Its run root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_annotation_index_20261029_HS3sPi/`.
Docker records exit 0 and `OOMKilled=false` at
`2026-10-30T02:50:18.720565643Z`; `index.stdout.json` reports
`status=complete`, 190,240,849 Parents, 265,064 gene IDs, and
2,539,421,338 fact-content bytes. The published index content ID is
`sha256:a0e25615cbc0fa35f9224718cb8d0e44862d3848618e6415d5ce2683f148e733`.
Native time was 12,546.4 s; container time was 12,950.65 s including cleanup.
The frozen producer binary is
`952b993fac47d773e36a3d258770d476da91f52dda85ead840f2b101c88b10ed`; the
bounded suite passed 185/185. The completed index does not establish count fit.

**Next authorized gate:** perform exactly one direct whole-genome `count` using
this index, both profiles, 128 threads, a 128-GiB aggregation budget, and a
600,000,000,000-byte hard memory cap with no swap. Use the GAMP, XG, and GBWT
input paths recorded in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_source_gene_count_20261029-JQD61Y/execute.sh`;
keep source inputs read-only. Do not run a separate preload or repeat
initialization. This is authorization for no other experiment, remapping,
graph/index rebuild, GFF regeneration, or production retry. The earlier 350-GB
count preload OOM-killed before GAMP processing; it remains historical evidence,
not a capacity result for indexed count. Host headroom must be freshly checked
at launch. No indexed count has yet run; counts are not ready until processing
completes with its run receipt.

## Implementation acceptance: native annotation index; 185/185 tests pass (2026-10-29)

User authorized compressed GTF/GFF indexing after the failed whole-genome
preload. Architecture/adversarial review accepted a bounded first D098 slice,
documented in D099: `panCollapse index ... --out annotation.pci` and native
`count --annotation-index annotation.pci --path-identity-ledger ledger.tsv`.
Implementation remains on dirty `fix/whole-genome-count-repair-20261028`, based
at `327b048`; preserve the preexisting D097/storage changes and others' edits.

The native `PCANIDX2` artifact contains prepared annotation facts for both
profiles, source identities, explicit formats, dictionaries and lexical 13-byte
Parent records. Count loads facts into reserved flat storage, reusing the
checksum-bound ledger's strings, with no annotation parsing/classification.
Raw/compiled routes preserve source fact identity and biological rules.
Explicit GTF supports quoted/repeated attributes and requires transcript rows;
decompression is cursor-buffered with bounds and checked zlib errors. No Python
production dependency, SQLite, per-read spool, graph-handle persistence, GFF
rewrite, cap raise or production relaunch was introduced.

Index and ledger digests are calculated during their required reads. The
loader retains exact consumed whole-file SHA/size for the count manifest, so
manifest generation neither reopens nor rehashes the index. Its distinct
`annotation_index_derivation_id` covers the payload, including formats and
semantic version. Staged checksum verification and atomic no-overwrite
publication protect outputs. Bounds, enums, source/content consistency,
profile/equivalence invariants and ledger identity fail before graph/GAMP.

Final build and CTest pass **185/185 in 45.46 s**, including raw/index count,
molecule, read-assignment, logical-hash, semantic-counter, MEX and RAD parity at
1/4 threads; gzip GFF3/GTF parity; deterministic bytes; deleted annotation
sources; wrong ledger; corrupt/truncated records; re-checksummed forged source,
format, flags and equivalence cycles; and loaded-inode identity retention.
Logs: `build/annotation-index-build.log`,
`build/annotation-index-ctest-final.log` (the earlier full run also passed
185/185 in 47.66 s at `build/annotation-index-ctest.log`). Initial test-environment
issues were an absent `/stock-vg` fixture mount and Python UMI-tools user-site
path, not native failures; configure now uses the mounted existing vg executable
and the full suite runs with explicit PYTHONPATH. `git diff --check` passes.
Commands/limits: `docs/annotation-index.md`; core:
`src/annotation_fact_index.{hpp,cpp}`, `direct_count_facts.*`, `main.cpp`.

**Historical pre-index D098 gate:** the index is not a full prepared-reference
snapshot or an OOM fix. Ledger maps, XG/GBWT loading, FastLocate, path positions
and exact Parent/exon/splice geometry still initialize per count, and their
whole-genome resource use remains unqualified. The current, explicitly
higher-cap indexed-count authorization above supersedes the former requirement
to reduce/persist this remaining state before a production rerun. The failed-run
receipts below remain the authoritative prior result: zero GAMP reads/counts.

## Historical: qualified whole-genome preload OOMKilled; reference index authorized (2026-10-29)

The D097-qualified bounded preload failed before counting. Container
`pancollapse-qualified-count-20261029-JQD61Y` (run root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_source_gene_count_20261029-JQD61Y/`)
started at 2026-10-29T15:00:25.246941463Z and was OOM-killed at
2026-10-29T21:20:25.857153601Z: Docker records exit 137 and `OOMKilled=true`
under Memory=MemorySwap=350,000,000,000 bytes (no swap). Do not infer the
exact allocation that triggered the kill. `stage.json` remains
`{"stage":"preload","gamp_opened":false}`; no count log or outputs exist.

The final preload records identity-ledger validation at 190,240,849 rows,
annotation facts for 190,240,849 Parents and 265,064 genes, metadata parsing at
21,544.8 s, XG deserialization at 19.8736 s, and GBWT deserialization at
271.377 s. It has no fast-locate-complete record. This is a failed resource
qualification, not a count, parity, or biological result; historical evidence is
preserved.

**Next authorized task:** implement D098's native reference-index function:
ingest compressed GTF/GFF and directly write a preprocessed binary that native
count loads. The index must capture reusable reference-only prepared state, not
just raw parsing inputs; retain input/version identity checks and complete/atomic
publication. Sample-specific barcode priors, observations, assignments and UMI
counts remain outside it. Establish raw-input versus indexed-reference parity and
bounded tests before any production use, along with startup wall-time, peak-memory
and I/O measurements. The existing scripts/fact bundle are not a complete runtime
reference snapshot. No full whole-genome relaunch, memory-cap increase, or new
experiment is authorized by this checkpoint.

## Historical: D097 implemented/verified; qualified whole-genome preload failed (2026-10-29)

D097 is implemented on dirty branch `fix/whole-genome-count-repair-20261028`:
native `count` and compatibility replay qualify a conflicting canonical
transcript by normalized source-gene ID while preserving both source-gene IDs
and all original provenance. GFF/index inputs are unchanged; legacy
`panCollapse convert` remains strict. No per-row target field was added (row
size remains 136 bytes). The real CHM13 chr2 fixture remains
`ENST00000477740.5` -> `ENSG00000241860.8` versus `ENSG00000238009.6`.

`build/qualified-target-ctest.log` records full native suite 183/183 passing
in 55.84 s; `build/qualified-target-focused.log` records 6/6 in 2.40 s,
including semantic counters and forged mixed-source facts. Native end-to-end
comparison found explicit canonical one/two-truth and qualified-canonical
ambiguity matrices, molecules, read assignments, and counters exactly equal at
one/four threads and replay; optional RAD unique keys also agree. This verifies
the scoped behavior, not whole-genome capacity, biological equivalence,
frozen-fact authority, protected-host compatibility, accuracy, or custom-profile
promotion.

Launched at 2026-10-29T15:00:25.246941463Z (08:00:25 Pacific):
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_source_gene_count_20261029-JQD61Y/`
(`RUN.json`, `launch.sh`, `execute.sh`). Container
`pancollapse-qualified-count-20261029-JQD61Y`, ID
`2e5b93d5c56d0783bc6e98a9d7b3ae14374dd4b91a2989353a2bf2a5d2628010`,
was later OOM-killed as recorded in the current checkpoint; it had Docker
Memory=MemorySwap=350,000,000,000 and NanoCpus=128,000,000,000. Frozen binary SHA-256:
`8200489e65aa056f18a5a367a6a21e9d778dd0c66a3d62223c1ceb1bc470ad1a`.
The first observed snapshot processed 3,000,000 ledger rows in 32.94 s and
reported about 2.287 GiB Docker memory; this is early loading, not peak RSS.
`stage.json` says preload/no GAMP opened; `preload.log` has million-row progress.
The workflow required bounded preload exit 0 with no GAMP reads before count,
but that condition was not met. No count started. No cap raise, pilot series,
BAM, remap, GFF rebuild, or whole-genome relaunch is authorized.

## Historical: storage compaction implemented; source-gene identity conflict observed (2026-10-29)

The compact-storage implementation now shares immutable identity text, releases
audit-only provenance after the original validation checks, shares annotation
metadata/tag sets and ParentFact strings, and stores graph handles/lexical ranks
directly on rows instead of two additional lookup dictionaries. Fixed row size
is 136 bytes (previously 744); this is not a measured whole-genome RSS reduction.
The representation still has map-based row/Parent catalogs; a complete packed
numeric catalog rewrite and whole-genome capacity qualification are not claimed.
Native startup now logs ledger byte/row progress at million-row milestones.

The actual conflict is not established to be a harmless copy label:
`ENST00000477740` / `ENST00000477740.5` occurs with
`source_gene=ENSG00000241860.8`, while
`chr2::CHM13_T0135505_R1` has `gene_id=ENSG00000238009` and
`source_gene=ENSG00000238009.6`. These normalize to different count-gene IDs.
Whether these represent historical annotation aliases, a source annotation
error, or different biological loci is unresolved. No gene merge, transcript
re-keying, or policy relaxation has been applied. The original canonical/gene
check remains fail-closed and reports the conflicting IDs/path.

The two-row real-data regression is
`tests/fixtures/path_identity_conflict_ENST00000477740.tsv`. Discovery evidence:
`build/compact-identity-conflicts.log` (search stopped once the contradiction was
captured; not a whole-ledger inventory), plus
`/mnt/ssd/lalli/pancollapse_compact_identity_20261029-t6nati/`.
Two complete native suites passed 181/181 before the final fixture was added;
a later parallel run exposed an existing pair of isoform tests writing the same
BAM path. Their shared output is now protected by a CTest resource lock. Final
build/suite results are at `build/compact-identity-build.log` and
`build/compact-identity-ctest.log`. Final rebuild and complete suite passed
181/181 in 26.66 s, including the real conflict in ordinary and exact-preflight
ledger modes. The standalone ledger regression reports identity=104 and row=136
bytes. `git diff --check` passes. Whole-genome counting remains unstarted.
The immutable-text unit also passes AddressSanitizer/UndefinedBehaviorSanitizer
with leak detection enabled.

No GFFs were regenerated, no persistent index/database was created, and no
whole-genome count or repeat preload was started. Existing input authority,
350-GB/no-swap startup cap and conditional count authorization are unchanged.
Next task requires resolving the transcript/source-gene identity conflict with
the user before another startup; storage changes alone cannot make that input
consistent. The historical preload receipt now records its observed exit 1,
not a stale running status.

## Historical: full-reference startup failed in the identity loader (2026-10-29)

`pancollapse-final-input-preload-20261029T041218Z` exited 1, not OOM, at
2026-10-29T04:30:16.299861101Z before opening GAMP or producing counts. Its sole
reported error is: `path identity ledger maps canonical transcript
ENST00000477740 to multiple genes`. The run used final corrected GFF/ledger,
original XG and repaired transcript GBWT, both profiles, 128 threads, a
350,000,000,000-byte no-swap whole-process cap and 128-GiB count budget. Native
binary SHA-256 is `ea4694b9a394fa4f95d1c3be03c56c2c957f3bbe69bfbfcb7441741f7dc4e4c8`
at source `327b048ce8747c83fe20c9f75074cc4ee77b1312`; receipts remain at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_annotation_count_20261029T041218Z/`.

**Observation, not policy:** the loader rejects this canonical-transcript/gene
relation; the receipt does not establish whether rows are legitimate haplotype
copy differences or a true gene contradiction. **Next authorized task:** replace
the full string-heavy ledger provenance representation with compact numeric
mappings/shared metadata, and add small regressions that distinguish those two
cases without relaxing biological identity rules. Only after those regressions
and a successful bounded startup check may the already-authorized real count be
reconsidered under the existing hard cap. No extra GFF generation, remapping,
BAM, database, persistent custom index, pilot series, or biological-policy
change is authorized.

## COMPLETE: corrected GFF input (2026-10-29)

`RUN/corrected_gffs_usable/annotation.corrected.gff3.gz` and its matching
hard-linked `path_identity_ledger.tsv` are published. Container
`pancollapse-count-gffs-20261028T173014Z` exited 0 at
2026-10-29T03:23:47.540000084Z: 24 chromosomes, 95,110,030 EXON contexts, and
BGZF integrity passed; 811,212,428 input and 1,016,498,852 final feature rows;
55,281,270,132-byte GFF; 3,841.709 s wall and 42,801,287,168-byte child peak RSS.
Authority: `RUN/corrected_gffs_usable/manifest.json` and
`RUN/logs/count-ready-gff-preparation.json`; reference commit
`f8a6fec7d33169ae6e72ccf2fc9c79e4f6b301d0`.

This closes corrected-GFF production only. The originally planned preload is
the failed loader observation recorded above; it does not resolve frozen-profile
facts, protected-host compatibility, or fallback-label physical-collision
count-readiness. Historical writer states below are superseded.

## Historical: simplified slice-stream job (superseded)

The generic native stream was stopped as requested, with partial output and
`RUN/logs/stream-generic-superseded*` preserved. New container
`pancollapse-simple-gffs-20261028T173014Z`
(`27230e467468c9d4bddb369f2cb27eb509e52ce5c45fc053f163650343215872`) started
2026-10-29T01:48:53.964771673Z. Reference commit
`74b7ce2511072f0c012d724059c768262b077c65` uses selected-field string views,
unchanged-line passthrough, replacement slices and consecutive-parent caches.
Ten small smoke checks passed in 0.216 s; no broad parity study was performed.
Target is `RUN/corrected_gffs_simple`; limits remain 200 GB/no swap/32 CPUs,
with bgzip 16 threads/level 1. Ledger loaded; initial chr1 source throughput is
about 371k rows/s (2M at 01:52:14.591 UTC to 8M at 01:52:30.754 UTC), about
6.7x the old 55k-row/s producer. Parent-summary and total runtime are not yet
measured; no completed GFF is claimed. This supersedes generic-stream running statements below.
No graph rebuild, remapping, biological policy change or native count has started.

## Historical: streamed corrected-GFF stage (superseded)

`batched3` completed with exit 0 in 709.319 seconds. Its `memory_reconciled`
ledger has 190,240,849 rows, 95,110,030 EXON contexts, and 13,898,646 repaired
identities; SHA-256 is
`25f4d0eb1c7bfbc9b2dfb1470598a30069371f9a123de33379a60bbab05ff6c0`.
All 24 original retained-GFF sources are verified. This supersedes the older
running/unmatched-finalizer statements below.

The Python corrected-GFF Docker writer was stopped as directed:
`pancollapse-corrected-gffs-20261028T173014Z`
(`f720aba70bd481f9b55b5cfea978d9386a74cffffbe9e4748eb4f9814ffbf95d`)
exited 143 at 2026-10-29T00:52:51.119312224Z. Preserve
`RUN/.corrected_gffs.staging-ypty38d6` and `logs/gff-python-superseded*` as
historical partial output.

The user waived the comparison/parity launch gate for the bounded native
stream. Production container `pancollapse-streamed-gffs-20261028T173014Z`
(`e7d448c1f2c2498d8fbf606f45514108f140c377f807d09eda46b2328faf27f5`) started
at 2026-10-29T01:27:27.22569375Z, launched by
`RUN/run_streamed_corrected_gffs.sh`. Active staging is
`RUN/.corrected_gffs_bgzip.staging-6l4rwj15`; final target is
`RUN/corrected_gffs_bgzip`.
It compacts the global-name ledger in RAM, parses every GFF row once, modifies
only allowed `Parent`/`ID`/`exon_unique_parent` fields, retains frozen PURE
ParentRollup semantics, and pipes stdout through threaded `bgzip` (16 threads,
level 1), under 200 GB/no swap/32 CPUs. No completed GFF is claimed. The
existing 21 Python tests passed, but no new native parity test ran after the
final port. Facts, compatibility, preload, admission, and count remain pending
independent gates. The 450-GB whole-process cap is unchanged. Reference
implementation worktree is clean at `28ea13cd142d74df7f1af98f14dae2f364da7697`.

## Historical: whole-genome global-Parent identity repair gate (superseded)

Authorized work is bounded implementation and tests for reconciling global
Parent identities, scoped streaming/compact loading, and preload admission;
conditional pilot and full counts are authorized only after their gates. The
pre-repair joint GBWT audit found 6,949,323
colliding generic names (13,898,646 records; 3.726% of 373,001,671 generic
paths), not genes or reads. Prefixing only `vg_path_name` cannot repair the
collision: `NA20282_ha_T0382806` also collides as `unique_parent` across
chr1-hap2 LUZP1 and chr3-hap1 SNRK. Keep opaque existing body IDs; repair exon
links/fallback IDs and retain original input/source Parent provenance,
biological ENSG/ENST, graph names/walks/coordinates, and frozen profiles. The
existing ledger rejects one `unique_parent` mapping to multiple identities.

Sources are located (including the cold-archived chr1 payload); do not request
them again. Reconcile metadata globally, not by chromosome shards; the existing
identity contract derives `panSCup1_<sha256>` for a non-unique source Parent.
Pending gates are metadata reconciliation, compiler-derived facts, graph/profile
compatibility, and resource admission. No native count has started; remapping,
BAM output, biological decision, and profile/policy change remain unauthorized.
A conditional count prefers original XG plus repaired GBWT and a certified derived-fact
bundle, not incomplete `from_annotation` facts with protected flag false.
Limits: 450,000,000,000 bytes, no swap, <=128 cores, and 128 GiB only for
aggregation (never the whole process).

**Bounded checkpoint (2026-10-28):** run root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/count_identity_repair_20261028T173014Z/`
records 24 sources (5 live/19 archived). Its 100-feature-per-chromosome,
observed-exon inspection is SAMPLEONLY, not a full composition/hash proof; no
full payload was restored or hashed at that earlier checkpoint. This is
superseded by the completed 24-shard materialization/hash verification below.
Native tests pass 180/180; `count
--preload-only` returns before writers/GAMP and
passes FIFO-unopened plus invalid-authority checks 7/7. The SQLite reconciliation
utility's registry suite passes 18/18. A 10k real metadata reconciliation
exited 0 in 1.081267 s at 33,554,432-byte peak child RSS; 100k was deliberately
skipped and is not a blocker. The initial 190,240,849-row/85-GB SQLite attempt ran
under 64 GiB/no swap/16 CPUs in the now-stopped
`pancollapse-global-identity-20261028T173014Z` (container
`ca9938a5a868d1a6cc734571fc6f2e0c799113b1954921a7fe2a3925285d7c6a`; Python
image `sha256:b4403f3b7acf5e0700a5dd4e7ab01456d62b557a208a04f8a42d3470d93b5730`;
utility commit `6c72e607dc548026256cd79b1c7010da270ae976`). Final timing receipt
`logs/global-reconciliation.json` is completion-only. Native implementation
is committed at `990872cf7ff8d68d52f459e2e93af4b7f5616da0`; that initial
attempt used unchanged `6c72e60` utility code. No native count or large
preload has started.

**Current source gate:** the prior full-raw-CAT requirement is superseded as
incorrect: the frozen chr20--22 command used retained exons/bodies, and raw CAT
would change the frozen competitor universe. While repair runs, bounded
tools/tests may stream-restore and SHA-verify exactly 24 accepted retained
graph shards, one original-byte copy preserving inner gzip. The 19 archive
hashes and five live producer checksums are recovered and bound to exact
SHA256SUMS sources in `live_annotation_source_hashes.json`. Do not rerun
selection/dedup, ingest raw-CAT models, or change biology.

Source restoration completed at 2026-10-28 22:04:52 with exit 0.
`RUN/verified_annotation_sources/manifest.json` is atomic and `verified` for
all 24 original source hashes; `logs/source-materialization.json` records
11,997.819248366868 seconds and 20,971,520-byte child peak RSS. Materializer
commit `fc1b7a68a43a47c6a4dfbae4d0ca03adf0d33e6b` passes 7 tests. Reference commit
`5d60a21af1f6c8e5824ec4b904b67d3c839d652c` adds an optional read-only global
registry to future projector runs: 6 registry/shard integration tests and the
12 existing projector tests pass; the reconciler's 18 tests remain passed.
Indexed context lookup avoids per-Parent full registry scans. Missing contexts
reject; the flag requires a complete preparation scope and does not yet change
build-driver defaults.

**Historical SQLite state:** its 190,240,849-row ingest completed, then its
container stopped at 2026-10-28T23:38:00.024748304Z with exit 143 after the
replacement passed relation validation. Preserve `RUN/global_registry`, its
WAL, `logs/sqlite-superseded.log`, and
`logs/sqlite-superseded-container-state.json`; do not infer a missing
`logs/global-reconciliation.json`.

**Historical fast-path state:** coalesced batch finalizer
`pancollapse-identity-batched3-20261028T173014Z`
(`8addebd83679f03e851d41c0b55c94142b57f859e513075cb9ec831f07a33afb`) was
started 2026-10-28T23:53:49.883231954Z with code
`76c1b6dee5b59b7b25807c3e3bfbae31350de4d9`, 128,000,000,000 bytes, zero
swap, 8 CPUs, and 2M bounded rows. It reuses the validated registry receipt;
there is no SQL or new map build. Three tests pass including coalescing, and
`finalizer_coalesced_qualification/attempt3/CHECKS.json` retains 10k
byte-exact SHA `18b73c...`; native code is unchanged. It coalesced 2M slots
into 96 joins including the final partial batch. The completed-ledger receipt
is recorded in the current section above.

The earlier 45k-chunk batched job stopped intentionally after 3.7M rows to
avoid 4,000 hash rebuilds (no OOM; partial retained). The SQLite and 200-GB
OOM runs are historical; their DB/intermediate/failure receipts remain
preserved under RUN.

**Next bounded action:** complete corrected GFFs and a matching identity ledger
against the completed registry, preserving opaque BODY IDs. Only a globally
non-unique Parent receives canonical `panSCup1_<sha256>`; globally unique
Parents remain unchanged. The raw graph GFFs are exon/body-only and
`from_annotation` ignores EXON, so namespace repair alone is not ingestion
ready. Use only frozen PURE ParentRollup observation/emission from existing
attributes (including established GENCODE gene-tag inheritance) to add gene,
transcript, and body-parent features; do not introduce a classifier or policy.
Validate namespace/structural-GFF readiness against the existing algorithm.
It is not strict-profile count readiness or full-count parity: facts,
protected-host compatibility, and preload retain their gates.
`build_corrected_count_gffs.py` is committed at
`31ec3b9802f5606717e66161e6869a0a869cfa37` with 21 passing tests (five gene
adapter plus 16 coordinator/rewrite). Native updated-fixture qualification
passes scoped tags for 3 Parents, isolated tag/body/gzip
(`RUN/native_gff_qualification/qualification-scoped-tags.log`). Fallback
count-identity collisions gate count readiness, not identifier-only structural
GFF generation. The Python writer was later stopped as recorded above; no
corrected GFF is published and no count/preload started.
Conditional counting and startup measurement remain authorized but are not yet
admitted or launched. No extra scale pilot, remapping, BAM, Nextflow,
release/tag, or cap increase is authorized.

## CURRENT: v0.10.0 local merge/release reconciliation (2026-10-27)

**User authorization:** merge the validated v0.10.0 source branch locally as a release;
create a local tag only after the merged-snapshot verification, with no push or publication.
This integration adds native `count`, the D093 cache/FALLBACK-only performance change, and
the D094 optional GBZ and graph-only XG+GBWT routes while retaining default XG behavior.
No scientific policy changed and no original scientific or release gate is waived. The
source-branch named-subset acceptance and its limits remain the applicable evidence. Fresh
merged commit `17d32ab5734b57cf53d1f0a45ac12426ae4cf35c` regression passed: build 56/56 and
serial CTest 179/179 (45.38 s; serial avoids known cross-test fixture-output collisions).
The binary SHA-256 is `5b61fbc5a9fe0ecdd5d57c7840e0c2a5bd8bcd9a06e29d7fd8885bd7ebbffc7e`.
Image packaging passed for `josephlalli/pancollapse:v0.10.0` (OCI version and CLI both
`0.10.0`); its existing all-format smoke passed both profiles (Parquet, MEX, RAD, and
read assignments) with no Python/libpython. The package script's fixture-work path was
mechanically repaired after a stale namespace failure; C++ was unchanged. Root will create
the local `v0.10.0` tag after committing this record, with no push. Original full scientific
gates remain pending and unwaived. The merge receipt root is
`/mnt/ssd/lalli/pancollapse_v0100_main_merge_20261027T225050Z/`.

## CURRENT: GBZ/GBWT feature implemented; scientific acceptance passed (2026-09-26)

**Gate Architecture Approved passed.** The accepted [design](docs/research/gbz-gbwt-count-inputs-design-20260926.md)
is implemented in `src/graph_input.{hpp,cpp}` and the common convert/count projection path.
`--gbz` and `--xg ... --gbwt ...` use in-memory FastLocate occurrence queries and selected
32/64-bit prefixes; the embedded-path XG route remains the default. Repeated visits are
preserved. No external `.ri` flag or custom persistent index is introduced. Annotation and
counting policies are unchanged.

The final XG build passes 179/179 tests; GBZ and XG+GBWT each pass 167/167 graph-dependent tests.
Both alternatives match 382 scientific fixture products. The sparse-ID presence check was
corrected after acceptance exposed that GBWT `contains()` checks numeric bounds only.
The named real-subset XG repeats pass scientific identity, and the new default-XG output also
matches the archived pre-feature baseline. Fresh XG runs took 2032.81/1500.07 seconds with
56,711,904/56,695,940 KiB peak RSS. GBZ (824.92 s / 39,134,904 KiB) and XG+GBWT
(788.47 s / 38,369,492 KiB) both pass scientific byte identity against XG: all 11 scientific
files, six gzip headers, scientific counters and manifest projection. This is the named
100,000-read subset, not the full GAMP. The archived-binary control also passes scientific
identity, at 914.99 s / 56,761,504 KiB. Both old and new XG runs exceed the earlier archive's
wall times, and the new runs are slower than the fresh control. Shared-host conditions were
uncontrolled; runtime nonregression and causal speedups are not established.
See the [implementation record](docs/research/gbz-gbwt-input-implementation-20260926.md) and
`/mnt/ssd/lalli/pancollapse_graph_input_implementation_v1_20260926/` for live receipts.

**Earlier design evidence:** repeated native XG counts on `filtered_first100k.gamp` (100,000 complete
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
The feature and named-subset acceptance are complete. Whole-process measurements, exact
commands, source/binary/library pins and comparison receipts are retained in the implementation
evidence root. Full-GAMP and chr2 native-count performance are not claimed. No external r-index
input flag is added, and no new optimization phase was started.

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
