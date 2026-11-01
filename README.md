# panCollapse

`panCollapse` counts genes directly from pangenome single-cell RNA-seq alignments
produced by `vg mpmap` (GAMP, multipath format). Run `panCollapse index` once to
prepare reference annotations, then `panCollapse count` for each sample. Native
counting writes compact Parquet counts and molecule tables, with optional 10x MEX
output; it needs no intermediate BAM.

`panCollapse convert` is retained as a third, **deprecated** command for BAM
conversion. It is not part of the native counting workflow.

## Current work

**CURRENT: [PROGRESS.md](PROGRESS.md) records the validated direct-facts count
(D110) and the next bounded optimization phase (D111).** The compiler stores
numeric node memberships, ready model metadata and directional splice facts in
one graph-specific object; count reads these facts directly without XG/GBWT/RI.
R7 completed the original 66,601,887-group GAMP in 6,372.41s with 245,153,902,592-B
HWM, exact R6 four-Parquet/six-MEX parity, and all 174 tests. Baseline commit
`c408857` preserves those artifacts and source. D111 permits only measured,
count-preserving engineering under the same <2h/<300e9-B/no-swap/no-spill gate;
it does not change biology, scoring, UMI handling, inputs, or release status.

### Required native-count inputs

Direct counting requires `--gamp`, a complete direct-facts `--annotation-index`,
`--barcode-whitelist`, one or more profile selectors, and `--out-dir`.
XG, transcript GBWT and its existing RI are **compiler inputs only**. No GFF,
ledger TSV, path lookup or splice discovery runs during counting. Build the
object once using the [annotation-index guide](docs/annotation-index.md).
Earlier PCANBND1 objects must be compiled into direct facts before counting.
Direct counting supports
Parquet and optional `--10x-mex`. BAM/RAD conversion is a separate deprecated
command; compatibility replay and per-read diagnostics are retired. GCSA/LCP and distance are mapper
inputs, not count inputs.

The assignment-cache ceiling scales from `--memory-budget SIZE` (or detected
available memory) and can be overridden with `--cache-limit assignment=SIZE`.
Node/edge/splice reference caches no longer exist on the count path. The cache
allocates on demand and does not spill. See [memory targeting](docs/annotation-index.md#choose-a-memory-target)
for the planning reserve, defaults and hard-limit distinction.

### Limit expensive reads

`panCollapse count` defaults to `--max-alignments-per-read 64000000`. It counts
complete alignment alternatives represented by the GAMP, including duplicates,
summed across all records for each read. Reads with more alternatives are skipped
before graph/annotation lookup; reads exactly at the limit are retained. This is
an input-complexity filter, not a gene-assignment rule. It does not enumerate or
score the alternatives, and skipped reads still contribute to the barcode prior.

Set a different nonnegative integer with `--max-alignments-per-read N`, or use
`0` to disable the filter. The manifest records the effective limit and
`high_complexity_read_groups`; the summary also reports the per-profile
`unassigned_high_complexity` terminal. The first-million-read fixture drops
982 reads (0.0982%) at the default; this is not a whole-genome accuracy result.

### Score complete alignments in node space

Counting uses the node-score scorer. It computes complete-alignment maxima
directly over each GAMP DAG without enumerating complete routes. At merges it
keeps separate best scores for distinct exon coverage, strand and directed-splice
eligibility summaries; models with identical local behavior share computation.
It never sums mutually exclusive alignments or requires an ordered annotated
reference walk. Known wrong-direction splices veto their transcript candidates.

Candidates compete in six exon-fraction/strand pools: fully exonic, at least
50% exonic, and below 50% exonic with gene-body support, on each strand. Each pool
retains transcript maxima within five flat VG score points of its best. Ranking
is sense E, antisense E, sense P, antisense P, sense B, antisense B before the
selected downstream counting policy. The manifest records
`assignment_algorithm: "node-score-v1"` and its five-point window.

Run `index` to compile annotations, bind their numeric metadata and prepare the
complete direct facts before counting. `count` accepts only the resulting
direct-facts index; it does not parse GFF/ledger text, query GBWT or build
expanded path-step indexes. Raw-annotation counting, expanded PCANCOUNT indexes,
compatibility capture/replay and the old assignment selector are retired.
The node-score algorithm is not claimed equivalent to legacy assignments.
BAM/RAD conversion remains available through deprecated `convert`.

## Historical current-work records

**GBZ and graph-only XG+GBWT inputs are implemented.** Both `convert` and `count`
accept the new graph flags through the same projection code. XG remains the default. All
projection fixtures pass in all three modes, and 382 scientific fixture products match per
alternative. All three modes produce identical scientific outputs on the named 100,000-read
real subset, including compressed MEX, RAD, Parquet and counters; repeated XG runs are also
identical. See the measurements and limits in the
[implementation record](docs/research/gbz-gbwt-input-implementation-20260926.md),
[PROGRESS.md](PROGRESS.md), and [D094](docs/decisions.md). The
[measured design](docs/research/gbz-gbwt-count-inputs-design-20260926.md) records chr21 graph
footprints and the explicitly conditional chr2 resource estimate.

**The full real input has a throughput cliff, and the current frontier is decoder cost after it.**
The full-input native count is complete: 25,755,749 reads and 124,261,698 alignments in a
210,645 s read phase (**817.86 s/100k reads**, peak RSS 137.48 GiB), validated with the completion
seal still pending. Its throughput holds 300–2,000 reads/s to read about 22,046,000 and then
collapses inside one roughly 12,288-read interval to about 21 reads/s: **85.0% of reads consume
14.2% of read time and the remaining 15.0% consume 85.8%.** Four cold 100,000-read windows cut from
one pass over the source GAMP rule out accumulated state, and post-cliff reads carry *fewer*
alignments each (5.41 against 9.99), ruling out multimapping. The cause is graph structure per
alignment — 326x the subpaths and 596x the mappings of the pre-cliff window. Nothing crosses a
software threshold; the cost function is smooth and the input steps. **Why the tail of this GAMP
carries roughly 600x more graph structure per alignment is not established**, and since retention
into the GAMP shifts at the same point (~69% locally against ~6.6% cumulatively early), the open
question belongs to the mapping pipeline, not to panCollapse; if these alignments are a mapper
artifact, making panCollapse consume them faster treats a symptom. The first profile ever taken on
the expensive regime puts **allocation at 38% of all CPU** there, against 17.13% on the 1M-read
prefix — so every prefix-measured estimate, including the allocator work below, understates this
regime. One fix is accepted and validated: sampling `SpaceUsedLong()` once per 4 MiB of serialized
bytes per decoder thread instead of calling it per record cut decoder CPU **14.7%** and raised
post-cliff throughput **102.8 → 114.3 reads/s**, with all four published tables byte-identical and
77/77 focused tests passing; it captured 68% of the predicted saving, and the residual ~148 CPU-s
is unexplained. Raising in-flight batch depth is rejected, and producer-side grouping is dead as a
hypothesis (93.5% of producer time is blocked waiting on the oldest batch to decode). An earlier
stage-attribution table is superseded: substring matching over demangled C++ signatures misfiled a
tally map into node-ledger build. Nothing is authorized next; arena allocation for decoded records
needs a design and human approval before any code, and asking the mapping pipeline owner comes
first. Full narrative,
evidence roots, and limits:
[full-input throughput cliff](docs/research/full-input-throughput-cliff-20260921.md);
see also [decisions D089](docs/decisions.md) and [current state](PROGRESS.md).

**Note on counter naming.** The counters `input_read_groups` and `completed_read_groups` count one
read's alignments, not a SAM/BAM `@RG` read group; see the terminology section of the record above.
Text written from 2026-09-21 says "read" and "alignments per read".

**The Allocator Substitution measurement is complete — superseded as the frontier, valid for the
1M-read prefix.** No drop-in allocator wins, and the next target it identified — the allocation
pattern itself — is superseded by the post-cliff decoder work above. Re-reading the already sealed
Parent-metadata profile with host glibc debug symbols (a re-analysis of the same `perf.data`, not
a new run) attributes **17.13% of user CPU to glibc allocator machinery** (`_int_malloc` 3.87%, `malloc_consolidate` 3.62%,
`unlink_chunk.isra.0` 2.57%, `cfree` 2.45%, `malloc` 2.44%, `_int_free` 2.18%) — larger than the
12.01% self share of the exact Ex50 evidence vector, and mostly invisible in the sealed self report
because the profiling container lacked debug symbols. Six sequential arms of the identical frozen
binary then ran on the same real first-1M-group input, differing only in `LD_PRELOAD` (glibc,
`tcmalloc_minimal`, jemalloc, two interleaved replicates each), with **zero source, build, or link
change**. All six validated PASS with byte-identical four tables and equal non-cache counters. The
repeated glibc arms set the within-session noise floor at **2.059 s/100k groups (6.76%)** on read
time and **417.48 s (6.82%)** on count-worker CPU. **tcmalloc-minimal is rejected**: read +173.74%
and worker CPU +130.50% against the baseline mean, about twenty times the floor, with GNU time
receipts showing lock contention (134.6M and 140.9M voluntary context switches against 3.4M for
glibc). **jemalloc is not established** at two replicates: read -5.52% against the mean does not
clear the floor, worker CPU -10.57% does but sits under a replicate spread two to three times the
floor, and peak RSS rises 1.86 GiB to about 74.06 GiB. The 17.13% attribution itself stands and is
strengthened, and caller attribution over all 350,676 sealed samples names the source: per-read
`uint32_t`-keyed `std::map` churn in the identity-collapse and resolve path
(`src/pathtally.hpp:196-197`, `src/main.cpp:5276-5287`), `AssignmentCandidate` teardown, and
protobuf record teardown. Reducing that allocation rate at its source was the next bounded
step identified at that checkpoint, and it remains **not authorized and not started**; it is no
longer the frontier, and it is a source change needing its own design,
review, and matched measurement. `__memcmp_avx2_movbe` (7.40% self) could not be attributed by the
same method and stays an open target. Whether to pursue jemalloc further is an open user decision;
promoting any allocator into the build would need a new decision entry and user approval. Evidence
root (STATUS `COMPLETE_VALIDATED`, 379 files sealed):
[results](/mnt/ssd/lalli/pancollapse_allocator_substitution_v1_20260918T035718Z/RESULTS.md),
[interpretation](/mnt/ssd/lalli/pancollapse_allocator_substitution_v1_20260918T035718Z/INTERPRETATION.md),
and
[caller attribution](/mnt/ssd/lalli/pancollapse_allocator_substitution_v1_20260918T035718Z/provenance/profile-reattribution/CALLERS.md).
See [decisions D089](docs/decisions.md) and [current state](PROGRESS.md).

**Fused exact-DP state insertion/deduplication trial is rejected for performance.**
The candidate passed build, 77/77 focused tests, 1/8/32 artifacts, byte-identical four tables, equal logical profiles, and equal non-cache counters. It regressed versus fresh frozen Parent-metadata binary `6eb3e7ca…d6843a`: **28.952 → 34.827 s/100k read** (+20.29%), worker CPU **5802.09 → 7073.95 s** (+21.92%), and RSS **72.292 → 93.585 GiB**. Only 0.479% of 12.271 billion emitted states were duplicates; the 8 MiB cap applies between groups, while one worker transiently reached 8.63 GiB. The candidate is preserved but active source was restored to pretrial bytes. See
[trial interpretation](/mnt/ssd/lalli/pancollapse_fused_state_v1_20260917T164000Z/INTERPRETATION.md),
[trial results](/mnt/ssd/lalli/pancollapse_fused_state_v1_20260917T164000Z/RESULTS.md), and
[current state](PROGRESS.md). No subsequent optimization had started at that checkpoint. The
allocator measurement
above has since re-measured the same frozen binary and confirmed this trial's 28.952 s/100k baseline
host-dependent: it measured 31.487 and 29.429 s/100k there.

**Profile context for the rejected trial.** The Parent-metadata profile found exact compatibility
at 29.76% inclusive CPU; its instrumented 36.221 s/100k read time and 72.162 GiB RSS are profile
context, not an unprofiled comparison. See its
[interpretation](/mnt/ssd/lalli/pancollapse_parent_metadata_profile_v1_20260917T152700Z/INTERPRETATION.md). Its
`libc.so.6` block (18.68% of self CPU over 15 rows, 13 of them unresolved addresses) has since been
resolved by re-reporting the same sealed `perf.data` against host `libc6-dbg`; that re-analysis
alters no sealed file and no prior conclusion.

**Immutable Parent metadata reuse is implemented and validated.** Native exact
and fallback candidates now reuse catalog references indexed by the existing Parent ranks.
Initialization validates each binding; profile policy and dynamic read evidence are unchanged.
The fresh unprofiled real-first-1M pair observed **39.306 → 27.755 s/100k groups** for read
processing (**29.39% lower**), finalization **0.243 → 0.247 s total**, and peak RSS
**72.248 → 72.155 GiB**. Count-worker CPU fell 26.62%. All four tables are byte-identical,
non-cache counters agree, 77 focused tests passed, and tiny artifact comparisons passed at
1/8/32 threads. This sequential shared-host observation had different host loads and build/test
overlap during part of the baseline; it is not representative full-input throughput or F1 evidence.
See the [bounded comparison](/mnt/ssd/lalli/pancollapse_parent_metadata_v1_20260917T105800Z/RESULTS.md)
and [current state](PROGRESS.md). The under-5-second aspiration and 16.5-second plausible endpoint
remain unmet; the completed profile above identifies the next hotspot.

**Earlier profile of the ordered-input binary, before Parent metadata reuse.**
Read time was **42.434 s/100k groups**, finalization **0.294 s**, and peak RSS **72.067 GiB**.
Parent catalog lookup alone used **16% of sampled CPU**; candidate construction used 22.02%
inclusive CPU. All four tables match the prior identical-input/binary result. This identified
the Parent metadata reuse target validated above. See the
[profile interpretation](/mnt/ssd/lalli/pancollapse_small_gamp_profile_v1_20260917T051400Z/INTERPRETATION.md).
The full 25.76M-group comparison was stopped at user request: native/coordinator exit 143,
baseline cancelled, partial evidence preserved in its
[stop receipt](/mnt/ssd/lalli/pancollapse_parallel_input_full_real_v1_20260917T011600Z/STOPPED.md).
The smaller input has 1M complete groups and the verified chr20–22 graph/bundle/whitelist
route. The existing small chr21-only input is simulated, so the parent proceeded with the
stated real-data assumption after an optional preference question. This is hotspot diagnosis,
not a representative full-input throughput claim. Detailed region timers and 49 Hz CPU stack
sampling are enabled; initialization is excluded from CPU samples. The frozen binary and
biological counting policy are unchanged. Exact scope, commands, phase boundaries, and results:
[small real-GAMP profile](/mnt/ssd/lalli/pancollapse_small_gamp_profile_v1_20260917T051400Z/README.md).
The original older full-data run below is independent and remains untouched.

**Ordered parallel native input is implemented and validated; the completed 1M-prefix trial did not improve speed.**
`src/ordered_gamp_reader.hpp` decodes bounded batches in parallel and delivers records
in original order to the existing read-grouping code. Native counting with at least
eight active requested threads uses this path; at `--threads 32`, the budget is 24
counting workers, four decoders, two BGZF workers, one BGZF input helper, and main.
The 64 MiB input-buffer setting is an admission watermark, not a strict memory cap.
All 77 focused tests and artifact comparisons at 1/8/32 threads passed; native
file/stdin hashes, cross-batch grouping, and fail-closed errors also passed.
On the same real first-1M input, read time was **43.683 → 55.891 s/100k** (+27.95%
observed), read plus finalization **43.715 → 55.933 s/100k**, and peak RSS
**72.096 → 72.219 GiB**. All four count tables are byte-identical and non-cache counters
agree. Counting workers spent only 0.67% of wall-time capacity waiting on an empty
input queue; decoders averaged 0.133 core. This prefix does not show input starvation
as the main limit. The candidate had 24 counting workers versus 32, and higher shared-host
load; no isolated causal percentage or whole-input claim follows. Both runtime targets
remain unmet. The uncommitted worktree retains the measured candidate, with no further
tuning or release promotion. Counting policy is unchanged.
Evidence: [ordered-input result](/mnt/ssd/lalli/pancollapse_parallel_input_v1_20260916T164124Z/RESULTS.md).

The completed bounded optimization is **per-read node-to-tally aggregation**, authorized after
the real-input inspection below. `src/node_tally_cache.hpp` now accumulates each node's
score/oriented bases before distributing the totals once to its raw reference paths, and
reuses its immutable node metadata during exact matching. Parent/transcript reductions and
biological policy are unchanged. The candidate passed 76/76 focused tests and small-fixture
count/compatibility/RAD/diagnostic/BAM-content comparisons at one and four workers.
The matched first-1M-real-group trial passed: read time was **63.018 → 55.226 s/100k**
(12.37% lower observed); read plus count finalization was **63.120 → 55.321 s/100k**,
with peak RSS **72.161 → 72.110 GiB**. All four count tables are byte-identical and
non-cache counters match. This single sequential pair had different shared-host loads;
both <5 and 16.5 s/100k targets remain unmet. See the
[result](/mnt/ssd/lalli/pancollapse_node_tally_cache_v1_20260916T125145Z/RESULTS.md).
The independent full real-data count below continues untouched.

The prior native-count fallback phase is complete and sealed in
`/mnt/ssd/lalli/pancollapse_native_larger_3m_v1_20260915T231536Z` (75 files verified,
completion-seal exit 0). Its 100k/1M/3M evidence remains authoritative: repeated 1M
resident median **10.878205 s/100k** was 27.52% lower observed than repeated baseline,
while standalone 100k was **21.427850 s** versus 20.355946 baseline (5.27% higher),
3M was 6.176290% higher observed, preferred **<5 s/100k** remains unmet, and 20M
scaling remains unproven.

The separate real-input measurement phase is **terminal and validated, with the completion seal
still pending** (STATUS `VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`; finished 2026-09-18 13:23 UTC per
`REPORT_RECEIPT.json`, every stage exit 0). Its
selected sealed input is the 25,755,749-group / 127,202,588,396-byte genuine 10x ARC GEX
`site1_donor2_asian` GAMP at
`/mnt/ssd/lalli/hprc_v2_vg_rna/chr20_compare/readsets/real_scrna_benchmark_v1/pangenome_mapping_reuse_v15r2_20260908T150525-0500/site1_donor2_asian/d10k_cap20k/mapping/reads.gamp`
(SHA-256 `000858121cdcbbc09364e423586ca57324e19c8ef0ad968a3308c91b7e7e87fe`). It uses
the matching chr20--22 graph and corrected-bundle ledger, and is not a whole-pangenome
or accuracy/parity measurement. The frozen resident binary is
`3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958` — note that this is **not**
the currently frozen `6eb3e7ca…d6843a` and predates ordered parallel input and Parent metadata
reuse, so no absolute comparison between this run and prefix runs is a scale effect; the launch was
`pancollapse-real-native-20260916T022706Z`; its receipts are
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/logs/count.stderr`,
`count.time`, and `count.host.samples`; terminal receipts are `logs/count.exit.tsv` and
`logs/coordinator.exit.tsv`. It ran 32 workers, CR7+strict, a 64 GiB aggregation budget,
and a 256 GiB no-swap Docker limit, with diagnostics off and fine timing unset. Terminal figures
are read phase 210,645.357663 s (**817.857627 s/100k reads**; 817.985728 s/100k including count
finalization), initialization 516.531992 s, count finalization 32.993320 s, publication 5.993920 s,
whole-command wall 211,323.00 s, and peak process RSS 137.476540 GiB. Both the preferred <5 s/100k
and the 16.5 s/100k fallback are missed, the latter by roughly fifty times. It establishes no
truth-based accuracy result and authorizes no mapping or policy change. Do not duplicate or
restart it, and do not seal it here.

An earlier bounded input inspection identified one source of extra work, and it is prefix-only:
the first
20,000 real groups contain 19.16x as many node mapping entries as the fixture prefix,
with 69.05% concentrated in the most complex 1% of groups. These expand scoring and
exact-compatibility traversals before assignment caching. See the
[structural comparison](/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/attribution/RESULTS.md)
for the extreme-read example and the limits of this prefix-only observation. It does not describe
the post-cliff regime, which the cliff characterization above measured directly.

Start with [PROGRESS.md](PROGRESS.md), then the
[full-input throughput cliff record](docs/research/full-input-throughput-cliff-20260921.md) for the
current frontier, the
[native-count performance record](docs/research/native-count-processing-performance-20260915.md)
for the prefix-measured phases it supersedes,
and D089 in [decisions](docs/decisions.md). The current gate remains the later
machine-verifiable increment after Vertical Slice Proven. The cache and ordered-input prefix
trials and immutable-Parent-metadata validation are complete; the full-data pair was user-stopped
and its baseline cancelled. The subsequent bounded profile is complete and validated,
following the reviewed documentation-agent checkpoint. Preserve the independent
original full count.

It bridges a pangenome aligner and the standard single-cell counting workflow: for each read
it reports the set of transcripts the read is compatible with (with orientation), reading
compatibility directly off the haplotype-specific transcript (HST) paths that `vg rna`
embedded in the graph — no GTF or annotation projection at run time.

## Requirements

- An existing VG installation/checkout with compatible XG, GBWT, GBWTGraph, VGio and
  handlegraph libraries. panCollapse does not fetch or rebuild VG.
- C++20 (GCC 15), CMake, and Ninja.
- Pinned Arrow/Parquet C++ headers and shared libraries for native count output
  (`PANCOLLAPSE_ARROW_ROOT` when CMake package discovery cannot find them).
- [alevin-fry](https://github.com/COMBINE-lab/alevin-fry) only when using legacy
  `convert`/RAD output with its downstream quantification route; native `count` has no
  alevin-fry runtime dependency.
- Python 3 to run the test suite.

## Build

```sh
VG=/path/to/vg                      # a vg checkout/install with lib/pkgconfig
PKG_CONFIG_PATH="$VG/lib/pkgconfig" cmake -S . -B build -G Ninja \
  -DPANCOLLAPSE_VG_ROOT="$VG" \
  -DPANCOLLAPSE_ARROW_ROOT=/path/to/arrow-parquet \
  -DCMAKE_PREFIX_PATH="$VG" \
  -DCMAKE_BUILD_RPATH="$VG/lib"
cmake --build build
ctest --test-dir build --output-on-failure
```

The executable is `build/src/panCollapse`.

The projection tests can use each backend without changing their scientific oracles:

```sh
cmake -S . -B build -DPANCOLLAPSE_TEST_GRAPH_MODE=gbz   # or xg, xg-gbwt
ctest --test-dir build -L vg --output-on-failure
```

`PANCOLLAPSE_TEST_VG` selects the VG executable used to build tiny fixtures. Set it to a
wrapper around the required stock VG container when reproducing this feature's acceptance.
Each mode has its own fixture directory. `tests/vg/compare_projection_outputs.py` compares
those directories, and `tests/vg/compare_graph_scientific.py` compares native-count datasets
under the documented scientific/provenance contract.

### Configure variables

| Variable | Required | Description |
|---|---|---|
| `PANCOLLAPSE_VG_ROOT` | Yes | Compatible VG checkout/build with `include/xg.hpp`, GBWT/GBWTGraph headers and static libraries, and `lib/pkgconfig`. |
| `PANCOLLAPSE_ARROW_ROOT` | Native count | Pinned Arrow/Parquet C++ prefix containing `include/`, `libarrow`, and `libparquet`; required when their CMake package files are not discoverable. |
| `CMAKE_PREFIX_PATH` | Recommended | Set to the same VG path so CMake finds `libhandlegraph`, `VGio`, and `Protobuf`. |
| `CMAKE_BUILD_RPATH` | Recommended | Set to `$VG/lib` so the built binary finds VG's shared libraries at run time. |
| `PKG_CONFIG_PATH` | Recommended | Prepend `$VG/lib/pkgconfig` so pkg-config finds `sdsl-lite`, `libdivsufsort`, and `absl_*`. |
| `PANCOLLAPSE_ABSL_PKGCONFIG` | No | Optional path to an Abseil pkg-config directory when Abseil is not on the standard search path (e.g. a non-default Homebrew or Linuxbrew Cellar). Leave unset if `PKG_CONFIG_PATH` already covers Abseil. |

## Usage

```
panCollapse convert --gamp reads.gamp|- GRAPH_INPUT --out-dir out
                    (--path-identity-ledger path_identity_ledger.tsv |
                     --legacy-adapter hst-v1 --t2g t2g.tsv)
                    [--raw-cb-length 16] [--raw-umi-length 12]
                    [--score flat|qualadj] [--molecule-identity-failures skip|fail]
                    [--strand both|forward|reverse]
                    [--count-mode score|gene|genefull|genefull_exonoverintron|genefull_ex50pas]
                    [--body-t2g body.t2g]
                    [--bam-out reads.bam] [--bam-multigene omit|first|all]
                    [--threads N]
                    [--no-ex50-score-window]

panCollapse count (--gamp reads.gamp GRAPH_INPUT \
                   [--compatibility-out compatibility] | \
                   --compatibility-in compatibility) \
                  (--count-bundle bundle | \
                   --annotation-index annotation.pci --path-identity-ledger ledger.tsv | \
                   --annotation corrected.gff3 --annotation-reference reference.gff3 \
                   --path-identity-ledger ledger.tsv) \
                  --barcode-whitelist barcodes.txt PROFILE_SELECTOR [PROFILE_SELECTOR ...] \
                  --out-dir counts [--t2g transcripts.tsv] [--body-t2g bodies.tsv] \
                  [--threads N] \
                  [--10x-mex] [--rad-out] \
                  [--read-assignments-out diagnostics/read_assignments.parquet]

GRAPH_INPUT := --xg transcripts.xg | --gbz transcripts.gbz | --xg graph_only.xg --gbwt transcripts.gbwt
PROFILE_SELECTOR := --cr7 | --pansc-strict-v1 | --profile ID
```

`count` takes annotation identity and policy from exactly one fact authority: its
checksum-verified `panSC-count-facts-v1` bundle (`--count-bundle`), or a corrected GFF3 carrying
the correction chain's classifier tags together with its reference GFF3 and the path-identity
ledger (`--annotation`, `--annotation-reference`, `--path-identity-ledger`), or compiled
annotation facts (`--annotation-index`, with the same `--path-identity-ledger`). Select exactly
one route. See [building an annotation index](docs/annotation-index.md).
Either annotation file may be gzipped. The annotation route is recorded as D088 in
[`docs/decisions.md`](docs/decisions.md). The corrected 1M comparison establishes CR7 parity;
panSC-strict differs on nested-host/protection facts, with strict parity no longer an
acceptance requirement per the 2026-09-15 user decision. It is refused with
`--compatibility-in`, which replays against a bundle. `count` refuses legacy adapters, external allowlists, and BAM/debug
evidence options. At least one unique profile selector is required. The
`--cr7` and `--pansc-strict-v1` aliases select immutable `cr7-v1` and
`pansc-strict-v1`; they may be combined in one GAMP pass or mixed with repeatable
`--profile ID`. Optional `--t2g` and `--body-t2g` inputs are compatibility assertions
against the fact authority, not assignment inputs.

From the repository root, with `build/src/panCollapse` built, compile facts once:

```bash
build/src/panCollapse index \
  --annotation corrected.gff3.gz --annotation-reference reference.gff3.gz \
  --path-identity-ledger ledger.tsv --out annotation.pci

build/src/panCollapse count \
  --gamp reads.gamp --xg graph.xg --gbwt transcripts.gbwt \
  --annotation-index annotation.pci --path-identity-ledger ledger.tsv \
  --barcode-whitelist barcodes.txt --cr7 --pansc-strict-v1 --out-dir counts
```

This skips annotation parsing and fact derivation, but does **not** yet persist the ledger,
graph lookup structures, path positions, or exon/splice geometry. Whole-genome memory fit
is unproven. Annotation inputs may be plain or gzipped; explicitly select GTF with
`--annotation-format gtf` and/or `--annotation-reference-format gtf` during indexing.
GTF requires explicit gene/transcript features with IDs matching the ledger; exon-only
GTF is not supported. Legacy `convert` is unchanged.

Count aggregation, barcode correction and UMI reduction are memory-only. No temporary
aggregate spill/merge files are written. `--count-memory-budget` has been removed;
use the whole-process/container memory limit. If memory is exhausted the run fails,
without publishing partial counts. This does not remove normal compressed count output.

Reference loading retains compact lexical ledger tables and numeric Parent policy records.
GBWT base positions use exact 64-step checkpoints rather than a coordinate for every step;
stderr reports selected steps, checkpoint counts and coordinate-buffer bytes. Native counting
uses bounded, discardable node/model lookup caches (1 GiB/256 MiB of accounted resident data),
while active reads may retain additional leased values beyond those bounds. Reference state is released
before UMI finalization. These are storage changes, not annotation filters or assignment rules;
existing `annotation.pci` files remain compatible. Whole-genome peak RAM and throughput are
not yet qualified for this implementation.

`--compatibility-out` additionally publishes a checksum-bound, non-BAM Parquet
bundle containing the complete ordered read-group denominator and its policy-neutral
E/P/B and S/U compatibility facts. A later `--compatibility-in` run rebinds those
facts to a current corrected count-facts bundle and reapplies the selected profiles
and barcode whitelist without opening GAMP or XG. This is the fast filter/tagging
iteration path; see [`docs/read-compatibility-bundle.md`](docs/read-compatibility-bundle.md).

Its default output is a Parquet dataset with barcode, feature, count, and molecule tables
plus `manifest.json` and `summary.tsv`. `--10x-mex` adds per-profile MEX and
`--rad-out` adds a RAD compatibility artifact. `--read-assignments-out` adds an ordered,
compressed Parquet per-read audit sidecar beneath `--out-dir`; it must be a normalized relative
path ending in `.parquet`. It is disabled by default because it intentionally restores
per-read-scale output I/O. Native `count` never writes BAM and requires no Python at production
runtime.

Assignment-policy sensitivities use repeatable
`--profile-override BASE:FIELD=VALUE` together with the mandatory
`--analysis-scope sensitivity-analysis`. Unknown, repeated, contradictory, or incorrectly typed
overrides fail before GAMP is read. An override always produces a
`derived-<base>-<hash-prefix>` profile; the manifest retains the full effective-profile SHA-256,
so derived results are never mislabeled as a frozen profile. The definitive typed field list and
output schemas are in [`docs/input-output-contract.md`](docs/input-output-contract.md).

### Inputs

- `--gamp` — name-grouped `vg mpmap -F GAMP` multipath alignments. All records for one read
  must be contiguous. Read names must carry the raw 10x barcode and UMI as
  `<original_name>_<raw_CB>_<raw_UMI>_cy<hex(CY)>_uy<hex(UY)>` (place them there during FASTQ
  preparation, before mapping). `CY` is the raw barcode-quality string encoded as ASCII hex
  so whitespace or `_` cannot corrupt the QNAME grammar. Legacy names without `_cy...` remain
  accepted but cannot emit `CY`. Pass `-` to read the GAMP stream from stdin.
- `--xg` — the `.xg` for the same graph that produced the GAMP. Without `--gbwt`, its
  embedded transcript paths remain the default input.
- `--gbz` — a GBZ containing graph sequence and transcript paths. Exclusive with `--xg`
  and `--gbwt`. GBZs carrying a node-to-segment translation are rejected. All aligned nodes
  must be retained in the GBZ; unselected guide paths can retain nodes outside transcripts.
- `--gbwt` — transcript paths used alongside `--xg`; the XG may have no paths. GBWT node
  IDs must belong to that XG. Metadata names must exactly match the ledger's `vg_path_name`,
  with matching path lengths. Both GBWT routes require bidirectional indexes and complete
  path-name metadata. FastLocate is built once in memory; no `.ri` input or persistent
  panCollapse index is required. Exact counting stores base coordinates only for selected
  ledger paths; membership-only projection omits those arrays.
- `--compatibility-in` — a previously published
  `pancollapse-read-compatibility-v1` directory. It is exclusive with GAMP and all graph inputs, and
  with `--compatibility-out`; replay still requires the corrected count-facts
  bundle, barcode whitelist, and profile selectors.
- `--path-identity-ledger` — production input using schema `panSC-path-identity-v1`. It maps each
  exact `vg_path_name` through `unique_parent` to `canonical_transcript` and `gene_id`, carries
  exon/body layer identity and graph provenance, and is checked against exact graph path names and
  lengths before any GAMP is read.
- `--legacy-adapter hst-v1 --t2g` — explicit compatibility input:
  `graph_path<TAB>gene[<TAB>canonical_transcript]`. With two columns,
  panCollapse preserves the original convention and strips a terminal `_H<n>` / `_R<n>`
  from the graph path to obtain the transcript ID. Optional column 3 explicitly aliases an
  arbitrary raw path name (for example, a CAT-projected haplotype transcript) to its canonical
  transcript. Multiple paths may alias to one transcript; one path may not alias to several
  transcripts, and one canonical transcript may not map to several genes.
  There is no file-shape auto-detection: old t2gs are rejected unless `hst-v1` is selected.

### Options

- `--raw-cb-length` / `--raw-umi-length` — lengths of the raw barcode and UMI parsed from the
  read name (defaults 16 and 12).
- `--score flat|qualadj` — per-node scoring. `flat` (default) reproduces vg's alignment scheme;
  `qualadj` reproduces vg's base-quality-adjusted scoring for exact fidelity to
  quality-adjusted mapping.
- `--molecule-identity-failures skip|fail` — how to treat reads whose name has a missing,
  malformed, or wrong-length CB/UMI/CY/UY field (default `skip`, counted in the summary).
- `--threads N` — number of initialization and complete-read-group workers (default `1`). Exact
  Parent models are built in adaptive lexical blocks; splice geometry is split by canonical target
  (or legacy gene); exact exon-edge geometry is immutable and Parent-local; and demand-built
  node/model-candidate cache misses use 256 independent shards. Initialization results commit in
  stable order through a bounded window, one parser still owns GAMP grouping and recurrence
  validation, and one ordered writer preserves original GAMP order. `N` must be positive. Exact
  inputs with fewer than 32 models automatically use one active processing worker (the requested
  ceiling is still reported), because queue handoff is measurably slower at that scale. Queued
  groups, initialization scratch, and worker scratch otherwise grow with `N`. Start with 8 or 16
  on a bounded representative slice, then increase only while measured throughput improves.
  For `convert`, supported requested thread counts produce byte-identical persisted artifacts.
  For native `count`, canonical rows and logical hashes are identical; operational manifest and
  summary fields still report the requested/active threads, timings and cache behavior;
  legacy aggregate-spill counters are always zero in the memory-only counter.
  Compatibility production uses this worker count and is byte-deterministic across supported
  counts. Compatibility replay currently scans with one active worker and reports that fact on
  stderr and in its count manifest; graph traversal has already been eliminated from replay.
- `--compatibility-out <dir>` — atomically publish reusable pre-policy compatibility evidence to
  a destination disjoint from `--out-dir`. Read rows stream through a checksummed ZSTD spool;
  only canonical unique fact sets remain resident while GAMP is read, and fact-table publication
  uses row-, UTF-8-byte-, and structural-list-value-bounded Parquet batches.
- `--strand both|forward|reverse` — target-relative orientation filter (default `both`, no
  filtering). `forward` keeps only targets the read aligns to in the same (sense) orientation;
  `reverse` keeps only antisense targets. Reads left with no matching target emit no record and
  are counted in `strand_filtered_groups`. Orientation is the majority of aligned bases (the RAD
  `dirs`). Use `forward` for a sense-stranded library to drop antisense artifacts.
- `--bam-out <path>` — also write a BAM for a CellRanger-style counting stack (UMI-tools +
  DropletUtils `emptyDropsCellRanger`). Opt-in; the RAD is byte-identical with or without it.
  See [`docs/bam-export.md`](docs/bam-export.md).
- `--bam-multigene omit|first|all` — `XT` tag policy for multi-gene reads (default `omit`). `all` is
  a ledger-`--count-mode`-only rescue path that carries a multi-gene read into the BAM instead of
  dropping it, for a downstream UMI-level rescue; see [`docs/bam-export.md`](docs/bam-export.md).
- `--no-ex50-score-window` — disable v0.8's default exact-Ex50 top-minus-five eligibility filter
  and send every complete compatible exact traversal to downstream E/P/B-orientation ranking.
  This is an explicit pre-D068 sensitivity mode; it does not change RAD or ordinary Gene evidence.
- `--body-t2g` — `hst-v1`-only body annotation. A two-column `body_path<TAB>gene` file preserves the
  legacy gene-body/span classifier. A consistently three-column
  `raw_body_path<TAB>gene<TAB>canonical_transcript` file enables transcript-first bodies: raw
  body copies/fragments MAX-collapse to the same canonical transcript used by `--t2g`, and S/U
  is classified per transcript before count_cr groups transcripts into genes. Mixed two- and
  three-column rows are rejected.

### GeneFull / STARsolo-style counting

`scripts/make-gene-annotation.sh -x graph.xg -n annotation.gtf -o genefull -l haplotypes.gbwt`
embeds one unspliced gene-body path per gene (covering introns) alongside the HST paths, and
writes `genefull.t2g.tsv`. There are two ways to use it:

- **Coarse, no flag:** run the default count with your HST t2g for a spliced count, or with the
  gene-body t2g for a GeneFull-ish count. Same graph, the t2g selects the layer.
- **`--count-mode` for STARsolo/CellRanger rules:** `gene` (STARsolo `Gene`), `genefull`
  (STARsolo `GeneFull`), and `genefull_exonoverintron` emit a transcript-specific spliced/unspliced
  ledger (`TX`/`GL`). Production `genefull_ex50pas` instead emits exact STARsolo 2.7.11b
  Parent-preserving E/P/B overlap evidence (`TX`/`GT`/`XP`/`XU`) so a downstream counter can apply
  the global six-rank sense/antisense priority. See [`docs/genefull.md`](docs/genefull.md).

Production count modes take both exon and body rows from one `--path-identity-ledger`; they do not
take `--t2g` or `--body-t2g`. The old two-file forms remain available only through
`--legacy-adapter hst-v1`, except exact `genefull_ex50pas`, which rejects the legacy adapter.

For transcript-first counting, use the production path ledger, `--bam-out`, and
`--bam-multigene all`. Ordinary modes retain the complete `TX`/`GX`/`GD`/`GL` ledger. Exact
`genefull_ex50pas` requires all three options and emits parallel `TX`/`GX`/`GD`/`GT`/`XP`/`XU`,
with one exact path/Parent per evidence slot and repeated canonical `TX` values allowed. count_cr
performs the only priority, transcript-to-gene, and UMI reduction step. By default, exact evidence
must first be within five alignment-score points of the global compatible optimum. Pass
`--no-ex50-score-window` to restore all-compatible exact evidence for a sensitivity run. The
existing RAD Unique policy and bytes are unchanged.

### Outputs (in `--out-dir`)

- `map.rad` — mapper-style uncollated RAD (raw CB/UMI, compatible transcript targets, and
  per-target orientation), written with a streaming seek-and-backpatch writer.
- `tx2gene.tsv` — transcript-to-gene map for `alevin-fry quant`.
- `summary.tsv` — per-run counters (records, emitted groups, no-compatible / unaligned reads,
  molecule-identity skips) plus `exact_ex50_score_window` provenance (`5`, `disabled`, or
  `not_applicable`).
- `reads.bam` (only with `--bam-out`) — one record per valid input read group. Feature-bearing
  records are mapped nominally and carry 10x molecule/feature tags (`CB`/`UB`/`CY`,
  `GX`/`GN`, `TX`/`XP`/`XU` for production identity, `GL` or exact-tier `GT`, and optional `XT`). Featureless groups
  are unmapped `XB:Z:barcode_only` records with no gene tags, retained only for downstream
  barcode correction. Genes come from the graph, not a linear reference. See
  [`docs/bam-export.md`](docs/bam-export.md).

Initialization time, processing time, effective group throughput, and periodic progress are
reported on stderr only. The bounded v0.9 scaling evidence and its interpretation limits are in
[`docs/research/v090-deterministic-parallelism.md`](docs/research/v090-deterministic-parallelism.md).
For a diagnostic run, setting `PANCOLLAPSE_PROFILE_TIMING=1` also reports queue high-water,
producer admission wait, worker compute, ordered wait, and ordered-region time. These intervals
overlap across workers and must not be added as though they were wall time; leave the variable
unset for ordinary runs.

The same variable adds a `read-path regions:` line attributing the whole per-read path across
nine regions, in the order they execute: `prelude`, `tally`, `evidence`, `facts`, `signature`,
`digest`, `resolve`, `observe`, `tail`. Each is reported in worker-seconds, and the line ends
with `region_sum_seconds` beside `compute_total_seconds` so a coverage shortfall is visible in
the output rather than inferred: a partial bracketing would otherwise point optimization at the
largest measured region while a larger unmeasured one stays invisible. Regions are per-worker on
their own cache lines and gated behind the variable, and enabling it does not change output.

Initialization is attributed separately by `initialization: phase=` lines, which are not gated
behind the variable and print on ordinary runs. The GAMP/XG path reports `metadata_parse`, `xg_deserialize`,
`ledger_path_handles`, `path_catalog`, `exact_parent_models`, `splice_geometry`, plus
`annotation_count_facts` on the `--annotation` route. `--compatibility-in` replay reports eight:
`count_bundle_load`, `identity_ledger_read`, `count_fact_catalog`, `compatibility_open`,
`structural_surface_check`, `fact_set_resolve`, `output_staging`, and `runtime_construct`.
`PANCOLLAPSE_FORCE_WORKER_POOL=1` disables, for `count` only, the serial fallback that small
exact surfaces would otherwise trigger; it exists so the test suite can exercise the worker pool.

## Example

```sh
# reads.fastq read names already end in _<CB>_<UMI>
vg mpmap -n rna -x graph.spliced.xg -g graph.spliced.gcsa -d graph.spliced.dist \
         -F GAMP -f reads.fastq \
  | panCollapse convert --gamp - --xg graph.spliced.xg \
      --path-identity-ledger path_identity_ledger.tsv --out-dir out

alevin-fry generate-permit-list -i out -d fw -o pl --unfiltered-pl 3M-february-2018.txt
alevin-fry collate -i pl -r out
alevin-fry quant -i pl -m out/tx2gene.tsv -o quant -r cr-like --use-mtx
```

## Docker

The current runtime image is built locally as `josephlalli/pancollapse:v0.10.0`;
publish that tag before using it from a host that does not already have the
validated local image. It bundles the panCollapse binary with the exact
shared-library closure it was built against, so it does not need vg installed
at runtime (vg and alevin-fry remain separate tools for the surrounding
pipeline steps). Mount your inputs and an output directory:

```sh
docker run --rm \
  -v "$PWD":/work \
  josephlalli/pancollapse:v0.10.0 count \
  --gamp /work/reads.gamp --xg /work/graph.spliced.xg \
  --count-bundle /work/count-facts --barcode-whitelist /work/barcodes.txt \
  --cr7 --out-dir /work/out
```

It also reads a GAMP stream on stdin (`--gamp -`). To build the image locally after compiling
the binary, run [`scripts/tooling/build-docker-image.sh`](scripts/tooling/build-docker-image.sh), which stages
the binary, Arrow/Parquet/Zstandard closure, and tags `josephlalli/pancollapse:v0.10.0`. The
release gate refuses a missing native-output library or any `libpython` runtime dependency;
Python is not required in the image.

## How it works

For each read, across all of its GAMP alignments, panCollapse scores every aligned node under
vg's own scoring scheme and adds each node's score to the exact ledger paths crossing it. Paths
MAX-collapse within their unique Parent, Parents MAX-collapse within canonical transcript, and
canonical transcripts tied at the global top become RAD `refs`. Alternative paths/Parents are
never summed. The converter records
each transcript's orientation from the read's direction along the HST path. The full algorithm
is in [`docs/conversion-algorithm.md`](docs/conversion-algorithm.md).

## Documentation

- [`docs/conversion-algorithm.md`](docs/conversion-algorithm.md) — the conversion algorithm.
- [`docs/input-output-contract.md`](docs/input-output-contract.md) — inputs, outputs, exits.
- [`docs/product-spec.md`](docs/product-spec.md) — the product contract.
- [`docs/decisions.md`](docs/decisions.md) — design decisions and rationale.

## License

Apache License 2.0.
