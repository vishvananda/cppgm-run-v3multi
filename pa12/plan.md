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

## Failure Map

The supplied turn-start baseline was `166/166` covered and `155/166` passing,
with exactly these 11 failures:

1. `pa12/tests/general/300-decltype-functional-cast.t` — final residual; expected success, got failure.
2. `pa12/tests/general/300-local-extern-function-declaration.t` — final residual; expected success, got failure.
3. `pa12/tests/general/300-member-function-pointer-return-pointer-const.t` — fixed; full-stage pass.
4. `pa12/tests/general/300-member-function-pointer-type-alias-and-function.t` — fixed; full-stage pass.
5. `pa12/tests/general/300-member-pointer-type-alias-and-function.t` — fixed; full-stage pass.
6. `pa12/tests/general/300-multidimensional-array-const-reference-binding.t` — fixed; full-stage pass.
7. `pa12/tests/general/300-reference-binding-pointee-const-pointer.t` — final residual; checked-output mismatch, intentionally outside this boundary.
8. `pa12/tests/general/300-scoped-enum-functional-cast-integral.t` — final residual; expected success, got failure.
9. `pa12/tests/general/300-static-cast-member-overload-prefers-nontemplate.t` — fixed; full-stage pass.
10. `pa12/tests/general/300-static-cast-overloaded-function-template-argument.t` — final residual; expected success, got failure.
11. `pa12/tests/general/300-zero-arg-functional-cast-alias.t` — final residual; expected success, got failure.

Final PA12 is `160/166` passing with six residuals. The final residual set is
exactly the supplied future set: cases 1, 2, 7, 8, 10, and 11; current-only
failures `0`, supplied-only fixed cases `5`.

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
  bounds and element structure. Static member declarations are tracked
  sparsely and render/select as ordinary function pointers.

## Performance Evidence

After the final implementation, a temporary layout probe (removed before the
final rebuild) measured: `TypeKey 80`, `Scope 440`, `DeclaratorOp 40`,
`FunctionFact 48`, `BindingSidecar 32`, and `SpecFact 32` bytes. `TypeKey` did
not gain fields; removing `FunctionFact::display_type` returned that record to
its prior compact layout. These are structural measurements, not timing claims.

A bounded probe generated 200 typedef declarations at each nested declarator
depth, ran three times per depth through the project compiler, and all runs
exited `0`:

| depth | three elapsed samples (ms) | median |
|---:|---:|---:|
| 32 | 56.879, 55.774, 57.938 | 56.879 |
| 128 | 192.884, 193.411, 193.567 | 193.411 |
| 512 | 726.358, 731.574, 741.727 | 731.574 |

The 4x depth ratios were approximately `3.40x` and `3.78x`. Startup and
process overhead are included; this is bounded representative evidence, not a
formal complexity proof.

## Checkpoint Ledger

| checkpoint | evidence | result |
|---|---|---|
| Previously landed namespace/type audit checkpoint | commit `3508ae67` | retained as the parent checkpoint; no prior commit amended |
| Current focused implementation | `make -C pa12 -j2`; five active PA12 tests plus 15 nearby PA12 controls | build exit `0`; PA12 `PASS (20/20)` |
| Declarator controls | four checked-in PA10 member-pointer controls | `PASS (4/4)` |
| Negative/shape probes | owner mismatch, function-cv mismatch, data/function distinction rejected; 3D array/cv accepted; static member output has no `this`, non-static const member does | expected statuses and output checks passed |
| File audit | `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` | exit `0`; two existing header-division warnings only |
| Full PA12 | `make test-pa12`, all `166/166` covered | `160/166` pass; exact six residuals are cases 1, 2, 7, 8, 10, 11 |
| Through PA11 | required `n=12` / `make test-report-through-pa11` command | `685/685` passed |
| Source integrity | `git diff --check` | clean before commit; final clean-tree check pending commit |

## Remaining Scope

The six final residuals remain visible for later checkpoints: functional-cast
`decltype`, local extern declarations, pointee-const-pointer reference
binding, scoped-enum functional casts, overloaded function-template target
selection, and zero-argument functional-cast aliases. No general class-aware
calls or template semantics were added. The corrected PA11 class-scope
regression and static-member probe are included in the final through-stage
evidence.
