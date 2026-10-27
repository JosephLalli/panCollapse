# Alternate graph inputs: intake and measurement prerequisites

Date: 2026-09-26. Status: historical intake snapshot, superseded by the
[completed measured design memo](gbz-gbwt-count-inputs-design-20260926.md).
Statements below about unmeasured inputs or pending tasks describe the intake stage.
This source/input record is not architecture approval.
No production code or graph-input route has changed.

**Current note after replacement inputs:** exact chr21 corrected/projected annotations do not
exist, so chr21 count-bundle construction stops; raw projection is not substituted. The prior
2,804,175-row result is an audit exact-ID intersection, not biological transcript coverage: it
contains 1,006,635 transcripts and 1,797,540 `panSCbody1` body-segment paths, linked through
`exact/annotation/body_parent_aliases.tsv`. The joint chr20–22 XG/PG/GBWT plus production
GAMP/bundle/whitelist is now the equivalence fixture. Use stock `quay.io/vgteam/vg:v1.74.1` for
all VG CLI steps; validate all ledger names/node IDs/lengths against XG and record commands/hashes.
Chr21 remains separate graph-scale microbenchmark evidence. The accepted output-byte scope is in
[the scientific comparison specification](graph-input-scientific-comparison-20260926.md).

## Checkout and scope

The newest local branch by tip commit date was `codex/compat-replay-integration-b8210`,
at `f51130675e23434b24919f558f305dad60340dec` (2026-09-26 01:39:42 -05:00).
Its worktree was clean. Following the user's branch choice,
`design/gbz-gbwt-count-inputs-20260926` was created from that commit at
`/mnt/ssd/lalli/panCollapse/.codex/worktrees/gbz-gbwt-count-inputs-20260926`.
The source worktree and main were left unchanged. No start-session handoff was found for
either worktree; tracked state governs this task. The new branch's orchestration document
permits scoped documentation writes, resolving the earlier discrepancy encountered on main.
The documentation checkpoint was completed and reviewed before measurements.

## Accepted clarifications (2026-09-26)

1. **Scientific-byte scope.** Result tables, MEX, RAD, and scientific counters must be identical.
   Timing, memory, host, timestamps, input mode, paths, and hashes are excluded through an explicit
   operational-provenance field list and remain truthful. Input mode must not appear in scientific
   files. Check compression determinism. A repeated XG scientific-byte comparison is first; failure
   stops GBZ/XG+GBWT comparisons. No repeated real count was run at intake.
2. **Chr21 fixture ledger/bundle (superseded).** Exact corrected/projected annotations do not
   exist, so construction stops and raw projection is not substituted. The joint chr20–22 source
   is the authorized equivalence fixture; use stock `quay.io/vgteam/vg:v1.74.1` for all VG CLI
   steps. Validate all ledger names, node IDs, and lengths against XG; `vg gbwt -E` may rebuild a
   pre-strip GBWT if required paths disagree. Extra unselected paths alone do not require a
   rebuild. Record exact commands and input hashes.

## Verified inputs

The supplied `chr21.xg`, `chr21.gbz`, `chr21.ref.gbz`, `chr21.ri`, and
`path_free_route/genic.pathfree.xg` exist under
`/mnt/ssd/lalli/hprc_v2_vg_rna/notes/evidence/chr21_gbz_indexing_fixture_20260920/`.
Presence is verified; structural equivalence has not been freshly measured in this task.

The matching source arm at
`/mnt/ssd/lalli/hprc_v2_vg_rna/notes/evidence/chr21_exact_arm_20260914/exact/`
contains `guide.gbwt`, `biological_info.tsv`, `genic.names`, and
`annotation/annotation_identity_ledger.tsv`. The last has schema
`panSC-annotation-identity-v1`, not runtime `panSC-path-identity-v1`.
The existing builder is
`/mnt/ssd/lalli/hprc_v2_vg_rna/scripts/transcript_graph/make_path_identity_ledger.py`:
it joins `--info` and `--annotation-ledger` with SQLite, writing `--path-ledger` and
`--summary`. Check archival identity/schema compatibility before invocation; do not silently
bypass errors or substitute a three-chromosome ledger.

The prior count command exists at
`/mnt/ssd/lalli/pancollapse_filtered_native_count_v1_20260924T105938Z/commands/run_count.sh`.
It uses a chr20-22 XG and
`/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z/count-facts`,
with both profiles, 32 threads, a 64GiB count budget, CB length 16 and UMI length 12.
This does not establish bundle compatibility with the chr21 graph.

Real R1/R2 FASTQs exist under
`/mnt/ssd/lalli/hprc_v2_vg_rna/chr21_compare/readsets/chr21_primary_seed3_frac0.075_original_orientation_v1_20260925T232549-0500/`.
Its STATUS records 1,031,847 reads and the manifest pins readset preparation and source BAM.
No real GAMP was found under that readset. The graph fixture's GAMPs are five-read transcript
smokes, unsuitable for real-count acceptance. `scripts/readsets/stage_real_scrna_gamp.py`
consumes already-mapped GAMP JSONL from stdin into SQLite; it does not tag FASTQ or map reads.

VG invocations must use existing binaries under
`/mnt/ssd/lalli/.codex/worktrees/e3e42ac4-cc4f-4bc9-850a-591d197313b9/vg-latest/bin/`.
Several exist; pin the chosen executable and libraries. Do not rebuild or replace production vg.

## Source capabilities at f511306

The concrete XG load is `src/main.cpp:3066-3077`; older request line numbers do not identify
a second current load. Production-ledger names and lengths are checked at `:3096-3121`.
Score membership is at `:3367-3397`; ledger membership/projection is at `:4211-4302`.
Both reject missing aligned node IDs.

| Execution path | Membership/orientation | Base positions | Ordered steps | Node sequence |
|---|---|---|---|---|
| convert score | yes | no | no | with qualadj |
| Ordinary Gene/GeneFull conversion without production BAM | yes | no | yes | with qualadj |
| Production non-score conversion with typed-union BAM | yes | yes | yes | with qualadj |
| Exact Ex50 conversion | yes | yes | yes | with qualadj |
| Native GAMP count, either profile | yes | yes | yes | with qualadj |
| Compatibility replay | no graph | no | no | no |

Every production-ledger route validates path lengths, including score. `exact_ex50` at
`src/main.cpp:3182-3185` requires production identity, non-score mode, and direct count or
BAM output. Native count forces exact mode at `:760-777`. Exact construction retains body
occurrences/positions at `:3632-3698`; distinct repeated occurrences cannot be replaced by a
set of paths. `src/pathtally_qualadj.hpp:88` calls `graph.get_sequence`; a .cpp-only audit
misses that conditional requirement. Flat scoring does not require node sequence.

Counting does not query graph edges. Splice edges come from consecutive path steps
(`src/main.cpp:3662-3675`); alignment adjacency comes from GAMP mappings and next/connection
arcs (`:1968-2004`). This supports investigating GBZ despite omitted untraversed graph edges;
node, sequence, walk, and count equivalence still need measurement.
Relevant fixtures: `tests/vg/fixtures/{ex50_tier_smoke,cyclic_body_smoke,repeated_body_resolved,genefull_isoform_smoke,splice_concordance_smoke}`.
No fixtures were run or changed.

## Installed API findings to measure

Under the vg-latest source root,
`deps/gbwtgraph/src/gbwtgraph.cpp:1102` delegates step enumeration to
`for_each_edge_and_path_on_handle` (around `:1448`): both node orientations are examined,
ordinary GBWT locate is called per occurrence, and forward-sequence occurrences are retained.
The result supplies occurrence-level steps, not just distinct paths.

`deps/gbwt/include/gbwt/fast_locate.h` offers locate, decompressDA, and decompressSA.
Uniqueness, orientation, occurrence identity, and offset direction must be checked before
substitution; locate alone is not a base-position interface.

PackedPositionOverlay's `extra_path_names` is additive: it still indexes GENERIC and REFERENCE
paths (`deps/libbdsg/bdsg/include/bdsg/overlays/packed_path_position_overlay.hpp:43-60`).
Passing ledger names there does not restrict it to the ledger. The targeted probe must actually
exclude other paths.

The existing VG dossier `docs/exact_dedup_indexing_feasibility/RECEIPTS.md` reports
1,398,636 REFERENCE and 4,209,052 GENERIC paths in ordinary chr21.gbz; chr21.ref.gbz has zero
REFERENCE and the same GENERIC paths, with other paths moved to HAPLOTYPE. It also reports
25,251 fewer GBZ edges and earlier locate microbenchmarks. These are prior receipts, not new
measurements or panCollapse count performance. Preserve their provenance and limits.
Upstream references: [GBWT](https://github.com/jltsiren/gbwt),
[GBWTGraph](https://github.com/jltsiren/gbwtgraph); installed revisions govern this experiment.

## Continuation

Resolve the pending answers, pin annotation/GAMP and binaries, then measure baseline
repeatability, graph identities, exact path names/lengths, locate alternatives, and restricted
positions. Separate load/construction, lookup, and count time; retain GNU time wall/RSS and
label the chr2 projection as a model. No new-backend end-to-end parity can be claimed before
an approved implementation exists. Finish the requested measured memo and stop for architecture
approval before production changes.
