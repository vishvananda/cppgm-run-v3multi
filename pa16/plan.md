# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10 declaration-specifier tokens and
PA11 layout become PA12 typed semantic facts and conversion facts, and PA15
lowers those facts into LowIR.  PA12 owns the `BitFieldFact` and its
`BindingId`; PA15 owns the `BitFieldAddressProjection` and `ProjectionId` used
by its typed `LoweredValue`.  The packed-field path carries declared type,
storage type, operation type, width, mask, and signedness through those owners.
Member reads, conversions, encodes, stores, inc/dec, and aggregate
initialization consume that same identity.  The selected implementation-defined
plain-`int` policy is unsigned storage/value signedness; a narrow field still
promotes to `int` when `int` represents its range, while a full-width field
promotes to `unsigned int`.  Explicit signed integral fields and
signed-underlying enum fields remain signed.  This checkpoint adds no text
transport, parallel analyzer, rescan/cache, retry, or second lowerer.

Constructor initialization follows the same typed continuity: PA12 resolves a
mem-initializer to the canonical direct-base/member identity before publishing
declaration-ordered `ConstructorActionFact` and argument ranges, and PA15
consumes those facts without recovering targets from spelling or lookup.

## Failure Map

Turn-start authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`215/243` passed, exactly `28` failed, and `243/243` identities are covered.
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
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Relative to the previous `214/243` / `29`-failure checkpoint, the landed
`a5c8e1664e5059e2453e3252021f3843d0ab23b6` increment had exactly one
baseline-only recovery, `400-bitfield-aggregate-init.t`, and no fresh-only
identity.  That is the turn-start authority for this checkpoint: `215/243`,
exactly `28` failures, and `243/243` coverage, with authority/fresh `28/28`
and no identity delta at that earlier run.

The final run after this repair is `216/243`, exactly `27` failures, and
`243/243` coverage.  Its exact comparison to the preserved map above is
authority `28`, fresh `27`, authority-only exactly
`200-aliased-base-mem-initializer-match.t`, fresh-only `0`; the current
residual map is every listed turn-start identity except that alias test.

## Active Checkpoint

This checkpoint records one coherent repair in
`dev/src/pa12_semantic_construction.cpp` for
`200-aliased-base-mem-initializer-match.t`.  PA12 previously classified a
direct-base mem-initializer only when its spelling matched the base record
name, so `Alias(...)` fell through to direct-member lookup.  The repair
resolves the single-component name at the constructor source point, maps it
through `class_record_for_object_type`, and compares the canonical
`NamedRecordId`; the existing spelling path remains for the injected
class-name case.  Resolution starts at the constructor function scope, uses
the constructor definition source point, and evaluates access in that same
constructor context.  Constructor action/argument arenas and declaration
order are unchanged, and PA15 receives the same typed base action once
classification is correct.  This keeps the fix at the semantic owner boundary
rather than reconstructing a target during lowering.

The other focused identities have distinct roots and remain intentionally
outside this diff: external-ctor currently reaches PA15 string-literal address
lowering, nested-braced initialization stops in PA10 parsing, reference-member
initialization fails typed overload selection, and the value-init case differs
only in zero-store shape.  The local class-array case has a checked reference
that orders destruction forward, while the current reverse order follows C++
destruction semantics; it remains an oracle tension, not a reason to change
lifetime lowering.

## Performance Evidence

The repair performs one existing typed lookup per mem-initializer.  Each
visited scope/candidate bucket uses its O(1) index; total lookup work is bounded
by the language-relevant scope/base graph visited.  Existing action publication
remains O(member count) and declaration-ordered.  No retry, new arena, lowerer
path, or fixture-dependent branch was added.  The representative post-build
semantic run measured `elapsed=0.00` seconds and `rss_kb=5188` for the small
alias test; this is non-scaling smoke evidence, not a benchmark.

## Validation

Final validation is complete:

- Ordinary alias/base/member constructor checks: status `0`, `PASS (4/4)`.
- `make test-pa16`: status `2`, `216/243` passed, `27` failures, inventory/run
  `243/243`, and full `243/243` coverage.  Exact comparison with the turn-start
  map gives authority/fresh `28/27`, authority-only exactly
  `200-aliased-base-mem-initializer-match.t`, fresh-only `0`.
- `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`:
  status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`,
  with the five known header-division warnings.
- `git diff --check`: status `0`; changed-path audit contains only
  `dev/src/pa12_semantic_construction.cpp` and `pa16/plan.md`.

No test, handout, reference, harness, comparator, or coverage surface was
changed.  The final commit hash and clean-status result are recorded in the
ledger as this landed checkpoint and in the final handoff.

## Next Checkpoint

PA16 now stands at `216/243`, with `27` residual failures and `243/243`
coverage.  The exact delta from the turn-start `215/243` / `28`-failure map is
the baseline-only recovery of
`200-aliased-base-mem-initializer-match.t`, with no final-only identity.  The
next checkpoint must choose a distinct constructor/lifetime boundary; retain
the array destruction-order oracle tension unless the contract is corrected.

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
| `PA16 alias direct-base checkpoint (parent 727417db; final commit hash in handoff)` | final `216/243`, `27` failures, `243/243` covered; exact delta is baseline-only `200-aliased-base-mem-initializer-match.t`, fresh-only `0`; through-PA15 `1167/1167`; file audit `0` with five known warnings; diff/path audit `0` | landed |
