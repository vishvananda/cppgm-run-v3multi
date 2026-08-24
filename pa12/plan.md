# PA12 callable-resolution checkpoint plan

## Stage Design

The production route remains one typed owner:

`PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 typed facts -> cold deterministic dump`

PA11 owns canonical types, bindings, scopes, and lookup. PA12 now classifies a
call as direct only when the resolved `IdExpression` names a function binding
set; an `IdExpression` naming a variable or parameter of function,
function-reference, or function-pointer type is analyzed as the indirect
callee. Overloaded function IDs are represented by a deferred typed
`FunctionIdResolution`. Target pointer/reference contexts inspect only the
relevant function candidates, then create one selected ID fact and its
conversion after the target is known. No candidate trial creates dump facts,
and no rendered name is reparsed.

Direct-call candidates retain deterministic lookup order. Each viable
candidate records one conversion rank per argument; an ellipsis tail receives
a rank above every supported standard conversion. A typed component-wise
ordering selects one maximal candidate or rejects ambiguity/no-viable-call.
Null integer zero conversion to pointer and `nullptr_t` is recorded at the
selected expression; reference temporary materialization records the required
typed cast fact. Ordinary arguments are analyzed once per call. With `C`
outer candidates, `A` arguments, `D` distinct deferred target slots, and `F`
relevant inner function candidates per target, ordinary scoring is O(C*A) and
uncached deferred classification adds O(C*D*F), hence O(C*A*F) in the bounded
case `D <= A`. The ordinary-argument probe is not evidence for that deferred
term. There are no whole-arena scans, retry loops, text-keyed semantic
caches, or hash-order output.

The PA11 core edits are limited to the two typed ownership gaps exposed by the
checkpoint: the assignment vocabulary's unqualified `nullptr_t` type lookup
and the AST representation of a named variadic parameter pack. They preserve
the existing canonical type and binding owners.

## Failure Map

The clean turn-start baseline is **103/166 passing, 63 failures**, with all
166 tests covered. The supplied earlier-stage baseline is **685/685 through
PA11**. Partition:

| checked-in partition | total | passed | failed |
| --- | ---: | ---: | ---: |
| `tests/spec/100-*.t` | 12 | 12 | 0 |
| `tests/general/100-*.t` | 25 | 25 | 0 |
| `tests/spec/200-*.t` | 9 | 8 | 1 |
| `tests/general/200-*.t` | 33 | 19 | 14 |
| `tests/spec/300-*.t` | 10 | 6 | 4 |
| `tests/general/300-*.t` | 77 | 33 | 44 |
| **all PA12 tests** | **166** | **103** | **63** |

The complete turn-start failure ownership is recorded below. After this
increment, full PA12 is **113/166 passing, 53 failing**, with all 166 covered:
all ten active-checkpoint paths pass, the exact current failure set is the same
53 residual paths listed below, and there are no extra failures or regressions.
The ten paths in the active checkpoint are the only callable/conversion
failures being changed; the remaining 53 retain explicit residual ownership
and are not hidden by added passing probes.

Checkpoint-owned paths (10):

```text
pa12/tests/general/200-bad-overloaded-function-id.t
pa12/tests/general/200-function-reference-call.t
pa12/tests/general/200-overloaded-function-argument.t
pa12/tests/general/200-qualified-overloaded-function-argument.t
pa12/tests/spec/200-function-pointer-call.t
pa12/tests/spec/300-ellipsis-worse-than-pointer-overload.t
pa12/tests/spec/300-nullptr-t-from-zero-overload.t
pa12/tests/spec/300-ranked-prefix-before-ellipsis-slot.t
pa12/tests/general/300-pointer-conversion-ranking.t
pa12/tests/general/300-reference-binding-ranking.t
```

Current residual failure paths (53), owned by later checkpoints or unrelated
PA12 families:

```text
pa12/tests/general/200-block-scope-using-declaration.t
pa12/tests/general/200-builtin-constant-p-propagated-expression.t
pa12/tests/general/200-constexpr-complete-object-cv.t
pa12/tests/general/200-function-pointer-array-deduced-bound.t
pa12/tests/general/200-local-alias-postfix-cv-declaration.t
pa12/tests/general/200-local-alias-statement.t
pa12/tests/general/200-local-anonymous-union-variable.t
pa12/tests/general/200-local-direct-initialization.t
pa12/tests/general/200-local-using-directive-preserves-nearer-namespace-type.t
pa12/tests/general/200-using-declaration-call.t
pa12/tests/general/300-array-xvalue-subscript.t
pa12/tests/general/300-block-anonymous-union-injected-members.t
pa12/tests/general/300-block-elaborated-enum-type-use.t
pa12/tests/general/300-builtin-abort-semantics.t
pa12/tests/general/300-builtin-constant-p-call.t
pa12/tests/general/300-decltype-functional-cast.t
pa12/tests/general/300-deref-function-returning-pointer-reference.t
pa12/tests/general/300-deref-string-literal.t
pa12/tests/general/300-elaborated-local-struct-copy-init.t
pa12/tests/general/300-enum-comparisons.t
pa12/tests/general/300-floating-arithmetic-comparisons.t
pa12/tests/general/300-floating-conditional-common-type.t
pa12/tests/general/300-floating-inc-dec.t
pa12/tests/general/300-integral-compound-assign-unscoped-enum.t
pa12/tests/general/300-local-extern-function-declaration.t
pa12/tests/general/300-member-function-pointer-return-pointer-const.t
pa12/tests/general/300-member-function-pointer-type-alias-and-function.t
pa12/tests/general/300-member-pointer-type-alias-and-function.t
pa12/tests/general/300-multidimensional-array-const-reference-binding.t
pa12/tests/general/300-namespace-function-body-later-anonymous-overload.t
pa12/tests/general/300-nullptr-equality.t
pa12/tests/general/300-pointer-bool-conversion.t
pa12/tests/general/300-pointer-nullptr-conditional.t
pa12/tests/general/300-pointer-plus-anonymous-enum.t
pa12/tests/general/300-pointer-subtraction-cv-compatible.t
pa12/tests/general/300-postfix-reference-return-call-inc.t
pa12/tests/general/300-prefix-reference-return-call-inc.t
pa12/tests/general/300-qualified-direct-function-hides-using-directive.t
pa12/tests/general/300-reference-binding-pointee-const-pointer.t
pa12/tests/general/300-reopened-unnamed-namespace-call.t
pa12/tests/general/300-scoped-enum-functional-cast-integral.t
pa12/tests/general/300-simple-assignment-reference-lhs.t
pa12/tests/general/300-static-cast-member-overload-prefers-nontemplate.t
pa12/tests/general/300-static-cast-overloaded-function-template-argument.t
pa12/tests/general/300-subscript-commuted-expression.t
pa12/tests/general/300-subscript-unscoped-enum-index.t
pa12/tests/general/300-switch-scoped-enum-condition.t
pa12/tests/general/300-unnamed-namespace-definition.t
pa12/tests/general/300-unnamed-namespace-qualified-call.t
pa12/tests/general/300-unnamed-namespace-unqualified-call.t
pa12/tests/general/300-unscoped-enum-integral-operators.t
pa12/tests/general/300-zero-arg-functional-cast-alias.t
pa12/tests/spec/300-block-scope-namespace-alias-qualified-call.t
```

## Active Checkpoint

Implemented in the coherent checkpoint commit:

- direct namespace/function overload-set calls are separated from indirect
  variable/parameter calls, preserving the selected `BindingId`, `ScopeId`,
  canonical type, value category, and call result/reference category;
- overloaded IDs, including qualified IDs, resolve against pointer/reference
  initialization and argument targets only after the target type is known;
- direct overload ordering compares every fixed argument, treats ellipsis as
  worse than a valid standard conversion, handles ranked fixed prefixes, and
  rejects incomparable maxima; target-directed function-ID ranking tracks the
  global best conversion and only rejects a final best-rank tie;
- supported null integer zero and `nullptr_t` conversions preserve the cold
  dump shape, while temporary reference conversions publish the expected cast
  fact;
- unnamed parameters retain the checked-in two-space dump spelling.

Changed paths are exactly:

```text
dev/src/pa12_semantic.cpp
dev/src/pa11_semantic_model.h
dev/src/pa11_semantic_core.cpp
pa12/plan.md
```

Build and focused validation:

```text
make -C pa12
```

passed, with only the pre-existing cast-path warning. The focused checked-in
command covered the ten checkpoint paths plus 18 nearby positive/negative
controls and passed **28/28**, with exact success dumps and expected failure
statuses. The exact command was:

```text
make -C pa12 check TEST='tests/spec/200-function-pointer-call.t tests/general/200-function-reference-call.t tests/general/200-bad-overloaded-function-id.t tests/general/200-overloaded-function-argument.t tests/general/200-qualified-overloaded-function-argument.t tests/spec/300-ellipsis-worse-than-pointer-overload.t tests/spec/300-ranked-prefix-before-ellipsis-slot.t tests/spec/300-nullptr-t-from-zero-overload.t tests/general/300-pointer-conversion-ranking.t tests/general/300-reference-binding-ranking.t tests/general/100-variadic-call-fixed-prefix.t tests/general/200-paren-argument-list-call.t tests/general/100-integer-zero-to-pointer-call.t tests/general/100-nullptr-to-pointer-return.t tests/spec/300-nullptr-pointer-conversion.t tests/spec/100-simple-call.t tests/spec/100-overload-exact.t tests/spec/100-overload-ranking.t tests/spec/300-bad-ambiguous-overload.t tests/general/300-overload-no-global-best-bad.t tests/general/300-bad-indirect-call-too-few-arguments.t tests/general/300-bad-indirect-call-too-many-arguments.t tests/general/300-indirect-call-argument-conversion-bad.t tests/general/200-function-decay-deref-call.t tests/general/200-deep-pointer-qualification-conversion.t tests/general/300-bad-deep-pointer-qualification-conversion.t tests/spec/300-nullptr-t-vs-long-overload-bad.t tests/spec/300-enumerator-is-not-null-pointer-constant-bad.t'
```

An adjacent nine-test control command passed **7/9**; its two failures,
`general/300-pointer-bool-conversion.t` and
`general/300-pointer-nullptr-conditional.t`, are both in the supplied
turn-start failure set and remain residual explicit-cast/conditional families.
The authorized broad `make test-pa12` run passed **113/166**, covered all 166,
and left exactly the 53 residual paths listed above. The required through-PA11
report passed **685/685**.

## Performance Evidence

The earlier immutable direct-argument probe is ordinary-only and does not
measure deferred target-ID work. The corrected executable used for the
representative probe was `/tmp/pa12-cppgm-corrected-immutable`, SHA-256
`abe033b89584757bf759eac15247d434356323621da0fc08e30fb947dfd39c95`.
The out-of-tree target-directed probes each contain one `use(inner)` call,
where `inner` is an overload set and `use` has function-pointer/reference
parameter overloads. The small case `/tmp/pa12-nested-c8-f4.t` has C=8 outer
and F=4 inner candidates, 14 source lines, a 27-line dump, two call
expressions, one selected inner ID, and one reference callee. The larger
`/tmp/pa12-nested-c32-f32.t` has C=31 outer and F=32 inner candidates, 65
source lines, a 78-line dump, two call expressions, one selected inner ID,
and one reference callee. Thus C*F grows from 32 to 992 (31x).
Each repetition invoked
`/tmp/pa12-cppgm-corrected-immutable --emit-semantics -o <dump> <probe>`;
the executable and both probe inputs remained unchanged throughout timing.

Five interleaved samples ran each immutable probe 40 times. Every sample was
**40/40 successful** for each case (200/200 per case overall):

| case (C,F) | wall samples (s) | user samples (s) | system samples (s) | peak RSS samples (KiB) |
| --- | --- | --- | --- | --- |
| small (8,4) | 0.15, 0.14, 0.14, 0.14, 0.15 | 0.07, 0.06, 0.07, 0.06, 0.06 | 0.08, 0.08, 0.07, 0.09, 0.09 | 4916, 4908, 4924, 4900, 4904 |
| large (31,32) | 0.43, 0.42, 0.42, 0.42, 0.42 | 0.23, 0.19, 0.21, 0.21, 0.21 | 0.20, 0.23, 0.21, 0.21, 0.21 | 7144, 7148, 7148, 7148, 7152 |

The observed approximately 3x wall increase is below the necessary 31x
candidate-product growth and shows no materially worse scaling. Deferred
classification remains typed and fact-free until selection; no text-keyed
cache or speculative semantic fact was added.

## checkpoint ledger

| row | evidence / ownership |
| --- | --- |
| turn-start | Clean `498043c5`; PA12 **103/166**, 63 failures, all 166 covered; supplied through-PA11 **685/685**; complete failure set listed above. |
| implementation | Three implementation paths plus this plan; no tests, refs, grammar, harnesses, or scripts changed. |
| focused result | Build pass; checkpoint/control focus **28/28**; adjacent controls **7/9** with two pre-existing residual failures named above. |
| performance | Corrected immutable executable; nested C/F probe 5x40 interleaved, 200/200 successful per case, measurements and structural counts recorded above. |
| broad PA12 | `make test-pa12`: **113/166 passed, 53 failed**, all 166 covered; ten checkpoint failures cleared, no extra regressions, exact residual set listed above. |
| through-PA11 | `n=12; make test-report-through-pa11`: **ALL TESTS PASSED SUCCESSFULLY! (685 / 685)**. |
| file audit | `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src`: passed with the two existing header-division warnings. |
| diff check | `git diff --check`: passed. |
| final state | This checkpoint commit contains the final plan state; the worktree is clean and no pending mutations remain. |
