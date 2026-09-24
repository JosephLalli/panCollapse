# The annotation tag model

How tags reach the counter: the vocabulary, the entity model, where each tag is determined, and
the order the producing steps actually ran in. `docs/annotation-correction.md` is the operator's
guide; this is the reference behind it.

Claims are graded. **[A]** a recorded invocation on disk. **[B]** measured from the data, with
the command noted. **[C]** code read, cited by `file:line`. **[?]** not determined.

## 1. Entities

| entity | what it is | count |
|---|---|---|
| gene row | synthesized GFF3 `gene` row | 470,208 **[B]** |
| transcript row | GFF3 `transcript` row; `mRNA` never occurs | 2,461,354 **[B]** |
| exon row | GFF3 `exon` row; **the counter never reads these** | 8,797,015 **[B]** |
| Parent | `unique_parent` in the ledger, one annotation identity | 2,461,354 **[B]** |
| count_gene | `normalized_locus()`, the counting unit | 23,416 **[B]** |

A Parent is one annotation identity, carried by exactly one `vg rna` haplotype path, and it
corresponds to exactly one GFF3 transcript row keyed by `ID`. Set equality is exact in both
directions: no Parent lacks a transcript row and no transcript `ID` lacks a Parent **[B]**.

`feature_layer` splits Parents into **exon 1,230,107** and **body 1,231,247**, and that split is
exactly the four ID shapes: PanSN `sample#hap#local` (1,216,194), `CHM13_*` (13,148) and `LOFF_*`
(765) are exon; `panSCbody1_<sha256>` (1,231,247) are body **[B]**. An exon Parent self-references
through `exon_unique_parent`; a body Parent points at its exon counterpart.

**One biological gene becomes many Parents.** The annotation is a pangenome projection: ~231
samples times up to two haplotypes, each carrying its own copy of every locus and its own isoform
set. 470,208 gene rows collapse to 23,416 count genes. This is why every quantifier decision
matters -- "any Parent" and "all Parents" differ by three orders of magnitude on a single locus.

### `Parent` is level-dependent, and not only between rows types

The rule in `.claude/rules/annotation-identifiers.md` says `Parent` is the gene on a transcript
row. That holds for the 1,230,107 exon-layer transcript rows. It is **false for the 1,231,247
body-layer rows**, whose `Parent` is another *transcript* -- the exon-layer row the body was
derived from **[B]**. The GFF3 is three levels deep on a body-carrying locus:

```
gene  ->  exon-layer transcript  ->  body-layer transcript (panSCbody1_<sha256>)
```

Following only one hop leaves every body Parent with no gene row. That was a live defect in
`from_annotation` until it was fixed by resolving the second hop (`src/direct_count_facts.cpp`,
the `locus_row` walk); it cost body Parents `overlaps_pseudogene` on 161,619 and
`readthrough_gene` on 20,062 **[B]**, both of which cr7-v1 excludes.

### Three unrelated meanings of "exon"

| usage | meaning |
|---|---|
| GFF3 `exon` row | one exon segment; the counter skips these entirely **[C]** `direct_count_facts.cpp:874-876` |
| ledger `feature_layer=exon` | the spliced transcript path, all exons concatenated |
| GFF3 attribute `feature_layer=transcript_body` | marks the 1,231,247 exon rows belonging to body transcripts **[B]** |

## 2. Vocabulary

**80 distinct `tag=` values** exist in the corrected annotation: 69 on transcript rows, the
identical 69 on exon rows, 13 on gene rows **[B]**, confirmed by two independent full passes.

**Twelve are acted on** -- `selected_category_tags()`, `src/direct_count_facts.cpp:641-649`
**[C]**, reproduced name-for-name by the bundle receipt's `selected_annotation_tags`:

`candidate_novel_locus`, `identity_candidate_reassignment`, `identity_unresolved`,
`independent_single_gene_support_strong_local_structure`, `label_conflict`, `nested_same_strand`,
`no_source_provenance`, `overlaps_pseudogene`, `readthrough_gene`, `readthrough_transcript`,
`source_gene_label_conflict`, `structural_readthrough`.

Two of the twelve, **`overlaps_pseudogene` and `readthrough_gene`, occur on gene rows only and on
zero transcript rows** **[B]**. They reach a Parent solely through the gene-row leg or the
reference leg -- which is what made the `Parent` defect above expensive.

**Sixty-eight are inert.** The counter never reads them. The largest is `alignment_only` at
1,347,673 transcript rows, deliberately omitted; then `ortholog_input_consistent`,
`multigene_unresolved`, `nested_within_host_gene` (the nested tagger's own tag is *not* selected --
only the `nested_host_gene=` attribute carries that relation), and 52 GENCODE metadata tags
carried through onto CAT rows.

**One is tested for but does not exist.** `foreign_component_only_except_host_concordant_small_rna`
appears in `pansc-strict-v1`'s excluded categories (`src/direct_count.cpp:261` **[C]**) and
nowhere else in either codebase; it occurs 0 times in the annotation and 0 times in the ledger
**[B]**. `from_annotation` cannot emit it, so the rule is inert -- but the bundle loader copies
the ledger's `categories` column verbatim with no vocabulary check, so a future ledger emitting
the token would make it fire.

### Encoding hazard

Multi-valued tags are joined with `%2C`, but gene rows mix in bare commas
(`tag=pseudo_consens,overlapping_locus%2Cnested_within_host_gene`) **[B]**. Both current readers
survive it because they percent-decode *before* splitting. A reader that splits first will be
wrong.

## 3. Where tags come from

**GENCODE is the origin, and it separates levels.** Gene rows carry `overlapping_locus`,
`pseudo_consens`, `ncRNA_host`, `overlaps_pseudogene`, `readthrough_gene` (661); transcript rows
carry `basic`, `CCDS`, `MANE_Select`, the `appris_*` family, and `readthrough_transcript` (2,120)
**[B]**. GENCODE is not internally uniform: of 1,570 transcripts under a `readthrough_gene` gene,
83 lack `readthrough_transcript`, and 633 `readthrough_transcript` transcripts belong to genes
not tagged `readthrough_gene` **[B]**.

**CAT projection drops the gene level.** The input CAT is exon-only -- every `gene` and
`transcript` row in the 8.9 GB file is synthesized downstream **[B]**. CAT does carry transcript
tags across, but nothing gene-level survives, because there are no gene rows to carry it. This is
why the reference leg exists.

**The correction scripts add the rest.** `apply_cat_denovo_corrections.py` writes the classifier's
verdict category, `alignment_only`, `no_source_provenance`, `source_gene_label_conflict`,
`independent_single_gene_support_strong_local_structure`, and `readthrough_transcript` (both
inherited from the source transcript and from a READTHROUGH verdict). Its `ParentRollup` writes
the reference gene's whole tag string onto the synthesized gene row, but only when all the gene's
transcripts agree on one `source_gene`. `tag_nested_reference_hosts.py` adds
`nested_within_host_gene` and `nested_host_gene=<host>`.

**The counter reconstructs a Parent's set from three legs** (`direct_count_facts.cpp:941-961`
**[C]**): the transcript row's own tags, the gene row's tags reached through `Parent` (two hops
for body rows), and the reference gene's tags reached through `source_gene` version-stripped.
Ten of the twelve arrive on the transcript row directly; the two gene-only tags arrive on the
other two legs.

### Which level owns which tag

| class | example | uniform across copies of one source transcript? |
|---|---|---|
| curated gene fact | `readthrough_gene`, `overlaps_pseudogene` | yes, inherited from one reference gene |
| curated transcript fact | `readthrough_transcript` inherited from GENCODE | yes |
| **per-copy structural verdict** | `readthrough_transcript` from a READTHROUGH verdict, `structural_readthrough`, `alignment_only` | **no, and correctly not** |

The third class is why **per-source-transcript uniformity is not a valid invariant**. Whether a
projected copy reads through depends on that assembly's local context -- a deletion can make two
genes adjacent in one haplotype and not another. 70 source transcripts are tagged on some copies
and not others; all 70 carry `alignment_only` and 63 carry `structural_readthrough` **[B]**,
which is the signature of a genuine per-haplotype call. The defensible invariant is
one-directional: **a copy must not lose a tag its source asserts.** That is what
`scripts/audit_readthrough_consistency.py` checks.

## 4. Order

The pipeline is three repositories deep. `scripts/README.md`'s chain is a subset.

| stage | what runs | grade |
|---|---|---|
| 1. narrow CAT correction, pre-graph | `assign_cat_denovo_transcripts.py` then `apply_post_dedup_cat_corrections.py`, via `run_chr20_chr21_chr22_dedup_then_correction.sh` | **[A]** |
| 2. graph and ledger | `build_fragment_intervals.py`, `make_retention_features.py`, `vg rna`, `partition_vg_info.py`, `make_path_identity_ledger.py`, then `vg index -x`/`-j` and `merge_path_identity_ledgers.py` | **[A]** |
| 3. detailed correction and tagging, **post-graph** | `prepare_selected_count_annotation.sh`: `verify-source-scope` -> `assign_cat_denovo_transcripts_detailed.py` -> `apply_cat_denovo_corrections.py` -> `tag_nested_reference_hosts.py` -> `validate_selected_annotation_graph_identity.py` -> `build_parent_category_ledger.py` -> `selected_count_bundle.py create` | **[A]**, every edge |
| 4. gene policy | `count_cr.py --gene-policy-only`, then `derive_projected_nested_host_policy.py` | **[A]** |
| 5. bundle compile | `build_pancollapse_count_bundle.py` | **[A]** |
| 6. count | `panCollapse count`, bundle route or annotation route | **[C]** `main.cpp:600` |

Two things this corrects. **The annotation is corrected twice**, by two differently-versioned
correctors in stages 1 and 3; the final file carries all three `cat_correction_method`
vocabularies. And **graph construction sits in the middle, not at the end**:
`tag_nested_reference_hosts.py:14` and `scripts/README.md` both say to run the nested tagger
"before building the graph", but the xg finished Aug 30 and the tagger ran Sep 3 **[A]**. The
*Parent-category cache* half of that instruction is correct and proven; the *graph* half is not.

`gff3_to_genebody.py` and `make_reference_path_identity_ledger.py` appear nowhere in the recorded
chain -- production used `make_path_identity_ledger.py`, and the body layer comes from
`exons_to_full_bodies.py` in stage 1.

## 5. Not determined

- **How `panSCbody1_<sha256>` is constructed.** 198 candidate constructions were tried and none
  reproduces it; it cannot be a digest of the exon Parent alone, since 1,140 exon Parents own two
  distinct bodies **[?]**. The producer is upstream of `apply_cat_denovo_corrections.py` and is
  not in `/mnt/ssd/lalli/panSC`.
- **44 body paths are shorter than their own exon path** (against 1,094,513 longer and 136,690
  equal) **[B]**, unexplained.
- **Which program wrote ~49,000 `readthrough_transcript` tags on de-novo Parents** whose
  `source_transcript` is `N/A` **[?]**.
- **Whether the other eleven selected tags satisfy the no-dropped-tag property.** Only readthrough
  is audited today.
- **What order a new user *should* run these in**, as opposed to what was run. Stage 3's internal
  order is proven; the stage boundaries are historical and have not been re-derived as
  requirements.
