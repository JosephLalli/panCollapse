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
