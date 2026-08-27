# PA16 implementation plan

## Stage Design

This checkpoint adds typed lifetime facts for fixed-bound arrays of class
objects.  `TypeId` owns the complete array shape: each expansion validates a
known bound and follows its typed element `TypeId` until the owning
`NamedRecordId` is reached.  Object and member ownership remains on
`BindingId`; constructor and destructor `FunctionFact`s, `ConstructorAction`
facts, `DestructorAction` facts, and automatic `LifetimeFact`s are selected in
PA12 before PA15 lowering.

Local automatic variables record one lifetime fact.  Synthesized constructors
retain declaration-order member actions, including recursive array members;
destructor actions walk members and array elements in reverse order.  A
throwing constructor expands elements in forward order and cleans only the
completed prefix in reverse order.  Its EH cleanup is a shared typed chain:
each needed prefix node destroys one newest completed element and transfers to
the prior node, whose base resumes.  Explicit member initializers retain
precedence over DMIs.  PA15 consumes typed actions and emits checked indexed
addresses, calls, scope cleanup, and EH cleanup without rendered-name
recovery, whole-scope retries, or reference/host-compiler execution.

The design follows `pa16/README.md` and `spec.md` §§2–5 and §7: typed identity
and ownership stay canonical, semantic selection precedes lowering, work is
bounded and deterministic, and arena-backed ranges are snapshotted before
publishing more facts.  Global/TLS lifetime, operators/access changes, and
unrelated cleanup are outside this checkpoint.

## Failure Map

The authoritative turn-start full-stage baseline is `91/243` passing, `152`
failures, and `243/243` coverage.  The final PA16 run is `93/243` passing,
`150` failures, and `243/243` coverage.  Normalized failure identities remove
exactly:

- `pa16/tests/general/200-return-preserves-value.t`
- `pa16/tests/general/300-array-member-empty-paren-value-init.t`

No baseline failure identity was added, and the covered identity set has no
additions or removals (`243/243` in both runs).  Thus the stage progress is
`+2` passes and `-2` failures with no coverage loss.  The two focus identities
above pass exact checked comparisons; the other four handout focus tests
remain LowIR-shape mismatches.  The first mismatch for each is unchanged:
the two ordinary lifetime controls first lack `eh_cleanup ^destructor_cleanup_1`
after `store ptr %this, $this`; the local array fixture first expects a fresh
`addr $elements`/decay for element two; and the synthesized-member fixture
first expects `binary mul i64 2, 4` plus an `i8` index while general member
access emits `index obj<4x4> ... , 2`.  No handout test, ref, or comparator was
edited.

The final focused matrix passed `200-return-preserves-value.t`,
`300-array-member-empty-paren-value-init.t`, course 410, and course controls
404, 408, and 409.  It retained the four named handout comparison failures.
`make test-pa16` returned its expected nonzero status for the remaining 150
failures; the progress gate is satisfied.  `make test-report-through-pa15`
passed all `1167/1167` tests.

## Active Checkpoint

The checkpoint is limited to local automatic array lifetime and recursive
synthesized array-member lifetime.  It includes typed destructor
discovery/implicit synthesis, automatic lifetime recording, constructor and
destructor action arenas, recursive fixed-bound expansion, reverse scope
cleanup, and serializer support for the EH instructions that this lowering
now emits.  Lowering activates a typed lifetime stack only after each
declarator initialization, records one depth marker per lexical scope, and
stores break/continue exit depths.  At an if split it snapshots the complete
`BindingId` sequence, restores it before each mutually exclusive branch, and
requires exact sequence equality for every surviving join state; divergent
terminal/recovery states fail closed.  Return/fallthrough and loop exits
destroy only the active suffix; goto is fail-closed when this checkpoint
cannot prove that a nontrivial lifetime is not crossed.  Completed constructor
elements retain a typed root and array-index path, so cleanup blocks recompute
fresh addresses instead of retaining LowIR temporaries.  The shared prefix
chain materializes each typed completed element at most once, and each chain
node contains one destructor call before its predecessor transfer.  Array element
offsets use checked `type_size(child)` for class and nested-array byte
projections while scalar value-initialization keeps its established element
index form.  The empty `words()` control remains exact.  Course regression 410
covers declaration activation, branch state restoration, loop exits, and
nested-array stride/order.

Excluded: global/static/TLS lifetime and guards, copy/move or by-value class
transfer, virtual/multiple inheritance, templates, new/delete, operators,
access-control work, and unrelated temporary/cleanup machinery.  The remaining
uncertainty is exact checked-reference shape for the four known comparison
failures; runtime control-exit and nested-array course evidence is passing.

## Performance Evidence

For a fixed array with `E` materialized elements and nesting depth `D`,
semantic action creation and lowered work are intended to be O(E·D), with
each materialized element visited once per containing array level.  Bounds and
`ordinal * type_size(child)` overflow are checked before index conversion;
arena ranges are copied before recursive demand can grow an arena.  No
whole-scope retry or repeated rendered-name lookup is used.

Course 410 reports one cleanup node/call per needed prefix and these
main-function line counts: `E=8 -> 7, 129`, `E=16 -> 15, 257`, and
`E=32 -> 31, 513`; the line deltas are `128` and `256`.  Its nested `[2][3]`
case observes six destructors, outer reverse stride `1,0`, and inner reverse
indices `2,1,0,2,1,0`.

The final smoke/scale sample used one mode `-r-xr-xr-x` immutable compiler copy
(`/tmp/v3multi-pa16-perf-final.9DAtdP/cppgm++-immutable`, SHA-256
`940274fab4359db0a8461f178881e24712265739c21ab7342b91b0ce4fbde532`) and
equivalent generated flat-array inputs.  Each size had five interleaved timed
batches of 20 compiler invocations; `/usr/bin/time` medians and ranges were:

| E | wall s | user s | system s | max RSS KiB |
| --- | --- | --- | --- | --- |
| 32 | `0.09` (`0.09..0.09`) | `0.04` (`0.03..0.05`) | `0.04` (`0.03..0.05`) | `6444` (`6400..6464`) |
| 128 | `0.18` (`0.18..0.18`) | `0.09` (`0.09..0.11`) | `0.08` (`0.07..0.09`) | `8448` (`8436..8500`) |

These are smoke/scale measurements, not a benchmark claim.  Final-output
structure was deterministic: E=32 had 531 total lines, 513 `main` lines, 382
instructions, 32 constructor calls, and 31 cleanup nodes/calls, hash
`67b17d7e3f7b2a3507dd795ed9cd05285dc1050c1eec600d15f92b70a6b16d0b`; E=128
had 2067 total lines, 2049 `main` lines, 1534 instructions, 128 constructor
calls, and 127 cleanup nodes/calls, hash
`cc0554ce1ed562f67be832da79110737001cbf9b96aa40c406960803c3e96399`.
The 4x element increase produces 4x main-line and instruction growth, while
cleanup calls remain exactly `E-1`, supporting the intended O(E·D) typed chain
representation (D is array nesting depth).

## checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 typed fixed-bound local/synthesized array lifetime | Final coherent increment: `93/243`, 150 failures, no added failure identities, two removed identities, 243/243 coverage; four focus comparison mismatches remain. |
| PA16 typed parameterized constructor selection | Historical prior work retained; no new gate claim in this checkpoint. |
| PA16 earlier static/member typed ownership work | Historical prior work retained; no new gate claim in this checkpoint. |
| Full-stage baseline/final delta | Baseline `91/243`, 152 failures, 243/243 coverage -> final `93/243`, 150 failures, 243/243 coverage; removed return-preserves-value and empty-paren array-member identities, added none. |
| Required gates | Through PA15 `1167/1167`; file audit exit 0 with five existing header-division warnings; `git diff --check` PASS; course 410/404/408/409 and exact return focus PASS. |
| Commit state | Checkpoint committed with required gates and clean-tree verification. |
