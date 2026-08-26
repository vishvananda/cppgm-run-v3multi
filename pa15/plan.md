# PA15 typed floating-scalar checkpoint

## Spec alignment

The production path is source -> PA10/PA11 typed facts -> PA12
`ConversionFact`/call facts -> PA15 typed LowIR -> PA13 serializer and
validator. This checkpoint is bounded to the PA15 floating scalar boundary:
signed/unsigned integer directions, f32/f64/f80 conversions, floating truth,
reference materialization, literal array/function decay, variadic defaults,
and generated helper identity. No textual semantic reconstruction, host or
reference compiler, whole-program retry, or duplicate semantic model is used.

PA12 remains the earliest owner of conversion selection, rank, category, and
constant-address publication. Its selected-call helper now also publishes
same-type lvalue-to-rvalue facts for supported variadic integral, floating,
and pointer scalars; fixed arguments are not revisited. PA15 lowers every call
argument through the recorded chain. `ReferenceBinding` owns typed pointer
prvalue temporaries and direct function-reference addresses, with lifetime and
physical type preserved into PA13 LowIR. `lower_logical` compares every
noncanonical RHS in its physical scalar type, without a floating-source gate;
canonical truth remains i64 in storage and receives explicit u8 materialization
when a value context requires bool. If a recorded conversion consumes semantic
bool after canonical truth, PA15 emits `trunc u8 i64` first and then preserves
the recorded conversion such as `zext i32 u8`, `sitofp f64 u8`, or
`ReferenceBinding`; the bridge runs before structural dispatch and never
retags the i64 operand. Only branch/condition consumers use
`lower_condition_expression`; initializer, assignment, return, unary `!`, call,
and other value paths use `lower_expression`, with integer-to-bool comparisons
performed in their physical integer type.

## Failure map

At turn start, the landed `d8d925563ea16945fa92a566f86fb743590e81c5`
checkpoint was `98/109` passing with `109/109` covered. The exact unchanged
residual set is:

    100-const-integral-lvalue-overload-category
    100-string-hex-escape-code-unit
    100-unnamed-parameter-storage
    200-comma-expression-xvalue-reference-return
    200-for-iteration-discards-void-comma-rhs
    200-goto-case-block-entry-label
    200-goto-case-block-label-after-statement
    200-literal-logical-short-circuit-omits-unreachable-call
    200-nested-conditional-array-decay
    200-return-void-call-expression
    300-return-empty-braces-scalar

The audit mechanically compared the final names with that authoritative list:
failure count `11`, missing `0`, and new/replacement `0`. The earlier
`ea846ea4` state was `90/109` with 19 failures; the landed increment removed
eight names before this bounded audit.

## Focused and owner evidence

The selected six-test matrix passed `6/6`:

    make -C pa15 check TEST='tests/general/200-const-ref-converted-float-argument.t tests/general/200-floating-compound-assign-integral-rhs.t tests/general/200-floating-condition-declaration-negative-zero.t tests/general/200-floating-logical-branch.t tests/general/200-floating-return-integral-conversion.t tests/general/200-variadic-float-argument-promotes-to-double.t'

The final log is `/tmp/pa15-audit-focused-reference-order.log`. Owner probes
400, 401, 402, 403, and 404 all exit zero in
`/tmp/pa15-audit-owner-probes-reference-order.log`. 404 checks all six floating
conversion opcode families, signedness and f80 truth, the pure-integral
logical RHS, direct bool return, bool local initialization, bool assignment,
scoped f80 condition branching, narrow integral/enum and float variadic
promotions, same-type int/double/pointer variadic loads, pointer/function
references, canonical bool-to-const/rvalue-reference prvalues, and generated-
slot collisions before `lowir2cy86` validation.

The final canonical-bool-to-nonbool correction changes exactly six existing
refs: `100-enum-default-argument-constant-fold.ref`,
`200-extern-c-internal-functions-stay-distinct.ref`,
`200-extern-c-internal-header-const.ref`,
`200-pointer-operator-array-decay.ref`,
`200-postfix-incdec-evaluates-lhs-once.ref`, and
`200-prefix-incdec-lvalue-address.ref`. Each changed line now materializes
canonical physical i64 truth to u8 before its recorded integer conversion.
Current enum/extern/header/prefix refs pass `lowir2cy86`; pointer/postfix full
files are blocked by unrelated pre-existing validator diagnostics, while
reduced current sequences pass and reduced old sequences fail exactly with
`conversion operand type mismatch`. The complete transcript and reduced
probes are `/tmp/tmp.YWSo4HvvH9/summary.tsv`. Across the amended checkpoint,
the complete existing-ref list is these six plus the prior four refs:
`100-unary-logical-conditional.ref`,
`200-reference-parameter-temp-name-collision.ref`,
`200-function-reference-static-cast-call.ref`,
and `200-floating-logical-branch.ref`. No reference tool or unrelated
fixture change was used.
The subsequent ordering repair changed no existing `.ref`; this ten-ref list
is the complete amended-checkpoint fixture list. Reduced ordering proof:
`/tmp/pa15-audit-reference-order-proof.0NdxJb` validates the corrected
reference store and rejects the pre-bridge `store u8` of physical i64.

## Broad gates

`make test-pa15` exits 2 only for the unchanged residual set and reports
`98/109` with `109/109` covered in
`/tmp/pa15-audit-test-pa15-reference-order.log`; the total `109` equals
`find pa15/tests -name '*.t' | wc -l`. The mechanical comparison has failure
count `11`, missing `0`, and new/replacement `0` against the exact turn-start
set.
The exact prior gate was run as:

    n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi

It reports `1058/1058` in
`/tmp/pa15-audit-through-pa14-reference-order.log`. The final file audit
passes with the five known header-division warnings in
`/tmp/pa15-audit-file-audit-reference-order.log`; `git diff --check` passes in
`/tmp/pa15-audit-diff-check-reference-order.log`. The exact failure-set proof
is `/tmp/pa15-audit-failure-set-reference-order.log`.

## Performance evidence

Retained artifact: `/tmp/pa15-checkpoint-audit-perf-reference-order.cgmxrA`.
The immutable candidate is mode `555`, SHA-256
`2d2310eaecaa41fd623c317788a5de85615b4d6b13a25dcf1fbda4ff6924347d`.
Seven bounded inputs, including the current 404 owner source, are
hash-recorded in `inputs.sha256`; each sample uses `--emit-lowir -O0`. Five
rounds interleave forward and reverse input order, with 20 compilations per
sample. `timings.tsv`, `medians.tsv`, structural counters, retained LowIR,
and validator results are present.

| input | LowIR lines | converts | calls | cmps | slots | median wall/call | median RSS KiB |
|---|---:|---:|---:|---:|---:|---:|---:|
| const_ref | 89 | 5 | 1 | 2 | 11 | 0.0030 s | 5360 |
| compound_assign | 36 | 1 | 1 | 1 | 3 | 0.0030 s | 5180 |
| condition_decl | 19 | 0 | 0 | 1 | 1 | 0.0030 s | 5184 |
| logical_branch | 77 | 1 | 1 | 5 | 5 | 0.0030 s | 5360 |
| return_integral | 31 | 2 | 1 | 1 | 2 | 0.0030 s | 5344 |
| variadic_float | 16 | 1 | 1 | 0 | 1 | 0.0030 s | 5132 |
| owner_404 | 569 | 32 | 6 | 13 | 70 | 0.0060 s | 5956 |

This is candidate-only evidence for the bounded affected path, not a
comparative or universal performance claim.

## Next checkpoint

The next checkpoint may address only the residual surfaces listed above:
overload/category, string decoding, unnamed-parameter storage, comma/xvalue,
control-flow labels/iteration, literal short-circuit, nested array decay,
void-call return, and empty-brace scalar return. No residual surface was
re-audited or repaired here.

## Completed checkpoint row

| checkpoint | result | movement |
|---|---:|---|
| `ea846ea4` incoming | `90/109`, `109/109` covered, 19 failures | typed floating/call boundary selected and landed in `d8d92556` |
| `d8d925563ea16945fa92a566f86fb743590e81c5` + checkpoint audit repair | `98/109`, `109/109` covered, 11 residual, 0 new/replacement | PA12 variadic publication, PA15 conversion-chain/reference ownership, physical-type logical RHS lowering, canonical i64-to-u8 bool materialization before every recorded non-Identity conversion including ReferenceBinding, 404 value/condition/integer/floating/variadic/const-and-rvalue-reference coverage, no new fixture changes, all broad gates complete |

## Audit delta files

    dev/src/pa12_semantic_calls.cpp
    dev/src/pa15_lowering.cpp
    dev/src/pa15_lowering.h
    dev/src/pa15_lowering_flow.cpp
    cppgm.tests/course/pa15/404-typed-floating-conversion-boundary-regression.sh
    cppgm.tests/course/pa15/404-typed-floating-conversion-boundary-regression.source
    pa15/tests/general/100-enum-default-argument-constant-fold.ref
    pa15/tests/general/100-unary-logical-conditional.ref
    pa15/tests/general/200-extern-c-internal-functions-stay-distinct.ref
    pa15/tests/general/200-extern-c-internal-header-const.ref
    pa15/tests/general/200-pointer-operator-array-decay.ref
    pa15/tests/general/200-postfix-incdec-evaluates-lhs-once.ref
    pa15/tests/general/200-prefix-incdec-lvalue-address.ref
    pa15/tests/general/200-floating-logical-branch.ref
    pa15/tests/general/200-reference-parameter-temp-name-collision.ref
    pa15/tests/general/200-function-reference-static-cast-call.ref
    pa15/audit.md
    pa15/plan.md
