#!/usr/bin/env python3
"""Rewrite a CAT annotation with de-novo transcript attributions applied.

Consumes the ``panSC-cat-parent-detailed-v4`` ``detailed_assignment.tsv.gz`` from the current
``assign_cat_denovo_transcripts_detailed.py`` attribution pass and emits a corrected GFF3.
Two edits are made, and only to de-novo transcripts the report gave an actionable verdict.

MISASSIGNMENT -- the transcript's splice structure belongs to a gene other than the one CAT
filed it under, and none of it belongs to that gene. `source_gene` is repointed at the gene
the structure came from, and the biotype and name fields follow it. `source_gene` is the
field a collapse-mode counter keys on, so this is the edit that changes counting.

READTHROUGH -- the transcript carries exons from two loci. It is tagged
`tag=readthrough_transcript`, the GENCODE vocabulary that CellRanger's reference build and
`count_cr.py` already filter on, so no downstream tool needs to learn a new attribute.

Structural attributes are deliberately left alone. `gene_id` and `Parent` define the GFF3
feature hierarchy: repointing `gene_id` at a reference gene with no matching `gene` feature
in this file would emit a structurally invalid annotation, and CAT gene IDs are per-assembly
local identifiers rather than reference ones. The active corrector therefore has no option to
rewrite `gene_id`, `Parent`, coordinates, strand, or exon membership.

INHERITED READTHROUGH -- separately from the judged transcripts, every row whose
`source_transcript` or `source_gene` names something the reference itself tags
`readthrough_transcript` / `readthrough_gene` inherits that tag, which CAT drops on
projection. Without this the corrected annotation would be half-tagged: carrying the
vocabulary on de-novo transcripts we judged and nowhere else. A consumer folding a per-gene
conjunction would then see the untagged ortholog projections of a readthrough locus vote
against it and conclude the locus is ordinary -- worse than carrying no tag at all.

STRONG LOCAL SUPPORT -- a retained, countable, structurally single-gene Parent whose selected
exact/query-subchain/gene-junction identity names its assigned count gene receives
`tag=independent_single_gene_support_strong_local_structure`. This positive, truth-blind fact is
keyed by annotation contig and Parent. It never removes a provenance warning and carries no
count-time host policy.

Every edited row records what it was, so the change is auditable and reversible:
`cat_corrected_from`, `cat_correction_method`, `cat_correction_support`.

WHICH DETAILED VERDICTS ARE ACTIONABLE
    single_gene_reassignment  + current_action=reassign_countable
        -> reassignment (repoint source_gene)
    structural_readthrough    + current_action=tag_readthrough_alignment_only
        -> tag readthrough_transcript
Every other verdict is tagged but not rewritten. In particular,
``identity_candidate_reassignment`` is explicitly weak evidence with
``current_action=alignment_only``; it must never authorize a reassignment. The superseded report
vocabulary and compatible parser are archived under
``archive/f1_evaluation/code/legacy_annotation_preparation``.

INPUTS
    annotation  CAT GFF3 to correct, plain or .gz.
    report      detailed_assignment.tsv.gz from assign_cat_denovo_transcripts_detailed.py.
    reference   reference GFF3, read for gene_name / gene_type and for the reference's own
                readthrough tag vocabulary.
    out_gff     corrected GFF3; a .gz suffix compresses. Parent directory is created.

OPTIONS
    --path-identity-ledger, --path-identity-ledger-out
                                also repoint the ledger's source_gene for reassigned
                                transcripts. Both are required together; supplying only one
                                silently does nothing. A collapse-mode counter takes its
                                count gene from the ledger rather than from the annotation,
                                so without this the reassignments change nothing downstream.
    --skip-readthrough          apply reassignments only, leaving readthrough transcripts
                                untagged. Inherited reference tags are still applied.

OUTPUTS
    out_gff                     the corrected annotation.
    <ledger-out>.corrections.tsv    when a ledger is repointed: unique_parent,
                                source_gene_was, source_gene_now, method, support.
    stderr                      per-category row counts: reassigned, tagged_readthrough,
                                inherited_readthrough_tag, unchanged.

KNOWN LIMITATIONS
  * Attributes are edited textually, one line at a time. The file is never parsed into a
    feature tree, so nothing here validates that the result is still structurally coherent
    beyond the deliberate decision to leave gene_id and Parent alone.
  * A transcript row is keyed by its own ID and every other row by its Parent. An
    annotation that identifies rows differently will simply match nothing and pass through
    unchanged rather than fail loudly.
  * The path-identity ledger rewrite assumes the fixed 24-column schema; any row with a
    different column count is copied through verbatim, as is the header.
  * Gene names and biotypes come from the first reference row seen for a gene id. A
    reference disagreeing with itself across rows resolves to whichever came first.
  * Nothing verifies that the report was produced from this same annotation. Applying a
    report from a different CAT run will quietly correct nothing.

SELECTED BASELINE SETTINGS
--------------------------
Use the defaults: emit parent features and retain readthrough tagging. The selected baseline then
applies ``tag_nested_reference_hosts.py`` so every nested
projection carries both ``tag=nested_within_host_gene`` and the specific ``nested_host_gene``.
Filtering remains a count-time choice: the winning count preset uses eleven explicit categories
and deliberately omits ``alignment_only`` and ``multigene_unresolved``.
"""
import argparse
import collections
import shutil
import csv
import gzip
import os
import re
import sys

# A rewrite needs both a structural verdict and the detailed classifier's explicit action. This
# prevents a weak candidate direction from being promoted merely because it names a proposed gene.
ACTIONABLE = {
    ("single_gene_reassignment", "reassign_countable"): "misassignment",
    ("structural_readthrough", "tag_readthrough_alignment_only"): "readthrough",
}

# Detailed verdicts are projected into the annotation as tags as well as into the actionable
# rewrite table below.
#
# ACTIONABLE is only about REWRITING a row (repointing a gene, or asserting GENCODE's own
# readthrough vocabulary). It is NOT the filter surface. Every classifier verdict is written to
# the annotation as a tag regardless, because forcing a verdict into GENCODE's vocabulary loses
# the ones it cannot express: `multigene_unresolved` spans several genes without the evidence to
# name which loci it fuses, and `candidate_novel_locus` has no reference locus to read THROUGH at
# all. Those are 82,418 of chr20's 84,543 alignment-only transcripts against 440 clean
# readthroughs, so a vocabulary-only route would carry 0.5% of the signal.
# Tagging all ten categories instead means the counter needs no new code: --demote-transcript-tags
# already takes an arbitrary token set, so which categories are disqualifying becomes a stated
# per-run choice recorded in the run's own summary, rather than a rule baked into an artifact.
DETAILED_REQUIRED_COLUMNS = frozenset({
    "schema_version",
    "contig",
    "parent",
    "assigned_gene",
    "identity_basis",
    "identity_gene",
    "local_independent_component_pairs",
    "structural_class",
    "count_gene",
    "current_category",
    "current_proposed_gene",
    "current_action",
    "category_reason",
    "footprint_evidence",
    "count_role",
})
DETAILED_SCHEMA_VERSION = "panSC-cat-parent-detailed-v4"
# WHETHER A MODEL'S GENE ASSIGNMENT IS BACKED BY ITS OWN PROVENANCE, carried as a tag because it
# is a different question from the structural verdict and a model can pass one while failing the
# other. `source_gene` reads as provenance -- the gene inherited from the transcript this model was
# projected from -- but it is only that when such a transcript exists and agrees. The classifier
# computes the comparison for every Parent and records it as descriptive evidence; naming it here
# is what lets a run act on it, without the counter learning a new attribute.
#
# The empty string means the relation is consistent and no tag is written: a tag on 186,553 of
# 200,489 countable Parents would be noise, and the filter surface only needs the negative cases.
SOURCE_RELATION_TAGS = {
    "source_transcript_not_in_reference": "no_source_provenance",
    "not_available": "no_source_provenance",
    "label_consistent": "",
}
LABEL_CONFLICT_RELATION = "label_conflict_descriptive_only"
LABEL_CONFLICT_TAG = "source_gene_label_conflict"
NO_PROVENANCE_TAG = "no_source_provenance"
STRONG_LOCAL_SUPPORT_TAG = "independent_single_gene_support_strong_local_structure"
STRONG_LOCAL_IDENTITY_BASES = frozenset(
    ("local_exact_chain", "local_query_subchain", "local_gene_junction_set")
)
SUPPORT_MISSING = frozenset(("", ".", "NA", "N/A", "None", "nan"))
ENSEMBL_LOCUS = re.compile(
    r"^(ENSG\d+)(?:\.\d+)?(?P<discriminator>_\d+|__LOC\d+)?$"
)
VERSION_SUFFIX = re.compile(r"\.\d+$")
# The classifier's raw relation cannot be used for this tag, because most of what it calls a
# conflict is a PARALOG PROJECTION, which is correct behaviour rather than a defect. When CAT finds
# a paralog of a gene it names the new locus `<parent>.<version>_<N>` -- `ENSG00000310527.1_1` is a
# real gene, distinct from `ENSG00000310527` -- and projects the parent's transcripts onto it. Such
# a model legitimately carries the PARENT's source_transcript, because that is where its sequence
# came from, while its source_gene names the new copy. The two fields therefore disagree by design,
# and a rule that read that disagreement as mislabelling would demote every discovered paralog.
# Measured on chr20: 15,238 of 15,365 reported conflicts are that paralog relationship and only 127
# are disagreements between unrelated genes. Tagging on the raw relation would demote 6,892
# countable Parents for doing the right thing and 114 for the real error, so the comparison is
# redone with the copy discriminator removed from BOTH sides -- which asks "is this model assigned
# somewhere its own provenance cannot explain", not "are these strings equal". Whether a paralog
# copy should count toward itself or toward its parent is a separate question, decided by
# count_cr.py --novel-paralog-policy, not here.
COPY_DISCRIMINATOR = re.compile(r"(?:__LOC\d+|_\d+)$")


def normalized_locus(value):
    """Strip PanSN qualification, a copy discriminator, and an annotation version."""
    payload = (value or "").strip()
    if payload.count("#") >= 2:
        payload = payload.split("#", 2)[-1]
    payload = COPY_DISCRIMINATOR.sub("", payload)
    return VERSION_SUFFIX.sub("", payload)


def normalize_support_gene(value):
    """Match the strong-local-support predicate's count-gene normalization exactly."""
    payload = (value or "").strip()
    if payload.count("#") >= 2:
        payload = payload.split("#", 2)[-1]
    match = ENSEMBL_LOCUS.fullmatch(payload)
    return match.group(1) if match else payload


def support_tokens(value):
    """Parse the detailed report's strict comma-token fields."""
    if value in SUPPORT_MISSING:
        return ()
    result = tuple(value.split(","))
    if any(not token or token.strip() != token for token in result):
        raise ValueError(f"invalid comma token list: {value!r}")
    return result


def strong_local_support(row):
    """Truth-blind strong-local predicate shared semantically with the sensitivity builder."""
    assigned = normalize_support_gene(row["assigned_gene"])
    count_gene = normalize_support_gene(row["count_gene"])
    if (
        assigned in SUPPORT_MISSING
        or count_gene != assigned
        or row["current_action"] != "retain_countable"
        or row["count_role"] != "countable"
        or row["structural_class"] != "single_gene"
        or support_tokens(row["local_independent_component_pairs"])
    ):
        return False
    return (
        normalize_support_gene(row["identity_gene"]) == assigned
        and row["identity_basis"] in STRONG_LOCAL_IDENTITY_BASES
    )


def open_text_auto(path, mode="rt"):
    return gzip.open(path, mode) if path.endswith(".gz") else open(path, mode)


def attr(text, key):
    match = re.search(rf"(?:^|;){key}=([^;]*)", text)
    return (match.group(1).strip() or None) if match else None


def set_attr(text, key, value):
    """Replace an attribute in place, or append it, keeping the rest of the column intact."""
    pattern = re.compile(rf"((?:^|;){re.escape(key)}=)([^;]*)")
    if pattern.search(text):
        return pattern.sub(lambda m: m.group(1) + value, text, count=1)
    return f"{text};{key}={value}"


class ParentRollup:
    """Collect the `gene` and `transcript` features the exon rows already point at.

    The corrected annotation is exon-only: every exon declares a `Parent` and a `gene_id` that
    no feature in the file ever defines. Nothing is missing except the rows that summarize what
    the exons already say, so this is a group-by rather than new inference. The one external
    fact is `tag=readthrough_gene`, taken from the reference gene each locus projects from --
    the same set this script already inherits transcript tags from.

    Why it matters: without gene rows the counter cannot see `tag=readthrough_gene`, GENCODE's
    curated fusion-locus declaration. Worse, because this annotation DOES emit `tag=`, it is
    entitled to vote on readthrough, and the transcript-level inference needs EVERY transcript
    of a locus tagged. A locus projected onto hundreds of haplotypes, only some of them tagged,
    therefore votes itself NOT readthrough. Measured on chr20: 26 of 29 readthrough loci return
    to countable under the cr7 tag set.

    Deliberately skipped: the `feature_layer=transcript_body` rows. Their `panSCbody1_` Parent
    namespace exists so a body SPLIT by pangenome sequence trimming can still be expressed --
    a split body is several segments, which is why it is an exon group and not one feature. A
    parent emitted for it would carry the same `transcript_id` as the real transcript and could
    shadow it with a tag-free observation, which is the very failure this rollup removes.
    """

    CARRIED = (
        "gene_id", "transcript_id", "source_gene", "source_transcript", "source_gene_biotype",
        "gene_biotype", "transcript_biotype", "transcript_class", "gene_name",
        "source_gene_common_name", "normalized_transcript_id",
    )

    def __init__(self):
        self.transcripts = {}
        self.bodies = {}
        self.genes = {}
        self.stats = collections.Counter()

    def observe(self, fields):
        attrs = fields[8]
        if attr(attrs, "feature_layer") == "transcript_body":
            self.observe_body(fields, attrs)
            return
        parent = attr(attrs, "Parent") or attr(attrs, "transcript_id")
        if parent is None:
            self.stats["rows_without_parent"] += 1
            return
        try:
            start, end = int(fields[3]), int(fields[4])
        except ValueError:
            self.stats["rows_with_bad_coordinates"] += 1
            return
        record = self.transcripts.get(parent)
        if record is None:
            record = {
                "seqid": fields[0], "source": fields[1], "strand": fields[6],
                "start": start, "end": end, "tags": set(), "split": False,
                "attrs": {key: attr(attrs, key) for key in self.CARRIED},
            }
            self.transcripts[parent] = record
        elif record["seqid"] != fields[0] or record["strand"] != fields[6]:
            # A parent whose rows disagree on contig or strand has no single span. Refuse to
            # invent one; the transcript-level path still sees every row.
            record["split"] = True
        record["start"] = min(record["start"], start)
        record["end"] = max(record["end"], end)
        record["tags"].update(split_tags(attr(attrs, "tag")))

    @staticmethod
    def qualified_gene_id(seqid, gene_id):
        """PanSN-qualify a gene id the way this file's own body rows already do.

        A CAT gene id is per-assembly local, so one projected onto several haplotypes has no
        single genomic span and no unique id. The body layer already spells the qualified form
        `sample#hap#gene_id`, so reuse that rather than invent a syntax.
        """
        prefix = seqid.split("#")[:2]
        return "#".join(prefix + [gene_id]) if len(prefix) == 2 else gene_id

    def observe_body(self, fields, attrs):
        """Collect the gene-body layer, whose Parent namespace is also undefined.

        The body layer exists so a transcript SPLIT by pangenome sequence trimming is still
        expressible: a split body is several segments, which is why it is an exon group rather
        than one feature. Its rows point at a `panSCbody1_` parent that, like the transcript
        parents, no feature defines -- so a read landing in an intron has no structural route
        back to its transcript, only an `exon_unique_parent` attribute.

        The emitted body parent carries the SAME attribute block as its transcript rather than
        the body rows' bare one. That is deliberate: a tag-free parent sharing a transcript_id
        could shadow the real transcript in a consumer that keeps the first row per scope, which
        is exactly the readthrough-vote failure this rollup exists to remove. Identical evidence
        cannot shadow anything.
        """
        parent = attr(attrs, "Parent")
        transcript = attr(attrs, "exon_unique_parent")
        if parent is None or transcript is None:
            self.stats["body_rows_without_parent"] += 1
            return
        try:
            start, end = int(fields[3]), int(fields[4])
        except ValueError:
            self.stats["body_rows_with_bad_coordinates"] += 1
            return
        record = self.bodies.get(parent)
        if record is None:
            self.bodies[parent] = {
                "seqid": fields[0], "source": fields[1], "strand": fields[6],
                "start": start, "end": end, "transcript": transcript, "split": False,
            }
            return
        if record["seqid"] != fields[0] or record["strand"] != fields[6]:
            record["split"] = True
        record["start"] = min(record["start"], start)
        record["end"] = max(record["end"], end)

    def _gene_rows(self, rt_genes):
        for parent, record in self.transcripts.items():
            gene_id = record["attrs"].get("gene_id")
            if gene_id is None:
                continue
            gene_id = self.qualified_gene_id(record["seqid"], gene_id)
            record["gene_uid"] = gene_id
            gene = self.genes.setdefault(gene_id, {
                "seqid": record["seqid"], "source": record["source"], "strand": record["strand"],
                "start": record["start"], "end": record["end"],
                "biotypes": set(), "source_genes": set(), "split": False,
            })
            if gene["seqid"] != record["seqid"] or gene["strand"] != record["strand"]:
                gene["split"] = True
            gene["start"] = min(gene["start"], record["start"])
            gene["end"] = max(gene["end"], record["end"])
            if record["attrs"].get("gene_biotype"):
                gene["biotypes"].add(record["attrs"]["gene_biotype"])
            if record["attrs"].get("source_gene"):
                gene["source_genes"].add(record["attrs"]["source_gene"])

    def emit(self, dst, rt_genes, reference_gene_tags=None):
        reference_gene_tags = reference_gene_tags or {}
        self._gene_rows(rt_genes)
        for gene_id, gene in sorted(self.genes.items()):
            if gene["split"]:
                self.stats["genes_skipped_split"] += 1
                continue
            attrs = [f"ID={gene_id}", f"gene_id={gene_id}"]
            if len(gene["biotypes"]) == 1:
                attrs.append(f"gene_biotype={next(iter(gene['biotypes']))}")
            elif gene["biotypes"]:
                self.stats["genes_with_mixed_biotype"] += 1
            # One CAT gene may hold transcripts from different source genes
            # (rna-transcript-scoped-paralog-policy). A single gene row cannot name one, so it
            # names none and the locus falls through to transcript-level evidence.
            if len(gene["source_genes"]) == 1:
                source_gene = next(iter(gene["source_genes"]))
                attrs.append(f"source_gene={source_gene}")
                stem = VERSION_SUFFIX.sub("", source_gene)
                inherited = reference_gene_tags.get(stem)
                if inherited:
                    # Every tag on a synthesized gene row is inherited by construction -- the row
                    # has none of its own -- so no separate provenance attribute is needed.
                    attrs.append(f"tag={inherited}")
                    self.stats["gene_rows_with_inherited_tags"] += 1
                if stem in rt_genes:
                    self.stats["gene_rows_tagged_readthrough"] += 1
            else:
                self.stats["genes_with_mixed_source"] += 1
            dst.write("\t".join((
                gene["seqid"], gene["source"], "gene", str(gene["start"]), str(gene["end"]),
                ".", gene["strand"], ".", ";".join(attrs),
            )) + "\n")
            self.stats["gene_rows_emitted"] += 1

        for parent, record in sorted(self.transcripts.items()):
            if record["split"]:
                self.stats["transcripts_skipped_split"] += 1
                continue
            attrs = [f"ID={parent}"]
            gene_uid = record.get("gene_uid")
            if gene_uid is not None and gene_uid in self.genes:
                attrs.append(f"Parent={gene_uid}")
            for key in self.CARRIED:
                value = record["attrs"].get(key)
                if value is not None:
                    attrs.append(f"{key}={value}")
            if record["tags"]:
                attrs.append("tag=" + "%2C".join(sorted(record["tags"])))
            dst.write("\t".join((
                record["seqid"], record["source"], "transcript",
                str(record["start"]), str(record["end"]), ".", record["strand"], ".",
                ";".join(attrs),
            )) + "\n")
            self.stats["transcript_rows_emitted"] += 1

        # The gene-body parents, so a read landing in an intron resolves to its transcript
        # structurally and not only through an attribute. Each inherits its transcript's
        # attribute block, so it cannot shadow the transcript with weaker evidence.
        for body_id, body in sorted(self.bodies.items()):
            if body["split"]:
                self.stats["bodies_skipped_split"] += 1
                continue
            record = self.transcripts.get(body["transcript"])
            if record is None:
                self.stats["bodies_without_transcript"] += 1
                continue
            attrs = [f"ID={body_id}", f"Parent={body['transcript']}",
                     "feature_layer=transcript_body"]
            for key in self.CARRIED:
                value = record["attrs"].get(key)
                if value is not None:
                    attrs.append(f"{key}={value}")
            if record["tags"]:
                attrs.append("tag=" + "%2C".join(sorted(record["tags"])))
            dst.write("\t".join((
                body["seqid"], body["source"], "transcript",
                str(body["start"]), str(body["end"]), ".", body["strand"], ".",
                ";".join(attrs),
            )) + "\n")
            self.stats["body_rows_emitted"] += 1


def load_reference_genes(path):
    """Gene id -> (name, biotype), plus the reference's own readthrough vocabulary.

    Also returns the transcripts tagged `readthrough_transcript` and the genes tagged
    `readthrough_gene`, so a projection can inherit what its source says. Without that the
    corrected annotation is half-tagged: it would carry the tag on de-novo transcripts we
    judged and nowhere else, which is worse than carrying it nowhere. A consumer folding a
    per-gene conjunction would see the untagged ortholog projections of a readthrough locus
    vote against it and conclude the locus is ordinary.
    """
    genes = {}
    rt_tx, rt_genes, gene_tags = set(), set(), {}
    with open_text_auto(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9 or fields[2] not in {"gene", "transcript"}:
                continue
            gene = attr(fields[8], "gene_id")
            if gene is None:
                continue
            gene = VERSION_SUFFIX.sub("", gene)
            if gene not in genes:
                genes[gene] = (attr(fields[8], "gene_name") or gene,
                               attr(fields[8], "gene_type") or "N/A")
            tags = attr(fields[8], "tag") or ""
            if fields[2] == "gene" and tags:
                # The whole gene-row vocabulary, not only readthrough_gene. A synthesized gene
                # row has no tags of its own, so anything not inherited here is simply absent
                # from the corrected annotation -- and GENCODE says more about a locus than
                # whether it is a fusion: overlapping_locus and ncRNA_host bear directly on the
                # spurious-overlap error mode, and reference_genome_error and fragmented_locus
                # on whether a locus should be trusted at all. ENSG00000249209 alone carries
                # ncRNA_host, overlapping_locus and readthrough_gene; keeping one of three
                # would be an odd place to stop.
                gene_tags[gene] = tags
            if fields[2] == "gene" and "readthrough_gene" in tags:
                rt_genes.add(gene)
            elif fields[2] == "transcript" and "readthrough_transcript" in tags:
                tx = attr(fields[8], "transcript_id")
                if tx:
                    rt_tx.add(VERSION_SUFFIX.sub("", tx))
    return genes, rt_tx, rt_genes, gene_tags


def nested_reference_genes(path):
    """Map a strictly nested gene to its longer same-strand reference host.

    This is a relation between reference genes, not a judgment that the nested gene is
    uncountable.  A consumer may prefer the host only when both genes explain the same read;
    the nested gene remains valid everywhere its host is absent.
    """
    by_group = collections.defaultdict(list)
    with open_text_auto(path) as handle:
        for line in handle:
            if line.startswith("#"):
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9 or fields[2] != "gene":
                continue
            gene = attr(fields[8], "gene_id")
            if gene is None:
                continue
            try:
                start, end = int(fields[3]), int(fields[4])
            except ValueError:
                continue
            by_group[(fields[0], fields[6])].append(
                (start, end, VERSION_SUFFIX.sub("", gene))
            )

    nested = {}
    for entries in by_group.values():
        # A container precedes what it contains. Carrying the furthest end and its gene
        # identifies the outermost host in one pass; equal spans are deliberately not nested.
        entries.sort(key=lambda entry: (entry[0], -entry[1]))
        best_end, best_length, best_gene = None, 0, None
        for start, end, gene in entries:
            length = end - start
            if best_end is not None and end <= best_end and length < best_length:
                nested[gene] = best_gene
            if best_end is None or end > best_end:
                best_end, best_length, best_gene = end, length, gene
    return nested


def split_tags(value):
    """Split a GFF3 `tag=` value on both spellings of the separator.

    The file escapes its own lists as `%2C`, but add_tag above appends with a bare `,`, so one
    attribute can carry both. A consumer that splits on only one spelling fuses tokens: a
    `basic%2Cfoo,readthrough_transcript` value would yield a token literally named
    `foo,readthrough_transcript`, which matches nothing. count_cr.py avoids this by URL-decoding
    before it splits; this parser does not decode, so it splits on both.
    """
    if not value:
        return ()
    return tuple(
        part.strip() for part in value.replace("%2C", ",").split(",") if part.strip()
    )


def add_tag(attrs, value):
    tags = attr(attrs, "tag")
    if value in split_tags(tags):
        return attrs
    return set_attr(attrs, "tag", value if not tags else f"{tags},{value}")


def load_corrections(path):
    """Load actionable corrections plus factual tags keyed by annotation contig and Parent.

    Only the current detailed classifier schema is accepted. The prior-compatible implementation
    is archived; rejecting its header here makes an old annotation-preparation command fail closed
    instead of silently applying obsolete verdict semantics.
    """
    corrections = {}
    classifications = {}
    seen_report_keys = set()
    with open_text_auto(path) as handle:
        reader = csv.DictReader(handle, delimiter="\t")
        fields = set(reader.fieldnames or [])
        missing = sorted(DETAILED_REQUIRED_COLUMNS - fields)
        if missing:
            raise ValueError(
                "report is not the current detailed-assignment schema; missing columns: "
                + ", ".join(missing)
            )
        for line_number, row in enumerate(reader, start=2):
            if row["schema_version"] != DETAILED_SCHEMA_VERSION:
                raise ValueError(
                    f"report line {line_number} has schema_version "
                    f"{row['schema_version']!r}; expected {DETAILED_SCHEMA_VERSION!r}"
                )
            report_key = (row["contig"], row["parent"])
            if any(value in SUPPORT_MISSING for value in report_key):
                raise ValueError(
                    f"report line {line_number} has empty contig/parent key {report_key!r}"
                )
            if report_key in seen_report_keys:
                raise ValueError(
                    f"report line {line_number} repeats contig/parent key {report_key!r}"
                )
            seen_report_keys.add(report_key)
            verdict = row["current_category"]
            # Every verdict becomes a tag, actionable or not. count_role is the classifier's own
            # two-value summary of those ten categories, carried as a second token so an operator
            # can disqualify the whole alignment-only set with one name instead of listing eight.
            tags = []
            if verdict:
                tags.append(verdict)
                if row.get("count_role") == "alignment_only":
                    tags.append("alignment_only")
            # A SECOND, INDEPENDENT AXIS. The verdict answers whether local structure resolves
            # the model; this answers whether the gene it claims is backed by the transcript it
            # was projected from. A model can pass either and fail the other, so folding them
            # into one token would lose the distinction.
            relation = row.get("source_reference_relation") or ""
            if relation == LABEL_CONFLICT_RELATION:
                # Only a disagreement that survives discriminator-aware normalization is a
                # conflict; a copy disagreeing with its own parent gene is a spelling artifact.
                if normalized_locus(row.get("source_reference_gene")) != normalized_locus(
                    row.get("assigned_gene")
                ):
                    tags.append(LABEL_CONFLICT_TAG)
            elif SOURCE_RELATION_TAGS.get(relation):
                tags.append(SOURCE_RELATION_TAGS[relation])
            # Positive support is a factual annotation property, independent of whether a negative
            # provenance tag is present and independent of any downstream host-protection policy.
            if strong_local_support(row):
                tags.append(STRONG_LOCAL_SUPPORT_TAG)
            if tags:
                classifications[report_key] = tags
            action = ACTIONABLE.get((verdict, row["current_action"]))
            if action is None:
                continue
            proposed = row.get("current_proposed_gene") or ""
            if action == "misassignment" and proposed in ("N/A", ""):
                continue
            corrections[row["parent"]] = (
                action, proposed,
                row.get("category_reason") or "N/A",
                row.get("footprint_evidence") or "N/A",
            )
    return corrections, classifications


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("annotation", help="CAT GFF3 to correct")
    ap.add_argument("report", help="detailed_assignment.tsv.gz from the current attribution step")
    ap.add_argument("reference", help="reference GFF3, for gene names and biotypes")
    ap.add_argument("out_gff", help="corrected GFF3; .gz compresses")
    ap.add_argument("--path-identity-ledger",
                    help="also repoint this ledger's source_gene for reassigned transcripts. "
                         "A collapse-mode counter takes its count gene from here rather than "
                         "from the annotation, so without this the reassignments change "
                         "nothing downstream.")
    ap.add_argument("--path-identity-ledger-out")
    ap.add_argument("--skip-readthrough", action="store_true",
                    help="apply reassignments only; leave readthrough transcripts untagged")
    ap.add_argument("--emit-parent-features", action=argparse.BooleanOptionalAction, default=True,
                    help="emit the `gene` and `transcript` rows the exon rows already point at "
                         "but that no feature in the file defines. Without them a counter cannot "
                         "see tag=readthrough_gene, and because this annotation does emit tag= it "
                         "votes rather than abstaining, so a locus projected onto many haplotypes "
                         "with only some copies tagged classifies itself NOT readthrough. This is "
                         "a group-by over rows already present, not new inference, and it is "
                         "additive: vg rna reads only --feature-type exon (default) and groups by "
                         "--transcript-tag, never by the GFF3 hierarchy, so an already-built graph "
                         "stays valid. Parents are written BEFORE the exon rows, because a "
                         "consumer that deduplicates per transcript keeps whichever row it sees "
                         "first.")
    args = ap.parse_args()

    try:
        corrections, classifications = load_corrections(args.report)
    except ValueError as exc:
        ap.error(str(exc))
    reference, rt_tx, rt_genes, reference_gene_tags = load_reference_genes(args.reference)
    print(f"{len(corrections):,} actionable transcripts; {len(reference):,} reference genes; "
          f"{len(rt_tx):,} readthrough transcripts and {len(rt_genes):,} readthrough genes "
          f"to inherit", file=sys.stderr)

    stats = collections.Counter()
    os.makedirs(os.path.dirname(os.path.abspath(args.out_gff)) or ".", exist_ok=True)
    rollup = ParentRollup() if args.emit_parent_features else None
    # Parents must precede their children, so corrected rows are staged beside the output and
    # concatenated after the rollup is complete. The temp file sits next to out_gff rather than
    # in $TMPDIR so it lands on the same filesystem as the result.
    body_path = args.out_gff + ".rows.tmp"
    def observe(fields):
        """Stage one corrected data row and fold it into the parent rollup."""
        dst.write("\t".join(fields) + "\n")
        if rollup is not None:
            rollup.observe(fields)

    with open_text_auto(args.annotation) as src, open(body_path, "wt") as dst:
        for line in src:
            if line.startswith("#"):
                dst.write(line)
                continue
            fields = line.rstrip("\n").split("\t")
            if len(fields) != 9:
                dst.write(line)
                continue
            # Exons, CDS, introns and codons hang off the transcript via Parent; a
            # transcript row's own Parent is its *gene*, so keying on Parent everywhere
            # corrects the children and silently leaves the parent row contradicting them.
            if fields[2] in {"transcript", "mRNA"}:
                parent = attr(fields[8], "ID") or attr(fields[8], "transcript_id")
            else:
                parent = attr(fields[8], "Parent") or attr(fields[8], "transcript_id")
            # The classifier judged this Parent, so its verdict rides on the row as a tag
            # whether or not the verdict is actionable. This is the filter surface:
            # count_cr.py --demote-transcript-tags takes an arbitrary token set, so which
            # categories disqualify a transcript becomes a stated per-run choice rather than a
            # rule baked into the artifact, and no side-channel ledger column is needed.
            verdict_tags = classifications.get((fields[0], parent))
            if verdict_tags:
                attrs = fields[8]
                for tag_value in verdict_tags:
                    attrs = add_tag(attrs, tag_value)
                if attrs != fields[8]:
                    fields[8] = attrs
                    stats["rows_tagged_with_verdict"] += 1

            entry = corrections.get(parent)
            if entry is None:
                # Not a de-novo transcript we judged, but its source may still carry the
                # reference's readthrough vocabulary, which CAT drops on projection.
                attrs = fields[8]
                src_tx = attr(attrs, "source_transcript")
                src_gene = attr(attrs, "source_gene")
                inherited = False
                if src_tx and VERSION_SUFFIX.sub("", src_tx) in rt_tx:
                    attrs = add_tag(attrs, "readthrough_transcript")
                    inherited = True
                if (fields[2] == "gene" and src_gene
                        and VERSION_SUFFIX.sub("", src_gene) in rt_genes):
                    attrs = add_tag(attrs, "readthrough_gene")
                    inherited = True
                if inherited:
                    fields[8] = attrs
                    observe(fields)
                    stats["inherited_readthrough_tag"] += 1
                else:
                    observe(fields)
                    stats["unchanged"] += 1
                continue
            action, proposed, method, support = entry
            attrs = fields[8]
            if action == "misassignment":
                original = attr(attrs, "source_gene") or "N/A"
                name, biotype = reference.get(proposed, (proposed, "N/A"))
                attrs = set_attr(attrs, "source_gene", proposed)
                attrs = set_attr(attrs, "source_gene_common_name", name)
                attrs = set_attr(attrs, "gene_name", name)
                for key in ("gene_biotype", "transcript_biotype", "source_gene_biotype"):
                    if attr(attrs, key) is not None:
                        attrs = set_attr(attrs, key, biotype)
                attrs = set_attr(attrs, "cat_corrected_from", original)
                stats["reassigned"] += 1
            elif args.skip_readthrough:
                observe(fields)
                stats["unchanged"] += 1
                continue
            else:
                attrs = add_tag(attrs, "readthrough_transcript")
                attrs = set_attr(attrs, "cat_corrected_from",
                                 attr(attrs, "source_gene") or "N/A")
                stats["tagged_readthrough"] += 1
            attrs = set_attr(attrs, "cat_correction_method", method)
            attrs = set_attr(attrs, "cat_correction_support",
                             support.replace(";", ",") or "N/A")
            fields[8] = attrs
            observe(fields)

    with open_text_auto(args.out_gff, "wt") as out:
        out.write("##gff-version 3\n")
        if rollup is not None:
            rollup.emit(out, rt_genes, reference_gene_tags)
        with open(body_path) as staged:
            shutil.copyfileobj(staged, out)
    os.remove(body_path)

    for key, value in stats.most_common():
        print(f"  {value:>12,}  rows {key}", file=sys.stderr)
    if rollup is not None:
        for key, value in rollup.stats.most_common():
            print(f"  {value:>12,}  {key}", file=sys.stderr)

    if args.path_identity_ledger and args.path_identity_ledger_out:
        # The ledger schema is a fixed 24 columns, so the corrected gene is written in place
        # and the original recorded in a sidecar rather than an extra column.
        moved = 0
        side = args.path_identity_ledger_out + ".corrections.tsv"
        with open_text_auto(args.path_identity_ledger) as src, \
                open_text_auto(args.path_identity_ledger_out, "wt") as dst, \
                open(side, "w", newline="") as prov:
            note = csv.writer(prov, delimiter="\t", lineterminator="\n")
            note.writerow(("unique_parent", "source_gene_was", "source_gene_now",
                           "method", "support"))
            header = True
            for line in src:
                fields = line.rstrip("\n").split("\t")
                if header or len(fields) != 24:
                    dst.write(line)
                    header = False
                    continue
                # An exon row is keyed by unique_parent; its body-layer counterpart carries a
                # different unique_parent and points back through exon_unique_parent. Both
                # must move together or the counter rejects the ledger for disagreeing with
                # its own provenance.
                key = fields[16] if fields[13] == "body" else fields[4]
                entry = corrections.get(key)
                if entry is None or entry[0] != "misassignment":
                    dst.write(line)
                    continue
                note.writerow((fields[4], fields[18], entry[1], entry[2], entry[3]))
                fields[18] = entry[1]
                dst.write("\t".join(fields) + "\n")
                moved += 1
        print(f"  {moved:>12,}  ledger rows repointed -> {side}", file=sys.stderr)


if __name__ == "__main__":
    main()
