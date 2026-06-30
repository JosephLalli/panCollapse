# Transcript Compatibility Semantics

This file is the behavioral reference for annotation-aware assignment. It intentionally
does not prescribe interval-tree types, graph algorithms, or source-file organization.

## Core rule

A candidate GAMP traversal contributes a transcript target when its aligned evidence is
consistent with that transcript under the annotation and splice-junction rules.
Compatibility includes annotated intronic evidence; it is not mature-transcript sequence
matching alone. At least one aligned reference-consuming base must overlap an exon or
implied intron of the candidate transcript. Other aligned sequence may overhang the
transcript's first-to-last-exon span. Parent-gene overlap alone is insufficient.

panCollapse does not remove compatibility by library strand. It preserves the actual
target-relative orientation of compatible evidence in RAD `dirs` so downstream
alevin-fry expected-orientation handling can make library-orientation decisions.

## Required cases

| Case | Expected transcript compatibility |
|---|---|
| Read lies wholly inside one exon | Compatible |
| Read lies wholly inside an annotated intron | Compatible |
| Unspliced read crosses exon into intron | Compatible |
| Unspliced read crosses intron into exon | Compatible |
| Read is exonic in isoform A and intronic in isoform B | Compatible with A and B |
| Read has a splice junction annotated in transcript A | Compatible with A |
| Read has a splice junction absent from transcript B | Incompatible with B |
| At least one aligned base overlaps the transcript model and other sequence extends beyond first/last exon span | Compatible if all other rules pass |
| Alignment overlaps only the parent gene locus, not the transcript's exons or implied introns | Incompatible |
| Forward target-relative orientation | Compatible if other rules pass; RAD `dirs` records forward |
| Reverse target-relative orientation | Compatible if other rules pass; RAD `dirs` records reverse |
| Mixed orientations for the same read group and emitted target | Dropped and counted rather than assigned a synthetic orientation |

## Multiple GAMP alternatives

- Evaluate complete candidate traversals separately.
- A target's score is the best score among compatible traversals and source paths that
  collapse to it.
- The union of eligible targets across retained alternatives forms the read-level target
  set.
- The implementation must not multiply evidence merely because one biological target is
  represented by multiple equivalent graph or haplotype paths.

## Historical/Future Transcript Versus Gene Uniqueness

D044 removes transcript- and gene-uniqueness filtering from active GAMP-to-RAD output.
The definitions below are retained only as future context if panCollapse later expands
beyond RAD compatibility-set emission.

```text
many graph paths -> many source transcript paths -> canonical transcripts -> genes
```

- One remaining transcript means transcript-unique.
- Several remaining transcripts that all map to one gene means gene-unique.
- Several remaining genes means gene-ambiguous.

## No V1 state classification

Intronic compatibility does not create an unspliced label. An intron-compatible source
transcript identity contributes the same canonical transcript ID that its manifest row
defines. The standard two-column transcript-to-gene map later projects that target to its
gene.

## Fixture requirement

Every row in the required-cases table must become at least one explicit expected-output
fixture before implementation broadens beyond the initial vertical slice. The
outside-first/last-exon behavior requires a dedicated fixture because it differs from a
strict transcript-span containment model.
