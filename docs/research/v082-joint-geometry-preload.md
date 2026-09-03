# v0.8.2 joint chr20-22 geometry preload check

Verified 2026-09-03 on branch `worktree-codex-v081-geometry-precompute`. This is a bounded
performance/correctness gate for initialization only. It is not a read-assignment result and none
of its partial RAD or BAM output is benchmark evidence.

## Inputs

| Role | Path | SHA-256 |
|---|---|---|
| XG | `/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500/chr20_chr21_chr22.final.genic.xg` | `1568ff5d682c5d11cdffaec34def02d3322d35d9e001b5fdd6bbbb35fc9849d6` |
| Path identity ledger | `/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500/chr20_chr21_chr22.final.path_identity_ledger.tsv` | `73c7a7a03abcbb9eb795c3ac2b1141cca82333c73134b190ee1a30be8657f10c` |
| GAMP | `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_three_reference_benchmark_v1_20260902T015452Z/pangenome/run.partial/reads.gamp` | `0a41b85bf505dcdaf0255c82faf77511d083b837cb9637b1b7c5a359fa0a451f` |

The ledger has 2,461,354 rows: 1,231,247 body paths and 1,230,107 exon paths. The independent
scan labels 13 body paths directed-cyclic. That scan is an upper-bound candidate detector;
v0.8.1/v0.8.2 exact occurrence resolution degrades only the 12 without a unique oriented exon
occurrence and retains the one resolvable repeated path.

## Method

The optimized `RelWithDebInfo` binary was run with the production command surface. At the time of
the full-graph check it contained the corrected runtime logic but still reported the pre-release
version string `0.8.1`:

```text
panCollapse convert --gamp <GAMP> --xg <XG> --path-identity-ledger <LEDGER> \
  --count-mode genefull_ex50pas --raw-cb-length 16 --raw-umi-length 12 \
  --strand both --bam-multigene all --score flat --molecule-identity-failures fail \
  --out-dir <temporary>/rad --bam-out <temporary>/genetag.bam
```

A watcher sent `SIGTERM` to this separate test process as soon as stderr emitted the geometry
summary. It had a two-hour hard cap and did not signal, renice, or otherwise touch the live v0.8.1
producer. The test used one CPU and approximately 60 GB RSS.

## Result

The geometry boundary appeared after approximately 73 minutes:

```text
panCollapse: transcript-body splice geometry: evaluated_target_edges=141543588 owned_target_edges=5937500 fragment_only_target_edges=207 adjacent_vetoed_target_edges=135606081 body_paths_degraded=12 body_path_degrade_reason=repeated_exonic_node
```

The live unpatched v0.8.1 producer had remained CPU-active in the same pre-GAMP region for more
than 25 hours. The v0.8.2 source also passes 119/119 CTests, including degraded-Parent/clean-sibling
byte parity, both gene-change fail-closed cases, and resolvable repeated-body orientation.

The only subsequent source changes were the patch-version string and documentation. The rebuilt
binary reports `panCollapse 0.8.2`, is 22,553,064 bytes, has SHA-256
`f1bb013cb6ac3671f43d66a550b927327d73cdaa20a182c0bc153bad58acff85`, and passes the same 119/119
CTest suite. The full graph was not rerun merely to change the embedded version string.

## Interpretation and restart gate

The improvement removes only the repeated all-exon-map membership scan. It does not change graph
paths, annotation structures, Parent identities, geometry resolution, evidence tiers, score
windows, or output policy. A replacement producer must record both independent quantities:
`directed_cyclic_body_paths=13` and `body_paths_degraded=12`. It must preserve the old partial
attempt and use a distinct checksum-pinned v0.8.2 image. Restarting the shared producer remains a
separate, explicitly authorized action.
