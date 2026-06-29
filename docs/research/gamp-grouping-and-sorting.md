# GAMP Grouping and Sorting Research Brief

**Status: Phase 0 researched. Scope: single-end 10x cDNA GAMP from local
VG v1.75.0-68-ge82694b69.**

## Verdict

Current single-end `vg mpmap -F GAMP` emits all alignments for one input read
contiguously, including with threads. Global read order is not guaranteed.
There is no supported VG command that name-groups GAMP after the fact, so V1
must keep the product contract: input GAMP is grouped by read name and
panCollapse validates that contract while streaming.

Paired-end GAMP can interleave mates and is out of V1 scope.

## Local implementation evidence

- In `src/subcommand/mpmap_main.cpp`, single-end mapping builds a
  `vector<multipath_alignment_t>` for one input read and passes the whole
  vector to `emit_singles`.
- `MultipathAlignmentEmitter::emit_singles_internal` converts the whole vector
  before writing it.
- The underlying `ProtobufEmitter::write_many` writes ordered items with no
  intervening items while holding the output mutex.
- `StreamMultiplexer` breakpoints occur after the batch, so threaded output can
  reorder read batches but does not interleave records inside one batch.

Primary source lines:

- `/mnt/ssd/lalli/usr/local/src/vg/src/subcommand/mpmap_main.cpp:1961-2018` maps one
  single-end input `Alignment` to one vector and calls `emit_singles`.
- `/mnt/ssd/lalli/usr/local/src/vg/src/subcommand/mpmap_main.cpp:2333-2344` routes
  single-end FASTQ records through that callback.
- `/mnt/ssd/lalli/usr/local/src/vg/src/multipath_alignment_emitter.cpp:225-258`
  converts the whole vector and calls `write_many` before any breakpoint.
- `/mnt/ssd/lalli/usr/local/src/vg/deps/libvgio/include/vg/io/protobuf_emitter.hpp:45-48`,
  `:74-76`, `:190-214` documents and implements ordered multi-object writes with no
  intervening items.
- `/mnt/ssd/lalli/usr/local/src/vg/deps/libvgio/src/stream_multiplexer.cpp:70-125`,
  `:239-360` queues and writes per-thread chunks at registered breakpoints.

## Sorting and transcoding commands

- No supported VG name-grouping or name-sorting command was found for GAMP.
- `vg gamsort` handles GAM/GAF and sorts by graph position or random order; it
  is not a GAMP read-name grouping solution.
- `vg view -K` and `vg view -k` transcode GAMP/JSON. They do not sort or group.
- Coordinate sorting is irrelevant to this contract and can be harmful because
  panCollapse groups by read name, not by graph position.

Primary source lines: `/mnt/ssd/lalli/usr/local/src/vg/src/subcommand/gamsort_main.cpp:1-4`,
`:31-57`, `:160-202`; `/mnt/ssd/lalli/usr/local/src/vg/src/subcommand/view_main.cpp:91-93`,
`:422-428`, `:687-809`.

## V1 input contract

- Accept only single-end GAMP that is grouped by read name.
- Treat duplicate input read names that recur after their earlier group has
  closed as a hard validation error.
- Do not require global preservation of FASTQ order.
- Do not silently repair ungrouped GAMP in V1.

## Lightweight validation procedure

For an audit outside panCollapse, names can be streamed through JSON:

```bash
/mnt/ssd/lalli/usr/local/bin/vg view -Kj input.gamp |
  jq -r '.name' |
  awk '
    NR == 1 { last = $0; next }
    $0 != last {
      closed[last] = 1
      if ($0 in closed) {
        print "noncontiguous read name: " $0 > "/dev/stderr"
        bad = 1
      }
      last = $0
    }
    END { exit bad }
  '
```

This is a validation aid only. It is not a supported preprocessing route for
large production GAMP.

## Remaining risks

- The contiguity conclusion is based on current local implementation, not a
  formal stable file-format guarantee.
- If a future VG emitter changes batching behavior, panCollapse's own streaming
  validator remains the protective contract.
