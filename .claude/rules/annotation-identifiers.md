# Annotation identifier rules

A transcript row in `corrected_annotation.gff3` carries four identifier-shaped
attributes. Only one of them is a key.

| attribute | on a transcript row | safe to key on |
|---|---|---|
| `ID` | the transcript | **yes — the only unique key** |
| `Parent` | the **gene** (it is the transcript on *exon* rows) | no, meaning is level-dependent |
| `transcript_id` | bare, assembly-local | **no** |
| `gene_id` | bare, assembly-local (qualified on *gene* rows) | **no** |

**Never key, join, group, or de-duplicate on `transcript_id`, or on `gene_id`
from a transcript row.** Use `ID`.

## Why

`ID` is unique across all 2,461,354 transcript rows — zero duplicates — in four
shapes, each unambiguous: PanSN-qualified `sample#hap#local` (1,216,194),
`panSCbody1_<sha256>` content hashes (1,231,247), `CHM13_*` (13,148, one haploid
assembly), and `LOFF_*` (765).

`transcript_id` is not. HPRC's CAT release numbers each assembly's transcripts
sequentially from `T0000001`, and for the 103 samples released as `hap1`/`hap2`
both haplotypes carry the same `_ha_` prefix, so the two ID spaces coincide
completely — 413,962 shared identifiers for HG00097 alone. The 127 samples
released as `mat`/`pat` get distinct `_ma_`/`_pa_` prefixes and never collide.
Within chr20/21/22, 5,237 bare ids are shared by two haplotypes across 57 donors.

This is upstream in `hprc_r2_v1.0.1_cat_v1.3` as published, not introduced here.
Contig names are GenBank accessions, unique per assembly and PanSN-qualified
throughout, so the graph and its node ids are unaffected.

## What it costs when ignored

`panSC/scripts/build_parent_category_ledger.py` de-duplicated on `transcript_id`
and so skipped the second haplotype of every colliding pair before reading its
tags. 25,893 ledger rows lost categories, 18,247 of them left completely empty,
including 2,333 transcripts that lost `readthrough_transcript` — an excluded
category, so the exclusion could never fire on them. Gene-row tag inheritance
was affected too, which is why `overlaps_pseudogene` alone accounts for 19,377
of the restored rows.
