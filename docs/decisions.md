# Decisions and Open Architecture Questions

This is an append-only decision log. Do not rewrite past decisions to make the history
look cleaner; add a superseding entry when necessary.

New decision entries should include **Decision source:** `User`, `Orchestrator`, or
`Research/oracle evidence`. If a decision combines sources, name each source and the
part it contributed.

## Settled decisions

### D001 — Project identity

**Decision:** The project is named `panCollapse` and will be delivered as a Git-ready
repository plus ZIP archive.

**Rationale:** The name reflects the V1 canonical collapse behavior while leaving room for
future copy-preserving modes.

### D002 — Implementation environment

**Decision:** C++20, GCC 15 environment, CMake with Ninja.

**Rationale:** C++ permits direct integration with VG libraries. CMake/Ninja is suitable
for a standalone external consumer of an existing VG installation.

### D003 — VG dependency boundary

**Decision:** Standalone project linking to existing VG libraries or checkout. The project
does not fetch or build VG as part of V1.

### D004 — Annotation and indexes

**Decision:** GTF is required. V1 uses preexisting index files and does not create a
panCollapse-specific persistent lookup index.

**Rationale:** Keep the first implementation close to the existing mapping environment.
If annotation lookup is too expensive, profiling may justify a future custom index.

### D005 — Name-grouped input

**Decision:** V1 requires adjacent GAMP records for each read name and validates this
contract.

**Rationale:** Enables streaming without making a sort engine part of the converter.

### D006 — Transcript compatibility includes introns

**Decision:** Entirely intronic and unspliced exon–intron-boundary reads may be compatible
with an annotated transcript. A read may be compatible with multiple isoforms under
exonic and intronic interpretations.

### D007 — Transcript-span extension

**Decision:** Do not reject an otherwise compatible transcript solely because aligned
sequence extends outside its first-to-last-exon span.

**Rationale:** Explicit user requirement. Must be represented by fixtures before coding.

### D008 — Strand modes

**Decision:** CLI supports sense, antisense, and both.

### D009 — Annotation tags

**Decision:** Auto-detect coherent corrected versus raw barcode/UMI annotation pairs,
prefer corrected values, and do not silently mix pairs.

### D010 — Copy collapse

**Decision:** V1 uses an explicit deterministic source-path to canonical-transcript and
gene manifest. Automatic genetically closest-copy inference is deferred.

### D011 — Target scores

**Decision:** Use the maximum compatible member score for a collapsed target. Default
eligibility is tied-best; an advanced option permits a nonzero score difference.

### D012 — Assignment policies

**Decision:** Support `all`, `unique-transcript`, `unique-gene`, and `starsolo-default`.
`all` is the default. Uniqueness is evaluated after score filtering and copy collapse.

### D013 — RAD backend

**Decision:** V1 emits mapper-style uncollated RAD for ordinary alevin-fry processing.

### D014 — No splicing state in V1

**Decision:** Intronic reads receive transcript targets but no spliced/unspliced state.
Synthetic unspliced targets and USA behavior are deferred.

### D015 — Tracked workspace

**Decision:** Agent-facing files are tracked but grouped for clean later removal. The
provenance guard exempts project documentation and agent-workspace files while scanning
the eventual production surface.

### D016 — Outside-span compatibility anchor

**Decision:** A candidate transcript requires at least one aligned reference-consuming
base overlapping one of its exons or implied introns. Alignment overhang beyond the
first-to-last-exon span is allowed.

### D017 — Missing or malformed barcode/UMI tags

**Decision:** Skip the affected read group by default and count the reason. A strict CLI
mode converts the same condition into a hard failure. Mixed corrected/raw pairs are not
silently repaired.

### D018 — Conflicting tags within a read group

**Decision:** If records grouped under one read name disagree on the selected cell
barcode or UMI, fail the run.

**Rationale:** The records no longer describe one coherent molecule identity.

### D019 — Complete collapse-manifest coverage

**Decision:** Every source identity that contributes compatible evidence requires an
explicit collapse-manifest row. A missing row is a hard error; no implicit identity
fallback is allowed.

### D020 — `starsolo-default` is `unique-gene`

**Decision:** `starsolo-default` is an exact alias for `unique-gene` after target score
filtering and source-copy collapse. All surviving transcripts from the single gene remain
in the RAD record.

### D021 — Byte-identical threaded output

**Decision:** Identical inputs and configuration must produce byte-identical RAD and
companion artifacts regardless of thread count.

### D022 — License

**Decision:** Apache License 2.0.

## Phase 0 architecture decisions proposed for Gate Architecture Approved

These entries resolve the Phase 0 external-contract forks for gate review. They become
settled implementation constraints after human approval of Gate Architecture Approved.

### D023 — VG input and dependency boundary

**Status:** Proposed for Gate Architecture Approved.

**Decision:** V1 converter inputs are name-grouped single-end GAMP, GTF, a collapse
manifest keyed to `(source_path_name, source_transcript_id)`, `.xg` for the same
graph/node-id space used to create the GAMP, and an output directory. The
`source_path_name` must exactly match a visible `.xg` path and the GTF seqname;
`source_transcript_id` must match the configured GTF transcript ID attribute. The
expected upstream `vg mpmap` artifact set may include `graph.xg`, `graph.gcsa`/GCSA-LCP,
and `graph.dist`, but `.gcsa`, `.gcsa.lcp`, `.dist`, `.snarls`, and minimizer indexes do
not cross the panCollapse converter boundary after GAMP has been emitted. GBZ/GBWT is a
future coordinate-index alternative, not a V1 replacement for `.xg`, unless later proven
to expose equivalent `PathPositionHandleGraph` behavior and pass the same projection
fixtures.

**Later extension (D094, 2026-09-26).** The required graph-fact equivalence and projection
fixtures now pass for optional GBZ and graph-only XG+GBWT inputs, with named real-read
scientific byte identity. D094 approves these alternatives; embedded-path XG remains the default.

**Decision:** Scope VG compatibility to the local checkout/build family at
`/mnt/ssd/lalli/usr/local/src/vg` and `/mnt/ssd/lalli/usr/local/bin/vg`: version
`v1.75.0-68-ge82694b69 "Spike"`, commit
`e82694b699988205d5105231240db000f9655335`, describe
`v1.75.0-68-ge82694b69-dirty`, GCC 15.2.0, libstd++ 20250808, HTSlib headers 101990,
and HTSlib library 1.19.1-29-g3cfe8769.

**Rationale:** `vg mpmap` commonly produces GAMP from XG, GCSA, and distance indexes, but
panCollapse's post-mapping coordinate join only needs GAMP plus the same XG.
`xg::XG` provides the path-position interface needed for annotation lookup. The dirty
VG/dependency checkout makes broader ABI claims unsafe without later compile/link
evidence.

### D024 — Narrow VG API and future build boundary

**Status:** Proposed for Gate Architecture Approved.

**Decision:** Stream GAMP with `vg::io::for_each<vg::MultipathAlignment>` and read
protobuf fields directly. Use `xg::XG` through `handlegraph::PathPositionHandleGraph`
for coordinate lookup. Avoid VG internal `multipath_alignment_t` and full `libvg.a`
unless Phase 1 smoke work proves the narrower boundary impossible. The VG programming
wiki supports using public vg-ecosystem libraries and handlegraph APIs, but its
submodule/libbdsg/GBWTGraph examples do not override the narrower V1 boundary or the
product rule that panCollapse does not fetch, build, or vendor VG.

**Decision:** Future CMake work should use
`CMAKE_PREFIX_PATH=/mnt/ssd/lalli/usr/local/src/vg` and
`PKG_CONFIG_PATH=/mnt/ssd/lalli/usr/local/src/vg/lib/pkgconfig`, link `VGio::VGio`,
`libhandlegraph::handlegraph_shared`, and a local imported target for
`/mnt/ssd/lalli/usr/local/src/vg/lib/libxg.a`. A throwaway Phase 0 smoke outside the repo
linked those with `PkgConfig::SDSL`, `PkgConfig::DIVSUFSORT`,
`PkgConfig::DIVSUFSORT64`, `OpenMP::OpenMP_CXX`, `pthread`, `m`, and `atomic`, then
instantiated `vg::MultipathAlignment` and `xg::XG`. Phase 1 must add this as a tracked
build smoke before behavior implementation. Phase 1 projection-smoke work supersedes the
throwaway boundary by adding `PkgConfig::ABSL_LOG_INTERNAL_CHECK_OP` and its transitives,
which are required by protobuf repeated-field checks in the local generated headers.

**Rationale:** This keeps V1 on externally usable serialization, handlegraph, and xg
interfaces while deferring build-file creation until after the current gate and keeping
VG source-code dependence limited to contract verification.

### D025 — GAMP grouping contract

**Status:** Proposed for Gate Architecture Approved.

**Decision:** For current single-end `vg mpmap -F GAMP`, alignments for one input read
are contiguous even with mapper threads, but global read order is not guaranteed. V1
validates adjacent grouping and fails if a completed read name recurs later. There is no
supported VG command for GAMP read-name grouping; do not prescribe `vg gamsort` as a
remedy because it is for GAM/GAF graph-position/random sorting, not this GAMP contract.

**Rationale:** Streaming remains viable while still detecting duplicate read names or
nonconforming upstream preprocessing.

### D026 — Complete GAMP traversal and score semantics

**Status:** Proposed for Gate Architecture Approved.

**Decision:** Traverse each `MultipathAlignment` as a DAG over `subpath`. Starts are
`start[]` when present, otherwise source subpaths with no incoming `next` or
`connection`. Complete traversals end at sinks with no outgoing edge. Edges include both
`Subpath.next` and `Subpath.connection`. Traversal score is the sum of selected
`Subpath.score` values plus selected `Connection.score` values; `next` edges add no
transition score.

**Decision:** For each read group, enumerate compatible complete traversals subject to a
Phase 1 cap/guard, project them to source transcript identities, then collapse. A
canonical target's score is the maximum score among compatible source members. Preserve
all tied compatible targets and multimapping evidence.

**Rationale:** This resolves ordinary-edge versus connection-arc ambiguity and preserves
pangenome multimapping without score inflation.

### D027 — Annotation lookup and compatibility implementation

**Status:** Proposed for Gate Architecture Approved.

**Decision:** Use GTF/path coordinates as canonical and convert GTF 1-based inclusive
intervals to 0-based half-open intervals. Evaluate compatibility before collapse at
source transcript identity `(source_path_name, source_transcript_id)`. Build only
in-memory `TranscriptModel` and `PathAnnotation` structures: source path,
source transcript ID, canonical transcript/gene target, strand, merged exons, implied
introns, model intervals, junction set, sorted per-path intervals, and junction hashes.

**Decision:** Project reference-consuming edit spans through
`PathPositionHandleGraph::for_each_step_position_on_handle`, filtered to manifest/GTF
source paths. Do not use nearest-path search. Preserve multiple path projections.
Compatibility requires strand-mode agreement, at least one projected reference-consuming
interval overlapping an exon or implied intron, and every observed splice jump in the
candidate transcript's annotated junction set. Strand modes are relative to the sequenced
GAMP query after projection to source path coordinates; panCollapse does not infer
library chemistry. A GAMP `connection` transition marks an observed splice jump, and a
non-connection same-source-path gap is a splice jump only when its length is at least
`--min-splice-jump` (default 20). Outside first/last exon overhang is allowed;
parent-gene-only overlap is never sufficient.

**Decision:** A persistent custom annotation index remains out of V1. Revisit only after
a pilot shows annotation lookup above 50 percent wall time/CPU, median above
50 us/read group, p95 above 500 us/read group, or p95 projected source-path occurrences
per aligned node above 500, and a prototype shows at least 2x end-to-end speedup.

**Rationale:** The design satisfies the compatibility contract using existing VG path
positions and ordinary memory structures.

### D028 — Barcode and UMI annotation access

**Status:** Proposed for Gate Architecture Approved.

**Decision:** V1 auto-detects barcode/UMI values by parsing both direct annotation
`Struct` string fields and SAM-style tags under annotation key `tags` produced by
`vg mpmap -C`. Candidate values are merged by tag name across sources before pair
selection. Coherent corrected `CB`/`UB` wins globally; raw `CR`/`UR` is used only when no
corrected-tag evidence appears anywhere in the read group. Source disagreement for the
same tag makes the record non-coherent. Corrected/raw disagreement is counted while the
corrected pair is selected. CLI overrides allow explicit tag names and source selection.

**Rationale:** This preserves corrected-tag preference and raw fallback while supporting
nonstandard annotations without silently mixing sources.

### D029 — RAD writer, schema, and orientation policy

**Status:** Proposed for Gate Architecture Approved.

**Decision:** Implement a native minimal C++ writer for fixed classic RnaShort,
uncompressed mapper-style RAD. Use alevin-fry v0.15.0 with libradicl v0.13.0 as
validation oracles, not production dependencies.

**Decision:** The RAD file has no magic/version prefix. The header is `is_paired` u8,
`ref_count` u64, reference names as u16 length plus bytes, and `num_chunks` u64. It is
followed by file, read, and alignment tag sections; each section is `u16 num_tags` and
tag descriptors of `u16 name_len`, name bytes, and `u8 type_id`. Required tags are file
`cblen:U16` and `ulen:U16`, read `b` and `u` integer types, and alignment
`compressed_ori_refid:U32`; file values are `cblen` then `ulen` immediately after the
three tag sections. Records are `u32 naln`, encoded CB, encoded UMI, then `naln`
`compressed_ori_refid` values. `compressed_ori_refid` is `ref_id | 0x80000000` for
forward and `ref_id` for reverse. Bases pack as A=00, C=01, G=10, T=11, with N treated
as A, and the last base in least significant bits. Widths are length 1..4 -> U8, 5..8 ->
U16, 9..16 -> U32, 17..32 -> U64, and >32 unsupported in V1. Chunk `nbytes` includes the
8-byte chunk header, `nrec`, and records. Active `num_chunks` handling is defined by
D045.

Supersession note: D045 sets active panCollapse output to write `num_chunks = 0` and emit
chunks incrementally; seek-and-backpatch remains a valid external RAD writer strategy but
is not the active panCollapse disk-output path.

**Decision:** V1 writes `output_dir/map.rad` and `output_dir/tx2gene.tsv`, sorts and
deduplicates target IDs per read. Active orientation policy is defined by D042:
panCollapse does not filter by library strand and writes actual target-relative
orientation into RAD `dirs`.

Supersession note: D042 replaces the earlier synthetic-forward orientation policy that
was originally paired with D029.

**Rationale:** The fixed schema is small enough to own directly, while external tools
remain the interoperability checks that matter.

### D030 — Deterministic execution model

**Status:** Proposed for Gate Architecture Approved.

**Decision:** Use lexicographically sorted canonical transcript IDs for the RAD target
dictionary. Read-group order follows input GAMP group order. Parallel workers may compute
groups, but the writer commits records in group sequence. Chunks are assembled in
deterministic order with no nondeterministic concurrent appends. Logs and stats must be
stable.

**Rationale:** This implements the existing byte-identical threaded-output decision with
a concrete ownership boundary around the writer.

### D031 — Phase 1 validation plan

**Status:** Proposed for Gate Architecture Approved.

**Decision:** Phase 1 begins with compile/link smoke for the VGio/handlegraph/imported-XG
boundary and a GAMP/XG/GTF annotation smoke. Fixtures must cover exonic, intronic,
exon/intron crossing, isoform exonic versus intronic, annotated versus absent splice
junction, outside overhang anchored, parent-gene-only negative, strand modes, multiple
source transcript identities collapsed without score inflation, missing manifest hard
failure, canonical transcript-to-gene conflict failure, GAMP group recurrence failure,
tag handling, mixed selected CB/UMI lengths, and a tiny RAD end-to-end `alevin-fry
generate-permit-list`, `collate`, and `quant` workflow.

**Rationale:** These tests directly exercise the risk points resolved during Phase 0
before broad implementation starts.

### D032 — Supersede broad VG dependency wording

**Status:** Proposed for Gate Architecture Approved.

**Decision:** D003 is narrowed for V1 by D023 and D024. The only supported VG dependency
target at this gate is the local dirty build family documented in D023 plus the
smoke-validated link boundary in D024. Another VG installation or checkout becomes
supported only after it passes the same compile/link and GAMP/XG projection smoke checks;
matching the public wiki tutorial dependency pattern is not sufficient by itself.

**Rationale:** Phase 0 found usable local VG artifacts but did not prove ABI or link-line
compatibility for arbitrary VG installations.

### D033 — Source transcript identity

**Status:** Proposed for Gate Architecture Approved.

**Decision:** A source transcript identity is the tuple
`(source_path_name, source_transcript_id)`. Path-only manifest rows are insufficient for
ordinary GTF because one source coordinate path may contain multiple transcript models.
The manifest header is
`source_path_name<TAB>source_transcript_id<TAB>canonical_transcript_id<TAB>canonical_gene_id`.

**Rationale:** This preserves intronic and isoform-aware compatibility semantics while
keeping graph coordinate lookup tied to visible `.xg` paths. Upstream `.gcsa`/`.dist`
artifacts do not encode the GTF coordinate join.

### D034 — Strand and observed-splice basis

**Status:** Proposed for Gate Architecture Approved.

**Decision:** V1 strand modes are relative to the sequenced GAMP query after projection
to source path coordinates. `sense` keeps query orientation matching transcript
orientation, `antisense` keeps the opposite, and `both` disables strand filtering.
Observed splice jumps are same-source-path skipped intervals: selected GAMP `connection`
transitions are always splice jumps, while non-connection gaps are splice jumps only when
their length is at least `--min-splice-jump` (default 20).

**Rationale:** This makes strand and splice behavior testable without inferring hidden
library chemistry or classifying short deletions as splice junctions.

## Gate transition notes

### N001 — Phase 0 baseline used for Phase 1

**Decision source:** User instruction to proceed into Phase 1; orchestrator record of the
working baseline.

The user directed the orchestrator to continue into Phase 1 after Phase 0 architecture
review and recentering around GAMP plus matching `.xg`. For Phase 1 contract work,
D023-D034 are treated as the approved architecture baseline unless Gate Behavior
Specified review supersedes them. Their original `Status` fields are left unchanged to
preserve the decision-log history.

## Phase 1 behavior-contract decisions proposed for Gate Behavior Specified

These entries resolve the Phase 1 test-contract shape for gate review. They become
settled implementation constraints after human approval of Gate Behavior Specified.

### D035 — Focused Phase 1 contract documents

**Status:** Proposed for Gate Behavior Specified.

**Decision source:** User requested avoiding a single monolithic Phase 1 contract;
orchestrator chose the specific document split and routing table.

**Decision:** Phase 1 contracts are split by future implementation boundary instead of
combined into one monolithic contract. `docs/phase1/README.md` routes agents to focused
documents for compatibility/projection fixtures, pure policy fixtures, input diagnostics,
RAD interoperability, CLI/run behavior, and build/test integration.

**Decision:** The Phase 1 contract documents are:

- `docs/phase1/compatibility-fixtures.md` for VG-dependent projection and transcript
  compatibility expectations;
- `docs/phase1/policy-fixtures.md` for score, collapse, assignment, target ordering, and
  summary policy expectations;
- `docs/phase1/input-diagnostics-fixtures.md` for barcode/UMI, grouping, manifest, and
  hard-failure diagnostics;
- `docs/phase1/rad-interop-fixture.md` for the exact tiny alevin-fry interoperability
  fixture and expected matrix;
- `docs/phase1/cli-run-contract.md` for CLI options, defaults, exits, traversal cap, and
  stable diagnostic counters;
- `docs/phase1/build-test-plan.md` for the CMake/Ninja/CTest skeleton and labels.

**Rationale:** Future agents can load only the contracts relevant to their work while the
gate reviewer still has an explicit complete fixture matrix and expected-output set.

### D036 — Phase 1 build/test skeleton boundary

**Status:** Proposed for Gate Behavior Specified.

**Decision source:** Orchestrator synthesis of the Phase 0 architecture baseline and
local VG smoke evidence.

**Decision:** The Phase 1 skeleton creates CMake/CTest infrastructure and smoke tests
only. It does not create `src/`, public panCollapse headers, production behavior code,
checked-in fixture data, checked-in generated GAMP/XG/RAD files, or a panCollapse
executable.

**Decision:** Tests are separated by labels. Pure policy tests compile without VG headers
or VG libraries. VG-dependent tests use the local VG checkout/build family and prove the
approved narrow boundary: `VGio::VGio`, `libhandlegraph::handlegraph_shared`, imported
`libxg.a`, SDSL, divsufsort/divsufsort64, Abseil protobuf check-op transitives, OpenMP,
Threads, `m`, and `atomic`.

**Decision:** The VG-dependent skeleton includes both a link smoke and a build-dir-only
GAMP/XG/GTF projection smoke. The projection smoke creates temporary GFA, GTF, JSON GAMP,
GAMP, and XG files under the CTest build directory, verifies GTF coordinate conversion,
streams one generated `MultipathAlignment`, and projects its aligned node span through
`PathPositionHandleGraph`.

**Rationale:** Gate Behavior Specified needs a reproducible test skeleton that matches the
approved architecture, while substantive implementation remains deferred until the next
human gate.

### D037 — Phase 1 CLI snapshot, score universe, and traversal cap

**Status:** Proposed for Gate Behavior Specified.

**Decision source:** Orchestrator judgment after oracle review; compatible-only score
universe derives from the product scoring text, and traversal-cap default is an
orchestrator-proposed guard for human Gate Behavior review.

**Decision:** `docs/phase1/cli-run-contract.md` is the focused Phase 1 CLI/run contract.
Future CLI, diagnostics, and run-summary implementation agents should load it with
`docs/input-output-contract.md` and `docs/architecture-proposal.md`.

**Decision:** `--max-traversals-per-read N` is a V1 CLI option with default `100000` and
valid values `N >= 1`. The cap applies to complete GAMP traversals attempted per read
group before compatibility filtering. If the next complete traversal would exceed the
cap, the run fails hard with `traversal_cap_exceeded_groups`; V1 does not emit a lossy
partial target set.

**Decision:** `read_best_score` is the maximum target score among compatible canonical
transcript targets after source-to-canonical collapse. Incompatible traversals do not
contribute to `read_best_score`. Score filtering removes lower-scoring targets; it cannot
by itself create a score-filtered-to-empty group.

**Rationale:** The CLI snapshot is needed for Gate Behavior Specified and gives future
agents a narrow interface bundle. The traversal cap prevents unbounded DAG enumeration
without silently discarding multimapping evidence, and compatible-only scoring follows the
product score definition.

## Phase 2 planning decisions

### D038 — Raw read-name barcode and UMI source

**Decision source:** User.

**Decision:** panCollapse extracts the uncorrected raw cell barcode and raw UMI from the
GAMP name field and writes those observed values to RAD. Upstream FASTQ preparation is
responsible for placing the raw values in the read name before alignment. panCollapse
does not correct cell barcodes or UMIs, does not build a permit list, and does not perform
UMI deduplication or resolution. alevin-fry performs permit-list construction and
cell-barcode correction, followed by UMI deduplication/resolution during quantification.

**Decision:** The related PanSC RNA convention is
`<original_read_name>_<raw_CB>_<raw_UMI>`, parsed from the right side of the name. This is
the active V1 barcode/UMI source model.

**Decision:** D038 supersedes the tag-source portions of D009, D017, D018, D028, D031,
and D037 for V1 implementation. Older Phase 1 barcode/UMI fixture text that describes
corrected/raw GAMP annotation selection is historical and must not guide implementation
without a new human-approved decision.

**Rationale:** The user clarified that upstream FASTQ preparation preserves the raw
barcode and UMI in the GAMP name field, and that downstream alevin-fry, not panCollapse,
owns correction and UMI resolution.

### D039 — Phase 2 vertical-slice scope

**Decision source:** User.

**Decision:** The Phase 2 planning artifact is `docs/phase2/implementation-plan.md`.
Phase 2 uses a minimal happy path based on `GRP-01`, D038 raw read-name molecule
identity, `MAN-01`, `CMP-01`, and `SC-01`. It uses build-dir-only generated fixtures,
consumes GAMP plus the matching `.xg`, emits mapper-style RAD plus a two-column
`tx2gene.tsv`, and proves alevin-fry consumption with a 1-cell x 1-gene matrix where
`GENE_A=1`.

**Decision:** Phase 2 is single-threaded. Multithreading and byte-identical comparison
across thread counts are deferred to Phase 3.

**Decision:** Raw cell-barcode and UMI lengths are CLI-controlled values. Their defaults
are `--raw-cb-length 16` and `--raw-umi-length 12` for Phase 2, matching the approved
fixture read `read000_AAACCCAAGTTTGGGA_AAAAAAAAAAAA` with raw CB `AAACCCAAGTTTGGGA` and
raw UMI `AAAAAAAAAAAA`. Parsed read-name values must match the configured lengths before
being written to RAD.

**Decision:** USA output is not a Phase 2 feature. USA output should be developed only
when unspliced target generation enters scope.

**Decision:** The active product, input/output, architecture, validation, progress,
decision, and glossary docs should be updated to reflect D038.

**Rationale:** The vertical slice should prove the smallest end-to-end mapper-to-RAD-to-
alevin-fry path before broadening into the full V1 behavior matrix. Single-threaded
execution and no-USA output keep Phase 2 focused on the approved happy path.

### D040 — RAD record fields and orientation semantics

**Decision source:** User clarification and research evidence from local
libradicl v0.13.0 / alevin-fry v0.15.0 source.

**Decision:** panCollapse keeps the D029 writer boundary: implement a native minimal C++
writer for the fixed classic `RnaShort` RAD schema, and use libradicl plus alevin-fry as
validation oracles rather than production dependencies.

**Decision:** Each emitted RAD read record conceptually contains `bc`, `umi`, `refs`, and
`dirs`. `refs` is the read's compatibility target set as zero-based IDs into the RAD
header target dictionary, not genomic coordinates. `dirs` is parallel to `refs`; each
`dirs[i]` describes the orientation associated with `refs[i]`. On the wire, the high bit
of `compressed_ori_refid` carries the direction and the lower 31 bits carry the target
ID.

**Decision:** Every emitted target ID must have a corresponding orientation value.
Direction must be target-level RAD metadata and must not be derived from arbitrary graph
node orientation. Supersession note: D042 replaces D040's earlier synthetic-forward
policy; active panCollapse preserves actual target-relative orientation in RAD `dirs`.

**Rationale:** alevin-fry uses `dirs` during expected-orientation filtering in
permit-list generation and collation, while `refs` drives target-to-gene evidence through
the supplied mapping.

### D041 — Phase 2 strand-mode deferral

**Decision source:** User.

**Decision:** Phase 2 implements only the approved `--strand sense` vertical-slice
behavior. `--strand antisense` and `--strand both` must fail with a to-be-implemented
error rather than silently behaving like `sense`.

**Decision:** Phase 3 will implement `antisense` and `both`, but only after researching
the best way to map GAMP/query orientation and source-path/transcript orientation into
the approved strand settings.

**Rationale:** The V1 product contract still includes all three strand modes. Phase 2 is
intentionally narrower: it proves the single happy path without exposing unimplemented
strand settings as if they were correct.

### D042 — Preserve target-relative orientation instead of panCollapse strand filtering

**Decision source:** User.

**Decision:** panCollapse does not filter read/target compatibility by library strand and
does not expose a `--strand sense|antisense|both` CLI. The final V1 converter writes the
observed raw barcode and UMI to RAD, emits compatible target IDs, and preserves the real
orientation of each read alignment relative to each emitted target/transcript in RAD
`dirs`.

**Decision:** Downstream `alevin-fry` expected-orientation handling owns library
orientation filtering. panCollapse must not encode all retained targets as synthetic
forward hits merely because they passed a panCollapse-side strand policy.

**Decision:** If one read group contributes mixed target-relative orientations for the
same emitted target in the current implementation scope, panCollapse drops that read
group and reports/counts the condition rather than inventing a consensus direction.

**Decision:** D042 supersedes D008, the strand-filtering portions of D027 and D034, the
synthetic-forward retained-hit policy in D029 and D040, and the Phase 3 strand-mode
implementation direction in D041. Older references to `sense`, `antisense`, and `both`
as panCollapse compatibility filters are historical unless a later human-approved
decision restores them.

**Rationale:** The user clarified that RAD `dirs` should carry true target-relative
alignment orientation. This lets alevin-fry apply its own expected-orientation behavior
from the preserved evidence instead of relying on panCollapse to pre-filter by strand.

### D043 — Raw molecule identity failure mode and counters

**Decision source:** User.

**Decision:** The active V1 CLI exposes `--molecule-identity-failures skip|fail` for
raw CB/UMI parse and encoding failures from the GAMP name field. The default is `skip`.
`fail` converts the same conditions into a hard failure with the stable counter name in
the diagnostic message and a nonzero count.

**Decision:** The stable summary counters for these conditions are
`raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
`raw_molecule_unsupported_groups`, and `raw_molecule_skipped_groups`.

**Rationale:** The user approved a raw-name-specific surface instead of reusing the
historical tag-failure terminology. This keeps active V1 diagnostics aligned with D038:
panCollapse reads observed raw CB/UMI from the GAMP name field, while tag-based
corrected/raw selection remains historical unless a later decision restores it.

### D044 — GAMP-to-RAD assignment surface is all-only

**Decision source:** User.

**Decision:** Active GAMP-to-RAD output preserves the full compatible transcript target
set. `all` is the default assignment behavior and the only active assignment option for
RAD output.

**Decision:** `unique-transcript`, `unique-gene`, and `starsolo-default` are not active
GAMP-to-RAD V1 behaviors. They should remain documented only as to-be-implemented future
options, potentially relevant if panCollapse later expands beyond GAMP-to-RAD conversion
to other output formats. If those values are accepted on the current RAD conversion CLI,
they must fail clearly as to-be-implemented rather than filtering RAD records.

**Decision:** D044 supersedes D012 and D020 for active GAMP-to-RAD implementation and
supersedes any Phase 1 fixture text that required uniqueness-policy RAD filtering or
uniqueness-policy removal counters.

**Rationale:** RAD output should carry compatibility evidence for downstream tools rather
than prefiltering to transcript- or gene-unique evidence inside panCollapse.

### D045 — Defer panCollapse multithreading and use streaming RAD-to-disk

**Decision source:** User.

**Decision:** For now, defer panCollapse-side multithreading. The active converter remains
single-threaded and should not add or implement a `--threads` production surface without a
later explicit approval.

**Decision:** Treat direct GAMP streaming from `vg mpmap` to panCollapse stdin as a future
interface direction to investigate. The possible shape is that `vg mpmap` streams binary
GAMP to panCollapse, panCollapse consumes and groups the stream, and panCollapse writes
RAD output incrementally.

**Decision:** Write `map.rad` to disk using a streaming RAD writer. The writer emits the
prelude/header with `num_chunks = 0`, which libradicl treats as an unknown chunk count,
then emits file-tag values and one complete chunk at a time. Only the current chunk needs
buffering so its byte count and record count can be written in the chunk header.

**Decision:** Do not add a stdout RAD-output production CLI in this change. If a later
human-approved decision adds `--output -`, stdout must carry only binary RAD data and all
logs/progress must go to stderr. Direct live piping into current alevin-fry commands is
not assumed.

**Decision:** D045 narrows the current Phase 3 implementation path relative to D021 and
D039. D021's byte-identical threaded-output requirement remains historical/future V1
context, but current active work should prioritize the single-threaded converter,
medium-scale known-truth RAD fixture, and stdin-streaming design research rather than
adding worker threads.

**Rationale:** The user identified that panCollapse may be more useful as a streaming
consumer of `vg mpmap` alignments than as a multithreaded postprocessor. Deferring
threads avoids committing to a concurrency architecture before the preferred process
boundary is settled. The local libradicl v0.13.0 reader confirms that header
`num_chunks = 0` means the chunk count is unknown and chunks are read until EOF, so
panCollapse does not need to build the whole RAD file in memory or backpatch the final
chunk count for the active disk-output path.

### D046 — Unaligned read-group diagnostic counter

**Decision source:** User (approved landing an interrupted in-flight increment).

**Decision:** A GAMP read group whose records all carry zero subpaths is treated as
unaligned input. panCollapse emits no RAD record for it, counts it under both
`no_compatible_transcript_groups` and the stable counter `unaligned_reads`, and continues.

**Decision:** `unaligned_reads` is an additive subset of `no_compatible_transcript_groups`,
not a replacement. The aggregate no-compatible-transcript count from product-spec Section 12
still includes unaligned groups, so existing summaries and fixtures remain valid.

**Decision:** Unaligned groups are not raw molecule-identity failures. The
`raw_molecule_*` counters and the `--molecule-identity-failures skip|fail` policy from D043
are unaffected, because the group is dropped for lack of any alignment, not for a CB/UMI
parse or encoding problem.

**Rationale:** `vg mpmap` emits a `MultipathAlignment` with an empty `subpath` list for a
read it cannot place. Folding that case into the general no-compatible-transcript bucket
hides mapper-side placement failure; splitting it out preserves the signal while keeping
the aggregate contract intact. Product-spec Section 12 lists the required counters as "at
least," so an additive counter is contract-compatible and needs no spec change.

### D047 — GAMP-to-RAD bridge scope and human-pangenome GAMP-driven fixture

**Decision source:** User.

**Decision:** panCollapse is scoped as a bridge from GAMP files to linear-genome RNA
count tools, with alevin-fry as the proof-of-concept downstream consumer. The
FASTQ -> `vg mpmap` alignment step is out of scope: panCollapse does not own it and the
fixtures do not test it. Read simulation and `vg mpmap` matter only as one way to obtain a
realistic GAMP input; a sufficient GAMP simulation is equally acceptable for fixtures.

**Decision:** The production-applicability validation fixture must be generated from the
human pangenome (the pinned MHC `sampleA` spliced bundle in
`docs/testing_fixture_creation.md`), not only toy graphs, so coverage reflects production
RNA-seq graphs.

**Decision:** The RAD oracle is GAMP-driven. Given a GAMP file plus the `.xg`, GTF, and
collapse manifest, the expected RAD is computed independently of panCollapse by applying
the documented projection, compatibility, collapse, scoring, and orientation rules
directly to the GAMP records. Expected RAD is not derived from upstream read origins and
never from panCollapse output.

**Decision:** D047 supersedes the BEERS2-plus-`vg mpmap` "primary path" framing and the
"artificial GAMP must never be the primary fixture" rule previously in
`docs/testing_fixture_creation.md`. BEERS2 and `vg mpmap` remain optional realistic
read/GAMP generators, not required interfaces, and the former mapping-stability rule is
retired because the GAMP is now the tested input with no upstream origin to reconcile.

**Rationale:** The user clarified that panCollapse's contract is GAMP -> RAD for downstream
linear-genome RNA counting. What must be proven is that a realistic human-pangenome GAMP
is converted to exactly the correct RAD. Whether the GAMP came from real `vg mpmap` or a
sufficient simulation does not change that contract, so the fixture oracle is anchored on
the GAMP itself.

### D048 — Graph-native transcript-compatibility scoring replaces traversal enumeration and GTF projection

**Decision source:** User.

**Decision:** The GAMP-to-RAD core is a graph-native transcript feature count, specified in
`docs/conversion-algorithm.md`. For each read, across all of its GAMP alignments (primary and
every supplementary/secondary record): score each aligned node under vg's own scoring scheme
(reproduced per node from the `Mapping` edits and validated against `Subpath.score`); add
each node's score to every HST path that traverses it; take the single highest HST score
across all the read's alignments plus every HST tied with it; collapse those winners to
unique transcript IDs to form RAD `refs`; and record per-transcript orientation from the
read's direction along the HST path, resolving disagreement by the majority of aligned bases.

**Decision:** Runtime inputs are the name-grouped GAMP, the graph with `vg rna` HST paths,
and a transcript-to-gene map. The GTF and `vg rna` are reference/fixture-creation tools only,
not runtime inputs. Transcript-copy collapse is implicit in HST path naming, not an explicit
manifest. Compatibility is HST-path node membership: no exon/intron/junction projection, no
complete-traversal enumeration, and therefore no traversal cap.

**Supersedes:** D048 replaces the traversal-enumeration and GTF-projection algorithm for
active GAMP-to-RAD conversion — D026 (complete-traversal enumeration and per-traversal
scoring), D027 and D034 (GTF-coordinate projection, exon/intron/junction compatibility,
`--min-splice-jump`), D033 and D010/D019 (explicit collapse manifest), D037's
`--max-traversals-per-read` and `traversal_cap_exceeded_groups` (no enumeration means no
cap), and the exon/intron compatibility framing of D004/D006/D007/D016. Orientation follows
D042 except that a mixed-orientation transcript now takes the majority-of-bases orientation
instead of being dropped. RAD wire format (D029/D040), streaming writer (D045), raw CB/UMI
sourcing (D038), name-grouping (D005/D025), unaligned counting (D046), and the
human-pangenome fixture (D047) are unchanged.

**Rationale:** The multipath format is a compact DAG that exists to avoid enumerating linear
paths; enumerating complete traversals reintroduced that exponential cost and hard-failed the
traversal cap on real MHC GAMP. Scoring nodes and reading transcript membership off the
embedded HST paths is linear, needs no annotation projection at runtime, and answers the
actual question — which transcripts a read is compatible with — directly from the graph the
mapper produced. vg source confirms per-node scoring reproduces `Subpath.score` exactly
(`alignment_scorer.cpp`) and that HST paths are stored transcript 5'->3' (`transcriptome.cpp`
`reorder_exons`), so orientation reads directly off the path.

### D049 — Streaming GAMP input and seek-and-backpatch RAD output

**Decision source:** User.

**Decision:** panCollapse reads GAMP as a stream, including from stdin via `--gamp -`, so it can
run as `vg mpmap ... | panCollapse convert --gamp - ...`. It writes `map.rad` to disk with a
streaming seek-and-backpatch writer: the header, target dictionary (from the t2g), and tag
sections are written up front with a placeholder `num_chunks`; each read record is written as
its group is flushed; and at finalize the writer seeks back to patch the chunk byte/record
counts and set `num_chunks` to the exact value. Only the current record is buffered, and the
chunk header is written lazily on the first record, so a run with no emitted record leaves a
header-only file with `num_chunks = 0` and no chunk.

**Decision:** D049 supersedes D045's `num_chunks = 0` unknown-count disk output. Seek-and-
backpatch produces exact `num_chunks` and chunk counts that the supported alevin-fry v0.15.0
accepts, resolving the D045-versus-alevin-fry chunk-count conflict. The seek requires a
regular output file, which is why RAD goes to disk rather than stdout.

**Rationale:** The user chose the go-back-and-patch method so output stays exact-count and
alevin-fry-compatible while still streaming to disk without buffering all emitted records.

### D050 — v0.1 release hardening (blocker and non-blocking review resolution)

**Decision source:** User (directed resolution of the pre-release review in
`docs/v0.1-review-findings.md`).

**Decision:** The following behaviors are settled for v0.1. They extend, and do not reopen,
D048/D049.

- Multi-chunk RAD output. The streaming writer (D049) rolls records into a new chunk once the
  current chunk would exceed a byte budget (`PANCOLLAPSE_MAX_CHUNK_BYTES`, default 1 GiB),
  seek-and-backpatching every chunk header plus the file-level `num_chunks`. This removes the
  single-chunk `u32` byte ceiling that would have aborted large runs, so v0.1 is no longer
  restricted to small-scale inputs.
- Atomic output. `map.rad` is written to a temporary path in the output directory and renamed
  on successful finalize; a run that hard-fails leaves no valid-looking `map.rad`.
- Node-id-space validation. An aligned GAMP node absent from the `--xg` graph is a hard failure
  (the GAMP was aligned to a different graph), whereas a node that exists but lies on no HST
  path remains legitimate and yields no target.
- Diagnostics. `summary.tsv` reports the per-emitted-group target-size histogram
  (`emitted_target_count_histogram`). The whole-run `completed_names` recurrence-detection set
  is retained (exact non-contiguous-input detection) and its O(distinct read-group names) memory
  cost is documented rather than bounded.
- Robustness and ergonomics. The quality-adjusted scorer bounds-checks all read/node indexing
  and throws on inconsistent input; `--version`/`--help` are success paths; project targets
  build with `-Wall -Wextra`; and machine-specific build paths are parameterized and documented.
- The canonical spec (`docs/product-spec.md`, `docs/validation-contract.md`) was reconciled to
  the D048/D049 behavior (no `num_chunks = 0`, no `score_removed_targets`, no
  mixed-orientation-drop counter).

**Rationale:** The adversarial pre-release review found three blockers and eleven non-blocking
issues; the user directed clearing all of them for a full (not small-scale-rescoped) v0.1. Each
change is covered by a hermetic CTest; the full suite is 30/30.

### D051 — Per-node HST lookup cache and performance-scaling analysis

**Decision source:** User (directed performance work).

**Decision:** The per-node HST lookup memoizes each distinct node's HST crossings the first
time the node is seen and replays the cached result on later visits, so a node's path steps
are scanned at most once per run rather than once per node-visit. The cache stores
`(HST-name pointer, orientation)` pairs pointing into the immutable `hst_path_name` map, and
one entry per step so a path visiting a node twice still contributes twice. Output is
byte-identical because the tally accumulates per path name and is order-independent; the
node-id-space hard failure (D050) is preserved by validating `has_node` on cache miss.

**Measured:** On the real 1M-read MHC GAMP (`build/real_mhc_scratch/mhcA.gamp`, 1,888,691
records, 39,103 emitted) against `sampleA` `spliced.xg`, wall clock dropped from 3:47 to
2:44 (about 28%, 1.39x) with `map.rad`, `tx2gene.tsv`, and `summary.tsv` byte-identical to
the pre-cache output and peak RSS up about 16 MB. The full CTest suite stays 30/30.

**Finding (multithreading, no implementation):** The run is about 99% single-core CPU-bound
(user 162.5s vs wall 163.6s), and per-read-group work (`tally_read_group` + `select_targets`)
is independent, so it is the natural parallel unit. The identified structure is a serial
producer (GAMP parse + name-grouping), a pool of workers sharing the read-only `xg`/t2g, and
a single writer, with an order-preserving reorder buffer keyed by input group index so RAD
bytes stay deterministic. The per-node cache must become read-only during the parallel phase
(eager precompute) or per-thread to avoid locking. This is identification only. D045 still
governs: no `--threads` production surface is added without a later explicit approval, and
any threaded mode returns to a gate and carries a byte-comparison acceptance criterion.

**Finding (sorted/indexed GAMP):** A sorted and/or randomly indexed GAMP is judged not
worthwhile for the current design. panCollapse is a single linear streaming pass that reads
every record once in order; name-grouping is already a required correctness precondition
(usually satisfied by `vg mpmap`), not a throughput lever, and there is no random access to
accelerate. Partitioning at read-group boundaries is useful only as a parallelization
enabler, and even then needs a few boundary split offsets, not a persistent index; the
bottleneck is graph-lookup CPU, not GAMP I/O. No custom GAMP index is planned.

**Rationale:** The cache attacks the measured hotspot (`for_each_step_on_handle` repeated per
node-visit) without changing behavior. The threading and GAMP-index findings were requested
scoping and are recorded as analysis, not as approved work, to keep D045 authoritative.

### D052 — Optimized default build and v0.2 runtime image

**Decision source:** User (directed performance work; requested a v0.2 image built from the
optimized tool).

**Decision:** The build defaults to an optimized configuration. When neither `CMAKE_BUILD_TYPE`
nor a multi-config generator is set, `CMakeLists.txt` forces `RelWithDebInfo` (`-O2 -g`).
Earlier builds compiled at `-O0` because no build type was set, and both the documented recipe
and `scripts/build-docker-image.sh` (which packages `build/src/panCollapse`) inherited that.

**Measured:** On the real 1M-read MHC GAMP, `-O2` cut wall clock from 2:44 to 0:45.6 (about
3.6x over the `-O0` cached build, roughly 5x over the original `-O0`), byte-identical output,
full CTest 30/30. `-O3` (Release) measured within noise of `-O2` (0:46.1) and drops debug
symbols, so `RelWithDebInfo` is the default rather than `Release`. There are no `assert()`s in
production code, so the `-DNDEBUG` that both types set disables no runtime checks.

**Decision:** The version is bumped to 0.2.0 (`project()` VERSION, propagated to `--version`
and the Dockerfile image label), and the runtime image tag default is
`josephlalli/pancollapse:v0.2`. v0.2 is a performance release: its output is byte-identical to
v0.1, only faster.

**Verification:** The v0.2 image built from the `-O2` binary reports `panCollapse 0.2.0`, and a
containerized conversion of the hermetic smoke fixture is byte-identical to the host `-O2` run.

**Rationale:** Shipping an `-O0` binary left roughly a 5x speedup unclaimed behind a one-line
build fix. Making the optimized build the default (not a flag users must remember) ensures the
documented build and the released image are fast. No production behavior changes, so no gate is
implicated beyond the byte-identity checks already required for performance work.

### D053 — Record move and reused flat tally (profile-guided single-threaded wins)

**Decision source:** User (directed performance work after profiling).

**Profiling** (callgrind on the `-O2` binary, 1M-read MHC): the scorer is about 1%; the run is
dominated by GAMP ingestion (~60%: protobuf parse plus a per-record deep copy plus BGZF
decompress) and the per-group tally (~34%). The largest cost categories are `malloc`/`free`
(~27%) and `std::string` comparison (`memcmp`, ~13%). Multithreading was judged low value:
only ~34% is parallelizable, so a producer/worker design is Amdahl-capped near 1.5x and would
need BGZF-stream sharding to go further; the high-ROI work is single-threaded copy and
allocation removal.

**Decision:** Two byte-identical changes.
- Move each parsed `MultipathAlignment` into the group buffer instead of copying it
  (`main.cpp`), removing the roughly 21% per-record deep copy.
- Key the per-group tally with a reused `absl::flat_hash_map` workspace instead of a per-group
  `std::map` (`pathtally.hpp`, `main.cpp`): the flat map hashes instead of doing red-black-tree
  string comparisons (removing the ~13% `memcmp`), and reusing one instance across groups
  removes the per-group node allocation. `cmake/PanCollapseVg.cmake` links `absl_raw_hash_set`;
  abseil is already a transitive project dependency, so this adds no new third-party library.

**Measured** (1M-read MHC, `-O2`): 45.6s -> 41.2s (move) -> 30.4s (flat tally), byte-identical
at each step, peak RSS 393 -> 281 MB, CTest 30/30. End to end the optimized v0.2 build is about
7.5x faster than the original `-O0` v0.1 (227s -> 30s) with identical output.

**Rationale:** The profile showed compute (scoring) is negligible and the cost is data movement
-- copies and allocations. Both wins are local, byte-identical, and single-threaded, so no gate
is implicated beyond the byte-identity checks. Threading stays deferred (D045) as low value per
the profile.

### D054 — Optional BAM output for a CellRanger-style counting stack

**Decision source:** User (new goal, with contig scheme, umi_tools, no-gene, and GN choices
confirmed via plan approval).

**Decision:** Add an opt-in BAM output (`--bam-out`) alongside the RAD, consumable by
`umi_tools count --per-gene --gene-tag=XT` then DropletUtils `emptyDropsCellRanger`. The RAD is
unchanged and byte-identical with or without `--bam-out`; the BAM is written from the same
`flush_group` pass, so gene assignments cannot diverge. No genome surjection, no new runtime
input, no new persistent index; genes come only from `t2g.transcript_gene` (the same ids the
RAD/tx2gene use). Single-threaded and streaming, via htslib (already a transitive dependency).

**Decision:** The BAM carries panCollapse's graph-derived gene set as 10x tags, not real
coordinates, because a `--per-gene --gene-tag` counter dedups by `(cell, UMI, gene)` and ignores
position. One mapped record per emitted read (no-gene/unaligned reads skipped, matching RAD
parity), placed at position 1 of a synthetic per-gene contig (one `@SQ` per gene). Tags: `CB`/`CR`,
`UB`/`UR` (raw values, so `CB`==`CR` and `UB`==`UR`), `GX` = full sorted compatible-gene set
(`;`-joined, never collapsed), `GN` = `GX` (the t2g has only gene ids), and `XT` = the single gene,
or omitted when multi-gene (`--bam-multigene omit`, default; `first` writes the first gene). Header
`@HD SO:unsorted` (sort + index downstream), `@SQ` per gene, `@PG panCollapse`.

**Verification:** Ten hermetic CTests (label `bam`): `samtools quickcheck`, header `@SQ`/`@PG`,
`map.rad` byte-identical with vs without `--bam-out`, tag/`GX` parity against the independent
oracle, `umi_tools count` reproducing a naive tag recount (GENE1=2, GENE2=1), and
position-independence (two records, same CB/UMI/XT, different POS -> one molecule). Full suite
40/40. On the 1M-read MHC GAMP: `map.rad` byte-identical to the pre-BAM baseline (sha
`7df66891...`), BAM `quickcheck` passes with 39,103 records over 317 gene contigs, and the full
`samtools sort`/`index` + `umi_tools count` pipeline yields 30,105 molecules across 9,701 cells x
203 genes (2,755 multi-gene reads correctly skipped for lacking `XT`).

**Rationale:** Counting off panCollapse keeps reads that never surject to GRCh38 (non-reference
alleles/insertions) that a genome-surject + featureCounts path would drop, because gene
assignment is graph-native. Per the scope rule a new output backend returned to a design gate;
the design was approved before implementation. This does not change any settled RAD behavior.

### D055 — GeneFull counting via gene-body graph annotation (no mode flag)

**Decision source:** User (directed after a counter-comparison analysis showed the STAR arm was
run with STARsolo `--soloFeatures GeneFull` while panCollapse is spliced-only, an exon-vs-body
confound worth an estimated ~6-7% of signal on MHC GEX; then observed during implementation that
gene calling can reuse the graph's native path mechanism instead of a custom map).

**Decision:** GeneFull (a read calls a gene if it overlaps the gene body, exon or intron) is
**not a panCollapse mode**. It is the ordinary spliced count run against a graph annotated with
**gene-body paths**, selected by the t2g. panCollapse already reads transcript membership off the
graph's embedded HST paths with no custom index (the t2g names which paths and their gene); a
gene body embedded as one unspliced path behaves identically, and its intron nodes carry the
gene. `scripts/make-gene-annotation.sh` embeds one gene-body path per gene (`vg rna
--feature-type gene`, grouped by `gene_id`; `-l` to thread through haplotypes for alt-node
coverage) alongside the original HST paths and writes the gene t2g (`gene_body_path <TAB> gene`).
The same annotated graph then gives a spliced count with the HST t2g or a GeneFull count with the
gene t2g. **No binary change:** the converter is byte-identical to D054/v0.3.

**Superseded during implementation:** an earlier attempt added a `--gene-mode {spliced,full}`
flag plus a `--gene-loci` node->gene TSV input and a parallel lookup branch. That was removed
before merge: it duplicated the graph's own path index (a custom node->gene map is exactly the
"custom lookup index" the transcript path avoids, against the D048/no-custom-index principle),
and `--gene-mode spliced` already produced GeneFull when pointed at a gene-body t2g -- so the flag
conflated "which mechanism" with "which feature layer," the latter being the t2g's job.

**Semantics (verified on fixtures):** GeneFull's only marginal call over spliced is the
purely-intronic read; any read touching an exon node is already called by spliced counting via
that node's HST. Overlapping gene bodies resolve by the same top-score-plus-ties rule (entirely
shared -> multi-gene; dominant -> that gene). A single t2g must not list both HST and gene-body
paths: the gene call stays correct but the transcript dictionary gains a redundant gene
pseudo-transcript (a spliced/GeneFull hybrid) -- one t2g per run. Prerequisite: the graph must
retain intron sequence (not built with `vg rna -d`).

**Verification:** hermetic `genefull` CTests on a gene with exon nodes 1,3 (HST `1+,3+`) and
intron node 2 (on no HST): `make-gene-annotation.sh` embeds the gene body; the SAME annotated
graph drops the intronic read with the HST t2g (`emitted=1`) and calls it with the gene t2g
(`emitted=2`, carried in the BAM as GENE1). Full suite 45/45.

**Rationale:** panCollapse being spliced-only was a genuine capability gap versus modern
CellRanger/STARsolo (include-introns by default). Closing it needs no code -- the annotation
already lives in the graph as paths, the same reason spliced mode needs no runtime GTF. Keeping
GeneFull a graph+t2g concern (not a flag) preserves the "annotation lives in the graph, no custom
index" invariant and lets any feature layer be added as more paths, each selected by its own t2g.

### D056 — Optional target-relative orientation filter (`--strand`)

**Decision source:** User (directed after reviewing STARsolo's `GeneFull_Ex50pAS` antisense
exclusion / `--soloStrand`: an orientation filter belongs at the gene-calling step).

**Decision:** Add `--strand {both,forward,reverse}` (default `both`). After winner selection, keep
only targets whose read orientation (majority of aligned bases -- the same value written to the
RAD `dirs`) matches: `forward` keeps sense targets, `reverse` keeps antisense targets, `both`
keeps all. A read whose compatible targets are all filtered out emits no RAD/BAM record and is
counted in a new `strand_filtered_groups` summary counter (distinct from
`no_compatible_transcript_groups`, which is for reads with no compatible target at all). The
filter is per target, so a read that is sense for one gene and antisense for another keeps the
matching target under a directional policy.

**Supersedes:** D042 removed a `--strand sense|antisense|both` surface in favor of preserving
orientation in the RAD `dirs` and leaving strand filtering to downstream (alevin-fry
expected-orientation). D056 reintroduces panCollapse-side strand filtering **as an opt-in**: the
default `both` preserves D042 exactly (output byte-identical, no filtering), and `dirs` is still
written; `forward`/`reverse` add the filter for callers who want it applied at this step, e.g. a
sense-stranded library dropping antisense reads before counting.

**Verification:** hermetic `strand` CTests on the smoke fixture (readfwd/readmulti forward,
readrev reverse): `--strand forward` emits 2 with `strand_filtered_groups=1` and no readrev in the
BAM; `--strand reverse` emits 1 (readrev) with `strand_filtered_groups=2`. Full suite 47/47;
`--strand both` output byte-identical to before.

**Rationale:** STARsolo/CellRanger apply library strandedness at feature assignment; panCollapse
had pushed it downstream (D042). Making it an opt-in at the gene-calling step matches the external
tools and closes the `GeneFull_Ex50pAS`-style antisense gap, without changing the default
(orientation-preserving) behavior.

### D057 — Ledger count modes reproducing STARsolo/CellRanger Gene and GeneFull

**Decision source:** User (proposed the per-read exon/intron base ledger to match STARsolo
`soloFeatures` Gene/GeneFull and the CellRanger variants).

**Decision:** Add opt-in `--count-mode {score,gene,genefull,genefull_exonoverintron,
genefull_ex50pas}` (default `score`). The ledger modes count, per read per gene, the aligned
**match** bases on exon nodes vs intron nodes, then apply the mode rule: `gene` keeps a gene the
read is >=50% exonic for; `genefull` keeps any gene body overlapped; `genefull_exonoverintron`
and `genefull_ex50pas` add exon-overlap tie-breaks and (Ex50pAS) exclude 100%-exonic antisense
reads. All apply the CellRanger `Unique` rule: a read kept for >1 gene is dropped and counted in
`multigene_dropped_groups`. The pure rule lives in `pathtally_ledger.hpp` (unit-tested).

**Decision:** Membership comes from **two ordinary path->gene t2gs** read off the graph's
embedded paths -- `--t2g` is the exon (HST) layer, `--body-t2g` the gene-body layer -- with no
custom node->gene map ([[graph-native-feature-annotation]]). Per node, a gene is exonic if the
node lies on any of its exon paths (exon wins over body), else intronic. It stays O(nodes): the
same per-node walk as D048, accumulating base counts instead of vg scores (no traversal
enumeration). This is genuinely different logic from the D048 top-score selection (overlap-based,
not score-based) and needs both layers, so unlike the coarse gene-body t2g (D055) it does warrant
a mode flag. `--count-mode score` (default) is the unchanged D048 count; RAD/BAM output is
byte-identical without the flag.

**Not implemented (documented):** exact splice-junction concordance for `Gene` (the >=50%-exonic
gate approximates CellRanger, and spliced reads naturally contribute zero intronic bases because
introns are deletions); the `Rescue`/`EM`/`Uniform` multimapper distribution modes (only
`Unique`); STARsolo `SJ` (a junction/edge feature outside the node->path model).

**Verification:** `pathtally_ledger_test` (14 assertions: Gene exonic gate incl. exon-A+intron-B
-> A, GeneFull intronic/intergenic, ExonOverIntron and Ex50pAS tie-breaks, Ex50pAS 100%-exon
antisense exclusion). Integration `genefull_ledger_*` on the both-annotated fixture: `gene` drops
the intronic read (emit 1), `genefull` keeps it (emit 2), `genefull_ex50pas` drops a 100%-exonic
antisense read while keeping the sense one; a read in two gene bodies is dropped with
`multigene_dropped_groups=1`. Full suite 51/51.

**Rationale:** the graph-native ledger is exactly CellRanger's overlap accounting, computed off
the two annotation layers with no new index and no traversal enumeration, giving a fair
like-for-like comparison against STARsolo `Gene`/`GeneFull`/`GeneFull_Ex50pAS` while keeping the
D048 count as the untouched default.

### D058 — `--bam-multigene all`: carry ledger multi-gene reads into the BAM for a downstream UMI-level rescue

**Decision source:** User (directed after a downstream consumer -- panSC, the sibling project that
runs panCollapse through `count_cr.py` -- root-caused a ~4% gene-UMI undercount vs STARsolo
(target-2 MHC GEX, 2,388,781 read pairs) to exactly this gap: panCollapse's ledger `Unique` rule
correctly drops a read compatible with >1 gene, but never carries it anywhere a UMI-level rescue
could see it, so STARsolo's `MultiGeneUMI_CR` rescue -- which panSC's `count_cr.py` already
implements -- had nothing to rescue. The deficit was concentrated in classical-HLA/paralog loci
(89.7% of |delta|; HLA-B alone -10,694 UMIs, Gene model); H2/H3/H4 (UMI dedup, mapping gap,
gene-set mismatch) were each ruled out as contributing 0. See panSC's own `docs/decisions.md` entry
`rna-crcount-multigene-rescue` for the full root-cause writeup this decision acts on).

**Problem:** D057's ledger `gene`/`genefull_ex50pas`/`genefull_exonoverintron`/`genefull` modes
correctly apply CellRanger's read-level `Unique` rule (a read kept for >1 gene is dropped,
`multigene_dropped_groups`) but that is only HALF of CellRanger/STARsolo's actual algorithm.
STARsolo layers a UMI-level rescue on top (`MultiGeneUMI_CR`): a UMI whose reads span >1 gene is
still assigned to the gene where it has the most reads, discarding only on an exact tie. Because
panCollapse hard-dropped the read before BAM emission, a downstream counter implementing that
rescue (panSC's `count_cr.py`) never received the read to rescue -- the drop and the rescue must
happen in different places (read-level drop is panCollapse's job pre-emission; UMI-level rescue
needs to see every read for a UMI across cells first, which only the downstream counter can do),
so closing the gap requires panCollapse to stop discarding the evidence, not to implement the
rescue itself.

**Decision:** Add `--bam-multigene all` alongside the existing `omit` (default) and `first`. It is
active ONLY when a ledger `--count-mode` is set (in `score` mode there is no Unique drop to rescue
from -- multi-target reads there are ordinary D048 multimapping evidence, already emitted to both
RAD and BAM regardless of `--bam-multigene`) and ONLY when `--bam-out` is given (it is otherwise
inert, matching how `omit`/`first` already behave without a BAM writer). Under `all`, a read the
ledger Unique rule keeps for more than one gene is:
- still counted in `multigene_dropped_groups` and still given NO `map.rad` record -- the RAD /
  `emitted_target_histogram` / alevin-fry-facing contract is completely unchanged from D057; this
  is a BAM-only change;
- but IS written to the optional BAM (when `--bam-out` is set), carrying its full candidate-gene
  set exactly as the existing `GX`/`GN` tags already do generically for any multi-gene read (no
  change needed there) and no `XT` (so a `--per-gene --gene-tag=XT` counter still skips it, but a
  counter reading `GX` can rescue it).

**Implementation:** the read-level drop (`main.cpp`, formerly one unconditional early-return) is
split into "drop from RAD" (unconditional for a ledger multi-gene read, as before) and "also skip
BAM" (now conditional on `--bam-multigene all` AND a BAM writer being active). The BAM-emission
block was already mode-agnostic (it builds `GX`/`GN`/`has_xt` from whatever `targets` it is given,
regardless of `--count-mode`), so letting a ledger multi-gene read reach that block needed no
changes to it beyond the existing `has_xt` computation, which already defaults to false for
`genes.size() > 1` under any policy but `first`. `score` mode's code path is unaffected byte-for-byte:
its multi-target reads were never subject to the D057 drop, so every new conditional degrades to
the pre-D058 behavior for it.

**Tie rule (verified against STARsolo source, not the docs, per the user's explicit request):**
panCollapse does not implement the rescue itself -- that is the downstream counter's job (panSC's
`count_cr.py`, which already had a correct `MultiGeneUMI_CR` implementation before this change; only
its read-acceptance gate needed to stop skipping multi-gene reads). But the rule was still confirmed
here because panCollapse's emission format (whether to carry the FULL gene set, and whether to omit
or fabricate an `XT`) has to match what that rule needs to operate on. STARsolo 2.7.11b
(`SoloFeature_collapseUMIall.cpp`, commit `b1edc1208d91a53bf40ebae8669f71d50b994851`, lines 203-238):
a UMI (already 1MM_CR-corrected within a cell) is assigned to the gene with the STRICTLY highest
read count; an exact tie for the max discards the UMI for every gene (not "keep first," not
EM/Rescue distribution -- those are a separate mechanism gated by a different flag and do not apply
here); a second guard re-checks each gene's PRE-correction raw read count for that UMI sequence and
discards even a non-tied winner if another gene had a strictly higher raw count there. Also verified
(`SoloReadFeature_record.cpp` `outputReadCB()`, the `reFe.geneMult` path): STARsolo's own upstream
per-gene raw tally, for a single-alignment read overlapping more than one gene locus, logs one
independent `(cell, gene, UMI, read)` entry per candidate gene -- i.e. the SAME physical read
contributes a raw read to every gene it is compatible with, before the cross-gene resolution above
picks the dominant one. This is the mechanism `--bam-multigene all`'s full `GX` set exists to make
possible: the full candidate-gene set is exactly the input that upstream tally step needs.

**Verification:** `pathtally_ledger_test` and the pure-rule unit tests are unaffected (the rule
itself, `apply_count_mode`, was already correct and already returns every kept candidate gene for a
multi-gene overlap -- see the existing `GeneFull: two bodies -> both` case -- the gap was entirely
in the caller's post-rule drop). New hermetic CTests (label `multigene`) on the `genefull_smoke`
fixture, extended with a second gene body overlapping the first gene's intron node so a read on
that shared node is genuinely multi-gene under a ledger mode: `--bam-multigene omit` (default)
still drops the read from BOTH RAD and BAM exactly as D057 (`multigene_dropped_groups` increments,
no BAM record); `--bam-multigene all` drops it from RAD (`multigene_dropped_groups` still
increments, `emitted_groups` unchanged) but the read now appears in the BAM tagged `GX` with both
genes and no `XT`. Full suite 58/58.

**Rationale:** This closes the gap without duplicating the rescue logic panSC's `count_cr.py`
already implements and had independently verified against STARsolo source -- panCollapse's job is
only to stop discarding the evidence a correct downstream rescue needs, which is exactly the
"graph annotation carries the evidence, no custom logic duplicated downstream" pattern D055/D057
already established for gene-body annotation. Making `all` an explicit opt-in (not the new default)
keeps `omit`/`first` byte-identical to before, so no existing consumer's BAM output changes unless
it asks for the new policy.

### D059 — Emit per-gene orientation (`GD`) and move the sense/antisense policy to the counter

**Decision source:** User (directed after the same downstream consumer -- panSC's `count_cr.py` --
benchmarked the vg pangenome arm against STARsolo on whole chr20 and found the intron-inclusive
`genefull_ex50pas` count inflated by antisense reads: large (-)-strand genes overlapping antisense
transcription over-counted up to ~700x, e.g. PTPRT vg 26,432 vs STAR 36, the pattern universal across
the top over-counted genes -- PAK5, KIF16B, RALGAPA2, ATP9A -- every one (-)-strand and swamped by
forward/antisense reads that STARsolo strand-excludes. The user's directive: "the correct strand
(forward, reverse, both) should be an argument for the counter (default forward)").

**Problem:** D056's `--strand` filter and D057's `genefull_ex50pas` both apply a strand *policy* inside
the collapse/feature stage. D057's Ex50pAS rule dropped only a read that was 100%-exonic AND antisense
(`pathtally_ledger.hpp`: `if (intronic_bases == 0 && !forward) break;`), so antisense *intronic* reads
passed. STARsolo's `GeneFull_Ex50pAS` instead excludes ALL antisense body overlap (`intronicAS`
included). The gap is invisible on small genes (the MHC benchmark missed it -- HLA genes have little
intron) but on a genomic pangenome a large gene's body path collects the uniquely-mapped antisense
reads physically present across its introns. The strand fact was also discarded at the BAM boundary
(every record written forward, no strand tag), so a downstream counter could not re-apply the policy
even though it owns the rest of the STARsolo-faithful counting.

**Decision:** panCollapse *classifies* orientation (it already computes `target.forward` per candidate)
and *emits* it as a new `GD` BAM tag -- one `F`/`R` per `GX` gene, `;`-separated and parallel to `GX`
(a gene is `F` if any of the read's targets for it is forward, matching how the D056 `--strand` filter
collapses targets to genes) -- rather than filtering on it. The sense/antisense *policy* moves
downstream to the counter (`count_cr.py --strand forward|reverse|both`, default `forward` = STARsolo
sense-strand). panCollapse runs strand-agnostic (`--strand both`, the default) so both orientations
reach the BAM with their `GD`. This keeps panCollapse's per-read gene summary complete and lossless
(the `GX`-is-the-full-set principle, D054), concentrates STARsolo-faithful feature semantics in the one
validated component, and lets a single panCollapse run serve any strand policy (or a velocyto-style
consumer that *uses* strand rather than filtering on it).

**Implementation:** `BamWriter::write_record` gains a `gd` parameter and appends a `GD:Z:` tag; the
emission block builds `gd` in the same sorted-gene-set loop that builds `gx`, from a `gene -> (any
target forward)` map. `apply_count_mode` and the D056 `--strand` RAD-side filter are untouched
(`--strand` still filters the RAD if a caller wants; the recommended path is `--strand both` +
counter-side filtering). Purely additive: `score` mode and the RAD are byte-identical.

**Verification:** all 58 CTests pass (the `bam`/`strand`-label tag-verify oracle tolerates the added
tag). Downstream, `count_cr.py`'s strand filter is gated on the `GD` tag being present, so a STARsolo
genome BAM (no `GD`) is never filtered and count_cr's counts are unchanged: Gene bit-identical to
STARsolo (0 diff), GeneFull / GeneFull_Ex50pAS to 100.000% (r=1.0) modulo a pre-existing 2-UMI/~922k
MultiGeneUMI_CR multimapper-tie residual that predates and is independent of this change. On the vg
arm, `--strand both`
reproduces the pre-0.4.4 numbers exactly (Ex50pAS 1,078,434; gene 740,205), and `--strand forward`
collapses the antisense over-count -- PTPRT 26,432 → 157, per-gene Pearson 0.966 → 0.995, shared-space
UMI delta +12.4% → +1.55%.

**Rationale:** the same "panCollapse provides information, the counter applies policy" split D054/D058
established: strand is an objective per-read fact best computed where the graph alignment is (only
panCollapse has it), but a feature *policy* is best applied where the validated STARsolo-faithful logic
lives (the counter). Welding the two inside `apply_count_mode` is exactly what produced the
antisense-intronic gap; separating classify (upstream) from filter (downstream) removes that class of
bug and makes strand a counter knob.

### D060 — Per-transcript intron-touch classification for the ledger spliced/unspliced flags

**Decision source:** User (root-caused a Gene-mode over-count on an all-haplotype pangenome
benchmark to D057/0.4.5's per-gene score-tie rule: a read sitting in a reference transcript's
intron can still tie the read's top score via some *other* haplotype's or isoform's exon path, so
the gene was wrongly flagged `spliced` even though no transcript actually explains the read
exon-only).

**Problem:** Since 0.4.5, a gene was `spliced` iff *any* of its exon-transcript paths tied the
read's single top score, and `unspliced` iff its gene-body path tied top — two independent,
per-gene facts about which *paths* tied, not about whether any specific transcript's own splicing
is consistent with the read. On a pangenome with many haplotype/isoform transcript copies per gene,
an intron-sitting read can tie top score against a transcript copy it does not actually lie inside
the exons of, so the per-gene rule calls the gene `spliced` on a coincidental tie rather than on any
transcript actually explaining the read exon-only.

**Decision:** Classify **per transcript**, not per gene — and emit the classification per
transcript, not as a per-gene summary. At graph load, for every gene, compute each of its exon
transcripts' on-body exon node-id span (the `[first, last]` on-body node crossed by that
transcript's own exon path). At read time, within each compatible gene (compatibility itself
unchanged — still the genes whose body or an exon transcript ties the read's top score, D048), a
transcript that *spans* the read (the read's gene-body node range falls inside the transcript's
span) is `spliced` (`S`) if its own exon path also ties the top score (within `kIntronFlankBases`
slop — the velocyto-MIN_FLANK/STARsolo-minOverlapMinusOne-style alignment-slop guard), or
`unspliced` (`U`) otherwise. A new `TX` BAM tag carries the read's emitted compatible transcript ids
(`;`-separated, sorted); `GX` (gene), `GD` (the gene's orientation), and `GL` (`S`/`U`) become one
entry **per `TX` entry**, positionally parallel — not one summary per gene. `GX` therefore repeats a
gene once per each of its compatible transcripts; that redundancy is what lets a downstream counter
(`count_cr`) map transcript to gene and derive per-gene ambiguity without a side file. A gene with
both an `S` and a `U` transcript among its compatible set is velocyto's "ambiguous" (spliced for one
isoform, unspliced for another) — panCollapse emits each transcript's own call unlabeled and leaves
gene grouping, ambiguity resolution, the count-mode rule, and the sense/antisense policy to the
counter, continuing the D059 "panCollapse classifies, the counter decides" split.

**Not implemented (documented):** `GD` still summarizes one majority orientation per *gene*
(repeated across that gene's transcripts, not a separate sense/antisense call per transcript).
Splice-junction evidence was not used for classification when this decision was first recorded;
D061 closes that gap.

**Rationale:** intron touch is a fact about a specific transcript's own splicing relative to the
read; a same-score tie against an unrelated transcript copy is not. Testing it per transcript
(spanning + exon-only, both against precomputed on-body spans) keeps the O(nodes)-per-read cost
D057 established — the one added expense is the one-time load-time span precompute — while fixing
the over-count without reopening D048's top-score-tie compatibility rule, which decides gene
membership, not splicing state. Emitting the classification per transcript rather than collapsing
it to a per-gene pair keeps panCollapse's own output lossless (the D054 "the counter decides"
principle) — a gene's ambiguity is a fact about the *set* of its transcripts, which only the
counter, not panCollapse, has reason to collapse.

**Verification:** node-membership alone (this decision without D061's concordance gate) took the
chr20 Gene-mode over-count from the pre-fix 0.4.5 baseline (+83.2% vs STARsolo) to -13.2% — a large
improvement, but still biased low from the coincidental-tie cases D061 later closed. See D061's
verification for the combined, fully re-validated numbers (-2.76%).

### D061 — Splice-junction concordance gates the `S` (spliced) call

**Decision source:** User (verified case at the TUBB1/ATP5F1E tail-to-tail overlap: a read's
aligned bases sit contiguously on a TUBB1 haplotype isoform's exon path, tying the read's top score,
but the node-skip it actually makes — nodes 3582591-3582626 via edge 3582590<->3582627 — is
**ATP5F1E's** intron (0 TUBB1 owners, 27 ATP5F1E owners). D060's node-membership test called this
`S` for TUBB1; STARsolo gives it `GX:Z:-`).

**Problem:** D060's per-transcript `S` test is node-membership only: a transcript's exon path ties
the read's top score. It never checks that the read's *own* splice junction is one of that
transcript's introns. A read can land its bases on transcript A's exon while the splice it actually
makes belongs to a *different*, overlapping transcript B's intron — most visibly at tail-to-tail
gene overlaps on a pangenome with many haplotype/isoform copies — and D060 wrongly calls it `S` for
A.

**Decision:** Gate `S` (and, symmetrically, `U`) on splice-junction concordance, mirroring STAR's
`classifyAlign` (a spliced read's donor/acceptor must equal one of the transcript's own introns),
kept **strand-blind** exactly as STAR does — orientation stays entirely downstream in `count_cr`
(the `GD` tag + `--strand`). At graph load, alongside `transcript_spans`, precompute a global
undirected splice-edge map: for each exon-transcript path, a consecutive node pair that is NOT
adjacent on the gene body (the transcript's path skips real body sequence there — an intron) keys
an entry `(node_id, node_id) -> {owning transcript target ids}` (`SpliceEdgeMap`,
`pathtally_ledger.hpp`). Per read, `collect_read_node_pairs` (`main.cpp`) collects every consecutive
aligned node pair the read's own alignment crosses (within one subpath's mapping list, or across a
`next`/`connection` link to another subpath — both scanned identically, as plain node-id adjacency,
since in this graph a splice is an ordinary graph edge/node-skip, not a `vg::Connection` record).
`splice_concordant_transcripts` intersects, over every crossed edge that is in the splice-edge map,
that edge's owner set — the read's splice-concordant transcript set — or returns "unconstrained"
(every transcript concordant) if the read crosses no splice edge at all, so a single-exon or
exon-internal read classifies exactly as before this decision. `classify_ledger_group` then requires
a transcript to be in that set to be called `S`; the **same** gate applies to `U` — a transcript
that ties the exon score, or spans the read's gene-body range, but fails concordance is emitted as
**neither** `S` nor `U` (absent from `TX`), not spuriously downgraded to `U`, because a spliced read
is not also an unspliced (pre-mRNA) one, and letting a concordance-failed transcript fall through to
`U` would spuriously make its gene ambiguous and drop a legitimately spliced read from `Gene` mode.
For the TUBB1/ATP5F1E case: ATP5F1E is `S` (it owns the junction) with `GD=R` (antisense) → dropped
downstream by `--strand forward`; TUBB1 is not `S` (does not own the junction) → no `Gene` credit for
either gene, matching STAR's `GX:Z:-`.

**Not implemented (documented):** concordance is computed from raw node-pair adjacency, not from
`vg::Connection` records — in this graph a splice is an ordinary graph edge/node-skip, so
`Connection` records are not a reliable source of every crossed junction. `GD` still summarizes one
majority orientation per gene; a per-transcript sense/antisense report remains unimplemented (D060).

**Rationale:** node-membership is a fact about where a read's *bases* land; concordance is a fact
about where its *splice* lands. On a pangenome with many overlapping/tail-to-tail gene pairs and
haplotype copies, the two diverge often enough to matter — the coincidental-tie cases D060 alone
left uncorrected. Testing concordance as an undirected node-pair-ownership intersection keeps the
per-read cost proportional to the read's own splice count (typically zero or a handful), not to
graph size, and requires only a one-time load-time precompute alongside the existing
`transcript_spans` pass.

**Verification:** `tests/vg/fixtures/splice_concordance_smoke/` exercises the TUBB1/ATP5F1E case;
`tests/vg/fixtures/genefull_isoform_smoke/` checks the ordinary cross-isoform case (a read's splice
IS the isoform's own intron) still classifies `S` as before. Full chr20, 2,000,000-read benchmark
against STARsolo (forward strand, all four ledger modes): `Gene` 294,116 vs STARsolo 302,475
(-2.76%, per-gene Pearson 0.967, per-cell 0.986) — down from D060-alone's -13.2% and the pre-D060
0.4.5 baseline's +83.2%; `GeneFull` -3.33% (r 0.999); `GeneFull_ExonOverIntron` -2.02% (r 0.998);
`GeneFull_Ex50pAS` +0.67% (r 0.998). Every ledger mode is now within a few percent of STARsolo on a
whole chromosome, closing the large-gene/tail-to-tail over-count D060 alone left open.

### D062 — Optional t2g path aliases carry CAT collapse identity

**Decision source:** User (clarified that CAT maps reference genes and transcripts onto added
haplotypes, and that an unambiguous projected copy must collapse to its source reference
gene/transcript; genuinely novel genes, paralogs, and extra copies remain distinct).

**Problem:** D048 inferred transcript identity only by stripping a terminal `_H<n>` / `_R<n>`
from each graph path. CAT-projected transcript paths must remain raw and haplotype-unique while
`vg rna` builds the graph, so their names do not necessarily encode the source ENST. Rewriting
those raw names before graph construction can merge distinct transcript models, while leaving
them unchanged gives panCollapse no way to collapse an unambiguous projection afterward.

**Decision:** `--t2g` accepts either two columns
`graph_path<TAB>gene` or three columns
`graph_path<TAB>gene<TAB>canonical_transcript`. A two-column row behaves exactly as before:
panCollapse derives the target by stripping a terminal `_H<n>` / `_R<n>`. In a three-column
row, column 3 explicitly supplies the target transcript while column 1 remains the exact raw
graph path. Several raw paths may map to one canonical transcript. One raw path mapping to
different transcripts, or one canonical transcript mapping to different genes, is a hard
input error. Output `tx2gene.tsv` remains the ordinary canonical two-column transcript-to-gene
map. This is an extension of the t2g, not a separate runtime collapse manifest.

**Score-order rule:** score and select raw graph paths first. Only raw paths tied at the single
top score are then grouped by canonical transcript. Orientation evidence is combined among those
winning paths only. Scores from several raw paths mapped to one transcript are never summed
before winner selection, so additional haplotype copies cannot manufacture a higher transcript
score.

**Ledger rule:** every raw exon path resolves to the same canonical target id before the
per-transcript ledger is built. Its exon score remains the maximum across those raw paths, never
their sum; duplicate spans cannot duplicate the final call, and splice-edge ownership is set-deduped
through the canonical target id. The existing suffix-stripped bare-t2g fallback remains for
canonical targets introduced by two-column ledger rows; a three-column alias is exact and cannot
activate that fallback for an unlisted graph path.
Gene-body `--body-t2g` stays an ordinary path-to-gene map.

**Verification:** the pure PathTally test has two raw paths aliasing to `CANON_A` tied with a
third path aliasing to `CANON_B`; both targets must survive, which fails under pre-selection score
summing. The independent RAD oracle exercises the same case end to end. Two negative fixtures
exercise the path-to-multiple-transcripts and transcript-to-multiple-genes errors. A ledger
fixture embeds two raw exon paths for one transcript and verifies one canonical target in RAD/BAM
and `tx2gene.tsv`; its adversarial body-dominant read classifies `U` under MAX collapse but would
incorrectly classify `S` if the two raw exon scores were summed.

**Rationale:** raw graph identity and biological collapse identity are different namespaces.
Keeping both in one t2g row lets `vg rna` retain correct CAT models while panCollapse emits the
reference transcript identity only where the reference builder has established an unambiguous
mapping. The ordering rule preserves D048's best-HST evidence model regardless of how many
haplotype paths represent a transcript.

### D063 — Optional body aliases keep ledger evidence transcript-specific

**Decision source:** User (requested that transcripts remain the counting unit through S/U
classification and be pooled into genes only by count_cr; requested that CAT body copies and
fragments collapse to the same canonical transcript as their exon paths).

**Supersedes:** D062's final sentence only. `--body-t2g` may now use a three-column
`raw_body_path<TAB>gene<TAB>canonical_transcript` contract in addition to its unchanged legacy
two-column `body_path<TAB>gene` contract.

**Decision:** A body t2g is consistently two-column or consistently three-column; mixing widths
is a hard error. In three-column mode, every canonical body transcript must occur in `--t2g` and
must name the same gene there. Several raw body paths may map to one canonical transcript. Their
scores collapse by maximum, never sum, so haplotype copies or graph fragments cannot manufacture
extra evidence.

**Transcript-first ledger rule:** Exon and body scores are keyed by canonical transcript and share
one global top-score threshold. A near-top exon score calls that transcript `S`; otherwise its own
near-top body score calls it `U`. Another transcript of the same gene cannot inherit the body call.
D061 splice concordance gates both states. Splice ownership is precomputed by comparing each
canonical transcript's exon paths only with that transcript's body paths, rather than with a pooled
gene body. Body-path traversal order, not numeric node-id order, defines an internal step. For a
body split at haplotype breaks, endpoint presence across separate fragments also establishes
ownership, but any adjacent endpoint occurrence on any body path conservatively makes the edge
ordinary; this also resolves repeated-node/copy ambiguity. Fragments need not overlap because
column 3's canonical transcript is the authoritative union key. `GD` is likewise computed per transcript
in this mode. No gene identity participates in
the classifier; count_cr groups the emitted `TX` entries by `GX`, derives gene ambiguity, applies
the selected count mode, and resolves multi-gene UMIs.

**Output policy:** Existing RAD Unique behavior and BAM policy do not change. Therefore a
transcript-first production run must use `--bam-out ... --bam-multigene all` so multi-gene ledger
records survive into count_cr; the RAD continues to omit them. A two-column body t2g continues to
use the D060/D061 gene-body/span classifier and gene-level orientation for backward compatibility.

**Verification:** Pure classifier cases cover exon-over-body precedence, exact-transcript body
assignment, no cross-isoform inheritance, splice-concordance gating, and empty input. Integration
fixtures cover raw exon and raw body MAX-vs-sum adversaries, transcript-specific splice ownership
(including fragmented bodies, nonmonotonic node ids, reverse paths, and repeated-node adjacency),
canonical-transcript/gene validation, missing exon transcripts, and mixed-width rejection. The
pre-existing two-column suite remains the compatibility gate.

**Rationale:** CAT projection creates several raw exon/body graph paths for one biological
transcript. Collapsing those paths at the transcript boundary preserves that biological identity;
pooling bodies by gene earlier discards isoform information and can assign one isoform's unspliced
evidence or splice geometry to another.

### D064 — Versioned path identity ledger is the production runtime contract

**Decision source:** User (required explicit, lossless path/Parent/canonical identity across
panSC graph construction and panCollapse; prohibited heuristic suffix parsing in production).

**Supersedes:** D062 and D063 only for their runtime input format and identity-inference surface.
Their transcript-first exon/body semantics, splice rules, MAX-not-sum rule, and explicit legacy
behavior remain in force.

**Decision:** Production conversion selects `--path-identity-ledger` and accepts only the headered
schema `panSC-path-identity-v1`. Each emitted path row carries the complete annotation provenance:
`unique_parent`, separate `source_parent` and `input_parent`, canonical transcript/gene,
source/sample/haplotype/annotation fields, exon/body layer and selection/fallback state, explicit
body-to-exon Parent crosslink, source identities/class/coordinates/strand, and exact vg path name,
length, and haplotype origins. Historical exon/body t2gs are available only with the explicit
`--legacy-adapter hst-v1`; neither CLI route auto-detects or silently falls back to the other.

**Identity and score rule:** Production lookup is exact
`vg_path_name -> unique_parent -> canonical_transcript -> gene_id`. Paths MAX-collapse within a
Parent, Parents MAX-collapse within a canonical transcript, and downstream gene pooling occurs only
after transcript-level classification. All tied winning paths and Parents survive as provenance;
scores are never summed. Identifier strings are opaque, including literal `_H<n>`/`_R<n>` suffixes.

**Validation:** The reader requires the exact canonical 24-column header/order and nonempty,
TSV-safe fields; numeric positive
coordinates/path lengths; `start <= end`; unique exact path names; one annotation identity and one
feature layer per Parent; canonical transcript to one gene; disjoint exon/body Parents; exon
self-links; and body links to an existing exon Parent with matching canonical transcript/gene.
Missing-source fallback canonicalizes to the exon Parent (`unique_parent` for exon rows,
`exon_unique_parent` for body rows). At runtime every ledger path must occur exactly in the XG with
the recorded length; extra non-ledger genomic paths are allowed. Comma/semicolon are rejected only
where they would corrupt BAM provenance grouping (`vg_path_name`/`unique_parent`; semicolon also for
`canonical_transcript`/`gene_id`); general provenance, including comma-separated
`vg_haplotype_origins`, is not subject to BAM delimiters.

**BAM provenance:** Production BAM records carry canonical `TX` and parallel `XP`/`XU` Z tags.
Groups are semicolon-separated in `TX` order; tied exact paths/Parents within a group are
comma-sorted. Count-mode `XP`/`XU` describe the exon/body layer that supplied the `S`/`U` call.
Production score-mode BAM also carries these tags, while its `GX`/`GD` stay gene-level. The
`hst-v1` adapter emits no new score-mode tags and retains its prior tested layout.

**Verification:** The strict reader unit fixture covers required fields/schema, Parent and
canonical conflicts, layer overlap, body crosslinks, fallback, coordinates, and reserved versus
general-provenance delimiters. Pure selection tests cover multiple paths per Parent, multiple
Parents per canonical, literal `_R1`, and MAX-not-sum tied provenance. End-to-end score/count
fixtures use sequence-identical paths with distinct canonical identities and assert exact
`TX`/`XP`/`XU`, including literal `_R1`, `panSCup1_<64 lowercase hex>`, and
`panSCbody1_<64 lowercase hex>` Parents; missing XG paths and path-length mismatches hard-fail. The full pre-existing suite
runs through explicit `--legacy-adapter hst-v1`.

### D065 — Preserve barcode quality and the all-read correction population

**Decision source:** User (barcode handling must match the intended STARsolo/Cell Ranger boundary;
runtime gene-ID normalization is out of scope and Gene any-S reduction is already downstream).

**Decision:** The supported RNA carry-along name is
`<original_name>_<raw_CB>_<raw_UMI>_cy<hex(CY)>_uy<hex(UY)>`, where `CY` and `UY`
are the raw R1 cell-barcode and UMI qualities encoded as ASCII hex. Hex avoids whitespace
and delimiter ambiguity in FASTQ/GAMP names. Legacy quality-free and CY-only suffixes
remain accepted and produce only the qualities they carry.

When `--bam-out` is active, panCollapse writes exactly one BAM record per valid raw-molecule read
group. A feature-bearing group keeps the nominal mapped ledger record and gains `CY`/`UY` when
available. Any group without a BAM count feature writes one unmapped `XB:Z:barcode_only` record
with `CB`/`CR`, `UB`/`UR`, optional `CY`/`UY`, and no feature tags. This includes unaligned,
no-compatible, and policy-omitted groups.

**Consumer boundary:** barcode correction includes all primary records when tallying exact
whitelist abundance, then removes `XB:Z:barcode_only` records from its output. count_cr also
recognizes and skips the tag defensively when correction is disabled. Thus barcode-only records
can change correction posteriors but cannot become gene evidence.

**Verification:** `genefull_spliced_drops_intron` asserts one feature plus one barcode-only BAM
record, decoded `CY`/`UY`, unmapped/no-`GX` layout, and summary conservation. The multigene-omit fixture
asserts the omitted read survives only as barcode evidence; the `all` fixture still carries its
complete feature ledger.

### D066 — Emit exact, Parent-preserving STARsolo GeneFull_Ex50pAS evidence

**Decision source:** User (required exact STARsolo 2.7.11b overlap tiers and prohibited collapsing
distinct locus Parents before the downstream six-rank selection).

**Supersedes:** D057/D060/D063 only for production `genefull_ex50pas` BAM evidence. Their S/U
classifier remains the ordinary ledger-mode and RAD compatibility path. D064's exact identity
ledger remains authoritative, but its path -> Parent -> canonical MAX collapse does not apply
before Ex50pAS tier selection.

**Decision:** Production `--count-mode genefull_ex50pas` requires
`--path-identity-ledger`, `--bam-out`, and `--bam-multigene all`. The `hst-v1` adapter is rejected
because t2g/body-t2g rows cannot establish the required exact exon/body path and Parent pair.

For each complete MultipathAlignment traversal contained by a ledger body path, compare its
reference-aligned bases with the exact exon path linked through `exon_unique_parent`. Following
STARsolo 2.7.11b `Transcriptome_alignExonOverlap.cpp`, emit E when all bases are exonic and every
splice junction is concordant; otherwise P when strictly more than half are exonic; otherwise B.
Exactly half is B. Fully exonic but splice-discordant is P. Target-relative orientation expands
these to the global priority E-sense, E-AS, P-sense, P-AS, B-sense, B-AS; count_cr selects the
first nonempty rank, and an antisense winning rank yields no count.

**BAM contract:** `TX`/`GX`/`GD`/`GT`/`XP`/`XU` are semicolon-parallel and `GL` is absent.
Every evidence slot carries exactly one path and its exact Parent: E/P use the exon path/Parent and
B uses its linked body path/Parent. Canonical `TX` may repeat across distinct exon locus Parents,
paths, tiers, or alignment alternatives. Every slot must resolve through the ledger to the same
canonical `TX` and gene `GX`; the Parent derived from `XP` must equal `XU`. No canonical- or
gene-level reduction occurs until count_cr applies priority and UMI resolution.

**Execution boundary:** Model-bound states propagate through the MultipathAlignment DAG, including
ordinary `next` and `connection` arcs, without enumerating complete traversals. States track exact
body coordinates, aligned-base total, exon overlap, junction concordance, and orientation.
Repeated-node or body/exon geometry that cannot assign an exact occurrence/orientation is a hard
error rather than guessed evidence.

**Verification:** The exact-tier fixture covers E; P; the exact 50% B boundary; fully exonic,
splice-discordant P; reverse orientation; two distinct locus Parents sharing one canonical
transcript/gene; and a branching MultipathAlignment that retains several tier entries. Its verifier
round-trips every parallel slot through the ledger. A separate assertion rejects legacy Ex50pAS,
and exact versus ordinary ledger conversion produces byte-identical RAD. D068 extends the fixture
to nine non-homopolymer UMIs with minimum pairwise Hamming distance six; downstream exact selection
should count eight `GENE` UMIs and reject the reverse E-AS read.

### D067 — Emit one superset counting BAM plus an opt-in audit-debug payload

**Decision source:** User, after reviewing the completed/interrupted chr20 seam
work on 2026-07-28.

**Supersedes:** D057/D060/D066 only where they require separate ordinary and
exact-Ex50 BAM conversions or make `GL` and `GT` mutually exclusive. Their
classification, exact-tier, Parent-preservation, strict-ledger, RAD, and
barcode-only contracts remain active.

**Decision:** PanCollapse is the GAMP-to-counting-BAM information producer.
One normal ledger-backed conversion must emit common `TX`/`GX`/`GD`, Gene
`GL`, exact-Ex50 `GT`/`XP`/`XU`, and `CB`/`UB`/`CY`/`UY` evidence needed by
both downstream count modes. A consumer requires its selected mode's fields
and permits extra fields. Producing separate mode BAMs is an interim
implementation, not the final production architecture.

Add an explicit debug mode that also emits the instrumented splice-edge count
and the exact-top candidate set before flank filtering. Normal mode remains
compact, and enabling debug evidence must not change any production
classification or RAD bytes.

**Implementation status (2026-07-28):** production ledger count-mode BAMs carry
the exact `@CO` schema marker plus parallel `XR/TX/GX/GD/GL/GT/XP/XU` rows,
using literal `.` for the inactive family. `--debug-evidence-out` writes
normalized `panCollapse-debug-evidence-v1` read and candidate rows atomically.
Focused tests compare debug versus normal BAM records and RAD bytes. Producing
the typed union requires at least one linked body row for every exon Parent;
general score-mode and non-BAM conversions remain less restrictive.

**Cross-repository contract:** source-gene normalization and UMI ordering remain
owned by panSC `count_cr.py`, not PanCollapse. The complete implementation,
workflow, audit, and QC gates are fixed in
`/mnt/ssd/lalli/panSC/docs/pancollapse_countcr_superset_plan.md`.

**Validation status (2026-07-29):** the D067 cross-repository plan completed.
The real chr20 generation corrected one 1,998,188-record typed-union BAM once
and consumed its 1,863,531-record corrected subset in both Gene and exact Ex50.
The source-ordered 3,996,376-row read-by-mode audit conserved the declared
universe and byte-replayed both production matrices; the checksum-bound
STARsolo comparison report regenerated byte-identically. PanCollapse passes
105/105 CTests as 0.7.0. Exact roots, hashes, the remaining chr20-exploratory
inference boundary, and cross-repository checks are recorded in the linked
panSC plan. The validated Docker image remains a local tag, not a published
registry artifact.

### D068 — Score-window exact Ex50 candidates before six-rank feature selection

**Decision source:** User, 2026-08-02, after identifying that equal `GT`/`GD` categories do not
establish equal alignment scores.

**Supersedes:** D066 only for eligibility of exact Ex50 BAM evidence. D066's Parent preservation,
body/exon geometry, splice-concordance, E/P/B definitions, orientation, and downstream six-rank
selection remain active. D067's typed-union, debug-neutrality, ordinary Gene, and RAD contracts are
unchanged.

**Decision:** Score every complete transcript-compatible traversal by summing the stored GAMP
subpath scores and any scored `connection` transitions along that traversal. For identical
Parent/path/tier/orientation evidence, retain its best score across DAG alternatives and all
MultipathAlignment records in the read group. Let `S` be the largest score among all compatible
exact evidence for the group. Retain evidence with score `>= S - 5`, inclusive, and discard evidence
below that window before `count_cr.py` applies the global E/P/B-orientation priority.

Typed-union BAM headers carry `@CO panCollapse-ex50-score-window:5` so this changed selection
semantics is distinguishable from pre-D068 BAMs that share the v1 field schema.

The five-point window is the approved one-mismatch fudge factor: with vg's default `+1` match and
`-4` mismatch scores, replacing one match by one mismatch lowers an otherwise identical alignment
by five. The comparison is made on complete model-constrained traversals, not individual subpaths,
MAPQ, or the legacy node-to-reference path tally. Distinct exact Parents remain distinct when both
survive the score window.

**Verification:** The exact-tier integration fixture contains otherwise-identical alternatives at
top-minus-five and top-minus-six both as branches inside one MultipathAlignment (including scored
connections) and as separate records in one read group. The five-point alternatives retain all
expected exact Parent/path rows; the six-point alternatives do not. Focused exact-Ex50 BAM,
debug-neutrality, and byte-identical RAD tests pass. The full current suite passes 105/105 tests.

The bounded real HG002 chr20 qualification targets the 445,665 reads STARsolo assigned but the
pre-D068 graph run rejected at direct Unique. D068 assigns 5,306 (1.190580%), leaves 440,305
(98.797303%) direct-multigene, and strand-vetoes 54 (0.012117%). Among residual exactly-two-gene
reads, no pair shares a curated HGNC identity and no pair has identical complete retained-exon
sequence sets. This establishes that the approved score ordering is operational but explains only
a small fraction of the observed loss; most remaining ambiguity is between distinct biological
count identities. The bounded artifact is not promoted production evidence because its process
started before the marker-only rebuild and its BAM therefore lacks the D068 header comment. Exact
results and checksums are recorded in
`/mnt/ssd/lalli/hg002_10x5p_q100_chr20_v1/ex50_read_loss_crosstab_v1_20260802T110935-0500/pair_analysis_v1/score_window5_v1/REPORT.md`.

### D069 — Promote D068 as an opt-out v0.8.0 feature

**Decision source:** User, 2026-08-02, after reviewing the bounded retained-read improvement.

**Supersedes:** D068 only where it made the five-point eligibility rule unconditional. D068's
default score definition, inclusive boundary, traversal scoring, Ex50 ordering, and biological
qualification remain active.

**Decision:** panCollapse v0.8.0 enables the inclusive five-point exact-Ex50 score window by
default. `--no-ex50-score-window` disables score pruning entirely and restores the pre-D068
all-compatible exact-evidence surface before downstream E/P/B-orientation selection. Disabled is
not equivalent to a zero-point window: zero would retain exact-top ties only. The opt-out is
accepted only where a production-ledger count-mode BAM actually carries exact Ex50 evidence;
otherwise it fails instead of becoming a silent no-op. Ordinary Gene evidence and RAD are
unchanged under either policy.

Typed-union BAMs record `@CO panCollapse-ex50-score-window:5` under the default or
`@CO panCollapse-ex50-score-window:disabled` under the opt-out. `summary.tsv` records the same
state as `exact_ex50_score_window` and uses `not_applicable` when no exact evidence is produced.

**Verification:** The exact-tier fixture runs in both modes. Default output retains top-minus-five
and excludes top-minus-six alternatives; disabled output restores the top-minus-six alternatives
both within one MultipathAlignment DAG (including a scored connection) and across records in one
read group. The verifier rejects the opposite header marker, invalid-scope opt-out use fails, and
default/disabled RAD files compare byte-identically.

### D070 — Keep compact exact-count output explicit, justified, and conspicuous

**Decision source:** User, 2026-08-21.

**Decision:** The normal information-complete typed-union BAM remains the production default.
`--compact-exact-count-bam` is an explicit, lossy research opt-in and must never be enabled
by default in the CLI, pipeline, configuration, or ordinary example recipe. Before a run
uses it, the plan must record a concrete case-specific reason: the constraint being
addressed, why the normal BAM is unsuitable for that run, and the evidence lost when the
producer applies score/Parent filtering and global winner selection. The run update and
result must prominently label compact mode as compact, lossy, and experimental.

**Current implementation:** The option is already default-off: its state is unset until the
flag is parsed, and the production panSC module/configuration does not pass it. This decision
adds a usage and reporting gate; it does not promote the compact format or change the normal
runtime path.

**Scope:** Existing terminal HG002 compact experiments remain bounded research evidence.
They do not become production validation, and their prior existence is not a reason for
future use.

### D071 — Compact necessity and authorization remain a user decision

**Decision source:** User, 2026-08-21.

**Decision:** Only the user determines whether `--compact-exact-count-bam` is necessary and
authorizes its use. It is currently not necessary, including for the next chr20 k32
pangenome-loss diagnostic. Agents must not substitute convenience, output size, an existing
runner, or prior compact artifacts for that determination. The normal typed-union BAM is
required unless the user later makes a new explicit decision.

**Scope:** This strengthens D070's use gate. It does not remove the compact implementation or
its tests, and it does not change the notice/evidence-loss requirements that would apply to a
future user-authorized run.

### D072 — Precompute degraded-target membership during exact geometry initialization

**Decision source:** Engineering correction, 2026-09-03. No biological or output-policy choice is
changed.

**Problem:** v0.8.1 identifies the small set of transcript-body Parents whose repeated-node
geometry cannot be resolved, but `record_geometry` then rescans the complete exon-path-to-target
map to answer whether each canonical target contains one of those Parents. On the joint chr20-22
graph this turns a yes/no membership query into an accidental quadratic pass over 1,230,107 exon
paths. The producer remained CPU-active before opening GAMP or output files for more than 25 hours.

**Decision:** While Parent-level occurrence resolution is already identifying an unresolvable
Parent, insert its canonical target ID into an in-memory set. The geometry worker uses one lookup
in that set. Parent-level degradation, clean-sibling precedence, exact occurrence/orientation,
splice ownership, score-window behavior, RAD bytes, BAM evidence, and all identifiers remain
unchanged. No persistent custom index is introduced.

**Verification:** The v0.8.2 source passes 119/119 CTests. The joint chr20-22 preload reaches the
same frozen `evaluated_target_edges=141543588` boundary in 73 minutes, versus a nonterminal v0.8.1
pass after more than 25 hours. It reports 12 degraded body paths from 13 directed-cycle scan
candidates because one repeated path has a uniquely resolvable oriented occurrence; this is the
existing v0.8.1 semantic distinction, not an optimization effect. The preload was terminated after
the geometry marker, and none of its partial output is benchmark evidence.

### D073 — v0.9.0 reopens deterministic PanCollapse multithreading

**Decision source:** User, 2026-09-03.

**Decision:** The earlier D045 deferral is superseded for PanCollapse v0.9.0. Add an explicit
`--threads N` complete-read-group worker surface. One parser remains the sole owner of binary GAMP
parsing, complete-name grouping, and the exact closed-name set. Workers receive only complete
groups and private scratch state. One ordered turn emits RAD, BAM, debug evidence, and warnings
strictly by input-group ordinal through a bounded in-flight window. Read-group counters accumulate
atomically; writer-owned record counts and histograms remain inside the ordered turn. The producer
queues at most two complete groups per worker, with at most one active group per worker, and creates
no per-group spill files.

Thread count is operational rather than scientific configuration. It is omitted from BAM `@PG CL`,
and all persisted artifacts must be byte-identical at supported thread counts. Normal typed-union
BAM evidence remains required; this decision does not authorize compact output, evidence loss,
chromosome/process sharding, a persistent custom index, or interruption of an active producer.

**Performance scope:** v0.9.0 also removes unused legacy work from transcript-specific conversion,
retains repeated-body occurrence maps only where repeated geometry requires them, and consolidates
future-equivalent exact states while retaining their maximum score. All changes return to the
existing biological fixtures plus cross-thread byte-identity and bounded-queue gates.

**Bounded evidence:** On the adversarial 2,000-model, 5,000-group exact fixture recorded in
`docs/research/v090-deterministic-parallelism.md`, median wall time was 24.86 s for v0.8.2,
19.88 s for v0.9.0 with one worker, 2.68 s with eight, and 1.83 s with sixteen. This establishes
the targeted hot-path and threading gains, but is not a whole-pangenome projection.

### D074 — Parallelize exact initialization without eager cache materialization

**Decision source:** User, 2026-09-03 (asked to assign workers to blocks of genes while building
the initialization cache).

**Decision:** `--threads N` also governs the two expensive initialization barriers. Exact model
construction is divided into adaptive contiguous blocks of lexically ordered exon Parents, with a
maximum block size of 32 and a desired scheduling granularity of four blocks per requested worker
(integer rounding generally yields about two to four). After every Parent result is committed,
splice geometry is divided by canonical target in production or gene in the legacy compatibility
path. The Parent barrier must complete before splice ownership begins, because target geometry
depends on the complete degraded-Parent set.

Workers read the already-deserialized XG and frozen ledger/catalog maps but return private models,
degradation records, edges, spans, and counters. One coordinator reduces them in Parent or numeric
target/gene order through at most `2 * effective_workers` in-flight blocks. Model IDs therefore
retain their v0.8.2 lexical Parent/exon/body order, errors replay from the lowest failing block, and
an initialization error occurs before an output directory is committed. This bounds temporary
parallel result storage by worker count rather than total gene count.

The score-node, ledger-node, and exact-model-candidate caches remain lazy. Each uses 256 shards with
a separate read/write lock and stores each completed value in an independently allocated immutable
object, so unrelated misses can build concurrently and references survive map rehashing. Exact exon
edges were initially handled the same way, but D075 replaces that hot shared cache with immutable
Parent-local geometry. This design avoids an eager whole-XG cache and adds no cache file or
external-sort I/O. Concurrent XG path access is supported by inspection and Helgrind testing of the
pinned implementation, not by an upstream portable thread-safety guarantee; changing the VG/XG
revision requires a new audit.

**Bounded evidence:** The 512-Parent fixture in
`docs/research/v090-deterministic-parallelism.md` has 32 exon and 32 body paths per Parent (524,288
exact models). Median Parent construction fell from 4.048 s at one worker to 0.116 s at 64 (34.8x),
and total initialization fell from 4.848 s at one worker to 0.931 s at 32 (5.21x). The 32-worker
candidate completed in 1.26 s versus 6.51 s for v0.8.2, with identical RAD, summary, and tx2gene
bytes. This synthetic result is not a chr20-22 or whole-pangenome runtime/RAM guarantee.

### D075 — Remove production hot-path strings and exact-edge contention without merging evidence

**Decision source:** User, 2026-09-03 (requested additional PanCollapse runtime improvements for
v0.9.0).

**Decision:** Keep XG path handles numeric through production tally and Parent/target collapse.
Resolve names and lexical output ranks through immutable ledger-owned rows only at the evidence
boundary. Use one layer-qualified Parent table, preserve the existing path-to-Parent-to-target MAX
hierarchy, and keep tied evidence identities distinct and lexically ordered. Build exon/body step
and occurrence geometry once per Parent. Exact models with identical ordered exon steps may share
an immutable splice-edge set, but retain separate path, Parent, target, orientation, score, and
terminal evidence identities. Repeated-body position maps remain model-local where occurrence
resolution requires them.

Debug-sidecar serialization begins only after read-local exact DP, so enabling diagnostics does not
serialize worker computation. `PANCOLLAPSE_PROFILE_TIMING=1` reports queue, worker-compute,
ordered-wait, and output-region timing to stderr without changing persisted artifacts. When an exact
surface has fewer than 32 models, a request for multiple processing workers automatically uses one
active worker to avoid synchronization regressions; initialization may still use the requested
workers. Both requested and active processing counts are reported.

**Non-decisions:** v0.9.0 does not merge distinct exact models merely because their geometry is
equal; broader cross-model DP sharing remains deferred until it has randomized differential and
representative biological-slice evidence. HTSlib/BGZF output threading is not enabled: bounded
profiling found the ordered BAM/RAD region negligible on the adversarial compute fixture, while
changing compression concurrency could weaken byte determinism.

**Verification:** On the 2,000-model/5,000-group fixture, final median wall time was 3.76 s with one
worker, 0.52 s with eight, 0.33 s with sixteen, and 0.25 s with thirty-two, versus 24.86 s for
v0.8.2. The generated reversed-order 64-Parent fixture matched a clean `5769f11` baseline
recursively at final one and eight workers, including BAM, RAD, summary, tx2gene, and debug evidence;
cyclic degradation and repeated-resolvable occurrence fixtures also matched. These synthetic
measurements validate the optimization and deterministic evidence contract, not whole-pangenome
runtime or memory.

### D076 — Add native, profile-versioned gene counting without a BAM boundary

**Decision source:** User, 2026-09-04 (requested implementation of the approved v0.10.0 native
direct-count plan and explicitly rejected an nf-core/Nextflow-owned implementation).

**Decision:** Add `panCollapse count` as an additive standalone command; preserve `convert`
unchanged for legacy/debug RAD and BAM workflows. Native count consumes name-grouped GAMP, its
matching XG, a barcode whitelist, and one checksum-bound `panSC-count-facts-v1` bundle. The bundle
is the only assignment authority and binds exact path identity, profile-specific gene policy,
Parent categories, projected nesting, independent support, gene metadata, producer receipts, and
source hashes. Optional historical exon/body t2gs are assertions against the bundle, never live
assignment inputs.

The initially immutable profiles are `cr7-v1` and `pansc-strict-v1`; either or both may run in one
GAMP pass. Repeatable typed assignment-policy overrides require explicit
`sensitivity-analysis` scope and always create a derived profile ID bound to the full effective
policy SHA-256. Frozen IDs are never edited in place, and barcode/UMI algorithms are not
overridable in v0.10. `cr7-v1` is described as CR7 emulation on PanCollapse alignments, not full
Cell Ranger equivalence.

The default product is atomically published Parquet dictionaries, counts, and molecules plus
`manifest.json` and stable `summary.tsv`. Raw 10x MEX, the existing pre-count RAD, and ordered
per-read diagnostics are explicit optional sinks. Native count has no BAM sink and no Python
runtime dependency. The implementation resides entirely in the PanCollapse repository; panSC
only compiles the annotation-side count-fact bundle. No Nextflow or nf-core route is added by this
decision.

Workers resolve CB-independent numeric evidence and aggregate observations without preserving
unnecessary read order. A bounded sharded signature cache reuses repeated profile-resolution
results. Exact-whitelist observations aggregate immediately; only potentially correctable raw
barcodes are deferred. Memory-pressure handling writes deterministic checksum-protected,
Zstandard-compressed sorted runs, including a secondary exact path for a pathological single
barcode. Canonically sorted decoded value rows and per-profile library-independent logical
hashes—not volatile manifest bytes or output-local dictionary indexes—are invariant to thread
count, scheduling, spill boundaries, and joint-versus-separate profile execution.

**Release boundary:** Hermetic parity, spill, threading, output-schema, optional-RAD, and container
gates are required before release. The scientific gates remain the frozen one-million-read
subset, full chr20 and chr21, then immutable joint chr20–22 without remapping. The final joint
requirements are exact oracle matrices/counters, at most 8.4 GB default persisted output, at
least twofold improvement over the matched 7 h 48 m v0.9 producer, under one day, and below
600 GB peak RSS. A resource or parity failure may change implementation, not the frozen biological
rules.

### D077 — Preserve barcode-correction and UMI-filter stage order explicitly

**Decision source:** Frozen `correct_cb.py` and `count_cr.py` parity audit during v0.10
implementation, 2026-09-04.

**Decision:** Model two independent per-profile gates on every resolved read:
`barcode_correction_eligible` and `reaches_umi_filter`. Every valid GAMP group updates the global
exact-whitelist barcode prior, even when featureless. A profile result may then exit before UMI
inspection, reach UMI inspection with zero genes, or produce one/multiple genes. Those states must
not be inferred from the final gene vector or terminal label alone.

For exact-whitelist reads, post-barcode counters are applied immediately. For a one-mismatch raw
barcode, the compact deferred key carries its post-correction disposition; assigned-read,
multi-gene, and UMI-drop counters are applied only if EOF posterior correction succeeds. A barcode
that fails correction and a Parent/category policy exit therefore cannot acquire an N/homopolymer
UMI terminal, while a successfully corrected read that survives evidence resolution but has zero
genes does reach the UMI filter. Global counters are sums over explicit per-profile counters where
the event is profile-specific. Barcode correction and off-whitelist rejection instead occur once
before the profile fork, so profile zero owns each pass-global increment while every profile keeps
its matching audit value. Spill encoding versions the disposition and validates its domain, so
memory pressure cannot alter stage semantics.

### D078 — Align native count semantics with the frozen rules after code review

**Decision source:** Code review of the v0.10.0 native count commit, 2026-09-04; fixes applied
with the user's approval of the review findings.

**Decision:** Five review findings changed settled behavior and are recorded here so they are
not reopened. (1) The runtime MultiGeneUMI_CR raw guard now sees every feature's pre-correction
read count for the winning UMI sequence, including features where that sequence was 1MM-relabeled
into a neighbor; the runtime carries such sequences as zero-support shadow candidates so the rule
survives the per-barcode spill. This matches STARsolo, `count_cr.py`, D058, and the in-repo
reference `count_observations`; production previously kept molecules the frozen rule discards.
(2) The count-fact bundle `content_id` is defined over the canonical serialization of `content`
(keys sorted by code point, no whitespace, ASCII-escaped strings, shortest round-trip floats),
byte-identical to Python's `json.dumps(content, sort_keys=True, separators=(",", ":"))`; on-disk
key order and whitespace no longer matter, and the loader implements that form directly.
(3) `novel-paralog-policy=lump` makes the origin gene both the identity and the equivalence key,
and `separate` keeps a paralog under its own equivalence key, so the policy is effective under
either bundle convention. Explicit `equivalence_gene` targets remain closed over the policy;
Parent identities or novel-paralog origins absent from it receive the frozen oracle's conservative
`UNKNOWN/missing_from_ledger` row. Production candidates take competition, gene type, and nesting policy
from the canonical equivalence row after novel-paralog identity resolution, matching the frozen
Python oracle even when only an alias occurs on a read. (4) Under a `strand=both` override the
winning strand is library sense whenever any best-tier candidate carries it, and the
equivalence-group evidence representative is the member with the most body support, then the
lexicographically smallest gene; both rules are independent of evidence order so cached and
uncached resolutions agree. (5) `count` parses
molecule identity exactly as `convert` does (malformed quality suffixes are counted and fatal
under `--molecule-identity-failures fail`), folds raw barcode and UMI case before the packed
runtime, rejects convert-only options (`--strand`, `--count-mode`, `--bam-multigene`,
`--no-ex50-score-window`), canonicalizes `--out-dir`, and canonicalizes profile selectors before
the repeat check. (6) The same commit also restored the frozen tagged-last-resort contract inside
the post-resolution category retry: when the retry filter empties the candidate set and the
profile tags last-resort rows, the unfiltered rows are retried so an excluded antisense winner
stays visible as a reverse winner and exact-strand Gene fallback can replace it with a sense
G-row. This changes `pansc-strict-v1` assignments (its retry categories are non-empty);
`cr7-v1` has no post-resolution retry categories and is unaffected. The frozen profiles' outputs
on the ex50 fixture are unchanged.

**Consequences:** The standalone release image remains free of workflow-engine-specific packages.
The CLI suite forces the worker pool through `PANCOLLAPSE_FORCE_WORKER_POOL` so the thread and
spool-only runs exercise four workers;
spill runs are removed by the runtime destructor and a caller-created spill directory is left in
place. The scientific gates in D076 must be rerun against the corrected raw guard before release.

### D079 — Persist a tiered read-compatibility intermediate before count policy

**Decision source:** User, 2026-09-05 (BAM was inefficient; requested an intermediate holding the
same tiered read-compatibility information).

**Supersedes:** D076–D078 only where they imply that the final `AssignmentFacts`/count output or
its diagnostic sidecar is the reusable per-read authority. Their count-fact bundle, profile
semantics, barcode/UMI rules, aggregate outputs, and release gates remain in force.

**Decision:** `panCollapse count` may write `--compatibility-out <dir>`, a deterministic,
non-BAM Parquet dataset plus checksum-bound manifest. It records policy-neutral evidence before
`CountFactCatalog::candidate`: every input read group's molecule identity and status, its
interned `fact_set_id`, and its exact E/P/B and structural S/U evidence, from which current Gene
fallback can be reconstructed. Evidence
retains the exact path, Parent, and canonical-transcript provenance required for replay. Invalid
raw-molecule and featureless groups are retained, so the artifact has the full input denominator.
`fact_set_id` resolves quickly to a canonical interned fact set, while a per-read pass supplies
the matrix and QC denominator. Publication requires the producer's independently accumulated
read-group count to equal the read-table rows; valid, missing, malformed, and unsupported totals
are also checksum-bound and revalidated.

Replay uses `count --compatibility-in <dir>` with the current count-facts bundle and whitelist;
it re-evaluates current profiles without GAMP/XG. Exactly one source is required: either
GAMP with matching XG, or compatibility input. The artifact is lossless for metadata, tagging,
whitelist, and count-policy changes that do not change transcript/exon structure. It is not a
GAMP replacement: the original GAMP/XG digests are frozen as provenance, while replay validates
the current ledger structural surface but does not reopen XG. Changed graph topology,
transcript/exon structure, or compatibility algorithm requires regenerated GAMP-derived evidence.

The intermediate deliberately contains no alignments, sequences, CIGARs, or duplicated GAMP/BAM
payload. It is therefore not BAM and avoids alignment duplication while retaining the tiered
compatibility facts needed by counting.

**Acceptance:** byte and logical determinism must hold across supported threads and spill modes;
the manifest/file hashes must cover schema, facts, rows, and inputs; every input group must be
accounted for; GAMP-to-intermediate-to-count must equal direct count; corruption, schema/source
mismatch, and incompatible structure must fail; profile re-evaluation must be demonstrated; and
a bounded storage/runtime benchmark must compare direct count, compatibility production, and
replay against the v0.9 BAM baseline.

## Architecture questions and Phase 0 resolution map

The historical questions below were external-contract facts to resolve from current
primary sources or installed source. They do not authorize reopening settled product
behavior.

### External-contract research

These must be resolved before **Gate Architecture Approved**; D023-D034 propose the
current gate-review resolutions.

#### F001 — Minimal existing VG/index input set

Determine the smallest set of already-produced graph, path, coordinate, GBWT, locate, or
related files that supports correct interpretation of GAMP and annotation coordinates.
Do not add a panCollapse-specific persistent index.

**D094 extension (2026-09-26):** the approved alternatives use GBZ or XG+GBWT, with standard
FastLocate and selected path coordinates built in memory. No additional persistent index is
introduced. The implementation and named scientific acceptance are complete.

#### F002 — Complete GAMP traversal semantics

Determine whether complete candidate enumeration follows ordinary `next` edges, scored
`connection` arcs, or both; define score composition and what constitutes a complete
candidate traversal.

#### F003 — GAMP output grouping guarantee

Determine whether the supported `vg mpmap` version keeps all records for one read
contiguous under multithreaded operation. If not guaranteed, identify a supported
name-grouping or sorting command/API that does not require a BAM intermediate.

#### F004 — RAD writer and schema strategy

Verify the current mapper-style uncollated RAD schema expected by alevin-fry and choose
between a maintained writer implementation and a native writer. The choice must include
an interoperability test and dependency-cost analysis.

#### F005 — GTF-to-graph lookup without a custom index

Propose ordinary in-memory GTF structures and existing VG path/position queries that
implement the compatibility contract. Estimate complexity and define measurements that
would justify revisiting the no-custom-index decision later.

#### F006 — VG dependency and ABI boundary

Define how a standalone build discovers VG and its transitive dependencies, which VG
versions or commits are supported, and how an incompatible installation fails clearly.

#### F007 — GAMP annotation access and override syntax

Verify how string annotations are represented and accessed in current VG APIs. Propose
explicit CLI overrides for nonstandard barcode and UMI annotation names while preserving
corrected/raw pair auto-detection by default.

Historical note: D038 supersedes this barcode/UMI source question for V1. GAMP annotation
access remains useful only for non-barcode metadata or future approved extensions.

### D080 — Attribute compatibility replay wall time before optimizing it

**Decision source:** Measurement, 2026-09-11, on the accepted one-million-read fixture.

**Context:** The replay speed proposal assumed the streaming read loop was the bottleneck,
because `count --compatibility-in` reports `active_workers=1`. That report is accurate but the
loop it describes was never measured against the rest of the invocation.

**Decision:** `run_compatibility_replay` now emits the same `initialization: phase=` timing lines
the direct count path already emitted, covering every stage between invocation and the first read
batch. Optimization targets are chosen from that attribution, not from the worker report.

**Measurement:** replaying the accepted bundle at 756.71 s wall and 63,049,972 KiB peak RSS
attributes initialization as `fact_set_resolve` 519.57 s (68.7 percent of wall), `compatibility_open`
127.93 s (16.9 percent), `count_fact_catalog` 20.77 s, `identity_ledger_read` 20.50 s,
`count_bundle_load` 8.38 s, `runtime_construct` 4.84 s, `structural_surface_check` 2.50 s, and
`output_staging` under a millisecond. The eight phases sum to 704.4829 s, which equals the
independently computed `initialization_seconds` of 704.482875929, so initialization has no
unattributed time. The streaming read loop is the 14.06 s processing stage, 1.9 percent of wall.

**Consequence:** parallelizing the streaming replay loop has an Amdahl ceiling near 1.02x and is
not the work to do. The two targets that together hold 85.6 percent of wall time are the fact-set
resolve loop, which resolves 64,275 fact sets against two profiles serially, and the compatibility
bundle open, which performs two full traversals with copies for integrity checks that need none.

**Acceptance:** the timing lines are additive stderr only. Replaying the accepted bundle with
instrumentation reproduces its checksum receipt: `summary.tsv` and all five Parquet tables,
including `read_assignments.parquet`, are byte-identical, and `manifest.json` differs only in
`command_line`, the whitelist path, and the two timing fields. Any future change to these phases
must preserve that property.

### D081 — Resolve compatibility fact sets in parallel ordered blocks

**Decision source:** Measurement, 2026-09-11, following D080's attribution.

**Decision:** the replay fact-set resolve loop runs through
`parallel_transform_ordered_blocks` rather than serially. Each fact set reads only const
`CountFactCatalog` and `PathIdentityLedger` state and writes one slot of its own, so blocks are
independent. Ordered blocks are required, not incidental: `validate_compatibility_structure`
fails closed by throwing, and the helper retains failures by block ordinal and rethrows the first
after every worker joins, so which invalid input is reported first remains a property of the
input rather than of scheduling.

**Thread-safety basis:** `CountFactCatalog::assignment_facts` and
`::validate_compatibility_structure` are const, the catalog declares no mutable members,
`resolve_assignment` is a free function over const references, and
`CompatibilityBundleReader::index()` returns a stored reference without lazy computation. The one
static in the call tree is a `static const std::regex` whose initialization is thread-safe and
whose use through `std::regex_match` does not mutate it.

**Measurement:** on the accepted one-million-read fixture the phase fell from 519.57 s to 24.59 s
at 128 workers, and total replay wall fell from the accepted 775.92 s to 304.85 s, a 2.545x
speedup. Peak RSS rose from 63,040,692 KiB to 64,078,328 KiB, 1.6 percent, from the bounded block
payloads the helper retains. The parallel run was measured under higher system load than the
serial baseline, so the speedup is a lower bound rather than a best case.

**Acceptance:** the accepted checksum receipt is reproduced. `summary.tsv` and all five Parquet
tables, including `read_assignments.parquet`, are byte-identical to the accepted run, and
`manifest.json` differs only in provenance and timing fields. 165/165 CTests pass. The
`requested_workers` notice no longer claims one active worker for the whole invocation, because
initialization now uses them; the streaming read loop remains serial and is 1.9 percent of wall.

### D082 — Verify compatibility fact integrity without copying the fact index

**Decision source:** Measurement, 2026-09-11, after D081 made `compatibility_open` the largest
remaining replay phase.

**Context:** sub-phase timers attributed the open path as `logical_hash` 68.32 s,
`canonical_check` 35.72 s, `exact_facts` decode 31.41 s, `structural_facts` decode 20.22 s, and
`manifest_verify` 0.55 s. The two largest were integrity checks whose cost was dominated by
copying rather than by checking. The bundle holds 50,614,005 exact and 25,684,302 structural
facts across 64,275 fact sets, each fact carrying owned strings, so the per-set copy and the
whole-collection copy duplicated roughly 152 million string-bearing facts between them.

**Decision:** neither integrity check copies. `canonical_check` uses an in-place `is_canonical`
predicate instead of canonicalizing a copy and comparing it, which is exact because `sort_unique`
leaves a vector unchanged precisely when that vector is already strictly increasing. The
fact-table digests move into `append_logical_fact_hashes`, so the reader hashes
`impl_->index.fact_sets` directly through `logical_fact_set_hashes` rather than assigning the
whole collection into a temporary `CompatibilityBundle`. The bundle overload remains for the
writer, which also has read rows to digest.

**Equivalence evidence:** no test covered the non-canonical rejection branch, so the predicate was
checked directly rather than inferred from a passing suite. Over 400,000 randomized fact sets, of
which the original logic accepted 158,101 and rejected 241,899, the predicate and the
canonicalize-and-compare check agreed on every case.

**Measurement:** `canonical_check` fell from 35.72 s to 4.97 s and `logical_hash` from 68.32 s to
19.55 s, taking `compatibility_open` from 173.27 s to 94.10 s. Total replay wall fell from 304.85 s
to 251.10 s and peak RSS from 64,078,328 KiB to 37,347,844 KiB, because the whole-collection copy
had been holding a second full fact index live. Against the accepted 775.92 s and 63,048,652 KiB
this is a 3.09x speedup at 59 percent of the memory. The other sub-phases read higher in this run
than in the one before it under heavier load, so the saving is a lower bound.

**Acceptance:** the accepted receipt is reproduced. `summary.tsv` and all five Parquet tables,
including `read_assignments.parquet`, are byte-identical, and `manifest.json` differs only in
provenance and timing fields. 165/165 CTests pass.

**Measurement update, same day:** the figures above compare single runs taken under different
machine states, so they were re-measured as an interleaved A/B against the pre-optimization
commit `02f717e` built in its own worktree, alternating arms with two repetitions each and
re-verifying the receipt on every run. Baseline wall was 834.59 s and 729.33 s, mean 781.96 s,
spread 105.26 s. Optimized wall was 246.71 s and 194.53 s, mean 220.62 s, spread 52.18 s. The
speedup is 3.544x on means, 3.170x restricted to the three runs sharing a load of 28 to 31, and
2.956x on the most conservative pairing of the fastest baseline against the slowest optimized
run. The gap between the fastest baseline and the slowest optimized run is 482.62 s, 4.6 times
the largest within-arm spread, so the difference clears the noise floor. Peak RSS was 63,039,050
KiB against 37,335,760 KiB, a reduction of 25,703,290 KiB or 40.8 percent, against a largest
within-arm RSS spread of 35,888 KiB. All four runs reproduced the accepted receipt. The single-run
3.09x quoted above is superseded by these figures; take 2.956x as the defensible floor.

### D083 — Key the annotation chain on ID, and make the chain-vote tie deterministic

**Decision source:** Audit of the folded annotation-chain scripts against
`.claude/rules/annotation-identifiers.md`, 2026-09-11. The rule was written on `main` after these
scripts were authored and was absent from this branch; it is now cherry-picked here so the
constraint travels with the code it governs.

**Decision:** the chain keys transcripts on the unique `ID` and resolves a gene per feature level.

  - `filter_cellranger_annotation.py` `transcript_ids` prefers `ID` over bare `transcript_id` on
    transcript and mRNA rows, which makes it agree with its own exon branch, since an exon's
    `Parent` is its transcript's `ID`.
  - `filter_cellranger_annotation.py` `local_gene_for` no longer reads `gene_id` uniformly. A gene
    row resolves to `ID`, a transcript row to `Parent`. `gene_id` is qualified on gene rows but
    bare on transcript rows, so reading it at every level put the two in different namespaces and
    then compared them. Exon rows still offer only the bare `gene_id`; they feed the exon
    fallback, which runs only for annotations with no transcript rows.
  - `gff3_to_genebody.py` prefers `Parent` on exon rows and `ID` on transcript rows over bare
    `transcript_id`.
  - `assign_cat_denovo_transcripts.py` `chain_verdict` orders the k-mer vote by support and then
    by gene id. `votes` was filled by iterating `index[gram]`, a set, so `max` returned the first
    maximal key in an order that varied with the process hash seed.

GTF inputs carry neither `ID` nor `Parent` and still fall back to `transcript_id`, so every
preference change is inert on reference input.

**Evidence:** nothing in the repository invokes these scripts, so no test could have caught any of
this and none did; each fix was checked directly against its pre-fix version.
On a reference GTF both filters are byte-identical before and after. On a CAT-style GFF3 where two
haplotypes share the bare id `T0000001`, `gff3_to_genebody.py` previously emitted one body instead
of two, silently overwriting the first haplotype's span with the second's, and now emits both. In
`filter_cellranger_annotation.py`, tagging only the second haplotype `readthrough_transcript`
previously produced `transcripts_observed=1`, `qualifying_transcripts=0` and
`retained_local_genes=0`, so one haplotype's excluded category disqualified the other haplotype's
valid transcript and both genes were lost; it now reports 2, 1 and 1 and retains `S1#1#G00001`.
The vote tie returned all four of four tied genes as the winner across twenty hash seeds before
the change and one gene across all twenty after, while a genuine two-vote winner still beats the
alphabetically first gene.

**Not changed:** `gff3_to_genebody.py` still derives the body's gene from bare `gene_id`, which
feeds the composite body name and the t2g. Whether two haplotypes' genes should collapse there is
a biological question about the t2g rather than an identifier bug, and it is left open.

**Acceptance:** 165/165 CTests pass, unchanged, since no test exercises these scripts.

### D084 — Stop duplicating the fact index, bound the spill merge, and resolve child rows by Parent

**Decision source:** Adversarial review plus independent measurement, 2026-09-11.

**Decision, four changes.**

`CompatibilityBundleWriter::finalize` drains `fact_ids` into `canonical.fact_sets` with
`extract()` and a move instead of copying each map key, and releases `canonical.fact_sets` before
constructing the staged reader. `fact_ids` owns every fact set as a map key; copying it duplicated
the bundle's dominant payload while the map stayed live, and the staged reader then decoded a
third copy from Parquet. Ascending map order is unchanged, so emitted order and every digest are
identical by construction.

`ZstdOutput` sets `ZSTD_c_windowLog` to 17 and the three spill stream buffers are one window
rather than one mebibyte each. `merge_runs` opens one reader per spill run before consuming any,
and a context sized for level 3's default 2 MiB window grows to about 2.5 MiB on first decode, so
the merge paid several MiB per run however small `--count-memory-budget` was. Spill runs are
transient intermediates, so a smaller window costs spill bytes and never reaches an output.

`filter_cellranger_annotation.py` `filter_annotation` resolves a child row's gene through its
transcript `Parent` before falling back to the bare `gene_id`. `allowed_genes` holds qualified
identities after D083, so consulting the bare `gene_id` first compared across namespaces; because
it is non-null the correct fallback never ran.

`filter_cellranger_annotation.py` `parse_attributes` accepts unquoted GTF attribute values.
GENCODE writes `level 2;` and `exon_number 1;` bare, and the parser refused them.

**Measurement.** One in-memory fact index for the accepted million-read bundle is about 25 GiB,
triangulated three ways that agree within 5 percent: a byte model over the actual struct layouts
and measured string-length profile gives 24.9 GiB; a 1/50 scale model of the handoff measured
+99.4 percent RSS across the copy loop, extrapolating to 24.4 GiB; and D082, which removed one
whole-index copy on the reader side of the same bundle, moved peak RSS by 25.5 GiB. The scale
model puts finalize at three simultaneous copies before this change and one after, so roughly
49 GiB of the recorded 203.5 GiB producer peak was redundant. For the spill merge, a scratch
program at a 1 MiB budget with one million distinct keys measured 87 file descriptors and
254,588 KiB peak, against 220 MB for the same job with the budget raised high enough never to
spill: the merge handed back more memory than the budget saved. Bounding the window measured
peak 254,588 to 103,816 KiB, 2.45x, with identical output and 9.5 percent more spill bytes.

**Not done.** A bounded fan-in multi-pass merge would bound descriptors and buffers rather than
lowering a constant, and it was prototyped at K=3 and shown to produce byte-identical output, but
it needs the run row count moved out of the compressed frame to be written properly. File
descriptors are not reachable on this host, where both limits are 1048576; at a stock 1024 limit
they are, at about 12.4 million distinct keys on a 1 MiB budget.

**Correction to D083.** Its evidence line "on a reference GTF both filters are byte-identical
before and after" was measured on a synthetic GTF whose attribute values were all quoted. The
parser could not read a real GENCODE GTF at all, so the no-op constraint had never been exercised
against one. It now is: the pre-fix script dies on the first gene row of GENCODE v47 with
`malformed GTF attribute 'level 2'`, the fixed script processes 199,995 rows and yields 2,494
allowlisted genes, and output on the quoted-only input is unchanged.

**Acceptance:** 165/165 CTests pass, including the fourteen that exercise the spill path at a
1 MiB budget. Replaying the accepted bundle reproduces its receipt: `summary.tsv` and all five
Parquet tables, including `read_assignments.parquet`, are byte-identical, at 221.01 s and
37,351,368 KiB against the A/B optimized mean of 220.62 s and 37,335,760 KiB. The writer-side
saving is not yet confirmed end to end, because that needs a producer run with
`--compatibility-out`.

### D085 — Narrow the count-fact bundle to what the runtime reads

**Decision source:** User, 2026-09-11, on noticing that preprocessing built to speed up iteration
over counting schemes had become required input. The stated aim is simplified, portable inputs;
input size is explicitly not the goal.

**Decision, three changes, none of which alters counting.**

Five bundle roles become optional: `corrected_annotation`, `parent_category_receipt`,
`projected_nested_policy`, and the two profile policy receipts. No code path opens any of them.
`corrected_annotation` is referenced once in all of `src/`, at `direct_count_facts.cpp:582`, and
only to compare a manifest schema string; there is no GFF3 parser in the binary to read it. They
are still validated when present, so a full bundle and a narrowed one both load.

The ledger path handles are resolved by enumerating the graph's paths once instead of asking for
each ledger row by name. `has_path` and `get_path_handle` are each a locate on the XG path-name
index, so the previous loop paid two per row across millions of rows. Validation still runs in
ledger order, which is sorted by path name, so the first failure reported remains a property of
the input. The phase is now timed as `ledger_path_handles`.

Nine ledger columns become optional: `vg_haplotype_origins`, `source_parent`, `input_parent`,
`haplotype`, `annotation_source`, `selection_status`, `fallback_status`, `source_transcript`,
`transcript_class`. Counting reads none of them, and none appears in
`compatibility_structural_surface_id`, so existing compatibility bundles stay replayable. They are
not deleted: they back a body/exon consistency check and a fallback validation that still run
whenever the columns are present. A header must still be a subsequence of the canonical order, and
a ledger missing a genuinely required column is still refused by name.

**Evidence.** Per-role audit of every `CountFactBundle::files` accessor gives five roles with no
reference outside `require_schema`, 8,896,817,114 bytes of the 10,736,677,291 a production bundle
occupies. A narrowed bundle of the remaining four roles was built by hardlink and loads: the first
attempt was correctly refused with `count-fact bundle content_id does not match content`, because
removing manifest entries changes the content address, and the canonicalization used to recompute
it was checked by first reproducing the original id exactly. Per-field audit of the ledger gives
nine fields with no reference outside `path_identity_ledger.hpp`; a 15-column ledger parses to the
same rows as the 24-column original, and an 8-column one is refused with `missing required column
gene_id`. 165/165 CTests pass after each change.

**What this does not do.** It does not make the input portable. The annotation the bundle carries
is itself a panSC product: 1,231,247 rows carry `feature_layer=transcript_body` with
`panSCbody1_<hash>` identifiers that exist in no upstream annotation. Portability requires reading
categories from an annotation directly, which needs a GFF3 parser and therefore contradicts
`product-spec.md`'s clause that the implementation must not require GFF3 in V1. That is a separate
decision and is not taken here.

**It is also not a speed change.** On a three-million-read production subset the bundle hash pass
is on the order of six seconds of 965, and the ledger read is 49.09 s, 5.1 percent. The one
runtime effect is the path-handle enumeration, whose previous form sat inside a 134.09 s
uninstrumented initialization block. Wall-clock work belongs in `splice_geometry`, 317.45 s and
32.9 percent of that run.

**Acceptance:** 165/165 CTests pass. Counting output is unchanged by construction: no assignment
code is touched, no field counting reads is removed, and the structural surface digest is
untouched.

### D086 — Batch splice geometry units and probe the small side

**Decision source:** Measurement, 2026-09-11. `splice_geometry` was 317.45 s, 32.9 percent of a
three-million-read production run and its largest single phase.

**What the phase is not.** It reports evaluating 141,543,588 candidate edges to keep 5,937,500, and
the 95.8 percent discarded looked like the target. It is not. The candidate set decomposes as
1,164,040 transcripts times 1.057 exon paths times 115.07 edges per path, and 115 edges per mRNA is
pangenome node granularity, about 19.4 bp per node; the 5.10 kept edges per transcript are its
introns. The ratio is nodes per mRNA over introns per mRNA and is intrinsic. The discarded
candidates cost hash lookups against a table of about 1,600 entries already resident in cache, and
they cannot be identified without first building the body index, which is the expensive part.
`evaluated - vetoed - owned` is exactly 7 across the whole graph, so the adjacency veto is not one
filter among several; it is the entire classifier.

**Decision, four changes, none of which alters a computed value.**

Work units are batched. `parallel_transform_ordered_blocks` was called with a block size of one, so
each of 1,164,040 transcripts took a scheduler handshake and an ordered-commit ring slot
individually. Every other parallel phase in the file batches; `exact_parent_models` runs 38,441
blocks for 1,230,107 parents. Geometry now uses the same sizing, four blocks per worker capped at
32 units. Blocks still commit in ordinal order, so the reduction sees results in task order.

The body loop probes the small side. A body path carries on the order of 1,578 nodes while a
transcript contributes about 115 candidate edges, and the loop walked the node side against an
index of the candidates, against the intent its own comment states. Asking each candidate whether
both endpoints lie on the path is the same condition, since the index keyed each edge under its
lower endpoint, and it removes both the index build and roughly 1,348 lookups per body path.

A body path's ledger row is resolved by graph handle. The loop built a path-name string and hashed
it into the 2,461,354-row by-name map once per body path, 1,231,247 times, when `ledger_path_info`
is already keyed by handle and holds a pointer to the same row.

`body_geometry_owns_splice_edge` is not called from the hot loop. It re-tested the adjacency the
preceding line had just ruled out and re-normalized an already-normalized pair; only its two
endpoint tests remain at the call site.

**Measurement.** Interleaved against `4afb084` built in its own worktree, two repetitions per arm,
on a 20,000-read GAMP since the phase is read-count independent. Before: 481.28 s and 456.675 s,
mean 468.98 s, spread 24.60 s. After: 67.285 s and 104.815 s, mean 86.05 s, spread 37.53 s. That is
5.45x on means, 4.36x on the most conservative pairing of the fastest before against the slowest
after, and 7.15x at best. The gap between those two extremes is 351.86 s against a largest
within-arm spread of 37.53 s, 9.4 times, so the difference clears the noise floor. The before arm
happened to run at a 1.12x higher mean load, which does not explain 5.45x. Total wall on the
20,000-read GAMP fell from a mean 878.86 s to 507.94 s. The machine carried unrelated jobs at load
150 to 209 throughout, so these are shared-machine figures and 4.36x is the defensible floor.

**Acceptance:** all four phase counters are identical across arms —
`evaluated_target_edges=141543588`, `owned_target_edges=5937500`,
`adjacent_vetoed_target_edges=135606081`, `fragment_only_target_edges=207`. `fragment_only` is the
sharpest of these: it is 207 out of 141.5 million and it is computed by exactly the loop whose
probe direction was inverted, so a non-equivalent rewrite would move it. 165/165 CTests pass.

### D087 — Key the assignment cache by digest and hold exact-edge geometry in sorted vectors

**Decision source:** Memory attribution, 2026-09-12, after a three-million-read production run
peaked at 122.1 GiB against a workload whose accumulating state is under a gigabyte.

**Context.** A first-principles budget for this job is roughly 750 MiB: 285 MiB of pre-collapse
UMI keys, 130 MiB of molecules, 10 MiB of count cells, and on the order of 200 MiB of reference
indexes. The run used 160 times that. Instrumenting each live structure in turn attributed
`assignment_cache` at 53.36 GiB and `exact_ex50_models` at 22.65 GiB, together 74 percent of the
103.65 GiB the allocator reported in use. Three earlier hypotheses were measured and rejected
first: glibc arena count (1.8 percent effect), heap fragmentation (only 2.89 GiB was free but
retained), and `node_ledger_cache` (6.26 GiB, not the tens of GiB assumed).

**Decision, two changes, neither altering a computed value.**

The assignment cache is keyed by a SHA-256 digest of its signature rather than by the signature.
`assignment_facts_signature` concatenates, for every candidate of a read and across both the
exact and gene-fallback lists, the full text of gene, equivalence gene, nested host, body-support
sample names, novel-origin gene, category names and gene-type names. On a ninety-haplotype
pangenome that text averaged over four hundred kilobytes per entry, and `BoundedShardedCache`
retained it as the map key. The entry count was bounded at 131,072; the entry size never was. The
signature is only ever compared for equality, so only a digest needs to be kept, and the
signature itself stays a per-read temporary.

`ExactEx50Model::exon_edges` holds a sorted unique `std::vector<ExactEdge>` rather than a
`std::set`. The geometries are built once and thereafter only membership-tested, and a red-black
node costs about eighty bytes to carry a thirty-two byte edge. The vector is sorted and
deduplicated at construction so its contents match the set exactly, and the single query site uses
`std::binary_search`.

**Measurement.** On the three-million-read chr20-22 production subset, peak resident memory fell
from 127,987,152 KiB to 61,911,068 KiB, 122.1 GiB to 59.0 GiB, a 2.07x reduction. The phase peaks
move as predicted: after `exact_parent_models` 59.3 GiB becomes 45.5 GiB, and at one million
completed read groups roughly 95 GiB becomes 56.2 GiB. Wall time fell from 965 s to 531 s on a
more heavily loaded machine, which is consistent with the reduced allocation pressure but is not
a controlled comparison and should not be quoted as a speedup.

**Acceptance:** byte-identical output. `summary.tsv` and all four Parquet tables match the
pre-fix reference exactly. 165/165 CTests pass.

**Residual.** Attribution accounted for 86.50 GiB of 103.65 GiB in use before the fixes, leaving
17.15 GiB unattributed, consistent with the XG's heap-side allocation and per-chunk overhead not
charged by the estimators. The remaining 59.0 GiB is still far above the roughly 750 MiB the
workload requires; the largest identified remainders are the XG itself at about 23 GiB, the
path identity ledger at 3.41 GiB holding 2,461,354 rows of twenty-one `std::string` fields where
interned integer identifiers would need tens of megabytes, and `node_ledger_cache` at 6.26 GiB,
which never evicts.

### D088 — Accept a corrected annotation as a second count-fact authority

**Status: ACCEPTED** (User, 2026-09-13). This entry was raised as a proposal because it appeared
to contradict a clause of `docs/product-spec.md`. The user resolved that on approval: the clause
"was only ever supposed to guard against gtf projection." The apparent conflict was therefore a
reading of the sentence wider than its intent, not a real constraint being lifted. The one
genuine change to settled behavior is to D076.

**Supersedes:** D076 only in its clause that the count-fact bundle "is the only assignment
authority". D076's profile semantics, override scope, output product, and release gates stand
unchanged.

**Clarifies rather than supersedes:** `docs/product-spec.md`'s no-GFF3/no-GBZ/no-custom-index
sentence. Its GFF3 term is narrowed to what it was written to mean -- no runtime GTF or GFF3
*projection* -- which the annotation route does not do. The GBZ and custom-index terms are
untouched and remain in force.

**Decision source:** D085 (User, 2026-09-11) established the aim: "preprocessing built to speed
up iteration over counting schemes had become required input", and "the stated aim is simplified,
portable inputs; input size is explicitly not the goal." D085 narrowed the bundle but recorded
that it could not reach that aim, because "portability requires reading categories from an
annotation directly, which needs a GFF3 parser and therefore contradicts `product-spec.md`'s
clause that the implementation must not require GFF3 in V1. That is a separate decision and is
not taken here." This is that decision.

**The conflict, stated plainly.** Two settled statements are inconsistent with the implemented
route and are amended here rather than quietly deleted.

- `docs/product-spec.md`: "The implementation must not require GFF3, GBZ as a substitute for
  `.xg`, or a custom lookup index in V1." The GFF3 half of that sentence no longer holds for
  `count`. The GBZ and custom-index halves are unchanged and remain in force.
- `CLAUDE.md`: "Runtime inputs are the GAMP, the graph with `vg rna` HST paths, and a
  transcript-to-gene map; the GTF and `vg rna` are fixture-creation only." That remains true of
  `convert`. It is no longer true of `count`, which may now read a corrected GFF3 at runtime.
- D076: "The bundle is the only assignment authority." Superseded to the extent of that word
  only. Everything else in D076 — profile semantics, override scope, output product, release
  gates — stands.

**What the clause was guarding, and why this route is outside it.** The clause guarded against a
runtime GTF projection: a design in which panCollapse would recompute exon and intron geometry
from an annotation at count time. The documentary record agrees -- D048 "replaces the
traversal-enumeration and GTF-projection algorithm", `.agent-workspace/USER_DECISIONS.md` records
"V1 compatibility is now HST-path node membership (no GTF projection)", and TASKS.md's Phase 3
closes with "do not reintroduce ... a runtime GTF projection" -- and the user confirmed on
approval that projection was the only thing it was ever meant to bar. Listing GFF3 beside GBZ in
an input-inventory sentence made the wording read wider than the intent.

The implemented route is narrower than a projection. It reads already-computed classifier
verdicts that the correction chain wrote onto the annotation as `tag=` attributes. No geometry is
derived from the GFF3; the graph and the path-identity ledger remain the only source of
structure. The projection prohibition is untouched and still binds.

The bundle was never intended to be a required input. It was an iteration-speed artifact for
trying counting schemes quickly, and it drifted into the input contract. Its cost is that it is
unobtainable: as D085 records, the annotation it carries is itself a panSC product carrying
1,231,247 rows with `panSCbody1_<hash>` identifiers that exist in no upstream annotation, and
building the bundle requires the panSC toolchain. A corrected GFF3 plus a reference GFF3 is a
standard portable format an end user can produce with the scripts in this repository. The goal
is simpler portable inputs, not smaller ones.

**This is not a custom persistent lookup index.** `.claude/rules/scope.md` and
`.claude/rules/project.md` forbid broadening V1 with one, and the distinction is recorded here so
it is not relitigated. `CountFactCatalog::from_annotation` parses a standard annotation format at
process startup into the same in-memory `parents_`, `genes_` and `profile_genes_` maps that
`CountFactCatalog::load` builds from a bundle, and writes nothing to disk. Nothing is built,
persisted, versioned, or reused across runs. A bundle, by contrast, is precisely a persisted
precomputed artifact; this decision reduces the project's reliance on one.

The user also recorded the contingency the index rule was written against: a custom index was
only ever meant to be an *optional emission*, to be reached for if parsing the annotation
directly proved too difficult. Parsing did not prove difficult -- `from_annotation` derives the
full catalog from the annotation at startup -- so the contingency never fired and no index is
emitted. That outcome strengthens the prohibition rather than eroding it: the case that would
have justified an index is the case that did not arise.

**What is implemented.** `panCollapse count` takes exactly one of two fact authorities.

- `--count-bundle <panSC-count-facts-v1>`, unchanged.
- `--annotation <corrected.gff3>` together with `--annotation-reference <reference.gff3[.gz]>`
  and `--path-identity-ledger <ledger.tsv>`. All three are required on this route; the ledger is
  otherwise rejected on `count`, because on the bundle route the bundle supplies it. Either
  annotation file may be gzipped; the reader handles both through zlib.

`CountFactCatalog::from_annotation` produces the same structures the bundle loader produces. Two
derivation details were each the subject of a defect and are recorded so a future
reimplementation does not repeat them.

- **Reference gene tags are inherited through `source_gene`.** CAT drops a reference gene's tags
  when it projects a model, and the correction chain restores them by following `source_gene`
  back to the reference gene row, matched with the Ensembl version suffix stripped because the
  projected `source_gene` carries one and the reference row need not agree on it. Omitting this
  step does not fail; it silently under-tags the annotation.
- **`count_gene` comes from the loader's own `normalized_locus()`**, which strips the Ensembl
  version suffix while preserving the `_N` and `__LOC` discriminators. An independent
  reimplementation that kept the version disagreed with the bundle. Sharing the function makes
  the two routes agree by construction rather than by review.

**What the derived catalog compiles into the binary.** On the bundle route the recognized
category vocabulary and the gene-type allowlist arrive as data in bundle tables. On the
annotation route they are code: the twelve selected category tags, the 10x 2024-A gene-type
allowlist, the transcript-type superset, and the demotion vocabulary are constants in
`direct_count_facts.cpp`, and `classify_annotation_aggregate` from `count_cr.py` is ported there.
Changing a category or an allowlist entry is therefore a code change on this route where it was
a data change on the other. That is a real cost of the route and is accepted knowingly.

**What the derived catalog cannot state.** `protected_primary_protein_nested_host` is left
`false` for every Parent. It is a spatial containment relation between a gene and a host gene
that the annotation does not assert, and a derived catalog does not guess it. The related
`nested_host` field is read only from a `nested_host_gene` attribute on a gene row. An annotation
that has not been through `scripts/tag_nested_reference_hosts.py` carries no nested-host relation
at all, so nested-host policy has nothing to act on, silently and with no error; run that pass
before counting on this route. Running it is sufficient: the attribute does reach gene rows.
Counted on the chr20/21/22 fixture's `corrected_annotation.gff3`, `nested_host_gene` appears on
39,108 `gene` rows, alongside 92,167 transcript and 111,833 exon rows, so the factory's gene-row
branch reads a populated attribute. For scale, the bundle that route must reproduce carries
1,934 non-empty `nested_host` values across 80,486 profile-gene rows. Note separately that
`nested_within_host_gene` is not one of the twelve selected category tags, so the tag itself is
dropped and only the attribute carries the relation.

**Receipt schema change.** `manifest.json` gains a string field `fact_authority`, emitted beside
`fact_bundle_content_id`, whose value is `count-bundle` or `annotation`. Both fields are now
validated as non-empty before any output is written. `fact_bundle_content_id` was deliberately
not renamed, so existing readers keep working; `fact_authority` is what tells a reader whether
the id it is looking at is a bundle's published `content_id` or one derived here. On the
annotation route the id is `sha256:` followed by a digest over the SHA-256 hex digests of the
three files read, in the order corrected annotation, reference annotation, path-identity ledger.
It hashes input bytes rather than the derived catalog, which matches how `gamp` and `xg` are
recorded in the same receipt and keeps the id cheap; a gzipped and a plain copy of one GFF3
therefore receive different ids.

The receipt's `inputs` map differs by route. The bundle route records
`count_bundle_manifest`; the annotation route records `annotation`, `annotation_reference`, and
`path_identity_ledger`.

`path_identity::PathIdentityLedger` gains a `source_path` member recording the file the rows were
read from, so a consumer deriving an identity from the ledger does not need the path handed to it
a second time alongside the ledger.

**Defect fixed in passing.** On the annotation route `count_bundle` is a `std::optional` holding
`nullopt`, and two sites dereferenced it unconditionally to read `content_id` and
`manifest_path`. This is undefined behavior. In practice it did not crash; it yielded an empty
string, which the output metadata check then rejected. A completed 47-minute chr19 production run
aborted at the output-writing step with `direct-count output metadata is incomplete`, after all
counting work was done.

**Evidence.** The derivation rule was established before the C++ factory was written, by
`scripts/derive_count_facts_from_annotation.py`, which derives the bundle tables in Python from
the same three inputs. It reached 100.00% agreement with the identifier-fixed bundle. That
harness validated the rule; it is not a pipeline step and is superseded at runtime by
`CountFactCatalog::from_annotation`.

**A readthrough quantifier defect, fixed, and what it revealed about the reference.** The
bit-exactness A/B was run on the one-million-read chr20/21/22 fixture and failed: cr7-v1 counts
rows 228,719 (bundle) against 177,392 (annotation), observed features 1,010 against 740, with 307
genes lost and 37 gained. All 307 lost genes are PRIMARY/allowlisted in the bundle's own policy.

The defect was in this route. `from_annotation` set one `readthrough` flag true if *any* of a
gene's Parents carried `readthrough_transcript`. Readthrough is two signals with different
quantifiers, and they are now kept apart. A `readthrough_gene` tag on a gene row, or on the
reference gene inherited through `source_gene`, excludes the locus on its own -- a disjunction.
A `readthrough_transcript` tag excludes the locus only when *every* observation carries it, a
conjunction, reported as `all_readthrough_transcripts`; a Parent with no transcript row abstains
rather than voting against. An observation is a Parent (User, 2026-09-13).

The conjunction is what keeps the gene-level rule from doing the transcript-level rule's job.
`resolve_assignment` already drops individual readthrough candidates in `filter_categories`
(`direct_count_assignment.cpp:398`) before `apply_gene_competition` runs (`:501`), so a locus with
some readthrough isoforms keeps its legitimate ones and loses only the tagged candidates. Under a
disjunction the gene-level verdict fired on the same evidence and discarded every candidate,
including the ones the transcript filter had just cleared. Across the 307 lost genes the median
fraction of Parents actually tagged was 0.025 and never reached 1.0; the cost was 125,944 of
692,194 cr7-v1 UMIs, 18.2 percent, and `unassigned_no_identity` -- the terminal set exactly when
competition empties the gene list -- rose from 3,118 to 176,031.

`tests/policy/count_fact_annotation_smoke.cpp` now pins all three arms, and reinstating the
disjunction fails the partial case.

**Bit-exactness against this bundle is not the right acceptance criterion for this rule, because
the bundle is wrong here.** The bundle's gene policy was derived by the frozen `count_cr.py`
(sha256 `4d9707ef7c62`) from this same annotation, byte-identical (`c793860d...`, 8,896,802,871
bytes). That builder deduplicates observations on `transcript_key = (annotation_path,
transcript_scope)`, and for transcript and mRNA rows `transcript_scope` is the bare
`transcript_id` -- the key `.claude/rules/annotation-identifiers.md` forbids because CAT numbers
each assembly's transcripts from `T0000001` and haplotype id spaces coincide. Only *exon* rows get
the qualified `exon_unique_parent`. Hundreds of haplotype copies therefore collapse to a handful
of observations, which makes the conjunction trivially satisfiable. That is where the bundle's 399
`all_readthrough_transcripts` genes come from: measured over Parents on the same annotation, only
15 of those 399 have every Parent tagged, and the median tagged fraction is zero even taking the
full union of transcript-row, gene-row and reference-inherited tags. This is the same identifier
defect the rules file documents in `build_parent_category_ledger.py`, in a second place -- the
gene policy path -- and it is not corrected by `count-facts-idfix`, which repairs only the parent
category ledger.

**The annotation is not at fault, which was checked rather than assumed.** This entry first
recorded that the annotation was under-tagged and that the correction chain needed a propagation
pass. That was wrong, and the measurements that refuted it are now in
`scripts/audit_readthrough_consistency.py`. All three paths by which a copy could lose a tag its
source asserts are intact on the chr20/21/22 annotation: every child transcript of a
`readthrough_gene` gene row is tagged (20,061/20,061), every copy of a readthrough source
transcript is tagged (5,723/5,723, joining on the version-stripped identifier, which resolves for
98.09 percent of source transcripts), and every body Parent agrees with its exon counterpart
(1,231,247/1,231,247). A propagation pass would have changed nothing.

The 664 partially tagged count genes are partial legitimately: a count gene aggregates several
source transcripts, and some read through while others do not. The conjunction keeps such a gene
and `filter_categories` drops only its tagged candidates, which is the intended two-layer
behavior. Of 23,416 count genes, 22,691 have no Parent tagged, 61 have every Parent tagged, and
those 61 are what the rule excludes. Separately, 70 source transcripts are tagged on some copies
and not others; all 70 carry `alignment_only` and 63 also carry `structural_readthrough`, so they
are per-haplotype structural calls written by `apply_cat_denovo_corrections.py` rather than lost
tags, and the audit deliberately permits that direction.

The residual disagreement with the fixture bundle is therefore attributable to the bundle's
builder alone. `docs/annotation-correction.md` states the contract for annotation preparers.

**The chr19 counts produced on this route before this fix are wrong** and must be regenerated.

**Historical gaps at initial drafting.** The dated update below supersedes the test and run
status in this list; it does not silently close production acceptance.

- No test drives `--annotation` through `write_count_outputs`. The C++ factory's agreement with
  the bundle route is therefore unverified by the test suite.
- A bundle-route versus annotation-route bit-exactness A/B on the one-million-read chr20/21/22
  fixture is running. Its result is not known, so the two routes must not be described as
  agreeing until it lands. `docs/validation-contract.md` carries it as a required case.
- `--compatibility-in` replay does not support this route. `run_compatibility_replay` loads
  `options.count_bundle` unconditionally and reads the ledger out of the bundle, so
  `--annotation` is a GAMP/XG-only route. `parse_options` now refuses the combination with a
  usage error; previously it was accepted and failed later as a bundle-load error on an empty
  path. Supporting replay on this route would mean sourcing the ledger independently of the
  bundle, and is not done here.
- chr19 has 514 unorientable body/exon pairs across 330 parents, 0.061 percent, a fixture
  generation skew rather than a counting defect. They are currently handled by degradation, which
  records `body_path_degrade_reason=repeated_exonic_node`. The planned fix is rebuilding the body
  layer from the corrected annotation with `scripts/gff3_to_genebody.py`.

**Production acceptance at the September 13 checkpoint.** The decision is accepted; the route is not validated and currently fails its own
acceptance. Two conditions stand. A test must exercise `--annotation` end to end through
`write_count_outputs`: satisfied by `tests/policy/count_fact_annotation_smoke.cpp`, whose
assertions were each confirmed to fail against a deliberately broken factory. The
bundle-versus-annotation A/B must be bit-exact on the one-million-read chr20/21/22 fixture: this
FAILS today, for the readthrough-quantifier reason recorded above.

The A/B reference is `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_parent_ledger_identifier_fix_v1_20260906T000000Z/count-facts-idfix`,
not the fixture's own bundle. The fixture bundle's `parent_category_ledger.tsv` is the pre-fix
build, reproduced byte-for-byte (sha256 `157cfae7...`) from the pre-fix builder its own receipt
names, and all four signatures in `.claude/rules/annotation-identifiers.md` reproduce on it:
`readthrough_transcript` 106,604 against the corrected annotation's 108,937, a gap of exactly
2,333; `overlaps_pseudogene` 376,827 against 396,204, a gap of 19,377; 18,247 rows left
explicitly untagged; 25,893 rows changed by the fix. The identifier fix is strictly additive --
across all twelve selected category tags and all 2,461,354 Parents, the bundle never carries a
category the annotation lacks. That defect is real but is *not* what fails the A/B; it was
measured and found to explain none of the 307-gene loss.

**2026-09-14 validation/resumption update (no policy change).** Recovered terminal manifests
and executable hashes distinguish the completed September 13 A/B and identifier-fixed-bundle
counterfactual from the later fixed source. Both used frozen binary
`490643ca4505977a9759fd4baae6eeb98d450d9fd2bd197a90d659d6793db6aa`; the current executable is
`251bfa0c948735fdf0ca33c346ac2ca63684679d6ccf13241d90792d0eaab763`. No recovered production
receipt demonstrates a run with the current executable. Exact paths and existing performance
evidence are recorded in `docs/research/compat-replay-resumption-20260914.md`.

The pre-session test log contains 166 passes, including the annotation factory/output smoke.
Added `direct_count_annotation_cli` to close the actual CLI-to-writer wiring gap: five tiny
count invocations cover plain/gzipped annotation inputs, provenance changes for each of the
three authority inputs, both profiles with independently expected eight UMIs, and biological
table agreement with the existing tiny bundle fixture. Six malformed authority combinations
are refused before initialization or output. Focused CTest passes 8/8 including fixture
dependencies. No production C++ or frozen counting policy changed in this increment.

The original production bit-exactness requirement and the later finding that its comparison
bundle has a gene-policy defect remain an unresolved acceptance conflict. These machine checks
do not resolve that scientific gate or establish counting accuracy. Reuse completed performance
receipts only for their recorded executable/input pair; do not mix their timing claims with
the annotation-policy changes. Existing A/B outputs are preserved.

**2026-09-15 bounded generator repair (no biological policy change).** The user authorized
repairing and testing the comparison gene-policy generator. An isolated panSC worktree at
`/mnt/ssd/lalli/panSC/.codex/worktrees/gene-policy-parent-identity` starts from
`61b1eaea741d86b8e3e883168b2c681c6e60cbd4`, whose `count_cr.py` matches the frozen `4d9707ef...`
generator byte-for-byte. The repair keys parent features by own `ID`, gives those features
precedence over child exons independent of row order, preserves the legacy exon-only body
linkage, and prevents bare local IDs from spreading transcript exclusion to clean siblings.
Parents still aggregate into canonical transcripts and genes under the existing rules.

The corrected generator SHA-256 is
`604faf4aee96f90180b0bb5ec9af91072df2173ea63b3bb0fc3c9013a8ee0f64`. Nine new policy-only tests,
both gene-competition/readthrough integration suites, and eleven bundle compiler tests pass;
the frozen-equivalent generator fails nine new regression assertions/subtests. Evidence is
in the panSC worktree's `docs/gene_policy_parent_identity_repair.md` and checksum-bound
`results/gene-policy-parent-identity.BMr8qj/` source/log snapshot. The additional annotation
pass has no production performance measurement.

No existing bundle was regenerated, and no production count/accuracy result is superseded.
This completes the bounded generator fix while leaving the fresh comparison authority and
current-binary 1M acceptance/accuracy phase open. This historical next action is superseded
by the terminal comparison below; all new scientific phases use the automatic
documentation-reconciliation checkpoint recorded in the current project state.

**2026-09-15 production contradiction (supersedes generator-complete scope above).** The
authorized production derivation retained all 40,243 gene class/reason pairs despite changed
Parent exclusion identities. A second Python defect treated every configured exclusion tag
as readthrough evidence. D088's existing exact `readthrough_gene` / `readthrough_transcript`
rule authorizes the correction; no new biological policy is introduced. The second fix
applies exact tags to CR7 inference and preserves non-CR7 custom demotion. Attempt1 outputs
and source remain immutable; attempt2 and current-binary validation are recorded in
`docs/research/parent-policy-accuracy-1m-20260915.md`. Production acceptance remains pending
terminal comparison and truth scoring. Historical eligibility-based F1 and corrected-policy
F1 cannot be compared as one denominator; the common comparison uses the frozen scorer's
existing all-mapped-origin mode on all 1M reads.

**2026-09-15 terminal corrected-bundle comparison (no new policy decision).** The fresh
production bundle, both current-binary count arms, decoded comparison, and full-truth plus
corrected-policy open-set scoring completed with exit 0. Run root:
`/mnt/ssd/lalli/pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z`.
Its `RESULTS.md`, `VALIDATION.json`, and stage receipts are the terminal authority. Executable
SHA is `251bfa0c948735fdf0ca33c346ac2ca63684679d6ccf13241d90792d0eaab763`; corrected bundle
content ID is `sha256:f1ffba4d4fe821e6bda6d0b88bba3cfe5df9df5fb263efe099e27fe14146c99e`.
The current generator `d18bd9be...` passes 11 focused tests and both counting integration
suites. A genuine fresh category producer receipt accompanies byte-identical reproduction
of all 2,461,354 retained corrected Parent rows; compiler source checks were not weakened.

CR7 bit-exactness passes for all 1M read diagnostics, counts, and 699,260 molecules. Strict
bit-exactness fails: 322 diagnostic assignment changes and 209 differing matrix coordinates,
with 709,623 annotation versus 709,532 bundle molecules. All observed assignment changes
are explained by projected nested-host direction (199 cleared-edge and 86 reversed-edge
effects) and per-Parent host protection (37 effects). These are the annotation authority's
two missing facts already documented in the input contract. The corrected readthrough
predicate, feature names, and unchanged missing `ENSG00000298824` policy entry do not explain
this strict-only difference. This terminal evidence supersedes the earlier attribution of
all remaining differences to the gene-policy builder.

The fixed all-mapped-origin comparison includes every 1M read and all 1,024 truth genes;
historical metrics reproduce exactly. Corrected bundle read F1 is 96.2336% CR7 / 96.9883%
strict; annotation CR7 is identical and annotation strict is 96.9861%. Historical/current
changes include both executable and authority changes. The unchanged corrected-policy
open-set guard also passes, with 999,085 eligible and 915 background reads; false assignments
on 819 CR7 / 820 strict background reads limit the aggregate accuracy claim. Read truth
scores are distinct from the completed matrix/molecule internal-consistency checks.

Execution and scoring are complete; D088 strict acceptance remains open. Resolving the
authority mismatch requires human product review under the existing gate. The projected
policy receipt retains `scientific_role: method_search_only` and `benchmark_eligible: false`;
this diagnostic comparison does not promote that policy into biological authority or select
a policy by truth scores. No further strict implementation decision is approved. Converter
and replay performance remain independent. The exact next task is human product review of
D088 strict acceptance; this comparison does not authorize a further strict-policy change.
Before any new scientific phase, automatically run a documentation-reconciliation agent to
refresh the local AI-facing state, decisions, and research record with goals, generated files
and experiments, results, decisions, existing code, exact source/input locations, and the next
gate. Full record:
`docs/research/parent-policy-accuracy-1m-20260915.md`.

**2026-09-15 user decision (supersedes the strict-parity acceptance clause above).** Strict
route bit-exact parity is no longer an acceptance requirement, especially if F1 improves.
Retain strict route discrepancies as diagnostic evidence; do not discard the completed 322
assignment changes or 209 differing matrix coordinates. This resolves the D088 product question
only. It does not promote the projected `method_search_only` / `benchmark_eligible: false` policy,
select arbitrary biological rules, authorize reruns, or alter sealed bundle receipts. Continue
with annotation accuracy under a fixed, explicitly stated truth denominator: report precision and
recall as percentages and F1 conventionally on a 0--1 scale. Extra assignments or the relaxed
parity gate do not prove improvement. Current annotation strict F1 is slightly lower than the
corrected bundle under the fixed all-mapped-origin denominator: read
`0.96986068839009076` versus `0.96988313775162238`, and gene-balanced
`0.94202475192394564` versus `0.94356879517102621`; no observed annotation improvement is
claimed. The next task is an annotation-accuracy investigation under the revised gate; a concrete
implementation experiment is not specified. Before a new scientific phase, the existing automatic
documentation-reconciliation-agent checkpoint remains required.

### D089 — Attribute native-count processing before optimizing it

**Full-input throughput cliff characterized; sampled-`SpaceUsedLong` decoder fix accepted
(2026-09-21; supersedes the allocator-substitution frontier below).** The record of this work is
`docs/research/full-input-throughput-cliff-20260921.md`; the paragraphs below remain valid for the
1M-read prefix and are no longer the frontier.

*Terminology.* The counters `input_read_groups` and `completed_read_groups` name one read's
alignments — all GAMP records sharing one QNAME (`src/main.cpp:6321`; `struct Group` at `:2004`) —
and not a SAM/BAM `@RG` read group. New text in this entry says **read** and **alignments per
read**.

*The full-input run is terminal, validated and unsealed.*
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z` finished 2026-09-18 13:23 UTC
(`REPORT_RECEIPT.json` `created_utc`), every stage exit 0, STATUS `VALIDATED_REPORT_READY_PENDING_DOCS_AND_SEAL`. It processed 25,755,749
reads and 124,261,698 alignment records in a 210,645.357663 s read phase — 817.857627 s/100k reads,
817.985728 s/100k with count finalization — at peak RSS 137.476540 GiB; both the preferred <5
s/100k and the 16.5 s/100k fallback are missed. It ran binary `3d13e082…d7d0f958`, not the frozen
`6eb3e7ca…d6843a`, and predates ordered parallel input and Parent metadata reuse, so **no absolute
prefix-versus-full comparison drawn from it is a scale effect**; it is confounded by binary version.
Within-run behaviour is clean.

*The cliff and its cause.* Interval throughput holds 300–2,000 reads/s to read about 22,046,000,
then collapses inside a single roughly 12,288-read interval to about 21 reads/s and never recovers:
85.0% of reads take 14.2% of read time and 15.0% take 85.8%, a mean per-read ratio of 34x. It is not
aggregation spill (`aggregate_spill_bytes`/`rows`/`runs` all zero), not host load (which fell), and
was never logged (11 non-progress lines in 58.5 hours).
`/mnt/ssd/lalli/pancollapse_cliff_characterization_v1_20260919T194550Z` (sealed, 101 files) cut four
100,000-read windows from one pass over the source GAMP at reads 500k, 12.0M, 21.8M and 22.3M and
counted each from a **cold** process on the current binary, excluding accumulated state; post-cliff
reads carry **fewer** alignments each (5.41 against 9.99), excluding multimapping. The cause is graph
structure per alignment: 326x the subpaths and 596x the mappings of the pre-cliff window, with the
whole distribution shifted (subpaths p50 1→6, p90 1→261, p99 7→15,295) and 56,950 measured
candidates/read confirming a 55,700 prediction to within 2%. No software threshold is crossed:
decode CPU per alignment rises 408x against 596x more mappings (sublinear), and the glibc
slow-path/fast-path ratio falls from 0.98 to 0.33. Counting scales only 23.1x per read against
decode's 219x because the exact-Ex50 prefilter (`src/main.cpp:4488-4545`) cuts candidates 10.1x
post-cliff against 1.36x pre-cliff. Wall scales 73.8x against 29.9x for total CPU; the 2.5x gap is
starvation.

*Open question for the mapping pipeline owner, unresolved.* Source ordinals rise monotonically with
file position, so this GAMP is one continuous run in sequencing order. Across ~725,000 source reads
at the cliff, alignment character (median subpaths 1→34) and retention into the GAMP (~69% locally
in the tail against ~6.6% cumulative early) shift together. **Why the tail carries ~600x more graph
structure per alignment is not established**; the candidates are flowcell/run-position effects, the
upstream `d10k_cap20k` capping scheme, and a permissive fallback re-mapping pass in
`pangenome_mapping_reuse_v15r2`. The retention shift points upstream rather than at biology. If
these alignments are a mapper artifact, optimizing panCollapse to consume them faster treats a
symptom; asking the owner is free and could moot the remaining work.

*Stage attribution corrected; a sealed root superseded.*
`/mnt/ssd/lalli/pancollapse_profile_tools_v1_20260919T052434Z` (sealed, 10 files) **supersedes**
`/mnt/ssd/lalli/pancollapse_stage_attribution_v1_20260918T192832Z` (sealed, 9 files, retained; its
stage table is wrong). Substring matching over demangled C++ signatures is unsound because a
demangled signature names every type in its template arguments: 40 symbols contain the count-worker
lambda's needle and only 20 are that lambda's own `operator()`, which misfiled
`std::_Rb_tree<unsigned int, pair<const unsigned int, {lambda}>>` into node-ledger build. Corrected
shares of worker-attributable CPU: node ledger build 15.72 → **13.13**, per-read tally maps 6.91 →
**9.40**, all others under 0.1 point. Corrected node-ledger build is 10.71% of all CPU, equal to
perf's independent inclusive 10.712% for `ReadNodeTallyCache::expand` — a check the superseded
number failed. The replacement tool matches anchored **mangled** prefixes against a closed,
conflict-checked table, fails loudly on unknown symbols, and makes one perf pass instead of four.

*Rejected and falsified.* Raising `max_inflight_batches` 8 → 64 → 256 is **rejected**
(`/mnt/ssd/lalli/pancollapse_inflight_depth_v1_20260920T181428Z`, sealed, 61 files): idle fell only
65.1% → 58.6% → 60.8% (non-monotone), headroom flat 3.55–3.72x, throughput worse than both
bracketing controls, and peak buffered bytes saturated 632/676/678/608 MB across a 32x slot increase
— admission is byte-gated, not slot-gated. Two identical control arms differed 14.8% in reads/s but
0.02 points in idle, so idle percentage is the trustworthy statistic on this host and raw rate is
not. The trial added `PANCOLLAPSE_INPUT_INFLIGHT_BATCHES` (`src/main.cpp:4977`), inert when unset,
byte-identical output. A working note that `max_buffer_bytes` never blocks and that
`exceeds_raw_budget` is dead code is **contradicted by the current worktree and must be re-checked
before anyone acts on it**: `options.max_buffer_bytes` gates producer admission in `append_record`
(`src/ordered_gamp_reader.hpp:564`, `:581`, `:585`) by sealing and draining through `commit_front()`
(`:511`), which blocks in `wait_until_complete()` (`:514`), and `exceeds_raw_budget` (`:73`) is
assigned positionally at `:600` and read at `:381`. Only the decoder-side member
`max_buffer_bytes_` (`:381-383`) is purely diagnostic. The 632–678 MB peak buffered against a 64 MiB
default budget (`:40`) is itself unexplained and is recorded as a limit, not reconciled.

*Producer partition and the first post-cliff profile.*
`/mnt/ssd/lalli/pancollapse_producer_partition_v1_20260920T220538Z` (sealed, 27 files) adds
timing-only instrumentation and partitions a 973.1 s read phase into **910.3 s (93.5%) blocked in
`wait_until_complete` on the oldest batch**, 53.9 s (5.5%) reading and framing raw input, and 7.9 s
(0.8%) grouping and submitting, summing to 99.9%; the grouping hypothesis is dead and decoder
dequeue is FIFO, so this is not an ordering defect.
`/mnt/ssd/lalli/pancollapse_cliff_profile_v1_20260920T232129Z` (sealed, 21 files) is the first
profile in this programme taken on the expensive regime — 895,383 samples of `cpu-clock:u` at 99 Hz,
no call graph, pools separated by thread name to avoid the DWARF truncation that left 18% of the
prefix profile unattributable. Threads: `pc-count` 74.05%, `pc-decode` 25.36%. Decoder buckets:
allocator 59.3% (1,423 CPU-s), `SpaceUsedLong()` accounting 21.7% (521 CPU-s), protobuf parse 10.1%
(242 CPU-s), ctor/dtor 5.9%, lock/futex 1.1%. Count buckets: other (exact DP, tally, resolve) 61.5%,
allocator 30.7% (2,146 CPU-s), parse 2.8%, ctor/dtor 2.3%, `memcpy` 2.1%. **Allocation totals 3,569
of 9,382 CPU-s, 38% of all CPU post-cliff, against 17.13% on the prefix.** Lock contention is dead
at 1.1% and 0.6%.

*Accepted fix.* `SpaceUsedLong()` was called per record, walks the whole nested object graph, and
feeds only diagnostics. It is now measured exactly once per 4 MiB of serialized bytes per decoder
thread with the remainder scaled by serialized size (`src/ordered_gamp_reader.hpp:47-48`, applied at
`:355-366`). The first iteration (`/mnt/ssd/lalli/pancollapse_spaceused_fix_v1_20260921T002651Z`,
sealed, 26 files) sampled per batch and recovered 247 CPU-s; because batches seal on bytes a large
record arrives alone and was always measured exactly, so the accepted iteration
(`/mnt/ssd/lalli/pancollapse_spaceused_fix_v2_20260921T043810Z`, sealed, 28 files) moved the budget
to a running counter carried across batches and recovered 66 CPU-s more. On `w_cliff`: decoder CPU
**2,123.5 → 1,810.9 s (-14.7%)**, read phase **973.1 → 874.6 s**, throughput **102.8 → 114.3
reads/s (+11.3%)**, with the v2 arm at host load 67.6 against the original's ~9 and still fastest.
All four published tables are byte-identical to the sealed `w_cliff` arm, non-cache counters are
equal, and 77/77 focused tests pass. Recorded beside the win: the profile predicted ~461 CPU-s
available and 313 were captured (**68%**), leaving ~148 CPU-s unexplained that needs a decoder
re-profile rather than another guess; and two diagnostics became estimates,
`budget_oversize_records` 1,859 → 1,666 and `decoded_bytes` about 9% lower. Neither gates anything
and no published output moved.

*Where the budget stands, post-cliff, per 100,000 reads.* Decode 1,811 CPU-s over 4 threads = 452.7 s
floor; counting 6,755 CPU-s over 24 workers = 281.4 s floor; read phase 874.6 s. Decode is the long
pole at 1.6x counting's floor, with 1.93x of in-order-commit penalty on top. `decoder_threads` is
hard-capped at 4 (`std::min<size_t>(4, requested_processing_threads / 4)`,
`src/main.cpp:4967-4968`), but decoders run only ~52% busy, so thread count is not obviously the
limit and raising the cap is not an evidenced step.

*Next steps — none started, each requiring approval.* (a) Ask the mapping pipeline owner about the
upstream question. (b) Arena allocation for decoded records, targeting the 38% and hitting both
pools (`_int_free` alone is 19.7% of counting). Arena support is already compiled into the vg
protobuf (`/mnt/ssd/lalli/vg/include/vg/vg.pb.h` carries the arena-aware generated API), but it is
**not** a drop-in: a cross-arena move deep-copies, so `Group.records` (`src/main.cpp:2010`, moved at
`:6334`) must become pointers plus a `shared_ptr<Arena>` whose lifetime spans every group drawing
from that batch; it needs a design and human approval. (c) Re-profile the decoder for the ~148
CPU-s residual. (d) Re-check the `max_buffer_bytes`/`exceeds_raw_budget` note above before treating
it as a cleanup. Three source changes are uncommitted in worktree branch
`codex/compat-replay-integration-b8210`: the validated sampled-`SpaceUsedLong` fix and the
producer-partition timers in the untracked `src/ordered_gamp_reader.hpp`, and the
`PANCOLLAPSE_INPUT_INFLIGHT_BATCHES` knob plus producer-partition receipt fields in `src/main.cpp`;
the latter two are instrumentation only. **None has been committed**, and nothing here authorizes a
source, build, dependency, or policy change, a full-data rerun, or any converter, replay, accuracy
or parity claim. Decision source: the sealed evidence roots named above.

**Terminal allocator-substitution measurement (2026-09-18; COMPLETE_VALIDATED, tcmalloc rejected,
jemalloc not established).** *Superseded as the frontier on 2026-09-21 by the entry
above: this arm set was measured on the 1M-read prefix, which sits inside the cheap regime covering
85.0% of reads but 14.2% of read time, so its 17.13% allocator share and the per-read `std::map`
next step it identified understate the post-cliff regime, where allocation is 38% of all CPU. The
measurement itself stands for the prefix.* Authority is
`/mnt/ssd/lalli/pancollapse_allocator_substitution_v1_20260918T035718Z`, STATUS
`COMPLETE_VALIDATED`, `SHA256SUMS` sealing 379 files; start with `RESULTS.md` and
`INTERPRETATION.md`, then `METRICS.json`, `provenance/time-receipts.md`,
`provenance/profile-reattribution/REATTRIBUTION.json` and `CALLERS.md`,
`provenance/preflight.json`, `provenance/tcmalloc-defaults.md`, and
`provenance/jemalloc-defaults.md`. Six sequential arms of the identical frozen binary
`6eb3e7ca…d6843a` ran on the same real first-1M-group input and configuration as the rejected
fused-state trial, differing only in `LD_PRELOAD` scoped to the native process, in the order
`glibc_a`, `tcmalloc_a`, `jemalloc_a`, `glibc_b`, `tcmalloc_b`, `jemalloc_b` (two interleaved
replicates per condition). All six arms validated PASS, the five `OUTPUT_COMPARISON.glibc_a.*.json`
files PASS, every arm published byte-identical four tables and equal non-cache counters over
1,000,000 validated complete read groups and 2,621,222 alignment records, and the container exited 0
with no OOM. Zero source, build, or link change was made. Per-arm read time was 31.487 / 29.429
s/100k groups for glibc, 89.335 / 77.414 for tcmalloc and 25.910 / 31.642 for jemalloc; count-worker
CPU 6327.10 / 5909.62 s, 15010.20 / 13195.20 s and 5077.91 / 5865.88 s; peak RSS 72.248 / 72.151
GiB, 72.665 / 72.733 GiB and 74.104 / 74.014 GiB; initialization 447.7 / 395.0 s, 467.1 / 509.8 s
and 360.0 / 363.5 s. The two identical glibc arms set the within-session noise floor at 2.059 s/100k
(6.76%) on read time and 417.48 s (6.82%) on worker CPU. The host carried unrelated concurrent work
throughout, including a 142 GiB independent count left untouched, at per-arm load1 medians of 56.98
to 60.36. **tcmalloc-minimal at gperftools 2.18 defaults is rejected**: read time +173.74% and
worker CPU +130.50% against the baseline mean, about twenty times the floor, replicated in both
rounds (+183.72% and +163.06% on read time). The mechanism is lock contention, observed in
`provenance/time-receipts.md` as 134.6M and 140.9M voluntary context switches against 3.4M for
glibc and 1808 and 1572 s of system time against 214 and 193 s; the documented default 32 MiB
process-wide thread-cache bound divided among 32 pipeline threads makes the central free list the
identifiable lock, though which lock it is remains a hypothesis. **jemalloc 5.3.0 at defaults is not
established as a read-time improvement at two replicates**: round a -17.71% read and -19.74% worker
CPU, round b +7.52% and -0.74%; against the baseline mean read time fell 5.52%, which does not clear
the 2.059 s/100k floor, and worker CPU fell 10.57%, which clears its floor but sits under a jemalloc
replicate spread of 19.92% and 14.40% that is itself two to three times the glibc floor, so it is
suggestive only. Its replicated secondary effects are initialization at 360.0 and 363.5 s against
447.7 and 395.0 s, decoder CPU at 36.5 and 39.8 s against 62.6 and 57.8 s, and peak RSS 1.86 GiB
higher at about 74.06 GiB, above the roughly 72 GiB the
sealed interpretation records as the previous high end of acceptable. The receipts rule out purge
re-faulting and preemption as the round-b cause and leave more user CPU per group in the final
third, consistent with concurrent host memory traffic and not further attributable. The hypothesis
that a drop-in allocator recovers the 17.13% glibc allocator share at zero source cost is therefore
**not supported on this host**, while the 17.13% attribution itself stands and is strengthened,
because the tcmalloc collapse is only explicable by a churn-heavy, cross-thread allocation pattern.
The 28.952 s/100k baseline from the rejected fused-state trial is confirmed host-dependent: the same
binary measured 31.487 and 29.429 s/100k here. Caller attribution over all 350,676 sealed samples
(`provenance/profile-reattribution/CALLERS.md`) resolves the allocator time: `_int_malloc` 100% via
`operator new`; `malloc_consolidate` 84.3% from `_int_free_maybe_consolidate`; `free` 57.5% from the
destructor of `std::_Rb_tree<unsigned int, std::pair<const unsigned int,
pathtally::CompactCollapsedIdentityTally>>`, 13.2% from `AssignmentCandidate::~AssignmentCandidate`,
11.4% from `std::vector<AssignmentFacts>::~vector` and the remainder from two further
`std::_Rb_tree<unsigned int, ...>` destructors, with `_int_free` adding protobuf
`RepeatedPtrFieldBase::DestroyProtos` and `vg::Mapping::~Mapping`. The sites are
`src/pathtally.hpp:196-197` (the `exon` and `body` `std::map<uint32_t,
CompactCollapsedIdentityTally>` returned per read by `collapse_ranked_identity_tallies` at `:201`),
`src/main.cpp:5276` to `:5287` (six per-read `std::map<uint32_t, ...>` instances, beside optional
maps at `:5235` and `:5236`), and `src/direct_count_assignment.hpp:93-96` (the two
`std::vector<AssignmentCandidate>` in `AssignmentFacts`). `__memcmp_avx2_movbe` (7.40% self) was
attempted with the same method and not obtained: 99.9% of its 25,993 samples have no unwindable
caller, because the assembly leaf carries no frame information for the DWARF unwinder, so it stays
an open target needing a different capture. The next bounded step this points to — identified, not
authorized, not started, and since superseded as the frontier — was replacing those per-read `uint32_t`-keyed `std::map` instances with
a representation that does not allocate per element and reusing the `AssignmentFacts` vectors across
reads, preserving any output-order dependence; it is a source change requiring its own design,
review and matched measurement with byte-identical tables, and it is not another allocator swap.
Open user decision: whether to pursue jemalloc further, which would need at least four replicates on
a quieter host, `MALLOC_CONF` tuning to recover the RSS, and instrumentation of the round-b
slowdown; promoting any allocator into the build remains a dependency change under
`.claude/rules/languages.md` requiring a decision entry and user approval, and none exists. This
measurement authorizes no source, build, or dependency change, no full-data restart, and no
converter, replay, full-input, or biological accuracy claim; the under-5-second aspiration and the
16.5-second practical endpoint remain unmet and the 1M prefix is not representative full-input
throughput. Decision source: `Research/oracle evidence`, the sealed measurement above.

**Historical allocator-substitution authorization (2026-09-17; executed, see the terminal receipt
above).** The harness root `/mnt/ssd/lalli/pancollapse_allocator_substitution_v1_20260918T035718Z`
was prepared and `provenance/run_config.json` (schema
`pancollapse-allocator-substitution-measurement-v1`) was the scope of record; at that checkpoint
`logs/` was empty and no arm had executed. Hypothesis: a large, previously
under-attributed share of native-count worker CPU is spent in glibc's allocator, and substituting a
thread-caching allocator at load time recovers part of it with zero source change. Method: sequential
unprofiled arms of the identical frozen binary `6eb3e7ca…d6843a` at
`/mnt/ssd/lalli/pancollapse_parent_metadata_v1_20260917T105800Z/provenance/panCollapse.parent-metadata`,
on the same real first-1M-group GAMP `39d4238d…e0aa8c`, same chr20–22 XG, corrected count-fact
bundle, whitelist, both profiles, 32 requested threads / 24 count workers, 64 GiB aggregation budget,
diagnostics and fine runtime timing off. Arms differ only in `LD_PRELOAD`. The recorded `arm_order`
is `glibc_a`, `tcmalloc_a`, `jemalloc_a`, `glibc_b`, `tcmalloc_b`, `jemalloc_b`: two interleaved
replicate rounds of glibc (no preload), gperftools 2.18 `libtcmalloc_minimal.so.4`
(`f3ab9ee1…385131`) and system `libjemalloc.so.2` (`567efa52…3baf12`), with `glibc_a` the reference
arm (root name timestamped UTC, 2026-09-18T03:57Z). Every condition is replicated; the repeated
arms, glibc in particular, establish the within-session noise floor, which matters because two
same-binary unprofiled baselines already differ 27.755 versus 28.952 s/100k across sessions (4.3%).
Acceptance for the measurement is byte-identical published tables and equal non-cache counters across
all arms, plus reported read s/100k, worker CPU and peak RSS. Peak RSS is a reported quantity, not a
pass criterion; a thread-caching allocator commonly raises it. The one harness check with a receipt
is `harness-smoke/VALIDATION.json`, status PASS: the arm validator and output comparator were
exercised over synthetic arms built from two sealed same-binary runs, accepting the valid case for
all six arms and rejecting wrong binary, wrong recorded preload, wrong allocator hash, group-count
mismatch, worker-count mismatch, and non-cache counter mismatch. That smoke test establishes
validator behavior only, not any allocator effect. Further pre-flight checks were reported as
passing by the orchestrator at this checkpoint and have since been persisted as the seven-check
receipt `provenance/preflight.json`, re-run after the arms launched so it is evidence rather than
assertion: the
frozen binary defines no allocator symbols itself, only `libc.so.6` among its dependencies defines
`malloc`, `libstdc++.so.6.0.34` resolves to linuxbrew GCC 15.2.0 for both the binary and
`libtcmalloc_minimal.so.4`, the binary reports `panCollapse 0.10.0` under no preload and under each
preload, and both allocator libraries already sit inside paths bind-mounted into the recorded run
container. Bounds: substitution cannot recover more than the 17.13% of user CPU attributed below, the
glibc-specific bin-maintenance part with no thread-caching equivalent is 6.19%, and an effect of that
size is close to the 4.3% cross-session floor, so the repeated-glibc arm may show that the comparison
cannot resolve it. That last caution held: the executed within-session floor was 2.059 s/100k
(6.76%) and it did not resolve jemalloc's read-time effect. Explicitly out of scope: any CMake or
link change. Promoting an allocator into the
build would be a dependency change under `.claude/rules/languages.md` and requires a new decision
entry plus user approval. This measurement authorizes no source, build, or dependency change, no
full-data restart, and no converter, replay, full-input or F1 claim. Decision source: `Orchestrator`,
under the user's existing bounded native-count optimization authorization; allocator promotion into
the build remains an open user decision.

**Symbol-resolved re-read of the sealed profile (2026-09-17; re-analysis of existing samples, no new run).**
The sealed root `/mnt/ssd/lalli/pancollapse_parent_metadata_profile_v1_20260917T152700Z` is unaltered.
This is a re-report of the same `profile/perf.data`, so the prior conclusions stand unchanged,
including exact compatibility at 29.76% inclusive CPU. The profiling container did not mount host
debug symbols, so `libc.so.6` appears in the sealed `profile/perf.self.txt` as 15 rows totalling
18.68% of self CPU. That 18.68% is the total of all fifteen visible rows, not of the unresolved ones:
13 unresolved hexadecimal address rows are 13.79% together, and the already-named `cfree` (2.45%)
and `malloc` (2.44%) are 4.89% combined. The host carries `libc6-dbg` for glibc 2.39-0ubuntu8.9 at
`/usr/lib/debug/.build-id/a4/a7992a8e66555c8141ab2a08a8465ff6e0ea65.debug`, build-id
`a4a7992a8e66555c8141ab2a08a8465ff6e0ea65`. Re-reporting with those symbols resolves the block:
`__memcmp_avx2_movbe` 7.40%, `__memmove_avx_unaligned_erms` 4.92%, `_int_malloc` 3.87%,
`malloc_consolidate` 3.62%, `unlink_chunk.isra.0` 2.57%, `cfree` 2.45%, `malloc` 2.44%, and
`_int_free` 2.18%. Those eight named rows sum to 29.45%, more than the 18.68% previously visible,
because the sealed reports were produced with `--percent-limit 0.5`
(`commands/run_profile.py`): per-address buckets below that display threshold were invisible and are
now aggregated into their functions. Self shares may be summed; inclusive shares may not.
Allocator machinery — `_int_malloc`, `malloc_consolidate`, `unlink_chunk.isra.0`, `cfree`, `malloc`,
`_int_free` — totals 17.13% of user CPU, larger than the 12.01% self share of the exact Ex50 evidence
vector, and only 4.89% of it was visible as allocator work in the sealed self report. The
`malloc_consolidate` plus `unlink_chunk.isra.0` pair, 6.19%, is glibc-specific free-list bin
maintenance with no equivalent in a thread-caching allocator. `__memcmp_avx2_movbe` at 7.40% is the
second largest self symbol overall; it is not allocator work, and caller attribution was later
attempted on it and not obtained (see the terminal receipt above), so it remains a separate open
target. These are shares of user-CPU samples only (event `cpu-clock:u`), so kernel-side allocation
cost such as page faults and `mmap` is outside the figure, and a CPU share is not by itself
recoverable time. This re-analysis adds resolution to a previously aggregated block; it invalidates
no sealed evidence and changes no earlier conclusion. The symbolized `perf report` output was not
persisted at that checkpoint; it is now sealed under the allocator root as
`provenance/profile-reattribution/perf.self.symbolized.txt`, `perf.self.libc-only.txt`,
`sealed-report.libc-rows.tsv`, and `REATTRIBUTION.json`, so every share above — including the two
memory-primitive rows `__memcmp_avx2_movbe` and `__memmove_avx_unaligned_erms` — is citable evidence
rather than a session record. Decision source: `Research/oracle evidence`,
a re-analysis of the sealed samples.

**Optimized-binary detailed profile complete and validated (2026-09-17).**
After the reviewed documentation-agent checkpoint, profiled sealed Parent-metadata binary
`6eb3e7ca…d6843a` on the same real first-1M GAMP. Evidence root:
`/mnt/ssd/lalli/pancollapse_parent_metadata_profile_v1_20260917T152700Z`.
Reuse the validated 49-Hz user CPU stack capture after initialization, detailed worker-wall
timers, `--no-inline` reports, and same-UID thread observer. Keep both profiles, graph/bundle/
whitelist, 32 requested/24 count workers, 64 GiB aggregation budget, and diagnostics-off output.
Require native/perf/observer exit 0, sampling-window acknowledgements, usable zero-loss CPU
reports, and byte-identical published tables plus matching non-cache counters against the
same-binary candidate output in the previous root. This phase identifies current hotspots;
it authorizes no implementation change, full-data restart, or F1/converter/replay performance claim.

Native count, perf, observer, and coordinator exited 0. All four tables are byte-identical to
the same-binary reference, logical profiles and non-cache counters agree, and operational cache
counts differ by one. The capture contains 350,676 CPU samples, zero lost samples, and one
retained out-of-order warning. Instrumented read time is 362.208603 s (36.220860 s/100k),
finalization 0.323943 s, initialization 444.555873 s, whole native wall 844.77 s, and peak
RSS 72.161621 GiB. Profiling overhead and shared-host conditions preclude a production speed
comparison with the previous unprofiled result.

Exact compatibility is 29.76% inclusive CPU, its mapping projection 8.01%, and state
deduplication 4.20%; inclusive shares overlap. Separately, evidence takes 41.968% of summed
worker-region elapsed time. Count workers average 19.737 cores; empty-input wait is 1.626%
of read-worker capacity and producer queue-space wait is 87.802%. Input starvation is not the
dominant limit on this prefix. The former Parent lookup hotspot is absent from leading rows.
Historical profile proposal: `INTERPRETATION.md` proposed one bounded increment to fuse projected-state insertion and
max-score deduplication in reusable worker-owned storage, preserving all StateKey fields,
first-seen order, ties, per-alignment isolation, and bounded retained capacity. Duplicate-state
frequency and attainable gain were then unmeasured. The terminal trial below supersedes this proposal.
These observations do not establish full-input throughput
or biological accuracy.

**Historical fused-state authorization (2026-09-17).** The trial implemented a reusable per-count-worker logical-state index in `exact_ex50_evidence_for`; the terminal receipt below supersedes this authorization. It preserved the stated `StateKey`, score/order, and isolation invariants, with scratch capacity reusable by its owner while logical state/evidence reset per alignment. It excluded body-path/model lookup and policy, replay, annotation, biological, full-data, and broader container/data-structure changes.

**Historical implementation checkpoint (2026-09-17).** The trial source change was confined to `src/main.cpp`, `src/score_dedup.hpp`, and `tests/policy/score_dedup_smoke.cpp`. `ExactStateScratch` was worker-owned (or serial), reused buffers/index slots, reset logical state/evidence per alignment, retained at most 8 MiB between groups, and emitted separate state-scratch observations. The preserved trial root records its initial missing-`/usr/include` configure failure and PASS harness.

**Historical build and launch checkpoint (2026-09-17).** Build, 77/77 focused tests, 1/8/32 artifacts, and readiness passed before the terminal pair. Candidate SHA-256 `e160d5ffe137667986c834e242192a6a174115718d654b9040bde59e705a1f2c`, `LAUNCH.SHA256SUMS`, and the pair receipts remain preserved in the trial root.

**Terminal fused-state trial (2026-09-17; COMPLETE_VALIDATED, rejected for performance).** Authority is `/mnt/ssd/lalli/pancollapse_fused_state_v1_20260917T164000Z/RESULTS.md` and `METRICS.json`. All four tables are byte-identical, logical profiles and non-cache counters equal; the 77/77 focused suite and 1/8/32 artifacts remain PASS. Frozen/candidate read time is 289.521504/348.267754 s (28.952150/34.826775 s/100k; 20.29% higher candidate), worker CPU 5802.09/7073.95 s (21.92% higher), RSS 72.292023/93.584633 GiB, initialization 394.595157/400.072956 s, and finalization 0.240441/0.272832 s. Shared-host timing establishes no gain but cannot isolate cause. Of 12,270,941,652 emitted states, 12,212,154,984 were retained (0.479% duplicate avoidance); near-unique states undermine this mechanism. The 8 MiB cap governs between-group retention, not transient allocation: peak worker capacity was 9,268,864,400 bytes, with 18,045 releases and 73,330,760 final retained bytes. Candidate evidence is preserved, while `provenance/SOURCE_RESTORATION.json` PASS records restoration of only `src/main.cpp`, `src/score_dedup.hpp`, and `tests/policy/score_dedup_smoke.cpp` to exact pretrial bytes. Active source and baseline remain `6eb3e7ca…d6843a`; the older 27.755 s/100k observation is historical under different conditions. The trial is complete; no subsequent optimization had started at that checkpoint (one has since been accepted; see the current section).

**Historical validation plan (completed).** The oracle and 1/8/32 artifacts passed, and the planned sequential unprofiled comparison at `/mnt/ssd/lalli/pancollapse_fused_state_v1_20260917T164000Z` produced the terminal receipt above. The shared matcher required conversion artifacts while converter code-path behavior remained unchanged. The gate remains the later machine-verifiable increment after Vertical Slice Proven.

**Parent metadata reuse implemented and validated (2026-09-17).**
The user authorized the profiled next target. Following the reviewed documentation checkpoint
and lifetime/thread-safety review, candidate construction now accepts a catalog-owned
`const ParentFact&`; the string overload delegates to it. A dense array binds existing lexical
Parent ranks to immutable catalog nodes during initialization, with key/identity checks.
Compact fallback winners retain their ranks alongside names; exact and fallback candidate
inputs use the same references. Cache keys still digest Parent identity strings and dynamic
score/tier/strand/support. Replay/capture reuses its already-validated Parent reference.
Gene/profile policy lookup and ordering remain unchanged. The dense array costs about
18.78 MiB for this bundle; no per-profile metadata expansion was adopted.

Validation authority: `/mnt/ssd/lalli/pancollapse_parent_metadata_v1_20260917T105800Z`.
Status is `COMPLETE_VALIDATED`, with native/coordinator/pair/build exits 0. All 77 focused tests
and tiny 1/8/32-thread artifact comparisons passed. The frozen binary is `06e1892e…7f84` and
the sealed candidate is `6eb3e7ca…d6843a`. Fresh unprofiled read time was 393.060940 →
277.546 seconds, or 39.306 → 27.755 s/100k groups (29.39% observed reduction); count-worker
CPU was 7584.72 → 5565.42 s (26.62% reduction). Finalization was 0.242798 → 0.246918 s,
read+finalization 39.330 → 27.779 s/100k, initialization 448.453 → 399.595 s, wall 870.61 →
708.00 s, and peak RSS 72.248 → 72.155 GiB. Four table bytes, logical profile tables, and
non-cache counters agree; operational cache hits differ by one.

This sequential pair ran on a shared host (load1 medians 31.34 → 26.88), with build/tests
overlapping part of the baseline, so the observed reduction is not an isolated causal estimate.
This is a data-access optimization, not annotation/F1 validation, converter or replay throughput
evidence, or representative full-input performance. The under-5-second aspiration and
16.5-second plausible endpoint remain unmet; fresh bounded profiling would be the next useful
measurement before choosing another hotspot. That phase has not started. Preserve the stopped
full pair and the independent older count.

**Small real-GAMP profile complete (2026-09-17, COMPLETE_PROFILE_VALIDATED).**
The profile root `pancollapse_small_gamp_profile_v1_20260917T051400Z` is terminal: native count,
perf capture, and final no-inline reporting exited 0; four count tables are byte-identical and
non-cache counters agree with the same-input/binary reference. Detailed interpretation and
source pointers are in its `INTERPRETATION.md`. On 1M real groups, profiled read time is
424.338510 s, finalization 0.293776 s, read+finalize 42.463229 s/100k, initialization
484.744215 s, native wall 943.18 s, and peak RSS 72.066700 GiB. These are diagnostic timings,
not an optimization speedup, full-input throughput estimate, or F1 result.
397,598 CPU samples (zero lost) show Parent ordered-map lookup at 16% self CPU, candidate
construction at 22.02% inclusive, and assignment rules at 4.95% inclusive. The resolve region
is 47.81% of worker elapsed time because it also contains materialization and waits; evidence
is 30.88%, tally 12.90%. Workers average 19.06 cores; empty-input wait is 1.33% of capacity,
producer queue-space wait 90.54%. The next target is immutable Parent/profile-fact references
that retain all identity/policy semantics. No production source optimization was made.
Default/explicit addr2line failures and a root-observer visibility issue were corrected only
in reporting/observation; final `logs/report_noinline.exit.tsv=0`, saved trace, and same-UID
thread snapshots are authoritative. The older independent full count remains untouched.

**User-stopped full run; smaller detailed profile (2026-09-17, historical launch record).**
The user requested stopping the full comparison and using a smaller GAMP, such as chr21,
with detailed profiling. The verified native candidate received SIGTERM; count/coordinator
receipts are 143, Docker exit time is 2026-09-17T05:09:51.441936302Z, OOM=false. Its queued
cached-serial baseline did not start. `STOPPED.md` in the full comparison root preserves the
11,845,632-group / 12,615-s last read checkpoint, 3:43:05 whole wall, and 117,540,436-KiB
peak RSS as interrupted evidence. This supersedes its automatic continuation authorization.

After the documentation-agent checkpoint, bounded discovery found no manageable genuine
chr21-only GAMP; the existing 151 MB chr21 file is simulated HG002. An optional user preference
question remained unanswered, so the parent stated and used the real-subset assumption:
the existing 261,652,666-byte genuine ARC first-1M-complete-group input, 2,621,222 records,
SHA `39d4238d…e0aa8c`, matched to the existing chr20–22 graph/bundle/whitelist. It is useful
for local hotspot diagnosis; its already-rejected full-workload representativeness is not
reinstated. No new biological accuracy or whole-pangenome performance claim follows.

Authority: `/mnt/ssd/lalli/pancollapse_small_gamp_profile_v1_20260917T051400Z/README.md` and
`provenance/run_config.json`. Frozen `06e1892e…7f84` runs CR7+strict, 32 requested threads,
24 count workers, 64 GiB aggregation budget, per-read output diagnostics off, and detailed
`PANCOLLAPSE_PROFILE_TIMING=1` regions. User-space perf CPU stacks run at 49 Hz with DWARF
unwind; external markers exclude initialization and bracket read processing, with actual
acknowledgement times in `profile/WINDOWS.json`. Worker-wall regions include waits and must
not be described as CPU percentages. A Docker CAP_PERFMON profiler records the native
process, which drops to workspace UID/GID; no host sysctl changes are made.

Harness smoke passed native/perf/coordinator exits, phase controls, and 140 symbolized samples.
Preflight passed with 421,780,467,712 available host bytes. The 256 GiB no-swap container
`pancollapse-small-gamp-profile-20260917T051400Z` started at 2026-09-17T05:19:59.208859207Z;
initialization is live. A 40-minute bound preserves failed/timeout evidence. After native
completion, compare published count tables and non-cache counters to the completed identical
input/binary result, then interpret stack samples with region and thread timing. No source
optimization, rebuild, policy change, or new mapping is part of this phase. The older independent
full-data run remains untouched.

**Full real-input comparison (2026-09-16 local / 2026-09-17 UTC, historical launch; now user-stopped).**
The user rejected the first-1M prefix as representative and authorized measurement on real
data. After the documentation-agent checkpoint and parent review, the complete existing
ARC GEX `site1_donor2_asian/d10k_cap20k` input launched: 25,755,749 read groups,
127,202,588,396 bytes, GAMP SHA-256
`000858121cdcbbc09364e423586ca57324e19c8ef0ad968a3308c91b7e7e87fe`.
Authority: `/mnt/ssd/lalli/pancollapse_parallel_input_full_real_v1_20260917T011600Z/README.md`
and `provenance/run_config.json`. This complete selected readset uses the matching chr20–22
graph and corrected count bundle; it does not establish whole-pangenome performance or F1.

The frozen ordered-input binary `06e1892e…7f84` runs first. Only after its native exit and
terminal validation succeed will cached-serial `e189ba54…4dba` run automatically on the same
full input. Both have the node-tally cache, CR7+strict, 32 requested threads, a 64 GiB
aggregation budget, diagnostics/fine profiling off, and a 256 GiB no-swap Docker limit.
The candidate uses 24 count workers plus the input pipeline; frozen uses 32 count workers
and serial input. No production source, policy, mapping, converter, replay, or annotation
change is part of this experiment. The old full `3d13…f958` run is independent and preserved.

Preflight is PASS with 703,372,435,456 available host bytes. Container
`pancollapse-parallel-input-full-real-20260917T011600Z` started at
2026-09-17T01:26:45.690675015Z; the native candidate is live. Commands/configuration are
frozen in `provenance/LAUNCH.SHA256SUMS`. Reporter smoke validation passed against adapted
completed receipts and rejected a deliberately wrong active-worker count. These are harness
checks, not completed full-data results. `STATUS`, `logs/{arm}.stderr`, `.time`, `.exit.tsv`,
and `.validation.exit.tsv` track execution; final `METRICS.json`, `RESULTS.md`,
`OUTPUT_COMPARISON.json`, and `UTILIZATION.json` remain pending. Report initialization,
read processing, finalization, publication, whole wall, RSS, and utilization separately.
Normalize by all completed groups; retain shared-host load context. Next: monitor this
sequence, review terminal validation, reconcile documentation, and seal completed evidence.

**Ordered parallel native input (2026-09-16, implemented and measured; no demonstrated speedup).**
The user's "Do it" authorizes the reviewed input/dispatch implementation and bounded
measurement. The documentation checkpoint is recorded in PROGRESS and the performance
research record. `src/ordered_gamp_reader.hpp` uses VG framed messages, parallel protobuf
decoding, and caller-thread ordered delivery to the existing grouping callback. It preserves
wrong/tag-only filtering, valid prefixes before failures, recurrence validation, and stream
ownership through hash completion. At 32 requested threads, it budgets 24 counting workers,
four decoders, two BGZF workers, one BGZF I/O helper, and main; native requests below eight
active threads retain serial input. Converter and replay input paths are unchanged.

The 64 MiB buffer setting limits admission, with eight in-flight slots at 32 threads;
already-admitted protobuf expansion, current parses, VG buffers, and complete read groups
can exceed it. Partial decoded memory is tracked, and total RSS must be measured.
All 77 focused tests passed, as did count/compatibility/RAD/diagnostic/BAM-content comparisons
at 1/8/32 threads and native file/stdin hash, cross-batch grouping, recurrence, and malformed
molecule checks. One initial build-scope error and one mislabeled malformed-input fixture
were corrected; their original failed receipts are preserved. Parent review subsequently
found an admission-triggered error-ordering defect missed by the first tests/review. The
inner catch now defers only framing-advance failures; callbacks and parse failures reached
during admission cancel immediately. New slot/byte-pressure regressions match the exact
serial prefix/error, and all 77 tests and integration checks passed again. The first candidate
was stopped during initialization (exit 143); its evidence remains in `attempt-1/` and is
excluded from timing results. Main-thread CPU includes read-phase setup; the new
empty-queue timer measures worker admission wait, distinct from job residence in the queue.

Trial root: `/mnt/ssd/lalli/pancollapse_parallel_input_v1_20260916T164124Z`.
Fresh frozen baseline is the prior cache binary `e189ba54…4dba`; candidate is
`06e1892ea613d7d7e4212f43f27a015d1b8ebbc6291e41c6340aa3e0cc467f84`.
Both use the preserved real first-1M-group input `39d4238d…2e0aa8c`, corrected count bundle,
matching graph and whitelist, CR7+strict, `--threads 32`, 64 GiB count budget, diagnostics
and fine timing off. Both native exits and final report validation are 0/PASS. Read time
was 436.827720 → 558.911560 s (**43.682772 → 55.891156 s/100k; +27.95% observed**).
Finalization was 0.325313 → 0.415605 s; combined rates 43.715303 → 55.932752 s/100k.
Peak RSS was 72.096287 → 72.218716 GiB, initialization 497.297147 → 455.994004 s,
and whole wall 967.47 → 1053.52 s. Four count tables are byte-identical; all non-cache
counters and exact-model candidate counts agree. Neither <5 nor 16.5 s/100k is met.

Candidate counting workers used 19.7566 core-equivalents across the read phase and
waited on an empty job queue for 0.6688% of their wall-time capacity (including wake-up
delay). Decoders used 74.2963 CPU-seconds, about 0.133 core; main used 39.183 CPU-seconds.
Stable sampled intervals show 22.5022 count-worker cores for the frozen 32-worker arm
versus 19.7851 for the 24-worker candidate. This supports adequate input supply in this
prefix; it does not identify the residual computation/lock/scheduling split. Buffer accounting
peaked at 153.257 MiB despite the 64 MiB admission watermark. Total RSS remains the
appropriate overall memory measure. Whole-command host load-1 medians were 97.52 and
122.625, with ranges 79.32–115.35 and 107.85–128.22; this sequential pair does not isolate
the causal effect of input parallelism or thread allocation.

The admission regression has old-header FAIL / corrected-header PASS evidence under
`trial/admission-regression/`; empty native input also passed at 8/32 threads. The first
reporter incorrectly expected manifest `threads=32` in both arms; that field stores active
count workers (candidate 24). Its failure is retained under `logs/report-attempt-1/`, and
`commands/report_only.sh` validated the existing outputs after correction without rerunning
counting. The bounded implementation/measurement phase is complete. The worktree retains
the measured uncommitted candidate; it is not promoted as a validated speed improvement.
Final audit exited 0 and verified 678 files in `COMPLETION.SHA256SUMS`; terminal status
is `COMPLETE_MEASURED_NO_SPEEDUP`. Further counting-side attribution/thread tuning remains unexecuted and needs a fresh
documentation checkpoint. This mechanical change does not constitute a biological accuracy
result or a whole-pangenome speed claim.
The independent full-data count remains untouched. Earlier discussion-only boundaries
below are historical and superseded by this explicit authorization.

**Input/dispatch API inspection (2026-09-16, discussion only).** Installed VG primary
sources confirm that `vg::io::MessageIterator` accepts a decompression thread count,
and `vg::io::ProtobufIterator::parse_from_string` decodes a complete serialized record
using VG's large-message limit. `vg::io::for_each_parallel` combines parallel decoding
with callbacks in unspecified order. It therefore cannot replace the current
`for_each` call while leaving an unordered callback to mutate contiguous read-group
state. One owner must retain grouping/recurrence validation and observable input order.
The API also requires exclusive stream access while threaded `MessageIterator` exists;
input-hash completion must follow its destruction. Exact installed sources are
`/mnt/ssd/lalli/usr/local/src/vg/include/vg/io/{message_iterator,protobuf_iterator,stream}.hpp`.
This inspection supports the requested explanation; no new reader architecture was
accepted, production code changed, or parallel-input experiment run.

**Read-only worker-utilization observation (2026-09-16).** To answer whether 32 workers
are computing or waiting, a 60.000186-second live read-phase sample of the original
full-count baseline (`3d13…f958`) collected 121 `/proc` snapshots without attaching,
stopping or restarting it. The 32 workers used 237.318824 CPU-seconds, averaging
3.9553 CPUs (12.3603% of their combined capacity). Of 3,872 worker observations,
3,401 were sleeping and 3,394 were in futex wait. The single main/input thread used
55.592096 CPU-seconds (92.6532% of a CPU). These establish substantial worker
underutilization in that minute; they do not identify the queue/lock causing sleep.
The producer decodes/groups GAMP serially, making input delivery a hypothesis to
test. Native-only output ordering is disabled. Kernel scheduler statistics were
disabled; raw runqueue deltas do not quantify scheduler delay. The cgroup was not
CPU-quota throttled. The terminal cached 1M candidate is a separate binary/workload
measurement, so this is not a post-cache utilization claim. Exact artifacts and
limits: `/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/worker-utilization-v1/README.md`.
No counting or biological policy changed. User-space wait attribution remains unexecuted.

**Authorized node-tally cache experiment (2026-09-16, terminal validation PASS).** The user
clarified that the requested cache is node-to-score-table work within each read. This
supersedes the earlier no-code-change boundary for this bounded optimization. After the
documentation checkpoint and arithmetic/pointer-lifetime review, `src/node_tally_cache.hpp`
and `src/main.cpp` now aggregate score and oriented bases by node, fan out once through
all raw reference-path occurrences, and memoize immutable `NodeLedger` metadata for exact
matching. Zero-valued entries, duplicate path occurrences, path orientation, and subsequent
Parent/transcript maxima remain intact. Checked regrouping overflow requests the original
iteration order. Assignment-cache capacity remains 131,072.

Frozen candidate `e189ba54c69d4a33c73364313e860a437585f93e0519c4fb7978bb2444ab4dba`
passed 76/76 focused tests plus baseline/candidate count, compatibility, RAD, diagnostic,
and BAM-content comparisons at one and four workers. Only BAM `@PG CL` command provenance
is normalized. Trial root: `/mnt/ssd/lalli/pancollapse_node_tally_cache_v1_20260916T125145Z`;
see `small-artifact-validation-v2/VALIDATION.json` and `logs/focused-tests.log`.
The first-1M-complete-real-group pair exited 0 and passed `OUTPUT_COMPARISON.json` and
`METRICS.json`: read time **630.178831531 → 552.256649014 s**, finalization
**1.017057797 → 0.952593026 s**, read plus finalization **63.119589 → 55.320924 s/100k**,
and peak RSS **72.160686 → 72.110222 GiB**. Initialization was
**516.848925 → 593.710554 s**, publication **0.339887 → 0.296603 s**, and whole wall
**1201.76 → 1197.19 s**. The 12.365% lower observed read time is one sequential
shared-host pair (load-1 medians 105.53 versus 94.7); it is not an isolated causal
estimate or whole-input scaling claim. Both runtime targets remain unmet. Four tables
are byte-identical, non-cache counters/profile logical tables agree, and exact-model
prefilter work is identical (626,965,346 before; 340,444,150 after). Summary differences
are limited to operational assignment-cache counters (four hits). Supplemental sanitizer
validation is unavailable with the installed Abseil toolchain; preserve
`sanitizer-test/RECEIPT.json`, without claiming a sanitizer pass or candidate failure.
The remaining read-stage attribution task is unexecuted; no further experiment was launched.
The original
25,755,749-group count continues separately. The mistaken assignment-capacity trial is
stopped, its changes reverted, and its outputs preserved in
`/mnt/ssd/lalli/pancollapse_cache_trial_v1_20260916T124251Z`. No biological policy or accuracy
claim follows from this engineering comparison.

**Real-input measurement phase (TERMINAL, validated 2026-09-18; seal pending; historical text
follows).** The prior measured
fallback phase is complete and sealed in
`/mnt/ssd/lalli/pancollapse_native_larger_3m_v1_20260915T231536Z`: 75 files verified,
completion-seal exit 0. Its 100k/1M/3M outcomes below remain historical authority.
The user has authorized a new bounded real-input measurement, not a scientific-policy
change. Preflight selected the sealed genuine 10x ARC GEX `site1_donor2_asian` GAMP at
`/mnt/ssd/lalli/hprc_v2_vg_rna/chr20_compare/readsets/real_scrna_benchmark_v1/pangenome_mapping_reuse_v15r2_20260908T150525-0500/site1_donor2_asian/d10k_cap20k/mapping/reads.gamp`
(25,755,749 groups, 127,202,588,396 bytes, SHA-256
`000858121cdcbbc09364e423586ca57324e19c8ef0ad968a3308c91b7e7e87fe`). Its chr20--22
XG and ledger match corrected-bundle content `sha256:f1ffba4d4fe821e6bda6d0b88bba3cfe5df9df5fb263efe099e27fe14146c99e`;
ledger SHA-256 is `73c7a7a03abcbb9eb795c3ac2b1141cca82333c73134b190ee1a30be8657f10c`.
The tier has 6,211 eligible cells and pre-mpmap A/B/C buckets, so it is not a
whole-pangenome claim. GAMP names carry raw GEX CR; the resolved, newly pinned
ARC GEX whitelist is in the new run root, not the original producer. Status at launch was
`COUNT_RUNNING` (the run is now terminal; see the header of this paragraph): parent preflight passed (`provenance/preflight.json`,
`logs/preflight.exit.tsv=0`, available bytes 414005080064). Container
`pancollapse-real-native-20260916T022706Z` (ID
`e465ff51a2d64dda303c0ad61c45725ce65a2a34a8c2ab9e3569599ddd8d429b`) started
2026-09-16T02:41:19.314463216Z. Monitor the run root's `logs/count.stderr`, `count.time`,
and `count.host.samples`; terminal receipts are `logs/count.exit.tsv` and
`logs/coordinator.exit.tsv`. It runs 32 workers, CR7+strict, a 64 GiB count aggregation budget,
and a 256 GiB no-swap Docker limit, with diagnostics off and fine timing unset. Output is
`counts/`; validation remains pending. Do not duplicate or restart.
At 2026-09-16T05:58:12Z, its latest recorded live checkpoint was 7,290,880 groups in 11,292.7 s
(645.625 groups/s; 154.88802 s/100k cumulative read-only partial). The 16.5 s/100k full-input
target is already missed: its entire read-plus-finalize budget is 4,249.698585 s, below the
read time already elapsed. This leaves the prior synthetic result intact; the real run is
still active, not a completed benchmark. RSS was 100,498,312 KiB (95.84266 GiB), not
terminal peak. Initialization/finalization are excluded; throughput is not stable and neither
value estimates whole-run wall time. The follow-up-only worker snapshot records waits, but does
not establish an actual cause or deadlock.

Use only frozen resident
`/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z/provenance/panCollapse.resident-finalize`
(SHA-256 `3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`; worktree
build is older). Measure initialization, native read plus count-finalization per 100k,
diagnostic merge, publication, whole wall time, and peak RSS. Real-data performance
and RAM do not prove truth-based accuracy; no mapping, converter, replay, annotation,
or biological-policy change is authorized. The gate remains the later machine-verifiable
increment after Vertical Slice Proven. Next action: monitor this single count, then perform
terminal validation and report its measured results.

**Fixture-versus-real interpretation (2026-09-16, interim).** Preserve the completed
synthetic measurements as workload-specific evidence, not a production throughput
estimate. The active run's `provenance/FIXTURE_COMPARISON_INTERIM.md` records the 3M
fixture's 96.5469% assignment-cache hits, the full-input 41.596-fold encoded bytes/group
difference, cache builder scope and locking, and the limits of worker-wait observations.
Real cache reuse and causal cost fractions remain unmeasured. These observations do
not authorize a biological-policy change or imply an accuracy result. The existing
full count continues; no profiling or code-change phase was started.

**Concrete structural attribution (2026-09-16).** After the documentation-agent
checkpoint, bounded decoding of the first 20,000 complete QNAME groups from each input
measured 130,161 versus 2,494,196 node mapping entries (fixture versus real, 19.16x).
The most complex 1% of real groups accounts for 69.05% of those entries. Real group
7,501 is a repetitive 90-base read with six records, 42,311 reachable subpaths, and
298,198 mapping occurrences across 1,338 distinct nodes. Main's tally loop visits
each mapping and its reference paths; exact matching also traverses the subpath DAG,
both before assignment-result caching. These are concrete extra operations; the
fraction of whole-run time they explain remains unprofiled. Prefix samples are not
random or whole-input estimates. Details, scripts, validation, and source pointers:
`/mnt/ssd/lalli/pancollapse_real_native_count_v1_20260916T022706Z/attribution/RESULTS.md`.
No new native count or production/policy change was made; the existing run continues.


**Full-goal measured-fallback follow-up (COMPLETE_FALLBACK_VALIDATED).** The
3M baseline--resident pair is terminal PASS after the candidate-only resume exited 0
at 2026-09-15T23:52:51.781Z and `final_audit.py` exited 0. Completion integrity is
recorded by `COMPLETION.SHA256SUMS` and `logs/completion_seal.exit.tsv` in the final
3M root. Final authority is
`/mnt/ssd/lalli/pancollapse_native_larger_3m_v1_20260915T231536Z/RESULTS.md` and its
`GOAL_ACCEPTANCE.md`, superseding the earlier 3M-pending checkpoint.

The recovered GAMP `tmp/chr202122_subset/reads.gamp` freshly verifies to
`3d857f957b5c585c97784e5e583dc166b63f59f8cf35045055b15e1aea708a67`, with current
XG `1568ff…9849d6`, ledger `73c7…7f10c`, fixed corrected-bundle content ID
`sha256:f1ff…c99e`, 32 workers, 64 GiB, CR7, and strict profiles. Both frozen binaries
passed fresh identity checks: baseline `e48c7d…ced2` and resident `3d13e0…f958`.
All four Parquet tables and the summary are byte-identical; profile/non-cache and
cache counters agree; source hashes passed. This is same-authority engineering
parity, not independent F1 evidence. The original exit-1 identity-verifier defect,
zero-byte receipt, and complete baseline output are preserved.

3M baseline/resident combined timing was 279.926737/297.215823 s
(9.330891/9.907194 s/100k); resident is **6.176290% higher observed**. Finalization
fell from 7.801599 to 4.729907 s, while read rose from 272.125138 to 292.485916 s.
RSS was 53.473888/53.598610 GiB. Whole-command load-1 medians were
157.855/183.04 (ranges 143.77--171.78 and 150.68--201.62); shared-host conditions
bound interpretation and do not identify cause. No 3M speedup is claimed.

The fallback remains supported by repeated 1M resident **10.878205 s/100k** median
(range 10.064581--11.691829), 27.52% lower observed than repeated baseline. Standalone
100k remains 21.427850 s versus 20.355946 baseline (5.27% higher). Preferred <5
s/100k remains unmet and whole-pangenome/20M scaling unproven. Focused tests are
74/74 PASS. The next unexecuted work is read-path cost per 100k and matched 20M
validation after the required documentation checkpoint; converter/replay/F1 behavior
is unchanged. Subsequent D089 detail is historical phase record unless explicitly
updated above.

The next unimplemented profiling target is temporary allocation/deduplication in
`src/main.cpp::exact_ex50_evidence_for` (`provenance/NEXT_BOTTLENECK.md`); do not skip structural
tally because fallback assignment requires it. Do not linearly infer 20M finalization from 0.485 s
at 100k versus 17.857 s at 1M.

**Historical completed-phase authorization (superseded).** Begin the bounded `exact_ex50_evidence_for` allocation/
deduplication optimizer preserving comparator-equivalent identity, maximum-score retention,
Parent filtering, score-window/final comparator order, aggregation across records, structural
tally, and fallback inputs. Reuse remains worker-local and bounded; report RAM. Measure read
processing and count finalization separately and combined, and investigate finalization scaling.
The phase finishes with combined native counting under 5 s/100k excluding initialization, or
evidence-backed combined performance of about 16.5 s/100k after optimization. Work cannot be
shifted into finalization to satisfy the target; debug merge/export remains separate.
Same-authority output determinism is required, while cross-authority strict parity is not. The 1M
chr20--22 roughly 52.1 GiB RSS is already high and does not establish whole-pangenome RAM. The
results workspace `/mnt/ssd/lalli/pancollapse_exact_evidence_perf_v2_20260915T205001Z` is
`BENCHMARK_IN_PROGRESS`. The implementation adds `src/score_dedup.hpp` in-place stable
maximum-score state deduplication with contiguous alignment-scoped open-address scratch;
`src/main.cpp` retains all `StateKey` fields and filter/window order, and
`src/direct_count_runtime.cpp` adds optional finalization profiling. The 2,000-case seeded map-
oracle smoke covers collisions, order, scratch reuse, and split records; 74/74 focused tests pass.
Candidate `provenance/panCollapse.exact-dedup` has SHA-256
`e7434ac59a2fb3c5acf688ba9ca1a28827ceac14f49b2f5c78109561004809ae`, with
`provenance/source.exact-dedup.tar.gz`; the worktree build remains untouched. Live Docker pair
`pancollapse-exact-initial-pair-20260915T205001Z` has both terminal exit-0 exact-biology and
input/execution-identity PASS arms. Baseline is 18.784944554 s read, 0.542118419 s finalization,
and 19.327062973 s combined; candidate is 21.794378933 s, 0.929468246 s, and 22.723847179 s.
Candidate is 17.6% slower in this initial pair: preserve it as no-speedup evidence, with no
promotion or goal completion. `RESULTS.100k.md` in the results workspace is the 100k receipt. Sol review
is ACCEPT; helper map-oracle ASan/UBSan passed but is not full native-runtime coverage. Candidate
1M is terminal PASS for all four biological tables, summary, non-cache counters, and
input/execution identity: 567.421721609 s initialization, 119.747233383 s read, 13.392732078 s
finalization, 133.139965461 s combined (13.3139965461 s/100k), 1.48518 s publication, 730.67 s
wall, and 51.9642486572 GiB RSS. Its first 1M baseline is also terminal exact-correct at
154.928523385 s combined (15.4928523385 s/100k; 141.787332656 s read and 13.141190729 s
finalization). Candidate is about 14.1% lower in this pair only, while the 100k candidate is 17.6%
slower; neither established speedup nor goal completion follows. A read-only
investigation concludes diagnostics alone cannot reconstruct `CountRuntime` observations exactly;
no harness/parser change exists. Existing `--compatibility-in` can profile finalization separately
without graph initialization and was not run. The 100k UMI-collapse measurement is 0.0459925 s, so
quadratic UMI cost is not presumed. The terminal candidate-then-baseline 1M pair is Docker
`pancollapse-exact-1m-pair-20260915T205001Z`, container
`7e296abb2d881b0c11fd90ab163caccf84a612dfcafbbc668d91fbf9ca430e3b`; it is terminal original-
pair evidence. The resident-finalization implementation is now in `src/direct_count_runtime.cpp`:
checked one-time reserve per resident vector and omitted redundant first exact sort; final exact
sort/combine, deferred sort, and spill stay unchanged. Focused tests are 74/74 PASS; frozen binary
`provenance/panCollapse.resident-finalize` SHA-256 is
`3d13e082de68b513ddc4de5689585bb2428d9cbfc104f1f88bd1fd02d7d0f958`. Build/test succeeded, but
the Docker Git-metadata wrapper exited 129; its failed receipt is preserved and host recovery
passed. First resident 1M is terminal exact-correct at 113.148701764 s read, 3.769589644 s
finalization, and 116.918291408 s combined (11.6918291408 s/100k), with 51.9621620178 GiB RSS;
resident prepare is 0.515448 s versus original-candidate 10.2197 s. This single observation
numerically meets fallback but does not establish reliable gain or completion before repeats,
debug, and `ACCEPTANCE.md` audit. First resident 100k is terminal exact PASS at 20.449478765 s
read, 0.664120164 s finalization, and 21.113598929 s combined, slower than baseline and not meeting
fallback at 100k; mixed-size evidence remains insufficient. Resident 100k-r2 is terminal exact
PASS at 21.146822184 s read, 0.595278286 s finalization, and 21.74210047 s combined. The two
resident 100k values span 21.113598929--21.74210047 s (median 21.4278496995), above standalone
fallback. Baseline 100k-r2 is terminal exact PASS at 21.384829 s combined; baseline 100k median
is 20.355946 s versus resident 21.427850 s (5.3% higher observed). Resident 1M-r2 is terminal exact PASS at 96.659688 s
read, 3.986118 s finalization, and 100.645807 s combined (10.064581 s/100k), with input identity
PASS. Two 1M values span 10.064581--11.691829 s/100k (median 10.878205); 100k remains
21.113599--21.742100 s (median 21.427850). This is not a causal speedup or full-scale claim.
Baseline 1M-r2 is terminal exact PASS at 145.251348 s combined and 52.036316 GiB RSS.
The repeated 1M baseline median is 15.008994 s/100k versus resident 10.878205 (27.5% lower
observed); standalone 100k remains 5.3% higher. Six of seven scheduled arms pass;
only complete debug 100k remains in Docker
`pancollapse-resident-repeats-20260915T205001Z`, container
`7aebee643a0e5058cf66ce28de73a8070570f9bf09a4503a634a045930181507`; debug 100k began
22:53:56Z. Poll it, do not duplicate it. `REPEATS_REPORT.md` separates PASS/PENDING/FAILED and
host load. Baseline CPU sampler ended before counting, so CPU window comparison is unavailable.
`provenance/REMAINING_PROCESSING_COSTS.md` records first-1M evidence 53.76%, resolution 18.98%,
and tally 15.11% of summed worker wall; historical `worker_queue_wait` is job-queue residence, not
worker idle.
`provenance/PERFORMANCE_ACCEPTANCE_SCOPE.md` clarifies that the original 1M baseline authority is
18.011 s/100k. Normalized 1M throughput may satisfy fallback only after remaining repeat, exact-
output, RSS, and complete-debug evidence; the failed 100k observation remains adjacent and bars a
blanket <=16.5 s/100k claim. Larger-input validation is conditional on suitable existing inputs,
not a new mapping or mandatory 20M gate; scaling remains unproven.
Bounded fixture search found no ready matching authority above 1M, not global absence; no F1 or
whole-pangenome claim exists. The later applicable gate is machine-verifiable behavior after
Vertical Slice Proven; this phase does not advance it.

### D090 — Make the read the unit of decode and count

**Status: ACCEPTED** (User, 2026-09-23). Implement for plain native `count` first, then extend
to `--rad-out` and the compatibility writer with a bounded reorder stage. Proceed directly to the
redesign; the `max_buffer_bytes` knob trial is not run. Fork (3) is not decided: the user asked
for an illustrated expansion of the score-summing observation before judging it.

**Context.** The mapping pipeline owner confirmed that the post-cliff alignments (p50 6, p90 261,
p99 15,295 subpaths per alignment on `w_cliff`) have no known upstream cause and that some genome
regions will always produce them. The question is therefore how native `count` processes dense
regions without stalling, not whether they will go away. Across the cliff, wall time per read
scaled 73.8x against 29.9x CPU; the gap is waiting.

**Finding.** Only the input reader orders work. It decodes whole batches on one thread each and
commits them in order, solely to group QNAME runs and assign ordinals; counting and the count
tables are order-independent. `max_buffer_bytes` (64 MiB) is checked at admission, but decoded
bytes grow about 28x afterwards, so about two batches are admitted and the producer then waits on
the oldest (93.5% of the read phase).

**Proposal.** Recover each record's `name` (field 3, ahead of `subpath`) from the serialized
bytes at the producer, group and assign ordinals there, and let each count worker decode, count
and discard one whole read, with a per-worker arena reset per read. This removes the ordered
commit, the separate decoder pool and decoded-byte budgeting, and makes arena allocation free of
cross-thread lifetimes. The gain is bounded by the wall/CPU gap (at most about 2.5x on dense
input); the remaining CPU work is addressed by follow-ons. Full design, limits and verification
plan: `docs/research/read-granular-decode-design-20260923.md`.

**Result (2026-09-23).** Implemented for native `count`. On `w_cliff` the read phase fell from
1,145.6 and 890.6 s to 222.8 and 207.7 s, idle from about 58% to 0.02%, and total CPU from
10,973 and 9,084 to 6,000 and 5,611 CPU-seconds; count tables byte-identical to the sealed
baseline and all global counters equal. The design's "at most about 2.5x" ceiling assumed CPU
would not change and was wrong. Evidence: `/mnt/ssd/lalli/pancollapse_read_granular_v1_20260923T160346Z`.

**Arena and ordered output (2026-09-23).** Per-worker arena decode cut `w_cliff` worker CPU a
further 18% at matched load. A bounded reorder stage replaced the output turn for `count
--rad-out` and the compatibility writer: `--rad-out` on `w_cliff` went from about 1,320 to 1,510 s
to about 155 s with byte-identical `map.rad`. `convert` keeps the turn and producer-side decode.
Evidence: `/mnt/ssd/lalli/pancollapse_decode_arena_v1_20260923T173617Z`,
`/mnt/ssd/lalli/pancollapse_rad_reorder_v1_20260923T182610Z`.

**Fork (3), expanded (2026-09-23).** The summed per-node tally decides no `cr7-v1` assignment and
about 0.9% of `pansc-strict-v1` assignments in two dense 2,000-read windows (via the
exact-strand Gene fallback); it does select RAD target sets. Illustrated write-up:
https://claude.ai/artifact/JNas3Gek3XzpPFn5D3LS2m ; evidence:
`/mnt/ssd/lalli/pancollapse_branch_summing_v1_20260923T160346Z`. Undecided.

**Open forks as raised.** (1) Plain native `count` only, or also a bounded reorder stage for `--rad-out`
and the compatibility writer, whose ordered sink stalls the pool behind one dense read.
(2) Run the `max_buffer_bytes` knob trial first as a measured floor, or proceed directly.
(3) Confirm that summing node scores across mutually exclusive subpath branches is the intended
D048 reading on very dense DAGs; recorded as an observation, not a proposed change.

### D091 — Score each alignment path separately; the best path wins

**Status: ACCEPTED** (User, 2026-09-23). Amends D048 steps 2-3.

**Context.** D048's structural tally visited every mapping of every subpath of every record of a
read and added each node's score to every HST crossing it. On a multipath alignment the subpaths
include alternatives for the same read bases, so a node listed in k alternatives was credited k
times, and separate records of one read were added together. On dense post-cliff reads the summed
score reached hundreds of times vg's optimal alignment score (D090 fork 3,
https://claude.ai/artifact/JNas3Gek3XzpPFn5D3LS2m). The exact Ex50 DP already scored one path at a
time; the summed tally drove RAD targets, compatibility structural facts and the
`pansc-strict-v1` exact-strand Gene fallback.

**Decision (user's words).** "I want each alignment path to be scored separately using this method,
and then the best alignment(s) is found among all alternate alignments." Asked how to break a
conflict between the best path overall and the best path for a transcript: "Score every (alignment
path, transcript) and choose the argmax score, then the transcripts found in those (alignment path,
transcript) tuples regardless of the path." Also: cache node results to minimize lookups.

**Rule.** For HST h, score(h) = max over records r and source-to-sink paths P of r that visit h of
the sum of per-node scores on P at nodes h traverses (once per h step, as before). Targets are the
HSTs, collapsed through Parent and canonical transcript by MAX, at the global top score plus ties.
Orientation evidence comes from the maximizing path, ties broken by more aligned bases, then more
forward bases. Paths that never visit h do not give h a score of 0; an HST whose every visiting path
is negative keeps its (negative) best.

**Implementation.** Dynamic programming over each record's topologically ordered subpaths; HSTs
with identical node visits within a record are grouped exactly and scored once; node-to-HST lookups
use the existing per-read and shared node caches. `PANCOLLAPSE_STRUCTURAL_TALLY=summed` restores the
D048 tally for comparison. Reads whose every record has one subpath and whose read has one record
score exactly as before.

**Result (2026-09-23).** Evidence: `/mnt/ssd/lalli/pancollapse_route_tally_v1_20260923T204025Z`
and `/mnt/ssd/lalli/pancollapse_route_tally_truth_1m_v1_20260923T204147Z` (`RESULTS.md`). `cr7-v1`
is unchanged everywhere. `pansc-strict-v1` changed 1,417 of 100,000 `w_cliff` reads, 1 of 100,000
`w_precliff` reads and 65 of 1,000,000 truth-benchmark reads, where true positives rose by 60 and
false positives fell by 2 (read F1 96.98831% to 96.99155%). Cost: read phase +35% on `w_cliff`,
+27% on `w_precliff`, before any single-route fast path.

**Open (RAD output).** RAD keeps only reads whose top transcripts belong to one gene. With honest
ties, a read's best alignment ties across a median of 657 gene identities, only 3 of them reference
genes; the rest are per-assembly and Liftoff copies. RAD records on `w_precliff` fell from 25,342 to
21. The summed tally kept about a third of reads only because loci hit by more of a read's ten
alignments won. Which gene identities count as distinct for RAD's multigene rule is undecided.

### D092 — Count the rDNA host lncRNAs ENSG00000280441 and ENSG00000278996 as rRNA

**Status: ACCEPTED** (User, 2026-09-25). Annotation correction; no code change.

**Context.** On the fastp-filtered real reads (site1_donor2_asian), `cr7-v1` put 1,209,383 of
2,597,568 UMIs (46.6%) in ENSG00000280441, against 127,794 in STARsolo GeneFull_Ex50pAS on GRCh38 and
2 on CHM13. The gene is a HAVANA lncRNA on GRCh38 chr21:8,380,643-8,454,792 whose transcripts join
shared first exons to copies of one exon block in two rDNA units 44 kb apart, so its introns contain
whole 18S/5.8S/28S units. Intron-inclusive counting therefore credits rRNA reads to it; STARsolo's
exonic `Gene` feature gives it 7 UMIs. In the CHM13 CAT annotation the same gene is only the 1.5 kb
exon block, with alternate copies on the chr13, chr14, chr15, chr21 and chr22 rDNA arms. Evidence:
`/mnt/ssd/lalli/star_vs_pancollapse_filtered_20260924/README.md`.

**Decision (user's words).** "Mark that gene as rRNA."

**Implementation.** In the count-facts bundle, the two `profile_gene_policy.tsv` rows for
ENSG00000280441 become `rRNA` / `FALLBACK` / `small_rna`, the classification the bundle already gives
annotated rRNA genes; FALLBACK genes are never emitted by either profile and lose to any PRIMARY
candidate. For the STAR arms, `gene_type`/`transcript_type` (`*_biotype` on CHM13) become `rRNA` on
every row of the gene before the Cell Ranger 2024-A filter, which then drops it. Bundle:
`/mnt/ssd/lalli/star_vs_pancollapse_filtered_20260924/rdna_host_as_rrna/count-facts` (content_id
`sha256:bfd7d097997fe366e1b615a321e6d3047e4e861b0442eb24ea091159a127c81a`; unchanged files hard-linked
from `pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z/count-facts`). The corrected annotation
and the bundle builder are not yet changed, so a rebuilt bundle would restore the lncRNA row.

**Twin host.** Relabelling ENSG00000280441 alone moved the reads to ENSG00000278996, the same
curated `ncRNA_host` model one rDNA unit away (its ENST00000623664 ends in the identical 97/1,001/
92/218/467 bp block; RNA5-8SN2 lies in its introns): `cr7-v1` 8,884 -> 1,229,564 UMIs, STAR GRCh38
862 -> 1,490,179 (STARsolo counts a multimapper whose loci fall in one gene). Of all counted GRCh38
chr20-22 genes, only these two contain an annotated 45S rDNA unit; 21 others contain only a 5S
pseudogene. Both are relabelled (bundle `.../rdna_host_as_rrna/both_hosts/count-facts`, content_id
`sha256:158f137587619eae2601de8ace546e91f8cb9d7e4b3dbcdc7a9f087daf4732e6`; CHM13 has no
ENSG00000278996).

**Result (2026-09-25).** On 6,211 cells, `cr7-v1` fell from 2,597,568 to 1,384,692 UMIs and STAR
GRCh38 GeneFull_Ex50pAS from 1,285,963 to 1,161,119; CHM13 changed by 2. On 2,949 shared genes STAR
has 0.886 of `cr7-v1`'s UMIs, pseudobulk Spearman 0.859. Evidence:
`/mnt/ssd/lalli/star_vs_pancollapse_filtered_20260924/rdna_host_as_rrna/both_hosts/compare.log`.

### D093 — Build assignment-cache misses unlocked; skip reads with only FALLBACK genes

**Status: ACCEPTED** (User, 2026-09-25: "Add the skip fallback only reads unless there is only
one gene in the overlap"; cache: "I'd be ok with increasing it to 5x current size").

**Context.** The full filtered count ran at 34.8% worker utilization: 59.6 M voluntary context
switches, all futex waits. The bounded assignment cache built each miss under its shard's
exclusive lock; once full (131,072 entries) 71% of reads missed and every lookup on a shard
waited behind the miss being built.

**Decision.** Misses build with no lock and the first insert wins (a race loser counts as a hit,
so counters equal single-flight construction). Capacity 655,360. A read is skipped (no route
tally, no exact evidence; unassigned with `reason_fallback_only_skipped`) when no node it touches
carries a Parent whose gene is PRIMARY/UNKNOWN in any profile, every aligned node's genes are
FALLBACK in every profile, and it overlaps at least two equivalence genes in every profile. Off for
compatibility capture and RAD; `PANCOLLAPSE_FALLBACK_ONLY_SKIP=off` restores the full path.

**Result.** Full filtered input, both-hosts bundle, 32 threads, old against new binary: read phase
15,162 -> 5,570 s (wall 4:20:47 -> 1:40:27), worker CPU 147,873 -> 150,993 s, utilization 34.8% ->
96.8%. Count, barcode, feature and molecule tables byte-identical. 624,659 reads skipped (2.6%);
cache full at 655,360 entries, ~233 MB, hit rate 28.5% -> 36.9%. Skipped reads no longer reach
barcode correction or the UMI filter, so `barcode_correction_succeeded` fell 288,270 -> 278,514 and
the UMI-drop counters by 38 to 42; no count changed. On a 1M-read slice, skip on/off: worker CPU
-15.0%, tables identical, all 105,303 skipped reads unassigned in both profiles without the skip.
The count is now CPU-bound (6.3 ms per read); under 1 h at 32 threads needs ~40% less CPU per read.
Evidence: `/mnt/ssd/lalli/star_vs_pancollapse_filtered_20260924/count_speed/` and
`.../rdna_host_as_rrna/both_hosts/pancollapse{,_new}/`.

### Phase 0 resolution map

F001 is resolved for gate review by D023 and D033. F002 is resolved by D026 and D034.
F003 is resolved by D025. F004 is resolved by D029. F005 is resolved by D027, D033, and
D034. F006 is resolved by D023, D024, and D032. F007 was originally resolved by D028 and
is superseded for barcode/UMI sourcing by D038. No remaining Phase 0 architecture fork is
known from the current proposal, but human approval is still required before production
code, build files, or fixtures begin.

All externally observable V1 product choices raised during workspace setup are now
settled in D016–D022 and `.agent-workspace/USER_DECISIONS.md`. Future review or
implementation work must not reopen them without a superseding decision.

### D094 — Checkpoint: evaluate GBZ and graph-only XG+GBWT count inputs

**Status: IMPLEMENTED; NAMED SCIENTIFIC ACCEPTANCE PASSED** (2026-09-26). See the
[design memo](research/gbz-gbwt-count-inputs-design-20260926.md) and implementation closure below.
The following intake records preserve the sequence of user instructions; later replacement
inputs supersede the original chr21 bundle and VG-binary directions. The later explicit
architecture approval and implementation acceptance supersede the intake's design-only gate. Worktree:
`design/gbz-gbwt-count-inputs-20260926`, based on `f511306`.

**Contract extension.** This satisfies D023's deferred equivalence condition and supersedes
the GBZ restriction retained in D088 for these optional graph-input routes. Existing XG,
annotation and counting semantics remain in place; the no-custom-persistent-index rule stands.

**Question.** Can existing GBZ or graph-only XG plus GBWT data supply every native `count` fact
currently obtained through the default XG route, with byte-identical output and acceptable
resource use? The required memo must state separately which operations need graph positions,
path/membership facts, metadata names, identity/translation, edges, targeted positions, and GBWT
locate; it must report chr21 wall/RSS observations and a clearly labelled chr2 projection.

**Evidence to reuse.** The retained chr21 fixture is
`/mnt/ssd/lalli/hprc_v2_vg_rna/notes/evidence/chr21_gbz_indexing_fixture_20260920/README.md`.
It recorded XG construction at 54:28.44/68.52 GiB and GBZ construction at 3:30.59/46.35 GiB;
those are index-construction observations, not native-count measurements. Its five-read mpmap
smoke establishes loading/mapping only, not count accuracy, throughput, or output identity. The
production-count baseline command is
`/mnt/ssd/lalli/pancollapse_filtered_native_count_v1_20260924T105938Z/commands/run_count.sh`.

**Constraints.** Use `vg-latest` for any VG invocation; no vg rebuild. Preserve the default XG
behavior. No production implementation or persistent panCollapse index is authorized. A new dated,
chr21-only fixture ledger and count bundle are authorized with the same existing tooling and policy
settings as `pancollapse_parent_policy_accuracy_1m_v1_20260915T171612Z`; do not invent policy or
modify existing bundles. Validate its names and lengths against the baseline chr21 XG mapped-node
graph and record hashes and exact commands.

**Accepted scientific identity scope.** Scientific result tables, MEX, RAD, and counters must be
identical. Timing, memory, host, timestamps, input mode, paths, and hashes are excluded through
an explicit operational-provenance field list and remain truthful. Input mode must not enter
scientific files. Check compression determinism; the first repeated XG scientific-byte comparison
is a stop gate, so failure prevents GBZ/XG+GBWT comparisons. The design memo and its measurements
are the first deliverable, followed by a stop for user approval at **Gate Architecture Approved**.

**Intake record (2026-09-26).**
[`gbz-gbwt-count-inputs-intake-20260926.md`](research/gbz-gbwt-count-inputs-intake-20260926.md)
records source capabilities and verified inputs. A matching runtime chr21 ledger/bundle has
not been located; fixture-only creation is now authorized under the stated constraints. No
benchmark, production implementation, or new runtime annotation input was created at intake.

**Policy-input gate reached (historical intake, 2026-09-26).** The authorized fixture audit
compared 2,804,175 distinct exact IDs against the matching raw annotation ledger.
The existing bundle's category ledger intersects 526,239 and misses 2,277,936. These are
identifier intersections; the transcript/body-segment interpretation is corrected below.
A checked missing Parent is absent from both corrected/projected GFF3 inputs
used by the final bundle producer. This does not measure whole-annotation absence or rule out
ordinary source-transcript tag joins, but it does not supply matching projection-local facts.
The raw exact-arm retained GFF3 is not a demonstrated correction-pipeline output. Per the
user's explicit condition, stop fixture construction and request the matching corrected selected
and projected annotations; do not silently emit empty categories or substitute the raw GFF3.
Evidence, input hashes, and commands:
`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/results/POLICY_INPUT_GATE.md`.
The [comparison specification](research/graph-input-scientific-comparison-20260926.md) lists the
precise operational fields excluded from scientific identity. No repeated count or backend
measurement has run; the measured design memo and architecture approval remain pending.

**Follow-up: replacement equivalence inputs (2026-09-26).** The chr21 corrected/projected
annotations do not exist, so the chr21 count-bundle stop stands and raw projection is not a
substitute. The 2,804,175 audited rows are not biological transcripts: 1,006,635 are transcripts
and 1,797,540 are `panSCbody1` body-segment paths, linked through
`exact/annotation/body_parent_aliases.tsv`; the audit remains an exact-ID intersection only.
The authorized equivalence fixture is the joint chr20–22 final XG/PG/pre-strip GBWT at
`/mnt/ssd/lalli/hprc_v2_vg_rna/joint_chr20_chr21_chr22/genic_flankpad_prerequisites_v1_20260830T192333-0500/`,
using the existing production GAMP, bundle, and whitelist named by the production count command.
Use stock `quay.io/vgteam/vg:v1.74.1` for every VG CLI step. Validate all ledger names, node IDs,
and lengths against XG; if required paths disagree, `vg gbwt -E` from XG embedded paths
is authorized. Extra unselected paths alone do not require rebuilding. Record exact commands and input hashes. A full GAMP or explicitly named
read-contiguous subset is allowed. First prove same-mode XG scientific determinism; failure stops
comparisons. Chr21 measurements remain graph-scale microbenchmarks, separate from this joint
equivalence fixture. The design memo remains first, and architecture approval precedes production code.

**Baseline determinism passed (2026-09-26).** On the read-contiguous
`joint/inputs/filtered_first100k.gamp` subset (100,000 read groups; 216,700 records), repeated
full-path XG counts exited 0. `xg_1`/`xg_2` were 459.06/448.25 s and 56,688,948/56,689,116 KiB
peak RSS. Scientific inventory, Parquet, MEX, RAD, summary projection, and manifest projection
were identical. All six MEX gzip files had matching compressed bytes and headers (`mtime=0`,
flags 0, no filename). Receipt:
`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/joint/results/comparison_xg_1_vs_xg_2.json`.
Joint fixture probes may proceed. No production backend is implemented, and architecture approval
remains pending.

**Measured closure (2026-09-26).** The [memo](research/gbz-gbwt-count-inputs-design-20260926.md)
is ready for review. Complete name/length/oriented-walk audits passed for 2,461,354 joint ledger
paths and 2,804,175 chr21 geometry paths. Sampled membership/position multisets match XG:
109,911 joint and 581,735 chr21 selected occurrences, over 256 fixed-seed nodes per graph.
Node-ID/sequence fingerprints agree across all graph representations. The chr21 GBZ has no
translation and omits 25,251 edges that the counting operations do not consult. Ordinary
`chr21.gbz` names match directly; `chr21.ref.gbz` changes 1,398,636 path senses/names and is
not the chosen exact-name fixture.

Preferred self-contained chr21 graph-access probes: XG 71.35 s / 52.663 GiB;
GBZ 229.60 s / 21.572 GiB; graph-only XG+GBWT 169.77 s / 20.260 GiB. New modes include
FastLocate construction. These are capability/resource probes, not native-count runs.
The memo also records eager 32/64-bit and lazy-prefix measurements, a deliberately stopped
dense-DA resampling experiment, matched query costs and a chr2 model based on its actual
19,538,776,098 forward GBWT steps. Raw receipts and hashes:
`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/`.

**Architecture approval and implementation authorization (2026-09-26).** The user approved this
memo design: expose only the graph facts consumed by projection; retain default XG and add GBZ
and graph-only XG+GBWT loaders; build standard FastLocate in memory from the loaded GBWT; and
retain selected-path prefixes in memory, 32-bit where valid with checked 64-bit fallback.
Occurrence enumeration must use `FastLocate::decompressSA`, preserving each repeated occurrence;
do not substitute per-state `locate` or DA-only enumeration. Omit an external r-index input
initially because the serialized index has no source-GBWT identity check. Preserve exact ledger
names, repeat/strand/position semantics, explicit capability errors and default XG behavior. No
custom persistent lookup index is authorized. Count and annotation policy are unchanged.

**Named implementation acceptance.** Run all projection fixtures and matched three-mode native
`count` on `joint/inputs/filtered_first100k.gamp` (100,000 complete read groups; 216,700 GAMP
records). Scientific inventory, Parquet, MEX, RAD, summary projection, manifest projection, and
scientific counters must be byte-identical across XG, GBZ, and graph-only XG+GBWT; operational
provenance remains excluded as specified above. Integrated wall/RSS are measured acceptance
evidence, not prior capability-probe claims.

**Implementation checkpoint (2026-09-26).** The approved input routes are implemented;
[implementation/acceptance record](research/gbz-gbwt-input-implementation-20260926.md).
The final XG suite passes 179/179 and each alternative passes 167/167 graph-dependent tests;
382 scientific fixture products match per alternative. Runtime uses nonempty GBWT records,
not numeric range membership, when validating nodes. Test-only unselected coverage paths retain
isolated nodes GBZ would otherwise omit. No missing-node failure is relaxed. Fresh same-build
native XG determinism passes, including all 11 scientific files, six compressed MEX headers,
and scientific counters. The implementation's XG output also matches the archived pre-feature
baseline. Both GBZ and XG+GBWT also pass every scientific comparison on the named 100,000-read
subset. Whole-process GBZ wall/RSS are 824.92 s / 39,134,904 KiB; XG+GBWT are 788.47 s /
38,369,492 KiB; fresh XG repeats are 2032.81/1500.07 s and 56,711,904/56,695,940 KiB.
These shared-host observations do not establish isolated speedups or full-GAMP/chr2 native
performance. The single archived-binary control passes scientific identity at 914.99 s /
56,761,504 KiB, also slower than its earlier 459.06/448.25 s runs but faster than the fresh
implementation XG runs. Runtime nonregression is therefore not established; the shared-host
measurements do not isolate adapter overhead from changing run conditions. Scientific behavior
and the named input-mode acceptance are established. Evidence:
`/mnt/ssd/lalli/pancollapse_graph_input_implementation_v1_20260926/`.

### D095 — v0.10.0 local integration and release authorization

**Status: ACCEPTED** (User, 2026-10-27: merge as v0.10.0, not a release candidate).

**Decision source:** User (local integration/release authorization only).

**Decision.** Merge source commit `851d8c1` into `main` locally as v0.10.0 and create a
local tag after merged-snapshot verification. Do not push or publish. This records the
release integration of the existing D093 and D094 work; it makes no annotation, counting,
or scientific-policy change and does not waive an original scientific or release gate.

**Evidence and limit.** Fresh merged commit
`17d32ab5734b57cf53d1f0a45ac12426ae4cf35c` regression passed: build 56/56 and serial CTest
179/179 in 45.38 s (serial avoids known cross-test fixture-output collisions); binary SHA-256
`5b61fbc5a9fe0ecdd5d57c7840e0c2a5bd8bcd9a06e29d7fd8885bd7ebbffc7e`. Image packaging is
now complete: `josephlalli/pancollapse:v0.10.0` has OCI version and CLI
`0.10.0`, and the existing all-format smoke passed both profiles (Parquet, MEX, RAD, read
assignments) without Python/libpython. A stale fixture-work namespace in the package script
was mechanically repaired; C++ is unchanged. Root will create the local `v0.10.0` tag after
committing this record, with no push. The source branch's named-subset input-mode acceptance
and its explicit limits remain controlling; full-GAMP/chr2 performance of the new graph-input
backends and their runtime nonregression are not established, and original full scientific gates remain
unwaived. The merge receipt root is
`/mnt/ssd/lalli/pancollapse_v0100_main_merge_20261027T225050Z/`.

### D096 — Whole-genome identity repair and count startup admission

**Status: ACCEPTED** (User, 2026-10-28: implement the global identity-repair plan).

**Decision.** Continue from v0.10.0 main in the isolated
`fix/whole-genome-count-repair-20261028` worktree. Reconcile Parent identities
across all chromosome ledgers using the existing full-context `panSCup1` rule;
preserve opaque body IDs, biological provenance, graph paths/walks, and the
completed GAMP. Derive graph-matched facts using the frozen counting mechanism.
The frozen prior preparation consumed `retained_exons_and_bodies.gff3`, so
the matched whole-genome source universe is the 24 accepted retained graph
annotations after namespace repair, not an enlarged set of raw CAT models.
Verify each producer/archive source checksum before issuing the new aggregate
scope receipt; the receipt is a mechanical certification of this scope.
This does not authorize a biological-policy change, remapping, BAM output,
Nextflow integration, a new release/tag, or a resource-cap increase.

**Scoped native changes.** Stream count-fact TSV rows into the existing catalog
instead of retaining a second vector of parsed rows. Add `count --preload-only`
on the graph/GAMP route: share production reference/model/whitelist/runtime
initialization, emit a resource receipt, and return before opening GAMP or
writers. No count directory is published. This is a startup measurement, not
worker/cache/finalization memory or successful counting. It is rejected for
compatibility replay. Legacy `convert` and frozen counting rules are unchanged.

**Evidence and next gate.** The isolated native suite passes 180/180 in 49.62 s
at `count_identity_repair_20261028T173014Z/logs/ctest-native-preload-full.log`
under `/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/`. Registry tests pass
18/18. The 10k-row metadata check passed; the redundant 100k step was skipped.
Full global metadata reconciliation is running, not a count or another pilot.
Complete facts, compatibility, and full-reference memory admission remain
pending. Counting retains the hard 450,000,000,000-byte, zero-swap cap, at most
128 cores, and at most 128 GiB for aggregation; aggregation is not the
whole-process cap. `PROGRESS.md` names the live container and exact run root.

**2026-10-29 operational checkpoint (D096, no new policy decision).** Corrected
GFF production completed: all 24 chromosomes and 95,110,030 EXON contexts covered,
BGZF integrity passed, matching ledger published at
`count_identity_repair_20261028T173014Z/corrected_gffs_usable` under
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/`. After the required documentation
checkpoint, one full-reference `count --preload-only` was launched against these
final inputs, original XG and repaired GBWT in
`pancollapse-final-input-preload-20261029T041218Z` at 04:12:47 UTC. The lower
350-GB/no-swap cap plus 50-GB host reserve fits current available memory; it
does not increase the previously authorized maximum. Both profiles are selected
for initialization, with 128 threads/128-GiB count budget. This is annotation
authority, not certification of frozen custom-fact equivalence. It cannot read
GAMP or produce counts. Exact command, binary hash, inputs and remaining
conditions are at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_annotation_count_20261029T041218Z/RUN.json`.

**2026-10-29 compact storage and observed identity boundary (D096, no new
biological decision).** Shared immutable text replaces per-row string ownership;
audit-only provenance is discarded only after existing validation passes.
Annotation metadata/tag combinations and ParentFact labels share storage. Graph
handles and lexical path ranks reside on the row instead of extra dictionaries.
Ledger-load progress is emitted at million-row milestones. Fixed row size is
136 bytes versus 744, not a whole-genome RSS claim; map catalogs remain.

The same `ENST00000477740.5` has source genes `ENSG00000241860.8` and
`ENSG00000238009.6`, including exact path `chr2::CHM13_T0135505_R1` for the latter.
Both raw and normalized source-gene IDs differ. These observations do not
establish historical aliasing, annotation error, or biological locus identity.
The real rows are retained in
`tests/fixtures/path_identity_conflict_ENST00000477740.tsv`. The canonical/gene
invariant remains unchanged and fail-closed. No repeat whole-genome startup was
launched into this known contradiction. Identity resolution remains a human
review boundary, not an implicit gene merge or canonical-target change.

Final rebuild/full CTest passed 181/181 in 26.66 s, including the real conflict
fixture in ordinary and deferred-body-preflight modes. Two tests sharing an
isoform BAM output are serialized using CTest RESOURCE_LOCK after an observed
parallel temporary-file rename race; production behavior was not changed to
accommodate it. Logs are `build/compact-identity-build.log` and
`build/compact-identity-ctest.log` in the repair worktree. This validates bounded
storage/behavior fixtures, not whole-genome capacity or count completion.

### D097 — Qualify conflicting native-count transcript targets by source gene

**Status: ACCEPTED** (User, 2026-10-29, explicit approval to preserve both source
IDs and leave GFF/index inputs unchanged). Only native `count` and compatibility
replay opt in. `convert` retains its strict canonical-transcript/gene invariant.

When exon identities share a canonical transcript but have distinct normalized
explicit source genes, give each pair a separate internal target. Use existing
count-gene normalization, without applying profile-specific novel-copy lumping
at this stage. Each pair must still map to exactly one raw gene ID. Missing
source provenance cannot justify a split; Parent conflicts and body/exon
provenance checks remain errors. Bodies inherit the validated exon partition.
Original canonical, source-gene and raw-gene fields remain untouched.

The implementation retains a conflict-only resolver rather than adding a target
string to every path row. Length-framed `panSCtxg1:` target keys and reserved-prefix
escaping prevent collisions with literal input labels. Geometry/scoring uses
these targets; optional RAD lists their unique keys. Compatibility/debug labels
retain original canonical IDs. Unsplit replay surfaces retain the v1 hash; split
surfaces bind the source-gene partition under a distinct hash domain. Replay
rejects structural/exact facts mixing source-gene targets. Count manifests
record target scheme, qualified-target count, and mapping digest. Legacy t2g
assertions are explicitly rejected when they cannot represent qualified targets.

This does not establish biological equivalence, reconcile annotation versions,
merge genes, change frozen counting policy, or promote the custom profile.

**Acceptance and startup evidence:** the full native suite passes 183/183 in
55.84 s; final focused tests pass 6/6 in 2.40 s. Count tables, molecule tables,
read assignments and semantic counters match an explicitly disambiguated tiny
reference exactly at 1/4 threads and replay; mixed-source facts are rejected.
This is not a whole-genome accuracy/capacity result. Logs:
`build/qualified-target-{ctest,focused}.log`.

The required checkpoint was reconciled before launch. Whole-genome annotation
startup began at 2026-10-29T15:00:25Z in
`pancollapse-qualified-count-20261029-JQD61Y`; run root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_source_gene_count_20261029-JQD61Y/`.
The frozen binary and source snapshots are retained there. A bounded preload
must exit 0 before any GAMP counting; then count starts automatically under the
same 350,000,000,000-byte/no-swap cap and <=128-core limit. Aggregation remains
at most 128 GiB and is reduced if measured startup peak plus 32-GiB reserve
requires it. No memory-cap increase, extra pilot, biological-policy change,
GFF rewrite, remapping, BAM, database, or custom persistent index was introduced.

### D098 — Prepare reference-only counting state once per reference

**Decision source: User** (2026-10-29). **Status: ACCEPTED requirement;
implementation and storage-format design pending.** User identifies multi-hour
reference initialization as indexing work that must become a once-per-reference
step rather than a per-sample cost. This authorizes future reference preparation;
it supersedes D004's no-custom-persistent-index restriction for that future
counting-reference feature only, not the current implementation or legacy convert.

Separate expensive reference compilation (identity validation/qualification,
annotation facts, path binding/positions, exact Parent models and splice geometry)
from sample counting. Persist the reusable reference state, with version/input
identity checks and complete/atomic publication. Rebuild when relevant reference
inputs or preparation semantics change, not when the sample changes. Sample-wide
barcode priors, observations, assignment results and UMI counts are not reference
state and must not enter the index. Frozen biological policies remain unchanged.

The existing count-fact bundle is not a complete serialization of runtime
reference state. A prepared reference must also avoid reconstructing the large
text/map catalogs on each sample; simply caching the original TSV/GFF parsing
inputs would not meet the requirement. A compact loadable representation is the
intended direction; exact layout, memory mapping, graph integration and CLI need
design/validation before production implementation. Loading bytes is still a
per-run cost; no startup-time or whole-genome fit guarantee is established.

Record raw-input versus prepared-reference parity, startup wall time, peak memory
and I/O as acceptance checks. The current full-reference run is unchanged: its
preload is a resource check that discards initialized state before count starts,
not an index build. No new job, index build, database, GFF rewrite, or active-run
restart is authorized by this documentation checkpoint.

### D099 — Native binary annotation compilation, first D098 slice

**Decision source: User** (2026-10-29 request to implement indexing compressed
GTF/GFF into a directly usable binary) plus scoped architecture review.
**Status: IMPLEMENTED and bounded parity/regression verified (185/185).**

Add `panCollapse index --annotation A --annotation-reference R
--path-identity-ledger L --out annotation.pci`. Both frozen profile fact tables
are compiled. `count --annotation-index annotation.pci --path-identity-ledger L`
loads them instead of reparsing annotation/classifying facts. It is exclusive
with `--annotation`/`--count-bundle`; replay remains bundle-only. Native convert
and biological policy are unchanged. This is an annotation index, not the full
prepared reference contemplated by D098 and not an observed OOM remedy.

GFF3 remains the default. Explicit `--annotation-format gtf` and
`--annotation-reference-format gtf` support GENCODE-style quoted attributes,
including repeated tags. GTF requires explicit gene/transcript features and
exact ledger Parent IDs; missing transcript surface, contradictory scalar
values or conflicting transcript/gene identities are errors. GTF source
provenance uses explicit `source_gene`, else explicit `gene_id`. No haplotype
identity is inferred. Decompression is streamed with cursor-based chunk framing,
16-MiB line limits and checked zlib errors; no database or external spool.

`PCANIDX2` is explicitly little-endian and versioned, with formats/semantic ID,
source hashes/sizes, profile identities, gene/category dictionaries and lexical
13-byte Parent records. Parent identities are reused from the hash-bound ledger;
facts load into reserved contiguous storage with binary search, not a recreated
Parent tree. Gene/profile maps remain small. Raw factories use the same flat
Parent table. The ledger digest is calculated during its required read.

Preserve the original annotation-route source content ID. Separately report
`annotation_index_derivation_id`, the canonical payload hash covering formats,
semantics, source binding, dictionaries, profiles and Parent records; manifest
input identity hashes the complete artifact. Validate sources/content identity,
checksum, bounds, enums, profile definitions, canonical equivalence/competition,
ledger digest/size and complete Parent coverage before graph/GAMP. Write through
exclusive staging, fsync, staged checksum verification, atomic no-overwrite
publication and directory fsync. Do not serialize graph handles.

Implementation: `src/annotation_fact_index.{hpp,cpp}`, `direct_count_facts.*`,
the supplied-stream ledger seam and main CLI. Tests:
`tests/policy/annotation_index_smoke.cpp`, extended annotation parser test and
`tests/vg/annotation_index_verify.py`. Build/focused/full logs are
`build/annotation-index-{build,focused,ctest}.log`. Full-reference build/load
memory remains unqualified: ledger maps, FastLocate, positions and exact
Parent/exon/splice geometry remain per run. No production relaunch or cap raise.

**Final acceptance evidence:** `build/annotation-index-ctest-final.log` records
185/185 passing in 45.46 s. Raw/index Parquet tables, read assignments, logical
hashes, semantic counters, MEX and RAD agree at 1/4 threads. Tests also cover
GTF/GFF gzip parity, deterministic bytes, removed source files, corrupt/truncated
data, forged re-checksummed source/format/flag/equivalence metadata and wrong
ledger rejection. The loader retains whole-file SHA/size from the actual read;
final output provenance does not reopen/re-hash a potentially replaced pathname.
The loaded-inode identity test verifies this retention. Earlier full suite
passed 185/185 in 47.66 s. `git diff --check` passes. No whole-genome performance,
memory fit or complete prepared-reference qualification is implied.

**2026-10-29 authorized index build:** after a fresh documentation checkpoint,
one native annotation-only build started at 23:14:27 UTC in
`pancollapse-annotation-index-20261029-HS3sPi`, run root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_annotation_index_20261029_HS3sPi/`.
It uses the same final corrected whole-genome GFF/ledger and reference as the
failed preload, frozen tested binary
`952b993fac47d773e36a3d258770d476da91f52dda85ead840f2b101c88b10ed`,
350,000,000,000-byte/no-swap cap and <=128 CPU limit. No graph, GAMP, count,
remapping or input rewrite is included. Early progress is 1M ledger rows,
not an index completion or memory-fit result. Exact launch and limits are in
`launch.sh`/`RUN.json`; completion is native exit 0 plus atomic `annotation.pci`
and complete stdout receipt. No automatic counting is enabled.

**2026-10-29 live resource amendment:** user explicitly permits a larger cap.
At 23:28:47 UTC, `MemAvailable` was 573,084,108 kB (~587 GB) and the index
container used ~42 GB. Raised this container's memory and memory-plus-swap
limits from 350 GB to 500 GB, preserving no-swap and CPU settings, with no
restart. This leaves approximately 129 GB of host headroom at the cap if other
loads stay steady. Original launch receipts retain their historical limits;
`RUN.json` records the amendment. Neither the higher cap nor small binary
records establishes full-reference runtime fit. No count was launched.

**2026-10-30 indexed count attempt authorized and launched:** the annotation
index completed (2,539,421,338 bytes, native exit 0/no OOM). After a fresh
documentation checkpoint, user-authorized actual count started at 04:15:28 UTC
using it, both profiles, 128 threads and 128-GiB aggregation budget under a
600,000,000,000-byte/no-swap whole-process cap. No separate preload or duplicate
initialization is included. Admission host `MemAvailable` was ~900 GB.
Run root:
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_indexed_count_20261030_uSVnCB/`;
container `pancollapse-indexed-count-20261030-uSVnCB`. `launch.sh`/`RUN.json`
retain exact inputs/command and `count.log` records phase progress. Initial
5M ledger rows/58.38 s are startup, not counted reads or a fit guarantee.
The independent aggregation budget controls approximate compressed spilling
and merge buffers; it is neither a reservation nor another hard process cap.
No annotation-policy or other scientific change accompanies this attempt.

## D100 — Memory-only native counting; remove disk fallback (2026-10-30)

**Accepted user requirement:** remove the count-path disk fallback altogether.
This supersedes earlier requirements for aggregation-budget-triggered spilling
and an external-memory single-barcode escape path. Exact/deferred/corrected
observations and per-barcode UMI reduction now remain in RAM. Remove spill
writers/readers, compressed runs, external merge paths and temporary aggregate
directory management, rather than merely disabling them by default. Preserve
barcode priors, correction posteriors, assignment policy, UMI semantics,
canonical output order and existing optional sinks.

Remove the independent runtime/output aggregation budget. Reject the old
`--count-memory-budget` CLI option explicitly; callers set the whole-process
memory cap externally. The manifest records `count_storage: "memory-only"`,
not a purported aggregation limit. Legacy aggregate-spill counters remain
present as zeros for summary compatibility. RAM exhaustion is failure, not a
reason to silently create disk-backed aggregates. Final compressed outputs
and opt-in per-read diagnostic staging are not aggregate fallbacks.

Validation must cover the existing barcode/UMI goldens, large single-barcode
state, insertion/thread/profile invariance, absence of aggregate files, removed
option rejection, annotation/index equivalence and the complete existing suite.
This changes no reference-index format or biological rule. In particular, the
current sense-bucket-first assignment agrees with the frozen Python default;
D066's older interleaved rank is historical, not a parity defect to fix here.

**Reference design clarification:** the completed D099 artifact contains
annotation-policy facts, not path identities or a node-posting lookup. The
runtime still loads the identity ledger and builds FastLocate/model geometry;
its `NodeLedger` is a lazy node-to-path association dictionary. A complete
prepared reference remains future work under D098. A bare node-to-gene union
would lose transcript coherence and exact overlap/strand/junction semantics;
that is not an authorized simplification. See `conversion-algorithm.md` for
the actual pipeline and index anatomy.

The active whole-genome run uses a frozen binary and is not hot-patched by this
source change. Leave it untouched pending the user's explicit restart choice;
do not imply that passing bounded tests changes that process or proves full
whole-genome memory fit.

**Acceptance evidence:** Docker build succeeds and
`build/memory-only-ctest-final.log` records 185/185 passing in 45.84 s.
Runtime tests retain barcode/UMI goldens and insertion/thread/joint-profile
invariance, including a 60,000-feature single-barcode case; indexed/raw fixture
outputs agree and legacy spill counters are zero. Static inspection finds no
runtime aggregate writers or external-merge paths. `git diff --check` and
`bash -n scripts/tooling/build-docker-image.sh` pass. No release image or new
whole-genome count was launched for this acceptance check.

**2026-10-30 production request and new failure evidence:** user now requests
the older spill-capable binary with a 500-GB whole-process limit and a 450-GB
spill threshold; this is an operational selection, not a reversal of the
memory-only source change. Preflight found the former frozen-binary container
`pancollapse-indexed-count-20261030-uSVnCB` exited 137 with `OOMKilled=true`
at 06:36:08 UTC under its 600-GB/no-swap cap. Its last logged phase completed
FastLocate; no selected-path-position completion, GAMP progress or published
counts were observed. Exact evidence remains in that run's `launch.sh`,
`RUN.json`, `count.log` and Docker state. Old spill logic tracks approximate
count aggregates only and starts above 3/4 of the budget; a 450,000,000,000-byte
aggregate trigger maps to `--count-memory-budget 600000000000`, not a process
limit. It cannot relieve reference startup. No retry has been launched; report
the newly discovered startup failure before spending another full initialization
under a lower cap. A startup code fix or different cap requires new authority.

**2026-10-30 read-only memory diagnosis:** user declines implementation and
requests diagnosis plus smaller live-state opportunities only. No retry or
production code change follows. Audit confines failure to the post-FastLocate
selection/prefix-preparation bracket (`main.cpp:3185-3205`), without identifying
the exact exhausted allocation. Header-only inspection finds 236,703,305,690
forward GBWT steps; eager 32-bit coordinates for all forward paths would alone
be 948,335,063,004 bytes. Actual selected-step count/RSS were not recorded;
that arithmetic is a scenario, not measured whole-genome attribution.
Frozen-binary DWARF gives a combined 42.61-GB ledger/Parent-tree node floor and
18.26-GB loaded Parent-fact rows at 190,240,849 entries, excluding text heaps,
category buffers and allocator overhead. Strings are already shared and audit
provenance already released. Later lazy caches/count tallies were not built at
this OOM. `PROGRESS.md` records exact evidence, arithmetic and unimplemented
opportunities. These observations approve no new representation, policy,
resource limit, claimed savings or memory-fit result.

## D101 — Compact exact reference state and safe optional caches (2026-10-30)

**Accepted scope:** user authorizes implementation of the preceding memory
diagnosis. Preserve assignment, identity, strand, exon/junction, barcode and UMI
semantics; avoid dense coordinates/expanded metadata and reduce retained state.
No graph pruning, gene-only union, new CLI/index format, disk fallback, production
retry or cap increase is included.

**Architecture:** retain GBWT prefix checkpoints at zero/every 64 steps/terminal.
Local GBWT primary source (`deps/gbwt/src/fast_locate.cpp:439-451`) establishes
that `decompressSA(node)` vector order is BWT-record offset order. Use that exact
record offset with LF, not the unpacked back-offset as a record offset. Stored
forward walks move to the next checkpoint and subtract node lengths; stored
reverse walks move to the prior checkpoint and add intervening lengths. At most
63 LF operations recover the exact coordinate, including repeated/mixed-oriented
visits. One active 32/64-bit checkpoint vector replaces two per-path vector
headers. The time tradeoff remains unmeasured on the whole reference.

Validate ledger text/conflicts first with the existing strict reader; then freeze
row/Parent trees to lexical arrays and rebind representative pointers before
fact construction. Keys stay immutable, runtime graph bindings remain mutable,
and ledger moves preserve backing storage. Main reuses lexical Parent ranks,
avoids selected-name copies and long-lived ParentFact pointers, and allocates
legacy path strings only on legacy paths. Parent policy records retain one
stable identity pointer plus three uint32 IDs/flags (24 bytes); shared categories
remain in a dictionary. Materialize only an active fact. PCANIDX2/source identities
and deterministic bytes remain unchanged.

Native node/model accelerator caches account resident values against 1-GiB and
256-MiB internal bounds. FIFO eviction/oversized bypass cannot change evidence;
read-local shared-pointer leases keep immutable values alive. Accounting excludes
active leases, allocator overhead and uncached construction, not a hard process
RSS cap. Legacy conversion remains effectively uncapped. Caches expose resident,
high-water, hit/build/eviction/bypass counters and release storage without erasing
cumulative statistics after workers finish.

Before UMI finalization, preserve transcript output identities, finalize
compatibility capture, flush workers, release graph/model/cache/ledger/Parent
state, and retain gene/profile/provenance data required by output writers.
Common UMI keys use lossless tagged encoding; longer/noncanonical strings retain
the historical behavior through an owned fallback. Drop consumed aggregate
vectors and hash buckets instead of retaining them through result construction.

**Acceptance:** coordinated Docker build succeeds; full existing suite plus the
new bounded-cache test passes **186/186 in 47.20 s**. Logs:
`build/reference-memory-build-final.log`, `build/reference-memory-ctest-final.log`.
Graph tests cover all path/node/ordinal/orientation/base-position tuples against
XG, repeated/mixed visits, checkpoint/terminal edges, wide positions and a
1,025-step walk using 18 checkpoints. Ledger tests cover moves/rebinding and
frozen-key behavior; fact tests cover 24-byte records, category sharing, release
before output and byte-exact index rewrite. Cache tests force eviction while
leases are live and exercise concurrent single-flight construction. UMI tests
preserve variable lengths, leading As, 31/32-base boundaries and exceptional
symbols. All prior conversion/count tests remain active.

`tests/vg/reference_memory_parity_verify.py` additionally passes **18/18**
frozen-binary comparisons on the nine-read fixture: three graph inputs, default
and all-format sinks, old/new one thread and new four threads, both profiles.
Tables, diagnostics, compressed MEX and RAD are byte-identical; scientific
counters and per-profile logical hashes match. Receipt:
`build/reference-memory-parity-wvzakxpv/VALIDATION.json`.
Tested new binary SHA is
`ddb57632d7c70b6d7fb080a52bbbcd76cc11d32d6ae0abb41ffcae1a2305dc82`;
the frozen old executable remains unchanged. No whole-genome RSS, throughput,
real-read F1, release image, complete prepared-reference or 500-GB-fit claim is
established. The user-requested 500-GB limit remains the future process cap;
no production retry was started.

**2026-10-30 operational amendment:** the user's subsequent "Run it" authorizes
one whole-genome count with this tested frozen binary, both profiles, 128 threads,
500,000,000,000-byte hard memory/memory-plus-swap limits (no swap) and no spill
fallback. The existing GAMP/XG/GBWT/PCANIDX2/ledger/whitelist are reused; there is
no separate preload, remapping or reference rebuild. After the documentation
checkpoint, container `pancollapse-memory-reduced-count-20261030-4sQJXD` started
at `2026-10-30T12:54:14.737507532Z`. Native logs begin with identity-ledger
initialization, not GAMP processing or completed counts. Run configuration,
frozen binary and logs are under
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_memory_reduced_count_20261030_4sQJXD/`.
The output target is `counts/`; fit and completion remain unestablished.

**Read-only startup explanation:** `GraphInput::initialize_gbwt()` currently
constructs FastLocate unconditionally. Local GBWT primary source exposes `.ri`
serialization/load plus `setGBWT`; loading validates its format, not source-GBWT
identity. Construction holds two 24-byte-per-logical-run sample arrays, an
8-byte-per-stored-sequence endmarker array, per-worker buffers and the emerging
compressed index. This is reusable reference preparation, not read tallying.
PCANIDX2 currently prepares annotation policies only. At 190,240,849 Parents,
code-derived ledger row/Parent-index/fact payload plus GBWT selected ranks is
about 38.05 GB, excluding text atoms, dictionaries, graph/GBWT/FastLocate and
allocator-retained freed pages. No allocation trace attributes the full process
RSS. Live anonymous memory changed from 413.4 GB to 347.5 GB between 14:43 and
14:46 UTC without a FastLocate completion milestone; no OOM was recorded.
These observations authorize no new index design, code edit or job restart.

## D102 — Standard GBWT FastLocate `.ri` artifact build (2026-10-30)

**Decision source: User.** The memory-reduced count container
`pancollapse-memory-reduced-count-20261030-4sQJXD` is terminal: it exited 1 at
`2026-10-30T15:17:19.33273047Z`, with `OOMKilled=false`, after FastLocate
(1592.66 s) and selected-path-position preparation (1511.04 s). The fatal
ledger lookup was absent path `chr2::HG00097#1#HG00097_ha_T0037310_R1`; therefore
there is no GAMP completion or count output, and no restart is authorized.

The authorized independent standard FastLocate `.ri` build started at
`2026-10-30T15:32:41.725382404Z` (08:32 PDT) as
`pancollapse-transcript-ri-20261030-eAma1l`, with result root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_transcript_ri_20261030_eAma1l/`.
It uses frozen `build_ri` 1.5.0 SHA-256
`3b1e02dff44c9973379c0e598b81267bb2cd93c319c22d5b284ef50e39023a0f`,
128 threads/CPUs, and 500,000,000,000-byte memory and memory-plus-swap limits
(no swap), using
`/mnt/ssd/lalli/usr/local/src/vg/deps/gbwt/bin/build_ri` and the existing,
unchanged 44,562,796,448-byte GBWT at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/joint_transcript_gbz_20261028_repair/joint/joint.transcripts.gbwt`.
`launch.sh`, `build.sh`, `RUN.json`, `container.id`, `build.stdout`, and
`build.log` are retained. Its `.staging/joint.transcripts.gbwt` is an unchanged
input symlink; staged output publishes as `joint.transcripts.ri` only after
builder exit 0, nonempty output, stable source, and recorded completed
source/output SHA-256 binding. At 15:33 UTC the container was running while
loading GBWT, with Docker displaying 23.4 GiB. This is neither artifact
completion nor query verification. This authorization neither restarts counting
nor changes biological policy, source, the GBWT, or existing GAMP/XG/ledger/
PCANIDX2 inputs. At authorization, host `MemAvailable` was 896,347,656,192
bytes and free disk was about 12.15 TB.

The standard `.ri` is not a complete panCollapse prepared reference. Current
panCollapse exposes no `.ri` flag or loader, notwithstanding GBWT-library load
support; it consequently cannot use this artifact today. It does not replace
the identity ledger, XG/GBWT graph access, annotation-policy index,
selected-path positions, or exact Parent/exon/splice geometry. A later loader
would require separate authorization and must bind an `.ri` to its exact source
GBWT because native `.ri` format loading alone does not establish that identity.

**Terminal artifact receipt:** the RI builder exited 0 at
`2026-10-30T16:03:46.588726829Z`; `joint.transcripts.ri` was published under the
run root above (44,433,079,158 bytes; SHA-256
`7c6f0526b35ccf6ce744f026da89e9203a2f3e7d8664691054dd2dcaaab711bd`).
Source GBWT SHA-256:
`165d0a427a82eec08310b7f2306d6cb2dbdadc89a60bbbcab67c2f3224086961`.
This establishes builder success/publication, not full query verification or
PanCollapse CLI loading.

**Read-only mismatch cause audit:** exact sorted-file lookups find the fatal
row byte-identical in the original graph-side and later count-repair ledgers.
The original Oct 28 scripts qualify all ledger names with `chromosome::`
(`qualify_ledgers.py:40-43`) but only generic GBWT names
(`repair_metadata.cpp:115`); both live under
`/mnt/ssd/lalli/hprc_v2_vg_rna/notes/joint_transcript_gbz_20261028_repair/`.
Its binding records 380,460,061 total versus 373,001,671 generic metadata paths.
Their 7,458,390-path difference equals the count's unmatched ledger count;
the non-generic paths are confined to chr2 by the recorded ranges. Thus the
earlier duplicate-name/namespace repair, not recent memory-only compaction or
row/biological dedup, introduced the inconsistent naming mechanism. Every
individual counterpart has not yet been audited. This finding authorizes no
reference rewrite, source-identity change or count restart.

## D103 — Authorized ledger/graph repair and bounded restart gate (2026-10-30)

**Current authorization superseded by user stop, 2026-10-31:** the user requested
"Stop everything" and a read-only explanation of persistent reference data versus
per-sample data structures. Container
`pancollapse-intronic-fragment-count-20261031-jFlfrm-r1` stopped at
`2026-10-31T01:38:02.955258791Z`, exit 143 and not OOM, during catalog setup
before GAMP. All run files are preserved. No restart, implementation, or biological
policy change is authorized by this review request. Earlier permissions and run
milestones below are historical and do not override this stop.

**Decision source: User.** Diagnose and repair the established ledger/graph
namespace mismatch, run preflight, and restart counting without further
intervention after a bounded technical review. Preserve the original GAMP and
GBWT where possible, preserve immutable inputs, make no biological-policy
change, and retain both profiles, 128 threads, and the 500,000,000,000-byte hard
memory and memory-plus-swap limits (no swap).

The authorization is grounded in the terminal missing-path failure, not OOM:
the fatal row is byte-identical in both ledgers, while the prior repair prefixes
all ledger names but skips non-generic GBWT paths. The 7,458,390 non-generic
chr2 paths exactly account for the difference between 190,240,849 ledger rows
and 182,782,459 matched paths. Existing completed `.ri` and failed-count
receipts remain historical evidence, not repair or restart success.

**Historical preflight result:** the first prefix-only repair preflight,
`pancollapse-ledger-name-repair-20261030-Dfwj5y`, exited 1 without OOM at the
first `HG00097` chr2 row after about 101 million rows. It neither published a
reference nor started a count. Its receipts remain in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_name_repair_count_20261030_Dfwj5y/`.

Its successful metadata-only diagnostic (`metadata-inspection.txt`) distinguishes
ID 47621561 phase 1, whose native
`HG00097#1#HG00097_ha_T0037310_R1#0` differs from the forced `REFERENCE` spelling
without `#0`, from distinct phase-2 ID 52589685. All 7,458,390 non-generic
records are actual `HAP` records with count 0 and fall in chr2 by the recorded
ranges. This invalidates prefix-only qualification as a repair design; it does
not establish a completed repair.

**Revised implementation acceptance:** metadata-derived legacy aliases with
collision, owner, and reuse guards passed bounded review. `ctest.log` records
188/188 passing in 23.76 s and `git diff --check` passed. Its expected output is
190,240,849 rows, 7,458,390 legacy rewrites, and 87,702,873,062 bytes.

**Repair completion:** repair ran in
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_name_count_20261030_DkAAtB/`.
The initial same-root container exited 126 because of an entrypoint-argument
error before any work; it is retained and the wrapper was corrected. Replacement
container `pancollapse-native-name-repair-20261030-DkAAtB-r1` exited 0 without
OOM and published `reference/`. `reference/repair.json` confirms 190,240,849
rows, 7,458,390 rewrites (all chr2), 182,782,459 unchanged rows,
87,702,873,062 bytes, and `non_name_bytes_preserved: true`; Docker wall time was
1068.03 s. Required inline old-ledger binding matches the original PCI. New
ledger SHA-256 is
`44c4c62d8fd7f79d51ba7d18d1f0349df962cd38d596d3f463b387c79f5faa7d`; PCI
SHA-256 is `736f928c2084d5c6923f019aa5d587a566eb441e79a3776449a74c8b28a8043a`.

**Historical stopped attempt:** supervisor exec session 51043 finished 0 and launched count
container `pancollapse-native-name-count-20261030-DkAAtB` at
`2026-10-30T17:53:19.121568821Z`, under verified
500,000,000,000-byte memory and matching memory-plus-swap limits (no swap), with
128 cores and both profiles. It reached 23 million ledger rows in 298.7 s at
about 16 GiB RSS (20M rows), then 34 million rows in 446.7 s at about 25.8 GiB
RSS. It did not reach GAMP processing or final counts. The user has issued
`docker stop` only for this container; it completed at
`2026-10-30T18:01:40.053909186Z` with exit 143 and not OOM (session 8256).

**Reader-EOF denominator:** read-only mapper receipts at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/mpmap_pbmc1k_v3_20261027T122300Z/attempt5/full/`
establish 66,601,887 input reads/pairs: `input_stats.json` gives L001 33,436,697
+ L002 33,165,190; `COMPLETE.json` records complete status, both FASTQ EOFs,
zero exit, and a 30,326,279,277-byte GAMP; `process_receipt.json` records mapper
and producer exit 0; and `stderr.log` ends `Mapping finished. Mapped 66601887 reads`.
Native `input_read_groups` therefore targets 66,601,887 pending reader EOF.
The serialized alignment-record count is not independently known and is not that
denominator. Stale `full/.progress.json` cutoffs are superseded.

The completed RI remains usable with the unchanged GBWT through its trusted
receipt flag; no RI rebuild or extra SHA passes occurred. The 188/188 CTest
receipt in 23.76 s remains bounded acceptance evidence. This root retains
`ctest.log`, `CI_ACCEPTED.json`, scripts,
`repair.log`, and current `r-index-binding.json`; the distinct prefix-only
failed root remains retained. Per user direction, avoid extra file hashes; use
only PCANIDX2-required inline bindings and a fast lookup hash. Repair and any
later count retain both profiles, 128 cores, and 500,000,000,000-byte hard memory
and memory-plus-swap limits (no swap). The prior authorized task was monitoring
and diagnosing any failure without biological-policy change. Neither whole-genome
fit, parity, count completion, nor published counts is established.

**New authorized phase checkpoint:** the user-directed stop completed at
`2026-10-30T18:01:40.053909186Z`, exit 143 and not OOM, after 35 million ledger
rows in 490.6 s. The user conditionally approved the refined PCANIDX3
architecture: a self-contained runtime ledger plus fact catalog, lexical shared
dictionary, 64-byte identity rows with Parent rank, 8-byte Parent representative
records, and serialized canonical/qualified/normalized/escaped/original maps.
It uses linear Parent validation, direct `FrozenFlatMapSortedBuilder`, bulk fixed
I/O, and inline checksums only (no V3 verification reread).

Three owned-result `std::launch::async` branches load reference, XG, and GBWT
then RI; all join before existing crosschecks. `--threads < 3` remains the serial
baseline. Legacy V2 APIs remain; normal indexing writes V3, count uses V3 without
TSV parsing, and `--from-annotation-index` upgrades V2 once from the corrected
ledger. Preserve the repaired reference, all receipts, GAMP, GBWT, and RI
unchanged; do not add extra SHA passes or change biological policy.

**Final implementation acceptance:** Sol review accepted PCANIDX3, including the
post-release embedded-provenance lifetime fix. `ctest` records 188/188 passing in
23.56 s. V3 parity covers three load branches at 1/4 threads, files/counters/read
assignments/RAD, TSV removal, and legacy V2 byte identity. The initial fixture
baseline-authority and truncation-expectation failures were corrected without
weakening exact comparison. This is bounded validation, not a whole-genome
performance or fit claim.

**Pre-production launch checkpoint:** the frozen root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_prepared_reference_count_20261030_7jj7q9/`,
with tested `panCollapse`, build/CTest logs, RI binding, and
`index.sh`/`launch-index.sh`/`count.sh`/`launch-count.sh`/`restart-after-index.sh`.
After checkpoint review, authorized index container
`pancollapse-prepared-index-20261030-7jj7q9` started at
`2026-10-30T18:27:46.588311157Z`. Verified limits are 500,000,000,000-byte
memory and matching memory-plus-swap (no swap), with 128 cores. Read-only mounted
GNU `/usr/bin/time -v` is verified in the image and writes peak RSS to
`index.time.txt` (and later `count.time.txt`). The index upgrades prior
`DkAAtB/reference/annotation.pci` and its corrected ledger through
`index --from-annotation-index OLD_V2 --path-identity-ledger TSV --threads 128
--out reference.pci`. No GFF reparse, RI rebuild, or extra SHA verification pass
is included; TSV is a one-time input parse and the V3 writer atomically publishes
with inline checksum. `index.log` is active; no V3 index is published and no
count has started.

Only after index exit 0 may the supervisor launch
`pancollapse-prepared-count-20261030-7jj7q9`, using V3 without TSV and the same
GAMP/XG/GBWT/RI/whitelist paths. Host supervisor session 59300 waits for that
exit. Both stages retain 500,000,000,000-byte/no-swap/128-core limits and both
profiles; mounted native `/usr/bin/time` records peak RSS. Expected reader EOF
remains 66,601,887 input read groups. The corrected prior DkAAtB `RUN.json` now
records stop exit 143. Monitor the index and conditional count through completion,
diagnosing failures as needed.

**Index-build milestone (2026-10-30T19:49Z):** the one-time TSV read, cross-row
validation, and compaction completed for all 190,240,849 rows; existing PCANIDX2
facts loaded and validated successfully. `index.log` is in `binary_write` with
190,240,849 Parents, collecting the shared dictionary before parallel lexical
sort and fixed-record serialization. Native PID 3037317 was active with VmRSS
170,644,716 KiB (~162.7 GiB) and VmHWM 170,698,396 KiB under the unchanged 500-GB
no-swap cap. These are live observations; exact GNU-time RSS/timings await index
exit. No V3 index had then been published, no count had started, and GAMP was
untouched.

**Live update (2026-10-30T19:57:39Z):** container
`pancollapse-prepared-index-20261030-7jj7q9` remains running and not OOM. Shared
dictionary collection completed with 489,252,881 string references; the recorded
stage is `identity_dictionary_sort`, with parallel sort launched (129 total
threads), and writer canonicalization active. A sleeping worker pool is not sort
completion evidence. VmRSS is 175,577,128 KiB (~167.4 GiB), VmHWM 179,215,400
KiB (~170.9 GiB), and the 500,000,000,000-byte cap is unchanged. No V3 index had
then been published and no count had started.

**Publication milestone:** PCANIDX3 atomically published at
`2026-10-30T20:22:32.302287029Z` as
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_prepared_reference_count_20261030_7jj7q9/reference.pci`,
40,929,379,277 bytes (`stat` verified at 20:22:50Z). The index container remains
running in native destructor cleanup; native exit, GNU-time receipt, and flushed
stdout are still pending, and supervisor 59300 continues waiting for exit 0.
PID 3037317 RSS was 161,159,892 KiB at 20:22:50Z, falling after final write.
At that publication checkpoint, terminal build success and count start were still
unconfirmed.

**Terminal index / current count gate:** at 2026-10-30T20:25Z the index container
exited 0, not OOM. `index.stdout` JSON reports 190,240,849 Parents, 265,064 genes,
and 40,929,379,277 bytes; `index.time.txt` records wall 1:57:11, peak RSS
179,215,400 KiB (~170.9 GiB), zero swaps, exit 0. Supervisor 59300 completed 0
and started `pancollapse-prepared-count-20261030-7jj7q9` at
`2026-10-30T20:24:58.75885233Z`. Docker verifies Memory and MemorySwap
500,000,000,000 and NanoCpus 128,000,000,000. `count.log` records
`reference_load_parallel branches=3`, `prepared_annotation_index_begin`,
`xg_deserialize_begin`, and `gbwt_deserialize_begin`; XG completed in 13.4924 s.
By 20:25:20 the compiled dictionary loaded 90M entries with no TSV flag. PID
3170470 remains in reference initialization. No GAMP processing, whole-genome
fit, final output, or count completion is established. Monitor through completion
and diagnose any failure without new scientific claims.

**Prepared-count startup milestone (2026-10-30T20:32:27Z):** prepared annotation
index deserialization completed in 442.225 s, loading 190,240,849 Parents and
190,240,849 embedded ledger rows. Parallel owners completed XG in 13.4924 s,
GBWT in 222.931 s, then RI in 98.9993 s; these overlap intentionally and all
three joined successfully. No TSV parse occurred. PID 3170470 was running and
not OOM at VmRSS 193,390,372 KiB (~184.4 GiB). Graph/path compatibility
initialization, GAMP/read processing, and terminal count/output validation remain
pending; `RUN.json` retains the measured timings.

**Terminal failure checkpoint:** at about `2026-10-30T21:39:50Z`,
`pancollapse-prepared-count-20261030-7jj7q9` exited 1, not OOM, before GAMP or
output. Terminal error: `genefull_ex50pas cannot establish body/exon orientation`
for linked paths
`chr19::panSCbody1_c9d010286f5911382668f6fe055ae707550636e675a81168114a9c1ac8764113_R1`
and `chr19::HG00128_ha_T0269130_R1`. Index loading, bindings, lengths, and path
catalog completed; failure is in exact-parent geometry before an
`exact_parent_models` completion receipt. `count.time.txt` records exit 1, wall
1:14:46, peak RSS 289,479,060 KiB (~276.1 GiB), and zero swaps.

**Post-failure diagnosis (observed):** retained evidence under
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_orientation_diagnosis_20261030_030HFr/`
(`walks.tsv`, `original-gff-parent.tsv`, `original-gff-query.log`, and
`source-fragments.*`) shows the failing body fragment has 68 unique nodes and
shares none with the exon’s 62 nodes. Its sibling body fragments share 30 and 32
nodes, respectively, in the same orientation. The original CAT GFF3 gives three
`HG00128_ha_T0269130` exons totaling 712 bp; the failing 642-bp body interval is
entirely intronic. This does not establish bad linkage. It establishes that the
runtime shared-node-only orientation inference cannot handle this valid
fragmented-body case.

**Implemented orientation repair:** `ExactOrientationBasis::IntrinsicBody` applies
only to a zero-shared-node body/exon pair; it is B-only and derives `GD` from the
emitted body walk. `SharedSame`, `SharedOpposite`, repeated-occurrence, and mixed
shared-node ambiguity retain their prior behavior. This corrects a recovered v0.8
source contract (`e047b07`); it does not add a biological overlap rule, filter, or
new eligibility.

**Producer evidence accepted by review:** archived `004333prepare_chromosome.sh`
passed chr19 `final/retained_exons_and_bodies.gff3` (`004277`) to the candidate
`mpmap_index_chromosome_v2.sh`. Both layers use GFF feature type `exon`, preserve
strand, and pass through the same pinned `vg rna`; `004314` guide and `004317`
ledger receipts were restored. This evidence required neither sequence-walk
reconstruction nor a full-file hash.

**Acceptance and live launch:** `build/disjoint-body-regression-ctest.log` records
190/190 CTest passes in 25.78 s (the initial 25.37-s receipt is retained). The registered
`disjoint_body_exact_orientation` test exercises disjoint plus/minus body walks,
both direct-count profiles at 1/4 threads, shared-opposite preservation, and
mixed-parity failure; the frozen prior binary fails the intended orientation
error. The repaired launcher started
`pancollapse-intronic-fragment-count-20261031-jFlfrm-r1` at
`2026-10-31T00:37:41.339941051Z` (17:37 PDT) in the prepared root
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_intronic_fragment_count_20261031_jFlfrm/`,
reusing PCI/RI under 500,000,000,000-byte memory and memory-plus-swap limits, 128
cores, and both profiles. `count.log` records reference initialization; zero reads
are processed so far. The initial container exited 126 before panCollapse from an
extraneous `/bin/bash` launcher argument, did not open GAMP, and is retained. No
BAM, spill, remap, or full-file hash is authorized. Whole-genome fit, RSS, output,
and the 66,601,887 reader-EOF denominator remain unverified; `count.time.txt` is
future terminal evidence.

All following dated live observations are historical milestones of this terminal
attempt, not current-running status.

**Reference-initialization update (2026-10-30T20:57:55Z):** serial name binding
transitioned to 128-core position preparation by 20:46:40 (129 threads, about
12,800% CPU). PID 3170470 remains not OOM at VmRSS 232,359,584 KiB (~221.6 GiB)
under the unchanged 500-GB cap. GAMP processing has not started.

**Read-only architecture clarification:** PCANIDX3 is annotation-prepared rather
than complete graph-prepared; repeated graph binding/geometry remains the startup
gap. A graph-bound numeric projection can preserve exact semantics only with
occurrence coordinates, model links, and junction ownership. This is proposed
future work, not authorization to implement, stop, or change biological policy.
Existing native RI receipts/header give maximum physical node ID 116,844,906 (not
actual node count), bidirectional GBWT size 474,167,531,502 including 760,920,122
endmarkers, and 380,460,061 metadata paths. The ledger selects 190,240,849 paths,
but selected steps cannot be inferred by ratio. Conditional 16-byte-posting CSR
offset scenarios at 1/4/10 associations per addressable ID are 2.80/8.41/19.63
GiB for a deduplicated category layer only, not a full-index forecast. All-forward
occurrence expansion is 236,703,305,690 × 16 = 3.787 TB; selected volume and
deduplication are unknown, so no tens-of-GB full-size promise is supported.
Continue current count monitoring unchanged.

**Selected-path geometry milestone (2026-10-30T20:59:11Z):** `count.log` records
`selected_path_coordinates` for 190,240,849 paths: 224,984,793,940 steps,
3,801,283,990 checkpoints, 15,205,135,960 coordinate bytes, and zero wide paths;
it completed in 1446.72 s. `ledger_path_handles` completed all 190,240,849 paths
in 29.7063 s. Selected bindings and length checks therefore passed with no
mismatch. RSS is 234,185,140 KiB (~223.3 GiB), running and not OOM. Path catalog
and exact geometry remain before GAMP; no read counting is claimed.

The 224.985-billion selected-step count is measured, not a path-ratio inference.
A 16-byte-per-occurrence expansion of this measured volume is 3.59975670304 TB.
That does not imply a shared gene/category layer is terabyte-scale: its
deduplication is unmeasured. The 15.205-GB coordinate payload is checkpoints only,
not the complete reference or all path descriptors. `RUN.json` retains metrics.

**Path-catalog milestone (2026-10-30T21:29:15Z):** production-1 path catalog
completed all 190,240,849 relevant paths in 1793.76 s (29m54s), including
fallback classification in 245.137 s; it records 4,122,279 fallback-only Parents
and 23,439 fallback genes. All catalog paths are ready. The run is now in exact
geometry setup, with no `exact_parent_models` milestone and no GAMP processing
yet. RSS is 257,874,236 KiB (~245.9 GiB), running and not OOM. `RUN.json`
retains path-catalog metrics. This quantifies the remaining graph-prepared cache
gap only; it authorizes no new implementation.

On native exit 0 and atomic publication, completion validation uses existing
receipts without new full-file hash scans: verify required `manifest.json`,
`summary.tsv`, and `parquet/{barcodes,features,counts,molecules}.parquet`; inspect
manifest count-storage, profile, input/provenance, and declared table-schema/sort
fields; and reconcile the stable global and per-profile summary counters. These
are publication and contract checks, not a new fit/parity claim.

## D104 — Simplify assignment implementation in node space (2026-10-31)

**Decision source: User. Status: AUTHORIZED IMPLEMENTATION; production remains
stopped pending tests.** The user authorizes a dramatic simplification of the
over-5,000-line assignment implementation in node space. Existing biological and
profile rules are unchanged. This authorization replaces the immediately prior
read-only-only restriction for this implementation work, but does not authorize a
production restart, a new whole-genome run, or a biological-policy change.

The simplified node-to-gene mapping may retain the local ordering information
needed for assignment (`previous`/`next` node and strand). It must eliminate,
rather than serialize or retain as a new persistent reference artifact, the
measured `selected_path_positions` checkpoint state: 3,801,283,990 checkpoints,
15,205,135,960 coordinate payload bytes, and the 1525.94-second startup stage.
The stopped r1 run and all existing receipts remain historical evidence only.

The user statement that this mapping "should not [use] repeated-node identity"
does not yet resolve whether repeated-node occurrence identity must be preserved
or eliminated in the implementation. No alternative is selected here. Resolve
that exact semantic question before implementing behavior that depends on it;
do not silently change repeated-path assignment semantics. Required tests must
demonstrate preservation of the existing biological/profile rules before any
production restart is considered.

**Correction (2026-10-31):** Implementation is paused for a policy/authority
audit. D104 does not establish that assignments require tracked positions or one
ordered reference walk. The recorded checkpoint cost does not establish a need
for a positional algorithm. No separate direct user instruction requiring
single-walk containment or position tracking has been located. The original
node-membership rule remains the relevant historical baseline; D066 source
qualifier names overlap tiers and Parent preservation; the additional full-walk
restriction is not independently supported by a direct user instruction located
in this audit. Do not infer any new assignment policy from the paused
simplification objective. No production job, build, or test started during this
pause.

## D105 — Remove mandatory full-read single ordered body-walk containment (2026-10-31)

**Decision source: User. Status: AUTHORIZED SEMANTIC CHANGE; production remains
stopped pending targeted validation.** The user removes the requirement that a
read map to one ordered reference body walk. This supersedes D066 and D068 only
where they make full-read single ordered body-walk containment mandatory. It
does not add a biological rule, option, or a fresh whole-genome count
authorization.

Native `count` and the typed-union exact producer intentionally share the
affected code, so both change together. This does not promise historical
exact-output parity. Preserve transcript-specific exon/intron evidence, strand,
scores, splice support, and profile/barcode/UMI rules. The existing 190/190
CTest receipt is historical evidence, not acceptance of this change.

Required targeted validation must cover crossing body fragments, missing-node
body membership, reversed ordered projections, repeats, GAMP branches, and
E/P/B/RAD preservation. The current whole-genome r1 attempt remains stopped at
exit 143, not OOM, before GAMP processing. No production restart is authorized.

**Implementation status:** GraphInput coordinate/checkpoint support is deleted
(157 net source lines), but main.cpp call sites and containment eligibility still
need replacement. This intermediate source state is not build-ready; no build or
tests have run. Implementation is paused for the user's complexity/time report.

## D106 — Graph-specific prepared numeric node-counting index (2026-10-31)

**Decision source: User. Status: IMPLEMENTATION AND TESTING AUTHORIZED;
whole-genome index build gated; production count remains paused.** Build the
just-the-information-needed representation for this graph, directly callable
while streaming reads. It must contain all primitives required for prepared
counting and move reference parsing, path binding, catalog, model, and junction
preparation offline. The prepared count path must not load or reconstruct GFF,
ledger, XG, GBWT, or RI when the index provides those primitives.

The index contains only count-needed interned feature/metadata dictionaries,
node category/strand memberships, and junction ownership. D105's removal of
single ordered body-walk containment remains in force. No new biological rule or
CLI option is authorized; transcript-specific exon/intron evidence, strand,
scores, splice support, and profile/barcode/UMI rules remain unchanged.

This authorizes implementation, tests, and then one whole-genome index build
from existing inputs. First require small fixtures, direct read-lookup goldens,
and the applicable existing checks. Only then may a named, recorded container
build the index under the existing 500-GB no-swap limit, retaining operational
updates. Do not remap or restart a full production count. The original
whole-genome count goal remains paused. The current GraphInput partial is
intermediate and not build-ready; the existing 190/190 receipt is historical,
not D106 acceptance.

## D107 — Directed splice ownership excludes wrong-way candidates (2026-10-31)

**Decision source: User. Status: AUTHORIZED REQUIREMENT; implementation and
targeted validation pending review.** Splice ownership is strand-specific at
the oriented node-edge/transcript level. When a read crosses an indexed splice
edge in the wrong direction for a candidate transcript, exclude that candidate
entirely. Do not reinterpret this as an E-to-P or E-to-B downgrade. The
prepared junction-evidence adapter must correctly account for minus-strand
genes and the read mapping orientation.

This corrects junction-evidence implementation, not the frozen profile
definition or profile hash. D105 remains unchanged: no candidate may require a
single ordered full-read body walk, positional containment, or repeated-node
occurrence identity. Preserve the existing exon/intron evidence, strand,
scores, barcode/UMI, and profile rules otherwise. No whole-genome index build
or production restart is authorized by this decision. Add directed-junction
fixtures and rebuild/validate the prepared path before considering the existing
whole-genome build gate.

## D108 — Conditional whole-genome build and production count are active (2026-10-31)

**Decision source: User. Status: AUTHORIZED AFTER PREPARED-INDEX VALIDATION.**
After the prepared index works and its applicable validation gate passes, build
the whole-genome index from the existing inputs under a 500-GB no-swap limit.
Then run the whole-genome production count under 500 GB/no swap/128 threads,
with no disk spill. The whole-genome count goal is active.

This supersedes the prior paused-production status only for these conditional
new stages. Retain the stopped r1 container and every earlier receipt as
historical evidence. D105--D107 and their validation requirements remain in
force; this decision does not relax prepared-index, directed-junction, or
targeted-fixture gates. It does not authorize remapping or an unrelated
production workflow.

**D106 acceptance note (2026-10-31):** The current 102,781,736-byte binary
passed 195/195 CTest checks in 26.39 s (`build/ctest-prepared-final.log`),
including focused PCANCOUNT1 codec, scoring, and directed-junction checks and
the nine-read Parquet/MEX logical-count/molecule parity fixture at 1/4 threads
and joint/separate profiles. This accepts the prepared-path validation gate;
it establishes neither whole-genome fit, reader EOF, final counts, nor
production completion. No biological or profile rule changes here: prepared
counting remains free of runtime GFF/ledger/XG/GBWT/RI, DNA, and full-graph-walk
loading; D105 node-space evidence and D107 hard wrong-way exclusion remain in
force. D108's 500-GB/no-swap/128-thread/no-spill whole-genome stages are next
authorized but not launched.

**Construction-compaction acceptance (2026-10-31):** The first whole-genome
PCANCOUNT1 attempt was stopped at `2026-10-31T05:01:53.714Z` (exit 143, not
OOM) during construction, after reaching 339 GB peak RSS at 2.4 million of
95.1 million linked groups. The accepted engineering response is target-local
construction with deterministic per-target sample-support dictionaries before
encoding, shared 256-node/64-edge bitmap membership blocks, ZSTD rows, and no more than
32 concurrent geometry workers irrespective of the 128-thread outer limit.
This is a construction-memory decision only: model IDs and distinct scoring IDs
remain preserved, and D105/D107 are unchanged. The new PCANCOUNT2 path has no
current acceptance receipt; the former 195/195 result applies only to v1.
Re-run all 195 tests before restarting the authorized whole-genome build/count
under the same 500-GB/no-swap/no-count-spill limits.

**PCANCOUNT2 validation and launch note (2026-10-31):** The final
104,884,760-byte binary passed 195/195 CTest checks in 26.72 s
(`build/ctest-prepared-blocks-release.log`; copied to the `rywaxM` run root).
The suite covers count/molecule/MEX parity, the missing-edge fixture, and
byte-identical 1/4-thread index construction. This accepts the compacted
implementation, not whole-genome fit, reader EOF, output counts, or production
completion. The index container launched at `2026-10-31T05:19:29.175716381Z`
with the frozen 500-GB/no-swap, 128-core upper-limit, 32-geometry-worker, and
no-count-spill configuration; count remains queued until atomic index
publication. The v1 195/195 receipt remains historical only.

**Allocator validation gate (2026-10-31):** The first PCANCOUNT2 whole-genome
index container stopped at `2026-10-31T06:06:23.480629105Z` (exit 143, not
OOM), before count. Its terminal construction statistics were 9,205,174,225
steps/225 billion, 3,936,680 Parents, 4,613,383,674 payload bytes, and
176,450,789,376-byte peak RSS (~176 GB). Review identifies a credible late
`std::vector` payload reallocation that could double memory above 500 GB even
when the final representation fits; this is not a demonstrated final-fit
failure.

The accepted engineering fix is an `EncodedBuffer` of 256-MiB RAM slabs,
appended once and streamed by used regions into the same flat PCANCOUNT2 encoded
section. Reader behavior, format, policy, disk-spill/I/O contract, and D105/D107
semantics are unchanged. The seven-byte-boundary raw/ZSTD tests, full
pre/post-fixture byte comparison, and a fresh 195-test run remain required
before restarting the authorized 500-GB/no-swap/128-thread/32-geometry-worker,
no-count-spill whole-genome stages.

**Slab validation and current launch note (2026-10-31):** The seven-byte
raw/ZSTD boundary checks and complete pre/post fixture byte comparison passed;
the final 104,928,704-byte slab binary passed 195/195 CTest checks in 27.04 s
(`build/ctest-prepared-slabs-final.log`). The frozen `iVn7rR` run root launched
its index container at `2026-10-31T06:12:28.764761604Z`; PID 157285 is in source
initialization. Its 500-GB/no-swap, 128-CPU upper-limit, 32-geometry-worker,
no-count-spill script retains both profiles and the 66,601,887-group input
expectation. No index publication or count start exists. This validates the
allocator-only repair, not final fit, reader EOF, or output results.

## D109 — Load-ready annotation object boundary after stopping the path-step index (2026-10-31)

**Decision source: User. Status: ARCHITECTURAL REVIEW AUTHORIZED; no new
implementation or production restart.** The user stops the current path-step
PCANCOUNT2 index as untenable. The next custom index is narrowed to a
load-ready annotation object, queried in combination with one or more of
XG/GCSA/GBZ/RI for all reference-dependent information. It must require no
preprocessing or rebuilding after load and before count.

D105's lack of ordered-walk containment and repeated-occurrence identities,
D107's directed-splice exclusion, and the per-gene-count goal are unchanged.
This decision selects no mechanism and makes no new biological or profile rule.
The next authorized task is a read-only architectural feasibility review, not a
production restart. The slab run stopped at `2026-10-31T08:15:15.87979019Z`
(exit 143, not OOM) before `counting.pci`, GAMP processing, or counts; its
195/195 and fixture-byte receipts remain implementation-only historical
evidence. During continuation, report progress at most every 30 minutes except
for direct answers or stopping a job.

**D109 evidence note (2026-10-31):** Read-only API review finds that native
FastLocate nodeDA supplies numeric oriented path IDs with visit multiplicity and
two-node find/locate supplies oriented adjacency IDs, but neither carries gene,
exon, or body-policy semantics. RI (44,433,079,158 bytes), GBWT
(44,562,796,448 bytes), and XG (5,560,818,652 bytes) are present; no hashes
were taken. Local GBZ loading invokes named-path LF-walk caching, while normal
XG+GBWT+RI deserialize/attach does not. That observed distinction selects no
route. Source review also finds an implementation-parity risk: the PCANCOUNT2
model equality at `src/count_reference_builder.cpp:122` can quotient different
raw exon Parents sharing full exon geometry, after which the construction at
`:245` combines body fragments/supports. Legacy D105 code at `main.cpp:4117`
retains raw exon-by-body pairs within Parent. Equivalence is unproven and the
195 small fixtures do not cover this counterexample; add a regression fixture
before future promotion. No whole-genome output exists, so this evidence does
not establish a wrong count.

## D110 — Approved load-ready annotation implementation (2026-10-31)

**Decision source: User. Status: IMPLEMENTATION AND TARGETED VALIDATION
AUTHORIZED; no whole-genome launch before gates.** The approved route is a
custom numeric annotation/metadata object used with XG, transcript GBWT, and
the existing RI; it is not a 225-billion-path-step index. Counting must load or
mmap the object, attach the native references, and start GAMP processing with no
string joins, metadata/path/node prepasses, `prepare_paths`, FastLocate
construction, geometry builder, or global warmup. Persist ready raw exon/body
pairs and policy/support facts. GBWT and RI may only serve touched-node and
local-edge queries.

D105 and D107 remain unchanged, as do frozen profile, barcode/UMI, and output
contracts. The unproven PCANCOUNT2 cross-Parent geometry merge is excluded from
this new route. Implement the bounded path with its parity-corner-case and
direct-native-query tests, then require a new full suite and performance fanout
gate. Both-profile whole-genome index/count authority remains conditional on
those gates under 500,000,000,000 bytes, no swap, 128 threads, and no spill;
do not launch yet. The existing RI is 44,433,079,158 bytes; no hash is required
unless it speeds the work. The source remains `327b048...` with preserved dirty
state; current containers are stopped and no index or count exists. Historical
195/195 receipts are not acceptance for D110. Continue updates at most every
30 minutes except for direct answers or stopping a job.

**D110 implementation continuation (2026-10-31):** `native_count_index.*` and
`native_count_reference.*` now provide PCANBND1 metadata-only compilation from
raw Parent exon-by-body pairs, CSR, and a feature catalog; associated prepared
CLI/runtime, resolver/shared-source, and verifier work avoid path/node walks,
the PCANCOUNT2 cross-Parent geometry merge, GFF/ledger parsing, reference joins,
and a count prepass. Native count loads the ready object with XG, transcript
GBWT, and RI for touched-node/local-edge queries. The trusted PCI3 reader skips
the whole-payload SHA while strict legacy checksum behavior remains default;
the native path relies on existing RI receipt sizes/prior hashes. The
FastLocate sequence-ID/packed-SA defect is fixed. A D107 empty-subpath
Gene-fallback leak now has a source-level endpoint-propagation fix covering
empty and insertion-only subpaths over `next`/`connection` links. The combined
Docker build passed, as did six focused codec/query/direction/runtime/index
tests in 0.91 s (`build/ctest-native-basics.log`). The semantic recheck accepts
the empty-subpath fix without a biological-policy change. A warning-only rebuild,
integration checks and the complete suite remain pending; no whole-genome
acceptance is claimed. The subsequent focused gate passed 11/12 tests: the
nine-read native/legacy biological tables, logical hashes and MEX match, but
one antisense terminal changed from opposite-strand to no-evidence when D107
removed its last candidate. That also loses barcode-stage eligibility. Preserve
rejected evidence only for a resolver-local pre-D107 diagnostic evaluation when
allowed exact evidence is empty; use the unchanged frozen resolver for stage
precedence, never expose vetoed candidates or permit a gene assignment from
that diagnostic view. This is an implementation-parity fix, not a profile or
biological-policy revision. Required gates are focused codec/query/direct
parity at 1,2,4,8,16,128 threads, a 1000-alias fanout fixture (1001 expected),
and the complete existing CTest suite before an immutable fresh native
index/count launch. The active goal remains both-profile output for the
66,601,887-group GAMP input under the existing 500-GB/no-swap/128-core/no-spill/
no-BAM authority; no production run root exists and production remains stopped.

**D110 gate acceptance (2026-10-31):** the final binary passed 198/198 CTest
checks in 31.03 s (`build/ctest-native-full-final.log`); the repaired focused
gate passed 12/12 in 6.40 s (`build/ctest-native-focused-repaired.log`).
Native/legacy tables, logical hashes, MEX, terminal/barcode-stage counters
match on the synthetic integration cases across 1/2/4/8/16/128 threads and
joint/separate/derived profiles. The fanout fixture retains 1001 raw models
from 1000 extra aliases (four-group native read phase 0.00711 s, not a
whole-genome speed prediction). D107 keeps forbidden candidates out of
returned facts/counts while privately preserving frozen terminal/CB/UMI
precedence. The conditional launch gate is satisfied. A fresh run root is
prepared at `/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/`; production
metadata-index construction and then unchanged-GAMP counting are next under
the already approved limits. No whole-genome result or fit is claimed.

**D110 native launch:** run root `/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/`
retains the frozen 111,622,752-byte binary, source archive and final receipts.
Replacement container `pancollapse-native-query-count-20261031-lkmga0-r1` started at
`2026-10-31T10:30:08.173300924Z` with verified 500-GB/no-swap/128-CPU
limits; PID 555663 is loading the existing annotation object for metadata-only
compilation. The first wrapper exited 127 before native work because GNU time
was absent; Bash's builtin timer replaces that optional dependency and the
failed attempt remains preserved. No index publication, GAMP processing, final
fit or whole-genome completion is established by this launch.

**D110 resource/performance gate (2026-10-31):** In the same run root,
`index.receipt.json` records completed PCANBND1 construction: 9,765,357,968
bytes, 190,240,849 paths, 95,130,819 raw pairs, 1749.71 s reported/1771.748 s
process wall, 183.38-GB cgroup peak, and no path steps. The subsequent count
loaded index/XG/RI/GBWT in 4.37089/14.6753/95.8872/176.179 s, reached ready in
181.525 s without query preload, and measured 7.81606 s for first 1k and
70.7681 s for 10k groups. It was deliberately SIGTERM-stopped at 11:08:59 UTC
as cgroup use reached 476,584,603,648 bytes (RSS 460,924,900 kB), before the
500-GB cap; Docker exited 143 at 11:10:07 UTC, OOM false, with 627.091 s total
in `count.time.txt`. No EOF or output exists; the completed index is safe to reuse.
This is not proof of a memory leak: WorkerCache clearing and transient
models/state/candidates plus 128 glibc arenas are plausible pressure sources.

An accepted but unqualified optimization preserves conservative unique-chain
fast-path `advance_state`/emission and retains generic DAG as a forced oracle;
it adds flat touched-model sort/unique, binary D107 lookups, and model hoisting.
It introduces no raw-pair deduplication, geometry merge, hit cap, or profile
rule change. Before restart, require exact frozen-binary versus optimized
`first10000.gamp` outputs/counters (10,000 QNAME groups, 25,000 records,
8,506,683 file bytes with unchanged protobuf payloads), complete CTest, and
allocator A/B if needed (production-image availability/compatibility unverified).
Only then restart the immutable full GAMP with the same completed index under
500 GB/no swap/128 threads/memory-only limits. No whole-genome count, sustained
speed, biological accuracy, or parity result is claimed.

**D110 strict-prefix and read-local-DP gate (2026-10-31):** The frozen
128-thread strict `first10000.gamp` control corrects the earlier unordered-10k
milestone: it was SIGTERM-stopped at 11:21:11 UTC with last anonymous RSS
453,770,848 kB, cgroup 498,153,754,624, peak 499,999,997,952, OOM 0,
`memory.events:max` 3223, and 39 threads remaining; Docker exited 143 at
11:22:08.815 UTC after 592.294 s. No EOF/count outputs exist and the native
index is unchanged. This neither proves a leak nor establishes a resource root
cause. The frozen plain 16-thread control container
`pancollapse-native-prefix-control16-20261031` started about 11:25:14 UTC.

Stage 1 (conservative unique-chain fast path, flat touched-model sort/unique,
binary D107 lookup, model hoist, and statistics) has focused 1/1 and 3/3
passes and 198/198 complete CTest passes in 60.57 s
(`build/ctest-native-linear-full.log`), but is not real-data resource-qualified.
The run root freezes `panCollapse-linear` and `source-linear.tar.gz`.
The approved computational-equivalence implementation may
group per-alignment DP only when full effective read-node membership/presence/
flags, all reachable effective oriented transitions, per-transition D107
known-target/owner flags, and fixed support status agree. Fixed native-body
support sets may be alpha-renamed internally then restored per raw model during
tier/candidate replay; mixed support IDs use original generic DP. Policies and
strand stay terminal, policy ownership stays deferred to replay, and transcript
variation is allowed only with complete D107 equivalence. Preserve every raw
pair ID, duplicate fact, emission order, and profile rule; retain generic DAG as
a forced oracle. No geometry merge, hit cap, or ordered-containment rule is
authorized. Required oracle coverage includes fixed-empty/multilabel/overlap and
mixed support, policy/transcript/edge/D107 differences, branches/scored merges,
empty/insertion, raw duplicates, and hash collisions. Require full CTest, exact
frozen 16-thread prefix outputs/counters, and a resource gate before full
restart. Host jemalloc 5.3 passed an image load smoke test but is only an
optional diagnostic, not an adopted allocator.

**D110 optimized-prefix validation checkpoint (2026-10-31):** Three original
10k baseline attempts did not complete: 128-thread was SIGTERM-stopped at the
cap, 16-thread glibc OOMed, and 16-thread jemalloc OOMed at
11:45:19.492 UTC. Final sampled heap estimates (`heap-frozen-i9.txt`) are
539,604 MB total, resolver 424,901 MB (~79%), propagation lambda 190,945 MB,
candidate-for 35,232 MB, and GBWT name metadata 42,240 MB. These are estimates,
not exact RSS or proof of a leak. `clearMetadata` is placed after numeric-binding
checks and before FastLocate attach; native APIs retain GBWT BWT/header and use
no derived GBWT data.

Stage 2 conditionally groups exact read-local behavior and executes independent
groups serially to bound live DP. It retains the all-model `force_generic`
small-fixture oracle and adds a `force_raw_model_serial` control. Sol approval
requires per-sink/profile allowed/vetoed buffers to restore
sink-to-raw-model-to-StateKey order. Preserve raw pairs, facts, multiplicity,
all biological/profile rules, D105/D107, and CB/UMI behavior. Multi-corrupt
input exception ordering is not promised; single-fault tests are required.

The completed 9.77-GB native index remains reusable. A frozen 100-group,
four-thread control (`pancollapse-native-frozen-first100-20261031`) is running
only to obtain an actual frozen-EOF baseline after the 10k failures; no outcome
is recorded. Before any full 66,601,887-group restart, require full facts/results
against the old oracle, metadata-clear API-query equality, full CTest, frozen
100 exact outputs against raw-serial and optimized modes, and raw-serial 10k
versus optimized 10k outputs/counters/resource. The eventual restart retains
the 500-GB/no-spill/no-BAM/no-extra-hash boundary. No scientific parity, count,
or performance success is established.

**D110 candidate-interning checkpoint (2026-10-31):** Stage 2 passed 198/198
in 30.24 s and restored automatic closure passed 7/7 in 5.72 s. Frozen-100
reader/gen output (209 records, 156 count rows) exactly matches automatic and
raw-serial in all four Parquet tables, logical hashes, MEX, and scientific
counters (`first100.stage2-parity.json`). The raw-serial 16-thread 10k resource
gate instead SIGTERM-stopped at 12:14:11 UTC, exited 143/OOM false at
12:14:59.802 UTC after 530.260 s, and last completed 3863 groups; sampled peak
RSS was 443,453,325,312 bytes and cgroup peak 435,976,323,072 bytes. The
grouped 16-thread jemalloc diagnostic also SIGTERM-stopped at 12:19:45 UTC
before cap; the prior pending-exit wording is historical. Its sampled heap estimate is 255,375 MB total:
`emit_exact_candidate`/`resolve_one_serial_group` 167,655 MB (~66%),
`candidate_for` 27,632 MB, `flush` 5,568 MB, and node membership below 250 MB.
This is evidence of terminal-candidate-buffer pressure, not exact RSS or proof
of cause. The reusable 9.77-GB index remains ready.

The proposed stable interning of structurally literal-identical
`AssignmentCandidate` values awaits minimum-coordination review and root
approval before implementation. If approved, use four separate per-profile
allowed/vetoed exact/fallback pools; retain the minimum canonical
(record,sink,raw-model,StateKey-emission) coordinate and final sort because
generation is noncanonical; and intern early, not after buffering. Default
retention is `all_raw`; compact facts are native-production opt-in and alter
only internal multiplicity. Preserve raw pair IDs/facts, all result/reason,
CB/UMI, profile, and output behavior: no Parent geometry/policy merge,
state-key quotient, lower-score pruning, or hit cap. Before any restart require
full-result and stable unique-fact projection comparisons across old all-model,
raw-serial, grouped modes and typed overrides, hash collisions, pools, and
ordering; then full CTest, frozen-100, and real-10k resource gates. Original
10k oracle parity remains unmet and is not waived. No whole-genome count,
release, accuracy, or scientific parity claim is made.

**D110 Stage 3 qualification checkpoint (2026-10-31):** Full-literal,
per-profile four-pool candidate interning with success-only numeric memoization
is implemented in automatic native `stable_unique_memoized`; legacy `all_raw`
remains unchanged. Sol source acceptance is conditional. An initial full suite
passed 198/198 in 30.34 s; the final all-15-override and novel-profile suite
passed 198/198 in 30.80 s (`build/ctest-native-interned-final.log`). The 16-thread real-100 control exited 0/no OOM at
`2026-10-31T12:46:32.549458304Z`; `first100.interned-parity.json` records exact
frozen-100 Parquet/MEX/scientific-counter/logical-hash parity, 156 count and
156 molecule rows, and read/ready/final times 1.545567871/144.149925717/
0.043175297 s. Its independent reader passed
(`interned-first100-t16.validation.json`); sampled max RSS was
131,736,555,520 bytes and cgroup peak 132,202,790,912 bytes.

The live 128-thread interned 10k control
`pancollapse-native-interned-first10000-20261031`
(`ff854b5a7742bb5972f2edb331ad04dca3da37851555a9879268db5a046cf655`) was at
9,981/10,000 groups with 128-GB RSS at 90 s; no EOF is established. A compact
raw-model-serial 10k 16-thread control is also live:
`pancollapse-native-interned-raw-serial-first10000-20261031`
(`66fd5937483da7e0c7dea4638993fe6b5368a7999ff14fa47d1d0c9ed97d6a8b`);
it also has no EOF.
The earlier grouped-jemalloc 10k run exited 143/not OOM at
12:20:36.087636348 UTC after 307.687 s with RSS 377,611,792,384 bytes. Before
the syntax-valid `run-count-only.sh` may launch, require final full CTest,
full-10k independent-reader validation, and exact raw-model-serial compact
control comparison. The reusable 9.77-GB index and XG/GBWT/RI stay fixed; any
subsequent count remains default-Parquet, memory-only, 500 GB/no swap/up-to-128
threads, no spill/BAM/hash. Whole-genome counts, old-10k oracle parity, F1, and
release remain unestablished.

**D110 exact read-local subpath-transfer checkpoint (2026-10-31):** The strict
interned 10k control fits at roughly 120--132 GB but is computationally
tail-bound: 9,987 groups at 390 s, approximately 1.4 billion state cells,
4.54 billion raw terminal emissions, and 24.4 million retained candidates.
This identifies computation rather than the reference index as the remaining
gate; neither control has completed and all receipts remain preserved.

Sol accepts a source proof, not implementation, for minimal mechanism 2: lazy
per-resolve/group/subpath delta state compiled with existing `advance_state`
from EMPTY after checked incoming-score reachability. Cache effects are keyed by
complete incoming last-tuple `(has,node,reverse,atend)`; an empty probe carries
only that tuple through the first reference-consuming mapping. Retain only
splice-concordance and wrong-F/R in the delta; combine four counts additively,
body anchor by OR, support by union, splice by AND(entry,delta), wrong flags by
OR, and overwrite final tuple only when delta has one. Reference-free subpaths
validate every mapping but leave evidence/last unchanged. There is no StateKey
quotient, score change, nested threading, or relaxation of the all-raw/
forced-generic oracle. Immutable valid source behavior must be exact;
multi-corrupt source error order remains outside the existing single-fault
contract.

The same proof accepts compact-only sink-local stable-unique terminal projection
before alias expansion: key tier, score, strand, wrong-F/R, and exact
non-normalized body-support set only for body tier; omit normalized support then
restore fixed alias support, retain first StateKey ordinal, and materialize all
alias policies separately. Preserve raw emission metrics/multiplicity. Root
reviewed this checkpoint and accepted bounded implementation at 13:09 UTC;
implementation and empirical gates remain pending. Required tests cover D107 orientation, node-end,
same-node/body-edge, empty/insertion/repeats/support union, multi-sink/scores/
unknown edges/source faults/hash collisions, plus unchanged full raw facts and
identical compact facts/full results/state-cell counts. Require full 198 and
complete 10k output comparison against frozen Stage 3 before whole-genome
launch; no F1 or release claim is authorized.

**D110 internal-count oracle amendment (2026-10-31):** After feasibility
review, complete frozen Stage 3 automatic 10k true EOF is accepted as the
direct optimization oracle for an internal, nonrelease whole-genome count. The
frozen baseline must retain its binary, input-prefix manifest and payload
validation, index identity, profiles, and configuration. Stage 4 must reach the
same true 10k EOF and independently match all four Parquet tables, logical
hashes, MEX, scientific terminal/CB/UMI counters, and route/model/state-cell
totals; it must remain below 500 GB and operationally viable. Only source-query,
memo/materialization, timing, and RSS metrics may differ.

Raw-model-serial 10k, Python/F1, and release certification remain unmet and are
not waived. This accepts residual risk of a latent shared Stage-3 grouping bug,
with frozen real-100 and all-raw unit anchors only. No additional large pilot
is required for the current goal. On the stated Stage-4 gate, an internal
provisional both-profile whole-genome count may use the existing 9.77-GB index
under 500 GB/no swap/up-to-128 threads, memory-only/no spill/no BAM/no hash;
it may not be promoted or released. Operational receipts and stopping the
running raw-serial diagnostic are outside this documentation amendment.

**D110 original-cache restoration review (2026-10-31):** Stage 4 transfer and
terminal projection are implemented and passed focused 1/1 in 0.91 s and full
198/198 in 31.06 s; frozen artifacts are `panCollapse-transfer`,
`source-transfer.tar.gz`, and `ctest-transfer-full.log`. Its 128-thread 10k
control reached 9,997 groups at 1320 s, versus frozen Stage 3's 9,997 at
3630 s, with matching completed-model/route/state-cell/logical-emission/
retained totals at that milestone and 132-GB peak. Neither run has EOF or output
parity; whole count remains stopped.

Observed omission: `prepared_count.cpp:258` always calls `resolver.resolve`,
and `main.cpp:3160` bypasses legacy assignment cache paths at 2447/5305/6108
(256 shards, 655,360 entries of `vector<AssignmentResult>`); native assignment
cache metrics remain zero. TLS node/edge caches clear each group. Sol
conditionally accepts a restoration proof only, pending root approval before
implementation: a per-Count fixed-reference/profile cache before Source
queries, keyed by full canonical serialized read (ordered record boundaries and
count, every sequence length/start vector, and complete Subpath Position/Edit
fields; fallback consumes `to_length` and Edit-sequence emptiness), excluding
QNAME, sequence contents, and qualities. Cache only successful final result
vectors after D107 shadow/fallback/reasons; preserve molecule parse and barcode
observation before lookup, terminal/UMI observation after lookup, and
`begin_read_group` on hits. Full key equality/hash routing, immutable leased
values, outside-lock misses, and bounded approximately 512-MiB/655,360-entry
memory are required; insertion/eviction remains parent-pending. Cache-on/off
must exactly match frozen Stage 4 10k four tables/hashes/MEX/scientific counters;
route/model/DP work may decrease but must not be faked. Require synthetic
finite concurrency, collision, key-field, CB/UMI, oversize, and throwing-builder
tests.

Separately, Boost 1.90 header-only `SupportIds=small_vector<uint32,1>` and
`SupportSet=flat_set<uint32,stdless,SupportIds>` is an accepted source proof
only; it preserves ordered unique values and lexical StateKey, and may likewise
inline NumericMemoKey support IDs. It is not implemented. Direct-container
operation/order/copy/spill checks, full 198, and frozen-stdset 10k parity are
required. Neither proof changes policy, D105/D107, whole-count, F1, or release
status.

**D110 A/B/C implementation authorization (2026-10-31):** Root authorizes
bounded implementation, not validation-gate or whole-launch acceptance. A
restores the original fixed-run/profile successful final-assignment cache in
native prepared count before Source queries. Its canonical serialized-read
evidence key includes record bounds/count, every start and sequence length, and
all subpath Position/Edit/score/next/connection fields; it excludes QNAME,
sequence contents, and qualities. The existing reusable sharded cache is
extended with full equality, outside-lock builds, no disk/CLI, a 512-MiB charged
limit, and 655,360 entries. `begin_read_group` remains first and molecule/barcode
and terminal/UMI observations always execute across hits.

B replaces numeric `std::set` support storage with inline
`small_vector<uint32,1>` `flat_set` and inlines numeric-memo support vectors,
preserving exact ordered values. C changes compact automatic map insertion to
`try_emplace` and successor lookup to `lower_bound`, retaining existing full-key
and strict score-only maximum semantics with checked-add first. All-raw,
forced-generic, and raw-serial remain unchanged. Sol proofs accept B/C and
conditionally accept A. New source/binary, focused/full tests, and exact
complete-10k output gates remain required; cache work metrics may decrease only
for actual work avoided. Frozen Stage 3/4 controls remain no-EOF at roughly
9,997/9,998 groups and 132-GB peak. A bounded detached GDB snapshot confirms
map find/iteration and set-clone allocation only, not percentages. No whole
count, F1, or release result is established.

**D110 Stage 5 cache/inline/compact checkpoint (2026-10-31):** A/B/C is
implemented and parent-reviewed against frozen Stage 4. Cache charging now
includes full string key plus approximate map/control/bucket and result-payload
cost globally within 512 MiB; it is not value-only. Legacy insertion-only
default, outside-lock builders, full equality, and observations remain unchanged.
Focused `native_count_query_test` passed 1/1 in 1.00 s; full 198/198 passed in
32.40 s (`build/ctest-cached-inline-full.log`, copied to the run root). Native
CLI legacy parity passed finite duplicate-evidence, different-CB/UMI-quality,
featureless, wrong-strand, and uncorrectable cases; headers cover full-key
mutations, support semantics, concurrency, collision, oversize, and failures.
Frozen artifacts are `panCollapse-cached-inline` (114,174,464 bytes) and
`source-cached-inline.tar.gz`.

The same-prefix 128-thread real-10k gate started at 14:28 UTC in
`pancollapse-native-cached-inline-first10000-20261031`
(`173f144717edcef1ff01a227163f9ca4a8936ab5a37fccbc13b29eabfbf4f30c`) using
existing native.pci/XG/GBWT/RI and same profiles under 500 GB/no swap. Index
load took 4.28836 s; initialization is ongoing and no EOF/output parity exists.
Frozen Stage 3/Stage 4 controls remain at 9,998/9,999 groups without EOF. This
does not authorize a whole count, F1, or release claim.

**D110 provisional parallel-count scheduling amendment (2026-10-31):** Sol and
root authorize scheduling only: frozen Stage 5 full whole-genome count may run
in parallel with unfinished Stage 3 and Stage 5 10k controls. This waives no
parity condition. Stage 4 has genuine 10k EOF (25,000 records/10,000 groups),
exit 0/no OOM at `2026-10-31T14:51:30.834648311Z`; independent reader receipt
`transfer-first10000-t128.validation.json` passed with 14,004 count and 14,095
molecule rows, ready/read/final 154.653820861/4997.695011896/0.810682403 s,
wall 5171.601 s, CPU 18636.597 s, and 132,201,127,936-byte peak. Stage 3 and
Stage 5 remain live at 9,999 groups without EOF.

Full output at `counts/` is PROVISIONAL_UNACCEPTED: do not publish, interpret,
or use downstream until exact Stage 3/4/5 true-EOF comparison of all four
Parquet tables, logical hashes, MEX, and scientific counters, plus independent
full-output validation. The unchanged `run-count-only.sh`, source/index/
GBWT/XG/RI/GAMP/whitelist, default-Parquet memory-only path, and 500-GB/no-swap/
128-CPU limit apply. Stop full count first for parity mismatch, source fix,
OOM/limit pressure, or host MemAvailable approaching 100 GB. Launch requires a
fresh preflight: MemAvailable >=660 GB (last 691.708 GB), no swap, 256 host
CPUs, 11-TB disk, and absent destination. This authorizes neither scientific
acceptance, output use, F1, nor release.

**D110 provisional whole-count launch (2026-10-31):** After parent review and
fresh preflight, frozen Stage 5 launched at `2026-10-31T15:09:26.107895372Z` in
`pancollapse-native-whole-cached-inline-20261031`
(`7648d97ab46b21c9cbd18c68b5b4830550a0ae8b29d40db4c2d37355311251a6`), host
init PID 1230154. It uses frozen `panCollapse-cached-inline`/source and
unchanged `run-count-only.sh`, native.pci/XG/GBWT/RI/full GAMP, 128 threads,
500-GB memory and memory-plus-swap/no-swap limits, default Parquet/memory-only
output, and no BAM/spill/hash. Preflight: MemAvailable 691,667,886,080 bytes
(>=660 GB), 256 CPUs/no swap, 11,916,846,014,464-byte disk, absent destination/
staging, and unchanged control PID/executables. Ordinary index/XG load took
4.87111/6.76435 s; other loading continues, with no read stream yet. Resource
sampling `whole-cached-inline.resources.tsv` is active. `counts/` remains
PROVISIONAL_UNACCEPTED pending all exact Stage 3/4/5 10k and independent
full-output gates; stop conditions are unchanged. No scientific acceptance or
whole-genome completion is claimed.

**D110 Stage 5 10k terminal and whole-streaming update (2026-10-31):** The
Stage 5 same-prefix control reached genuine EOF (25,000 records/10,000 groups),
exit 0/no OOM at `2026-10-31T15:24:07.078856015Z`; independent reader receipt
`cached-inline-first10000-t128.validation.json` passed. It exactly matches
Stage 4 in all four Parquet tables, logical hashes, MEX, and scientific counters
(`first10000.cached-inline-vs-transfer-parity.json`): 14,004 count and 14,095
molecule rows; ready/read/final 159.204247955/3141.699891474/0.465537699 s,
wall 3318.905 s, CPU 13121.952 s, system 247.774 s, and peak
132,201,291,776 bytes. Cache totals are 9,382 entries, 618 hits, zero uncached,
and 159,451,941 charged bytes; reduced work counters reflect actual cache hits.
Stage 3 remains at 9,999 groups without EOF, so the all-three gate remains
incomplete and full output quarantine continues.

The provisional whole count is streaming: index/XG/RI/GBWT load
4.87111/6.76435/38.0166/158.238 s with zero pre-read native queries; it reached
230,095 groups at 750.008 s (`2026-10-31T15:24:53Z`), about 279-GB cgroup peak,
no OOM, and host MemAvailable about 503 GB after Stage 5 control release. This
is operational progress only; it establishes no full EOF, output validity,
scientific acceptance, F1, or release.

**D110 live whole-count monitor checkpoint (2026-10-31):** At
`2026-10-31T16:31:10Z`, provisional frozen Stage 5 whole count is genuinely
streaming at 1,200,000/66,601,887 groups after 4734.65 s (PID 7, 128 threads,
500 GB/no swap). Current/peak cgroup values are 390,104,215,552/
406,490,640,384 bytes; `memory.events` max/oom/oom_kill are all zero, and host
MemAvailable is 394,150,000 KiB. This establishes neither EOF, finalization,
output validity, nor scientific acceptance. Stage 3 control remains live at
9,999 groups after 13260.1 s on one active CPU core, so Stage 4/5 exact 10k
parity remains partial of the required three-way gate. The existing
batch-streaming 65,536-row `validate_counts.py` is reviewed ready for full EOF.

Scoped cleanup stopped only idle developer container `3d21f8a16391` at 15:33:50
UTC (tail process only; no files deleted); full count and required old-parity
control remain active. Restart the developer container only on demand for
validation, then stop it again. Only full count and required Stage 3 control are
active. User update was sent at 16:31 UTC (the earlier 16:11 marker was prepared,
not sent); next is not before 17:01 UTC except for an exception. Read-only memory audit observes NativeSource
TLS caches clear per group and fixed references; completed-name and exact/
deferred-UMI aggregates plus DP/allocator high-water are plausible RSS sources,
not proof of leak. No source fix, policy/semantic change, or new experiment is
authorized; recurrence detection and existing stop/quarantine conditions remain.

**D110 performance-diagnostic transition (2026-10-31):** User amends the goal
to full same-GAMP per-gene counts in <2 h wall and <300 GB RAM, and directs
stopping the provisional whole run for diagnosis. Root sent SIGTERM to count
PID 7 at 16:33:45 UTC, then stopped its remaining wrapper container. Docker
confirms exit 137/no OOM at `2026-10-31T16:34:24.640829452Z` (user stop, not a
memory failure), recorded in `RUN.json`. Last measured
milestone was 1,236,704 groups in 4890.05 s, 406,490,640,384-byte peak and
391,276,007,424-byte pre-stop current cgroup memory, max/oom zero, and no EOF
or final counts. This is about 253 groups/s versus about 9,250 required for two
hours (~37x gap): measured performance, not a diagnosed cause. No restart is
authorized before a justified read-only diagnosis/approach. The next task is
read-only sealed-source/statistics performance diagnosis; no implementation,
extra large-input hashing, or full rerun is authorized. Stage 3 independently
reached genuine EOF (25,000 records/10,000 groups), exit 0/no OOM at
`2026-10-31T16:37:50.15274514Z`: processing/wall/user/system
13672.3/13842.887/43214.267/306.462 s, 14,004 count and 14,095 molecule rows.
Native query stats were 1,076,285 queries/889,626,679 occurrences; fallback
records 24,892; linear 12,132; groups 130,767; DP 336,017,391. Resolver counts
were records 24,739; fast/generic 12,030/12,709; raw models 50,767,307; groups
14,050,176; states 22,036,880,196; raw emissions 62,136,132,362; retained
25,727,760; memo/materializations 62,110,380,842/25,751,520. These route/model/
state totals match Stage 4 expectation. Existing-file three-way comparison
passed (`first10000.three-way-byte-parity.json`): Stage 3 versus Stage 4 and
Stage 3 versus Stage 5 have byte-identical four Parquet tables and six compressed
MEX files; both-profile definitions/logical tables/global table logical hashes
and scientific summary counters are equal, excluding only `assignment_cache_`
and `aggregate_spill_`. Stage 3 independent reader was not rerun; Stage 4/5
were already independently validated. This is limited to genuine-EOF 10k
existing outputs, not full genome, F1, or release. All containers remain stopped.
**D110 approved performance phase (2026-10-31T16:49Z):** User authorizes
semantics-preserving performance work: consequential design/proof review of
alias pre-deduplication, shared immutable graph facts, and DP factorization,
then bounded implementation/tests. No biological rule changes: D105 remains
without ordered containment, D107 remains wrong-way splice veto, and raw Parent
exon/body facts remain separate. Frozen profile/barcode/UMI/output invariants,
no BAM/spill/hash, and <=128 cores remain. Existing three-way 10k parity is the
baseline. A new whole count requires bounded synthetic/real heavy-tail evidence
supporting <2-h wall and <300-GB RAM; no 37x promise is made. New production
runs are capped at 300,000,000,000 bytes/no swap. Existing frozen artifacts and
500-GB historical receipts remain preserved. Sealed source/statistics show
167.66-s ready time, about 124 busy cores and >99.5% CPU user, 105.08M native
queries/95.43B occurrences, 5.684B raw-model visits, and 537.51B DP state cells
in per-model/behavior `std::map<StateKey,State>` traversal. Actual collector
visits are 431,860,084,428 (=1,995,995,637,646 raw logical minus
1,564,135,553,218 counter-only avoided): 428,975,326,656 memo hits plus
2,884,757,772 materializations, so 99.332% rediscover existing candidates after
alias/profile expansion. Whole-signature cache is 18.9% hit at its 512-MiB bound.
Near-ready cgroup memory was 73--76 GB versus 406.49-GB peak. Exact heap/CPU
percent attribution is unmeasured; those diagnostic observations alone selected
no remedy, policy, or implementation.

**D110 Stage 6 real-data validation checkpoint (2026-10-31):** After user
approval, alias/cache implementation is complete and parent-reviewed; Sol
accepts its bounded architecture/proof. No biological policy changes. Focused
native-query/resolver/cache 3/3 passed in 1.02 s; full 198/198 passed in 31.92 s
(`build/ctest-alias-cache-full.log`). Frozen artifacts are
`panCollapse-alias-cache`, `source-alias-cache.tar.gz`, and
`ctest-alias-cache-full.log`. Conservative keys (policy plus optional fixed
support ID) preserve malformed-source faults; cached per-sink E/P/B vectors
retain logical multiplicity. Shared immutable leaf singleflight uses 8+8-GiB
source bounds with active/current/high-water, resident/max-entry, build/hit/
evict/uncached diagnostics and checked charges/leases/cleanup.

Next authorized gate is one genuine-EOF same-`first10000.gamp` run under
300,000,000,000 bytes/no swap/128 CPUs, both profiles plus MEX, with exact
comparison to frozen Stage 5 outputs/scientific counters. The unchanged-source
1M-QNAME prefix is frozen at 2,502,433 records/465,121,668 bytes; it has not
run. No performance claim or whole launch is authorized until this gate and a
larger fixture support the <2-h/<300-GB objective.

**D110 GAMP-branch computation review checkpoint (2026-10-31):** The Stage 6
same-`first10000.gamp` gate is live in
`pancollapse-native-alias-cache-first10000-20261031`
(`310171b9a5f62a5d12d3f3cf2f7873a7795ff751bf3aa6492e24308d8ef654a8`), started
17:11:14.932 UTC under 300 GB/no swap/128 CPUs. Native ready was 164.257 s; at
600 s it was 9,997/10,000 groups, 8.8315B states, and 150,791,712,768-byte peak.
This is not a performance result. The mapper command's `-M10` caps records, not
represented GAMP DAG routes. Counting traverses existing alignments, does not
remap, and does not require full-reference-walk containment. The next authorized
work is read-only review of minimal exact computation over GAMP branches before
further container optimization. No new hard cap/top-K/score pruning, biology or
profile change, full launch, or unsupported speedup claim is authorized.

**D110 route-scoring clarification (2026-10-31):** User describes
wrong-way-splice filter plus per-alternative node-score transcript exon/body
tally and argmax. D091 explicitly agrees selection is max over (route,
transcript), never sum of mutually exclusive alternatives. Compact `RouteTally`
max-prefix DP differs from richer exact Ex50/support-state DP; the latter was
previously overstated as scientifically necessary. Frozen CR7/custom parity is
unchanged. No cap, pruning, or new counting mode is approved. Pending user
choice is separately named node-score argmax whole counts with frozen profiles
retained separately versus exact frozen CR7/custom parity. Current Stage 6
binary/gate remains unchanged.

Diagnostic `first10000.route-complexity.json` records 25k records/10k groups,
maximum 10 records/group, 340 records with >1000 structural routes, maximum
14,875,194,015,048,794,112 routes, and 44,989 subpaths; structural routes are
not distinct biological alignments. `inspect_gamp_routes.py` passed four toy
assertions. No production change or hashing occurred.

**D110 CountOutcome performance API checkpoint (2026-10-31):** Under existing
performance authority, Sol conditionally approves implementation of a typed
CountOutcome-only API (terminal, barcode-correction eligibility, UMI-filter
reachability, genes) consumed exclusively by prepared count/cache. Full
`resolve()` facts/reasons/oracles remain unchanged; prepared count currently
discards facts/reasons and runtime counters do not use reasons. Outcome DP may
merge same aligned/exonic state, exon margin, strand margin, while retaining
exact body-support sets, body anchor, last-node/end flags, splice/wrong flags,
alias, and policy. Higher score may dominate identical reduced keys only for
identical final-count outcomes across both frozen profiles. No score-window,
strand, tier, body, or nesting rule changes.

A conservative no-graph certificate sums all nonnegative `from_length` values
within `INT64_MAX` and bounds absolute subpath/connection scores within
`INT64_MAX-max_profile_window`; failure uses unchanged full resolve without
throw/new validation. Full API remains diagnostic oracle. New API exposes no
facts/reasons/coordinates: internal reason/order/work metrics may differ and are
not preserved claims, but the four consumed fields, final counters, and tables
must match. Sorted single-copy frontier storage is accepted under prior proof.
No hard cap/pruning/model union/containment change is authorized. Focused/full,
adversarial CountOutcome parity, and same-prefix EOF gate are required; no
speedup promise or full launch. Stage 6 frozen binary/source gate is unchanged
and immutable (~1380 s, 9,997 groups, 150,791,712,768-byte peak). This does not
resolve the optional separately named node-score argmax choice.

**D110 user-authorized node-score scoring replacement (2026-10-31):** User
supersedes the prior CountOutcome-only proposal for default native scoring.
Per candidate, correct library strand; veto known wrong-direction splice
alternatives; credit existing VG per-node score contributions once to transcript
exon/body memberships; never add complete GAMP alternatives; classify each
transcript/branch E=100%, P=50--<100%, B=<50% with gene-body support; then keep
best score/ties separately per class across routes/records for downstream
assignment and CB/UMI. D091 remains max over (route, transcript), not sum of
alternatives. No ordered reference-walk containment or per-route body-support-
subset enumeration is allowed. Default native scoring must identify
`node-score-v1`; frozen profile definitions are not edited in place, and legacy
full resolve remains frozen oracle rather than claimed scientific-count parity.
Whole-count goal is paused; no full launch/resume is authorized.

Known pitfalls requiring parent consequential review: official CR7 uses STAR
near-best/window and per-splice-segment >=50% semantics, while STARsolo Ex50
uses strict >50% and antisense E veto can outrank sense P/B. User-selected
whole-branch >=50% and early strand discard can differ. Do not silently import
CR7 extras or recalibrate windows. Three prefix maxima alone are unsound at
merges, so compact margin/best-score envelopes are needed. Evidence/version
identity remains to be designed; no implementation/test launch is authorized by
this record.

**D110 node-score implementation clarifications (2026-10-31):** Retain argmax
and inclusive within-one-additional-mismatch behavior: D068 +1/-4 means a
pool-local five-VG-point window, not literal one STAR point. Score both sense
and antisense; antisense exon evidence vetoes, superseding early sense discard.
Sol accepts a separate NodeScoreResolver with six independent E/P/B by
orientation pools, per-transcript maximum/ties, then inclusive five-point
window per pool only--never existing global cross-pool `score_window`.
Complete alignment routes are scored without exclusive-branch summing; P stays
whole-route >=50%. Per-splice-segment CR7 is requested expansion only, not an
approved replacement.

Sparse best-score envelope key is last oriented endpoint, exon margin,
all-exonic, body-seen, strand margin, valid-orientation mask, and aligned state;
support sets are excluded. Exact ties merge Parent provenance/support units from
retained winning aliases. Credit node scores once for actual mapped segments,
not reference-path multiplicity; keep raw placement score separate, including
connections only there. Preserve legacy full resolve and `node-score-v1` manifest
identity; new gene assignments claim no old parity. Complete potential-path node
sets are the unit, with prefixes only shared bookkeeping. No new production
fatal biological-validity checks are authorized: flat per-node scorer
certification is reported pitfall, not default read rejection. Whole goal stays
paused, while the authorized coding request may proceed after parent review.

**D110 raw alignment-alternative cutoff transition (2026-10-31):** User
overrides deduplication: count all complete alignment alternatives encoded in
GAMP, duplicates included and summed across records per read, as cheap first
filter. The concluded 10k dedup fixture (166/10,000 or 1.66%, zero unknown) is
retained evidence but not target metric. Prior raw structural-route 167/10,000
(1.67%) is the relevant prior exact-10k receipt. Native simple scanner
`inspect_gamp_alignment_counts.cpp` is requested to produce exact `cpp_int`
histogram percentiles and >1000 drop fraction without rescoring, node/gene/
assignment/index work. The full 1M Python route run remains deliberately stopped.

Stage 6 pause receipt persists; development is running only for this diagnostic.
Whole goal remains paused. No biological policy change, production cap/scorer
implementation, or count restart is authorized.

**D110 raw-alternative diagnostic result (2026-10-31):**
`total-alignment-diagnostic.validation.json` independently validates raw
complete routes per QNAME, duplicates included and without rescoring/dedup. Toy
real GAMP passes 7 records/6 groups histogram {0:1,1:1,2:3,4:1}. Exact 10k has
25k records and 167 >1000 (1.67%); first 1M has 2,502,433 records, 17,617 >1000
(1.7617%), and 19,787 zero. Nearest-rank P99/P99.9/P99.99 are
6912/59,719,688/334,430,208,000; literal low P1/P0.1/P0.01 are zero. Root's
independent Python histogram/percentile recomputation agrees. `cpp_int` avoids
overflow at maximum 15,303,008,032,705,444,785. The 1M scan took 60.2414 s;
333,632-KiB RSS is snapshot, not measured peak. No full 66.6M rescan or whole
count occurred.

For the user-requested cheapest filter, recommend a GAMP-only capped DAG route
count at 1001, summed across records per QNAME before graph lookup:
O(subpaths+arcs), with no node/edit/scoring/dedup work. This guard is not
implemented or production-authorized; whole goal remains paused. Node/set-dedup
helper evidence is historical and inactive.

**D110 raw-alignment cutoff implementation authorization (2026-10-31):** User
authorizes bounded implementation of tunable raw total-complete-GAMP-route cutoff,
default 64,000,000 (near first-1M P99.9 59,719,688). Metric is complete routes,
duplicates included and summed across records per QNAME. Strict greater-than
cutoff drops whole read before graph/annotation queries, without enumeration,
dedup, or rescoring. Proposed `--max-alignments-per-read N` defaults 64,000,000
and zero disables; parent will settle validation. Preserve global barcode prior
over all valid groups.

This authorizes implementation/tests only: no whole-count resumption, full
launch, or scientific profile change. Required coverage includes capped
arithmetic overflow; branches/connections/parallel edges/sources/multiple
records; equality N retained/N+1 dropped; rejected read makes no graph query;
barcode-prior retention; and CLI/default/manifest/counter behavior. Dirty
worktree remains preserved and unstaged.

**D110 terminal cutoff implementation continuation (2026-10-31):** The
authorized bounded implementation is complete. `gamp_alignment_complexity.hpp`
counts complete DAG routes in O(subpaths+edges) using saturating capped
arithmetic; same-QNAME records are summed and duplicates remain included. The
default is `--max-alignments-per-read 64000000`; strict greater-than drops and
explicit `0` disables. The guard executes after the valid barcode prior but
before graph lookup, caches, and assignment in prepared/native and legacy direct
count. Each retained complete alignment remains separately scored and classified;
shared piece/prefix work may be reused, but mutually exclusive alternatives are
never summed. A rejection is terminal `unassigned_high_complexity` with reason `1<<18`;
summary and manifest contain the parameter and dropped counters. `convert` is
unchanged.

Because the compatibility schema cannot encode a dropped read,
legacy compatibility-in/out requires explicit `0`; the schema was not expanded.
The simplified node-score scorer is not implemented. This continuation makes no
performance or scientific-parity claim and does not resume the paused whole
count or authorize production launch.

Build succeeded at `build/build-alignment-cutoff.log`; focused CTest passed 10/10
at `build/ctest-alignment-cutoff-focused.log`; and full CTest passed 200/200 in
31.14 s at `build/ctest-alignment-cutoff-full.log` (exit 0). The three
mixed-configuration parity fixes set both comparison arms to explicit `0`
(cutoff disabled) while retaining all semantic/output comparisons. The
first-1M non-random-prefix helper receipt
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/cutoff-guard-first1000000.validation.json`
reports 2,502,433 records/1,000,000 read groups; 982 >64,000,000 drops
(0.0982%), 17,617 >1,000 drops, and 57.3979 s wall. The exact histogram has
990,090 <=7,000 reads/35,251,126 alternatives and 8,928 reads at
7,001..64,000,000/21,981,204,583 alternatives, totaling 22,016,455,709 retained
alternatives. The 0.8928% tail therefore accounts for 99.839887% of retained
alternatives, not measured runtime. Earlier D110 pending cutoff wording is
superseded by this terminal implementation record while retained as history.

**D110 superseded scientific-phase checkpoint (2026-10-31):** The prior
current-binary legacy-resolver 10k authorization is superseded by the user's
explicit simplified per-alignment scorer requirement; no run launched.

**D110 checkpoint correction — simplified scorer required first (2026-10-31):**
The requested one bounded 10k count must use the enabled 64M cutoff and
simplified per-alignment scorer. The current tested binary has the cutoff only
and cannot fulfill the test. Implement and correctness-review that scorer first;
no old-resolver test is authorized. Then one count may use existing
`R/first10000.gamp` (25,000 records/10,000 QNAME groups/8,506,683 bytes),
unchanged native.pci/XG/GBWT/RI, `cr7-v1` plus `pansc-strict-v1`, 128 threads,
300,000,000,000-byte cap/no swap, and no BAM/spill/hash/remap/index build.
Fresh empty `R/cutoff-first10000-t128.gVysNL` may be reused after implementation.
Whole counts remain paused.

**D110 simplified scorer and bounded 10k launch (2026-10-31):** The separate
`NodeScoreResolver` now implements explicit-stack complete-alignment DFS,
read-local node facts and sparse push/pop prefix tallies, with no sequence
realignment, ordered reference walk or subset-state DP. It credits each mapped
piece once per raw exon/body model, classifies whole-route E/P/B by consumed
reference bases (deletions included), takes per-transcript/pool maxima retaining
only exact-tied alias payloads, and applies independent inclusive five-point
windows. Known splice direction is checked without endpoint containment;
rejected-only shadow evaluation retains barcode/UMI-stage eligibility but no
gene assignment. Six-pool exact ranking retains the explicit downstream custom
Gene fallback; CR7 has no such fallback. No frozen profile definition is edited.

`--assignment-scoring node-score-v1` selects this new engine and the manifest
records its identity and five-point window. Legacy remains the default/oracle
during validation. The final build passed and full CTest passed 201/201 in
31.43 s (`build/ctest-node-score-full.log`), including new-mode concurrency,
joint/separate profiles, cutoff and adversarial scorer checks. Sol's bounded
review found no remaining correctness blocker, not scientific-parity or
production approval.

The authorized test launched at `2026-10-31T18:47:25.587391629Z` in
`pancollapse-node-score-cutoff-first10000-20261031`, container
`b75cf2019b406bcf588daef45127c1be35edd0d391a9c80ce59c207e4b557b69`,
using `R/cutoff-first10000-t128.gVysNL/run.sh`. That root contains the copied
tested binary, CTest/build logs, `run-receipt.json`, count log and five-second
resource samples. Both requested scorer and 64M cutoff are explicit; resource
limits remain 128 threads/300 GB/no swap. Whole counts remain paused. Count EOF,
output validation, memory and actual read throughput are pending this gate.

**D110 authorized bounded structural diagnostic (2026-10-31):** The user
authorizes read-only inspection of existing first-10k and first-1M GAMP only,
not a production filter/rescue/argmax change. A standalone degree/route-count
group diagnostic will report outer-record histograms conditional on per-record
complexity >7,000, >1M, and >64M; nonempty remaining-record counts after
removal; and totals checked against 25,000 records/10,000 groups and 2,502,433
records/1,000,000 groups. No remap, hash, graph load, or full scan is allowed.
Validated raw-sum drops remain 14 (10k) and 982 (1M). Preserve the current 10k
container and frozen binary; 9,989/10,000 at 510.007 read seconds is not EOF.
Smaller surviving records do not prove winning-gene safety because a discarded
record can contain the best or competitive gene. Whole count remains paused;
parent review precedes execution.

**D110 authorized minimum-record-complexity rescan (2026-10-31):** The
per-read sum histogram cannot answer minimum record-route-count percentiles.
The user authorizes `R/inspect_gamp_record_complexity.cpp` to rescan existing
first-10k and first-1M GAMP only, reporting per-read minimum nonempty record
complexity (zero when all records are zero), literal minimum over all records to
expose zero-sibling effects, weighted nearest-rank percentiles on the same 1M
denominator, and zero/mixed-zero counts. Validate input records/groups and
known summed-drop counts (14/982). No graph/remap/hash/filter/production change
is authorized. Standalone compilation is confined to the existing dev image,
without shared Ninja, in isolated 1--2-core/2-GiB diagnostic containers; the
production 10k test remains untouched.

**D110 per-read percentile clarification (2026-10-31):** Latest user request is
percentiles of the total raw complete alignments per QNAME, summed across all
its GAMP records, not per-record percentiles. The existing first-1M histogram
already measures exactly this; reuse it without rescanning. Weighted
nearest-rank P50/P90/P95/P99/P99.5/P99.9/P99.99/P99.999/max are
3/24/67/6912/118672/59719688/334430208000/681661459881772/
15303008032705444785. No conditional-record scan result exists: preparation
failed before input open, and the user redirected before retry. The running
10k count is unchanged.

**D110 minimum-record-complexity diagnostic complete (2026-10-31):**
Standalone compilation reused known CMake link flags inside DEV (no Ninja); DEV
is stopped and the count producer is unchanged. Toy 9-record/7-group checks
passed: min-any {0:2,1:2,2:3}, min-nonempty {0:1,1:2,2:4}, zero-only 1, mixed 1.
Receipts `R/first10000.min-record-routes.json` and
`R/first1000000.min-record-routes.json` validate 25,000/10,000 and
2,502,433/1,000,000 records/groups plus raw-sum >64M/>1000 counts 14/167 and
982/17,617. The 1M scan took 57.1735 s in an isolated 2-CPU/2-GiB/no-swap
container. Both minimum histograms: zero-only 19,787, mixed 0; weighted
nearest-rank P50/P90/P95/P99/P99.5/P99.9/P99.99/P99.999/max =
1/4/8/64/160/9216/1990656/2388787200/149593924085760; strict-above counts =
447441/85651/42006/8659/4978/980/98/10/0. Min >64M is 39/1M. Of 982 raw-sum
failures, 943 have a <=64M nonempty record and 39 do not. Records >7k affect
9,447 reads (6,055 with ten records, 1,352 with 1--2; 8,332 with a <=7k
sibling); >64M records affect 960 (711 with ten), while 22 additional raw-sum
failures lack an individual >64M record. This supersedes the older failed-attempt
wording. Minimum record complexity does not establish a safe winning or
competing gene and authorizes no policy change.

**D110 authorized first-1M sibling-winner diagnostic plan (2026-10-31):**
Before execution, analyze only
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/first1000000.gamp`.
For reads whose least-complex GAMP record has 9,000--10,000 complete routes
inclusive, report the fraction whose argmax-scored alignment occurs in a
more-complex sibling record. Keep two results distinct: cheaply exact per-record
maximum VG alignment score via DAG DP, and transcript-aware `node-score-v1`
argmax. For each, report ties separately from complex-only winners; neither
proves a safe rescue or policy change. This is a bounded structural/scoring
diagnostic, not a production count. The current 10k node-score/cutoff gate stays
untouched and running; whole count remains paused.

**D110 first-1M sibling VG-score diagnostic complete (2026-10-31):**
`R/sibling-vg-score.validation.json` PASS validates the bounded global
source-to-sink VG-score diagnostic. The 9,000--10,000 minimum-record cohort has
43 reads/45 records: 42 single-record and one three-record read. The latter's
records are 9,140 -> 51, 121,440 -> 46, and 922,200 -> 60, yielding one
complex-only winner (1/43 = 2.32558%; 1/1 with a complex sibling), gap 9, and
zero ties. All 43 pass the 64M summed-route guard; inferred starts are zero.
The 1M scan took 57.925 s in a 2-CPU/2-GiB container. Annotation-aware
`node-score-v1` comparison was not implemented or launched and remains pending.
This diagnostic intentionally uses global source-to-sink score; vg's default
`optimal_alignment_score` is local subpath-to-subpath DP. Normal VG ranking can
use DAG DP without route enumeration, whereas current `node-score-v1` enumerates
routes. The existing 10k benchmark is untouched and whole count remains paused.

**D110 authorized exact sparse node-score max-DP implementation phase
(2026-10-31):** Reconciled on `fix/whole-genome-count-repair-20261028` at
`327b048`; existing dirty state is preserved. User authorizes replacing
`node-score-v1` complete-route DFS with sparse exact max-DP. Preserve every
E/P/B-by-strand pool and its inclusive five-point window, raw-model coherence,
directed-splice masks and last covered endpoint, projected alias/evidence
identity, body-support metadata and separate body-fallback score, then the same
assignment/CB/UMI/output behavior. No new biology, ordered containment, spill,
remap, or production launch is authorized. Existing exact counters/oracle and
live 10k gate remain historical/live verification, not altered by this
authorization. Required next evidence is bounded DP tests followed by the same
10k gate before any larger test; whole count remains paused.

**D110 pre-real-10k sparse-DP checkpoint (2026-10-31):** Final sparse DP is
439 lines with a frozen 354-line oracle. `NodeScoreExecution` test controls
cover automatic, force-sparse, and enumerate-oracle modes; exact keys retain
independent A/G maxima, same-TX operation grouping, support/alias restoration,
fixed native-body support skipper state/payload, and metrics for represented
routes, DP states/transitions, max frontier, groups, and offset conflicts. Sol
conditionally accepted the design without a blocking flaw. Expanded tests pass:
256 seeded small DAG full-facts/results three-mode comparisons, adverse cases,
and a >=2^32-route DP-only case with <1,000 states. Full CTest passed 202/202
in 31.05 s with `PYTHONPATH=/mnt/ssd/lalli/.local/lib/python3.11/site-packages`
(`build/ctest-node-score-dp-full.log`); the first full attempt's `umi_tools`
import environment failure required no production-code change.

Frozen binary/source archive, copied CTest receipt, and runner are ready at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-dp-first10000-t128.UxhZc8`.
The then-next authorized phase was pending launch, not live: same
`first10000.gamp` (25,000 records), both profiles, 64M cutoff, 128 threads,
300,000,000,000-byte/no-swap, no BAM/spill/hash, Parquet+MEX, independent
validation, and baseline table/logical/byte/scientific-counter comparison only
after `cutoff-first10000-t128.gVysNL` reaches EOF. That original remains at
9,999 groups after >3,000 s. Whole launch remains paused.

**D110 sparse-DP same-10k launch live (2026-10-31):**
`pancollapse-node-score-dp-first10000-20261031` launched at
`2026-10-31T19:43:41.682094185Z` (container
`6599e6ed92b8b5439b8d19b541e9f31e97263331eee3019e7cf22e89d1686211`) with
300,000,000,000-byte memory and memory+swap limits (no swap) and 128,000,000,000
NanoCPUs. DEV is stopped idle and the old baseline is untouched. EOF, independent
validation, and parity/comparison are not yet established; whole launch remains
paused.

**D110 sparse-DP same-10k terminal result (2026-10-31):** The run reached EOF:
25,000 records/10,000 groups, 14 cutoff drops, 13,904 count rows, and 13,995
molecules; `validation.json` passed. Native count exit was 0. Manifest
initialization/read/processing were 154.724650670/202.183747006/202.281111332 s;
shell wall was 377.946674 s including cleanup, or about 49.46 read groups/s on
the inclusive 10k denominator. Cgroup peak was 166,680,592,384 bytes with no
OOM; sampled RSS was 176,101,588,992 bytes. The wrapper exited 127 because its
post-count validator invoked bare `python3` not on PATH; outputs were already
published, frozen runner is unchanged, and the independent validator reran via
explicit `/mnt/ssd/lalli/.linuxbrew/opt/python@3.11/bin/python3.11` in a
separate 2-CPU/2-GiB/no-swap Docker container with exit 0. The original explicit
enumerator baseline remains live at 9,999 groups after >3,600 s, so table/logical/
byte/scientific-counter equality is pending. Full CTest remains 202/202 and DP
differential facts/results match the oracle on 256 seeded small DAGs plus adverse
fixtures. No CR7/legacy F1, two-hour-genome, or promotion claim follows. Pending
root review is read-only cross-TX local-behavior grouping for state/model tail;
it is not implemented. Whole count remains paused.

**D110 pre-benchmark cross-TX local-behavior quotient checkpoint (2026-10-31):**
Sol accepted an exact quotient now implemented in `src/node_score_resolver.cpp`,
with no CLI or policy-schema change. It groups by complete operation vector,
fixed support mode, and full sparse per-local-edge two-bit splice-mask vector—not
TX ID, aggregate masks, or digest-only keys. It collects unique reachable
last-endpoint pairs through empty/insertion pieces once and uses inverted edge
effects rather than a model-by-edge dense matrix; all raw TX/policy/support and
winners restore per alias unchanged. Force-sparse singleton and original
enumerator remain. The rationale is topological induction to equal frontiers for
identical transfers/masks; bridge-only mask differences must split. Focused/full
tests are still being added (identical-behavior reduction, bridge-mask split,
seeded mask differentials) and have not run. A fresh same-10k gate requires them
to pass. Frozen first-DP artifacts/output and the live old enumerator are
untouched. Closure/signature overhead can offset savings; no 187x claim or full
run is authorized. Whole genome remains stopped.

**D110 pre-real-10k final cross-TX quotient checkpoint (2026-10-31):** The
exact quotient in `src/node_score_resolver.cpp` is implemented and Sol final
review passed without correction. It preserves complete-operation/fixed-support/
full sparse per-local-edge two-bit-mask grouping, raw TX/policy/support/winner
restoration per alias, force-sparse singleton, and original enumerator. A pure
performance guard skips cross-edge closure when all piece deltas are empty,
preserving route/offset telemetry and outputs. Focused passed 1/1 in 0.20 s and
full passed 202/202 in 32.57 s (`build/ctest-node-score-behavior-full.log`),
including 64 cross-TX seeded cases atop 256 existing: absent-vs-mask3,
fixed-vs-variable support, bridge veto, exact different policy/support
restoration, and group reduction. Frozen binary/source/logs/runner/receipt are
at `/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-behavior-first10000-t128.gISKSl`.
Launch is pending, not live: same 10k, profiles, 64M cutoff, 128 threads, 300 GB
no swap, existing indexes, no BAM/spill/hash/remap/index build; independent
validation and automatic ten-file byte/logical/scientific-counter comparison
against completed first-DP output are required. The old enumerator remains live.
Comparator receipts are future `R/node-score-enumerator-independent-validation.json`
and `R/node-score-dp-vs-enumerator.parity.json`; neither is passing evidence.
Whole genome remains paused.

**D110 cross-TX quotient same-10k launch live (2026-10-31):**
`pancollapse-node-score-behavior-first10000-20261031` started at
`2026-10-31T20:00:51.074088999Z` (container
`416becaaf21aa5160bd02c0d48a54194513d1e382e05ca5c6f9a4c93c4563eae`) with
verified 300,000,000,000-byte memory and memory+swap limits and
128,000,000,000 NanoCPUs. DEV is stopped idle; first-DP fixed output remains
validated and the original enumerator remains live. EOF, validation, and parity
are not yet established; whole genome remains paused.

**D110 cross-TX quotient same-10k terminal gate (2026-10-31):** Container exit
was 0/no OOM at `2026-10-31T20:05:57.313310042Z` after EOF on 25,000 records/
10,000 groups: 14 drops, 13,904 count rows, and 13,995 molecules. Independent
reader passed; all ten outputs are byte-identical to completed first-DP output
with equal profile definitions, logical identities, and scientific counters
(cache work excluded). No F1, CR7/legacy, or production claim follows.
Init/read/processing/wall were 150.430731581/128.025707080/128.138611337/
303.977775 s; read rate 78.1093 groups/s. Sampled RSS/cgroup peak were
167,081,734,144/157,636,481,024 bytes, no OOM or spill. Model groups and DP
states were 158,583 and 1,258,500,451 versus first-DP 8,784,273 and
4,376,860,251 (55.39x/3.48x fewer); count phase was 1.58x faster, while ~150-s
initialization is unchanged. Raw-model transfer work remains 629,485,930, so a
two-hour genome target is unproven. Full 202/202 and 320 seeded small-DAG
differentials/goldens pass. The original enumerator remains live at 9,999 groups
after >4,530 s; the background comparator awaits EOF, not full-real oracle
parity. Next safe work is larger-fixture/performance evidence after its own
checkpoint; whole genome remains stopped.

**D110 documentation checkpoint — next performance phase (2026-10-31):** The
active objective remains same-GAMP whole-genome per-gene counts in under two
hours wall time and under 300 GB RAM. The cross-TX quotient terminal gate is
bounded exact-DAG behavior/performance evidence, not completion: its frozen
root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-behavior-first10000-t128.gISKSl/`,
whose `terminal-receipt.json`, `validation.json`, and `parity.json` record EOF
on 25,000 records/10,000 groups, 14 cutoff drops, validation PASS, and ten
files byte-identical to first-DP. Read/init/runner-wall were
128.025707080/150.430731581/303.977775 s; sampled RSS/cgroup peak were
167,081,734,144/157,636,481,024 bytes. The run used 158,583 model groups,
1,258,500,451 DP states, and 629,485,930 raw-model transfers. Relative to
first-DP it has 55.39x fewer model groups, 3.48x fewer DP states, and a 1.58x
faster count phase, but unchanged roughly 150-s initialization. These results
do not establish the two-hour/300-GB goal, legacy/CR7 truth parity, or a
biological change.

Accepted boundaries are exact node-score/D105/D107 semantics; frozen profiles,
barcode/UMI/output; the 64M raw-alternative cutoff; no spill, hash, remap, or
index expansion; and 128 cores with 300 GB/no swap. The whole count remains
stopped. The live old enumerator
`pancollapse-node-score-cutoff-first10000-20261031` and its parity watcher were
confirmed observationally live; neither is full-real oracle parity. Current
code is the exact DAG scorer plus cross-TX quotient. A parallel review will
settle, but has not approved, any exact semantics-preserving optimization of
raw-model/node-transfer construction and/or further exact DAG factorization.
The authorized next gate is focused/full tests followed by larger-fixture
performance evidence; no production or whole-count launch is authorized.

**D110 pre-O3-benchmark authority checkpoint (2026-10-31):** Sol approved the
bounded implementation mechanism only: immutable edge-only `NativeQueryMode`
(no lazy node scoring), fixed-support specialization, and early read-local
node-membership/splice-mask quotienting before piece-by-raw-model expansion.
Mixed-support cases must remain singletons, and raw aliases must restore before
downstream processing. No policy, cutoff, profile, output, or biological
semantics changes. The native edge-only implementation and early quotient are
in progress and are not represented by the O3 snapshot or authorized for this
benchmark.

The user authorized an O3-only count-target CMake change if it helps: final
count-target flags are O3 without fast-math; Debug is unchanged. The frozen
O3-only binary/source/log are at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-o3-first10000-t128.A5MNAe/`;
full CTest passed 202/202 in 31.58 s. A same-10k O3-versus-O2 behavior A/B is
authorized with the existing fixture, profiles, 300-GB/no-swap cap, 128 threads,
and indexes. It remains unlaunched and requires independent reading plus ten-file
byte/logical/scientific-counter comparison to the O2 behavior result. Retain O3
only for measured improvement; neither a two-hour claim nor a whole-count launch
is authorized.

**D110 pre-structural-10k checkpoint and O3 terminal result (2026-10-31):** The
O3-only control at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-o3-first10000-t128.A5MNAe/`
terminally passed: independent reading passed and all ten files are byte-identical
to O2. Its terminal/timing/CPU/parity receipts report 129.682-s read time and
2720.94-s CPU versus O2's 128.026 s and 2758.87 s. That is no clear improvement,
so O2 is restored as the default CMake configuration while O3 artifacts remain
preserved.

The tested but unlaunched structural candidate is frozen at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-early-membership-first10000-t128.JwNFph/`.
Using the same O3 compiler flags as the control, it passed full CTest 202/202 in
31.41 s and focused 3/3, including 320 seeded cases plus mixed/multi-record,
second-quotient, and cache-lifetime fixtures; Sol final review found no critical
bug. It forms full read-local node-incidence/splice-mask classes before Delta/Op,
omits fixed-support vectors, keeps mixed support singleton, restores raw aliases,
and retains the second Op quotient. Node scoring is eager; edge-only
`NativeQueryMode` skips unused edge postings while legacy `lazy_edge` remains
exact and lease-safe. Metrics report actual model visits versus raw-model
transfers, membership groups, and edge-model builds.

The authorized next gate is the unchanged same-10k fixture (128 threads,
300 GB/no swap, existing indexes/profiles, 64M cutoff, no hash/spill/remap),
then independent reading and ten-file byte/logical/scientific-counter comparison
to the O3 control. It has not launched. Whole-goal success and larger-fixture
evidence remain pending. A fixed/none vector-frontier score-storage experiment
(exact sort+max; mixed generic) is prospective only: approved for later
investigation, not implemented and not a speed claim.

**D110 early-membership terminal benchmark; CompactScoreFrontier pending
(2026-10-31):** The structural benchmark at
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-early-membership-first10000-t128.JwNFph/`
exited 0/no OOM at `2026-10-31T20:39:52.465535056Z`. Validation passed 10,000
groups with 13,904 count and 13,995 molecule rows. Its parity receipt reports
all ten files, scientific counters, and logical definitions byte-equal to O3.
Init/read/processing were 146.854756294/78.422407757/78.486712665 s; runner
wall was 237.103126 s, CPU 1589.931948 s, cgroup peak 132,202,778,624 bytes, and
sampled RSS maximum 132,463,898,624 bytes. Metrics record 29,791,873 model visits
against 629,606,801 raw-model transfers, zero edge-model builds, and
1,258,506,306 DP states. Compared with O3's 129.68213154-s read and
2720.935242-s CPU, this is 1.65x/1.71x on the 10k fixture only, not proof of the
whole two-hour/300-GB objective.

`CompactScoreFrontier` is prelaunch-gate ready: fixed/zero support vectors use
exact sort plus componentwise maxima for independent annotation/Gene objectives,
preserving full `StateKey` and generic step; mixed support remains map-backed,
with `force_map` test control. The initial new-test build failed only for a
mixed-type initializer and that historical receipt remains retained. The
corrected retry build exited 0; focused CTest passed 4/4 in 1.36 s
(`build/ctest-node-score-compact-frontier-focused.log`) and full CTest passed
203/203 in 32.18 s (`build/ctest-node-score-compact-frontier-full.log`). The
O2 binary, `source-vector.tar.gz`, and both logs are frozen at
`R/node-score-vector-first10000-t128.jfIkiW/`; `build.ninja:655-659` verifies
default O2 flags. E now has `terminal-receipt.json`.

The next gate remains unlaunched: same 10k at 128 threads/300 GB no swap, both
profiles, 64M cutoff, unchanged inputs/indexes/MEX, no spill/hash/remap, then
independent reading and ten-file byte/logical/scientific-counter parity against
E and the O2 control. No profile, biology, or approximate-pruning change is
authorized. Require 1M performance evidence afterward; whole-count authority
still depends on supported fit/throughput. Reverse-DAG suffix-bound balancing is
unimplemented: Sol's proof is conditional and a final bound alone cannot retain
the checked-overflow contract, so it is deferred pending measured need.

**D110 vector same-10k gate live (2026-10-31):** This supersedes only the
preceding unlaunched status. `pancollapse-node-score-vector-first10000-20261031`
(`59fe443bc40227e93585062d59b99db984c9453c847481dbed1eea4a8dd3dc19`) started
at `2026-10-31T20:41:58.387715404Z` from
`R/node-score-vector-first10000-t128.jfIkiW/`. `launch-receipt.json` records
300,000,000,000-byte memory and memory+swap limits, 128,000,000,000 NanoCPUs,
and no network. `count.log` has begun four concurrent load branches only; no
ready/count/parity result exists. No whole launch is authorized. Next is EOF,
reader/parity/resource/performance validation, then a larger 1M gate only if
appropriate; the <2-hour/<300-GB whole goal remains unmet.

**D110 vector 10k terminal; prepared 1M profiled gate (2026-10-31):** This
supersedes only the preceding live status. The vector 10k run exited 0/no OOM at
`2026-10-31T20:45:29.460668780Z`; reader validation passed,
`terminal-receipt.json` is persisted, and `parity-early-membership.json` plus
`parity-o2-control.json` report all ten files, scientific counters, and
definitions byte-equal. Init/read/processing were
148.532392104/48.117434958/48.181194519 s; CPU was 1515.247936 s, cgroup peak
132,201,652,224 bytes, and sampled RSS 132,021,186,560 bytes. Read wall is about
1.63x faster than E but CPU only 4.7% lower, not a two-hour full-input proof.

The prepared root is `R/node-score-vector-first1000000-t128.TzO6UH/`, with the
frozen binary and source referenced relative to the 10k source archive, run
receipt, and bash-syntax-checked runner for
the existing 465,121,668-byte first-1M GAMP (2,502,433 records/1,000,000 groups;
expected 982 cutoff drops). It retains both profiles, 64M cutoff, 128 threads,
300 GB/no swap, MEX/indexes, and no hash/spool/remap. Sol approved external
gprofng 2.45 clock profiling across all target worker threads only: 10-ms
sampling, 10,800-s recording cap, and no I/O/heap/sync/per-read tracing.
Collector/dispatcher source confirms that the
cap closes profiling rather than counting. Correct relocation environment and a
collector/symbolized-display smoke receipt are recorded; PID identity is checked
before RSS sampling and all processes are cgroup-bounded. Profiled timing is not
a clean benchmark. After checkpoint review, launch/monitor the 1M gate and
independently validate counts plus profile header/sample warnings/symbols. No
full launch is authorized until feasibility is supported.

**D110 vector 1M profiled gate live (2026-10-31):** This supersedes only the
prepared unlaunched status. `pancollapse-node-score-vector-first1000000-20261031`
(`9e3398436ff6317580c8c155170cd7cbcf4fed74771b98cb06c61fbec7188556`) started
at `2026-10-31T20:47:43.684753337Z` from
`R/node-score-vector-first1000000-t128.TzO6UH/`. Inspection confirms
300,000,000,000-byte memory and memory+swap limits, 128,000,000,000 NanoCPUs,
and no network; `launch-receipt.json` records them. Native PID 7 command and
executable were checked; collector profile/log XML exist and four load branches
have started, but no counts are ready. Next is 1M EOF, profile attribution,
output/MEX independent validation, and resource/performance evidence. No profile
or biological change and no whole launch are authorized; the 66.6M <2-hour/
<300-GB goal remains unmet.

**D110 pre-exact-cache/index-validation implementation checkpoint (2026-10-31):**
The frozen live 1M run is unaffected and at 100,000 groups reached 195.179 s
(about 512 groups/s). Its 8-GiB node cache records 1,100,159 builds/985,911
evictions/367,691 hits; its 8-GiB edge cache 4,325,307/2,750,543/1,944,385;
active leases are 2.26 GB. Open-profile snapshots
`R/early-cpu-functions.txt` and `R/early-cpu-calltree.txt` total 2835.1 sampled
CPU-s, with self time `sdsl::select` 18.77%, `FastLocate::locateNext` 9.85%,
`Index::path_row` 8.25%, and `Index::model_ids` 8.01%. These include startup
and are partial, not final or phase-additive attribution. `profiling-notes.json`
records an expected SIGPROF mask warning and no initial errors; closed-profile
quality remains pending. The golden 10k MEX validator passed; 1M MEX validation
waits for EOF.

Sol approved two exact, test-gated mechanisms, with no implementation started:
(A) a 512-MiB shared immutable canonical physical-pair `SpliceEvidence` cache,
including negative/same-node cases, leases/charges, unchanged directional owners,
and lock order splice -> node/edge -> index (the existing bounded reference
cache does hold its shard factory lock); (B) one atomic byte per actual
`Impl.path_count` row with separate path-valid/model-refs-valid bits, acquire
load/release fetch-or only after complete successful checks, while concurrent
first checks may duplicate. Bounds/provenance/span checks remain per call; no
scan, hash, format/profiling-policy, or budget increase is authorized. Required
evidence is malformed/retry/concurrency/lease/eviction coverage plus existing
320-oracle/full-203 tests, then exact 10k parity. Flat builder vectors, DP
saturation, larger caches, and reverse-DAG work remain deferred. The whole goal
is still unmet and no whole launch is authorized.

**D110 exact-cache/index-validation prelaunch readiness (2026-10-31):** Both
approved mechanisms are implemented; Sol's final read-only review found no
blocker subject to the existing gates. Combined build exited 0; focused CTest
passed 5/5 in 1.23 s (`build/ctest-static-splice-validation-focused.log`) and
full passed 203/203 in 33.11 s
(`build/ctest-static-splice-validation-full.log`). Coverage includes concurrent
validation, malformed/retry/late-bad-model-ref and foreign/move/zero index cases,
plus canonical negative/same-node splice cache, singleflight leased spans,
failed retry/no partial publish, reset/direction, and tiny generic-cache
eviction. No source budget API was added.

Frozen binary/source/logs and runner are at
`R/node-score-static-cache-first10000-t128.pLa3n0/`; `run-native-count-gate.sh`
passed `bash -n`. The same 10k correctness/resource gate is ready but unlaunched:
128 threads, 300 GB/no swap, both profiles, 64M cutoff, existing indexes, no
hash/spill/remap, independent reader and new MEX validation, plus ten-file parity
against vector 10k and O2. The live 1M profiled control remains unmodified at
320k groups/609.241 s (about 525/s), peak about 132 GB. Concurrent-gate results
will not be a clean speed comparison; atomic-byte allocation/RSS must be
measured. Node/edge warm-cache caps remain 8 GiB. No biological/profile/API
change or whole-launch authority follows.

**D110 static-cache same-10k gate live (2026-10-31):** This supersedes only the
preceding unlaunched status. `pancollapse-node-score-static-cache-first10000-20261031`
(`897ead8beab5428f67741f67a67f0f886a3c1375a5f3d94a4493a25bc3389cf3`) started
at `2026-10-31T21:02:24.658520064Z` from
`R/node-score-static-cache-first10000-t128.pLa3n0/`. Inspection confirms
300,000,000,000-byte memory and memory+swap limits, 128,000,000,000 NanoCPUs,
and no network; `launch-receipt.json` records them. Four load branches started.
The frozen 1M control remains live at 390k groups/736.37 s (about 530/s), about
132-GB peak, and no OOM. Next evidence is 10k EOF, reader/MEX validation and
two ten-file parities, plus 1M EOF, closed-profile quality, and format validation.
No full count is authorized.

A future node-64-GiB/edge-16-GiB/assignment-8-GiB configuration under the same
300-GB cap is read-only feasibility review with Sol only—not implemented or
authorized as default. Current node/edge 8-GiB and assignment 512-MiB caps are
unchanged. No scientific or policy change; the <2-hour/<300-GB goal is unmet.

**D110 static-cache 10k terminal and next exact phase checkpoint (2026-10-31):**
`node-score-static-cache-first10000-t128.pLa3n0` exited 0/no OOM at
`2026-10-31T21:06:16.673302741Z`. It completed 25,000 records/10,000 groups
with 14 drops, 13,904 counts, and 13,995 molecules; independent counts/MEX
validation and both ten-file byte parities versus vector `jfIkiW` and O2
`gISKSl` passed. Receipt: `terminal-receipt.json`. Manifest init/read/processing
were 156.313917758/58.028528258/58.082092723 s; CPU usage was 2,151,175,835 us
and cgroup peak 132,392,271,872 bytes, no spill. Concurrent frozen vector 1M
`node-score-vector-first1000000-t128.TzO6UH` remains live (582,995 at 1110 s), so
this is not a clean speed comparison. Full tests passed 203/203. The accepted
next exact technical phase is node-score-only NodeEvidence slimming: retain exact
metadata score_count, postings, and sorted-unique body transcript IDs; discard
raw hits/scoring vectors; reconstruct node_scoring lazily only on explicit
request; leave full mode unchanged. Sol conditionally requires metadata/
multiplicity/span/lifetime/concurrency/retry tests; production node-score never
requests node_scoring. Diagnostic query/cache counts may change, science may not.
Experimental 64/16/4/0.5-GiB node/edge/assignment/splice caps are feasibility
only under the same 300-GB hard cap, not defaults or proven fit; retain the
655,360-entry assignment cap. This supersedes the preceding 8-GiB-assignment
review only for the experimental candidate. Freeze a candidate binary after slim
tests, restore normal source defaults, then run one isolated 1M parity/resource
gate after M EOF. No extra mandatory 10k pilot, full launch, or <2h/<300GB claim
is authorized.

**D110 prelaunch 1M slim-wide checkpoint (2026-10-31):** Frozen, unlaunched
candidate root is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-slim-wide-first1000000-t128.GwhDPE`
with `panCollapse`, `source-slim-wide.tar.gz`, `run-receipt.json`, `run.sh`, and
`ctest-slim-node-evidence-{focused,full}.log`; new scorer sources are
`src/node_score_resolver.{cpp,hpp}` and `src/node_score_oracle.cpp`. Normal slim
tests passed focused 5/5 (1.22 s) and full 203/203 (32.14 s); experimental caps
passed focused 5/5 and full 203/203 (32.87 s). Normal defaults were restored and
rebuilt at node/edge/assignment 8/8/0.5 GiB. The frozen candidate alone uses
64/16/4/0.5 GiB node/edge/assignment/splice caches, 655,360 assignment entries,
300 GB/no swap/128 cores. Slimming retains exact `score_count`, postings, and
sorted-unique body IDs; it reconstructs `node_scoring` lazily only on explicit
request, which production node-score never makes. Unique body capacity and the
original interleaved overflow-check order are preserved.

Sol found no blocker (including the stated failure-test limitation) and accepts
one isolated 1M gate, not another 10k pilot. It cannot launch yet: control
`node-score-vector-first1000000-t128.TzO6UH` is live at 870k/1645.92 s, no OOM,
about 132-GB cgroup/137-GB RSS; its native EOF, independent MEX/record
(2,502,433)/982-drop validation, and closed-profile quality are pending. On
terminal pass, launch one isolated candidate gate, record resources, and require
all-ten-file/scientific parity. The old enumerator 10k control is not whole
production. No 66,601,887-group count, <2h/<300GB proof, or whole launch exists.

**D110 vector 1M terminal and slim-wide launch readiness (2026-10-31):**
`node-score-vector-first1000000-t128.TzO6UH` exited 0/no OOM at
`2026-10-31T21:23:02.08623378Z`: native EOF 2,502,433 records/1,000,000 groups,
982 drops, 982,842 count rows, and 1,302,814 molecules. Both validation and MEX
validation passed (29,788 barcodes/20,621 features; CR7/custom NNZ
480,803/502,039). Init/read/processing/finalize/native-wall were
149.469227426/1918.133442001/1920.488174038/2.354732067/2090.459025 s; CPU use
131,680,755,620 us, cgroup peak 133,257,510,912 bytes, sampled RSS max
141,840,748,544 bytes across 418 samples, all OOM/max counters zero, no spill.
`terminal-receipt.json` records the result. Profile header reports no errors and
only the previously explained SIGPROF-mask warning; no timer/dispatcher/lost-
packet/unwind warnings. Exclusive functions prioritize native reference/cache
work (select 18.96%, locateNext 9.74%, path_row 8.54%, model_ids 8.32%), not a
clean unprofiled speed, F1, or full-goal claim.

Prerequisites now pass to launch exactly one isolated frozen slim-wide 1M gate
at `node-score-slim-wide-first1000000-t128.GwhDPE`, under 300 GB/no swap/128
cores, then monitor EOF, byte/scientific parity, and resources. Root must verify
actual container limits and record launch receipt. This does not authorize a
whole count; the 66,601,887-group <2h/<300GB goal remains unmet.

**D110 slim-wide isolated 1M gate live (2026-10-31):** Root launched
`pancollapse-node-score-slim-wide-first1000000-20261031` at
`2026-10-31T21:24:33.681372597Z` (container
`a81d3879ea37f0ffaf575b09e5983beece73a3e472008fb1cfc2dccd15d5ecfe`). Its
`launch-receipt.json` records actual 300,000,000,000-byte memory and
memory+swap limits, 128,000,000,000 NanoCPUs, network none, and exact image ID;
Docker verified native PID 8/host 3320043 command and executable. Four parallel
reference branches started; index/XG loaded in 5.04916/6.91632 s and GBWT/RI was
pending at last inspection. No EOF, counts, parity, or resource gate is yet
established; whole count is not launched. Main next work is monitoring C
throughput/cache churn/EOF/exact 1M parity under 300 GB. A separate EdgeEvidence
slimming proposal is read-only and unaccepted: production could retain only
canonical transcript exon-direction/body presences while a legacy getter lazily
requeries raw Parent hits. C is unchanged; no full-speed or F1 claim follows.

**D110 pre-implementation EdgeEvidence checkpoint (2026-10-31):** C remains
live and unchanged (92,823 groups/150.002 s, about 619/s). Partial cold
observations are node cache 24,145,165,576 B/812,909 builds/0 evictions; edge
cache cap 17,179,869,184 B/resident 17,161,571,192 B/3,672,024 builds/10,476
hits/551,738 evictions; splice 115,985,252 B/921,362 builds/536,920 hits/0
evictions; sampled RSS 135,713,783,808 B. They are not whole-feasibility
evidence; eviction does not prove repeated FastLocate because splice caching
bypasses most leaf-query reuse, though raw edge Parent IDs create capacity
pressure.

Sol conditionally approves, and root accepts, exact node-score-only EdgeEvidence
slimming before a second-level raw cache. Retain sorted-unique
`{canonical_transcript,uint32_flags}` with exon-forward, exon-reverse, and body
presence; splice/junction must reduce exactly those bits. Full mode keeps raw
hits/eager postings. Legacy node-score `Source::edge` first uses canonical
evidence, then requeries raw hits outside the cache lock, builds identical
checked model postings locally, and leases/publishes. Query diagnostics may
change only for the explicit getter; production does not call it. Required tests
cover full/slim order/flags, duplicate Parents, both directions, body veto,
negative/all orientations, lifetime/reset/concurrency, and failed-model-ref
retries. No index format, biology, score, or worker budget changes. Only
`native_count_reference.cpp/.hpp` and `native_count_query_test` are authorized;
root alone runs Ninja. No extra 10k pilot; a later frozen 1M candidate requires
exact C completion and new 203-test success plus a fresh checkpoint. Whole count
and <2h/<300GB feasibility remain absent.

**D110 node-compression/locality reconciliation (2026-10-31):** Fresh Docker
poll finds C and the old enumerator 10k control both still running with verified
300,000,000,000-byte memory/memory+swap and 128,000,000,000 NanoCPU limits; no
terminal claim is made. C's latest count milestone is 790,000 groups at 985.452
read seconds. Latest C resource sample is 199,106,256,896-byte RSS,
189,827,350,528-byte cgroup peak; no OOM is observed. C remains immutable and
EOF/parity pending.

The root-resolved worker helper is low-level POD codec only; prepared wrappers,
file validation, 31-bit limit/errors, and level-3 writer behavior remain unchanged.
The NodeCompression authorization stands. Sol separately approves only a later
read-only locality experiment: opt-in, no position sort/default, anchored at
32,768 complete QNAME groups/256 MiB window and globally capped at 65,536/512 MiB
including workers; no code is authorized. Current per-UMI counts are integer
aggregates by (profile, CB, gene, UMI) after per-QNAME assignment. Same-CB/UMI
reads cannot be discarded absent gene evidence/support; the exact graph-signature
cache is already safe. Whole count and <2h/<300GB feasibility remain absent.

**D110 C live/provenance checkpoint (2026-10-31):** One fresh poll finds C still
running (no exit/EOF/validator/parity receipt). Latest sampled RSS/cgroup peak
are 200,757,465,088/191,563,079,680 bytes; no OOM is observed. Do not infer a
terminal result from the near-EOF milestone. NodeCompression native code and
4,096-fanout tests are written, but TerraMax repairs, robust-test work, and a
4-MiB encoder trim remain pre-build; no new build started. Continue only the
approved same-fixture optimization path. Provenance caveat: tracked same-GAMP
path attempt 5/full directory records Oct-27 stat mtime and
`process_receipt.completed_utc=2026-10-27T17:35:27`, despite later 10/30 goal
wording; it must not substitute for this input, and root will flag the mismatch
before any whole launch. This is frozen node-score baseline evidence only, not a
biological or Cell Ranger parity claim.

**D110 C slim-wide 1M terminal gate (2026-10-31):** Docker exited 0/no OOM at
`2026-10-31T21:49:11.934653515Z`. Native EOF completed 2,502,433 records/
1,000,000 groups with 982 drops, 982,842 counts, and 1,302,814 molecules.
Independent count and MEX validations passed; `parity.json` independently
confirms all ten output files byte-identical with equal scientific counters.
Manifest init/read/processing/finalize were 154.522569225/1264.504138590/
1266.863654962/2.359516433 s. `resources.tsv`/`cpu.stat` give cgroup peak
192,385,966,080 bytes and CPU use 82,515,471,681 us. Assignment cache was
655,360 entries, 286,517 hits, 712,501 misses, 57,141 uncached, and
2,875,590,906 charged bytes. `terminal-receipt.json` records this frozen
node-score baseline result only—not Cell Ranger parity, F1, or whole feasibility.
NodeCompression helper/robust tests are still pre-build. Sol accepts raw typed
fallback for small/no improvement and ZSTD only for worker decode; the 4-MiB
encoder trim remains. A new candidate requires those tests and a fresh checkpoint.
Preserve the 10/30-goal versus Oct-27-recorded-input provenance caveat.

**D110 prelaunch D node-compression 1M checkpoint (2026-10-31):** Frozen,
unlaunched D is
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/node-score-compressed-first1000000-t128.zvlqnP`,
containing immutable `panCollapse` (120,690,672 B), `source-compressed-wide.tar.gz`
(575,880 B), `run.sh`, `run-receipt.json`, and
`ctest-{normal,wide}-{focused,full}.log`. Sol approved implementation/storage/
concurrency including typed raw fallback. Normal focused/full passed 10/10 and
204/204 in 31.89 s; same-C wide focused/full passed 10/10 and 204/204 in 31.38 s.
Source defaults are restored to node/edge/assignment 8/8/0.5 GiB; root's
restored-default build/focused check is still running and must pass before launch.

D alone uses C's 64/16/4/0.5-GiB node/edge/assignment/splice caps, 655,360
assignment entries, 128 threads, 300 GB/no swap, network none, both profiles,
`node-score-v1`, and 64M cutoff. It adds canonical edge summary and lossless
node POD ZSTD-1/raw fallback with once-per-worker/group decode, strong publish,
and 4-MiB encoder trim; full legacy getters and prepared level-3 byte compatibility
remain unchanged. No biology, per-alignment scoring, CB/UMI, policy, spill,
spool, or input-hash change exists. Its script independently validates counts/MEX
and all-ten-file/scientific parity against C and profiled M. After root reviews
the restored build, exactly one isolated D 1M gate is authorized—then limits,
native PID, EOF, parity, and resources must be recorded. The future 8M-entry
assignment-cap experiment and locality opt-in are not in D. Preserve the same
Oct-27-recorded-input versus 10/30-goal caveat; whole count and <2h/<300GB proof
remain absent.

**D110 pre-implementation node-posting compression checkpoint (2026-10-31):** C
is live and immutable at 550,000 groups/705.927 s (about 779/s): node 64-GiB
resident 68,698,509,904 B/3.524M builds/1,229,290 evictions; edge 16-GiB
11.925M evictions; splice 473.6 MB/0 evictions; sampled RSS 194,051,293,184 B,
no OOM. These are partial observations, not whole feasibility; C EOF/parity is
pending. EdgeSlim is implemented only in the worktree
`src/native_count_reference.cpp` and `tests/vg/native_count_query_test.cpp`;
after comparator/corrupt-copy fixture repair before load, focused 5/5 (1.27 s)
and full 203/203 (32.27 s) passed (`build/ctest-edge-summary-{focused,full}.log`)
and Sol found no blocker. Full and node-score preserve science; canonical edges
retain only E-forward/E-reverse/body presence and the legacy getter exactly
reconstructs checked raw postings. It is not in C.

Accepted next technical phase: lossless in-memory raw-or-ZSTD-level-1 numeric
posting payload in node-score, retaining uncompressed NodeRecord count/body IDs;
decode worker-local once per node/group outside cache lock and publish only after
exact count/size/sorted-field validation plus lease accounting; reset clears it.
Full mode remains eager; no model-ID/gene/Parent collapse, 2-GiB restriction,
format/CLI change, or cap increase. Reuse `decode_rows` in `count_reference.cpp`
and raw/ZSTD selection in `count_reference_builder.cpp`; retain wrapper, 31-bit/
raw-payload flags, and prepared writer level-3 exactly; native uses level 1 and
fresh selected payload bytes avoid compressBound capacity. Authorized worker
files are `src/pod_row_codec.hpp`, `src/count_reference.cpp`,
`src/count_reference_builder.cpp`, `src/native_count_reference.cpp`,
`tests/vg/native_count_query_test.cpp`, `tests/vg/pod_row_codec_test.cpp`, and
`tests/vg/CMakeLists.txt`; root alone runs Ninja. Required tests cover raw/
compressed/empty/size/truncation/corruption/high IDs/sorted posts/getter reuse/
reset decode without FastLocate/lifetime/concurrency/rollback. Node compression
ratio/CPU benefit is unmeasured. Producer emission is read-group, not position,
ordered; locality batching is read-only architecture evaluation only. Future
candidate launch still requires C terminal exact parity, new full tests, and a
fresh checkpoint. Whole count and <2h/<300GB proof remain absent.

**D110 D launch and old enumerator terminal reconciliation (2026-10-31):** Root
launched D at `2026-10-31T21:55:51.499119795Z` as
`pancollapse-node-score-compressed-first1000000-20261031` (container
`1f3e71b622a195c0750430abcb42ce62cc4eee9f10c8273a57a95c0ce24a4073`).
`launch-receipt.json` records actual 300,000,000,000-byte memory/memory+swap
(no swap), 128 CPUs (128,000,000,000 NanoCPUs), network none, image ID, and verified native PID 8/host
3461347 command/executable. Restored-default build session 55946 exited 0 and
focused passed 10/10 in 1.35 s (`ctest-default-restored-focused.log`); branch
head remains `327b048` with normal 8/8/0.5-GiB defaults. Index/XG took
5.0188/7.21022 s; GBWT/RI was pending at last poll. D has no EOF claim.

Old B is terminal, not live: Docker exited 127/no OOM at
`2026-10-31T21:45:50.925503247Z` because frozen wrapper post-validation used
bare Python, while `timing.tsv` records native count exit 0 after 25,000
records/10,000 groups, 14 drops, 13,904 counts, and 13,995 molecules. Existing
watcher exited 0; explicit-Python B count/MEX validations and
enumerator-versus-DP `node-score-dp-vs-enumerator.parity.json` passed all ten
files/scientific counters. B `terminal-receipt.json` records the native/wrapper
distinction and 132,202,708,992-byte cgroup peak. This is 10k output parity only,
not F1 or CR7/Cell Ranger parity. D monitoring remains the safe next task; whole
66.6M <2h/<300GB goal remains unmet.

**D110 pre-implementation per-key singleflight checkpoint (2026-10-31):** Frozen
D remains live after about 90k groups/148.093 read seconds, no OOM; its EOF/parity
is still required before any future candidate. Read-only receipts
`cache-wait-diagnostic-receipt.json`, `thread-wait-snapshot-{1,2}.tsv`,
`cache-lock-layout-check.tsv`, and `cache-wait-attribution.json` attribute 86/134
and 67/134 advancing-read-phase threads to exact node/edge/splice shard mutex
words (six other futexes each). Object base was read only for the three known
cache budgets (24 bytes); all waits are offsets 8/12 in actual shard mutexes.
Observer was network-none, PID-namespace/SYSPTRACE, with no attach, signal,
pause, profiler, or large hash. Current `get_or_build` holds shard exclusivity
through FastLocate/nested splice/node/edge builds; this satisfies Sol's acceptance
condition, but speedup and unrelated-versus-same-key fraction are unmeasured.

Approved scope is only `src/bounded_reference_cache.hpp` and
`tests/vg/bounded_reference_cache_test.cpp`: brief shard-lock flight install/join;
owner build and waiter waits outside all shard locks; atomic ready/FIFO/accounting
publication under shard lock before wake; one immutable same-key builder; error
fanout/retry; and oversized one-flight uncached behavior. Preserve the three-cache
acyclic dependency, capacity/hash/biology/scoring/profile/CB/UMI/locality, and
cumulative stats; define an explicit post-join production contract. Required
tests are same-shard different keys, same key, shared exception/clean retry,
oversized joins, and existing FIFO/lease/byte bounds. Root alone runs Ninja;
implementation has not started. The future 8M-entry cap experiment is separate
and must not combine here. Whole goal remains unmet.

**D110 ongoing per-key phase update (2026-10-31):** Root reviewed the checkpoint
and assigned Sol only header/focused-test implementation; no code result is
claimed. Accepted `hits` semantics are diagnostic-only: increment for ready
returns and in-flight joins at join time, including failed flights, with no
scientific-counter or new public-counter change. Deterministic tests require N-1
wait hits before owner release plus bounded timeout cleanup. D remains immutable/
live at 250,000 groups/350.581 s, sampled RSS 126,236,172,288 B, no OOM. Matched
220k evidence: C/D read 313.674/313.449 s, node 49,247,789,088/13,335,578,885 B,
edge 17,157,766,328/5,455,473,800 B, and both about 1.636M node builds. C/D edge
evictions are 4,305,615/0 but both edge queries about 7.45M: memory savings only,
not speed or repeated-query proof. D parity remains pending. Native init was
already complete after loading; no extra hash. The future 8M entry-cap experiment
remains separate after D/E, neither implemented. Whole goal remains unmet.

**D110 background sort and UMI-design checkpoint (2026-10-31):** The user
authorizes two bounded next tasks. First, inspect and, only after feasibility
confirmation, run a background position-sorted GAMP preparation on the preserved
original
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/mpmap_pbmc1k_v3_20261027T122300Z/attempt5/full/alignments.gamp`.
The sort must preserve complete QNAME groups—every alternate record remains
together; it cannot treat records independently. Local `vg gamsort` source
supports GAM/GAF, not GAMP, so actual combined-vg support remains under
inspection and no sort is launched or implied. Second, analyze potential early
UMI-deduplication speed savings only. It does not authorize a frozen counting
policy, biology, scoring, CB/UMI, or output change. Compressed D remains live;
keyflight wide focused/full tests have terminal 0 and root is reviewing/staging.
Whole-goal feasibility remains unmet.

**D110 D terminal / E prelaunch checkpoint (2026-10-31):** D exited 0/no OOM at
`2026-10-31T22:18:39.626954194Z` under actual 300-GB memory/no-swap limits.
It reached native EOF with 2,502,433 records/1M groups, 982 drops, 982,842
counts, and 1,302,814 molecules. Independent count/MEX validation and both
all-ten-byte/scientific parities (`parity.json` versus C and
`parity-vs-vector.json`) passed. Read/init/native-wall were 1087.0224/164.0927/
1348.4599 s; cgroup peak was 158,858,080,256 B. `terminal-receipt.json` records
the frozen node-score baseline result only, not F1, Cell Ranger, or whole-goal
evidence.

Frozen unlaunched E is `node-score-keyflight-first1000000-t128.KrM8LH`, differing
from D only by per-key flight. Its binary/source archive and runner are frozen;
focused/full tests passed 6/6 and 204/204, 100-cache repeats and TSAN/ASLR-off
passed, and normal 8/8-GiB plus 0.5-GiB assignment defaults were restored with
focused pass. E uses D parameters and must establish expected byte/scientific
parity, but is not launched. The 10k sort adapter round trip passed exactly in
`build/gamp-sort-check.C3cWqR`; final guard/toy tests continue and no full sort
is launched. UMI grouping is already required and implemented as exact
(profile, CB, feature, PackedUMI) aggregation after per-QNAME assignment, final
CB correction/1MM/cross-gene handling; analysis concerns upstream computational
reuse, not a missing count stage or policy change.

**D110 E/sort/UMI live-state correction (2026-10-31):** The prior E wording is
superseded: at that checkpoint only E binary/source/logs were frozen; its
`run.sh` and `run-receipt.json` were created and frozen afterward. E is now live
as `pancollapse-node-score-keyflight-first1000000-20261031` (container
`042b057de24529656193368c2fa27280e45bdd7edbea6aba1cadaf4a28a44a37`), started
22:23:52.566 UTC, with Docker 300,000,000,000-B/no-swap/128-CPU limits and
cgroup `memory.max=299999997952`, `memory.swap.max=0`, `cpu.max=12800000 100000`.
It is loading indexes; no EOF/parity claim exists.

Background sort attempt 2 is live after the first attempt exited 126 from an
ENTRYPOINT double-bash before input use. It uses explicit entrypoint,
4 OMP threads/6-CPU quota/32-GiB no-swap cap; pack passed 1M reads/2,502,433
records/4,370,496,823 payload bytes. Output
`R/position-sorted-gamp.KhGLHP/position-sorted.gamp` is not published until
pipeline counters pass; final toy/empty/10k guards passed and malformed input is
rejected while unknown field 101 is preserved. Frozen UMI diagnostic
`R/umi-reuse-first1000000.NAojV1` is live in a 16-GiB/2-CPU container: 10k
groups/records/drop counters passed; 1M is pending. Exact raw UMI families are
not safe representative-read assignment. E timing is confounded by shared sort
storage and the 2-CPU diagnostic: establish parity first, not isolated A/B speed.

**D110 UMI diagnostic terminal / E-sort live update (2026-10-31):**
`R/umi-reuse-first1000000.NAojV1` exited 0/no OOM at 22:25:09.307 UTC with
`memory.peak=3466788864`; `validation.json` passed 10k and 1M. Its
`first1000000.json` has 1M valid reads, 899,427 raw families, 100,573 repeated-
family reads, 999,018 eligible after 982 drops, 89,896 multi-read families,
37,856 mixed full-graph signatures, 5,447 same-sequence CB/UMI mixed families,
709,598 distinct graphs, and 938,970 distinct family+signature pairs. Identity
redundancy is 60,048 (6.0107%) within UMI family versus 289,420 (28.9704%)
globally. Sol's exact D-cache algebra finds 54,238 distinct unretained plus
2,903 repeated uncached entries: capacity-only additional-reuse ceiling 0.2906%
eligible, with actual reuse unknown because hits can be loser builds. This is
fixture-only theoretical redundancy, not speedup, safe representative-read
assignment, or biology/policy evidence. Diagnostic-only helpers are
`scripts/gamp_sort_envelope.cpp`, `scripts/gamp_umi_reuse.cpp`, and their CMake
targets; runtime count UMI handling is unchanged.

E completed initial loading in 152.925 s (native GBWT/RI/XG/index
146.852/32.157/6.851/4.815 s) and its latest count log has no EOF. Sort pack is
live at 3M groups/7,508,995 records/12,961,427,041 bytes, unpublished. Exact next
task is monitoring E terminal parity and sort completion/counters; no full-genome
count or goal claim exists.

**D110 reference-query hot-path analysis checkpoint (2026-10-31):** The unchanged
whole goal is 66,601,887 groups in <2 h and <300 GB memory; after initialization
it needs about 9,500 groups/s and is not met. E is fixed running to EOF/parity,
not restarted for a poll timeout: verified 50,000 groups/49.3897 read seconds
(about 1,012/s), initialization 152.925 s, live/no OOM. Sort remains live at 3M
groups. The authorized next phase is bounded analysis of existing M gprofng
evidence: `FastLocate::locateNext` 36.22% inclusive/9.74% self,
`Index::path_row` 8.54% self, `model_ids` 8.32% self, and `build_node` 34.23%
inclusive/4.94% self. Investigate exact direct numeric model/path-row lookup and
duplicate path-model work only. No annotation biology, containment rule, startup
reference preprocessing, heuristic cap, or patch is authorized before Sol and
root review. No new experiment or hash is authorized.

**D110 pre-implementation distinct-document node-builder checkpoint (2026-10-31):**
Sol approved one coherent node-score-only change, accepted by root but not yet
implemented. `path_row` is already direct mmap `uint32[gbwt_path]` with
380,460,061 entries/190,240,849 rows and model IDs CSR. New node-score query
would use `FastLocate::find/locate` unique sequence IDs while retaining
state-size raw-occurrence statistics; sort/reduce annotated row+orientation,
call `model_ids` once per row, then flat-vector sort/reduce raw
pair/record-ID/flags/support conflict and score count by distinct row. Full legacy
mode and lazy `node_scoring` raw requery remain unchanged. No index format,
startup join, biology, approximation cutoff, or avoidance of per-occurrence
FastLocate cost is authorized. Required evidence: repeated node/path/both
orientations, exon+body pair, unannotated paths/supports/malformed bounds,
full-vs-node-score getter parity, full 204 suite, all-ten-file/scientific parity
versus E, and 300-GB cap. It is not expected to provide 9x speed alone: old
profile bounds metadata plus builder are about 22% self, so the goal remains
unmet. E stays immutable to EOF and sort stays background.

**D110 read-only legacy-work audit checkpoint (2026-10-31):** User requests a
simplification audit, beginning with a legacy visit-count map that node-score
scoring may not use. Root may scan the active count/node-score path for obsolete
executed work and retained structures versus legacy-only/convert code, recording
exact callers, consumers, and cost bounds. This is not authorization to remove
counting rules or code: CR7/custom scientific branches are not presumed legacy,
and no broader implementation follows. The next authorized action is an audit
report, not a purge. The already-approved distinct-document worker continues only
in `native_count_reference.cpp` and `native_count_query_test.cpp`; its work must
not be reverted. E and the background sort continue unchanged.

**D110 production-surface retirement checkpoint (2026-10-31):** User explicitly
overrides prior legacy preservation: production is `index` plus native `count`;
everything else is default-off or deleted, recoverable through Git if needed.
Plan retirement of convert, BAM/reference-walk assignment, compatibility replay,
raw-GFF count, and old Ex50 resolver routes rather than retaining new legacy
flags or active branches. This does not retire frozen biological profile, CB/UMI,
node-score, or directed-splice rules. Safeguard dirty/untracked work outside the
production surface before deletion (existing R snapshots assist); no commit or
reset. Active index/count tests and parity goldens remain, with frozen binaries
as external comparators. The bounded node-builder worker finishes its patch and
full-mode parity first; root then coordinates deletion. E/sort runners remain
unchanged until a tested replacement. Next authorized task is Sol's bounded
extraction/removal architecture with caller/dependency checks—not a mass purge.
No release/restart is authorized; whole-goal feasibility remains unmet.

**D110 converter-preservation correction (2026-10-31):** User supersedes
converter deletion only. Supported production commands are `index` and native
`count`; `convert` remains as a third, explicitly **deprecated** BAM-conversion
command. It is not a new legacy flag or a supported production count path.
Retirement continues to target obsolete count engines, compatibility replay, and
count-startup reference-walk work, without biological-rule change. The node-builder
worker has finished its two owned files pending root review/build; E and sort
continue. Next architecture review is minimal actual count-legacy deletion while
preserving deprecated `convert`.

**D110 E terminal / F-freeze and purge sequencing checkpoint (2026-10-31):** E
native count exited 0: `timing.tsv` spans 950.123016 s, manifest initialization/
read are 156.876415759/705.031969301 s, and cgroup peak is 158,946,189,312 B.
Independent count/MEX validation and both all-ten-file/scientific parities passed.
`terminal-receipt.json` records one-million-read frozen node-score evidence only,
not a whole-genome/F1/Cell Ranger timing claim. Background sort remains live at
9M packed groups.

The distinct-document node patch finished; root removed temporary reduce-vector/
dead checks. A stale reverse+body fixture expectation was repaired to exact
F2/R1 and F1/R0; focused repair passed 5/5 and full 205 is running in root's
session. Deprecated-convert help/warning/README and `convert_deprecation.cmake`
test pass; no obsolete count code is removed yet. Pre-purge safeguard
`R/pre-count-legacy-purge.DfQFWi` holds HEAD/status JSON, tracked patch, and
complete 4.7-MB source/tests/docs/scripts archive without a new hash. After F
freeze, Sol's accepted plan is to extract shared projection/ResolvedAssignment,
then remove legacy CountReferenceResolver/rawcount/compatibility/expanded-index
production routes while retaining deprecated BAM `convert`, active profiles,
CB/UMI/splice rules, and goldens. Do not mix the hot-path F comparison with this
later purge until freeze and normal restore. Whole <2h/<300GB goal remains unmet.

**D110 F distinct-document 1M prelaunch checkpoint (2026-10-31):** Frozen F is
`R/node-score-distinct-doc-first1000000-t128.GEL63B` with binary and
`source-distinct-doc-wide.tar.gz`. Normal and wide focused tests passed 5/5 and
full suites 205/205. Logs retain the initial stale fixture assertion and missing
`umi_tools` Python-path failures; corrected complete runs passed with no disabled
tests. Source defaults are restored to node/edge 8/8 GiB and assignment 512 MiB;
root rebuild/focused session 69481 remains pending. F is unlaunched and changes
only distinct-document node construction; convert-label changes are non-counting.

F uses E's 64/16/4-GiB experimental capacities, 128 threads, 300e9-byte/no-swap
cap, same 1M input, two profiles, node-score, 64M cutoff, and independent output/
ten-file parity gates. E terminal parity passed. Background sort remains live
(last 12M groups), so F timing is not isolated A/B evidence. After restored-
default success and review, the next authorized action is F launch. Broader count
purge stays separate after this freeze; third `convert` stays deprecated and kept.

**D110 F launch / native-only cleanup checkpoint (2026-10-31):** Restored-default
focused tests passed 5/5. Frozen F launched as
`pancollapse-node-score-distinct-doc-first1000000-20261031`, container
`4be03058dab99db2a14c3771efa3000d06285a23a89feaad806dbd5ae0a4ad17`;
verified memory 299,999,997,952 B, swap 0, CPU quota 128 cores. Native PID 8 is
loading references; F completion/parity remain pending. Disjoint workers may
now extract shared projection and retire obsolete count CLI/runner routes.
Preserve deprecated `convert`, frozen biological rules and the immutable F/sort
runners. This is implementation cleanup, not a new scientific counting policy.

**D110 current cleanup and research gate (2026-10-31):** The 66,601,887-group
whole-GAMP goal (<2 h, <300e9 B) remains unmet; no whole count is launched. E is
terminal frozen one-million-read evidence at
`R/node-score-keyflight-first1000000-t128.KrM8LH/terminal-receipt.json`: native
exit 0, 950.123016-s wall, 705.031969301-s read time, 158,946,189,312-B peak,
and independent count/MEX plus both ten-file/scientific-counter parities passed.
F is separate, frozen, and live at
`R/node-score-distinct-doc-first1000000-t128.GEL63B` under
299,999,997,952 B/no swap/128 cores; a read-only observation is 320,000
completed groups at 228.906 s, with terminal completion/parity pending. The
background sort is live; `R/position-sorted-gamp.KhGLHP/pack.jsonl` last records
17M packed groups and no final output.

Pre-cleanup is code removal, not data or biological filtering. Implemented work
is NativeSource+NodeScore-only prepared count and verbatim shared metadata
projection extraction. The main worker is physically deleting entangled
unreachable count/replay branches while preserving converter behavior. Root has
migrated integration tests to fixed pre-refactor node-score fixture expectations
and preserved converter tests, but no build/test has yet covered this refactor;
its component build exposed only a fixed missing explicit vg protobuf include
and a stale native bool in `prepared_count` for root to fix. Root must build and
run the full suite after source freeze. `convert` stays
deprecated; obsolete count engines are deleted, not hidden behind flags.
Dirty/untracked pre-purge material is safeguarded outside production at
`R/pre-count-legacy-purge.DfQFWi`, without hash/reset/commit.

Only after refactor verification and F parity may root review a bounded,
read-only native-lookup-cost profile for exact runtime optimization; it does not
authorize an algorithm or biology change. Before any UMI design decision,
research/review evidence-aware pangenome UMI-family inference,
consensus/representative selection, locus+CB+UMI grouping versus post-gene
counting, and current implementation. Do not describe UMI dedup as random read
selection. This research gate authorizes no UMI algorithm, grouping, or
biological-policy change pending root review.

**D110 UMI timing clarification (2026-10-31):** User asks about moving family
inference, correction/deduplication and consensus before counting or alignment;
do not reduce that question to random representative selection. Exact attempt5
producer/command/input receipts confirm all 66,601,887 R2s were individually
aligned after raw CB/UMI/quality tagging only, with no prior family grouping,
deduplication or consensus. Current counting groups/corrects molecules after
graph assignment. Primary-source methods research is distinct from adopting a
new molecular resolver; no UMI-policy change has been made. Native component
refactor tests passed 4/4, and pruned query test passed independently; main/full
suite and F terminal parity remain pending.

**D110 F terminal / lookup-design gate (2026-10-31):** Frozen F terminal receipt
is `R/node-score-distinct-doc-first1000000-t128.GEL63B/terminal-receipt.json`:
exit 0/no OOM; 884.117372-s wall (166.691148893-s initialization,
617.838627167-s read, 2.999921962-s finalize); 159,724,564,480-B peak; EOF at
2,502,433 records/1M groups; 982 complexity drops; 982,842 count rows; and
1,302,814 molecules. Validation/MEX validation and both all-ten-byte/scientific-
counter parities passed. It is one-million-read node-score evidence, not
whole-genome, F1, or CR full-parity evidence. Sort remains live at 22M packed
groups, so F is not isolated speed A/B.

The 66,601,887-group <2h/<300GB objective remains unmet; no whole count is
launched. Native components passed focused 4/4 and retired query 1/1, but
converter-only extraction is finishing and full-program tests are pending. UMI
research is complete as source findings plus verified preprocessing, with no
policy change. In parallel with converter-only cleanup, root authorizes
read-only native-hot-path correctness/performance design from frozen F
source/tests/parity; no changed-binary launch precedes full build/tests.
Review-only candidate: exact ordinary-body witness short-circuit in
`build_splice_evidence`. It must prove that ordinary adjacency for every possible
common body target in any orientation makes the splice-target set empty under
existing semantics. A bounded RI witness scan/fallback may only accelerate;
budget exhaustion cannot drop reads/evidence. This is separate from CLI removal
and UMI research and authorizes no biology or eligibility restriction.

**D110 native-only verification / witness implementation gate (2026-10-31):**
Root full build plus 168 remaining tests passed in 27.12 s at
`build/ctest-native-only-full.log`; `native_count_verify`, `convert_deprecation`,
and `count_cli_surface` passed. Immutable snapshot
`R/native-only-refactor-check.SYZ2ls` contains binary/source/logs and
`receipt.json` (168/168, unchanged policy, no recomputed hashes). Main is about
4,880 lines with no active old count/replay branch; old expanded builder/resolver
are physically deleted, while RAD/BAM `convert` stays. New-binary 1M performance
is unmeasured; the whole goal remains unmet.

Sol/root accepted, as the next implementation only, an exact ordinary-body
witness short-circuit in `build_splice_evidence`, independent of CLI removal and
UMI science. Under valid trusted native inputs, existing splice formula is
`E intersect C minus B`; empty `C` or witnessed `C` subset `B` proves no splice
target, else full fallback. Raw-SA global target-scaled budget is `max(256,4*C)`,
capped at 4096, solely an accelerator: it never drops reads/evidence. Preserve
resume prefix/full RI cursor fallback without rescan, thread-local synchronous
edge-cache builder, and splice per-key singleflight. Local `FastLocate`
find/unpack/locateNext APIs were checked. Shortcut may skip incidental malformed
RI-tail detection; no new full-scan validation/hash is required under existing
trusted-input contract. Test-only probe limits 0/1 force fallback; no count CLI
mode. Worker owns native cpp/hpp/query tests; root owns prepared-count stats and
assignment-cache width. Retain 8/8-GiB source cache and 512-MiB assignment cache
until patch tests pass, then operationally use tested F 64/16/4-GiB capacities
under the external 300-GB cap. Required before the changed-binary 1M benchmark:
full 168 plus forced-probe cases. Required before the whole-GAMP launch:
1M ten-file/scientific parity versus F and resource/throughput evidence.
No profile, biology, or UMI-model change is authorized.

Alevin review is separate: molecule-level resolution is appropriate, but PUG+EM
versus family argmax remains an unresolved scientific choice; no resolver is
adopted.

**D110 ongoing witness implementation reconciliation (2026-10-31):** Exact
ordinary-body witness cpp/hpp cursor-resumption implementation is complete;
worker tests are pending and root will build. The required path is witness tests
and full suite, changed-binary same-1M all-ten-file/scientific parity versus
frozen F plus resource/throughput evidence, then the unchanged whole
66,601,887-group <2h/<300GB gate. No smaller substitute goal, whole run, or new
speed measurement is claimed.

Lazy wrong-way-splice evaluation only for competitive alignments is a user
proposal, not adopted. It is sound only with complete ties/window/support/
antisense/rejected-diagnostic certification, so it authorizes neither code nor
speedup claim. Defaults and biology remain unchanged. Native `count` is not PUG;
`convert` already supplies `map.rad`, and full-168 interoperability is cr-like,
not original-Alevin-parsimony replication. PUG+EM versus family argmax remains
an independent unresolved scientific choice.

**D110 G body-witness prelaunch checkpoint (2026-10-31):** Default witness
suite passed all 168 in 27.63 s (`build/ctest-body-witness-full.log`). Root fixed
common-empty success, immediate complete-body proof, cursor unordered-map
comparator, and sorted common-body intersection. Scope is native cpp/hpp/query
test plus prepared-count operational statistics; Sol found RI cursor/prefix/
synchronous-cache invariants sound in principle, with no blocker. Matched-F-cache
wide (64/16/4 GiB) build is complete. Root verified its terminal 168/168 pass in
27.25 s at `build/ctest-body-witness-wide-full.log` before launch.

G is prepared only at `R/node-score-body-witness-first1000000-t128.FCVKgz`,
with `run.sh`, the matched-cache binary, logs, receipt and F/counts comparator.
Following immutable source freeze, proceed with same-1M execution at 128 CPUs,
300e9 B/no swap, independent validators, and all-ten-file/scientific parity.
Do not treat preparation as Docker launch or a speed result. Whole launch remains
conditional on that parity/resource gate; no profile/biology/UMI change is made,
and the <2h/<300GB whole objective remains unproven.

**D110 G 1M live (2026-10-31):** Root verified both full-168 terminal passes,
reviewed the documentation checkpoint, froze matched-cache G, then launched
`pancollapse-node-score-body-witness-first1000000-20261031` at
`2026-10-31T23:19:45.976220066Z` (ID
`4f2744cde4538aaa84111088af90e24e59522a6f909d37c41092f9a0706a455d`).
Live native PID 119155; cgroup limit 299999997952 B, swap 0, CPU quota 128.
Four reference branches are loading in parallel. No performance or parity
result is claimed; whole production is still pending the empirical gate.

**D110 endpoint-free DP specialization pre-edit checkpoint (2026-10-31):**
Sol/root accept the bounded proof only: if unchanged complete reachable-edge
prepass leaves a coalesced group's final `SpliceOp` vector empty, every internal,
cross-piece, and empty-bridge transition permits both final strands. Thus
`StateKey.last` stays null and `valid` mask 3; skip only last tracking and
allowed/internal-mask work, retaining scores, exon/strand balance, body, bases,
support, and normal merge. Restrictive groups remain unchanged. No fallback,
biology/molecule change, preliminary-winner certificate, or cross-record cache.

Worker scope is `node_score_resolver.cpp` and `tests/vg/node_score_dp_test.cpp`;
root builds. Required: independent-oracle parity; empty/insertion bridges;
explicit mask 3; restrictive internal/bridge refusal; variable-support tie union;
record-local masks; both strands/Gene objective; reduced targeted-toy states;
full 168; and existing 320 seeded cases. Frozen live G is reference witness
benchmark only (current 10k 12.7004 s), not a speed claim; source does not alter
it. No adaptive cache work is implemented. Whole <2h/<300GB remains the goal.

**D110 G live CPU-profile / cache requirements checkpoint (2026-10-31):**
Endpoint-free is not promoted: unlaunched cpp/tests passed focused 2/2
(`build/ctest-endpoint-free-focused.log`), but current code stays running for CPU
profile interpretation because the certified case may be rare. No performance
conclusion, new algorithm, or further implementation is authorized first.

Frozen G is live at 420,000 groups/271.038 read seconds. Separate profile
container `pancollapse-live-cpu-profile-G-20261031`
(`13cf850f726f0d41a92436cf6abd89d846ce64ee71cee844e91bcc28654d4c08`) attaches
to PID-namespace process 8/host 119155; artifact `G/cpu-live-profile.5WMEZo`.
It uses perf 6.8.12, `cpu-clock:u` 19 Hz, DWARF 4 KiB, 30 s, in separate
4-GB/4-CPU PERFMON+SYS_PTRACE container; no host sysctl change/target restart.
`perf.data` grows; report pending. Post-attach timing is not an unprofiled G
baseline. Future cache limits must be configurable, overall-target-scaled and
generous, with arbitrary fixed ceilings removed; no implementation, optimum, or
default allocation is yet claimed. Whole <2h/<300GB remains pending.

**D110 G terminal live-profiling evidence (2026-10-31):** G remains live at
700,000 groups/412.740 read seconds; RSS 149.7e9 B, cgroup current 140.5e9 B,
peak 140.7e9 B. Exclude initial stack profile `G/cpu-live-profile.5WMEZo` from
percentages: 336.979-MB data lost 198,970 samples (71.96%); inclusive analysis
stopped its report container only, not count.

Valid repeat `G/cpu-live-flat.BMof18` record/report status 0 used 49-Hz
`cpu-clock:u`, no callgraph, 30 s, 8.433-MB data, 186K samples, zero loss, and
empty report stderr. Self samples: `select_support_mcl::select` 21.64%,
`FastLocate::locateNext` 10.90%, `NodeScoreResolver::resolve` 8.60%,
`int_vector::get_int` 7.49%, node-build add-path lambda 5.14%, `Index::path_row`
4.46%, `model` 2.66%, plus DP helpers. These are self, not inclusive; scoring is
not thereby 8.6% total. Initial hardware stats/10.00975 s: 124.906 CPUs, 0.72
IPC, 31.94% cache misses/reference, 75% multiplex coverage. Profiling contaminates
G wall timing; no full parity, endpoint-free promotion, or cache implementation
follows. Whole <2h/<300GB remains unchanged.

**D110 configurable generous cache-limits preimplementation checkpoint
(2026-10-31):** Under the user's configurable/scaled/generous requirement, root
selects a startup-only planner, not a live adaptive controller. Pending endpoint-free code stays out of next binary; root preserves/
undoes its experiment. `count --memory-budget <bytes>` selects planning target
(default effective cgroup limit/available RAM); repeatable
`--cache-limit node|edge|assignment|splice=<bytes>` has positive-unit strict
parsing and duplicate rejection before GAMP. Post-reference-load RSS is measured;
reserve 10% target for workers/counts (heuristic headroom, neither hard aggregate
cap nor spill); pool is `max(0,90% target-RSS)`; unoverridden pool uses
renormalized 60/25/10/5 node/edge/assignment/splice weights. Overrides sum at
most pool. No optimality/upfront-allocation claim.

Assignment entry ceiling becomes byte-only. `NativeSource.configure_cache_limits`
is startup-only with empty/no-flight caches, all shards held and validation before
change; no hot resize. Log/manifest target, postload RSS, reserve, pool, actual
caps. Cache limits do not guarantee RSS; external 300-GB cap governs; defaults
account shared cgroup/MemAvailable. Acceptance: parser/planner overflow,
duplicates, partial/all overrides, rounding, insufficient headroom;
configure-after-query reject; tiny/default/huge scientific parity; full suite.
Frozen G is running (999,998 groups/570 s), full parity pending; documentation
does not wait for terminal G.

**D110 G terminal / cache-planner integration checkpoint (2026-10-31):** G
exited 0/no OOM. `G/terminal-receipt.json` records 801.679596-s wall,
140.956499312-s initialization, 588.488848619-s read, 2.920263331-s UMI final,
154,255,171,584-B peak, and EOF at 2,502,433 records/1M groups, 982 drops,
982,842 count rows, 1,302,814 molecules. Count/MEX validation and
`parity-0.json` all-ten-byte/scientific-counter/summary/logical-table checks
against F passed. Profiling plus concurrent sort contaminate timing: not isolated
A/B performance, whole-genome, F1, or CR full-parity evidence.

Endpoint-free is unpromoted at `R/unpromoted-endpoint-free.KrSF4h/source.tar.gz`
with focused logs; root undid cpp/test changes, so next binary retains G scorer.
Configurable generous memory-budget/cache-limit CLI/helper/runtime integration is
implemented; `build/build-memory-scaled-caches.log` and
`build/ctest-memory-scaled-caches-full.log` for pending 169-test build/full suite
are not yet acceptance evidence. No full-GAMP production launch; whole
<2h/<300GB goal remains unmet.

**D110 whole memory-scaled prelaunch checkpoint (2026-10-31):** Cache
configuration full suite passed 169/169 in 27.67 s
(`build/ctest-memory-scaled-caches-full-rounding.log`): pure planner
rounding/minimum-1-byte correction, invalid options, and `native_count_verify`
tiny-1-B/default/generous-64-MiB fixture scientific parity. Startup memory
target/caches are configurable; old 655,360-entry assignment ceiling removed.
No new 1M run is required for cache-retention-only integration: scorer restored
G and G already has 1M ten-file parity.

Whole attempt is prepared only at `R/whole-genome-memory-scaled-t128.XOrxaC`
(binary, logs, `run.sh`, `run-count.sh`, `run-receipt.json`), not launched. It
uses current tracked full/pilot `alignments.gamp` (30,326,279,277 B; 66,601,887
reads), while 30-Oct versus recorded-27-Oct provenance remains asynchronous; no
new dataset. Planned: 128 threads, 300e9 B/no swap, `--memory-budget 300GB`,
automatic generous post-load caps, both profiles, MEX+Parquet, 64M guard, no
profiler. Background sort stays original-envelope transport. After root review,
freeze then launch; Docker/PID/cgroup, EOF/groups, validators, wall/RSS/cgroup
are required before any performance/goal claim. No more pilot; <2h unproven.

**D110 whole memory-scaled live launch checkpoint (2026-10-31):**
`R/whole-genome-memory-scaled-t128.XOrxaC` was frozen with
`source-memory-scaled.tar.gz`; binary matched `build/src/panCollapse` by `cmp`,
and binary/archive/run scripts/prepared receipt are read-only. Container
`pancollapse-whole-genome-memory-scaled-20261031`
(`68825f7852326af087266ebb1abec3114a564efeceb81f85376bb52ce0d656a4`) started
2026-10-31T23:43:46.622153755Z (host init PID 236004; native host PID 236044).
Cgroup: 299,999,997,952-B memory.max, swap 0, 128 CPUs, all memory.events zero.
Parallel native.pci/XG/GBWT/RI load has begun, not streaming. Same tracked full
66,601,887-read/30,326,279,277-B input retains 27-Oct/30-Oct provenance caveat;
no profiler. G 1M is control evidence only. Verify ready/memory plan/read rate,
EOF/groups, validators and resources before any claim; launch does not establish
the <2h/<300GB goal.

**D110 live whole performance-investigation checkpoint (2026-10-31):** Frozen
whole container `68825f7852326af087266ebb1abec3114a564efeceb81f85376bb52ce0d656a4`
continues unchanged; still GBWT loading at 23:45:07Z, with no new rate. Recent
G 1M was ~1,699 reads/s versus ~9,250/s <2h minimum (~5.5x gap); ~11-h linear
projection is not a full-run ETA. Whole run continues.

Authorize read-only investigation of exact neighboring-node membership reuse via
occurrence-bijection certificate, to avoid redundant RI occurrence decoding and
measure prevalence before promotion. Profile self evidence is `select` 21.64%,
`locateNext` 10.90%, `get_int` 7.49%; main scorer 8.60% self is not inclusive.
Consequential proof review precedes code; retain frozen G 1M outputs/full 169.
Measure live cache-warm throughput first. No shortcut/endpoint-free promotion,
biology change, memory relaxation, or whole restart is authorized.

**D110 neighbor-membership pre-diagnostic-code checkpoint (2026-10-31):**
Sol/root accept universal physical-adjacency empty-splice certificate under
trusted valid bidirectional GBWT/RI: if either orientation of either endpoint
has nonempty outgoing successors, all successors are physical other endpoint,
and no endmarker, each `C` body target has ordinary `B` adjacency; thus
`E intersect C minus B` is empty. Starts/ends/repeats/mixed orientation/missing
annotation rows covered; `NativeSource` enforces bidirectionality. G 1M had
8.92M edge queries/3.855B probe-SA steps, but this/neighbor reuse cannot close
~5.5x gap; 45.17% profile share implies 1.82x ideal Amdahl.

Authorize only standalone no-RI 10k prevalence diagnostic: mirror resolver
endpoint propagation; unique physical pairs/query-weighted prevalence;
record-predicate versus exact GBWT-find cross-check; potential avoided edge
occurrences; bounded aggregate log. No production scorer/cache edit, promotion,
restart, or hash. Whole live >260k/~1,820 recent reads/s at 300GB/no swap/128.
NUMA anon N0 ~34.9 GiB/N1 ~76.4 GiB (~69% N1) is not remote-cost proof; no
affinity change. Suffix saturation separate proof; endpoint-free held.

**D110 sample-dependent DP threshold-saturation implementation checkpoint
(2026-10-31):** New Sol proof supports root authorization of automatic,
semantics-preserving saturation; frozen live whole binary unchanged. For each
coalesced group/piece: exon margin `2*exon-bases`, strand forward-minus-reverse;
reverse-DAG `[L_j,U_j]` includes successor `j` over all completions. Before each
seed/merge canonicalize entering `b`: `b>=-L_j` -> `-L_j`; `b<-U_j` ->
`-U_j-1`; else exact. Exon/strand independent; last/valid/all-exonic/body/bases/
objectives/support unchanged. Sequential quotient is exact.

Wide-128 preflight requires representable original signed bases/margins,
exon/strand sum-absolute piece deltas <= `(INT64_MAX-1)/2`, score/gene sumabs
<= `INT64_MAX`;
unsafe group retains original DP/errors. Automatic only; force-map/sparse/
enumerator controls unchanged. Private compact helper only for tests, no CLI.
Require 320 seeded oracle, threshold/overflow/multistart/mask/support cases,
full suite, real same-10k/1M parity before promotion, then state/time measure
without presumed 5.5x. Adjacency diagnostic is separate/precedes shortcut;
endpoint-free remains unendorsed.

**D110 threshold/adjacency real-gate prelaunch checkpoint (2026-10-31):**
`src/threshold_balance.hpp`, resolver integration, and prepared logging are
implemented. Pure tests plus variable-length positive-balance fixture retain
enumerated counts and reduce states >3x versus force-map. Focused DP/helper
including 320 seeded and final full 171/171 passed 30.07 s
(`build/ctest-threshold-full.log`). Corrected adjacency diagnostic API/fixture
issues: edge pair, histogram/count sums, separate reversed fixture, GBWT dynamic
include; selftest passes/full 171 includes it.

Frozen unlaunched A `R/adjacency-prevalence-first10000.yxZIo8` is GBWT-only/no
RI/no GAMP count, 10k structural-pair upper bound, 128GB/no swap/4 CPU,
aggregate JSON/no speed claim. Frozen unlaunched T
`R/node-score-threshold-first10000-t128.bDX1Cb` is read-only binary/archive/logs/
script: same 10k, both profiles/MEX, 128 threads/300GB/no swap/memory target;
independent validators and ten-file/scientific compare to frozen static cache.
No production adjacency shortcut. Root review/capacity check precede launches.
Whole W unchanged >1.4M/~2,100 recent/s/~180GB; concurrency makes T timing
nonisolated. T parity is not 1M/F1/CR proof; 1M parity precedes promotion/restart.

**D110 threshold/adjacency real-gate live checkpoint (2026-11-01):** A live as
`pancollapse-adjacency-prevalence-first10000-20261101`
(`ac4edd70cc1f91192e7519ce85b0d0c0122852eae600c78947c95306f8f91fa5`), started
00:00:45.322173614Z; host init 313521/helper 313557; 128GB/no swap/4 CPUs,
GBWT loading. T live as `pancollapse-node-score-threshold-first10000-20261101`
(`b456b6f2d1615859163dee3d39779ae3bd0cfb9cb20dff844b0d20d8860ab1f0`), started
00:01:03.719884679Z; host init 317333/native 317369; 299,999,997,952-B cap,
swap 0/128 CPUs. Frozen source/binary/scripts. Sol accepted code proof
conformance: unsafe fallback retains original invalid errors; linear-prepass
overhead unproven, stronger resolver exception test optional. Await A JSON and T
EOF/validation/parity/state/timing; no promotion/restart/goal claim. W unchanged
>1.52M/~2,300/s/~188GB; W/T shared loads/background sort make timing nonisolated.

**D110 A/T terminal / adjacency implementation checkpoint (2026-11-01):** A
exited 0/no OOM, 179.341119 s, 81,317,462,016-B peak; 10k/25k/14 drops;
unique 131,436/111,135 certified, weighted 170,183/144,613 (~85%), four edge
occurrences 239,596,894/204,572,195 (~85.4%). Direct-record/GBWT-find crosscheck
passed; load/scan/certify 170.278/5.56907/0.524194 s. Structural superset only,
not count work/CPU speed.

T exited 0/no OOM: 219.087795-s wall; init/read/finalize
175.339366509/29.650317813/0.039151048 s; 133,459,050,496-B peak; 13,904/13,995
counts/molecules. Validation/MEX/parity-0 all-ten/scientific/summary/logical
against static passed. DP states 49,948,641 vs 1,258,500,869 (~25x); frontier
69 vs 1411; 158,551 saturated/zero unsafe. Cache races slightly differ raw work;
concurrent timing is not 2x. W unchanged 2.175M/~2k/s/no OOM.

Authorize node-score-only universal adjacency return after same-node return plus
both node-evidence validations/before common intersection/SA: either anchor
orientation nonempty/all non-ENDMARKER successors physical other endpoint.
Facts/profiles/strand unchanged. Add checks/success stats/test-only disable bool
after existing load max-probe arg; no CLI/full mode control. Root production,
bounded query tests. Require adversarial/random predicate-native-full parity,
171, then one combined threshold+adjacency 1M ten-file parity versus G/F before
promotion. No separate T 1M/10k/restart/version/policy/memory change.

**D110 complexity/SIMD read-only review checkpoint (2026-11-01):** User asks
complexity/SIMD review; root pauses further production edits/new fast paths.
Combined threshold+adjacency passed 171/171 in 34.07 s
(`build/ctest-threshold-adjacency-full.log`). Fixture assertion corrected:
ordinary small pairs already old-probe edge 0; new 512-unannotated-path-heavy
case forces probe exhaustion, certificate zero SA/edge versus baseline >0. Full
parity: 440 ordered pairs, 32 random predicate-FM checks, endpoint-invalid, cold
four clients.

Combined 1M `R/node-score-threshold-adjacency-first1000000-t128.zLDSex` is
empty/prepared only: no frozen binary/launch. Read-only review assesses actual
profile algorithm cost, maintainability/minimal DS, SIMD fit on AMD EPYC 7713
(AVX2/no AVX-512). No SIMD implementation, policy/UMI change, promotion, whole
restart, or review experiment. W unchanged 3.27M/no OOM; A/T evidence remains.

**D110 read-only multi-agent per-read-speed review gate (2026-11-01):** User
requests independent reexamination of necessary per-read work for gene
compatibility and uniquely assigned UMI counting. This authorizes
assumption/complexity/speed review only: no production or scientific-policy edit,
experiment, launch, or goal resumption. Retain historical G/A/T/SIMD/profile
receipts and combined 171/171 as evidence; combined 1M remains prepared only,
never frozen/launched. Do not infer current job state from prior checkpoints.
Root reviews this checkpoint before reviewers begin.

**D110 completed read-only per-read review (2026-11-01):** Root reviewed
`/root/minimal_gene_compatibility_review`, `/root/umi_first_assumptions_review`,
and `/root/per_read_work_inventory` plus source/receipts. Verified observations:
reference assignment projection round-trips numeric-to-owned-string-to-numeric
(`reference_assignment_projection` 70--111; runtime 666--670); resolver already
pools a five-point window (542--556) before repeated assignment (407--420);
rejected shadow resolver (578--594) clears genes, affecting reporting not
molecule calls; body-support ratio assignment (313--333) is body-multicandidate
only. Existing 1M UMI receipt has 999,018 eligible, 709,598 global graphs, and
938,970 family+signature; current G summary has 286,517 hits. Existing profile
self samples select 21.64%, locate 10.90%, packed get 7.49% predate combined.
No SIMD claim/profile or design is adopted. Exact count-preserving changes,
reporting changes, and scientific-algorithm changes remain distinct; none is
authorized. Goal stays paused; combined 1M unlaunched; future work needs user
direction, not implicit resume.

**D110 numeric-native-count implementation pre-edit checkpoint (2026-11-01):**
User authorizes bounded implementation: retain numeric identities through native
count and convert to strings only while writing output; diagnostic-only work may
be removed and counters may change as simplification requires. Exact count and
molecule tables remain the target against frozen G/T controls. Biology, CB prior,
UMI correction, splice and score-window semantics remain frozen. This does not
change sequence consensus/family inference, regenerate indexes, or adopt a new
scientific policy. Combined 171/171 remains implementation evidence; combined
1M is unlaunched. Whole goal remains paused: authorization covers code and
bounded tests, not whole launch/resumption. Root reviews this checkpoint before
implementation.

**D110 custom-index scope addition (2026-11-01):** User permits custom
panCollapse-index improvement, including dropping RI only if unnecessary;
external XG/GAMP/RI remain unchanged. No full path-step expansion/graph copy is
selected and no representation is adopted. Next is a read-only, specific audit
of current index/native-query opportunities after root review. Numeric core and
runtime integration are unbuilt; frozen control is `R/pre-numeric-count.wjLPSM`,
prepared 1M `R/numeric-count-first1000000-t128.vWzYte` unlaunched. Whole goal
stays paused; biology and exact count/molecule tables unchanged.

**D110 numeric 1M prelaunch checkpoint (2026-11-01):** Numeric engine/runtime/
test-only oracle completed; full 172/172 passed in 33.43 s
(`build/ctest-numeric-count-final.log`, `build/build-numeric-count-final.log`).
First DAG failure was diagnostic-only retry bit 112 versus 96: absent fallback
label compiler omits no-op retry. Focused regression now constrains `.same` to
that reason difference only for nonempty; genes/facts remain strict. Native
goldens unchanged; no index/external modification.

N `R/numeric-count-first1000000-t128.vWzYte` is prepared, not launched, while
root freezes binary/source/172 log. It uses same 1M/2,502,433-record/465,121,668-B
GAMP; 128 threads/300e9 B/no swap/memory target, profiles, MEX+Parquet,
read-only external indexes; no BAM/spill/hash/remap/profiler. After root review,
compare four Parquet/six MEX byte/decoded/logical plus CB-prior counters to G via
`tests/vg/count_table_parity.py`; diagnostic diff records, not acceptance failure.
Old F/G helper passed (`build/count-table-parity-helper.log`). Goal paused: no
whole launch/resume. Custom index not adopted; RI remains needed absent complete
replacement; audit built no index.

**D110 numeric 1M live parity checkpoint (2026-11-01):** N at
`R/numeric-count-first1000000-t128.vWzYte` is launched, not passed, as
`pancollapse-numeric-count-first1000000-20261101-vwzyte`
(`a40768615a9b3c4c67da70ecf5ea696813f48e1a95ead135d9177a64077e26a0`), started
2026-11-01T00:53:08.968726919Z. At 00:54:13Z it was loading GBWT/RI; index/XG
load were 10.0791/14.3376 s, with no count/parity result. Source/binary are
frozen in N; full 172/172 passed in 33.43 s. External files stay read-only under
300GB/no-swap/128-CPU limits. Scope is bounded numeric parity only; whole goal
remains paused with no restart authorized. On terminal result, apply planned G
byte/decoded/logical/CB-prior comparison; launch itself is not acceptance.

**D110 proposed numeric live CPU-profile checkpoint (2026-11-01):** After N
finishes reference load, root may take one external 30-s, 49-Hz flat CPU sample
of live N for current numeric+threshold/adjacency hot-path attribution; prior G
profile predates those changes. This launches no new count and changes no code or
index. It requires a separate immutable profile subdirectory and CAP_PERFMON
helper Docker, no callgraph, target un-restarted, files read-only, record/report
exit 0, and lost-sample validation. Sampling plus concurrent W/sorter means N
wall time is not isolated A/B evidence. Custom-index opportunity audit is now
complete but adopts no representation: readyModel is unimplemented and RI drop
unsupported. Whole goal remains paused.

**D110 N ready/profile snapshot checkpoint (2026-11-01):** N native ready
238.36 s: index/XG/RI/GBWT 10.0791/14.3376/77.9083/227.785 s; combined index
load 232.472 s. At 00:58:34Z ~90,771 groups/90.005 read seconds, live, peak
144,729,862,144 B; parity pending. `N/cpu-live-flat.jdqg24` passed: helper
`56571969...` exit 0/no OOM; 30 s/49 Hz `cpu-clock:u`, no stacks, ~165K, zero
lost, record/report 0, stderr 0. Self snapshot select/locateNext/packed-get
primary/resolver/node-add-path/path-row/model/model-ids:
20.51/12.16/11.46/10.18/9.22/5.20/2.32/2.06%. Do not sum shared helpers into
exclusive stages. Profile overhead plus W/sorter concurrency precludes isolated
speedup claim. Whole goal paused.

**D110 N matched-window performance checkpoint (2026-11-01):**
`N/cpu-live-flat.jdqg24/runtime-cost-checkpoint.json`: 165,127 all-symbol
samples; 2,741 named ZSTD, compression 1.6599%. Matched first 300k: G
204.721 s/~1,465.409/s; N 249.291 s/~1,203.413/s, 17.8787% lower, not halved.
Old complete-run/new cold-prefix comparison was misleading. Approximate
100k--300k 5-s samples: G/N mean CPUs 125.726/113.117; CPU seconds
15,087.106/17,080.740. Host 128 physical/256 logical; Docker N/W/sorter
10800.69/12031.13/123.10%. Do not claim contention-only cause or exclude code
regression. Rank/select/FastLocate/packed access differ from generic ZSTD;
no index/cache choice. N live 400k/320.04 s, parity pending; goal paused.

**D110 numeric 1M terminal reconciliation (2026-11-01):** N terminal receipt
records exit 0/no OOM at 2026-11-01T01:10:43.120947404Z, peak 149,023,064,064 B,
no memory max/oom or spill, and 172/172 CTests. Exact four Parquet and six MEX
gzip byte parity, decoded/logical tables, profile definitions, and all-read
CB priors match G; 982,842 count rows/1,302,814 molecule rows. Native wall was
1012.284079 s (init/read/UMI 238.359599499/715.319922106/3.189814716;
~1,397.976/s) versus G read 588.488848619 s (~1,699.268/s), observed 17.730678%
lower under different concurrent workload: neither speedup nor isolated cause.
Accepted diagnostics include 27,523 early noncountable groups and complexity
937 versus 982. Final RI calls node/edge 4,840,728/1,934,367; node hits
7,560,283; no eviction, so cache-miss design cost is not undersized cache.

Custom-index review finds only complete node-membership plus complete sparse
strand-aware splice target/ownership tables could replace runtime RI; build cost,
storage and completeness remain unproven. Partial observed-edge/GTF-only changes
semantics. RI remains required; one avoidable edge-detail loss is unimplemented.
Bounded numeric gate is accepted, not whole-performance, F1, or CR7 equivalence;
whole goal stays paused with no index adoption or resume.

**D110 historical caller-attributed strand/splice profile prelaunch checkpoint
(2026-11-01):** User authorizes one bounded external profile only: unchanged
frozen N binary/source at `R/numeric-count-first1000000-t128.vWzYte`, same 1M
GAMP, 128 threads, 300e9 B/no swap, profiles/sinks, and read-only XG/GAMP/RI/
custom index. During streaming, take one low-frequency DWARF caller-stack sample
with larger ring buffer to avoid prior G 19-Hz stack-loss 72%; distinguish node
from splice RI by caller. Require record/report/script exit 0 plus lost-sample
and unwind checks. Exact outputs versus N remain required. No production edit,
veto removal, full restart, hash, or causal-speedup inference is authorized.
Whole goal stays paused; existing W/sorter continue unmodified and make shared-
host performance nonisolated. Root reviews before launch.

**D110 historical caller-attributed strand/splice profile live checkpoint (2026-11-01):**
P `R/caller-splice-first1000000-t128.J7G5Fx` is launched, not passed, as
`pancollapse-caller-splice-first1000000-20261101-j7g5fx`
(`52bf7917d8bf2a930dddde3bf88511c05c7b6802f11ec7e0d7b054924a53bf28`), started
2026-11-01T01:46:44.466183548Z. Frozen binary `cmp` matches N. At 01:49:05Z,
GBWT still loading; index/XG/RI 3.34644/9.57119/67.3175 s. Helper
`40c4d4e18b04cdd8b2483fcb0ada55bcdab271bef5a0efc007285b587d1b5ab5`
waits for 100k groups then captures 3-Hz/60-s `cpu-clock:u`, DWARF 8192,
`-m1024`, 8GB/4CPU/no swap at `P/cpu-callers.gZyHMW`. Parser/synthetic tests are
five DEV passes only, with no production edit; P scripts/receipt are frozen.

Existing junction work is annotated exon-path versus body-path adjacency followed
by along-RNA direction, not novel-splice detection or realignment; no policy
change. W/sorter unchanged and whole goal paused.

**D110 P terminal caller-attribution reconciliation (2026-11-01):** P passed
profile/exact N parity: container exit 0/no OOM at 2026-11-01T01:58:11.866689848Z;
1M/2,502,433 records; 982,842 counts/1,302,814 molecules; four Parquet/six MEX
byte plus decoded/logical/CB-prior parity; `changed_manifest_counters={}`. Peak
181,873,664,000 B includes file cache; memory max/oom 0, 300GB/no swap/128.
Wall/init/read/UMI 654.467462/178.336196209/425.698263286/2.498493246 s are
nonisolated, not speedup evidence.

`P/cpu-callers.gZyHMW` valid capture: 3 Hz, DWARF 8192, `-m1024`, 60.370597 s,
22,090 samples, zero lost/empty, 99.9894% RI-like attribution. Inclusive
userspace CPU: reference-splice 5.81258488% (direct 5.80805794%, nested node
0.00452694%), node-only 59.31190584%, resolver-other 31.63875057%, other
3.23675871%; classified node/splice samples all reach resolver. Unknown
frames/leaves/outermost are 21,597/1,968/0: symbol gaps do not block known
ancestry; Sol accepted measurement review; six classifier tests/reprocessing
match. Excludes inlined masks/DAG state/pruning and counterfactuals. No source,
index, policy change or goal resume. Next task is report measured cost only.

**D110 direct-facts custom-index implementation pre-phase checkpoint
(2026-11-01):** User authorizes implementation/testing of immutable read-ready
count source holding graph-specific node length/membership lists, numeric
ready-model metadata, physical splice targets, and directed ownership. No path
positions/steps/global occurrence arrays. XG/GBWT/RI remain unchanged compiler
inputs; no new input hashes or count spill. Preserve count algorithm, profiles,
barcode/UMI behavior, and output. N/P accepted 1M receipts remain controls;
speed and <2h/<300GB whole performance unproven. Prior Sol semantics review
accepted plan but warned raw FastLocate occurrence cost and unknown complete
index size/build time. Work is pending, not completed; direct build follows
bounded acceptance only, and no whole launch occurs absent parity/resources.

**D110 direct-facts bounded acceptance / full-reference prebuild checkpoint
(2026-11-01):** Implemented `direct_count_index.{hpp,cpp}` streamed numeric
shared lists/mmap reader and `direct_count_compile.{hpp,cpp}` bounded ordered
parallel compiler. Count runtime direct-only: no XG/GBWT/RI/receipt loading,
immutable scorer spans/local-ID sidecars, assignment-only cache planning. Final
174/174 passed 31.83 s (`build/direct-facts-ctest.log`, total line 410); focused
12/12 8.18 s (`build/direct-facts-focused.log`); build log
`build/direct-facts-build.log`. First suite's four old UMI PYTHONPATH misses were
rerun in Docker with existing PYTHONPATH/no install; final all passed. Frozen
count hashes unchanged; thread/profile 1--128 parity, serial/4 byte-identical
compile, exhaustive node/splice oracle and hidden graph-input count tests pass.
Root `pancollapse_direct_facts_20261101_UKcX1N` records inputs and frozen
92,908,360-B binary, 596,678-B `source-direct-facts.tar.gz`, copied three logs;
`RUN.status=bounded-tests-passed-awaiting-reference-build`, no empirical job.
Next authorized: full compile <=128 threads/300e9-B/no swap, quantify size/build
resources, then same frozen 1M exact N comparison. No whole launch or speed/F1/
full-genome claim.

**D110 historical direct-reference empirical launch (2026-11-01):** Reviewed documentation
checkpoint above before launch. Container
`pancollapse-direct-index-20261101-ukcx1n-a2` started at
03:07:16.644025201Z, hard 300e9-B/no swap, 128 CPUs; constructor entered load.
`RUN.json`, `index.log` and `index.time.txt` in the same direct-facts root are
operational evidence only. First wrapper lacked `/usr/bin/time` and exited 127
before construction; retained log, retry uses Bash timing without install.
Full index and 1M parity remain pending; no whole-count launch.

**D110 empirical continuation:** Compiler load completed in 198.368 s and node
compilation began. `after-index.sh` in the frozen direct-facts root waits for
successful compiler exit before launching only the frozen 1M count under the
same cap/threads, independent Parquet/MEX validation, and exact N byte/logical
comparison. `continuation.log`, `run-count.sh` and copied parity helpers retain
the commands. This is the previously authorized empirical engineering gate,
not a new biological policy or whole-count promotion.

**D110 A2 full-reference direct-facts build failure / retry checkpoint
(2026-11-01):** A2 exited 1/no OOM at 2026-11-01T03:12:31.792628943Z after
314.924-s wall/198.368-s load at last 1M graph-node slots (not reads): `worker
ordered-batch payload exceeds memory budget`. Peak 132,390,883,328 B versus
300e9-B hard cap; memory max/oom/oom_kill all 0, swap 0. RAII removed incomplete
regenerable temporary postings; no index published, inputs/controls changed, or
count started; `after-index.sh` exited 1 correctly. Terminal evidence:
`R/pancollapse_direct_facts_20261101_UKcX1N/{index.log,index.time.txt,index-resources.log,continuation.log}`.
Bounded 174 remains valid but does not establish full build. Sol fixes fixed
256-item capacity with adaptive/flush batches preserving targets/facts: no cap
raise/spill/hash/policy change. Next regression/full suite, freeze retry, full
compile 300e9-B/no-swap/128-core, then 1M N parity only after success; no whole
production promotion.

**D110 direct-facts batch-fix retry acceptance checkpoint (2026-11-01):** Fixed
compiler full 174/174 passed 30.92 s (`build/direct-facts-batch-fix-ctest.log`,
lines 351/410); `direct_count_compile_test` regression 3.11 s (line 110); build
log `build/direct-facts-batch-fix-build.log`. Sol replaced fixed 256 result
batches with ordered waves: one completed Result/actual OpenMP worker, stable
ascending serial commit; Writer/candidate budgets unchanged, other memory
rebalanced to one-result scratch/retention. 48-B cap regression fits nodes but
not queued dense results, serial artifact byte-identical. CLI/profiles/biology
unchanged; barrier overhead remains measured risk. Root reviewed barriers,
exceptions/limits; files frozen. Retry root
`pancollapse_direct_facts_batchfix_20261101_1REM6g`; failed A2 root preserved.
No job yet. After review: same 300e9-B/no-swap/128-core compile; `after-index.sh`
gates frozen 1M count and N byte/logical Parquet/MEX validation on success only.

**D110 active whole-objective / live retry checkpoint (2026-11-01):** User
reactivated 10/30 whole-GAMP per-gene counts <2h wall/<300GB. Existing 174/174
batch-fix and live retry are progress gates, not success. At 03:33:47Z Docker
`pancollapse-direct-index-20261101-batchfix` was verified running PID 1265651,
started 03:19:24.28115287Z. Load 194.834 s; 8M/116,844,907 graph-node slots;
partial postings ~68 GiB rounded; cgroup current/peak
151,776,280,576/151,778,562,048 B, max/oom zero. No index/count parity yet.
After successful direct reference <=300GB: exact 1M N parity/resources, optimize
if needed, then full same-GAMP count/full-scope validation. Reactivation changes
no biological policy, remapping, or input contract; frozen controls remain.

**D110 batch-fix retry launched after checkpoint review:**
`pancollapse-direct-index-20261101-batchfix` started at
2026-11-01T03:19:24.28115287Z, entered load; verified 128 CPUs and 300e9-B
hard cap/no swap. `RUN.json` in the batch-fix root is live authority;
`after-index.sh` waits for successful exit then performs the previously
authorized frozen 1M exact comparison. No full index/parity/speed result yet.

**D110 R2/R3 post-change reconciliation (2026-11-01):** R2 live builder is
unchanged frozen batch-fix binary. At 03:47:30Z: 17M/116,844,907 slots, partial
postings 157,543,858,176 B, cgroup current/peak
240,284,389,376/240,287,301,632 B, max/oom zero; count not started. Missing
turn-scoped waiter was restored, without index restart, by `nohup setsid --fork`:
bash PID 1297730/PPID 1/SID 1297730 and live docker-wait 1297737; R2 RUN records
recovery. Full input remains tracked
`.../mpmap_pbmc1k_v3_20261027T122300Z/attempt5/full/alignments.gamp`
(30,326,279,277 B); process receipt/input stats/stderr EOF, not absent stale
COMPLETE citation, establish 10/27 producer completion; prefix receipt establishes
1M source.

R3 `pancollapse_compact_assignment_key_20261101_TN29f2` is frozen/prepared only:
`count_assignment_signature.hpp` typed visitor/size pass/canonical unsigned
varint, one output string, no per-record/Subpath SerializeToString; scorer fields
retained, unused score/edit/base/metadata omitted. No profile/score/UMI change;
174/174 30.79 s (`build/assignment-signature-ctest.log` 351/410), build log
`build/assignment-signature-build.log`; real parity/speed pending. Sorted
background terminal receipt has 0/0/0 exits, 66,601,887 groups, 166,824,266
records, 282,861,084,610 payload bytes pack/unpack, sorted 22,935,754,026 B;
opaque totals only, no multiset proof/adoption. R2 feasibility probe is bounded
16MiB-prefix ZSTD/fixed-row only, no final size estimate/reserve change. Next R2
index+1M N, R3 1M parity/speed, optimize if needed, then full EOF under limits.

**D110 exact-storage feasibility checkpoint (2026-11-01):** Active objective is
66,601,887 full GAMP groups <2h/<300e9 B. At 03:51:32Z R2 was live, frozen
binary/source unchanged, nodes 19M/116,844,907/no counts; detached waiter
1297730/1297737 live 14 minutes. R3 compact varint signature passed 174/174 in
30.79 s, real data pending. Next authorized work: exact storage-size feasibility
and, if necessary, compact read-only representation only—not biology/policy or
planner relaxation. Existing 16MiB-prefix compression receipt and
`storage-feasibility.json` are indicative prefix only; no rerun. Objective unmet.

**D110 compact V2/repack empirical prelaunch checkpoint (2026-11-01):** Exact
V2 codec/repack implemented: `count_reference.hpp` PostingView,
`direct_count_index.{hpp,cpp}` V1/V2 body factoring, resolver range iteration;
nonbody nonzero/conflicting body support fails publication. Profiles/scoring/UMI
unchanged. Full 174/174 31.46 s (`build/compact-direct-index-ctest.log` 351/410),
focused C++ 3/3 2.00 s. CLI/repack/native verification covers canonical/
no-overwrite/invalid/count equality. R4
`pancollapse_compact_direct_facts_20261101_FAuuqd` frozen/prepared only:
93,109,696-B binary, 566,500-B archive, RUN/logs/repack/count scripts.

R2 24M/116,844,907 has 237,611,110,572-B partial postings, not final size;
memory limit includes cache: max 20/oom 0/oomkill 0, anon 77.9GB/file 215.5GB.
After review replace only host waiter 1297730/1297737, then R2 success -> exact
R4 repack <=300GB -> one combined 1M exact-N parity/resources gate, consolidating
R2/R3 pilots. Only then rate/full original-GAMP under active target. No whole
launch/final-fit/speed claim/hash.

**D110 R4 caller-profile prelaunch checkpoint (2026-11-01):** At 04:07:45Z R2
live 29M slots; R4 waiter 1381666/1381670 confirmed. Existing P exact-N 1M
terminal capture remains control: 60-s/3-Hz DWARF, 22,090 zero-lost/empty,
RI-node/splice/resolver-other 59.31/5.81/31.64%; no redundant fixture. If needed
to diagnose R4 direct-counter cost, prepare one fail-open external 60-s
low-frequency caller profile of R4 first-1M using existing setup, profiler
8GB/4CPU (accepted P configuration); no host sysctl, binary change, extra read run, policy/input/profile/
counter-matrix change. Full objective timing unprofiled; prefix rate profiled and
nonisolated only. No full raw-count admission: 271GB >270GB proof. Root review
before installation.

**D110 late-winner hotpath candidate checkpoint (2026-11-01):** R4 remains
frozen 174/174 control; queued repack/1M exact-N gate and optional caller capture
unchanged. Helpers 8 tests, corrected mocks, and pinned-image perf/Python
preflight passed. Observer 1602210 (PPID 1/SID 1602210, 04:17:35Z) waits only for
R4 count, separate from 1381666/1381670 and R2 PID 1265651; no capture. Sol
completed proof; root reviewed minimal `node_score_resolver.cpp`/DP-test edits:
lazy WinnerKey creation omits only no-score keys and retains scored missing-policy
winners, ties/negative scores/support exactly. Build exited 0; focused 2/2
passed 0.20 s and full 174/174 passed 31.22 s (`build/late-winner-{build,
focused-ctest,ctest}.log`). Frozen R5
`pancollapse_late_winners_20261101_wc5G9s` holds a 93,081,344-B binary,
566,996-B archive, RUN/logs; eager oracles/R4 unchanged. R5 parity/speed are
unmeasured, and there is no job/script: use only after R4 rate/profile establishes
need. R2 now 40M/116,844,907/no OOM. No policy/input/hash or whole-count launch.

**D110 raw-V1 admission ruled out (04:06:42Z):** Live R2 at 28M node slots had
271,496,812,248 B of partial raw postings alone. This exceeds 300e9 minus the
unchanged 30e9 count safety reserve, even without other reference/resident
data. Do not attempt raw-V1 full count or relax admission; queued exact V2
repack remains the route, actual V2 final size unknown. The 37-s retained
`resource-observation-20261101T040409Z.json` in R2 records CPU-active construction
(67.2 cores) and clean-file reclaim, not a storage-bandwidth or count-speed
claim. R2 and detached R4 waiter were verified live; no restart or policy change.

**D110 administrative pause checkpoint (2026-11-01):** User requested the
autonomous goal pause while external R2 compilation and R4 repack/1M continuation
remain live; no count/index stop or replacement. At 04:29:05Z R2 PID 1265651 was
42M/116,844,907/no OOM with 397,631,746,048-B partial postings; waiter
1381666/1381670 and observer 1602210 recently confirmed. R5 stays tested/frozen,
unlaunched. The <2h/<300e9-B objective and all evidence/gates persist. The latest
user direction supersedes hourly checks/06:31Z resumption: wait 30 minutes between
updates and resume goal work when the completed index is confirmed. No durable
native scheduler/task is created and monitoring ends with the session.
At 04:34:50Z R2 was 44M/116,844,907/no OOM; waiter
1381666/1381670 and observer 1602210 live.

**D110 compact empirical-gate checkpoint (2026-11-01):** Raw R2 completed exit
0/no OOM (03:19:24–10:43:17Z): 116,844,906 nodes; 50,822,105 lists;
91,593,453,719 postings; 95,130,819 models; 1,801,422 splice pairs;
100,138,278 targets; 3,602,844 junction rows; 200,276,556 owners; and
1,104,794,294,344 B (`R2/index.log`; node/edge/write/total
11129.8/9363.22/5848.09/26535.9 s). User condition to continue is met, but tool
goal flag remains paused. R4 PID 1381666 auto-launched live compact repack
`pancollapse-direct-repack-20261101-fauuqd`; ~65-GiB encoded postings at 11:10Z,
no count yet. Next: V2 admission <300GB, one exact-N 1M parity/resources/caller
profile, R5 only if needed, then full original GAMP. Rules unchanged; observer
1602210 live. No speed/full-result claim.

**D110 total-memory compact-count acceptance checkpoint (2026-11-01):** Goal
status verified active at 11:31Z. R4 repack remains live (encoded spool
119,160,709,126 B; no count); do not restart. Acceptance is lossless V2 plus
total count memory <300e9 B/no swap/no spill: actual V2 size, loaded metadata,
cache/runtime and headroom, then one exact-N 1M parity/resources/caller-profile
gate and only then full 66,601,887 groups <2h/<300e9 B. N's 2,930,386,935-B
assignment cache (691,075 entries/280,465 hits) is accounting evidence only.
Historical D110 means shared lists/raw pairs/persisted facts and V2 support
factoring/deltas with per-read local groups; global model collapse is not
implemented or established by the historical plan. Further representation
changes must be lossless, not biological-policy changes. R5 remains
frozen/unlaunched; rules unchanged.

**D110 R6 RSS memory-guard candidate (2026-11-01):** Sol proof review
accepted new R6, combining late-winner changes and the exactness-preserving guard;
R4 frozen repack/1M queue
is untouched (spool 133.8GB at 11:37:44Z, final size unknown). Keep mapped-file
admission; periodic RSS reader at target minus existing 10% reserve permanently
disables retention and clears cache under one shard lock at a time; shared leases
remain valid, then best-effort `malloc_trim`; later pressure invokes existing
clean stop. Snapshot/destroy cache before finalization and guard EOF residency.
No LRU/refill/arbitrary ceiling/model drop/cap raise or rule change. This is not
a hard OOM proof: full-run RSS/HWM+cgroup peak <300e9, zero OOM/swap/no spill are
required; `memory.events:max` is reclaim-tolerant. Mounted `/usr/bin/time`
preflight (64MiB host-Python exit 0, 77,824KiB peak) supports future native HWM
measurement only. R6 focused 8/8 passed 7.22 s and full 174/174 passed 31.92 s
(session 89139 exit 0). Frozen
[`R6/RUN.json`](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_memory_guard_20261101_O70tyk/RUN.json)
records 93,141,152-B binary/495,153-B archive; binary/source compares, Docker
RUN validation and `run-count.sh -n` passed. Its authorized same-index 1M job
has reached EOF but awaits terminal validation below. R5 remains late-winner-only.

**D110 R4 repack/count distinction (2026-11-01):** At 11:58Z R4 remained live;
161,711,487,246-B spool unchanged since 11:49Z, no final `direct.pci`/count and
phase unknown. Its repack constructor showed ~318GB proc RSS (anon ~9.09GB,
file ~308.95GB); shared mapped-file versus 300GB memcg accounting is not count
admission proof. Compact size then same-1M exact-N parity/resources/caller
profile is the first empirical gate; R6 follows only if needed.

**D110 read-only performance-review gate (2026-11-01):** User authorized twelve
distinct pending reviews of direct-index speed/memory, alternate approaches,
cache hits, and empirical proof against the old same-1M control. At most two
reviews run concurrently. This is no biological/policy change and no review-led
implementation without root synthesis. R4 remains untouched with queued 1M;
R6 stays under prior authorization.

**D110 completed direct-index review synthesis (2026-11-01):** The twelve
read-only reviews are recorded in
[`direct-index-performance-review-20261101.md`](research/direct-index-performance-review-20261101.md).
Priority is unchanged R4 benchmark, then read-local reuse/allocation removal,
conditional measured cache-key work, and structural sharing only as fallback.
Root reviewed and accepted this priority order; no proposal is automatically
adopted, and there is no measured speedup or policy change from these reviews.
At 11:55:16Z R4 was live without final `direct.pci`/count; postings
161,711,487,246 B unchanged since 11:49:02Z (phase unknown), thus no fit/speed
claim. R6 is tested/frozen but unlaunched; R4/R5 unchanged.

**D110 R4 terminal compact 1M gate / R6 prelaunch checkpoint (2026-11-01):**
R4 repack exited 0 at 12:20:20Z (5,822.268 s real), final index
167,968,148,664 B. Its queued 1M count/validators exited 0 at 12:24:11Z: native
wall 200.040151 s (init/read/finalize 6.694304950/178.824724895/2.478044831),
1M groups/2,502,433 records/982,842 counts/1,302,814 molecules; four Parquet/six
MEX byte, logical/profile/prior parity against N passed (`PARITY_PASSED`). Peak
memcg 126,853,758,976 B, zero OOM/max/swap/spill; 5-s RSS is not kernel HWM.
Cache 674,931 entries/296,609 hits/1,009,589,118 charged B is diagnostic only.
Profile valid 22,474 samples/60.435917 s/zero loss-empty, 0 FastLocate, 97.477%
resolver-other/0.049% splice. ~5,592 groups/s prefix is not a full-run falsifier:
cache warmup asymptote unknown and hits include duplicate builder losers. R6
started 12:38:01Z and reached EOF by 12:40:25Z at 1M/2,502,433/982,842/1,302,814
group/record/count/molecule totals, but validator/resource terminal evidence is
pending, so it is not passed. On exact-N parity, GNU-time HWM <300e9 B and zero
OOM/swap/spill, launch frozen R6 on original 66,601,887 groups/166,824,266
records/30,326,279,277 B at 128 threads/300GB/no swap/no spill, unprofiled except
native GNU time/progress/cache/RSS guard. Do not defer for prefix microchanges;
safe >2h continues. Only terminal wall/HWM/memcg/EOF/output validation decides.

**D110 R6 1M terminal / frozen full launch (2026-11-01):** Prefix exited 0/no
OOM 12:38:01.593650971–12:40:55.325512877Z: GNU time 143.57s/native 143.580467s
(init/read/finalize 6.190571497/126.821282846/2.826345579), 7,885.111848 groups/s,
kernel HWM 77,308,772KiB/79,164,182,528B and memcg peak 28,207,353,856B, zero
max/OOM/swap/spill. Exact N four-Parquet/six-MEX/logical/profile/prior parity
passed (982,842 rows/1,302,814 molecules); validated receipt is
`R6/first1000000-terminal.json`. Root launched same frozen full original GAMP at
12:42:36.558147793Z: `pancollapse-memory-guard-full-20261101-o70tyk`, ID
`a3daffbffed697ad2b274767241592c5bc6d832305b5b06a25a031af4f08a04c`, host PID
1071618, 300e9/128 quota, no profiler/input/rule change; ~220k/28.0s/zero OOM-swap.
Goal remains unproven until full terminal wall <7200s, HWM+memcg <300e9, zero
OOM/swap/spill, EOF and outputs validate; safe >2h continues.

**D110 R6 full native EOF / R7 preimplementation checkpoint (2026-11-01):** R6
native completed 0: GNU time 7,305.22s/native 7,305.224792s (2h1m45.22), kernel
HWM 240,016,211,968B, memcg peak 166,879,653,888B, zero max/OOM/swap/spill.
Native epochs 1793536956.764815–1793544261.989607; EOF 66,601,887 groups/
166,824,266 records, barcode prior 66,601,887/exact 63,212,921, high complexity
62,198, 11,327,565 count rows/29,287,473 molecules, 290,648 active barcodes,
36,603 features. Read/finalize were
7117.351268304/130.084563239s; cache 30,381,813 entries/34,632,404 reported
hits/59,792,689,563 charged B, EOF-clear only. Outputs are published but Python
validation remains running: no terminal full-output claim. <2h misses by 105.22s
(~1.46%); RAM passes, goal not achieved. Authorized R7 is only read-local decoded
Posting descriptors plus cached `has_annotation` in `node_score_resolver.cpp`,
preserving model IDs/support/flags/order (8B/touched posting versus 4B local ID).
After focused/oracle, root full 174, same-1M exact-N parity/RSS, then full versus
R6. No index/layout/input/biology or UMI-parallelization change.

**D110 R6 terminal validation / R7 pre-prefix checkpoint (2026-11-01):** R6
container exited 0 at 14:48:07Z; both independent full Parquet/MEX validations
passed and `R6/full-terminal.json` validates. Counts are `R6/full/counts`, four
Parquet 295,286,773 B. RAM pass/speed miss (105.22s) remains the full result.
R7 only changes `node_score_resolver.cpp`/DP test: numeric `LocalPosting`
(local model/support/flags) and exact cached `has_annotation` replace repeated
membership decompression, retaining IDs/support/flags/order (8B touched posting
versus 4B old local ID). Annotation-free/repeated-node/body-support two-record,
two-profile oracle passed; build 0, focused 2/2 .24s, full 174/174 32.09s;
independent oracle/eager/goldens unchanged. Frozen
`pancollapse_posting_reuse_20261101_nf6RG9` has 93,112,944-B binary/495,380-B
archive; comparisons and `run-count.sh -n` pass. No job: next same R4 index/1M
exact-N parity/RSS 300GB/128 threads, then full four-Parquet/six-MEX vs R6.

**D110 R7 1M terminal / pre-full checkpoint (2026-11-01):** R7 prefix exited
0/no OOM 14:59:01.188515981–15:01:35.253471293Z: GNU 123.64s/native 123.646394s,
init/read/finalize 6.025417827/107.214604235/2.642745386, 9,327.087547 groups/s,
kernel HWM 81,771,417,600B/memcg peak 30,953,394,176B, zero max/OOM/swap/spill
and EOF cache-clear only. Exact N four-Parquet/six-MEX/logical/profile/prior
parity passed at 1M/2,502,433/982,842/1,302,814 group/record/count/molecule
totals (`PARITY_PASSED`); root is writing terminal receipt. After review, full
uses same original GAMP/R4 direct index/profiles/filters, 300GB/128/no swap/spill,
unprofiled GNU time, then independent validators and exact four-Parquet/six-MEX
byte/logical/profile/prior comparison to R6. Strict goal requires full native
<7200s, HWM+memcg <300e9, zero OOM/swap/spill, EOF, outputs and parity; prefix
rate is not proof. No other optimization authorized.

**D110 R7 full native terminal / validation-pending follow-up (2026-11-01):**
R7 native exited 0 by 16:52:28Z: `full/native-resource.txt` records 6,372.41s
(user/system 758,308.28/1,942.66s), kernel HWM 239,408,108KiB/245,153,902,592B;
`timing.tsv` spans 1793545492.809295–1793551865.226191. Memcg peak
128,493,842,432B; max/OOM/oomkill/swap zero; manifest spill/evidence spool absent
and cache clear EOF-only. EOF is 66,601,887 groups/166,824,266 records,
11,327,565 count rows/29,287,473 molecules, 290,648 barcodes/36,603 features;
CR7/panSC NNZ 5,533,452/5,794,113, priors 66,601,887/exact 63,212,921, high
complexity 62,198; init/read/finalize 6.512723941/6185.877905775/129.235389815.
Native time/RAM gates pass. Counts are published at `R7/full/counts`, but
container validation (Parquet, MEX, full R6 four-Parquet/six-MEX byte/logical/
profile/prior parity) remains live; no goal-complete or full-parity claim until
those pass and terminal receipt is written.

**D110 R7 full terminal validated / objective achieved (2026-11-01):**
[`R7/full-terminal.json`](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_posting_reuse_20261101_nf6RG9/full-terminal.json)
was audited 17:03:05Z after container/native exit 0 (container 16:59:37Z).
Original 30,326,279,277-B GAMP reached all 66,601,887 groups/166,824,266 records.
Native wall 6,372.41s (<7200; 827.59s headroom), kernel HWM 245,153,902,592B and
count-memcg peak 128,493,842,432B (<300e9; 54,846,097,408-B HWM headroom), zero
max/OOM/oomkill/swap/spill. Full validation/MEX passed; R6 parity passed with
all four Parquet and six MEX bytes plus decoded/logical tables, profile contracts
and complete/exact-whitelist priors identical. Counts are
[`R7/full/counts`](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_posting_reuse_20261101_nf6RG9/full/counts):
11,327,565 count rows/29,287,473 molecules, 290,648 barcodes/36,603 features.
All twelve reviews are complete; only R7 read-local reuse and authorized R6 work
implemented. No F1/biological-truth/external-CR7-workflow equivalence, release,
or isolated causal-speedup claim follows.

## D111 — Measured count-preserving optimization phase (2026-11-01)

**Decision source: User. Status: implementation/measurement authorized after
baseline commit; no biology or release decision.** Baseline `c408857` is clean
(119 files); root verified frozen R7 source archive by `tar --compare` and
`build/src/panCollapse` by `cmp`. R7 terminal remains the preserved oracle:
66,601,887 groups/166,824,266 records, 6,372.41s, 245,153,902,592-B HWM,
167,968,148,664-B index, all 174 tests and exact R6 four-Parquet/six-MEX parity.

Authorized order: fresh bounded R7 profile plus aggregate-only cache saved-work
and ingest metrics; read-local scratch allocation removal; independent buffered
index writer; conditional cache canonical keys only after original complexity cap
and measured duplicate work, then per-key singleflight only if justified.
Metadata mapped views/accounting, exact full-byte QNAME guard, conditional exact
observation multiplicity, bounded complete-group locality, and structural
factorization/SIMD/grammar are evidence-gated fallbacks. Per patch requires
oracle/CTest; consolidate exact 1M R7 parity, then one full original GAMP under
<7200s/<300e9/no OOM/swap/spill. Diagnostic counters may differ, counts may not.
Revert unhelpful changes. No scoring/biology/UMI/input XG-GAMP-RI/spill/release
change. Root solely runs Ninja; at most two children active.

**D111 64-core target / fresh-profile follow-up (2026-11-01):** User clarifies
the optimization endpoint as <2h on 64 cores (96 intermediate), <300GB and exact
R7 counts. R7 user+system CPU was 760,250.94s; 64*7200=460,800, requiring at
least 39.4% less CPU work before overhead—not a predicted wall speedup. In
`pancollapse_remaining_optimizations_20261101_gGNqCQ`, frozen R7 fresh-profile
count exited 0 with exact 1M R7 parity: native 107.48s/HWM 80,584,964KiB;
profile timing is not a causal unprofiled A/B. Caller sample 23,036/61.562883s,
zero lost/empty: resolver-other 96.4273%, splice 0.05209%; no-children self
resolve/posting-decode/model/malloc/free 35.44/9.41/4.99/3.89/3.29%. Fractions
are not additive. 64KiB buffered TempFile tails/read-your-writes boundary patch,
Delta scratch patch, and aggregate cache duplicate-build/ingest metrics are
implemented but unbuilt; next root build/CTest then 64/96/128 prefix comparisons.
No scientific count change.

**D111 A/B measurement and C preimplementation checkpoint (2026-11-01):** In
`pancollapse_remaining_optimizations_20261101_gGNqCQ`, A passed full 174 (32.45s),
exact 1M R7 parity and 64-core prefix: native 141.03s/7,690.11 CPU-s/HWM
68,703,408KiB versus frozen R7 same-64 197.86s/11,367.23 CPU-s (~29% wall/~32%
CPU). This shared-host prefix is not whole/64-core goal proof. A metrics report
10,469 duplicate builds/685,400 completed (1.53% build count, not CPU cost),
286,140 ready hits, and ingest zero fallbacks/854,778 full-queue submissions.
B adds only exact sorted/deduplicated length-framed cache records after original
cap, never scoring records; Sol accepted max/tie-union proof, focused 3/full 174
passed, 1M/64 pending. C is conditionally exact: expose existing direct
`NodeSlot.list` via optional source-local `shared_membership_id`, no file/index/
GAMP/XG/RI change; cache per-read decoded memberships only, retaining every node
length/ID/incidence/credit/strand/splice check. Missing-source fallback is unique;
ID 0 valid. Require contract/direct shared-vs-missing oracles then 64-prefix
byte parity/CPU/RAM before promotion. No full-64 claim.

## D112 — V1 direct-index cleanup integration gate (2026-11-01)

**Decision source: User. Status: manual integration authorized; active-tree
validation pending.** Source is uncommitted isolated worktree
`/mnt/ssd/lalli/panCollapse/.codex/worktrees/direct-index-v2-only-20261101` and
supplied `remove-v1-direct-index.patch` in
`pancollapse_v2_only_validation_20261101_Az1LNV`. Scope removes only PCDIRECTV1
reading/decoding, repack/upgrader API/CLI and obsolete V1/repack fixtures/tests.
Keep compact V2 count, PCANIDX2/3 and native annotation preparation. Isolated
evidence is bounded: `RUN.json` `validated-v2-only-cleanup`, `parity.json` pass
at expected 1,000,000 with `changed_counters={}`, and `validation.json` passed
(982,842 count/1,302,814 molecule rows); it does not validate active D111.

Root manually resolves index/index-test overlaps, preserving D111 64KiB writer
boundaries and partial C shared-ID API; C is paused. B is rejected after exact
1M/64: 153.54s/8,524.55 CPU-s vs A 141.03s/7,690.11, only 765 keys saved
(674,166 vs 674,931); key header/native-query test restored. Required active
gates: full build, 174 CTest, consolidated 1M exact R7 parity. No 168GB rebuild
or cleanup-only whole run; jobs/frozen A/B/R7/indexes remain untouched.

**D112 integration complete / 64-core full prelaunch (2026-11-01):** Root
manually integrated the six-file cleanup patch, preserving D111 A writer tests/C
sharing; PCANIDX2/3 retained, no 168GB rebuild/index-byte change. Candidate C64
exited 0 with `PARITY_PASSED`: native 123.46s, CPU 6,514.29+82.56=6,596.85s,
HWM 66,413,420KiB, memcg 17,943,756,800B, zero memory max/OOM/swap; full 174
tests 31.25s (`candidate-c/d112-candidate-c-ctest.log`). Four Parquet/six MEX,
logical/profile/prior parity against R7 passed. Shared membership reuse was
3,327,984 lists/8,837,666,038 posting rows versus 8,662,357/16,616,919,983
decoded; this is single-prefix evidence only. Next is the already authorized
full original 66,601,887-group 64-core gate <7200s/<300e9/no spill/swap with
exact full R7 parity, not a cleanup-only run.

**D112 committed launch-state reconciliation (2026-11-01):** Active clean HEAD
`3b4c76f54b708c94b2c36e48eede9ef4f3bc3807` is the exact full-gate source;
`RUN.json` records candidate-C plus V2-only cleanup 174-test/exact-million-pass,
and `v1-cleanup-integration.json` records active-tree validation. C receipt
confirms preserved A/C, B absent, and same-64 baseline comparison; cleanup
receipt confirms no index rebuild and no cleanup-only whole run. Thus one already
authorized full original 66,601,887-group 64-core <7200s/<300e9/no-swap/no-spill
measurement with exact full R7 parity is next. This is the only full gate; it is
not launched or proven. Singleflight, metadata views/accounting, full-byte QNAME,
multiplicity, locality, structural factorization, SIMD and grammar remain
evidence-gated, not implementation commitments.

**D111 full64 launch (2026-11-01 18:10:54Z):** After root reviewed the fresh
documentation checkpoint, frozen candidate C source/binary matched active
production code at `3b4c76f`. The one optimization full gate is running in
`pancollapse-d111-candidate-c64-full-20261101-ggnqcq` (`b09511c5c7d0`) with 64
threads/CPU quota, 300e9 hard RAM/no swap, unchanged original GAMP/index and
both profiles/MEX. `gGNqCQ/candidate-c64-full-launch.json` retains the input,
resource and output identities. Startup 160k/20.1481s and zero max/OOM/swap do
not establish full performance or parity. No rebuild, input hash, extra
cleanup-only whole run, profiling, or scientific-policy change.

**D112 full64 terminal reconciliation (2026-11-01):**
[`candidate-c64-full-terminal.json`](/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_remaining_optimizations_20261101_gGNqCQ/candidate-c64-full-terminal.json)
audits native exit 0 and container exit 0 (20:46:08Z) after original 66,601,887 groups/
166,824,266 records. Native 8,770.10s (2h26m10.1) misses the 64-core <7200s
target by 1,570.10s. Memory/parity pass: HWM 225,357,627,392B, memcg
184,388,919,296B, zero max/OOM/swap/spill; `parity.json` expected 66,601,887,
`changed_counters={}`, independent Parquet/MEX pass, and exact R7 four-Parquet/
six-MEX byte/decoded/logical/profile/prior parity. Counts are 11,327,565 rows/
29,287,473 molecules. CPU 534,792.51s vs R7 760,250.94s is ~29.66% lower but
not an isolated causal claim across differing threads/shared host. Prior 128-core
goal stays historical achieved; 64-core goal failed. Future targeted profiling,
scratch reuse or 96-core comparison are evidence-gated/unlaunched; no automatic
full rerun or new phase authorization.

**D112 64-core feasibility/comparator question (2026-11-01):** Read-only
assessment requested at HEAD `5003be0` (source `3b4c76f`). Terminal C64 cost is
534,792.51 CPU-s = 148.553475 CPU-hours for both count profiles including MEX;
wall 8,770.10s is 1,570.10s/21.8% over target and needs 17.9% less wall time
(1.218x throughput). Memory/parity remain accepted.
No fresh C profile exists: the available fresh profile predates A/C, so it cannot
assign the remaining gap or prove 64-core feasibility. STARsolo comparison is
unverified until a matched receipt establishes inputs, threads, stages and output
scope; no CPU-hours difference is claimed. Authorized next work is read-only
profile/receipt interpretation only, not code, run, or new phase.

**D112 comparator receipts verified (2026-11-01):** STARsolo GRCh38/T2T
`count-resource.txt` under
`/mnt/ssd/lalli/pancollapse-wg-analysis-20261101/data/linear/starsolo-{grch38,t2t}/run/`
record native wall 455.30/469.75s and exit 0; Docker quotas were 32 cores.
Their CPU user/system fields were not recorded. Quota*wall gives approximately
4.05/4.18 core-hour upper bounds, not actual CPU measurements. Each
`Log.final.out` confirms 66,601,887 reads; commands include linear alignment,
decompression and three raw feature modes, no BAM, prebuilt indexes. C64's
148.553475 measured CPU-hours (two profiles and MEX, counting only) exceed these
bounds by at least ~144.51/144.38 hours (~36.7/35.6x CPU). Same named PBMC sample,
different graph/linear reference and assignment machinery; not output equivalence
or isolated causal efficiency evidence.

The original attempt5 `full/process_receipt.json` cpu.stat counter is scoped to
one mapping-attempt container, not a reused unrelated-job container: 446.2702
CPU-hours, but it includes a 10k pilot, helpers and validation. Full-only start
CPU delta is unavailable. Adding later C64 gives 594.8236 recorded CPU-hours for
the composed attempt lifecycle plus counting, not a same-container/full-mapping-
only benchmark. No new large-input hashes, profiling, code or runs were made.

## D113 — Current-build 64-core optimization phase (2026-11-01)

**Decision source: User. Status: bounded profiling/candidate implementation
authorized; no full run yet.** “Do it” supersedes D112's read-only comparator
restriction. Starting at current HEAD `5003be0`/production source `3b4c76f`,
profile representative early/middle/late current-C membership/scoring work; the
available fresh R7 profile predates C and cannot attribute its remaining gap.
Implement only the most promising exact-count candidate, requiring per-patch
tests, exact 1M parity, and representative windows before a gain-justified
consolidated full gate. C64 needs 17.9% less wall time (1.218x); 64*7200=460,800
CPU-s vs 534,792.51 observed requires at least 13.8% work reduction plus
overhead. No new index build; no XG/GAMP/RI input, biology/scoring/UMI/policy or
complexity-limit change; no artificial representative-family counts or spill.
R7/C64 and STAR receipts remain scoped controls, not output-equivalence or causal
claims.
