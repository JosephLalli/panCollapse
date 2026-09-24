# Annotation correction and fixture-creation scripts

These scripts prepare the inputs `panCollapse count` reads. None of them runs at count time:
the binary has no Python dependency at production runtime. They fall into three groups.

Scripts under `tooling/` are repository harness (git hooks, provenance guard, workspace check,
Docker image build) and are unrelated to annotation.

## Correcting a CAT annotation

The count-fact route added by D088 ingests a GFF3 that already carries the classifier verdicts
as `tag=` attributes. The chain that writes them. Only two orderings below are documented in the
scripts themselves — step 2 consumes step 1's report, and step 3's docstring places it after step
2 and before graph building — so treat the rest as a grouping rather than a fixed sequence:

1. `assign_cat_denovo_transcripts_detailed.py` (or the narrower
   `assign_cat_denovo_transcripts.py`) attributes CAT de-novo predictions to the locus their
   exon structure actually came from, rather than the transMap gene they happen to overlap. It
   emits a `panSC-cat-parent-detailed-v4` `detailed_assignment.tsv.gz`.
2. `apply_cat_denovo_corrections.py` consumes that report and writes a corrected GFF3. It
   repoints `source_gene` on a MISASSIGNMENT verdict and writes `tag=readthrough_transcript` on
   a READTHROUGH verdict. `source_gene` is the field a collapse-mode counter keys on, so this is
   the edit that changes counting.
3. `tag_nested_reference_hosts.py <corrected> <reference> <output>` adds the pairwise
   nested-host fact. Each projected row whose version-stripped `source_gene` lies strictly
   inside a longer same-strand reference gene receives both `tag=nested_within_host_gene` and
   `nested_host_gene=<host>`. Naming the host matters because the downstream policy is a
   pairwise preference, never a global deletion of the nested gene. Run it after step 2 and
   before building the graph. On the `--annotation` route this pass is the only source of
   `nested_host`: an annotation that skips it carries no nested-host relation and nested-host
   policy silently has nothing to act on. Note two constraints on the consuming side:
   `CountFactCatalog::from_annotation` reads `nested_host_gene` only from a gene row, and
   `nested_within_host_gene` is not one of the twelve categories a count profile acts on, so the
   tag itself is dropped and only the attribute carries information.
4. `filter_cellranger_annotation.py` builds the Cell Ranger reference-style annotation and the
   matching graph Parent allowlist, following the public 10x recipe.
5. `audit_readthrough_consistency.py <corrected> --reference <ref> --path-identity-ledger
   <ledger>` verifies the corrected annotation against what the counter assumes about
   readthrough before a run. It checks the three paths by which a copy could lose a tag its
   source asserts -- gene row to child transcripts, reference source transcript to projected
   copy, and body Parent to exon counterpart -- and exits non-zero on a violation. A copy
   tagged where the reference is not is permitted, because a projection can read through in one
   assembly and not another. See `docs/annotation-correction.md`.

The scripts D083 audited — `filter_cellranger_annotation.py`, `gff3_to_genebody.py`, and
`assign_cat_denovo_transcripts.py` — key transcripts on the unique GFF3 `ID`, never on bare
`transcript_id` or on a transcript row's `gene_id`. The remaining scripts were not part of that
audit. See the annotation identifier rules under the repository's agent rules
directory (`annotation-identifiers.md`) for why the distinction matters.

## Building the graph and ledger layers

- `gff3_to_genebody.py` turns a spliced transcript annotation into the per-transcript body
  annotation `vg rna` projects into the body path layer, so ledger count modes can reproduce
  STARsolo `GeneFull`.
- `make-gene-annotation.sh` is the shell equivalent at gene granularity: it annotates a graph
  with gene-body paths and writes the matching gene t2g.
- `make_reference_path_identity_ledger.py` builds `panSC-path-identity-v1` for a reference GTF
  projected by `vg rna`. It is deliberately limited to locally built reference and toy indexes;
  a production CAT or pangenome annotation must supply its own audited ledger, and identities
  must never be reconstructed from path-name suffixes.
- `build_reclassified_parent_allowlist.py` builds the producer-side Parent allowlist, filtering
  by countability first and frozen Cell Ranger gene-set membership second.
- `project_cat_parent_count_policy.py` projects an existing CAT Parent report onto a path
  identity ledger. It is report-only and does not re-run CAT classification.

## Validation artifacts

- `derive_count_facts_from_annotation.py` derives the count-fact bundle tables in Python from
  `--annotation`, `--reference`, and `--path-identity-ledger`. It established the derivation rule
  at 100.00% agreement with the identifier-fixed bundle before the C++ factory was written. It is
  a validation harness, not a pipeline step: at runtime `CountFactCatalog::from_annotation`
  performs the same derivation in the binary, and running this script is not a prerequisite for
  the `--annotation` route.
