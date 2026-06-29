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

## License

The project is licensed under Apache License 2.0.
