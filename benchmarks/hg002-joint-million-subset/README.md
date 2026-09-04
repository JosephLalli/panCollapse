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

The frozen optimization objective makes overall eligibility-aware uniform-truth-gene
(`gene_unit_*`) F1 primary. Ordinary read-weighted precision/recall/F1 are mandatory
co-reports. Per-chromosome and all-mapped-origin fields are diagnostics.

| Scope | Gene-unit P / R / F1 | Read-weighted P / R / F1 |
|---|---|---|
| Overall | 0.963216 / 0.947364 / **0.955225** | 0.983341 / 0.949933 / **0.966348** |
| chr20 | 0.970439 / 0.971584 / 0.971011 | 0.997125 / 0.991406 / 0.994257 |
| chr21 | 0.946436 / 0.930093 / 0.938193 | 0.984482 / 0.913553 / 0.947692 |
| chr22 | 0.962005 / 0.926986 / 0.944171 | 0.966711 / 0.917230 / 0.941321 |

The read-weighted co-report has TP 945,586, FP 16,019, FN 49,838, and TN 3,322 over
995,424 eligible plus 4,576 background truth reads. Overall all-mapped-origin diagnostic
F1 is 0.964094 read-weighted and 0.932606 gene-unit.

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

The original `BENCHMARK.MANIFEST.json` is retained byte-for-byte and contains the detected
metric-label defect. `INTERPRETATION.AMENDMENT.json` and `BENCHMARK.MANIFEST.v2.json`
supersede only that hierarchy; no count or score changed. The small terminal receipts in
this directory are copies of the immutable result metadata.
The large BAM, classifications, and per-gene tables remain under the benchmark root and are
bound by hashes in `EXTERNAL.TERMINAL.SHA256SUMS`; run that manifest from the external
benchmark root. It seals 46 files, including the corrected BAM, classifications, matrices,
subset artifacts, commands, logs, timings, and both benchmark manifests.
