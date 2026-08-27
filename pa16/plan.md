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
class/special-member/local-object nodes feed canonical PA11 class `ScopeId`,
field `BindingId`, constructor `BindingId`, and `FunctionFact` records; PA12
preserves scalar, pointer, aggregate, and class-subobject DMI facts and emits
ordered constructor-action ranges; PA15 roots demand at a local object and
lowers direct-base-first/member-declaration-order projections, stores, calls,
and unwind metadata. Explicit mem-initializers override DMIs and synthetic
helpers are demand-driven. This matches the PA16 README boundary lines
215--292 and `spec.md` §§2--5 and §7.

The construction audit repairs also preserve the C++ value-initialization distinction:
implicit and in-class-defaulted constructors receive typed zero-initialization
before their call, while user-provided constructors do not. Empty named class
construction now publishes a real `FunctionFact`; aggregate empty-list
lowering applies direct/nested DMI facts. Dense `NamedRecordId`,
`FunctionFactId`, and semantic-fact caches explicitly track unseen,
in-progress, and complete states, reject cycles, share DAG work, and retain no
fact reference across vector growth.

## Failure Map

Authoritative turn-start full-stage state: `80/243` passing, `163` failures,
and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
Final `make test-pa16` is `80/243` with `163` failures and `243/243` covered;
exact failure-identity additions/removals are `∅`/`∅`, and exact
coverage-identity additions/removals are `∅`/`∅`.  The complete final log is
`/tmp/v3multi-pa16-full-final2.znmVUW.log`; no inline failure list is duplicated
here.

Focused copied-fixture evidence is `10/11`: the eight construction identities,
`200-single-inheritance.t`, and `200-empty-class-member-declaration.t` pass;
`300-value-init-aggregate-with-nontrivial-member.t` remains a baseline
failure because the reference expects a private `i64` bulk store and also has
the unrelated boolean-conversion diff.  Course-404 and controls 400--407 are
green.  The prior static-data increment's six exact removals remain retained
historical progress:

- `pa16/tests/general/100-static-member-object-access.t`
- `pa16/tests/general/200-nested-injected-class-name-hides-base-name.t`
- `pa16/tests/general/200-static-thread-local-member-object-call.t`
- `pa16/tests/general/200-static-thread-local-member.t`
- `pa16/tests/general/300-static-const-member-address.t`
- `pa16/tests/general/300-static-member-definition-private-nested-type.t`

No handout test, checked-in reference, fixture, or `.ref` changed.

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
This checkpoint is complete: focused evidence, broad PA16, exact identity and
coverage comparison, through-PA15, and the file audit are recorded below.
Next implementation checkpoint (not audited here): PA16 parameterized
class-constructor argument lowering.

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

Retained prior landed-checkpoint evidence includes the static-data and
member-demand probes described above.  The final PA16 stage smoke exits `2`
at `80/243` with `163` failures and `243/243` coverage; its complete output is
`/tmp/v3multi-pa16-full-final2.znmVUW.log`.  The exact through-PA15 gate exits
`0` at `1167/1167`, and the final file audit exits `0` with five pre-existing
header `bad-division` warnings.  Earlier landed-checkpoint smoke remains
available at `/tmp/v3multi-pa16-full-final3.qPe5bP.log`.

For this construction diff, DMI ownership is one typed sidecar fact per
canonical field. Across the translation unit, dense runtime demand classifies
each reachable record and its direct member/base dependencies once; dense
nothrow state classifies each reachable constructor fact and semantic edge
once. Lowering uses the completed layout's typed member-offset index. A
13-link single-base/32-member-DMI probe compiled in five runs at `0.00s` each,
with maximum RSS `5824--6056KB`; timing files are in
`/tmp/codex-pa16-stress-final.Tn9MSH` as `stress-1.time` through `stress-5.time`;
its current LowIR has 14 constructor helpers, 14 constructor calls, 13 base
projections, 45 field projections, and 45 typed field stores. Separate
alignment probes exercised `i8` for byte/alignment-one objects, `i32` for
alignment-four objects, and `i64` for alignment-eight objects. These are
representative smoke/structural values, not a benchmark.

## checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 typed class-object construction boundary | Completed bounded audit/repair; final full stage is `80/243` with `163` failures and `243/243` coverage, exact failure and coverage additions/removals are `∅`/`∅`, focused copied comparison is `10/11`, course controls 400--407 are green, through-PA15 is `1167/1167`, and the file audit passes with five pre-existing warnings. |
| PA16 static data storage/access milestone | Completed bounded audit/repair; final full stage is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`; focused 16-fixture evidence is `11/16`. |
| PA16 typed static member-function lookup/emission | Previously completed and retained; selected static-function, inherited, protected, and demand controls remain green. |
| PA16 protected object access / inherited member boundary | Previously completed and retained; course 405 now checks the superseding static-data boundary. |
| PA16 shared semantic DAG / typed owner complexity correction | Completed; shared default facts and repeated transparent casts pass without singleton-parent rejection, and direct redeclaration ownership is BindingId-indexed. |
| Through PA15 | Completed exact required gate at `1167/1167`. |
| PA16 file audit | Completed exact audit with exit `0` and five pre-existing header `bad-division` warnings. |
