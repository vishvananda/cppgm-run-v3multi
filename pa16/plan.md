# PA16 implementation plan

## Stage Design

PA11 keeps type/tag lookup and ordinary-value lookup as separate typed
indices. A using-declaration now publishes both independently when the target
has both identities, while value entries retain the canonical `BindingId`,
origin `ScopeId`, and declaration source point. No rendered name is used to
recover a binding. PA12 consumes the resulting typed facts: a class
enumerator selected through `.` or `->` becomes its declared integral-type
constant prvalue owned by its `BindingId` and owner scope, while retaining
exactly one typed child for postfix object evaluation. PA15 lowers that child
once before producing the constant, so `holder()->enumerator` preserves the
call and a plain local object retains its canonical no-op evaluation. An
unqualified member call is resolved against the implicit object before an
outer type can turn the parser's call-shaped statement into a declaration.
Direct member functions therefore hide an outer type with the same spelling,
while a namespace using-declaration still imports the ordinary function in
the ordinary-value namespace.

The existing typed member-call selector is shared by ordinary member calls and
the grammar-ambiguity recovery path. It ranks the complete candidate set with
implicit-object cv/base conversion, explicit conversions, defaults, and
variadics; recovery supplies already-typed argument facts and does not run a
reduced resolver. PA15 consumes selected bindings and child facts without
reconstructing lookup or spelling. The design follows the PA16 README and
`spec.md` §§2, 4, 5, and 7: relevant lexical, using, and member/base
candidate lists are walked in deterministic order, with no merged namespaces,
whole-TU scan, retry loop, or second semantic model. Unrelated bit-field,
lifetime, and broad layout work remains outside this checkpoint.

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

At the target turn start, clean HEAD `1093c2b7` was `184/243` passing,
`59` failing, with `243/243` identities covered. The authoritative target
baseline map is:

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

The prior `64 -> 59` reduction is historical. The complete authoritative
turn-start map above is the `184/243`, 59-failure baseline. Broad validation
finished at `187/243`, 56 failures, with `243/243` identities covered. The
exact baseline-to-final delta is:

- baseline-only (fixed): `100-object-member-enumerator-constant.t`,
  `200-member-call-hides-outer-type-declaration.t`, and
  `300-using-declaration-function-hides-tag.t`;
- final-only (new): none;
- final residual: exactly the 59-item baseline map above minus those three
  identities.

The focused matrix moved the three owned identities from `6/9` passing to
`9/9`; the six preservation identities stayed passing. Three additional
nearby invalid controls also pass, for `12/12` combined. The durable exact-set
comparison is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-pa16-failure-set-comparison-lookup.log`.

## Active Checkpoint

This checkpoint implements typed ordinary-value-over-tag lookup and member
enumerator values in the following implementation files:

- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic_model.h`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa11_semantic.cpp`
- `dev/src/pa12_semantic_member.cpp`
- `dev/src/pa15_lowering_flow.cpp`

The scope is limited to the three owned baseline failures: class enumerator
member access, a direct member call hiding an outer type, and a namespace
using-declaration importing a function alongside a tag. PA11 publishes the
two lookup identities independently; PA12 creates the typed enumerator
literal with one object-evaluation child or the normal implicit-object member
call. PA15 performs the one required child evaluation before the enumerator
literal lowering; no new source-set entry is needed.

The implementation preserves separate type/value namespaces, canonical
`BindingId`/origin `ScopeId`/source-point facts, deterministic relevant-scope
candidate order, and existing invalid-access rejection. It does not edit
handout tests, fixtures, harnesses, comparators, coverage rules, or add a
course regression; it adds no whole-scope/TU scan, textual name heuristic,
retry loop, duplicate overload resolver, or duplicate lowering model. The
previously rejected bit-field diff was restored before this checkpoint and is
not in this scope.

## Performance Evidence

This checkpoint changes lookup behavior but not the lookup/index storage
model. Existing per-scope typed indices are queried in average O(1) per
scope; the relevant lexical/using walk plus member/base walk is O(S + B + C),
where S is the visited scope/using chain, B is the relevant base-path work,
and C is the returned candidate count. The shared selector performs one pass
over C candidates; its general typed work is O(C*A + C*B) in the worst case
for A explicit arguments, and the one-argument grammar-recovery call is O(C)
candidate work aside from the existing base-relation checks. Its temporary
score storage is O(C*A), with no persistent or parallel semantic model. There
is no whole-TU scan or retry loop.

The final deterministic source generator outside the repository created N
unrelated namespaces, each with a same-spelled `struct f` and `int f(int)`,
then one using-import plus a direct class member with the same spelling and
calls that exercise both boundaries. Repeated `--emit-semantics` outputs were
identical:

| noise namespaces | bytes | lines | repeat SHA-256 |
| ---: | ---: | ---: | --- |
| 8 | 3220 | 84 | `6001b3f2e08ac4d6ba0756355dc67ee9a2185c4aa4198beaa52173762835b079` |
| 32 | 8376 | 228 | `47bbb35964ae2a5a1e4c41efe5b994120e75a31c60f6c7fb0cb3d62ca2ccc974` |

This is structural scaling and determinism evidence only; no timing,
allocation, RSS, or speed claim is made. The two post-correction semantic
probes were also run outside the repository: the effectful enumerator probe
emitted one `call ptr @get_holder()` in 30 lines/677 bytes (SHA-256
`448b9b925f6edc2b44e6b22b2afad5874e2762c2f8b25d85dffbaee8e4af91d8`), and the
overload probe emitted the const-mangled `fuzz::fields` call and its `store
i32 2` body in 52 lines/1251 bytes (SHA-256
`ff5ce34db0f5032e78d59989fe39d68c76c8fe80658ce113cd423961b34e2c9f`).

## Validation

- `make -C dev cppgm++ -j2`: exit `0`.
- After restoring clean HEAD, the required 9-test matrix was `6/9`: the six
  preservation controls passed and the three owned identities failed. With
  this diff, the same matrix is `9/9`; the three additional nearby invalid
  controls also pass, for `12/12` in the final combined focused run. The
  durable log is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-focused-pa16-lookup.log`.
- The effectful external member-enumerator probe retains exactly one
  `get_holder` call in LowIR. The external same-spelled outer-tag/member
  overload probe selects the const member (`_ZNK4fuzz6fieldsER6buffer`) and
  emits its `chosen = 2` store.
- `make test-pa16` exits `2` because the 56 residual fixture mismatches remain;
  its measured summary is `187/243` with `243/243` identities covered. The
  exact baseline comparison has three baseline-only identities and zero
  final-only identities. Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-test-pa16-lookup.log`.
- The exact prior command (`n=16; ... make test-report-through-pa$((n - 1))`)
  exits `0` at `1167/1167`. Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-test-report-through-pa15-lookup.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with the five known header-division warnings. Durable log:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-file-audit-pa16-lookup.log`.
- The final noise probe repeated identical N=8/N=32 semantic-output hashes;
  the two semantic probes and their counts/hashes are recorded in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-semantic-probes-pa16-lookup.log`
  and `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/final-lookup-noise-pa16.log`.
- `git diff --check` exits `0`; no handout, fixture, harness, comparator,
  source-set, or test file was changed. Turn-start authority records PA1--PA15
  at `1167/1167`.

## Next Checkpoint

This lookup checkpoint is validated and its final residual is the exact
turn-start 59-failure map minus the three owned identities above, with no new
failures and full coverage. The next PA16 checkpoint should preserve this
typed type/value owner flow and address another residual boundary only after
an authorized focused design review.

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
| `PA16 typed ordinary-value-over-tag lookup checkpoint` | Completed checkpoint: `187/243` passing, `56` failures, `243/243` covered; exact delta is three baseline-only fixes (`100-object-member-enumerator-constant`, `200-member-call-hides-outer-type-declaration`, `300-using-declaration-function-hides-tag`) and zero final-only identities. Final focused matrix is `12/12`, prior-through is `1167/1167`, audit passes with five known warnings, and final LowIR/noise probes are deterministic. |
