# PanCollapse v0.9.0 deterministic-parallelism benchmark

Date: 2026-09-03

Status: bounded engineering evidence, not a whole-pangenome runtime projection.

## Question

Does v0.9.0 reduce the exact GeneFull hot path before threading, and do initialization plus
complete-read-group parallelism scale without changing persisted evidence?

## Compared builds

- v0.8.2: commit `f5b5cae74bd69ca44e39dbdba80d7f86c90cdf33`, existing optimized build.
- Initial v0.9.0 parallel candidate: commit `5769f11a5c7216db72d5ab01d5a448538bc8199a`.
- Final v0.9.0 candidate: branch `worktree-codex-v090-performance`, optimized
  `RelWithDebInfo` build after numeric production collapse and immutable exon-edge sharing.

Both builds used the normal typed-union BAM, the default five-point exact score window, and no
compact projection.

## Bounded adversarial fixture

The generated graph has one exon path and 2,000 linked body paths crossing the same nodes. Each
of 5,000 independent read-name groups therefore presents 2,000 exact body models to the dynamic
program while collapsing to one canonical transcript. This isolates the candidate/state hot path
without generating large evidence records. It is intentionally adversarial and synthetic: it
tests computational scaling, not biological representativeness.

Scratch root:
`/mnt/ssd/lalli/pancollapse-v090-bench.63dBCi/complex2000`

| Input | Bytes | SHA-256 |
|---|---:|---|
| `graph.gfa` | 45,000 | `f3308d8405f1b89060258e8cd56dfe53b3d134ae37a62655429053d9b57fa8c8` |
| `graph.xg` | 338,536 | `1994b2d36fb74d6588be0a158baa348129d6baee98ecb8503f61b403438cf9aa` |
| `ledger.tsv` | 366,286 | `b613e4ad0598cc4c6956f22c31e5c21c223e129d9fc343a60ecb694210adc1bd` |
| `reads.5000.json` | 1,443,890 | `128397a2d6ef107785f71d806bb423cf109513faed4282f8949ec13c4b08550b` |
| `reads.5000.gamp` | 14,951 | `cb54c9df75d2e28f88df3bcd22873892dec89c0c18280663a9fb5fe31c6293eb` |

Each primary point below is the median of three sequential `/usr/bin/time` runs on the same host.

| Build | Workers | Wall replicates (s) | Median wall (s) | Speedup vs 0.8.2 | Speedup vs v0.9.0 one worker |
|---|---:|---|---:|---:|---:|
| v0.8.2 | 1 | 24.86, 25.65, 24.67 | 24.86 | 1.00x | - |
| initial v0.9.0 | 1 | 19.89, 19.67, 19.88 | 19.88 | 1.25x | 1.00x |
| initial v0.9.0 | 8 | 2.84, 2.68, 2.62 | 2.68 | 9.28x | 7.42x |
| initial v0.9.0 | 16 | 1.50, 1.83, 1.95 | 1.83 | 13.58x | 10.86x |

Single observations at two and four workers were 10.03 s and 5.07 s, respectively, consistent
with the primary scaling series. Median peak RSS stayed between 16 and 20 MiB for the tabled
points; this small graph does not estimate whole-pangenome resident memory.

The v0.8.2 and v0.9.0 benchmark RAD and summary artifacts were identical:

- `map.rad`: `aa55756ed6fc646901671a51243f1383c2fcecb82a1330ceddaf7a5794bacea6`
- `summary.tsv`: `ffc97d611650d0bb19536ac020dc3b3a3a7bba1689738ec74d8420d6d40306b8`

The committed fixture tests separately hold invocation paths constant and require byte identity
for RAD, BAM, summary, tx2gene, and debug evidence at 1, 2, 4, and 8 workers. Score mode has an
independent default/1/8-worker byte-identity test for its separate node cache.

## Final production hot-path amendment

Profiling the initial candidate showed that production count mode still hashed/copy-constructed
long path and Parent strings inside every read group, reserved two full Parent hash tables, and
took a shared exact-exon-edge cache lock for every gapped model transition. The final candidate:

- tallies XG path handles and performs the path -> Parent -> target MAX hierarchy with numeric
  IDs, one layer-qualified Parent table, and ledger-owned name pointers;
- constructs exon/body occurrence geometry once per Parent, retaining per-model maps only for
  repeated-but-resolvable body occurrences;
- shares exon splice-edge sets only after exact ordered-step equality and lets each distinct model
  retain its own path, Parent, target, rank, orientation, and terminal evidence; and
- waits until read-local exact DP has completed before taking the ordered turn for debug output.

The final no-debug measurements use the same 2,000-model/5,000-group fixture and are medians of
three sequential runs. Speedup is reported against v0.8.2 and against the final one-worker run;
64 and 128 workers are included to show the overhead boundary, not as recommendations.

| Build | Workers | Wall replicates (s) | Median wall (s) | Speedup vs 0.8.2 | Speedup vs final one worker |
|---|---:|---|---:|---:|---:|
| final v0.9.0 | 1 | 3.79, 3.74, 3.76 | 3.76 | 6.61x | 1.00x |
| final v0.9.0 | 2 | 1.88, 1.91, 1.91 | 1.91 | 13.02x | 1.97x |
| final v0.9.0 | 4 | 1.02, 0.98, 0.95 | 0.98 | 25.37x | 3.84x |
| final v0.9.0 | 8 | 0.52, 0.51, 0.53 | 0.52 | 47.81x | 7.23x |
| final v0.9.0 | 16 | 0.35, 0.31, 0.33 | 0.33 | 75.33x | 11.39x |
| final v0.9.0 | 32 | 0.25, 0.26, 0.24 | 0.25 | 99.44x | 15.04x |
| final v0.9.0 | 64 | 0.56, 0.44, 0.46 | 0.46 | 54.04x | 8.17x |
| final v0.9.0 | 128 | 0.74, 1.02, 0.93 | 0.93 | 26.73x | 4.04x |

The final RAD and summary hashes remain the values above. A separate same-host diagnostic-sidecar
spot check fell from 7.32 s at eight workers on `5769f11` to 0.53 s on the final candidate; the
sidecar, BAM, RAD, summary, and tx2gene artifacts were byte-identical.

### Baseline differential receipt

The release transition was also checked directly against a separately built, clean `5769f11`
binary. Both binaries were copied in turn to one common invocation path so BAM `@PG CL` text was
identical. The generated 64-Parent fixture used three exon and three body aliases per Parent,
reversed GFA path insertion relative to lexical names, explicit exon- and body-tier reads, and
forced one-byte RAD chunks. Baseline one-worker output matched final one- and eight-worker output
recursively. Its artifact SHA-256 values were:

| Artifact | SHA-256 |
|---|---|
| `evidence.tsv` | `9ae2527c201f635c01bdb015dee1477bd48090a1c61f90b2c1f237a039d38d14` |
| `map.rad` | `cc1b3fa2ececf7de5c1a67defdd4c715f83531a39572652ea48dc7dfd83dc19b` |
| `reads.bam` | `9d2522245027fc67446269072ca6fbcb25a65038e5c7d92d52851e30cd0a6469` |
| `summary.tsv` | `f1ae566e076cbca7488a99688f6103d39f31a27395dbd4e935363dc25bfb065d` |
| `tx2gene.tsv` | `cce4bff1b78b8544d2a063b7fda1c2ebe9e8410b08e5e653b39cd2c9298068c7` |

The same baseline/final recursive comparison passed for the cyclic-body degradation fixture and
the repeated-but-resolvable occurrence fixture. This transition receipt complements the permanent
current-code tests; it is not embedded as a CTest because release builds do not retain an old
binary.

## Parallel initialization amendment

A second generated fixture isolates startup work: 512 lexical Parents and canonical targets, each
with 32 exon paths and 32 linked body paths. It produces 524,288 exact exon/body models. One GAMP
record keeps the program's non-empty-input contract while making processing a small, separately
reported tail. The source command is:

```sh
python3 tests/vg/make_parallel_exact_init_fixture.py INPUT \
  --parents 512 --paths-per-parent 32 --reads 1
```

Scratch root: `/mnt/ssd/lalli/pancollapse-v090-init-bench`

| Input | Bytes | SHA-256 |
|---|---:|---|
| `graph.gfa` | 1,224,218 | `d4975c65bad53b7b7d8cb8f1880798240487345f7dbe7ea4a4155b13ffe89c2f` |
| `graph.xg` | 6,463,308 | `7ef6b827c8815fa95c49ecbd627a90d0730429b3460b0f80818c0725331a377c` |
| `path_identity_ledger.tsv` | 7,176,524 | `8567bf1a6ec799bee022cbecd38803888c825d803837c773005221899368446b` |
| `reads.gamp` | 158 | `1e5f6401b2af6a24ba62c02b0ba56b0f35b32659b1dc92ad3d518ba190457a33` |
| `reads.gamp.json` | 289 | `9db8422c72b71ca35a58d8e1e14df855bf3431ea0f8cdb9b7b8de63c2e4ef7ab` |

Each row is the median of three sequential runs. Parent and total initialization times are parsed
from v0.9.0 stderr; v0.8.2 predates those phase diagnostics, so only wall time is available.

| Build | Workers | Wall replicates (s) | Median wall (s) | Median Parent phase (s) | Median total initialization (s) | Median peak RSS (MiB) |
|---|---:|---|---:|---:|---:|---:|
| v0.8.2 | 1 | 6.54, 6.51, 6.38 | 6.51 | - | - | 463.0 |
| initial v0.9.0 | 1 | 5.23, 5.03, 5.05 | 5.05 | 4.048 | 4.848 | 175.8 |
| initial v0.9.0 | 8 | 1.68, 1.64, 1.71 | 1.68 | 0.552 | 1.324 | 193.0 |
| initial v0.9.0 | 16 | 1.49, 1.40, 1.40 | 1.40 | 0.299 | 1.089 | 192.0 |
| initial v0.9.0 | 32 | 1.34, 1.26, 1.21 | 1.26 | 0.156 | 0.931 | 163.1 |
| initial v0.9.0 | 64 | 1.33, 1.29, 1.34 | 1.33 | 0.116 | 0.937 | 170.3 |
| initial v0.9.0 | 128 | 1.38, 1.35, 1.29 | 1.35 | 0.160 | 0.982 | 174.3 |
| final v0.9.0 | 1 | 1.17, 1.12, 1.04 | 1.12 | 0.291 | 1.016 | 131.1 |
| final v0.9.0 | 8 | 1.02, 1.12, 1.36 | 1.12 | 0.165 | 0.978 | 159.6 |
| final v0.9.0 | 16 | 1.11, 1.35, 1.19 | 1.19 | 0.221 | 1.036 | 155.5 |
| final v0.9.0 | 32 | 1.42, 0.96, 1.21 | 1.21 | 0.331 | 1.068 | 141.4 |
| final v0.9.0 | 64 | 1.03, 1.05, 1.06 | 1.05 | 0.120 | 0.881 | 133.8 |
| final v0.9.0 | 128 | 1.16, 1.18, 1.15 | 1.16 | 0.218 | 0.989 | 135.1 |

The initial Parent phase scaled 34.8x from one to 64 workers. The final candidate then removed most
of that phase's work: it reuses each exon/body path geometry across the 32x32 model cross product,
shares 16,384 exon aliases as 512 exact edge geometries, and avoids duplicate production metadata
maps. One-worker wall time fell from 5.05 s to 1.12 s and median RSS from 175.8 to 131.1 MiB. This
one-record fixture is now dominated by ledger/XG loading, so additional workers change total time
little and the noisy minimum was 1.05 s at 64. The read-heavy fixture above, not this startup-only
fixture, is the appropriate bound for processing-worker scaling.

The final output remains byte-identical to v0.8.2. The RAD, summary, and tx2gene SHA-256 values are
`82abce877aabb0145f52093ef36a151975a3424533cd5e84fb05842d2c80b659`,
`60d58c0e8cb5e51591e4bf922189ae340814fe0a15c16d9e68eb196d637e6197`, and
`543e4d93ccaaf25a1c3c7516c86886561d9af08b179bfdaa73452c9851c436ad`, respectively.

The committed 64-Parent/64-target test exercises production Parent and target initialization,
concurrent node/model-cache misses, exact-edge geometry sharing, and legacy multi-gene geometry at
one and eight workers. It requires every persistent artifact to match, verifies that three distinct
exon and body evidence identities survive shared geometry, and adds two invalid Parents—an
intentionally slow lexical-first one and a faster later one—to prove deterministic error replay and
no partial output directory. Helgrind completed that eight-worker production fixture with zero
error contexts. The local ThreadSanitizer binary still cannot start on this host (`unexpected
memory mapping`), so TSan is not counted as a passing check.

## Lightweight boundary

A second fixture repeated the two-model exact smoke input to 900,000 groups. Its groups are so
cheap that queue synchronization and ordered emission would dominate, so v0.9.0 automatically
uses one active worker when the exact surface contains fewer than 32 models while reporting both
the requested and active counts. The final candidate took 9.40 s when one worker was requested and
9.34 s when eight were requested (one active). Larger exact surfaces use the requested count;
production selection should still be checked on a bounded representative slice.

## Interpretation boundary

These results establish algorithmic gains and effective parallel execution on the targeted startup
and read-processing paths. They do not establish chr20-22 or whole-pangenome wall time, startup
memory, or the best production worker count. Those depend on graph path multiplicity, read
complexity, cache growth, BAM volume, and storage throughput. The already-running v0.8.2 chr20-22
producer was not stopped or altered; a controlled production-scale comparison remains a separate,
explicitly authorized run.
