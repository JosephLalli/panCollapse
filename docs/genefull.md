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
panCollapse convert --gamp reads.gamp --xg genefull.xg --legacy-adapter hst-v1 \
  --t2g your_hst.t2g.tsv --out-dir out_spliced

# GeneFull (exon + intron), same graph, gene t2g
panCollapse convert --gamp reads.gamp --xg genefull.xg --legacy-adapter hst-v1 \
  --t2g genefull.t2g.tsv \
  --out-dir out_full --bam-out out_full/reads.bam
```

## What calls, and what does not

GeneFull's only marginal call over spliced is the **purely-intronic** read. Any read that touches
an exon node is already called in spliced mode (via that node's HST), so exon–intron-boundary and
multi-exon (intron-spanned) reads call the gene either way. A read whose nodes lie entirely in a
shared region of two genes' bodies is multi-gene; a read dominant in one gene is called to that
gene (the top-score-plus-ties rule, applied to gene-body paths just as to transcripts).

## Exact STARsolo/CellRanger rules (`--count-mode`)

The t2g-selects-the-layer approach above is a legacy coarse count. The `gene`, `genefull`, and
`genefull_exonoverintron` ledger modes read both layers at once and emit a per-transcript `S`/`U`
ledger in the optional BAM's parallel `TX`/`GX`/`GD`/`GL` tags. Production
`genefull_ex50pas` is deliberately different: it emits exact STARsolo 2.7.11b E/P/B
alignment/transcript evidence in `TX`/`GX`/`GD`/`GT`/`XP`/`XU`.

### Production path identity ledger (D064)

Use one `--path-identity-ledger path_identity_ledger.tsv`; its explicit exon/body rows replace both
t2gs and body rows crosslink to their exon Parent. Ordinary score and S/U ledger modes MAX-collapse
exact paths within `unique_parent` and Parents within canonical transcript. Run transcript-first
counting with `--bam-out ... --bam-multigene all`; BAM `XP`/`XU` preserve every tied exact
path/Parent behind each `TX`. See [Input and Output Contract](input-output-contract.md).

### Exact production GeneFull_Ex50pAS evidence (D066)

`--count-mode genefull_ex50pas` requires the production path identity ledger, `--bam-out`, and
`--bam-multigene all`. The `hst-v1` adapter is rejected because its t2gs do not identify the exact
linked exon/body path and Parent needed for base-resolved evidence.

For every complete MultipathAlignment traversal contained by an exact body path, panCollapse
accumulates its stored GAMP subpath and scored-connection alignment score and counts
reference-aligned bases overlapping its linked exact exon path. Across all records in the read
group, it retains only evidence within five points of the global best compatible traversal,
inclusive. This is the v0.8 default; `--no-ex50-score-window` instead retains every complete
compatible traversal, reproducing pre-D068 evidence eligibility before downstream ranking. It then
emits one strand-neutral tier for each retained traversal/model identity:

- `E`: every reference-aligned base is exonic and every splice junction is concordant;
- `P`: strictly more than half of the reference-aligned bases are exonic;
- `B`: the traversal is body-contained but meets neither E nor P.

Exactly 50% exonic is B. A fully exonic traversal with a discordant splice junction falls through
to P. The five-point window is applied before these tiers compete and corresponds to one
mismatch-equivalent under vg's default `+1` match / `-4` mismatch scoring. `GD` combines with `GT`
to give STARsolo's six global priority ranks:
E-sense, E-antisense, P-sense, P-antisense, B-sense, B-antisense. The consumer selects the first
nonempty rank; an antisense winning rank produces no count.

The BAM header records `panCollapse-ex50-score-window:5` or `disabled`, and `summary.tsv` records
the same policy. Disabled means no pruning; it is not an exact-tie-only zero-width window.

This mode does **not** Parent- or canonical-collapse the evidence first. Every semicolon-parallel
`TX`/`GX`/`GD`/`GT`/`XP`/`XU` slot contains one exact path and its one exact Parent. E/P slots name
the exon path/Parent; B slots name the linked body path/Parent. A canonical `TX` can therefore
repeat for distinct locus Parents, paths, tiers, or alignment alternatives. Each slot must resolve
through the ledger to that same `TX` and `GX`, and the Parent derived from `XP` must equal `XU`.
count_cr performs the six-rank selection, gene pooling, and UMI resolution downstream.

The implementation propagates model-bound states through the MultipathAlignment DAG; it does not
enumerate its potentially exponential complete traversals. The established S/U classifier still
drives `map.rad`, so selecting exact Ex50pAS changes only its BAM evidence and leaves RAD bytes
unchanged.

### Legacy transcript-first body t2g (D063)

Use a consistently three-column body t2g:

```text
raw_body_graph_path<TAB>gene<TAB>canonical_transcript
```

The canonical transcript must occur in the exon t2g and map to the same gene. A canonical
transcript may have several raw body paths — haplotype copies, CAT projections, or fragment-local
segments — and their scores MAX-collapse, never sum, just like raw exon paths. For each read:

1. panCollapse independently MAX-collapses raw exon and raw body paths into the same canonical
   transcript target space and finds the global top score across both layers;
2. a transcript whose exon score is within the existing flank tolerance of top is `S`;
3. otherwise, that exact transcript is `U` only when its own body score is within tolerance;
4. D061 splice concordance gates both calls. Splice ownership is built by comparing each canonical
   transcript's exon paths only with that transcript's own body paths. An exon edge is a splice
   when its endpoints have internal steps on one body path, or when the endpoints occur across
   separate body fragments and no body path contains them adjacently. Any adjacent occurrence is
   a conservative veto, including ambiguous repeated-node/copy geometry. A transcript that does
   not own every splice the read crosses is absent, not demoted to `U`. Fragment paths need not
   overlap: their shared canonical-transcript column is the authoritative body-union grouping.

No gene identity enters this S/U classifier. `GD` is likewise computed per `TX` target in this
mode. Only after panCollapse emits the complete transcript ledger does count_cr map `TX` to `GX`,
derive gene ambiguity, apply the selected count-mode rule, and resolve multi-gene UMIs. Run with
`--bam-out ... --bam-multigene all`; without `all`, the established ledger output policy can omit
multi-gene records before count_cr sees them. The RAD Unique policy remains unchanged.

At graph load, three-column mode reports `evaluated_target_edges`, `owned_target_edges`,
`fragment_only_target_edges`, and `adjacent_vetoed_target_edges` to stderr. These count canonical
transcript/edge associations (not only distinct graph edges), so a real d46 run can audit how much
splice ownership depends solely on body-fragment union without changing RAD/BAM contents.

### Legacy two-column body t2g

A fully two-column `body_path<TAB>gene` file under `--legacy-adapter hst-v1` preserves D060/D061
unchanged for compatibility. It
MAX-collapses bodies by gene, infers each transcript's U state from its on-body span, and computes
orientation per gene. A body t2g may be entirely two-column or entirely three-column; mixed widths
are rejected so one run cannot silently mix classifiers.

Every classified transcript is emitted (`TX`) with its gene (`GX`), orientation (`GD`), and call
(`GL`), one positionally parallel entry per transcript. A gene with both an `S` and a `U`
transcript is velocyto's "ambiguous"; panCollapse does not compute that gene label in the
transcript-first path. count_cr performs the final grouping and applies the count-mode rule.

For `gene`, `genefull`, and `genefull_exonoverintron`, panCollapse emits the identical
`TX`/`GX`/`GD`/`GL` classification above and the value selects which rule a downstream counter
applies. Exact production `genefull_ex50pas` is the D066 exception and emits `GT` evidence instead:

| `--count-mode` | a downstream counter keeps a gene when… | STARsolo |
|---|---|---|
| `gene` | `spliced` and not `unspliced` (a gene flagged both — "ambiguous" — is excluded) | `Gene` |
| `genefull` | `spliced` or `unspliced` (any compatible gene) | `GeneFull` |
| `genefull_exonoverintron` | as `genefull`, but prefer purely-`spliced` genes over intron-touching ones when a read has both among its candidates | `GeneFull_ExonOverIntron` |
| `genefull_ex50pas` | select the first nonempty global E-sense, E-AS, P-sense, P-AS, B-sense, B-AS rank; an AS winning rank is not counted | `GeneFull_Ex50pAS` (CellRanger v7 default) |

`count_cr.py` (panSC) is the reference implementation of this table. All ledger modes still apply
the **Unique** multimapper rule at the RAD/`map.rad` level: a read compatible with more than one
gene is dropped from the RAD and counted in `multigene_dropped_groups` regardless of
`--count-mode` (CellRanger's default; `Rescue`/`EM` distribution are not implemented) —
`--bam-multigene all` carries it to the optional BAM anyway for a downstream UMI-level rescue (see
below). The default `--count-mode score` is the D048 count and is unchanged.

For D063 transcript-first runs, `--bam-multigene all` is required: it preserves every multi-gene
`TX` candidate in the BAM so count_cr, rather than panCollapse, performs the final gene pooling.

> **Strand and `GeneFull_Ex50pAS`.** D066 emits orientation beside each exact E/P/B slot instead of
> filtering it in panCollapse. count_cr must apply the six-rank policy globally: an E-antisense
> candidate outranks P- or B-sense candidates and causes the read to be excluded, exactly as
> STARsolo 2.7.11b does. See [Strandedness](#strandedness).

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

By default GeneFull counts regardless of orientation. In the D063 transcript-first path, the
read's orientation is computed and emitted per `TX`; exact Ex50pAS emits it per E/P/B evidence
slot; the legacy two-column body path retains its per-gene orientation. Which orientation to count
remains a counter choice rather than being filtered during evidence production:

- **Recommended:** run panCollapse strand-agnostic (`--strand both`, the default) so both
  orientations reach the BAM, each tagged in `GD` (see [BAM export](bam-export.md)). For exact
  Ex50pAS, count_cr applies orientation inside the six-rank E/P/B priority. For ordinary S/U modes,
  its configured strand policy can keep forward, reverse, or both orientations from the same BAM.
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
