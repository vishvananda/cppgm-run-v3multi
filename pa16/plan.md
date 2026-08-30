# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10/PA11 syntax and names become PA12
typed semantic facts and conversion facts, and PA15 lowers those facts into
LowIR.  The packed-field path carries one PA12 `BitFieldFact` with declared,
storage, operation, width, mask, and signedness data into PA15's
`LoweredValue { BindingId, ProjectionId }`; member reads, encodes, stores,
inc/dec, and aggregate initialization consume that same typed identity.  This
checkpoint adds no text transport, parallel analyzer, rescan/cache, retry, or
second lowerer.  Comparisons retain their PA12 operation type and canonical
truth; explicit conversions own their destination type.

## Failure Map

Turn-start authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`214/243` passed, exactly `29` failed, and `243/243` identities are covered.
The complete turn-start failure map is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
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
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The parent checkpoint had already recovered `200-nonliteral-field-condition-
not-folded.t`, `200-placement-new-expression-constructor-call.t`, and
`300-pragma-pack-followed-by-endif.t`; they are absent from this turn-start
authority.  Final validation is an exact strict subset: the final failure set
is this 29-item set minus `400-bitfield-aggregate-init.t`, with no new
identity; the independent inventory and run total remain `243`, and coverage
is `243/243`.

## Active Checkpoint

This checkpoint repairs the typed packed-bit-field value/update boundary.
`process_bit_field_declaration` now distinguishes the selected
implementation-defined plain-`int` bit-field representation from an explicit
`signed` declaration using PA10 specifier tokens.  Explicitly signed integral
fields and signed-underlying enum fields remain sign-preserving; the narrow
plain-`int` convention recovers `400-bitfield-aggregate-init.t` without
changing the PA12 promoted `operation_type`.

PA15 keeps the existing single lowering path: direct aggregate paths capture
the first field projection from the canonical storage root; binary operands
consume the extracted bit-field temporary without a redundant publication
copy, while direct scalar value boundaries retain their explicit copy.
Inc/dec still uses one typed packed-unit encode/RMW-store, the saved
`BindingId`/`ProjectionId`, and the old-value postfix versus updated-value
prefix result category.  The aggregate cursor remains declaration-ordered,
and no source expression is re-evaluated.

The two signed-read references omit the required sign extension and remain
intentionally unresolved.  The prefix reference also requests `cmp eq u32`
for values whose PA12 [conv.prom] operation type is `int`; using that carrier
with the emitted `i32` operands is rejected by `lowir2cy86`.  Reproducing
either sequence would violate the typed/signed contract, so these tensions
are recorded for supervisor steering rather than hidden with fixture-specific
branching.

## Performance Evidence

The fact/projection lookups remain O(1) per typed bit-field operation, with a
contiguous projection arena and bounded width/mask work.  The focused prefix
output has eight field projections, two add operations, two packed-unit update
stores, and no value-publication copies in its comparison operands; the
aggregate output has two field projections, one packed store, one packed load,
and no sign/copy sequence for its plain-`int` convention.  The 412 regression
exercises neighboring-field preservation and assignment without duplicate
source evaluation.  There is no rescan, retry, parallel lowerer, or
fixture-dependent branch.  No timing, RSS, or code-size measurement was taken;
these are structural observations, not measured performance claims.

## Validation

Final validation is complete:

- `make cppgm++ CXX=g++` from `dev/`: status `0`.
- `make -C pa16 check TEST='tests/general/400-bit-field-prefix-postfix-increment.t tests/general/400-bitfield-aggregate-init.t tests/general/400-signed-bit-field-read.t tests/general/400-signed-enum-bit-field-read.t tests/general/300-bit-field-layout-sizeof.t tests/general/300-zero-width-bit-field-layout.t tests/general/400-bit-field-constructor-member-init.t tests/general/400-bit-field-sparse-member-init.t tests/general/400-address-of-bit-field-bad.t tests/general/400-bit-field-member-access-bad.t'`:
  status `2`, `PASS (7/10)`, all 10 identities covered; the aggregate target
  plus six controls pass, while prefix and the two signed-read targets remain.
- `sh cppgm.tests/course/pa16/412-typed-bit-field-initialization-root-regression.sh`:
  status `0`, `PASS`; assignment, packed neighbors, promotions, references,
  address, and `sizeof` restrictions are exercised.
- `sh cppgm.tests/course/pa16/422-typed-pack-wide-bitfield-layout-regression.sh`:
  status `0`, `PASS`.
- `make test-pa16`: status `2`, `215/243` passed, exactly 28 failures, all
  `243/243` identities covered; the failure set is the turn-start set minus
  `400-bitfield-aggregate-init.t`, with no new identity.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`:
  status `0`, `1167/1167` through PA15.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`,
  five known header-division warnings and no fatal issue.
- `git diff --check`: status `0`.

No test, harness, reference, comparator, or coverage surface changed.

## Next Checkpoint

PA16 remains incomplete at `215/243`, with 28 residual failures and
`243/243` identities covered.  The next coherent boundary is the remaining
typed constructor/member-initialization and lifetime semantic flow; future
work should extend that PA12 fact-to-PA15 consumer path.  The signed-read and
prefix oracle tensions remain and require a contract-preserving design.

## Checkpoint Ledger

| checkpoint | result | status |
| --- | --- | --- |
| `1694bc3e` bit-field baseline | `200/243`, 43 failures, `243/243` covered | prior landed |
| `7e060b28` packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior landed |
| `d95a6fe7` local-class start | `202/243`, 41 failures, `243/243` covered | prior checkpoint |
| `d83e927f` local-class materialization | `206/243`, 37 failures, `243/243` covered | prior landed |
| `70327e4d` exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | prior landed |
| `3b2b4882` checkpoint audit baseline | `208/243`, 35 failures, `243/243` covered | prior baseline |
| `ee8f44d5` typed array cleanup | `209/243`, 34 failures, `243/243` covered | prior landed |
| `08472cce` typed pragma-pack layout | prior landed pack-layout checkpoint | prior landed |
| `9f7101ac` pack-layout audit | `210/243`, 33 failures, `243/243` covered | prior landed |
| `fb4f46ed` placement-new semantic/lowering | `211/243`, 32 failures, `243/243` covered | prior landed; historical |
| `85b819b7` pre-increment authority | `211/243`, 32 failures, `243/243` covered | prior baseline |
| `typed truth-width continuity (parent 85b819b7)` | final `214/243`, 29 failures, `243/243` covered; authority-only 3 named identities; fresh-only 0; through-PA15 `1167/1167`; audit 0 with five known warnings; diff-check 0 | landed in this checkpoint commit |
| `96e80152` truth-width checkpointAudit | Focused build `0`, PA16 `7/7`, PA15 `5/5`; fresh PA16 status `2` at `214/243` with authority/fresh `29/29` failures, baseline-only/fresh-only `0/0`, and `243/243` coverage; through-PA15 `1167/1167`; file audit `0` with five pre-existing warnings; final diff/path audits `0`; exact-pointee class-pointer guard repaired | completed audit |
| typed packed-bit-field value/update boundary | Final PA16 `215/243`, 28 failures, `243/243` covered; strict subset removes `400-bitfield-aggregate-init.t`; through-PA15 `1167/1167`; audit passed with five known warnings | landed |
