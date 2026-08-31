# PA16 nested braced aggregate-member checkpoint

## Stage Design

PA10 records `Value{...}` as one `CallExpression` carrying a
`BracedInitList`; PA11 retains that AST node without reparsing.  PA12's
`AggregateAppertainer` recognizes the same-record class expression and passes
its list's typed literal children to the existing
`semantic_aggregate_constructor_value` and `select_constructor` paths.  That
owner publishes a `ConstructorAction` with selected binding/scope/callable
type and semantic argument edges.  PA15 consumes the action from the
aggregate range directly; its conservative memoized proof suppresses only a
side-effect-free call to an empty, memberless user constructor after retaining
the typed subobject address.  Ordinary constructors and side-effecting or
volatile arguments still lower as calls.  The flow follows `spec.md` Purpose
and §§1–5,7 and the PA16 aggregate/constructor boundary: one forward typed
pipeline, no textual key, reparse, or host/ref compiler invocation.

## Failure Map

Turn-start authority was `234/243` with complete `243/243` identity coverage
and exactly these nine residual failures:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
3. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
4. `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
5. `pa16/tests/general/300-friend-function-definition-skip.t`
6. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
7. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
8. `pa16/tests/general/400-signed-bit-field-read.t`
9. `pa16/tests/general/400-signed-enum-bit-field-read.t`

All `243/243` identities remain represented.  Final validation reports
`235/243`; the target is removed with no new or baseline-only failure.  The
exact eight residual identities are:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
3. `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
4. `pa16/tests/general/300-friend-function-definition-skip.t`
5. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
6. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
7. `pa16/tests/general/400-signed-bit-field-read.t`
8. `pa16/tests/general/400-signed-enum-bit-field-read.t`

## Active Checkpoint

This completed checkpoint covers
`pa16/tests/general/200-nested-braced-member-aggregate-init.t`.  It aligns the
PA16 README aggregate-initialization/constructor boundary and `spec.md`
Purpose/§§1–5,7 with one typed flow: PA12 owns same-record brace appertainment
and constructor selection, publishing the action and argument IDs that PA15's
aggregate consumer lowers.  General temporary-object construction, copy/move
transfer, and raw braced function parameters are deliberately out of scope.
The exact target must pass with these controls:
`200-aggregate-class-member-subobject-init-target.t`,
`200-member-initializer-aggregate-member.t`,
`200-aggregate-array-member-brace-elision.t`,
`300-value-init-empty-functional-cast-aggregate.t`,
`200-direct-list-init-explicit-ctor.t`,
`200-copy-list-init-explicit-ctor-bad.t`, and the typed constructor boundary
regression 409, plus durable regression 429.  Constructor selection remains
the existing `O(C*A)` work for `C` candidates and `A` arguments; the direct
brace route adds one clause pass.  The no-op proof is memoized over typed fact
IDs and is bounded by `O((A+E) log(A+E))` with balanced-tree memo/recursion
sets, for `A` argument roots and `E` reachable child edges.  No test or
fixture is changed, so complete `243/243` identity coverage is preserved; the
target leaves the nine-item map at final `235/243`.  The eight residual owners
above remain out of scope.

## Performance Evidence

Brace parsing creates one AST list and one typed argument vector; it does not
scan source text again.  `AggregateAppertainer` consumes each initializer
clause once, while constructor selection retains the existing near-linear
`O(C*A)` candidate/argument work.  The PA15 no-op proof memoizes each reachable
typed fact and validates each child range once; `std::map`/`std::set` lookups
give the documented `O((A+E) log(A+E))` bound and `O(A+E)` temporary storage.
Regression 429 compiles a fixed 64-argument nested member constructor: its
pure case retains the typed field projection and helper definition while
omitting the call, while side-effecting and volatile-read cases retain the
argument evaluation and constructor call.  This is bounded structural
evidence only; no timing or RSS claim is made.

## Focused Evidence

The target previously failed in PA10 parsing.  After the typed parser/PA12
same-record aggregate boundary and conservative PA15 consumer correction,
these focused commands pass:

```text
make -C dev cppgm++
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-nested-braced-member-aggregate-init.t'
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-aggregate-class-member-subobject-init-target.t tests/general/200-member-initializer-aggregate-member.t tests/general/200-aggregate-array-member-brace-elision.t tests/general/300-value-init-empty-functional-cast-aggregate.t tests/spec/200-direct-list-init-explicit-ctor.t tests/general/200-copy-list-init-explicit-ctor-bad.t'
sh cppgm.tests/course/pa16/409-typed-constructor-boundary-regression.sh
sh -n cppgm.tests/course/pa16/429-nested-braced-aggregate-member-regression.sh
sh cppgm.tests/course/pa16/429-nested-braced-aggregate-member-regression.sh
```

The target reports `PASS (1/1)`, the six narrowly relevant PA16 controls report
`PASS (6/6)`, and regression 409 is silent with exit `0`.  The target LowIR
retains the member projection and helper definition while omitting only the
side-effect-free empty-memberless constructor call.  Regression 429 reports
`PASS` and independently checks 64 literal arguments, a side-effecting call,
and a volatile read.

The exact exit checks also pass their required criteria:

```text
make test-pa16                         # exit 2; TEST SUMMARY: 235 / 243
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
                                        # exit 0; ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
                                        # exit 0; six known warnings in abi_mangle.h,
                                        # cpp_semantic_core.h, lowir_model.h,
                                        # pa11_semantic_model.h, pa12_semantic_selection.h,
                                        # and pa15_lowering.h
git diff --check                         # exit 0
```

The final uncertainty is limited to the eight residual owners listed above;
general temporary-object and raw-braced-parameter paths remain out of scope.

## Changed Paths

The implementation, evidence, and plan paths in this checkpoint are:

- `dev/src/pa10_ast.cpp`, `dev/src/pa10_renderer.cpp`
- `dev/src/pa12_semantic_aggregate.cpp`
- `dev/src/pa15_lowering.h`, `dev/src/pa15_lowering_aggregate.cpp`,
  `dev/src/pa15_lowering_construction.cpp`
- `cppgm.tests/course/pa16/429-nested-braced-aggregate-member-regression.sh`
- `pa16/plan.md`

## Next Checkpoint

This checkpoint is complete at `235/243` with the target removed, complete
`243/243` identity coverage, and the eight residual owners still out of scope.
Future work begins with a separately reviewed residual owner.

## Checkpoint Ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Authority was 224/243 with 19 failures and complete identity coverage. |
| b58ddd2a | Typed `nullptr_t` carrier path completed through PA11/PA12/PA15. |
| e09d8223 | Recorded nullptr state: 225/243 authority, 18 residual failures, 243/243 inventories. |
| d5bf2600 | Typed constructor-overload/lifetime audit reached 227/243 with 16 residual failures. |
| 29d9c4ce | PA10 elaborated-member parameter repair/PA15 ABI ownership reached 228/243 with 15 residuals. |
| 69bbe800 | Empty-base layout/address projection reached 230/243 with 13 residuals. |
| 75f7944a | Empty-base identity validation audit completed; through-PA15 remained 1167/1167. |
| 2ca2323a | Clean turn-start state; baseline 230/243 and exact 13-item map. |
| 2cfa1111 | Final committed UDL checkpoint: PA16 231/243, exact 12 failures, 243/243 coverage, through-PA15 1167/1167, and file audit 0 with six known warnings. |
| 4a5bbdd5 | Clean turn-start baseline: PA16 231/243 with complete 243/243 coverage; through-PA15 and file audit passed. |
| `617c137a` typed object-call boundary audit | Completed committed audit/repair: final PA16 is exit `2` at `234/243` with the exact unchanged nine residual identities and `243/243` coverage; baseline-only/final-only/unrecognized comparison is `0/0/0`; focused 428/PA16/PA15/access controls pass; through-PA15 is `1167/1167`; file audit exits `0` with six known header-division warnings. |
| PA16 nested-braced aggregate-member checkpoint (completed) | PA10 braced type-expression boundary, PA12 same-record aggregate constructor action, and conservative memoized PA15 aggregate consumption reached `235/243` from `234/243`; the target is removed with exactly the eight listed residuals, no new/baseline-only failures, and complete `243/243` coverage. Through-PA15 is `1167/1167`; file audit passes with six known bad-division warnings; focused 429, audit, and `git diff --check` pass. |
