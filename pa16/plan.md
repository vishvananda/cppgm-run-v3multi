# PA16 implementation plan

## Current checkpoint

The bounded target is the typed alignment and complete non-virtual single-base
layout foundation at `5f8983b6`, with the canonical static-member, alignment
redeclaration, and union-boundary repairs.  The production flow is:

```text
PA10 typed AST -> PA11 canonical facts -> PA11 RecordLayout -> PA15 LowIR
```

This preserves `spec.md`'s one production pipeline, typed fact continuity and
canonical ownership, stable semantic identity, explicit completion states, and
bounded work.

PA10 owns sparse typed `alignas` argument ranges and type-id versus expression
classification.  PA11 owns the effective class request on `NamedRecord`, a
sparse typed `NamedRecordId` declaration-consistency fact, non-static member
requests on `BindingSidecar`, and one resolved direct-base `NamedRecordId`.
`pa11_record_layout.cpp` is the sole layout implementation and is registered
only for `cppgm++`; PA15 reads complete size/alignment through its existing
object type.  No text recovery, duplicate layout model, fake lifetime
behavior, or derived zero-storage shortcut is introduced.

The active slice handles natural padding, strictest alignment within one
declaration, `alignas(0)`, complete member types, one complete non-virtual
direct base at offset zero, and checked overflow.  It rejects conflicting
same-translation-unit declaration/definition alignment facts, weak requested
class or member alignment, incomplete/self/union/virtual/multiple-base routes,
and retains explicit `Incomplete`/`Computing`/`Complete`/`Failed` states.
Lookup, methods/calls, lifetime, constructors/destructors, bit-fields, packing,
ADL, polymorphism, and value semantics remain outside this checkpoint.

## Failure map and final gates

Fresh `make test-pa16` exits `2` with `35/243` passing, `208` failing, and
`243/243` covered.  The failure categories are 4 generated-LowIR mismatches,
201 expected-success / actual-failure mismatches, and 3 expected-failure /
actual-success mismatches.  Exact comparison with the supplied baseline log
finds zero failure-identity additions and zero removals; no baseline-passing
identity newly fails.

The exact command `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi` exits `0` with `1167/1167` tests passed.  The
PA16 source file audit exits `0` with five nonfatal pre-existing warnings for
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
and `pa15_lowering.h`.  `git diff --check` exits `0`.

Focused checks and the course boundary regression pass for alignment, direct
base layout, union-derived rejection, static/non-static member handling,
declaration consistency, `alignas(0)`, and same-declaration strictest
merging.

## Ownership and performance evidence

Alignment capture is a bounded typed range walk; declaration consistency is a
sparse `NamedRecordId`-keyed O(1) merge of compact flags, effective values, and
a conflict bit; base identity is resolved once; and completed layout scans
each class binding vector once with checked arithmetic per member.  These are
structural and representative observations only.  No timing, RSS, allocation,
or counter measurement was collected, so no numerical performance claim is
made.

## Next technical checkpoint

Implement the qualified out-of-class nested class-declarator alignment route
as the next narrow PA16 technical slice, after defining its typed parser-to-
semantic ownership and adding a compact regression.  It is not implemented by
this checkpoint.  PA16 completion is not claimed.

## Checkpoint ledger

| checkpoint/evidence | result and disposition |
| --- | --- |
| Completed: `5f8983b6` + checkpoint repairs | Focused and final gates pass; PA16 preserves `243/243` coverage and the exact `208`-failure baseline set, with the next technical slice explicitly limited to qualified nested class-declarator alignment |
