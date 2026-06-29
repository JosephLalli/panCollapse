# Annotation Lookup and Performance Research Brief

**Status: Phase 0 researched. Constraint: no persistent custom lookup index in
V1.**

## Coordinate model

- Use GTF/path coordinates as canonical.
- Convert GTF 1-based inclusive intervals to 0-based half-open intervals:
  `[start - 1, end)`.
- Normalize VG path fragments with `get_path_base_name` and
  `get_path_base_offset` when fragment naming is present.
- Evaluate compatibility before collapse as `(source_path_name, source_transcript_id)`.
- `source_path_name` must match both a visible `.xg` path and the GTF seqname.
- `source_transcript_id` must match the configured GTF `transcript_id` attribute.
  Source paths are source-coordinate paths, such as reference, haplotype, or copy paths;
  they are not GBZ/GBWT-only compressed paths or mature spliced transcript paths for V1
  compatibility evaluation. Mapper-side `.gcsa` and `.dist` indexes do not provide this
  annotation coordinate basis after GAMP has been emitted.
  Apply the explicit collapse manifest only after compatible source
  transcripts have been identified.

## Compatibility rule

A transcript is compatible with a read group only when all of these are true:

- The selected strand mode passes.
- At least one projected reference-consuming aligned interval overlaps that
  transcript's exon-or-implied-intron model.
- Every observed splice jump is present in the transcript's annotated junction
  set.

Outside first/last exon overhang is allowed when the read is otherwise anchored
by overlap with the transcript model. Parent-gene overlap alone is never
evidence for transcript compatibility.

## In-memory structures

Build these from the GTF at startup. They are ordinary process memory, not a
persistent custom index.

- `TranscriptModel`:
  - source path;
  - source transcript ID and canonical transcript/gene targets;
  - strand;
  - merged exons;
  - implied introns;
  - exon/intron model intervals;
  - annotated junction set;
  - first-to-last transcript span.
- `PathAnnotation` per source path:
  - sorted interval collections for overlap queries;
  - junction hash keyed by donor/acceptor coordinates and transcript ID.

## Projection through existing VG indexes

Use the existing VG `PathPositionHandleGraph` interface. For each
reference-consuming edit span in a GAMP path:

1. Enumerate path steps for the aligned node with
   `for_each_step_position_on_handle`.
2. Filter to source paths present in the GTF and collapse manifest.
3. Convert node-local spans to path intervals using
   `get_position_of_step`, `get_path_handle_of_step`, `get_length`, and step
   orientation.
4. Compare projected intervals, strand, and splice jumps to `PathAnnotation`.

Strand filtering is relative to the sequenced GAMP query after projection to source path
coordinates. A projected block is forward on the source path when the path step
orientation matches the queried mapping handle orientation. A `+` transcript is forward
on its source path and a `-` transcript is reverse. `sense` keeps matching orientations,
`antisense` keeps opposite orientations, and `both` disables the filter.

Observed splice jumps are same-source-path gaps between adjacent projected aligned blocks
in traversal order. A selected GAMP `connection` edge always marks the gap as an observed
splice. A non-connection gap is treated as deletion unless it is at least
`--min-splice-jump` bases, default 20. The splice key is the skipped 0-based half-open
path interval and must match an adjacent-exon intron key for the candidate transcript.

Do not use nearest-path search. Avoid `multipath_alignment_path_offsets` for V1
boundary semantics.

## Complexity and bottleneck thresholds

- GTF load: `O(E log E)` for sorting/merging intervals.
- Memory: `O(E + I + J)` for exons, implied introns, and junctions.
- Per candidate projection: `O(sum path occurrences for aligned nodes)`.
- Main risk: high path multiplicity for aligned nodes.

Revisit a custom lookup index only after pilot metrics show annotation lookup is
the dominant cost:

- annotation lookup is more than 50% of wall time or CPU time; or
- median lookup exceeds 50 us per read group; or
- p95 lookup exceeds 500 us per read group; or
- p95 projected path occurrences per aligned node exceeds 500.

Even then, require a prototype showing at least 2x end-to-end speedup before
proposing a persistent custom index.

## Phase 1 fixture list

- Exonic evidence.
- Intronic evidence.
- Exon/intron crossing evidence.
- Isoform case where the same interval is exonic for one transcript and
  intronic for another.
- Annotated splice junction.
- Absent splice junction negative.
- Outside overhang anchored by transcript-model overlap.
- Parent-gene-only overlap negative.
- Sense, antisense, and both strand modes.
- Multiple source transcript identities collapsed by manifest.
- Missing manifest entry hard failure.
- Source path/transcript ID mismatch hard failure.

## Risks

- Path fragment normalization must be tested against exact VG path names used
  by the manifest.
- The source transcript identity model must be tested on a path with multiple transcript
  models to prevent path-only collapse.
- Multipath traversal must preserve all best or tied-best transcript evidence
  before collapse.
- Path multiplicity can dominate runtime; this is a measurement trigger, not a
  Phase 0 reason to add a custom index.

## Primary source citations

- VG path-position APIs:
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libhandlegraph/src/include/handlegraph/path_position_handle_graph.hpp:16-51`;
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libhandlegraph/src/path_position_handle_graph.cpp:11-15`.
- XG path lookup and step-position implementations:
  `/mnt/ssd/lalli/usr/local/src/vg/include/xg.hpp:181`, `:315-328`, `:370-375`;
  `/mnt/ssd/lalli/usr/local/src/vg/deps/xg/src/xg.cpp:2145-2185`,
  `:2275-2293`, `:2297-2308`.
- VG GAMP mapping/path fields used for projection:
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libvgio/deps/vg.proto:60-63`,
  `:87-91`, `:181-203`.
- VG local splice/deletion threshold precedent:
  `/mnt/ssd/lalli/usr/local/src/vg/src/surjector.hpp:84`;
  `/mnt/ssd/lalli/usr/local/src/vg/src/subcommand/mpmap_main.cpp:422`;
  `/mnt/ssd/lalli/usr/local/src/vg/src/multipath_alignment.cpp:3768`.
- GTF coordinate semantics are 1-based inclusive in the primary GTF/GFF documentation
  from Ensembl (`https://www.ensembl.org/info/website/upload/gff.html`) and GENCODE
  (`https://www.gencodegenes.org/pages/data_format.html`); panCollapse converts them
  internally to 0-based half-open intervals before lookup.
