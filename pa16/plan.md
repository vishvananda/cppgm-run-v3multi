# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10/PA11 typed syntax, lookup, and
layout become PA12 semantic facts and PA15 LowIR.  This checkpoint consumes
PA12's typed value-initialized constructor action and PA15's canonical
`TypeId`/`RecordLayout`/`LowType` facts to clear the complete object
representation, then retains the existing declaration/member/array
constructor order.

This follows `spec.md` §§1--5 and 7: facts remain typed and single-owned,
there is no spelling transport, duplicate analyzer, retry, second lowerer, or
host/reference shortcut, and the bounded evidence is structural rather than
an unsupported timing claim.  The PA13 `zeroinit` bulk contract remains
available, but substituting it is not required for this bounded scalar-width
correctness/representation fix; that substitution would be a distinct public
LowIR-shape optimization.  The current ordinary-store path is valid on the
specified LowIR/Linux x86_64 boundary.

## Failure Map and Authority

The supplied turn-start/current authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`217/243` passed, exactly `26` failed, and `243/243` identities are covered.
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
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The pre-landed parent baseline `d503a9c0` was `216/243` with exactly `27`
failures and full coverage.  Its complete 27-name map is exactly the current
26-name map above plus
`pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`.  That
27-name set is retained as baseline context; it is not the turn-start/current
authority.

StageProgressPreserved compares the fresh final PA16 run against the actual
turn-start/current 26-failure set: final failures must be at most `26`, all
`243` identities must be discovered and covered, and fresh-only failure
identities must be `0`.  Extra passes cannot compensate for a fresh failure.

## Active Checkpoint

The landed source change is confined to
`dev/src/pa15_lowering_construction.cpp`, in
`zero_initialize_value_initialized_object`.  PA12 publishes
`value_initialize` and the selected synthetic constructor.  Existing PA15
owners route automatic variables and constructor actions through
`initialize_constructor_value`, `lower_constructor_action`, and
`emit_constructor_elements`; the five existing helper call sites preserve
the typed target, constructor record, destination projection, and
declaration/array order.

For a complete object, `LowType` supplies layout-derived bytes and alignment.
The helper validates the span, then advances a byte offset with 8/4/2/1
integer stores selected by remaining bytes and offset only.  Nonzero offsets
use an `i8` index.  Every width is within the remaining span and the final
offset is exactly the object size, including scalar tails.  The landed change
removes only the incorrect object-alignment divisibility gates.  LowIR
ordinary stores have no alignment operand; the validator and Linux x86_64
backend permit the resulting unaligned integer stores.  Thus `obj<8x4>` gets
one `store i64 0` before its nontrivial member constructor, including when the
actual subobject address is under-aligned by packing.

Recursive array/nested-object addressing and constructor actions are
unchanged.  Zeroing precedes the constructor's EH region; stores do not
throw, and existing completed-element cleanup still destructs only
successfully constructed prior elements in reverse order.  No source repair or
additional regression is justified.

## Performance Evidence

The helper is one bounded pass with O(bytes/8) scalar stores and constant
selection state.  It adds no scan, allocation, retry, cache, or alternate
lowerer.  Focused generated LowIR structurally shows the intended reduction
from two `i32` stores plus an offset projection to one `i64` store, four
stores for the 32-byte aggregate control, and four ordered stores for the
array control.  No timing or RSS claim is made.

## Validation Status

Final validation:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- `make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/300-value-init-aggregate-with-nontrivial-member.t tests/general/300-array-member-empty-paren-value-init.t tests/general/300-value-init-empty-functional-cast-aggregate.t tests/general/200-aggregate-class-member-subobject-init-target.t tests/general/200-member-initializer-aggregate-member.t tests/general/100-default-member-initializer-aggregate-member.t'`: status `0`, `PASS (6/6)`.
- `make test-pa16`: status `2`, `217/243` passed, exactly `26` failures,
  and `243/243` identities covered.
- `KEEP_GOING=1 CPPGM_CHECK_MODE=1 CPPGM_CHECK_AUTO_KEEP_GOING=1 scripts/compare_results.pl ref my tests` (from `pa16`): status `1` because the expected residuals remain; exact comparison against the supplied current authority is `26` vs `26`, fresh-only `0`, authority-only `0`.
- Recursive status-artifact audit: `243` tests discovered, `243` reference
  statuses, `243` fresh statuses, `243` covered, missing `0`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`: status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`
  with five known `bad-division` warnings in existing headers.
- `git diff --check`: status `0`.

The fresh final failure set is exactly the current 26-name authority map;
the pre-landed 27-name parent baseline remains historical context only.

## Next Checkpoint

Keep this value-initialization/zero-store boundary closed.  The next distinct
residual boundary is associated-namespace ADL lookup:
`pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`.

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
| `31a938ac` typed aggregate value-initialization compact zero-store checkpointAudit | Fresh final `217/243`, `26` failures, `243/243` covered; current-authority comparison `26` vs `26`, fresh-only `0`, authority-only `0`; focused matrix `6/6`; through-PA15 `1167/1167`; file audit `0` with five known header warnings; no source repair | completed audit |
