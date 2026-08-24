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
and the AST representation of a named variadic parameter pack. The lookup
fallback now consults an ordinary alias first and synthesizes the assignment
vocabulary type only when no binding exists; the pack walk covers nested
pointer/reference declarators but stops at nested function-type clauses.
These edits preserve the existing canonical type and binding owners.

## Failure Map

The clean turn-start and post-repair results are both **113/166 passing,
53 failures**, with all 166 tests covered. The earlier-stage gate is
**685/685 through PA11**. The current partition is:

| checked-in partition | total | passed | failed |
| --- | ---: | ---: | ---: |
| `tests/spec/100-*.t` | 12 | 12 | 0 |
| `tests/general/100-*.t` | 25 | 25 | 0 |
| `tests/spec/200-*.t` | 9 | 9 | 0 |
| `tests/general/200-*.t` | 33 | 23 | 10 |
| `tests/spec/300-*.t` | 10 | 9 | 1 |
| `tests/general/300-*.t` | 77 | 35 | 42 |
| **all PA12 tests** | **166** | **113** | **53** |

The complete turn-start and current failure ownership is recorded below. All
ten active checkpoint paths pass, and the exact 53 residual paths remain
listed below. The ten paths are the landed callable/conversion gains. The
audit repairs additionally close two PA11 ownership gaps—ordinary alias
precedence and nested named-pack traversal—without changing fixture
inventory. The remaining 53 retain explicit residual ownership and are not
hidden by added passing probes.

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

Landed in the coherent checkpoint commit:

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

Repaired in this checkpoint audit:

- ordinary unqualified type lookup now preserves an existing `nullptr_t`
  alias before the synthetic assignment-vocabulary fallback;
- named variadic-pack markers remain attached to the enclosing parameter
  declarator, without leaking nested function-type ellipses.

Reviewed implementation and record paths for this checkpoint are:

```text
dev/src/pa12_semantic.cpp
dev/src/pa11_semantic_model.h
dev/src/pa11_semantic_core.cpp
pa12/plan.md
pa12/audit.md
```

The final audit commit changes exactly `dev/src/pa11_semantic_core.cpp`,
`dev/src/pa11_semantic_model.h`, `pa12/plan.md`, and `pa12/audit.md`;
`dev/src/pa12_semantic.cpp` was inspected and is unchanged.

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
The required broad `make test-pa12` run exited **2** with **113/166 passed**,
covered all 166, and left exactly the 53 residual paths listed above. Failure
normalization against
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
found **53 baseline**, **53 current**, **0 current-only**, and
**0 baseline-only** paths. The exact required through-PA11 command printed
`===== ALL TESTS PASSED SUCCESSFULLY! (685 / 685) =====`.

Out-of-tree focused probes also passed: qualified and parenthesized function
IDs in pointer/reference initialization and arguments; named `int... args`
packs in direct and nested pointer/reference declarators plus a nested
function-type control; and a user `using nullptr_t = int` alias after the
fallback repair. These probes are not fixtures.

## Performance Evidence

The prior cited timing artifacts are absent, so this audit gathered a fresh
out-of-tree replacement from the repaired build. The immutable executable is
`/tmp/pa12-cppgm-checkpoint-audit-v2-immutable`, SHA-256
`3e2d092dc1cf8968187179e30d9f9c447079034d297ca8aca635e594000551e7`; the
unchanged inputs are `/tmp/pa12-nested-c8-f4.t` (SHA-256
`777e23a45637dd43fb8f07448c055a928c645c855384649e9ce1ac12465cd851`) and
`/tmp/pa12-nested-c31-f32.t` (SHA-256
`3954e3cd7f7054b3c0e8f6b1ec7083278d1b6899f0a97f98a83b6fc4336c6048`). Both
workloads use one direct `use(inner)` call with A=4 arguments and D=1
deferred function-ID slot; only C outer candidates and F inner lookup
candidates vary:

| case (C,F) | source lines | dump lines | C*F | selected inner IDs |
| --- | ---: | ---: | ---: | ---: |
| small (8,4) | 13 | 106 | 32 | 1 |
| large (31,32) | 64 | 462 | 992 | 1 |

The fixed argument shape and target type are otherwise equivalent. Candidate
scoring therefore performs C*(A-D) ordinary conversions plus C*D*F deferred
candidate checks; the measured C*F term grows 31x. Five interleaved rounds ran
each immutable input 40 times, with **200/200 successful invocations per
case**:

| case | wall samples (s) | user samples (s) | system samples (s) | peak RSS samples (KiB) |
| --- | --- | --- | --- | --- |
| small | 0.14, 0.14, 0.14, 0.13, 0.14 | 0.07, 0.07, 0.07, 0.07, 0.06 | 0.06, 0.06, 0.06, 0.06, 0.07 | 4924, 4924, 4912, 4896, 4920 |
| large | 0.27, 0.28, 0.28, 0.27, 0.27 | 0.14, 0.14, 0.15, 0.13, 0.13 | 0.13, 0.13, 0.13, 0.13, 0.13 | 6188, 6204, 6192, 6188, 6196 |

Median wall time is about 1.9x while C*F is 31x; the timing includes the
larger parse, semantic traversal, and cold dump, so it is evidence against a
worse-than-bounded candidate path rather than an isolated resolver benchmark.
Repeated dumps were byte-identical. Deferred classification remains typed and
fact-free until selection; no text-keyed cache or speculative semantic fact
was added.

## Next checkpoint

The next checkpoint is a separately authorized residual-family pass, beginning
with the namespace/using and expression/control paths in the 53-path map (for
example `general/200-block-scope-using-declaration.t` and
`general/200-builtin-constant-p-propagated-expression.t`). It must preserve
the current exact failure baseline while addressing those families; this
checkpoint does not claim PA12 complete.

## checkpoint ledger

| row | evidence / ownership |
| --- | --- |
| turn-start | Clean `5fa28b37`; supplied PA12 **113/166**, 53 failures, all 166 covered; supplied through-PA11 **685/685**; complete failure set listed above. |
| implementation | Three implementation paths plus this plan and audit; bounded repairs make ordinary `nullptr_t` lookup win over the synthetic fallback and keep nested named packs attached to the enclosing parameter; no tests, refs, grammar, harnesses, or scripts changed. |
| focused result | Build pass; checkpoint/control focus **28/28**; adjacent controls **7/9** with two pre-existing residual failures named above; out-of-tree ownership probes passed. |
| performance | Fresh immutable repaired executable; nested C/F probe 5x40 interleaved, 200/200 successful per case, equivalent A/D shape and measurements recorded above. |
| broad PA12 | Required `make test-pa12` exited **2**: **113/166 passed, 53 failed**, all 166 covered; normalized against the supplied baseline: **53 current, 53 baseline, 0 current-only, 0 baseline-only**. |
| through-PA11 | Required `n=12; if [ "$n" -le 1 ]; then ...; else make test-report-through-pa$((n - 1)); fi`: **ALL TESTS PASSED SUCCESSFULLY! (685 / 685)**. |
| file audit | Post-repair `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src`: passed with exactly the two existing header-division warnings. |
| diff check | `git diff --check`: passed; changed-path audit found only the four approved files and no tests, refs, fixtures, grammar, harnesses, scripts, generated artifacts, or unrelated files. |
| checkpointAudit | Review anchored at `5fa28b37`; typed callable trace, exact 53-path map, fresh immutable C/A/D/F evidence, broad/through-stage gates, and the bounded alias plus nested-pack repairs are finalized in this checkpoint record. |
| final state | PA12 remains incomplete with the 53 residual paths. The final audit commit contains the four approved changed files, and the next checkpoint is the separately authorized residual-family pass described above. |
