#!/usr/bin/env python3
"""Turn a spliced transcript annotation into a per-transcript BODY annotation for `vg rna`.

`VG_RNA_INDEX` builds a body layer so panCollapse's ledger count modes can reproduce STARsolo
`GeneFull` (a read overlapping an intron still calls the gene), alongside the exonic `Gene`
matrix (decision rna-count-modes-gene-body). This helper turns a spliced transcript annotation
into that body annotation: for each transcript it emits ONE `exon` record spanning the
transcript's own TSS..polyA (its `transcript` row's coordinates when present, else the
`min(start)..max(end)` of its exons) -- an unspliced, intron-including span. `vg rna -y exon -s
transcript_id` on the result projects one contiguous body path per transcript (traversing intron
nodes, since a single-exon record gives `add_splice_junction_edges` nothing to splice); the t2g
then collapses those body paths to the gene, so a read landing anywhere in the transcript body
(exon or intron) is compatible with the path and counts (methods.md rna-genefull-intronic-scope).

Why per transcript, not per gene: this matches the per-source_transcript granularity of the
all-haplotype clustering recipe (selective_embed), so the same body annotation is correct whether
the index is built by reference projection or by clustered embedding. The union of a gene's
transcript bodies covers its locus, and the t2g collapses them to one gene.

Naming: each body's `transcript_id` is rewritten to the composite `<transcript_id>_<gene_id>`,
distinct from the spliced transcript path of the same name (which keeps the bare transcript_id),
so both layers can be embedded in the same graph and read from the same `genebody.xg`. `gene_id`
is preserved as its own attribute for direct recovery. A `<composite_name>\\t<gene_id>` sidecar
(`--t2g-out`) lets the caller build the body t2g without re-splitting the composite name (the
gene_id may itself contain underscores, so splitting `_[HR][0-9]+$`-stripped names on the last
underscore is not reliable). `--identity-map-out` additionally writes
`<composite_name>\\t<source transcript_id>\\t<gene_id>` so the body paths can be cross-linked to
their exon-path identities without parsing either identifier.

Coordinate source: prefers the transcript's own `transcript` row when the input annotation ships
one (GTF and GFF3 both may); falls back to the exon min..max collapse when a transcript row is
absent for a given transcript_id. Numerically identical to the exon-collapse span on every
transcript row checked so far (0 mismatches across the reference annotation used to validate
this), so the fallback is a robustness net, not an expected-common-case.

Format-robust: reads GTF (`transcript_id "X"; gene_id "Y"`) or GFF3 (`transcript_id=X`/`Parent=X`;
`gene_id=Y`), since the pipeline feeds a normalized GTF but the CAT annotations are GFF3. Emits GTF.
"""
from __future__ import annotations

import argparse
import re
import sys
from collections import OrderedDict

__version__ = "0.3.0"


def get_attr(attrs: str, key: str) -> str | None:
    """Value of `key` from a GTF (`key "v"`) or GFF3 (`key=v`) attribute column, else None."""
    m = re.search(rf'{key}[ =]"?([^";]+)"?', attrs)
    return m.group(1).strip() if m else None


def transcript_of(attrs: str) -> str | None:
    """Transcript id: GTF/GFF3 `transcript_id`, falling back to GFF3 `Parent`."""
    return get_attr(attrs, "transcript_id") or get_attr(attrs, "Parent")


def collapse(
    inp: str,
    out: str,
    t2g_out: str | None,
    identity_map_out: str | None = None,
) -> tuple[int, int, int]:
    """Build one TSS..polyA body span per transcript, preferring `transcript` row coordinates
    and falling back to the exon min..max collapse. Writes the body GTF to `out` and, if
    `t2g_out` is given, a `<composite_name>\\t<gene_id>` sidecar. If
    `identity_map_out` is given, also write the explicit composite-to-transcript-to-gene
    cross-link. Returns (n_exons_read, n_transcript_rows_read, n_bodies_written)."""
    # transcript id -> [seqid, source, start, end, strand, gene_id, from_transcript_row]
    tx: "OrderedDict[str, list]" = OrderedDict()
    n_exon = 0
    n_txrow = 0
    with open(inp) as f:
        for line in f:
            if line.startswith("#") or not line.strip():
                continue
            c = line.rstrip("\n").split("\t")
            if len(c) < 9 or c[2] not in ("exon", "transcript"):
                continue
            tid = transcript_of(c[8]) if c[2] == "exon" else (get_attr(c[8], "transcript_id") or get_attr(c[8], "ID"))
            if tid is None:
                continue
            s, e = int(c[3]), int(c[4])
            gid = get_attr(c[8], "gene_id") or tid
            if c[2] == "transcript":
                n_txrow += 1
                # transcript row is authoritative: overwrite any exon-derived span so far.
                tx[tid] = [c[0], c[1], s, e, c[6], gid, True]
                continue
            n_exon += 1
            if tid not in tx:
                tx[tid] = [c[0], c[1], s, e, c[6], gid, False]
            else:
                r = tx[tid]
                if r[6]:
                    # a transcript row already fixed this span; exons must not widen it.
                    continue
                r[2] = min(r[2], s)
                r[3] = max(r[3], e)
                if r[0] != c[0]:
                    sys.stderr.write(f"WARN {tid}: exons on >1 seqid ({r[0]},{c[0]})\n")
    n_from_txrow = 0
    t2g_lines = []
    identity_lines = []
    with open(out, "w") as o:
        for tid, r in tx.items():
            seqid, source, start, end, strand, gid, from_txrow = r
            if from_txrow:
                n_from_txrow += 1
            composite = f"{tid}_{gid}"
            attrs = f'transcript_id "{composite}"; gene_id "{gid}";'
            o.write("\t".join([seqid, source, "exon", str(start), str(end), ".", strand, ".", attrs]) + "\n")
            t2g_lines.append(f"{composite}\t{gid}\n")
            identity_lines.append(f"{composite}\t{tid}\t{gid}\n")
    if t2g_out:
        with open(t2g_out, "w") as tg:
            tg.writelines(t2g_lines)
    if identity_map_out:
        with open(identity_map_out, "w") as identity:
            identity.writelines(identity_lines)
    sys.stderr.write(f"gff3_to_genebody: {n_from_txrow}/{len(tx)} bodies from transcript rows\n")
    return n_exon, n_txrow, len(tx)


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("--in", dest="inp", required=True, help="spliced transcript annotation (GTF or GFF3) with exon (and optionally transcript) records")
    ap.add_argument("--out", required=True, help="body GTF: one TSS..polyA exon span per transcript, transcript_id rewritten to <transcript_id>_<gene_id>")
    ap.add_argument("--t2g-out", default=None, help="optional sidecar TSV: composite body transcript_id -> gene_id (one row per transcript)")
    ap.add_argument(
        "--identity-map-out",
        default=None,
        help="optional sidecar TSV: composite body transcript_id -> source transcript_id -> gene_id",
    )
    ap.add_argument("--version", action="version", version=__version__)
    a = ap.parse_args(argv)

    n_exon, n_txrow, n_body = collapse(
        a.inp, a.out, a.t2g_out, a.identity_map_out
    )
    print(f"gff3_to_genebody: exons={n_exon} transcript_rows={n_txrow} bodies={n_body}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
