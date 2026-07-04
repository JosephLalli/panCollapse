#!/bin/sh
# Build a small real-score GAMP for the scoring-fidelity CTest: index the tiny HST
# graph for mpmap, simulate spliced reads from HST paths, and map them. The GAMP's
# Subpath.score fields are then vg's own, so the per-node scorer can be checked
# against them. Args: <vg> <graph.gfa> <graph.xg> <workdir>
set -e
VG="$1"; GFA="$2"; XG="$3"; W="$4"
"$VG" convert -g "$GFA" -p > "$W/fid_g.vg" 2>/dev/null
"$VG" index -g "$W/fid_g.gcsa" -k 8 "$W/fid_g.vg" 2>/dev/null
"$VG" index -j "$W/fid_g.dist" "$XG" 2>/dev/null
"$VG" sim -x "$XG" -P NM_1.1_H1 -P NM_1.1_H2 -P NM_2.2_R1 \
      -n 30 -l 25 -e 0.04 -i 0.01 -s 7 -q > "$W/fid.fq" 2>/dev/null
"$VG" mpmap -n rna -x "$XG" -g "$W/fid_g.gcsa" -d "$W/fid_g.dist" \
      -F GAMP -f "$W/fid.fq" > "$W/fid.gamp" 2>/dev/null
