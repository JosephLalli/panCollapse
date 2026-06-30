# Fixture Source Inventory and Integration Status

## Original objective

Create a small, inspectable set of test fixtures for panCollapse, a tool that post-processes `vg mpmap` GAMP alignments and determines:

1. Which annotated transcripts are compatible with each aligned read or fragment.
2. The orientation of the read relative to each compatible transcript.
3. Correct behavior for multipath, spliced, ambiguous, reverse-complemented, soft-clipped, edited, and paired-end alignments.

Do not begin by inventing a large synthetic transcriptome. Reuse existing fixtures from `rpvg` and `vg` wherever possible. The goal is to obtain low-complexity fixtures with explicit ground truth that can be understood by inspecting a few graph nodes and transcript paths.

panCollapse is not currently hosted on GitHub, so this task concerns external fixture discovery, extraction, and adaptation rather than modifying an existing public repository.

## panCollapse integration status

As of 2026-06-30, the active panCollapse test policy is build-dir-only generation for
generated graph/alignment/RAD artifacts. Do not check in generated `.vg`, `.xg`, `.gamp`,
`.rad`, or alevin-fry outputs. The useful fixture definitions from this document are
integrated as inspectable text sources in `tests/vg/phase2_fixture_writer.cpp`, and CTest
converts them with the local `vg` binary under the ignored build directory.

Implemented fixture families:

* The RPVG single-end multipath `AlignmentPathFinder<vg::MultipathAlignment>` fixture is
  adapted into `rpvg_graph.json`, `rpvg_read.gamp.json`, `rpvg_annotation.gtf`, and
  `rpvg_collapse.tsv` in the CTest work directory. Its GBWT transcript walks are exposed
  as visible XG paths for panCollapse, with the limitation that this preserves the two
  concrete walks rather than making GBWT a V1 converter input.
* The vg `test/tiny/tiny.fa` and `test/tiny/tiny.gtf` fixture is adapted into
  `tiny_reference.fa`, `tiny_annotation.gtf`, `tiny_reads.gamp.json`, and
  `tiny_collapse.tsv` in the CTest work directory. It covers exon-only, intron-only,
  exon/intron-boundary, and transcript-specific splice-junction compatibility.
* A synthetic connection-score fixture in the same build-dir generator verifies that
  scored `connection` arcs participate in traversal score aggregation and score-window
  filtering.
* Additional synthetic build-dir fixtures cover same-path short gaps treated as
  deletions, same-path long gaps treated as splice junctions, `--min-splice-jump`
  threshold override, anchored outside-transcript overhang, and parent-gene-only
  negative evidence.
* A connection-through-insertion fixture verifies that a scored GAMP `connection` remains
  an observed splice when the connection passes through an insertion-only subpath before
  the next reference-consuming block.
* The vg tiny fixture covers both exon-to-intron and intron-to-exon unspliced boundary
  reads.

The scratch agent conversions verified these commands against the installed VG build:

```bash
vg view -J -v graph.json > graph.vg
vg index -x graph.xg graph.vg
vg view -J -K -k reads.gamp.json > reads.gamp
vg view -K -j reads.gamp
vg construct -m 1000 -r tiny_reference.fa > tiny_graph.vg
```

## Main finding

The closest preexisting tests are in the RPVG repository:

```text
Repository: jonassibbesen/rpvg
File: src/tests/alignment_path_finder_test.cpp
```

These tests exercise almost exactly the logical operation panCollapse must perform: identify graph paths compatible with a single-path or multipath alignment.

The fixtures are embedded directly in the C++ test source as:

* Small graph JSON strings.
* GBWT path definitions that function as transcript paths.
* `vg::Alignment` JSON strings.
* `vg::MultipathAlignment` JSON strings.
* Explicit assertions describing the compatible path IDs, orientation behavior, clipping behavior, and failure cases.

The first fixture contains a four-node branching graph and multiple GBWT paths. It tests compatibility with a complete path, a shorter path, and an unmatched/noise outcome. It also tests reverse-complement equivalence, soft clipping, alternative branch rejection, and bidirectional path indexes.

Later sections of the same file use `vg::MultipathAlignment`, the protobuf object serialized in GAMP. These tests cover reverse-complemented multipath alignments, soft clipping, bidirectional path orientation, multiple subpath starts, paired-end multipath alignments, insertions, deletions, and reverse-oriented node traversals.

These RPVG fixtures should be treated as the primary source for exact, low-complexity unit tests.

## Secondary existing resources in vg

The `vgteam/vg` repository contains several useful end-to-end transcriptomic fixtures.

### Minimal two-isoform fixture

```text
Repository: vgteam/vg
Files:
  test/tiny/tiny.fa
  test/tiny/tiny.gtf
```

`tiny.fa` contains a 51-base reference sequence. `tiny.gtf` defines two positive-strand transcripts:

* `transcript1` contains three exons.
* `transcript2` skips the middle exon.

This is suitable for testing:

* Reads compatible with both transcripts.
* Reads specific to the included middle exon.
* Reads specific to the exon-skipping junction.
* Basic spliced GAMP processing.

The annotation is small enough to inspect manually.

It does not test negative-strand transcript orientation.

### Five-isoform positive/negative-strand fixture

```text
Repository: vgteam/vg
Files:
  test/small/x.fa
  test/small/x.gtf
  test/small/x.vcf.gz
  test/small/x_rna_1.fq
  test/small/x_rna_2.fq
```

`small/x.gtf` defines five transcripts:

* Three positive-strand isoforms over one gene.
* Two negative-strand isoforms over another gene.
* Shared exons.
* Shared splice junctions.
* Exon skipping.
* Alternative negative-strand exon structures.

This is the strongest existing `vg` fixture for testing both transcript overlap and read-to-transcript orientation.

The official `vg` README uses these exact files to construct an `mpmap` transcriptomic index and generate GAMP from paired RNA-seq FASTQs.

The existing FASTQ files contain thousands of simulated reads and sequencing errors. They are appropriate for a smoke or integration test, but not as the first diagnostic unit test. Extract a small number of reads only after their expected transcript compatibility has been independently established.

### Multilocus multimapping fixture

```text
Repository: vgteam/vg
Files:
  test/small/xy.fa
  test/small/xy.gtf
```

The `x` and `y` sequences in this fixture are identical, and the GTF defines corresponding transcript structures on both contigs. This provides a compact way to generate reads with multiple genomic placements and multiple transcript equivalence classes.

This fixture is useful for testing:

* Multiple reported mappings for one read.
* Equivalent transcripts on different loci.
* Disconnected or agglomerated multipath alignments.
* Whether panCollapse combines or separates transcript sets from distinct genomic placements correctly.

### Inline spliced-read examples

```text
Repository: vgteam/vg
File: test/t/33_vg_mpmap.t
```

This test script constructs several RNA reads inline and verifies:

* A single spliced alignment.
* A paired-end alignment where read 1 crosses a splice junction.
* A paired-end alignment where read 2 crosses a splice junction.

These reads are useful for testing GAMP `subpath` and `connection` processing. They were created to test splice discovery rather than transcript compatibility, so they should not be assigned transcript ground truth without additional analysis.

## Important limitation

There does not appear to be a preexisting committed directory containing all of the following together:

```text
reads.fastq
graph.xg
alignments.gamp
expected_transcripts.tsv
```

The available resources fall into two categories:

1. RPVG provides excellent low-complexity graphs, alignments, transcript-like paths, and expected results, but they are embedded in C++ source.
2. `vg` provides FASTA, GTF, FASTQ, and graph-construction examples, but XG and GAMP files are generally generated during tests rather than committed.

The task is therefore to materialize existing logical fixtures, not to design the entire test matrix from scratch.

## Future extraction guidance

The sections below preserve the original fixture-extraction brief as source inventory and
future-work guidance. They are not active deliverables for the current committed slice
unless they are explicitly listed above under "panCollapse integration status".

First, clone or inspect the following repositories:

```text
jonassibbesen/rpvg
vgteam/vg
```

Start with:

```text
rpvg/src/tests/alignment_path_finder_test.cpp
```

Identify every `TEST_CASE` that uses:

```cpp
AlignmentPathFinder<vg::MultipathAlignment>
```

For each relevant test, extract:

* The graph JSON.
* The GBWT path definitions.
* The multipath alignment JSON.
* Whether the alignment is single-end or paired-end.
* Which paths are expected to match.
* Whether the matching path traversal is forward or reverse.
* Any expected clipping, insertion, deletion, branch, or noise behavior.
* Any strandedness mode used when constructing `AlignmentPathFinder`.

Do not infer expectations only from comments. Translate the actual `REQUIRE(...)` assertions into a structured expected-results table.

## Materialize the RPVG fixtures

Create a script or build-dir fixture generator that reads or contains the extracted RPVG
fixture definitions and emits standalone fixture files. In panCollapse's active test
suite, this must mean CTest/build-directory outputs, not checked-in generated binary
artifacts.

A reasonable scratch output layout for exploratory extraction is:

```text
tests/fixtures/rpvg_alignment_path_finder/
  single_end_basic/
    graph.json
    graph.vg
    graph.xg
    alignments.gamp.jsonl
    alignments.gamp
    transcript_paths.tsv
    expected.tsv
    README.md

  single_end_multipath/
    graph.json
    graph.vg
    graph.xg
    alignments.gamp.jsonl
    alignments.gamp
    transcript_paths.tsv
    expected.tsv
    README.md

  paired_end_multipath/
    graph.json
    graph.vg
    graph.xg
    alignments.gamp.jsonl
    alignments.gamp
    transcript_paths.tsv
    expected.tsv
    README.md
```

If an XG alone is insufficient to represent the transcript paths used by RPVG, also materialize the GBWT or other path index required to preserve the original fixture semantics. Do not silently replace GBWT transcript paths with graph paths unless the equivalence is explicitly verified.

For the implemented panCollapse V1 fixture, the replacement is explicit and limited:
the two RPVG GBWT transcript walks are exposed as visible XG paths because V1 consumes
GAMP plus `.xg`, not GBWT. This is a panCollapse compatibility fixture, not a claim that
XG paths reproduce all RPVG `PathsIndex`/GBWT behavior.

The expected table should use stable semantic identifiers rather than internal vector ordering. For example:

```text
read_name	transcript_id	orientation	compatible	notes
read1	tx1	F	true	full path
read1	tx2	F	true	prefix-compatible path
read1	noise	.	false	no indexed transcript path
```

Where the RPVG test identifies paths only numerically, assign readable names such as `tx1`, `tx2`, and record the mapping from the original GBWT path ID.

## Converting embedded JSON into vg files

Use the `vg` command line when possible.

For graph JSON:

```bash
vg view -Jv graph.json > graph.vg
vg index -x graph.xg graph.vg
```

Verify the exact `vg view` flags against the installed version rather than assuming the command above is correct.

For multipath alignment JSON, determine the correct `vg view` invocation for converting JSON representations of `MultipathAlignment` into binary GAMP. Validate the output by converting it back:

```bash
vg view -Kj alignments.gamp
```

The round-tripped JSON must preserve:

* `start`
* `subpath`
* `next`
* `connection`
* node IDs
* offsets
* `is_reverse`
* edits
* scores
* mapping quality
* read name and sequence
* paired-end linkage where present

Do not substitute ordinary GAM output for GAMP. panCollapse must be tested against actual serialized `MultipathAlignment` records.

## Create end-to-end vg fixtures

After the extracted RPVG fixtures work, create two small end-to-end fixtures using the existing `vg` data.

### Tiny isoform fixture

Use:

```text
vg/test/tiny/tiny.fa
vg/test/tiny/tiny.gtf
```

Build the graph and indexes using either `vg autoindex --workflow mpmap` or explicit `vg construct`, `vg rna`, and `vg index` commands.

Prefer explicit commands in the fixture README so it is clear how transcript paths entered the graph.

The `vg rna` command uses `transcript_id` as the default transcript path name and can embed reference transcripts as graph paths with `--add-ref-paths` or `-r`.

Create or select only a few reads:

* A read wholly inside a shared exon.
* A read crossing the splice junction shared by both isoforms, if applicable.
* A read crossing the middle-exon inclusion junction.
* A read crossing the exon-skipping junction.
* The reverse complement of at least one read.

Generate the GAMP with `vg mpmap`, which outputs GAMP by default.

### Small positive/negative-strand fixture

Use:

```text
vg/test/small/x.fa
vg/test/small/x.gtf
```

Select or construct a small number of exact reads that cover:

* A shared positive-strand exon.
* A positive-strand shared junction.
* A positive-strand isoform-specific junction.
* A shared negative-strand exon.
* A negative-strand isoform-specific junction.
* Reverse complements of representative reads.

For every read, determine orientation relative to each transcript, not merely relative to the genomic `x` path.

Expected orientation must be represented per transcript. A genomic forward alignment can be reverse relative to a negative-strand transcript, and one alignment may theoretically have different orientation relationships to different overlapping transcript paths.

## Multimapping fixture

Use:

```text
vg/test/small/xy.fa
vg/test/small/xy.gtf
```

Generate one or more reads that map equally well to both `x` and `y`.

Run `mpmap` with multiple mappings retained, for example using an appropriate `--max-multimaps` value.

Also generate an agglomerated alignment using the `mpmap` option that combines separate mappings into one potentially disconnected multipath alignment. In the current source, `-a`/`--agglomerate-alns` performs this operation.

The expected output should distinguish:

* One GAMP record containing one genomic placement.
* Multiple GAMP records for the same read.
* One disconnected GAMP containing multiple placements.
* Transcript IDs associated with each placement.
* The union or grouping behavior expected from panCollapse.

## FASTQ requirement

The RPVG source fixtures begin as serialized alignments, not FASTQ reads. Where possible, derive a FASTQ record from the alignment’s `sequence` and assign a deterministic quality string.

However, do not assume that re-running `mpmap` on the reconstructed FASTQ will reproduce the exact hand-authored multipath alignment. Mapper output may vary with `vg` version, scoring defaults, index construction, and heuristics.

Maintain two test categories:

### Parser and path-resolution unit tests

Use the extracted, serialized RPVG GAMP records directly. These provide exact and stable multipath structures.

### End-to-end mapper tests

Use FASTQ plus XG/GCSA/distance indexes and generate GAMP with `mpmap`. These verify integration but should use semantic expectations rather than byte-for-byte GAMP equality.

## Validation requirements

For every materialized fixture:

1. Confirm that the graph loads successfully.
2. Confirm that the XG loads successfully.
3. Confirm that binary GAMP round-trips through `vg view`.
4. Confirm that transcript paths can be enumerated and named.
5. Record the installed `vg` version used to materialize the fixture.
6. Run panCollapse on the fixture.
7. Compare panCollapse output with `expected.tsv`.
8. Ensure the test compares transcript IDs and orientations without depending on unstable subpath numbering.
9. Ensure paired-end tests evaluate fragment-level compatibility where appropriate.
10. Ensure reverse-complement tests explicitly check orientation, not just transcript membership.

## Avoid these mistakes

Do not use the entire `small/x_rna_1.fq` and `small/x_rna_2.fq` datasets as the primary test. They are useful as a later smoke test but are too large and noisy for diagnosing path-resolution failures.

Do not treat graph path direction as automatically equivalent to transcript orientation.

Do not assume one orientation value applies to all transcripts compatible with an alignment.

Do not compare raw GAMP bytes.

Do not assume subpath indices are stable across `vg` versions.

Do not rebuild the RPVG fixture conceptually from memory. Extract the actual graph, path definitions, alignments, and expected assertions from the source file.

Do not discard unmatched or noise outcomes from the RPVG tests. panCollapse should have explicit behavior for an alignment that is valid on the graph but incompatible with every indexed transcript.

## Future deliverables not implemented in this slice

Future fixture expansion may produce:

```text
tests/fixtures/
  rpvg_alignment_path_finder/
  vg_tiny_isoforms/
  vg_small_stranded_isoforms/
  vg_xy_multimapping/
```

It may also produce:

```text
scripts/materialize_rpvg_fixtures.py
scripts/build_vg_fixture_indexes.sh
tests/fixtures/README.md
```

The top-level README should include a fixture matrix with these columns:

```text
fixture
source repository
source file
single/paired
GAM/GAMP
spliced
multipath branching
multiple transcripts
negative-strand transcript
reverse-complement read
soft clipping
insertion/deletion
multilocus mapping
expected-output source
```

At the end of such a future expansion, report:

* Which RPVG tests were successfully materialized.
* Which tests could not be represented as standalone XG/GAMP files and why.
* Which existing `vg` files were reused unchanged.
* Which FASTQ reads were newly selected or derived.
* Any behavior that varied with the installed `vg` version.
* The smallest fixture set that collectively covers all required panCollapse behaviors.
