# Correcting an annotation for panCollapse

`panCollapse count --annotation` reads a GFF3 that already carries every classifier verdict as a
`tag=` attribute. The binary does not classify: it parses the annotation, joins it to the
path-identity ledger, and applies profile policy. Everything that decides which transcripts are
countable happens before the run, in the scripts described here.

This document has two halves. **Preparing an annotation** is what to run. **What the correction
actually does** is the algorithm, for anyone who needs to reason about the output or port it.

## Preparing an annotation

### Inputs

| input | what it is |
|---|---|
| CAT annotation | the projected GFF3 for the assemblies in the graph |
| reference annotation | the GENCODE release the projection came from, gzipped is fine |
| path-identity ledger | `unique_parent` rows for the graph's paths, one per haplotype copy |

The reference is required at correction time and again at count time: CAT drops a reference
gene's tags when it projects a model, and both stages restore them by following `source_gene`.

### The chain

These four steps are one recorded orchestration, not a guess: `prepare_selected_count_annotation.sh`
runs them in this order and the invocation is on disk. Two cautions the older documentation got
wrong. The graph and the path-identity ledger are built *before* this chain, not after it, so
`tag_nested_reference_hosts.py`'s own docstring instruction to run "before building the graph" is
contradicted by the record; what is true is that it must precede the Parent-category cache. And
the annotation reaching step 1 has already been corrected once upstream, by a different corrector
in the graph-building stage. `docs/annotation-tag-model.md` section 4 has the full six-stage
order with evidence grades.

1. `assign_cat_denovo_transcripts_detailed.py <reference> <cat> <out_dir>` attributes CAT
   de-novo predictions to the locus their exon structure came from, rather than to whichever
   transMap gene they happen to overlap. Emits `detailed_assignment.tsv.gz`.
2. `apply_cat_denovo_corrections.py <annotation> <report> <reference> <out_gff>` applies that
   report: it repoints `source_gene` on a MISASSIGNMENT verdict and writes
   `tag=readthrough_transcript` on a READTHROUGH verdict.
3. `tag_nested_reference_hosts.py <corrected> <reference> <output>` adds the pairwise
   nested-host fact, writing both `tag=nested_within_host_gene` and `nested_host_gene=<host>`.
4. `filter_cellranger_annotation.py <annotation> ...` builds the Cell Ranger reference-style
   annotation and the matching graph Parent allowlist.

`gff3_to_genebody.py` and `make_reference_path_identity_ledger.py` build the body feature layer
and the ledger; they are graph-construction rather than annotation correction, but the ledger
they produce must describe the same annotation generation the counter is given.

### Verify before counting

```
scripts/audit_readthrough_consistency.py <corrected.gff3> \
    --reference <reference.gff3.gz> --path-identity-ledger <ledger.tsv>
```

Exits non-zero on a violation. It takes a couple of minutes on a whole-pangenome annotation.
See "The readthrough contract" below for what it checks and why those three checks and not
others.

### Running the count

```
panCollapse count --gamp reads.gamp --xg graph.xg \
    --annotation corrected.gff3 --annotation-reference reference.gff3.gz \
    --path-identity-ledger ledger.tsv \
    --barcode-whitelist barcodes.txt --cr7 --out-dir counts
```

`--annotation` and `--count-bundle` are mutually exclusive; exactly one is required.
`--path-identity-ledger` is accepted only on the annotation route. Replay
(`--compatibility-in`) is bundle-only and refuses `--annotation`.

## What the correction actually does

For the full reference -- the 80-value tag vocabulary, the entity model with measured
cardinalities, which level owns which tag, and the recorded execution order -- see
`docs/annotation-tag-model.md`. This section covers what an operator needs to reason about the
output.

### Identifiers

Only `ID` is a unique key. `transcript_id` is bare and assembly-local: HPRC's CAT release
numbers each assembly's transcripts from `T0000001`, and for samples released as `hap1`/`hap2`
both haplotypes carry the same prefix, so the two id spaces coincide completely. `gene_id` on a
transcript row is likewise bare, and `Parent` means the gene on a transcript row but the
transcript on an exon row. Never key, join, group, or de-duplicate on anything but `ID`. See
`.claude/rules/annotation-identifiers.md`, which records what ignoring this cost, and D088,
which records a second instance found later in panSC's gene-policy builder.

### Which rows the counter reads

`gene`, `transcript` and `mRNA` rows only. Exon rows are skipped entirely, so tags written to
them never reach the counter -- they exist for other consumers. Note the word is overloaded: a
GFF3 `exon` row is annotation granularity, while the ledger's `feature_layer` of `exon` or
`body` says which graph path a Parent is, and a body-layer Parent is still a `transcript` row in
the GFF3 carrying a `panSCbody1_<sha256>` ID.

From a gene row the counter reads `ID`, the biotype, `nested_host_gene`, and `tag`. From a
transcript row it reads `ID` -- which must equal a ledger `unique_parent` -- plus `Parent`,
`source_gene`, the gene and transcript biotypes, and `tag`. Of the tag vocabulary only twelve
selected categories are acted on.

### Tag inheritance

A Parent's category set is the union of three sources: its own transcript row's tags, its gene
row's tags reached through `Parent`, and the reference gene's tags reached through
`source_gene` with the Ensembl version stripped. The third is not optional. CAT drops a
reference gene's tags on projection, and on the joint chr20-22 surface `overlaps_pseudogene`
alone reaches 234,932 Parents only through that path.

### The readthrough contract

Readthrough is the one place where two signals are combined with deliberately different
quantifiers, and getting it wrong is silent.

| signal | read from | quantifier |
|---|---|---|
| `readthrough_gene` | gene row, and reference gene via `source_gene` | **any** -- one declaration excludes the locus |
| `readthrough_transcript` | transcript / mRNA row | **all** -- excludes only when every observation carries it |

An observation is a Parent: one `unique_parent`, one haplotype copy.

The conjunction is what keeps the gene-level rule from doing the transcript-level rule's job.
`resolve_assignment` already drops individual readthrough candidates in `filter_categories`
before `apply_gene_competition` runs, so a locus with some readthrough isoforms keeps its clean
ones and loses only the tagged candidates. The gene-level rule is reserved for the case where
nothing legitimate remains.

**A partially tagged gene is therefore correct, not a defect.** It means some isoforms read
through and some do not, and the conjunction keeps the gene. On the chr20/21/22 surface, of
23,416 count genes 22,691 have no Parent tagged, 61 have every Parent tagged, and 664 are
partial. The 61 are what gets excluded.

What must not happen is a copy *losing* a tag its source asserts, because that turns a wholly
readthrough locus into a partial one and it silently escapes exclusion. Three propagation paths
could drop such a tag, and those are exactly what the audit checks:

- every child transcript of a `readthrough_gene` gene row is tagged;
- every copy whose `source_transcript` is readthrough in the reference is tagged;
- every body Parent agrees with its exon counterpart.

The reverse -- a copy tagged where the reference is not -- is allowed and expected. A projection
can read through in one assembly and not another because the genomic context differs, and
`apply_cat_denovo_corrections.py` writes exactly that per-copy verdict. On the chr20/21/22
annotation 70 source transcripts are tagged on some copies and not others; all 70 carry
`alignment_only`, and 63 also carry `structural_readthrough`, which is the signature of a
genuine per-haplotype structural call rather than a lost tag.

That annotation passes all three checks: 20,061/20,061, 5,723/5,723, and 1,231,247/1,231,247.

### Not audited

Whether the other eleven selected category tags satisfy the same no-dropped-tag property across
haplotype copies is open. `nested_host_gene` has a different shape because the counter reads it
only from gene rows. Extending the audit is the same query.
