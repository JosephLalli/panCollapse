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
