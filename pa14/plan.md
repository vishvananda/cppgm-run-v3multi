# PA14 Checkpoint Plan

## Truthful checkpoint boundary

This checkpoint owns the complete checked-in `100-*` family and the landed
PA14 line-adapter -> canonical typed `AbiFactCase` -> reusable Itanium encoder
path. It does not extend the 200--600 grammar. The starting tree was clean at
`a95729060db60598a9e1f490346d093db7e99c3e`; no later commit is assumed.

The exercised path now aligns with spec.md §§1--4 and §7: one append-based
type encoder, typed continuity after line parsing, numeric/fixed vocabulary
facts, canonical qualified-name components, numeric-only per-case definition
IDs with an adapter sidecar, deterministic ordering, dense cycle state, and
explicit rejection of typed 128-bit values that exceed stored representation.
Chapter 5.1 spellings are preserved for nested names, `std` abbreviations,
builtin and compound types, special terminals, supported integral values, and
C linkage.

The dead write-only substitution table, recursive structural string key, and
parallel type encoder were removed. This checkpoint makes no substitution or
new later-family support claim.

## Current failure map

The final `make test-pa14` covered the same 111 tests and passed 35, with 76
failures: 100 is 25/25; 200 is 3/25; 300 is 7/37; 400 is 0/4; 500 is 0/13;
and 600 is 0/7. The later-family failure counts are 22, 30, 4, 13, and 7.
The ten established exact 200/300 passes remain included in those ten later
passes.

## Evidence and limits

- Forced `make -B -C dev abimangle` and direct C++11 `-Wall -Wextra -Werror`
  syntax checks for `dev/abimangle.cpp` and `dev/src/abi_mangle.cpp` passed
  without warnings.
- `make -C pa14 check TEST='tests/abi/100-*.t'`: 25/25 passed; the ten
  pre-existing exact later checks also passed individually.
- Generated probes reject typed `uint128 -1`, preserve signed minimum, and
  retain duplicate/unknown binder diagnostics. Structural counters show 23
  type-kind cases in one `append_type` and zero retired encoder,
  substitution-key, joined-name, or node-set symbols.
- The immutable corrected candidate has SHA-256
  `7d99b0c57154421f3405bb95dea31d439032de5e818a7f83cd34b78434029b40`.
  Five interleaved repetitions over equivalent generated inputs measured
  median wall/user/system/RSS of 0.14/0.08/0.05/62,808 KiB at 5,000 cases,
  0.27/0.17/0.10/122,072 KiB at 10,000, and
  0.57/0.35/0.21/240,604 KiB at 20,000. The corresponding input sizes were
  438,893, 878,894, and 1,768,894 bytes. This is full-process evidence with
  retained-case RSS, not a comparison ratio or encoder-only claim.
- Through-PA13 passed 947/947. The PA14 source file audit passed with four
  policy warnings, and `git diff --check` passed.

## Next checkpoint

The next authorized checkpoint is the 200-family typed grammar and
substitution boundary. It must receive its own ownership, complexity, and
reference audit; this plan does not claim that support here.

## Completed-checkpoint ledger

| Checkpoint | Starting point | Result | Status |
|---|---|---|---|
| PA14 bounded typed-foundation audit | `a95729060db60598a9e1f490346d093db7e99c3e` | Numeric IDs, one append type path, dense cycle state, explicit wide-value boundary, focused/broad validation, through-PA13, and source audit complete | Broad validation complete; bounded five-path repair finalized |
