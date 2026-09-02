# Optional BAM export

panCollapse can emit a BAM alongside its alevin-fry `map.rad`, for counting with a
CellRanger-style stack: panSC's count_cr consumes the complete transcript ledger, while
`umi_tools count --per-gene --gene-tag` remains available for simpler `XT`-based exports. The RAD
remains the primary output; the BAM is opt-in via `--bam-out` and changes nothing about the RAD
(the `map.rad` is byte-identical with or without `--bam-out`).

## Why a BAM with no real coordinates

The point of counting off panCollapse rather than a genome-surjected BAM is to keep reads that
never surject to the linear reference (non-reference alleles, insertions absent from GRCh38).
panCollapse already derives each read's gene **from the graph's HST paths** (D048), so it never
surjects and never drops those reads.

One normal production BAM carries the evidence required by both Gene and exact
Ex50. Consumers require their selected mode's rows and permit the other
evidence. An opt-in debug sidecar records splice-edge counts and pre-flank
exact-top candidates without changing production classifications.

CellRanger itself cannot ingest a BAM (it re-aligns from FASTQ). "CellRanger-compatible" here
means the BAM follows 10x **tag conventions** so the CR-ecosystem counter can read it. And
because count_cr (and `umi_tools count --per-gene --gene-tag`) deduplicates by
`(cell, UMI, gene)` and **ignores alignment position**, feature records need no real coordinates.
Positions on those records are nominal. Barcode-only records are genuinely flagged unmapped and
carry no feature tags.

## Record layout

Exactly one record per valid input read group, independent of alignment/feature success. A
feature-bearing read gets the historical mapped record below. A read with no BAM count feature
(unaligned, no compatible transcript, or deliberately omitted by a BAM policy) gets one barcode-only
record so barcode correction sees the same all-read prior that STARsolo builds before alignment.

- **Feature record:** mapped (FLAG 0), placed at position 1 of a **synthetic per-gene contig** — one `@SQ` per
  gene, named by the gene id, with a nominal length. The contig is the read's *primary* gene =
  the first gene in its sorted gene set. The FLAG strand is nominal (every record is written
  forward); the true per-gene target orientation is carried explicitly in the `GD` tag (and in the
  RAD `dirs`), so a downstream counter — not panCollapse — owns the sense/antisense policy.
- QNAME = the original read name (the prefix before the raw CB/UMI in the GAMP name).
- SEQ/QUAL = the read's sequence/quality from the GAMP; CIGAR = `<length>M`.
- **Barcode-only record:** unmapped (FLAG 4; RNAME/POS unset), no
  `GX`/`GN`/`GD`/`TX`/`GL`/`GT`/`XP`/`XU`/`XT`, and `XB:Z:barcode_only`. It retains
  `CB`/`CR`, `UB`/`UR`, optional `CY`/`UY`, and
  available SEQ/QUAL.
  count_cr ignores it; correct_cb uses it only while constructing the exact-barcode prior and
  removes it from the corrected feature BAM.

### Tags (10x conventions)

| Tag | Meaning |
|-----|---------|
| `CB` / `CR` | cell barcode (raw). panCollapse carries only raw barcodes, so `CB` == `CR`. |
| `UB` / `UR` | UMI (raw). Likewise `UB` == `UR`. |
| `CY` | raw cell-barcode quality, parallel to `CB`, when the upstream name has `_cy<ASCII hex>`. |
| `UY` | raw UMI quality, parallel to `UB`/`UR`, when the upstream name has `_uy<ASCII hex>`. |
| `XB` | `barcode_only` only on an unmapped prior-evidence record; absent from feature records. |
| `GX` | compatible gene ids, `;`-separated. **`score` mode:** the **full set**, sorted and deduplicated; it remains gene-level even when a production-ledger record also has transcript-level `TX`/`XP`/`XU`, so it is not positionally parallel to `TX`. **Ledger count modes** (`gene`/`genefull`/`genefull_exonoverintron`/`genefull_ex50pas`): one entry per `TX`, positionally parallel, and a gene repeats for each compatible transcript. |
| `GN` | gene names paired with `GX`, one-to-one. The identity projection carries gene ids only, so `GN` == `GX`. |
| `GD` | **target orientation**, `;`-separated. **`score` mode:** one char per `GX` gene, positionally parallel to `GX` and not to production `TX`: `F` = sense/forward, `R` = antisense/reverse; a gene is `F` if any target for it is forward. **Ledger count modes:** one char per `TX`, positionally parallel to `TX`/`GX` and either `GL` or `GT`. Production and three-column legacy bodies compute it from the exact transcript/layer supplying the call; a two-column legacy body retains gene-level orientation. |
| `TX` | Compatible canonical transcript ids, `;`-separated. Present in every production-ledger BAM mode. Under `hst-v1`, it remains count-mode-only, preserving the old score-BAM layout. Ordinary modes emit one sorted entry per canonical target. Exact `genefull_ex50pas` deliberately permits repeated `TX` values because distinct locus Parents and distinct E/P/B evidence entries must survive separately. |
| `XP` | Production ledger only. Semicolon-separated groups parallel to `TX`. Ordinary modes comma-sort exact paths tied after path→Parent→canonical MAX collapse. Exact `genefull_ex50pas` instead emits exactly one path per evidence slot: the exon path for E/P and the linked body path for B. |
| `XU` | Production ledger only. Semicolon-separated groups parallel to `TX`. Ordinary modes comma-sort tied Parents. Exact `genefull_ex50pas` emits the one Parent belonging to that slot's `XP` path; therefore the Parent set derived from each `XP` group must equal its `XU` group. |
| `GL` | **non-Ex50 ledger modes only**, `;`-separated and positionally parallel to `TX`/`GX`/`GD` — one call per compatible transcript: `S` (spliced) or `U` (unspliced). Production ledger rows and a three-column `hst-v1` body t2g independently MAX-collapse exon/body paths to the same canonical key: a near-top exon score calls `S`; otherwise a near-top body score for that exact transcript calls `U`. A legacy two-column body retains D060's gene-body/span inference. D061 splice concordance gates every form. |
| `GT` | **production `genefull_ex50pas` only**, `;`-separated and parallel to `TX`/`GX`/`GD`/`XP`/`XU`: `E` = every reference-aligned base is exonic and all splice junctions are concordant; `P` = strictly more than half exonic; `B` = contained by the linked transcript body but neither E nor P. Exactly 50% is B; fully exonic with a discordant junction is P. `GT` replaces `GL` in this mode. |
| `XT` | a **single** resolved gene for `umi_tools --gene-tag=XT`. Present (= the one gene) when the read is compatible with exactly one gene; **omitted** when compatible with more than one gene, so a `--per-gene` counter skips it (matching CellRanger `soloMultiMappers Unique`). `--bam-multigene first` instead writes `XT` = the first gene; `all` also omits `XT` (like the default) but ONLY has an effect in a ledger `--count-mode`, where it additionally stops the read from being dropped before it reaches the BAM at all (see below). `GX`/`GN` always carry the complete compatible-gene information regardless (deduplicated in `score` mode, one entry per `TX` in ledger modes). |

The gene ids in `GX`/`XT` are exactly the identity-ledger (or legacy t2g) gene ids the RAD uses, so a
BAM-derived matrix and a RAD-derived matrix live over the same gene space.

## Header

- `@HD VN:1.6 SO:unsorted` — records are written in GAMP order; sort + index downstream.
- `@SQ` — one per gene (nominal length).
- `@PG` — `panCollapse`, its version, and the command line.

## CLI

- `--bam-out <path>` — write the BAM in addition to the RAD.
- `--bam-multigene {omit|first|all}` — `XT` policy for multi-gene reads; default `omit`. `all` is a
  ledger-`--count-mode`-only rescue path: see [GeneFull counting](genefull.md#multi-gene-bam-rescue).
- `--count-mode genefull_ex50pas` is a production BAM/count_cr evidence mode. It requires
  `--path-identity-ledger`, `--bam-out`, and `--bam-multigene all`; `--legacy-adapter hst-v1`
  is rejected because it cannot identify exact linked exon/body paths and Parents.
- `--debug-evidence-out <path>` adds the audit-only
  `panCollapse-debug-evidence-v1` normalized TSV sidecar. It is accepted only on that production
  superset invocation and is never emitted implicitly.
- `--no-ex50-score-window` disables v0.8's default inclusive top-minus-five eligibility filter and
  restores every complete compatible exact traversal before downstream Ex50 ordering. It is
  accepted only on a production-ledger count-mode BAM invocation.
- `--compact-exact-count-bam forward|reverse` is a nondefault, lossy research projection
  that serializes producer-selected exact winners instead of the information-complete
  typed-union evidence. Only the user may decide that it is necessary and authorize a use;
  the current decision is that it is not necessary. Every later authorized use requires a
  recorded case-specific reason and a prominent compact/lossy/experimental label in the run
  plan and result. It must not appear in a production default or ordinary recipe.

## Non-ledger multi-gene reads (`--count-mode score`, the default)

Everything above is the unconditional `score`-mode behavior: a multi-gene-compatible read is
ordinary D048 multimapping evidence and is ALWAYS written to both the RAD and the BAM (with the full
`GX` and, per `--bam-multigene`, an `omit`ted/`first`/absent `XT`). `--bam-multigene all` has no
additional effect here — there is nothing to rescue, since `score` mode never drops the read in the
first place.

## Ledger multi-gene reads (`--count-mode gene`/`genefull`/`genefull_exonoverintron`/`genefull_ex50pas`)

The ledger count modes are different: they apply CellRanger's `Unique` rule and DROP a multi-gene
read from the RAD (`docs/genefull.md`). Under the default BAM `omit` policy, that read has only an
unmapped barcode-prior record. `--bam-multigene all` instead emits its feature ledger —
see `docs/genefull.md#multi-gene-bam-rescue`. It is required for the D063 transcript-first path:
the full multi-gene `TX` ledger must reach count_cr so that transcript-to-gene pooling happens only
there, after S/U classification.

## Recipe

```sh
panCollapse convert --gamp reads.gamp --xg graph.spliced.xg \
  --path-identity-ledger path_identity_ledger.tsv --count-mode genefull \
  --out-dir out --bam-out out/reads.bam --bam-multigene all

samtools sort -o out/reads.sorted.bam out/reads.bam
samtools index out/reads.sorted.bam
```

For transcript-first ledger counting, use panSC's count_cr implementation so it can read
the parallel `TX`/`GX`/`GD` plus `GL` (ordinary modes) or `GT` (exact Ex50pAS) and the
`XP`/`XU` provenance, pool transcripts into genes, and resolve multi-gene UMIs.
The exact invocation is owned by the panSC workflow. Coordinate-sort and index the BAM first. The
older `umi_tools --gene-tag=XT` route remains usable for single-gene/legacy exports, but cannot
consume the complete multi-gene transcript ledger.

### Versioned production typed-union rows

Production ledger count-mode BAMs declare `@CO\tpanCollapse-evidence-schema:panCollapse-superset-v1`.
v0.8 BAMs additionally declare either `@CO\tpanCollapse-ex50-score-window:5`, pinning the default
inclusive complete-traversal score window, or
`@CO\tpanCollapse-ex50-score-window:disabled`, pinning the explicit all-compatible opt-out.
`XR:Z` is semicolon-parallel with `TX/GX/GD/GL/GT/XP/XU`: `G` rows carry ordinary `GL`, current
winning `XP/XU` provenance, and literal `.` in `GT`; `X` rows carry exact `GT/XP/XU` and literal
`.` in `GL`. All eight vectors have
the same number of entries. A literal `.` in an emitted identity is rejected by the production
ledger reader. Repeated `TX` rows are distinct evidence rows; `TX -> GX` remains single-valued and
only identical full rows deduplicate. Marker-absent BAMs retain the legacy interpretation.

### Audit debug sidecar

`--debug-evidence-out audit.tsv` writes one `read` row per input group, followed by one
`candidate` row per exon/body transcript whose raw score equals the global top before the
five-base flank admits near-top calls. The read row reports the number of distinct recognized
splice edges crossed by any alignment branch in the group. Candidate rows report their layer,
canonical transcript, gene, score, and splice-concordance result. Missing fields are the literal
`.` sentinel.

The key is `(input_group_ordinal, input_name, qname)`: ordinal preserves authoritative GAMP order,
`input_name` is the full GAMP group name, and `qname` is the BAM QNAME (or `.` when molecule-name
parsing failed). The file is promoted atomically from a temporary path only after conversion
finishes. It is diagnostic only: enabling it does not participate in tier classification, BAM tag
construction, RAD emission, or counting.

## Lossless vs. a genome-surjected count

panCollapse assigns genes purely from graph HST paths and never surjects to a linear reference,
so any read compatible with an HST gets a `GX` regardless of whether it would map to GRCh38.
The hermetic test fixture contains no linear-reference/backbone path at all, so the BAM parity
test — which checks that every graph-compatible read carries the gene set an independent oracle
derives — directly demonstrates this: genes are emitted with no linear reference present.
