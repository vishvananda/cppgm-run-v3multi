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

Hidden friends use a sparse same-name typed index: hidden-hidden
redeclarations merge by complete function signature, while a later matching
visible declaration reuses the hidden `BindingId` and publishes visibility at
that declaration point. Hidden-only functions remain absent from ordinary and
qualified lookup. Nonmember operator declarations require a class or enum
operand. Candidate discovery is deterministic and bounded to relevant
same-name/member/associated candidates; it does not rescan whole namespaces,
parse rendered names, retry lowering, or use an unbounded cache.

The PA16 boundary excludes templates, class by-value transfer/copy/move,
conversion operators, member pointers, virtual or multiple inheritance, and
unrelated lifetime/global-initialization work. The only temporary support
added is the existing constructor-path functional cast needed to form the
operator call boundary.

## Failure Map

The turn-start baseline was `93/243` passing and `150` failing, with all 243
identities covered; the source log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final PA16 run covers all 243 identities and reports `122/243` passing
and `121` failing; its log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-final-test.log`.
The exact comparison is recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-final-identity-compare.log`:
the new-failure set is empty, and the exact 29 removed baseline identities
are:

```text
general/200-friend-function-member-access.t
general/200-inherited-base-typedefs-in-derived-members.t
general/200-out-of-line-member-inherited-typedef-body.t
general/300-chained-member-subscript-operator-call.t
general/300-discarded-comma-reference-result-no-copy.t
general/300-hidden-friend-definition-adl-call.t
general/300-late-member-subscript-shadows-type.t
general/300-member-binary-operator-eq.t
general/300-member-binary-operator-ne-wrapper.t
general/300-member-callable-field-call.t
general/300-member-deref-after-prefix-decrement.t
general/300-member-operator-bang-out-of-class.t
general/300-member-postfix-increment-operator.t
general/300-member-prefix-decrement.t
general/300-member-subscript-operator-call.t
general/300-nonmember-operator-requires-class-or-enum-bad.t
general/300-operator-token-result-typing.t
general/300-out-of-class-private-nested-return-type.t
general/300-overloaded-arrow-star-operator.t
general/300-postfix-ref-return-deref-member-call.t
general/300-private-base-using-method-call.t
general/300-qualified-friend-function-access.t
general/300-subobject-member-deref-after-prefix-decrement.t
general/300-temporary-functor-call.t
general/300-using-base-same-signature-derived-preferred.t
general/300-using-declaration-public-private-base-member.t
spec/300-logical-operator-overload.t
spec/300-operator-lookup-ordinary-adl-union.t
spec/300-overloaded-comma-nonviable-falls-back-builtin.t
```

The original seven focused identities all reach the required semantic
boundary; the two remaining focused handout mismatches are normalized LowIR
shape differences for the bool-return member equality and overloaded logical
cases. The expanded focused set is `15/17`: the other two are the known
unnamed-namespace hidden-friend symbol/init presentation and implicit-object
cv-rank address/order presentation differences, both with successful
compiler exits. Hidden-friend ADL/qualified-lookup visibility, redeclaration,
operator-token typing, ranking, fallback, and the minimal temporary-functor
path are covered without fixture or comparator changes.

## Active Checkpoint

The implementation changes are limited to:

- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic_model.h`
- `dev/src/pa12_semantic.cpp`
- `dev/src/pa12_semantic_calls.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa12_semantic_member.cpp`
- `dev/src/pa12_semantic_resolution.cpp`
- `dev/src/pa12_semantic_selection.h`
- `dev/src/pa15_lowering_calls.cpp`
- `dev/src/pa15_lowering_construction.cpp`
- `dev/src/pa15_lowering_flow.cpp`

The data flow is PA11 typed identity and sparse hidden-friend indexing -> PA12
candidate selection and one canonical typed call -> PA15 call lowering and
ABI identity. Bool returns and bool-context operands use the canonical typed
boundary, preserving the established i64 logical physical path without an
untyped lowering workaround. Final gate results are recorded below after the
full run; no test, fixture, reference, comparator, or generated output is
changed.

## Performance Evidence

The immutable compiler copy is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/cppgm++-immutable`.
Five interleaved small/large/declaration-noise runs used generated inputs
outside the repository. Medians are:

| input | operator candidate declarations | operator expressions | unrelated declarations | lines | LowIR functions/calls | wall/user/sys | max RSS |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| small | 2 | 128 | 0 | 268 | 131 / 256 | 0.01 / 0.01 / 0 | 9676 KB |
| large | 12 | 512 | 0 | 1046 | 525 / 1024 | 0.06 / 0.04 / 0.02 | 22136 KB |
| declnoise | 2 | 128 | 1000 | 1271 | 131 / 256 | 0.04 / 0.03 / 0.01 | 16392 KB |

The equal LowIR counts for small and declaration-noise inputs, despite 1000
unrelated namespace declarations, are evidence against a per-expression
whole-namespace hidden-friend scan. The large case scales with relevant
candidate and expression counts; its wall time also includes larger parsing
and output work, so this is bounded-scaling evidence rather than a phase-cost
claim. Raw inputs, timing rows, medians, and structural counts are in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/expanded-input/`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/timing-final-labeled.tsv`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/median-final-summary.txt`,
and
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/final-structure-summary.txt`.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed. |
| PA16 coverage and regression gate | `122/243` passed, `243/243` covered, 121 remaining baseline failures, zero new failure identities. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; `git diff --check` passed. |
