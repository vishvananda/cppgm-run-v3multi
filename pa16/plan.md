# PA16 implementation plan

## 1. Stage Design

This checkpoint is bounded to the PA16 purpose and `spec.md` Purpose plus
sections 1, 2, 5, and 7.  N3485 [expr] p11 and [expr.static.cast] p6 make an
explicit conversion to `void` a discarded-value expression: the operand is
evaluated, but ordinary nonvolatile lvalue-to-rvalue conversion is not
generally implied.  The residual PA16 fixtures require a stable O0 read at
one typed boundary, so the implementation keeps that exception narrow.

The production data flow remains one typed pipeline:

```text
PA10 syntax
  -> PA11 canonical types/bindings/scopes
  -> PA12 semantic_cast_to_target: CastExpression + ToVoid conversion fact
  -> PA15 CastExpression validation + typed discarded lowering
  -> typed LowIR
```

PA12 owns the selected `ConversionKind::ToVoid` fact and its typed child.
PA15 validates the parent cast's single `ToVoid` record and passes an explicit
`DiscardedExpressionContext::ExplicitToVoid` to the existing discarded-value
consumer.  The extra scalar read is enabled only for a direct `IdExpression`
that is an lvalue of scalar object type, has a valid non-reference
`BindingKind::Parameter` binding, and therefore has initialized formal storage
at function entry.  Ordinary local variables and other lvalue-producing facts
remain non-materializing.

The existing function/reference early exits, volatile reads, class-lvalue
address materialization, comma and void-conditional sequencing, and
assignment/increment/decrement effect paths remain intact.  No source spelling,
name recovery, lookup retry, or parallel semantic path is introduced.  The
invariants are typed conversion ranges, direct fact/binding ownership, source
evaluation order in LowIR, and no redundant assignment or increment result
load.  The decision reads one fact, binding, and type tuple: O(1) time and
O(1) additional storage per discarded fact, with no cache or whole-program
scan.

## 2. Failure Map

Turn-start authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` at
landed HEAD `14cadc0c135156ed20583e3b5adb07b1260cabe2`: `222/243` passing,
`21` failures, and `243/243` identities covered.

The complete authoritative turn-start failure set is:

```text
pa16/tests/general/100-function-pointer-nested-param-name-shadow.t
pa16/tests/general/200-elaborated-member-forward-type.t
pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-callable-field-hides-private-base-method.t
pa16/tests/general/300-enum-class-nonmember-operator-bitand.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-operator-nullptr-t-from-zero.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

Fresh `make test-pa16` evidence is `224/243` passing, `19` failures, exit
status `2`, with exactly `243/243` discovered/reference/fresh identities.
The fresh residual identities are:

```text
pa16/tests/general/200-elaborated-member-forward-type.t
pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-callable-field-hides-private-base-method.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-operator-nullptr-t-from-zero.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The exact delta is turn-start-only:
`100-function-pointer-nested-param-name-shadow.t` and
`300-enum-class-nonmember-operator-bitand.t`; fresh-only is `0`.  Missing and
unexpected counts are `0` for all three inventories.  No unrelated residual
identity is reclassified.  Durable broad and comparison logs are
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-final.log`
and
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-final-identity-delta.log`.

## 3. Active Checkpoint

The selected boundary is the PA12 `ConversionKind::ToVoid` child consumed by
the PA15 `CastExpression` case.  In
`pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`,
`(void)on_immediate` now emits `load ptr $on_immediate` before the remaining
parameter loads and call.  The enum value parameters in
`300-enum-class-nonmember-operator-bitand.t` receive the same typed treatment.

Focused validation after the narrowed correction:

```text
make -C dev cppgm++ CXX=g++                                      # status 0
make -C pa16 check TEST=tests/general/100-function-pointer-nested-param-name-shadow.t
                                                                  # PASS (1/1)
make -C pa16 check TEST=tests/general/300-enum-class-nonmember-operator-bitand.t
                                                                  # PASS (1/1)
make -C pa15 check TEST='tests/general/200-literal-logical-short-circuit-omits-unreachable-call.t tests/general/200-for-iteration-discards-void-comma-rhs.t tests/general/200-comma-expression-xvalue-reference-return.t tests/general/200-return-void-call-expression.t'
                                                                  # PASS (4/4)
make -C pa16 check TEST='tests/general/200-derived-pointer-member-init.t tests/general/200-derived-pointer-overload-prefers-base-over-void.t tests/general/200-const-subobject-member-call.t'
                                                                  # PASS (3/3)
```

The external temporary probe at
`/tmp/pa16-to-void-probe.HGWxRf/probe.source` compiled with the dev compiler
at `--emit-lowir -O0` and all structural assertions passed.  It proves the
explicit parameter, uninitialized-local, ordinary scalar/reference, and
assignment/increment cases without adding a checked-in test or fixture.  Its
counter record is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-to-void-probe.log`.

The exact prior-through gate was run as requested with `n=16`: status `0`,
`ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)`, recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa15-through-pa16-final.log`.
The exact file audit passed with status `0` and five pre-existing
header-division warnings.  The existing course PA15 discarded-return script
was not counted as a gate because its first assertion still expects the
baseline spelling `return ptr 0` while the compiler emits `return ptr nullptr`;
its later discarded-value assertions were not used as substitute evidence.

Remaining uncertainty is limited to the known 19 unrelated residuals and
future lvalue-producing shapes outside the parameter boundary; the broad run
introduced no new identity and retained full coverage.

## 4. Performance Evidence

The risk is one extra scalar load and corresponding LowIR growth at the
explicit typed boundary.  The probe compiled eight functions and emitted 22
instruction lines; the seven checked functions emitted 21.  Exact counters
(instruction lines, `load ptr`, `load i32`) are:

| Probe function | Instructions | `load ptr` | `load i32` |
| --- | ---: | ---: | ---: |
| `explicit_pointer` | 3 | 1 | 0 |
| `explicit_enum` | 3 | 0 | 1 |
| `explicit_uninitialized_local` | 1 | 0 | 0 |
| `ordinary_scalar` | 2 | 0 | 0 |
| `ordinary_reference` | 2 | 0 | 0 |
| `assignment_control` | 4 | 1 | 0 |
| `increment_control` | 6 | 1 | 1 |

The selected generated functions have `do_start_op`: 11 instructions and
four loads (`3` pointer, `1` i32), including exactly one `on_immediate` load;
the enum operator has 5 instructions and exactly two i32 loads.  These are
structural IR counters, not a timing claim.  The implementation adds at most
one local load decision per matching parameter fact, with no allocation, cache,
whole-program traversal, dependency edge, or repeated lowering.  The bounded
O(1) branch and representative probe/broad IR counts show no material
performance risk; no benchmark conclusion is drawn from one sample.

## 5. Checkpoint Ledger

| Commit | Status |
| --- | --- |
| `24d555c8` | Completed the prior PA16 checkpoint audit; its focused and broad evidence, exact 21-identity comparison, full 243-identity coverage, file audit, and clean-tree verification remain historical record. |
| `PA16 ToVoid typed-boundary checkpoint (this final commit)` | Narrowed explicit typed discarded-value lowering to initialized non-reference scalar parameter lvalues, validated the parent `ToVoid` fact, preserved discarded-expression controls, and recorded focused/broad/probe/through-gate/file-audit evidence above. |
