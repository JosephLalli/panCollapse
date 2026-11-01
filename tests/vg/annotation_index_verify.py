#!/usr/bin/env python3
"""Exact raw-annotation/index parity using the existing real GAMP toy fixture."""
import csv
import gzip
import hashlib
import json
import subprocess
import sys
import tempfile
from pathlib import Path

import pyarrow.parquet as pq


def run(command, succeeds=True):
    result = subprocess.run([str(x) for x in command], text=True, capture_output=True)
    if (result.returncode == 0) != succeeds:
        raise AssertionError(f"unexpected exit: {command}\n{result.stderr}")
    return result


def counters(output):
    with (output / "summary.tsv").open() as handle:
        rows = csv.reader(handle, delimiter="\t")
        next(rows)
        return {tuple(row[:3]): row[3] for row in rows
                if not row[2].startswith(("assignment_cache_", "aggregate_spill_"))}


def main():
    program, fixture, inputs = map(Path, sys.argv[1:])
    root = Path(tempfile.mkdtemp(prefix="annotation-index-", dir=inputs))
    print(f"retained index parity outputs: {root}", flush=True)
    ledger = root / "ledger.tsv"
    ledger.write_bytes((fixture / "path_identity_ledger.tsv").read_bytes())
    with ledger.open() as handle:
        rows = list(csv.DictReader(handle, delimiter="\t"))
    genes = sorted({row["gene_id"] for row in rows})
    annotation = "##gff-version 3\n"
    for gene in genes:
        annotation += f"chr1\tt\tgene\t1\t100\t.\t+\t.\tID={gene};gene_type=protein_coding\n"
    for row in rows:
        annotation += ("chr1\tt\ttranscript\t1\t100\t.\t+\t.\t"
                       f"ID={row['unique_parent']};Parent={row['gene_id']};"
                       f"source_gene={row['source_gene']};gene_type=protein_coding;"
                       "transcript_type=protein_coding\n")
    annotation_path, reference = root / "a.gff3.gz", root / "r.gff3.gz"
    annotation_path.write_bytes(gzip.compress(annotation.encode(), mtime=0))
    reference.write_bytes(gzip.compress(b"##gff-version 3\n", mtime=0))
    index = root / "facts.pci"
    build = [program, "index", "--annotation", annotation_path,
             "--annotation-reference", reference, "--path-identity-ledger", ledger]
    receipt = json.loads(run(build + ["--out", index]).stdout)
    assert receipt["status"] == "complete"
    other = root / "repeat.pci"
    run(build + ["--out", other])
    assert index.read_bytes() == other.read_bytes(), "index bytes not deterministic"
    run(build + ["--out", index], succeeds=False)
    common = [program, "count", "--gamp", inputs / "reads.gamp", "--xg", inputs / "graph.xg",
              "--barcode-whitelist", fixture / "count_whitelist.txt",
              "--cr7", "--pansc-strict-v1", "--10x-mex", "--rad-out",
              "--read-assignments-out", "diagnostics/reads.parquet"]
    raw = root / "raw"
    run(common + ["--path-identity-ledger", ledger, "--annotation", annotation_path, "--annotation-reference", reference,
                  "--threads", 1, "--out-dir", raw])
    # V3 loading must not read the source annotation or external ledger.
    annotation_path.unlink()
    reference.unlink()
    hidden_ledger = root / "ledger-hidden.tsv"
    ledger.rename(hidden_ledger)
    for threads in (1, 4):
        output = root / f"indexed-{threads}"
        result = run(common + ["--annotation-index", index, "--threads", threads, "--out-dir", output])
        assert "phase=annotation_index_load" in result.stderr
        assert "phase=annotation_count_facts" not in result.stderr
        for name in ("barcodes", "features", "counts", "molecules"):
            table = Path("parquet") / f"{name}.parquet"
            assert pq.read_table(raw / table).to_pylist() == pq.read_table(output / table).to_pylist()
        assert pq.read_table(raw / "diagnostics/reads.parquet").to_pylist() == pq.read_table(output / "diagnostics/reads.parquet").to_pylist()
        assert counters(raw) == counters(output), "semantic counters changed"
        assert (raw / "map.rad").read_bytes() == (output / "map.rad").read_bytes(), "RAD changed"
        expected = json.loads((raw / "manifest.json").read_text())
        actual = json.loads((output / "manifest.json").read_text())
        assert expected["fact_bundle_content_id"] == actual["fact_bundle_content_id"]
        assert expected["fact_authority"] == actual["fact_authority"]
        assert expected["count_storage"] == actual["count_storage"] == "memory-only"
        assert "memory_budget_bytes" not in actual
        with (output / "summary.tsv").open() as handle:
            for row in list(csv.reader(handle, delimiter="\t"))[1:]:
                if row[2].startswith("aggregate_spill_"):
                    assert int(row[3]) == 0
        assert not list(output.rglob("*.zst")), "count aggregate spill file was written"
        assert actual["annotation_index_derivation_id"].startswith("sha256:")
        assert {k: v["logical_sha256"] for k, v in expected["tables"].items()} == {
            k: v["logical_sha256"] for k, v in actual["tables"].items()}
        for path in (raw / "mex").rglob("*.gz"):
            relative = path.relative_to(raw)
            assert gzip.decompress(path.read_bytes()) == gzip.decompress((output / relative).read_bytes())
    # Explicit GTF compilation must preserve the same biological facts as GFF3.
    ledger = hidden_ledger
    gtf = ""
    for gene in genes:
        gtf += f'chr1\tt\tgene\t1\t100\t.\t+\t.\tgene_id "{gene}"; gene_type "protein_coding";\n'
    for row in rows:
        gtf += (f'chr1\tt\ttranscript\t1\t100\t.\t+\t.\tgene_id "{row["gene_id"]}"; '
                f'transcript_id "{row["unique_parent"]}"; source_gene "{row["source_gene"]}"; '
                'gene_type "protein_coding"; transcript_type "protein_coding";\n')
    gtf_path, gtf_reference = root / "a.gtf.gz", root / "r.gtf.gz"
    gtf_path.write_bytes(gzip.compress(gtf.encode(), mtime=0))
    gtf_reference.write_bytes(gzip.compress(b"# GTF reference\n", mtime=0))
    gtf_index = root / "gtf.pci"
    run([program, "index", "--annotation", gtf_path, "--annotation-format", "gtf",
         "--annotation-reference", gtf_reference, "--annotation-reference-format", "gtf",
         "--path-identity-ledger", ledger, "--out", gtf_index])
    gtf_path.unlink(); gtf_reference.unlink()
    gtf_output = root / "indexed-gtf"
    run(common + ["--annotation-index", gtf_index, "--out-dir", gtf_output])
    for table in ("parquet/barcodes.parquet", "parquet/features.parquet", "parquet/counts.parquet",
                  "parquet/molecules.parquet", "diagnostics/reads.parquet"):
        assert pq.read_table(raw / table).to_pylist() == pq.read_table(gtf_output / table).to_pylist()
    assert counters(raw) == counters(gtf_output)
    # Invalid input must fail before an intentionally absent graph can be opened.
    bad = root / "corrupt.pci"
    data = bytearray(index.read_bytes()); data[-1] ^= 1; bad.write_bytes(data)
    invalid = [program, "count", "--gamp", inputs / "reads.gamp", "--xg", root / "missing.xg",
               "--barcode-whitelist", fixture / "count_whitelist.txt",
               "--cr7", "--out-dir", root / "not-published"]
    failure = run(invalid + ["--annotation-index", bad], succeeds=False)
    assert "checksum mismatch" in failure.stderr and "cannot open xg" not in failure.stderr
    assert not (root / "not-published").exists()
    # Re-checksummed malicious metadata must still pass semantic validation.
    # A checksum detects corruption, not internally contradictory provenance.
    payload = bytearray(index.read_bytes()[:-64])
    content_start = payload.index(b"sha256:") + len(b"sha256:")
    payload[content_start] = ord("0") if payload[content_start] != ord("0") else ord("1")
    bad.write_bytes(payload + hashlib.sha256(payload).hexdigest().encode())
    failure = run(invalid + ["--annotation-index", bad], succeeds=False)
    assert "source content identity mismatch" in failure.stderr
    # The two format enums immediately follow the length-framed semantics ID.
    payload = bytearray(index.read_bytes()[:-64])
    semantic_size = int.from_bytes(payload[12:16], "little")
    payload[16 + semantic_size] = 255
    bad.write_bytes(payload + hashlib.sha256(payload).hexdigest().encode())
    failure = run(invalid + ["--annotation-index", bad], succeeds=False)
    assert "unknown annotation format" in failure.stderr
    failure = run(common + ["--path-identity-ledger", ledger, "--annotation-index", index,
                            "--out-dir", root / "v3-with-ledger"], succeeds=False)
    assert "contains its ledger; omit --path-identity-ledger" in failure.stderr
    print("annotation-index raw/index counts, reads, logical hashes, MEX, RAD and counters match")


if __name__ == "__main__":
    main()
