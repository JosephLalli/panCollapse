#!/usr/bin/env python3
"""Create a deterministic multi-Parent production-ledger exact-Ex50 fixture."""

import argparse
import json
import pathlib


def row(path, length, parent, target, gene, layer, exon_parent, provenance_parent):
    return [
        "panSC-path-identity-v1", path, str(length), "HAP1", parent,
        f"SOURCE_{provenance_parent}", f"INPUT_{provenance_parent}", target, gene, "chr1", "SAMPLE",
        "H1", "CAT", layer, "selected", "none", exon_parent, "chr1:1-30",
        gene, f"SOURCE_TX_{target}", "mapped", "+", "1", "30",
    ]


def main(output, invalid=False, parent_count=64, paths_per_parent=1, read_count=None,
         reverse_path_order=False, include_body_tier_read=False):
    if read_count is None:
        read_count = parent_count
    output.mkdir(parents=True, exist_ok=True)
    gfa = ["H\tVN:Z:1.0", "S\t1\tAAAAAAAAAA"]
    ledger = [[
        "schema_version", "vg_path_name", "vg_path_length", "vg_haplotype_origins",
        "unique_parent", "source_parent", "input_parent", "canonical_transcript", "gene_id",
        "source_path_or_contig", "sample", "haplotype", "annotation_source", "feature_layer",
        "selection_status", "fallback_status", "exon_unique_parent", "gene_locus", "source_gene",
        "source_transcript", "transcript_class", "strand", "start", "end",
    ]]
    t2g = []
    body_t2g = []
    reads = []
    for index in range(parent_count):
        intron = 1000 + index
        endpoint = 2000 + index
        parent = f"PARENT_{index:03d}"
        target = f"CANON_{index:03d}"
        gene = f"GENE_{index:03d}"
        exon_paths = [
            f"{parent}_EXON" if paths_per_parent == 1 else f"{parent}_EXON_{path_index:03d}"
            for path_index in range(paths_per_parent)
        ]
        body_paths = [
            f"{parent}_BODY" if paths_per_parent == 1 else f"{parent}_BODY_{path_index:03d}"
            for path_index in range(paths_per_parent)
        ]
        exon_length = 20
        body_length = 30
        if invalid and index in (0, 10):
            # Parent 000 is deliberately much slower than Parent 010. With eight workers the
            # later error should finish first, but PanCollapse must still report lexical Parent 000.
            exon_node = 3000 + index
            body_nodes = ([10000 + offset for offset in range(10000)]
                          if index == 0 else [1, intron, endpoint])
            gfa.append(f"S\t{exon_node}\tTTTTTTTTTT")
            if index == 0:
                gfa.extend(f"S\t{node}\tCCCCCCCCCC" for node in body_nodes)
                gfa.extend(
                    f"L\t{left}\t+\t{right}\t+\t0M"
                    for left, right in zip(body_nodes, body_nodes[1:]))
            else:
                gfa.extend((
                    f"S\t{intron}\tCCCCCCCCCC", f"S\t{endpoint}\tGGGGGGGGGG",
                    f"L\t1\t+\t{intron}\t+\t0M",
                    f"L\t{intron}\t+\t{endpoint}\t+\t0M",
                ))
            gfa.extend(f"P\t{path}\t{exon_node}+\t*" for path in exon_paths)
            body_steps = ",".join(f"{node}+" for node in body_nodes)
            gfa.extend(f"P\t{path}\t{body_steps}\t*" for path in body_paths)
            exon_length = 10
            body_length = 10 * len(body_nodes)
        else:
            gfa.extend((
                f"S\t{intron}\tCCCCCCCCCC", f"S\t{endpoint}\tGGGGGGGGGG",
                f"L\t1\t+\t{intron}\t+\t0M", f"L\t{intron}\t+\t{endpoint}\t+\t0M",
                f"L\t1\t+\t{endpoint}\t+\t0M",
            ))
            gfa.extend(f"P\t{path}\t1+,{endpoint}+\t*" for path in exon_paths)
            gfa.extend(f"P\t{path}\t1+,{intron}+,{endpoint}+\t*" for path in body_paths)
        for exon_path in exon_paths:
            ledger.append(row(exon_path, exon_length, parent, target, gene, "exon", parent, parent))
            # Keep all raw exon/body paths of one Parent on the same canonical target. This
            # makes the legacy transcript-body geometry path exercise the same reuse shape as
            # the production identity ledger instead of multiplying geometry units by aliases.
            t2g.append((exon_path, gene, target))
        for path_index, body_path in enumerate(body_paths):
            ledger.append(row(body_path, body_length, f"BODY_{index:03d}_{path_index:03d}",
                              target, gene, "body", parent, parent))
            body_t2g.append((body_path, gene, target))
        if index < read_count:
            reads.append({
                "name": f"read_{index:03d}_AAACCCAAGTTTGGGA_ACGTACGTACGT",
                "sequence": "AAAAAAAAAAGGGGGGGGGG", "start": [0],
                "subpath": [{"score": 20, "path": {"mapping": [
                    {"position": {"node_id": "1"}, "edit": [{"from_length": 10, "to_length": 10}]},
                    {"position": {"node_id": str(endpoint)}, "edit": [{"from_length": 10, "to_length": 10}]},
                ]}}],
            })
    if include_body_tier_read:
        if invalid:
            raise ValueError("--include-body-tier-read is only valid for the clean fixture")
        reads.append({
            "name": "body_tier_AAACCCAAGTTTGGGA_ACGTACGTACGT",
            "sequence": "CCCCCCCCCC", "start": [0],
            "subpath": [{"score": 10, "path": {"mapping": [
                {"position": {"node_id": "1000"},
                 "edit": [{"from_length": 10, "to_length": 10}]},
            ]}}],
        })
    if reverse_path_order:
        # Deliberately make numeric XG path-handle order disagree with lexical path/Parent
        # order. Deterministic output must derive from identity ranks, not graph insertion.
        non_paths = [line for line in gfa if not line.startswith("P\t")]
        paths = [line for line in gfa if line.startswith("P\t")]
        gfa = non_paths + list(reversed(paths))
    (output / "graph.gfa").write_text("\n".join(gfa) + "\n")
    (output / "path_identity_ledger.tsv").write_text(
        "\n".join("\t".join(fields) for fields in ledger) + "\n")
    (output / "t2g.tsv").write_text(
        "\n".join("\t".join(fields) for fields in t2g) + "\n")
    (output / "body_t2g.tsv").write_text(
        "\n".join("\t".join(fields) for fields in body_t2g) + "\n")
    (output / "reads.gamp.json").write_text(
        "\n".join(json.dumps(read, separators=(",", ":")) for read in reads) + "\n")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("--invalid", action="store_true")
    parser.add_argument("--parents", type=int, default=64)
    parser.add_argument("--paths-per-parent", type=int, default=1)
    parser.add_argument("--reads", type=int)
    parser.add_argument("--reverse-path-order", action="store_true")
    parser.add_argument("--include-body-tier-read", action="store_true")
    args = parser.parse_args()
    if args.parents < 1 or args.paths_per_parent < 1 or (args.reads is not None and args.reads < 1):
        parser.error("--parents, --paths-per-parent, and --reads must be positive")
    main(args.output, args.invalid, args.parents, args.paths_per_parent, args.reads,
         args.reverse_path_order, args.include_body_tier_read)
