# Phase 1 RAD Interoperability Fixture

This fixture defines the smallest alevin-fry interoperability target for V1 RAD output.
It is a contract for the future writer and fixture generator; no binary RAD fixture is
checked in during Phase 1.

Supersession note: D039 narrows Phase 2 RAD interoperability to one single-threaded
1-cell x 1-gene vertical slice. D045 further defers panCollapse-side multithreading; any
future supported execution modes, including stdin GAMP streaming if approved, must get
byte-comparison acceptance criteria before implementation.

Supersession note: D042 removes synthetic all-forward orientation as an active V1 policy.
This tiny fixture may still use forward target-relative mappings, but future orientation
fixtures must also cover reverse `dirs`.

## RAD Input Directory

Directory: `rad-in/`

Files:

- `map.rad`
- `tx2gene.tsv`

Target dictionary in `map.rad`, lexicographic canonical transcript order:

| ref_id | transcript |
|---:|---|
| 0 | `TX_A` |
| 1 | `TX_B` |
| 2 | `TX_C` |

`tx2gene.tsv` has no header:

```tsv
TX_A	GENE_1
TX_B	GENE_1
TX_C	GENE_2
```

## RAD Records

Classic mapper-style uncollated `RnaShort`:

- `is_paired=0`
- `cblen=16`
- `ulen=10`
- header `num_chunks=0` to exercise streaming unknown-count RAD output
- read tags `b:U32` and `u:U32`
- alignment tag `compressed_ori_refid:U32`
- one chunk with `nrec=3`
- all retained hits in this fixture encoded as target-relative forward hits with
  `ref_id | 0x80000000`

Conceptually, every record carries `bc`, `umi`, `refs`, and `dirs`. The `refs` values are
zero-based IDs into the RAD header target dictionary, and `dirs` is the parallel
orientation vector decoded from the high bit of each `compressed_ori_refid`. This fixture
uses forward `dirs`; it is not a synthetic substitute for panCollapse-side strand
filtering.

| record | CB | packed CB | UMI | packed UMI | target refs | compressed refs |
|---|---|---:|---|---:|---|---|
| 1 | `ACGTACGTACGTACGT` | `0x1B1B1B1B` | `AAAAAAAAAA` | `0x00000000` | `TX_A` | `0x80000000` |
| 2 | `ACGTACGTACGTACGT` | `0x1B1B1B1B` | `AAAAAAAAAC` | `0x00000001` | `TX_A`, `TX_B` | `0x80000000`, `0x80000001` |
| 3 | `ACGTACGTACGTACGT` | `0x1B1B1B1B` | `AAAAAAAAAG` | `0x00000002` | `TX_C` | `0x80000002` |

Target IDs inside each record must be sorted and deduplicated. The fixture must contain
no USA/splicing-state labels and no three-column USA `tx2gene` mapping.

## Alevin-Fry Commands

Baseline: `alevin-fry 0.15.0`.

```bash
rm -rf af_permit af_quant
alevin-fry generate-permit-list -i rad-in -d fw -o af_permit -f 1 -t 1
alevin-fry collate -i af_permit -r rad-in -t 1
alevin-fry quant -i af_permit -m rad-in/tx2gene.tsv -o af_quant -r cr-like -t 1 --dump-eqclasses
```

## Expected Matrix

Rows from `af_quant/alevin/quants_mat_rows.txt`:

```text
ACGTACGTACGTACGT
```

Columns from `af_quant/alevin/quants_mat_cols.txt`:

```text
GENE_1
GENE_2
```

Logical cell-by-gene matrix:

| cell | `GENE_1` | `GENE_2` |
|---|---:|---:|
| `ACGTACGTACGTACGT` | 2 | 1 |

Matrix Market content must have shape `1 x 2`, `nnz=2`, `(1,1)=2`, and `(1,2)=1`.
Column names must not contain `-U`, `-S`, or `-A` state suffixes.

## Required Assertions

- `generate-permit-list`, `collate`, and `quant` all complete successfully.
- `tx2gene.tsv` covers every transcript in the RAD header.
- Quantification emits gene-level rows only, with no splicing-state labels.
- Repeated panCollapse runs in the active single-threaded mode must produce
  byte-identical RAD and companion artifacts. Any future supported execution modes must
  satisfy the same byte-identical output requirement before implementation.
