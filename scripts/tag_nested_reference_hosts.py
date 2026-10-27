#!/usr/bin/env python3
"""Add pairwise nested-reference-host evidence to a corrected CAT annotation.

This is the stable, narrow replay surface for the nested-host pass used by the selected
annotation rules. It deliberately does not rerun the broader CAT correction pipeline: the input
annotation is already corrected, and only the reference-coordinate containment fact is added.

Each projected row whose version-stripped ``source_gene`` lies strictly inside a longer
same-strand reference gene receives both ``tag=nested_within_host_gene`` and
``nested_host_gene=<host>``. Naming the host is essential because the downstream policy is a
pairwise preference, never a global deletion of the nested gene.

This exact narrow pass is part of the best-performing baseline. Run it after
``apply_cat_denovo_corrections.py`` and before building the graph and Parent-category cache. Do not
replace it with the archived global nested-gene demotion/cache augmenters.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from apply_cat_denovo_corrections import VERSION_SUFFIX, nested_reference_genes


NESTED_WITHIN_HOST_TAG = "nested_within_host_gene"


def attribute_value(attributes: str, key: str) -> str | None:
    offset = attributes.find(key + "=")
    while offset != -1:
        if offset == 0 or attributes[offset - 1] == ";":
            end = attributes.find(";", offset)
            start = offset + len(key) + 1
            return attributes[start:] if end < 0 else attributes[start:end]
        offset = attributes.find(key + "=", offset + 1)
    return None


def add_nested_host_evidence(source: Path, reference: Path, output: Path) -> tuple[int, int, int]:
    nested = nested_reference_genes(str(reference))
    tagged = 0
    rows = 0
    with source.open() as source_handle, output.open("w") as output_handle:
        for line in source_handle:
            if line.startswith("#"):
                output_handle.write(line)
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9:
                output_handle.write(line)
                continue
            rows += 1
            source_gene = attribute_value(fields[8], "source_gene")
            host = nested.get(VERSION_SUFFIX.sub("", source_gene or "")) if source_gene else None
            if not host:
                output_handle.write(line)
                continue

            attributes = fields[8]
            tag_offset = attributes.find("tag=")
            if tag_offset != -1 and (tag_offset == 0 or attributes[tag_offset - 1] == ";"):
                tag_end = attributes.find(";", tag_offset)
                if tag_end < 0:
                    attributes += "%2C" + NESTED_WITHIN_HOST_TAG
                else:
                    attributes = (
                        attributes[:tag_end]
                        + "%2C"
                        + NESTED_WITHIN_HOST_TAG
                        + attributes[tag_end:]
                    )
            else:
                attributes += ";tag=" + NESTED_WITHIN_HOST_TAG
            fields[8] = attributes + f";nested_host_gene={host}"
            tagged += 1
            output_handle.write("\t".join(fields) + "\n")
    return len(nested), tagged, rows


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("corrected_annotation", type=Path)
    parser.add_argument("reference_annotation", type=Path)
    parser.add_argument("output_annotation", type=Path)
    args = parser.parse_args()
    for label, path in (
        ("corrected annotation", args.corrected_annotation),
        ("reference annotation", args.reference_annotation),
    ):
        if not path.is_file():
            raise ValueError(f"{label} is not a file: {path}")
    if args.output_annotation.exists():
        raise ValueError(f"refusing existing output path: {args.output_annotation}")

    nested, tagged, rows = add_nested_host_evidence(
        args.corrected_annotation, args.reference_annotation, args.output_annotation
    )
    print(f"nested reference genes mapped to a host: {nested:,}")
    print(f"{tagged:,} of {rows:,} rows carry the tag and a named host")


if __name__ == "__main__":
    main()
