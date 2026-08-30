# PA16 implementation plan

## Stage Design

PA16 preserves one typed PA10 NamePath -> PA11 lookup/type/access -> PA12
ordinary-plus-ADL -> PA15 lowering pipeline.  This checkpoint owns
source-point-aware visibility of effective using-directive relations and typed
publication of the parser’s ambiguous one-argument call statement into the
existing narrow PA15 empty-class by-value bridge.  The typed
`xxx::nested::aaa` path was already correct and is not claimed as repaired.
The design follows `spec.md` §§1--5 and 7: canonical typed identities,
source-point-aware lookup, deterministic bounded traversal, typed LowIR, and
no textual downgrade, retry, second lookup engine, host/reference shortcut,
global scan, or broad class-value support.

## Exact Failure Map and Authority

Turn-start authority is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-
xhigh/last-test.log`: `218/243` passed, exactly `25` failures, and `243/243`
test identities covered.  The exact 25-name failure map is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Coverage is an identity invariant: discovered `243`, reference sidecars
`243`, fresh sidecars `243`, missing reference/fresh `0/0`.

## Active Checkpoint: Effective Using Visibility and Typed Call Publication

The primary test initially failed with `ERROR: unknown PA11 type name`.  The
typed `xxx::nested::aaa` path itself was already forming the canonical
`TypeId`/`NamedRecordId`; the failure was ordinary lookup while recovering the
parser’s ambiguous single-argument call statement.  A local `using namespace
nnn` is stored on the effective namespace ancestor, but
`append_effective_using_targets` incorrectly applied the function source point
against that ancestor.  The fix checks the relation’s typed lexical owner.

`EffectiveUsingDirective` carries `target`, `lexical_scope`, and the original
`declaration_point`; `process_using_directive` records it at the common
ancestor.  Lookup first requires `lexical_scope_is_applicable`, formed from
the actual lexical start and its ancestors.  It then calls
`relation_visible_at(directive.lexical_scope, ...)`: block/function lexical
relations retain the existing local formation/order behavior, while a
namespace lexical owner—including the implicit unnamed-namespace relation—
continues to require `declaration_point <= function source point`.  This
existing invariant is sufficient; no additional relation mutation was needed.

The recovered call then uses the same typed PA12 class-value eligibility
predicate as ordinary calls.  It publishes one `ClassValue` conversion fact
only for the existing narrow slice: one nonvariadic namespace-owned function,
one matching empty class by-value parameter, an lvalue object of the same
canonical object type, and no class result.  PA15 continues to validate and
lower that fact through its existing opaque object bridge.

Data flow is `NamePath` components -> typed PA11 lookup and source-point
filtering -> ordinary using lookup -> `resolve_single_argument_function` and
the shared narrow typed `ClassValue` predicate -> PA15 call lowering.  Normal
ADL remains typed and uses only the first enclosing namespace plus inline
closure; it does not climb ordinary namespace parents or traverse
using-directives.
The checkpoint excludes unrelated residual LowIR/lifetime/operator failures,
general class pass-by-value/return support, and any new lookup or lowering
engine.

Complexity risk is bounded: the repair changes one existing effective-edge
visibility check; the shared class-value predicate performs constant-time
metadata checks plus existing typed record/layout predicates.  No
program-wide declaration scan or retry was added.

## Performance Evidence

Structural evidence only; no unsupported timing claim is made.  Qualified
resolution visits the typed qualifier components and the existing source-
visible lookup graph.  Effective using edges are considered only from marked
lexical scopes, and the existing lookup-generation marks bound graph cycles.
PA12 association remains bounded by typed wrappers, validated records/bases,
and first-enclosing namespaces.  Candidate identity remains canonical and
deduplicated in the existing bounded vectors.

## Focused and Final Validation Evidence

Focused validation after the final rebuilt source:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- Primary typed `--emit-types` and `--emit-semantics`: status `0`/`0`; the
  semantic call callee is `boost_no_adl_barrier::nnn::begin`.
- Primary `300-adl-associated-namespace-does-not-climb-parents.t`: direct
  LowIR emission status `0`; emitted call target is
  `boost_no_adl_barrier__nnn__begin`.
- PA16 controls, status `0`, `PASS (16/16)`: primary
  `300-adl-associated-namespace-does-not-climb-parents.t`; qualified-type
  positives `200-qualified-inherited-member-typedef.t`,
  `200-inherited-base-typedefs-in-derived-members.t`,
  `300-lazy-nested-class-enclosing-alias-lookup.t`, and
  `spec/100-decltype-qualified-nested-type-local.t`; valid namespace using
  `100-using-directive-imported-value-method-body.t`; later source-point
  negative `300-adl-using-declaration-source-point.t`; suppression/union and
  hidden-friend controls `200-implicit-member-call-suppresses-adl.t`,
  `300-hidden-friend-definition-adl-call.t`,
  `300-enum-operator-adl-selects-matching-overload.t`,
  `spec/300-operator-lookup-ordinary-adl-union.t`,
  `spec/300-lazy-class-lookup-ignores-later-using-directive.t`,
  `spec/300-hidden-friend-not-visible-to-unrelated-adl.t`, and
  `spec/300-hidden-friend-not-visible-to-qualified-lookup.t`; qualified
  access negatives `200-qualified-friend-not-hidden-adl-bad.t` and
  `300-qualified-friend-function-access.t`.
- PA12 ordinary using/inline controls, including valid block-local
  `200-local-using-directive-preserves-nearer-namespace-type.t`: status `0`,
  `PASS (3/3)`.
- Existing course `426-typed-adl-inline-namespace-regression.sh`: status `0`,
  `PASS`; shell syntax check status `0`.

No new course test was added: the checked-in authority case directly exercises
the owner contract, and the focused controls protect its boundaries.

Final validation:

- `make test-pa16`: status `2`, `219/243` passed, `24` failures.
- Exact sorted failure comparison against
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
  authority `25`, fresh `24`, authority-only `1` (the primary
  `300-adl-associated-namespace-does-not-climb-parents.t`), fresh-only `0`.
- Coverage inventory: `243` discovered tests, `243` reference status sidecars,
  `243` fresh status sidecars, missing reference/fresh `0/0`.
- `n=16` through-PA15 gate: status `0`, `1167/1167`.
- PA16 file audit: status `0`, five pre-existing `bad-division` warnings, no
  fatal findings.
- Final `git diff --check`: status `0`; bounded changed-path/coverage audit:
  status `0`, exactly the five approved paths, no handout/reference diff, and
  coverage `243/243/243`.

## Checkpoint Ledger

| checkpoint | result | status |
| --- | --- | --- |
| `1694bc3e` bit-field baseline | `200/243`, 43 failures, `243/243` covered | prior landed |
| `7e060b28` packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior landed |
| `d95a6fe7` local-class start | `202/243`, 41 failures, `243/243` covered | prior checkpoint |
| `d83e927f` local-class materialization | `206/243`, 37 failures, `243/243` covered | prior landed |
| `70327e4d` exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | prior landed |
| `ee8f44d5` typed array cleanup | `209/243`, 34 failures, `243/243` covered | prior landed |
| `9f7101ac` pack-layout audit | `210/243`, 33 failures, `243/243` covered | prior landed |
| `fb4f46ed` placement-new semantic/lowering | `211/243`, 32 failures, `243/243` covered | prior landed |
| typed truth-width continuity | `214/243`, 29 failures, `243/243` covered | landed |
| typed packed-bit-field value/update audit | `215/243`, 28 failures, `243/243` covered | completed audit |
| alias direct-base mem-initializer | `216/243`, 27 failures, `243/243` covered | completed |
| compact zero-store aggregate value-init | `217/243`, 26 failures, `243/243` covered | completed audit |
| `ab1b2a8c` source-point-aware associated ADL | `218/243`, 25 failures, `243/243` covered; focused ADL matrix `11/12` with only the known nested-enum residual | completed audit |
| current effective-using visibility and typed call-publication checkpoint | final `219/243`, `24` failures, exact authority-only primary delta, fresh-only `0`, coverage `243/243/243`; through-PA15 `1167/1167`; audit passes with five warnings; focused PA16 `16/16`, PA12 `3/3`, course 426 pass; diff/path checks `0` | validated, committed |
