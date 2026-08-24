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
constexpr behavior remain unchanged. The bounded repair publishes an
unqualified named-record type for ordinary cv-qualified anonymous record
objects while retaining typedef alias qualification. Member functions,
overloaded operators, constructor overload selection, and all residual PA12
families remain out of scope.

## Failure Map

Parent checkpoint `bf67e445`: PA12 `146/166` passing, `20` failures, and all
`166/166` paths covered. The fresh final `make test-pa12` result after the
bounded repair is exit `2`, `149/166` passing, exactly `17` failures, and all
`166/166` paths covered. Its normalized failure set is identical to the
supplied post-landed `e45d0795` set: `0` current-only and `0` supplied-only
paths. The complete residual map is:

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

The exact supplied residual set is these 17 paths: the local-extern path, the
ten member-pointer/cast/reference paths, and the six namespace/lookup paths
above. The bounded cv repair does not touch those families.

## Active Checkpoint

The landed implementation scope is four existing source owners plus these
records; the final bounded repair is in `pa11_semantic_core.cpp`:

- `dev/src/pa11_semantic_model.h`: removes rare metadata from every hot
  `Binding`/`NamedRecord`; adds compact `BindingId`/`NamedRecordId` sidecars.
- `dev/src/pa11_semantic_core.cpp`: owns sidecar insertion/lookup, canonical
  `add_value` and `inject_anonymous_union` declaration/scope formation,
  typed anonymous-union storage/member relations, visible elaborated-record
  reuse, direct block class-specifier processing, the narrow const-record
  initializer guard, and canonical unqualified naming for cv-qualified
  anonymous record objects.
- `dev/src/pa11_semantic.cpp`: cold PA12-mode record/storage/constructor
  display and binding rendering, accepted as the renderer owner.
- `dev/src/pa12_semantic.cpp`: PA12-only builtin/rollback helpers, synthetic
  constructor binding and `AnonymousUnionFact` access, dot/arrow member facts,
  injected-member roots, constructor actions, anonymous-union statement facts,
  cv-preserving member types, and PA12 analysis.

No tests, refs, fixtures, grammar, harness, or generated repository files were
changed. The semantic hot path performs direct typed sidecar/index lookups; it
does not scan arenas or recover relations from rendered names. The final
checkpoint commit contains only the bounded source repair and these two
records, and leaves the repository clean.

## Spec Alignment

- README contract: supports ordinary local anonymous-union declarations,
  injected members, local simple declarations, copy-initialization, and
  deterministic resolved expression output. Class-aware calls,
  member-function calls, overloaded operators, and constructor selection are
  not implemented.
- `spec.md` sections 1-2: preserves the PA10 AST -> PA11 canonical typed model
  -> PA12 fact -> cold renderer pipeline and canonical identity ownership.
- `spec.md` sections 3-4: dot/arrow access uses the operand record/class scope,
  rejects non-record/non-pointer, missing, and function members, retains the
  canonical object cv on the selected member type, and uses one-pass
  declaration formation without whole-program retry or broad invalidation.
- `spec.md` section 7: sparse sidecars and synthetic facts are bounded typed
  storage; no name rendering/reparsing, duplicate owner, or unsupported
  performance claim is introduced.

## Performance Evidence

The retained out-of-tree size probe, compiled against the current header and
against the parent `bf67e445` header, reports:

| type | repaired/current | parent bf67e445 |
|---|---:|---:|
| `sizeof(Binding)` | 80 | 80 |
| `sizeof(NamedRecord)` | 120 | 120 |

The sparse sidecar values are 24 bytes each (`BindingSidecar` and
`NamedRecordSidecar`) and are present only for participating rare identities.

The current immutable executable `/tmp/pa12-record-checkpoint.PJtyG8/cppgm++-immutable`
was mode `555`. Five interleaved rounds on three checked-in representative
shapes all exited `0` on repeated runs and produced one output hash per shape:

| shape | output lines | member facts | constructor actions | run exits | repeated output SHA-256 |
|---|---:|---:|---:|---:|---|
| local anonymous-union object | 27 | 2 | 1 | `0/0/0/0/0` | `1cae118288362f66d28a2f4862c14a5175630289669d9e67492bb7fe2b01b639` |
| block anonymous-union storage | 26 | 2 | 1 | `0/0/0/0/0` | `b9801b8e58907189f3562058b2186343513bc2468629848a9ac6ea37e687794b` |
| elaborated const record | 17 | 1 | 0 | `0/0/0/0/0` | `d5e2807e90812a10fba4e675e93de4f4ee8f291c510c42273ca1aebb24ddaae1` |

Median timed wall/user/system values were `0.00/0.00/0.00` seconds and peak
RSS was `4360/4376/4384 KiB` for the three shapes. The timer resolution and
tiny inputs make these structural/determinism evidence only; they are not a
timing, asymptotic, memory-scaling, or full-suite performance claim. Separate
valid `S*`/`const S*` arrow probes rendered `int`/`const int` member types, and
non-pointer, missing-member, and member-function operands exited `1/1/1`.

## Checkpoint Ledger

| checkpoint | exact evidence | status |
|---|---|---|
| Parent/supplied baseline | Parent `bf67e445`: `146/166`, 20 failures; supplied landed `e45d0795`: `149/166`, 17 failures, all 166 covered; earlier through-PA11 and file-audit results are retained evidence. | recorded |
| Build | `make -C pa12 -j2` after the cv repair | passed |
| Focused checked-in paths | Three active paths plus nine PA12 controls | `12/12` |
| PA11 control | `pa11/tests/spec/200-namespace-anonymous-union-injected-members.t` | `1/1` |
| Bounded repair/probes | cv-qualified anonymous records now succeed; invalid dot/arrow/member cases reject; integral constexpr control remains successful. | passed |
| Structure/determinism | Immutable five-round representative probe and size evidence above; one hash per shape, direct typed sidecar/index path. | passed |
| Broad PA12 gate | `make test-pa12`: exit `2`, `149/166` passing, all `166/166` covered; exact supplied 17-path residual set, `0` current-only / `0` supplied-only. | accepted residual baseline |
| Through-PA11 gate | Exact required command: exit `0`, `685/685`. | passed |
| File audit | Exact required command: exit `0`; two known `[warning][bad-division]` findings for `dev/src/cpp_semantic_core.h` and `dev/src/pa11_semantic_model.h`, no fatal issues. | passed with known warnings |
| Scope/commit | `git diff --check`; exact changed paths are `dev/src/pa11_semantic_core.cpp`, `pa12/audit.md`, and `pa12/plan.md`; one final commit contains only those paths and the tree is clean. | final/clean |

## Next Checkpoint

This checkpoint is complete, committed, and clean. The exact 17 residual paths
remain for a separately authorized checkpoint; do not expand this repair into
the local-extern, member-pointer/cast/reference, or namespace/lookup families.
