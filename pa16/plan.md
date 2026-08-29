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
`spec.md` §§2, 3, 4, 5, and 7: relevant lexical, using, and member/base
candidate lists are walked in deterministic order, with no merged namespaces,
whole-TU scan, retry loop, or second semantic model. Unrelated bit-field,
lifetime, and broad layout work remains outside this checkpoint.

The approved follow-up is a readable, line-neutral refactor of
`process_using_declaration`: it keeps `dev/src/pa11_semantic.cpp` at exactly
`3000` lines, merges the redundant value validation/classification/dedup
staging work, and uses `base_path_accessible` as the single canonical
relation/access walk. No validation boundary is weakened and no newly added
follow-up line exceeds `118` characters.

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
turn-start map above is the `184/243`, 59-failure baseline. The landed HEAD at
turn start measured `187/243`, 56 failures, with `243/243` identities covered;
that is the pre-audit authority; the final authorized result is recorded
below. The exact landed baseline-to-HEAD delta is:

- baseline-only (fixed): `100-object-member-enumerator-constant.t`,
  `200-member-call-hides-outer-type-declaration.t`, and
  `300-using-declaration-function-hides-tag.t`;
- final-only (new): none;
- final residual: exactly the 59-item baseline map above minus those three
  identities.

The final focused matrix is `8/8`: the three owned identities and five
preservation controls all pass. The current exact broad delta, coverage, and
stage-progress result is final: `187/243` passing, `56` failures, and
`243/243` identities covered. The sorted comparison against the authoritative
turn-start log has baseline-only `0` and final-only `0`; the complete residual
is exactly the 59-item target map above minus the three landed identities.
The durable derivation is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/failure-identity-comparison.log`.

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
using-declaration importing a function alongside a tag. The audit correction
also fixes the same PA11 namespace boundary when the destination already has
the other namespace's identity. PA11 publishes the two lookup identities
independently; PA12 creates the typed enumerator literal with one
object-evaluation child or the normal implicit-object member call. PA15
performs the one required child evaluation before the enumerator literal
lowering; no new source-set entry is needed.

The implementation publishes canonical type and ordinary-value identities
independently, but the using-declaration coexistence exception is limited to a
real class/enum tag (`BindingKind::Type`); a typedef/alias remains conflicting
with an ordinary value/function in the other namespace. Canonical
`BindingId`/origin `ScopeId`/source-point facts, deterministic relevant-scope
candidate order, and existing invalid-access rejection are preserved. It does not edit
handout tests, fixtures, harnesses, comparators, coverage rules, or frontend
source sets; the audit adds only the focused executable course regression
`421-typed-using-separate-namespaces-regression.sh`. It adds no whole-scope/TU
scan, textual name heuristic, retry loop, duplicate overload resolver, or
duplicate lowering model. The audit also hardens enumerator owner/type/width
and member default-fact range boundaries without changing the representation.
The previously rejected bit-field diff was restored before this checkpoint and
is not in this scope.

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

The readability follow-up does not change these ownership or complexity
boundaries. It only makes `process_using_declaration` line-neutral at `3000`
source lines by removing redundant validation/base-relation work and retaining
the single canonical `base_path_accessible` walk. This record makes no timing,
RSS, allocation, or speedup claim.

The landed checkpoint's deterministic source generator outside the repository
created N unrelated namespaces, each with a same-spelled `struct f` and
`int f(int)`, then one using-import plus a direct class member with the same
spelling and calls that exercise both boundaries. Repeated `--emit-semantics`
outputs were identical:

| noise namespaces | bytes | lines | repeat SHA-256 |
| ---: | ---: | ---: | --- |
| 8 | 3220 | 84 | `6001b3f2e08ac4d6ba0756355dc67ee9a2185c4aa4198beaa52173762835b079` |
| 32 | 8376 | 228 | `47bbb35964ae2a5a1e4c41efe5b994120e75a31c60f6c7fb0cb3d62ca2ccc974` |

This is structural scaling and determinism evidence only; no timing,
allocation, RSS, or speed claim is made. The current audit probe log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-structural-correction-20260829.log`:
the typed-using output repeats byte-identically at `442` bytes/`17` lines
(SHA-256
`f872964c24e02f053e386733d9f5567c03d86f2c4d94abede253fea531ae1fc6`), with
two `type f` and two `function f` identities; the effectful enumerator has one
`get_holder` call, the ambiguity member/static
probes have one `Base__f`/`C__f` call, and the function-id probe has one
`addr @selected`. Unsigned-32, signed-minimum, and 64-bit-width enum probes
also exit `0`.

## Validation

- `make -C dev cppgm++ -j2`: exit `0`.
- The readability follow-up leaves `dev/src/pa11_semantic.cpp` at exactly
  `3000` lines; its `67` added physical lines have maximum length `118`, and
  no newly added follow-up line exceeds `118` characters.
- The focused handout matrix containing the three landed identities and five
  preservation controls is `8/8`; its final log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-focused-matrix-final-20260829-v3.log`.
- Courses 402, 403, 405, 419, and new 421 pass with `sh -n`; 402 prints the
  expected rejected inherited-noncallable diagnostic while exiting `0`. Course
  421's four legal class/enum-tag/value-function cases exit `0`; its source
  typedef/alias into an existing value/function and source value/function into
  an existing typedef/alias each exit exactly `1`. The final syntax/run log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/course421.log`;
  final controls 402/403/405/419 are in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-course-controls-final-20260829-v3.log`.
- The current structural/determinism and exactly-once probes are recorded in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-structural-correction-20260829.log`.
- The exact final command `n=16; ... make test-report-through-pa15` exits `0`
  at `1167/1167`; its durable log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/through-pa15.log`.
  Exact `make test-pa16` exits `2` with `187/243` passing, `56` failures, and
  `243/243` identities covered; its durable log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/test-pa16.log`.
  The sorted final-vs-turn-start identity derivation has baseline-only `0` and
  final-only `0` in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/failure-identity-comparison.log`.
- File audit exits `0` with five existing header-division warnings; its final
  log is
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/file-audit-pa16.log`.
- No handout, fixture, harness, comparator, coverage, source-set, or unrelated
  source file changed. Final `git diff --check` is recorded in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/v4-final/diff-check.log`.
- Course 406 is unchanged from clean `a5b496e8`: its first
  `qualified-parenthesized-static.cpp` case reports `ERROR: unknown PA11 type
  name` and exits `1` in both builds. The current and clean ASTs are identical
  (`1943` bytes, SHA-256
  `b25a06952b8e4cf9e40a1fed597daeb0c920b4208e41f3d8d54a47cc8491c303`) and
  use a cast-shaped `type-id Qualified::f`, before the permitted shared
  selector; fixing that parser/cast ambiguity would widen this bounded audit.
  The current and clean-a5 evidence logs are
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-course406-current-final-20260829-v3.log`
  and
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-course406-baseline-trace-20260829.log`.

## Next Checkpoint

This lookup audit is complete. Its final authority is the exact turn-start
59-failure map minus the three landed identities: `56` failures, full
`243/243` coverage, no final-only identity, and preserved stage progress, as
shown by the v4 through-PA15, PA16, file-audit, diff-check, and identity logs.
The next PA16 checkpoint should preserve this typed type/value owner flow—
including the real-tag-versus-alias conflict boundary—and address another
residual boundary only after an authorized focused design review.

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
| `PA16 typed ordinary-value-over-tag lookup checkpoint` | Completed audit of landed HEAD `a5b496e8` relative to `1093c2b7`: final PA16 is `187/243` passing, `56` failures, and `243/243` covered; v4 sorted comparison with the turn-start `last-test.log` has baseline-only `0` and final-only `0`, preserving the complete residual map and stage progress. The source repair permits cross-space coexistence only for canonical real class/enum tags (`BindingKind::Type` backed by `NamedKind::Class`/`Enum`), retains typedef/alias conflicts, and hardens enumerator/default-fact ranges. The approved follow-up is a readable, line-neutral `process_using_declaration` refactor at exactly `3000` lines, merging redundant value validation/classification/dedup staging work and using `base_path_accessible` as the single canonical relation/access walk; no newly added follow-up line exceeds `118` characters. Focused handout matrix is `8/8`; course 421 proves four status-0 legal cases and four exact status-1 alias conflicts; structural probes are deterministic and exactly-once. Through-PA15 is `1167/1167`; file audit passes with five warnings; diff-check passes. Course 406 has the same first qualified-static-call status-1 failure on clean `a5b496e8` and current code before the shared selector, so it remains outside this bounded audit. Final v4 logs and exact-set derivation are recorded above. No handout, fixture, harness, comparator, coverage, source-set, or unrelated source changed. |
