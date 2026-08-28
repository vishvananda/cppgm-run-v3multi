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

Hidden friends use a sparse `(namespace ScopeId, NameId)` typed index:
hidden-hidden redeclarations merge by complete function signature, while a
later matching visible declaration reuses the hidden `BindingId` and publishes
visibility at that declaration point. Hidden-only functions remain absent from
ordinary and qualified lookup. Nonmember operator declarations require a
class or enum operand. Candidate discovery is deterministic and bounded to
relevant same-name/member/associated candidates; it does not rescan whole
namespaces, parse rendered names, retry lowering, or use an unbounded cache.

Friend-definition bodies preserve the introducing class's lexical type/value
scope while their canonical function binding remains namespace-owned.  Enum
identity is checked before integral promotion; derived-to-public-base
reference binding reuses typed access/path conversion; and one narrow
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
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-final-v2-test.log
/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-final-v2-identity-compare.log
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
turn-start `121`, and coverage remains exactly `243`.  The focused final
status matrix is `28/29` in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-focused-final-direct.log`.
Its one mismatch is the PA10 parser-owned `nullptr_t` declaration described
in the audit; the other semantic rows cover member/nonmember ranking, enum
identity/ADL and nested friend lookup, friend visibility/redeclaration,
derived/base references, reference-result chaining, fallback, logical
operators, and shift/string chains.

## Active Checkpoint

The current audit changes are limited to these six authorized source files:

- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa12_semantic_calls.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa12_semantic_resolution.cpp`

The full trace is PA10 operator token/name metadata -> PA11 typed operator
kind/token, canonical `BindingId`/`ScopeId`, friend relation and sparse
`(namespace ScopeId, NameId)` key -> PA12 member/ordinary/ADL candidate union,
typed implicit-object and argument conversion, one `CallExpression`, and
expression-owned bool provenance -> PA15 typed call demand/ABI lowering and
LowIR call/result.  Hidden-only visibility, visible redeclarations,
declaration points, class/enum nonmember rules, built-in fallback, reference
return chaining, constructor-backed reference binding, and overloaded versus
built-in logical behavior stay on that path.  No test, fixture, reference,
comparator, generated output, or course test is changed.

## Performance Evidence

The final immutable compiler copy is mode `0555` at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf-final-v2/cppgm++-immutable`.
It byte-matches the final `dev/cppgm++`; the SHA-256 for both is
`eb5365ac9251363502dd4f191f9f492ee0fcab52345511634750384d4f6ca604`.
Five interleaved final/immutable rounds used preserved small, large, and
same-name-noise inputs.  `/usr/bin/time` measured whole compiler invocations,
including parsing and LowIR output, rather than an isolated lookup phase.
Raw inputs and all timing rows are in the artifact's `input/` and `timing.tsv`;
medians are in `medians.tsv`, and structural LowIR counts are in
`structure.tsv`.

| input | lines | target decls | unrelated same-name hidden friends | target expressions | LowIR functions/calls | wall median (range) | RSS median (range) |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| small | 268 | 2 | 0 | 128 | 131 / 256 | `0.01s (0.01..0.01)` | `9504 (9356..9568) KiB` |
| large | 1046 | 12 | 0 | 512 | 525 / 1024 | `0.07s (0.06..0.07)` | `21896 (21828..22000) KiB` |
| same-name-noise | 1804 | 2 | 256 | 128 | 387 / 256 | `0.05s (0.05..0.05)` | `17760 (17544..17808) KiB` |

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
final-only identities, and the `1167/1167` through-PA15 gate before PA17.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed; final log `through-pa15-followup-final.log`. |
| PA16 coverage and regression gate | Follow-up `122/243` passed, `121` failed, `243/243` covered; zero new identities versus baseline, with the exact same failure set as the prior final. |
| Follow-up bool/index audit | Expression-owned typed bool provenance, composite hidden-friend key, corrected same-name performance evidence; all 29 removed identities retained. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; log `pa16-followup-file-audit-final.log`; final `git diff --check` passed with log `pa16-followup-diff-check-final.log`. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpoint audit | Completed bounded repair and audit of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: friend lexical lookup, enum identity/ranking, reference/base/access selection, narrow constructor-backed reference binding, reference/address facts, and typed bool boundaries. Final PA16 is `127/243` with `116` failures and `243/243` coverage; comparison to the `122/243` audit baseline has five baseline-only repairs and zero final-only failures. Through-PA15 is `1167/1167`; focused semantic status is `28/29` with the PA10 `nullptr_t` parser holdout; final state-matched performance is `pa16-operator-perf-final-v2`. |
