#!/usr/bin/env python3
"""Compare scientific fixture products from two PANCOLLAPSE_TEST_GRAPH_MODE roots."""
import argparse
import hashlib
import json
from pathlib import Path
import pysam
from compare_graph_scientific import scientific_summary

parser = argparse.ArgumentParser()
parser.add_argument("left", type=Path)
parser.add_argument("right", type=Path)
parser.add_argument("--receipt", type=Path, required=True)
args = parser.parse_args()


def scientific_files(root):
    names = {"map.rad", "tx2gene.tsv", "summary.tsv", "evidence.tsv", "matrix.mtx.gz",
             "features.tsv.gz", "barcodes.tsv.gz"}
    return {str(p.relative_to(root)) for p in root.rglob("*")
            if p.is_file() and (p.name in names or p.suffix in {".parquet", ".bam"})}


def digest(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def bam_science(path):
    # CL is retained, truthful operational provenance. All other header fields,
    # records, their order and optional tags are compared exactly.
    with pysam.AlignmentFile(path, "rb", check_sq=False) as bam:
        header = bam.header.to_dict()
        for pg in header.get("PG", []):
            pg.pop("CL", None)
        records = [record.to_string() for record in bam.fetch(until_eof=True)]
    return header, records

left_files, right_files = scientific_files(args.left), scientific_files(args.right)
failures = []
if left_files != right_files:
    failures.append({"inventory_only_left": sorted(left_files - right_files),
                     "inventory_only_right": sorted(right_files - left_files)})
products = {}
for relative in sorted(left_files & right_files):
    left, right = args.left / relative, args.right / relative
    left_hash, right_hash = digest(left), digest(right)
    method = "bytes"
    equal = left_hash == right_hash
    if left.suffix == ".bam":
        method = "SAM header except @PG CL, plus exact ordered SAM records"
        equal = bam_science(left) == bam_science(right)
    elif left.name == "summary.tsv" and left.read_text().startswith("scope\tprofile_id\t"):
        method = "explicit native-count scientific counters"
        equal = scientific_summary(left)[0] == scientific_summary(right)[0]
    products[relative] = {"method": method, "equal": equal, "left_sha256": left_hash,
                          "right_sha256": right_hash}
    if not equal:
        failures.append(relative)
receipt = {"left": str(args.left), "right": str(args.right), "products": products,
           "failures": failures, "pass": not failures}
args.receipt.parent.mkdir(parents=True, exist_ok=True)
args.receipt.write_text(json.dumps(receipt, indent=2, sort_keys=True) + "\n")
print(json.dumps({"pass": not failures, "products": len(products), "failures": failures}))
raise SystemExit(bool(failures))
