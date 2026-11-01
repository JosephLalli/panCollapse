#!/usr/bin/env python3
"""Regression for exact Parent models whose body and exon walks share no nodes.

The input paths are deliberately synthetic: each transcript-oriented body walk is
disjoint from its exon walk.  A body-only read is therefore admissible only via
the intrinsic-body orientation basis; the reverse walk must remain antisense.
"""

from __future__ import annotations

import json
import subprocess
import sys
import tempfile
from pathlib import Path

import pysam


HEADER = ["schema_version", "vg_path_name", "vg_path_length", "vg_haplotype_origins",
          "unique_parent", "source_parent", "input_parent", "canonical_transcript", "gene_id",
          "source_path_or_contig", "sample", "haplotype", "annotation_source", "feature_layer",
          "selection_status", "fallback_status", "exon_unique_parent", "gene_locus", "source_gene",
          "source_transcript", "transcript_class", "strand", "start", "end"]


def call(argv: list[object], ok: bool = True) -> subprocess.CompletedProcess:
    result = subprocess.run([str(x) for x in argv], capture_output=True, text=True)
    if (result.returncode == 0) != ok:
        raise AssertionError(f"unexpected exit {result.returncode}: {' '.join(map(str, argv))}\n{result.stderr}")
    return result


def ledger_row(path: str, parent: str, gene: str, layer: str, exon_parent: str, strand: str,
               length: int = 20) -> list[str]:
    # Linked body rows retain the exon Parent's provenance, as production ledgers do.
    return ["panSC-path-identity-v1", path, str(length), "H1", parent, f"SRC_{exon_parent}", f"IN_{exon_parent}",
            f"TX_{gene}", gene, "chr1", "S1", "H1", "CAT", layer, "selected", "none", exon_parent,
            "chr1:1-20", gene, f"SOURCE_TX_{gene}", "mapped", strand, "1", "20"]


def make_inputs(root: Path) -> tuple[Path, Path, Path, Path, Path, Path]:
    # PLUS paths use + handles. MINUS paths use the transcript-oriented - handles.
    # Neither body shares an exon node.
    gfa = """H\tVN:Z:1.0
S\t1\tAAAAAAAAAA
S\t2\tCCCCCCCCCC
S\t3\tGGGGGGGGGG
S\t4\tTTTTTTTTTT
S\t5\tACACACACAC
S\t6\tGTGTGTGTGT
S\t7\tAGAGAGAGAG
S\t8\tCTCTCTCTCT
P\tPLUS_EXON\t1+,2+\t*
P\tPLUS_BODY\t3+,4+\t*
P\tMINUS_EXON\t5-,6-\t*
P\tMINUS_BODY\t7-,8-\t*
"""
    graph = root / "graph.gfa"; graph.write_text(gfa)
    ledger = root / "ledger.tsv"
    rows = [HEADER,
            ledger_row("PLUS_EXON", "PLUS_EXON_PARENT", "PLUS_GENE", "exon", "PLUS_EXON_PARENT", "+"),
            ledger_row("PLUS_BODY", "PLUS_BODY_PARENT", "PLUS_GENE", "body", "PLUS_EXON_PARENT", "+"),
            ledger_row("MINUS_EXON", "MINUS_EXON_PARENT", "MINUS_GENE", "exon", "MINUS_EXON_PARENT", "-"),
            ledger_row("MINUS_BODY", "MINUS_BODY_PARENT", "MINUS_GENE", "body", "MINUS_EXON_PARENT", "-")]
    ledger.write_text("\n".join("\t".join(row) for row in rows) + "\n")
    (root / "t2g.tsv").write_text("PLUS_EXON\tPLUS_GENE\tTX_PLUS_GENE\nMINUS_EXON\tMINUS_GENE\tTX_MINUS_GENE\n")
    (root / "body_t2g.tsv").write_text("PLUS_BODY\tPLUS_GENE\tTX_PLUS_GENE\nMINUS_BODY\tMINUS_GENE\tTX_MINUS_GENE\n")
    (root / "whitelist.txt").write_text("AAACCCAAGTTTGGGA\n")
    # Forward means traversal in the emitted body path's own order, including MINUS_BODY.
    records = []
    for name, nodes, sequence, umi in (
        ("plus_fwd", [(3, False), (4, False)], "GGGGGGGGGGTTTTTTTTTT", "ACGTACGTACGT"),
        ("plus_rev", [(4, True), (3, True)], "AAAAAAAAAACCCCCCCCCC", "TGCAACGTTGCA"),
        ("minus_fwd", [(7, True), (8, True)], "CTCTCTCTCTAGAGAGAGAG", "GATCGATCGATC"),
        ("minus_rev", [(8, False), (7, False)], "CTCTCTCTCTAGAGAGAGAG", "CTAGCTAGCTAG"),
    ):
        records.append({"name": f"{name}_AAACCCAAGTTTGGGA_{umi}", "sequence": sequence,
                        "start": [0], "subpath": [{"score": 20, "path": {"mapping": [
                            {"position": {"node_id": str(node), "is_reverse": rev},
                             "edit": [{"from_length": 10, "to_length": 10}]} for node, rev in nodes]}}]})
    json_path = root / "reads.gamp.json"; json_path.write_text("\n".join(json.dumps(x) for x in records) + "\n")
    annotation = root / "annotation.gff3"
    annotation.write_text("##gff-version 3\n" + "".join(
        f"chr1\tt\tgene\t1\t20\t.\t{strand}\t.\tID={gene};gene_type=protein_coding\n"
        f"chr1\tt\ttranscript\t1\t20\t.\t{strand}\t.\tID={parent};Parent={gene};source_gene={gene};gene_type=protein_coding;transcript_type=protein_coding\n"
        for parent, gene, strand in (("PLUS_EXON_PARENT", "PLUS_GENE", "+"),
                                     ("PLUS_BODY_PARENT", "PLUS_GENE", "+"),
                                     ("MINUS_EXON_PARENT", "MINUS_GENE", "-"),
                                     ("MINUS_BODY_PARENT", "MINUS_GENE", "-"))))
    reference = root / "reference.gff3"; reference.write_text("##gff-version 3\n")
    return graph, ledger, json_path, annotation, reference, root / "whitelist.txt"


def assert_shared_controls(program: Path, vg: Path, root: Path) -> None:
    """D105: mixed body orientation is evidence, not a global path eligibility failure."""
    for name, body_steps, succeeds in (("opposite", "12-,13+,11-", True),
                                       ("mixed", "11+,13+,12-", True)):
        case = root / name; case.mkdir()
        (case / "graph.gfa").write_text(
            "H\tVN:Z:1.0\nS\t11\tAAAAAAAAAA\nS\t12\tCCCCCCCCCC\nS\t13\tGGGGGGGGGG\n"
            f"P\tEXON\t11+,12+\t*\nP\tBODY\t{body_steps}\t*\n")
        (case / "ledger.tsv").write_text("\n".join("\t".join(row) for row in (
            [HEADER, ledger_row("EXON", "EXON_PARENT", "CONTROL_GENE", "exon", "EXON_PARENT", "+"),
             ledger_row("BODY", "BODY_PARENT", "CONTROL_GENE", "body", "EXON_PARENT", "+", 30)])) + "\n")
        (case / "reads.gamp.json").write_text(json.dumps({
            "name": "control_AAACCCAAGTTTGGGA_ACGTACGTACGT", "sequence": "CCCCCCCCCCGGGGGGGGGGAAAAAAAAAA",
            "start": [0], "subpath": [{"score": 30, "path": {"mapping": [
                {"position": {"node_id": "12", "is_reverse": True}, "edit": [{"from_length": 10, "to_length": 10}]},
                {"position": {"node_id": "13", "is_reverse": False}, "edit": [{"from_length": 10, "to_length": 10}]},
                {"position": {"node_id": "11", "is_reverse": True}, "edit": [{"from_length": 10, "to_length": 10}]}]}}]}) + "\n")
        xg, gamp = case / "graph.xg", case / "reads.gamp"
        with xg.open("wb") as stream:
            subprocess.run([str(vg), "convert", "-g", str(case / "graph.gfa"), "-x"], check=True, stdout=stream)
        with gamp.open("wb") as stream:
            subprocess.run([str(vg), "view", "-J", "-K", "-k", str(case / "reads.gamp.json")], check=True, stdout=stream)
        result = call([program, "convert", "--gamp", gamp, "--xg", xg, "--path-identity-ledger", case / "ledger.tsv",
                       "--count-mode", "genefull_ex50pas", "--bam-multigene", "all", "--strand", "both",
                       "--out-dir", case / "out", "--bam-out", case / "out" / "reads.bam"], ok=succeeds)
        if not succeeds:
            assert "cannot establish body/exon orientation" in result.stderr, result.stderr
        else:
            with pysam.AlignmentFile(case / "out" / "reads.bam", "rb", check_sq=False) as bam:
                record = next(bam)
                tags = {tag: record.get_tag(tag).split(";") for tag in ("XR", "GT", "GD")}
                exact = [(tags["GT"][i], tags["GD"][i]) for i, tier in enumerate(tags["XR"]) if tier == "X"]
                # The shared-node case keeps its legacy tier; only its old opposite
                # orientation result is relevant to this regression.
                assert exact == [("P", "R")], exact


def main() -> None:
    program, vg, work = map(Path, sys.argv[1:])
    with tempfile.TemporaryDirectory(prefix="disjoint-body-", dir=work) as temp:
        root = Path(temp)
        graph, ledger, json_path, annotation, reference, whitelist = make_inputs(root)
        xg, gamp = root / "graph.xg", root / "reads.gamp"
        with xg.open("wb") as stream:
            subprocess.run([str(vg), "convert", "-g", str(graph), "-x"], check=True, stdout=stream)
        with gamp.open("wb") as stream:
            subprocess.run([str(vg), "view", "-J", "-K", "-k", str(json_path)], check=True, stdout=stream)

        convert_out = root / "convert"
        call([program, "convert", "--gamp", gamp, "--xg", xg, "--path-identity-ledger", ledger,
              "--count-mode", "genefull_ex50pas", "--bam-multigene", "all", "--strand", "both",
              "--out-dir", convert_out, "--bam-out", convert_out / "reads.bam"])
        expected = {"plus_fwd": ("PLUS_BODY", "PLUS_BODY_PARENT", "F"),
                    "plus_rev": ("PLUS_BODY", "PLUS_BODY_PARENT", "R"),
                    "minus_fwd": ("MINUS_BODY", "MINUS_BODY_PARENT", "F"),
                    "minus_rev": ("MINUS_BODY", "MINUS_BODY_PARENT", "R")}
        with pysam.AlignmentFile(convert_out / "reads.bam", "rb", check_sq=False) as bam:
            observed = {}
            for record in bam:
                name = record.query_name.split("_AAAC", 1)[0]
                values = {tag: record.get_tag(tag).split(";") for tag in ("XR", "GT", "XP", "XU", "GD")}
                exact = [(values["GT"][i], values["XP"][i], values["XU"][i], values["GD"][i])
                         for i, tier in enumerate(values["XR"]) if tier == "X"]
                assert len(exact) == 1, (name, exact)
                observed[name] = (exact[0][1], exact[0][2], exact[0][3])
                assert exact[0][0] == "B", (name, exact)
        assert observed == expected, observed
        assert_shared_controls(program, vg, root)

    print("disjoint_body_verify: deprecated-convert intrinsic body orientation passed")


if __name__ == "__main__":
    main()
