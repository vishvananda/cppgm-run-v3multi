# PA15 Typed Callable/Reference Conversion Checkpoint

## Scope and spec alignment

The landed increment under audit is `fbc3cce76cfbe89872651f8c2d8e5ab410e3607c`
(`PA15: preserve typed callable and reference conversions`), parent
`ca3c38ca`.  The follow-up repair is the bounded PA15 guard in
`dev/src/pa15_lowering.cpp`; the durable owner regression is under
`cppgm.tests/course/pa15/403-*`.

The path remains source -> typed PA10/PA11/PA12 facts -> typed PA15 LowIR.
PA10 owns declarator/name-kind and ambiguity facts, PA11 owns canonical
`TypeId` identity, PA12 owns cast validity, value category, conversion facts,
and the selected callable `TypeId`, and PA15 consumes those facts without
text/name/signature rediscovery.  This satisfies `spec.md` sections 1-5 and 7:
one production pipeline and LowIR model, typed fact continuity, canonical
ownership, bounded work, typed lowering, and measured output evidence.  No
host/reference compiler, second semantic model, whole-program retry, or
test-specific branch was added.  PA12's sparse floating sidecar retains PA2's
f32/f64/f80 bytes; PA15 decodes them once at the typed literal edge.

## Prior checkpoint movement

The retained landed evidence records the authoritative movement from the
parent baseline to `fbc3cce7`:

| checkpoint | result | movement |
|---|---:|---|
| Parent `ca3c38ca` | `79/109`, all `109` covered, 30 failures | baseline before the callable/reference increment |
| Landed `fbc3cce7` | `90/109`, all `109` covered, 19 failures | 11 names removed; landed final-minus-parent failure set empty |

The 11 removed names were:

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

The landed focused matrix was `15/15`; the earlier-through gate was
`1058/1058`; and the prior source file audit passed with five known
header-division warnings.  Those results are retained here as checkpoint
history, without citing the transient old `/tmp` paths that are no longer
available.

## Current exact failure map

The authoritative incoming log remains
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`90/109`, all `109/109` covered, exactly these 19 failures:

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

Final validation after the guard is also `90/109`, all `109/109` covered, with
the same 19 names.  The mechanical proof is
`/tmp/pa15-checkpoint-failure-set.log`: incoming count `19`, final count `19`,
final-minus-incoming count `0`, and incoming-minus-final count `0`.

## Ownership audit and bounded repair

PA10 uses nearest-binding `PA10NameKind` scopes: temporary declarator scopes do
not leak, while namespace/class/compound scopes persist. Parameter names are
collected once from the immediate parameter clause and published when a
function-definition body scope opens. C-style and functional cast routing uses
nearest typed/value classification plus bounded indexed delimiter facts.

PA11 owns canonical `TypeId` identity. PA12 publishes `FunctionToPointer`,
reference/value categories, conversion facts, and `SemanticFact::callable_type`.
PA15 consumes the callable type directly for direct/indirect calls and emits
typed indirect signature/parameter metadata; function and function-reference
values cross into LowIR as pointers.

`cv_cast_compatible_impl` recursively preserves pointer, reference, array,
member-pointer, and function signature constructors while stripping only cv
wrappers. The corrected const-cast owner probe is
`const_nested_store`, frontend/backend `0/0`, in
`/tmp/pa15-checkpoint-cast-probes.log`; it isolates nested similar-pointer
qualification from the unrelated bool-result residual. Function-signature
mismatch is rejected. Reinterpret references remain limited to the supported
typed scalar domain.

PA12's scalar-to-pointer reinterpret boundary could otherwise reach PA15 as a
nonzero integer literal. PA13 has no pointer/integer conversion opcode and only
permits a typed zero pointer literal. `apply_reinterpret_conversion` now rejects
nonzero integer literals before emitting invalid `copy ptr N`. Typed zero
integer/enum/nullptr, pointer-to-pointer, and the typed global address
relocation path remain valid. Dynamic integer-to-pointer and pointer-to-integral
runtime values remain rejected; expanding that capability would require an
explicit PA13 contract change and is outside this bounded audit.

PA12's transactional floating sidecar retains sparse decoded bytes and PA15
checks its range/fundamental type before one-time decode. The fresh f32/f64/f80
probe and current `sizeof(SemanticFact) = 208` are in
`/tmp/pa15-checkpoint-float-sidecar.log`.

## Final evidence and performance

The durable owner regression
`cppgm.tests/course/pa15/403-typed-reinterpret-boundary-regression.sh` exits
`0` (`/tmp/pa15-checkpoint-403.log`). It validates LowIR for typed integer and
enum zero-to-pointer plus pointer-to-pointer, and rejects nonzero integer and
enum-to-pointer before invalid LowIR emission. No PA15 test or `.ref` fixture
was changed.

The focused matrix expanded to `17/17` and includes the global address and
literal guards (`/tmp/pa15-checkpoint-focused.log`). The callable, parser, and
cast owner probes are in `/tmp/pa15-checkpoint-callable-probe.log`,
`/tmp/pa15-checkpoint-parser-probes.log`, and
`/tmp/pa15-checkpoint-cast-probes.log`.

The earlier landed performance record measured `sizeof(SemanticFact)` as 192
bytes at the parent, 208 bytes in the landed candidate, and 240 bytes for an
intermediate inline-`long double` candidate. Its bounded callable observations
included 8/32/128 repeated calls producing 9/33/129 LowIR calls, and
1/4/8/16-parameter signatures producing 5 calls with 38/44/52/68 LowIR lines.
Those historical observations are retained as measured progress, not a
universal complexity claim.

The refreshed §7 measurement uses immutable compiler copy
`/tmp/pa15-checkpoint-perf.oyAXma/cppgm++-immutable`, mode `555`, SHA-256
`e969765789a92ec74832ee0a9da1bfe04038b172dc26fc2666d54f8988d66655`. Equivalent
generated call inputs were interleaved as 8/32/128 across three rounds. The
verified medians and structural counts are:

| size | calls | LowIR lines | LowIR bytes | median wall | median RSS KiB |
|---:|---:|---:|---:|---:|---:|
| 8 | 8 | 41 | 1204 | 0.00000s | 4896 |
| 32 | 32 | 113 | 3914 | 0.00000s | 5104 |
| 128 | 128 | 401 | 15362 | 0.00000s | 6128 |

Raw interleaved measurements, medians, compiler hash/mode, and generated
inputs are retained under `/tmp/pa15-checkpoint-perf.oyAXma/`; the compact log
is `/tmp/pa15-checkpoint-performance-immutable.log`. This is bounded evidence
of proportional call/line growth and observed resource behavior for equivalent
inputs, not a universal asymptotic claim.

## Gate results and ledger

| gate | result | log |
|---|---|---|
| `make test-pa15` | exit `2`, `90/109`, all `109/109`, exact 19 residuals | `/tmp/pa15-checkpoint-full-pa15.log` |
| through PA14 command | exit `0`, `1058/1058` | `/tmp/pa15-checkpoint-through-pa14.log` |
| PA15 file audit | exit `0`, five known warnings | `/tmp/pa15-checkpoint-file-audit.log` |
| `git diff --check` | exit `0` | `/tmp/pa15-checkpoint-diff-check.log` |
| failure-set comparison | final-minus-incoming `0`; incoming-minus-final `0` | `/tmp/pa15-checkpoint-failure-set.log` |

| status | checkpoint | evidence or next boundary |
|---|---|---|
| Historical | PA15 typed enum/global pointer boundaries | Preserved in git history and `pa15/audit.md`; not broadened into this callable/reference checkpoint. |
| Completed (landed) | `fbc3cce76cfbe89872651f8c2d8e5ab410e3607c` — typed callable/reference boundary | Parent `79/109` with 30 failures -> landed `90/109` with 19; all `109` covered, 11 names removed, final-minus-parent empty, focused `15/15`, through-PA14 `1058/1058`, and prior file audit pass with five warnings. |
| Completed (current audit) | PA15 LowIR-safe reinterpret boundary plus owner regression 403 | Final `90/109`, all `109/109`, no new/replacement failures, focused `17/17`, regression `403` exit `0`, refreshed immutable §7 evidence, through/file-audit/diff gates complete. |
| Next | Remaining PA15 residuals | Preserve the typed ownership boundary; no further work in this bounded checkpoint. |
