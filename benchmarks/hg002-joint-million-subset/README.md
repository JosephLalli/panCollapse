# HG002 joint chr20-22 one-million-read benchmark

This branch-only study evaluates gene assignment from the PanCollapse 0.9.0 joint
chr20/21/22 producer. It is deliberately separated from `main`: product speedups end at
`5f4daffff17e503c63bb0d58dbd0e779573621a0`, while selection, downstream correction,
counting, scoring, and their receipts live only on
`benchmark/hg002-joint-million-subset`.

## Release and branch boundary

- PanCollapse runtime source: `b128919293f11f5d1528d9bd8feadde806717582`
  (`v0.9.0`).
- PanCollapse release/performance receipt: `5f4daffff17e503c63bb0d58dbd0e779573621a0`
  (`main` and this branch's base).
- Runtime image ID:
  `sha256:3886de3476a9ed0553ac49147bccf61740f68dacd8603d4c5c283a0185baa211`.
- Benchmark root:
  `/mnt/ssd/lalli/hg002_chr20_chr21_chr22_pancollapse_v090_gene_assignment_1m_v1_20260904T171055Z`.

The benchmark implementation was transplanted from the frozen panSC evaluation branch
without importing its unrelated history:

| Purpose | panSC commit | This branch |
|---|---|---|
| Assignment-blind subsetter | `dfd59a7` | `8200bd2` |
| Frozen benchmark runner | `dd69682` | `e23f3fb` |
| Selected-only correction | `6c0ef65` | `4782ad8` |
| HTSlib QNAME filtering | `56dc0ea` | `78ced46` |
| Parallel BGZF output | `4786d02` | `2ab220a` |
| Validated wrapper-failure resume | `083088d` | `424dcc5` |

## Sampling and evaluation contract

The subset is the global bottom 1,000,000 canonical R2 QNAMEs under
`SHA256(seed + NUL + QNAME)`, using seed
`pansc-hg002-joint-v090-gene-assignment-1m-v1`. Selection did not inspect BAM assignment
fields. Realized truth composition was 448,595 chr20, 151,579 chr21, and 399,826 chr22
reads, totaling exactly 1,000,000 mapped truth reads and 1,024 represented truth genes.

Barcode correction retained the frozen full-data prior over all 11,881,577 producer BAM
records. It emitted 999,759 selected records; 220 uncorrectable and 21 barcode-only reads
were not emitted. Scoring retained the selected FASTQ as the denominator, so missing
classifications count as false negatives rather than disappearing.

The primary metric is eligibility-aware open-set, read-weighted F1. The all-mapped-origin
and gene-balanced fields are diagnostics.

| Scope | Truth reads | Precision | Recall | F1 |
|---|---:|---:|---:|---:|
| Overall | 1,000,000 | 0.983341393 | 0.949932893 | **0.966348480** |
| chr20 | 448,595 | 0.997124837 | 0.991406059 | 0.994257225 |
| chr21 | 151,579 | 0.984481874 | 0.913553147 | 0.947692223 |
| chr22 | 399,826 | 0.966710999 | 0.917230163 | 0.941320785 |

Overall primary counts are TP 945,586, FP 16,019, FN 49,838, and TN 3,322 over
995,424 eligible plus 4,576 background truth reads. Overall all-mapped-origin diagnostic
F1 is 0.964094198; represented eligible-gene-balanced diagnostic F1 is 0.955224516.

This is an authorized partial unblinding, not the completed full final test. It evaluates
the frozen integrated annotation/filter/counting policy on a deterministic read sample; it
does not isolate the causal effect of PanCollapse's speedups, measure PanCollapse conversion
runtime, or establish whole-genome accuracy. Genes absent from the selected reads are outside
the represented-gene diagnostic denominator.

## Tracked tooling and external boundary

- `scripts/subset_bam_by_fastq_hash.py` selects the deterministic subset.
- `scripts/correct_cb_selected.py` applies the full-data barcode prior while emitting only
  selected reads.
- `scripts/run_hg002_joint_v090_one_million_assignment_benchmark.sh` binds immutable inputs,
  frozen downstream policies, validation, and scoring.
- `tests/test_subset_bam_by_fastq_hash.py` checks selection determinism, correction parity,
  and preservation of the full-data prior.

The runner intentionally references checksum-pinned panSC-owned `correct_cb.py`,
`count_cr.py`, scoring code, ledgers, truth, and annotation artifacts. Those downstream
policy implementations are not vendored into PanCollapse. Set
`PANCOLLAPSE_BENCHMARK_CORRECT_CB` to the checksum-identical frozen correction oracle when
running its cross-repository integration test elsewhere; that one test skips if the oracle
is unavailable.

The small terminal receipts in this directory are copies of the immutable result metadata.
The large BAM, classifications, and per-gene tables remain under the benchmark root and are
bound by hashes in `BENCHMARK.MANIFEST.json` and `score.MANIFEST.json`.
