# PA16 implementation plan

## Stage Design

The canonical owner for this checkpoint is the PA11/PA12 typed declaration and
constructor fact flow. `semantic_variable_initializer` classifies an automatic
class object and publishes a `SemanticFactKind::ConstructorAction` with the
selected constructor binding, selected scope, callable type, and
`value_initialize` bit. `constructor_action_is_noop` consumes that typed
selection and its `FunctionFact`/sidecar; it does not inspect source text.

PA15 has two typed declaration consumers: `lower_variable_expression` handles
condition declarations, while the `Variable` statement path handles ordinary
local declarations. Both obtain canonical storage with `storage_for` and must
use `address_of_storage` once for an automatic class object when the selected
synthetic default constructor has no actions. The exact invariant is: an
automatic class-object declaration materializes its storage/address exactly
once, whether its declaration fact has a childless class path or a no-op
`ConstructorAction`; non-no-op, value-initializing, braced, and scalar paths
retain their existing owners and action order. `activate_lifetime` remains a
separate typed lifetime operation.

The narrow class-value overlap check is also owner-typed: its canonical
`variable_facts_` entry must pair with the keyed `declaration_by_binding_`
`DeclarationFact` owner whose `automatic_storage` is true and whose scope is a
block. Namespace/class owners and static locals therefore cannot suppress a
function-body source address. Trivial default construction need not have a
`LifetimeFact`; the declaration owner is the canonical storage-class fact.

The prior typed packed-bit-field boundary is closed. PA11/PA12 own one
`BitFieldFact` per named field keyed by `BindingId`; PA15 consumes its declared,
storage, promoted-operation, signedness, offset, width, and mask facts through
the shared load/encode/RMW-store helpers. The prior checkpoint preserved
one-evaluation and sign-extension invariants, landed `202/243`, and is not
reopened here.

## Failure Map

Checkpoint-start authority is `202/243` passed, `41` failed, and `243/243`
covered. The complete current residual set is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-member-object-lifetime.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-aggregate-brace.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
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
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The chosen cluster is exactly these four residuals: `300-operator-shift-
stress-chain`, `300-mixed-member-free-shift-stress-chain`,
`300-compound-assignment-adl-nonmember-after-member-reject`, and
`300-member-vs-nonmember-operator-implicit-object-cv-rank`. Their canonical
diffs show the selected/lowered calls already agree and only the declaration-
time address of `Token token`, `boost::Cstring s`, or `Period period` is absent.
Focused start was `0/4`; after the local focused run the chosen cluster is
`4/4` and its focused delta is `+4`. The authorized broad run is `206/243`
passed, `37` failed, and `243/243` covered. The exact sorted final residual
set is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-member-object-lifetime.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-aggregate-brace.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-packed-class-layout.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The sorted identity delta from the complete 41-entry checkpoint baseline is
baseline-only: the four chosen identities above; final-only: none. Thus the
full delta is `+4` passed identities, with no coverage reduction. This exact
`243/243` status/output comparison establishes the progress condition; no new
external `stageProgress` run is claimed.

## Active Checkpoint

Implement only the typed PA15 materialization repair in
`dev/src/pa15_lowering_construction.cpp` and `dev/src/pa15_lowering_flow.cpp`:
the no-op `ConstructorAction` branch emits one address for a typed class object
in each declaration consumer. The existing childless class-declaration branch
is the public LowIR shape to match. The existing narrow class-value constructor
path in `dev/src/pa15_lowering_calls.cpp` consumes the canonical variable fact
and the keyed typed declaration owner to omit only its redundant pre-copy source
address when that automatic-local declaration-owned address is present; its
later source address and class-value boundary remain unchanged. This preserves
one semantic owner and adds O(1) work per affected declaration, with no
scan/cache/text recovery or eager constructor/destructor helper emission.

The stable boundary is the selected `ConstructorAction` fact plus
`constructor_action_is_noop`, `storage_for`, `class_object_type`, and
`address_of_storage`. Exclusions are the other 37 residual identities, the
closed bit-field boundary, semantic-fact redesign, source-text/type recovery,
fixture or reference edits, and any change to nontrivial constructor, lifetime,
or helper-demand behavior.

This aligns with the PA16 implicit-default-constructor/local-lifetime contract,
spec sections 3 and 5 (selected actions and typed lowering facts flow from
their semantic owners), section 4 (ordinary work remains O(n)), and section 7
(record structural evidence without unsupported timing claims).

## Performance Evidence

The repair adds one typed predicate branch and at most one `IK_ADDR` per
affected declaration. The class-value overlap check uses the `std::map`-
backed `variable_facts_` lookup in O(log V), the keyed declaration owner
lookup in O(log V), and bounded typed checks after those lookups; it is not
O(1) call-side work, but remains within the spec's O(n log n) ordinary-work
ceiling. It performs no scan, cache, text recovery, or per-operator work, so
the two shift chains retain linear instruction growth in
their source operations and gain only one bounded declaration action each.
The focused local check passed `11/11`, including the four chosen identities,
the requested unary-address-of and overloaded-comma controls, the fixed nested
class-value regression, and five additional constructor/class controls. Fresh
`.check` LowIR main-function
counts match their references: the 24-link chain is `76` instructions with
`25` `Token` addresses; the 256-link mixed chain is `412` with `129`; the
compound/ADL case is `19` with `2` `s` addresses; and the cv-ranking case is
`14` with `2` `period` addresses. Each count includes one bounded
declaration-time address plus the source-operation addresses; there is no
per-link declaration work. Closest PA15 controls
`100-condition-declaration-variable-rvalue` and `100-local-arith` also passed
`2/2`. A temporary namespace/static class-value probe was accepted by the
narrow ABI and emitted both `addr @token` and `addr @static_token` source
operations in `main`, confirming that neither non-automatic owner qualified
for suppression; it was not checked in. The typed-demand course script exited
`0`. The structural count command used only `.check` main-block lines and
named `addr` lines; it reports no timing, RSS, allocation, or speedup claim.

## Checkpoint Ledger

| checkpoint | PA16 identities / coverage | delta from baseline | focused evidence | prior-through / audit | commit |
| --- | --- | --- | --- | --- | --- |
| `1694bc3e` baseline | `200/243`, `43` failures, `243/243` covered | baseline | six bit-field identities `0/6` | inherited through-PA15 `1167/1167`; prior audit passed with five known header warnings | existing HEAD |
| `7e060b28` typed packed bit-field boundary | fresh `202/243`, `41` failures, `243/243` covered | `+2` identities from the retained `43`-failure landed baseline; baseline-only exactly constructor-member-init and member-access; final-only empty | landed focused evidence is six-test `2/6` plus a non-equivalent `7/11`; fresh audit selection is explicitly separate at `6/11`; course regression exit 0 | fresh through-PA15 `1167/1167`, file audit exit 0 with five known warnings, exact current-authority comparison `41 -> 41` with no set delta, diff-check exit 0 | landed source `7e060b28`; audit commit at current HEAD (handoff hash) |
| `d95a6fe7` checkpoint start | `202/243`, `41` failures, `243/243` covered | current authority | chosen four `0/4` | prior through-PA15 and file-audit evidence retained below | current HEAD |
| typed local-class materialization checkpoint | `206/243`, `37` failures, `243/243` covered | `+4` passed identities; baseline-only exactly the four chosen identities; final-only empty | focused PA16 `11/11`; nested class-value regression included; four chosen `4/4`; PA15 controls `2/2`; global/static probe accepted; course script exit 0; 24-link `76/25`, mixed `412/129`, short cases `19/2` and `14/2` | `make test-report-through-pa15` `1167/1167`; file audit exit 0 with five known header warnings; `git diff --check` exit 0; exact `41 -> 37` comparison covered `243/243` with no final-only identity | amended commit; final hash in post-commit handoff |

Durable landed evidence retained from the source checkpoint:

- Full PA16: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-final-rerun.log` (`make test-pa16`, exit 2; `202/243`).
- Sorted identity comparison: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-identity-delta-final.txt` (43 baseline failures, 41 final failures, two baseline-only, final-only empty).
- Focused cluster/matrix/course checks: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-focused-final.log` (landed six-test `2/6`, non-equivalent 11-test `7/11`, course exit 0, diff-check exit 0).
- Prior-stage gate: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-through-pa15-final.log` (exit 0, `1167/1167`).
- File audit: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-audit-final.log` (exit 0; five pre-existing header-division warnings, no fatals).
- Performance evidence: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-compact-worker-performance-final.log` (size probe exit 0; `LoweredValue` 336 bytes, handle 8 bytes, descriptor 352 bytes, one append site, source bounds recorded).

Earlier bounded audit evidence retained from the pre-owner-guard review:

- Focused cluster/matrix/course checks: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-preauth-focused-20260829.log` (fresh six-test `2/6`, explicitly listed different 11-test selection `6/11`, course exit `0`). The fresh selection adds the known unrelated `300-packed-class-layout` residual; it is not a regression and does not replace the landed `7/11` evidence or the full `243/243` authority.
- Fresh full PA16: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-test-20260829.log` (`make test-pa16`, exit `2`; `202/243`, `41` failures, `243/243` covered).
- Fresh sorted identity comparison: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-identity-comparison-20260829.log` (supplied current authority `41` versus fresh `41`, baseline-only `0`, final-only `0`, `243/243` both; retained landed comparison remains the separate `43 -> 41` log above).
- Fresh prior-stage gate: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-through-pa15-20260829.log` (exit `0`, `1167/1167`).
- Fresh file audit: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-file-audit-20260829.log` (exit `0`; five known header-division warnings, no fatals).
- Fresh structural/performance evidence: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-performance-20260829.log` (size probe `336/8/352`, one projection append site, source sizes, and exact 240-line aggregate brace span; no timing claim).
- Fresh diff check: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-final-diff-check-20260829.log` (exit `0`).
- Structural size and append-site observations: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-audit-preauth-structure-20260829.log` (`LoweredValue` 336 bytes, handle 8 bytes, descriptor 352 bytes, one append site). It does not contain constructor/evaluated-root traces.

Current owner-audit validation: `make test-pa16` exited `2` with `206/243`
passed, `37` failures, and `243/243` covered. The exact sorted comparison
against the 41-entry checkpoint-start map is baseline-only precisely the four
chosen identities and final-only empty. The requested focused PA16 controls
passed `11/11`, PA15 controls passed `2/2`, the typed-demand course script
exited `0`, and the namespace/static temporary probe compiled with both
non-automatic source addresses retained. The exact through-PA15 command passed
`1167/1167`; the file audit exited `0` with five known header warnings; and
`git diff --check` exited `0`.

The authoritative current-stage log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` and
the progress failure log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/last-stageProgress.log`.

Next-checkpoint boundary: select one future residual identity separately. Do
not reopen this typed local-class materialization boundary or the prior typed
projection path without new evidence; preserve the current 37-identity
authority, canonical `BindingId`/`BitFieldFact` ownership, typed operation
facts, and the evaluated-root/one-evaluation invariants.
