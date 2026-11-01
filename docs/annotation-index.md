# Build the direct counting index once

`panCollapse index` prepares reference data; `panCollapse count` uses the
complete graph-bound direct-facts object. XG, transcript GBWT and a prebuilt
FastLocate RI are used during indexing, not during counting. Counting does not
parse annotation/ledger text, locate paths, discover junctions or build reference
structures.

From the repository root, compile corrected annotations if no PCANIDX3 catalog
already exists:

```bash
build/src/panCollapse index \
  --annotation corrected.gff3.gz --annotation-reference reference.gff3.gz \
  --path-identity-ledger path_identity_ledger.tsv --out annotation.pci
```

Bind that catalog to the matching transcript GBWT, compile its node and splice
facts, then count each sample. Run these commands from the repository root:

```bash
build/src/panCollapse index --from-annotation-index annotation.pci \
  --gbwt transcripts.gbwt --out native.pci --threads 128

build/src/panCollapse index --from-annotation-index native.pci \
  --xg graph.xg --gbwt transcripts.gbwt \
  --r-index transcripts.ri --r-index-manifest binding.json \
  --threads 128 --memory-budget 300GB --out direct.pci

build/src/panCollapse count --gamp reads.gamp --annotation-index direct.pci \
  --barcode-whitelist barcodes.txt --cr7 --pansc-strict-v1 \
  --threads 128 --memory-budget 300GB --out-dir counts
```

Alternatively, native binding accepts `--count-bundle bundle` instead of
`--from-annotation-index annotation.pci`. PCANIDX2/3 catalogs and PCANBND1
bindings are preparation inputs, not count-ready direct facts. Reusing a native
binding avoids rereading GFF or ledger text. `index --xg` now compiles direct
facts; it does not revive the retired path-position/occurrence builder.
Historical expanded PCANCOUNT indexes and raw-annotation count routes remain
retired. BAM/RAD export remains a separate, deprecated `convert` command.

Direct facts are written in compact V2 format as they are compiled; no expanded
intermediate or separate repacking step is needed. Counting accepts only this
direct-index format. Older direct indexes must be rebuilt from preparation inputs.

## Choose a memory target

`count --memory-budget 300GB` sets an overall planning target. Without it, count
uses available host memory and remaining capacity in its cgroup. The effective
target cannot exceed those detected limits. Use a cgroup or Docker `--memory`
to enforce a hard process limit; the planning target alone does not enforce one.

After loading metadata, count conservatively accounts for the entire mapped
index as well as current resident memory and reserves 10% of the target for
workers and count accumulation. The remaining pool is the assignment-cache
ceiling. The reserve is a planning heuristic, not an RSS guarantee. Allocation
grows on demand; cache capacity is not preallocated RAM. Memory mapping avoids
a separate payload copy, but accessed file-backed pages still consume memory.

Override the ceiling with `--cache-limit assignment=SIZE`. Sizes accept bytes
or `KB`, `MB`, `GB`, `TB`, `KiB`, `MiB`, `GiB`, `TiB`. The limit must fit the
cache pool. Zero, duplicate, unknown and obsolete reference-cache limits fail.
The startup log and output manifest record the effective plan.

The assignment cache evicts older entries when full and uses a byte
ceiling without a separate practical entry-count ceiling. Evicted or uncached
assignments are recomputed, not discarded or spilled to disk. Worker-held values,
allocator overhead and count tables also consume memory, so the reserve is a
heuristic, not an RSS guarantee. Ceilings do not resize during counting.

## What the object contains

The object stores compact node length/list records; shared, sorted numeric
model-membership lists with exon/body orientation and support; ready model,
gene and policy metadata; known physical splice targets; oriented splice
ownership; and the output feature dictionary. Identical node annotation lists
share one payload. Distinct model identities remain distinct even when gene
names match. It contains no DNA, full graph topology, path-step arrays,
coordinate checkpoints or repeated-node occurrence identities.

While streaming GAMP, count reads immutable spans for touched nodes and local
transitions. Reads need not fit one ordered annotation walk.
Known wrong-way splices exclude their candidates. Counting uses the node-score
DAG maxima algorithm; there is no alternative legacy assignment engine.

During direct compilation, annotation, XG, GBWT and RI load independently in
parallel at four or more threads. RI attaches to GBWT after loading. Keep the
compiler inputs from one graph generation together. The existing
`pancollapse-r-index-binding-v1` receipt supplies GBWT/RI sizes and prior hashes.
compiler checks receipt sizes and native structures without repeating
large-input hash scans; supplied hashes remain explicitly unverified during
this build. Provenance is embedded in the direct object and copied to the count
manifest as `reference_provenance_json`. An adjacent RI is not auto-detected.

The one-time compiler can still visit many raw GBWT occurrences. It processes
them without retaining a global path-step array; index size and construction
time must be measured rather than inferred from the node-directory size.

Counts are memory-only, with no intermediate BAM or evidence spool. Parquet is
always written; `--10x-mex` is optional. Outputs are staged and published together
after all requested sinks close. Existing destinations are refused. Whole-genome
completion, the two-hour target and the 300-GB limit remain unproven until their
full-run receipts pass.

## Annotation inputs and identities

Both annotation inputs may be plain, gzip or BGZF. GFF3 is the default; use
`--annotation-format gtf` and/or `--annotation-reference-format gtf` for GTF.
There is no extension-based format guessing.

GFF3 joins on `ID` and `Parent`. GTF requires explicit gene/transcript features
and joins on `gene_id`/`transcript_id`; it does not invent models from exon rows.
Transcript IDs must match ledger Parents, including namespace. Repeated tags are
supported; conflicting scalar attributes or transcript/gene mappings fail.
The compiler preserves annotation policy; it does not correct or filter the GFF.

Indexing requires the ledger to establish graph path identity. Source text and
barcode/UMI data are not count-time reference inputs. The compiler embeds the
needed numeric identities and records source provenance. It validates framing,
bounds, references and compatibility before publishing to a new destination.

An existing facts-only PCANIDX2 catalog can first be upgraded without rereading
GFF/GTF:

```bash
build/src/panCollapse index --from-annotation-index old-v2.pci \
  --path-identity-ledger path_identity_ledger.tsv --out annotation.pci --threads 128
```

Then bind `annotation.pci` to GBWT as above. Rebuild to a new filename if
annotation, reference, ledger, graph generation or compilation semantics change.
