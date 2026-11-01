#!/usr/bin/env python3
"""Small native RI CLI contract: binding rejection and count-table parity."""
import hashlib
import json
import csv
import gzip
from pathlib import Path
import subprocess
import sys
import tempfile

binary, vg, root_arg, fixture_arg, ex50_arg, bundle_arg = sys.argv[1:]
root_parent, fixture, ex50, bundle = map(Path, (root_arg, fixture_arg, ex50_arg, bundle_arg))
root_parent.mkdir(parents=True, exist_ok=True)
root = Path(tempfile.mkdtemp(prefix="r-index-", dir=root_parent))
pg, xg, gbwt, ri = (root / name for name in ("graph.pg", "graph.xg", "paths.gbwt", "paths.ri"))
with xg.open("wb") as out:
    subprocess.run([vg, "convert", "-g", str(fixture / "graph.gfa"), "-x"], stdout=out, check=True)
with pg.open("wb") as out:
    subprocess.run([vg, "convert", "-g", str(fixture / "graph.gfa"), "-p"], stdout=out, check=True)
subprocess.run([vg, "gbwt", "--num-threads", "2", "--num-jobs", "2", "-E", "-x", str(pg), "-o", str(gbwt)], check=True)
subprocess.run([vg, "gbwt", "-r", str(ri), str(gbwt)], check=True)

# Build a normal CLI V3 reference from a private copy, then make that copy and
# both source annotations unavailable before the embedded-reference count runs.
ledger = root / "ledger.tsv"
ledger.write_bytes((fixture / "path_identity_ledger.tsv").read_bytes())
with ledger.open() as handle:
    ledger_rows = list(csv.DictReader(handle, delimiter="\t"))
annotation = "##gff-version 3\n"
for gene in sorted({row["gene_id"] for row in ledger_rows}):
    annotation += f"chr1\tt\tgene\t1\t100\t.\t+\t.\tID={gene};gene_type=protein_coding\n"
for row in ledger_rows:
    annotation += ("chr1\tt\ttranscript\t1\t100\t.\t+\t.\t"
                   f"ID={row['unique_parent']};Parent={row['gene_id']};"
                   f"source_gene={row['source_gene']};gene_type=protein_coding;"
                   "transcript_type=protein_coding\n")
annotation_path, reference_path = root / "annotation.gff3.gz", root / "reference.gff3.gz"
annotation_path.write_bytes(gzip.compress(annotation.encode(), mtime=0))
reference_path.write_bytes(gzip.compress(b"##gff-version 3\n", mtime=0))
prepared_index = root / "prepared.pci"
subprocess.run([binary, "index", "--annotation", str(annotation_path), "--annotation-reference",
                str(reference_path), "--path-identity-ledger", str(ledger), "--out",
                str(prepared_index)], check=True)
annotation_baseline = root / "annotation-constructed"
subprocess.run([binary, "count", "--gamp", str(ex50 / "reads.gamp"), "--xg", str(xg),
                "--gbwt", str(gbwt), "--annotation", str(annotation_path), "--annotation-reference",
                str(reference_path), "--path-identity-ledger", str(ledger), "--barcode-whitelist",
                str(fixture / "count_whitelist.txt"), "--t2g", str(fixture / "count_t2g.tsv"),
                "--body-t2g", str(fixture / "count_body_t2g.tsv"), "--cr7", "--pansc-strict-v1",
                "--threads", "1", "--out-dir", str(annotation_baseline)], check=True)
embedded_ledger_path = str(ledger)
embedded_ledger_sha256 = hashlib.file_digest(ledger.open("rb"), "sha256").hexdigest()
embedded_ledger_size = ledger.stat().st_size
annotation_path.unlink(); reference_path.unlink(); ledger.unlink()
assert not ledger.exists()

def digest(path):
    return hashlib.file_digest(path.open("rb"), "sha256").hexdigest()

def binding(g=gbwt, r=ri):
    return {"schema": "pancollapse-r-index-binding-v1", "gbwt_sha256": digest(g),
            "gbwt_size_bytes": g.stat().st_size, "r_index_sha256": digest(r),
            "r_index_size_bytes": r.stat().st_size}

manifest = root / "binding.json"
manifest.write_text(json.dumps(binding()))
common = [binary, "count", "--gamp", str(ex50 / "reads.gamp"), "--xg", str(xg), "--gbwt", str(gbwt),
          "--count-bundle", str(bundle), "--barcode-whitelist", str(fixture / "count_whitelist.txt"),
          "--t2g", str(fixture / "count_t2g.tsv"), "--body-t2g", str(fixture / "count_body_t2g.tsv"),
          "--cr7", "--pansc-strict-v1"]

def run(name, extra, threads=1):
    out = root / name
    subprocess.run(common + extra + ["--threads", str(threads), "--out-dir", str(out)], check=True)
    return out

def run_prepared(name, threads):
    out = root / name
    command = [binary, "count", "--gamp", str(ex50 / "reads.gamp"), "--xg", str(xg),
               "--gbwt", str(gbwt), "--r-index", str(ri), "--r-index-manifest", str(manifest),
               "--trust-r-index-manifest", "--annotation-index", str(prepared_index),
               "--barcode-whitelist", str(fixture / "count_whitelist.txt"),
               "--t2g", str(fixture / "count_t2g.tsv"), "--body-t2g",
               str(fixture / "count_body_t2g.tsv"), "--cr7", "--pansc-strict-v1",
               "--threads", str(threads), "--out-dir", str(out)]
    return out, subprocess.run(command, text=True, capture_output=True, check=True)

def scientific_counters(output):
    with (output / "summary.tsv").open() as handle:
        rows = csv.reader(handle, delimiter="\t")
        next(rows)
        return {tuple(row[:3]): row[3] for row in rows
                if not row[2].startswith(("assignment_cache_", "aggregate_spill_"))}

def reject(name, payload, expected):
    path = root / (name + ".json")
    path.write_text(payload)
    result = subprocess.run(common + ["--r-index", str(ri), "--r-index-manifest", str(path),
                                     "--out-dir", str(root / (name + "-out"))], text=True, capture_output=True)
    if result.returncode == 0 or expected not in result.stderr:
        raise AssertionError(f"{name}: {result.returncode}: {result.stderr}")

valid = binding()
reject("missing", json.dumps({k: v for k, v in valid.items() if k != "r_index_sha256"}), "missing, duplicate")
reject("duplicate", '{"schema":"pancollapse-r-index-binding-v1","schema":"pancollapse-r-index-binding-v1","gbwt_sha256":"' + valid["gbwt_sha256"] + '","gbwt_size_bytes":1,"r_index_sha256":"' + valid["r_index_sha256"] + '","r_index_size_bytes":1}', "missing, duplicate")
unknown = dict(valid); unknown["unexpected"] = 1
reject("unknown", json.dumps(unknown), "missing, duplicate")
bad_gbwt = dict(valid); bad_gbwt["gbwt_sha256"] = "0" * 64
reject("bad-gbwt", json.dumps(bad_gbwt), "GBWT does not match")
bad_ri = dict(valid); bad_ri["r_index_sha256"] = "0" * 64
reject("bad-ri", json.dumps(bad_ri), "r-index does not match")
for name, contents in (("truncated", ri.read_bytes()[:16]), ("header", b"not a FastLocate index")):
    bad = root / (name + ".ri"); bad.write_bytes(contents)
    sidecar = root / (name + ".json"); sidecar.write_text(json.dumps(binding(r=bad)))
    result = subprocess.run(common + ["--r-index", str(bad), "--r-index-manifest", str(sidecar),
                                     "--out-dir", str(root / (name + "-out"))], text=True, capture_output=True)
    if result.returncode == 0:
        raise AssertionError(f"{name} RI was accepted")

baseline = run("constructed", [])
strict1 = run("strict1", ["--r-index", str(ri), "--r-index-manifest", str(manifest)], 1)
strict4 = run("strict4", ["--r-index", str(ri), "--r-index-manifest", str(manifest)], 4)
trusted = run("trusted", ["--r-index", str(ri), "--r-index-manifest", str(manifest), "--trust-r-index-manifest"], 4)
prepared1, prepared1_result = run_prepared("prepared1", 1)
prepared4, prepared4_result = run_prepared("prepared4", 4)
assert "phase=reference_load_serial branches=3" in prepared1_result.stderr
assert "phase=reference_load_parallel branches=3" in prepared4_result.stderr
for result in (prepared1_result, prepared4_result):
    assert "phase=prepared_annotation_index_begin" in result.stderr
    assert "phase=xg_deserialize_begin" in result.stderr
    assert "phase=gbwt_deserialize_begin" in result.stderr
    assert "phase=identity_ledger_begin" not in result.stderr
    assert "reference_released_before_umi=1" in result.stderr
def assert_outputs(reference, candidates):
    for table in sorted(p.relative_to(reference) for p in reference.rglob("*.parquet")):
        expected = (reference / table).read_bytes()
        for out in candidates:
            if (out / table).read_bytes() != expected:
                raise AssertionError(f"Parquet table differs: {table} in {out}")
    reference_manifest = json.loads((reference / "manifest.json").read_text())
    reference_logical = {profile["id"]: profile["logical_tables"]
                         for profile in reference_manifest["profiles"]}
    for out in candidates:
        actual_manifest = json.loads((out / "manifest.json").read_text())
        actual_logical = {profile["id"]: profile["logical_tables"]
                          for profile in actual_manifest["profiles"]}
        if actual_logical != reference_logical:
            raise AssertionError(f"profile logical tables differ in {out}")
        if scientific_counters(out) != scientific_counters(reference):
            raise AssertionError(f"scientific summary counters differ in {out}")

assert_outputs(baseline, (strict1, strict4, trusted))
assert_outputs(annotation_baseline, (prepared1, prepared4))
for output in (prepared1, prepared4):
    embedded_identity = json.loads((output / "manifest.json").read_text())["inputs"]["path_identity_ledger"]
    if (embedded_identity["path"] != embedded_ledger_path or
            embedded_identity["sha256"] != embedded_ledger_sha256 or
            embedded_identity["size_bytes"] != embedded_ledger_size or
            embedded_identity.get("identity_source") != "embedded-annotation-index" or
            embedded_identity.get("sha256_verified") is not False):
        raise AssertionError(f"embedded ledger provenance differs in {output}")
trusted_manifest = json.loads((trusted / "manifest.json").read_text())
ri_identity = trusted_manifest["inputs"]["r_index"]
if ri_identity.get("identity_source") != "trusted-preparation-receipt" or ri_identity.get("sha256_verified") is not False:
    raise AssertionError("trusted RI provenance marker missing")
# Embedded-reference failure wins over an unavailable graph and joins all loaders.
bad_reference = root / "bad-prepared.pci"
# Corrupt only the trailer: truncation may correctly fail a bounds check earlier.
bad_reference.write_bytes(prepared_index.read_bytes()[:-1] + b"X")
failure = subprocess.run([binary, "count", "--gamp", str(ex50 / "reads.gamp"), "--xg", str(root / "missing.xg"),
                          "--gbwt", str(gbwt), "--r-index", str(ri), "--r-index-manifest", str(manifest),
                          "--trust-r-index-manifest", "--annotation-index", str(bad_reference),
                          "--barcode-whitelist", str(fixture / "count_whitelist.txt"), "--cr7",
                          "--out-dir", str(root / "bad-reference-out")], text=True, capture_output=True)
if failure.returncode == 0 or "checksum mismatch" not in failure.stderr or "cannot open xg" in failure.stderr:
    raise AssertionError(f"embedded-reference failure priority changed: {failure.stderr}")
print("r-index CLI binding, parity, and trusted provenance passed")
