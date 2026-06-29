# Task Plan

This plan separates machine-verifiable implementation work from judgment-sensitive
architecture and biological decisions. The orchestrator owns edits and review state.
Read-only agents may investigate and recommend but do not change the repository.

## Phase 0 — External contracts and architecture proposal

**Purpose:** establish how to implement the settled product using actual VG and RAD APIs.
**Production code is prohibited in this phase.**

### P0.1 — VG library and data-contract investigation

Owner: orchestrator, assisted by `vg-integration-researcher`.

Determine from installed headers and official VG source:

- which existing graph/index files are minimally required;
- how to deserialize GAMP and access annotations, subpaths, ordinary edges, connection
  arcs, scores, orientations, and graph positions;
- which existing VG/handlegraph/GBWT facilities can support transcript compatibility;
- how a standalone CMake project should discover and link the required libraries;
- what ABI/version assumptions must be documented.

Deliverable: `docs/research/vg-integration.md`, updated with evidence, alternatives, and a
recommended integration boundary.

### P0.2 — GAMP grouping and sorting investigation

Owner: orchestrator, assisted by `gamp-ordering-researcher`.

Establish whether `vg mpmap` emits all GAMP records for a read contiguously under normal
single-end 10x cDNA operation, including multithreaded output. If not guaranteed, identify
a supported way to name-group or sort GAMP without converting through BAM.

Deliverable: `docs/research/gamp-grouping-and-sorting.md` with a clear verdict, version
scope, exact commands or API if sorting is needed, and a recommended V1 input contract.

### P0.3 — RAD writer investigation

Owner: orchestrator, assisted by `rad-format-researcher`.

Confirm from current primary sources:

- the exact mapper-style uncollated RAD header and record schema expected by alevin-fry;
- required chunking, tag definitions, orientation fields, target dictionary, and metadata;
- whether a reusable writer exists in Salmon, piscem, libradicl, or another supported
  library;
- the smallest end-to-end alevin-fry integration test.

Deliverable: `docs/research/rad-format.md` with a recommended writer strategy and a
byte-level or library-level test plan.

### P0.4 — Annotation lookup and compatibility design

Owner: orchestrator, assisted by `annotation-semantics-reviewer`.

Propose how GTF-derived exon/intron structures and existing graph/index coordinates can
implement the exact rules in `docs/compatibility-semantics.md` without a custom index.
Document expected complexity and identify measurable conditions under which a later
custom index would be justified.

Deliverable: update `docs/research/annotation-lookup-performance.md` and propose concrete
fixture cases for Phase 1.

### P0.5 — Architecture and CLI proposal

Owner: orchestrator, assisted by `build-integration-researcher` and the researchers above.

Synthesize P0.1–P0.4 into:

- a component/data-flow proposal;
- exact V1 required and optional inputs;
- a proposed CLI, diagnostics, exit conditions, and metrics;
- dependency discovery and version policy;
- explicit implementation increments;
- ADR entries for each resolved open fork.

Do not implement the proposal.

### Gate Architecture Approved

Required artifacts:

- all four research briefs contain evidence and recommendations;
- `docs/decisions.md` records new decisions and remaining open forks;
- a proposed architecture is added to `docs/architecture-proposal.md`;
- the input inventory and CLI are concrete enough to implement;
- no production source or build configuration has been created.

Stop and request review.

---

## Phase 1 — Executable behavioral contract

Begins only after passing Gate Architecture Approved.

### P1.1 — Synthetic fixture specification

Design a minimal graph/GTF/GAMP test matrix covering every compatibility, strand,
score-window, collapse, raw molecule-identity parsing, ordering, and assignment-policy rule. Expected
outputs must be expressed as transcript target sets and observable diagnostics before
fixture generation code is written.

### P1.2 — RAD interoperability fixture

Define a tiny expected RAD dataset and alevin-fry command sequence whose final gene matrix
can be checked exactly.

### P1.3 — Build/test skeleton proposal

Only now create the CMake/Ninja test skeleton approved at Gate Architecture Approved.
Keep VG-dependent and pure policy tests separable so local failures are diagnosable.

### Gate Behavior Specified

Required: fixture matrix, expected outputs, CLI snapshot, and test strategy. Stop for
review before substantive implementation.

---

## Phase 2 — Thin vertical implementation

Begins only after passing Gate Behavior Specified.

Implement one end-to-end happy path for a single name-grouped read with one compatible
canonical transcript and valid raw CB/UMI values carried in the GAMP name field. Emit RAD
and prove alevin-fry can consume it. Do not broaden behavior until this path is green.

### Gate Vertical Slice Proven

Required: reproducible build, focused tests, exact alevin-fry consumption evidence, and
an implementation review against the contract.

---

## Phase 3 — Complete V1 behavior

After Gate Vertical Slice Proven, add one independently testable behavior at a time:

- multipath traversal and score aggregation;
- intronic and boundary compatibility;
- strand modes;
- manifest collapse;
- all assignment policies;
- name-grouping validation;
- diagnostics and summary metrics;
- larger pilot and performance characterization.

Any proposal to add a custom lookup index is a separate decision requiring measured
bottleneck evidence and human approval.
