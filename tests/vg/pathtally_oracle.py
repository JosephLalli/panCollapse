#!/usr/bin/env python3
"""Independent GAMP-driven oracle for PathTally (increment 6).

Re-derives expected RAD from the GAMP by an implementation independent of the C++
production code: reproduces the documented per-node vg scoring, node-to-HST
attribution, top-score-plus-ties winner selection, transcript-id collapse, and
majority-of-bases orientation, then compares to panCollapse's RAD record for
each emitted read. It does not call or import any PathTally code.

Usage: pathtally_oracle.py subset.json graph_paths.gfa t2g.tsv map.rad
"""
import json
import re
import struct
import sys

MATCH, MISMATCH, GAP_OPEN, GAP_EXT, BONUS = 1, 4, 6, 1, 5
CB_LEN, UMI_LEN = 16, 12
_BASE = {"A": 0, "N": 0, "a": 0, "n": 0, "C": 1, "c": 1, "G": 2, "g": 2, "T": 3, "t": 3}
_SUFFIX = re.compile(r"_[HR]\d+$")


def transcript_id(hst):
    return _SUFFIX.sub("", hst)


def pack(seq):
    v = 0
    for ch in seq:
        v = (v << 2) | _BASE[ch]
    return v


def parse_molecule(name):
    i = name.rfind("_")
    if i <= 0 or i + 1 == len(name):
        return None
    j = name.rfind("_", 0, i)
    if j == -1:
        return None
    bc, umi = name[j + 1:i], name[i + 1:]
    if len(bc) != CB_LEN or len(umi) != UMI_LEN or not bc or not umi:
        return None
    if any(c not in _BASE for c in bc) or any(c not in _BASE for c in umi):
        return None
    return bc, umi


def subpath_read_offsets(mp):
    subs = mp.get("subpath", [])
    n = len(subs)
    read_len = []
    for sp in subs:
        total = 0
        for m in sp.get("path", {}).get("mapping", []):
            for e in m.get("edit", []):
                total += e.get("to_length", 0)
        read_len.append(total)
    offset = [0] * n
    known = [False] * n
    starts = mp.get("start")
    if starts:
        for s in starts:
            known[s] = True
    else:
        incoming = [False] * n
        for i, sp in enumerate(subs):
            for nx in sp.get("next", []):
                incoming[nx] = True
            for c in sp.get("connection", []):
                incoming[c["next"]] = True
        for i in range(n):
            known[i] = not incoming[i]
    for i, sp in enumerate(subs):
        if not known[i]:
            continue
        child = offset[i] + read_len[i]
        for nx in sp.get("next", []):
            if not known[nx]:
                offset[nx] = child
                known[nx] = True
        for c in sp.get("connection", []):
            nx = c["next"]
            if not known[nx]:
                offset[nx] = child
                known[nx] = True
    return offset


def score_subpath(sp, read_start, total_len):
    per_node, read_pos, in_del = [], read_start, False
    for m in sp.get("path", {}).get("mapping", []):
        ns = 0
        for e in m.get("edit", []):
            frm, to, seq = e.get("from_length", 0), e.get("to_length", 0), e.get("sequence", "")
            if frm > 0:
                if to > 0:
                    ns += MATCH * frm if seq == "" else -MISMATCH * frm
                    if read_pos == 0:
                        ns += BONUS
                    if read_pos + to == total_len:
                        ns += BONUS
                    in_del = False
                elif in_del:
                    ns -= frm * GAP_EXT
                else:
                    ns -= GAP_OPEN + (frm - 1) * GAP_EXT
                    in_del = True
            elif to > 0:
                at_end = read_pos == 0 or read_pos + to == total_len
                if not at_end:
                    ns -= GAP_OPEN + (to - 1) * GAP_EXT
                in_del = False
            read_pos += to
        per_node.append(ns)
    return per_node


def mapping_aligned_bases(m):
    return sum(e.get("from_length", 0) for e in m.get("edit", [])
               if e.get("from_length", 0) > 0 and e.get("to_length", 0) > 0)


def load_node_paths(gfa_path, hst_set, touched):
    node_paths = {}
    with open(gfa_path) as fh:
        for line in fh:
            f = line.rstrip("\n").split("\t")
            if len(f) < 3 or f[0] != "P" or f[1] not in hst_set:
                continue
            name = f[1]
            for step in f[2].split(","):
                node = step[:-1]
                if node not in touched:
                    continue
                node_paths.setdefault(node, []).append((name, step[-1] == "-"))
    return node_paths


def predict(group, node_paths):
    tally = {}  # hst -> [score, fwd_bases, rev_bases]
    for mp in group:
        total_len = len(mp.get("sequence", ""))
        offsets = subpath_read_offsets(mp)
        for si, sp in enumerate(mp.get("subpath", [])):
            per_node = score_subpath(sp, offsets[si], total_len)
            for mi, m in enumerate(sp.get("path", {}).get("mapping", [])):
                pos = m.get("position", {})
                node = str(pos.get("node_id"))
                read_rev = pos.get("is_reverse", False)
                ab = mapping_aligned_bases(m)
                for name, path_rev in node_paths.get(node, ()):  # noqa: default empty
                    t = tally.setdefault(name, [0, 0, 0])
                    t[0] += per_node[mi]
                    if read_rev == path_rev:
                        t[1] += ab
                    else:
                        t[2] += ab
    if not tally:
        return []
    top = max(v[0] for v in tally.values())
    per_tx = {}
    for name, (score, fwd, rev) in tally.items():
        if score != top:
            continue
        tx = transcript_id(name)
        agg = per_tx.setdefault(tx, [0, 0])
        agg[0] += fwd
        agg[1] += rev
    return sorted((tx, fwd >= rev) for tx, (fwd, rev) in per_tx.items())


def read_rad(path):
    with open(path, "rb") as fh:
        data = fh.read()
    off = [0]

    def take(n):
        b = data[off[0]:off[0] + n]
        off[0] += n
        return b

    def u(n):
        return int.from_bytes(take(n), "little")

    u(1)  # is_paired
    ref_count = u(8)
    names = []
    for _ in range(ref_count):
        ln = u(2)
        names.append(take(ln).decode())
    num_chunks = u(8)

    def skip_tags():
        widths = []
        for _ in range(u(2)):
            ln = u(2)
            take(ln)
            widths.append({1: 1, 2: 2, 3: 4, 4: 8}[u(1)])
        return widths

    skip_tags()             # file tags (cblen, ulen)
    read_w = skip_tags()    # read tags: [cb_width, umi_width]
    skip_tags()             # alignment tags
    u(2)  # cblen value
    u(2)  # ulen value
    records = []
    if num_chunks:
        u(4)  # chunk nbytes
        nrec = u(4)
        cb_w, umi_w = read_w[0], read_w[1]
        for _ in range(nrec):
            naln = u(4)
            cb = u(cb_w)
            umi = u(umi_w)
            refs = []
            for _ in range(naln):
                v = u(4)
                refs.append((names[v & 0x7FFFFFFF], bool(v & 0x80000000)))
            records.append((cb, umi, sorted(refs)))
    return records


def main():
    subset_json, gfa, t2g_path, rad = sys.argv[1:5]

    hst_gene = {}
    with open(t2g_path) as fh:
        for line in fh:
            p = line.split()
            if len(p) >= 2:
                hst_gene[p[0]] = p[1]
    hst_set = set(hst_gene)

    groups, cur, cur_name = [], [], None
    touched = set()
    with open(subset_json) as fh:
        for line in fh:
            mp = json.loads(line)
            name = mp.get("name", "")
            if name != cur_name:
                if cur:
                    groups.append((cur_name, cur))
                cur, cur_name = [], name
            cur.append(mp)
            for sp in mp.get("subpath", []):
                for m in sp.get("path", {}).get("mapping", []):
                    touched.add(str(m.get("position", {}).get("node_id")))
        if cur:
            groups.append((cur_name, cur))

    node_paths = load_node_paths(gfa, hst_set, touched)

    expected = []
    for name, group in groups:
        mol = parse_molecule(name)
        if mol is None:
            continue
        targets = predict(group, node_paths)
        if not targets:
            continue
        bc, umi = mol
        refs = sorted((tx, fwd) for tx, fwd in targets)
        expected.append((pack(bc), pack(umi), refs))

    actual = read_rad(rad)

    if len(expected) != len(actual):
        print(f"FAIL: emitted count oracle={len(expected)} rad={len(actual)}")
        return 1
    for i, (e, a) in enumerate(zip(expected, actual)):
        if e != a:
            print(f"FAIL at record {i}:\n  oracle={e}\n  rad   ={a}")
            return 1
    print(f"oracle: PASS ({len(actual)} emitted records match exactly; {len(groups)} read groups)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
