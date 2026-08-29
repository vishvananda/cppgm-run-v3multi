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
family and reverse-construction-ordered fini family.

Generated TLS helper names are derived once from typed `BindingId`, its
canonical owner/name, and an explicit generated-kind prefix. `SymbolId` and
`SpellingId` are carried as output identities; no rendered spelling is read
back or parsed to recover ownership or kind. The low-level output therefore
retains typed ownership, typed lifetime actions, and deterministic symbols.
This follows the PA16 README and the relevant `spec.md` §§1--5 and 7
boundaries; unrelated bit-fields, operators, access control, and broad layout
work remain outside this checkpoint.

## Failure Map

The landed increment's parent authority was clean HEAD `c2247924`: `179/243`
PA16 identities passed, `64` failed, and all `243/243` identities were
covered. The complete parent failure map was:

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

At the target turn start, clean HEAD `a1a2cf83` was `184/243` passing,
`59` failing, with `243/243` identities covered. The exact target/final map
is:

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
- `dev/src/pa11_semantic_core.cpp`
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

The durable focused regression is
`cppgm.tests/course/pa16/420-typed-redeclaration-lifetime-order-regression.sh`.

`LifetimeStorageKind` is intentionally only `Automatic` or `Namespace`.
Thread-local objects do not receive a namespace destruction fact; their
per-thread construction remains an explicit typed pending action. PA12
preserves aggregate-array initialization and local-object lifetime behavior,
while named namespace/static class objects use the declaration-owned typed
constructor path. PA15 emits static storage, constructor calls, destructor
calls, and TLS guard/wrapper/init helpers only after typed demand reaches the
binding. The TLS family uses canonical owner/name data plus generated-kind
prefixes and passes output identities directly to materializers.

The checkpoint audit carries PA11's exact per-declarator `definition` bool in
a compact typed arena parallel to `declaration_bindings_`. PA12 validates and
consumes that bit rather than reclassifying the current declarator from
`is_extern` or AST shape, so a bodyless `extern` redeclaration cannot publish a
second constructor/lifetime fact. PA15 validates the same typed range and keeps
the actual definition declaration as the ordering owner, rather than replacing
it with a bodyless canonical redeclaration.

Focused validation is green at `8/8` for the five fixed identities, both
static-thread-local member tests, the collision test, and
`100-global-class-zero.t`. The latter remains a no-eager-helper control.
The new 420 executable regression and `sh -n` also pass. No handout,
reference, existing test, harness, comparator, or exit-status file was changed.

## Performance Evidence

A temporary generated case with many non-automatic `Item` objects was run at
N=8 and N=32. Each input had `destroyed`, an `Item` with an integer
constructor/destructor, one source-ordered declaration per item
(`item1(1)` through `itemN(N)`), and a trivial `main`. Repeated compiler
outputs were byte-identical for each N. The observed structural counts were:

| objects | init functions | fini functions | ctor calls | dtor calls | LowIR lines | SHA-256 |
| ---: | ---: | ---: | ---: | ---: | ---: | --- |
| 8 | 1 | 1 | 8 | 8 | 102 | `f2b10beb5a642aa2d176762572f9590088c4f5fa74c48927f415a392a42fe1b3` |
| 32 | 1 | 1 | 32 | 32 | 270 | `33b39208fea1238e270880616f99c3c838370648c4fe4064bc97257ab5eb3bda` |

The N=32 init calls appeared in source order with arguments 1 through 32,
and the fini family contained the corresponding 32 destructor calls in reverse
source/construction order. This is structural bounded-work evidence only; no
timing, allocation, RSS, or speed claim is made.

## Validation

- `make -C dev cppgm++ -j2`: exit `0`.
- Focused PA16 control set: `8/8`, exit `0`.
- `make test-pa16` exits `2` with `184/243` passing, `59` failing, and
  `243/243` identities covered. The sorted comparison with the authoritative
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` has
  baseline-only `0` and final-only `0`; the exact final map is above.
- The five fixed identities pass `5/5`; courses 404, 407, 409, 410, and 415
  each exit `0`; the new course 420 passes `sh -n` and execution; and the
  repeated N=8/N=32 structural probe passes. The relevant 12-test handout
  matrix is `9/12` with the three known LowIR-shape residuals.
- The required `n=16` through command exits `0` at `1167/1167` through PA15.
  `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with five known header-division warnings, and `git diff --check` exits `0`.
  Durable logs are `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-test-pa16.log`,
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-test-report-through-pa15.log`,
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-file-audit-pa16.log`,
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-pa16-failure-set-comparison.log`,
  and `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-focused-pa16-full.log`.

## Next Checkpoint

The remaining 59 identities are outside this typed non-automatic lifetime
increment. This checkpoint audit is complete; any next PA16 work must preserve
this exact map unless an authorized run proves a new net improvement, retain
typed ownership and mode-sensitive demand, and avoid eager helper emission.

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
| `PA16 typed non-automatic lifetime checkpoint` | Completed checkpoint audit: PA11 exact per-declarator definition continuity, PA12 typed lifetime ownership, and PA15 definition-owner ordering are repaired and traced. PA16 is `184/243` passing, `59` failures, `243/243` covered; exact sorted comparison has baseline-only `0` and final-only `0`; focused matrix is `9/12` with the same three LowIR-shape residuals; course 420, through-PA15 `1167/1167`, file audit with five known warnings, diff-check, and N=8/N=32 repeatability all pass. |
