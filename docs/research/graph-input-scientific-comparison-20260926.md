# Scientific-output comparison contract for graph-input experiments

User acceptance clarification, 2026-09-26. This is an experiment comparison specification,
not a change to the current production output format. Source inspected at `f511306`.

Use one frozen GAMP, bundle, barcode whitelist, executable/dependency set, profiles, score
configuration, and requested resource settings. Map once; repeated mapper output is not an
input to this comparison. The first comparison must be repeated full-path-XG counting.
Stop on any scientific difference before comparing new graph modes.

## Byte comparisons

Require `cmp` equality of all emitted scientific files, including:

- `parquet/barcodes.parquet`, `features.parquet`, `counts.parquet`, `molecules.parquet`;
- every requested per-profile MEX matrix, barcode, and feature file;
- requested RAD and its scientific companion files;
- any requested diagnostic scientific table, if present in the matched run configuration.

Require the same file inventory in both runs. Do not silently ignore an extra or missing
scientific output. Preserve and compare all profile identities, logical table hashes, table
row counts, and output byte hashes. Input mode must not appear in scientific files.

MEX currently uses zlib `gzopen(..., "wb6")` in `src/direct_count_output.cpp:258` without an
explicit custom header. Inspect the actual gzip headers and compare compressed bytes on
repeated runs; record MTIME and filename flags. A decompressed comparison is diagnostic
only unless the report explicitly states and obtains acceptance for that weaker criterion.
The repeated-XG result below establishes deterministic compressed bytes for the named fixture.

## Explicit operational exclusions

The current native-count schema embeds resource/cache statistics in summary and manifest.
Retain raw originals. Create a separate canonical scientific projection for comparisons,
without changing production files. Unknown fields or output roles require review rather
than a broad ignore rule.

In `summary.tsv`, exclude only rows with `scope=global`, `profile_id=.`, and one of these
metric names:

```text
aggregate_spill_runs
aggregate_spill_rows
aggregate_spill_bytes
assignment_cache_capacity
assignment_cache_entries
assignment_cache_hits
assignment_cache_misses
assignment_cache_uncached
```

Compare every other row, including every per-profile row and all input, molecule, barcode,
assignment, rejection-reason, and final-size counters. Cache/spill fields describe resource
execution, not assignment or molecule identity; keep their actual values in raw receipts.

For `manifest.json`, the exclusions in the current schema are precisely:

- `/command_line`: retain verbatim in provenance; separately verify the fixed scientific
  arguments and whitelist the intended graph/output-path differences.
- `/assignment_cache/{capacity,entries,hits,misses,uncached}`.
- `/counters/global/` followed by each of the eight summary metric names above.
- `/count_storage` (and legacy `/memory_budget_bytes`) and `/threads`: retain truthful values and enforce the matched
  experiment settings separately; their exclusion does not authorize changing the comparison.
- `/timings/initialization_seconds`, `/timings/processing_seconds`,
  `/timings/read_processing_seconds`, `/timings/compatibility_finalize_seconds`,
  `/timings/count_finalize_seconds`, `/timings/diagnostics_merge_export_seconds`.
- `/inputs/{xg,gbz,gbwt}/{path,sha256,size_bytes}` for cross-graph comparisons only (D094).
  Require precisely one valid graph role combination: `xg`, `gbz`, or `xg` plus `gbwt`.
  Reject unknown fields inside these roles; remove the emptied graph roles for the scientific
  projection. GAMP, whitelist, bundle and every other input identity must match. Command-line
  normalization removes only these graph flag/value pairs and normalizes `--out-dir`; all
  scientific arguments and the executable remain compared.
- `/outputs/summary/{logical_sha256,output_sha256,size_bytes}`, because these bind the raw
  summary containing the excluded operational counters. Its scientific projection is compared
  independently; summary path and row count remain checked.

All scientific profile/counter/table fields remain compared. The primary experiment omits
compatibility capture; `/compatibility_bundle` must therefore remain null. If capture is later
included, its input provenance and scientific content need their own explicit comparison.
Host, timestamps, and input-mode fields are not top-level fields in the inspected count schema;
do not add wildcard exclusions anticipating them. External GNU time/host receipts remain
truthful and separate from scientific comparisons.

Current results contain operational data in manifest and summary, so whole-directory `cmp`
is not the criterion. Result files themselves remain directly byte-compared. No production
output separation is introduced by the graph-input feature.

## Baseline XG repeat: PASS (2026-09-26)

The first stop gate passed on the explicitly named read-contiguous subset
`/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/joint/inputs/filtered_first100k.gamp`:
100,000 read groups and 216,700 GAMP records. `xg_1`/`xg_2` exited 0 in 459.06/448.25 s with
peak RSS 56,688,948/56,689,116 KiB. The comparator found identical scientific inventory,
Parquet, MEX, RAD, scientific summary projection, and scientific manifest projection. All six
MEX gzip files had identical compressed bytes and headers (`mtime=0`, flags 0, no filename).
Receipt: `/mnt/ssd/lalli/pancollapse_graph_input_design_v1_20260926T154801Z/joint/results/comparison_xg_1_vs_xg_2.json`.
This establishes same-mode XG repeatability for this subset only; it does not implement or
approve a new graph-input backend.

## Projection-fixture comparisons during implementation

`tests/vg/compare_projection_outputs.py` compares every RAD, Parquet, MEX, tx2gene,
evidence table and summary produced under the fixture roots, with equal scientific-file
inventories. Native-count summaries use only the exclusions above; converter summaries
compare directly. The converter's optional BAM already embeds its command line in `@PG CL`.
For those fixture BAMs only, compare the complete ordered SAM records and all headers except
that single provenance field. Preserve the original BAMs and their hashes. This does not
weaken the byte comparison of native-count products; native `count` has no BAM output.

`tests/vg/compare_graph_scientific.py --allow-executable-change` is reserved for the default-XG
regression comparison to the archived baseline executable. It records both executable hashes
and permits only argv[0] to differ beyond the ordinary output-path exclusion. Same-build
repeats and cross-graph acceptance do not use that option.

## Implementation acceptance: PASS (2026-09-26)

The frozen implementation executable passes a fresh repeated-XG comparison and both GBZ and
XG+GBWT comparisons on the same named subset. Each compares all 11 scientific files, all six
compressed MEX headers, scientific summary counters and the manifest projection. Default XG
also matches the archived pre-feature scientific output using the executable-change exception.
Receipts are under `/mnt/ssd/lalli/pancollapse_graph_input_implementation_v1_20260926/results/`;
the [implementation record](gbz-gbwt-input-implementation-20260926.md) gives measurements and limits.
