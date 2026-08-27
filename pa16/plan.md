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

The current construction checkpoint keeps the owner/data flow typed: PA10
class-member and constructor nodes feed canonical PA11 class `ScopeId`, field
`BindingId`, and constructor `BindingId` records; PA12 preserves each
brace-or-equal DMI as a semantic fact and publishes ordered constructor-action
ranges; PA15 roots demand at a local class object and lowers direct-base-first,
declaration-order field projections, stores, and calls. Explicit mem-initializers
override DMIs, synthetic helpers are demand-driven, and constructor unwind
metadata follows the emitted calls. This aligns with the PA16 README
construction contract and `spec.md` §§2--5 plus §7.

Constructor runtime demand is cached in dense `NamedRecordId` state with
explicit unseen/in-progress/complete transitions; constructor identity and
availability come from the per-record side index. Synthetic-constructor
nothrow analysis uses dense `FunctionFactId` and semantic-fact caches with an
iterative, cycle-conservative worklist. Function facts are published from
local values or reacquired by ID, with no AST-count reserve or retained fact
reference across vector growth.

## Failure Map

Authoritative turn-start full-stage state: `61/243` passing, `182` failures,
and `243/243` covered. Final full-stage state is `80/243` passing, `163`
failures, and `243/243` covered. The full command exits `2` because the
remaining failures are still baseline failures; no previously passing
baseline identity regressed. The exact failure-identity delta is 19 removed
and none added:

- `tests/general/100-default-member-initializer-aggregate-member.t`
- `tests/general/100-default-member-initializer-class-member.t`
- `tests/general/100-default-member-initializer-scalar-brace.t`
- `tests/general/100-default-member-initializer-scalar.t`
- `tests/general/100-default-member-initializer-user-ctor.t`
- `tests/general/100-defaulted-constructor-default-member-initializer.t`
- `tests/general/200-base-field-access.t`
- `tests/general/200-constructor-member-init.t`
- `tests/general/200-empty-class-member-declaration.t`
- `tests/general/200-in-class-member-initializer.t`
- `tests/general/200-member-initializer-aggregate-member.t`
- `tests/general/200-member-initializer-overrides-default-member-initializer.t`
- `tests/general/200-out-of-class-member-default-argument.t`
- `tests/general/200-parenthesized-member-call.t`
- `tests/general/200-pointer-member-zero-brace-init.t`
- `tests/general/200-pointer-member-zero-paren-init.t`
- `tests/general/200-single-inheritance.t`
- `tests/general/300-nested-class-private-member-call.t`
- `tests/spec/200-nested-class-enclosing-access.t`

Added identities: `∅`. The eight focused identities passed normalized LowIR
comparison (`8/8`):

- `100-default-member-initializer-scalar.t`
- `100-default-member-initializer-scalar-brace.t`
- `100-default-member-initializer-aggregate-member.t`
- `100-default-member-initializer-class-member.t`
- `100-default-member-initializer-user-ctor.t`
- `100-defaulted-constructor-default-member-initializer.t`
- `200-in-class-member-initializer.t`
- `200-member-initializer-overrides-default-member-initializer.t`

Course-404 and the existing course controls 400--407 passed, retaining the
state-free no-eager-helper control, unused-DMI no-helper check, and demanded
base/member construction checks. The prior static-data increment's six
exact removals remain retained historical progress:

- `pa16/tests/general/100-static-member-object-access.t`
- `pa16/tests/general/200-nested-injected-class-name-hides-base-name.t`
- `pa16/tests/general/200-static-thread-local-member-object-call.t`
- `pa16/tests/general/200-static-thread-local-member.t`
- `pa16/tests/general/300-static-const-member-address.t`
- `pa16/tests/general/300-static-member-definition-private-nested-type.t`

No handout test, checked-in reference, or fixture changed. Full output was
preserved at `/tmp/v3multi-pa16-full-final3.qPe5bP.log`.

## Active Checkpoint

Active checkpoint: typed class-object construction rooted at a demanded local
object. Included are canonical scalar/pointer empty-brace and scalar DMI
facts, supported aggregate and class-subobject DMI, implicit and explicitly
defaulted default constructors, in-class user-ctor prefix actions,
direct-base-first/member-declaration-order actions, explicit DMI override,
demanded helper emission, and truthful unwind metadata. Excluded are
copy/value semantics, out-of-class constructor/destructor definitions, virtual
or multiple inheritance, parameterized class-constructor argument lowering,
global/TLS lifetime and guards, and unrelated operators/ADL/access work.
The earlier static-data checkpoint and its through-PA15 `1167/1167` evidence
are retained as prior gates; the final through-PA15 gate also passes
`1167/1167`.

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

Retained earlier smoke evidence includes the static-data and member-demand
probes described above. Final broad PA16 smoke measured `/usr/bin/time` wall
`1.04s` and maximum RSS `20824KB`; its complete output is
`/tmp/v3multi-pa16-full-final3.qPe5bP.log`. The final through-PA15 gate is
`1167/1167`; the file audit exits `0` with five pre-existing header
`bad-division` warnings. All are smoke measurements, not formal benchmarks.
Typed probes verified class initializer preservation, inherited/nested owner
continuity, storage-free constant folding, address/reference-triggered
materialization, duplicate/incompatible definition rejection, and TLS wrapper
metadata/collision isolation.

For this construction diff, DMI ownership is one typed sidecar fact per
canonical field. Across the translation unit, dense runtime demand classifies
each reachable record and its direct member/base dependencies once; dense
nothrow state classifies each reachable constructor fact and semantic edge
once. Lowering uses the completed layout's typed member-offset index. A
temporary probe with a 13-link single-base chain and a 32-member DMI class
measured `/usr/bin/time` wall `0.00s`, maximum RSS `5888KB`, and structural
counts of `14` constructor helpers, `14` constructor calls, `13` base
projections, `45` field projections, and `45` typed field stores. This is smoke
evidence, not a benchmark.

## checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 typed class-object construction boundary | Completed; final full stage is `80/243` with `163` failures and `243/243` coverage, 19 baseline failures removed and none added, focused normalized comparison `8/8`, course controls 400--407 green, and through-PA15 `1167/1167`. |
| PA16 static data storage/access milestone | Completed bounded audit/repair; final full stage is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`; focused 16-fixture evidence is `11/16`. |
| PA16 typed static member-function lookup/emission | Previously completed and retained; selected static-function, inherited, protected, and demand controls remain green. |
| PA16 protected object access / inherited member boundary | Previously completed and retained; course 405 now checks the superseding static-data boundary. |
| PA16 shared semantic DAG / typed owner complexity correction | Completed; shared default facts and repeated transparent casts pass without singleton-parent rejection, and direct redeclaration ownership is BindingId-indexed. |
| Through PA15 | Final exact required gate passes `1167/1167`. |
| PA16 file audit | Final audit exits `0` with the same five pre-existing header `bad-division` warnings. |
