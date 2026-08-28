# PA16 implementation plan

## Stage Design

This checkpoint audits landed commit 9718b98797312753e33023fe97d36d74afd0a84a
(PA16: type member-function definition declarators) relative to 97d1e7a5,
plus the bounded follow-up corrections in the PA11 typed declarator path.
PA10 preserves qualified declarator shape, parameter clauses, cv/noexcept/ref
suffix nodes, and TrailingReturnType. PA11 carries SpecFact::is_auto,
DeclaratorBaseKind, and the trailing TypeId as a DeclaratorOp. PA12 and PA15
consume the canonical ScopeId, NamedRecordId, BindingId, FunctionFact,
parameter facts, function scope, body scope, and typed return TypeId.

The PA16 contract includes in-class definitions and qualified out-of-class
ordinary non-static member definitions, including private nested leading return
types. It explicitly excludes out-of-class constructor/destructor definitions.
The 9718 special-member widening was reverted: process_special_member is again
class-scope-only, and the namespace/root PA12 special-member analysis and
preparation additions are absent. The existing in-class special-member path
remains intact.

Spec alignment is direct: spec.md §2 supplies typed fact continuity; §4
supplies bounded local work and storage discipline; §5 supplies typed PA12/
PA15 lowering continuity; and §7 supplies executable conformance and
measurement evidence. N3485 [dcl.fct], [dcl.fct.def], and [class.mfct] supply
the trailing-return and member-definition rules:
[N3485](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2012/n3485.pdf).

## Failure Map

The audit-turn starting state is exactly 132/243 passed, 111 failed, and
243/243 identities covered. The authoritative baseline is
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log. Its
diagnostic map is 61 expected-success exit mismatches, 2 expected-failure
exit mismatches, and 48 LowIR comparison mismatches.

Final make test-pa16 is 132/243 passed with 111 failures and all 243 identities
covered. The exact normalized comparison is in
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/identity-compare.log:
baseline errors 111, final errors 111, inventory 243, baseline-only empty,
and final-only empty. No pass identity regressed. The earlier implementation-
turn delta remains the one baseline-only repair
general/300-member-function-trailing-return.t; it is included in the unchanged
failure map comparison.

The required prior-through command passed at 1167/1167. The exact file audit
passed with five pre-existing bad-division header warnings. git diff --check
passed.

## Focused Evidence

Course 413 passes. It publicly rejects mixed and duplicate auto, cv-qualified
auto, typedef auto, auto in a parser-accepted trailing TypeId, missing auto,
non-function arrows, invalid suffix order, unsupported ref qualifiers, auto
parameters, auto without a trailing return, and auto simple declarations. It
also accepts a valid static auto trailing-return control and
auto (*callback)() -> int. The durable log is
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/course-413.log.

The exact seven-test matrix is 5/7: passing
300-member-function-trailing-return.t, 100-out-of-class-methods.t,
300-out-of-class-private-nested-return-type.t,
200-constructor-overload-default-arg-nonfirst-argument.t, and
200-return-preserves-value.t. The two residuals are
300-out-of-class-member-trailing-return.t (existing PA12 invalid conversion
in the member-typedef pointer-return path) and the explicitly excluded
200-nested-out-of-class-constructor-enclosing-type.t (PA11 special member has
no class owner). The separate 200-constructor-member-init.t control is 1/1.
The matrix and control logs are in the same final-v2 evidence directory.

## Completed Checkpoint

The bounded repairs are complete and validated: explicit auto-placeholder
state is threaded through all declarator entry points and nested declarators,
consumed only by the trailing-return operation, and unrelated invalid TypeIds
fail closed. spec_fact rejects duplicate/mixed/cv-qualified/typedef auto;
typed parameter and type-id paths reject auto; suffix and unsupported ref
qualifiers fail closed. The out-of-contract special-member widening is not
retained or covered.

Completed-row disposition: record 9718b987 as a final audit/follow-up
milestone, not PA16 completion. PA16 remains incomplete only for later
residual audit work; the excluded out-of-class constructor is not a repair
target.

## Performance Evidence

The current structural/determinism evidence is durable at
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-member-definition-audit-final-v2/structure.log.
The executable SHA-256 is
375482e808d7c1c1251e9c9ebf186eee8b4ff6014f6e4410c5d2ca5a18d12379 and the
log SHA-256 is
bcb4f7ac160a94f1cbb499ca5a823c657d8487659d58d0b55664b1b9f4a4d1a1.
For existing N=1,4,16,64 member-definition inputs, each run has N
declarations and N definitions; two semantic runs per size exit 0 with
identical hashes. Output lines/bytes are 14/468, 32/1227, 104/4269, and
392/16461; function-record counts are 2, 5, 17, and 65. This is structural
boundedness and determinism evidence only, with no timing, RSS, allocation,
or speedup claim.

## Next Checkpoint

The next checkpoint is a later PA16 residual audit, not broad validation,
focused on the existing member-typedef pointer-return residual and the
remaining explicitly staged PA16 boundaries. Preserve the current exact
identity map and do not claim the excluded out-of-class special-member forms.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed; final log `pa16-operator-followup-through-pa15.log`. |
| PA16 coverage and regression gate | Final `127/243` passed, `116` failed, `243/243` covered; exact comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only identities. |
| Follow-up bool/index audit | Expression-owned typed bool provenance, composite hidden-friend key, corrected same-name performance evidence; all 29 removed identities retained. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; log `pa16-operator-followup-file-audit.log`; final `git diff --check` is recorded in `pa16-operator-followup-diff-check.log`. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpoint audit | Completed bounded repair and audit of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the corrective follow-up separates exact friend-definition lexical ownership from access friendship and records typed public/private/protected base-reference accessibility, including a bounded further-derived protected proof. Enum identity/ranking, narrow constructor-backed reference binding, reference/address facts, and typed bool boundaries remain covered. Final PA16 is `127/243` with `116` failures and `243/243` coverage; comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only failures. Through-PA15 is `1167/1167`; focused status is `29/32` with three documented pre-existing holdouts; course 411 passes; final state-matched performance is `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. |
| `da4252b6` typed bit-field boundary checkpointAudit/follow-up | Complete: PA10--PA15 typed operation/promotion facts, semantic-owner validity checks, const-reference temporaries, overload-before-address-of, owner-stable mixed/zero-width/unnamed/union layout, checked oversized allocation spans, masked signed/unsigned projection, and isolated initialization roots. Final PA16 is `131/243` passed with `112` failures and `243/243` identities; exact comparison to the turn-start map is baseline-only `0` and final-only `0`. Course 412 and the direct alias control pass; focused matrix is `5/11` with six documented LowIR mismatches; through-PA15 is `1167/1167`; file audit and diff-check pass. Corrected bit-field performance is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-bitfield-perf-final-v1` with 30/30 zero-exit runs, actual bit-field inputs/counters, and final/immutable SHA-256 `c98edbf143904e0b09b451310de38e7966149b4374ad912b55a1b9f8c96aaf02`; final wall medians small/large/nested are `0.00/0.07/0.00s` and RSS medians `6104/30600/6620` KiB. |
| `9718b987` member-function-definition declarator audit/follow-up | Final audit/follow-up: special-member widening reverted; explicit auto-placeholder and typed/fail-closed declarator validation remain. Final PA16 is `132/243` with `111` failures, `243/243` identities, and exact failure-set baseline-only/final-only `∅`/`∅`; through-PA15 `1167/1167`, course 413 pass, focused `5/7`, constructor-member-init `1/1`, file audit pass with five pre-existing warnings, and diff-check pass. The excluded nested constructor is a failing non-contract identity. PA16 remains incomplete; next checkpoint is later residual audit. |
