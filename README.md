# panCollapse

`panCollapse` converts pangenome single-cell RNA-seq alignments from `vg mpmap` (GAMP,
multipath format) into mapper-style, uncollated RAD records that
[alevin-fry](https://github.com/COMBINE-lab/alevin-fry) quantifies into a cell-by-gene matrix.

Version 0.10 also provides native `panCollapse count`: a count-fact-bundle-authoritative
path that emits a verified Parquet count dataset by default. It can additionally emit 10x
MEX and RAD with explicit flags. Legacy `panCollapse convert` remains the independent
GAMP-to-RAD/BAM evidence interface.

It bridges a pangenome aligner and the standard single-cell counting workflow: for each read
it reports the set of transcripts the read is compatible with (with orientation), reading
compatibility directly off the haplotype-specific transcript (HST) paths that `vg rna`
embedded in the graph — no GTF or annotation projection at run time.

## Requirements

- A VG installation/checkout to build and link against (the tool consumes `vg mpmap` GAMP and
  a matching `.xg`). Build it, or point at an existing checkout.
- C++20 (GCC 15), CMake, and Ninja.
- Pinned Arrow/Parquet C++ headers and shared libraries for native count output
  (`PANCOLLAPSE_ARROW_ROOT` when CMake package discovery cannot find them).
- [alevin-fry](https://github.com/COMBINE-lab/alevin-fry) only when using legacy
  `convert`/RAD output with its downstream quantification route; native `count` has no
  alevin-fry runtime dependency.
- Python 3 to run the test suite.

## Build

```sh
VG=/path/to/vg                      # a vg checkout/install with lib/pkgconfig
PKG_CONFIG_PATH="$VG/lib/pkgconfig" cmake -S . -B build -G Ninja \
  -DPANCOLLAPSE_VG_ROOT="$VG" \
  -DPANCOLLAPSE_ARROW_ROOT=/path/to/arrow-parquet \
  -DCMAKE_PREFIX_PATH="$VG" \
  -DCMAKE_BUILD_RPATH="$VG/lib"
cmake --build build
ctest --test-dir build --output-on-failure
```

The converter is `build/src/panCollapse`.

### Configure variables

| Variable | Required | Description |
|---|---|---|
| `PANCOLLAPSE_VG_ROOT` | Yes | Path to a VG checkout/build with `include/xg.hpp` and `lib/pkgconfig`. |
| `PANCOLLAPSE_ARROW_ROOT` | Native count | Pinned Arrow/Parquet C++ prefix containing `include/`, `libarrow`, and `libparquet`; required when their CMake package files are not discoverable. |
| `CMAKE_PREFIX_PATH` | Recommended | Set to the same VG path so CMake finds `libhandlegraph`, `VGio`, and `Protobuf`. |
| `CMAKE_BUILD_RPATH` | Recommended | Set to `$VG/lib` so the built binary finds VG's shared libraries at run time. |
| `PKG_CONFIG_PATH` | Recommended | Prepend `$VG/lib/pkgconfig` so pkg-config finds `sdsl-lite`, `libdivsufsort`, and `absl_*`. |
| `PANCOLLAPSE_ABSL_PKGCONFIG` | No | Optional path to an Abseil pkg-config directory when Abseil is not on the standard search path (e.g. a non-default Homebrew or Linuxbrew Cellar). Leave unset if `PKG_CONFIG_PATH` already covers Abseil. |

## Usage

```
panCollapse convert --gamp reads.gamp|- --xg graph.xg --out-dir out
                    (--path-identity-ledger path_identity_ledger.tsv |
                     --legacy-adapter hst-v1 --t2g t2g.tsv)
                    [--raw-cb-length 16] [--raw-umi-length 12]
                    [--score flat|qualadj] [--molecule-identity-failures skip|fail]
                    [--strand both|forward|reverse]
                    [--count-mode score|gene|genefull|genefull_exonoverintron|genefull_ex50pas]
                    [--body-t2g body.t2g]
                    [--bam-out reads.bam] [--bam-multigene omit|first|all]
                    [--threads N]
                    [--no-ex50-score-window]

panCollapse count --gamp reads.gamp --xg graph.xg --count-bundle bundle \
                  --barcode-whitelist barcodes.txt PROFILE_SELECTOR [PROFILE_SELECTOR ...] \
                  --out-dir counts [--t2g transcripts.tsv] [--body-t2g bodies.tsv] \
                  [--threads N] [--count-memory-budget 128GiB] \
                  [--10x-mex] [--rad-out] \
                  [--read-assignments-out diagnostics/read_assignments.parquet]

PROFILE_SELECTOR := --cr7 | --pansc-strict-v1 | --profile ID
```

`count` takes annotation identity and policy only from its checksum-verified
`panSC-count-facts-v1` bundle; it refuses legacy adapters, external ledgers/allowlists,
and BAM/debug evidence options. At least one unique profile selector is required. The
`--cr7` and `--pansc-strict-v1` aliases select immutable `cr7-v1` and
`pansc-strict-v1`; they may be combined in one GAMP pass or mixed with repeatable
`--profile ID`. Optional `--t2g` and `--body-t2g` inputs are compatibility assertions
against the bundle, not assignment inputs.

Its default output is a Parquet dataset with barcode, feature, count, and molecule tables
plus `manifest.json` and `summary.tsv`. `--10x-mex` adds per-profile MEX and
`--rad-out` adds a RAD compatibility artifact. `--read-assignments-out` adds an ordered,
compressed Parquet per-read audit sidecar beneath `--out-dir`; it must be a normalized relative
path ending in `.parquet`. It is disabled by default because it intentionally restores
per-read-scale output I/O. Native `count` never writes BAM and requires no Python at production
runtime.

Assignment-policy sensitivities use repeatable
`--profile-override BASE:FIELD=VALUE` together with the mandatory
`--analysis-scope sensitivity-analysis`. Unknown, repeated, contradictory, or incorrectly typed
overrides fail before GAMP is read. An override always produces a
`derived-<base>-<hash-prefix>` profile; the manifest retains the full effective-profile SHA-256,
so derived results are never mislabeled as a frozen profile. The definitive typed field list and
output schemas are in [`docs/input-output-contract.md`](docs/input-output-contract.md).

### Inputs

- `--gamp` — name-grouped `vg mpmap -F GAMP` multipath alignments. All records for one read
  must be contiguous. Read names must carry the raw 10x barcode and UMI as
  `<original_name>_<raw_CB>_<raw_UMI>_cy<hex(CY)>_uy<hex(UY)>` (place them there during FASTQ
  preparation, before mapping). `CY` is the raw barcode-quality string encoded as ASCII hex
  so whitespace or `_` cannot corrupt the QNAME grammar. Legacy names without `_cy...` remain
  accepted but cannot emit `CY`. Pass `-` to read the GAMP stream from stdin.
- `--xg` — the `.xg` for the same graph that produced the GAMP, carrying the `vg rna` HST paths
  (`<transcript_id>_H<n>` / `_R<n>`).
- `--path-identity-ledger` — production input using schema `panSC-path-identity-v1`. It maps each
  exact `vg_path_name` through `unique_parent` to `canonical_transcript` and `gene_id`, carries
  exon/body layer identity and graph provenance, and is checked against exact XG path names and
  lengths before any GAMP is read.
- `--legacy-adapter hst-v1 --t2g` — explicit compatibility input:
  `graph_path<TAB>gene[<TAB>canonical_transcript]`. With two columns,
  panCollapse preserves the original convention and strips a terminal `_H<n>` / `_R<n>`
  from the graph path to obtain the transcript ID. Optional column 3 explicitly aliases an
  arbitrary raw path name (for example, a CAT-projected haplotype transcript) to its canonical
  transcript. Multiple paths may alias to one transcript; one path may not alias to several
  transcripts, and one canonical transcript may not map to several genes.
  There is no file-shape auto-detection: old t2gs are rejected unless `hst-v1` is selected.

### Options

- `--raw-cb-length` / `--raw-umi-length` — lengths of the raw barcode and UMI parsed from the
  read name (defaults 16 and 12).
- `--score flat|qualadj` — per-node scoring. `flat` (default) reproduces vg's alignment scheme;
  `qualadj` reproduces vg's base-quality-adjusted scoring for exact fidelity to
  quality-adjusted mapping.
- `--molecule-identity-failures skip|fail` — how to treat reads whose name has a missing,
  malformed, or wrong-length CB/UMI/CY/UY field (default `skip`, counted in the summary).
- `--threads N` — number of initialization and complete-read-group workers (default `1`). Exact
  Parent models are built in adaptive lexical blocks; splice geometry is split by canonical target
  (or legacy gene); exact exon-edge geometry is immutable and Parent-local; and demand-built
  node/model-candidate cache misses use 256 independent shards. Initialization results commit in
  stable order through a bounded window, one parser still owns GAMP grouping and recurrence
  validation, and one ordered writer preserves original GAMP order. `N` must be positive. Exact
  inputs with fewer than 32 models automatically use one active processing worker (the requested
  ceiling is still reported), because queue handoff is measurably slower at that scale. Queued
  groups, initialization scratch, and worker scratch otherwise grow with `N`. Start with 8 or 16
  on a bounded representative slice, then increase only while measured throughput improves.
  For `convert`, supported requested thread counts produce byte-identical persisted artifacts.
  For native `count`, canonical rows and logical hashes are identical; operational manifest and
  summary fields still report the requested/active threads, timings, cache, and spill behavior.
- `--strand both|forward|reverse` — target-relative orientation filter (default `both`, no
  filtering). `forward` keeps only targets the read aligns to in the same (sense) orientation;
  `reverse` keeps only antisense targets. Reads left with no matching target emit no record and
  are counted in `strand_filtered_groups`. Orientation is the majority of aligned bases (the RAD
  `dirs`). Use `forward` for a sense-stranded library to drop antisense artifacts.
- `--bam-out <path>` — also write a BAM for a CellRanger-style counting stack (UMI-tools +
  DropletUtils `emptyDropsCellRanger`). Opt-in; the RAD is byte-identical with or without it.
  See [`docs/bam-export.md`](docs/bam-export.md).
- `--bam-multigene omit|first|all` — `XT` tag policy for multi-gene reads (default `omit`). `all` is
  a ledger-`--count-mode`-only rescue path that carries a multi-gene read into the BAM instead of
  dropping it, for a downstream UMI-level rescue; see [`docs/bam-export.md`](docs/bam-export.md).
- `--no-ex50-score-window` — disable v0.8's default exact-Ex50 top-minus-five eligibility filter
  and send every complete compatible exact traversal to downstream E/P/B-orientation ranking.
  This is an explicit pre-D068 sensitivity mode; it does not change RAD or ordinary Gene evidence.
- `--body-t2g` — `hst-v1`-only body annotation. A two-column `body_path<TAB>gene` file preserves the
  legacy gene-body/span classifier. A consistently three-column
  `raw_body_path<TAB>gene<TAB>canonical_transcript` file enables transcript-first bodies: raw
  body copies/fragments MAX-collapse to the same canonical transcript used by `--t2g`, and S/U
  is classified per transcript before count_cr groups transcripts into genes. Mixed two- and
  three-column rows are rejected.

### GeneFull / STARsolo-style counting

`scripts/make-gene-annotation.sh -x graph.xg -n annotation.gtf -o genefull -l haplotypes.gbwt`
embeds one unspliced gene-body path per gene (covering introns) alongside the HST paths, and
writes `genefull.t2g.tsv`. There are two ways to use it:

- **Coarse, no flag:** run the default count with your HST t2g for a spliced count, or with the
  gene-body t2g for a GeneFull-ish count. Same graph, the t2g selects the layer.
- **`--count-mode` for STARsolo/CellRanger rules:** `gene` (STARsolo `Gene`), `genefull`
  (STARsolo `GeneFull`), and `genefull_exonoverintron` emit a transcript-specific spliced/unspliced
  ledger (`TX`/`GL`). Production `genefull_ex50pas` instead emits exact STARsolo 2.7.11b
  Parent-preserving E/P/B overlap evidence (`TX`/`GT`/`XP`/`XU`) so a downstream counter can apply
  the global six-rank sense/antisense priority. See [`docs/genefull.md`](docs/genefull.md).

Production count modes take both exon and body rows from one `--path-identity-ledger`; they do not
take `--t2g` or `--body-t2g`. The old two-file forms remain available only through
`--legacy-adapter hst-v1`, except exact `genefull_ex50pas`, which rejects the legacy adapter.

For transcript-first counting, use the production path ledger, `--bam-out`, and
`--bam-multigene all`. Ordinary modes retain the complete `TX`/`GX`/`GD`/`GL` ledger. Exact
`genefull_ex50pas` requires all three options and emits parallel `TX`/`GX`/`GD`/`GT`/`XP`/`XU`,
with one exact path/Parent per evidence slot and repeated canonical `TX` values allowed. count_cr
performs the only priority, transcript-to-gene, and UMI reduction step. By default, exact evidence
must first be within five alignment-score points of the global compatible optimum. Pass
`--no-ex50-score-window` to restore all-compatible exact evidence for a sensitivity run. The
existing RAD Unique policy and bytes are unchanged.

### Outputs (in `--out-dir`)

- `map.rad` — mapper-style uncollated RAD (raw CB/UMI, compatible transcript targets, and
  per-target orientation), written with a streaming seek-and-backpatch writer.
- `tx2gene.tsv` — transcript-to-gene map for `alevin-fry quant`.
- `summary.tsv` — per-run counters (records, emitted groups, no-compatible / unaligned reads,
  molecule-identity skips) plus `exact_ex50_score_window` provenance (`5`, `disabled`, or
  `not_applicable`).
- `reads.bam` (only with `--bam-out`) — one record per valid input read group. Feature-bearing
  records are mapped nominally and carry 10x molecule/feature tags (`CB`/`UB`/`CY`,
  `GX`/`GN`, `TX`/`XP`/`XU` for production identity, `GL` or exact-tier `GT`, and optional `XT`). Featureless groups
  are unmapped `XB:Z:barcode_only` records with no gene tags, retained only for downstream
  barcode correction. Genes come from the graph, not a linear reference. See
  [`docs/bam-export.md`](docs/bam-export.md).

Initialization time, processing time, effective group throughput, and periodic progress are
reported on stderr only. The bounded v0.9 scaling evidence and its interpretation limits are in
[`docs/research/v090-deterministic-parallelism.md`](docs/research/v090-deterministic-parallelism.md).
For a diagnostic run, setting `PANCOLLAPSE_PROFILE_TIMING=1` also reports queue high-water,
producer admission wait, worker compute, ordered wait, and ordered-region time. These intervals
overlap across workers and must not be added as though they were wall time; leave the variable
unset for ordinary runs.
`PANCOLLAPSE_FORCE_WORKER_POOL=1` disables, for `count` only, the serial fallback that small
exact surfaces would otherwise trigger; it exists so the test suite can exercise the worker pool.

## Example

```sh
# reads.fastq read names already end in _<CB>_<UMI>
vg mpmap -n rna -x graph.spliced.xg -g graph.spliced.gcsa -d graph.spliced.dist \
         -F GAMP -f reads.fastq \
  | panCollapse convert --gamp - --xg graph.spliced.xg \
      --path-identity-ledger path_identity_ledger.tsv --out-dir out

alevin-fry generate-permit-list -i out -d fw -o pl --unfiltered-pl 3M-february-2018.txt
alevin-fry collate -i pl -r out
alevin-fry quant -i pl -m out/tx2gene.tsv -o quant -r cr-like --use-mtx
```

## Docker

The current runtime image is built locally as `josephlalli/pancollapse:v0.10.0`;
publish that tag before using it from a host that does not already have the
validated local image. It bundles the panCollapse binary with the exact
shared-library closure it was built against, so it does not need vg installed
at runtime (vg and alevin-fry remain separate tools for the surrounding
pipeline steps). Mount your inputs and an output directory:

```sh
docker run --rm \
  -v "$PWD":/work \
  josephlalli/pancollapse:v0.10.0 count \
  --gamp /work/reads.gamp --xg /work/graph.spliced.xg \
  --count-bundle /work/count-facts --barcode-whitelist /work/barcodes.txt \
  --cr7 --out-dir /work/out
```

It also reads a GAMP stream on stdin (`--gamp -`). To build the image locally after compiling
the binary, run [`scripts/build-docker-image.sh`](scripts/build-docker-image.sh), which stages
the binary, Arrow/Parquet/Zstandard closure, and tags `josephlalli/pancollapse:v0.10.0`. The
release gate refuses a missing native-output library or any `libpython` runtime dependency;
Python is not required in the image.

## How it works

For each read, across all of its GAMP alignments, panCollapse scores every aligned node under
vg's own scoring scheme and adds each node's score to the exact ledger paths crossing it. Paths
MAX-collapse within their unique Parent, Parents MAX-collapse within canonical transcript, and
canonical transcripts tied at the global top become RAD `refs`. Alternative paths/Parents are
never summed. The converter records
each transcript's orientation from the read's direction along the HST path. The full algorithm
is in [`docs/conversion-algorithm.md`](docs/conversion-algorithm.md).

## Documentation

- [`docs/conversion-algorithm.md`](docs/conversion-algorithm.md) — the conversion algorithm.
- [`docs/input-output-contract.md`](docs/input-output-contract.md) — inputs, outputs, exits.
- [`docs/product-spec.md`](docs/product-spec.md) — the product contract.
- [`docs/decisions.md`](docs/decisions.md) — design decisions and rationale.

## License

Apache License 2.0.
