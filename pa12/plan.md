# PA12 scalar-expression semantic-boundary checkpoint

## Stage Design

The owner remains `PA11SemanticModel`. PA11 core owns block-enum publication, array-bound formation, and the declaration-shaped assignment AST/lookup classifier; `dev/src/pa12_semantic.cpp` owns expression, call, and braced-init semantic facts plus conversion ranking/application and PA12 dumping. The typed flow is PA10 AST -> PA11 canonical `TypeId`/`BindingId`/`ScopeId` facts -> PA12 `ExprInfo`/`SemanticFact` nodes -> recorded `ConversionFact` nodes -> the existing deterministic cold renderer. Enum identity, enumerator values, reference categories, anonymous-enum identity, and array bounds remain typed; types and names are not recovered from rendered text.

The landed boundary normalizes expression object/top-level cv types, performs unscoped-enum integral promotion, computes integral/floating common types, records built-in conversions, and preserves lvalue/xvalue result categories for assignment, conditional, inc-dec, and dereference. It handles xvalue arrays, subscript facts, target-directed array braced initializers, and the PA11-published block-enum bindings. This audit repaired the bounded conditional array-decay gap: array operands now form pointer operands for conditional common-type selection, including pointer/null cases, and their conversions are attached to the source facts.

Expression handlers use AST arity: binary/assignment nodes inspect two operands, conditionals three, braced initializers only local elements, and callable selection its existing candidate vector. Conversion facts are attached at the owning source expression for context, arithmetic, pointer/array decay, subscript, dereference, conditional, inc-dec, and compound-assignment conversions. No new whole-arena scan, retry loop, or rendered-name lookup was introduced. Anonymous-enum naming uses one model-owned ordinal counter.

## Spec Alignment

- Sections 1-2: one PPPreprocessingSession/typed `PPTokenBuffer` -> PA10 AST -> PA11 canonical identity -> PA12 typed facts pipeline; rendering is restricted to the requested dump boundary.
- Sections 3-4: selected bindings, conversions, value categories, array bounds, enum identities, and child ranges are recorded at their semantic owners; work is local to AST arity, local initializer elements, candidate vectors, and existing bounded lookup indexes.
- Section 7: this checkpoint records immutable executable/input hashes, repeated byte-identical outputs, and structural counters. It makes no timing or asymptotic scaling claim.

## Failure Map

Turn-start baseline: `d7bd97b7`, PA12 `120/166` passing, `46` failing, all `166/166` covered; through-PA11 was supplied passing. The active family is the first group.

Active built-in scalar-expression/conversion owner (20):

- `tests/general/200-function-pointer-array-deduced-bound.t`
- `tests/general/300-array-xvalue-subscript.t`
- `tests/general/300-deref-function-returning-pointer-reference.t`
- `tests/general/300-deref-string-literal.t`
- `tests/general/300-enum-comparisons.t`
- `tests/general/300-floating-arithmetic-comparisons.t`
- `tests/general/300-floating-conditional-common-type.t`
- `tests/general/300-floating-inc-dec.t`
- `tests/general/300-integral-compound-assign-unscoped-enum.t`
- `tests/general/300-nullptr-equality.t`
- `tests/general/300-pointer-bool-conversion.t`
- `tests/general/300-pointer-nullptr-conditional.t`
- `tests/general/300-pointer-plus-anonymous-enum.t`
- `tests/general/300-pointer-subtraction-cv-compatible.t`
- `tests/general/300-postfix-reference-return-call-inc.t`
- `tests/general/300-prefix-reference-return-call-inc.t`
- `tests/general/300-simple-assignment-reference-lhs.t`
- `tests/general/300-subscript-commuted-expression.t`
- `tests/general/300-subscript-unscoped-enum-index.t`
- `tests/general/300-unscoped-enum-integral-operators.t`

The other turn-start failures were explicitly excluded:

- Builtin declaration/constant semantics (3): `200-builtin-constant-p-propagated-expression.t`, `300-builtin-abort-semantics.t`, `300-builtin-constant-p-call.t`.
- Declaration, anonymous-type, and local declaration formation (6): `200-constexpr-complete-object-cv.t`, `200-local-anonymous-union-variable.t`, `300-block-anonymous-union-injected-members.t`, `300-block-elaborated-enum-type-use.t`, `300-elaborated-local-struct-copy-init.t`, `300-local-extern-function-declaration.t`.
- Member-pointer, cast/type-formation, and reference-binding families (8): `300-decltype-functional-cast.t`, `300-member-function-pointer-return-pointer-const.t`, `300-member-function-pointer-type-alias-and-function.t`, `300-member-pointer-type-alias-and-function.t`, `300-multidimensional-array-const-reference-binding.t`, `300-reference-binding-pointee-const-pointer.t`, `300-scoped-enum-functional-cast-integral.t`, `300-zero-arg-functional-cast-alias.t`.
- Lookup, namespace, and overload-resolution families (8): `300-namespace-function-body-later-anonymous-overload.t`, `300-qualified-direct-function-hides-using-directive.t`, `300-reopened-unnamed-namespace-call.t`, `300-static-cast-member-overload-prefers-nontemplate.t`, `300-static-cast-overloaded-function-template-argument.t`, `300-unnamed-namespace-definition.t`, `300-unnamed-namespace-qualified-call.t`, `300-unnamed-namespace-unqualified-call.t`.
- Scoped-enum switch/control preparation (1): `300-switch-scoped-enum-condition.t`.

The grouped inventory is `20 + 3 + 6 + 8 + 8 + 1 = 46`; no residual was silently omitted.

The supplied current-state log `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` reports `142/166` passing, `24` failing, and `166/166` covered after `47a64a66`; normalized against the supplied turn-start set, it reports `46` baseline failures, `24` current failures, `0` current-only failures, and `22` baseline-only failures. This is supplied evidence, not fresh final evidence.

The exact current residual inventory is:

- `tests/general/200-builtin-constant-p-propagated-expression.t`
- `tests/general/200-constexpr-complete-object-cv.t`
- `tests/general/200-local-anonymous-union-variable.t`
- `tests/general/300-block-anonymous-union-injected-members.t`
- `tests/general/300-builtin-abort-semantics.t`
- `tests/general/300-builtin-constant-p-call.t`
- `tests/general/300-decltype-functional-cast.t`
- `tests/general/300-elaborated-local-struct-copy-init.t`
- `tests/general/300-local-extern-function-declaration.t`
- `tests/general/300-member-function-pointer-return-pointer-const.t`
- `tests/general/300-member-function-pointer-type-alias-and-function.t`
- `tests/general/300-member-pointer-type-alias-and-function.t`
- `tests/general/300-multidimensional-array-const-reference-binding.t`
- `tests/general/300-namespace-function-body-later-anonymous-overload.t`
- `tests/general/300-qualified-direct-function-hides-using-directive.t`
- `tests/general/300-reference-binding-pointee-const-pointer.t`
- `tests/general/300-reopened-unnamed-namespace-call.t`
- `tests/general/300-scoped-enum-functional-cast-integral.t`
- `tests/general/300-static-cast-member-overload-prefers-nontemplate.t`
- `tests/general/300-static-cast-overloaded-function-template-argument.t`
- `tests/general/300-unnamed-namespace-definition.t`
- `tests/general/300-unnamed-namespace-qualified-call.t`
- `tests/general/300-unnamed-namespace-unqualified-call.t`
- `tests/general/300-zero-arg-functional-cast-alias.t`

The prior checkpoint evidence is retained rather than replaced: the earlier
ledger records PA12 progress at `90/166` (`eee242c6`), `103/166`
(`47ca58be`), `113/166` (`5fa28b37`), and `120/166` (`61b60cb1`), with
166/166 coverage at each recorded broad checkpoint.  Its supplied/fresh
through-PA11 `685/685` result, file-audit pass with the two known header
warnings, focused counts, normalized failure sets, and performance evidence
remain in `pa12/audit.md` and the prior ledger rows.  The immediately prior
`47a64a66` plan evidence is also preserved below in the performance and
checkpoint records.

Fresh final normalization after the bounded repairs and source relocation was
performed from `/tmp/pa12-final-test.log` against the supplied primary log:
both sets contain exactly the 24 paths listed above, with `24` supplied,
`24` fresh, `0` supplied-only, and `0` fresh-only paths.  Relative to the
turn-start 46-path inventory, this is `46` baseline, `24` current,
`0` current-only, and `22` baseline-only paths.  The fresh stage summary is
`142/166` passing; all 166 paths were executed and covered.

## Active Checkpoint

Scope is the shared PA12 built-in expression/conversion boundary: top-level cv and reference-object normalization; modifiable-lvalue decisions; unscoped-enum promotion and integral/floating common types; unary/binary/assignment/conditional/inc-dec semantics; pointer/nullptr equality, object-only pointer-to-cv-void conversion, complete same-object-type pointer subtraction, pointer arithmetic, and pointer-to-bool; array decay/subscript, string-literal dereference, xvalue array input/call result handling, function-pointer array-bound deduction, target-directed braced initialization, the declaration-shaped assignment ambiguity owner, single-argument call selection, anonymous-enum generated identity, and local block-enum publication.

The bounded repair spans `dev/src/pa12_semantic.cpp`,
`dev/src/pa11_semantic_core.cpp`, and `dev/src/pa11_semantic_model.h`:
conditional array decay now forms canonical pointer operands; object-to-cv-void
conversion rejects function pointers; subtraction uses a typed common pointer
only for cv-qualified versions of the same complete object type; and the
limited `const T*`/`volatile T*` conditional case forms canonical
`const volatile T*`.  Conversion-range attachment now guards the contiguous
`(conversion_begin, conversion_count)` invariant.  No test, reference,
fixture, grammar, harness, script, generated artifact, or new `.cpp` was
changed.

The declaration-shaped assignment remains classified once by PA11 core and executed by the PA12 ambiguous-call owner only when the callee has a function binding. Child order is semantic source order except where the language operation intentionally canonicalizes commuted subscripting to sequence then index; all children remain typed fact IDs.

Focused validation used these exact commands:

```sh
make -C pa12 -j2
make -C pa12 check TEST='tests/general/200-function-pointer-array-deduced-bound.t tests/general/300-array-xvalue-subscript.t tests/general/300-deref-function-returning-pointer-reference.t tests/general/300-deref-string-literal.t tests/general/300-enum-comparisons.t tests/general/300-floating-arithmetic-comparisons.t tests/general/300-floating-conditional-common-type.t tests/general/300-floating-inc-dec.t tests/general/300-integral-compound-assign-unscoped-enum.t tests/general/300-nullptr-equality.t tests/general/300-pointer-bool-conversion.t tests/general/300-pointer-nullptr-conditional.t tests/general/300-pointer-plus-anonymous-enum.t tests/general/300-pointer-subtraction-cv-compatible.t tests/general/300-postfix-reference-return-call-inc.t tests/general/300-prefix-reference-return-call-inc.t tests/general/300-simple-assignment-reference-lhs.t tests/general/300-subscript-commuted-expression.t tests/general/300-subscript-unscoped-enum-index.t tests/general/300-unscoped-enum-integral-operators.t'
make -C pa12 check TEST='tests/spec/100-local-arith.t tests/spec/200-subscript-expression.t tests/general/100-pointer-equality.t tests/general/100-pointer-plus-assign.t tests/general/100-pointer-postincrement.t tests/general/200-bool-conditional-mixed-value-category.t tests/general/300-bad-pointer-integer-equality.t tests/general/300-bad-pointer-multiply-assign.t tests/general/300-bad-scoped-enum-if-condition.t'
make -C pa12 check TEST='tests/general/300-pointer-null-comparisons.t tests/general/100-pointer-relational-compare.t tests/general/300-pointer-cv-conditional.t tests/general/200-comma-bitwise-shift.t tests/general/300-compound-assignment-rhs-conversion-bad.t'
```

Build passed. The exact active set passed `20/20`, the exact controls passed
`9/9`, and the five additional neighboring controls passed `5/5`. Fresh
supervisor probes passed for object-pointer-to-cv-void, cv-compatible object
subtraction, and `const T*`/`volatile T*` conditional composition; function
pointer-to-void, object/void subtraction, and same-function-pointer
subtraction each exited `1` as required. Existing pointer-to-void call,
pointer-ranking, pointer-null comparison, and pointer-cv conditional controls
also exited `0`. The 20 active paths and nine controls executed `29/29`
inputs, all still covered by their harnesses. The conversion-range guard was
exercised by these expression paths without a non-contiguous append.

## Performance Evidence

Structural-only evidence was collected with immutable executable `/tmp/pa12-audit-47a64-array-immutable`, mode `555`, SHA-256 `29de56a2b47dac7977e1ac4a756aa78f11e57effbb4d95487a3d170f0f0a6ae5`. Equivalent out-of-tree inputs were `/tmp/pa12-audit-47a64-array-small.t` (SHA-256 `53e6e0d7c81bc96d925c421bbc4f9736691084a06d7ac1e909e364b545b95821`) and `/tmp/pa12-audit-47a64-array-large.t` (SHA-256 `b108e02a9bb7ccf7e6aeb8f1ccabed460f99c009a9e2fef88dda913eb2e515ec`).

| probe | initializer elements | subscript facts | binary facts | run exits | repeated output SHA-256 |
|---|---:|---:|---:|---|---|
| small | 8 | 8 | 7 | 0/0 | `4126eac24909af726b66bb1cef7f34f3c061d0233b46422e2f1b201df4ce603b` |
| large | 64 | 64 | 63 | 0/0 | `2ac88440c04349558dfcd02b7f455a2dbbaeb33853e135558eff726ed7e4031c` |

Each pair of outputs was byte-identical. This supports bounded local initializer/AST-arity work and deterministic rendering; no timing, RSS, scaling coefficient, or full-suite performance claim is made.

The earlier checkpoint's performance evidence is preserved: immutable
executable `/tmp/pa12-owner-cppgm-immutable`, mode `555`, SHA-256
`0949639ce92dd74f920d2659d1ac619477d854c75d04495d45b5bbb2b40eca33`, with
the recorded 8/64-element structural probes and output hashes in the prior
checkpoint record.  The earlier immutable direct-using audit also recorded
50/50 successful interleaved invocations per case and median batch
wall/user/system/RSS of `1.06/0.03/0.06/7206 KiB` and
`1.06/0.03/0.06/7194 KiB`; those measurements remain historical evidence,
not a claim about this scalar repair.  No timing or scaling claim is made for
the current audit.

## Next Checkpoint

The bounded checkpoint is complete. The next checkpoint is a separately
authorized residual-family audit; the exact 24 residual paths and excluded
builtin, declaration/anonymous-union, member-pointer/cast/reference-binding,
lookup/namespace, and scoped-enum-control families remain outside this audit.
The final changed paths are exactly:
`dev/src/pa11_semantic_core.cpp`, `dev/src/pa11_semantic_model.h`,
`dev/src/pa12_semantic.cpp`, `pa12/plan.md`, and `pa12/audit.md`.

Final fresh gates were run sequentially: `make test-pa12` exited `2` with
`142/166` and the exact 24 residual paths above; then
`n=12; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
passed `685/685`; then
`perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` passed with
exactly two known warnings at `dev/src/cpp_semantic_core.h:1` and
`dev/src/pa11_semantic_model.h:1` (`bad-division`).

## Checkpoint Ledger

| row | evidence / ownership | status |
|---|---|---|
| turn-start | Clean `d7bd97b7`; PA12 `120/166`, `46` failures, `166/166` covered; through-PA11 supplied passing. | recorded |
| landed increment | `47a64a66` changed the four approved source files and `pa12/plan.md`; the scalar checkpoint supplied current state is `142/166`, `24` failures, `166/166` covered. | audited |
| audit repair | Conditional array decay, object-only cv-void conversion, typed complete-object subtraction, composite conditional cv, conversion-range guarding, and source-attached conversion facts were repaired within the three changed source owners; no tests/refs/fixtures or new `.cpp`. | focused-passed |
| focused | Exact active family `20/20`; exact nine controls `9/9`; five neighboring controls `5/5`; supervisor valid/invalid pointer probes matched expected outcomes. | passed |
| broad/gates | Fresh `make test-pa12` `142/166` with exactly the supplied 24 paths and all 166 covered; exact through-PA11 command `685/685`; exact file audit passed with the two known warnings. | passed |
| performance | Retained pre-supervisor immutable structural evidence and prior performance evidence are recorded above; they are limited to deterministic local-work observations, with no current timing/scaling claim. | recorded |
| handoff | `git diff --check` passed; the final changed-path audit found exactly the five approved paths; only those paths were staged and committed together as this checkpoint audit. | complete |
