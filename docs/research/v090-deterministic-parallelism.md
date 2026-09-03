# PanCollapse v0.9.0 deterministic-parallelism benchmark

Date: 2026-09-03

Status: bounded engineering evidence, not a whole-pangenome runtime projection.

## Question

Does v0.9.0 reduce the exact GeneFull hot path before threading, and do initialization plus
complete-read-group parallelism scale without changing persisted evidence?

## Compared builds

- v0.8.2: commit `f5b5cae74bd69ca44e39dbdba80d7f86c90cdf33`, existing optimized build.
- v0.9.0 candidate: branch `worktree-codex-v090-performance`, optimized
  `RelWithDebInfo` build.

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
| v0.9.0 | 1 | 19.89, 19.67, 19.88 | 19.88 | 1.25x | 1.00x |
| v0.9.0 | 8 | 2.84, 2.68, 2.62 | 2.68 | 9.28x | 7.42x |
| v0.9.0 | 16 | 1.50, 1.83, 1.95 | 1.83 | 13.58x | 10.86x |

Single observations at two and four workers were 10.03 s and 5.07 s, respectively, consistent
with the primary scaling series. Median peak RSS stayed between 16 and 20 MiB for the tabled
points; this small graph does not estimate whole-pangenome resident memory.

The v0.8.2 and v0.9.0 benchmark RAD and summary artifacts were identical:

- `map.rad`: `aa55756ed6fc646901671a51243f1383c2fcecb82a1330ceddaf7a5794bacea6`
- `summary.tsv`: `ffc97d611650d0bb19536ac020dc3b3a3a7bba1689738ec74d8420d6d40306b8`

The committed fixture tests separately hold invocation paths constant and require byte identity
for RAD, BAM, summary, tx2gene, and debug evidence at 1, 2, 4, and 8 workers. Score mode has an
independent default/1/8-worker byte-identity test for its separate node cache.

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
| v0.9.0 | 1 | 5.23, 5.03, 5.05 | 5.05 | 4.048 | 4.848 | 175.8 |
| v0.9.0 | 8 | 1.68, 1.64, 1.71 | 1.68 | 0.552 | 1.324 | 193.0 |
| v0.9.0 | 16 | 1.49, 1.40, 1.40 | 1.40 | 0.299 | 1.089 | 192.0 |
| v0.9.0 | 32 | 1.34, 1.26, 1.21 | 1.26 | 0.156 | 0.931 | 163.1 |
| v0.9.0 | 64 | 1.33, 1.29, 1.34 | 1.33 | 0.116 | 0.937 | 170.3 |
| v0.9.0 | 128 | 1.38, 1.35, 1.29 | 1.35 | 0.160 | 0.982 | 174.3 |

The Parent phase scales 34.8x from one to 64 workers. Serial ledger/XG catalog work then dominates:
total initialization is best at 32 workers (5.21x over v0.9.0 one-worker initialization), and
128 workers do not improve this fixture. The 32-worker candidate's wall time is 5.17x faster than
v0.8.2. Its RAD, summary, and tx2gene files are byte-identical to v0.8.2, with SHA-256 values
`82abce877aabb0145f52093ef36a151975a3424533cd5e84fb05842d2c80b659`,
`60d58c0e8cb5e51591e4bf922189ae340814fe0a15c16d9e68eb196d637e6197`, and
`543e4d93ccaaf25a1c3c7516c86886561d9af08b179bfdaa73452c9851c436ad`, respectively.

The committed 64-Parent/64-target test exercises production Parent and target initialization,
sharded lazy misses, and legacy multi-gene geometry at one and eight workers. It requires every
persistent artifact to match and adds two invalid Parents—an intentionally slow lexical-first one
and a faster later one—to prove deterministic error replay and no partial output directory.
Helgrind completed that eight-worker production fixture with zero error contexts. The local
ThreadSanitizer binary still cannot start on this host (`unexpected memory mapping`), so TSan is not
counted as a passing check.

## Lightweight boundary

A second fixture repeated the small exact smoke input to 900,000 groups. Its groups are so cheap
that queue synchronization and ordered emission dominate: v0.9.0 one worker was approximately
12 s, while 2-8 workers were approximately 14-15 s. This is why the default remains one worker.
`--threads` is intended for complex graphs/read groups, and should be benchmarked on a bounded
slice before choosing a production count.

## Interpretation boundary

These results establish algorithmic gains and effective parallel execution on the targeted startup
and read-processing paths. They do not establish chr20-22 or whole-pangenome wall time, startup
memory, or the best production worker count. Those depend on graph path multiplicity, read
complexity, cache growth, BAM volume, and storage throughput. The already-running v0.8.2 chr20-22
producer was not stopped or altered; a controlled production-scale comparison remains a separate,
explicitly authorized run.
