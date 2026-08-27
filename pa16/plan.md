# PA16 implementation plan

## Stage design/spec alignment and owner/data flow

This checkpoint keeps one typed production path from PA10
`CallExpression(MemberExpression)` through PA12 semantic facts, PA15, and
LowIR.  The member-call probe unwraps only the supported parenthesized callee,
looks up the direct class value-table entries, and admits ordinary non-static
functions.  PA12 owns object category/cv compatibility, explicit conversion
and default viability, access, selection, and the canonical selected
`BindingId`/`callable_type`; it does not recover a declaration from text.

`prepare_pa12_member_parameter` owns the exact synthetic first `this`
parameter in the member Function `Scope`.  A successful call publishes the
typed object as semantic child zero and converted explicit arguments in source
order.  Its callable Function type has the hidden object pointer first, while
the original member Function type remains the ABI/signature owner.

The only member emission-demand edge is a successful typed member
`CallExpression` with its selected `BindingId` in a reachable emitted
FunctionFact body.  PA15 maps that binding through the existing typed
binding-to-FunctionFact index, then follows `FunctionFactId` work items.  It
seeds the same namespace FunctionFact eligibility used by `collect_functions`
and uses dense byte vectors for function/fact scans and class-method demand.
Transitive class methods are followed once; unrelated class bodies are not
scanned.  PA15 validates the selected FunctionFact, class owner, exact hidden
binding/signature, object qualification, direct symbol, and ABI method cv.
Dot uses one object address and arrow uses one object expression before
lowering explicit arguments.

This matches the root specification's one production pipeline, typed fact
continuity, canonical semantic owners, typed demand roots/edges, bounded
worklists, and typed lowering.  There is no parallel textual demand model or
second canonical member-demand owner.

## Failure map and coverage identity

The immutable turn-start landed evidence for `0b534f2f` relative to
`b1e8272d` was `47/243` passing, `196` failing, `243/243` covered, and
`make test-pa16` exit `2`.  Its complete failure map is preserved in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.

The authorized final full-stage run is also `47/243` passing, `196` failing,
`243/243` covered, and exit `2`.  Comparing test identities from the immutable
baseline and the final run gives:

```text
baseline failures: 196
final failures:    196
added failures:      0
removed failures:    0
handout coverage:  243/243
turn-start passes lost: 0
```

Thus no new pass is being used to offset a regression, and the full PA16
failure map is identity-equivalent to the turn-start map.  The focused direct
cv handout now reaches its pre-existing aggregate-brace conversion failure;
the unqualified `implicit-this` overload failure remains a separate owner
boundary.  Earlier through-PA15 evidence is now revalidated at `1167/1167`.

## Current checkpoint

The selector ranks the implicit object as the first conversion sequence and
reuses typed direct-call ranks for explicit arguments.  Qualification
compatibility is checked before ranking; exact cv matches beat added
qualification.  A variadic function is not preferred merely because it is
non-variadic when no ellipsis conversion occurs; an actual extra variadic
argument receives the worst rank, and equal no-ellipsis candidates are
ambiguous.  Defaults are checked before selection and materialized only for
the selected binding.

The PA15 demand repair removes the landed boolean index that could leak a
member helper from an un-emitted member body.  A single reachable typed-fact
traversal starts at namespace FunctionFacts that `collect_functions` emits,
follows selected member bindings transitively, and marks each FunctionFact
once with dense typed metadata.  The course regression proves hidden-object
call formation, both cv ABI identities and call targets, transitive demand,
unreachable suppression, actual ellipsis handling, and variadic ambiguity.

The semantic tail guard rolls back failed probe facts and no failed probe can
publish a demand edge.  Direct symbol/mangling and hidden-first-argument
lowering are validated against the FunctionFact and callable Function type;
free and indirect calls retain their PA15 controls.  PA16 remains incomplete;
unqualified member lookup, inherited/protected/friend/using-imported lookup,
static calls, constructors/lifetime, virtual/ref-qualified methods, and broad
user-defined overload conversion are deferred slices.

## Performance evidence and uncertainties

Member viability/ranking is approximately `O(C * (P + A))` for `C` local
candidates, `P` parameters, and `A` explicit arguments after direct class
lookup.  PA15's `FunctionFactId` worklist and dense byte/flag vectors process
each reachable FunctionFact and SemanticFact at most once; storage is bounded
by the existing typed fact domains.  `pa15_lowering.cpp` is 2964 lines after
the helper moved to the already-registered call-lowering translation unit, and
the final file audit passes with only the five pre-existing header warnings.

No timing/RSS/allocation or structural-counter measurement was collected, so
the evidence is structural rather than a numerical performance claim.  The
remaining bounded risks are large-input constants for the linear reachable
fact walk and the explicitly deferred PA16 feature slices.  Demand ownership,
speculation rollback, naming, and retry behavior are resolved for this
checkpoint; no speculative-demand uncertainty is carried forward.

Focused evidence after the final repair is:

- PA16 affected controls: `6/6`.
- PA15 free/indirect controls: `3/3`.
- Relevant course regressions 400, 401, and 402: each exits `0`.
- `git diff --check` and the 402 shell syntax check: pass.

Broad evidence is through-PA15 `1167/1167`, file audit exit `0` with five
pre-existing warnings, and full PA16 `47/243`, `196` failures,
`243/243` coverage, exit `2`, with zero failure-identity additions/removals.

## Next checkpoint

The next bounded checkpoint should own unqualified member method calls and
hidden-`this` lookup inside member bodies, beginning with the existing
`implicit-this` overload control and a reduced earliest-owner regression.
Inherited lookup, operators, constructors/lifetime, virtual dispatch, and
broader conversion expansion remain separate checkpoints; this record does
not claim full PA16 completion.

## Checkpoint ledger

| checkpoint | status |
| --- | --- |
| `37265733` typed member projection audit/repair | Completed bounded audit/repair; direct/nested dot and arrow ownership remains traced through PA12, PA11 `RecordLayout::member_offsets`, and PA15 LowIR, with PA16 still incomplete at that checkpoint's existing failure baseline. |
| `b1e8272d` + PA16 typed implicit-object boundary | Prior landed checkpoint record preserved: canonical Function-scope hidden-object ownership, fail-closed viability, typed demand indexing, direct PA15 lowering, focused `6/6` + `2/2`, and reported final `47/243` with `243/243` coverage. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv ranking, N3485 variadic comparison, dense typed PA15 reachability, single-owner demand edges, hidden-object call formation, and source-file sizing are repaired. Focused controls and course regressions pass; broad gates record through-PA15 `1167/1167`, final PA16 `47/243` with `196` failures and `243/243` coverage, zero failure-identity additions/removals, and PA16 still incomplete. |
