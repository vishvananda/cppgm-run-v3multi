# PA15 Typed Callable/Reference Conversion Checkpoint

## Stage Design

PA10 owns the syntax/declarator AST and the parser's declaration-name shape
facts. PA11 owns canonical `TypeId` structure and equality for functions,
pointers, arrays, and lvalue/rvalue references. PA12 owns the typed semantic
boundary: explicit-cast validity/kind, reference compatibility and value
category, function-to-pointer conversion, the selected indirect callable
signature, and call argument/result facts.

An indirect-call fact publishes one selected function `TypeId` in
`SemanticFact::callable_type`; PA15 consumes that field directly. Function and
function-reference values cross into LowIR as pointers, with the typed
`FunctionToPointer` fact handling decay. Reference casts preserve address flow
and scalar/pointer casts use the PA12 conversion kind. No text/name/signature
rediscovery, second type model, whole-program retry, host/reference compiler,
or test-specific branch is used.

Floating literal payload is a sparse typed sidecar owned by the semantic
model: each fact stores only an index, while PA12 records the exact f32/f64/f80
bytes and PA15 decodes that payload once at lowering. `source_type` and
`cast_kind` are not stored in every hot `SemanticFact`; the latter is local to
PA12 cast validation and the former is already available from the typed fact
and conversion boundary.

## Exact Failure Map

Turn-start evidence is the authoritative baseline: 79/109 passing, all 109
covered, exactly these 30 failures; through PA14 passed and the file audit
passed:

    100-const-integral-lvalue-overload-category
    100-function-pointer-ref-call
    100-string-hex-escape-code-unit
    100-unnamed-parameter-storage
    200-comma-expression-xvalue-reference-return
    200-const-cast-pointer-const-drop
    200-const-cast-reference-array-subscript
    200-const-ref-converted-float-argument
    200-extern-function-pointer-indirect-call
    200-floating-compound-assign-integral-rhs
    200-floating-condition-declaration-negative-zero
    200-floating-logical-branch
    200-floating-return-integral-conversion
    200-for-iteration-discards-void-comma-rhs
    200-function-reference-static-cast-call
    200-functional-reference-typedef-cast
    200-global-address-reinterpret-cast-initializer
    200-goto-case-block-entry-label
    200-goto-case-block-label-after-statement
    200-included-namespace-global-definition
    200-literal-logical-short-circuit-omits-unreachable-call
    200-local-function-type-typedef-reference
    200-nested-conditional-array-decay
    200-qualified-namespace-overload-definition-symbol
    200-reinterpret-enum-to-pointer
    200-reinterpret-reference-conditional-materialization
    200-return-void-call-expression
    200-scalar-reference-static-cast-return
    200-variadic-float-argument-promotes-to-double
    300-return-empty-braces-scalar

The targeted related cluster is:

    100-function-pointer-ref-call
    200-extern-function-pointer-indirect-call
    200-local-function-type-typedef-reference
    200-function-reference-static-cast-call
    200-functional-reference-typedef-cast
    200-const-cast-reference-array-subscript
    200-const-cast-pointer-const-drop
    200-scalar-reference-static-cast-return
    200-reinterpret-reference-conditional-materialization
    200-reinterpret-enum-to-pointer

The focused checkpoint matrix is 15/15: all ten targeted cases plus ordinary
function-pointer calls, reference qualification/returns, array decay, and
enum lowering guards.

Final candidate evidence is 90/109 passed, 109/109 covered, and 19 failures.
Mechanical comparison of the incoming and final sorted name sets reports
`final - incoming = empty` and these 11 removals:

    100-function-pointer-ref-call
    200-const-cast-pointer-const-drop
    200-const-cast-reference-array-subscript
    200-extern-function-pointer-indirect-call
    200-function-reference-static-cast-call
    200-functional-reference-typedef-cast
    200-global-address-reinterpret-cast-initializer
    200-local-function-type-typedef-reference
    200-reinterpret-enum-to-pointer
    200-reinterpret-reference-conditional-materialization
    200-scalar-reference-static-cast-return

The exact 19 residual failures are:

    100-const-integral-lvalue-overload-category
    100-string-hex-escape-code-unit
    100-unnamed-parameter-storage
    200-comma-expression-xvalue-reference-return
    200-const-ref-converted-float-argument
    200-floating-compound-assign-integral-rhs
    200-floating-condition-declaration-negative-zero
    200-floating-logical-branch
    200-floating-return-integral-conversion
    200-for-iteration-discards-void-comma-rhs
    200-goto-case-block-entry-label
    200-goto-case-block-label-after-statement
    200-included-namespace-global-definition
    200-literal-logical-short-circuit-omits-unreachable-call
    200-nested-conditional-array-decay
    200-qualified-namespace-overload-definition-symbol
    200-return-void-call-expression
    200-variadic-float-argument-promotes-to-double
    300-return-empty-braces-scalar

## Active Checkpoint

This coherent implementation and plan change the following files:

    dev/src/lowir_model.cpp
    dev/src/pa10_ast.cpp
    dev/src/pa10_parser_support.cpp
    dev/src/pa10_parser_support.h
    dev/src/pa11_semantic_core.cpp
    dev/src/pa11_semantic_model.h
    dev/src/pa12_semantic.cpp
    dev/src/pa12_semantic_facts.cpp
    dev/src/pa12_semantic_resolution.cpp
    dev/src/pa15_lowering.h
    dev/src/pa15_lowering.cpp
    dev/src/pa15_lowering_flow.cpp
    pa15/plan.md

PA10 accepts nested reference/function declarators, known typedef names, and
postfix calls on keyword casts. PA12 validates the supported
const/functional/static/reinterpret reference subset, publishes the selected
indirect signature and callee conversion, and preserves argument/result
categories. PA15 lowers those typed facts without inferring a signature from
the callee expression; it also carries reference addresses as LowIR pointers
and preserves typed pointer temporaries for scalar-to-pointer casts. Only
entry `main` receives the standard zero result when it falls through; non-main
fallthrough remains rejected. The LowIR call-parameter serializer remains
factored to expose already-typed metadata.

The exact focused command was:

    make -C dev cppgm++
    make -C pa15 check TEST='tests/general/100-function-pointer-ref-call.t tests/general/200-extern-function-pointer-indirect-call.t tests/general/200-local-function-type-typedef-reference.t tests/general/200-function-reference-static-cast-call.t tests/general/200-functional-reference-typedef-cast.t tests/general/200-const-cast-reference-array-subscript.t tests/general/200-const-cast-pointer-const-drop.t tests/general/200-scalar-reference-static-cast-return.t tests/general/200-reinterpret-reference-conditional-materialization.t tests/general/200-reinterpret-enum-to-pointer.t tests/general/100-global-function-pointer-argument-call.t tests/general/200-const-cast-reference-similar-pointer.t tests/general/200-lvalue-conditional-reference-return.t tests/general/200-pointer-operator-array-decay.t tests/general/200-enum-class-scalar-lowering.t'

Both commands exit 0; the focused log `/tmp/pa15-callable-focused-amend.log`
reports `PASS (15/15)`. Its five guards are exactly
`100-global-function-pointer-argument-call`,
`200-const-cast-reference-similar-pointer`,
`200-lvalue-conditional-reference-return`,
`200-pointer-operator-array-decay`, and
`200-enum-class-scalar-lowering`.

The three owner probes for scoped parser classification also exit 0:
`/tmp/pa15-outer-typedef-cast.cpp` verifies a parenthesized C-style cast
through an outer typedef, and `/tmp/pa15-shadowed-function-pointer-call.cpp`
verifies that a nearer same-spelling function-pointer variable parses and
lowers `(Callback)(4)` as a load followed by an indirect call.
`/tmp/pa15-parameter-shadowed-function-pointer-call.cpp` verifies the same
nearest-binding behavior for a function-pointer parameter in the function-body
scope. The parser now uses a fixed `PA10NameKind` enum and bounded
nearest-binding type/value scopes rather than a global type-name exception.
The exact probe log is `/tmp/pa15-parser-scope-probes-amend.log`.

The full command `make test-pa15` covered 109/109 and reported 90/109 with
the 19 residuals above (exit 2 because residual failures remain). The full
log is `/tmp/pa15-callable-amend-test.log`. The mechanical
comparison used that log and the incoming
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`; it
reported incoming 30, final 19, removed 11, and an empty `final - incoming`
set.

The required prior-through command was:

    n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi

It passed with `===== ALL TESTS PASSED SUCCESSFULLY! (1058 / 1058) =====`;
the log is `/tmp/pa15-callable-through-pa14-amend.log`.

## Performance Evidence

Canonical `TypeId` equality remains O(1). `sizeof(SemanticFact)` was measured
as 192 bytes at the turn-start parent/baseline commit `ca3c38ca` and 208 bytes
in the final candidate; an intermediate
inline-`long double` candidate measured 240 bytes. The final candidate keeps
one compact callable `TypeId` and a sparse floating-literal index in each fact;
the typed sidecar stores the literal's type and 4/8/16 raw bytes in append-only
storage. No dead source/cast payload or inline 16-byte floating value remains
in every fact. The direct size evidence is in
`/tmp/pa15-semantic-fact-size-final.log`.

`cv_cast_compatible_impl` strips only cv wrappers and recursively preserves
lvalue/rvalue reference, pointer, array, member-pointer, and function
constructors, including function parameters/results; recursive calls do not
use `expression_object_type`. Reinterpret-reference validity uses the typed
supported-scalar rule (integral, floating, enumeration, or pointer), not a
same-size test. Positive, signature-negative, and class-negative probes exited
0, 1, and 1 respectively: `/tmp/pa15-cv-positive.cpp`,
`/tmp/pa15-cv-negative-signature.cpp`, and
`/tmp/pa15-cv-negative-class.cpp`.

The bounded stress script `/tmp/pa15-callable-stress.sh` used an immutable
mode-555 compiler copy and three interleaved repetitions. Its final artifact
directory is `/tmp/pa15-callable-stress.hH2cUV` and log is
`/tmp/pa15-callable-stress-final-parser.log`. Reference calls with 8/32/128
repeated calls produced 9/33/129 LowIR calls, 56/152/536 lines, and median RSS
4880/5392/7064 KiB, with median times 0.00/0.00/0.01s. Pointer calls produced
9/33/129 calls, 49/121/409 lines, and 4860/5120/6400 KiB, with median times
0.00/0.00/0.01s. Signatures with 1/4/8/16 parameters produced 5 calls,
38/44/52/68 lines, and 4868/4832/5104/5132 KiB; median times were 0.00s.
These are bounded observations, not universal claims;
output call/line counts track input growth, while the measured RSS stays in
the reported range. Typed walks, call-parameter serialization, and fact-edge
publication remain linear in their declaration/signature/fact sizes, with no
repeated whole-program traversal. The parameter collector adds one bounded walk
over each function's immediate parameter declarators and their nested declarator
nodes. The through-PA14 result corroborates the existing PA13/PA14 boundary.
Parser enum-based type/value classification performs one nearest-binding lookup
per ambiguous parenthesized expression; its scope walk is bounded by the PA10
nesting limit. Stress artifacts are retained at the
paths above.

The source audit command passed with five known header-division warnings; its
log is `/tmp/pa15-callable-file-audit-amend.log`. `git diff --check` passed
after the final plan update. Landed as the PA15-focused checkpoint commit
`PA15: preserve typed callable and reference conversions`; see git history.

## Checkpoint Ledger

| status | checkpoint | evidence or next boundary |
|---|---|---|
| Historical | PA15 typed enum scalar boundary | Prior checkpoint raised the stage from 70/109 to the authoritative 79/109 baseline; retained only as history. |
| Current (Landed) | Typed callable/reference conversion boundary | 90/109, 109/109 covered, 11 incoming failures removed, no replacements; focused 15/15, through PA14 1058/1058, stress and probes passed, source audit passed with five warnings. Landed as the PA15-focused checkpoint commit; see git history. |
| Next | Remaining PA15 residual boundary | The exact 19 residuals listed above; preserve the typed ownership boundary and do not broaden this checkpoint. |
