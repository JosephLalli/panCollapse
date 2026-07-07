#!/usr/bin/env bash
# Annotate a graph with gene-body paths so panCollapse can count GeneFull-style, and write the
# matching gene t2g.
#
# panCollapse already reads transcript membership off the graph's embedded HST paths (no custom
# index) -- the t2g just says which path names to use and their gene. Gene bodies (exon AND
# intron) work the same way: project each GTF *gene* span as a single, unspliced vg rna path
# (--feature-type gene grouped by gene_id) and embed it. Because a gene is one feature there are
# no exon junctions to splice, so the path is contiguous over the whole locus and covers intron
# nodes too. A read entirely in an intron then lands on that gene-body path and calls the gene.
#
# GeneFull is therefore not a mode in panCollapse: it is spliced counting run against a
# gene-body-annotated graph with a gene t2g. The output graph keeps the original HST paths, so
# the SAME graph serves both counts -- pick spliced vs GeneFull by which t2g you pass.
#
# Pass the haplotype GBWT (-l) so gene bodies are threaded through the pangenome haplotypes and
# cover non-reference (alt) nodes; otherwise only reference nodes are, and reads on alt alleles
# (the ones counting off the graph is meant to keep) are missed. Always pass -l for a real run.
#
# Usage: scripts/make-gene-annotation.sh -x graph.xg|graph.vg|graph.gfa -n annotation.gtf \
#          -o out_prefix [-l haplotypes.gbwt] [-s gene_id] [--vg /path/to/vg]
# Writes: <out_prefix>.xg      graph with gene-body paths embedded (plus the original paths)
#         <out_prefix>.t2g.tsv gene-body path name <TAB> gene id
set -euo pipefail

VG=vg
GRAPH=""; GTF=""; OUT=""; GBWT=""; TAG="gene_id"
while [ "$#" -gt 0 ]; do
  case "$1" in
    -x|--graph) GRAPH="$2"; shift 2 ;;
    -n|--gtf) GTF="$2"; shift 2 ;;
    -o|--out) OUT="$2"; shift 2 ;;
    -l|--haplotypes) GBWT="$2"; shift 2 ;;
    -s|--gene-tag) TAG="$2"; shift 2 ;;
    --vg) VG="$2"; shift 2 ;;
    -h|--help) grep '^#' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "unknown argument: $1" >&2; exit 2 ;;
  esac
done
if [ -z "$GRAPH" ] || [ -z "$GTF" ] || [ -z "$OUT" ]; then
  echo "error: -x graph, -n gtf, and -o out_prefix are required (see --help)" >&2; exit 2
fi

hap_args=()
[ -n "$GBWT" ] && hap_args=(-l "$GBWT" -e -a)

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

# vg rna needs a mutable graph; convert whatever was passed (xg/vg/gbz/gfa) to a packed graph.
case "$GRAPH" in
  *.gfa) "$VG" convert -g "$GRAPH" -p > "$tmp/graph.pg" ;;
  *)     "$VG" convert "$GRAPH" -p > "$tmp/graph.pg" ;;
esac

# Embed one unspliced gene-body path per gene (named by gene id + a copy suffix vg rna adds).
# Record path names before/after so the gene t2g lists only the newly added gene-body paths --
# the input graph may already carry HST transcript paths (also _H<n>/_R<n>-suffixed).
echo "embedding gene-body paths with vg rna (--feature-type gene, group by ${TAG})" >&2
"$VG" paths -x "$tmp/graph.pg" -L 2>/dev/null | sort > "$tmp/before.txt"
"$VG" rna -n "$GTF" -y gene -s "$TAG" -r "${hap_args[@]}" "$tmp/graph.pg" > "$tmp/annotated.vg"
"$VG" paths -x "$tmp/annotated.vg" -L 2>/dev/null | sort > "$tmp/after.txt"

echo "writing annotated graph -> ${OUT}.xg" >&2
"$VG" convert "$tmp/annotated.vg" -x > "${OUT}.xg"

# Gene t2g: each new gene-body path -> its gene (strip the _H<n>/_R<n> copy suffix), the same
# form as an HST t2g. panCollapse reads it exactly like the transcript t2g.
echo "writing gene t2g -> ${OUT}.t2g.tsv" >&2
comm -13 "$tmp/before.txt" "$tmp/after.txt" \
  | awk '{ gene = $0; sub(/_[HR][0-9]+$/, "", gene); print $0 "\t" gene }' \
  | sort -u > "${OUT}.t2g.tsv"

echo "wrote $(wc -l < "${OUT}.t2g.tsv") gene-body paths for $(cut -f2 "${OUT}.t2g.tsv" | sort -u | wc -l) genes" >&2
