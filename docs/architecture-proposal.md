# Architecture Proposal

**Status:** Proposed for Gate Architecture Approved. At the end of Phase 0, no
production source, build files, fixtures, or generated outputs were included.

Phase 1 has since added the focused behavior contracts and CMake/CTest smoke skeleton
listed in `PROGRESS.md` and `docs/phase1/`.

> **Algorithm superseded by D048.** The active GAMP-to-RAD algorithm is
> `docs/conversion-algorithm.md`. The data-flow and compatibility sections below describe the
> earlier traversal-enumeration/GTF-projection design and are Phase 0 history; the
> dependency-boundary and VG-linkage material remains current.

## Scope and dependency boundary

V1 is a standalone C++20 converter over existing mapper outputs. It does not build VG,
create mapping indexes, create a panCollapse-specific persistent index, surject through
BAM, or introduce a RAD library dependency at runtime.

The VG programming wiki's ecosystem-library guidance supports using public vg-adjacent
interfaces and libraries instead of broad `vg/src` internals. For V1, that means
protobuf GAMP streaming, handlegraph path-position APIs, and XG, not vendoring libbdsg,
GBWTGraph, or the full VG source tree.

The supported VG boundary is the local checkout/build family:

- source: `/mnt/ssd/lalli/usr/local/src/vg`
- binary: `/mnt/ssd/lalli/usr/local/bin/vg`
- version: `v1.75.0-68-ge82694b69 "Spike"`
- commit: `e82694b699988205d5105231240db000f9655335`
- describe: `v1.75.0-68-ge82694b69-dirty`
- compiler/runtime: GCC 15.2.0, libstd++ 20250808
- HTSlib: headers 101990, library 1.19.1-29-g3cfe8769

The dirty checkout and dependency tree mean compatibility is scoped to this build family,
not to arbitrary VG installations with the same release tag. Phase 1 must start with a
compile/link smoke test before broader behavior work.

Future CMake work, after this gate, should discover the local build with:

```text
CMAKE_PREFIX_PATH=/mnt/ssd/lalli/usr/local/src/vg
PKG_CONFIG_PATH=/mnt/ssd/lalli/usr/local/src/vg/lib/pkgconfig
```

The smoke-validated link boundary is `find_package(VGio)`, `find_package(libhandlegraph)`,
`VGio::VGio`, the exported libhandlegraph target, and a local imported static target for
`/mnt/ssd/lalli/usr/local/src/vg/lib/libxg.a` with
`/mnt/ssd/lalli/usr/local/src/vg/include`. A throwaway Phase 0 probe outside the repo
linked `VGio::VGio`, `libhandlegraph::handlegraph_shared`, `libxg.a`,
`PkgConfig::SDSL`, `PkgConfig::DIVSUFSORT`, `PkgConfig::DIVSUFSORT64`,
`OpenMP::OpenMP_CXX`, `pthread`, `m`, and `atomic`, then instantiated
`vg::MultipathAlignment` and `xg::XG`. Phase 1 recreated this as tracked build smoke
before behavior implementation. Phase 1 projection smoke additionally found that
protobuf repeated-field access from local headers requires the Abseil
`absl_log_internal_check_op` pkg-config target and its transitives. Avoid full `libvg.a`
and VG internal
`multipath_alignment_t` unless that smoke proves the narrower boundary impossible.

## Exact V1 inputs

Required converter inputs:

- name-grouped single-end GAMP from the same graph/node-id space used for coordinate
  lookup;
- `.xg` for that graph, implementing `handlegraph::PathPositionHandleGraph` with visible
  source coordinate paths;
- GTF annotation;
- collapse manifest keyed to source transcript identities: exact `.xg` source path names
  plus source transcript IDs from the GTF;
- output directory.

The manifest is a required headered TSV:

```text
source_path_name<TAB>source_transcript_id<TAB>canonical_transcript_id<TAB>canonical_gene_id
```

Fields may not contain tabs or newlines. Each source transcript model is keyed by
`(source_path_name, source_transcript_id)`. `source_path_name` must exactly match both a
visible `.xg` path name and the GTF seqname for that source coordinate system.
`source_transcript_id` must match the configured GTF `transcript_id` attribute on exon
records. Contradictory duplicates for one source key are errors; repeated identical rows
are diagnostic-only duplicates. One canonical transcript mapped to multiple genes is an
error. Identity mappings are valid only when explicitly listed.

The required `.xg` paths are source-coordinate paths used by the GTF, such as reference,
haplotype, or copy paths. They are not GBZ/GBWT-only compressed paths and they are not
mature spliced transcript paths for V1 compatibility evaluation.

The expected upstream `vg mpmap` artifact set for RNA/splice-variation graphs commonly
uses `-x graph.xg -g graph.gcsa -d graph.dist` to produce `reads.gamp`. The panCollapse
converter boundary begins after that mapping step: it requires `reads.gamp` and
`graph.xg`, and excludes `.gcsa`/`.gcsa.lcp`, `.dist`, `.snarls`, and minimizer indexes
from the V1 converter inputs. GBZ/GBWT is a separate future coordinate-index question,
not part of this V1 `mpmap` artifact contract, and is not accepted as a sole replacement
for `.xg` unless a later decision proves equivalent path-position behavior.

V1 does not add `.gcsa` or `.dist` provenance CLI flags. The run summary records the
actual panCollapse input identities: GAMP, XG, GTF, manifest, configuration, and build
version. Mapper-side index provenance remains the responsibility of the upstream mapping
workflow unless a later metadata-file input is explicitly approved.

## Proposed CLI

```text
panCollapse convert \
  --gamp reads.gamp \
  --xg graph.xg \
  --gtf annotation.gtf \
  --collapse-manifest collapse.tsv \
  --out-dir out \
  [--assignment all] \
  [--score-window N] \
  [--min-splice-jump N] \
  [--max-traversals-per-read N] \
  [--raw-cb-length N] \
  [--raw-umi-length N]
```

panCollapse does not expose a strand-filtering option; library-orientation filtering is
handled downstream by alevin-fry expected-orientation settings using the RAD `dirs`
values panCollapse preserves. Defaults: `--assignment all`, `--score-window 0`,
`--min-splice-jump 20`,
`--max-traversals-per-read 100000`, `--raw-cb-length 16`, `--raw-umi-length 12`, and
deterministic single-writer output. Tag-source and tag-name overrides are not part of the
active V1 barcode/UMI source contract. D045 defers a `--threads` production surface for
now.

## Data flow

1. Load and validate the manifest. Build a deterministic RAD target dictionary from
   lexicographically sorted canonical transcript IDs and emit a `tx2gene.tsv` row for
   every target.
2. Parse GTF into in-memory transcript structures using GTF/path coordinates as
   canonical. Convert 1-based inclusive GTF intervals to 0-based half-open intervals.
3. Open `.xg` as `xg::XG` and use it through `handlegraph::PathPositionHandleGraph`.
4. Stream GAMP with `vg::io::for_each<vg::MultipathAlignment>` and form adjacent
   read-name groups. Track completed names and fail if a name recurs later.
5. For each read group, extract raw CB/UMI from the GAMP name field, enumerate complete
   compatible traversals, project aligned reference-consuming spans to source paths, evaluate
   compatibility for source transcript identities, collapse source transcript identities
   to canonical transcripts, score targets, preserve all retained targets, sort/deduplicate
   retained target IDs, and enqueue a RAD record.
6. Commit records to `out/map.rad` in input group sequence and write stable summary
   diagnostics.

## GAMP grouping and traversal

For the current single-end `vg mpmap -F GAMP` implementation, all alignments emitted for
one input read are contiguous even with mapper threads, but global read order is not
guaranteed. Duplicate input names can recur later, so V1 validates both adjacency and
non-recurrence and fails clearly on violation. There is no supported VG command that
name-groups GAMP for this purpose; `vg gamsort` sorts GAM/GAF by graph position or random
order and must not be prescribed as the remedy.

A GAMP record is traversed as a DAG over `MultipathAlignment.subpath`:

- start nodes are `start[]` when present, otherwise subpaths with no incoming `next` or
  `connection`;
- outgoing edges include both `Subpath.next` and `Subpath.connection`;
- a complete traversal reaches a sink with no outgoing edge;
- traversal score is the sum of selected `Subpath.score` values plus
  `Connection.score` values for selected connection edges;
- `next` edges add no transition score.

For each read group, V1 enumerates complete traversals subject to a safety cap before
compatibility filtering. The default cap is `--max-traversals-per-read 100000`. Cap
overflow is a hard failure unless a later decision approves a lossy mode. A collapsed
target's best score is the maximum compatible traversal score observed for any source
path mapping to that canonical transcript. The read-group best is the maximum score among
compatible canonical transcript targets after collapse; incompatible traversals do not
contribute to it. Tied best targets are retained by default; `--score-window N` retains
targets within `N` of the read-group best. All tied compatible targets and multimapping
evidence are preserved.

## Annotation lookup and compatibility

The annotation model is in memory only:

- `TranscriptModel`: source path name, source transcript ID, canonical transcript/gene
  targets from the manifest, transcript strand, merged exons, implied introns, model
  intervals, and annotated splice junction set;
- `PathAnnotation`: per-source-path sorted interval structures plus a junction hash.

This is not a persistent custom index. For each traversal, reference-consuming edit spans
are projected through `PathPositionHandleGraph::for_each_step_position_on_handle`,
filtered to source paths present in both the manifest and GTF. Do not use nearest-path
search. Multiple path projections are preserved.

Compatibility is evaluated before collapse at source transcript identity:

- at least one projected reference-consuming interval must overlap the transcript's exon
  or implied-intron model;
- every observed splice jump must be present in that transcript's annotated junction set;
- outside-first/last-exon overhang is allowed once the positive model anchor exists;
- parent-gene-only overlap is never sufficient.

For each projected aligned block, panCollapse records the read alignment orientation
relative to the source path/transcript target. It does not use that orientation to remove
compatibility. The preserved direction becomes the RAD `dirs` value for the emitted
target, allowing downstream alevin-fry expected-orientation filtering to make
library-geometry decisions. If one read group supplies mixed directions for the same
emitted target in the current implementation scope, the group is dropped and counted.

Observed splice jumps are evaluated before collapse on one source transcript model.
Adjacent projected aligned blocks on the same source path create a candidate skipped
interval when they have a positive path-coordinate gap in traversal order. The junction
key is the skipped 0-based half-open path interval. A selected GAMP `connection` edge
always marks that gap as an observed splice. A non-connection same-path gap is treated as
a deletion unless its length is at least `--min-splice-jump` (default 20), in which case
it is an observed splice jump. Every observed splice key must match an adjacent-exon
intron key in the candidate transcript.

Custom annotation indexing remains out of V1. Revisit only after a pilot shows annotation
lookup above 50 percent of wall time or CPU, median lookup above 50 us/read group,
p95 above 500 us/read group, or p95 projected source-path occurrences per aligned node
above 500, and a prototype shows at least 2x end-to-end speedup.

## Barcode and UMI handling

Upstream FASTQ preparation carries the observed raw molecule identity in the biological
read name before VG alignment. For RNA, the GAMP name field follows:

```text
<original_read_name>_<raw_CB>_<raw_UMI>
```

panCollapse parses the raw cell barcode and raw UMI from the right side of the GAMP name
field and writes those observed values to RAD. It does not correct cell barcodes or UMIs,
does not build a permit list, and does not deduplicate UMIs. alevin-fry performs
permit-list construction and cell-barcode correction, followed by UMI
deduplication/resolution during quantification.

The raw CB and UMI lengths are explicit CLI-controlled values. Parsed values whose
lengths differ from the configured lengths are malformed molecule identities. The same
configured lengths are written to RAD file tags as `cblen` and `ulen`.

The older corrected/raw tag-selection model is superseded for V1. Tag-source and tag-name
overrides are not part of the active barcode/UMI source contract.

## RAD output

V1 writes `out/map.rad` and `out/tx2gene.tsv`. The implementation should be a native
minimal C++ writer for fixed classic RnaShort, uncompressed mapper-style RAD. Baseline
compatibility is alevin-fry v0.15.0 with libradicl v0.13.0; those projects are validation
oracles, not production dependencies.

The RAD schema is fixed:

- no magic/version prefix;
- header: `is_paired` u8 set to 0, `ref_count` u64 little-endian, each reference name as
  u16 little-endian length plus bytes, and `num_chunks` u64 little-endian set to `0`
  for unknown chunk count;
- three tag sections immediately after the header, in order: file, read, alignment;
- each tag section is `u16 num_tags`, then for each tag `u16 tag_name_len`, tag bytes,
  and `u8 type_id`; array type ID 7 additionally writes one length type ID byte and one
  element type ID byte, though V1 does not need arrays;
- file tags and immediately following file values: `cblen:U16` then `ulen:U16`;
- read tags: `b:<integer>` and `u:<integer>` for barcode and UMI widths;
- alignment tag: `compressed_ori_refid:U32`;
- chunk: `u32 nbytes` including the 8-byte chunk header, `u32 nrec`, then records;
- record: `u32 naln`, encoded CB, encoded UMI, then `naln` `u32 compressed_ori_refid`
  values.

Conceptually, each alevin-fry read record contains four pieces of per-read information:

- `bc`: the raw cell barcode encoded according to the configured CB length;
- `umi`: the raw UMI encoded according to the configured UMI length;
- `refs`: the read's accepted compatibility target IDs;
- `dirs`: a parallel orientation vector where `dirs[i]` describes the orientation of
  `refs[i]`.

`refs` are zero-based indices into the RAD header target dictionary, not genomic
coordinates. In panCollapse V1 they refer to canonical transcript targets after collapse.
alevin-fry maps them to genes or gene-state IDs using `tx2gene.tsv`.

`compressed_ori_refid` is `ref_id | 0x80000000` for forward and `ref_id` for reverse.
Barcode and UMI bases are packed as A=00, C=01, G=10, T=11 with the last base in the least
significant bits; `N` is encoded as A=00 for parity with alevin-fry conversion. Widths are
selected from length 1..4 -> U8, 5..8 -> U16, 9..16 -> U32, and 17..32 -> U64. Lengths
greater than 32 are unsupported in V1. Raw CB/UMI length mismatch against the configured
lengths is a skippable raw-identity failure by default and fatal under
`--molecule-identity-failures fail`. Chunks are emitted in deterministic order to
`out/map.rad`; only the current chunk needs buffering before it is written.

For every retained target, panCollapse must emit both a `ref_id` and a direction bit.
libradicl decodes the high bit into `dirs[i]` and the lower 31 bits into `refs[i]`;
alevin-fry uses `dirs` for expected-orientation filtering during permit-list generation
and collation. Do not derive `dirs` from arbitrary graph-node orientation.

For V1, retained hits are encoded with the true target-relative orientation: forward as
`ref_id | 0x80000000` and reverse as `ref_id`. Downstream workflows should choose the
appropriate alevin-fry expected-orientation setting for the experiment. Target IDs are
sorted and deduplicated per read before writing, without losing their paired direction
values.

## Determinism and threading

The target dictionary is sorted by canonical transcript ID. Read-group order follows the
input GAMP group stream. Current active work remains single-threaded under D045. If a
future decision restores worker-thread execution, parallel workers must compute complete
read groups and a single RAD writer must commit results strictly by group sequence.
Chunks must be assembled in that sequence with no concurrent appends, and logs/summary
stats must remain stable across supported execution modes.

D045 also opens a future interface question: panCollapse may be used as a stdin consumer
for `vg mpmap` GAMP output. RAD output itself should use the streaming-to-disk approach:
write `num_chunks = 0`, file-tag values, and complete chunks incrementally. Do not add a
stdout RAD-output CLI unless a later decision approves it; any such mode must keep binary
RAD on stdout and logs/progress on stderr.

## Diagnostics and failures

Hard failures include unreadable/incompatible inputs, GAMP group recurrence, contradictory
manifest rows, missing manifest rows for compatible source identities, annotation/index
identity mismatch, and invalid RAD encoding. Skippable per-read raw CB/UMI parse or
encoding failures are counted and become hard failures under strict molecule-identity
handling.

The summary must include input record/read-group counts, raw CB/UMI skip reasons, grouping
violations, no-compatible-transcript groups, compatible targets removed by score-window
filtering (`score_removed_targets`), emitted groups, targets per emitted group, manifest
misses, and annotation/index consistency failures.

## Alternatives rejected

- Requiring GCSA/LCP, distance, snarls, or minimizer indexes as V1 converter inputs:
  these are upstream `vg mpmap` concerns. They may be available next to the GAMP, but
  panCollapse does not need them after mapping and does not accept them as V1 provenance
  flags.
- Accepting GBZ/GBWTGraph-only V1 coordinate input: the VG API wiki shows normal
  `HandleGraph` access for GBZ, but panCollapse requires path-position lookup; local XG
  already provides `PathPositionHandleGraph`, while GBZ would require a separate
  dependency boundary and projection proof.
- Using `vg gamsort` as a name-grouping remedy: it does not provide GAMP read-name
  grouping for this contract.
- Building a persistent panCollapse annotation index in V1: product scope rejects it
  until profiling and a separate decision justify the complexity.
- Linking the full VG executable/library surface by default: the narrower VGio,
  handlegraph, and xg boundary has a passing throwaway smoke and is less exposed to
  internal API churn, but it must be represented by a tracked Phase 1 build smoke.
- Depending on alevin-fry/libradicl writer code at runtime: the fixed schema is small,
  while the external projects remain better suited as interoperability oracles.

## Phase 1 validation plan

Phase 1 starts with a tracked compile/link smoke for the VGio/handlegraph/imported-XG
boundary and a build-dir-only GAMP/XG/GTF projection smoke before fixture broadening.
The Phase 1 skeleton has now added those smokes. Behavioral fixtures must cover traversal
cap overflow, exonic, intronic, exon/intron crossing, isoform exonic versus intronic,
annotated versus absent splice junction, outside overhang with an anchor,
parent-gene-only negative, target-relative orientation cases, multiple source transcript
identities collapsed without score inflation, missing manifest hard failure, GAMP group
recurrence failure, raw read-name CB/UMI parsing and malformed-input handling, mixed raw
CB/UMI lengths, and source transcript identity validation. Older tag-centric Phase 1
barcode fixtures are superseded by the raw read-name barcode source decision for V1.

RAD validation must include a tiny end-to-end `alevin-fry generate-permit-list`,
`collate`, and `quant` run using `out/map.rad` and `out/tx2gene.tsv`, with an exact
expected gene matrix. Phase 2 performs this proof with one thread. D045 defers
panCollapse-side multithreading; any future supported execution modes must satisfy
byte-identical output acceptance criteria before implementation.

## Risks for gate review

- The VG checkout is dirty; support should remain pinned to this build family until
  compatibility against another build is proven.
- Multipath traversal can grow combinatorially; Phase 1 needs an explicit cap behavior.
- Source path names and source transcript IDs are the join keys across `.xg`, GTF, and
  manifest; fixture coverage must catch subtle naming mismatches. Availability of
  `.gcsa`/`.dist` from the upstream mapper does not help this coordinate join.
- Target-relative RAD direction must be derived from path projection semantics, not from
  arbitrary graph-node orientation or synthetic all-forward encoding.
- Projection multiplicity may dominate runtime on dense graphs; the pilot thresholds
  above define when to reopen the no-custom-index decision. D045 defers panCollapse-side
  multithreading while stdin GAMP streaming is researched.
