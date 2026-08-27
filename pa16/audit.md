# PA16 checkpoint audit

## Current Checkpoint Review

This review covers landed commit `0b534f2fc163af1817d679eed484f0c210b54891`
relative to parent `b1e8272d`, including the completed audit repairs for its
typed direct non-static member-call boundary.  Unqualified member-call lookup,
inherited lookup, operators, constructors/lifetime, virtual dispatch,
ref-qualified methods, and unrelated PA16 surfaces remain outside this review.

The owned path is:

```text
PA10 CallExpression(MemberExpression) syntax
  -> PA12 member-call probe and typed candidate/argument facts
  -> PA11/PA12 access, cv-compatible implicit-object, default, and conversion
     owners, plus the exact Function-scope hidden-object BindingId
  -> reachable FunctionFact body roots and typed member-call demand edges
  -> PA15 direct symbol, ABI, hidden-first-object lowering
  -> LowIR call with the object followed by explicit arguments
```

### Findings

- `semantic_member_call_expression` uses the direct class value-table entries
  and admits ordinary non-static functions.  Dot requires an lvalue record
  object; arrow requires a pointer and preserves its typed pointer conversion.
  The object is semantic child zero and explicit arguments follow it in source
  order.
- The landed selector deferred every viable member overload set.  The repair
  ranks the implicit object as the first conversion sequence, checks
  qualification compatibility before ranking, and retains the existing typed
  explicit-argument/default/conversion facts.  Equal best candidates fail as
  ambiguous rather than being selected by collection order.
- The member comparator now follows the bounded N3485 conversion ordering: a
  non-variadic candidate receives no blanket preference when no ellipsis
  conversion is used, while an actual variadic argument receives the worst
  conversion rank.  The course regression covers both the positive ellipsis
  path and the no-ellipsis ambiguity.
- Access is owned by the PA11 binding sidecar and checked on the selected
  binding after viability and ranking.  Default arguments are checked before
  selection and materialized only for the selected binding.  The focused
  private-method, default-argument, cv-selection, and return/arity controls
  pass.
- `prepare_pa12_member_parameter` creates the synthetic object parameter once
  and stores its exact `BindingId` in the member Function `Scope`.  PA12
  publishes a callable Function type with that hidden pointer first; PA15
  validates the selected FunctionFact, owner, exact hidden binding, parameter
  list, result, and object qualification before lowering.
- Emission demand has one semantic owner: a successful typed
  `CallExpression` with `has_implicit_object` and a selected `BindingId`, when
  reachable from a FunctionFact body that `collect_functions` emits, is the
  member demand edge.  The redundant global `BindingId -> bool` demand index,
  its writer, and its PA15 integrity check were removed.  The existing typed
  `function_binding_fact_index_` is only the binding-to-FunctionFact identity
  lookup needed to follow that edge; it is not a second demand owner.
- PA15 seeds exactly the namespace FunctionFacts eligible under the same
  namespace-owner and valid-function-scope predicate used by
  `collect_functions`.  It follows selected member targets as typed
  `FunctionFactId` work items.  Dense byte vectors mark scanned functions,
  scanned semantic facts, and demanded class functions, so each reachable
  function/fact is processed once.  It does not scan class bodies that are not
  reached from an emitted namespace root, recover names, or retry the program.
- Dot lowering takes one object address, while arrow lowering evaluates one
  pointer expression; explicit argument children are lowered afterward in
  source order.  Direct symbol lookup, ABI cv mangling, and the hidden first
  object argument remain typed.  Ordinary free calls and indirect calls retain
  their prior path.
- `SemanticTailGuard` rolls back failed member probes' fact, child, name,
  conversion, and literal arenas.  Demand is published only by the committed
  typed fact edge, so failed speculative probes do not create emission demand.
- Moving the reachability helper into `pa15_lowering_calls.cpp` repaired the
  source-file size gate without changing the source set.  The final PA16 file
  audit passes with the five pre-existing `bad-division` warnings in
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`.

## Focused Evidence

- `make -C pa16 check TEST='tests/general/200-method-cv-overload-preference.t tests/general/200-member-function-default-arguments.t tests/general/200-member-call-return-type-overload-arity.t tests/general/100-private-method-bad.t tests/general/100-member-methods.t tests/general/100-out-of-class-methods.t'`
  exits `0` with `6/6` passing after the final source organization repair.
- `make -C pa15 check TEST='tests/general/100-simple-call.t tests/general/100-function-pointer-ref-call.t tests/general/200-extern-function-pointer-indirect-call.t'`
  exits `0` with `3/3` passing.
- The relevant course regressions
  `400-typed-layout-boundary-regression.sh`,
  `401-typed-member-projection-boundary-regression.sh`, and
  `402-typed-member-call-demand-roots-regression.sh` each exit `0`.
  The new call regression checks both cv-selected ABI symbols and their
  hidden-object call edges, transitive member demand, exactly-once helper
  emission, unreachable-member suppression, actual ellipsis lowering, and
  no-ellipsis variadic ambiguity.
- `sh -n cppgm.tests/course/pa16/402-typed-member-call-demand-roots-regression.sh`
  and `git diff --check` pass.
- The representative handout
  `200-member-call-implicit-object-cv-overload.t` gets past the original
  deferred-overload defect but still fails later in the pre-existing aggregate
  class-brace initialization path (`PA12 invalid conversion` at `Cell cell =
  {7}`).  The unqualified `implicit-this` overload test fails in its separate
  unqualified member-call path and is not changed here.

The authorized broad gates produce the following final evidence:

- `n=16; ... make test-report-through-pa$((n - 1))` exits `0` with
  `1167 / 1167` through PA15.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`:
  file audit passed with the five warnings listed above.
- `make test-pa16` exits `2` with `47 / 243` passing, `196` failures, and
  `243 / 243` covered.  Against
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`,
  the final failure identity set has `0` additions and `0` removals; therefore
  no turn-start passing identity regressed.  The unchanged failure count does
  not claim full PA16 completion.

## Performance and Boundaries

For `C` local candidates, `A` explicit arguments, and `P` parameters, member
viability/ranking is approximately `O(C * (P + A))` after direct class-table
lookup.  PA15 uses typed `FunctionFactId` and `SemanticFactId` worklists plus
dense byte vectors over the existing fact domains.  Each reachable function
and semantic fact is scanned once, so demand traversal is linear in the
reachable function/fact graph and has no name-recovery scan or whole-program
retry.  The final implementation keeps `pa15_lowering.cpp` at 2964 lines,
below the file-audit limit.

No timing, RSS, allocation, or structural-counter measurement was collected;
these are structural bounds, not numerical performance claims.  The bounded
remaining uncertainties are numerical performance on unusually large fact
graphs and the explicitly separate unqualified, inherited/protected/friend,
static-call, constructor/lifetime, virtual, ref-qualified, and broader
conversion/overload slices.  The typed member-demand ownership itself has no
speculative-demand side-index uncertainty remaining.

## Audit ledger

| checkpoint | result and disposition |
| --- | --- |
| `37265733` typed member projection audit/repair | Direct/nested dot and arrow ownership is traced through PA12, PA11 `RecordLayout::member_offsets` keyed by the object's canonical `NamedRecordId`, and PA15 LowIR; the reference-cv and class anonymous-injection defects are repaired. Broad validation and exact identity/coverage checks pass their bounded invariants; PA16 remains incomplete with the existing 205 failures. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv ranking, N3485 variadic comparison, typed reachable member demand, dense PA15 reachability metadata, hidden-object call formation, and source-file sizing are repaired. Focused PA16/PA15 controls and all relevant course regressions pass; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and full PA16 remains `47/243` with `196` failures and `243/243` coverage, with zero failure-identity additions or removals. |
