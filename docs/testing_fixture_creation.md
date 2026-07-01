# Human-Pangenome GAMP-to-RAD Fixture Plan

## 1. Purpose

Prove that, given a realistic GAMP produced from the **human pangenome**, panCollapse
emits exactly the correct RAD.

```text
Given this human-pangenome GAMP and this .xg / GTF / collapse manifest, does panCollapse
emit exactly the RAD records an independent oracle derives from the same GAMP?
```

The contract under test is:

```text
GAMP (+ .xg + GTF + collapse manifest) -> panCollapse -> RAD
```

alevin-fry is the downstream proof-of-concept consumer of that RAD, but this fixture stops
at the RAD file (see Section 9). It does not re-validate permit lists, collation, quant, or
count matrices.

## 2. Scope (D047)

panCollapse is a bridge from GAMP files to linear-genome RNA count tools. Per D047:

* **FASTQ -> `vg mpmap` is out of scope.** panCollapse does not own or test the aligner.
  `vg mpmap` and read simulators (including BEERS2) matter here only as *one way to obtain
  a realistic GAMP input*. A **sufficient GAMP simulation is equally acceptable.**
* **The fixture must be generated from the human pangenome** (the pinned MHC `sampleA`
  spliced bundle in Section 4), so coverage reflects production RNA-seq graphs rather than
  toy graphs.
* **The RAD oracle is GAMP-driven** (Section 6): expected RAD is computed directly from the
  GAMP by an independent implementation of the documented rules, never from upstream read
  origins and never from panCollapse output.

D047 supersedes the earlier BEERS2-plus-`vg mpmap` "primary path" framing and the earlier
"artificial GAMP must never be primary" and "mapping-stability" rules. Because the GAMP is
now the tested input, there is no upstream origin to reconcile.

## 3. Non-goals

The fixture must not:

* test or assert FASTQ -> `vg mpmap` alignment correctness;
* replace the small hand-authored behavior fixtures under `tests/vg/`;
* test alevin-fry behavior, count matrices, or expression accuracy;
* distribute whole-genome graphs, third-party FASTQs, or full reference assets;
* use panCollapse output to define expected RAD;
* infer biological correctness beyond the documented panCollapse compatibility rules.

## 4. Human-pangenome source stack

Version one uses the local panSC MHC `sampleA` spliced bundle as the human-pangenome graph
and annotation source. These files are already present locally and were existence-checked
before this plan; checksums must be re-verified at generation time.

```text
source_id	source_kind	location_type	location	sha256	size_bytes
mhc_spliced_xg	xg	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.xg	a2b57c5d0994ffd3d63a49cc23e00fec740047463698cabb20b0926f0f8e8f88	63282478
mhc_spliced_gcsa	gcsa	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.gcsa	925a5d040ec00c2e2629f16b10d8ae32f6c4a192c2509de28df0d34581ab8edb	30606896
mhc_spliced_gcsa_lcp	gcsa_lcp	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.gcsa.lcp	7543c7cbe9d61421e072b5d11d29d47c63c17840aca83778c47254267360c04e	19645441
mhc_spliced_dist	dist	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.dist	8b31c8bfb5d7f5bfff1f8e1b6da6fea182248471f474f53c7fc6e7ad4dd2b342	15933800
mhc_refpath_gtf	gtf	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/mhc.refpath.gtf	df5aaba5196bf979e3369777fba8256fdb659d395735d976feb07bf88a079cf5	9587476
mhc_hst_paths	hst_paths	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.hst_paths.txt	cacfafbac77358af2c74df3fe3aeb02198b47bf63e0e0f9922a7985ab81f8ebc	480330
mhc_hst_info	hst_info	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.info.tsv	5fd2a1e4482d2da7ee9ef140417b30c2e984959f53ce3e83a8ccba64cc73dd1c	11888710
mhc_t2g	t2g	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.t2g.tsv	f2049c58f8a3060c44544f0d694d3a880200b1cd5049c0b04b38ed3701244c7a	654779
```

The `.gcsa`, `.gcsa.lcp`, and `.dist` files are only needed if a realistic GAMP is
generated with `vg mpmap` (Section 5, option A). panCollapse itself consumes only the
`.xg`, the GTF, the collapse manifest, and the GAMP.

Why this stack: it is a real human MHC pangenome, medium-sized rather than toy, already has
a spliced index bundle and a graph-keyed GTF, and contains unique, haplotype-shared,
isoform-shared, and multi-gene transcript structure.

Do not rebuild the graph/index bundle; its GCSA build is expensive and version-sensitive.
Verify existence, checksums, and readability with the pinned local `vg`.

## 5. Obtaining a realistic GAMP

Either route is acceptable under D047. The GAMP is an *input*, not a tested interface.

**Option A — real `vg mpmap` (preferred for realism where cheap).** Generate reads from
HST/transcript-template sequences of the MHC bundle, then run `vg mpmap` once against the
matching `.xg`/`.gcsa`/`.dist` to emit binary GAMP. `vg mpmap` here is a fixture-generation
convenience only; its correctness is not asserted. Record the exact command and vg version.

**Option B — sufficient GAMP simulation.** Author multipath GAMP records directly against
real MHC graph nodes/paths, including multi-subpath records with `connection` (splice) arcs
and multi-node traversals, so the GAMP has realistic structure. Convert reviewed JSON to
binary GAMP with `vg view -J -K -k`.

Requirements for either route:

* GAMP node IDs belong to the MHC `.xg`;
* records for one read are contiguous (name-grouped);
* read names carry the synthetic raw CB/UMI suffix panCollapse parses
  (`<original_name>_<raw_CB>_<raw_UMI>`);
* the GAMP exercises realistic structure: single- and multi-subpath records, `connection`
  arcs across annotated splice junctions, reverse orientation, and multi-node traversals,
  not only single perfect exon-interior mappings.

## 6. Independent GAMP-driven RAD oracle

The oracle is the core deliverable. It computes expected RAD **from the GAMP**, using an
implementation independent of the C++ production code. For each name-grouped read:

1. Decode the GAMP records (e.g. `vg view -j` to JSON, or a reader) into subpaths,
   `next` edges, `connection` arcs, scores, edits, positions, and orientations.
2. Enumerate complete `MultipathAlignment` traversals through `next` and `connection`
   arcs, applying the same traversal cap panCollapse uses.
3. Project every reference-consuming edit to source-path coordinates via the `.xg`,
   mirroring reverse mappings and reverse source-path steps.
4. Test compatibility against the GTF exon/intron models, including exon anchors, implied
   introns, exon/intron boundaries, and annotated splice junctions for `connection` arcs.
5. Map each compatible source identity `(source_path_name, source_transcript_id)` to its
   canonical target via the collapse manifest.
6. Take `best_score = max` per canonical target (never a sum across collapsed sources),
   retain targets within the score window, drop the group if any retained target has mixed
   target-relative orientation, and sort targets by the RAD dictionary rule (alphabetical
   canonical transcript id).
7. Emit the predicted RAD record (`bc`, `umi`, `refs`, `dirs`) or the expected non-emitted
   reason plus the summary counter it increments.

The oracle must not call panCollapse. It re-derives the RAD from first principles so that a
mismatch indicts the production implementation (or a genuine spec ambiguity), not the
oracle agreeing with itself.

### Oracle layering

* Small hand-authored `tests/vg/` fixtures validate the *rules* against human judgment
  (specification correctness on tiny inputs).
* This fixture validates the C++ *implementation* against an independent reimplementation
  of the same rules on a realistic human-pangenome GAMP (implementation correctness and
  scale). The two layers are complementary; neither replaces the other.

## 7. Required coverage

The human-pangenome fixture should include exact RAD assertions for, at minimum:

1. unique single-transcript reads (the dominant class);
2. exon shared across isoforms of one gene (multi-isoform, same gene);
3. sequence shared across genes (multi-gene target set in one record);
4. two source paths / haplotypes collapsing to one canonical target, with no score
   inflation from summing;
5. transcript-unique splice junction spanning a `connection` arc;
6. intronic and exon/intron-boundary evidence;
7. target-relative reverse orientation;
8. mixed orientation for one emitted target, dropped and counted;
9. no-compatible-target read, absent from RAD and counted;
10. unaligned read group (zero subpaths), absent from RAD and counted;
11. raw molecule-identity skip (missing/malformed/unsupported), counted.

Cases 1-4 and 5-6 are the realistic-structure content that the toy `tests/vg/` medium
fixture cannot represent because its graph is trivial.

## 8. Comparison and completion checks

The fixture is complete when:

* the MHC source files exist and match recorded checksums;
* the GAMP is binary, readable by the pinned `vg`, name-grouped, and its node IDs belong
  to the MHC `.xg`;
* expected RAD is generated by the GAMP-driven oracle before panCollapse runs;
* panCollapse completes with the expected exit status and writes `map.rad`, `tx2gene.tsv`,
  and the run summary;
* parsed panCollapse RAD records match the oracle's expected records exactly (target sets,
  per-target orientation, raw CB/UMI);
* `tx2gene.tsv` is consistent with the expected canonical targets and genes;
* expected non-emitted reads are absent and their summary counters match;
* there are no unexplained extra or missing RAD records;
* re-running panCollapse on the same GAMP is byte-identical (single-thread reproducibility).

## 9. Validation stops at RAD

Do not run or assert alevin-fry `generate-permit-list`, `collate`, `quant`, count
matrices, permit-list behavior, barcode correction, or UMI deduplication. Those belong to
the downstream tool; alevin-fry interoperability is proven separately by the small
`tests/vg/` RAD fixtures.

## 10. Staging

* **Smoke slice:** a handful of read groups from a couple of MHC loci through the full
  pipeline (GAMP -> oracle -> panCollapse -> exact comparison). This proves the oracle and
  the interfaces before scaling.
* **Scaled fixture:** grow to a medium human-pangenome set (order 10^4-10^5 read groups)
  once the smoke slice passes, keeping every emitted read exactly asserted.

Do not scale before the smoke slice passes.

## 11. Artifact policy

Small reviewed truth tables, the collapse manifest, and the oracle/comparator code may be
checked in. Large generated artifacts must stay in a build or external work directory and
must not be committed: extracted FASTA/VCF, graph indexes, FASTQ, GAMP, and binary RAD.

## 12. Agent rules

* Work one phase at a time; prove the smoke slice before scaling.
* Never use panCollapse output to define expected RAD.
* Treat `vg mpmap` / BEERS2 as optional GAMP generators, not tested interfaces.
* Verify every pinned path and command against the actual local files and pinned `vg`
  before treating it as specification.
* Do not stop at RAD-adjacent alevin-fry validation.
* Record command lines, tool versions, input checksums, output checksums, and seeds.
* Stop for review when a biological or product judgment is needed.
