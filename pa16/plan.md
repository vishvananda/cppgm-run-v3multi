# PA16 implementation plan

## Stage Design (owner/data flow and spec alignment)

PA11 retains one canonical class-owned static data binding and an O(1)
`BindingId -> owner ScopeId` side index. A qualified out-of-class declaration
resolves its target class scope, merges only the unique directly-owned
`ValueEntry` with matching typed variable identity, linkage, and type, and
carries the definition bit onto that binding. Using imports, unrelated
redeclarations, ambiguous direct entries, inherited/absent targets, and
duplicate definitions fail closed. Declaration facts remain available for
storage, initializer, and TLS metadata; TLS agreement is accumulated across
the declaration/definition boundary, including bindings created by narrow
synthetic paths.

PA12 continues to publish typed member selection for qualified and unqualified
inherited static data, retaining the declaring owner and applying access at
that owner. PA15 makes two deterministic scope/binding collection passes over
namespace variables and class-owned static variables, mapping each canonical
binding to one `global_symbols_` identity.
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

Authoritative turn-start full-stage state: `61/243` passing, `182` failures,
and `243/243` covered. Final authorized validation is `61/243` passing with
`182` failures and `243/243` covered; failure-identity delta is added `∅` and
removed `∅`. The landed increment's six exact removals remain:

- `pa16/tests/general/100-static-member-object-access.t`
- `pa16/tests/general/200-nested-injected-class-name-hides-base-name.t`
- `pa16/tests/general/200-static-thread-local-member-object-call.t`
- `pa16/tests/general/200-static-thread-local-member.t`
- `pa16/tests/general/300-static-const-member-address.t`
- `pa16/tests/general/300-static-member-definition-private-nested-type.t`

The focused 16-fixture probe is `11/16`; its unchanged failures are the three
documented out-of-bound identities below plus the two pre-existing controls.
The exact eight-fixture probe's remaining three identities are
`300-static-class-member-object-definition.t`,
`300-static-member-aggregate-array-dynamic-init.t`, and
`300-thread-local-synthetic-symbol-family-isolation.t`. Their unchanged
diagnostics are respectively unsupported PA11 special-member declaration
form, unsupported PA12 scalar braced initialization, and the same special
member/lifetime boundary. No handout test, reference, or fixture changed.

## Active Checkpoint

Completed checkpoint: bounded audit/repair milestone for the typed static
scalar/class storage boundary. It covers direct definition merging,
defined and demand-driven declaration-only class globals, inherited/nested
owner continuity, static object evaluation/global lowering, storage-free
integral folding, address/reference storage demand, declaration+definition
de-duplication, TLS wrapper metadata, and nested member-demand traversal.
Course regressions 400--407 are green, and the exact through-PA15 gate is
`1167/1167`; no handout or reference was changed.

The broader class-object constructor, aggregate-array dynamic initialization,
and TLS guard/init lifetime path remains explicitly deferred rather than
synthesized ad hoc. Final broad validation and the exact file audit are clean
within the bounded baseline, so the next checkpoint is the class-object and
lifetime boundary.

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

The landed-stage smoke evidence recorded an eight-fixture probe at `0.38s` and
`24008KB` maximum RSS, and a no-rebuild full PA16 run at `0.86s` and `24092KB`
with the turn-start `61/243`, `182`, `243/243` result; it is retained as prior
evidence. Final smoke measurements are `407: 0.08s, 7256KB`, the 16-fixture
probe: `0.38s, 24152KB`, and full PA16: `0.87s, 24092KB` maximum RSS. The
through-PA15 gate is `1167/1167`; the file audit exits `0` with five
pre-existing header `bad-division` warnings. All are smoke measurements, not
formal benchmarks. Typed probes verified class initializer preservation,
inherited/nested owner continuity, storage-free constant folding,
address/reference-triggered materialization, duplicate/incompatible
definition rejection, and TLS wrapper metadata/collision isolation.

## checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 static data storage/access milestone | Completed bounded audit/repair; final full stage is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`; focused 16-fixture evidence is `11/16`. |
| PA16 typed static member-function lookup/emission | Previously completed and retained; selected static-function, inherited, protected, and demand controls remain green. |
| PA16 protected object access / inherited member boundary | Previously completed and retained; course 405 now checks the superseding static-data boundary. |
| PA16 shared semantic DAG / typed owner complexity correction | Completed; shared default facts and repeated transparent casts pass without singleton-parent rejection, and direct redeclaration ownership is BindingId-indexed. |
| Through PA15 | Fresh exact required gate `1167/1167`. |
| PA16 file audit | Fresh exact required gate exits `0` with the five pre-existing header `bad-division` warnings and no fatal findings. |
