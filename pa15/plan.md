# PA15 Typed Address/Value Boundary Checkpoint

## Spec Alignment and Ownership

This checkpoint implements the PA11-to-PA12-to-PA15 typed boundary required by
`spec.md` §§2, 3, 4, 5, and 7:

- PA11 owns canonical `BindingId`, `TypeId`, `ScopeId`, declaration, linkage,
  storage, function, and scope facts.
- PA12 owns `SemanticFactId` expression identity, selected bindings, value
  categories, conversions, typed `sizeof`, one-time integral constant values,
  and one-time namespace-initializer relocation facts. Each
  `ConstantAddressFact` has explicit `evaluated`/`valid` state and canonical
  target/addend/projection/index fields. Its resolver carries typed `Value`,
  `ObjectAddress`, and `ArrayDecay` context: variable IdExpressions become
  relocations only for recorded array decay or explicit address-of operands;
  function identity remains a function relocation.
- PA15 builds deterministic binding/declaration/function-scope/global/local
  indexes once and consumes typed facts for global declarations/definitions,
  local/global addresses, references as referent addresses, arrays, decay,
  subscripts, projections, pointer scaling, address/value conversion,
  assignment/comma/conditional category preservation, and compound/prefix/
  postfix single LHS evaluation.
- LowIR remains one typed in-memory `Program`; `IK_INDEX` preserves typed
  element/projection information and `lowir_model.cpp` is only the serializer.
  `frontend_source_sets.mk` wires `pa12_semantic_facts.cpp` and the PA15
  lowering units. No semantic text, token lookup, fixture name, or host
  compiler is used in the source-to-LowIR path.

The repaired address path is ordinary linear work over its typed expression
facts, with the existing deterministic indexes bounded by `O(n log n)`. The
former PA15 recursive `constant_address` reconstruction and second
`find_address_subscript` traversal are gone, and PA15 cannot reinterpret a
bare scalar/pointer lvalue as its storage address.

## Exact Failure Map and Coverage

Historical evidence preserves the pre-increment `21/109` passing baseline and
the resulting `21` to `68` progress. The turn-start full-stage result was
`68/109` passing, exactly 41 failures, all `109` covered; its authoritative log
is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The fresh final log is
`/tmp/pa15-typed-relocation-correction.6EMMbb/full-pa15-context-final.log`.
Both inventories are byte-for-byte equal after sorting, with 41 names:
the fresh sorted extraction is retained at
`/tmp/pa15-typed-relocation-correction.6EMMbb/failures-context-sorted.txt` and
was compared with the historical
`/tmp/pa15-typed-relocation-correction.6EMMbb/failures-start-sorted.txt`.

```text
100-const-integral-lvalue-overload-category
100-enum-default-argument-constant-fold
100-function-pointer-ref-call
100-scoped-enum-braced-assignment
100-scoped-enum-previous-enumerator-bitwise-or
100-string-hex-escape-code-unit
100-unnamed-parameter-storage
200-comma-expression-xvalue-reference-return
200-const-cast-pointer-const-drop
200-const-cast-reference-array-subscript
200-const-ref-converted-float-argument
200-enum-class-scalar-lowering
200-extern-function-pointer-indirect-call
200-floating-compound-assign-integral-rhs
200-floating-condition-declaration-negative-zero
200-floating-logical-branch
200-floating-return-integral-conversion
200-for-iteration-discards-void-comma-rhs
200-function-reference-static-cast-call
200-functional-reference-typedef-cast
200-global-address-reinterpret-cast-initializer
200-global-pointer-array-null-fill
200-global-pointer-array-nullptr-init
200-goto-case-block-entry-label
200-goto-case-block-label-after-statement
200-included-namespace-global-definition
200-literal-logical-short-circuit-omits-unreachable-call
200-local-function-type-typedef-reference
200-namespace-default-argument-declaration-lookup
200-nested-conditional-array-decay
200-qualified-namespace-overload-definition-symbol
200-reinterpret-enum-to-pointer
200-reinterpret-reference-conditional-materialization
200-return-void-call-expression
200-scalar-reference-static-cast-return
200-scoped-enum-global-constant-init
200-scoped-enum-unsigned-high-bit
200-unscoped-enum-promotion-overload
200-variadic-float-argument-promotes-to-double
200-wide-unscoped-enum-promotion
300-return-empty-braces-scalar
```

No residual failure is counted as passing, and no new failure substitutes for
one of these names.

## Focused and Final Validation

Fresh focused validation passed `20/20` for the representative address/value,
array, reference, category, pointer-scaling, and single-evaluation cases.
The focused matrix output is retained at
`/tmp/pa15-typed-relocation-correction.6EMMbb/focused-matrix-context.log`.
The temporary probes under
`/tmp/pa15-typed-relocation-correction.6EMMbb/probes` show `&object` as
`addr @value`, array decay and one-past as `addr @data` and
`addr @data + 16`, `&array[1]` as the checked-in runtime
`index i32 [projection=array_element]` form, and both direct and explicit
function pointers as `addr @function`. The bare pointer probe exits with
`PA15 nonconstant global initializer` and emits no `addr @pointer`.

The exact final gate results are:

| command | result |
|---|---|
| `make -C dev cppgm++` | exit `0` |
| exact `n=15` through-PA14 command | exit `0`, `1058/1058`; log: `/tmp/pa15-typed-relocation-correction.6EMMbb/through-pa14-context-final.log` |
| `make test-pa15` | exit `2`, `68/109`, 41 unchanged failures, all `109` covered; log: `/tmp/pa15-typed-relocation-correction.6EMMbb/full-pa15-context-final.log` |
| `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` | exit `0`, five nonfatal header warnings; log: `/tmp/pa15-typed-relocation-correction.6EMMbb/file-audit-context-final.log` |
| `git diff --check` | pass |

The exact through-PA14 command was:

```sh
n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

The five existing file-audit warnings are the header-division findings for
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
and `pa15_lowering.h`.

## Representative Performance Evidence

The immutable corrected executable is
`/tmp/pa15-typed-relocation-correction.6EMMbb/context-perf/cppgm++-corrected`,
mode `0555`, size `1,924,688` bytes, SHA-256
`69b7221e16dffec0e266c91b651cfcab6851fcd8678906941d5845882f4cfe77`.
Inputs, LowIR, semantic dumps, raw timings, structural counts, and medians are
retained in `context-perf`. Its `timings.tsv` contains seven interleaved
samples per family and size, each with 20 repeated compilations. The exact
order in rounds 1, 3, 5, and 7 is
`long-32, many-32, long-64, many-64, long-128, many-128, long-256,
many-256`; rounds 2, 4, and 6 reverse the size order. The executable and
generated evidence files are immutable after generation.

Structural counts from `structure.tsv` are:

| family | n | input bytes/lines | LowIR lines | semantic lines | LowIR globals | binary facts |
|---|---:|---:|---:|---:|---:|---:|
| long-expression | 32 | 131/3 | 9 | 75 | 2 | 32 |
| long-expression | 64 | 195/3 | 9 | 139 | 2 | 64 |
| long-expression | 128 | 324/3 | 9 | 267 | 2 | 128 |
| long-expression | 256 | 580/3 | 9 | 523 | 2 | 256 |
| many-global | 32 | 1403/65 | 69 | 168 | 64 | 0 |
| many-global | 64 | 2811/129 | 133 | 328 | 128 | 0 |
| many-global | 128 | 5711/257 | 261 | 648 | 256 | 0 |
| many-global | 256 | 11727/513 | 517 | 1288 | 512 | 0 |

Per-compilation medians from `medians.tsv` are wall/user/system seconds and
peak RSS KiB:

| family | n | wall | user | system | RSS KiB |
|---|---:|---:|---:|---:|---:|
| long-expression | 32 | 0.003500 | 0.001500 | 0.002000 | 5152 |
| long-expression | 64 | 0.003500 | 0.001500 | 0.002000 | 5128 |
| long-expression | 128 | 0.004000 | 0.001500 | 0.002000 | 5388 |
| long-expression | 256 | 0.004500 | 0.002000 | 0.002500 | 5676 |
| many-global | 32 | 0.004500 | 0.002000 | 0.002500 | 5672 |
| many-global | 64 | 0.006000 | 0.003500 | 0.002500 | 6164 |
| many-global | 128 | 0.010000 | 0.005500 | 0.004000 | 6892 |
| many-global | 256 | 0.016500 | 0.010500 | 0.006000 | 8604 |

The long-expression family retains a constant-size serialized relocation while
its typed binary facts grow from 32 to 256. The many-global family grows from
64 to 512 LowIR globals and from 168 to 1288 semantic lines. These counts and
the timing medians provide representative bounded evidence, not a universal
performance proof.

## Explicit Next Checkpoint

The next bounded PA15 capability is typed global pointer null/zero initializer
lowering, selected from the residual `200-global-pointer-array-null-fill` and
`200-global-pointer-array-nullptr-init` failures. It will trace PA12
`nullptr`/null-integer conversion ownership through PA15 scalar and structured
pointer zero initialization while preserving the current 41-failure ceiling.

## Checkpoint Ledger

| status | checkpoint | completed evidence |
|---|---|---|
| Complete | PA15 full-stage / checkpointAudit — typed address/value ownership | Amended PA12 relocation ownership with explicit `Value`/`ObjectAddress`/`ArrayDecay` context, rejecting bare pointer/scalar lvalue relocations while preserving object, array, one-past, function, and array-element forms; focused `20/20` plus probes, through-PA14 `1058/1058`, PA15 `68/109` with the exact unchanged 41 names and all `109` covered, fresh immutable `n=256` performance evidence, file audit pass, and diff-check pass. |
