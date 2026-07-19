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

The t2g-selects-the-layer approach above is coarse (it counts by the D048 top-score rule). A ledger
`--count-mode` (`gene`, `genefull`, `genefull_exonoverintron`, `genefull_ex50pas`) reads **both**
layers at once — `--t2g` is the exon (HST) layer, `--body-t2g` the gene-body layer — and makes
panCollapse emit a per-**transcript** **`spliced`/`unspliced` classification** (the optional BAM's
`TX`/`GL` tags; see [BAM export](bam-export.md)) that a downstream counter groups by gene and
collapses into STARsolo/CellRanger's exact `soloFeatures` semantics (D060/D061).

Gene compatibility itself is still D048's top-score tie (the genes whose body or an exon transcript
ties the read's single top score). Within each compatible gene, panCollapse classifies **every
candidate transcript**, never the gene as a whole: a transcript "spans" the read if the read's
gene-body node range falls inside that transcript's own on-body exon span (precomputed once per
gene at graph load). It is **spliced (`S`)** if its own exon path also ties the read's top score
**and** (D061) it owns every splice edge (node-skip) the read's alignment crosses — a strand-blind
concordance check mirroring STAR's `classifyAlign`, which catches a read that lands its aligned
bases on one transcript's exon while the splice it actually makes belongs to a different,
overlapping transcript's intron. It is **unspliced (`U`)** if it spans the read, its gene's body
ties the top score, and it is likewise splice-concordant. A transcript that ties the exon score but
fails concordance is **neither** `S` nor `U` — absent from the emitted set entirely, because a
spliced read is not also an unspliced one; without this gate a concordance-failed transcript would
fall through to `U` and spuriously make its gene ambiguous. Every classified transcript is emitted
(`TX`) with its gene (`GX`), the gene's orientation (`GD`), and its call (`GL`), one entry per
transcript, positionally parallel. A gene with **both** an `S` and a `U` transcript among its
compatible set is velocyto's "ambiguous" (spliced for one isoform, unspliced for another) —
panCollapse never computes that; a downstream counter groups `TX` by `GX`, derives ambiguity, and
applies the count-mode rule.

**panCollapse's own RAD/BAM output does not depend on which ledger `--count-mode` value is
passed** — `gene`/`genefull`/`genefull_exonoverintron`/`genefull_ex50pas` all drive the identical
`TX`/`GX`/`GD`/`GL` classification above (only `--count-mode score` differs, skipping the ledger
machinery entirely). The value selects which STARsolo/CellRanger rule a *downstream counter*
applies to those flags:

| `--count-mode` | a downstream counter keeps a gene when… | STARsolo |
|---|---|---|
| `gene` | `spliced` and not `unspliced` (a gene flagged both — "ambiguous" — is excluded) | `Gene` |
| `genefull` | `spliced` or `unspliced` (any compatible gene) | `GeneFull` |
| `genefull_exonoverintron` | as `genefull`, but prefer purely-`spliced` genes over intron-touching ones when a read has both among its candidates | `GeneFull_ExonOverIntron` |
| `genefull_ex50pas` | as `genefull_exonoverintron`, and also drop a purely-`spliced` gene that is **antisense** | `GeneFull_Ex50pAS` (CellRanger v7 default) |

`count_cr.py` (panSC) is the reference implementation of this table. All ledger modes still apply
the **Unique** multimapper rule at the RAD/`map.rad` level: a read compatible with more than one
gene is dropped from the RAD and counted in `multigene_dropped_groups` regardless of
`--count-mode` (CellRanger's default; `Rescue`/`EM` distribution are not implemented) —
`--bam-multigene all` carries it to the optional BAM anyway for a downstream UMI-level rescue (see
below). The default `--count-mode score` is the D048 count and is unchanged.

> **Strand and `GeneFull_Ex50pAS`.** The "drop purely-exonic antisense" step in the table is only
> panCollapse's in-mode *fragment* of Ex50pAS. STARsolo's `GeneFull_Ex50pAS` excludes ALL antisense
> body overlap (`intronicAS` too), not just purely-exonic antisense — keeping antisense *intronic* reads
> inflates large (−)-strand genes on a genomic pangenome (e.g. PTPRT ×734 vs STARsolo). Since v0.4.4
> (D059) panCollapse does the rest by *emitting* each read's per-gene orientation (the `GD` tag) rather
> than filtering on it, and the counter applies the full sense-strand policy — see
> [Strandedness](#strandedness).

## Multi-gene BAM rescue (`--bam-multigene all`)

`Unique` is only HALF of CellRanger/STARsolo's actual multi-gene handling. STARsolo layers a
UMI-level rescue on top (`MultiGeneUMI_CR`): a UMI whose reads span more than one gene is still
assigned to the gene where it has the most reads, discarding only on an exact tie. panCollapse does
not implement that rescue itself — it needs to see every read for a UMI across the whole run first,
which is a downstream counter's job, not a per-read-group streaming converter's. What panCollapse
*can* do is stop discarding the evidence the rescue needs.

`--bam-multigene all` does exactly that, and ONLY for the optional BAM: a ledger-mode read kept for
more than one gene is still dropped from `map.rad` (`multigene_dropped_groups` still counts it, the
RAD/alevin-fry contract is unchanged), but it is now ALSO written to `--bam-out`, carrying its full
candidate-gene `GX` set and no `XT` — the same tags any multi-gene read already gets under `omit`,
except the read is no longer hard-dropped before reaching the BAM at all. A downstream counter that
reads `GX` for a no-`XT` record (rather than skipping it, as a plain `--gene-tag=XT` counter does)
can then apply its own `MultiGeneUMI_CR`-equivalent resolution across the candidate genes. `all` has
no effect in `--count-mode score` (there is no Unique drop there to rescue from — a multi-target
`score`-mode read was never dropped, under any `--bam-multigene` policy) or without `--bam-out`.

## Strandedness

By default GeneFull counts a read for a gene regardless of the read's orientation. The read's
per-gene orientation (sense/forward vs antisense/reverse — the majority of aligned bases, matching
the RAD `dirs`) is classified by panCollapse and **emitted**, so which orientation to count is a
*counter* choice rather than baked into the collapse:

- **Recommended (D059):** run panCollapse strand-agnostic (`--strand both`, the default) so both
  orientations reach the BAM, each tagged with its per-gene orientation in `GD` (see
  [BAM export](bam-export.md)). The counter then applies the policy: `count_cr.py --strand forward`
  keeps sense reads and drops **all** antisense — the full `GeneFull_Ex50pAS` antisense exclusion
  (`intronicAS` included), not the 100%-exonic-only approximation of the in-mode rule. `--strand
  reverse`/`both` select the other policies from the same BAM, no re-run.
- **RAD-side (D056):** `--strand forward`/`reverse` still make panCollapse filter targets by
  orientation before writing the RAD (dropped reads counted in `strand_filtered_groups`), for a
  RAD/alevin-fry consumer that cannot read `GD`. `--strand both` (default) filters nothing.

Strand is an objective per-read fact, but which orientation to *count* is a feature policy; emitting
`GD` keeps panCollapse's gene summary complete and puts the STARsolo-faithful policy in the one
validated counter (D059). On a chr20 pangenome benchmark this is what removes the antisense
over-count of large (−)-strand genes: PTPRT 26,432 → 157 UMIs (STARsolo 36), and per-gene Pearson
0.966 → 0.995 for `GeneFull_Ex50pAS`.

## Prerequisite: the graph must retain intron sequence

None of this works if the graph has no intron nodes. `vg rna` retains them by default (it only
adds junctions/paths); `vg rna -d/--remove-non-gene` deletes intronic/intergenic regions — build
the graph without `-d`, or intronic reads have nowhere to align and GeneFull has nothing to count.
