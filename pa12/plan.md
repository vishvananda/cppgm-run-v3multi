# PA12 typed local record-object/member checkpoint

## Stage Design

`PA11SemanticModel` remains the sole semantic owner: PA10 AST is reduced to
canonical `TypeId`, `NamedRecordId`, `ScopeId`, and `BindingId` facts, PA12
adds typed expression/statement children, and the cold renderer formats the
facts. This checkpoint handles ordinary local anonymous-union objects,
block-scope anonymous-union storage/injected members, and elaborated local
record copy-initialization. Storage, constructor-action, member selection,
value category, cv-qualified member type, and children are recorded once.

Rare relations are sparse typed sidecars keyed by `BindingId` and
`NamedRecordId`; ordinary `Binding` and `NamedRecord` objects remain at their
baseline size. Built-in dot/arrow access resolves from the operand's
canonical record type and class scope. Synthetic facts derive from generated
identity and typed IDs. Ordinary `const` record objects skip only the
integral constant-evaluation shortcut; integral/enum propagation and
constexpr behavior remain unchanged. Member functions, overloaded operators,
constructor overload selection, and all residual PA12 families remain out of
scope.

## Failure Map

Turn-start baseline at HEAD `bf67e445`: PA12 `146/166` passing, `20` failures,
and all `166/166` paths covered. The complete map and current status are:

Declaration/record/parser:

- `pa12/tests/general/200-local-anonymous-union-variable.t` — resolved.
- `pa12/tests/general/300-block-anonymous-union-injected-members.t` — resolved.
- `pa12/tests/general/300-elaborated-local-struct-copy-init.t` — resolved.
- `pa12/tests/general/300-local-extern-function-declaration.t` — residual; excluded PA10 parser/declarator boundary.

Member-pointer/cast/reference:

- `pa12/tests/general/300-decltype-functional-cast.t` — residual.
- `pa12/tests/general/300-member-function-pointer-return-pointer-const.t` — residual.
- `pa12/tests/general/300-member-function-pointer-type-alias-and-function.t` — residual.
- `pa12/tests/general/300-member-pointer-type-alias-and-function.t` — residual.
- `pa12/tests/general/300-multidimensional-array-const-reference-binding.t` — residual.
- `pa12/tests/general/300-reference-binding-pointee-const-pointer.t` — residual.
- `pa12/tests/general/300-scoped-enum-functional-cast-integral.t` — residual.
- `pa12/tests/general/300-static-cast-member-overload-prefers-nontemplate.t` — residual.
- `pa12/tests/general/300-static-cast-overloaded-function-template-argument.t` — residual.
- `pa12/tests/general/300-zero-arg-functional-cast-alias.t` — residual.

Namespace/lookup:

- `pa12/tests/general/300-namespace-function-body-later-anonymous-overload.t` — residual.
- `pa12/tests/general/300-qualified-direct-function-hides-using-directive.t` — residual.
- `pa12/tests/general/300-reopened-unnamed-namespace-call.t` — residual.
- `pa12/tests/general/300-unnamed-namespace-definition.t` — residual.
- `pa12/tests/general/300-unnamed-namespace-qualified-call.t` — residual.
- `pa12/tests/general/300-unnamed-namespace-unqualified-call.t` — residual.

The expected fresh residual set after the active checkpoint is exactly these
17 paths: the local-extern path, the ten member-pointer/cast/reference paths,
and the six namespace/lookup paths above.

## Active Checkpoint

Implementation scope is four existing source owners plus this plan:

- `dev/src/pa11_semantic_model.h`: removes rare metadata from every hot
  `Binding`/`NamedRecord`; adds compact `BindingId`/`NamedRecordId` sidecars.
- `dev/src/pa11_semantic_core.cpp`: owns sidecar insertion/lookup, canonical
  `add_value` and `inject_anonymous_union` declaration/scope formation,
  typed anonymous-union storage/member relations, visible elaborated-record
  reuse, direct block class-specifier processing, and the narrow const-record
  initializer guard.
- `dev/src/pa11_semantic.cpp`: cold PA12-mode record/storage/constructor
  display and binding rendering, accepted as the renderer owner.
- `dev/src/pa12_semantic.cpp`: PA12-only builtin/rollback helpers, synthetic
  constructor binding and `AnonymousUnionFact` access, dot/arrow member facts,
  injected-member roots, constructor actions, anonymous-union statement facts,
  cv-preserving member types, and PA12 analysis.

No tests, refs, fixtures, grammar, harness, or generated repository files were
changed. The semantic hot path performs direct typed sidecar/index lookups;
it does not scan arenas or recover relations from rendered names.

## Spec Alignment

- README contract: supports ordinary local anonymous-union declarations,
  injected members, local simple declarations, copy-initialization, and
  deterministic resolved expression output. Class-aware calls,
  member-function calls, overloaded operators, and constructor selection are
  not implemented.
- `spec.md` sections 1-2: preserves the PA10 AST -> PA11 canonical typed model
  -> PA12 fact -> cold renderer pipeline and canonical identity ownership.
- `spec.md` sections 3-4: dot/arrow access uses the operand record/class scope,
  rejects non-record/non-pointer, missing, and function members, and retains
  the canonical object cv on the selected member type.
- `spec.md` section 7: sparse sidecars and synthetic facts are bounded typed
  storage; no name rendering/reparsing or broad invalidation is introduced.

## Performance Evidence

Out-of-tree size probe, compiled against the current header and against the
HEAD `bf67e445` header, reports:

| type | current | HEAD |
|---|---:|---:|
| `sizeof(Binding)` | 80 | 80 |
| `sizeof(NamedRecord)` | 120 | 120 |

The sparse sidecar values are 24 bytes each (`BindingSidecar` and
`NamedRecordSidecar`) and are present only for participating rare identities.

Immutable amended-state output probe: `/tmp/pa12-sidecar-amended-structure.b6ldpj/cppgm++-immutable`,
mode `555`, size `1179784`, SHA-256
`ce5192f9c7264f6c415769240114019333f206df88e5aa9e03fae6c3d3447622`.
The source shapes and outputs are outside the repository. Both runs for each
shape exited `0` and were byte-identical:

| shape | output lines | member facts | constructor actions | run exits | repeated output SHA-256 |
|---|---:|---:|---:|---:|---|
| small | 26 | 2 | 1 | `0/0` | `043a755a11e74be2bec17bd2c3293ad1d918ebc6232990081d9984977331e0c7` |
| large | 103 | 23 | 1 | `0/0` | `bfcb9836a791539adbcb526632853be75d6d900303940fdeef576370325b0add` |

Arrow/cv probe on that immutable amended-state executable: valid `S*`/`const
S*` member access exited `0` and rendered
`member-expression lvalue int OP_ARROW:x` /
`member-expression lvalue const int OP_ARROW:x`; non-pointer operand, missing
member, and member-function access exited `1/1/1`, respectively. These are
structural/deterministic probes, not timing or asymptotic claims.

## Checkpoint Ledger

| checkpoint | exact evidence | status |
|---|---|---|
| Turn start | HEAD `bf67e445`; PA12 `146/166`, 20 failures, all 166 covered; through-PA11 and file audit supplied as passing. | recorded |
| Build | `make -C pa12 -j2` | passed |
| Active paths | `pa12/tests/general/200-local-anonymous-union-variable.t`, `pa12/tests/general/300-block-anonymous-union-injected-members.t`, `pa12/tests/general/300-elaborated-local-struct-copy-init.t` | `3/3` |
| PA12 controls | `pa12/tests/general/300-most-vexing-local-function-member-call-bad.t`, `pa12/tests/general/200-bad-noncallable-variable.t`, `pa12/tests/spec/100-simple-call.t`, `pa12/tests/spec/100-local-arith.t`, `pa12/tests/general/100-cast-to-void-expression.t`, `pa12/tests/general/200-constexpr-complete-object-cv.t`, `pa12/tests/general/300-pointer-plus-anonymous-enum.t`, `pa12/tests/spec/100-integral-conversions.t`, `pa12/tests/general/100-builtin-prefix-user-function-call.t` | `9/9` |
| PA11 control | `pa11/tests/spec/200-namespace-anonymous-union-injected-members.t` | `1/1` |
| Size/structure | Final and HEAD `Binding`/`NamedRecord` sizes equal; immutable small/large and arrow probes recorded above. | passed |
| Broad PA12 | `make test-pa12`: all `166/166` paths covered, exit `2`, `149/166` passed, and exactly the 17 residual paths listed above; no active or current-only failure. | passed |
| Through PA11 | `n=12; make test-report-through-pa$((n - 1))` | `685/685` passed |
| File audit | `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` passed with exactly two known warnings: `dev/src/cpp_semantic_core.h` and `dev/src/pa11_semantic_model.h`. | passed |
| Scope | `git diff --check` passed; changed-path audit found exactly `dev/src/pa11_semantic.cpp`, `dev/src/pa11_semantic_core.cpp`, `dev/src/pa11_semantic_model.h`, `dev/src/pa12_semantic.cpp`, and `pa12/plan.md`. | passed |
| Commit | `pa12: implement typed local record-object semantics` | complete/current |

## Next Checkpoint

This checkpoint is committed and clean. The next bounded residual family
requires separate authorization; do not expand into local-extern,
member-pointer/cast/reference, or namespace/lookup families.
