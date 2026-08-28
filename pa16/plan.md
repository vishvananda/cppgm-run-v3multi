# PA16 implementation plan

## Stage Design

This increment implements the ordinary, non-template overloaded-operator
call boundary required by PA16. PA11 carries typed operator kind/token
metadata, canonical `BindingId`/`ScopeId` ownership, and sparse hidden-friend
relations. PA12 discovers relevant member, ordinary-lookup, ADL, and
hidden-friend candidates for unary, postfix, binary, subscript, and call-form
operators; ranks the implicit object with the existing typed conversion,
access, cv, and base machinery; and publishes one typed `CallExpression` with
the selected identity, return category, converted arguments, and call
children. PA15 consumes that canonical call and its existing operator-ABI
path. Absent or nonviable overloads retain built-in fallback, and only
built-in `&&`/`||` retain short-circuit lowering.

Hidden friends use a sparse `(namespace ScopeId, NameId)` typed index, while an
in-class friend definition records one exact `Function ScopeId` to lexical
class `ScopeId`/`NamedRecordId` relation:
hidden-hidden redeclarations merge by complete function signature, while a
later matching visible declaration reuses the hidden `BindingId` and publishes
visibility at that declaration point. Hidden-only functions remain absent from
ordinary and qualified lookup. Nonmember operator declarations require a
class or enum operand. Candidate discovery is deterministic and bounded to
relevant same-name/member/associated candidates; it does not rescan whole
namespaces, parse rendered names, retry lowering, or use an unbounded cache.

Friend-definition bodies preserve the introducing class's lexical type/value
scope while their canonical function binding remains namespace-owned; the
access-friend set is not reused for lexical lookup.  Enum identity is checked
before integral promotion; direct-base access is retained as a typed
`NamedRecord` fact and checked along a bounded path; and one narrow
non-explicit converting constructor may produce a selected class-reference
argument through the existing constructor path.  These repairs do not add
general conversion, copy/move, or by-value semantics.

PA12 facts carry typed bool provenance. Canonical comparison/logical truth is
materialized to the established bool representation only when the expression
does not already own a direct bool boundary; the decision does not inspect
operator/friend function categories. Typed call results carry the ABI-bool
operand relation used when a value-producing logical expression normalizes its
RHS to i64, while built-in scalar and literal bool operands retain their
existing physical path.

The PA16 boundary excludes templates, class by-value transfer/copy/move,
conversion operators, member pointers, virtual or multiple inheritance, and
unrelated lifetime/global-initialization work. The only temporary support
added is the existing constructor-path functional cast needed to form the
operator call boundary.

## Failure Map

The original implementation baseline, before the `23a26df5` operator landing,
was `93/243` passed and `150` failed.  The audit turn-start baseline after the
landed implementation and `2d93a5e9` tightening was `122/243` passed and
`121` failed, with all `243/243` identities covered; its authoritative log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final audit run is `127/243` passed, `116` failed, and `243/243` covered;
it exits `2` because PA16 still has failures.  The durable final log and exact
identity comparison are:

```text
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-test-pa16.log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-identity-compare.log
```

The comparison has five baseline-only repaired identities:

```text
pa16/tests/general/200-inherited-member-overload-set.t
pa16/tests/general/300-basic-operator-overloads.t
pa16/tests/general/300-enum-operator-adl-selects-matching-overload.t
pa16/tests/general/300-hidden-friend-operator-nullptr-compare.t
pa16/tests/general/300-stream-shift-selection-chain.t
```

Final-only is `0`; the failure count is therefore no greater than the audit
turn-start `121`, and coverage remains exactly `243`.  The final direct
focused matrix is `29/32` in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-focused-final.log`:
the original 29-row operator matrix is `28/29`, and the three mismatches in
the extended matrix are the pre-existing `nullptr_t`, private-base static-cast
member, and inherited-protected-field friend controls.  Course 411 passes
with the exact lexical-owner/access-friend and public/private/protected
further-derived cases.  The semantic rows cover member/nonmember ranking,
enum identity/ADL and nested friend lookup, friend visibility/redeclaration,
derived/base references, reference-result chaining, fallback, logical
operators, and shift/string chains.

## Active Checkpoint

The current audit changes are limited to these eight authorized source files
and one focused course regression:

- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa12_semantic_calls.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa12_semantic_member.cpp`
- `dev/src/pa12_semantic_resolution.cpp`
- `dev/src/pa11_semantic_model.h`
- `cppgm.tests/course/pa16/411-typed-operator-lexical-base-access-regression.sh`

The full trace is PA10 operator token/name metadata -> PA11 typed operator
kind/token, canonical `BindingId`/`ScopeId`, friend relation and sparse
`(namespace ScopeId, NameId)` key plus the exact friend-definition lexical
owner -> PA12 member/ordinary/ADL candidate union,
typed implicit-object and argument conversion, one `CallExpression`, and
expression-owned bool provenance -> PA15 typed call demand/ABI lowering and
LowIR call/result.  Hidden-only visibility, visible redeclarations,
declaration points, class/enum nonmember rules, built-in fallback, reference
return chaining, constructor-backed reference binding, and overloaded versus
built-in logical behavior stay on that path.  The 411 course regression is the
only added test; no handout test, fixture, reference, comparator, or generated
output is changed.

## Performance Evidence

The final immutable compiler copy is mode `0555` at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf-followup-v5/cppgm++-immutable`.
It byte-matches the final `dev/cppgm++`; the SHA-256 for both is
`e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`.
Five interleaved final/immutable rounds used preserved small, large, and
same-name-noise inputs.  `/usr/bin/time` measured whole compiler invocations,
including parsing and LowIR output, rather than an isolated lookup phase.
Raw inputs and all timing rows are in the artifact's `input/` and `timing.tsv`;
medians are in `medians.tsv`, and structural LowIR counts are in
`structure.tsv`.

| input | lines | target decls | unrelated same-name hidden friends | target expressions | LowIR functions/calls | wall median (range) | RSS median (range) |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| small | 268 | 2 | 0 | 128 | 131 / 256 | `0.02s (0.02..0.02)` | `9352 (9336..9544) KiB` |
| large | 1046 | 12 | 0 | 512 | 525 / 1024 | `0.11s (0.11..0.11)` | `21932 (21892..22076) KiB` |
| same-name-noise | 1804 | 2 | 256 | 128 | 387 / 256 | `0.08s (0.08..0.08)` | `17668 (17560..17832) KiB` |

The noise case keeps the target expression/call counts equal to small while
adding 256 unrelated same-name hidden friends, structurally corroborating the
exact-key bounded lookup.  The measurements are representative evidence and
do not overclaim an isolated phase or timeout bound.

## Next Checkpoint

Keep the `127/243` PA16 map frozen while separately classifying the remaining
full-stage identities.  A future checkpoint may address the `nullptr_t`
declaration only if PA10 parser ownership is explicitly opened; this ordinary
operator audit does not widen into that path or into general conversion/value
semantics.  Any next PA16 repair must preserve `243/243` coverage, zero
final-only identities, the protected further-derived boundary, and the
`1167/1167` through-PA15 gate before PA17.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed; final log `pa16-operator-followup-through-pa15.log`. |
| PA16 coverage and regression gate | Final `127/243` passed, `116` failed, `243/243` covered; exact comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only identities. |
| Follow-up bool/index audit | Expression-owned typed bool provenance, composite hidden-friend key, corrected same-name performance evidence; all 29 removed identities retained. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; log `pa16-operator-followup-file-audit.log`; final `git diff --check` is recorded in `pa16-operator-followup-diff-check.log`. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpoint audit | Completed bounded repair and audit of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the corrective follow-up separates exact friend-definition lexical ownership from access friendship and records typed public/private/protected base-reference accessibility, including a bounded further-derived protected proof. Enum identity/ranking, narrow constructor-backed reference binding, reference/address facts, and typed bool boundaries remain covered. Final PA16 is `127/243` with `116` failures and `243/243` coverage; comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only failures. Through-PA15 is `1167/1167`; focused status is `29/32` with three documented pre-existing holdouts; course 411 passes; final state-matched performance is `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. |
