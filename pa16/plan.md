# PA16 typed source-to-LowIR checkpoint

## Stage Design

PA11 owns canonical class, lifetime, layout, and `BitFieldFact` identities,
including declared/storage/operation types, signedness, widths, masks, and
offsets.  PA12 carries those facts through conversions, actions, and retained
`(ReturnStatement, FunctionFactId)` owner pairs; its finalizer accepts both
body-bearing function node kinds and resolves bindings through
`definition_by_binding_` in O(1).  PA15 consumes the facts through typed
`LoweredValue`, `integer_i64`, construction/destruction paths, and bit-field
extraction; PA13 validates typed LowIR directly.

Both automatic storage-root and action-based/member-array construction use one
typed `ArrayCleanupChain`.  Each completed element adds one persistent reverse
cleanup node; its address is recomputed in its own unwind block and it jumps to
the preceding node.  Deferred constructor edges retain typed identity without
cross-block SSA producers.  For fixed array rank this is deterministic O(n)
cleanup and LowIR work, with O(rank) typed address reconstruction per element;
there are no per-width scans or LowIR-text/source-spelling reconstruction.
This aligns with `spec.md` Purpose and §§1–5/§7, C++ reverse lifetime order,
and the PA13 same-operation-type operand contract.  The old member-array
fixture shape was established as non-ABI behavior by the source-preserving
local check and independent course stress regression.

## Failure Map

Turn-start authority was clean `ff83cef4`, `make test-pa16` `239/243`, earlier
through PA15 `1167/1167`, and exactly four residual local failures:

1. `200-local-default-class-array-lifecycle.t`: the fixture encoded forward
   flat destruction and repeated root recomputation; PA15 and C++ require
   reverse order with root reuse.
2. `200-reference-indexed-pointer-member-access.t`: the fixture fed `i32`
   directly to an `i64` multiply; PA13 requires matching binary operand types,
   so PA15 widens with typed signed `sext`.
3. `400-signed-bit-field-read.t`: the fixture discarded sign reconstruction
   and returned a masked positive value instead of the represented negative
   value required by PA16.
4. `400-signed-enum-bit-field-read.t`: the same error occurred for a signed
   `int`-underlying enum bit-field.

The four fixture repairs preserve their sources, sidecars, comparator,
validator, and coverage identities.  Course 410 exposed the actual
`SpecialMemberDefinition` owner node; PA12 validates its canonical body,
identity, and binding exactly like an ordinary function definition.  PA15's
remaining array residual is closed by removing the quadratic
`emit_constructor_call_with_cleanup` path and routing action/member roots
through the persistent chain.  The authorized 300 fixture records that shape,
and course 434 checks it at three sizes.

## Active Checkpoint

Final validation is green: `make test-pa16` `243/243`; the exact `n=16`
through-PA15 command `1167/1167`; `make test-report-through-pa16`
`1410/1410`; the PA16 source audit passes with six established
header-division warnings; course 410 and 434 pass; the four original residual
locals pass `4/4`; local 300 passes `1/1`; courses 431, 412, 418, 420, 423,
424, 429, 432, and 433 pass; and `git diff --check` passes.

The original PA16 local corpus remains `243` `.t` files, `243` matching
`.ref.exit_status` sidecars, and `242` `.ref` LowIR fixtures.  The sole absent
LowIR fixture is the intentional rejected
`200-protected-member-typedef-access-bad.t`, whose sidecar is `EXIT_FAILURE`.
There are `35` PA16 course scripts in both the source directory and the
`pa16/course/pa16` symlink view.  No tests, sidecars, harnesses, comparators,
or coverage paths were deleted or renamed.

## Performance Evidence

Automatic storage-root arrays from course 410 report cleanup calls
`E=8/16/32: 7/15/31` and main-function lines `143/287/575`, with incremental
line deltas `144` and `288`.  Action-based/member-array roots from course 434
report cleanup nodes and calls `7/7`, `15/15`, `31/31` and Holder-constructor
lines `141/285/573`, with the same `144` and `288` deltas.  Thus both root
kinds have linear cleanup-node/call and LowIR growth; nested course 410 also
checks reverse order (`outer 1,0`; `inner 2,1,0`).  Each chain node emits one
destructor and one predecessor transfer.  Retained return owners finalize in
one pass with O(1) definition lookup, and the audit-visible construction unit
remains `2834` lines.

## Checkpoint Ledger

- Start: clean `ff83cef4`; supplied authority `239/243`, four residual
  identities, and complete original coverage.
- Trace: PA11/PA12 facts and PA15 consumers establish reverse lifetime order,
  typed index widening, signed integral/underlying-enum extraction, the
  `SpecialMemberDefinition` body owner, and shared cleanup-chain ownership.
- Repair: four allowed LowIR fixture contracts repaired; PA12 canonical
  ownership generalized; PA15 now uses the persistent chain for automatic and
  action/member arrays; the obsolete helper is removed; course 434 is added.
- Broad evidence: PA16 `243/243`; through PA15 `1167/1167`; through PA16
  `1410/1410`; audit passed with six warnings; coverage is `243/243/242` as
  documented above; and `git diff --check` passed.
- Final handoff: one coherent PA16 checkpoint contains the typed implementation,
  authorized 300 fixture, course 434 regression, and plan, with parent
  `ff83cef4` and no unrelated coverage or history mutation.
