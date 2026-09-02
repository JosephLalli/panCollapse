# Resolved Human Product Decisions

All seven product questions presented on 2026-06-28 were answered **A**. These choices
are binding V1 behavior unless superseded by a later entry in `docs/decisions.md`.

D038 supersedes the barcode/UMI tag-source portions below: V1 reads observed raw CB/UMI
from the GAMP name field, not corrected/raw GAMP annotation tags.

D048 supersedes the "Outside-span compatibility anchor" and "Collapse-manifest coverage"
sections below: V1 compatibility is now HST-path node membership (no GTF projection,
no exon/intron/junction test at runtime), and transcript-copy collapse is implicit in HST
naming (no explicit manifest). See `docs/conversion-algorithm.md`.

D062 extends D048's naming-only collapse without restoring a separate manifest: optional t2g
column 3 maps each raw graph path to a canonical transcript. Two-column rows retain D048's
suffix behavior. Winner selection remains per raw path and precedes collapse; ledger scores
use the maximum across paths mapped to the same transcript.

D063 makes the optional third column apply to ledger bodies as well. With a consistently
three-column body t2g, exon and body evidence share one canonical-transcript key space, raw
body paths MAX-collapse, and panCollapse classifies S/U per transcript. Gene grouping and the
count-mode rule belong to count_cr; transcript-first runs use `--bam-multigene all` so
multi-gene evidence reaches it. Two-column body t2gs retain the legacy classifier.

## Outside-span compatibility anchor

At least one aligned reference-consuming base must overlap an exon or implied intron of
the candidate transcript. Aligned sequence may extend beyond the transcript's
first-to-last-exon span; that overhang does not by itself make the transcript
incompatible.

## Missing or malformed barcode/UMI tags

Skip the affected read group by default, record the reason in diagnostics and summary
counts, and provide a strict CLI mode that converts this condition into a run-aborting
error. Never invent, repair, or mix tag values silently.

## Conflicting tags within one read group

If adjacent records with the same read name disagree on the selected cell barcode or UMI,
fail the run. Such records violate the molecule-identity contract.

## Collapse-manifest coverage

Every source identity that can contribute compatible evidence must have an explicit row
in the collapse manifest. A missing row is a hard error. There is no implicit identity
fallback.

## `starsolo-default` policy

D044 supersedes this policy for active GAMP-to-RAD output. RAD conversion now preserves
all compatible retained targets; `all` is the default and only active RAD assignment
behavior. `starsolo-default`, `unique-gene`, and `unique-transcript` are deferred
to-be-implemented modes outside active RAD conversion.

Historical decision:

`starsolo-default` is an exact alias for post-score-filtering, post-collapse
`unique-gene`: retain a read when all eligible canonical transcripts belong to one gene,
and preserve all eligible transcripts from that gene in the RAD target set.

## Output determinism across threads

For identical inputs and configuration, RAD and companion artifacts must be
byte-identical regardless of thread count.

D039 defers multithreading and thread-count byte comparison out of Phase 2 and into
Phase 3.

D045 further defers panCollapse-side multithreading for now. The current active converter
remains single-threaded; direct stdin streaming from `vg mpmap` to panCollapse is a future
interface direction to research before adding worker-thread execution. RAD output should
be written to disk with a streaming writer using `num_chunks = 0` and complete chunks
emitted incrementally.

## Phase 2 strand scope

D041 limits Phase 2 execution to `--strand sense`. `--strand antisense` and
`--strand both` return to-be-implemented errors in Phase 2 and are deferred to Phase 3,
where the implementation approach must be researched before coding.

## Phase 3 orientation supersession

D042 supersedes the earlier panCollapse strand-mode direction. panCollapse does not
filter compatibility by `sense`, `antisense`, or `both`, and the `--strand` CLI is removed
from the active V1 interface. For every emitted target, panCollapse writes RAD `dirs`
from the actual read alignment orientation relative to that target/transcript. If one
read group has mixed orientations for the same emitted target in the current
implementation scope, the group is dropped and counted rather than assigned a synthetic
direction. alevin-fry, not panCollapse, handles expected library orientation downstream.

## Raw molecule identity failure mode

D043 sets the active V1 CLI for malformed raw CB/UMI values parsed from the GAMP name
field to `--molecule-identity-failures skip|fail`, default `skip`. The stable counters
are `raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
`raw_molecule_unsupported_groups`, and `raw_molecule_skipped_groups`.

## License

The project is licensed under Apache License 2.0.

## 2026-07-28 PanCollapse/count_cr seam supersession

- PanCollapse must emit one information-complete counting BAM for both Gene and
  exact GeneFull_Ex50pAS; it is intended to reformat one GAMP into the BAM
  needed for counting, not require per-count-mode BAMs.
- Normal BAMs carry all production evidence. Audit-only splice-edge counts and
  pre-flank exact-top candidates belong behind an explicit PanCollapse debug
  mode.
- Downstream source-gene normalization remains a `count_cr.py` responsibility
  and must happen before the read-level `Unique` gate and all UMI correction.

## 2026-08-02 Exact Ex50 score eligibility

- Use complete transcript-compatible alignment scores to define the candidates
  that may reach exact Ex50 ordering.
- Retain every candidate within five score points of the read group's global
  compatible top, inclusive. Five points is the intended one-mismatch fudge
  factor under vg's default scoring.
- Apply E/P/B plus library-orientation ordering only after that score filter.

## 2026-08-02 Exact Ex50 score-window release policy

- Keep the five-point exact-Ex50 score window as a valid improvement and the
  default behavior.
- Provide an explicit flag that turns the score filter off and restores all
  compatible exact transcripts before Ex50 ordering.
- Advance this optionalized behavior to panCollapse v0.8.0.

## 2026-08-21 Compact exact-count output policy

- The normal information-complete typed-union BAM is the production default.
- `--compact-exact-count-bam` is an explicit, lossy research opt-in. It must never
  become a CLI, pipeline, configuration, or example-recipe default.
- Every proposed use must record a concrete case-specific reason before execution,
  including the constraint being addressed, why the normal BAM is not being used, and
  the evidence discarded by compact producer-side winner selection.
- Every actual use must be highlighted in the user-facing run update and result and
  labeled compact, lossy, and experimental. The flag must never be added silently.
- Only the user determines whether compact output is necessary and authorizes its use.
  The current decision is that it is not necessary, including for the next chr20 k32
  pangenome-loss diagnostic.
