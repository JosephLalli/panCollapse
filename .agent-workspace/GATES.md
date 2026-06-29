# Development Gates

Each gate is named for the condition that must be true before work can continue.

## Gate Architecture Approved

Human judgment required.

Pass only when:

- all Phase 0 research briefs cite current primary evidence;
- exact required inputs and external dependencies are proposed;
- GAMP grouping has a version-scoped verdict and remedy;
- RAD writing has a concrete standards-conformance strategy;
- annotation lookup honors every settled compatibility rule without a custom index;
- the architecture and CLI are specific enough to implement;
- decisions and remaining forks are explicit;
- no production code or build configuration exists.

## Gate Behavior Specified

Human judgment required.

Pass only when:

- every settled behavior has an explicit fixture and expected result;
- the outside-first/last-exon rule and required positive transcript-model anchor are
  represented;
- tag skip/strict/fail behavior and complete manifest coverage are represented;
- policy ordering—score, collapse, uniqueness—is unambiguous;
- RAD interoperability has an exact expected matrix;
- failure and diagnostic behavior are testable;
- the build/test skeleton matches the approved architecture.

## Gate Vertical Slice Proven

Human review plus machine checks.

Pass only when:

- a single name-grouped, tagged read reaches a canonical transcript RAD record;
- the supported alevin-fry version consumes the output;
- the exact expected gene matrix is produced;
- build and tests are reproducible;
- output is byte-identical across supported thread counts;
- implementation review finds no hidden scope expansion.

## Later increments

After Gate Vertical Slice Proven, autonomous work may continue only on one
machine-verifiable behavior at a time. Any biological ambiguity, interface change,
custom-index proposal, or new output backend returns to the nearest applicable named gate.
