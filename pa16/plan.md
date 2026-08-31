# PA16 typed user-defined-literal checkpoint

## Stage Design

The posttoken collector owns decoded `UserDefinedLiteralData` (kind, suffix,
prefix/value).  PA10 transfers each occurrence once into a sparse typed
sidecar range; source spelling remains presentation text only.  PA11 owns
literal-operator declaration identity, namespace-scope ownership, C++11
non-template parameter legality, and suffix matching through the shared
namespace/value indexes and ordinary using-directive lookup.  PA12 forms the
cooked string array/size arguments, admits only the canonical non-variadic
`const decoded-element*`/`unsigned long` signature, filters legal other-form
same-suffix entries, applies normal typed call selection and conversions, and records
the selected call.  PA15 consumes that call and its conversion/address facts
through existing typed call and literal-storage lowering.  No suffix/value
reparsing, parallel semantic path, retained whole-program scan, or
host/reference invocation is introduced.  This follows
`spec.md` Purpose and §§1–5,7: one forward pipeline, typed fact continuity,
canonical owners, demand-driven lookup, bounded work, and an auditable
typed-to-emission path.

## Failure Map

Turn-start authority was 231/243 with complete 243/243 coverage and exactly
these 12 failures:

- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The final authority and fresh runs both cover `243/243` identities.  The
normalized failure map is unchanged: all 12 entries above remain outside this
checkpoint, with no fresh-only or authority-only identity.

## Active Checkpoint

Audit and complete the C++11 cooked string UDL expression pipeline for
`operator ""_pick(const char*, size_t)`, including block `using namespace`
visibility, suffix-aware merging in the shared literal-operator lookup bucket,
canonical signature/owner validation, malformed-declaration rejection, legal
other-form filtering, normal selection/conversion facts,
literal storage/address bytes, and ABI suffix emission.  The bounded repair
separates ordinary source names from the internal lookup bucket and filters
before scope shadowing.  Regression 427 checks `_other`, 64 visible
`_slot00`--`_slot63` suffixes, a late `_slot63` selection, early-shadow
fallback, legal-other-form filtering, malformed declaration rejection,
ordinary `operatorliteral` collision,
and class/function-scope rejection.

## Performance Evidence

The implementation performs O(1) typed sidecar transfer per UDL, reachable
scope-graph plus visible-candidate work for suffix filtering, expected O(C)
candidate deduplication, and existing candidate-by-argument selection.  It
adds no persistent transitive closure or whole-program retry.  Regression 427
generates 64 declaration/definition pairs, observes 64 unique `_slot` ABI
symbols, selects late `_slot63`, and executes the early-shadow,
legal-other-form, malformed-declaration, scope-negative, and bucket-collision
controls.  This is
structural bounded-work evidence only; no timing or RSS claim is made.

## Focused Evidence

The final focused build and checks pass:

```text
git diff --check
sh -n cppgm.tests/course/pa16/427-typed-user-defined-literal-boundary-regression.sh
make -C dev cppgm++
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST=tests/general/300-user-defined-string-literal-operator.t
make -C pa10 CPPGM_SKIP_DEV_REBUILD=1 check TEST=tests/general/200-literal-operator-id.t
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST=tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
sh cppgm.tests/course/pa16/427-typed-user-defined-literal-boundary-regression.sh
```

Results are respectively `exit 0`, `exit 0`, `exit 0`, `PASS (1/1)`,
`PASS (1/1)`, `PASS (1/1)`, and `PASS`.  Full PA16 exits `2` at `231/243`
with the exact 12-item authority map and `243/243` coverage.  Normalized
comparison is `12/12`, fresh-only/authority-only `0/0`, inventories
`243/243/243`; through-PA15 is `1167/1167`.  The final file audit exits `0`
with six known nonfatal `bad-division` warnings, and diff/path audits pass.

## Checkpoint Ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Authority was 224/243 with 19 failures and complete identity coverage. |
| b58ddd2a | Typed `nullptr_t` carrier path completed through PA11/PA12/PA15. |
| e09d8223 | Recorded nullptr state: 225/243 authority, 18 residual failures, 243/243 inventories. |
| d5bf2600 | Typed constructor-overload/lifetime audit reached 227/243 with 16 residual failures. |
| 29d9c4ce | PA10 elaborated-member parameter repair/PA15 ABI ownership reached 228/243 with 15 residuals. |
| 69bbe800 | Empty-base layout/address projection reached 230/243 with 13 residuals. |
| 75f7944a | Empty-base identity validation audit completed; through-PA15 remained 1167/1167. |
| 2ca2323a | Clean turn-start state; baseline 230/243 and exact 13-item map. |
| 2cfa1111 | Final committed checkpoint audit: suffix-aware lookup and ordinary/internal separation, canonical PA11 namespace/linkage and non-template parameter validation, PA12 legal-other-form filtering, durable 427 malformed-declaration controls, PA16 `231/243` with exact unchanged 12 failures and `243/243` coverage, authority/fresh `12/12` with fresh-only/authority-only `0/0`, discovered/reference/fresh `243/243/243`, through-PA15 `1167/1167`, and file audit `0` with six known warnings. |
