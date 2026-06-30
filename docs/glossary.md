# Glossary

**Canonical transcript** — The transcript target ID emitted after applying the explicit
collapse manifest.

**Collapse manifest** — Deterministic mapping from graph/haplotype/copy-specific source
transcript identities to canonical transcript and gene identities.

**GAMP** — VG's serialized `MultipathAlignment` stream format.

**Gene-unique** — Future/non-RAD concept: all eligible canonical transcript targets
belong to one gene, even if there are several graph paths, copy paths, or transcript
isoforms. Active GAMP-to-RAD output does not filter on this property.

**Mapper-style RAD** — RAD produced by a mapper before alevin-fry collation and UMI
resolution.

**Raw read-name molecule identity** — The uncorrected cell barcode and UMI carried in the
GAMP name field by upstream FASTQ preparation, written by panCollapse to RAD as observed.

**Name-grouped** — All serialized records for one read name are adjacent, and a completed
name never recurs later.

**Source path** — A graph-, haplotype-, or copy-specific coordinate path visible in the
existing graph/index before canonical collapse.

**Source transcript identity** — The pair `(source_path_name, source_transcript_id)`.
The path anchors graph coordinates; the transcript ID selects the GTF transcript model on
that path.

**Transcript compatibility** — Annotation-aware consistency of a candidate GAMP traversal
with a transcript's exon/intron structure and splice-junction rules. In V1 this includes
intronic evidence and preserves target-relative orientation in RAD rather than filtering
by panCollapse-side strand mode.

**Transcript-unique** — Future/non-RAD concept: exactly one eligible canonical transcript
remains. Active GAMP-to-RAD output does not filter on this property.

**USA output** — An output/reference convention that can represent unspliced, spliced,
and ambiguous molecule status. Outside Phase 2 and outside V1 unless unspliced target
generation enters scope by later approval.
