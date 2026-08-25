# PA15 Checkpoint Plan

## Boundary and Spec Alignment

PA15 keeps PA12 as the semantic owner and lowers its typed statement graph
directly into the shared typed `lowir_model::Program`. No rendered semantic
dump, LowIR text, fixture, or reference output is parsed for implementation
state. The bounded slice is `while`, `do`, `for`, `break`, `continue`,
condition declarations, `switch`/`case`/`default`, fallthrough, and direct
`&&`/`||` condition branching. Scalar/global/pointer/enum/floating/goto and
other remaining PA15 groups stay explicit nonclaims.

Typed `ScopeId`, `BindingId`, `SemanticFactId`, `TypeId`, `BlockId`,
`Operand`, `Instruction`, and `SlotId` identities remain intact through
lowering and serialization. The scope owner index is one stage-wide
parent-before-child propagation over `O(S+B+F)` structural input and uses
ordered ownership maps for an honest `O((S+B+F) log B)` slot bound. Switch
label collection is one traversal per owning switch and stops at nested switch
facts. The PA12 switch-transfer prepass is one source-order structural walk
with typed lexical frames (`ScopeId`, initialized-automatic count) and an
active total; it does not allocate a node-based map per switch. `DeclarationFact`
stores the automatic-storage fact sourced once from `SpecFact`, so local
`static`, `extern`, and `thread_local` declarations are not misclassified as
automatic in this procedural subset. The PA15 repair records each typed CFG
edge at emission and propagates reachability monotonically through a worklist;
loop recovery reuses a dense `SemanticFactId`-indexed target table allocated
once for the translation unit rather than resetting it per function. The
reachability bitset has no duplicate adjacency index: a newly reachable block
inspects its canonical typed terminator once, while an emitted edge from an
already reachable source marks its target immediately.

For `A` consumed PA12 facts, `S` scopes, `B` bindings, `N` functions, and `E`
typed IR edges, the transfer walk is `O(A)` with lexical-depth state; the
stage-wide scope/slot owner index is `O(S+B+N)` propagation plus the existing
deterministic `O((S+B+N) log B)` ordered indexes; the loop-target table is
initialized once in `O(A)` space/time; and reachability is `O(B+E)`. Structured
label/recovery traversal is linear in owned facts, giving an affected total of
`O(n log n)` under spec.md §4.

## Current Failure Map and Coverage

The clean turn-start baseline is **21/109 passing, 88 failing, all 109
covered**, from `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The exact failing inventory is:

```text
100-array-cv-rvalue-reference-overload  100-c-linkage-reference-declaration-metadata
100-condition-declaration-variable-rvalue  100-const-integral-lvalue-overload-category
100-enum-default-argument-constant-fold  100-extern-unknown-bound-array-reference
100-function-pointer-ref-call  100-global-function-pointer-argument-call
100-global-variable  100-scoped-enum-braced-assignment
100-scoped-enum-previous-enumerator-bitwise-or  100-sizeof-local-value-shadows-type-name
100-string-hex-escape-code-unit  100-subscript-sizeof
100-unary-logical-conditional  100-unary-plus-array-decay
100-unnamed-parameter-storage  100-using-directive-imported-value-function-body
200-address-of-local-const-integral-uses-storage  200-comma-expression-lvalue-address
200-comma-expression-xvalue-reference-return  200-compound-assignment-evaluates-lhs-once
200-conditional-array-decay-subscript  200-const-cast-pointer-const-drop
200-const-cast-reference-array-subscript  200-const-cast-reference-similar-pointer
200-const-ref-converted-float-argument  200-enum-class-scalar-lowering
200-extern-c-internal-header-const  200-extern-function-pointer-indirect-call
200-floating-compound-assign-integral-rhs  200-floating-condition-declaration-negative-zero
200-floating-logical-branch  200-floating-return-integral-conversion
200-for-init-assignment-expression  200-for-iteration-discards-void-comma-rhs
200-function-reference-static-cast-call  200-functional-reference-typedef-cast
200-generated-slot-name-collision  200-global-address-reinterpret-cast-initializer
200-global-array-bitwise-or-enum-init  200-global-array-conditional-cast-initializer
200-global-array-decay-compare  200-global-array-element-address-initializer
200-global-array-one-past-end-pointer  200-global-array-scalar-cast-init
200-global-array-static-const-byte-init  200-global-object-address-initializer
200-global-pointer-array-null-fill  200-global-pointer-array-nullptr-init
200-global-pointer-array-subscript-load  200-goto-case-block-entry-label
200-goto-case-block-label-after-statement  200-included-namespace-global-definition
200-inferred-local-array-bound  200-integral-multiply-compound-assignment
200-literal-logical-short-circuit-omits-unreachable-call
200-local-direct-init-array-subscript  200-local-function-type-typedef-reference
200-local-int-slot-width  200-local-lvalue-reference-alias-init
200-lvalue-conditional-address  200-lvalue-conditional-reference-return
200-namespace-default-argument-declaration-lookup  200-nested-conditional-array-decay
200-partial-local-array-zero-initialization  200-pointer-compound-assignment-scale
200-pointer-deref-byte-load  200-pointer-operator-array-decay
200-postfix-incdec-evaluates-lhs-once  200-prefix-incdec-lvalue-address
200-prefix-pointer-decrement-reference-argument
200-qualified-namespace-overload-definition-symbol
200-reference-parameter-temp-name-collision  200-reinterpret-enum-to-pointer
200-reinterpret-reference-conditional-materialization  200-return-void-call-expression
200-scalar-assignment-address-lvalue  200-scalar-reference-static-cast-return
200-scoped-enum-global-constant-init  200-scoped-enum-underlying-type
200-scoped-enum-unsigned-high-bit  200-signed-enum-compare-lowering
200-switch-case-nested-inside-if  200-unscoped-enum-promotion-overload
200-variadic-float-argument-promotes-to-double  200-wide-unscoped-enum-promotion
300-return-empty-braces-scalar
```

The structured residuals are condition-declaration rvalues, unary/logical
conditional expressions, floating condition declarations, for expressions,
literal logical short-circuit call references, goto/case entry, and the
scalar expression in `200-switch-case-nested-inside-if`. They were not
expanded in this checkpoint.

## Final Checkpoint Evidence

- Final source repair: typed PA12 switch-entry validation plus typed PA15
  switch-label/loop recovery.
- `make -C dev cppgm++`: passed.
- Focused implicated checked-in set: **9/9 passed**, including the expected
  failure for `100-switch-label-bypasses-initialization-bad`.
- Fresh probe matrix: no-label, braced-case, exhaustive non-void, nested
  while/do/for-without-init/if labels all compiled and passed `lowir2cy86`;
  nested for-init and while-condition bypass probes were rejected by PA12.
- Storage-duration probes: initialized local `static` compiled and passed
  `lowir2cy86`; initialized automatic storage was rejected by PA12. Four
  label-after-termination probes (top-level, `while`, `do`, `for`) compiled
  and passed `lowir2cy86`.
- No test, fixture, reference, harness, or coverage file changed. The final
  root through-PA14 gate passed **1058/1058**. The final full PA15 report is
  **21/109 passed, 88 failed, all 109 covered**; its complete failing set is
  identical to the turn-start baseline. The final PA15 file audit passed with
  the four existing header-division warnings listed in `pa15/audit.md`, and
  the final diff check passed.

## Retained Performance and Prior Gate Evidence

The following table is historical target-commit evidence copied from the
prior checkpoint. The previously recorded paths
`/tmp/pa15-perf-amend.hd5A6F/measurements.tsv` and
`/tmp/pa15-perf-amend.hd5A6F/cppgm++-pa15-fresh` do not exist at this
checkpoint, so neither artifact is claimed to be inspectable. The recorded
executable SHA-256 was
`31093ff568ac9d787855b64720c6933b593d2b3d734e7e4b91dba55d71a32972`.

| Family / size | Median ms | Input B | Output B | Functions | Blocks | Instructions | Cases |
|---|---:|---:|---:|---:|---:|---:|---:|
| many functions / 8 | 5.310059 | 1430 | 8806 | 9 | 89 | 226 | 0 |
| many functions / 16 | 7.253170 | 2836 | 17490 | 17 | 177 | 450 | 0 |
| many functions / 32 | 11.035204 | 5652 | 34866 | 33 | 353 | 898 | 0 |
| nested switches / 4 | 4.029036 | 320 | 1435 | 2 | 18 | 29 | 4 |
| nested switches / 8 | 4.088879 | 656 | 2603 | 2 | 34 | 53 | 8 |
| nested switches / 16 | 4.904032 | 1616 | 4939 | 2 | 66 | 101 | 16 |

Prior target-commit evidence also recorded **1058/1058 through PA14**, a
successful PA15 file audit with four existing header-division warnings, and
the then-clean diff check. Those measurements remain historical; the final
gates for this corrected checkpoint are recorded above.

Fresh corrected-executable evidence uses the immutable copy
`/tmp/pa15-structured-correction.wf5una/cppgm++-corrected`, SHA-256
`6e5843f5e44966fd1b4f62b98e3d5ed829b306afb78024b6c1f4e8e180054688`.
The interleaved raw log is
`/tmp/pa15-structured-correction.wf5una/measurements-final.tsv`, with medians
in `.../measurements-final-medians.tsv`. The nested sources are
`recovery-{32,64,128,256}.cpp`; the many-function/one-loop-per-function
sources are `many-functions-{32,64,128,256}.cpp`, all in that same existing
directory. Each round interleaves `nested:32, many:32, nested:128, many:128,
nested:64, many:64, nested:256, many:256`; all 40 compilations returned
status 0. The depth-256 LowIR outputs for both families passed `lowir2cy86`.

| Nested recovery depth | Samples | Median ms | Source B | Blocks | Instructions | Switches |
|---:|---:|---:|---:|---:|---:|---:|
| 32 | 5 | 115 | 1131 | 102 | 175 | 1 |
| 64 | 5 | 114 | 2091 | 198 | 335 | 1 |
| 128 | 5 | 115 | 4011 | 390 | 655 | 1 |
| 256 | 5 | 114 | 7851 | 774 | 1295 | 1 |

| Many functions / loops | Samples | Median ms | Source B | Functions | Blocks | Instructions | Switches |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 32 | 5 | 115 | 1775 | 33 | 129 | 321 | 0 |
| 64 | 5 | 114 | 3535 | 65 | 257 | 641 | 0 |
| 128 | 5 | 114 | 7083 | 129 | 513 | 1281 | 0 |
| 256 | 5 | 115 | 14251 | 257 | 1025 | 2561 | 0 |

Nested counters are `blocks = 3d + 6`, `instructions = 5d + 15`; the
many-function counters are `functions = d + 1`, `blocks = 4d + 1`, and
`instructions = 10d + 1`. The repeated medians show no timeout or
multiplicative growth in either family. The exact unreachable nonempty-tail
probe retained its source stores and ended with a typed self-jump; its LowIR
passed `lowir2cy86`. The reachable non-void fallthrough probe rejected with
`PA15 function falls through without return`. The 114–115 ms process medians
are startup-dominated, so the structural counters and repeated successful
runs are the stronger complexity evidence.

## Next Checkpoint

Next checkpoint: the next remaining PA15 code capability after this audit,
starting with the scalar expression/lvalue boundary. It will be scoped and
audited separately; this structured-control checkpoint completes its own
validation and release evidence here.

## Checkpoint Ledger

| Status | Evidence |
|---|---|
| Completed — structured-control audit/repair milestone | PA12 typed switch-transfer ownership, PA15 loop/switch recovery, direct short-circuit branching, deterministic block retention, scope indexing, focused 9/9 validation, and validator probes completed; final broad gates and file audit are recorded in the checkpoint review. |
