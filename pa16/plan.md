# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10 and PA11 typed syntax, lookup, and
layout become PA12 semantic facts and PA15 LowIR.  Earlier bit-field
checkpoints retain one typed `BitFieldFact`/`BindingId` path into PA15
projections; this audit does not alter that owner chain.

For the landed constructor increment, PA12 resolves an unqualified
mem-initializer in class/member lookup and the constructor definition-point
context, publishes canonical `NamedRecordId`/`BindingId` targets, typed
`ConstructorActionFact` ranges, and declaration order.  PA15 consumes those
facts through owner/type/layout validation without spelling recovery.  This
follows `spec.md` §§1--5 and 7, the PA16 constructor/base/member boundary, and
N3485 §12.6.2 p2--p5.  No textual transport, parallel analyzer, retry, second
lowerer, or host/reference shortcut is involved.

## Failure Map

The supplied current authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`216/243` passed, exactly `27` failed, and `243/243` identities are covered.
The complete current failure map is:

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
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

This supplied map is also the final map after the repair.  `make test-pa16`
returned status `2` with `216/243` passing and `243/243` covered.  Exact
comparison is authority `27` -> final `27`, authority-only `0`, fresh-only
`0`, and missing artifacts `0`; course 425 covers an invariant outside the
243 handout identities.

## Active Checkpoint

The landed comparison fixed ordinary `typedef Base Alias` selection but
performed type resolution before class-member hiding.  The audit found that a
direct member named like the base or its alias could therefore publish a base
action, and that array unwrapping was too broad for base identity.  The repair
in `dev/src/pa12_semantic_construction.cpp` first applies typed member lookup,
claims direct or inherited non-constructor values as hiding, then resolves a
single unqualified type with the constructor source point and function access
scope.  It uses exact `named_record_for_type` identity: cv aliases remain
valid, while array/pointer/reference/fundamental aliases do not become bases.
The spelling fallback is allowed only when both typed type and declaration
lookup are unresolved, preserving the injected-name case without bypassing a
hidden or inaccessible declaration.  The final steering also makes the
constructor-name scan validate BindingId validity/bounds before sidecar access
and keeps blocked-value lookup single-read.

Duplicate detection, malformed/missing lookup failure, typed argument ranges,
base-first publication, and declaration-ordered member actions remain at the
PA12 owner.  PA15 continues to consume canonical owner/type/layout facts and
does not recover a target from spelling.  Qualified-name handling remains the
existing checkpoint boundary.  Course 425 is the smallest added regression
for direct base-name/alias-name hiding, duplicate detection, array-alias
rejection, and nested-type hiding.

## Performance Evidence

Each mem-initializer performs the existing typed member lookup and, when no
member value claims the name, one existing typed type lookup.  Work is bounded
by the relevant class/base lookup graph and candidate bucket; the constructor
bucket check is bounded by that already-selected value list.  Action and
argument publication remains linear in the declaration-ordered actions.  No
whole-program scan, retry, cache, text round-trip, parallel analyzer, or
second lowerer was added.

Representative startup-sized smoke evidence is the final checked-in
aliased-base run: `elapsed=0.00`, `user=0.00`, `sys=0.00`, `rss_kb=6040`, exit
`0`.  This is not a scaling benchmark or a material timing claim.

## Validation

Final validation is complete:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- The six-test handout matrix of alias, ordinary base/member, default, and
  in-class initializer controls: status `0`, `PASS (6/6)`.
- `sh -n` for course 425: status `0`; courses 408, 409, 418, and 425: status
  `0` each; direct reductions cover private cv-qualified aliases (success) and
  an inherited data-member collision (status `1`).
- `make test-pa16`: status `2`, `216/243` passed, exactly `27` failures, and
  `243/243` identities covered; exact authority/final comparison is `27/27`
  with authority-only/fresh-only `0/0`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`: status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`,
  with five known header-division warnings.
- Final smoke and `git diff --check`: status `0`; the bounded changed-path
  audit found exactly the four approved paths and no staged path before commit.

No handout, fixture, reference, harness, comparator, coverage surface,
generated output, or source-set file was changed.

## Next Checkpoint

PA16 closes this bounded alias direct-base checkpoint at `216/243`, with `27`
unchanged residual failures and `243/243` coverage.  The next checkpoint is a
distinct residual constructor/lifetime boundary; preserve the qualified-name
boundary and the unrelated residual identities rather than widening this
owner path.

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
| `a5c8e166` typed packed-bit-field value/update checkpointAudit | Final PA16 status `2` at `215/243`, exactly `28` failures and `243/243` covered; independent comparison authority/fresh `28/28`, authority-only/fresh-only `0/0`, inventory/run total `243/243`; landed delta is exactly baseline-only `400-bitfield-aggregate-init.t`; through-PA15 `0` at `1167/1167`; file audit `0` with five known warnings; focused 412/422/424, probes, diff-check, and path audit pass. Durable evidence is under `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-plain-int-bitfield-checkpoint-audit-20260830/`; no forbidden surface changed | completed audit |
| `1d7e6860` alias direct-base mem-initializer checkpoint | final `216/243`, `27` failures, `243/243` covered; exact authority/final failure comparison `27/27`, authority-only/fresh-only `0/0`; focused `6/6`, courses `408/409/418/425`, through-PA15 `1167/1167`, file audit `0` with five known warnings, smoke, and diff/path checks pass; four approved paths ready for commit | completed |
