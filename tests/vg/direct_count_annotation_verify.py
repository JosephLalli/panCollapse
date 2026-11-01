#!/usr/bin/env python3
"""Exercise annotation authority routing through the real CLI and output writer."""

from __future__ import annotations

import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

from direct_count_verify import EXPECTED_UMIS, profile_semantics, sha256


def main() -> None:
    binary, fixture, work, bundle_output = map(Path, sys.argv[1:])
    binary = binary.resolve()
    with tempfile.TemporaryDirectory(prefix="annotation-cli-", dir=work) as temporary:
        root = Path(temporary)
        annotation = root / "corrected.gff3"
        reference = root / "reference.gff3"
        ledger = root / "ledger.tsv"
        ledger.write_bytes((fixture / "path_identity_ledger.tsv").read_bytes())
        reference.write_text("##gff-version 3\n")
        annotation.write_text(
            "##gff-version 3\n"
            "chr1\tfixture\tgene\t1\t40\t.\t+\t.\tID=GENE;gene_type=protein_coding\n"
            + "".join(
                f"chr1\tfixture\ttranscript\t1\t40\t.\t+\t.\tID={parent};"
                f"Parent={owner};source_gene=GENE;gene_type=protein_coding;"
                "transcript_type=protein_coding\n"
                for parent, owner in (
                    ("EXON_PARENT_1", "GENE"),
                    ("BODY_PARENT_1", "EXON_PARENT_1"),
                    ("EXON_PARENT_2", "GENE"),
                    ("BODY_PARENT_2", "EXON_PARENT_2"),
                )
            )
        )
        common = [
            str(binary), "count", "--barcode-whitelist",
            str(fixture / "count_whitelist.txt"), "--cr7", "--pansc-strict-v1",
            "--threads", "1",
        ]
        graph = ["--gamp", str(work / "reads.gamp"), "--xg", str(work / "graph.xg")]
        authority = [
            "--annotation", str(annotation), "--annotation-reference", str(reference),
            "--path-identity-ledger", str(ledger),
        ]

        def invoke(arguments: list[str], output: Path) -> subprocess.CompletedProcess:
            return subprocess.run(
                common + arguments + ["--out-dir", str(output)],
                capture_output=True, text=True, timeout=30,
            )

        def count(name: str, selected_authority: list[str]) -> dict:
            output = root / name
            run = invoke(graph + selected_authority, output)
            assert run.returncode == 0, run.stderr
            manifest = json.loads((output / "manifest.json").read_text())
            assert manifest["fact_authority"] == "annotation"
            inputs = dict(zip(selected_authority[::2], selected_authority[1::2]))
            paths = {
                "annotation": Path(inputs["--annotation"]),
                "annotation_reference": Path(inputs["--annotation-reference"]),
                "path_identity_ledger": Path(inputs["--path-identity-ledger"]),
            }
            content = "".join(sha256(path) + "\n" for path in paths.values())
            assert manifest["fact_bundle_content_id"] == (
                "sha256:" + hashlib.sha256(content.encode()).hexdigest()
            )
            assert "count_bundle_manifest" not in manifest["inputs"]
            for role, path in paths.items():
                assert manifest["inputs"][role]["sha256"] == sha256(path)
            for profile in ("cr7-v1", "pansc-strict-v1"):
                counts, molecules, logical = profile_semantics(output, profile)
                assert counts == [("AAACCCAAGTTTGGGA", "GENE", 8)]
                assert {row[2] for row in molecules} == EXPECTED_UMIS
                assert (counts, molecules, logical) == profile_semantics(bundle_output, profile)
            # Display-name metadata is available only in the bundle fixture. Compare the
            # biological tables, not features.parquet's optional gene_name column.
            for table in ("barcodes", "counts", "molecules"):
                relative = Path("parquet") / f"{table}.parquet"
                assert (output / relative).read_bytes() == (bundle_output / relative).read_bytes()
            return manifest

        baseline = json.loads((bundle_output / "manifest.json").read_text())
        assert baseline["fact_authority"] == "count-bundle"
        original = count("plain", authority)
        # Whitespace changes input identity while leaving derived facts unchanged. The
        # ledger forbids empty rows, so remove its final newline instead of adding a row.
        for name, path in (("annotation", annotation), ("reference", reference), ("ledger", ledger)):
            contents = path.read_bytes()
            path.write_bytes(contents.rstrip(b"\n") if name == "ledger" else contents + b"\n")
            changed = count(f"changed-{name}", authority)
            assert changed["fact_bundle_content_id"] != original["fact_bundle_content_id"]
            path.write_bytes(contents)
        compressed_authority = authority.copy()
        for index in (1, 3):
            path = Path(authority[index])
            compressed = path.with_suffix(path.suffix + ".gz")
            compressed.write_bytes(gzip.compress(path.read_bytes(), mtime=0))
            compressed_authority[index] = str(compressed)
        count("gzip", compressed_authority)

        # Nonexistent graph/read paths make a late I/O failure distinguishable from the
        # required parse-time refusal. No output or initialization may occur in these cases.
        absent_graph = ["--gamp", str(root / "absent.gamp"), "--xg", str(root / "absent.xg")]
        bundle = ["--count-bundle", str(work / "count_bundle")]
        cases = [
            ("both", absent_graph + authority + bundle, "requires exactly one of"),
            ("neither", absent_graph, "requires exactly one of"),
            ("no-reference", absent_graph + authority[:2] + authority[4:], "requires --annotation-reference"),
            ("no-ledger", absent_graph + authority[:4],
             "requires --annotation-reference and --path-identity-ledger"),
            ("bundle-ledger", absent_graph + bundle + authority[4:], "accepted only with --annotation"),
            ("replay", ["--compatibility-in", str(root / "absent-compatibility"),
             "--max-alignments-per-read", "0"] + authority,
             "--compatibility-in replays against a --count-bundle"),
        ]
        for name, arguments, diagnostic in cases:
            output = root / f"refused-{name}"
            run = invoke(arguments, output)
            assert run.returncode == 1 and diagnostic in run.stderr, (name, run.stderr)
            assert "initialization:" not in run.stderr, (name, run.stderr)
            assert not output.exists(), name

    print("annotation CLI: five count runs, both profiles, three input identities, gzip, six refusals passed")


if __name__ == "__main__":
    main()
