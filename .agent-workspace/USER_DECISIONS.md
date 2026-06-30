# Resolved Human Product Decisions

All seven product questions presented on 2026-06-28 were answered **A**. These choices
are binding V1 behavior unless superseded by a later entry in `docs/decisions.md`.

D038 supersedes the barcode/UMI tag-source portions below: V1 reads observed raw CB/UMI
from the GAMP name field, not corrected/raw GAMP annotation tags.

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
