# PA16 implementation plan

## Stage Design (owner/data flow and spec alignment)

PA11 retains one canonical class-owned static data binding. A qualified
out-of-class declaration resolves its target class scope, merges only the
unique directly-owned `ValueEntry` with matching typed variable identity,
linkage, and type, and carries the definition bit onto that binding. Using
imports, unrelated redeclarations, ambiguous direct entries, and duplicate
definitions fail closed. Declaration facts remain available for storage,
initializer, and TLS metadata; TLS agreement is accumulated across the
declaration/definition boundary.

PA12 continues to publish typed member selection. PA15 makes one deterministic
scope/binding collection pass over namespace variables and class-owned static
variables, mapping each canonical binding to one `global_symbols_` identity.
Referenced declaration-only class statics receive one opaque
`GlobalDeclaration`; defined statics receive one `GlobalDefinition`. Static
member object operands are evaluated once, then static addresses use the
canonical global instead of a field projection. A value-used in-class
integral constant remains an immediate typed value without storage; address,
reference, or nonconstant use demands storage. TLS class statics receive one
collision-safe typed wrapper declaration with `tls_for` and ABI wrapper/object
metadata. No static `selected_scope` claim is added to ordinary PA12
`IdExpression` facts where that fact is not published.

The storage-demand index now validates child/conversion ranges and the typed
semantic graph once, then uses a dense cast worklist: shared facts are valid,
each transparent cast is classified at most once, and only actual cycles fail
closed. The label prepass likewise memoizes completed shared expression facts;
it retains the first structural path only for its label-recovery data. No
rendered-name recovery, parent singleton assumption, or per-use ancestor walk
remains.

This follows `spec.md` §§2--5: hot equality is BindingId/ScopeId identity,
collection is dense and near-linear, wrapper/global emission is demand-aware,
and no rendered-name recovery, whole-program retry, or per-expression value
scan was introduced. Nested class function preparation was extended so the
existing PA15 member-demand worklist can reach a static-data use in a nested
member body.

## Failure Map

Authoritative turn-start: `55/243` passing, `188` failures, `243/243` covered.
Final validation: `61/243` passing, `182` failures, `243/243` covered.
Failure identity delta: added `∅`; removed exactly:

- `pa16/tests/general/100-static-member-object-access.t`
- `pa16/tests/general/200-nested-injected-class-name-hides-base-name.t`
- `pa16/tests/general/200-static-thread-local-member-object-call.t`
- `pa16/tests/general/200-static-thread-local-member.t`
- `pa16/tests/general/300-static-const-member-address.t`
- `pa16/tests/general/300-static-member-definition-private-nested-type.t`

The exact eight-fixture probe is `5/8`; its remaining three identities are
`300-static-class-member-object-definition.t`,
`300-static-member-aggregate-array-dynamic-init.t`, and
`300-thread-local-synthetic-symbol-family-isolation.t`. Their unchanged
diagnostics are respectively unsupported PA11 special-member declaration
form, unsupported PA12 scalar braced initialization, and the same special
member/lifetime boundary. No handout test, reference, or fixture changed.

## Active Checkpoint

Completed typed static scalar/storage boundary: direct definition merging,
defined and demand-driven declaration-only class globals, static object
evaluation/global lowering, storage-free integral folding, static address
storage, declaration+definition de-duplication, TLS wrapper metadata, and
nested member-demand traversal. The broader class-object constructor,
aggregate-array dynamic initialization, and TLS guard/init lifetime path
remains explicitly deferred rather than synthesized ad hoc.

Prior controls remain in scope: the selected PA15 namespace-global probe is
`4/4`, the selected PA16 namespace/static-function/inherited/protected handout
probe is `6/6`, and course regressions 401--406 exit zero. Course 405 now
asserts the required protected-static success and absence of field projection;
this is the one legitimate course-control correction, with no handout or ref
mutation.

## Performance Evidence

`index_global_storage_demands` validates the semantic graph in `O(F + E)` and
scans the conversion arena in `O(V)`, where `F` is fact count, `E` is the
typed child-edge count, and `V` is the typed conversion-entry count. Its
demand worklist visits each transparent cast at most once, so the total is
`O(F + E + V)` rather than `O(K * depth)` and it tolerates arbitrary DAG
sharing. Binding-owner validation is `O(1)` per `ValueEntry`; it no longer
rescans a scope's complete binding vector for every redeclaration. Global
setup is two deterministic scope/binding passes, `O(S + B)`; typed global
lookup and storage selection remain `O(1)` identity checks. TLS wrapper
emission is one set membership check per collected TLS binding.

Representative measurements after the correction: the eight-fixture probe
completed in `0.38s` with maximum RSS `24008KB` and reported only the three
documented remaining identities. The full no-rebuild `make test-pa16` run
covered all 243 fixtures in `0.86s` at `24092KB` maximum RSS and reported the
exact `61/243`, `182`, `243/243` result above. Course 402's shared-default,
repeated-cast regression also passed. These are smoke measurements, not
formal benchmarks. Typed probes verified namespace `extern`→definition
merging, duplicate namespace definition rejection, storage-free constant
folding, address-triggered declaration materialization, and TLS metadata
retention when the definition omits `thread_local`.

## checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 static data storage/access milestone | Completed; final broad identity is `61/243` with `182` failures, six exact removals, no additions, and `243/243` coverage. |
| PA16 typed static member-function lookup/emission | Previously completed and retained; selected static-function, inherited, protected, and demand controls remain green. |
| PA16 protected object access / inherited member boundary | Previously completed and retained; course 405 now checks the superseding static-data boundary. |
| PA16 shared semantic DAG / typed owner complexity correction | Completed; shared default facts and repeated transparent casts pass without singleton-parent rejection, and direct redeclaration ownership is BindingId-indexed. |
| Through PA15 | Final required gate `1167/1167`. |
| PA16 file audit | Final required gate passes with the five pre-existing header bad-division warnings and no fatal findings. |
