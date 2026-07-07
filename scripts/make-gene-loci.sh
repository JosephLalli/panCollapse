#!/usr/bin/env bash
# Build a panCollapse --gene-loci map (node_id<TAB>gene<TAB>strand) from a graph + GTF.
#
# --gene-mode full needs, per graph node, which gene bodies (exons AND introns) cover it. That
# map is obtained by projecting each GTF *gene* span as a single, unspliced vg rna path: with
# --feature-type gene grouped by gene_id, each gene becomes one contiguous path over its whole
# locus (no exon splicing), so the path covers intron nodes too. The node -> gene map is then
# read off those gene-body paths' GFA P-lines.
#
# Pass a haplotype GBWT (-l) to thread the gene-body paths through the pangenome haplotypes so
# non-reference (alt) nodes inside each gene body are covered -- otherwise only reference nodes
# are, and reads on alt alleles (the ones counting off the graph is meant to keep) are missed.
#
# Usage: scripts/make-gene-loci.sh -x graph.vg|graph.xg -n annotation.gtf -o gene_loci.tsv \
#          [-l haplotypes.gbwt] [-s gene_id] [--vg /path/to/vg]
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
  echo "error: -x graph, -n gtf, and -o out are required (see --help)" >&2; exit 2
fi

hap_args=()
[ -n "$GBWT" ] && hap_args=(-l "$GBWT" -e)

tmp="$(mktemp -d)"; trap 'rm -rf "$tmp"' EXIT

# vg rna needs a mutable graph (it embeds paths); convert whatever was passed (xg/vg/gbz/gfa)
# to a packed graph first.
case "$GRAPH" in
  *.gfa) "$VG" convert -g "$GRAPH" -p > "$tmp/graph.pg" ;;
  *)     "$VG" convert "$GRAPH" -p > "$tmp/graph.pg" ;;
esac

# Project each gene's full span as one unspliced gene-body path named by gene id (+ a copy
# suffix vg rna appends). -r embeds reference gene bodies; with -l/-e, -a would add haplotype
# copies for alt coverage. Record the path names before and after so only the newly added
# gene-body paths are read -- the input graph may already carry HST transcript paths (which
# also have _H<n>/_R<n> suffixes) that must not be mistaken for gene bodies.
echo "projecting gene bodies with vg rna (--feature-type gene, group by ${TAG})" >&2
"$VG" paths -x "$tmp/graph.pg" -L 2>/dev/null | sort > "$tmp/before.txt"
"$VG" rna -n "$GTF" -y gene -s "$TAG" -r "${hap_args[@]}" "$tmp/graph.pg" > "$tmp/genebody.vg"
"$VG" paths -x "$tmp/genebody.vg" -L 2>/dev/null | sort > "$tmp/after.txt"
comm -13 "$tmp/before.txt" "$tmp/after.txt" > "$tmp/genepaths.txt"

# Read node -> (gene, strand) off the new gene-body path P-lines. The path name is the gene id
# plus a trailing _H<n>/_R<n> copy suffix that vg rna adds; strip it to recover the gene. Each
# P-line step is node<orient>; orient (+/-) is the gene's strand across that node.
echo "extracting node -> gene from gene-body paths -> ${OUT}" >&2
"$VG" convert "$tmp/genebody.vg" -f 2>/dev/null | awk -F'\t' -v pf="$tmp/genepaths.txt" '
  BEGIN { while ((getline l < pf) > 0) keep[l] = 1 }
  $1=="P" && ($2 in keep) {
    gene = $2; sub(/_[HR][0-9]+$/, "", gene);
    n = split($3, steps, ",");
    for (i = 1; i <= n; i++) {
      step = steps[i];
      orient = substr(step, length(step), 1);
      node = substr(step, 1, length(step) - 1);
      strand = (orient == "-") ? "-" : "+";
      print node "\t" gene "\t" strand;
    }
  }' | sort -u > "$OUT"

echo "wrote $(wc -l < "$OUT") node->gene rows for $(cut -f2 "$OUT" | sort -u | wc -l) genes" >&2
