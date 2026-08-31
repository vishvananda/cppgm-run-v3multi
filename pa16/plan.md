# PA16 typed object-call candidate/result checkpoint

## Stage Design

PA11 publishes canonical function types, reference-qualified parameter facts,
member origins, and using-declaration access views.  PA12 owns the typed
object-call boundary: it normalizes direct/imported member signatures, gathers
reachable candidates, ranks conversions, and publishes selected argument,
binding, result type, and value-category facts.  Its transient signature key is
a non-owning view over a canonical `TypeKey`; it includes parameter TypeIds,
function cv, variadic state, and static/non-static category, while excluding
return type.  Constructor storage actions, overloaded assignment calls, and
static-member calls consume that same selection boundary; PA15 lowers the
resulting typed facts.  The pipeline has one forward path with no reparse,
parallel lookup, or host/reference compiler invocation, following `spec.md`
Purpose and §§1–5,7.

## Failure Map

Turn-start authority was `234/243` with complete `243/243` identity coverage
and exactly these nine residual failures:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
3. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
4. `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
5. `pa16/tests/general/300-friend-function-definition-skip.t`
6. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
7. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
8. `pa16/tests/general/400-signed-bit-field-read.t`
9. `pa16/tests/general/400-signed-enum-bit-field-read.t`

All `243/243` identities remain represented; this checkpoint must not alter
coverage or hide any authority failure.

## Active Checkpoint

Complete the shared typed PA11/PA12 object-call boundary for the three repaired
identities removed from the turn-start map.  Same-class functional construction
in copy initialization retains selected constructor reference arguments,
including parenthesized and multiargument forms; simple assignment probes the
selected `operator=` after an overloaded unary `operator*` publishes an
`Iter&` lvalue result; and direct derived declarations suppress imported base
candidates only within the matching static/non-static signature category.  The
signature key retains parameter TypeIds, function cv, variadic state, and
category while excluding return type.  PA11 remains the typed-fact producer,
PA12 remains candidate/viability/ranking owner, and PA15 remains a typed
consumer.  No other PA16 failure owner is in scope.

## Performance Evidence

For each static/non-static candidate query, the member boundary builds one
direct-signature index and makes one ordered candidate pass.  The non-owning
key hashes/equates the canonical parameter TypeIds, cv, variadic state, and
category, so it avoids per-candidate owning parameter-vector copies; its
expected normalization cost is `O(C*A)`, followed by the existing `O(C*A)`
argument-conversion work for `C` reachable candidates and `A` parameters or
arguments, with `O(D)` transient index entries for `D` direct declarations.
Separate category filtering prevents one collection from erasing the only
candidate in the other.  Regression 428 publishes 64 base and 65 derived
`choose` candidates, selects the late direct `tag63*` result and imported
non-colliding `tag62*` result, and adds static/non-static and const-signature
using-view controls.  It also runs explicit one- and two-reference constructor
actions, a parenthesized wrapper, an lvalue assignment-result check,
scalar/pointer builtin assignment, and non-convertible overloaded-assignment
rejection through repository LowIR tools.  This is structural evidence only;
no timing or RSS claim is made.  A two-argument variadic-member probe reached
PA12 but exposed an unowned PA15 fixed-call-arity limitation, so it is not
passing evidence or an in-scope fix.

## Focused Evidence

The checkpoint-start three targets failed with the listed PA12 diagnostics.
The exact correction and fresh focused evidence is:

```text
make -C dev cppgm++
sh -n cppgm.tests/course/pa16/428-typed-object-call-candidate-boundary-regression.sh
sh cppgm.tests/course/pa16/428-typed-object-call-candidate-boundary-regression.sh
```

These exit `0`; 428 reports `PASS`, including typed publication `64/65`,
LowIR/CY86 runtime success, static/non-static and cv using-view controls,
explicit/reference construction, an assignment lvalue-result check, scalar and
pointer builtin assignment, and EXIT_FAILURE for the non-convertible
`operator=(int)` pointer-argument control.  The focused PA16 check below
passes `14/14`, and the focused PA15 check passes `4/4`:

```text
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-reference-member-class-init.t tests/general/300-overloaded-deref-user-assignment.t tests/general/300-using-base-static-same-signature-derived-preferred.t tests/general/300-reference-member-same-name-as-class.t tests/general/300-overloaded-unary-deref-base-ref-return.t tests/general/300-subobject-member-deref-after-prefix-decrement.t tests/general/300-using-base-same-signature-derived-preferred.t tests/general/300-value-init-empty-functional-cast-aggregate.t tests/general/200-copy-init-explicit-ctor-overload-refinement.t tests/general/200-static-nonstatic-same-pointer-signature.t tests/general/300-basic-operator-overloads.t tests/general/300-member-binary-operator-eq.t tests/general/300-member-binary-operator-ne-wrapper.t tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t'
make -C pa15 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-scalar-assignment-address-lvalue.t tests/general/200-pointer-compound-assignment-scale.t tests/general/200-prefix-pointer-decrement-reference-argument.t tests/general/200-global-pointer-array-nullptr-init.t'
```

The access-view/static/member control set passes `10/10`:

```text
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/100-static-member-qualified-call.t tests/general/100-static-member-object-access.t tests/general/100-static-member-overload-skips-nonstatic-this.t tests/general/200-inherited-static-member-qualified-call.t tests/general/200-const-member-call-prefers-const-object-overload.t tests/general/200-const-object-nonconst-member-call-bad.t tests/general/300-private-base-using-method-call.t tests/general/300-using-declaration-public-private-base-member.t tests/general/300-class-using-declaration-reexposes-protected-field.t tests/general/300-static-const-member-address.t'
```

The final broad validation evidence is:

```text
make test-pa16
```

This exits `2` at `234/243`: all 243 identities are enumerated, the three
repaired identities are absent, and the exact residual map is the nine entries
above.  Compared with the turn-start authority, baseline-only, final-only, and
unrecognized identities are `0/0/0`; coverage remains complete at `243/243`.

```text
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

The exact prior-through command exits `0` with `1167/1167`.  The exact file
audit exits `0` with six known nonfatal `bad-division` warnings for
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
`pa11_semantic_model.h`, `pa12_semantic_selection.h`, and `pa15_lowering.h`.
`git diff --check` is clean; the final bounded path audit and commit contain
only `dev/src/pa12_semantic_construction.cpp`, regression 428, this plan, and
`pa16/audit.md`.

## Next Checkpoint

Begin the next separate residual audit with
`pa16/tests/general/200-local-default-class-array-lifecycle.t`.  The other
eight residual owners listed in the Failure Map remain unclaimed by this
completed audit.

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
| 2cfa1111 | Final committed UDL checkpoint: PA16 231/243, exact 12 failures, 243/243 coverage, through-PA15 1167/1167, and file audit 0 with six known warnings. |
| 4a5bbdd5 | Clean turn-start baseline: PA16 231/243 with complete 243/243 coverage; through-PA15 and file audit passed. |
| `617c137a` typed object-call boundary audit | Completed committed audit/repair: final PA16 is exit `2` at `234/243` with the exact unchanged nine residual identities and `243/243` coverage; baseline-only/final-only/unrecognized comparison is `0/0/0`; focused 428/PA16/PA15/access controls pass; through-PA15 is `1167/1167`; file audit exits `0` with six known header-division warnings. |
