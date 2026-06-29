# Phase 2 Implementation Plan

Status: approved and implemented. This document records the approved Phase 2 direction
and the vertical-slice checks that prove it. It omits any unapproved choices.

Phase 2 begins only after **Gate Behavior Specified** has passed. It implements one thin
happy path, proves RAD interoperability with alevin-fry, and stops before broadening into
the full V1 fixture matrix.

## Approved Decisions

Each decision below is recorded with its source. Do not reinterpret these as orchestrator
choices.

| ID | Source | Decision |
|---|---|---|
| D1 | User | The Phase 2 planning artifact is `docs/phase2/implementation-plan.md`. |
| D2 | User | The vertical slice is based on the minimal happy-path combination `GRP-01`, D038 raw read-name molecule identity, `MAN-01`, `CMP-01`, and `SC-01`: one grouped read, one coherent raw molecule identity, one manifest-covered compatible source transcript, one canonical target, and one emitted record. |
| D3 | User | panCollapse extracts the uncorrected raw cell barcode and raw UMI from the GAMP name field and writes those observed/raw values to RAD. panCollapse does not correct cell barcodes or UMIs. alevin-fry performs permit-list construction/cell-barcode correction and UMI deduplication/resolution during quantification. |
| D4 | User | Phase 2 fixtures are generated under the build directory only. Synthetic GFA, GTF, manifest, and JSON GAMP inputs are converted with local `vg` tools as needed; generated XG/GAMP/RAD files are not checked in. |
| D5 | User | Phase 2 is single-threaded. Multithreading is deferred to Phase 3. |
| D6 | User | Byte-identical output comparison across thread counts is deferred to Phase 3. |
| D7 | User | USA output is not a Phase 2 feature. Phase 2 uses a two-column transcript-to-gene map and asserts a 1-cell x 1-gene matrix with `GENE_A=1`. USA output is developed only when unspliced target generation enters scope. |
| D8 | User | Phase 2 uses `--raw-cb-length 16` and `--raw-umi-length 12`. The fixture read name is `read000_AAACCCAAGTTTGGGA_AAAAAAAAAAAA`, with raw CB `AAACCCAAGTTTGGGA` and raw UMI `AAAAAAAAAAAA`. |
| D9 | User | Update the active product, input/output, architecture, validation, progress, decisions, and glossary docs to state raw read-name CB/UMI extraction, and mark older tag-centric Phase 1 barcode assumptions as superseded for V1. |
| D10 | User | Phase 2 accepts only `--strand sense`. `antisense` and `both` strand modes return a to-be-implemented error and are deferred to Phase 3, where the best implementation strategy must be researched before development. |

## PanSC Read-Name Evidence

The related PanSC fixtures and helper scripts specify barcode carry-along through the
read name before VG alignment:

- `/mnt/ssd/lalli/panSC/bin/extract_barcodes.py`: RNA biological read names are written
  as `<name>_<CB>_<UMI>`, where the RNA 10x v3 technical read is `CB[0:16] +
  UMI[16:28]`.
- `/mnt/ssd/lalli/panSC/bin/restore_cb.py`: RNA read names are parsed as
  `<name>_<CB>[_<UMI>]`, using a right split on underscores, then restored to raw
  `CR:Z:<CB>` and `UR:Z:<UMI>` tags for downstream tooling.
- `/mnt/ssd/lalli/panSC/modules/local/barcode_extract/main.nf`: documents the operation
  as carry-along transport only and states that correction is deferred to downstream
  tools.
- `/mnt/ssd/lalli/panSC/tests/fixtures/toy/make_toy_reads.py`: generates 10x v3 toy RNA
  reads with 16 bp CB plus 12 bp UMI.

D3 adopts this upstream convention for panCollapse's Phase 2 input interpretation:
panCollapse reads the raw values from the GAMP name field and writes those raw values to
RAD. It does not call the values corrected, repair them, or run a permit-list step.

## Phase 2 Scope

Phase 2 implements exactly one happy path:

1. Read one name-grouped GAMP group with one multipath alignment.
2. Extract raw CB/UMI from the GAMP name field according to the approved read-name
   convention.
3. Project one aligned block through the matching `.xg` source path.
4. Match one GTF transcript model.
5. Resolve one collapse manifest row from `(source_path_name, source_transcript_id)` to
   `TX_A` and `GENE_A`.
6. Emit one mapper-style uncollated RAD record targeting `TX_A`.
7. Emit a two-column `tx2gene.tsv` containing `TX_A<TAB>GENE_A`.
8. Run alevin-fry permit-list, collate, and quantification and assert a 1-cell x 1-gene
   matrix with `GENE_A=1`.

Phase 2 does not implement:

- cell-barcode correction or UMI correction;
- tag-based corrected/raw barcode selection from GAMP annotations;
- reuse of the Phase 1 RAD interop CB/UMI values as Phase 2 defaults;
- broad missing/malformed barcode diagnostics;
- multithreading;
- byte-identical multi-thread output comparison;
- `antisense` or `both` strand modes beyond a to-be-implemented error;
- USA output or splicing-state labels;
- GBZ/GBWT input as a replacement for `.xg`;
- custom annotation indexes;
- the full Phase 1 fixture matrix.

## Input Boundary

The vertical slice follows the recentered mapper-side contract:

- upstream `vg mpmap` may have used `-x graph.xg -g graph.gcsa -d graph.dist`;
- panCollapse Phase 2 consumes the emitted GAMP plus the matching `.xg`;
- `.gcsa`, `.gcsa.lcp`, and `.dist` are not panCollapse inputs;
- GBZ/GBWT remain future-only alternatives unless a later approved decision proves
  equivalent path-position behavior and passes the same projection fixtures.

## Minimal Fixture Shape

Fixture files are generated under the CTest build directory.

| Item | Phase 2 value |
|---|---|
| source path | `chrFixture` |
| source transcript | `SRC_A` |
| canonical transcript | `TX_A` |
| canonical gene | `GENE_A` |
| read name | `read000_AAACCCAAGTTTGGGA_AAAAAAAAAAAA` |
| raw CB | `AAACCCAAGTTTGGGA` |
| raw UMI | `AAAAAAAAAAAA` |
| raw CB length | `--raw-cb-length 16` |
| raw UMI length | `--raw-umi-length 12` |
| strand mode | `--strand sense` |
| aligned block | one reference-consuming block, expected source interval `[310,330)` |
| score | `40` |
| manifest row | `chrFixture<TAB>SRC_A<TAB>TX_A<TAB>GENE_A` |
| `tx2gene.tsv` | `TX_A<TAB>GENE_A` |
| RAD `refs` | one target ID, the header index for `TX_A` |
| RAD `dirs` | one synthetic-forward orientation value corresponding to the `TX_A` target ID |
| expected quant matrix | one cell by one gene, `GENE_A=1` |

The GTF should contain one source transcript model sufficient for `CMP-01` exonic
compatibility. The exact sequence content should be as small as possible while preserving
the projection and coordinate checks already proven by the Phase 1 VG smoke.

## Agent Work Packages

Agents should receive only the documents needed for their slice.

| Work package | Agent type | Required context |
|---|---|---|
| VG/GAMP/XG fixture generation | explorer or worker | `docs/phase2/implementation-plan.md`, `docs/research/vg-integration.md`, `docs/phase1/build-test-plan.md`, `tests/vg/gamp_xg_gtf_projection_smoke.cpp`, `tests/vg/gamp_projection_fixture_writer.cpp` |
| Raw read-name CB/UMI extraction | worker | `docs/phase2/implementation-plan.md`, PanSC evidence paths listed above, and the code file that owns GAMP record parsing |
| GTF, manifest, and compatibility happy path | worker | `docs/phase2/implementation-plan.md`, `docs/compatibility-semantics.md`, `docs/phase1/compatibility-fixtures.md`, `docs/phase1/policy-fixtures.md` |
| RAD writer and alevin-fry check | worker | `docs/phase2/implementation-plan.md`, `docs/research/rad-format.md`, `docs/phase1/rad-interop-fixture.md` |
| CLI and diagnostics surface | worker | `docs/phase2/implementation-plan.md`, `docs/phase1/cli-run-contract.md`, `docs/input-output-contract.md` |
| Skeptical review | oracle | `AGENTS.md`, `PROGRESS.md`, `TASKS.md`, `.agent-workspace/GATES.md`, this plan, `docs/product-spec.md`, `docs/input-output-contract.md`, `docs/validation-contract.md`, `docs/decisions.md`, and the relevant diff |

Use low-cost explorer agents for filesystem inventory and source lookups. Reserve the
oracle role for skeptical review of judgment calls, contract contradictions, and gate
readiness.

## Implementation Sequence

Do not start this sequence until the gate state permits Phase 2 implementation.

1. Add the smallest production entry point and parsing path needed for one read group.
2. Add raw read-name CB/UMI extraction and validate parsed values against the configured
   raw CB and UMI lengths.
3. Reuse the Phase 1 build-dir-only VG fixture pattern to create GFA, XG, GTF, manifest,
   and JSON GAMP inputs under the test build directory.
4. Implement the one-transcript compatibility and manifest-collapse path.
5. Emit `map.rad` and `tx2gene.tsv`.
6. Run the supported alevin-fry sequence with one thread.
7. Assert the expected 1 x 1 matrix and stable basic diagnostics.
8. Send the implementation diff to the oracle before treating Gate Vertical Slice Proven
   as satisfied.

## Verification Commands

Expected local verification, adjusted only if the build directory names change:

```bash
cmake --build build-pure
ctest --test-dir build-pure --output-on-failure
cmake --build build
ctest --test-dir build --output-on-failure
./scripts/verify-workspace.sh
```

The Phase 2 alevin-fry smoke should follow this shape with one thread:

```bash
rm -rf af_permit af_quant
alevin-fry generate-permit-list -i pc_out -d fw -o af_permit -f 1 -t 1
alevin-fry collate -i af_permit -r pc_out -t 1
alevin-fry quant -i af_permit -m pc_out/tx2gene.tsv -o af_quant -r cr-like -t 1 --dump-eqclasses
```

Required assertions:

- `pc_out/map.rad` exists.
- `pc_out/tx2gene.tsv` has exactly `TX_A<TAB>GENE_A`.
- RAD `cblen=16` and `ulen=12`.
- the single RAD record contains `bc`, `umi`, `refs=[TX_A_ID]`, and one corresponding
  synthetic-forward `dirs` entry.
- alevin-fry completes `generate-permit-list`, `collate`, and `quant`.
- the quantification output has one cell row, one gene column, and value `1`.
- no output column contains a USA suffix such as `-U`, `-S`, or `-A`.
- all Phase 2 execution is single-threaded.
