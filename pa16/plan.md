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

Turn-start authority was `231/243` with complete `243/243` identity coverage
and exactly these 12 failures:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
3. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
4. `pa16/tests/general/200-reference-member-class-init.t` [PA12 no viable function]
5. `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
6. `pa16/tests/general/300-friend-function-definition-skip.t`
7. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
8. `pa16/tests/general/300-overloaded-deref-user-assignment.t` [PA12 invalid conversion]
9. `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t` [PA12 ambiguous function call]
10. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
11. `pa16/tests/general/400-signed-bit-field-read.t`
12. `pa16/tests/general/400-signed-enum-bit-field-read.t`

All `243/243` identities remain represented; this checkpoint must not alter
coverage or hide any authority failure.

## Active Checkpoint

Complete the shared typed PA11/PA12 object-call boundary for failures 4, 8,
and 9.  A same-class functional construction in copy initialization must
retain its selected constructor's ordinary lvalue-reference argument binding;
simple assignment must probe the selected `operator=` after an overloaded
unary `operator*` publishes an `Iter&` lvalue result; and a direct derived
member declaration must suppress an imported base candidate with the same
function signature, including static members.  PA11 remains the typed-fact
producer, PA12 remains candidate/viability/ranking owner, and PA15 remains a
typed consumer.  No other PA16 failure owner is in scope.

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
`choose` candidates, selects the late direct `tag63*` result and the imported
non-colliding `tag62*` result, and runs Holder, Iter, scalar, pointer, and
non-convertible overloaded-assignment rejection controls through repository
LowIR tools.  This is structural evidence only; no timing or RSS claim is
made.

## Focused Evidence

The checkpoint-start three targets failed with the listed PA12 diagnostics.
The exact correction and focused evidence is:

```text
make -C dev cppgm++
sh -n cppgm.tests/course/pa16/428-typed-object-call-candidate-boundary-regression.sh
sh cppgm.tests/course/pa16/428-typed-object-call-candidate-boundary-regression.sh
```

These exit `0`; 428 reports `PASS`, including typed publication `64/65`,
LowIR/CY86 runtime success, scalar and pointer builtin assignment controls,
and EXIT_FAILURE for the non-convertible `operator=(int)` pointer-argument
control.  The focused PA16 check below passes `14/14`, and the focused PA15
check passes `4/4`:

```text
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-reference-member-class-init.t tests/general/300-overloaded-deref-user-assignment.t tests/general/300-using-base-static-same-signature-derived-preferred.t tests/general/300-reference-member-same-name-as-class.t tests/general/300-overloaded-unary-deref-base-ref-return.t tests/general/300-subobject-member-deref-after-prefix-decrement.t tests/general/300-using-base-same-signature-derived-preferred.t tests/general/300-value-init-empty-functional-cast-aggregate.t tests/general/200-copy-init-explicit-ctor-overload-refinement.t tests/general/200-static-nonstatic-same-pointer-signature.t tests/general/300-basic-operator-overloads.t tests/general/300-member-binary-operator-eq.t tests/general/300-member-binary-operator-ne-wrapper.t tests/general/300-compound-assignment-adl-nonmember-after-member-reject.t'
make -C pa15 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-scalar-assignment-address-lvalue.t tests/general/200-pointer-compound-assignment-scale.t tests/general/200-prefix-pointer-decrement-reference-argument.t tests/general/200-global-pointer-array-nullptr-init.t'
```

The authorized broad evidence is:

```text
make test-pa16
```

This exits `2` at `234/243`: all 243 identities are enumerated, the repaired
failures 4, 8, and 9 are absent, and the exact residual map is 1, 2, 3, 5,
6, 7, 10, 11, and 12 above.  No new failure identity appears.  The required
through report exits `0` with `1167/1167`:

```text
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
```

`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
with six existing `bad-division` warnings for `abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`,
`pa12_semantic_selection.h`, and `pa15_lowering.h`.  The repeated 428
`sh -n` and runtime checks exit `0`, and `git diff --check` is clean.  The
bounded changed-path audit contains only the three PA12 sources, this plan,
and `cppgm.tests/course/pa16/428-typed-object-call-candidate-boundary-regression.sh`.

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
| PA16 typed object-call boundary | Final `234/243` with residual failures 1, 2, 3, 5, 6, 7, 10, 11, and 12; complete `243/243` identity coverage; through-PA15 `1167/1167`; audit exit `0` with six known warnings; 428 and focused controls pass. Committed in this checkpoint. |
