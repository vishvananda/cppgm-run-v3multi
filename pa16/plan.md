# PA16 implementation plan

## Stage design

PA11 owns the canonical typed identity. A qualified special-member
NamePath/declaration_scope resolves to the class ScopeId and NamedRecordId;
the destructor declaration, qualified definition, and base entry retain one
canonical BindingId and typed function signature. PA12 performs the
destructor lookup and access check, evaluates the operand once, and publishes
one DestructorCall fact with exactly one child. PA15 consumes that fact,
callable signature, selected binding, and semantic base-subobject path
directly. It emits a hidden-object destructor call for a class or evaluates a
scalar pseudo-destructor operand with no callee.

Every speculative PA12 semantic transaction checkpoints and rolls back the
semantic base-path arena together with its facts, children, conversions, and
names. A retained fact therefore owns the only path tail that PA15 may
consume.

For an explicit destructor expression, PA12 is responsible for the
cv-insensitive class lookup, scalar exact-TypeId check, access, and
derived-to-base projection. The selected base-record path is stored in the
typed fact arena. PA15 validates the published path and complete layout
metadata only to form zero-offset projections; it does not redo lookup or
recompute the relation. Invalid owner, range, binding, signature, access,
layout, and index facts fail closed.

The README puts class pass-by-value and out-of-class constructor definitions
outside the general PA16 boundary. The checked-in nested constructor fixture
requires one narrow compatibility extension: one lvalue source whose exact
class type is an empty, complete, non-union, non-virtual, base-free,
member-free, one-byte record with no requested alignment, default-member
initializer, user-declared constructor, or user-declared destructor, passed by
value to a direct out-of-class constructor definition. The declaration-only
constructor surface is retained only so that fixture-required definition can
be paired with its canonical declaration. This does not make general
class-value semantics in scope. PA11 and PA12 preserve prior typed semantics
for ordinary non-constructor class-valued declarations, definitions, results,
and calls; they do not publish the narrow `ClassValue` conversion for those
ordinary calls.
PA15 rejects unsupported class-value result/parameter ABI when a function,
declaration, or call is demanded or lowered, using canonical typed facts and
failing closed. The exception is only the exact constructor shape above:
nonvariadic, one empty-class value parameter, with the declaration-only point
or normal out-of-class definition admitted by the semantic owner and the
canonical declaration reused. In-class, defaulted, deleted, and other
definition forms reject before body analysis, while wider nonempty/member/
base/union/virtual/incomplete/stateful-lifetime/non-lvalue/additional-
argument/copy/move/return forms never receive the exception.

The implementation has one typed production pipeline. It uses dense indexed
arenas and reachable worklists, with no spelling downgrade, parallel
resolver/model, broad retry, or test-specific hardcoding.

## Failure map

Turn-start authority is clean HEAD e9d928125399aaad099b48d59e977e80771007af:
194/243 identities passed, 49 failed, and 243/243 identities were covered.
The exact sorted turn-start failure map is:

- pa16/tests/general/100-function-pointer-nested-param-name-shadow.t
- pa16/tests/general/100-global-aggregate-nested-array-initializer.t
- pa16/tests/general/200-aliased-base-mem-initializer-match.t
- pa16/tests/general/200-const-subobject-member-call.t
- pa16/tests/general/200-defaulted-constructor-still-aggregate.t
- pa16/tests/general/200-deleted-constructor-still-aggregate.t
- pa16/tests/general/200-destructor-body-local-before-base-destruction.t
- pa16/tests/general/200-elaborated-member-forward-type.t
- pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
- pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t
- pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
- pa16/tests/general/200-local-default-class-array-lifecycle.t
- pa16/tests/general/200-member-object-lifetime.t
- pa16/tests/general/200-mutable-member-const-method.t
- pa16/tests/general/200-nested-braced-member-aggregate-init.t
- pa16/tests/general/200-nonliteral-field-condition-not-folded.t
- pa16/tests/general/200-placement-new-expression-aggregate-brace.t
- pa16/tests/general/200-placement-new-expression-constructor-call.t
- pa16/tests/general/200-qualified-friend-function-member-access.t
- pa16/tests/general/200-reference-indexed-pointer-member-access.t
- pa16/tests/general/200-reference-member-class-init.t
- pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
- pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
- pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t
- pa16/tests/general/300-adl-using-declaration-source-point.t
- pa16/tests/general/300-callable-field-hides-private-base-method.t
- pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t
- pa16/tests/general/300-enum-class-nonmember-operator-bitand.t
- pa16/tests/general/300-friend-function-definition-skip.t
- pa16/tests/general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t
- pa16/tests/general/300-mixed-member-free-shift-stress-chain.t
- pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
- pa16/tests/general/300-operator-nullptr-t-from-zero.t
- pa16/tests/general/300-operator-shift-stress-chain.t
- pa16/tests/general/300-overloaded-deref-user-assignment.t
- pa16/tests/general/300-packed-class-layout.t
- pa16/tests/general/300-pragma-pack-followed-by-endif.t
- pa16/tests/general/300-prvalue-derived-base-friend-operator.t
- pa16/tests/general/300-synthesized-array-member-lifecycle.t
- pa16/tests/general/300-unary-address-of-builtin-fallback.t
- pa16/tests/general/300-user-defined-string-literal-operator.t
- pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
- pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t
- pa16/tests/general/400-bit-field-constructor-member-init.t
- pa16/tests/general/400-bit-field-member-access-bad.t
- pa16/tests/general/400-bit-field-prefix-postfix-increment.t
- pa16/tests/general/400-bitfield-aggregate-init.t
- pa16/tests/general/400-signed-bit-field-read.t
- pa16/tests/general/400-signed-enum-bit-field-read.t

The c507120c parent authority was 189/243 with 54 failures. The landed
c507120c..e9d92812 span removed these five identities from that map:
200-nested-out-of-class-constructor-enclosing-type.t,
200-pointer-subscript-class-reference-return.t,
300-const-pointer-explicit-destructor-call.t,
300-explicit-destructor-call-enclosing-namespace-type.t, and
300-scalar-pseudo-destructor-call.t. The current worker audit preserves the
e9d92812 map exactly: final-only and baseline-only identity sets are empty,
and coverage remains 243/243.

## Active checkpoint

The bounded source audit covers the complete typed path in the allowed PA11,
PA12, and PA15 files. The repair is:

- PA12 now stores the selected member/destructor derived-to-base
  NamedRecordId path in a semantic arena. The path is recomputed, if needed,
  only while semantic selection chooses the binding; the final selected
  member path is what is published. PA15 demand checks the indexed range and
  lowering consumes the path and layout offsets without
  member_object_convertible or name lookup.
- The class-value compatibility predicate is centralized on canonical
  NamedRecord, RecordLayout, NamedRecordSidecar, and constructor FunctionFact
  identity. A class-valued constructor signature is accepted only when the
  complete signature is nonvariadic with exactly one empty-class value
  parameter. The semantic owner additionally admits it only for the precise
  normal declaration-only pairing point or the normal out-of-class definition;
  in-class, defaulted, deleted, and other definition locations reject before
  body analysis. Ordinary class-valued declarations, definitions, results, and
  typed non-constructor calls remain available to PA12's earlier semantic dump, without a
  `ClassValue` conversion. PA15 instead proves the whole canonical signature
  at function/call demand and rejects unsupported ABI before lowering or any
  per-parameter store suppression. The proof requires the exact pre-existing
  class-owned BindingId/signature, a defined canonical binding, raw void
  result, and clean constructor-entry metadata; materialization reuses it.
- Qualified destructor names retain PA11 semantic components and resolve
  through canonical class scope/type identity. PA12 creates one typed
  DestructorCall child for both class and scalar forms; PA15 preserves
  exactly-once operand evaluation.

Focused protected identities are:

- pa16/tests/general/200-nested-out-of-class-constructor-enclosing-type.t
- pa16/tests/general/300-explicit-destructor-call-enclosing-namespace-type.t
- pa16/tests/general/300-const-pointer-explicit-destructor-call.t
- pa16/tests/general/300-scalar-pseudo-destructor-call.t
- pa16/tests/general/200-pointer-subscript-class-reference-return.t

## Focused evidence

The bounded build and focused controls pass:

- make -C dev -j2 cppgm++ exits 0.
- The PA12 class-result regression
  `tests/general/300-elaborated-local-struct-copy-init.t` passes 1/1, as does
  the most-vexing-function negative control.
- The five protected handout identities pass individually, 5/5.
- Course controls 401, 402, 403, 405, 408, 409, and 418 pass.
- Temporary negative boundary probes reject at status 1 for wrong lexical
  destructor type, inaccessible destructor, scalar mismatch, nonempty class
  value, ordinary free class-value call, class-value return, two empty
  class-value constructor parameters, class-value plus scalar constructor
  parameters, and a mismatched out-of-class constructor definition. The
  no-call in-class constructor probe also rejects at the semantic owner, as
  does the required unused-body form.
- A qualified derived-object Base destructor probe succeeds with one
  Base destructor call. A next()->~X() probe emits one next call and one
  X destructor call.
- A temporary `__builtin_constant_p(p->get())` probe enters the existing
  type-only operand guard, semantically publishes an inherited Base::get
  path, and discards that operand fact/path before a retained `p->get()`.
  The accepted construct's `Derived::run` LowIR has exactly one
  `projection=base_subobject` and one `call i32 @Base__get`, with no lowered
  builtin call; repeated runs are byte-identical with SHA-256
  `9af1ec7acae62eefa838b112e3d77c04985d63f98991a581bd7c16a20f51e068`.
- The nested constructor LowIR has an object-valued 1x1 parameter, stores
  only hidden this in the constructor entry, and emits one constructor call
  with the typed object slot.

## Performance and structural bounds

Qualified-name work is bounded by the path length and indexed scope/type
lookups. A selected base path is bounded by the direct single-inheritance
depth and is published once per selected fact; the type-only probe exercises
SemanticTailGuard rollback from one saved path tail. PA15 demand visits each
semantic fact once per reachability mode, and lowering walks only the
published path. The whole-signature class-value guard is indexed
record/layout/sidecar work plus one parameter-list pass. No timing, RSS,
allocation, or speedup claim is made.

## Validation and next checkpoint

The authorized broad gate log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-c507120c-e9d92812-checkpoint-final-authorized-20260829.log`.
The gate remains incomplete at exactly the e9d92812 result:
`make test-pa16` exits 2 with 194/243 passed and 49 failures. Exact sorted
failure comparison against `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
and the 49-entry map above has empty final-only and baseline-only sets; all
243/243 identities remain covered. The exact n=16 through-PA15 command exits
0 with `1167 / 1167`. The required file audit exits 0 with five known
`bad-division` warnings. `git diff --check` and the post-commit diff check
are clean, and the checkpoint is committed in one coherent audit commit with
an empty worktree. PA16 is still incomplete; the next checkpoint owns the
unchanged 49-identity residual without widening this typed boundary.

| checkpoint | status |
| --- | --- |
| c507120c..e9d92812 plus bounded typed-path audit | final audit preserves 194/243, the exact 49-failure map, and 243/243 coverage; through-PA15 is 1167/1167, file audit passes with five warnings, and the single coherent checkpoint commit leaves a clean tree |
