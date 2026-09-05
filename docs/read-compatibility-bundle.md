# Reusable tiered read-compatibility bundle

## Purpose

`pancollapse-read-compatibility-v1` is the fast iteration boundary between graph alignment
interpretation and gene-count policy. It replaces the abandoned BAM handoff for native counting.
The bundle contains no alignments, sequence, CIGAR, or SAM tags. It retains the policy-neutral
facts needed to rerun annotation metadata/tagging and counting decisions, plus exactly one ordered
row for every input GAMP read group.

Produce it once while doing a direct count:

```sh
panCollapse count \
  --gamp reads.gamp --xg graph.xg \
  --count-bundle count-facts \
  --barcode-whitelist barcodes.txt \
  --cr7 --pansc-strict-v1 \
  --compatibility-out compatibility \
  --out-dir direct-count
```

Replay it under current corrected annotation metadata and profiles:

```sh
panCollapse count \
  --compatibility-in compatibility \
  --count-bundle revised-count-facts \
  --barcode-whitelist barcodes.txt \
  --cr7 --pansc-strict-v1 \
  --read-assignments-out diagnostics/read_assignments.parquet \
  --out-dir replay-count
```

Replay bypasses XG deserialization, graph-path scoring, exact-transcript dynamic programming, and
GAMP decoding. It still performs current annotation-policy binding, assignment, barcode
correction, UMI collapse, matrix generation, optional MEX, and optional per-read diagnostics.

## Evidence boundary

The bundle is captured before `CountFactCatalog::candidate` and contains:

- exact compatible path/Parent facts with canonical transcript, score, strand, and E/P/B tier;
- structural winning path/Parent facts with canonical transcript, score, strand, and S/U layer;
- original read-group ordinal and name;
- raw CB/CY/UMI/UY values when valid, or an explicit missing/malformed/unsupported status;
- empty fact sets for valid featureless or unaligned reads.

It does **not** store a policy row, candidate gene, filter decision, selected gene, or final
assignment. On replay, the current count-facts bundle maps the retained Parent identities to
current gene metadata, comma-separated category policy, competition/equivalence/nested-host
policy, and profile behavior. Structural S facts reconstruct the current Gene fallback; U facts
remain available for diagnosis and future policy but do not become fallback under the frozen
profiles.

Allowed replay changes include gene assignment, gene metadata and tags, gene-category policy,
competition/equivalence/nested-host policy, selected profile and profile overrides, barcode
whitelist, diagnostics, MEX, and count-memory budget. Transcript/exon geometry, path/Parent to
canonical-transcript structure, or the graph-to-tier compatibility algorithm may not change.
The source GAMP and XG SHA-256 values remain in the manifest. Replay deliberately does not reopen
XG; if the graph changes, regenerate the bundle rather than treating ledger validation as a graph
topology comparison.

## Physical layout

The directory is atomically published only after staged validation:

| Path | Rows and columns |
|---|---|
| `manifest.json` | schema, logical content ID, producer/algorithm/structural IDs, source digests, input record/read-group denominators, molecule-status totals, and table receipts |
| `parquet/read_rows.parquet` | `ordinal`, `original_name`, `molecule_status`, nullable raw molecule fields, `fact_set_id` |
| `parquet/fact_sets.parquet` | one canonical interned `fact_set_id` and `complete_provenance` flag |
| `parquet/exact_facts.parquet` | fact-set ID, transcript, locus Parent, exact path/Parent, score, E/P/B tier, strand |
| `parquet/structural_facts.parquet` | fact-set ID, transcript, S/U layer, score, strand, winning path and Parent lists |

Every table has both a physical SHA-256 and a typed logical SHA-256. `content_id` is the SHA-256
of canonical manifest content. The reader checks table paths, sizes, physical hashes, exact Arrow
schemas, row counts, canonical fact ordering, referential integrity, logical hashes, contiguous
ordinals, producer read-group equality, and molecule-status totals before count publication.

## Storage and execution properties

During production, ordered read rows go immediately to a checksummed ZSTD spool. Equivalent fact
sets are interned; final IDs are assigned by canonical sort, making one-worker and multi-worker
outputs byte-identical. Final Parquet row groups and replay batches are capped at 1,048,576 rows;
the default is 65,536. Memory is independent of read-row count except for the set of **unique**
compatibility fact sets; the latter is not yet spillable and must be measured on the authoritative
large fixture before a production memory bound is claimed.

Compatibility publication is an independently terminal stage: a completely validated bundle may
remain usable if later count finalization fails. A successful count manifest links the produced or
replayed bundle, but failure of that later output does not invalidate the already published
compatibility evidence.

Replay currently uses one active read-row worker even if a larger `--threads` ceiling is supplied;
stderr and the count manifest report one active worker. Eliminating graph/GAMP work is expected to
be the dominant speedup, but production runtime and storage guidance remains pending a bounded
benchmark on the current fixed fixture.

## Validation state

The repository's fast graph-backed fixture proves:

- direct GAMP/XG count equals compatibility replay for count/molecule/barcode/feature Parquet,
  MEX, diagnostics, logical identities, and every biological counter;
- profile score-window override replay equals a fresh direct override run;
- one-worker and forced four-worker compatibility bundles are byte-identical;
- a revised authenticated annotation bundle that adds `readthrough_transcript` Parent tags changes
  CR7 assignments while leaving compatibility bytes unchanged; replay matches a fresh direct
  count, and panSC-strict's explicit relaxation remains unchanged;
- malformed aligned and valid unaligned reads survive production capture and replay;
- batch and streaming writers are byte-identical, equal fact sets intern deterministically, and
  corrupt physical/logical data is rejected;
- changed exon/body path structure is rejected while candidate facts are rebound through the
  current count-facts catalog.

Run the focused gate with:

```sh
ctest --test-dir build -R '^direct_count_compatibility_verify$' --output-on-failure
```

The fixture is deliberately tiny so filter/tagging methods can be exercised on every iteration.
A larger prevalence-bearing fixture remains the separate gate for F1, storage, runtime, and
single-cell QC estimates.
