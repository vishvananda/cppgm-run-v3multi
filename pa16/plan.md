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

The turn-start baseline was `93/243` passing and `150` failing, with all 243
identities covered; the source log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The prior final and follow-up PA16 runs each cover all 243 identities and
report `122/243` passing and `121` failing.  The follow-up log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-followup-test.log`;
the prior final log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-final-test.log`.
The exact comparison is recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-followup-identity-compare.log`.
Against the turn-start baseline, the new-failure set is empty and the exact
29 removed baseline identities are:

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

The old-final-only and follow-up-only identity sets are also empty.  The
focused results are recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/followup-focused-required.log`
and the bool-specific checks in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/followup-bool-fact-focused.log`.

The original seven focused identities all pass their final semantic/exit
checks (the nonmember bad-declaration case retains its expected
`EXIT_FAILURE`).  The expanded focused set is `15/17` under normalized LowIR
comparison.  The exact two baseline holdouts are
`general/300-unnamed-namespace-hidden-friend-single-definition.t` and
`general/300-member-vs-nonmember-operator-implicit-object-cv-rank.t`; both
have successful compiler exits and are presentation-only LowIR differences.
Member equality, overloaded logical, hidden-friend ADL, operator-token result
typing, fallback, visibility, redeclaration, and semantic ranking checks pass.
No full-stage passing identity is described as a mismatch.

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

The data flow is PA11 typed identity and the `(namespace ScopeId, NameId)`
sparse hidden-friend index -> PA12 candidate selection and one canonical typed
call -> PA15 call lowering and ABI identity.  Bool returns and bool-context
operands use expression-owned typed provenance at the canonical boundary,
preserving the established i64 logical physical path without function-kind,
rendered-name, or fixture-specific branching.  No test, fixture, reference,
comparator, or generated output is changed.

## Performance Evidence

The immutable compiler copy is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/cppgm++-followup-immutable`.
Its SHA-256 equals the final `dev/cppgm++` hash
`c5722af6b0fd3d27943485cc4360cac3a7f63f594415136a436dc856d9248850`.
Five interleaved small/large/same-name-noise runs used generated inputs
outside the repository.  The noise input contains 256 unrelated namespaces
with same-name hidden `operator+` friends, while the target namespace retains
two relevant candidates and the same 128 target expressions as small.  Median
wall/user/system times and maximum RSS are:

| input | relevant candidates | unrelated same-name hidden friends | expressions | lines | LowIR functions/calls | wall/user/sys | max RSS |
| --- | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| small | 2 | 0 | 128 | 268 | 131 / 256 | 0.01 / 0.01 / 0.00 | 9608 KB |
| large | 12 | 0 | 512 | 1046 | 525 / 1024 | 0.06 / 0.04 / 0.02 | 21944 KB |
| same-name-noise | 2 | 256 | 128 | 1804 | 387 / 256 | 0.05 / 0.03 / 0.01 | 17652 KB |

The same-name-noise and small cases have identical target expression/call
counts despite the 256 unrelated same-name hidden friends; this is structural
evidence that the hidden-friend merge lookup is bounded by the exact
namespace/name key rather than a whole-namespace scan.  The large case scales
with relevant candidate and expression counts; its wall time also includes
larger parsing and output work, so these measurements demonstrate bounded
candidate discovery rather than proving an isolated compiler phase cost.  Raw
inputs, timing rows, medians, and structural counts are in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/expanded-input/`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf.UDjY8t/timing-followup.tsv`,
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-followup-performance-medians.log`,
and
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-followup-performance-structure.log`.
The final hash comparison is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/followup-immutable-hash-final.log`.

## Checkpoint ledger

| checkpoint | result |
| --- | --- |
| Ordinary non-template overloaded-operator boundary | Implemented: typed member/nonmember/fallback calls, hidden-friend identity/visibility, implicit-object ranking, bool boundary, and PA15 ABI consumption; 29 existing PA16 failures removed. |
| Through PA15 compatibility | `make test-report-through-pa15`: `1167/1167` passed; final log `through-pa15-followup-final.log`. |
| PA16 coverage and regression gate | Follow-up `122/243` passed, `121` failed, `243/243` covered; zero new identities versus baseline, with the exact same failure set as the prior final. |
| Follow-up bool/index audit | Expression-owned typed bool provenance, composite hidden-friend key, corrected same-name performance evidence; all 29 removed identities retained. |
| File audit and diff check | File audit passed with 5 pre-existing warnings; log `pa16-followup-file-audit-final.log`; final `git diff --check` passed with log `pa16-followup-diff-check-final.log`. |
