# PA15 Audit

## Current Checkpoint Review

This review is bounded to the landed structured-control increment
`4bca2ad8f6a4b14d3f6922f1c3a20e42f852eac3` and its affected ownership path.
The repair in this milestone is limited to typed PA12 switch-entry validation
and typed PA15 CFG recovery; no unrelated PA15 feature group was re-audited.

The ownership path is continuous:

- PA12 owns `FunctionFact`, `Scope`, `Binding`, `SemanticFact`, conversion
  facts, condition declarations, loop/switch statement shapes, and typed case
  literals. The new PA12 check keeps switch-transfer legality with that
  semantic owner: an owned case/default target cannot enter an active lexical
  scope after an initialized automatic declaration. `DeclarationFact` carries
  the storage-duration fact sourced from `SpecFact`; the validator counts only
  initialized automatic declarations. Its lexical frames carry `ScopeId` and
  the initialized count, with one active total, so nested switches are
  independent owners without a node-based scope map.
- `Pa15Lowerer` indexes the complete PA12 scope arena once per translation
  unit, assigns typed slots, and lowers facts directly into typed `Program`
  functions. Loop targets, switch break targets, and continue targets remain
  separate typed stack entries; `continue` skips switches and selects the
  nearest loop. A dense `SemanticFactId`-indexed loop-target table is sized
  once for the translation unit and retained across all function lowerings.
- Switch labels are collected once for their owning switch and stop at nested
  switch facts. Recovery starts at dispatch targets, skips ordinary
  pre-label statements, preserves fallthrough to already-lowered labels, and
  recovers labels nested in `if`, `while`, `do`, and `for` without duplicating
  their ordinary loop lowering. Each loop fact records its typed break and
  continue blocks once; recovery reuses those targets when a label follows a
  terminating path. Exhaustive non-void switches retain a required typed
  continuation block without manufacturing a return value.
- Conditions remain typed. Direct root `&&` and `||` conditions lower to
  operand CFG branches; they do not materialize a `land__`/`lor__` result slot.
  Block IDs, operands, instructions, and final block order remain typed and
  deterministic. The PA13 serializer is the only text boundary and emits the
  typed `switch` terminator contract.

The bounded defects repaired in this path are switch recovery before the first
label, exhaustive-switch continuation handling, duplicate ordinary lowering
while recovering nested loops, and storage-duration-blind transfer rejection.
The PA12 transfer check rejects initialized automatic declarations while
accepting the focused initialized local-`static` case. This is the procedural
PA15 boundary: it does not claim complete C++ class/object lifetime or every
storage-duration rule outside the represented `SpecFact` subset. No fixture,
reference output, harness, or unrelated semantic feature was changed.

CFG reachability has one owner: the typed block terminator already stored in
each `Block`. When an edge is emitted from a reachable source, its target is
marked immediately. When a block first becomes reachable, its canonical
terminator is inspected once to propagate its existing jump, branch, or
switch targets. There is no second per-block adjacency allocation. Each block
bit transitions at most once and each terminator edge is considered a bounded
number of times. Loop recovery reuses the dense targets saved for an
already-lowered loop.

For `A` consumed PA12 facts, `S` scopes, `B` bindings, `N` functions, and `E`
typed IR edges, the PA12 transfer walk is `O(A)` with lexical-depth state; the
stage-wide scope/slot owner index is `O(S+B+N)` structural propagation plus
the existing deterministic `O((S+B+N) log B)` ordered indexes; the retained
loop-target table is initialized once in `O(A)` space/time; and reachability is
`O(B+E)`. Structured label/recovery traversal is linear in its owned facts,
so the total affected path is `O(n log n)` under the specification bound, with
no whole-CFG scan per switch or recovered loop.

PA13 has no typed `unreachable` terminator. A self-jump is therefore emitted
only at final function exit when monotonic reachability proves the retained
continuation unreachable. The continuation may be empty or may contain a
lowered unreachable lexical tail; reachable non-void fallthrough remains an
error. This is a deliberate LowIR representation, validated below with
LowIR and CY86 output.

## Focused Evidence

- Turn-start evidence in `last-test.log`: **21/109 passing, 88 failing, all
  109 covered**. The final root through-PA14 gate passed **1058/1058**. The
  final full PA15 report is **21/109 passing, 88 failing, all 109 covered**,
  and its complete failing set is identical to the turn-start set.
- `make -C dev cppgm++`: passed.
- The implicated checked-in control set, including the expected-failure
  switch-initialization fixture, passed **9/9**:
  `100-bad-switch`,
  `100-switch-label-bypasses-initialization-bad`,
  `100-continue-inside-switch-targets-loop`,
  `100-do-while-lowering`, `100-for-loop`,
  `100-nested-switch-cases-stay-inner`,
  `100-switch-condition-declaration`, `100-while-break`, and
  `200-direct-short-circuit-condition-branch`.
- Fresh stdin probes passed LowIR compilation and `lowir2cy86` validation for
  nested `while`, `do`, `for` without an initializer, and `if` labels, plus
  no-label, braced-case, and exhaustive-switch probes. Separate `for`-init
  and while-condition bypass probes were rejected by PA12 with the typed
  initialization diagnostic.
- The focused storage-duration probes passed: initialized local `static`
  compiled (LowIR and CY86 both status 0), while initialized automatic storage
  was rejected with `PA12 case or default label bypasses variable
  initialization`.
- Four focused label-after-termination probes (top-level, `while`, `do`, and
  `for`) each compiled and passed `lowir2cy86` (0); the nested recovery sample
  at `/tmp/pa15-structured-correction.wf5una/measurements-final.tsv` has five
  interleaved samples per depth, all status 0, and the corresponding depth-256
  LowIR passed CY86 validation. Its structural counts grow linearly from 102
  blocks/175 instructions at depth 32 to 774/1295 at depth 256. The exact
  unreachable nonempty-tail probe compiled, retained its tail instructions,
  and passed CY86; the reachable non-void fallthrough probe was rejected.
- The many-function/one-loop-per-function family in the same final measurement
  log reached 257 functions, 1,025 blocks, and 2,561 instructions at size 256
  with status 0. This exercises the translation-unit loop-target table rather
  than a per-function semantic-arena reset.
- The immutable corrected executable was
  `/tmp/pa15-structured-correction.wf5una/cppgm++-corrected` with SHA-256
  `6e5843f5e44966fd1b4f62b98e3d5ed829b306afb78024b6c1f4e8e180054688`; its
  interleaved raw measurements are at
  `/tmp/pa15-structured-correction.wf5una/measurements-final.tsv`.
- The prior target-commit performance table remains historical evidence only;
  its old temporary artifacts are not present or inspectable at this
  checkpoint. The fresh immutable-executable table, SHA-256, and actual paths
  are recorded in `pa15/plan.md`.
- The final PA15 file audit passed with four existing header-division warnings:
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`, and
  `pa11_semantic_model.h`. The final `git diff --check` also passed.

## Audit Ledger

| Checkpoint | Evidence and disposition |
|---|---|
| PA15 full-stage / checkpointAudit — structured control and typed switch ownership | Audited PA12-to-LowIR ownership for loops, break/continue, condition declarations, switch labels/fallthrough, nested labels, direct short circuit, deterministic blocks, and stage-wide scope indexing; repaired the bounded semantic/CFG defects above; focused evidence is 9/9, through-PA14 is 1058/1058, PA15 is 21/109 with the baseline failure set unchanged, all 109 covered, and the file audit passes with four existing warnings. |
