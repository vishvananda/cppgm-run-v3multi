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

The active PA16 parameterized-constructor boundary keeps the initializer's
typed `=` context in PA10, constructor explicitness and special-member
default arguments in PA11 sidecars/facts, and the canonical class owner,
constructor binding, access scope, and argument-node sequence at PA12.  A
shared typed function-selection primitive now serves ordinary direct calls
and constructors: it performs one bounded candidate score, reference-aware
conversion ranking, function-id target resolution, trailing-default
publication, ambiguity check, context conversion, and variadic conversion.
Constructor-specific policy is limited to canonical per-name
`Scope::values[record.name]` collection and owner/origin/identity validation,
explicit-context filtering/rejection, class-by-value exclusion, constructor
access, and publication of the hidden destination plus constructor callable
type.  PA15 consumes the selected converted/defaulted facts in source order.
This aligns the PA16 README constructor boundary with `spec.md` §§2--5 and
§7 without name recovery, whole-scope rescans, or retry loops.

## Failure Map

Authoritative turn-start full-stage baseline: `80/243` passing, `163`
failures, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The fresh post-extraction stage log is
`/tmp/v3multi-pa16-parameterized-constructor-post-extraction-final.log`: `91/243`
passing, `152` failures, and `243/243` covered.  All original 243 local test
identities remained covered; exact failure comparison has zero additions and
these 11 removals:

- `pa16/tests/general/200-local-class-direct-init-free-function.t`
- `pa16/tests/general/200-local-class-direct-init-member-function.t`
- `pa16/tests/general/200-local-class-direct-init-inherited-member-call.t`
- `pa16/tests/general/200-local-class-direct-init-parameter-hides-type.t`
- `pa16/tests/spec/200-direct-list-init-explicit-ctor.t`
- `pa16/tests/general/200-constructor-overload-default-arg-nonfirst-argument.t`
- `pa16/tests/general/200-base-default-argument-constructor-action.t`
- `pa16/tests/general/200-copy-init-explicit-ctor-overload-refinement.t`
- `pa16/tests/general/200-derived-base-constructor-member-init.t`
- `pa16/tests/general/200-derived-method-hides-base-field-call.t`
- `pa16/tests/general/200-implicit-member-call-suppresses-adl.t`

Focused required controls passed `8/8`; the preservation suite passed `7/7`.
The separate course control
`cppgm.tests/course/pa16/408-typed-constructor-explicit-context-regression.sh`
passed and is outside the root PA16 243-test count.  It covers copy-init
exclusion, copy-list explicit-winner rejection, and a later-declared
constructor default visible to an earlier in-class body.  The direct
base/member probe emitted `@Base__Base(%t2, %t3)` and
`@Member__Member(%t5, %t6)`.  No handout test, checked-in reference, fixture,
or `.ref` changed.

The exact through-PA15 gate passed `1167/1167`; its log is
`/tmp/v3multi-through-pa15-parameterized-constructor-post-extraction-final.log`.
`git diff --check` passed.  The audit-only correction reduced
`pa11_semantic_core.cpp` to 2965 lines and `pa11_semantic_model.h` to 2352
lines.  The exact post-extraction file audit passes with five pre-existing
`bad-division` warnings; its log is
`/tmp/v3multi-pa16-file-audit-post-extraction-final.log`.
No unrelated external string-literal failure is claimed as fixed.

## Active Checkpoint

Active checkpoint: typed parameterized class-constructor selection and
lowering at a demanded local class object, for direct parenthesized and
direct-list initialization, including the four local name/owner cases,
explicit-constructor context, overload/default ranking, reference binding,
function-id targets, source-order argument evaluation, and one direct base or
class member constructor action.  Status is complete: focused controls,
fresh stage progress, through-PA15, audit, and diff-check all pass; the
single coherent commit follows this plan update.

Parameterized default member-initializer construction is excluded: existing
DMI facts remain supported, but a nonempty class DMI does not enter this
PA16 constructor-argument selection path.  Copy/move construction and class
pass/return by value, out-of-class constructor/destructor definitions,
virtual or multiple inheritance, template-backed cases, global/TLS lifetime
and guards, unrelated operators/ADL, and unrelated external string-literal
address lowering are also excluded.

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

For one use, canonical candidate lookup is `O(C)` over
`Scope::values[record.name]`, independent of unrelated class bindings.
Shared scoring/conversion/default publication is `O(C*A)` and typed argument
storage plus one lowering pass is `O(A)`, so selector/storage/lowering is
`O(C*A + A)` in the normal argument-bearing case, with the explicit `O(C)`
walk retained for zero-argument/default-only calls.  `C` is constructor
overload count and `A` is explicit argument count.  Defaults are published
once in the class prepass; there is no per-use whole-scope scan or retry.

Fresh smoke/scale input `/tmp/pa16-constructor-selector-scale.cpp` was run
five times with the same built `./dev/cppgm++`; timing/output artifacts are in
`/tmp/pa16-constructor-selector-scale-reduced-final-*`.  It has 854 lines,
26169 bytes, 480 unrelated field bindings, 120 unrelated methods, 8
constructor overload declarations, and 240 constructor invocations.  The
first output has 24437 bytes, 740 lines, and 240 lowered constructor calls.

| sample | wall (s) | user (s) | system (s) | max RSS (KB) |
| --- | ---: | ---: | ---: | ---: |
| 1 | 0.04 | 0.02 | 0.01 | 12004 |
| 2 | 0.03 | 0.02 | 0.01 | 11868 |
| 3 | 0.03 | 0.02 | 0.01 | 11908 |
| 4 | 0.03 | 0.02 | 0.01 | 11940 |
| 5 | 0.03 | 0.02 | 0.00 | 11976 |

These are representative smoke/scale samples, not a benchmark claim; no
unrelated historical stress probe is evidence for this selector.

## checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 typed parameterized class-constructor boundary | Complete: focused `8/8`, preservation `7/7`, course 408 pass, post-extraction stage `91/243` with 152 failures, 11 exact baseline removals, 0 additions, `243/243` original coverage, through-PA15 `1167/1167`, audit pass with five pre-existing warnings, and diff-check pass. |
| PA16 typed class-object construction boundary | Prior completed milestone retained as history; its earlier stage and audit claims are not current-gate evidence. |
| PA16 static data storage/access milestone | Completed bounded audit/repair; final full stage is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`; focused 16-fixture evidence is `11/16`. |
| PA16 typed static member-function lookup/emission | Previously completed and retained; selected static-function, inherited, protected, and demand controls remain green. |
| PA16 protected object access / inherited member boundary | Previously completed and retained; course 405 now checks the superseding static-data boundary. |
| PA16 shared semantic DAG / typed owner complexity correction | Completed; shared default facts and repeated transparent casts pass without singleton-parent rejection, and direct redeclaration ownership is BindingId-indexed. |
| Through PA15 | Completed exact required gate at `1167/1167`. |
| PA16 file audit | Current exact audit passes with five pre-existing `bad-division` warnings after the bounded helper move and declaration-header extraction. |
