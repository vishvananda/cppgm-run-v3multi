# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10 and PA11 typed syntax, lookup, and
layout become PA12 semantic facts and PA15 LowIR.  Earlier bit-field and
constructor checkpoints retain their typed `BitFieldFact`/`BindingId`,
`NamedRecordId`, `RecordLayout`, and `ConstructorActionFact` owner chains.

This checkpoint consumes PA12's typed value-initialized constructor action and
PA15's canonical `TypeId`/layout-derived `LowType`.  It clears a complete
aggregate object representation with compact scalar stores, then preserves the
existing declaration/member constructor order.  This follows `spec.md`
§§1--5 and 7 and the PA16 aggregate/value-initialization boundary.  No textual
transport, parallel analyzer, retry, second lowerer, or host/reference shortcut
is involved.

## Failure Map

The supplied current authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`216/243` passed, exactly `27` failed, and `243/243` identities are covered.
The complete current failure map is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

This is the complete authority map before this checkpoint: `216/243` passed,
`27` failed, and `243/243` identities were covered.  The final PA16 run is
`217/243` with `26` failures and `243/243` identities covered.  Exact set
comparison found one baseline-only failure,
`pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`, and
zero fresh failures; the final set is precisely the 27-name baseline minus
that identity.

## Active Checkpoint

The reviewed production change is confined to
`dev/src/pa15_lowering_construction.cpp`, in
`zero_initialize_value_initialized_object`.  PA12 already marks the empty
aggregate construction as `value_initialize` and publishes the selected
synthetic constructor; PA15 validates that constructor and derives object size
and alignment from the canonical target `TypeId`/`RecordLayout`.  Alignment is
still validated as layout metadata, but it no longer prevents an exact-width
LowIR scalar clear of the object representation.  Thus `obj<8x4>` is cleared by
one `store i64 0`, followed by the existing nontrivial member constructor call.

The zeroing remains before constructor actions, and no PA12 owner, constructor
fact, member order, or aggregate appertainment path changed.  The implementation
does not recover type/layout facts from spelling or duplicate analysis/lowering.
The six focused handout controls cover ordinary aggregate initialization,
default/member initialization, nested class subobjects, array value-init, and
the trivial functional-cast aggregate boundary.  The complete PA16 run removed
only the intended target identity.

## Performance Evidence

For an object of `m` bytes, zero-store selection and emission are O(m/8)
with constant-width selection state; constructor action traversal remains the
existing O(members/layout facts) typed walk.  The change adds no scans,
allocations, retries, caches, or alternate lowering path.  Final code-quality
evidence is the representative target reduction from two i32 stores plus an
offset projection to one i64 store before the constructor call.  A bounded
single-run target compile smoke completed successfully with `elapsed=0.00`,
`user=0.00`, `sys=0.00`, and `maxrss_kb=6160`; this is smoke evidence, not a
scaling claim.

## Validation

Final validation:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- `make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/300-value-init-aggregate-with-nontrivial-member.t tests/general/300-array-member-empty-paren-value-init.t tests/general/300-value-init-empty-functional-cast-aggregate.t tests/general/200-aggregate-class-member-subobject-init-target.t tests/general/200-member-initializer-aggregate-member.t tests/general/100-default-member-initializer-aggregate-member.t'`: status `0`, `PASS (6/6)`.
- `make test-pa16`: status `2` (residual failures), `217/243` passed,
  `26` failures, and `243/243` identities covered; exact baseline/final set
  comparison is baseline-only target, fresh-only `0`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`: status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`,
  with five pre-existing `bad-division` warnings in the known headers.
- The target artifact has `EXIT_SUCCESS` and an empty canonical comparison
  diff; the emitted constructor body contains one `store i64 0` and no offset
  projection for the zero clear.
- Bounded target compile smoke: status `0`, single-run timing recorded above.
- `git diff --check`: status `0`; final changed-path and tracked-artifact
  checks found only the approved production and plan files, with no handout,
  fixture, reference, harness, comparator, or generated artifact tracked or
  staged.

## Next Checkpoint

This bounded value-initialization checkpoint is landed at `217/243`, with
`26` residual failures.  The distinct next residual boundary is associated-
namespace ADL lookup, represented by
`pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`;
keep the typed aggregate zero-store semantics closed to this checkpoint.

## Checkpoint Ledger

| checkpoint | result | status |
| --- | --- | --- |
| `1694bc3e` bit-field baseline | `200/243`, 43 failures, `243/243` covered | prior landed |
| `7e060b28` packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior landed |
| `d95a6fe7` local-class start | `202/243`, 41 failures, `243/243` covered | prior checkpoint |
| `d83e927f` local-class materialization | `206/243`, 37 failures, `243/243` covered | prior landed |
| `70327e4d` exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | prior landed |
| `3b2b4882` checkpoint audit baseline | `208/243`, 35 failures, `243/243` covered | prior baseline |
| `ee8f44d5` typed array cleanup | `209/243`, 34 failures, `243/243` covered | prior landed |
| `08472cce` typed pragma-pack layout | prior landed pack-layout checkpoint | prior landed |
| `9f7101ac` pack-layout audit | `210/243`, 33 failures, `243/243` covered | prior landed |
| `fb4f46ed` placement-new semantic/lowering | `211/243`, 32 failures, `243/243` covered | prior landed; historical |
| `85b819b7` pre-increment authority | `211/243`, 32 failures, `243/243` covered | prior baseline |
| `typed truth-width continuity (parent 85b819b7)` | final `214/243`, 29 failures, `243/243` covered; authority-only 3 named identities; fresh-only 0; through-PA15 `1167/1167`; audit 0 with five known warnings; diff-check 0 | landed in this checkpoint commit |
| `96e80152` truth-width checkpointAudit | Focused build `0`, PA16 `7/7`, PA15 `5/5`; fresh PA16 status `2` at `214/243` with authority/fresh `29/29` failures, baseline-only/fresh-only `0/0`, and `243/243` coverage; through-PA15 `1167/1167`; file audit `0` with five pre-existing warnings; final diff/path audits `0`; exact-pointee class-pointer guard repaired | completed audit |
| `a5c8e166` typed packed-bit-field value/update checkpointAudit | Final PA16 status `2` at `215/243`, exactly `28` failures and `243/243` covered; independent comparison authority/fresh `28/28`, authority-only/fresh-only `0/0`, inventory/run total `243/243`; landed delta is exactly baseline-only `400-bitfield-aggregate-init.t`; through-PA15 `0` at `1167/1167`; file audit `0` with five known warnings; focused 412/422/424, probes, diff-check, and path audit pass. Durable evidence is under `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-plain-int-bitfield-checkpoint-audit-20260830/`; no forbidden surface changed | completed audit |
| `1d7e6860` alias direct-base mem-initializer checkpoint | final `216/243`, `27` failures, `243/243` covered; exact authority/final failure comparison `27/27`, authority-only/fresh-only `0/0`; focused `6/6`, courses `408/409/418/425`, through-PA15 `1167/1167`, file audit `0` with five known warnings, smoke, and diff/path checks pass; four approved paths ready for commit | completed |
| `typed aggregate value-initialization zero-store checkpoint` | final `217/243`, `26` failures, `243/243` covered; exact delta is baseline-only target and fresh-only `0`; through-PA15 `1167/1167`; file audit `0` with five known warnings; focused `6/6`; bounded compile smoke `0.00s`, `6160 KB` | landed |
