# PA12 typed constant/intrinsic call boundary

## Stage Design

`PA11SemanticModel` remains the single semantic owner. PA11 core forms canonical
`TypeId`, `BindingId`, `ScopeId`, declaration, and constant-value facts. PA12
forms expression/call/conversion facts and renders the existing deterministic
dump. The flow is PA10 AST -> PA11 typed identity -> PA12 typed facts -> cold
rendering; names and types are never rendered and reparsed.

This checkpoint classifies the exact source spelling of the two supported
intrinsics once into `BuiltinKind`. `__builtin_constant_p` validates its one
operand locally, folds the supported integral constant-expression subtree, and
returns a typed `int` literal (1 or 0); the intrinsic call and operand facts do
not remain in the dump. `__builtin_abort()` accepts exactly zero arguments and
uses one model-owned function `BindingId` for its typed void call and stable
callee spelling. Ordinary unknown identifiers still use ordinary lookup.

Constexpr declaration facts retain their typed spec flag. Only a complete
constexpr object initializer gets the checked target-directed literal fact
typing; the original conversion is still recorded against the source fact, so
ordinary declarations and conversion rules are unchanged. Call work is local
to the callee AST and argument count/subtrees. No whole-arena scan, retry loop,
per-node owning string, new semantic path, test/ref/grammar/harness change, or
new `.cpp` is permitted for this checkpoint.

## Failure Map

Turn-start baseline at commit `43105867`: PA12 `142/166` passing, exactly `24`
failing, and all `166/166` tests covered. The complete turn-start map is:

Active four-path checkpoint:

- `pa12/tests/general/200-builtin-constant-p-propagated-expression.t`
- `pa12/tests/general/200-constexpr-complete-object-cv.t`
- `pa12/tests/general/300-builtin-abort-semantics.t`
- `pa12/tests/general/300-builtin-constant-p-call.t`

Excluded residual families (20 paths; not changed by this checkpoint):

- Declaration/anonymous-union formation:
  - `pa12/tests/general/200-local-anonymous-union-variable.t`
  - `pa12/tests/general/300-block-anonymous-union-injected-members.t`
  - `pa12/tests/general/300-elaborated-local-struct-copy-init.t`
  - `pa12/tests/general/300-local-extern-function-declaration.t`
- Member-pointer, cast, and reference binding:
  - `pa12/tests/general/300-decltype-functional-cast.t`
  - `pa12/tests/general/300-member-function-pointer-return-pointer-const.t`
  - `pa12/tests/general/300-member-function-pointer-type-alias-and-function.t`
  - `pa12/tests/general/300-member-pointer-type-alias-and-function.t`
  - `pa12/tests/general/300-multidimensional-array-const-reference-binding.t`
  - `pa12/tests/general/300-reference-binding-pointee-const-pointer.t`
  - `pa12/tests/general/300-scoped-enum-functional-cast-integral.t`
  - `pa12/tests/general/300-zero-arg-functional-cast-alias.t`
- Lookup, namespace, and overload resolution:
  - `pa12/tests/general/300-namespace-function-body-later-anonymous-overload.t`
  - `pa12/tests/general/300-qualified-direct-function-hides-using-directive.t`
  - `pa12/tests/general/300-reopened-unnamed-namespace-call.t`
  - `pa12/tests/general/300-static-cast-member-overload-prefers-nontemplate.t`
  - `pa12/tests/general/300-static-cast-overloaded-function-template-argument.t`
  - `pa12/tests/general/300-unnamed-namespace-definition.t`
  - `pa12/tests/general/300-unnamed-namespace-qualified-call.t`
  - `pa12/tests/general/300-unnamed-namespace-unqualified-call.t`

The focused run removed the active four (`4/4` passing). Authorized broad
validation covered all `166/166` paths and produced `146/166` passing: the
turn-start `24` failures became exactly the excluded `20`, with no
current-only failures and no supplied-baseline residual left unresolved.
The current residual set is therefore the 20 paths listed above; these remain
excluded because they belong to declaration/anonymous-union, member-pointer/
cast/reference, or lookup/namespace/overload families rather than this
intrinsic boundary.

## Active Checkpoint

Implementation scope is exactly:

- `dev/src/pa11_semantic_model.h`: typed builtin identity, declaration-owned
  constexpr flag, and model-owned builtin binding state.
- `dev/src/pa11_semantic_core.cpp`: initialize builtin identities and publish
  the constexpr declaration fact; own builtin spelling classification, the
  abort binding, and shared type normalization.
- `dev/src/pa12_semantic.cpp`: local intrinsic call semantics and narrow
  constexpr literal retargeting; the call helper validates the AST first and
  catches only the supported integral-folding boundary.
- `pa12/plan.md`: this compact stage plan and review ledger.

Focused validation commands:

```sh
make -C pa12 -j2
make -C pa12 check TEST='tests/general/200-builtin-constant-p-propagated-expression.t tests/general/300-builtin-abort-semantics.t tests/general/300-builtin-constant-p-call.t tests/general/200-constexpr-complete-object-cv.t'
make -C pa12 check TEST='tests/spec/200-switch-statement.t tests/general/300-nonconstant-case-label-bad.t tests/general/200-sizeof-expression.t tests/spec/200-sizeof-typeid.t tests/general/300-enum-comparisons.t tests/general/200-bool-conditional-mixed-value-category.t tests/general/300-nullptr-equality.t tests/general/300-pointer-nullptr-conditional.t tests/spec/300-nullptr-pointer-conversion.t tests/general/300-pointer-to-void-drops-cv-bad.t tests/general/300-bad-pointer-integer-equality.t tests/spec/100-simple-call.t tests/general/100-builtin-prefix-user-function-call.t'
```

## Spec Alignment

- README contract: supports the PA12 propagated integral constant query,
  zero-argument abort recognition, bounded intrinsic arity, deterministic
  call/literal output, and constexpr complete-object cv facts while retaining
  ordinary call and conversion behavior.
- `spec.md` sections 1-2: one forward production pipeline and typed fact
  continuity; builtin kind, `BindingId`, `TypeId`, and source-owned conversion
  facts remain typed through the dump boundary.
- `spec.md` section 3: the semantic owner records the selected builtin callee,
  value category, result type, and conversion facts; ordinary lookup remains
  scope/candidate based.
- `spec.md` section 4: intrinsic arity and operand traversal are bounded by
  the local call AST; no whole-program retry, broad invalidation, or hot-path
  owning text is introduced.
- `spec.md` section 7: evidence below is structural and deterministic only;
  no timing, memory, scaling, or generated-code performance claim is made.

Exception-boundary audit: `semantic_expression` runs before the
`eval_constexpr` catch, so unknown names, invalid operators/conversions,
nested-call failures, and intrinsic arity errors escape as PA12 errors. The
catch is reached only for a semantically valid integral operand within the
README-supported query; a folding `runtime_error` there means that the
expression is not a propagated integral constant and therefore yields the
required typed zero. The constexpr retarget call sites are both guarded by
the declaration-owned `is_constexpr` flag, and the helper additionally
requires a complete object target and a literal source; ordinary `const`
declarations and recorded source conversions are untouched.

## Performance Evidence

Structural probes used immutable executable
`/tmp/pa12-builtin-structure-cppgm-final-immutable` (mode `555`, SHA-256
`d3f8456f118c61513ac8a41ba3d7cb9f2003b446560886101e13417e9bb80bc4`) and
inputs `/tmp/pa12-builtin-structure-small.t` (SHA-256
`4c40a09039fb6ecddb3ce5d6ad79d61669a42d86d6dad242981b45197220072a`) and
`/tmp/pa12-builtin-structure-large.t` (SHA-256
`2ba421be9f0a386ce1ab9199db11c551ed5127ac912b314398aed9390d4ec725`).
Each input was compiled twice with the immutable executable:

| probe | builtin calls | literal facts | binary facts | output lines | exits | repeated output SHA-256 |
|---|---:|---:|---:|---:|---|---|
| small | 4 | 4 | 2 | 17 | 0/0 | `f074ccf4132fe32f18913243be1c8a6c8d00551b8c93717600df481ab995538b` |
| large | 16 | 16 | 8 | 60 | 0/0 | `f36e38c55c5f65fe6b38e3cde16abe12f8d12d6e3a9785d204a31316b406db39` |

The large probe repeats the same local call/subtree shape as the small probe.
The counts and byte-identical pairs support bounded local work and
determinism. This is not a timing or asymptotic scaling measurement.

## Checkpoint Ledger

| state | evidence | status |
|---|---|---|
| turn-start | Clean workspace at `43105867`; PA12 `142/166`, `24` failures, `166/166` covered; earlier PAs supplied passing. | recorded |
| implementation | Four-path intrinsic/constexpr boundary implemented in the three approved source owners; no tests, refs, fixtures, grammar, harness, scripts, generated artifacts, or new `.cpp`. | complete |
| focused active | Exact active command: build passed; active set `4/4`. | passed |
| neighboring controls | Exact control command: `13/13`, including valid and checked-invalid constant/case/sizeof/enum/conditional/nullptr/direct-call controls. | passed |
| narrow regressions | Additional non-constexpr const-initializer and ordinary zero-to-pointer controls: `3/3`. | passed |
| boundary probes | Out-of-tree zero/many-argument constant-p, one-argument abort, ordinary unknown call, and unknown operand all exited `1`. | passed |
| structural evidence | Small/large local-call probes had 4/16 source calls and 4/16 output literal facts; repeated output pairs were byte-identical. | recorded |
| broad PA12 | `make test-pa12` ran all `166/166`; exit `2` is expected for the 20 checked residuals; summary `146/166`. Baseline normalization: `24 -> 20`, supplied-only active four, fresh-only `0`. | passed checkpoint gate |
| through-PA11 | Exact `n=12` gate: `make test-report-through-pa11`; exit `0`, `685/685`. | passed |
| file audit | Exact `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src`; exit `0`, two pre-existing header warnings, no fatal issues. | passed |
| final scope | Final implementation paths are exactly `dev/src/pa11_semantic_model.h`, `dev/src/pa11_semantic_core.cpp`, `dev/src/pa12_semantic.cpp`, and `pa12/plan.md`; no test/ref/fixture/grammar/harness/script/generated path changed. | verified |
| staging/commit | One coherent worker-authored commit contains exactly the four approved paths; final clean-tree verification follows. | complete |

Historical context: earlier PA12 checkpoints established the shared typed
expression, conversion, statement, lookup, and deterministic dump foundation;
this row is the next isolated semantic-boundary increment rather than a reset
of that work.
