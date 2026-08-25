# PA15 Typed Null Initializer Checkpoint

## Spec Alignment and Ownership

This checkpoint covers the landed increment `dea5352e70fc42b3fa5a56bbe2b17682c581777a`
and its complete affected path under `spec.md` §§2, 3, 4, 5, and 7.

- PA11 owns canonical bindings, types, scopes, and value-conversion/common-type
  facts. Pointer-object cv is discarded in value conversion; pointee
  qualification remains checked. Reference binding continues to use the
  qualification-preserving path.
- PA12 owns semantic identities, contiguous conversion ranges, one-time
  constant evaluation, and namespace initializer facts. A transactional
  `resolve_constant_address` records one validated `ConstantAddressFact`.
  `Literal` facts carry a PA12 byte-arena range and typed element metadata;
  non-literal symbol facts carry an in-range binding target. Failed or
  unsupported recursive resolution rolls back appended literal bytes.
- PA10 remains the source owner of decoded AST literal payloads. The PA12
  literal-byte arena is the canonical downstream snapshot. PA15 consumes only
  `ConstantAddressFact` identity/ranges for literal relocation and does not
  inspect semantic kind, source payload, or conversions to rediscover it.
  PA15’s fact-identity-to-`SymbolId` map is only deterministic LowIR identity
  materialization for the required backing globals.
- PA15 consumes typed null conversion chains only when the null conversion is
  first, the linked suffix is pointer-valued and destination-safe, and the
  terminal target matches the destination. Scalar nulls lower to `INIT_ZERO`;
  array null and omitted slots lower to coalesced `ITEM_ZERO`; typed addresses
  lower to `ITEM_ADDR` or the existing typed runtime projection form.
- `apply_conversions` loads lvalues before pointer qualification/void value
  conversion. Runtime `KW_NULLPTR` is a typed pointer copy; `NullptrToBool`
  lowers through a typed pointer comparison rather than a pointer-to-bool
  type retag. LowIR `Program` remains the sole production IR model and the
  serializer only renders it.

The literal wrapper scope is deliberately narrow: a transparent cast may pass
`ArrayDecay` to an already-decoded literal, while unsupported literal pointer
arithmetic is rejected. No text/token/name lookup, fixture shortcut, host
compiler/reference shell-out, duplicate semantic/IR model, or broad string
expression lowering is present. The affected work is ordinary `O(n)` or
`O(n log n)` and zero data is coalesced in one pass.

## Exact Failure Map and Coverage

The authoritative turn-start baseline was `70/109` passing, all `109` covered,
with exactly the following 39 failures. The fresh final result is also
`70/109`, all `109` covered, and its failure set is byte-for-byte identical:
zero new names and zero removed/replaced names.

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

The two landed target names are absent from this residual set and pass in the
focused and full gates.

## Focused and Final Evidence

| evidence | fresh result |
|---|---|
| `make -C dev cppgm++` | exit `0` |
| affected PA15 matrix: 10 address/null/array cases | pass, `10/10`; `/tmp/pa15-final-focused-affected-matrix.log` |
| `cppgm.tests/course/pa15/401-typed-pointer-null-cv-regression.sh` | exit `0`; scalar nulls, top-level pointer cv load, terminal chain, nullptr-to-bool, and pointee-drop rejection |
| transparent literal-cast probe | `global @__strlit__1` plus `global @value ... = addr @__strlit__1`; `/tmp/pa15-final-literal-wrapper.log` |
| unsupported literal arithmetic probe | rejected as `PA15 nonconstant global initializer`; `/tmp/pa15-final-literal-arithmetic.log` |
| `make test-pa15` | exit `2`, `70/109`, all `109` covered, exact unchanged 39-name set; `/tmp/pa15-final-full.log` |
| exact `n=15` through-PA14 command | exit `0`, `1058/1058`; `/tmp/pa15-final-through-pa14.log` |
| `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` | exit `0`, five pre-existing header warnings; `/tmp/pa15-final-file-audit.log` |
| `git diff --check` | pass |

The exact through-stage command was:

```sh
n=15; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

The turn-start/final set comparison artifacts are
`/tmp/pa15-turn-start-failures-final-audit.txt` and
`/tmp/pa15-final-failures-final-audit.txt`; both contain 39 names, with empty
set differences.

## Representative Performance Evidence

Fresh final evidence uses immutable candidate
`/tmp/pa15-final-perf-final.hvQKB0/cppgm++-final-immutable`, mode `0555`,
SHA-256
`4d9ae4004642bdf402118ef3328efe417a5d8d4427de033de6eba700b8658dd9`.
The artifact directory contains `candidate.sha256`, generated bounded inputs
and outputs, `structure.tsv`, raw interleaved `timings.tsv`, and
`medians.tsv`. Five rounds interleave `mixed` and `repeated` inputs at sizes
`32, 128, 512` (reverse size order on even rounds); each timing sample is 20
repeated compilations.

| family | n | input bytes/lines | LowIR lines | semantic lines | globals | addr/zero items | literal facts | median wall/user/sys s | median RSS KiB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| mixed | 32 | 276/27 | 99 | 41 | 13 | 12/12 | 28 | 0.003500/0.001000/0.002000 | 5160 |
| mixed | 128 | 817/99 | 315 | 113 | 49 | 48/48 | 100 | 0.004000/0.002000/0.002000 | 5396 |
| mixed | 512 | 2977/387 | 1179 | 401 | 193 | 192/192 | 388 | 0.006000/0.003500/0.003000 | 5928 |
| repeated | 32 | 479/35 | 349 | 51 | 33 | 32/0 | 37 | 0.004000/0.001500/0.002000 | 5160 |
| repeated | 128 | 1632/131 | 1309 | 147 | 129 | 128/0 | 133 | 0.005000/0.002500/0.002000 | 5648 |
| repeated | 512 | 6240/515 | 5149 | 531 | 513 | 512/0 | 517 | 0.010500/0.006000/0.004000 | 6896 |

`mixed` exercises repeated literal facts, pointer `nullptr`, integer-zero
pointer initializers, and omitted trailing slots. `repeated` stresses literal
identity/materialization. These bounded measurements show no timeout or
unexpected superlinear behavior on the affected path; they are not universal
performance claims.

Historical evidence is preserved but not used as final proof. The prior
typed-address/value candidate and measurements remain under
`/tmp/pa15-typed-relocation-correction.6EMMbb/context-perf`; the prior
pre-repair null-pointer candidate and measurements remain under
`/tmp/pa15-null-pointer-perf.QgTN8n`. The earlier progress/evidence is
historical, while the performance claims above use the final immutable
candidate.

## Active and Next Checkpoint

The typed global pointer null/zero initializer checkpoint is complete. The
next bounded checkpoint is typed enum scalar lowering, selected from residual
`200-enum-class-scalar-lowering` and related scoped/unscoped enum failures:
`200-scoped-enum-global-constant-init`,
`200-scoped-enum-unsigned-high-bit`, and
`200-unscoped-enum-promotion-overload`.

## Checkpoint Ledger

| status | checkpoint | completed evidence |
|---|---|---|
| Complete | PA15 full-stage / typed global pointer null-zero lowering | Transactional PA12 literal snapshot and binding invariants; PA15 typed `ConstantAddressFact` consumer; terminal-safe null chains; pointer cv and lvalue-load repairs; typed runtime `nullptr`-to-bool path; target/affected matrix `10/10`, narrow regression pass, final PA15 `70/109` with the exact unchanged 39 failures and all `109` covered; through-PA14 `1058/1058`; file audit, diff check, and fresh immutable performance evidence passed. |
| Next | PA15 / typed enum scalar lowering | Selected from the residual enum failures above; no enum surfaces were changed in this checkpoint. |
| Historical | PA15 full-stage / checkpointAudit — typed address/value ownership | Preserved prior PA12 `Value`/`ObjectAddress`/`ArrayDecay` relocation ownership correction with focused `20/20`, through-PA14 `1058/1058`, historical PA15 `68/109`/41-name evidence, immutable performance evidence, file audit, and diff-check pass. |
