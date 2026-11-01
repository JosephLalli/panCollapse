# GAMP-to-RAD Conversion Algorithm

panCollapse converts a name-grouped GAMP into a RAD file for alevin-fry. For each read it
reports the set of transcripts the read is compatible with, each with an orientation, and
writes that to RAD. This is graph-native transcript feature counting on a pangenome: the
question is "which transcripts is this read compatible with," answered directly from the
graph the mapper already produced.

This document is the canonical algorithm, governed by decision D048 as amended by D091
(each alignment path scored separately; the best path wins).

## Inputs (runtime)

- Name-grouped GAMP: all records for one read name are contiguous.
- The graph (`.xg` or equivalent) carrying the haplotype-specific transcript (HST) paths
  that `vg rna` embedded (`<transcript_id>_H<n>` / `_R<n>`).
- A production `--path-identity-ledger` in schema `panSC-path-identity-v1`, selecting exact graph
  paths and mapping them through unique Parents to canonical transcripts/genes. Historical t2gs
  are accepted only through `--legacy-adapter hst-v1`.

A GTF and `vg rna` build the annotated graph during reference/fixture creation only; they
are not runtime inputs, and there is no separate collapse manifest.

## Per-read algorithm

For each read, across all of its alignments in the GAMP (the primary record and every
supplementary/secondary record):

1. **Score each node.** For every node the read aligns to, compute that node's alignment
   score under vg's own scheme, recomputed from the `Mapping` edits: match `+1`/base,
   mismatch `-4`/base, affine gaps (open `6`, extend `1`), and the `+5` full-length bonus at
   read ends. Carry affine-gap state and the read-end bonus across node boundaries. vg does
   not serialize per-node scores, but the per-node contributions of a subpath sum to that
   subpath's `Subpath.score` — the built-in correctness check.

2. **Attribute to transcripts, one alignment path at a time (D091).** An alignment path is one
   source-to-sink route through a record's subpath graph: one choice among the alternative
   subpaths vg lists for each stretch of the read. Along one path, every HST path that
   traverses a scored node receives that node's score (native `for_each_step_on_handle`).
   Compatibility is graph membership: the HST path visits the node. The HST path already
   encodes the transcript's spliced structure, so there is no exon/intron/junction test. An
   HST's score is the maximum, over every alignment path of every record of the read, of the
   score it receives on that path; only paths that visit the HST count. Alternative subpaths
   and alternative records are never added together. Orientation evidence (aligned bases
   forward and reverse relative to the HST) comes from the maximizing path; among paths tied
   on score, the one with more aligned bases, then more forward bases, is used. This is
   computed by dynamic programming over the subpath graph, not by enumerating paths.

3. **Collapse exact identity with MAX.** Take the per-HST maximum over all alignments. Resolve
   `vg_path_name -> unique_parent -> canonical_transcript`; MAX-collapse paths within a Parent and
   Parents within a canonical transcript. Retain every path/Parent tied at either winning boundary.
   Alternative evidence is never summed.

4. **Pick canonical winners.** Take the global highest canonical-transcript score plus ties. This
   unique canonical set is the read's compatible targets — RAD `refs`. Identity strings are opaque;
   only the explicit `hst-v1` adapter applies D062's suffix/t2g rules.

5. **Record orientation.** For each winning HST, the read is forward if it traverses the HST
   path's nodes in the path's direction, reverse if opposite. HST paths are stored 5'->3' of
   the transcript, so this is the read's sense/antisense relationship directly. Per
   transcript, if the winning HSTs disagree, record the orientation backed by the majority of
   aligned bases (deterministic forward fallback for an exact tie). Encode into RAD `dirs`
   (forward = high bit of `compressed_ori_refid`).

6. **Emit.** Write the RAD record: raw cell barcode and UMI parsed from the read name,
   `refs`, and `dirs`.

A read whose records carry no aligned subpaths, or whose alignments cross no HST node, emits
no RAD record and increments the corresponding summary counter.

## Scoring: replicating vg exactly

vg computes a subpath score as a left-to-right sum over the subpath's mappings (nodes) and
edits (`vg` source `alignment_scorer.cpp:score_partial_alignment`; defaults in
`alignment_scorer.hpp`). Because it is additive over nodes, we reproduce it per node and
attribute each node's contribution to the HSTs crossing it. Summed over a subpath, our
per-node scores must equal `Subpath.score`; that equality is the acceptance test for the
scorer. If vg mapped with base-quality-adjusted scoring, the flat per-node sum will not match
`Subpath.score` and the quality-adjusted matrix must be reproduced instead; the checksum
reveals which regime applies.

The default per-node scorer is the flat scheme. An optional `--score qualadj` selects a
reimplementation of vg's QualAdjAligner (`src/pathtally_qualadj.hpp`) that reproduces
`Subpath.score` for 99.2% of subpaths on the MHC GAMP (residual +/-1 is score-matrix
rounding). The flat scorer is sufficient for ranking; qualadj is available when exact
fidelity to vg's quality-adjusted scores is wanted.

## Output and determinism

- `map.rad` via a streaming seek-and-backpatch writer (D049: header + target dictionary and
  tags up front with a placeholder `num_chunks`, records streamed as groups flush, chunk and
  `num_chunks` counts patched at finalize), `tx2gene.tsv` from the identity projection, and a run summary with
  stable counters. GAMP input streams from a file or stdin (`--gamp -`). RAD records
  stream to disk incrementally, but the converter also maintains an in-memory set of
  every completed read-group name to guarantee exact detection of unsorted
  (non-contiguous) input; peak resident memory therefore grows with the number of
  distinct read-group names seen and can reach many GB on a whole-genome run. It is an exact-name
  hash set; bounding
  the set would risk silent mis-grouping of reads whose alignments are non-adjacent in
  the GAMP, so the tradeoff is accepted for v0.1.
- Complete read-name groups may be classified by multiple workers. A single parser assigns
  monotonically increasing group ordinals and owns recurrence validation; a single writer emits
  completed results strictly by ordinal. The RAD target dictionary is lexicographically ordered,
  and output is byte-reproducible across worker counts and runs. Cross-platform byte identity is guaranteed only for the
  default flat scorer (integer arithmetic). Under `--score qualadj`, the score matrix and
  full-length-bonus table are constructed with `std::exp`, `std::log`, `std::pow`, and
  `std::round` (`src/pathtally_qualadj.hpp` `build_matrix`/`build_bonuses`); last-bit
  rounding can differ across platforms or libm versions, so a score tie may resolve
  differently and RAD bytes can vary across machines. Within one machine and build, qualadj
  output is deterministic.
- The producer queue holds at most two complete groups per worker; each worker owns one reusable
  tally workspace. This bounds scheduling state by the requested worker count without adding
  per-group spill files or extra output I/O.
- alevin-fry consumes `map.rad` downstream (permit-list, collate, quant).

## Implementation increments

Build one testable behavior at a time:

1. Reproduce vg's per-node alignment score from the `Mapping` edits; accept when per-node
   sums equal `Subpath.score` on real GAMP subpaths.
2. Attribute each node's score to exact ledger paths along each alignment path separately, and
   keep each HST's best path (D091); MAX-collapse path -> Parent -> canonical.
3. Select canonical winners at the single top score across all alignment paths, plus ties.
4. Record per-transcript orientation by the majority of aligned bases into RAD `dirs`.
5. Emit `map.rad` (streaming writer) and `tx2gene.tsv`; verify alevin-fry consumption.
6. Human-pangenome MHC fixture (`docs/testing_fixture_creation.md`): slice HPRC v1.1 GRCh38,
   build the spliced/HST graph with `vg rna`, simulate reads, map to GAMP, and compare
   panCollapse RAD against the independent GAMP-driven oracle.

Per-increment verification is in `docs/conversion-algorithm-tests.md`.

## Native gene counting and reference preparation

The algorithm above describes the original RAD producer. Native `count` adds
gene-assignment and molecule-counting stages; it does not replace transcript
compatibility with a node-to-gene vote. Its production path is:

1. Load graph/path identities and gene-policy facts. Prepare graph-to-transcript
   lookup, ordered Parent models, exon/body positions and splice geometry.
2. Stream grouped GAMP alignments. Score each alternative alignment route
   separately, retaining the best compatible route rather than summing mutually
   exclusive alternatives (D091).
3. Associate aligned nodes with transcript/Parent models and compute exact
   exon/body overlap, strand and splice-compatible evidence. MAX-collapse
   equivalent paths instead of multiplying support by the number of haplotypes.
4. Resolve that evidence independently under each requested frozen profile.
   Current native assignment follows the frozen oracle's sense-bucket-first
   policy; D066's earlier interleaved ranking is not the current rule. Profile
   membership, categories, nesting and body-support rules are policy, not extra
   votes from the graph.
5. Aggregate integer `(profile, barcode, gene, UMI)` support in RAM. Every valid
   input group, including a featureless one, contributes to the exact-whitelist
   barcode prior. Potentially correctable off-whitelist observations retain
   aggregated quality signatures until the final prior is known; raw alignments
   are not retained.
6. At EOF, finish barcode correction, merge corrected support, apply the frozen
   non-transitive 1MM and cross-gene UMI rules, and write canonical count and
   molecule tables. Publish the staged output directory only after validation.

D100 removes all aggregation, corrected-observation and single-barcode disk
fallbacks. These stages are memory-only and have no independent spill budget.
The external whole-process limit covers reference state and counting together;
insufficient RAM fails the run rather than switching to temporary aggregate
files. Compressed final Parquet/MEX output is unchanged. Explicitly requested
per-read diagnostics retain their separate ordered staging implementation;
they are neither a counting fallback nor a default output.

### What the current annotation index contains

`PCANIDX2` (`annotation.pci`, D099) is a compiled **annotation-policy cache**:
source hashes/sizes, format and semantic versions, profile/gene facts,
gene/category dictionaries and a lexically ordered 13-byte numeric record per
Parent (gene ID, category-set ID, novel-origin ID and flags). Parent names and
path-to-Parent/canonical identities still come from the separately loaded,
checksum-bound path-identity ledger. The index contains no graph handles, node
postings, path-identity mapping, ordered exon positions or splice geometry.
It avoids annotation decompression/parsing/classification, but is not the full
once-only prepared reference contemplated by D098.

`NodeLedger` already supplies a node-to-associations dictionary. It is populated
lazily through FastLocate against the GBWT and the identity ledger during each
sample run, rather than persisted as the prepared counting reference. The
remaining ledger load, FastLocate construction and model/geometry preparation
explain why a small annotation index does not eliminate reference startup.

### Why associations must retain transcript identity

A node-to-gene **set alone** loses transcript coherence. Suppose gene G has one
transcript containing node A and a different transcript containing node B,
whereas gene H has a transcript containing both. A read spanning A and B must
not acquire an invented full-length G transcript by unioning the two G models.
The current per-transcript MAX distinguishes partial G support from full H
support. Strand, exon versus intron, repeated-node occurrences and junctions
likewise cannot be recovered from a bare gene set.

The appropriate lookup backbone is therefore node-to-dense-transcript/model
associations, with shared Parent/gene metadata and the occurrence/junction
context required by exact assignment. Persisting that lookup and prepared
geometry could move repeated work into a once-only reference build. It is not
implemented by `annotation.pci`, and its whole-genome size and loading costs
remain unqualified: materializing all GBWT postings naively could be much larger
than the compressed GBWT. No new reference format or biological-policy change
is introduced by the memory-only runtime change.

### Compact live reference state (D101)

GBWT coordinates retain a prefix at step zero, every 64 steps, and the terminal
step. FastLocate's decompressed node-record suffix-array order supplies the
exact BWT record offset. LF follows that same occurrence for at most 63 steps:
forward stored sequences walk to the next checkpoint and subtract node lengths;
reverse stored sequences walk to the prior checkpoint and add intervening node
lengths. This preserves mixed orientations, repeated nodes, and exact base
positions. A checkpoint vector holds either 32- or checked 64-bit coordinates,
never both. It is still constructed per sample, not persisted in `annotation.pci`.

The ledger first validates arbitrary text-input order with its existing trees,
then retains lexical row and Parent arrays. Parent pointers are rebound before
the compact fact catalog is created. Fact rows retain one identity pointer,
three numeric dictionary IDs and flags (24 bytes); category vectors live once
per distinct set. Numeric lexical Parent ranks replace duplicate name/rank and
long-lived materialized-fact catalogs. GBWT path selection reuses each ledger
row's binding flag to detect duplicate names instead of copying selected names.

Native node/model lookup caches account resident data against internal 1-GiB
and 256-MiB bounds. FIFO eviction or oversized-entry bypass only changes reuse:
immutable values remain alive through shared-pointer leases held by active reads.
Those leases and allocator overhead are not included in the resident bounds;
the external process cap remains the final resource guard. No cache eviction
spills data or changes count policy. Legacy conversion remains uncapped here.

After workers join and compatibility capture closes, counting preserves its
output provenance, clears reference caches, and releases graph, ledger, Parent
policy and model geometry before barcode/UMI finalization. Gene/profile metadata
needed for final outputs remains. Common A/C/G/T UMI strings up to 31 bases use
tagged numeric keys; a lossless owned-string fallback preserves the existing
API for longer or noncanonical strings. Intermediate ordering is operational;
final canonical row ordering and frozen UMI rules are unchanged.

Bounded tests include checkpoint/terminal boundaries, 1,025-step paths, mixed
orientations/repeated occurrences, wide coordinates, ledger move/rebinding,
compact policy/index byte roundtrips, cache eviction with active leases, and
UMI length/fallback edges. The full suite and a frozen-binary nine-read
comparison qualify tested behavior, not whole-genome peak RAM or throughput.

## Open items

- Step 1 is verified on real MHC GAMP: the flat per-node scorer reproduces `Subpath.score`
  exactly for 93.1% of subpaths; the remainder are lower-quality bases whose mismatch penalty
  vg attenuated under base-quality-adjusted mapping (deviating subpaths mean base quality 22.4
  vs 34.4 for exact matches). The flat score is adopted as the per-node weight; exact
  quality-adjusted reproduction is deferred as unnecessary for ranking.
