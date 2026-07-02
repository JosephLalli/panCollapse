# Roadmap

## V1 — GAMP to transcript-target RAD

Defined in `docs/product-spec.md`.

## V1.1 — Copy-resolution alternatives

Potential selectable behavior:

- preserve specific annotated transcript-copy identity;
- collapse source copies to an explicitly chosen canonical copy;
- investigate genetically closest-copy selection against annotated, T2T, or GRCh38
  references.

Automatic closest-copy inference is not part of V1.

## V2 — Splicing-state aware assignment

Potential additions:

- synthetic unspliced or gene-body targets;
- explicit spliced/unspliced/ambiguous evidence;
- USA/splici-compatible output;
- more complete GAMP connection-arc treatment if not already required by V1;
- molecule-level state resolution.

## V3 — Additional quantifier backends

Potential independent backends:

- direct projection to transcript coordinates and transcriptome BAM for Salmon or other
  alignment-based tools;
- BUS/TCC-compatible output for kallisto/bustools-style workflows.

Transcriptome BAM and BUS are separate backends, not interchangeable representations.

## Deferred optimization idea

If V1 profiling shows per-node scoring and HST path attribution (the `for_each_step_on_handle`
loop) are a dominant cost, investigate a reusable panCollapse-specific path-attribution
structure. This remains an idea until evidence and a human-approved ADR justify it.
