# PA12 scalar-expression semantic-boundary checkpoint

## Stage Design

The owner remains `PA11SemanticModel`.  PA11 core owns block-enum publication,
array-bound formation, and the declaration-shaped assignment AST/lookup
classifier; `dev/src/pa12_semantic.cpp` owns expression, call, and braced-init
semantic facts plus conversion ranking/application and PA12 dumping.  The
typed flow is PA10 AST -> PA11 canonical `TypeId`/`BindingId`/`ScopeId` facts
-> PA12 `ExprInfo`/`SemanticFact` nodes -> recorded `ConversionFact` nodes ->
the existing deterministic cold renderer.  The increment keeps enum identity,
enumerator values, reference categories, and anonymous-enum identity typed;
it does not reconstruct types or names from rendered text.

The shared boundary now normalizes expression object/top-level cv types,
performs unscoped-enum integral promotion, computes integral/floating common
types, records built-in conversions, and preserves lvalue/xvalue result
categories for assignment, conditional, inc-dec, and dereference.  It handles
an xvalue array input/call result while keeping the subscript fact an lvalue,
as required by the checked-in fixture.  It also gives target-directed array
braced initializers a bounded `BracedInitList` fact and consumes the block-enum
bindings published by PA11.  The declaration-shaped reference assignment is
classified by the PA11 core classifier and executed through the PA12
ambiguous-call owner only when the callee has a function binding.

The new expression work follows AST arity plus the existing local
candidate/type decomposition: binary/assignment nodes inspect two operands,
conditionals three, braced initializers their local elements, and callable
selection its existing candidate vector.  No new whole-arena scan, retry
loop, or rendered-name lookup is introduced.  Anonymous-enum naming uses one
counter; no stronger timing or asymptotic claim is made about pre-existing
lookup helpers.
Single-argument selection scans that vector once, retaining the best rank and
resetting the ambiguous-best flag when a strictly better candidate appears.

## Failure Map

Turn-start baseline: `d7bd97b7`, PA12 `120/166` passing, `46` failing, all
`166/166` covered; through PA11 was passing.  The complete residual inventory
is classified below.  The active family is the first group and contains the
20 paths exercised by the focused checkpoint.

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

Builtin declaration/constant semantics, explicitly excluded (3):

- `tests/general/200-builtin-constant-p-propagated-expression.t`
- `tests/general/300-builtin-abort-semantics.t`
- `tests/general/300-builtin-constant-p-call.t`

Declaration, anonymous-type, and local declaration formation, excluded (6):

- `tests/general/200-constexpr-complete-object-cv.t`
- `tests/general/200-local-anonymous-union-variable.t`
- `tests/general/300-block-anonymous-union-injected-members.t`
- `tests/general/300-block-elaborated-enum-type-use.t`
- `tests/general/300-elaborated-local-struct-copy-init.t`
- `tests/general/300-local-extern-function-declaration.t`

Member-pointer, cast/type-formation, and reference-binding families, excluded
(8):

- `tests/general/300-decltype-functional-cast.t`
- `tests/general/300-member-function-pointer-return-pointer-const.t`
- `tests/general/300-member-function-pointer-type-alias-and-function.t`
- `tests/general/300-member-pointer-type-alias-and-function.t`
- `tests/general/300-multidimensional-array-const-reference-binding.t`
- `tests/general/300-reference-binding-pointee-const-pointer.t`
- `tests/general/300-scoped-enum-functional-cast-integral.t`
- `tests/general/300-zero-arg-functional-cast-alias.t`

Lookup, namespace, and overload-resolution families, excluded (8):

- `tests/general/300-namespace-function-body-later-anonymous-overload.t`
- `tests/general/300-qualified-direct-function-hides-using-directive.t`
- `tests/general/300-reopened-unnamed-namespace-call.t`
- `tests/general/300-static-cast-member-overload-prefers-nontemplate.t`
- `tests/general/300-static-cast-overloaded-function-template-argument.t`
- `tests/general/300-unnamed-namespace-definition.t`
- `tests/general/300-unnamed-namespace-qualified-call.t`
- `tests/general/300-unnamed-namespace-unqualified-call.t`

Scoped-enum switch/control preparation, excluded (1):

- `tests/general/300-switch-scoped-enum-condition.t`

The grouped inventory is `20 + 3 + 6 + 8 + 8 + 1 = 46`; no residual is
silently omitted.  The active family is deliberately narrower than the full
residual and does not claim fixes for lookup, anonymous-union formation,
member-pointer parsing, builtins, or unrelated statement preparation.

## Active Checkpoint

Scope is the shared PA12 built-in expression/conversion boundary: lvalue to
rvalue and top-level-cv normalization; unscoped-enum promotion in unary,
arithmetic, bitwise, shift, comparison, compound assignment, pointer offset,
and subscript contexts; arithmetic/equality/relational typing; floating types
needed by the checked-in residual contract; pointer/nullptr equality and
pointer-to-bool conversion; compatible pointer subtraction and conditional
typing; prefix/postfix inc-dec; reference-preserving assignment; commuted
subscript, array decay, string-literal dereference, xvalue array input/call
result handling with the fixture's lvalue subscript result, and target-directed
array initialization where they share this owner.  Assignment and inc-dec
also reject canonical top-level `const` objects and const pointer objects,
while mutable pointers-to-const remain writable as pointer objects.

The invariants are canonical typed identities, conversion facts attached to
the source expression, deterministic child order, no test-specific answers,
and no coverage-changing test/ref edits.  The PA12 README requires these
built-in scalar, pointer, subscript, assignment, conditional, cast, and
reference expression families.  The README describes floating support as
out of the required subset, but the three checked-in floating residuals have
existing semantic refs and are included here because this checkpoint's
contract explicitly permits supported floating operands.

Focused validation covers all 20 active turn-start failures and nine nearby
valid/invalid controls.  The exact 20-path command is:

`make -C pa12 check TEST='tests/general/200-function-pointer-array-deduced-bound.t tests/general/300-array-xvalue-subscript.t tests/general/300-deref-function-returning-pointer-reference.t tests/general/300-deref-string-literal.t tests/general/300-enum-comparisons.t tests/general/300-floating-arithmetic-comparisons.t tests/general/300-floating-conditional-common-type.t tests/general/300-floating-inc-dec.t tests/general/300-integral-compound-assign-unscoped-enum.t tests/general/300-nullptr-equality.t tests/general/300-pointer-bool-conversion.t tests/general/300-pointer-nullptr-conditional.t tests/general/300-pointer-plus-anonymous-enum.t tests/general/300-pointer-subtraction-cv-compatible.t tests/general/300-postfix-reference-return-call-inc.t tests/general/300-prefix-reference-return-call-inc.t tests/general/300-simple-assignment-reference-lhs.t tests/general/300-subscript-commuted-expression.t tests/general/300-subscript-unscoped-enum-index.t tests/general/300-unscoped-enum-integral-operators.t'`

The exact nine-control command is:

`make -C pa12 check TEST='tests/spec/100-local-arith.t tests/spec/200-subscript-expression.t tests/general/100-pointer-equality.t tests/general/100-pointer-plus-assign.t tests/general/100-pointer-postincrement.t tests/general/200-bool-conditional-mixed-value-category.t tests/general/300-bad-pointer-integer-equality.t tests/general/300-bad-pointer-multiply-assign.t tests/general/300-bad-scoped-enum-if-condition.t'`

The formerly failing set is `20/20` passed and the controls are `9/9` passed;
all 29 selected inputs were executed.  Temporary probes outside the
repository passed: unsigned-int plus long and enum plus long yielded `long
int`, fixed `unsigned short` enum complement yielded `int`, fixed
`unsigned int` enum shift yielded `unsigned int`, `int` plus `unsigned int`
yielded `unsigned int`, and `long` plus `unsigned long` yielded `unsigned long
int`; one mutable/pointer-to-const valid probe passed and const-object,
const-pointer, and const-pointee mutation probes were rejected.

Final broad result is `142/166` with `166/166` covered: `46` baseline failures,
`24` current failures, `0` current-only failures, and `22` baseline-only
failures.  The normalized current residual inventory is:

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

The current list is a subset of the turn-start list; no unrelated residual
family was changed deliberately.

## Performance Evidence

Structural evidence remains limited to what the code proves: expression
handlers use fixed AST operand arity, braced-init traversal visits only local
initializer elements, and function selection walks its existing candidate
vector.  No whole-arena scan or retry loop was added.

For deterministic bounded probes, the immutable executable was
`/tmp/pa12-owner-cppgm-immutable`, mode `555`, SHA-256
`0949639ce92dd74f920d2659d1ac619477d854c75d04495d45b5bbb2b40eca33`.  Both
runs for each input exited successfully and produced byte-identical output:

| probe | initializer elements | subscript nodes | binary nodes | output SHA-256 |
|---|---:|---:|---:|---|
| `/tmp/pa12-review-array-small.t` | 8 | 8 | 7 | `9e80a22b0829def6c5a8510e0350585273f2357503aea9e9579ba999f2802e11` |
| `/tmp/pa12-review-array-large.t` | 64 | 64 | 63 | `6e0882bda8f3044ecda7aa4a5738a3939a1537f3bd892bcde393ddd1881658f8` |

The output hashes matched across the two runs for each size.  No timing or
scaling claim is made; no interleaved median timing study was performed.

## checkpoint ledger

| checkpoint | evidence | status |
|---|---|---|
| turn-start | `d7bd97b7`; PA12 `120/166`, `46` failures, `166/166` covered; through PA11 passing | recorded |
| implementation | Changed `dev/src/pa11_semantic.cpp`, `dev/src/pa11_semantic_core.cpp`, `dev/src/pa11_semantic_model.h`, and `dev/src/pa12_semantic.cpp`; no new `.cpp`, so source-set list unchanged | complete |
| focused | `make -C pa12 -j2`; exact 20-path residual check `20/20`; exact nine-control check `9/9`; promotion/modifiability probes passed | passed |
| broad | Exact `make test-pa12`: `142/166`, all `166/166` covered, 24 residuals; `46 -> 24`, no current-only failures | passed |
| prior gate | Exact `n=12; ... make test-report-through-pa$((n - 1))`: `685/685` | passed |
| file audit | Exact `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src`: passed with 2 known header warnings | passed |
| performance | Immutable executable and 8/64-element deterministic probes recorded above; both repeated hashes matched | passed; no timing claim |
| diff/commit status | `git diff --check` passed; exactly the approved five paths are changed and no tests/refs/fixtures/generated tracked artifacts changed; implementation, plan, and evidence landed together as **this checkpoint commit**, with no follow-up audit commit | complete |
