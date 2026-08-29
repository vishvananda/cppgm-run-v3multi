# PA16 implementation plan

## Stage Design

PA12 owns typed constructor/destructor selection and records lifetime facts at
the declaration that owns the object. Namespace and static class-member
definitions use the same typed initializer path; supported named class
subobjects receive typed constructor and namespace-lifetime facts, while
aggregate arrays retain their existing aggregate path. A declaration-only
object has no construction fact. Thread-local construction is represented by
the typed pending global action and is not modeled as a namespace destruction
edge in this checkpoint.

PA15 consumes those facts through a separate semantic-demand and
emission-demand path. Its global work item is `(SemanticFactId, global_root)`:
the ordinary-global and TLS roots occupy separate bits in
`scanned_global_fact_modes`, so a fact reached in both modes is scanned once
per mode. Each scan follows only typed semantic children and demanded typed
constructor/destructor edges, giving bounded O(facts + typed edges) work with
no whole-TU rescan and no retry-until-stable loop. The global/TLS materializer
emits only demanded actions and keeps one deterministic source-ordered init
and fini family.

Generated TLS helper names are derived once from typed `BindingId`, its
canonical owner/name, and an explicit generated-kind prefix. `SymbolId` and
`SpellingId` are carried as output identities; no rendered spelling is read
back or parsed to recover ownership or kind. The low-level output therefore
retains typed ownership, typed lifetime actions, and deterministic symbols.
This follows the PA16 README and the relevant `spec.md` §§1--5 and 7
boundaries; unrelated bit-fields, operators, access control, and broad layout
work remain outside this checkpoint.

## Failure Map

Turn-start authority was clean HEAD `c2247924`: `179/243` PA16 identities
passed, `64` failed, and all `243/243` identities were covered. The complete
turn-start failure map was:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/100-global-aggregate-nested-array-initializer.t`
- `pa16/tests/general/100-global-reference-incomplete-referent.t`
- `pa16/tests/general/100-object-member-enumerator-constant.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-defaulted-constructor-still-aggregate.t`
- `pa16/tests/general/200-deleted-constructor-still-aggregate.t`
- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-extern-class-object-declaration.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-global-constructor.t`
- `pa16/tests/general/200-global-function-style-constructor.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-member-call-hides-outer-type-declaration.t`
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
- `pa16/tests/general/300-header-static-class-init.t`
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
- `pa16/tests/general/300-static-class-member-object-definition.t`
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-thread-local-synthetic-symbol-family-isolation.t`
- `pa16/tests/general/300-unary-address-of-builtin-fallback.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-using-declaration-function-hides-tag.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-constructor-member-init.t`
- `pa16/tests/general/400-bit-field-member-access-bad.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Final PA16 is `184/243` passing, `59` failing, with `243/243` identities
covered. The exact final map is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/100-global-aggregate-nested-array-initializer.t`
- `pa16/tests/general/100-global-reference-incomplete-referent.t`
- `pa16/tests/general/100-object-member-enumerator-constant.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-defaulted-constructor-still-aggregate.t`
- `pa16/tests/general/200-deleted-constructor-still-aggregate.t`
- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-extern-class-object-declaration.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-member-call-hides-outer-type-declaration.t`
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
- `pa16/tests/general/300-using-declaration-function-hides-tag.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-constructor-member-init.t`
- `pa16/tests/general/400-bit-field-member-access-bad.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The five baseline-only identities fixed are
`200-global-constructor`, `200-global-function-style-constructor`,
`300-header-static-class-init`, `300-static-class-member-object-definition`,
and `300-thread-local-synthetic-symbol-family-isolation`; final-only is zero.
The failure delta is `64 -> 59` (`+5` passing), with no coverage reduction.

## Active Checkpoint

This checkpoint implements typed non-automatic class-object lifetime and
demand-driven helper emission in the following implementation files:

- `dev/frontend_source_sets.mk`
- `dev/src/pa11_semantic_model.h`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa12_semantic_construction.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa15_lowering.cpp`
- `dev/src/pa15_lowering.h`
- `dev/src/pa15_lowering_calls.cpp`
- `dev/src/pa15_lowering_construction.cpp`
- `dev/src/pa15_lowering_flow.cpp`
- `dev/src/pa15_lowering_globals.cpp`

`LifetimeStorageKind` is intentionally only `Automatic` or `Namespace`.
Thread-local objects do not receive a namespace destruction fact; their
per-thread construction remains an explicit typed pending action. PA12
preserves aggregate-array initialization and local-object lifetime behavior,
while named namespace/static class objects use the declaration-owned typed
constructor path. PA15 emits static storage, constructor calls, destructor
calls, and TLS guard/wrapper/init helpers only after typed demand reaches the
binding. The TLS family uses canonical owner/name data plus generated-kind
prefixes and passes output identities directly to materializers.

Focused validation is green at `8/8` for the five fixed identities, both
static-thread-local member tests, the collision test, and
`100-global-class-zero.t`. The latter remains a no-eager-helper control.
No handout, reference, existing test, harness, comparator, or exit-status
file was changed.

## Performance Evidence

A temporary generated case with many non-automatic `Item` objects was run at
N=8 and N=32. Repeated compiler outputs were byte-identical for each N. The
observed structural counts were:

| objects | init functions | fini functions | ctor calls | dtor calls | LowIR lines |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 8 | 1 | 1 | 8 | 8 | 122 |
| 32 | 1 | 1 | 32 | 32 | 290 |

The N=32 init calls appeared in source order with arguments 1 through 32,
and the fini family contained the corresponding 32 source-ordered destructor
calls. This is structural bounded-work evidence only; no timing, allocation,
RSS, or speed claim is made.

## Validation

- `make -C dev cppgm++ -j2`: exit `0`.
- Focused PA16 matrix: `8/8`, exit `0`.
- `make test-pa16`: exit `2`, `184/243` passing, `59` failing, `243/243`
  covered; exact final map above and no final-only identities.
- Required `n=16` through command: exit `0`, PA1--PA15 `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: exit `0`
  with five known header-division warnings.
- `git diff --check`: required before commit.

## Next Checkpoint

The remaining 59 identities are outside this typed non-automatic lifetime
increment. Any next PA16 work must preserve this exact final map unless a
later authorized run proves a new net improvement, retain typed ownership and
mode-sensitive demand, and avoid eager helper emission.

## Checkpoint Ledger

| checkpoint | result |
| --- | --- |
| `36b93869` historical aggregate handoff | `159/243` PA16 passing, `84` failures, `243/243` covered; PA1--PA15 `1167/1167`. |
| `dea01c52` aggregate implementation | Historical aggregate increment: `164/243` passing, `79` failures, `243/243` covered; through-PA15 and audit evidence preserved. |
| `3c2114b6` typed-builtin turn-start | Historical clean state at `164/243`, `79` failures, `243/243` covered; exact residual map carried forward. |
| `d7ed98aa` typed builtin boundary | `167/243` passing, `76` failures, `243/243` covered; typed builtin semantic/lowering owner added. |
| `f290784f` typed builtin audit | Historical clean baseline: `167/243`, `76` failures, `243/243` covered; PA1--PA15 and audit pass. |
| `3b7d8e6a` qualified-type checkpoint | `173/243` passing, `70` failures, `243/243` covered; prior broad evidence preserved. |
| `working tree after 3b7d8e6a` historical constructor first stop | Selected constructor family and preservation controls passed before the later constructor increment. |
| `30d69fc3` landed inheriting-constructor checkpoint | Historical landed increment: `176/243` passing, `67` failures, `243/243` covered; typed N3485 wrapper/default/DMI/copy/order-independent evidence retained. |
| `0fb73ad4` PA16 access turn start | Clean authority for that checkpoint: `176/243` passing, `67` failures, `243/243` covered; through-PA15 `1167/1167`, audit with five known header-division warnings. |
| `PA16 typed access-control checkpoint` | Previous bounded access repair: `179/243` passing, `64` failures, `243/243` covered; exact residual map carried forward. |
| `PA16 typed non-automatic lifetime checkpoint` | `184/243` passing, `59` failures, `243/243` covered; five baseline identities fixed, zero final-only identities; focused matrix `8/8`, through-PA15 `1167/1167`, audit clean except five known warnings, and structural N=8/N=32 evidence recorded above. |
