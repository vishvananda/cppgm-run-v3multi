# PA16 implementation plan

## Stage Design

PA11 forms the first typed owner fact: a qualified special-member declarator is
resolved through its `NamePath` and `declaration_scope` to the canonical class
`ScopeId`, `NamedRecordId`, and (for destructors) `BindingId`. PA12 consumes
that owner and publishes one `DestructorCall` fact whose only operand is the
already-semantically-evaluated object expression. PA15 consumes the selected
binding/type facts directly: class calls lower to the canonical destructor
with its hidden object pointer, while scalar pseudo-destructor calls lower to
the operand evaluation with no callee.

Invariants are: owner scope is a class and the terminal typed name matches its
record; an out-of-class destructor definition reuses an existing declaration
binding; destructor names are cv-insensitive at the class-type boundary;
scalar pseudo-destructors require the same scalar `TypeId` as the object; and
the object expression has exactly one PA12 child/evaluation root. All malformed
owner, binding, signature, access, and lowering facts fail closed. The path
lookup is canonical and bounded; there is no spelling-key recovery, secondary
semantic model, whole-TU retry, or broad rescan.

## Failure Map

The authoritative PA16 turn-start baseline is HEAD `c507120c`: `189/243`
identities passed, `54` failed, and `243/243` identities were covered. The
complete 54-failure set is the existing full map below with the two already
fixed entries (`100-global-reference-incomplete-referent.t` and
`200-extern-class-object-declaration.t`) omitted:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/100-global-aggregate-nested-array-initializer.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-defaulted-constructor-still-aggregate.t`
- `pa16/tests/general/200-deleted-constructor-still-aggregate.t`
- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-member-object-lifetime.t`
- `pa16/tests/general/200-mutable-member-const-method.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-nested-out-of-class-constructor-enclosing-type.t`
- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-aggregate-brace.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/200-pointer-subscript-class-reference-return.t`
- `pa16/tests/general/200-qualified-friend-function-member-access.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t`
- `pa16/tests/general/300-const-pointer-explicit-destructor-call.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-explicit-destructor-call-enclosing-namespace-type.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t`
- `pa16/tests/general/300-mixed-member-free-shift-stress-chain.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-operator-shift-stress-chain.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-packed-class-layout.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-scalar-pseudo-destructor-call.t`
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-unary-address-of-builtin-fallback.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-constructor-member-init.t`
- `pa16/tests/general/400-bit-field-member-access-bad.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Exact owned subset for this checkpoint:

- `pa16/tests/general/200-nested-out-of-class-constructor-enclosing-type.t`
- `pa16/tests/general/300-explicit-destructor-call-enclosing-namespace-type.t`
- `pa16/tests/general/300-const-pointer-explicit-destructor-call.t`
- `pa16/tests/general/300-scalar-pseudo-destructor-call.t`

Final residual comparison: `make test-pa16` is `194/243` with `49` failures
and `243/243` identities covered. Relative to the 54-failure baseline, the
five removed identities are the four owned tests plus
`pa16/tests/general/200-pointer-subscript-class-reference-return.t`; the
final-only set is empty. The remaining residual map is therefore the listed
baseline map minus those five entries.

## Active Checkpoint

Scope is typed qualified out-of-class constructor ownership plus typed class
and scalar explicit destructor calls. The retained source files each carry
one part of that path:

- `dev/src/pa11_semantic_model.h`: `DestructorCall` and its typed API boundary;
- `dev/src/pa11_semantic_core.cpp`, `dev/src/pa11_semantic_types.cpp`:
  destructor `NamePath` components and scoped typed-name handling;
- `dev/src/pa11_semantic.cpp`: PA12 dump publication for the typed destructor
  fact and out-of-class special-member body;
- `dev/src/pa12_semantic.cpp`, `dev/src/pa12_semantic_construction.cpp`:
  top-level special-member preparation/analysis, qualified owner resolution,
  and the existing constructor selection boundary needed by the owned nested
  definition;
- `dev/src/pa12_semantic_calls.cpp`, `dev/src/pa12_semantic_selection.h`:
  the bounded class-value constructor conversion and its typed conversion
  kind;
- `dev/src/pa12_semantic_member.cpp`, `dev/src/pa12_semantic_facts.cpp`:
  one typed destructor-expression fact path and typed complete/base entry
  identities;
- `dev/src/pa15_lowering.h`, `dev/src/pa15_lowering.cpp`,
  `dev/src/pa15_lowering_calls.cpp`, `dev/src/pa15_lowering_construction.cpp`,
  `dev/src/pa15_lowering_flow.cpp`: typed destructor demand, strong ABI entry
  metadata, class-value argument materialization, direct/pseudo lowering, and
  expression dispatch.

The implementation reuses canonical `lookup_type_path`, `ScopeId`,
`NamedRecordId`, `BindingId`, and PA15 destructor emission/signature checks. It
does not add virtual dispatch, templates, placement new, unrelated cleanup,
general member overload work, or any test/ref/harness/comparator/source-set
change. Temporary verbose diagnostics in `pa12_semantic_calls.cpp` were
removed; its remaining changes are limited to the bounded constructor value
conversion.

The focused owned set is now `4/4`. The bounded controls also pass for a
lexically shadowed wrong destructor type (the injected class name is selected),
an inaccessible class destructor (rejected), a mismatched scalar
pseudo-destructor (rejected), and a derived-object qualified base destructor
(the base binding is called). The constructor value slice is limited to one
lvalue class argument for an out-of-class constructor; other class-by-value
shapes fail closed.

## Performance Evidence

Typed name resolution is O(L) in qualified path length with indexed scope/type
lookups; the unqualified destructor rule performs only the two required typed
candidates (lexical scope and object class scope). The destructor fact adds
O(1) work after lookup. Demand and lowering follow the one object child and
existing reachable function edges, so the added work is bounded by those edges
and does not scan the translation unit. Representative `/tmp` probes cover
shadowed lookup, access, scalar mismatch, and derived-base projection; no
timing, RSS, or speedup claim is made.

## Validation and Checkpoint Ledger

Broad validation and the repository commit are complete:

- `make -C pa16 dev-shared-target`: pass.
- Focused `make -C pa16 check TEST='tests/general/200-nested-out-of-class-constructor-enclosing-type.t tests/general/300-explicit-destructor-call-enclosing-namespace-type.t tests/general/300-const-pointer-explicit-destructor-call.t tests/general/300-scalar-pseudo-destructor-call.t'`:
  `4/4` passed; the extended directly relevant set was `7/7`.
- Direct controls: `100-out-of-class-methods.t`,
  `spec/200-nested-class-enclosing-access.t`, and
  `300-member-operator-bang-out-of-class.t` passed. The four bounded `/tmp`
  controls passed with expected accept/reject statuses.
- The four requested identities were exercised; the turn-start coverage
  authority remains `243/243` and no coverage rule or test identity changed.
- `make test-pa16`: `194/243` passed, `49` failed, `243/243` covered, with no
  final-only failure identity.
- Exact `n=16` through-PA15 command: `1167/1167` passed.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: passed with
  five pre-existing header-division warnings.
- `git diff --check`: passed. The coherent checkpoint was committed; the final
  commit id is reported in the repository handoff.

### Historical Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 passed. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered. |
| `d7ed98aa` typed builtin boundary | `167/243` passing, `76` failures, `243/243` covered. |
| `3b7d8e6a` qualified-type checkpoint | `173/243` passing, `70` failures, `243/243` covered. |
| `30d69fc3` landed inheriting-constructor checkpoint | `176/243` passing, `67` failures, `243/243` covered. |
| `PA16 typed access-control checkpoint` | `179/243` passing, `64` failures, `243/243` covered. |
| `PA16 typed non-automatic lifetime checkpoint` | `184/243` passing, `59` failures, `243/243` covered; focused matrix `9/12`. |
| `PA16 typed ordinary-value-over-tag lookup checkpoint` | `187/243` passing, `56` failures, `243/243` covered; its focused matrix was `8/8`. |
| `b3bbf052` typed non-owning namespace object checkpointAudit | Completed the bounded ownership-path audit and repair: incomplete named-class references/glvalues retain pointer representation while owned values remain layout-gated; namespace declaration-only globals are rooted in emitted typed facts; owner/range checks fail closed; and unused member/default facts no longer create storage roots. Post-repair `make test-pa16` is `189/243` passing, `54` failures, and `243/243` covered; comparison with the landed 54-failure authority has baseline-only `0` and final-only `0`. The focused handout matrix is `16/16`, the exact through-PA15 command is `1167/1167`, file audit exits `0` with five known header-division warnings, repeated structural probes are byte-identical, and `git diff --check` passes. The incomplete namespace-object address case is out-of-contract under the PA16 complete-class namespace-object scope; course-400 DMI remains outside this increment. |
| `PA16 typed qualified special-member/destructor checkpoint` | Focused owned set `4/4`, extended focused set `7/7`; final `make test-pa16` `194/243` with `49` failures and `243/243` covered; five baseline-only failures removed and no final-only failures; through-PA15 `1167/1167`; file audit passed with five pre-existing warnings; `git diff --check` passed; committed as the coherent checkpoint. |
