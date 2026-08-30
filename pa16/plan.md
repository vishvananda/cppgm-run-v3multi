# PA16 typed `nullptr_t` carrier checkpoint

## Current Spec Alignment

The Purpose and §§1, 2, 5, and 7 remain satisfied by one self-contained,
typed production path: PA11 publishes `FundamentalType::NullptrT`; PA12 owns
typed zero-to-nullptr conversion and overload selection; PA15 carries that
semantic type through ABI and LowIR; adapters parse/render only at their real
boundaries.  `NullptrT` remains distinct semantic and ABI identity while its
Linux x86_64 physical carrier is `i64`.  The path has no textual downgrade,
host/reference shortcut, duplicate pipeline, test-name branch, or unbounded
scan/retry.  New source guards fail closed on malformed nullptr identity,
target, or carrier facts.  The cohesive ABI owners are now in a dedicated
PA15 translation unit, named directly in the `cppgm++` source set; this is an
ownership extraction, not a second pipeline.

## Active Ownership Path

```text
PA11 FundamentalType::NullptrT (size/alignment 8/8)
  -> PA12 typed NullIntegerToNullptr ConversionFact and overload ranking
  -> PA15 pa15_lowering_abi.cpp::abi_type_nested
     -> ABI_BUILTIN_NULLPTR -> existing Itanium Dn
  -> PA15 low_type -> TYPE_INTEGER/INTEGER_I64
  -> typed parameter records, slots/stores, call operands, literals,
     conversions, returns, and LowIR validator/adapter boundaries
```

`abi_type_nested` preserves the separate `ABI_BUILTIN_NULLPTR` identity and the
existing encoder produces `_ZneRK3PtrDn`.  `collect_function`, declarations,
`lower_call`, and `lower_function` all use the existing typed `low_type` owner,
so pass-by-value `nullptr_t` has an i64 parameter, slot, store, and call
operand.  `literal()` now gives semantic `NullptrT` keyword literals the same
i64 carrier; a typed `NullptrToPointer` conversion evaluates that carrier and
emits a pointer-null operand, while `NullptrToBool` requires a semantic bool
target with canonical u8 physical width, compares the i64 carrier, preserves
i64 in condition context, and materializes u8 only outside that context.
Pointer-typed literal output remains unchanged.  The extracted ABI module
contains the six existing ABI definitions, with no duplicate or fragment
include; shared operator ABI helpers remain in `pa15_operator_abi.cpp`.

## Exact Authority Failure Map

The supplied latest authority
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` reports
status `2`, `225/243` passed, exactly `18` failures, and full
`243/243` discovered/reference/fresh identity coverage.  The final
post-extraction run is also `225/243` with the exact same normalized failure
set: authority `18`, fresh `18`, retained `18`, authority-only `0`, and
fresh-only `0`.  Final discovered/reference/fresh coverage is `243/243/243`,
with zero delta in every pairwise direction.  This is the complete residual
map:

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
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The 18 residuals are outside this nullptr checkpoint and remain untouched.  The
signed bit-field references remain a known residual oracle tension; no fixture
or reference change is warranted here.

## Finding and Disposition

The landed same-line switch cases were reformatted.  The deeper owner defect
was that `literal()` still emitted keyword `nullptr` as a pointer temporary
after `NullptrT` acquired an i64 physical carrier.  The repair makes direct
semantic `NullptrT` literals i64, preserves pointer-typed literal output, and
makes typed `NullptrToPointer`/`NullptrToBool` consumption validate semantic
endpoints and canonical carriers before emitting valid LowIR.  The pointer
consumer allows a typed reference source so its lvalue load and effects remain
evaluated, but requires a non-reference semantic pointer target; the bool
consumer requires a non-reference semantic bool and canonical u8 target, while
retaining i64 condition-context truth.  The narrow `NullIntegerToNullptr`
consumer check complements PA12's existing typed producer/global invariants
by requiring an integral typed zero source, exact `NullptrT` target, i64
carrier, and integer zero.  No generic conversion refactoring, fixture
change, or reference regeneration is justified.

The file-audit size correction extracts the six existing cohesive ABI method
definitions into `dev/src/pa15_lowering_abi.cpp` and adds its basename to the
`cppgm++` source set.  `pa15_lowering.cpp` is `2886` lines and the new module is
`264` lines; this is a move of definitions with no implementation fragment,
duplicate pipeline, or behavior change.

## Focused Evidence

Final post-extraction evidence, run serially:

```text
make -C dev cppgm++ CXX=g++                                      status 0
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check \
  TEST='tests/general/300-operator-nullptr-t-from-zero.t'          PASS (1/1)
make -C pa12 CPPGM_SKIP_DEV_REBUILD=1 check \
  TEST='tests/spec/300-nullptr-t-from-zero-overload.t tests/general/300-nullptr-equality.t tests/spec/300-nullptr-pointer-conversion.t tests/general/100-nullptr-static-cast-pointer.t'
                                                                    PASS (4/4)
make -C pa13 CPPGM_SKIP_DEV_REBUILD=1 check \
  TEST='tests/spec/100-nullptr-return-lowir.t'                     PASS (1/1)
make -C pa15 CPPGM_SKIP_DEV_REBUILD=1 check \
  TEST='tests/general/200-global-pointer-array-nullptr-init.t'    PASS (1/1)
```

The final direct `nullptr` endpoint probe, including global-lvalue and
reference-return sources, compiled with `cppgm++` and passed `dev/lowir2cy86`
(status `0`).  Pointer paths show `load i64` before `copy ptr nullptr`; bool
paths show `cmp ne i64` followed by `convert trunc u8 i64`.  The checked-in
PA12 `300-nullptr-equality.t` emitted direct `cmp ne i64 nullptr, nullptr` and
`store i64 nullptr`; its final output passed `dev/lowir2cy86` (status `0`).
Temporary `return nullptr` and `sink(nullptr)` probes also passed the backend
and emitted the same typed compare/truncation boundary.

Final post-extraction broad evidence is `make test-pa16` status `2` at
`225/243`, with exactly the 18 identities above.  The exact normalized
comparison against the supplied authority is `18` authority / `18` fresh,
authority-only `0`, fresh-only `0`, and retained `18`.  Discovered/reference/
fresh inventories are `243/243/243`, with zero pairwise deltas.  The exact
prior-through-PA15 command passes at `1167/1167`.  The exact file audit passes
with five known `bad-division` warnings in `abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`, and
`pa15_lowering.h`.  `git diff --check` passes.

## Representative Performance Evidence

Final post-extraction structural O0 counters from generated LowIR are counts,
not timing claims:

| input | functions | instructions | loads | stores | calls | comparisons | i64 lines |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| nullptr operator target | 2 | 20 | 3 | 5 | 1 | 1 | 3 |
| PA12 nullptr equality control | 3 | 33 | 6 | 8 | 2 | 4 | 15 |
| PA15 pointer-null control | 1 | 28 | 5 | 4 | 0 | 3 | 5 |
| nullptr lvalue endpoint probe | 7 | 58 | 8 | 9 | 6 | 9 | 25 |

The target has one i64 pass-by-value parameter and one exact `Dn` symbol.  The
ABI extraction changes translation-unit ownership only; the final target,
equality, pointer, and endpoint LowIR outputs are byte-identical before and
after extraction.  The changed boundary adds two constant-time type mappings
and no per-expression scan, allocation, cache, or whole-program traversal.
Raw LowIR, structural counters, validator results, and command logs are under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-nullptr-carrier-checkpoint-20260830/`.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `d54e32d1` parent authority | `224/243`, exactly 19 failures, `243/243` identities; clean baseline before `b58ddd2a` |
| rejected packed-bit-field candidate | reverted completely in all four PA15 source files; no rejected source diff remains |
| `b58ddd2a` typed nullptr carrier | Completed the bounded carrier audit and structural ABI extraction: PA11/PA12/PA15 typed ownership, exact pointer/bool endpoints, canonical i64/u8 carriers, lvalue source evaluation, and the narrow integer-zero consumer guard are recorded. Final post-extraction build and focused PA16 `1/1`, PA12 `4/4`, PA13 `1/1`, PA15 `1/1` pass; target/equality/pointer/endpoint LowIR is byte-identical across extraction; final PA16 is `225/243` with exactly 18 failures, exact comparison authority-only `0`/fresh-only `0`, and `243/243/243` inventories; prior-through is `1167/1167`; file audit passes with five known warnings; no fixture/reference change. Final changed paths are exactly `dev/src/pa15_lowering.cpp`, `dev/src/pa15_lowering_abi.cpp`, `dev/frontend_source_sets.mk`, `pa16/audit.md`, and `pa16/plan.md`. |
