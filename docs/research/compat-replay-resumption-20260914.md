# Compatibility/replay and native-count resumption — 2026-09-14

## Current checkout and gate

- Worktree: `/mnt/ssd/lalli/panCollapse/.codex/worktrees/compat-replay-integration-b8210`.
- Branch: `codex/compat-replay-integration-b8210`.
- HEAD: `258f22e889b94129024ebe8cdf0c295dfd0d12a2` (stream assignment signature into digest).
- Main checkout: `/mnt/ssd/lalli/panCollapse`, branch `main`,
  `d4a60aed001bc03b00baf7ed54e39176d2266b55`.
- Common ancestor: `5f4daffff17e503c63bb0d58dbd0e779573621a0`.
  `git rev-list --left-right --count main...HEAD` reports `1 23`. Main's separate commit
  adds annotation identifier rules; that file has identical contents on this branch.
- Existing uncommitted work includes D088 annotation ingestion/provenance, readthrough and
  body-to-gene fixes, reusable state buffers, and the call-position strand vector. It is
  neither main nor a release identified by main's changelog. All pre-existing edits remain.
- Gate: D088 accepted; post-Vertical-Slice machine-verifiable increments may continue.
  Production scientific validation remains open. No project-specific latest handoff exists at
  `/mnt/ssd/lalli/.codex/session-handoffs/b13386838e2b5b34/LATEST.md`.

## Executable identities

| Role | Path | SHA-256 (verified during recovery) |
|---|---|---|
| Current fixed worktree | `build/src/panCollapse` in this worktree | `251bfa0c948735fdf0ca33c346ac2ca63684679d6ccf13241d90792d0eaab763` |
| September 13 A/B and identifier-fixed counterfactual | `/mnt/ssd/lalli/annotation_route_ab_20260913T173120Z/bin/panCollapse` | `490643ca4505977a9759fd4baae6eeb98d450d9fd2bd197a90d659d6793db6aa` |
| September 5 integration replay baseline | `build-vg-compatibility/src/panCollapse` in this worktree | `6a0ef3956b3782195f9219ed06c54bb108cfcde0506a30f8142155b19c613351` |

The current executable reports `panCollapse 0.10.0`, is 51,490,528 bytes, and was built September
13 at 17:52 CDT. The existing CMake cache uses GCC 15, `RelWithDebInfo`, graph-backed tests,
`/mnt/ssd/lalli/usr/local/src/vg`, and Arrow/Parquet 17 from the existing pyarrow prefix.
No production binary was rebuilt in this session. The earlier A/B binary predates the current
readthrough/body-parent fixes; changing source does not change a pinned historical executable.

## Latest unfinished validation and retained terminal results

The September 13 D088 annotation-route comparison finished. Older PROGRESS text that calls it
running is stale. Its outputs differ and its reference bundle has the gene-policy identifier
defect documented in D088. The newer `count-facts-idfix` bundle fixes Parent categories only;
its name does not mean the gene-policy defect is repaired.

All three native-count arms requested 32 threads and a 64 GiB count-memory budget, with both
`--cr7` and `--pansc-strict-v1` on the same one-million-read-group GAMP and XG. Their retained
statistics are output comparisons, not truth-based accuracy or controlled speed measurements.

| Native count arm | CR7 count rows | CR7 molecule rows | Wall seconds | Peak RSS KiB | Terminal evidence |
|---|---:|---:|---:|---:|---|
| Old bundle | 228,719 | 692,194 | 967.10 | 54,679,104 | A/B `done`: `bundle_exit=0` |
| Annotation, old executable | 177,392 | 577,973 | 872.54 | 54,574,536 | A/B `done`: `annot_exit=0` |
| Identifier-fixed bundle | 231,084 | 699,222 | 840.38 | 54,632,060 | `idfix.err`: time exit status 0 and published manifest/tables |

Preserved output roots:

- `/mnt/ssd/lalli/annotation_route_ab_20260913T173120Z` (`bundle/`, `annot/`, `*.time`, `done`).
- `/mnt/ssd/lalli/annotation_route_idfix_counterfactual_20260913T183000Z`
  (`idfix/`, `idfix.err`, `binary.sha256`, `run.sh`, recorded predictions/findings).

The counterfactual has no separate terminal marker or final checksum receipt. No recovered
production output proves a run of current binary `251bfa0c...`. D088 also records invalid
pre-fix chr19 counts; those outputs were neither overwritten nor rerun here.

## Exact benchmark inputs

These hashes come from the completed run manifests; multi-GB inputs were not rehashed during
startup. The executable hashes above were rehashed. Full original argv and complete manifests
are copied into the local evidence directory below.

| Input | Exact path | Manifest-recorded SHA-256 |
|---|---|---|
| GAMP | `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_pancollapse_v010_realign_1m_v1_20260905T013227Z/gamp/reads.gamp` | `98b4a062de3cf5ca904c29c159eb72737e9b3bfff426029df45bdafc67f32bb7` |
| XG | `/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500/chr20_chr21_chr22.final.genic.xg` | `1568ff5d682c5d11cdffaec34def02d3322d35d9e001b5fdd6bbbb35fc9849d6` |
| Whitelist | `/mnt/ssd/lalli/3M-february-2018.txt` | `843a6f7038db8cb3c06f3dc21cc69d04139ffa10689780518d7a9e42dc2e819b` |
| Corrected annotation | `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_pancollapse_v010_realign_1m_v1_20260905T013227Z/count-facts/files/corrected_annotation.gff3` | `c793860d587096c20147059330fbd10834c2ee40340ed1dcc392cd9ad85fd598` |
| Reference annotation | `/mnt/ssd/lalli/hprc_v2_vg_rna/chr20_compare/readsets/chr20_primary_seed3_frac0.21_original_orientation_v1/runs/crv7plus_v21_d46_catcollapse_v1/prepared/reference_source_gencode_v48_comprehensive_20260722T005804-0500/gencode.v48.annotation.gff3.gz` | `3d977d5e20bbe3ea899d887ab505644b117ac9d9704e60d5133161ba87b31db5` |
| Path ledger | `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_pancollapse_v010_realign_1m_v1_20260905T013227Z/count-facts/files/path_identity_ledger.tsv` | `73c7a7a03abcbb9eb795c3ac2b1141cca82333c73134b190ee1a30be8657f10c` |
| Old bundle manifest | `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_pancollapse_v010_realign_1m_v1_20260905T013227Z/count-facts/MANIFEST.json` | `17105ad7405debdc694d07d4dc5824a36b0e912e09ec6d423a6bbcdacc2c61e8` |
| Identifier-fixed bundle manifest | `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_parent_ledger_identifier_fix_v1_20260906T000000Z/count-facts-idfix/MANIFEST.json` | `513915bc7352569ee766481e773481ae383e0a27c75a73f581bced4d8c6279b3` |
| Compatibility manifest | `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_compatibility_bundle_1m_v2_20260905T093217Z/compatibility/manifest.json` | `018ea0d996cbf959c2a16f0ef0bc1ec0169568a84052e2f2ea4619016d56ef29` |

## Keep the performance and accuracy records separate

1. **Converter performance.** The v0.9 deterministic converter evidence is recorded in
   `v090-deterministic-parallelism.md`. Its generated fixtures and matched output bytes do not
   establish native-count biological accuracy or the current annotation route's runtime.
2. **Compatibility replay performance.** The accepted September 5 integration baseline is at
   `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_compatibility_replay_b8210_integration_baseline_v1_20260905T160230Z`.
   `BASELINE.STATUS`, `RESULTS.tsv`, and `provenance/PARITY_RECEIPT.tsv` record the accepted
   serial replay and parity. Later D080-D082 records document attributed and interleaved replay
   optimizations: mean 781.96 s versus 220.62 s, 3.544x on means, with the shared-host limits
   stated there. Those later results are recovered documentation, not new measurements here.
3. **Native GAMP/count performance.** D086/D087 and HEAD's signature optimization concern the
   GAMP-side workload. The recorded 1.46x worker-CPU improvement from streaming the signature
   still lacks an interleaved A/B. It is not a newly established converter or replay speedup.
4. **Biological counting accuracy.** D088's A/B is an implementation/reference comparison,
   not a truth score. A changed UMI total cannot establish improvement. No F1 or current
   production parity result was produced in this session.

## Bounded continuation completed

Added `tests/vg/direct_count_annotation_verify.py` and registered
`direct_count_annotation_cli` in `tests/vg/CMakeLists.txt`. The existing annotation test covers
the factory and writer directly; the new test exercises actual CLI routing and publication.

- Five count invocations on the nine-group Ex50 graph: plain inputs, one harmless byte change
  in each authority input, and gzipped corrected/reference annotations.
- Both profiles must produce the independently specified eight expected UMIs. Barcode/count/
  molecule Parquet bytes match the existing tiny bundle fixture. Optional gene display names
  are bundle-only on that fixture, so feature-table byte equality is not asserted.
- Each authority input must appear with its SHA-256 and affect the derived content ID.
- Six invalid authority combinations must fail with the appropriate message before initialization
  or output creation, including annotation-plus-replay.

Validation uses the existing executable and Python/Arrow runtime inside local `ubuntu:24.04`,
with no network or dependency installation, a 2-CPU/4-GiB container limit, and one requested
count worker with a 1 MiB count-memory budget. CMake reconfiguration succeeded. Acceptance:

```sh
ctest --test-dir build \
  -R '^(direct_count_annotation_cli|count_fact_annotation_smoke)$' --output-on-failure
```

**8/8 passed**, including fixture dependencies. The earlier complete log records 166 passes,
including the formerly failing cross-isoform test; its older `LastTestsFailed.log` is stale.
The new test raises registration to 167; a complete 167-test run was not performed because no
production C++ changed. The production executable hash is unchanged.

Local evidence directory:
`/mnt/ssd/lalli/panCollapse/.codex/worktrees/compat-replay-integration-b8210/results/resume-validation.xQS3dp`.
It contains prior test logs, the focused result, binary SHA-256, a working-tree patch/status,
and `recovered-benchmarks.json` with four copied original manifests and their hashes/argv.
Original benchmark inputs and completed output directories were not modified.

## Next boundary

The production annotation route still needs current-executable validation. D088's original
bit-exactness gate conflicts with its later documented reference-policy defect; this increment
does not invent a replacement biological acceptance rule. Resolve that comparison authority
before claiming parity or accuracy. Offer `/reload` before a new substantive scientific phase,
and retain the exact 1M inputs and old outputs above when that phase is authorized.
The unperformed signature interleaved A/B remains a separate performance task.
