# PA16 implementation plan

## Stage Design/spec alignment and owner/data flow

The checkpoint keeps one typed path from a PA10
`CallExpression(MemberExpression)` through PA12 to PA15.  PA12 resolves the
direct class value-table entry set, admits only ordinary non-static member
functions, and checks object cv compatibility and explicit-argument
conversion/default viability.  It accepts the result only when exactly one
candidate is viable, then checks access on that selected binding; access does
not remove candidates before ambiguity is determined.  The selected `BindingId` and hidden-object
`callable_type` are canonical semantic identities.

`prepare_pa12_member_parameter` owns the synthetic object parameter and stores
its exact `BindingId` in the canonical member function `Scope`.  PA12's
`implicit_this_binding` walks lexical parents only to that exact Function
`Scope` and reads the stored field.  A successful member call publishes the
selected binding, callable type, and a typed semantic child at child zero for
the implicit object; converted explicit arguments follow it.  PA12 also uses
the same typed `this` fact for unqualified member-field expressions, while
direct field projection continues through the existing path.

PA12 records each successful selected member binding once in its typed
`BindingId -> bool` demand index.  PA15 collects function facts in its
ordinary fixed pass and uses that index to demand class methods; it does not
scan `semantic_facts_` to discover member calls or retry from rendered text.
PA15 validates the selected
`FunctionFact`, owner, exact Function-scope object binding, and hidden-object
signature.  It lowers a dot object with `lower_address` or an arrow pointer
with `lower_expression` exactly once, prepends that value as `this`, then
emits the normal typed direct call.

## Failure Map

The immutable turn-start baseline at HEAD `b1e8272d` was `38/243` passing,
`205` failing, and `243/243` covered.  Final `make test-pa16` evidence is
`47/243` passing, `196` failing, and `243/243` covered: nine failure
identities were removed, no passing identity was lost, and no new failure
identity was added.

The exact baseline failure identities now passing are:

- `general/100-member-methods.t`
- `general/100-out-of-class-methods.t`
- `general/100-this-arrow-member-binds-lvalue-ref.t`
- `general/100-using-directive-imported-value-method-body.t`
- `general/200-member-call-return-type-overload-arity.t`
- `general/200-member-function-default-arguments.t`
- `general/200-method-cv-overload-preference.t`
- `general/200-parenthesized-noexcept-member-definition.t`
- `general/300-const-method-array-member-binds-const-reference.t`

Focused recheck results were `6/6` for the historical direct/member-field
set and `2/2` for the `this->a` implicit-this and private-access controls.
The two focused groups include explicit user parameters and the explicit
arrow-call default-argument case.  Remaining PA16 failures are outside this
boundary, including initialization/special-member paths, inherited members,
and broader operator/overload behavior.

## Active Checkpoint/current checkpoint

This is a coherent typed implicit-object boundary for ordinary direct `.` and
`->` member calls, not a claim that the PA16 assignment is complete.  The
selection boundary is deliberately fail-closed: after object cv and explicit
argument viability (including supported defaults), zero viable candidates is
an error and more than one viable candidate is deferred as an ambiguous
overload set.  General all-argument overload ranking is not claimed.

Inherited lookup, ref-qualified methods, operators, constructors,
destructors, virtual dispatch, and speculative overload semantics remain
deferred.  The class-access side fact preserves rejection of inaccessible
direct methods without changing the direct-field projection behavior.

## Performance Evidence (including uncertainties)

For `C` local candidate entries, `A` explicit arguments, and `P` parameters,
member viability is bounded by the local lookup and approximately
`O(C * (P + A))`, with existing indexed value/type operations; no whole
program candidate scan or retry was added.  The typed `BindingId` demand
`FlatIndex` deduplicates successful calls with one indexed membership check and
typed insertion for a new demand.  The ordinary PA15 function collection pass
still visits the fixed function-fact set once and performs typed demand-index
membership checks; it is not a semantic-fact discovery scan.

The selected object is lowered once per call, then explicit children are
lowered in order.  No timing, RSS, allocation, or absolute-complexity
measurement was collected, so this is structural evidence only.  The source
audit passes with five pre-existing `bad-division` warnings for shared
headers.

## Checkpoint Ledger

| checkpoint | status |
| --- | --- |
| `37265733` typed member projection audit/repair | Completed bounded audit/repair; prior focused and broad invariants passed, with PA16 still at its pre-checkpoint incomplete baseline. |
| `b1e8272d` + PA16 typed implicit-object boundary | Completed coherent checkpoint after canonical Function-scope hidden-object ownership, fail-closed unique viability, typed demand indexing, direct PA15 lowering, focused `6/6` + `2/2`, final `47/243` (`196` failures), `243/243` coverage, through-PA15 `1167/1167`, and a passing PA16 source audit. |
