# PA16 implementation plan

## Stage Design

This checkpoint closes the typed member-function-definition declarator
context. PA10 already preserves the declarator shape, including
`TrailingReturnType`, qualifiers, and qualified member names. PA11 now carries
`auto` as a declaration-specifier fact and lowers a trailing return to a typed
`TypeId` declarator operation. The suffix walk consumes the trailing `TypeId`
before constructing the function type, so return-type lookup is performed by
the existing typed scope indexes rather than by rendered text.

For an ordinary member definition, the qualified declarator resolves one
canonical class owner; the same owner is used for the leading/trailing return
type, parameter declarations, function scope, and body lookup. For a special
member, a qualified constructor resolves its class owner from the declarator
path; a qualified destructor resolves the complete class type path. The
special-member fact is then stored with the owning class scope and canonical
record identity. PA12 prepares and analyzes namespace-scope special-member
definitions through that typed fact, and PA15 continues to consume the
existing `FunctionFact`/binding identities.

This follows C++11 declarator and member-definition rules in N3485 [dcl.fct]
(including the trailing-return grammar), [dcl.fct.def], and [class.mfct], and
the typed-fact requirements in `spec.md` sections 2, 5, 6, and 7. It does not
add class-by-value transfer, aggregate/lifetime behavior, parser recovery, or
unrelated LowIR presentation repairs.

## Failure Map

The authoritative turn-start baseline is `131/243` passed, `112` failed, and
`243/243` identities covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` and
the corresponding failure-gate log. The exact owner-context cluster for this
checkpoint is only:

- `pa16/tests/general/300-member-function-trailing-return.t` — `auto`,
  trailing return, in-class member, `const noexcept`.
- `pa16/tests/general/300-out-of-class-member-trailing-return.t` — qualified
  out-of-class member, `auto`, trailing return, member typedef lookup,
  `noexcept`.
- `pa16/tests/general/200-nested-out-of-class-constructor-enclosing-type.t` —
  qualified nested special-member owner and enclosing-class parameter lookup.

The final PA16 gate is `132/243` passed, `111` failed, and `243/243` covered;
its durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-final-v2.log`.
Compared with the baseline, the baseline-only set is exactly
`general/300-member-function-trailing-return.t` and the final-only set is
empty.

The remaining 109 baseline failures are explicitly excluded. The following is
the exhaustive identity map, grouped by baseline first diagnostic; the two
`declaration has no PA11 type` entries are the two in-scope identities above,
and the one marked special-member owner entry is the third in-scope identity.

- `declaration has no PA11 type` (2):
  `general/300-member-function-trailing-return.t`,
  `general/300-out-of-class-member-trailing-return.t`.
- `PA11 special member has no class owner` (2):
  `general/200-nested-out-of-class-constructor-enclosing-type.t`,
  `general/300-explicit-destructor-call-enclosing-namespace-type.t`.
- `PA11 name has no semantic component` (2):
  `general/300-const-pointer-explicit-destructor-call.t`,
  `general/300-scalar-pseudo-destructor-call.t`.
- `PA12 aggregate initializer has too many elements` (1):
  `spec/200-aggregate-brace-elision.t`.
- `PA12 ambiguous function call` (2):
  `general/300-using-base-static-same-signature-derived-preferred.t`,
  `spec/200-derived-base-reference-overload-rank.t`.
- `PA12 constructor is inaccessible` (1):
  `general/200-friend-derived-private-base-defaulted-constructor.t`.
- `PA12 incompatible conditional operands` (1):
  `spec/200-conditional-derived-base-lvalue-reference.t`.
- `PA12 invalid addition operands` (1):
  `general/300-using-declaration-function-hides-tag.t`.
- `PA12 invalid conversion` (6):
  `general/200-const-member-call-prefers-const-object-overload.t`,
  `general/200-derived-pointer-member-init.t`,
  `general/200-function-reference-return-expression-type.t`,
  `general/200-global-function-style-constructor.t`,
  `general/200-mutable-member-const-method.t`,
  `general/300-overloaded-deref-user-assignment.t`.
- `PA12 mem-initializer is not a direct field` (1):
  `general/200-aliased-base-mem-initializer-match.t`.
- `PA12 member call is inaccessible` (1):
  `general/200-friend-intermediate-derived-protected-base-method.t`.
- `PA12 member function access is unsupported` (1):
  `general/100-object-member-enumerator-constant.t`.
- `PA12 no viable constructor` (2):
  `general/500-inherited-constructor-using-access.t`,
  `general/500-inheriting-constructors.t`.
- `PA12 no viable function` (3):
  `general/200-reference-member-class-init.t`,
  `general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`,
  `spec/200-const-reference-binds-derived-pointer-prvalue.t`.
- `PA12 record member is inaccessible` (1):
  `general/200-friend-derived-access-inherited-protected-field.t`.
- `PA12 reference temporary conversion is invalid` (1):
  `general/300-prvalue-derived-base-friend-operator.t`.
- `PA12 unknown expression name` (4):
  `general/200-function-boundary-metadata-emission.t`,
  `general/200-parameter-access-metadata-emission.t`,
  `general/200-parameter-alias-metadata-emission.t`,
  `general/300-adl-using-declaration-source-point.t`.
- `PA12 unknown or ambiguous record member` (2):
  `general/200-member-call-hides-outer-type-declaration.t`,
  `general/300-class-using-declaration-reexposes-protected-field.t`.
- `PA12 unsupported expression form` (2):
  `general/200-placement-new-expression-aggregate-brace.t`,
  `general/200-placement-new-expression-constructor-call.t`.
- `expected declarator-id` (2):
  `general/200-nested-class-private-enclosing-access.t`,
  `general/300-alignas-out-of-class-nested-type.t`.
- `expected primary expression` (2):
  `general/200-elaborated-member-forward-type.t`,
  `general/300-user-defined-string-literal-operator.t`.
- `unexpected fixed token` (7):
  `general/100-qualified-typedef-cstyle-cast-same-name-operand.t`,
  `general/200-derived-pointer-overload-prefers-base-over-void.t`,
  `general/200-nested-braced-member-aggregate-init.t`,
  `general/200-private-base-static-cast-member.t`,
  `general/300-member-function-pointer-field-call.t`,
  `general/300-operator-nullptr-t-from-zero.t`,
  `spec/100-decltype-qualified-nested-type-local.t`.
- `unknown PA11 type name` (3):
  `general/200-inherited-injected-class-name-qualified-type.t`,
  `general/200-qualified-inherited-member-typedef.t`,
  `general/300-adl-associated-namespace-does-not-climb-parents.t`.
- `using declaration target is not a binding` (1):
  `general/500-inheriting-external-transitive-constructor.t`.
- `SEMANTICS_OK` but LowIR/reference residual (61):
  `general/100-function-pointer-nested-param-name-shadow.t`,
  `general/100-global-aggregate-nested-array-initializer.t`,
  `general/100-global-reference-incomplete-referent.t`,
  `general/100-qualified-const-method-definition.t`,
  `general/100-qualified-typedef-const-method-definition.t`,
  `general/200-aggregate-array-member-brace-elision.t`,
  `general/200-aggregate-class-member-subobject-init-target.t`,
  `general/200-aggregate-reference-member-binds-storage.t`,
  `general/200-const-subobject-member-call.t`,
  `general/200-defaulted-constructor-still-aggregate.t`,
  `general/200-deleted-constructor-still-aggregate.t`,
  `general/200-destructor-body-local-before-base-destruction.t`,
  `general/200-extern-class-object-declaration.t`,
  `general/200-external-ctor-overload-nonfirst-argument.t`,
  `general/200-global-class-array-enum-trivial-dtor.t`,
  `general/200-global-class-array-init.t`,
  `general/200-global-constructor.t`,
  `general/200-global-scalar-dynamic-init.t`,
  `general/200-local-default-class-array-lifecycle.t`,
  `general/200-local-struct-array-init.t`,
  `general/200-member-call-implicit-object-cv-overload.t`,
  `general/200-member-call-implicit-this-cv-overload.t`,
  `general/200-member-object-lifetime.t`,
  `general/200-member-pointer-const-typedef-return.t`,
  `general/200-nonliteral-field-condition-not-folded.t`,
  `general/200-out-of-class-getter-only.t`,
  `general/200-pointer-subscript-class-reference-return.t`,
  `general/200-protected-member-typedef-access-bad.t`,
  `general/200-qualified-friend-function-member-access.t`,
  `general/200-reference-indexed-pointer-member-access.t`,
  `general/200-reference-member-conditional-lvalue.t`,
  `general/200-unnamed-namespace-hidden-friend-single-definition.t`,
  `general/300-callable-field-hides-private-base-method.t`,
  `general/300-compound-assignment-adl-nonmember-after-member-reject.t`,
  `general/300-derived-shift-prefers-free-char-pointer.t`,
  `general/300-enum-class-nonmember-operator-bitand.t`,
  `general/300-friend-function-definition-skip.t`,
  `general/300-header-static-class-init.t`,
  `general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t`,
  `general/300-mixed-member-free-shift-stress-chain.t`,
  `general/300-namespace-aggregate-array-string-members.t`,
  `general/300-nested-enum-hidden-friend-bitmask-adl.t`,
  `general/300-operator-shift-stress-chain.t`,
  `general/300-overloaded-unary-deref-base-ref-return.t`,
  `general/300-packed-class-layout.t`,
  `general/300-pragma-pack-followed-by-endif.t`,
  `general/300-reference-member-same-name-as-class.t`,
  `general/300-static-class-member-object-definition.t`,
  `general/300-static-member-aggregate-array-dynamic-init.t`,
  `general/300-synthesized-array-member-lifecycle.t`,
  `general/300-thread-local-synthetic-symbol-family-isolation.t`,
  `general/300-unary-address-of-builtin-fallback.t`,
  `general/300-value-init-aggregate-with-nontrivial-member.t`,
  `general/300-value-init-empty-functional-cast-aggregate.t`,
  `general/400-bit-field-constructor-member-init.t`,
  `general/400-bit-field-member-access-bad.t`,
  `general/400-bit-field-prefix-postfix-increment.t`,
  `general/400-bitfield-aggregate-init.t`,
  `general/400-signed-bit-field-read.t`,
  `general/400-signed-enum-bit-field-read.t`,
  `spec/200-list-init-narrowing-bad.t`.

The nearby exclusions were checked directly: the qualified const/typedef
definitions, getter-only definition, and const-typedef member-pointer return
all reach PA12 and differ only in existing LowIR address presentation; the
private nested leading-return control remains a passing control. The nested
out-of-class constructor reaches PA12 after this slice, but its class-by-value
argument remains an unrelated unsupported conversion boundary.

## Active Checkpoint

Status: complete for this checkpoint; the final evidence is recorded below and
this exact implementation plus plan is the single authorized commit scope.
Changes are limited to:

- `dev/src/pa11_semantic_model.h`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic_types.cpp`
- `dev/src/pa11_semantic.cpp`
- `dev/src/pa12_semantic_construction.cpp`
- `dev/src/pa12_semantic.cpp`
- `pa16/plan.md`

Invariants are: `auto` definitions acquire their result from a typed trailing
`TypeId`; every trailing type and parameter is resolved against the qualified
member owner; `SpecFact::is_auto` cannot publish an invalid declaration or
function result; `FunctionFact.owner`, binding owner, function-scope parent,
and body lexical scope agree; constructor/destructor sidecars retain the
canonical `NamedRecordId`; and namespace-scope special definitions enter the
same PA12 preparation/analyze path as in-class definitions. No text key,
rendered output, reference executable, handout fixture, comparator, coverage
rule, or new source set is introduced.

Focused evidence is `5/7`: the baseline identity
`300-member-function-trailing-return.t` now passes; the other two cluster
identities remain at typed PA12 boundaries with exact diagnostics `PA12
invalid conversion` (`300-out-of-class-member-trailing-return.t`) and `PA12
no viable function` (`200-nested-out-of-class-constructor-enclosing-type.t`).
The four controls in that matrix, `100-out-of-class-methods.t`,
`300-out-of-class-private-nested-return-type.t`,
`200-constructor-overload-default-arg-nonfirst-argument.t`, and
`200-return-preserves-value.t`, pass; an additional
`200-constructor-member-init.t` control also passes. All five are baseline
pass identities, while the first listed cluster identity is the only baseline
failure repair established here. Temporary typed probes for an explicitly
global-qualified ordinary member, nested constructor, and nested destructor
all exit successfully. The additional focused constructor control
`200-constructor-member-init.t` also passes. The PA16 gate removed no
unrelated identities and introduced no final-only identity.

## Performance Evidence

The change has no whole-program scan or retry. For a declarator with `d`
children, PA11 performs one bounded shape scan and one suffix operation walk;
the trailing return performs one typed `TypeId` walk and each parameter is
processed once, so the work is O(d + p + t) plus the existing indexed
qualification lookup. A special-member owner resolves one qualified path of
length `q`, O(q), and then reuses the resulting class scope for all parameters
and the body. No new cache, textual key, or nested program/scope rescan is
present.

Structural evidence for the focused inputs is small and bounded: the
in-class trailing-return declarator has one parameter clause, two qualifiers,
and one trailing `TypeId`; the out-of-class case has one qualified owner path
and one member typedef return; the nested special member has a three-component
qualified constructor path and one enclosing-class parameter. External
structural scaling evidence is durable at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-structure-v1/run.log`.
It uses the read-only executable copy
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-structure-v1/cppgm++.pa16-structure` with SHA-256
`e5aad4ec7b6a2c7638925ea598f6c3771fbcabc930c1370ef2c6664731e7c40d`. For
`N=1,4,16,64` generated inputs, each has `N` declarations and `N`
definitions; both `--emit-semantics` runs exit 0 and produce identical hashes.
The recorded output lines/bytes/function records are respectively
`14/468/1`, `32/1227/4`, `104/4269/16`, and `392/16461/64`.
The run-log SHA-256 is
`ba3c5a39c80a36e908252cbe6bbf13d624a8e5019733157776705b0b7cf69ed0`.
These are structural determinism and scaling observations only; no timing or
speedup claim is made.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed; final log `pa16-operator-followup-through-pa15.log`. |
| PA16 coverage and regression gate | Final `127/243` passed, `116` failed, `243/243` covered; exact comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only identities. |
| Follow-up bool/index audit | Expression-owned typed bool provenance, composite hidden-friend key, corrected same-name performance evidence; all 29 removed identities retained. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; log `pa16-operator-followup-file-audit.log`; final `git diff --check` is recorded in `pa16-operator-followup-diff-check.log`. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpoint audit | Completed bounded repair and audit of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the corrective follow-up separates exact friend-definition lexical ownership from access friendship and records typed public/private/protected base-reference accessibility, including a bounded further-derived protected proof. Enum identity/ranking, narrow constructor-backed reference binding, reference/address facts, and typed bool boundaries remain covered. Final PA16 is `127/243` with `116` failures and `243/243` coverage; comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only failures. Through-PA15 is `1167/1167`; focused status is `29/32` with three documented pre-existing holdouts; course 411 passes; final state-matched performance is `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. |
| `da4252b6` typed bit-field boundary checkpointAudit/follow-up | Complete: PA10--PA15 typed operation/promotion facts, semantic-owner validity checks, const-reference temporaries, overload-before-address-of, owner-stable mixed/zero-width/unnamed/union layout, checked oversized allocation spans, masked signed/unsigned projection, and isolated initialization roots. Final PA16 is `131/243` passed with `112` failures and `243/243` identities; exact comparison to the turn-start map is baseline-only `0` and final-only `0`. Course 412 and the direct alias control pass; focused matrix is `5/11` with six documented LowIR mismatches; through-PA15 is `1167/1167`; file audit and diff-check pass. Corrected bit-field performance is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-perf-final-v1` with 30/30 zero-exit runs, actual bit-field inputs/counters, and final/immutable SHA-256 `c98edbf143904e0b09b451310de38e7966149b4374ad912b55a1b9f8c96aaf02`; final wall medians small/large/nested are `0.00/0.07/0.00s` and RSS medians `6104/30600/6620` KiB. |
| Member-function-definition declarator context (complete) | Typed `auto` trailing-return operation is ordered before function construction; special-member owner resolution accepts explicitly global-qualified paths and preserves canonical constructor/destructor ownership; namespace-scope PA12 preparation/analysis uses those facts; and invalid `auto` results fail closed in PA11. Final PA16 is `132/243` passed, `111` failed, `243/243` covered, with exactly one baseline-only repair (`general/300-member-function-trailing-return.t`) and no final-only identities. Focused matrix is `5/7`; the two residual diagnostics are `ERROR: PA12 invalid conversion` and `ERROR: PA12 no viable function`, with five existing constructor/destructor/member-definition controls passing across the focused runs. Through-PA15 is `1167/1167`; file audit passes with five pre-existing warnings; final `git diff --check` passes. Structural evidence is the external artifact recorded above. Excluded aggregate/lifetime, class-by-value, pointer/reference conversion, parser, bit-field, and LowIR presentation boundaries remain unchanged. Commit checkpoint status: this finalized implementation and plan are the single authorized coherent PA16 commit scope. |
