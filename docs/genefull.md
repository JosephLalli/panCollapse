# GeneFull counting (intron-inclusive)

By default panCollapse counts **spliced/exonic**: it attributes each aligned node to the
graph's spliced `vg rna` HST (transcript) paths, so a read must lie on a transcript's exon
structure to call a gene. A purely-intronic read aligns to nodes on no HST and is dropped — the
equivalent of CellRanger's `Gene`.

**GeneFull** (STARsolo `GeneFull` / CellRanger include-introns) counts a read for a gene if it
overlaps the gene **body — exons and introns**. In panCollapse this is **not a mode flag**: it
is the ordinary spliced count run against a graph that also carries **gene-body paths**,
selected by the t2g.

## Why there is no `--gene-mode`

panCollapse reads transcript membership straight off the graph's embedded HST paths (no custom
index); the t2g only says *which* path names to use and their gene. Gene bodies work the same
way. Embed one **unspliced gene-body path per gene** — a path over the gene's whole genomic span,
so it covers intron nodes — and a read entirely in an intron lands on that path and calls the
gene. The counting code does not change; only the graph annotation and the t2g do.

Because the HST paths and the gene-body paths coexist in one graph without interfering
(panCollapse only attributes to the paths a given t2g names), the **same graph** gives you either
count:

| run with | counts |
|---|---|
| the graph + your **HST t2g** | spliced (exonic) |
| the graph + the **gene-body t2g** | GeneFull (exon + intron) |

Do not merge the two t2gs into one run: the gene call stays correct (both layers collapse to the
same gene), but the transcript dictionary picks up a redundant gene pseudo-transcript and the
count is a spliced/GeneFull hybrid. One t2g per run.

## Annotating the graph

`scripts/make-gene-annotation.sh` embeds the gene-body paths and writes the gene t2g:

```sh
scripts/make-gene-annotation.sh -x graph.xg -n annotation.gtf -o genefull -l haplotypes.gbwt
# writes genefull.xg (original paths + gene-body paths) and genefull.t2g.tsv
```

It projects each GTF **gene** span as one `vg rna` path (`--feature-type gene`, grouped by
`gene_id`). One feature per gene means no exon junctions to splice, so the path is contiguous
over the whole locus and covers intron nodes. It keeps the original HST paths, and writes a gene
t2g (`gene_body_path <TAB> gene`) in the same form as an HST t2g.

**Pass the haplotype GBWT the graph was built with (`-l`).** That threads the gene bodies through
the pangenome haplotypes so the **non-reference (alt)** nodes inside each gene are covered.
Without it only reference nodes are, and reads on alt alleles — the ones counting off the graph
is meant to keep — are missed.

## Running both counts off one graph

```sh
# spliced (exonic)
panCollapse convert --gamp reads.gamp --xg genefull.xg --t2g your_hst.t2g.tsv --out-dir out_spliced

# GeneFull (exon + intron), same graph, gene t2g
panCollapse convert --gamp reads.gamp --xg genefull.xg --t2g genefull.t2g.tsv \
  --out-dir out_full --bam-out out_full/reads.bam
```

## What calls, and what does not

GeneFull's only marginal call over spliced is the **purely-intronic** read. Any read that touches
an exon node is already called in spliced mode (via that node's HST), so exon–intron-boundary and
multi-exon (intron-spanned) reads call the gene either way. A read whose nodes lie entirely in a
shared region of two genes' bodies is multi-gene; a read dominant in one gene is called to that
gene (the top-score-plus-ties rule, applied to gene-body paths just as to transcripts).

## Exact STARsolo/CellRanger rules (`--count-mode`)

The t2g-selects-the-layer approach above is coarse (it counts by the D048 top-score rule).
`--count-mode` reproduces STARsolo/CellRanger's exact `soloFeatures` semantics by counting
per-gene **exonic vs intronic aligned bases**. It reads **both** layers at once — `--t2g` is the
exon (HST) layer, `--body-t2g` the gene-body layer — so each aligned node is classified exonic
(on an HST of the gene) or intronic (in the gene body but not an exon).

For each read it accumulates, per gene, the aligned match bases on exon nodes vs intron nodes
(deletions over introns, i.e. spliced reads, contribute no intronic bases), then applies:

| `--count-mode` | keeps a gene when the read is… | STARsolo |
|---|---|---|
| `gene` | ≥50% of the read on the gene's **exons** | `Gene` |
| `genefull` | overlapping the gene **body** at all (exon or intron) | `GeneFull` |
| `genefull_exonoverintron` | as `genefull`, but among overlapped genes prefer 100%-exon ones | `GeneFull_ExonOverIntron` |
| `genefull_ex50pas` | as `genefull`, prefer >50%-exon genes, and **drop 100%-exonic antisense** reads | `GeneFull_Ex50pAS` (CellRanger v7 default) |

All modes then apply the **Unique** multimapper rule: a read kept for more than one gene is
dropped and counted in `multigene_dropped_groups` (CellRanger's default; `Rescue`/`EM`
distribution are not implemented). Reads kept for exactly one gene are emitted to the RAD/BAM as
usual. The default `--count-mode score` is the D048 count and is unchanged.

## Strandedness

By default GeneFull counts a read for a gene regardless of the read's orientation. For a
sense-stranded library, add `--strand forward` to keep only reads that align to the gene in the
same (sense) orientation and drop antisense reads before counting — the orientation is the
majority of aligned bases (the RAD `dirs`), and dropped reads are counted in
`strand_filtered_groups`. This approximates STARsolo `GeneFull_Ex50pAS`'s antisense exclusion.
`--strand reverse` keeps only antisense; `--strand both` (default) filters nothing.

## Prerequisite: the graph must retain intron sequence

None of this works if the graph has no intron nodes. `vg rna` retains them by default (it only
adds junctions/paths); `vg rna -d/--remove-non-gene` deletes intronic/intergenic regions — build
the graph without `-d`, or intronic reads have nowhere to align and GeneFull has nothing to count.
