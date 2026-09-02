# Validation Contract

Validation is organized around observable behavior. The GAMP-to-RAD algorithm is defined by
D048 and `docs/conversion-algorithm.md`.

## 1. Workspace and planning gates

- external API claims have primary-source evidence;
- Phase 0 research briefs are complete;
- fixture expectations exist before broad implementation.

## 2. Barcode and UMI cases

- RNA GAMP name follows
  `<original_read_name>_<raw_CB>_<raw_UMI>_cy<hex(CY)>_uy<hex(UY)>`; CB/UMI and
  optional quality suffixes are parsed from the right (the original name may contain
  underscores). Legacy quality-free and CY-only names remain accepted;
- decoded `CY`/`UY` is printable and exactly parallel to CB/UMI; malformed hex/length is a molecule
  identity failure;
- extracted values are written to RAD without correction;
- parsed values must match the configured `--raw-cb-length` / `--raw-umi-length` (Phase 2
  defaults 16 and 12);
- panCollapse does not permit-list, correct, or deduplicate; alevin-fry does that;
- `--molecule-identity-failures skip|fail` defaults to `skip`; missing, malformed,
  unsupported, or length-mismatched values are skipped and counted by default and fatal
  under `fail`.

## 3. Grouping cases

- one record for one read; several adjacent records with the same name; next read begins
  normally;
- a completed read name recurs later: hard failure;
- empty input and truncated/corrupt stream: defined behavior.

## 4. Scoring cases

- per-node scores reproduce vg's scheme: on real GAMP subpaths, the sum of a subpath's
  per-node scores equals `Subpath.score`;
- a match/mismatch/gap mix on one node scores as vg does (match +1/base, mismatch -4/base,
  affine gaps open 6 / extend 1, read-end bonus +5);
- an affine gap spanning a node boundary is charged gap-open once;
- base-quality-adjusted input is detectable when the flat per-node sum does not equal
  `Subpath.score`.

## 5. Compatibility and collapse cases

- production accepts only schema `panSC-path-identity-v1` through
  `--path-identity-ledger`; old t2gs require explicit `--legacy-adapter hst-v1`, and supplying a
  t2g without that adapter fails;
- a production ledger count mode with `--bam-out` requires every exon Parent to have at least one
  linked body row before graph traversal; score mode and non-BAM RAD conversion retain the general
  ledger contract;
- the strict reader requires the exact canonical header and column order, then checks every
  required provenance field, numeric ordered coordinates and path
  length, unique path identity, one annotation identity and layer per Parent, canonical-to-one-gene,
  exon self-links, and matching body-to-exon crosslinks;
- exact path names, Parents, and canonical transcripts include literal `_R1` strings unchanged;
  hashed `panSCup1_<64 lowercase hex>` exon Parents and `panSCbody1_<64 lowercase hex>` body
  Parents also survive byte-for-byte into BAM `XU`;
  every ledger path exists in the XG at the recorded length, while missing paths and mismatched
  lengths fail before GAMP processing;
- tabs/newlines fail in every field; comma/semicolon fail where reserved by `XP`/`XU` grouping,
  while commas in general provenance such as `vg_haplotype_origins` remain legal;
- several exact paths may share one Parent, and several Parents may share one canonical transcript;
  path→Parent→canonical uses MAX at both boundaries and retains all tied winners;
- sequence-identical paths with different explicit canonical identities remain distinct targets;
- a read whose aligned nodes lie on one HST path is compatible with that transcript;
- a read whose aligned nodes lie on no HST path emits no target and is counted;
- a node shared by two isoforms' HST paths makes the read compatible with both;
- several HST paths of one transcript (haplotype copies) collapse to one transcript ID and
  do not inflate the transcript's score;
- optional t2g column 3 can map arbitrary raw CAT path names to one canonical transcript;
  raw top-score paths are selected before alias collapse, so two tied copies do not gain a
  summed score and displace another tied transcript;
- three-column aliases match exact raw graph paths only and cannot activate the legacy
  suffix-stripped ledger fallback; only canonical targets introduced by two-column rows may do so;
- conflicting raw-path-to-transcript aliases and conflicting canonical-transcript-to-gene
  mappings are hard failures;
- a three-column body t2g MAX-collapses raw body copies/fragments to canonical transcripts,
  rejects mixed row widths, rejects body transcripts absent from the exon t2g, and rejects
  exon/body gene disagreement or one raw graph path appearing in both layers;
- transcript-body ledger classification calls a near-top exon score `S`, otherwise a near-top
  body score for that same transcript `U`; another isoform cannot inherit body evidence;
- transcript-specific splice ownership compares each canonical transcript's exon paths only
  with its own body paths, not a pooled gene body; path-internal steps and endpoint evidence across
  body fragments establish ownership, while any adjacent endpoint occurrence vetoes the splice;
  load diagnostics distinguish all owned target-edges from fragment-only and adjacent-vetoed cases;
- the winners are the HSTs tied at the single top score pooled across all of the read's
  alignments; lower-scoring HSTs are not emitted;
- a read with a supplementary alignment contributes its winning HSTs to the same pooled
  top-score selection.

## 6. Orientation cases

- read running along an HST path: RAD `dirs` records forward;
- read running opposite: RAD `dirs` records reverse;
- a transcript whose winning HSTs disagree: the majority of aligned bases decides, with a
  deterministic forward fallback for an exact tie.

## 7. RAD interoperability

A tiny fixture must prove:

1. panCollapse output is accepted by the supported alevin-fry version;
2. permit-list generation and collation complete;
3. quantification with the two-column t2g completes;
4. the final cell-by-gene matrix matches expected UMI counts;
5. RAD `cblen`/`ulen` match the configured lengths (Phase 2: 16, 12);
6. no USA/splicing-state rows;
7. Phase 2 uses one thread; D045 defers multithreading; any future mode must still prove
   byte-identical output;
8. streaming-to-disk framing (chunk headers and file-level `num_chunks` seek-and-backpatched
   to their final values, D049) decodes through the supported libradicl/alevin-fry path,
   including a forced multi-chunk split.

Production BAM fixtures additionally prove `TX` is sorted and `XP`/`XU` are semicolon groups
parallel to `TX`, with comma-sorted tied paths/Parents inside a group. This applies to production
score and ledger count modes; score-mode `GX`/`GD` remain sorted gene-level fields. The explicit
`hst-v1` suite proves the legacy feature-tag layout remains unchanged. BAM conservation tests also
prove exactly one record per valid molecule group: feature records are mapped, while no-compatible,
strand-filtered, and policy-omitted groups are unmapped `XB:Z:barcode_only` records with molecule
tags (including decoded `CY`/`UY` when present) and no feature tags.
Exact-Ex50 fixtures require the typed-union schema marker plus either
`@CO\tpanCollapse-ex50-score-window:5` or `:disabled`. The default proves inclusive top-minus-five
versus exclusive top-minus-six selection both within a MultipathAlignment DAG and across records
in one read group. The opt-out must restore both top-minus-six alternatives, reject use outside a
production-ledger count-mode BAM, and leave RAD byte-identical.

## 8. Failure and diagnostics

Each hard failure has a stable nonzero exit status and actionable message. Summary counts
are independently checkable against fixtures.

## 9. Production-applicability fixture

- `docs/testing_fixture_creation.md` is the human-pangenome GAMP-to-RAD fixture plan (D047):
  slice the HPRC v1.1 GRCh38 graph over the MHC region, build the spliced/HST graph with
  `vg rna`, simulate reads, map to GAMP, and compare panCollapse RAD against an independent
  GAMP-driven oracle that re-implements the D048 algorithm;
- record throughput, peak memory, and target-set size distribution;
- evaluate direct `vg mpmap` to panCollapse stdin streaming while keeping RAD output on the
  streaming-to-disk path;
- do not add a custom index unless the measured lookup cost is material and a human approves.
