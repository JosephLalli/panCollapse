# Direct-index performance review — 2026-11-01

Twelve independent read-only reviews examined the direct count path. This is a
design evidence record, not an implementation authorization. Controls: accepted
numeric/caller 1M receipts remain under
`/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_native_query_count_20261031_LkMGA0/`;
R4 is `/mnt/ssd/lalli/hprc_v2_vg_rna/whole_genome_runs/pancollapse_compact_direct_facts_20261101_FAuuqd/`.
The full objective remains 66,601,887 groups <2h and <300e9 B.

1. **Cache keys.** Canonicalize order and exact duplicate records only after the
   existing complexity cap, contingent on proof and measured cost/reuse. Starts
   or adjacency normalization is conditional; arbitrary subpath splitting is
   unsafe.
2. **Posting decode.** `node_score_resolver.cpp:143-155` builds local models,
   then `323-335` repeats membership work per record; `227-245` rescans with
   `any_of`. Reuse read-local descriptors and `has_annotation`, not a global
   expanded cache.
3. **Cache storage.** `bounded_sharded_cache.hpp:60-79` builds before publish;
   racing losers count as hits. Measure actual avoided builds before claiming
   reuse; generous capacity is justified only by marginal reuse.
4. **Reference memory.** Main arrays are mapped, while strings/sets are copied.
   Models are about 16 B plus 4-B support per 95M (~1.9 GB). Account resident
   memory and full file size together: the current plan conservatively counts
   already-faulted mapped pages twice. Do not weaken admission absent proof.
5. **Resolver hot path.** Replace tree `Delta` mapping at `node_score_resolver.cpp:357`
   with indexed reusable scratch/touched IDs; `407/451` allocate per-group
   vectors/frontiers across 12.1M groups in the 1M fixture. Preserve canonical
   order/exact rules; SIMD is not first.
6. **GAMP ingest.** `prepared_count.cpp:419-425` has unbounded `completed_names`
   (~5.3 GB before QNAME bytes at 66.6M). Preserve recurrence detection unless a
   valid contract permits change; measure serial wire peek/fallback/decode totals.
7. **UMI reuse.** Keep global graph cache, not a one-family representative:
   37,856 mixed-signature families (5,447 after sameSeq); exact family+signature
   reuse 60,048/999,018 (6.01%) versus global 289,420/999,018 (28.97%). Only
   exact multiplicity batches with full signature/raw CB/UMI/canonical CB quality
   are candidates; every group retains prior, cap, and terminal behavior.
8. **Reference factorization.** Flag-partitioned model sets/interned bitmap
   containers may be an exact sequential V2 transform, gain unmeasured. Global
   classes require identical node flags and every oriented splice-veto mask;
   aliases retain policy/support. SAMEGENEONLY union is unsafe: two 50%-exonic
   models can become a 100%-tier model. No ordered-reference-walk requirement is
   implied.
9. **Locality.** A bounded dispatch window keyed by coarse first-node/list could
   improve pages but not necessarily assignment hits. Do not adopt the 23-GB
   sort file (multiset unproven) or affinity changes without evidence.
10. **Index build.** `TempFile::append` pwrite-per-list (`513-540`) is consistent
    with the observed 47.7M write calls/152.8GB. A bounded buffer is low-risk
    future build work. V2Writer
    already writes V2 directly; no raw-1TB future intermediate is needed, though
    sealed node facts remain needed for the edge pass. Do not remove deep
    validation/copy serialization without a failure-boundary decision.
11. **Budget.** Required rate is 9,250.3 groups/s excluding overhead. P was
    2,349.1/s: removing 59.3119% RI CPU gives an optimistic 2.458x/5,773/s;
    RI+splice 65.1245% gives 2.868x/6,737/s. These are optimistic Amdahl estimates
    assuming sampled CPU fractions translate to wall time, not measured forecasts
    or universal ceilings. The old profile does not support promising the target
    from indexing alone.
12. **Alternates.** Grammar-compressed recurring model-ID subsequences, exact
    complement postings, uniformity tiles/reference certificates,
    cardinality-first ordering, bit-sliced automata/semiring, and AVX-wide
    batching are ideas only. Corpus-wide GAMP prepass/scoring IR/differential
    programs are de-prioritized; hardware offload requires new exactness work.

## Synthesis and gate

Benchmark unchanged R4 first. If insufficient, prioritize read-local reuse plus
allocation removal, then conditionally cache-key canonicalization; structural
bitmap/grammar sharing is fallback work. No review changed biology, count rules,
or source. At 11:55:16Z R4 was live with no final `direct.pci`/count; postings
161,711,487,246 B were unchanged since 11:49:02Z (phase unknown), so no final
size or speed claim. R6 guard build exited 0 but focused/full tests and freeze
remain pending; R4/R5 are untouched. Root reviewed and accepted this priority
order, not a measured speedup or automatic adoption of the proposals. Further
engineering must remain within the existing authorized optimization scope and
pass exact parity/resource gates; biological-policy changes require user approval.

## Measured follow-up — 2026-11-01 terminal audit 17:03:05Z

All twelve reviews remain completed; implemented work is limited to R7 read-local
posting reuse and separately authorized R6 lazy-winner/memory guard. R6 full is
terminally validated: native 7,305.22s (105.22s over two hours), kernel HWM
240,016,211,968B, independent Parquet/MEX validation passed. Its retained receipt
is `pancollapse_memory_guard_20261101_O70tyk/full-terminal.json`.

Frozen R7 full terminal audit (17:03:05Z) validates objective achievement:
container/native exit 0; native 6,372.41s, HWM 245,153,902,592B, count-memcg
128,493,842,432B, zero max/OOM/oomkill/swap/spill, original 66,601,887 groups/
166,824,266 records EOF, and independent Parquet/MEX plus exact R6 four-Parquet/
six-MEX byte/decoded/logical/profile/prior parity all pass. R7's retained 1M receipt
`pancollapse_posting_reuse_20261101_nf6RG9/first1000000-terminal.json` records
123.64s/native read 107.214604235s/9,327.087547 groups/s, HWM 81,771,417,600B,
and exact N four-Parquet/six-MEX/logical/profile/prior parity; full 174 tests
passed in 32.09s. All twelve review findings remain bounded: only R7 read-local
reuse and separately authorized R6 work were implemented; alternate proposals
remain unadopted. This result is not F1, biological truth, external CR7 whole-
workflow equivalence, release, or isolated causal-speedup proof.
