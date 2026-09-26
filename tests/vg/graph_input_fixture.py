#!/usr/bin/env python3
"""Run an existing projection fixture against one of the three graph inputs."""
import fcntl
import hashlib
import os
from pathlib import Path
import subprocess
import sys

mode, vg, binary, *args = sys.argv[1:]
if mode not in {"xg", "gbz", "xg-gbwt"}:
    raise SystemExit(f"unknown test graph mode: {mode}")
if mode != "xg" and "--xg" in args and "--gbwt" not in args and "--gbz" not in args:
    offset = args.index("--xg")
    xg = Path(args[offset + 1])
    # Invalid/missing-input CLI tests must still reach the production parser.
    if xg.is_file():
        root = xg.parent / (xg.name + ".graph-input")
        root.mkdir(exist_ok=True)
        gbwt, gbz, pathfree = (root / name for name in ("paths.gbwt", "graph.gbz", "graph.pathfree.xg"))
        with (root / "build.lock").open("w") as lock:
            fcntl.flock(lock, fcntl.LOCK_EX)
            with xg.open("rb") as source:
                digest = hashlib.file_digest(source, "sha256").hexdigest()
            if not (root / "complete").exists() or (root / "complete").read_text().strip() != digest:
                with (root / "commands.log").open("w") as log:
                    def run(*command, output=None):
                        print(repr(command), file=log, flush=True)
                        subprocess.run(command, stdout=output or log, stderr=log, check=True)
                    # GBZ retains only GBWT-covered nodes. Preserve path-free nodes
                    # (including unassigned-read controls) via unselected fixture paths.
                    gfa = root / "graph.covered.gfa"
                    with gfa.open("w") as out:
                        run(vg, "convert", "-f", str(xg), output=out)
                    lines = gfa.read_text().splitlines()
                    nodes, covered = set(), set()
                    for line in lines:
                        fields = line.split("\t")
                        if fields[0] == "S":
                            nodes.add(fields[1])
                        elif fields[0] == "P":
                            covered.update(step[:-1] for step in fields[2].split(",") if step != "*")
                    with gfa.open("a") as out:
                        for node in sorted(nodes - covered, key=int):
                            out.write(f"P\tunselected_fixture_node_{node}\t{node}+\t*\n")
                    packed = root / "graph.pg"
                    with packed.open("wb") as out:
                        run(vg, "convert", "-g", str(gfa), "-p", output=out)
                    run(vg, "gbwt", "--num-threads", "2", "--num-jobs", "2", "-E", "-x", str(packed), "-o", str(gbwt))
                    run(vg, "gbwt", "--num-threads", "2", "--num-jobs", "2", "-x", str(packed), "-g", str(gbz), str(gbwt))
                    dropped = root / "graph.pathfree.pg"
                    with dropped.open("wb") as out:
                        run(vg, "paths", "-x", str(packed), "-d", output=out)
                    run(vg, "index", "-x", str(pathfree), str(dropped))
                (root / "complete").write_text(digest + "\n")
        args[offset:offset + 2] = (["--gbz", str(gbz)] if mode == "gbz" else
                                  ["--xg", str(pathfree), "--gbwt", str(gbwt)])
os.execv(binary, [binary, *args])
