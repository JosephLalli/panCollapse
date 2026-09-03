# PanCollapse v0.9.0 deterministic-parallelism benchmark

Date: 2026-09-03

Status: bounded engineering evidence, not a whole-pangenome runtime projection.

## Question

Does v0.9.0 reduce the exact GeneFull hot path before threading, and does complete-read-group
parallelism scale without changing persisted evidence?

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

## Lightweight boundary

A second fixture repeated the small exact smoke input to 900,000 groups. Its groups are so cheap
that queue synchronization and ordered emission dominate: v0.9.0 one worker was approximately
12 s, while 2-8 workers were approximately 14-15 s. This is why the default remains one worker.
`--threads` is intended for complex graphs/read groups, and should be benchmarked on a bounded
slice before choosing a production count.

## Interpretation boundary

These results establish an algorithmic gain and effective parallel execution on the targeted hot
path. They do not establish chr20-22 or whole-pangenome wall time, startup memory, or the best
production worker count. Those depend on graph path multiplicity, read complexity, cache growth,
BAM volume, and storage throughput. The already-running v0.8.2 chr20-22 producer was not stopped or
altered; a controlled production-scale comparison remains a separate, explicitly authorized run.
