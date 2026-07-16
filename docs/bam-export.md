# Optional BAM export

panCollapse can emit a BAM alongside its alevin-fry `map.rad`, for counting with a
CellRanger-style stack (`umi_tools count --per-gene --gene-tag`, then DropletUtils
`emptyDropsCellRanger`) instead of alevin-fry. The RAD remains the primary output; the BAM is
opt-in via `--bam-out` and changes nothing about the RAD (the `map.rad` is byte-identical with
or without `--bam-out`).

## Why a BAM with no real coordinates

The point of counting off panCollapse rather than a genome-surjected BAM is to keep reads that
never surject to the linear reference (non-reference alleles, insertions absent from GRCh38).
panCollapse already derives each read's gene **from the graph's HST paths** (D048), so it never
surjects and never drops those reads.

CellRanger itself cannot ingest a BAM (it re-aligns from FASTQ). "CellRanger-compatible" here
means the BAM follows 10x **tag conventions** so the CR-ecosystem counter can read it. And
because `umi_tools count --per-gene --gene-tag` (like CellRanger's UMI counting) deduplicates by
`(cell, UMI, gene)` and **ignores alignment position**, the BAM needs no real coordinates: each
record only has to be flagged mapped and carry the barcode, UMI, and gene tags. Positions here
are nominal.

## Record layout

One record per emitted read (exactly the reads that get a RAD record; reads with no compatible
gene are skipped, as they are in the RAD). Each record is:

- **Mapped** (FLAG 0), placed at position 1 of a **synthetic per-gene contig** — one `@SQ` per
  gene, named by the gene id, with a nominal length. The contig is the read's *primary* gene =
  the first gene in its sorted gene set. The FLAG strand is nominal (every record is written
  forward); the true per-gene target orientation is carried explicitly in the `GD` tag (and in the
  RAD `dirs`), so a downstream counter — not panCollapse — owns the sense/antisense policy.
- QNAME = the original read name (the prefix before the raw CB/UMI in the GAMP name).
- SEQ/QUAL = the read's sequence/quality from the GAMP; CIGAR = `<length>M`.

### Tags (10x conventions)

| Tag | Meaning |
|-----|---------|
| `CB` / `CR` | cell barcode (raw). panCollapse carries only raw barcodes, so `CB` == `CR`. |
| `UB` / `UR` | UMI (raw). Likewise `UB` == `UR`. |
| `GX` | the **full set** of compatible gene ids, sorted and `;`-separated. Never collapsed to one gene — emitting the complete set is what keeps the export lossless. |
| `GN` | gene names paired with `GX`. panCollapse's t2g carries only gene ids, so `GN` == `GX`. |
| `GD` | per-gene **target orientation**, one char per `GX` gene, `;`-separated and positionally parallel to `GX`: `F` = sense/forward (read aligns in the gene's path orientation), `R` = antisense/reverse. panCollapse runs strand-agnostic (default `--strand both`) and records the orientation here rather than filtering on it, so a downstream counter applies the sense/antisense policy (e.g. `count_cr.py --strand forward` = STARsolo sense-strand counting). A gene is `F` if any of the read's targets for it is forward. This is what lets strand be a *counter* choice while keeping panCollapse's per-read gene summary complete. |
| `XT` | a **single** resolved gene for `umi_tools --gene-tag=XT`. Present (= the one gene) when the read is compatible with exactly one gene; **omitted** when compatible with more than one gene, so a `--per-gene` counter skips it (matching CellRanger `soloMultiMappers Unique`). `--bam-multigene first` instead writes `XT` = the first gene; `all` also omits `XT` (like the default) but ONLY has an effect in a ledger `--count-mode`, where it additionally stops the read from being dropped before it reaches the BAM at all (see below). `GX`/`GN` always carry the full set regardless. |

The gene ids in `GX`/`XT` are exactly the t2g/`tx2gene.tsv` gene ids the RAD uses, so a
BAM-derived matrix and a RAD-derived matrix live over the same gene space.

## Header

- `@HD VN:1.6 SO:unsorted` — records are written in GAMP order; sort + index downstream.
- `@SQ` — one per gene (nominal length).
- `@PG` — `panCollapse`, its version, and the command line.

## CLI

- `--bam-out <path>` — write the BAM in addition to the RAD.
- `--bam-multigene {omit|first|all}` — `XT` policy for multi-gene reads; default `omit`. `all` is a
  ledger-`--count-mode`-only rescue path: see [GeneFull counting](genefull.md#multi-gene-bam-rescue).

## Non-ledger multi-gene reads (`--count-mode score`, the default)

Everything above is the unconditional `score`-mode behavior: a multi-gene-compatible read is
ordinary D048 multimapping evidence and is ALWAYS written to both the RAD and the BAM (with the full
`GX` and, per `--bam-multigene`, an `omit`ted/`first`/absent `XT`). `--bam-multigene all` has no
additional effect here — there is nothing to rescue, since `score` mode never drops the read in the
first place.

## Ledger multi-gene reads (`--count-mode gene`/`genefull`/`genefull_exonoverintron`/`genefull_ex50pas`)

The ledger count modes are different: they apply CellRanger's `Unique` rule and DROP a multi-gene
read from the RAD (`docs/genefull.md`). `--bam-multigene all` is the exception for the BAM only —
see `docs/genefull.md#multi-gene-bam-rescue`.

## Recipe

```sh
panCollapse convert --gamp reads.gamp --xg graph.spliced.xg --t2g t2g.tsv \
  --out-dir out --bam-out out/reads.bam

samtools sort -o out/reads.sorted.bam out/reads.bam
samtools index out/reads.sorted.bam

umi_tools count --per-gene --gene-tag=XT --per-cell \
  --extract-umi-method=tag --umi-tag=UB --cell-tag=CB \
  -I out/reads.sorted.bam -S out/counts.tsv
```

`umi_tools count` requires a coordinate-sorted, indexed BAM. Its default `--method=directional`
does 1-Hamming UMI correction within each `(cell, gene)`; the tests use `--method=unique` for a
deterministic count. Feed `counts.tsv` (or a matrix built from it) to DropletUtils
`emptyDropsCellRanger` for cell calling.

## Lossless vs. a genome-surjected count

panCollapse assigns genes purely from graph HST paths and never surjects to a linear reference,
so any read compatible with an HST gets a `GX` regardless of whether it would map to GRCh38.
The hermetic test fixture contains no linear-reference/backbone path at all, so the BAM parity
test — which checks that every graph-compatible read carries the gene set an independent oracle
derives — directly demonstrates this: genes are emitted with no linear reference present.
