# panCollapse

`panCollapse` converts pangenome single-cell RNA-seq alignments from `vg mpmap` (GAMP,
multipath format) into mapper-style, uncollated RAD records that
[alevin-fry](https://github.com/COMBINE-lab/alevin-fry) quantifies into a cell-by-gene matrix.

It bridges a pangenome aligner and the standard single-cell counting workflow: for each read
it reports the set of transcripts the read is compatible with (with orientation), reading
compatibility directly off the haplotype-specific transcript (HST) paths that `vg rna`
embedded in the graph — no GTF or annotation projection at run time.

## Requirements

- A VG installation/checkout to build and link against (the tool consumes `vg mpmap` GAMP and
  a matching `.xg`). Build it, or point at an existing checkout.
- C++20 (GCC 15), CMake, and Ninja.
- [alevin-fry](https://github.com/COMBINE-lab/alevin-fry) for downstream quantification.
- Python 3 to run the test suite.

## Build

```sh
VG=/path/to/vg                      # a vg checkout/install with lib/pkgconfig
PKG_CONFIG_PATH="$VG/lib/pkgconfig" cmake -S . -B build -G Ninja \
  -DPANCOLLAPSE_VG_ROOT="$VG" \
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
| `CMAKE_PREFIX_PATH` | Recommended | Set to the same VG path so CMake finds `libhandlegraph`, `VGio`, and `Protobuf`. |
| `CMAKE_BUILD_RPATH` | Recommended | Set to `$VG/lib` so the built binary finds VG's shared libraries at run time. |
| `PKG_CONFIG_PATH` | Recommended | Prepend `$VG/lib/pkgconfig` so pkg-config finds `sdsl-lite`, `libdivsufsort`, and `absl_*`. |
| `PANCOLLAPSE_ABSL_PKGCONFIG` | No | Optional path to an Abseil pkg-config directory when Abseil is not on the standard search path (e.g. a non-default Homebrew or Linuxbrew Cellar). Leave unset if `PKG_CONFIG_PATH` already covers Abseil. |

## Usage

```
panCollapse convert --gamp reads.gamp|- --xg graph.xg --t2g t2g.tsv --out-dir out
                    [--raw-cb-length 16] [--raw-umi-length 12]
                    [--score flat|qualadj] [--molecule-identity-failures skip|fail]
                    [--gene-mode spliced|full] [--gene-loci loci.tsv]
                    [--bam-out reads.bam] [--bam-multigene omit|first]
```

### Inputs

- `--gamp` — name-grouped `vg mpmap -F GAMP` multipath alignments. All records for one read
  must be contiguous. Read names must carry the raw 10x barcode and UMI as
  `<original_name>_<raw_CB>_<raw_UMI>` (place them there during FASTQ preparation, before
  mapping). Pass `-` to read the GAMP stream from stdin.
- `--xg` — the `.xg` for the same graph that produced the GAMP, carrying the `vg rna` HST paths
  (`<transcript_id>_H<n>` / `_R<n>`).
- `--t2g` — a two-column `HST_name<TAB>gene` map (as produced alongside `vg rna`). panCollapse
  collapses HST haplotype copies to their transcript id.

### Options

- `--raw-cb-length` / `--raw-umi-length` — lengths of the raw barcode and UMI parsed from the
  read name (defaults 16 and 12).
- `--score flat|qualadj` — per-node scoring. `flat` (default) reproduces vg's alignment scheme;
  `qualadj` reproduces vg's base-quality-adjusted scoring for exact fidelity to
  quality-adjusted mapping.
- `--molecule-identity-failures skip|fail` — how to treat reads whose name has a missing,
  malformed, or wrong-length CB/UMI (default `skip`, counted in the summary).
- `--bam-out <path>` — also write a BAM for a CellRanger-style counting stack (UMI-tools +
  DropletUtils `emptyDropsCellRanger`). Opt-in; the RAD is byte-identical with or without it.
  See [`docs/bam-export.md`](docs/bam-export.md).
- `--bam-multigene omit|first` — `XT` tag policy for multi-gene reads (default `omit`).
- `--gene-mode spliced|full` — gene-calling rule (default `spliced`). `full` implements
  GeneFull (STARsolo/CellRanger include-introns): a read calls a gene if it overlaps the gene
  body (exon or intron), keeping the intronic reads spliced mode drops. Requires `--gene-loci`.
  See [`docs/genefull.md`](docs/genefull.md).
- `--gene-loci loci.tsv` — for `--gene-mode full`: a `node_id<TAB>gene[<TAB>strand]` map of each
  gene's full locus (exon + intron nodes), built at fixture time from the GTF spans and graph.

### Outputs (in `--out-dir`)

- `map.rad` — mapper-style uncollated RAD (raw CB/UMI, compatible transcript targets, and
  per-target orientation), written with a streaming seek-and-backpatch writer.
- `tx2gene.tsv` — transcript-to-gene map for `alevin-fry quant`.
- `summary.tsv` — per-run counters (records, emitted groups, no-compatible / unaligned reads,
  molecule-identity skips).
- `reads.bam` (only with `--bam-out`) — one mapped record per emitted read carrying 10x tags
  (`CB`/`UB`/`GX`/`GN`, and `XT` for `umi_tools --gene-tag`), for a CellRanger-style counter.
  Positions are nominal; genes come from the graph, not a linear reference. See
  [`docs/bam-export.md`](docs/bam-export.md).

## Example

```sh
# reads.fastq read names already end in _<CB>_<UMI>
vg mpmap -n rna -x graph.spliced.xg -g graph.spliced.gcsa -d graph.spliced.dist \
         -F GAMP -f reads.fastq \
  | panCollapse convert --gamp - --xg graph.spliced.xg --t2g t2g.tsv --out-dir out

alevin-fry generate-permit-list -i out -d fw -o pl --unfiltered-pl 3M-february-2018.txt
alevin-fry collate -i pl -r out
alevin-fry quant -i pl -m out/tx2gene.tsv -o quant -r cr-like --use-mtx
```

## Docker

A runtime image is published as `josephlalli/pancollapse:v0.2`. It bundles the panCollapse
binary with the exact shared-library closure it was built against, so it does not need vg
installed at runtime (vg and alevin-fry remain separate tools for the surrounding pipeline
steps). Mount your inputs and an output directory:

```sh
docker run --rm \
  -v "$PWD":/work \
  josephlalli/pancollapse:v0.2 convert \
  --gamp /work/reads.gamp --xg /work/graph.spliced.xg \
  --t2g /work/t2g.tsv --out-dir /work/out
```

It also reads a GAMP stream on stdin (`--gamp -`). To build the image locally after compiling
the binary, run [`scripts/build-docker-image.sh`](scripts/build-docker-image.sh), which stages
the binary and its library closure and tags `josephlalli/pancollapse:v0.2`.

## How it works

For each read, across all of its GAMP alignments, panCollapse scores every aligned node under
vg's own scoring scheme, adds each node's score to the HST paths crossing it, keeps the HSTs
tied at the top score, collapses them to unique transcript ids (RAD `refs`), and records each
transcript's orientation from the read's direction along the HST path. The full algorithm is
in [`docs/conversion-algorithm.md`](docs/conversion-algorithm.md).

## Documentation

- [`docs/conversion-algorithm.md`](docs/conversion-algorithm.md) — the conversion algorithm.
- [`docs/input-output-contract.md`](docs/input-output-contract.md) — inputs, outputs, exits.
- [`docs/product-spec.md`](docs/product-spec.md) — the product contract.
- [`docs/decisions.md`](docs/decisions.md) — design decisions and rationale.

## License

Apache License 2.0.
