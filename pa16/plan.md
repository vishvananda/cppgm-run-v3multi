# PA16 implementation plan

## Stage Design

The implementation keeps one forward typed flow:

```text
PA10 typed AST -> PA11 canonical semantic owners -> PA11 RecordLayout -> PA15 LowIR
```

`PA10Ast` now owns sparse typed `alignas` argument facts. Each argument is
classified as a type-id or expression and remains an AST node; no later stage
recovers it from rendered text. `NamedRecordId` remains the sole canonical
record identity. Its owner carries the strongest requested class alignment,
one resolved direct-base `NamedRecordId`, and the rejected virtual flag. A
member's requested alignment is a typed `BindingSidecar` fact.

PA11 resolves a `BaseName` once, requires one complete non-virtual class base,
and records its subobject separately in `RecordLayout` at offset zero. Layout
then consumes the completed base representation followed by ordinary fields,
applies checked alignment/padding and `size_t` overflow checks, and publishes
one explicit `Incomplete`/`Computing`/`Complete`/`Failed` state. PA15 consumes
the resulting size/alignment through its existing LowIR object type. Its
namespace zero-storage eligibility remains conservatively false for derived
records; no lifetime behavior is fabricated.

This follows `spec.md`'s single typed flow, fact continuity/ownership, stable
semantic identity, and bounded-work requirements, and the N3485 [class.mem]
complete-type/member-allocation and [class.derived] direct-base facts. Access
lookup, constructors, destructors, calls, value semantics, virtual machinery,
and multiple inheritance remain outside this checkpoint.

## Failure Map

Turn-start full-stage baseline, before this diff: `32/243` passing, `211`
failures, all `243/243` covered. Its category split was five generated-LowIR
mismatches and 206 exit-status mismatches: 203 expected-success/actual-failure
and three expected-failure/actual-success.

Fresh full validation with `make test-pa16` (complete output captured outside
the tree at `/tmp/v3multi-pa16-full-post-audit-fix.log`) reports `35/243`
passing, `208` failures, all `243/243` covered, exit code `2`: a gain of three
passes and removal of three failures. The residual split is four generated-
LowIR mismatches (`200-unnamed-namespace-hidden-friend-single-definition`,
`300-enum-operator-adl-selects-matching-overload`, `300-packed-class-layout`,
and `300-pragma-pack-followed-by-endif`), 201 expected-success/actual-failure
exit mismatches, and three expected-failure/actual-success mismatches.

The current failure identity set is exactly the baseline set minus
`300-alignas-class-layout.t`, `300-alignas-derived-base-layout.t`, and
`300-member-alignas-layout.t`; there are no new identities and therefore no
previously passing PA16 regression. Remaining failures are later lifetime,
constructor/destructor, lookup/call, value-semantics, bit-field, aggregate,
polymorphism, nested-parser, and other PA16 surfaces. PA16 is not complete.

## Active Checkpoint

Implemented and validated as the bounded PA16 foundation:

- PA10 typed alignment sidecar capture at class-head and declaration-specifier
  boundaries, with generic attribute skipping still bounded and presentation
  output compatible with the existing PA10 fixture.
- PA11 canonical class alignment and member-alignment facts; one semantic base
  resolution with conservative rejection of incomplete, virtual, union, and
  multiple bases.
- Distinct `RecordLayout` direct-base subobject metadata at offset zero;
  derived size/alignment starts after the occupied base representation and
  preserves stronger requested alignment.
- Conservative derived zero-storage eligibility and explicit failed-state
  cleanup.

Final focused evidence against the rebuilt `dev/cppgm++`: `make -C dev cppgm++`
passed, and the exact three-test command
`make -C pa16 check TEST='tests/general/300-alignas-class-layout.t tests/general/300-alignas-derived-base-layout.t tests/general/300-member-alignas-layout.t'`
reported `pa16 check: PASS (3/3)` with exit code 0. The checked-in
`make -C pa16 check TEST=tests/general/300-under-aligned-class-bad.t` also
passed its expected failure. The exact PA10 regression command,
`make -C pa10 check TEST=tests/general/100-class-alignas-after-class-key.t`,
reported `pa10 check: PASS (1/1)` with exit code 0. The exact prior gate
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
reported `ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)` with exit code 0.
The exact file-audit command
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` passed with
exit code 0 and five pre-existing warnings; there were no fatal findings.
`git diff --check` passed. An additional
out-of-class nested-type alignment probe still stops in the existing qualified
class-declarator parser route (`expected declarator-id at token 24`), so it is
not included in the checkpoint pass claim. The validated checkpoint is
landed/committed and validated in this checkpoint; it does not claim broad
PA16 completion.

## Performance Evidence

Alignment attributes are parsed in one forward pass and stored by compact
ranges into a cold typed sidecar. Base identity is resolved once per class.
Each completed record scans its binding vector once; the base chain contributes
only its already-completed layout facts, and member layout uses checked
constant-time arithmetic per member. Explicit layout states prevent retries
and there is no whole-program rescan. No benchmark, RSS, or instrumentation
counters were collected, and no timing/RSS claim is made. No material
performance risk at this bounded checkpoint required representative
measurement; the evidence is structural complexity plus the successful build,
focused checks, and full-stage coverage.

## Checkpoint Ledger

| checkpoint/evidence | result and disposition |
| --- | --- |
| Turn-start repository and baseline | Clean tree; PA16 `32/243`, `211` failures, all covered; baseline full log and stage-progress evidence recorded. |
| Typed single-base/alignment foundation | Implemented in nine `dev/src/` implementation files plus `pa16/plan.md` (ten tracked files total); focused trio `3/3`, checked-in weak-alignment negative `1/1`, and PA10 regression `1/1`; derived zero-storage remains rejected conservatively. |
| Full PA16 validation | `make test-pa16`: `35/243`, `208` failures, all `243/243` covered, exit `2`; three baseline failure identities removed and none added. |
| Prior-stage gate | Exact through-PA15 command: `1167/1167`, exit `0`. |
| File audit | Exact PA16 audit: passed, exit `0`; five warnings, zero fatal findings. |
| Diff and disposition | `git diff --check` passed; the change comprises nine `dev/src/` implementation files plus `pa16/plan.md` (ten tracked files total), with no tests or refs. The checkpoint is landed/committed and validated in this checkpoint; later PA16 work remains deferred. |
