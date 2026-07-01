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
