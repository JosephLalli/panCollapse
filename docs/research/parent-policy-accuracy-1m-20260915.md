# Corrected comparison bundle and current-binary 1M validation

## Outcome and scope

The corrected bundle, both native 1M count arms, full decoded comparison, and both scoring
runs completed with exit 0. **CR7 parity passes; panSC-strict differs.** The user subsequently
removed strict parity as an acceptance requirement on 2026-09-15; continuing work targets
annotation accuracy. The sealed root status remains `COMPLETE_WITH_STRICT_DISCREPANCY`,
preserving the original comparison result and acceptance checkpoint.

The run-local `RESULTS.md` contains the full precision/recall/F1 tables and limitations;
`VALIDATION.json` summarizes terminal checks, and `COMPLETION.SHA256SUMS` seals the run's
final reports and receipts. These results evaluate read assignment on the
recorded HG002 simulation. Matrix checks validate internal consistency and route agreement;
they do not independently establish cell-by-gene matrix truth accuracy. Converter performance
and compatibility replay performance are separate workstreams.

## Execution provenance

Explicitly authorized by the user on 2026-09-15 after the bounded Parent identity generator
repair passed. At that time `/reload` was offered before this substantive scientific phase;
the user had already authorized execution, so it was not made a second permission gate. That
historical workflow is superseded for future phases by the automatic documentation-
reconciliation checkpoint recorded below.

Fresh result root:
`/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z`.
Its `README.md`, `RESULTS.md`, `commands/`, per-stage status files, logs, and exit receipts are the run
authority. Existing count bundles, frozen generators, and benchmark outputs are preserved.

- All eleven `INPUTS.SHA256SUMS` checks passed, including the 29 GB XG and both annotation
  authorities. The exact paths are in that file and the earlier resumption record.
- Native executable is a copied `251bfa0c948735fdf0ca33c346ac2ca63684679d6ccf13241d90792d0eaab763`
  binary from this worktree, with both `cr7-v1` and `pansc-strict-v1`, 32 workers, a 64 GiB
  count budget, and per-read diagnostics. No rebuild occurred.
- The first generator snapshot was `604faf4aee96f90180b0bb5ec9af91072df2173ea63b3bb0fc3c9013a8ee0f64`.
  The successful producer is `d18bd9be30689174f7d4acc2f9ec295e33714ae0aba0b50bc81b7e46d8fdfebe`,
  with its complete compiler source closure in `attempt2/provenance/compiler/`.
- The corrected Parent categories reproduce the retained `count-facts-idfix` data exactly;
  the successful bundle carries a freshly derived producer receipt. CR7 policy, projected
  strict policy, and policy-dependent support were regenerated. Failed attempts are retained.
- At completion, this worktree remains on `codex/compat-replay-integration-b8210`, HEAD
  `258f22e889b94129024ebe8cdf0c295dfd0d12a2`. Main remains
  `d4a60aed001bc03b00baf7ed54e39176d2266b55`: main has one exclusive commit and this branch
  has 23. The dirty source and binary are identified separately; no merge, commit, C++ edit,
  or rebuild occurred during this production comparison.

## Production contradiction and second repair

The first policy derivation (generator `604faf4a...`) completed but retained all 40,243
gene class/reason pairs, including 399 `all_readthrough_transcripts` genes. Its excluded
identity count changed from 1,658,582 to 1,665,663, confirming the Parent-ID repair ran.
This exposed a second defect: CR7 policy inference used every configured Parent exclusion
tag as readthrough evidence. D088 and the current native implementation require exact
`readthrough_gene` declarations or the conjunction of exact `readthrough_transcript`
Parent observations. Other category exclusions remain independent Parent facts.

The isolated generator now applies these exact predicates to CR7 while preserving legacy
non-CR7 configurable demotion. SHA-256:
`d18bd9be30689174f7d4acc2f9ec295e33714ae0aba0b50bc81b7e46d8fdfebe`.
Six new regression subcases fail before this second repair; all eleven focused tests pass
after it, along with both counting integration suites. The first generation, commands,
logs, and source snapshot remain preserved.
Its support-audit stage separately failed with exit 2 because method-search scope refused
a matrix-capable invocation. Attempt2 uses sensitivity-analysis for that zero-read audit.
Attempt2 owns new policy/source/log directories and published the fresh `count-facts/`.

The current native annotation arm completed with exit 0 at 17:49:32 UTC: 1,000,000 read
groups, 2,000,000 diagnostic rows, 699,260 CR7 molecules and 709,623 strict molecules.
Decoded comparison and truth scoring subsequently completed, as recorded below.
Its manifest and terminal receipt are retained under `native-annotation/` and
`logs/annotation.exit.tsv`. Descriptive wall time was 27:38.51 on the shared host.

The reused category receipt binds builder `95c40e99...`, while this repair base carries
older builder `95814ce4...`. Exact producer recovery failed after bounded file, Git, and
session-log checks. Compilation correctly refused that mismatch before publishing output.
The available corrected builder `2177f983...` has the same own-ID functional insertion and
an expanded comment. New derivation in `attempt2/revalidated_categories/` reproduced the
retained category ledger byte-for-byte (`cmp` plus pinned SHA-256), covering all 2,461,354
Parents. The separate compiler closure preserved the corrected policy producer and passed
with exit 0 using that genuine new receipt. The original snapshots, driver, category data,
and receipts are retained. Corrected bundle content ID is
`sha256:f1ffba4d4fe821e6bda6d0b88bba3cfe5df9df5fb263efe099e27fe14146c99e`.
Its matching native count started at 18:30 UTC with the pinned binary and unchanged settings
and completed with exit 0. Both native published manifests, every output byte hash and row
count, and both scorer checksum manifests passed validation.

Attempt2 policy and support generation completed: 586 policy records changed, including
273 FALLBACK-to-PRIMARY transitions; all are referenced through the compiler's normalized
Parent count identities. Full policy: PRIMARY 5,417, FALLBACK 3,345, UNKNOWN 31,481.
Readthrough reasons are 46 explicit `readthrough_gene` and 15 `all_readthrough_transcripts`.
The 1,665,663 excluded identities did not change. The compiler-normalized ledger cohort
contains 21,351 gene keys (one missing from every compared policy). This matches the native
Parent count-identity core; the native factory additionally inserts 2,066 missing novel-paralog
origin lookup targets, yielding its 23,417-entry catalog. These are different sets, not
conflicting counts of the same universe. Although the bundle lacks optional gene-name
metadata, the actual published feature tables and barcode tables are identical between
routes; metadata does not explain the strict divergence. Exact audit:
`attempt2/policy-comparison/policy-comparison.json` and `policy_changes.tsv`.

## Scoring contract

The historical v1.3.1 read-classification scorer uses `competition_class == PRIMARY` to
define eligible truth, and rejects an assigned gene outside that eligibility set. The old
defective gene policy cannot silently remain the scoring authority for corrected predictions.
The same complete 1M FASTQ and original truth TSV remain fixed. The frozen v1.3.1 scorer's
existing policy-free mode supplies the common all-mapped-origin comparison for historical
and current arms. A separate corrected-policy open-set score is permitted only if the
unchanged evaluator accepts all current predictions under that policy. No reads or genes
are dropped to make that guard pass. Different eligibility denominators are reported apart.

Converter performance, compatibility replay performance, and biological counting accuracy
remain separate. Timing under overlapping shared-host work is descriptive only.

## Terminal comparison and accuracy

CR7 is identical across all 1M diagnostic records, all 231,159 nonzero matrix coordinates,
and all 699,260 corrected molecules. Strict differs on 322 diagnostic assignments: 190
multigene-to-unique changes, 122 unique-gene switches, nine still-multigene candidate-set
changes, and one selected-gene change on a barcode-rejected read. Strict has 209 differing
matrix coordinates and an absolute UMI difference of 319. Annotation has 709,623 molecules
versus bundle 709,532: 205 annotation-only, 114 bundle-only, and 14 shared molecules with
different support. Each matrix agrees exactly with its own corrected-molecule rows.

Fixed-denominator scoring covers all 1M reads and 1,024 truth genes in each arm, with zero
missing classifications. The historical scores reproduce exactly.

| Authority / executable | CR7 read F1 | Strict read F1 | CR7 gene-balanced F1 | Strict gene-balanced F1 |
| --- | ---: | ---: | ---: | ---: |
| Historical frozen run | 0.957309 | 0.963628 | 0.926405 | 0.932214 |
| Corrected bundle / current binary | 0.962336 | 0.969883 | 0.933264 | 0.943569 |
| Annotation / current binary | 0.962336 | 0.969861 | 0.933264 | 0.942025 |

Gene-balanced metrics weight each represented truth gene equally. Historical/current gains
include both executable and authority changes, so this is not a generator-only causal test.
CR7 read precision decreases slightly while recall and F1 increase. Relative to bundle
strict, annotation strict adds 70 true and 120 false assignments, with slightly lower read
F1 and lower gene-balanced F1. The complete P/R/F1 and chromosome rows are retained in the
run's `score-full-truth/metrics.tsv` and summarized in `RESULTS.md`.

The corrected-policy open-set guard passes unchanged, with 999,085 eligible reads / 1,002
genes and 915 background reads / 22 genes; no read is omitted. Both profiles have the same
eligibility map and zero missing classifications. CR7 falsely assigns 819 background reads
(89.5082%); strict falsely assigns 820 (89.6175%) under either authority. Background rejection
is therefore weak despite improved aggregate F1. These denominators differ from historical
open-set evaluation and must not be silently combined. See `eligibility-preflight.json` and
`score-corrected-open-set/metrics.tsv`.

## Native-count resource record

Both 1M native-count invocations completed with exit 0, each executing CR7 and strict profiles
with 32 requested threads and a 64 GiB count budget. On the shared host, annotation took
27:38.51 with maximum RSS 54,646,512 KiB (52.1149749756 GiB); stderr reports 577.057 s
initialization and 1,044.36 s processing. Bundle took 25:49.16 with maximum RSS 54,569,692 KiB
(52.0417137146 GiB); stderr reports 551.372 s initialization and 970.332 s processing. These
wall times cover startup, counting, and output only; they exclude bundle construction and
post-hoc scoring. They are individual shared-host observations, not a controlled speedup claim.
Converter and replay performance remain separate.
Exact GNU-time receipts are `logs/annotation.time` and `logs/bundle.time` under the result
root above; phase breakdowns are in the corresponding `.stderr` files. RSS values are in
KiB and converted to GiB by dividing by 1,048,576.

## Strict discrepancy and acceptance boundary

A bounded source/policy audit accounts for every one of the 322 assignment changes:

| Missing annotation-route fact | Changed records | Current projected-ledger evidence |
| --- | ---: | --- |
| Projected clearing of nested-host edges | 199 | `attempt2/projected_policy/projected_nested_relations.tsv`, lines 923 and 925 |
| Projected reversal of a nested-host edge | 86 | Same ledger, line 335; includes one barcode-rejected record |
| Per-Parent host protection | 37 | Same ledger, line 8, plus bundle strong-support protection |

The projected producer implements relation clearing/reversal in
`attempt2/provenance/compiler/scripts/derive_projected_nested_host_policy.py`. Annotation
loading in `src/direct_count_facts.cpp` reads only gene-row `nested_host_gene` and sets Parent
protection false; the bundle loads both projected gene policy and the protection ledger.
Strict filtering and directional host removal in `src/direct_count_assignment.cpp` consume
those facts. This is an authority mismatch already anticipated in the input contract.
`comparison/assignment_transitions.tsv` retains the complete transitions. The unchanged
missing policy entry `ENSG00000298824` never appears in these transitions.

The Parent/readthrough repair is validated for this fixture. **2026-09-15 user decision
(supersedes D088's original strict-parity acceptance clause):** strict bit-exact parity is no
longer an acceptance requirement, especially where F1 improves. The 322 assignment changes and
209 differing matrix coordinates remain diagnostic evidence. The projected policy manifest still
declares `scientific_role: method_search_only` and `benchmark_eligible: false`; this decision
does not promote it into biological authority, select a biological rule, authorize reruns, or
alter sealed bundle receipts. Annotation accuracy must be evaluated with truth-based precision
and recall reported as percentages and F1 on the conventional 0--1 scale, using a fixed,
explicit denominator. Extra assignments or relaxed parity do not prove improvement. On the
current fixed all-mapped-origin denominator, annotation strict is slightly lower than corrected
bundle strict: read F1 `0.96986068839009076` versus `0.96988313775162238`, and gene-balanced F1
`0.94202475192394564` versus `0.94356879517102621`; no observed annotation improvement is
claimed. The next task is an annotation-accuracy investigation under this revised gate; no
concrete implementation experiment is yet specified. Before any new scientific phase, automatically run a
documentation-reconciliation agent to update the local AI-facing current state, decision
record, and research record with the intellectual state/goals, generated files and experiments,
terminal results, settled decisions, existing code, exact source/input locations, and the next
gated task. This does not authorize a new scientific policy or run.
