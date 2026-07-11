#!/bin/sh
# Regression: a suffix-stripped (bare) tx<TAB>gene t2g must give the same ledger result as the
# path-keyed t2g. panCollapse strips each embedded path's trailing _R<n>/_H<n> copy suffix and
# falls back to the bare id, so the standard downstream tx2gene file works unchanged.
# Usage: bare_t2g_regression.sh <panCollapse-binary> <GF_FIX dir> <GF_DIR work dir>
set -eu
PANC="$1"; GF_FIX="$2"; GF_DIR="$3"
strip() { awk -F'\t' '{ sub(/_[HR][0-9]+$/, "", $1); print $1 "\t" $2 }' "$1" | sort -u; }
strip "$GF_FIX/t2g.tsv"          > "$GF_DIR/t2g.bare.tsv"
strip "$GF_DIR/genefull.t2g.tsv" > "$GF_DIR/body.bare.tsv"
"$PANC" convert --gamp "$GF_DIR/reads.gamp" --xg "$GF_DIR/genefull.xg" \
  --count-mode genefull --t2g "$GF_DIR/t2g.bare.tsv" --body-t2g "$GF_DIR/body.bare.tsv" \
  --out-dir "$GF_DIR/ledger_bare"
grep -qP '^emitted_groups\t2$' "$GF_DIR/ledger_bare/summary.tsv"
