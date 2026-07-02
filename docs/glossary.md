# Glossary

**GAMP** — VG's serialized `MultipathAlignment` stream format.

**HST (haplotype-specific transcript)** — A transcript path embedded in the graph by
`vg rna`, one per haplotype copy, named `<transcript_id>_H<n>` or `_R<n>`. The HST path
encodes the transcript's spliced structure as a walk through the graph, in transcript 5'->3'
orientation.

**Transcript compatibility** — Graph-path membership: a read is compatible with a transcript
when one of its HST paths crosses a node the read aligns to. There is no runtime exon/intron
or splice-junction test; the HST path already encodes splicing.

**Transcript-copy collapse** — Reducing the winning HSTs to their unique transcript IDs
(dropping the `_H<n>` / `_R<n>` suffix). Haplotype copies of one transcript collapse to one
ID. Collapse is implicit in HST naming; there is no runtime collapse manifest.

**Transcript target** — A transcript ID in the RAD dictionary: the collapse of a read's
winning HST copies. Projected to a gene through the t2g.

**Transcript-to-gene map (t2g)** — Two-column `transcript_id`/`gene_id` map; the runtime
annotation input, used to write `tx2gene.tsv` and to project targets to genes.

**Mapper-style RAD** — RAD produced by a mapper before alevin-fry collation and UMI
resolution.

**Raw read-name molecule identity** — The uncorrected cell barcode and UMI carried in the
GAMP name field by upstream FASTQ preparation, written by panCollapse to RAD as observed.

**Name-grouped** — All serialized records for one read name are adjacent, and a completed
name never recurs later.

**USA output** — An output/reference convention that can represent unspliced, spliced, and
ambiguous molecule status. Outside V1 unless unspliced target generation enters scope by
later approval.
