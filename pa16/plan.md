# PA16 typed constructor-overload checkpoint

## Stage Design

PA11 owns typed AST/model identities.  PA12 owns relevant lookup, direct and
user-defined conversion choices and scores, constructor selection, and typed
conversion/action facts.  PA15 consumes those selected declarations and calls
and lowers the single typed model to LowIR.  The PA16 boundary here is ordinary
overload resolution, constructors, converting constructors, external
declarations, and the supported class subset.  Class value semantics,
copy/move transfer, and pass-by-value class objects remain PA17 scope.

The owner path is:

    AST arguments -> ExprInfo and semantic facts
      -> PA12 relevant candidate set
      -> per-argument ConversionChoice and ConversionScore
      -> selected declaration/constructor and target-aware materialization
      -> ConstructorAction/CallExpression and typed conversion facts
      -> PA15 declaration/call/constructor LowIR lowering

This follows the `spec.md` one-owner and typed-fact-continuity requirements:
lookup, selected conversions, and constructor materialization stay at the PA12
semantic owner, while PA15 consumes typed declarations/calls without textual
recovery or a duplicate model.  The checkpoint's indexed candidate work is
bounded by relevant overloads, arguments, and inheritance depth.

## Baseline Authority and Failure Map

The pre-landed checkpoint was `225/243` with exactly 18 failures.  Its
discovered/reference/fresh inventory was `243/243/243`; through PA15 was
`1167/1167`; and the file audit passed with five pre-existing bad-division
warnings.  The turn-start authority supplied for landed `d5bf2600` is
`make test-pa16` status `2`, `227/243`, exactly 16 failures, and complete
`243/243/243` coverage.  The two identities removed from the prior 18-item
set are exactly:

```text
pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
```

The exact residual budget, unchanged by this bounded audit, is:

```text
pa16/tests/general/200-elaborated-member-forward-type.t
pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-callable-field-hides-private-base-method.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The final serial rerun retains exactly this supplied 16-item identity set:
authority-only `0`, fresh-only `0`, and `16` on each side.  Discovered tests,
reference status sidecars, and fresh status sidecars are each `243`.  The
through-stage gate is `1167/1167`; the final file audit passes with six exact
non-fatal warnings recorded below.  No residual identity is in this audit's
repair scope.

## Active Checkpoint

The evidence-backed defect was that shared ordinary-call scoring did not reuse
the existing typed converting-constructor viability path for a class-reference
parameter.  A candidate's argument loop is correctly stopped once any
argument lacks an implicit conversion sequence; the outer loop still considers
the remaining relevant candidates.  The invariant is that every still-viable
candidate is checked argument-by-argument, a candidate is rejected once any
argument lacks an ICS, and all relevant candidates remain considered.

The checkpoint adds one shared implicit-constructor conversion owner.  It
validates the target and source facts, considers relevant constructors while
ignoring access/deleted status during ICS formation, honors defaulted trailing
parameters, and scores the constructor's first parameter.  It returns a valid
`UserDefined` result whenever at least one best constructor sequence exists,
including an ambiguous constructor-level sequence.  Its dedicated ephemeral
`ImplicitConstructorConversion` carries the target-scoped constructor binding,
the second standard sequence, and ambiguity state; only the compact typed
`ConversionScore` enters per-candidate ranking, while general
`ConversionChoice` remains unchanged.  N3485 13.3.3.2 p3 compares that second
sequence only when both outer candidates use the same constructor and target;
different constructors and ambiguous constructor-level sequences remain
indistinguishable.  The selected call then re-enters target-aware semantic
construction, where the canonical selector diagnoses ambiguity, access, or
deletion if that outer candidate wins.

For the external Box case, both three-argument overloads are considered:
Box(int,const Token&,int) rejects its second argument, while the declared
Box(int,const char*,int) remains viable and lowers as the selected external
function declaration/call.  Generic array-to-pointer conversion now also
publishes the typed constant-address fact needed by PA15 for a string literal
returned from Source::c_str().

For the library case, the mutable void* overload remains nonviable because
discarding the string literal's const qualification is not permitted.  The
const path& overload obtains one user-defined sequence through path(const
char*), then binds the temporary to the const reference.  The separate
const-void-pointer initializer records the two standard edges
array-to-pointer and pointer-to-void.  No class-value, copy, or move semantics
were added.  The array fact remains the PA15 lowering root, while the returned
ExprInfo now has the target pointer type and prvalue category.

N3485 13.3.1.3, 13.3.2, 13.3.3, 13.3.3.1/.1.2 p2/p10, 13.3.3.2 p3, and
12.3.1 are the
governing overload, viability, implicit-conversion-sequence, and
converting-constructor rules; the implementation follows the typed PA12-to-
PA15 boundary described above.  Accessibility is not used to form the ICS;
the selected declaration is checked afterward.

## Validation

The final correction rebuilt cleanly:

    make -C dev cppgm++
    status 0

The two resolved tests plus five focused constructor/external controls passed:

    make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-external-ctor-overload-nonfirst-argument.t tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t tests/general/200-constructor-overload-default-arg-nonfirst-argument.t tests/general/200-copy-init-explicit-ctor-overload-refinement.t tests/general/500-inheriting-external-transitive-constructor.t tests/general/500-inheriting-constructors.t tests/general/500-inherited-constructor-using-access.t'
    pa16 check: PASS (7/7)

The disposable layout probe reports `ConversionChoice` parent/current
`56/56` bytes and `ConversionScore` `20/40` bytes.  The score increase is the
typed constructor/target identity and ambiguity/rvalue markers; its existing
standard payload stores the second standard sequence.  No
`user_defined_second_ambiguous` metadata remains, and no timing or RSS claim
is made.

The disposable probes `/tmp/pa16-outer-ambiguous-udc-probe.cpp`,
`/tmp/pa16-ctor-access-discriminating-probe.cpp`,
`/tmp/pa16-ctor-deleted-discriminating-probe.cpp`,
`/tmp/pa16-ctor-ambiguity-probe.cpp`,
`/tmp/pa16-inherited-implicit-probe.cpp`,
`/tmp/pa16-inherited-implicit-after-direct-probe.cpp`,
`/tmp/pa16-inherited-default-implicit-probe.cpp`,
`/tmp/pa16-inherited-explicit-derived-hide-probe.cpp`,
`/tmp/pa16-inherited-explicit-implicit-probe.cpp`,
`/tmp/pa16-inherited-deleted-implicit-probe.cpp`,
`/tmp/pa16-inherited-private-implicit-probe.cpp`,
`/tmp/pa16-ctor-nonconst-reference-probe.cpp`,
`/tmp/pa16-ctor-class-value-probe.cpp`,
`/tmp/pa16-operator-constructor-probe.cpp`, and
`/tmp/pa16-same-constructor-second-scs-probe.cpp` retained their expected
statuses.  The outer ambiguity probe is status `1`,
`ERROR: PA12 ambiguous function call`; private-best/public-worse is status
`1`, `ERROR: PA12 constructor is inaccessible`; deleted-best/public-worse is
status `1`, `ERROR: PA12 call selects deleted function`; the single-target
ambiguity probe is status `1`, `ERROR: PA12 ambiguous function call`.  The
three inherited positive probes and the operator probe are status `0`; the
inherited explicit/hidden/deleted/private controls are status `1` with
post-selection/no-viable diagnostics.  The non-const-reference control is
status `1`, `ERROR: PA12 no viable function`, and the class-value control is
status `1`, `ERROR: PA11 unsupported class-value constructor boundary`.
The same-constructor probe is status `0` and its LowIR selects
`call i32 @f__2(%t1)`, object `_Z1fO1A` (`A&&`), over `_Z1fRK1A`.

The final serial broad validation is:

    make test-pa16
    status 2; TEST SUMMARY: 227 / 243 TESTS PASSED
    exact authority/fresh residuals: 16/16, authority-only 0, fresh-only 0
    discovered/reference/fresh coverage: 243/243/243

    n=16; ... make test-report-through-pa$((n - 1))
    status 0; ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)

    perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
    status 0; six non-fatal warnings, each:
      [warning][bad-division] dev/src/<header>:1: header contains substantial
      implementation body; prefer .cpp ownership
      headers: abi_mangle.h, cpp_semantic_core.h, lowir_model.h,
        pa11_semantic_model.h, pa12_semantic_selection.h, pa15_lowering.h

`git diff --check` exits `0`.  The exact path audit contains only the five
modified implementation/source-set paths, the two new PA12 source/header
paths, and `pa16/audit.md` plus `pa16/plan.md`; the new implementation is
registered only for `cppgm++`.  No forbidden test/reference/fixture/sidecar,
harness, comparator, generated-output, or coverage artifact changed.

The PA16 residual map is exactly the supplied 16-item map above.  No tests,
fixtures, exit-status sidecars, references, harnesses, comparators, generated
outputs, or coverage rules changed; the cppgm++ source set was updated only to
register `pa12_semantic_constructor_candidates.cpp`.

## Performance Evidence

These are structural bounds and representative counts, not timing or RSS
measurements:

- Box evaluates at most 2 relevant constructor candidates across 3 explicit
  arguments.  The Token overload rejects at its second argument; the outer
  candidate loop still evaluates the declared const-char overload.  Ordinary
  candidate scoring is O(C*A) conversion-choice work, stopping each candidate
  at its first failed argument while retaining all candidate identities.
- library evaluates 2 relevant outer constructor candidates for 1 argument;
  the const path& candidate performs one relevant path-constructor probe and
  the mutable void* candidate fails its standard conversion.  The accepted
  const-void initializer has two typed standard-conversion edges.  The pure
  constructor view is expected/amortized O(D + I + R^2) per probe for `D`
  direct signatures, `I` inherited identities, and bounded inheritance depth
  `R`; the R^2 term is only the current vector active-stack cycle guard, while
  every inherited/direct hiding decision is one lookup in a contiguous typed
  flat index rather than a D-way scan.
- Source::c_str() publishes one ArrayToPointer constant-address fact, which
  PA15 consumes directly.  Source-to-slot metadata is captured once per owned
  slot with at most an O(log B) declaration-map lookup; stable `SlotId`
  allocation is preserved while only the source-anchored presentation vector
  is reordered.

Selected materialization is expected/amortized O(D + I + W + R^2) for `D`
direct signatures, `I` inherited identities, `W` generated wrapper arities,
and relation depth `R`; its interned full-function-type index performs one
lookup per wrapper arity rather than a direct-constructor scan.  Across `C`
outer candidates and `A` arguments, a constructor-based probe therefore has the
structural worst-case O(C*A*(D + I + R^2)) candidate-work bound, with the
per-candidate first-impossible-argument break still active.  Representative
hiding inputs are D=1, I=1, and one lookup for the explicit-derived-hide
probe; the default inherited probe has one relation, one base constructor, and
two wrapper arities.  The final disposable size probe reports parent/current
`ConversionChoice` `56/56` bytes and parent/current `ConversionScore` `20/40`
bytes.  `ConversionChoice` remains unchanged; `ConversionScore` reuses its
standard payload for the second SCS and carries only the typed UDC identity and
markers needed for p3.  These are structural bounds, sizes, and candidate
counts, not timing or RSS measurements.

## Next Checkpoint

This d5bf2600 checkpoint is complete.  The next separate residual checkpoint
is `pa16/tests/general/200-elaborated-member-forward-type.t`; it must preserve
the `227/243` boundary and `243/243/243` coverage while leaving the other 15
residual identities outside scope.  The constructor-overload ownership path
and its final audit record are not reopened without new evidence.

## Checkpoint Ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Prior PA16 authority was 224/243 with exactly 19 failures and 243/243 identity coverage. |
| b58ddd2a | Completed the typed nullptr_t carrier path through PA11/PA12/PA15, including ABI and LowIR ownership and the bounded endpoint audit. |
| e09d8223 | Recorded the completed nullptr carrier audit state: 225/243 authority, exact 18-item residual map, 243/243/243 inventories, through-PA15 1167/1167, and passing file audit with five known warnings. |
| d5bf2600 checkpointAudit | Completed the bounded typed ownership audit and PA12 repair for constructor-overload viability/materialization, inherited candidate visibility, declaration hiding, access/deleted post-selection diagnosis, ambiguous UDC retention, and same-constructor second-SCS ranking. The inheriting-constructor publisher is extracted to the registered 215-line `pa12_semantic_constructor_candidates.cpp`; constructor/lifetime facts are in the normal 89-line `pa12_semantic_constructor_facts.h`, `pa12_semantic_construction.cpp` is 1876 lines, and `pa11_semantic_model.h` is 2374 lines. The final size probe is `ConversionChoice` 56/56 and `ConversionScore` 20/40 parent/current bytes. Focused build/matrix pass 7/7; all discriminating probes pass their expected statuses; final PA16 is 227/243 with exactly the unchanged 16 failures, exact authority/fresh identity sets 16/16 with zero deltas, and 243/243/243 coverage. Through-PA15 is 1167/1167; file audit exits 0 with six exact warnings; diff-check and changed-path/artifact checks pass. No handout/test/reference mutation; next is the separate residual `200-elaborated-member-forward-type.t`. |
