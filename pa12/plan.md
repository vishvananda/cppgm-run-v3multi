# PA12 canonical declarator/type checkpoint

## Stage Design

`PA11SemanticModel` remains the sole semantic owner in the forward PA10 ->
PA11 -> PA12 pipeline. `TypeKey`/`TypeId` is the only semantic type identity;
member-pointer class ownership is a typed `NamedRecordId`, function cv is a
typed flag, and equality remains O(1). Canonical type construction and
declarator application now live in `pa11_semantic_types.cpp`; PA12
target-directed function/member selection lives in
`pa12_semantic_resolution.cpp`. Declarator work is O(depth): prefix operators,
reverse suffix binding, nested operators, array cv recursion, and structural
array qualification are resolved before PA12 use. The renderer is cold and
derives member display types on demand; there is no reparsing, parallel model,
or string-based identity.

The PA12 extension is narrow: exact owner/function `TypeId` matching supports
the active member-pointer overload case. Static member functions are retained
in a sparse binding sidecar and do not acquire an implicit object parameter.
Class-aware calls and general template semantics remain outside this checkpoint.

## Spec Alignment

Sections 1–3 remain a single forward production pipeline with one PA11
semantic owner, typed fact continuity, stable `TypeKey`/`TypeId` identity, and
cold deterministic rendering. Section 4 is met by the typed canonical key,
sparse static-member sidecar, bounded vector/index storage, and O(depth)
declarator and qualification work. Section 7 is met by deterministic output
and the inherited structural/depth evidence; this checkpoint adds no new
timing claim.

## Failure Map

The supplied turn-start baseline is `166/166` covered and `160/166` passing,
with exactly these six residuals; no focused change replaces one with a new
failure:

1. `pa12/tests/general/300-decltype-functional-cast.t` — expected success, got failure.
2. `pa12/tests/general/300-local-extern-function-declaration.t` — expected success, got failure.
3. `pa12/tests/general/300-reference-binding-pointee-const-pointer.t` — checked-output mismatch.
4. `pa12/tests/general/300-scoped-enum-functional-cast-integral.t` — expected success, got failure.
5. `pa12/tests/general/300-static-cast-overloaded-function-template-argument.t` — expected success, got failure.
6. `pa12/tests/general/300-zero-arg-functional-cast-alias.t` — expected success, got failure.

The increment's earlier pre-implementation baseline was `155/166` with
eleven failures; relative to that baseline, it fixed these five paths:
`300-member-function-pointer-return-pointer-const.t`,
`300-member-function-pointer-type-alias-and-function.t`,
`300-member-pointer-type-alias-and-function.t`,
`300-multidimensional-array-const-reference-binding.t`, and
`300-static-cast-member-overload-prefers-nontemplate.t`. Separately, the
turn-start audit baseline was `160/166` with the exact six residuals above;
fresh final PA12 has the same six paths, with zero current-only and zero
baseline-only paths. No residual family was admitted by this checkpoint.

## Active Checkpoint

- `TypeKind::MemberPointer` uses `TypeKey.named` for its class owner, `child`
  for the data/function member type, and `cv` for top-level qualifiers;
  plain pointers and data/function member pointers remain distinct.
- Function cv is canonical and member-function expression types use one typed
  implicit-object helper. PA12 preparation adds the synthetic `this` binding
  only after PA11 analysis, so PA11 class-scope output is unchanged. The cold
  renderer derives the member definition view without storing a dump-only
  `FunctionFact` field.
- Nested arrays bind in reverse source order. Qualifying an array alias
  recursively qualifies its element type, and qualification conversion checks
  bounds and element structure. The bounded repair also makes
  `cv_qualifiers` recurse through array elements and preserve direct
  pointer/member-pointer cv, so const array and const pointer objects cannot
  incorrectly convert to unqualified `void*`. Static member declarations are
  tracked sparsely and render/select as ordinary function pointers.

## Performance Evidence

The landed increment's retained structural measurements are: `TypeKey 80`,
`Scope 440`, `DeclaratorOp 40`, `FunctionFact 48`, `BindingSidecar 32`, and
`SpecFact 32` bytes. This repair adds no fields, so no layout rerun was needed;
these remain structural measurements, not timing claims.

Fresh performance evidence used the newly built executable copied to an
immutable out-of-tree path. Five interleaved rounds each compiled equivalent
generated inputs containing 200 typedef declarations with pointer-prefix
declarator depths 32, 128, and 512; every run exited `0`.

| depth | wall samples (ms) | median wall (ms) | median user (ms) | median sys (ms) | median max RSS (KiB) |
|---:|---|---:|---:|---:|---:|
| 32 | 10, 10, 10, 10, 10 | 10 | 0 | 0 | 10464 |
| 128 | 60, 50, 50, 50, 60 | 50 | 20 | 30 | 26892 |
| 512 | 230, 220, 230, 220, 220 | 220 | 120 | 100 | 91860 |

Startup, process, and timer-resolution effects are included. This is bounded
representative evidence only and makes no broader timing or asymptotic claim.

## Checkpoint Ledger

The audit history preserves prior progress at `90/166`, `103/166`, `113/166`,
`120/166`, `142/166`, `146/166`, `149/166`, and `155/166`, followed by the
increment's `160/166` turn-start baseline. The later broad records covered all
166 PA12 paths; through-PA11 remained `685/685`, and the known file-audit
result retains two header-division warnings.

| checkpoint | evidence | result |
|---|---|---|
| PA12 canonical declarator/member-target checkpoint | landed `4f890322` plus the bounded `cv_qualifiers` repair; focused PA12 `20/20`; PA10 `4/4`; 12 probes; two rendering assertions; fresh broad PA12 `160/166`; through-PA11 `685/685` | complete; the exact six turn-start residuals are unchanged |

## Next Checkpoint

The next separately authorized checkpoint is the remaining
functional-cast/alias residual family: `decltype` functional cast,
scoped-enum functional cast, and zero-argument functional-cast alias. Local
extern declarations, pointee-const-pointer reference binding, and overloaded
function-template target selection remain residual and outside this audit.

## Remaining Scope

The six final residuals remain visible for later checkpoints: functional-cast
`decltype`, local extern declarations, pointee-const-pointer reference
binding, scoped-enum functional casts, overloaded function-template target
selection, and zero-argument functional-cast aliases. No general class-aware
calls or template semantics were added.
