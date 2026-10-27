#!/usr/bin/env python3
"""Audit readthrough tagging in a corrected annotation against what the counter assumes.

`panCollapse count --annotation` reads two readthrough signals with deliberately different
quantifiers. `tag=readthrough_gene`, taken from a gene row or from the reference gene reached
through `source_gene`, excludes a locus on its own. `tag=readthrough_transcript`, taken from a
transcript row, excludes a locus only when *every* observation carries it, because
`filter_categories` already drops individual readthrough candidates per read and the gene-level
rule exists only for the case where nothing legitimate remains. An observation is a Parent: one
`unique_parent` row in the path-identity ledger, one haplotype copy.

A gene may therefore be legitimately partial -- some isoforms readthrough, some not -- and the
conjunction keeps it. What must NOT happen is a copy losing a tag its source asserts, because
that turns a wholly readthrough locus into a partial one and it silently escapes exclusion.
This audit checks the three propagation paths that could drop such a tag, and reports the
gene-level distribution for context. Exit status is non-zero only on a genuine violation.
"""
import argparse
import collections
import csv
import gzip
import re
import sys

ID_RE = re.compile(r"(?:^|;)ID=([^;\n]+)")
PARENT_RE = re.compile(r"(?:^|;)Parent=([^;\n]+)")
SRC_TX_RE = re.compile(r"(?:^|;)source_transcript=([^;\n]+)")
SRC_GENE_RE = re.compile(r"(?:^|;)source_gene=([^;\n]+)")
MISSING = {"", ".", "N/A", "NA", "nan", "None", "N%2FA"}


def opener(path):
    return gzip.open(path, "rt") if str(path).endswith(".gz") else open(path)


def unversioned(value):
    """Strip an Ensembl version suffix. Identifiers are matched on the stable part because a
    projected source_transcript carries a version the reference row need not agree on."""
    return value.split(".")[0] if value else value


def rows(path, wanted):
    with opener(path) as handle:
        for line in handle:
            if not line or line[0] == "#":
                continue
            columns = line.split("\t", 9)
            if len(columns) < 9 or columns[2] not in wanted:
                continue
            yield columns[2], columns[8]


def read_reference(path):
    readthrough_tx, readthrough_gene, every_tx = set(), set(), set()
    for feature, attributes in rows(path, {"transcript", "gene"}):
        found = ID_RE.search(attributes)
        if not found:
            continue
        key = unversioned(found.group(1))
        if feature == "gene":
            if "readthrough_gene" in attributes:
                readthrough_gene.add(key)
        else:
            every_tx.add(key)
            if "readthrough_transcript" in attributes:
                readthrough_tx.add(key)
    return readthrough_tx, readthrough_gene, every_tx


ENSEMBL_RE = re.compile(r"^(ENS[A-Z]*G\d+)(?:\.\d+)?(_\d+|__LOC.*)?$")


def count_gene(source_gene, gene_locus, gene_id):
    """The counter's normalized_locus(): source_gene if present, else gene_locus, with the
    Ensembl version stripped and the _N / __LOC discriminator preserved. Grouping must use the
    same key the counter groups by, or the distribution below describes a different partition
    than the one the conjunction is evaluated over."""
    for value in (source_gene, gene_locus):
        if value and value not in MISSING:
            found = ENSEMBL_RE.match(value)
            return (found.group(1) + (found.group(2) or "")) if found else value
    return "__panSC_runtime_gene__:" + (gene_id or "")


def read_ledger(path):
    layer, exon_partner, gene = {}, {}, {}
    with open(path) as handle:
        for row in csv.DictReader(handle, delimiter="\t"):
            parent = row["unique_parent"]
            layer[parent] = row.get("feature_layer", "")
            exon_partner[parent] = row.get("exon_unique_parent", "")
            gene[parent] = count_gene(row.get("source_gene"), row.get("gene_locus"),
                                      row.get("gene_id", ""))
    return layer, exon_partner, gene


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("annotation", help="corrected GFF3 (optionally gzipped)")
    parser.add_argument("--reference", help="reference GFF3 supplying authoritative tags")
    parser.add_argument("--path-identity-ledger",
                        help="ledger, enabling the body/exon and per-Parent checks")
    args = parser.parse_args()

    reference_tx = reference_gene = reference_all = set()
    if args.reference:
        reference_tx, reference_gene, reference_all = read_reference(args.reference)
        print(f"reference: {len(reference_tx)} readthrough transcripts, "
              f"{len(reference_gene)} readthrough genes, {len(reference_all)} transcripts")

    layer, exon_partner, ledger_gene = ({}, {}, {})
    if args.path_identity_ledger:
        layer, exon_partner, ledger_gene = read_ledger(args.path_identity_ledger)
        print(f"ledger: {len(layer)} Parents")

    declared_genes = set()
    for _, attributes in rows(args.annotation, {"gene"}):
        found = ID_RE.search(attributes)
        if found and "readthrough_gene" in attributes:
            declared_genes.add(found.group(1))

    tagged, gene_of, source_of = {}, {}, {}
    unresolved_source = 0
    for _, attributes in rows(args.annotation, {"transcript", "mRNA"}):
        found = ID_RE.search(attributes)
        if not found:
            continue
        identifier = found.group(1)
        tagged[identifier] = "readthrough_transcript" in attributes
        parent = PARENT_RE.search(attributes)
        gene_of[identifier] = parent.group(1) if parent else ""
        source = SRC_TX_RE.search(attributes)
        value = source.group(1) if source else ""
        source_of[identifier] = "" if value in MISSING else unversioned(value)
        if source_of[identifier] and reference_all and source_of[identifier] not in reference_all:
            unresolved_source += 1

    violations = 0

    # 1. A gene row declaring readthrough must have every child transcript tagged, or the
    #    conjunction sees a partial locus where the declaration already settled the question.
    missing = [t for t, g in gene_of.items() if g in declared_genes and not tagged[t]]
    children = sum(1 for g in gene_of.values() if g in declared_genes)
    print(f"\ngene to transcript   : {children - len(missing)}/{children} children of a "
          f"readthrough_gene locus are tagged")
    if missing:
        violations += len(missing)
        print(f"  VIOLATION: {len(missing)} untagged, e.g. {missing[:3]}")

    # 2. A copy must not lose a tag its source asserts. The reverse is allowed: a copy may be
    #    tagged where the reference is not, because a projection can read through in one
    #    assembly and not another, and that is a real per-haplotype structural call.
    if args.reference:
        dropped = [t for t, s in source_of.items()
                   if s and s in reference_tx and not tagged[t]]
        inherited = sum(1 for s in source_of.values() if s and s in reference_tx)
        print(f"reference to copy    : {inherited - len(dropped)}/{inherited} copies of a "
              f"readthrough source transcript are tagged")
        if unresolved_source:
            print(f"  note: {unresolved_source} rows name a source_transcript absent from the "
                  f"reference; no inheritance is possible for them")
        if dropped:
            violations += len(dropped)
            print(f"  VIOLATION: {len(dropped)} dropped, e.g. {dropped[:3]}")

    # 3. A body row and its exon counterpart describe one biological transcript, so they must
    #    agree; a body row that disagrees makes its gene partial for a purely structural reason.
    if layer:
        disagree = [p for p, l in layer.items()
                    if l == "body" and exon_partner.get(p) in tagged and p in tagged
                    and tagged[p] != tagged[exon_partner[p]]]
        bodies = sum(1 for l in layer.values() if l == "body")
        print(f"body to exon partner : {bodies - len(disagree)}/{bodies} body Parents agree "
              f"with their exon partner")
        if disagree:
            violations += len(disagree)
            print(f"  VIOLATION: {len(disagree)} disagree, e.g. {disagree[:3]}")

        # Context, not a violation: how the conjunction will see each count gene. A partial
        # locus is legitimate -- it means some isoforms read through and some do not, and the
        # conjunction keeps the gene while filter_categories drops the tagged candidates.
        per = collections.defaultdict(lambda: [0, 0])
        for parent, gene in ledger_gene.items():
            if parent not in tagged:
                continue
            per[gene][0] += 1
            per[gene][1] += tagged[parent]
        none = sum(1 for n, t in per.values() if t == 0)
        every = sum(1 for n, t in per.values() if n and t == n)
        partial = len(per) - none - every
        print(f"\ncount genes          : {len(per)} total, {none} with no Parent tagged, "
              f"{every} with every Parent tagged, {partial} partial")
        print(f"  the {every} fully tagged are what the conjunction excludes; the {partial} "
              f"partial keep their untagged isoforms and lose only the tagged candidates")

    print(f"\n{'FAIL' if violations else 'PASS'}: {violations} violations")
    return 1 if violations else 0


if __name__ == "__main__":
    sys.exit(main())
