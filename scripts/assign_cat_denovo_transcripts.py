#!/usr/bin/env python3
"""Attribute CAT de-novo transcripts to the locus their structure actually belongs to.

CAT files a de-novo prediction under whichever transMap gene it overlaps
(`AssignedGeneId`) and records nothing about where its exon structure came from. Where a
projection failed, a prediction reconstructing the missing gene is filed under a neighbour
it merely abuts, and every read at that locus becomes ambiguous between the two.

Two tests run in order, because they fail in opposite directions.

SPLICE CHAIN (specific, narrow). Internal exon lengths survive projection onto another
assembly even though coordinates do not, so an ordered run of them identifies the source
locus. Matched against an index of reference internal-exon-length k-mers. Decisive where it
applies, but a locus needs k+2 exons to have a signature at all, which excludes most lncRNA
and pseudogene loci, and 63% of reference genes have three exons or fewer.

LOCAL OVERLAP (general, weaker). For everything the chain test cannot reach, ask which
genes the transcript overlaps exonically among the ortholog projections on its own contig.
Ortholog paths are faithful projections, so they anchor the reference genes in this
haplotype's own coordinates and no cross-assembly comparison is needed.

The two are complementary rather than redundant: the chain test resolves the case where a
gene has no ortholog projection on this haplotype at all, which is exactly when overlap has
nothing to match against, while overlap covers the single- and few-exon transcripts that
have no splice signature.

Transcripts overlapping no annotated gene on their contig are reported as candidate novel
loci rather than forced onto a neighbour.

Report-only: this writes a TSV of proposed attributions and never edits an annotation or a
count matrix. `apply_cat_denovo_corrections.py` is what acts on the report.

READTHROUGH follows GENCODE's definition -- a transcript overlapping two or more
INDEPENDENT loci. Independence is load-bearing and is enforced by `loci_are_disjoint`: two
genes that genuinely overlap each other are one locus for this purpose, and a transcript
sitting inside their shared extent has bridged nothing. A call is sub-classified as
complete (`overlap_readthrough`) when transcription demonstrably began at one gene's TSS,
ran through an entire transcript of it, and reached a whole exon of another; and as partial
(`overlap_readthrough_partial`) when the disjointness criterion is met but that origin
story cannot be established.

Both tiers are strand-restricted throughout. Exon and intron lengths carry no orientation,
so an antisense gene annotated across the same locus can match by coincidence; and
readthrough transcription is same-strand by definition, since you cannot transcribe through
into a gene pointing the other way.

ONLY THE CHAIN TIER MAY PROPOSE A REASSIGNMENT. The overlap tier may propose a readthrough
but never a rename: sharing exonic bases with a gene is not evidence of being a copy of it,
and a gene found through its own ortholog projection is by definition not the missing gene a
de-novo model stood in for. An overlap-tier disagreement with CAT's label is therefore
reported as `overlap_label_conflict` and left alone.

INPUTS
    reference   The reference GFF3/GTF the CAT run was projected from, plain or .gz. Read
                for exon rows carrying transcript_id and gene_id; gene_id is stripped of
                any PanSN prefix and version suffix.
    cat         The CAT annotation to attribute, plain or .gz. Read for exon, transcript
                and mRNA rows carrying transcript_class, source_gene and source_transcript.
                Rows are streamed one contig at a time and must be grouped by contig, which
                CAT output is; memory stays flat in the size of one haplotype. Only exon
                rows contribute coordinates -- a transcript/mRNA row spans introns too, and
                admitting one as an exon would corrupt the chain and inflate every overlap.
    out_dir     Created if absent.

OUTPUTS
    out_dir/denovo_assignment.tsv.gz   one row per de-novo transcript, columns:
        contig, parent, transcript_class, assigned_gene, strand, verdict, resolved_by,
        proposed_gene, introns, exons, span, chain_verdict, evidence
      resolved_by is splice_chain or local_overlap. chain_verdict preserves the chain
      tier's own answer even when the overlap tier produced the final verdict, so a
      chain_untestable or chain_no_match fallback stays visible. evidence is the top five
      gene:vote or gene:shared_bases pairs.
    out_dir/SUMMARY.tsv   the parameters used, index sizes, and a count per verdict.

VERDICT VOCABULARY
    chain tier
      chain_consistent       chain matches the gene CAT filed it under
      chain_misassignment    chain belongs to exactly one other gene -- actionable rename
      chain_readthrough      chain hits two or more genes, or begins/ends in the assigned
                             gene while matching another -- actionable readthrough
      chain_untestable       fewer than --ngram introns, so no signature exists
      chain_no_match         has a signature, but it matches no reference gene
    overlap tier (reached only after chain_untestable or chain_no_match)
      overlap_consistent               best overlap is the gene CAT filed it under
      overlap_label_conflict           overlaps another gene more; reported, never acted on
      overlap_readthrough              spans disjoint loci, with a complete origin story
      overlap_readthrough_partial      spans disjoint loci, origin not established
      nested_within_gene_no_exonic_overlap
                                       inside a same-strand gene's extent, sharing no exon
      candidate_novel_locus            no same-strand exonic overlap; any antisense gene
                                       overlapping it is recorded as context only
      novel_locus_no_anchor            no ortholog projections at all on this contig

USAGE
    python3 assign_cat_denovo_transcripts.py reference.gff3.gz cat.gff3.gz out_dir/

KNOWN LIMITATIONS
  * Coverage of the chain tier is bounded by exon count. A locus needs --ngram + 1 introns
    to have a signature at all. At the default 3, 98.3% of signatures are unique to one
    gene but only 36.5% of genes are testable; --ngram 2 reaches 48.6% of genes at 96.5%
    uniqueness. 63% of reference genes have three exons or fewer, which excludes most
    lncRNA and pseudogene loci from the chain tier entirely.
  * The overlap tier can only see genes that have an ortholog projection on the same
    contig. Where a projection failed -- the case that motivates this whole script -- it
    has nothing to match against, which is why the chain tier exists.
  * `--min-identity` is accepted, recorded in SUMMARY.tsv, and passed to `overlap_verdict`,
    but no verdict currently depends on it. The helpers written for that gate,
    `structural_identity` and `projected_source_transcripts`, are likewise defined and
    reachable as functions but not called. They are retained because they encode the
    intended identity criterion; treat the option as inert until they are wired in.
  * Verdicts are proposals. Nothing here is validated against an orthogonal source of
    truth, and the readthrough calls in particular rest on the completeness of the ortholog
    projections present on the contig.
"""
import argparse
import bisect
import collections
import csv
import gzip
import os
import re
import sys

VERSION_SUFFIX = re.compile(r"\.\d+$")
DENOVO_CLASSES = ("putative_novel_isoform", "putative_novel", "possible_paralog",
                  "poor_alignment")


def open_text_auto(path):
    return gzip.open(path, "rt") if path.endswith(".gz") else open(path)


def canonical(gene):
    if not gene:
        return None
    return VERSION_SUFFIX.sub("", gene.split("#")[-1])


def attr(text, key):
    match = re.search(rf"(?:^|;){key}=([^;]*)", text)
    if not match:
        return None
    return match.group(1).strip() or None


def intron_lengths(exons):
    """Intron lengths in transcript order.

    Every intron is bounded by a splice donor and acceptor, so all n-1 of them survive
    projection onto another assembly. Internal exon lengths share that property but there are
    only n-2 of them, and human exons cluster tightly around 50-300 bp so one exon length is
    weakly diagnostic. Introns run from ~60 bp to hundreds of kb, so two of them already
    identify a single gene 96.5% of the time against 51.8% for two internal exons.
    """
    coords = sorted(exons)
    return [coords[i + 1][0] - coords[i][1] for i in range(len(coords) - 1)]


def terminal_exon_lengths(exons):
    """First and last exon lengths -- diagnostic of where a transcript begins and ends."""
    coords = sorted(exons)
    return {coords[0][1] - coords[0][0], coords[-1][1] - coords[-1][0]}


def ngrams(values, n):
    return [tuple(values[i:i + n]) for i in range(len(values) - n + 1)]


def build_reference_index(path, n):
    """Map internal-exon-length k-mers to genes, and record every gene's exon lengths.

    The per-gene exon lengths decide a case the chain alone cannot: a gene with fewer than
    n+2 exons contributes no k-mer, so a fusion out of a two-exon lncRNA collects votes only
    for its multi-exon partner and is indistinguishable from a transcript misfiled under the
    lncRNA. The terminal exons separate them.
    """
    exons = collections.defaultdict(list)
    tx_gene = {}
    gene_strand = {}
    with open_text_auto(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9 or fields[2] != "exon":
                continue
            transcript = attr(fields[8], "transcript_id")
            gene = canonical(attr(fields[8], "gene_id"))
            if transcript is None or gene is None:
                continue
            exons[transcript].append((int(fields[3]), int(fields[4])))
            tx_gene[transcript] = gene
            gene_strand[gene] = fields[6]
    index = collections.defaultdict(set)
    gene_exons = collections.defaultdict(set)
    for transcript, coords in exons.items():
        gene = tx_gene[transcript]
        chain = intron_lengths(coords)
        for gram in ngrams(chain, n):
            index[gram].add(gene)
        for start, end in coords:
            gene_exons[gene].add(end - start)
    return index, gene_exons, gene_strand, len(exons)


def stream_contigs(path, denovo_classes):
    """Yield one contig at a time: its de-novo transcripts and its ortholog exon anchors.

    CAT output is grouped by contig, so a block is complete when the contig changes and can
    be released before the next begins. Memory stays flat in the size of one haplotype.
    """
    contig = None
    denovo = collections.OrderedDict()
    orth = collections.defaultdict(list)
    orth_meta = set()
    with open_text_auto(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9 or fields[2] not in {"exon", "transcript", "mRNA"}:
                continue
            if fields[0] != contig:
                if contig is not None:
                    yield contig, denovo, orth, orth_meta
                contig, denovo, orth, orth_meta = fields[0], collections.OrderedDict(), \
                    collections.defaultdict(list), set()
            attrs = fields[8]
            cls = attr(attrs, "transcript_class")
            gene = canonical(attr(attrs, "source_gene"))
            strand = fields[6]
            # Only exon rows carry exonic coordinates. A transcript/mRNA row spans the whole
            # locus including introns, so admitting one as an exon corrupts the intron chain
            # and inflates every overlap measured against it. Those rows are still read, for
            # annotations that put transcript_class or source_gene only on the parent row.
            is_exon = fields[2] == "exon"
            if cls in denovo_classes:
                parent = attr(attrs, "Parent") or attr(attrs, "transcript_id")
                if parent is None:
                    continue
                entry = denovo.setdefault(parent, [gene, cls, [], strand])
                if entry[0] is None and gene is not None:
                    entry[0] = gene
                if is_exon:
                    entry[2].append((int(fields[3]), int(fields[4])))
            elif cls == "ortholog" and gene is not None and is_exon:
                # Keyed per ortholog transcript, not per gene: the readthrough test needs a
                # complete individual transcript to ask whether one was run through entirely.
                otx = attr(attrs, "Parent") or attr(attrs, "transcript_id")
                orth[(gene, strand, otx)].append((int(fields[3]), int(fields[4])))
                # Which reference transcript this projection came from, so the overlap tier
                # can ask whether the transcript a de-novo model reproduces is already here.
                src = canonical(attr(attrs, "source_transcript"))
                if src is not None:
                    orth_meta.add(((gene, strand), src))
    if contig is not None:
        yield contig, denovo, orth, orth_meta


def chain_verdict(chain, exons, assigned, strand, index, gene_exons, gene_strand, n,
                  min_support):
    """Attribute a splice chain by k-mer vote over internal exon lengths.

    Votes are restricted to genes on the transcript's own strand before anything is decided.
    Exon lengths carry no orientation, so an antisense partner annotated across the same
    locus can match by coincidence; and readthrough transcription is same-strand by
    definition, since you cannot transcribe through into a gene pointing the other way.
    """
    if len(chain) < n:
        return "chain_untestable", {}, None
    votes = collections.Counter()
    for gram in ngrams(chain, n):
        for gene in index.get(gram, ()):
            votes[gene] += 1
    votes = {g: c for g, c in votes.items()
             if c >= min_support and gene_strand.get(g, strand) == strand}
    if not votes:
        return "chain_no_match", {}, None
    best = max(votes, key=votes.get)
    if len(votes) > 1:
        return "chain_readthrough", votes, best
    if best == assigned:
        return "chain_consistent", votes, best
    terminals = terminal_exon_lengths(exons)
    if terminals & gene_exons.get(assigned, set()) - gene_exons.get(best, set()):
        # Begins or ends in the gene it is filed under: a fusion, not a misfiling.
        return "chain_readthrough", votes, best
    return "chain_misassignment", votes, best


def build_overlap_anchor(orth):
    """Flatten this contig's ortholog exons into one coordinate-sorted lookup.

    Strand travels with every exon. An antisense partner is annotated across the same locus
    as its sense gene, so ignoring strand silently proposes reassigning a transcript onto
    the gene transcribed the other way -- wrong for any stranded protocol.
    """
    merged = []
    by_gene = collections.defaultdict(list)
    for (g, st, _), coords in orth.items():
        by_gene[(g, st)].extend(coords)
    for (g, st), coords in by_gene.items():
        # Union each gene's exons first. A gene with 40 annotated isoforms otherwise
        # contributes 40 near-identical copies of every exon, and summing a hit against each
        # inflates its shared-base total in proportion to how well annotated it is.
        lo = hi = None
        for a, b in sorted(coords):
            if lo is None:
                lo, hi = a, b
            elif a <= hi:
                hi = max(hi, b)
            else:
                merged.append((lo, hi, g, st))
                lo, hi = a, b
        if lo is not None:
            merged.append((lo, hi, g, st))
    starts, ends, genes, strands, spans = [], [], [], [], {}
    for s, e, g, st in sorted(merged):
        starts.append(s)
        ends.append(e)
        genes.append(g)
        strands.append(st)
        lo, hi = spans.get((g, st), (s, e))
        spans[(g, st)] = (min(lo, s), max(hi, e))
    widest = max((e - s for s, e in zip(starts, ends)), default=0)
    return starts, ends, genes, strands, spans, widest


def loci_are_disjoint(orth, strand, genes):
    """Do these genes occupy separate extents on this contig, sharing no exonic base?

    The guard that keeps the readthrough test honest. GENCODE defines a readthrough
    transcript as one overlapping "two or more independent loci", and independence is the
    load-bearing word: a transcript sitting inside a region where two genes genuinely
    overlap has not bridged anything, while one spanning the gap between separate loci has.
    """
    exons_by_gene = collections.defaultdict(list)
    for (g, st, _), coords in orth.items():
        if g in genes and st == strand:
            exons_by_gene[g].extend(coords)
    named = list(exons_by_gene)
    if len(named) < 2:
        return False
    for i, a in enumerate(named):
        for b in named[i + 1:]:
            if any(s1 < e2 and s2 < e1
                   for s1, e1 in exons_by_gene[a] for s2, e2 in exons_by_gene[b]):
                return False
    return True


def runs_through(exons, introns, orth, strand, gene, tss_slack):
    """Did transcription start at this gene's TSS and run through one of its transcripts?

    Two conditions, both structural. The 5' end must sit in the first exon of one of the
    gene's transcripts (a long-read TSS call may extend modestly upstream, so a little slack
    is allowed). And that transcript's entire intron chain must appear as a contiguous run
    inside this transcript's own chain -- the exact meaning of running through all of it.
    A single-exon transcript has no chain, so containment of its exon stands in.
    """
    lo = min(s for s, _ in exons)
    hi = max(e for _, e in exons)
    for (g, st, _), coords in orth.items():
        if g != gene or st != strand:
            continue
        coords = sorted(coords)
        if len(coords) == 1:
            if coords[0][0] >= lo and coords[0][1] <= hi:
                return True
            continue
        first = coords[0] if strand == "+" else coords[-1]
        if strand == "+":
            started = first[0] - tss_slack <= lo <= first[1]
        else:
            started = first[0] <= hi <= first[1] + tss_slack
        if not started:
            continue
        chain = [coords[i + 1][0] - coords[i][1] for i in range(len(coords) - 1)]
        n = len(chain)
        if any(introns[i:i + n] == chain for i in range(len(introns) - n + 1)):
            return True
    return False


def covers_whole_exon(exons, orth, strand, gene):
    """Is at least one complete exon of this gene contained in one of ours?"""
    for (g, st, _), coords in orth.items():
        if g != gene or st != strand:
            continue
        for a, b in coords:
            if any(s <= a and b <= e for s, e in exons):
                return True
    return False


def overlap_verdict(exons, introns, assigned, strand, anchor, orth, min_bases, tss_slack):
    """Attribute by same-strand exonic overlap with ortholog projections on this contig."""
    starts, ends, genes, strands, spans, widest = anchor
    if not starts:
        return "novel_locus_no_anchor", {}, None
    exonic = collections.Counter()
    for s, e in exons:
        # Scan back to the first interval that could still reach this exon, rather than a
        # fixed window: after merging, one interval can be arbitrarily long.
        lo_i = bisect.bisect_left(starts, s - widest)
        hi_i = bisect.bisect_right(starts, e)
        for j in range(lo_i, hi_i):
            if strands[j] != strand:
                continue
            shared = min(e, ends[j]) - max(s, starts[j])
            if shared > 0:
                exonic[genes[j]] += shared
    exonic = {g: b for g, b in exonic.items() if b >= min_bases}
    if len(exonic) > 1 and loci_are_disjoint(orth, strand, set(exonic)):
        # One transcript carrying exons from two loci that share no exonic base has bridged
        # the gap between them -- GENCODE's own criterion, "overlaps two or more independent
        # loci". Whether it also began at a promoter and ran through an entire transcript
        # only decides how confidently we can call it complete; either way every read landing
        # on it is compatible with both genes at once.
        origin = [g for g in exonic
                  if runs_through(exons, introns, orth, strand, g, tss_slack)]
        for g in origin:
            if any(o != g and covers_whole_exon(exons, orth, strand, o) for o in exonic):
                return "overlap_readthrough", exonic, g
        return "overlap_readthrough_partial", exonic, max(exonic, key=exonic.get)
    if exonic:
        best = max(exonic, key=exonic.get)
        if best == assigned:
            return "overlap_consistent", exonic, best
        # Overlap never licenses a reassignment. Sharing exonic bases with a gene is not
        # evidence of being a copy of it, and the two tests that could supply that evidence
        # both fail here: identity normalised against the reference transcript lets a long
        # model "fully reproduce" a short isoform on a few hundred bases, and a gene located
        # through its own projection is by definition not the missing gene a de-novo model
        # stood in for. Reassignment needs the splice chain, so this is reported, not acted
        # on -- the transcript overlaps a gene other than its label, and that is all we know.
        return "overlap_label_conflict", exonic, best
    lo = min(s for s, _ in exons)
    hi = max(e for _, e in exons)
    # Nesting is a same-strand relationship. On a stranded protocol a transcript inside the
    # intron of a gene on the other strand shares no reads with it, and calling that "nested"
    # would hide the fact that its own strand is unannotated here -- a novel-locus candidate.
    nested = {g for (g, st), (gs, ge) in spans.items()
              if st == strand and gs < hi and lo < ge}
    if nested:
        return "nested_within_gene_no_exonic_overlap", {g: 0 for g in nested}, None
    antisense = {g for (g, st), (gs, ge) in spans.items()
                 if st != strand and gs < hi and lo < ge}
    # Retained as context, not as an assignment: the locus is novel on its own strand.
    return "candidate_novel_locus", {f"antisense:{g}": 0 for g in antisense}, None


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("reference", help="reference GFF3/GTF the CAT run was projected from")
    ap.add_argument("cat", help="CAT annotation to attribute")
    ap.add_argument("out_dir")
    ap.add_argument("--ngram", type=int, default=3,
                    help="consecutive introns per splice signature (default 3; 98.3%% of "
                         "signatures are unique to one gene, 36.5%% of genes testable. "
                         "2 covers 48.6%% of genes at 96.5%% unique)")
    ap.add_argument("--min-support", type=int, default=1,
                    help="k-mers a gene must win before its chain match counts")
    ap.add_argument("--min-overlap-bases", type=int, default=1,
                    help="exonic bases shared before an overlap match counts (default 1)")
    ap.add_argument("--tss-slack", type=int, default=200,
                    help="bp a long-read TSS call may extend upstream of the annotated "
                         "first exon and still count as starting there (default 200)")
    ap.add_argument("--transcript-class", action="append", default=None,
                    help="CAT transcript_class to attribute; repeatable "
                         f"(default {', '.join(DENOVO_CLASSES)})")
    args = ap.parse_args()

    classes = set(args.transcript_class or DENOVO_CLASSES)
    os.makedirs(args.out_dir, exist_ok=True)

    index, gene_exons, reference_strand, n_ref = build_reference_index(
        args.reference, args.ngram)
    print(f"indexed {n_ref:,} reference transcripts into {len(index):,} "
          f"{args.ngram}-intron splice signatures", file=sys.stderr)

    verdicts = collections.Counter()
    resolved_by = collections.Counter()
    rows = 0
    report = os.path.join(args.out_dir, "denovo_assignment.tsv.gz")
    with gzip.open(report, "wt", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow((
            "contig", "parent", "transcript_class", "assigned_gene", "strand", "verdict",
            "resolved_by", "proposed_gene", "introns", "exons", "span",
            "chain_verdict", "evidence",
        ))
        for contig, denovo, orth, orth_meta in stream_contigs(args.cat, classes):
            anchor = None
            for parent, (assigned, cls, exons, strand) in denovo.items():
                if not exons:
                    continue
                exons.sort()
                chain = intron_lengths(exons)
                cv, votes, best = chain_verdict(chain, exons, assigned, strand, index,
                                                gene_exons, reference_strand, args.ngram,
                                                args.min_support)
                if cv in {"chain_consistent", "chain_misassignment", "chain_readthrough"}:
                    verdict, proposed, evidence, how = cv, best, votes, "splice_chain"
                else:
                    if anchor is None:
                        anchor = build_overlap_anchor(orth)
                    verdict, evidence, proposed = overlap_verdict(
                        exons, intron_lengths(exons), assigned, strand, anchor, orth,
                        args.min_overlap_bases, args.tss_slack)
                    how = "local_overlap"
                verdicts[verdict] += 1
                resolved_by[how] += 1
                rows += 1
                writer.writerow((
                    contig, parent, cls, assigned or "N/A", strand, verdict, how,
                    proposed or "N/A", len(chain), len(exons),
                    exons[-1][1] - exons[0][0], cv,
                    ";".join(f"{g}:{v}" for g, v in
                             sorted(evidence.items(), key=lambda kv: -kv[1])[:5]),
                ))

    with open(os.path.join(args.out_dir, "SUMMARY.tsv"), "w", newline="") as handle:
        writer = csv.writer(handle, delimiter="\t", lineterminator="\n")
        writer.writerow(("metric", "value"))
        for key, value in (("ngram", args.ngram), ("min_support", args.min_support),
                           ("min_overlap_bases", args.min_overlap_bases),
                           ("tss_slack", args.tss_slack),
                           ("reference_transcripts", n_ref), ("signatures", len(index)),
                           ("denovo_transcripts", rows)):
            writer.writerow((key, value))
        for name, count in resolved_by.most_common():
            writer.writerow((f"resolved_by_{name}", count))
        for verdict, count in verdicts.most_common():
            writer.writerow((verdict, count))

    print(f"attributed {rows:,} de-novo transcripts", file=sys.stderr)
    for name, count in resolved_by.most_common():
        print(f"  resolved by {name}: {count:,}", file=sys.stderr)
    for verdict, count in verdicts.most_common():
        print(f"  {count:>9,}  {100.0 * count / rows if rows else 0:6.2f}%  {verdict}",
              file=sys.stderr)


if __name__ == "__main__":
    main()
