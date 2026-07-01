# Pangenome Transcriptome RAD Fixture Creation Plan

## 1. Purpose

Create a deterministic test dataset for panCollapse where every simulated read is
generated from a known pangenome transcriptome template and has an independently
calculated expected RAD record.

The fixture is meant to answer one question:

```text
Given these known-location reads and this graph/annotation/manifest, does panCollapse
emit the correct mapper-style RAD records?
```

The preferred path is:

```text
test pangenome graph + annotation + collapse manifest
-> pangenome transcriptome templates
-> BEERS2 cDNA reads with known template coordinates
-> renamed cDNA FASTQ
-> vg mpmap against the matching graph/index bundle
-> binary GAMP
-> panCollapse RAD
-> semantic RAD comparison
```

Validation stops at the RAD file. Do not run or validate:

* alevin-fry `generate-permit-list`;
* alevin-fry `collate`;
* alevin-fry `quant`;
* count matrices;
* permit-list behavior;
* barcode correction;
* UMI deduplication;
* downstream biological quantification.

The expected output is calculated before panCollapse is run. The expected RAD records
must not be derived from panCollapse output.

## 2. Scope

This document defines instructions for a project-wide agent to create a RAD-level test
dataset.

The dataset should contain:

* a reviewed reference and annotation bundle, either local or downloaded from pinned
  URLs;
* a documented test pangenome graph bundle;
* the `.xg` graph used by panCollapse;
* the associated indexes needed to produce GAMP with `vg mpmap`;
* pangenome transcriptome templates derived from the graph/reference/annotation bundle;
* BEERS2 bulk RNA-style reads generated from those transcriptome templates;
* renamed cDNA reads whose names include synthetic raw barcode and UMI suffixes required
  by panCollapse input parsing;
* binary GAMP generated from the renamed cDNA reads;
* truth tables describing read origins, source intervals, splice evidence, barcode,
  UMI, transcript, gene, and expected orientation;
* an independent expected RAD model;
* a comparator that parses panCollapse RAD and checks it against the expected RAD model.

Artificial GAMP may be used only as a fallback for narrow parser or failure-mode tests
when `vg mpmap` cannot stably create a required case. It must be marked as artificial and
must not replace the primary aligned-read fixture.

The first useful fixture should be medium-sized but curated. It should prefer exact,
reviewed unique and multimapping cases over broad biological realism.

## 3. Non-goals

The first implementation must not attempt to:

* replace the existing small build-directory behavior fixtures;
* create a general-purpose single-cell simulator;
* distribute whole-genome graph indexes;
* distribute third-party FASTQs or full reference assets;
* test alevin-fry behavior;
* prove count-matrix correctness;
* estimate expression accuracy;
* choose universal biological error rates;
* use real public 10x reads as an oracle;
* make artificial GAMP the primary fixture path;
* define correctness for every possible multimapping biological molecule;
* use panCollapse to generate the expected target sets.

Population-aware donor haplotypes and real phased variants are useful, but they are not
the core oracle. The core oracle is known read location plus independently evaluated
RAD compatibility.

## 4. Repository and artifact policy

The source repository may contain small source and oracle files:

```text
validation/rad_fixture/
├── README.md
├── config/
│   ├── fixture.yaml
│   ├── beers2.config.yaml
│   ├── input_locations.tsv
│   ├── sources.lock.yaml
│   └── tools.lock.yaml
├── metadata/
│   ├── loci.tsv
│   ├── donors.tsv
│   ├── transcripts.tsv
│   └── graph_bundle.tsv
├── reference/
│   └── collapse_manifest.tsv
├── schemas/
├── scripts/
├── workflow/
├── tests/
└── truth/
    ├── curated_cases.tsv
    ├── transcript_templates.tsv
    ├── template_window_catalog.tsv
    ├── beers2_molecules.tsv
    ├── beers2_read_origins.tsv
    ├── reads.tsv
    ├── expected_alignment_classes.tsv
    ├── expected_rad_records.tsv
    └── expected_non_emitted.tsv
```

Generated artifacts must be written to a build or external work directory, for example:

```text
build/rad_fixture/
├── reference/
├── graph/
├── transcriptome/
├── beers2/
├── fastq/
├── alignments/
├── expected/
├── pancollapse/
└── reports/
```

Do not check in large generated artifacts, including:

* extracted FASTA or full reference FASTA;
* extracted or full VCF;
* generated GTF when large;
* graph indexes;
* BEERS2 intermediate molecule packets and logs when large;
* FASTQ files;
* GAMP files;
* binary RAD files;
* mapper caches.

Small reviewed truth tables and compact expected RAD tables may be checked in.

## 5. Version-one target

Version one should be medium-sized but still oracle-controlled.

Recommended first target:

* one identified test pangenome graph bundle;
* one `.xg` plus the exact associated mapper indexes needed by `vg mpmap`;
* an MHC-scale annotated pangenome transcriptome when the local panSC MHC bundle is
  available;
* approximately 50,000 BEERS2-generated read pairs;
* bulk-RNA-style simulation, not a single-cell count benchmark;
* known transcript-template origin for every read;
* exact expected RAD assertion for every emitted read;
* exact expected skip/drop assertion for every non-emitted read.

Use a smaller smoke subset only to debug the workflow. The publishable fixture target is
the 50,000-read BEERS2 dataset.

### 5.1 Required BEERS2 pangenome-transcriptome simulation method

The primary simulation method is BEERS2 run on molecule packets derived from an annotated
pangenome transcriptome.

Custom code is allowed to prepare BEERS2 inputs and RAD oracle tables. Custom code must
not replace BEERS2 as the read/library simulator unless a narrow case is explicitly
marked as artificial.

BEERS2 supports molecule-file input with tab-separated rows:

```text
transcript_id
chrom
parental_start
parental_cigar
ref_start
ref_cigar
strand
sequence
```

BEERS2 then simulates the library and sequencing process and can emit FASTQ plus SAM/BAM
containing true simulated origins. In this fixture, those molecule files are generated
from pangenome transcript templates. The `transcript_id` field should encode a stable
fixture molecule identifier and template/case identity; the sidecar truth tables map that
identifier back to source transcript, source path, canonical targets, and expected RAD
status.

#### Step A: Establish graph-coordinate source paths

Before any read is generated, identify the source-coordinate paths in the `.xg` graph.

Rules:

* every source path used for read simulation must be visible in the `.xg`;
* every source path used for read simulation must have matching GTF records;
* every source path used for read simulation must appear in the collapse manifest when
  compatible evidence from that path should be emitted;
* GTF `seqname`, manifest `source_path_name`, and `.xg` path name must match exactly;
* if haplotype or copy-specific paths are used, they must be real graph paths with
  extractable sequence, not only labels in a truth table.

For an optional interface-smoke profile only, it is acceptable to create a small
artificial pangenome graph with explicit source paths such as:

```text
L001_REF
L001_HAP1
L001_HAP2
```

The default 50,000-read fixture should use the pinned MHC graph bundle in Section 6. The
important requirement for any profile is that reads are generated from graph path
sequences, and `vg mpmap` aligns back to the same graph/index bundle.

#### Step B: Build or select the graph/index bundle

Build or select the test pangenome graph before transcript-template generation.

The bundle must identify:

* the exact `.xg` consumed by panCollapse;
* the exact GCSA index consumed by `vg mpmap`;
* the exact GCSA-LCP file consumed by `vg mpmap`;
* the exact distance index consumed by `vg mpmap`;
* the command or source package that produced them.

If the graph is built from FASTA, VCF, and GTF, record the graph-construction command.
If the graph is prebuilt, record its local path or download URL and checksums. In either
case, do not generate reads until the graph/index bundle is identified and checksum-
recorded.

#### Step C: Extract source-path sequence from the graph

For each selected source path, extract sequence from the `.xg` path, not from an
unrelated FASTA file. The implementation may call `vg` commands or use a graph API, but
it must verify the exact command/API against the pinned vg version.

For every extracted path interval, validate:

* path exists in `.xg`;
* extracted interval length matches the requested half-open interval;
* sequence is reproducible from the same graph bundle;
* if a reference FASTA is also available, reference-path extraction agrees with FASTA for
  reference paths.

This prevents accidentally simulating reads from a sequence that is not present in the
graph used for mapping.

#### Step D: Construct pangenome transcriptome templates

For each selected source transcript, construct one or more transcriptome templates from
GTF intervals projected onto a graph source path.

Use GTF coordinates as 1-based inclusive input and convert them to 0-based half-open
source-path coordinates internally.

Template kinds:

* `mature_spliced`: concatenate annotated exon intervals for one transcript.
* `pre_mrna`: use the transcript or gene span containing exons and implied introns.
* `retained_intron`: concatenate explicitly selected exons and retained intron intervals.
* `custom_curated`: use an explicitly reviewed segment list for a special case.

Strand handling:

* for positive-strand transcripts, template order follows increasing source-path
  coordinate;
* for negative-strand transcripts, first collect the source-path intervals, then
  reverse-complement the resulting sequence so template coordinate 0 is transcript
  5-prime sequence;
* retain a coordinate map from template offsets back to source-path coordinates,
  including orientation, so expected target-relative read orientation can be calculated
  without using panCollapse.

The template builder must write:

* template FASTA in the build directory;
* `truth/transcript_templates.tsv`;
* a template-to-source coordinate map in the build directory, preferably run-length
  encoded by contiguous source interval.

The coordinate map should include at least:

```text
template_id
template_start
template_end
source_path_name
source_start
source_end
source_strand
source_transcript_id
feature_type
```

Allowed `feature_type` values should include:

```text
exon
intron
exon_intron_boundary
custom
```

#### Step E: Build a template-window equivalence catalog

Before creating BEERS2 molecule packets, enumerate candidate windows from the pangenome
transcriptome templates and classify their expected alignment/compatibility behavior.

The catalog should include:

```text
window_id
template_id
template_start
template_end
window_sequence_sha256
source_segments
overlaps_splice_junction
overlaps_intron
compatible_source_targets
compatible_canonical_targets
expected_orientation
alignment_class
case_label
```

Allowed `alignment_class` values:

```text
unique_template
multi_haplotype_same_transcript
multi_isoform_same_gene
multi_gene
no_compatible_target
```

How to classify windows:

* `unique_template`: the window sequence is specific to one source template and should
  produce one expected source target before collapse.
* `multi_haplotype_same_transcript`: the sequence is shared across multiple haplotype or
  copy-specific HST paths for the same source transcript.
* `multi_isoform_same_gene`: the sequence is shared across multiple transcript isoforms
  of one gene.
* `multi_gene`: the sequence is shared across templates from more than one gene.
* `no_compatible_target`: the sequence is expected to map or project without satisfying
  the panCollapse compatibility contract.

The equivalence catalog is the bridge between BEERS2 true origins and expected RAD
target sets. It must be built before running panCollapse and must not use panCollapse
output.

For publishable results, include both unique and multimapping windows in the 50,000-read
design. A reasonable initial mix is:

```text
unique_template:                 40-60%
multi_haplotype_same_transcript: 15-25%
multi_isoform_same_gene:         10-20%
multi_gene:                       5-10%
edge/failure cases:               1-5%
```

The exact mix should be pinned in `fixture.yaml`.

#### Step E2: Convert annotated pangenome templates into BEERS2-ready molecules

The annotated pangenome is converted into BEERS2 inputs through an explicit template
catalog, not by asking BEERS2 to understand graph coordinates directly.

Use this algorithm:

1. Treat each selected pangenome transcriptome template as a BEERS2 reference contig.
   The template FASTA written in Step D is the `reference_genome_fasta` for the BEERS2
   run that emits true-origin SAM/BAM.
2. Preserve a coordinate map from each template base to graph source path, source
   transcript, gene, feature type, and strand.
3. Enumerate candidate insert windows on each template using the pinned BEERS2 read
   length, paired-end setting, and fragment-size constraints.
4. For each candidate window, derive the expected read-end intervals before sequencing
   error is applied.
5. Build an exact-sequence index over all candidate read-end sequences and their reverse
   complements across all templates.
6. Classify each candidate window by the set of templates, transcripts, source paths,
   genes, and canonical targets that contain the same expected read evidence.
7. Select a reviewed mixture of `unique_template`, `multi_haplotype_same_transcript`,
   `multi_isoform_same_gene`, `multi_gene`, and expected non-emitted windows.
8. Write selected windows as BEERS2 molecule-file rows. The BEERS2 `chrom` value is the
   pangenome template ID, and the BEERS2 coordinate fields point to the selected template
   interval.

This produces BEERS2 inputs with known expected alignment classes:

* A unique read is a read whose expected read-end sequence occurs in exactly one
  compatible pangenome transcriptome template before collapse.
* A haplotype-shared read is a read whose expected read-end sequence occurs in multiple
  source paths for the same transcript or same canonical transcript.
* An isoform-multimapping read is a read whose expected evidence is compatible with
  multiple transcripts of one gene.
* A multi-gene read is a read whose expected evidence is compatible with transcripts
  from more than one gene.
* A non-emitted read is a read whose source evidence is known but should not produce a
  RAD record under the declared panCollapse compatibility rules.

For strict RAD assertions, the first BEERS2 profile should either disable cDNA sequencing
errors or place error-containing reads in a separate challenge subset. Multimapping in
the primary fixture should come from the annotated pangenome and transcriptome
equivalence classes, not from untracked sequencing errors.

After BEERS2 runs, translate each BEERS2 true-origin SAM/BAM record from template
coordinates back through the template coordinate map. The translated origin must match
one of the preclassified catalog windows before that read is allowed into the strict RAD
oracle.

#### Step F: Generate BEERS2 molecule packets

Create BEERS2 molecule packet files from the window equivalence catalog. Each BEERS2
input molecule should correspond to one selected pangenome transcriptome window or
template segment whose expected RAD behavior is already known.

For each molecule, record:

* fixture molecule ID;
* template ID;
* template start and end;
* BEERS2 `transcript_id` value;
* BEERS2 `chrom` value;
* BEERS2 `parental_start`, `parental_cigar`, `ref_start`, and `ref_cigar`;
* BEERS2 `strand`;
* molecule sequence;
* expected source targets;
* expected canonical targets;
* expected emitted or non-emitted RAD status.

Recommended BEERS2 mapping:

* Use the pangenome template ID as `chrom` in a BEERS2 transcriptome reference FASTA.
* Use `template_start + 1` for `parental_start` and `ref_start`, because BEERS2 molecule
  file starts are 1-based.
* Use `<molecule_length>M` for both CIGAR fields when the molecule sequence is contiguous
  on the pangenome transcript template.
* Use `+` or `-` in `strand` according to the selected molecule orientation.
* Use a BEERS2 `transcript_id` like:

```text
MOL000001__TEMPLATE_NM_000063.6_H1__CASE_multi_haplotype_same_transcript
```

The sidecar truth table, not the BEERS2 `transcript_id` string alone, remains
authoritative.

Important: the BEERS2 reference for this run may be a synthetic HST/transcript-template
FASTA generated from the pangenome graph paths. It does not need to be the genomic FASTA
used to build the graph. The BEERS2 SAM/BAM true-origin alignments then point to HST
template contigs, and the fixture's coordinate maps translate those origins back to graph
source paths and expected RAD targets.

#### Step G: Run BEERS2 and extract true read origins

Run BEERS2 with:

* molecule-file input enabled;
* `from_distribution_data` packet generation disabled or set to zero;
* FASTQ output enabled;
* SAM or BAM true-alignment output enabled;
* a pinned random seed;
* enough input molecules and BEERS2 retention settings to yield approximately 50,000
  read pairs.

The BEERS2 SAM/BAM true alignments are not panCollapse inputs. They are used to connect
each FASTQ read back to the fixture molecule/window and expected target-set class.

After BEERS2 finishes, create:

```text
truth/beers2_molecules.tsv
truth/beers2_read_origins.tsv
truth/expected_alignment_classes.tsv
```

`truth/beers2_read_origins.tsv` should include at least:

```text
beers2_read_name
fixture_molecule_id
template_id
template_start
template_end
beers2_true_chrom
beers2_true_start
beers2_true_cigar
beers2_true_strand
observed_read_sequence_sha256
alignment_class
expected_source_targets
expected_canonical_targets
expected_rad_status
```

For strict RAD assertions, use reads whose BEERS2 true origin can be mapped back to one
or more preclassified windows in the equivalence catalog. Reads that BEERS2 turns into
adapter-only, all-polyA, or otherwise uninformative sequence may remain in the FASTQ for
realism, but they must be marked with an explicit expected non-emitted status or excluded
from strict RAD comparison.

#### Step H: Add panCollapse molecule suffixes and align

BEERS2 produces bulk RNA-style FASTQ. panCollapse currently obtains raw CB/UMI from the
GAMP read name, so add deterministic synthetic suffixes after BEERS2:

```text
<BEERS2_read_name>_<raw_CB>_<raw_UMI>
```

Rules:

* the suffixing step is panCollapse adapter glue, not part of the biological simulator;
* use one or a small number of fixed raw barcodes for bulk-style data;
* assign deterministic UMIs or molecule IDs sufficient to make RAD record parsing
  unambiguous;
* record the suffix map in `truth/reads.tsv`;
* do not use downstream barcode correction or UMI deduplication as a validation target.

Then align the renamed BEERS2 FASTQ with `vg mpmap` against the matching pangenome
graph/index bundle to produce binary GAMP.

The GAMP is an input to panCollapse, not an oracle. If `vg mpmap` does not produce the
expected evidence for a BEERS2 read, record that as a mapping discrepancy and do not
change the expected RAD table to match panCollapse.

#### Step I: Optional direct curated slices

If BEERS2 cannot reliably produce a rare edge case, a small direct-template-slicing
supplement may be added. These reads must be marked `direct_curated_slice` and kept
separate from the BEERS2 primary set in reports.

The direct slice supplement is a fallback for targeted coverage, not the main simulation
method.

#### Artificial GAMP fallback

Artificial GAMP is allowed only for narrow cases that cannot be made stable through the
aligned-read route. Such cases must be explicitly marked `artificial_gamp`, and the
fixture must still contain at least one primary path where transcript-template reads are
aligned to a graph/index bundle with `vg mpmap`.

## 6. Decisions that must be pinned before generation

The fixture is not implementation-ready until the following values are explicit.

### 6.1 Source stack

Version one uses the local panSC MHC HST index bundle as the medium graph and annotation
source for the 50,000-read BEERS2 fixture.

This is the default source stack for the first implementation. Do not ask the
implementation agent to choose a different graph, GTF, transcriptome, or simulator unless
this plan is explicitly revised.

Pinned local source files:

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
mhc_derived_whitelist	barcode_list	local_path	/mnt/ssd/lalli/panSC/tests/fixtures/mhc/derived_whitelist.txt	b6793738b0489f989849c921ac9bff537907912047bac2a5e1cc5ff593a2099f	779722
```

Why this stack:

* it is already present locally under `~/panSC`;
* it is medium-sized rather than toy-sized;
* it has a prebuilt spliced `vg mpmap` index bundle;
* it contains 27,011 HST paths and a graph-keyed MHC GTF;
* panSC has already used it for a 1M-read MHC-scale `vg mpmap`/surject pilot;
* it is large enough to contain unique, haplotype-shared, isoform-shared, and
  multi-gene read classes.

The tiny vg `test/small/x` fixture may be kept as an optional interface-smoke profile,
but it is not the default source stack for the 50,000-read BEERS2 fixture.

BEERS2 itself should be pinned separately in `tools.lock.yaml`. The current upstream
installation source is:

```text
tool_id	location_type	location
BEERS2	https_url	https://github.com/itmat/BEERS2
```

The exact BEERS2 git tag, release, or commit must be pinned before generating the
publishable fixture.

For any later profile, pin:

* FASTA source, version, file location, and checksum;
* GTF source, version, file location, and checksum;
* optional phased VCF source, version, file location, and checksum;
* optional donor metadata source, version, file location, and checksum;
* optional prebuilt graph/index bundle location and checksum;
* vg version or commit;
* panCollapse revision or release under test.

Every source must have either a local file path or a web download address. Record this in
`config/input_locations.tsv` or `config/sources.lock.yaml` with at least:

```text
source_id
source_kind
location_type
location
version_or_release
checksum_algorithm
checksum
license_or_terms
required_for
notes
```

Allowed `location_type` values:

```text
local_path
https_url
ftp_url
s3_url
gs_url
```

Local paths should be absolute paths or paths relative to the repository root. Download
URLs must be versioned or otherwise stable; do not use `latest` URLs in a reviewed
fixture specification.

### 6.2 Loci and transcripts

Pin reviewed tables for:

```text
locus_id
source_path_name
chromosome_or_contig
start
end
gene_ids
transcript_ids
required_behavior
selection_reason
```

No implementation agent may choose substitute loci or genes automatically. Automated
ranking is allowed only as a proposal step; the final locus table must be reviewed.

### 6.3 Collapse manifest

Pin the exact manifest used by panCollapse:

```text
source_path_name
source_transcript_id
canonical_transcript_id
canonical_gene_id
```

The expected RAD oracle must apply this manifest independently of panCollapse.

### 6.4 Read and molecule configuration

Pin:

* random seed;
* cell barcode length;
* UMI length;
* cDNA read length;
* read-name format;
* selected raw cell barcodes;
* selected raw UMIs or UMI generation rule;
* source transcript for each curated molecule;
* source interval or junction for each curated read;
* expected target-relative orientation;
* expected emitted or non-emitted status.

For version one, prefer explicit selected molecule/window rows and deterministic
post-BEERS2 filtering over ad hoc stochastic generation.

### 6.5 Graph and index bundle

The fixture must identify the graph bundle used to create GAMP.

Version one graph bundle:

```text
graph_bundle_id: panSC_mhc_sampleA_spliced
graph_build: prebuilt local panSC MHC HST index
xg_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.xg
gcsa_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.gcsa
gcsa_lcp_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.gcsa.lcp
dist_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.spliced.dist
gtf_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/mhc.refpath.gtf
hst_paths_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.hst_paths.txt
hst_info_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.info.tsv
t2g_path: /mnt/ssd/lalli/panSC/tests/fixtures/mhc/sampleA.t2g.tsv
```

Do not rebuild the graph/index bundle for the default 50,000-read fixture. Rebuilding is
a separate provenance task because the panSC MHC GCSA build is expensive and version-
sensitive. The fixture should verify that the prebuilt files exist, match the recorded
checksums, and are readable by the pinned local `vg`.

For any graph bundle, the manifest should include:

```text
graph_bundle_id
xg_path
gcsa_path
gcsa_lcp_path
dist_path
source_fasta_id
source_gtf_id
source_vcf_id
build_command_file
vg_version
xg_checksum
gcsa_checksum
gcsa_lcp_checksum
dist_checksum
notes
```

Rules:

* `xg_path` is the exact `.xg` panCollapse receives.
* `gcsa_path`, `gcsa_lcp_path`, and `dist_path` are the exact indexes used by
  `vg mpmap`.
* all graph/index files must come from the same graph build;
* graph path names must match the GTF seqnames and collapse-manifest source paths;
* graph checksums must be recorded before GAMP generation;
* a GAMP produced from one graph bundle must not be compared against a different `.xg`.

### 6.6 Mapping commands

Pin and verify:

* graph construction command;
* graph index outputs expected by `vg mpmap`;
* `vg mpmap` command that emits binary GAMP;
* panCollapse command;
* RAD parser/comparator command.

Commands must be checked against the pinned tool versions before they are treated as
specification.

## 7. Truth and oracle files

### 7.1 Pangenome transcriptome template table

`truth/transcript_templates.tsv` should describe each template from which reads may be
simulated:

```text
template_id
graph_bundle_id
source_path_name
source_gene_id
source_transcript_id
template_kind
template_strand
source_segments
template_length
template_sequence_sha256
template_fasta_path
coordinate_map_path
construction_method
```

Allowed `template_kind` values should include:

```text
mature_spliced
pre_mrna
retained_intron
custom_curated
```

`construction_method` must state how the sequence was generated, for example:

```text
extract exons from fixture GTF on source path and concatenate in transcript order
```

Template sequences may be generated into the build directory, but the template table must
make their source path, transcript identity, coordinates, orientation, and checksum
auditable.

### 7.2 Template-window catalog

`truth/template_window_catalog.tsv` should describe every pangenome transcriptome window
that may be selected for BEERS2 input or accepted into the strict RAD oracle:

```text
window_id
template_id
template_start
template_end
read_end_signature
window_sequence_sha256
source_segments
observed_junctions
compatible_source_targets
compatible_canonical_targets
expected_orientation
alignment_class
case_label
manual_rationale
```

`read_end_signature` should encode the expected one-end or paired-end evidence used for
classification. For paired-end runs, include both read-end intervals, strands, and the
expected insert orientation in a deterministic parseable format.

The catalog is built from template sequences, template coordinate maps, GTF models, and
the collapse manifest. It must not use `vg mpmap` output or panCollapse output.

### 7.3 BEERS2 molecule table

`truth/beers2_molecules.tsv` should describe every molecule packet row supplied to
BEERS2:

```text
fixture_molecule_id
window_id
case_id
beers2_packet
beers2_transcript_id
beers2_chrom
beers2_parental_start
beers2_parental_cigar
beers2_ref_start
beers2_ref_cigar
beers2_strand
molecule_sequence_sha256
expected_alignment_class
expected_source_targets
expected_canonical_targets
expected_rad_status
```

The `beers2_chrom` value must be a contig in the BEERS2 reference FASTA generated from
pangenome transcriptome templates.

### 7.4 BEERS2 read-origin table

`truth/beers2_read_origins.tsv` should be extracted from BEERS2 true-origin SAM/BAM and
joined to the molecule table:

```text
beers2_read_name
read_end
fixture_molecule_id
window_id
case_id
beers2_true_chrom
beers2_true_start
beers2_true_cigar
beers2_true_strand
template_id
template_start
template_end
source_segments
observed_read_sequence_sha256
origin_match_status
```

Allowed `origin_match_status` values:

```text
strict_catalog_match
adapter_or_polya_only
outside_catalog
mapping_expected_to_fail
excluded_from_strict_oracle
```

Only `strict_catalog_match` reads are required to appear in
`truth/expected_rad_records.tsv`.

### 7.5 Expected alignment class table

`truth/expected_alignment_classes.tsv` should record the expected alignments independent
of `vg mpmap`:

```text
read_id
beers2_read_name
window_id
alignment_class
expected_alignment_targets
expected_source_segments
expected_splice_evidence
expected_orientation
allowed_extra_alignment_reason
strict_rad_oracle
```

`expected_alignment_targets` should contain graph/template/source-transcript identifiers,
not only canonical transcript IDs. This file is the mapping-stability oracle used to
decide whether decoded GAMP evidence is compatible with the known BEERS2 origin.

### 7.6 Curated case table

`truth/curated_cases.tsv` should contain one row per intentional test case:

```text
case_id
required_behavior
read_id
source_path_name
source_gene_id
source_transcript_id
source_strand
molecule_class
template_id
template_start
template_end
read_orientation
source_segments
observed_junctions
raw_cell_barcode
raw_umi
expected_source_targets
expected_canonical_targets
expected_orientation
expected_rad_status
manual_rationale
```

`read_orientation` should be one of:

```text
template_forward
template_reverse_complement
```

`source_segments` should use a deterministic parseable format, for example:

```text
path:start-end:strand[,path:start-end:strand...]
```

Coordinates should be 0-based half-open on graph source paths unless the fixture
explicitly documents a different convention.

`expected_rad_status` should be one of:

```text
emit
skip_raw_molecule
drop_mixed_orientation
no_compatible_target
mapping_expected_to_fail
```

### 7.7 Read truth table

`truth/reads.tsv` should contain the final panCollapse-input read-name mapping after
BEERS2 output is suffixed with synthetic raw CB/UMI values:

```text
read_id
case_id
beers2_read_name
renamed_read_name
molecule_id
template_id
template_start
template_end
template_strand
read_orientation
raw_cell_barcode
raw_umi
observed_r2_sequence_sha256
source_segments
observed_junctions
expected_orientation
expected_rad_status
```

`template_start` and `template_end` are 0-based half-open coordinates on
`template_id`. `source_segments` records the corresponding graph/source-path
coordinates.

The renamed read name must follow panCollapse's molecule-identity convention:

```text
<original_read_name>_<raw_CB>_<raw_UMI>
```

The BEERS2 smoke subset should avoid underscores in the original read-name portion.
Version one should include one explicit underscore-containing renamed-read case to test
right-side parsing.

### 7.8 Expected RAD record table

`truth/expected_rad_records.tsv` is the primary oracle for panCollapse comparison.

It should contain one row per expected emitted RAD record:

```text
read_name
raw_cell_barcode
raw_umi
canonical_targets
target_orientations
case_ids
```

Rules:

* `read_name` is the GAMP/RAD read name, including raw barcode and UMI suffix.
* `canonical_targets` is a deterministic delimiter-separated list of canonical
  transcript IDs after manifest collapse.
* `target_orientations` has one value per canonical target in the same order.
* target order must be deterministic and must match the expected RAD target dictionary
  ordering rule.
* no row may be generated from panCollapse output.

### 7.9 Expected non-emitted table

Use `truth/expected_non_emitted.tsv` for reads that should not appear in RAD:

```text
read_name
case_id
expected_rad_status
expected_counter
manual_rationale
```

This table supports explicit checks for skipped or dropped groups without using
downstream count behavior.

## 8. Independent RAD oracle

The oracle must calculate expected RAD records from fixture truth, not from panCollapse.

For each BEERS2 read retained in the strict RAD comparison:

1. Read the BEERS2 true-origin SAM/BAM record and the suffixed FASTQ read name.
2. Translate the BEERS2 true origin from template coordinates to graph source-path
   segments using the template coordinate map.
3. Match the translated interval and observed read-end sequence to a preclassified
   `template_window_catalog.tsv` row.
4. Evaluate compatibility against the GTF transcript models using the declared
   panCollapse semantics.
5. Apply the collapse manifest.
6. Preserve all compatible canonical targets that remain after collapse.
7. Assign target-relative orientation for each emitted target.
8. Sort targets deterministically.
9. Emit the expected RAD semantic row or the expected non-emitted row.

Reads that cannot be matched from BEERS2 true origin back to a preclassified catalog
window are allowed in exploratory reports, but they are not part of the strict expected
RAD table.

The oracle may output a compact semantic table instead of a binary RAD file. The required
comparison is semantic RAD equivalence:

```text
read name
raw barcode
raw UMI
target set
per-target orientation
target-to-gene map consistency
summary counters
```

A binary expected `map.rad` may be generated only if the project has an independent RAD
writer or a deliberately shared test writer. Binary byte-for-byte equality is not
required unless RAD dictionary order, chunking, and metadata are explicitly pinned.

## 9. Mapping stability rule

panCollapse consumes GAMP, so the fixture must verify that `vg mpmap` produced mapping
evidence consistent with the BEERS2 true-origin oracle and the template-window catalog.

For each retained BEERS2 read, validate the decoded GAMP before comparing RAD:

* read name is preserved;
* record is present unless the case expects mapping failure;
* records for the same read name are adjacent;
* graph node IDs belong to the matching XG;
* projected source path, interval, orientation, and splice evidence match one of the
  expected alignments recorded for that read's catalog class;
* unexpected extra mappings are either part of the expected ambiguity case or are a
  fixture failure.

If `vg mpmap` produces unexpected evidence, do not change the expected RAD table to match
panCollapse. Fix the read, graph, mapping command, or case definition.

Artificial GAMP may bypass this mapping-stability rule only for cases explicitly marked
`artificial_gamp`. Those cases must still have independent expected RAD semantics and
must not be used as evidence that the graph/index/read simulation path works.

## 10. Required version-one cases

Version one should include exact RAD assertions for these cases:

1. positive-strand ordinary exon;
2. negative-strand ordinary exon;
3. exon shared by several transcripts of one gene;
4. transcript-unique splice junction;
5. intronic fragment;
6. exon-intron boundary fragment;
7. intron-exon boundary fragment;
8. retained-intron transcript where applicable;
9. two source transcripts collapsing to one canonical transcript;
10. two source paths or haplotypes collapsing to one canonical transcript without score
    inflation;
11. two canonical targets retained in one RAD record;
12. multi-gene compatible target set retained in one RAD record;
13. target-relative reverse orientation;
14. mixed orientation for the same emitted target, expected to be dropped and counted;
15. read name with valid raw barcode and UMI suffix;
16. original read name containing underscores, parsed from the rightmost two fields;
17. malformed raw molecule identity, expected to be skipped or failed according to the
    chosen panCollapse mode;
18. no-compatible-target read, expected to be absent from RAD and counted.

Optional later cases:

* haplotype-informative SNV;
* haplotype-informative indel;
* cDNA sequencing error that changes mapping or target compatibility;
* paralogous or pseudogene-related ambiguity.

Do not add optional cases until all required version-one cases pass.

## 11. Workflow

### Phase 1: Specification closure

Create and review:

* source lock file;
* input-location manifest with local paths or download URLs;
* tool lock file;
* fixture configuration;
* graph bundle manifest;
* locus table;
* transcript table;
* transcript-template table;
* collapse manifest;
* curated case table;
* expected RAD schemas;
* completion checks.

No read generation should begin until these files have reviewed, non-placeholder values.

### Phase 2: Minimal vertical slice

Build one small BEERS2 smoke subset, for example 500 read pairs from one or two loci.

Demonstrate:

```text
pinned reference inputs
-> extracted reference and annotation
-> identified .xg and associated mpmap indexes
-> validated graph/index bundle
-> pangenome transcriptome templates
-> template-window equivalence catalog
-> BEERS2 molecule packets
-> BEERS2 FASTQ plus true-origin SAM/BAM
-> renamed cDNA FASTQ
-> binary GAMP
-> expected RAD semantic table
-> panCollapse RAD
-> RAD semantic comparison
```

This phase proves interfaces before scaling to the 50,000-read fixture.

### Phase 3: Required curated cases and 50,000-read fixture

Add the required version-one cases from Section 10 and scale the reviewed BEERS2 design
to approximately 50,000 read pairs.

Each case must have:

* known source location;
* manually reviewed expected compatibility;
* expected canonical target set;
* expected orientation;
* expected emitted or non-emitted RAD status.

### Phase 4: Optional population variation

Only after the curated fixture passes, add real phased variants and donor haplotypes.

Keep the same RAD-level validation structure:

```text
known transcript-template origin -> independent expected RAD -> panCollapse RAD comparison
```

Do not add post-RAD validation.

### Phase 5: Optional noisy read cases

Only add sequencing, barcode, or UMI errors when the expected RAD-level consequence is
explicitly defined.

Examples:

* barcode suffix changes, panCollapse preserves observed raw barcode;
* UMI suffix changes, panCollapse preserves observed raw UMI;
* cDNA error causes expected mapping failure;
* cDNA error changes expected compatible targets.

Every injected error must have a recorded expected RAD consequence.

## 12. Completion checks

The fixture is complete when all checks below pass.

### 12.1 Source checks

* Every source has a local path or web download address.
* Every source file has a verified checksum.
* FASTA, GTF, and optional VCF use compatible coordinates.
* Every selected transcript is complete within the extracted reference.
* Collapse manifest source paths and transcript IDs match the annotation.

### 12.2 Graph checks

* The test `.xg` path is explicitly identified.
* Required graph indexes are identified or produced.
* Associated `vg mpmap` indexes are identified and checksum-recorded.
* Graph/index files belong to the same graph build.
* Graph path names match GTF seqnames.
* Selected variants, if used, are present in the graph.
* Graph checksums and graph statistics are recorded before GAMP generation.

### 12.3 Transcriptome template checks

* Every template has a source path, source transcript, template kind, and construction
  method.
* Every template sequence can be regenerated from the graph/reference/annotation inputs.
* Template checksums match the recorded table.
* Template source segments are compatible with GTF coordinates and graph path names.

### 12.4 Read checks

* Every retained read has a BEERS2 true-origin record.
* Every retained read maps from BEERS2 template coordinates back to one known
  pangenome transcriptome template interval or an explicitly documented expected
  ambiguity class.
* Every retained read matches one row in `template_window_catalog.tsv`.
* BEERS2 FASTQ counts and SAM/BAM true-origin records agree after deterministic
  filtering.
* Renamed R2 headers contain the observed raw barcode and UMI.
* Read-name parsing is unambiguous.
* Re-running BEERS2 with the same pinned seed and config produces identical retained
  truth tables and FASTQ records.

### 12.5 GAMP checks

* Output is binary GAMP.
* GAMP is readable with the pinned vg version.
* GAMP and XG belong to the same graph build.
* GAMP records are grouped by read name.
* Renamed read names are preserved.
* Every retained BEERS2 mapping case is present or has a documented expected mapping
  failure.
* Unexpected extra mappings are zero unless the case explicitly expects them.

### 12.6 Expected RAD checks

* Expected RAD records are generated before panCollapse is run.
* Expected target sets are derived from BEERS2 true origins, known transcriptome
  templates, known source locations, GTF models, and the collapse manifest.
* Expected target order is deterministic.
* Expected orientations are present for every expected target.
* Expected non-emitted reads have explicit expected counters.

### 12.7 panCollapse RAD comparison checks

* panCollapse completes with the expected exit status.
* `map.rad`, `tx2gene.tsv`, and run summary are produced when success is expected.
* Parsed RAD records match `truth/expected_rad_records.tsv`.
* Raw barcodes and UMIs match expected observed values.
* Canonical target sets match exactly.
* Per-target orientations match exactly.
* `tx2gene.tsv` is consistent with expected canonical targets and genes.
* Expected non-emitted reads are absent from RAD.
* Summary counters match `truth/expected_non_emitted.tsv`.
* There are no unexplained extra RAD records.
* There are no unexplained missing RAD records.

## 13. Agent rules

An implementation agent must follow these rules:

* Work one phase at a time.
* Do not generate downstream alevin-fry outputs.
* Do not create or validate count matrices.
* Do not use panCollapse output to define expected RAD records.
* Use BEERS2 as the primary read and library simulator.
* Use custom code only to build annotated pangenome templates, BEERS2 molecule files,
  suffix maps, and independent RAD oracle tables.
* Prefer BEERS2 reads aligned with `vg mpmap` over artificial GAMP.
* Use artificial GAMP only for explicitly marked narrow fallback tests.
* Follow the annotated-pangenome-to-BEERS2 algorithm in Section 5.1.
* Record every input file location as either a local path or a web download address.
* Do not silently change loci, transcripts, reads, or expected target sets when mapping
  is unexpected.
* Do not infer biological correctness for ambiguous molecules beyond the explicit RAD
  oracle.
* Keep generated large artifacts outside the source tree.
* Record command lines, tool versions, input checksums, output checksums, and seeds.
* Stop for review when a biological or product judgment is needed.

## 14. Definition of implementation-ready

The plan is ready for implementation only after these files contain reviewed,
non-placeholder values:

```text
validation/rad_fixture/config/sources.lock.yaml
validation/rad_fixture/config/input_locations.tsv
validation/rad_fixture/config/tools.lock.yaml
validation/rad_fixture/config/fixture.yaml
validation/rad_fixture/config/beers2.config.yaml
validation/rad_fixture/metadata/graph_bundle.tsv
validation/rad_fixture/metadata/loci.tsv
validation/rad_fixture/metadata/transcripts.tsv
validation/rad_fixture/reference/collapse_manifest.tsv
validation/rad_fixture/truth/transcript_templates.tsv
validation/rad_fixture/truth/template_window_catalog.tsv
validation/rad_fixture/truth/beers2_molecules.tsv
validation/rad_fixture/truth/beers2_read_origins.tsv
validation/rad_fixture/truth/expected_alignment_classes.tsv
validation/rad_fixture/truth/curated_cases.tsv
validation/rad_fixture/truth/expected_rad_records.tsv
validation/rad_fixture/truth/expected_non_emitted.tsv
```

Until those values are pinned, an agent may build scaffolding, schemas, validators, and
RAD parsing utilities, but must not choose biological sources, loci, transcript sets,
or expected RAD semantics on behalf of the project.
