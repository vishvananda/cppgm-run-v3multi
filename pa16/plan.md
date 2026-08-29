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

For the canonical-truth boundary, PA12 owns a typed
`CanonicalTruthPolicy` on each recorded conversion whose source is the
semantic bool produced by a comparison or logical expression. `Preserve` is
assigned only when that conversion's owning `SemanticFact` has the existing
typed direct-boundary provenance: a member-derived value, including a value
returned through a selected function's typed return-result summary and carried
through a variable binding, or an implicit `this` operand. The function
summary is OR-ed only from returned expression/`ReturnStatement` facts, never
from the enclosing body. Plain procedural facts retain the default
`Materialize` policy. PA15 carries that policy in `LoweredValue` and consumes
it once while emitting the recorded conversion. The comparison destination
remains the PA13 canonical physical `i64` truth; this policy only chooses the
semantic-bool bridge for the checked PA16 value shape. N3485 4.5 and 4.7 still
govern the semantic `bool` and its integral promotion; no source spelling,
translation-unit flag, or block scan participates.

## Failure map

Turn-start authority is clean HEAD e92b1e184a585eedb0190de527b512d0b9df9f3e:
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

The selected residual cluster is the five exact LowIR-shape failures for the
global aggregate initializer, defaulted/deleted aggregate constructors,
qualified friend member access, and the member unary-address fallback. The
same extra truth bridge appears in the pragma-pack, value-initialization, and
bit-field neighbors, but each retains an independent mismatch.

## Active checkpoint

The bounded source audit covers the complete typed path in the allowed PA11,
PA12, and PA15 files. The repair is:

- PA12 records `CanonicalTruthPolicy::Preserve` at the conversion owner when
  its `SemanticFact` is a direct typed boundary for member/object-derived
  truth. Child facts propagate `contains_member_value`; each
  `ReturnStatement` updates the selected `FunctionFact`'s typed
  `return_result_contains_member_value` summary, and a call consumes that
  summary. An initialized variable binding preserves the resulting provenance
  for later ID expressions, while an implicit-`this` `BindingId` marks the
  member-function comparison. Definitions are still analyzed in deterministic
  source order; the focused global case deliberately defines `read()` before
  its call. PA15 propagates the policy through `LoweredValue`, so the
  canonical truth bridge is selected from typed conversion data, not from
  rendered spelling, a test identity, a translation-unit proxy, or a block
  rescan. The PA13 comparison destination remains physical `i64`.
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

The current focused protected identities are:

- pa16/tests/general/100-global-aggregate-nested-array-initializer.t
- pa16/tests/general/200-defaulted-constructor-still-aggregate.t
- pa16/tests/general/200-deleted-constructor-still-aggregate.t
- pa16/tests/general/200-qualified-friend-function-member-access.t
- pa16/tests/general/300-unary-address-of-builtin-fallback.t

The earlier special-member five-test focus remains part of the preceding
checkpoint record below.

## Focused evidence

The bounded build and focused controls pass:

- make -C dev -j2 cppgm++ exits 0.
- The PA12 class-result regression
  `tests/general/300-elaborated-local-struct-copy-init.t` passes 1/1, as does
  the most-vexing-function negative control.
- The exact focused `pa16 check` invocation selected and ran the five protected
  identities (the runner printed all five `Running ...` lines), rewrote their
  `.check` artifacts at 16:21:22 UTC, and passed 5/5. These are `.check`
  outputs; the `.my` files are not the local `check` target's artifacts.
- A temporary member-side-effect/plain-return probe compiled with status 0;
  its caller's procedural comparison retained `convert trunc u8 i64`, proving
  an unrelated member access in a function body does not taint the returned
  value. A second temporary pair, one with an unused `struct Unrelated` and
  one without it, compiled with status 0 and compared byte-identically with
  `cmp` status 0; both retained the ordinary `trunc u8 i64` bridge.
- A temporary diagnostic pass recorded `Preserve` for the global aggregate's
  `read() -> value.signature -> observed` return conversion, the two aggregate
  member comparisons, the qualified friend member comparison, and the
  implicit-`this` bool return; the latter remained `return u8` without an
  identity conversion. The diagnostic source instrumentation was removed
  before the final build.
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
published path. The truth disposition is one enum write/read per existing
conversion, child provenance uses already-published semantic edges, each
return contributes one boolean OR to its owning `FunctionFact`, and a call
uses one indexed function-fact lookup. Binding provenance is an O(1) indexed
sidecar lookup. No block rescan or second conversion model is introduced. No
timing, RSS, allocation, or speedup claim is made.

## Validation and next checkpoint

The final `make -C dev -j2 cppgm++` exits 0; its complete log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-final-build-20260829.log`.
The final broad `make test-pa16` exits 2 with 199/243 passed and 44 failed;
its complete external log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-final-20260829.log`.
All 243 checked-in identities are covered: 199 passed plus 44 current
failures. The exact sorted current failure set is the complete 49-entry
turn-start map above minus these five removed identities:

- pa16/tests/general/100-global-aggregate-nested-array-initializer.t
- pa16/tests/general/200-defaulted-constructor-still-aggregate.t
- pa16/tests/general/200-deleted-constructor-still-aggregate.t
- pa16/tests/general/200-qualified-friend-function-member-access.t
- pa16/tests/general/300-unary-address-of-builtin-fallback.t

Set comparison against both `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
and this preserved map found no new current failure and no baseline-passing
regression; the current set is a strict subset. The exact prior gate
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
exits 0 with 1167/1167; its complete log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/through-pa15-final-rerun-20260829.log`.
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits 0
with five pre-existing warnings; its log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-file-audit-final-rerun-20260829.log`.
The focused five-test and PA15 course-404 controls remain passing, and the
temporary member-side-effect/plain-return and unused-class probes retain
procedural materialization. Structural evidence remains one typed enum
read/write per existing conversion, one boolean OR per returned expression,
and one indexed function-fact lookup per call; no timing claim is made.
Remaining uncertainty is the conservative OR-summary for functions with
multiple returns and the unrelated residual 44-test failure set.

| checkpoint | status |
| --- | --- |
| c507120c..e9d92812 plus bounded typed-path audit | final audit preserves 194/243, the exact 49-failure map, and 243/243 coverage; through-PA15 is 1167/1167, file audit passes with five warnings, and the single coherent checkpoint commit leaves a clean tree |
| PA16 typed canonical-truth conversion boundary (return-summary revision, validated) | Per-expression `contains_member_value`/implicit-`this` provenance plus the precise `FunctionFact` return-result summary assigns PA12 `CanonicalTruthPolicy` and carries it through PA15 `LoweredValue`; final PA16 is 199/243 with the exact baseline 49-failure set reduced by the five listed identities, 243/243 covered, no baseline-passing regression, prior-through is 1167/1167, and file audit passes with five warnings; focused controls and negative probes retain procedural materialization |
