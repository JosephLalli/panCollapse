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
`genefull_ex50pas` count inflated by antisense reads: large (−)-strand genes overlapping antisense
transcription over-counted up to ~700x, e.g. PTPRT vg 26,432 vs STAR 36, the pattern universal across
the top over-counted genes -- PAK5, KIF16B, RALGAPA2, ATP9A -- every one (−)-strand and swamped by
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
chr20 Gene-mode over-count from the pre-fix 0.4.5 baseline (+83.2% vs STARsolo) to −13.2% — a large
improvement, but still biased low from the coincidental-tie cases D061 later closed. See D061's
verification for the combined, fully re-validated numbers (−2.76%).

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
(−2.76%, per-gene Pearson 0.967, per-cell 0.986) — down from D060-alone's −13.2% and the pre-D060
0.4.5 baseline's +83.2%; `GeneFull` −3.33% (r 0.999); `GeneFull_ExonOverIntron` −2.02% (r 0.998);
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
runtime, rejects convert-only options, canonicalizes `--out-dir`, and canonicalizes profile
selectors before the repeat check. The frozen profiles' outputs on the ex50 fixture are unchanged.

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
