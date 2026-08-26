# PA16 checkpoint audit

## Current Checkpoint Review

This review covers landed `5f8983b6` and the checkpoint repairs in
`pa11_semantic.cpp`, `pa11_semantic_core.cpp`, `pa11_semantic_model.h`, and the
course boundary regression.  The audited slice is the typed alignment and
complete non-virtual single-base layout foundation; PA16 is not complete.

The owned fact flow is:

```text
PA10 typed alignas argument/range or BaseName
  -> PA11 canonical NamedRecordId / BindingSidecar fact
  -> PA11 RecordLayout state, size, alignment, base, and member offsets
  -> PA15 complete object LowIR type / existing sizeof consumer
```

PA10 retains each `alignas` argument as typed syntax classified as a type-id or
expression, including sparse ranges for multiple specifiers.  PA11 evaluates
the argument semantically and combines all specifiers belonging to one
declaration by the strictest non-zero value.  `alignas(0)` is retained as
specifier presence but has no layout effect.  At canonical `NamedRecord`
ownership, the effective non-zero request remains directly available to
`RecordLayout`.  Only records that need alignment redeclaration checking
receive a sparse `NamedRecordId`-keyed typed fact.  Its compact flags retain
specifier presence, the first non-defining effective alignment, an O(1)
conflict bit, and the defining effective alignment; an unaligned definition
uses the canonical record's existing `defined` fact and does not allocate this
cold entry.  Equivalent redeclarations, omitted non-defining specifiers, and
the converse requirement that every defining declaration agree with any
aligned declaration are checked without text recovery or a global rescan.
Class natural-alignment validation remains in `RecordLayout`.

Class alignment belongs to the canonical `NamedRecord`.  A member alignment is
attached only after canonical binding merge and only when the binding is a
non-static variable in a class scope, using the canonical static-member fact;
static objects remain outside this narrow `RecordLayout` member fact.  Natural
member placement, checked padding/size arithmetic, and the class alignment
are then consumed by the layout owner.

`BaseName` is resolved once to a `NamedRecordId`.  The path requires a complete
non-union, non-virtual class base, rejects self, union-derived, virtual, and
multiple-base forms, and records the direct base as a distinct subobject at
offset zero.  Derived member placement starts after that complete base
representation.  `RecordLayout` exposes explicit `Incomplete`, `Computing`,
`Complete`, and `Failed` states; failure cleanup clears published storage and
member/base facts, and derived records remain ineligible for the narrow
namespace zero-storage shortcut.

The production layout owner remains `pa11_record_layout.cpp`, registered only
for `cppgm++`.  PA15 consumes the completed typed layout and does not implement
a second layout map or reconstruct facts from rendered output.

## Final evidence

- `make test-pa16`: exit `2`, `35/243` passed, `208` failed, all `243/243`
  covered.
- Exact failure categories: 4 generated-LowIR mismatches, 201 expected-success
  / actual-failure mismatches, and 3 expected-failure / actual-success
  mismatches.
- Comparison with
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
  failure-set additions `0`, removals `0`; no baseline-passing identity newly
  fails; coverage remains `243/243`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`:
  exit `0`, all `1167/1167` through-PA15 tests passed.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: exit `0`;
  five nonfatal pre-existing `bad-division` warnings remain for
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`.
- `git diff --check`: exit `0`.
- Focused representative checks and the course boundary regression pass,
  including direct-base consumption, union-derived rejection, declaration
  alignment consistency, `alignas(0)`, and same-declaration combination.

Structural performance evidence is representative only: alignment capture is a
bounded forward range walk, redeclaration consistency is a sparse
`NamedRecordId`-keyed O(1) fact merge, base identity is one lookup per class,
and layout scans each class binding vector once with checked arithmetic per
member.  No timing, RSS, allocation, or instrumentation-counter measurement
was taken, so no numerical performance claim is made.

Known boundary: the qualified out-of-class nested class-declarator alignment
route remains outside this checkpoint.  The next technical PA16 checkpoint is
to design and implement that narrow parser/semantic route with its own focused
regression, while preserving the same typed ownership boundary.

## Audit ledger

| checkpoint | current result | disposition |
| --- | --- | --- |
| `5f8983b6` plus checkpoint repairs | Typed alignment/redeclaration and single-base focused checks pass; final PA16 remains `35/243` with `208` failures and `243/243` coverage, exactly matching the supplied failure set; through-PA15 and file audit pass, with five listed nonfatal warnings | Current audited checkpoint; PA16 remains incomplete and the qualified nested class-declarator route is the next technical slice |
