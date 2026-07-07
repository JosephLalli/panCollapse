# GeneFull gene-calling (`--gene-mode full`)

By default panCollapse is **spliced/exonic**: it attributes each aligned node to the spliced
`vg rna` HST (transcript) paths crossing it, so a read must lie on a transcript's exon
structure to call a gene. A purely-intronic read aligns to nodes on no HST and is dropped —
the equivalent of CellRanger's `Gene` counting.

`--gene-mode full` implements **GeneFull** (STARsolo `GeneFull` / CellRanger include-introns):
a read is consistent with a gene if it overlaps the gene's **body — exons and introns**. This
keeps the intronic reads spliced mode drops, matching the modern CellRanger default.

## How it works

GeneFull reuses the entire scoring/selection pipeline and changes only the per-node lookup:

- **Spliced:** node -> HST transcript paths (from the graph), collapse to transcript id.
- **Full:** node -> gene loci (from `--gene-loci`), which cover exon **and** intron nodes.

Everything downstream is identical: per-node vg scores are summed per gene, the top-scoring
gene(s) plus ties win, orientation is the majority of aligned bases, and the result is written
to the RAD and (optionally) the BAM. In full mode the genes are the RAD targets directly, so
`tx2gene.tsv` is the identity map (gene -> gene).

## Input: `--gene-loci`

A node -> gene-locus map, tab-separated:

```
node_id <TAB> gene_id [<TAB> strand(+/-)]
```

A node may appear on several rows (overlapping gene loci). `strand` (default `+`) is the gene's
orientation across that node and feeds the same forward/reverse accounting the HST path
direction does. The map is built **once at index time** from the GTF gene spans and the graph —
panCollapse consumes the map, not the GTF (consistent with D048: no runtime GTF).

### Generating the map

Use `scripts/make-gene-loci.sh`:

```sh
scripts/make-gene-loci.sh -x graph.xg -n annotation.gtf -o gene_loci.tsv \
  [-l haplotypes.gbwt]
```

It works by the same mechanism that built the HST paths, run one level up: it projects each GTF
**gene** span as a single unspliced `vg rna` path (`--feature-type gene`, grouped by `gene_id`).
Because a gene is one feature, there are no exon junctions to splice, so the path is contiguous
over the whole locus — it covers **intron** nodes, not just exons. The script then reads
`node -> gene` off those gene-body paths (selecting only the paths `vg rna` newly adds, so
pre-existing HST transcript paths are not mistaken for gene bodies) and records each step's
orientation as the strand.

Pass the haplotype GBWT the graph was built with (`-l`) so the gene-body paths are threaded
through the pangenome haplotypes and cover the **non-reference (alt)** nodes inside each gene
body. Without it only reference nodes are covered, and reads on alt alleles — the ones counting
off the graph is meant to keep — are missed. So for a real pangenome run, always pass `-l`.

```
panCollapse convert --gamp reads.gamp --xg graph.xg \
  --gene-mode full --gene-loci gene_loci.tsv --out-dir out --bam-out out/reads.bam
```

## What calls, and what does not (verified semantics)

| Read | Spliced (`Gene`) | Full (`GeneFull`) |
|---|---|---|
| within an exon | gene (on the HST) | gene |
| spanning an exon-intron boundary (touches an exon node) | gene — the exon node is on the HST | gene |
| across multiple exons, intron spanned (splice/deletion) | gene — exon nodes are on the HST | gene |
| **purely intronic** (no exon overlap) | **dropped** | **gene** |

So GeneFull's only marginal addition over spliced is the **purely-intronic** reads: any read
that touches an exon is already called in spliced mode (via the exon node on its HST). This also
means Gene mode does not require genes to avoid introns — it just never counts intronic
membership, so the ambiguity never arises.

### Overlapping gene loci

Introns are large and routinely overlap neighboring genes, so full mode produces more
inter-gene overlap than spliced mode. Resolution follows the same top-score-plus-ties rule:

- A read whose nodes lie **entirely** in a shared region of genes A and B scores A and B equally
  -> **multi-gene** (`GX:Z:A;B`, `XT` omitted under the default policy, so a `--per-gene` counter
  skips it — matching CellRanger `Unique`).
- A read that overlaps A fully and B only partially scores A higher -> **A** (the dominant gene),
  which is slightly more permissive than a strict any-overlap rule that would call it multi-gene.
  This is a deliberate score-weighted resolution of proportional overlap; a strict-overlap
  variant could be added if needed.

## Note on the benchmark

If you are comparing panCollapse against STARsolo run with `--soloFeatures GeneFull` (introns
included), the spliced default is exon-only and is not a like-for-like counter — use
`--gene-mode full` with intron-covering loci for a fair comparison, or restrict STAR to `Gene`.
