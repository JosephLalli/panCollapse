# Input and Output Contract

This document translates the product specification into interface obligations without
prescribing the code architecture. The GAMP-to-RAD algorithm is defined by D048 and
`docs/conversion-algorithm.md`.

## Native count interface (v0.10)

**Current D110 interface:** `count` accepts name-grouped GAMP, a complete
direct-facts `--annotation-index`, a barcode whitelist, profile selectors and
an output directory. It does not accept XG/GBWT/RI inputs or earlier annotation
catalogs/bindings as runtime substitutes. Compile those once with `index` using
the [index guide](annotation-index.md). There is no runtime reconstruction
fallback. Parquet and optional `--10x-mex` are the active count sinks; RAD/BAM
remain on deprecated `convert`, and count replay/per-read export are retired.
Only the assignment cache remains configurable; obsolete reference-cache
limits fail rather than being ignored. Earlier preparation/replay contracts
below describe superseded interfaces, not alternate active count routes.

`panCollapse count` requires GAMP, a graph input, a barcode whitelist, at least one frozen profile, and
exactly one count-fact authority. The command refuses external legacy identity/policy, BAM,
debug, compact-BAM, and allowlist options, and the convert-only policy switches `--strand`,
`--count-mode`, `--bam-multigene`, and `--no-ex50-score-window`. Its default output is an
atomic Parquet dataset and manifest; `count` has no BAM sink and no production Python
dependency. `--10x-mex` and `--rad-out` add compatibility outputs.

Count aggregation, barcode correction and UMI reduction are memory-only (D100).
There is no aggregate, corrected-observation or single-barcode disk fallback,
and `--count-memory-budget` is rejected with a migration error. An external
whole-process memory limit covers reference initialization and counting; RAM
exhaustion fails rather than changing storage mode. `manifest.json` records
`count_storage: "memory-only"`; legacy aggregate-spill counters remain zero for
summary compatibility. This does not prohibit compressed final outputs or
explicitly requested diagnostic staging.

`--read-assignments-out <relative/path.parquet>` optionally adds ordered per-read diagnostic
Parquet. The path must be normalized, relative to `--out-dir`, end in `.parquet`, and not
collide with a reserved result path. It is opt-in because it is the only count sink with
per-read-scale I/O.

### Count-fact authority

Exactly one of `--count-bundle` and `--annotation` is required; supplying both, or neither, is a
usage error. Both routes build the same in-memory Parent, gene, and per-profile gene facts.

`--count-bundle <dir>` reads a `panSC-count-facts-v1` bundle, authoritative for path identity,
profile-specific gene policy, Parent categories, projected-nested policy, strong-support facts,
and provenance receipts. Per D085 five of its nine roles are optional and none of them is opened
by the runtime: `corrected_annotation`, `parent_category_receipt`, `projected_nested_policy`, and
the two profile policy receipts are validated when present and otherwise absent. The four roles
the runtime reads are `path_identity_ledger`, `profile_gene_policy`, `parent_category_ledger`,
and `strong_support_ledger`. On this route
`--path-identity-ledger` is rejected, because the bundle supplies the ledger.

`--annotation <corrected.gff3>` reads the correction chain's classifier verdicts directly off a
GFF3, and requires two companions:

- `--annotation-reference <reference.gff3[.gz]>` — the reference annotation whose gene rows carry
  the tags CAT drops when it projects a model. They are inherited back onto each projected model
  through `source_gene`, matched with the Ensembl version suffix stripped. The inheritance is not
  optional: omitting it under-tags the annotation silently rather than failing.
- `--path-identity-ledger <ledger.tsv>` — required on this route and only on this route, since
  there is no bundle to carry it.

Either annotation file may be gzipped. Recognized categories are the twelve selected tags, and
gene-type policy is the 10x 2024-A allowlist with `count_cr.py`'s demotion vocabulary; on this
route both are compiled into the binary rather than read from bundle tables. Two facts the
annotation does not state are consequently absent: `protected_primary_protein_nested_host` is
always `false`, and `nested_host` is read only from a `nested_host_gene` attribute on a **gene**
row. `scripts/tag_nested_reference_hosts.py` writes that attribute onto every row carrying a
matching `source_gene`, which on a CAT projection is the transcript rows; whether gene rows also
receive it, which they do: `nested_host_gene` is present on 39,108 gene rows of the chr20/21/22
fixture annotation. An annotation that has not been through the nested-host pass carries no
nested-host relation at all, silently. See D088, which records this route and its outstanding
validation conditions. The `--compatibility-in` replay path does not support it: replay loads a
bundle unconditionally, so `--annotation` is a GAMP/graph-only route and the combination is refused
as a usage error.

To reach a corrected annotation from a stock reference GTF, see
[`scripts/README.md`](../scripts/README.md).


`--compatibility-out <dir>` adds an atomically published compatibility dataset and manifest. Its
deterministic, ordinal-ordered Parquet rows retain every input group, including invalid-molecule
and featureless groups:
read ordinal, molecule identity/status, and an interned `fact_set_id`; the fact-set table holds
canonical exact E/P/B and structural S/U facts (the latter reconstruct current Gene fallback)
with path, Parent, and canonical transcript provenance. The manifest pins schema/version,
canonical logical hashes, output hashes, source digests, the independently accumulated
input-group denominator and molecule-status totals, and structural/compatibility-algorithm
identity. This is a non-BAM artifact:
it contains no alignment records or duplicated alignment payload. Read and fact tables are
published in row-, UTF-8-byte-, and structural-list-value-bounded Parquet batches so a large
compatibility dictionary never requires an oversized Arrow data or offset array.

`--compatibility-in <dir>` replaces GAMP and all graph inputs and requires the current count-facts bundle and
whitelist. Replay is a bundle-route feature; `--annotation` is not wired into it. Exactly one input mode is legal: GAMP with one graph source, or compatibility input. The reader validates
all hashes, schema, denominator, and ledger structural/algorithm compatibility before using the
interned facts for fast resolution and scanning the complete read rows for matrix/QC accounting.
It permits metadata/tagging/whitelist/count-policy replay, but rejects any mismatch that could
change the ledger transcript/exon surface or compatibility semantics. The source graph-input digests remain
frozen in the manifest, but replay does not open a graph and therefore does not independently compare
graph topology to a current graph. A graph change requires fresh compatibility production.

The bundle's `MANIFEST.json` binds its `content` object to `content_id`, which is
`sha256:` followed by the hex SHA-256 of the canonical serialization of `content`: object
keys sorted by code point, no whitespace, integers in decimal, floats in shortest
round-trip form, and strings ASCII-escaped (`\uXXXX`, surrogate pairs above the BMP). This
is byte-identical to Python's `json.dumps(content, sort_keys=True, separators=(",", ":"))`;
the key order and whitespace of the file itself do not affect the identity. The loader parses
standard JSON only: non-finite floats, integers outside the signed or unsigned 64-bit range,
nesting deeper than 1024 levels, and duplicate object keys are rejected before the identity is
compared.

Profiles are selected independently: `--cr7` is the `cr7-v1` alias,
`--pansc-strict-v1` is the identically named frozen custom profile alias, and repeatable
`--profile ID` is the extensible form. At least one selector is required, duplicates are errors,
and several profiles may execute in the same GAMP pass. Repeatable typed
`--profile-override BASE:FIELD=VALUE` is accepted only with
`--analysis-scope sensitivity-analysis`. The v0.10 override surface is limited to assignment
policy: `strand`, `score-window`, `membership-class`, `novel-paralog-policy`,
`excluded-categories`, `post-resolution-fallback-categories`, `tagged-last-resort`,
`exact-strand-gene-fallback`, `nested-host-rule`, `nested-host-action`,
`body-support-ratio`, `body-support-gene-types`,
`strong-local-support-exemption`, `strong-support-clearable-categories`, and
`protect-primary-protein-nested-hosts`.
Barcode correction and UMI algorithms are not overridable. Any override creates a
`derived-<base>-<hash-prefix>` ID whose full effective-profile SHA-256 is recorded in the
manifest; it can never be reported as the frozen base profile.

The default dataset contains the following non-nullable fields unless marked nullable:

- `parquet/barcodes.parquet`: `barcode_index:uint32`, `barcode:string`,
  `exact_read_prior:uint64`;
- `parquet/features.parquet`: `feature_index:uint32`, `gene_id:string`,
  `gene_name:string nullable`, `gene_type:string nullable`;
- `parquet/counts.parquet`: `profile_id:string`, `barcode_index:uint32`,
  `feature_index:uint32`, `umi_count:uint64`;
- `parquet/molecules.parquet`: `profile_id:string`, `barcode_index:uint32`,
  `feature_index:uint32`, `corrected_umi:string`, `supporting_read_count:uint64`,
  `raw_umis_collapsed:uint64`.

Barcode and feature tables are the lexical union of nonempty rows used by any requested profile;
their compact integer dictionaries are referenced by count and molecule rows. Counts are sorted
by profile, barcode, and feature; molecules add UMI as the final sort key. Writers use the stored
Arrow schema, Parquet 2.6, fixed row-group sizing, and Zstandard level 3 without volatile
timestamps. Because future Arrow versions may encode equivalent tables differently,
`manifest.json` records library-independent logical hashes over canonical rows as well as the
current output-file hashes. It also records which authority produced the facts:
`fact_bundle_content_id` holds the identity of those facts, and `fact_authority` names the route
that produced it, `count-bundle` or `annotation`. Both are required and validated non-empty
before any output is written. On the bundle route the id is the bundle's published `content_id`.
On the annotation route it is `sha256:` followed by a digest over the SHA-256 digests of the
corrected annotation, the reference annotation, and the path-identity ledger, in that order, so a
reader must consult `fact_authority` before treating the id as a bundle address. The `inputs` map
records `count_bundle_manifest` on the bundle route, and `annotation`, `annotation_reference`, and
`path_identity_ledger` on the annotation route. `summary.tsv` records stable global and per-profile counters. Barcode
prior/correction totals are counted once globally because they precede profile evaluation; the
corresponding per-profile counters remain available for joint-versus-separate audit. The
entire staged directory is published atomically only after all requested outputs close and
validate.

For count and molecule tables, logical hashing decodes compact dictionary indexes back to barcode
and gene strings. The manifest also records a count hash and molecule hash for each profile over
those decoded values. Corresponding per-profile hashes are therefore directly comparable between
a joint multi-profile pass and separate single-profile runs, even when their active dictionary
unions—and therefore their stored numeric indexes—differ.

`--10x-mex` adds one `<out-dir>/mex/<profile_id>/raw_feature_bc_matrix` directory per profile,
containing `matrix.mtx.gz`, `features.tsv.gz`, and `barcodes.tsv.gz`. Matrix rows are features,
columns are barcodes, entries are raw UMI counts, and the dictionary order agrees with the
Parquet tables; cell calling remains downstream. `--rad-out` invokes the existing pre-count RAD
writer once and does not render either profile's final matrix.

The diagnostic sidecar records input ordinal, QNAME, profile, raw/corrected barcode, UMI,
terminal class, selected gene/tier, and reason bits. QNAME is the read-name prefix before
the last two underscore-separated fields (raw CB and UMI, after any quality suffix), including
on `invalid_raw_molecule` rows; a name with fewer than two such fields, or an empty prefix,
records the raw GAMP name instead. It is an audit product, not an input to
counting or a default production artifact.

Native GAMP counting writes worker-private diagnostic streams and merges them after count
finalization. The final sidecar remains ordered by input ordinal and profile index. Merge
fan-in is at most 32 streams, with intermediate merges for larger worker counts; every
stream is checksum/trailer validated, and every expected read/profile pair must appear
exactly once. Diagnostic output alone does not impose a worker ordering barrier. RAD and
compatibility capture retain their existing ordering requirements.

Native-count manifest `timings` separates `read_processing_seconds`,
`count_finalize_seconds`, `diagnostics_merge_export_seconds`, and optional-work
`compatibility_finalize_seconds`. The existing `processing_seconds` remains the aggregate
of traversal and finalization. Read processing includes requested diagnostic-row generation
and spooling; diagnostic merging/export is measured afterward. Measure core counting with
`--read-assignments-out` omitted. Initialization remains separate; stderr additionally
reports count-output publication time, and whole-command wall/RSS come from an external
measurement such as GNU time. Compatibility replay retains its existing timing fields.

## Runtime inputs

### GAMP

- Binary VG multipath alignment stream.
- Records for a read name are contiguous; the converter validates recurrence of a
  previously completed name.
- The GAMP name field carries the observed raw molecule identity as
  `<original_read_name>_<raw_CB>_<raw_UMI>_cy<hex(CY)>_uy<hex(UY)>`. Raw barcode, UMI, and optional
  barcode quality are parsed from the right. Legacy names without the quality suffix remain
  accepted. `convert` preserves those values; `count` corrects them only after building the
  all-valid-read exact-whitelist prior.
- Parsed raw barcode and UMI values must match the configured lengths (`--raw-cb-length`,
  `--raw-umi-length`; Phase 2 defaults 16 and 12).
- Every alignment of the read (the primary record and each supplementary/secondary record)
  is scored. Each aligned node is scored under vg's own alignment scheme, recomputed from
  the `Mapping` edits (match/mismatch, affine gaps, read-end bonus). There is no
  complete-traversal enumeration and no traversal cap.

### Graph with HST paths

Both `convert` and GAMP-based `count` accept exactly one of:

- `--xg transcripts.xg`: existing embedded-path XG behavior, unchanged by default.
- `--gbz transcripts.gbz`: a GBZ supplying the node graph and transcript GBWT.
- `--xg graph_only.xg --gbwt transcripts.gbwt`: XG supplies nodes and sequences; GBWT
  supplies paths, even if the XG also contains paths.

`--gbwt` alone and any combination of `--gbz` with `--xg` or `--gbwt` are errors. Replay
rejects all graph options. The GAMP and graph must use the same node IDs and sequences;
missing aligned nodes abort counting. Equal node counts alone do not establish identity.
GBZs with node-to-segment translations are rejected; translation is not applied implicitly.
A GBZ retains only nodes visited by its GBWT paths. Its paths must therefore also cover nodes
that a GAMP may align to without transcript membership; unselected guide/retention paths can
provide this coverage. An aligned node missing from GBZ remains a hard error.
Counting uses path walks and GAMP adjacency, so it does not require edges absent from every path.

For GBWT-backed inputs, the index must be bidirectional and carry complete path, sample and
contig metadata. Names are composed using VG/GBWTGraph path-sense rules and must exactly
match selected `vg_path_name` values; there is no automatic suffix stripping or reference-sense
remapping. Ambiguous selected names, missing ledger paths and incorrect path lengths are fatal.
Unselected extra GBWT paths do not contribute evidence. Every GBWT node must exist in the node
graph; the tool cannot infer which sequence was originally assigned that ID during mapping.

Standard FastLocate is constructed in memory from the loaded GBWT unless native count
explicitly supplies `--r-index FILE --r-index-manifest FILE` with XG+GBWT. The strict
`pancollapse-r-index-binding-v1` sidecar contains exactly the schema, GBWT SHA-256/size,
and RI SHA-256/size fields. Both identities are checked in their load streams by default.
Explicit `--trust-r-index-manifest` uses the recorded RI digest without recomputing it,
while preserving GBWT verification, actual RI-size checks, native framing/structure checks
and a sanity query. Its manifest input is marked `trusted-preparation-receipt` and
`sha256_verified:false`. This controlled-local-file mode cannot detect same-size RI
corruption. Invalid input never falls back to construction. RI requires native count with
XG+GBWT; GBZ, replay and convert reject the option. Occurrence-level queries
preserve repeated visits, strand and step offsets. Exact-overlap modes, including native count,
retain in-memory base-position checkpoints for selected paths at step zero, every 64 steps, and
the terminal step, using checked 32-bit storage with a 64-bit fallback. At most 63 LF steps
recover an exact coordinate. Membership-only modes omit coordinate storage; a request for
unavailable positions fails explicitly. Position checkpoints are discarded at exit. RI is
not auto-detected and does not persist selected-path checkpoints or exact model geometry.

Count manifests and compatibility provenance truthfully identify `xg`, `gbz`, or `xg` plus
`gbwt`; graph identity never enters scientific tables or RAD. Scientific equivalence uses the
[explicit comparison contract](research/graph-input-scientific-comparison-20260926.md).
GCSA/LCP, distance, snarls and minimizer indexes remain unnecessary after mapping.

### Production path identity ledger

`--path-identity-ledger` accepts only the headered schema `panSC-path-identity-v1`, one row per
emitted vg path. Required columns are:

`schema_version`, `vg_path_name`, `vg_path_length`, `vg_haplotype_origins`, `unique_parent`,
`source_parent`, `input_parent`, `canonical_transcript`, `gene_id`, `source_path_or_contig`,
`sample`, `haplotype`, `annotation_source`, `feature_layer`, `selection_status`,
`fallback_status`, `exon_unique_parent`, `gene_locus`, `source_gene`, `source_transcript`,
`transcript_class`, `strand`, `start`, and `end`.

That header and order are exact; missing, extra, duplicated, or reordered columns are rejected so
the consumer cannot drift from the panSC writer contract.

All required fields are nonempty; `feature_layer` is `exon` or `body`, strand is `+` or `-`,
coordinates and path length are positive integers with `start <= end`, and canonical transcript
and gene are resolved identities rather than `N/A`. `unique_parent` is globally layer-disjoint
and has one annotation identity, while allowing several emitted paths with that identical Parent
identity. A canonical transcript maps to one gene. Exon rows self-reference
`exon_unique_parent`; each body row crosslinks to an existing exon Parent with the same canonical
transcript and gene. For `source_transcript=N/A`, fallback is explicit and the canonical identity
is the exon Parent (`unique_parent` on exon rows, `exon_unique_parent` on body rows).

For native `count` and replay only, D097 allows a canonical label associated with
different explicit normalized source-gene IDs to form separate internal targets.
Each `(canonical transcript, normalized source gene)` pair must map to one raw
gene ID. These labels and the input files are not rewritten; optional RAD uses
unique length-framed target keys. The manifest records `transcript_target_scheme`,
`qualified_transcript_targets`, and `transcript_target_mapping_id`. Split replay
surfaces hash the source-gene partition, while unsplit v1 hashes stay unchanged.
Legacy t2g assertions are rejected explicitly for split targets. `convert` keeps
the original single-gene canonical rule.

Every `vg_path_name` must exist exactly in the XG and have the recorded `vg_path_length`; extra
non-ledger genomic XG paths are allowed. Names are opaque, so `_H<n>`/`_R<n>` are never stripped.
Tabs/newlines are illegal in all fields. Because BAM provenance uses semicolon-separated groups
and comma-separated ties, those delimiters are reserved in `vg_path_name` and `unique_parent`;
semicolon is also reserved in `canonical_transcript` and `gene_id`. Commas remain legal in
general provenance, including `vg_haplotype_origins`.

### Explicit legacy adapter

Historical `--t2g` and `--body-t2g` inputs require `--legacy-adapter hst-v1`; no file-shape
auto-detection exists. The exon t2g is
`graph_path<TAB>gene_id[<TAB>canonical_transcript_id]`. Its two-column form strips a terminal
`_H<n>`/`_R<n>`; column 3 is an exact raw-path alias. Conflicting path/transcript or
transcript/gene mappings remain hard errors.

A GTF and `vg rna` build the annotated graph during reference/fixture creation only; they
are not runtime inputs.

### Count mode and body identity

The default `--count-mode score` uses exon rows. The ledger count modes (`gene`, `genefull`,
`genefull_exonoverintron`, `genefull_ex50pas`) require body rows in the production path ledger.
When a production ledger count mode also requests `--bam-out`, the versioned typed union requires
at least one body row linked to every exon Parent. This completeness check is mode-specific:
score mode and non-BAM RAD conversion do not promise exact Ex50 evidence and therefore do not
impose it.
Under `hst-v1`, the first three additionally take `--body-t2g`:

- two columns, `path<TAB>gene`, preserve the legacy gene-body/span classifier;
- three columns, `raw_body_path<TAB>gene<TAB>canonical_transcript`, enable D063's
  transcript-first classifier. Every row in a body t2g must use the same width. Its canonical
  transcript must occur in the exon t2g with the same gene. Multiple raw body paths, including
  haplotype copies or fragment-local segments, may map to one transcript and MAX-collapse.

Exact `genefull_ex50pas` is production-only and requires `--bam-out --bam-multigene all`;
`hst-v1` is a hard error. It emits exact base-overlap evidence rather than the S/U ledger:
parallel `TX`/`GX`/`GD`/`GT`/`XP`/`XU`, with one exact exon path/Parent for E/P or linked body
path/Parent for B per slot. Exactly 50% exonic is B, and a fully exonic but splice-discordant
alignment is P. Repeated canonical `TX` values are valid because locus Parents and alignment
alternatives are not collapsed before count_cr's six-rank selection.
By default, only complete compatible alternatives within five score points of the global
compatible optimum reach that selection. `--no-ex50-score-window` restores all complete compatible
alternatives for a pre-D068 sensitivity comparison. The BAM header and `summary.tsv` record which
policy was used.

Production and legacy annotation name paths already embedded in the graph; there is no node->gene
map. A transcript-first count uses `--bam-out --bam-multigene all` so the complete transcript
ledger reaches count_cr, which performs transcript-to-gene pooling. See
[`genefull.md`](genefull.md).

### Barcode and UMI

The raw cell barcode and raw UMI come from the GAMP name field. `convert` writes them to RAD or
BAM as observed and performs no correction or deduplication. Native `count` instead updates the
dataset-wide exact-whitelist prior from every valid read group, including featureless groups;
applies the frozen Hamming-distance-one quality posterior and 0.975 acceptance threshold at EOF;
then performs the frozen basic-UMI filters, non-transitive `1MM_CR`, and
`MultiGeneUMI_CR` maximum-support/tie-discard rule. A read rejected before barcode correction or
before its profile reaches the UMI stage cannot contribute an assigned-read or UMI-drop counter.
Missing, malformed, or unsupported raw molecule fields (including configured-length mismatch)
are skipped and counted by default; `--molecule-identity-failures fail` makes them fatal.

## Outputs

### RAD

- Mapper-style, uncollated RAD with a target dictionary of the emitted transcript IDs.
- Raw cell barcode and raw UMI encoded in the schema alevin-fry expects.
- In production, exact paths MAX-collapse within `unique_parent`, Parents MAX-collapse within
  `canonical_transcript`, and canonical transcripts tied at the global top score become RAD
  targets. Scores are never summed; all tied winning paths and Parents remain BAM provenance.
  `hst-v1` preserves D062/D063's tested selection behavior.
- One orientation value per emitted target: forward if the read runs along the HST path,
  reverse if opposite; on disagreement among a transcript's winning HSTs, the majority of
  aligned bases decides (deterministic forward fallback for an exact tie).
- No transcript likelihood weights and no splicing-state labels.
- Streaming RAD-to-disk writing (D049): the header, target dictionary, and tags are written
  up front with a placeholder `num_chunks`; records stream to `map.rad` as groups flush; and
  the writer seeks back at finalize to patch the exact chunk byte/record counts and
  `num_chunks`. A run with no emitted record leaves a header-only file (`num_chunks = 0`).
- Byte-identical output for identical inputs/configuration at supported worker counts. Thread count
  is operational and is omitted from BAM `@PG CL`; one parser owns complete-name grouping and one
  writer preserves input order. This guarantee applies fully to the default flat scorer, which is integer
  arithmetic throughout and produces bit-for-bit identical RAD on any conforming platform.
  Under `--score qualadj`, the score matrix and full-length-bonus table are constructed with
  `std::exp`, `std::log`, `std::pow`, and `std::round` (`src/pathtally_qualadj.hpp`
  `build_matrix`/`build_bonuses`); the last-bit rounding of those libm functions can differ
  across platforms or libm versions. A difference of one in a matrix entry can flip a score
  tie and change RAD bytes across machines. Within one machine and build, qualadj output
  remains deterministic.
- GAMP read groups whose records all have empty subpaths are unaligned: no RAD record, and
  `unaligned_reads` is incremented. With `--bam-out`, every valid molecule group still emits
  exactly one BAM record for downstream barcode correction; featureless groups use the
  unmapped `XB:Z:barcode_only` layout below.

Conceptually each record contains `bc`, `umi`, `refs`, and `dirs`; `refs` is the target
compatibility set (not coordinates) and `dirs` is parallel to `refs`, consumed by
alevin-fry expected-orientation filtering.

### Companion artifacts

- `tx2gene.tsv` from the ledger projection (or legacy t2g);
- configured raw barcode and UMI lengths, which set RAD `cblen` and `ulen`;
- a run summary with the product-spec Section 12 counters, including groups skipped and
  unaligned read groups, `strand_filtered_groups` (reads dropped by `--strand`) and
  `multigene_dropped_groups` (reads dropped by the ledger `Unique` rule), `bam_records`, and
  `barcode_only_bam_records`; the field
  `emitted_target_count_histogram` holds a semicolon-separated list of
  `target_count:group_count` pairs (ascending by target count) recording how many emitted
  groups had each distinct number of compatible targets;
- version/build information and input identities/checksums where practical.

### Optional BAM (`--bam-out`)

An opt-in BAM for a CellRanger-style counting stack. It does not change the RAD: `map.rad` is
byte-identical with or without `--bam-out`. Every valid input read group has exactly one BAM
record. Feature-bearing records are mapped nominally to a synthetic gene contig and carry
`CB`/`CR`, `UB`/`UR`, optional raw qualities `CY`/`UY`, plus the feature ledger. A group with
no BAM count feature instead emits one unmapped `XB:Z:barcode_only` record with only molecule
tags. That record contributes to STARsolo's pre-feature barcode-abundance prior and must never
be treated as gene evidence. Production-ledger feature records additionally carry `TX` and its
parallel `XP`/`XU` provenance groups. Ordinary count modes carry `GL`; exact
`genefull_ex50pas` carries `GT` instead. Full contract in
[`bam-export.md`](bam-export.md).

## Exit behavior

Hard failure is expected for:

- unreadable or incompatible inputs;
- GAMP name-grouping violations (a completed name recurs);
- a GAMP/xg node-id-space mismatch: if any aligned node ID is absent from the graph the run aborts
  with a non-zero exit; a node that exists but lies on no HST path is legitimate and produces no
  target hits for that node (not an error);
- a production ledger with missing/invalid fields, identity conflicts, layer/crosslink errors,
  reserved identifier delimiters, an XG-missing exact path, or an XG path-length mismatch;
- a graph missing the HST paths, or an explicit `hst-v1` t2g inconsistent with the emitted
  transcripts;
- inability to encode standards-conformant RAD.

Skippable per-read conditions are missing, malformed, or unsupported raw CB/UMI values, or a
malformed encoded barcode-quality suffix; they
are counted (`raw_molecule_missing_groups`, `raw_molecule_malformed_groups`,
`raw_molecule_unsupported_groups`, `raw_molecule_skipped_groups`) and become fatal under
`--molecule-identity-failures fail`. Unaligned read groups (all-empty subpaths) are not
molecule-identity failures: no record, counted under `no_compatible_transcript_groups` and
`unaligned_reads`.

GAMP input streams from a file or from stdin via `--gamp -` (D049), so `vg mpmap ... |
panCollapse convert --gamp - ...` works with the same grouping/recurrence validation. A
stdout RAD mode is not in the active CLI, because the seek-and-backpatch writer needs a
seekable output file.

## Compiled annotation identities and facts (D099/D103)

`panCollapse index --annotation A --annotation-reference R --path-identity-ledger L
--out annotation.pci` produces `pancollapse-annotation-index-v3` (`PCANIDX3`) with
both profiles. GFF3 is the default; select GTF using `--annotation-format gtf` /
`--annotation-reference-format gtf`. Inputs may be plain/gzip/BGZF. GTF requires
explicit gene/transcript features and exact ledger Parent IDs.

`count --annotation-index annotation.pci` is exclusive
with `--annotation` and `--count-bundle`; replay is unsupported on this route.
Source annotations and the TSV ledger are not needed after indexing. Original source fact identity
is retained; manifests add `annotation_index_derivation_id` and artifact identity.
Integrity, source/ledger binding and numeric-reference checks precede GAMP. Independent
reference, XG and GBWT/RI loads overlap at three or more threads, then cross-input validation
runs before evidence processing. The binary includes compiled ledger identities, but excludes
FastLocate, positions and geometry. Legacy PCANIDX2 still requires its external TSV; upgrade
once with `index --from-annotation-index old-v2.pci --path-identity-ledger L --out reference.pci`.
Embedded-ledger receipts retain the original TSV identity without reopening it. See
[annotation indexing](annotation-index.md) for commands and limits.
