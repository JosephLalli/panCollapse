# Read-Granular Decode: keeping dense graph regions from stalling native `count`

Status: **implemented for native `count`, including `--rad-out` and the compatibility writer,
and measured (2026-09-23); accepted as D090.** The design below is kept as written; where the
measurement contradicted it, the "Measured result" section at the end says so. `convert` still
decodes on its producer thread and commits through the output turn.

## Why this was needed

After input read about 22,046,000 the full-input GAMP shifts to alignments that carry far more
graph structure: subpaths per alignment move from p50 1, p90 1, p99 7 (`w_precliff`) to p50 6,
p90 261, p99 15,295 (`w_cliff`), and mean mappings per alignment from 8.67 to 5,164.95
(`docs/research/full-input-throughput-cliff-20260921.md`, alignment-shape table). The mapping
pipeline owner has confirmed (2026-09-23) that the upstream cause is not known, and that some
regions of the genome will always produce alignments of this kind. The earlier plan treated the
question "why is the tail heavy" as possibly mooting optimization; it does not. Dense regions are
permanent workload, and the question is how `count` processes them without the rest of the input
waiting behind them.

The measured slowdown has two separable parts. Across the cliff, wall time per read scales 73.8x
while total CPU per read scales 29.9x. The 29.9x is work; the 2.5x gap between them is threads
waiting, not computing. The two need different fixes and are kept separate below.

## What was examined

Nothing was run for this proposal. It rests on sealed measurement roots and a read-only code
audit of the current worktree:

- `pancollapse_cliff_characterization_v1_20260919T194550Z` — window shapes, the wall/CPU gap.
- `pancollapse_producer_partition_v1_20260920T220538Z` — of a 973.1 s read phase, 910.3 s
  (93.5%) is the producer blocked on the oldest in-flight batch finishing its decode.
- `pancollapse_inflight_depth_v1_20260920T181428Z` — slots 8 to 256 gave no gain; idle 65.11%
  and 65.09% in the two depth-8 controls; peak buffered bytes 608–678 MB in every arm.
- `pancollapse_spaceused_fix_v2_20260921T043810Z` — accepted decoder fix, committed as `2b5e466`.
- Code: `src/ordered_gamp_reader.hpp` (committed `2ce20c9`), `src/main.cpp` (uncommitted),
  `src/direct_count_runtime.cpp`, `src/direct_count_diagnostics.cpp`, `src/node_tally_cache.hpp`,
  `src/pathtally_score.hpp`, and `MultipathAlignment` in
  `/mnt/ssd/lalli/usr/local/src/vg/deps/libvgio/deps/vg.proto`.

## Findings

### Where the waiting comes from

Only the input reader imposes order. Its commit is in order because the consumer callback groups
records into reads by QNAME run and assigns read ordinals (`src/main.cpp:6317-6335`, recurrence
check `:4894-4901`). After that, reads go into a FIFO job deque of 2 x workers (`:6180-6187`) and
any worker takes the next one (`:6200-6230`), so counting is already unordered. The count tables
are `uint64` sums into sharded maps, sorted at finalize (`src/direct_count_runtime.cpp:1241-1271`,
`:413-421`); the one floating-point step, `correct_barcode` (`:878-895`), runs after all workers
join, on integer priors. Plain native `count` therefore does not depend on processing order
anywhere after grouping.

The reader's unit of decode is a batch, not a read. A decoder takes a whole batch and decodes it
serially on one thread (`src/ordered_gamp_reader.hpp:326`, `:345-393`); batches seal at 1 MiB of
raw payload or 512 records (`:607-610`). The producer only ever waits on the oldest batch
(`:513-514`).

Admission is bounded by `max_buffer_bytes` (64 MiB default, `:40`), compared against raw plus
decoded bytes at the moment a record is admitted (`:556-588`). Decoded bytes then keep growing as
admitted batches decode. On `w_cliff` a batch holds about 27.5 records — about 1 MiB of
uncompressed serialized payload, the batch seal — and decodes to about 29 MB (about 1.055 MB
decoded per record), roughly 28 decoded bytes per serialized byte. (The 5,972 bytes per alignment
in the cliff document's shape table is on-disk BGZF size, not serialized size.) The 64 MiB
watermark therefore admits about two batches before decoding already-admitted work pushes it over,
which is how every depth-trial arm peaked at 608–678 MB. From there the producer can only commit
the oldest batch, so at most a couple of decoders have work while the rest idle, and the 24 count
workers starve behind them. That matches the 93.5% producer wait and the 65% idle, and it explains
why adding slots did nothing: bytes, not slots, were the limit.

### Where the work comes from

Decode CPU per alignment rises 408x against 596x more mappings; the post-cliff profile puts
allocation at 38% of all CPU (decoder threads 59.3% allocator, count threads 30.7%). The count
stage traverses every mapping three times per read — node tally, node-pair collection, and the
exact Ex50 DP over the subpath DAG (`src/main.cpp:4683-4727`, `:1971-1999`, `:4274-4660`) — and
builds per-read `std::map`/`std::set` structures (`:5282-5290`). Counting scales only 23.1x per
read, because the exact-Ex50 prefilter (`:4488-4545`) cuts candidates 10.1x post-cliff.

The count path reads only these `MultipathAlignment` fields: `name`, `sequence` (length only),
`start`, and within `subpath` the `path` mappings (`position.{node_id, offset, is_reverse}`,
`edit.{from_length, to_length}`, and whether `edit.sequence` is empty), `next`, `score` and
`connection.{next, score}`. `quality`, `sample_name`, `read_group`, `mapping_quality`,
`paired_read_name` and `annotation` are never read in the default scorer (QualAdj and BAM export
aside).

## Proposed design: make the read the unit of parallelism

**Group on raw bytes at the producer.** In `vg.proto`, `MultipathAlignment.name` is field 3,
after `sequence` (1) and `quality` (2) and before `subpath` (6). The C++ serializer writes fields
in number order, so the producer recovers a record's name by reading at most three
length-delimited tags, without touching any subpath data. A full top-level tag scan is the
fallback if a record is ever not in canonical order. The producer frames records, forms QNAME
runs, assigns ordinals and enforces the no-recurrence check exactly as today, then enqueues each
read as its raw serialized records.

**Fuse decode into the count worker.** A worker takes the next raw read from the queue, decodes
its records, counts it, and discards the decoded objects. A dense read occupies one worker for
longer while every other worker continues on the reads behind it. No thread ever waits for a
specific earlier read to finish.

**Consequences.**

- The in-order batch commit, the separate decoder pool and its fixed 4-decoder/24-worker split
  go away. At `--threads 32`, all 28 non-I/O threads both decode and count, so the split can no
  longer be wrong for the regime the input is in.
- The per-record `SpaceUsedLong` accounting, now sampled, can go entirely: decoded objects never
  sit in a queue, so there is nothing decoded to budget. `max_buffer_bytes` bounds raw bytes in
  the queue only. Peak memory becomes raw queue bytes plus, per worker, one decoded read, instead
  of a watermark that decoded growth overshoots tenfold.
- **Arena allocation becomes simple.** The earlier objection — a cross-arena move deep-copies, so
  `Group.records` (`src/main.cpp:2010`, moved at `:6334`) would have to become pointers plus a
  `shared_ptr<Arena>` spanning every read drawn from one batch — was an artifact of decoding in a
  shared batch on another thread. With decode on the worker, each worker owns one arena, decodes
  one read into it, and resets it when the read is counted. That targets the decoder threads'
  59.3% allocator share without any cross-thread lifetime.
- Reads above the current oversized threshold keep their isolation rule, but it no longer has to
  drain the whole pipeline (`:575-579`); an oversized read is simply one large queue entry.

**Ordering, mode by mode.**

- Plain native `count`: no ordered consumer; the design applies directly.
- `--read-assignments`: each worker's spool requires nondecreasing ordinals
  (`src/direct_count_diagnostics.cpp:554-555`) and the merge requires strict (ordinal, profile)
  order (`:461-481`). A single FIFO queue fed in ordinal order preserves both, as it does today.
- `--rad-out` and the compatibility writer: `OrderedOutputCoordinator`
  (`src/main.cpp:5066-5069`, `:6137-6138`) makes every finished read wait its turn, so one dense
  read parks each worker that finishes a later one, and the pool stalls about 24 reads behind it.
  This design does not remove that; it is left open below.

**Stopgap before the redesign.** `max_buffer_bytes` has no knob today. Exposing it as an
environment variable and running 64 MiB (control), 512 MiB and 2 GiB on `w_cliff`, with a
bracketing repeat control, would measure how much of the idle time a larger watermark recovers
with a one-line change. It cannot remove head-of-line blocking, because commit stays in order and
one thread still decodes each batch, and it raises peak memory roughly in proportion.

## Critique and limits

The strongest objection is that this design saves waiting, not work. It moves decode CPU from the
decoder pool onto the workers; total CPU is unchanged. The bound on the gain is the measured
wall/CPU gap: across the cliff, wall scaled 73.8x against CPU 29.9x, so removing all waiting could
lower wall time on dense input by at most about 2.5x. That figure comes from the pre-fix binary
and from scaling ratios, not from a run of this design. It is a ceiling, not a prediction. The
29.9x of real work remains, and arena allocation, count-side allocation and the three traversals
per mapping are the follow-ons that address it.

Three further limits. First, when the dense region is the last thing in the input, a few very
large reads can leave most workers idle at the end; the p99 of 15,295 subpaths per alignment
suggests single reads large enough for that to matter. Splitting such reads across workers is
possible, because per-alignment evidence merges by `insert` plus `sort_best_evidence` independent
of order (`src/main.cpp:5390-5395`) and DP states never change model (`:4395`), but the DP
resolves nodes through a worker-owned cache (`:5079-5081`). Build that only if a per-read latency
histogram shows the tail. Second, the assignment-cache hit, miss and uncached counters
(`:6640-6643`) will change because which keys fill the 131,072-entry insert-only cache depends on
arrival order; counts do not. Third, the `--rad-out` stall above is unchanged.

One semantic observation, recorded and not acted on: node scores are summed across all subpaths
and all alignments of a read, including mutually exclusive branches of the subpath DAG, so on a
690-subpath alignment a transcript crossing more branches accrues more score. Whether that is the
intended reading of D048 on DAGs this dense is a question for the user, not a performance change.

## Verification the implementation would need

- All four count tables byte-identical to the frozen baseline `6eb3e7ca…d6843a` on both `w_cliff`
  and `w_precliff`, the second guarding the cheap regime against regression.
- Non-cache counters equal, including read and alignment totals and the recurrence check. Counters
  expected to change and to be named in the receipt: assignment-cache hit/miss/uncached, and the
  reader's batch, `decoded_bytes`, `budget_oversize_records` and peak-buffer statistics, which lose
  their meaning.
- `--read-assignments` output identical on a small fixture, proving the spool ordering holds.
- Compare on idle% and CPU-seconds with a bracketing repeat control, not raw reads/s; host load
  varied from 5.7 to 110 across earlier arms.

## Decision needed

Whether to implement Read-Granular Decode for plain native `count` only, leaving the ordered
output modes on the current reader, or to also design a bounded reorder stage for `--rad-out` and
the compatibility writer. And whether to run the `max_buffer_bytes` stopgap trial first as a
measured floor, or go straight to the redesign.

## Measured result (2026-09-23)

Evidence root: `/mnt/ssd/lalli/pancollapse_read_granular_v1_20260923T160346Z` (`RESULTS.md`,
`METRICS.json`). One binary ran both input paths, selected by `PANCOLLAPSE_INPUT_MODE`
(`read_granular` default, `ordered_batch` for A/B), in alternating arms on a shared host.

On `w_cliff` the read phase fell from 1,145.6 and 890.6 s (ordered) to 222.8 and 207.7 s
(granular), 4.0 to 5.5x; count-worker idle fell from 58.97% and 57.21% to 0.02%. On `w_precliff`
it fell from 23.06 to 10.96 s. All count tables are byte-identical to the sealed baseline
(`6eb3e7ca`) and all 25 global manifest counters are equal between modes.

**The critique section's ceiling was wrong.** It assumed total CPU would not change and bounded
the gain at about 2.5x. Total CPU fell from 10,973 and 9,084 to 6,000 and 5,611 CPU-seconds and
decode CPU from 2,364 and 1,899 to 548 and 516, a CPU difference about twice the repeat-arm
spread. The cause was not profiled; same-thread allocation and release of each decoded read is
the leading candidate (voluntary context switches fell from about 60 M to 2.7 M). Peak RSS on
`w_cliff` rose 5.3 GB (8%), cause not established.

Implementation: `src/read_granular_input.hpp` (record peek, raw framing, decode helper);
`src/main.cpp` (grouping from serialized records, raw-byte admission bound, decode in
`decode_group_records` on the counting worker). Tests: `read_granular_input_test`,
`direct_count_input_modes`.

### Arena decode and the ordered-output reorder stage

**Arena.** Each count worker decodes a read into its own `google::protobuf::Arena` and resets it
after counting; `Group.records` holds pointers. At matched load on `w_cliff` the read phase fell
from 193.8 to 160.6 s and worker CPU from 5,186 to 4,240 CPU-seconds, byte-identical tables.
Evidence: `/mnt/ssd/lalli/pancollapse_decode_arena_v1_20260923T173617Z/RESULTS.md`.

**Reorder stage** (`src/ordered_output_reorder.hpp`). In `count`, RAD records, compatibility rows
and their warnings are captured per read and committed in ordinal order by whichever worker
deposits the next-expected read; 16,384 pending reads at most, never blocking the next-expected one.
`PANCOLLAPSE_ORDERED_OUTPUT=turn` restores the output turn. `count --rad-out` on `w_cliff` fell from
1,319.8 and 1,510.5 s to 155.3 and 154.7 s, with `map.rad` byte-identical to the turn's. Evidence:
`/mnt/ssd/lalli/pancollapse_rad_reorder_v1_20260923T182610Z/RESULTS.md`. Tests:
`ordered_output_reorder_smoke`; `direct_count_input_modes` now also compares `map.rad`.
