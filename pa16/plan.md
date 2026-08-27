# PA16 implementation plan

## Stage Design (owner/data flow and spec alignment)

This checkpoint owns the typed path from a class-object initializer to its
constructor call.  PA10 retains the initializer's `=` token, preserving the
direct/copy/copy-list context as a fact.  PA11 publishes one canonical class
`ScopeId`, field/constructor `BindingId`s, constructor `FunctionFact`s,
explicitness, and default-argument facts.

PA12 collects constructors only from the owning class-name value list and
validates `ValueEntry` origin, binding owner, constructor record, type, and
duplicate identity.  The shared typed selector ranks reference-aware
conversions, contextual and variadic arguments, function-id targets, and
trailing defaults.  It then records selected conversion/default facts in
source order, applies constructor access, and publishes the hidden destination
and callable `TypeId`.  Aggregate-eligible copy/direct-list forms with only
explicitly-defaulted/deleted constructors stay on the typed aggregate path.

PA15 demands only reachable constructor definitions and consumes
`[hidden-destination, converted-explicit-arguments, default-arguments]` in
that order.  Constructor actions retain base-first/member-declaration-order
`BindingId` and arena ranges; explicit member initializers override DMIs.
Member lists and signature values are copied before arena growth, and calls,
projections, stores, and unwind metadata use the retained typed identities.

This matches the PA16 README Assignment Boundary/Out Of Scope clauses around
lines 215--292 and `spec.md` §§2--5 and §7: equality and ownership stay on
typed IDs, candidate work is bounded and deterministic, LowIR is not rebuilt
from text, and no whole-scope retry or test-specific shortcut is used.

## Failure Map

The authoritative checkpoint-turn-start full-stage map was `91/243` passing,
`152` failures, and `243/243` coverage, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` log is
`/tmp/v3multi-pa16-fb4348b-final.log`: `91/243` passing, `152` failures, and
`243/243` coverage.  Normalized failure identities are unchanged: additions
`∅`, removals `∅`; coverage identities are also unchanged: additions `∅`,
removals `∅`.

## Active Checkpoint

Active checkpoint: typed parameterized class-constructor selection and
lowering at a demanded local class object, for direct parenthesized and
direct-list initialization, including the four local name/owner cases,
explicit-constructor context, overload/default ranking, reference binding,
function-id targets, source-order argument evaluation, and one direct base or
class member constructor action.  This checkpoint is complete: focused checks,
the exact PA16 stage, through-PA15, file audit, and diff-check all passed their
bounded criteria, and the final commit records the result.

Parameterized default member-initializer construction is excluded: existing
DMI facts remain supported, but a nonempty class DMI does not enter this
PA16 constructor-argument selection path.  Copy/move construction and class
pass/return by value, out-of-class constructor/destructor definitions,
virtual or multiple inheritance, template-backed cases, global/TLS lifetime
and guards, unrelated operators/ADL, and unrelated external string-literal
address lowering are also excluded.

Next implementation checkpoint: the next bounded PA16 increment is
parameterized class default-member-initializer constructor
selection/lowering.  It continues the same typed class-owner,
constructor-argument, and ordered constructor-action path, within the PA16
Assignment Boundary and Out Of Scope limits.  PA17 value semantics begins
only after the remaining PA16 increments are complete.

## Performance Evidence

For `C` constructor candidates and `A` explicit arguments, canonical local
class-name collection and aggregate eligibility are `O(C)`, shared selection
and conversion work is `O(C*A)`, and selected argument storage/lowering is
`O(A)`. Defaults are published once in the class prepass. PA12/PA15 use dense
typed IDs and ranges, with no whole-scope retry or textual/name recovery;
snapshotting avoids stale arena references. These are structural smoke/scale
claims, not benchmark comparisons.

The bounded scale input `/tmp/pa16-constructor-selector-scale.cpp` contains
`853` lines and `20175` bytes: `480` unrelated fields, `120` methods, `8`
constructor candidates, and `240` six-argument constructions. Each of five
outputs contains one `Scale` constructor helper and `240` constructor calls;
sample 1 is `770` lines and `27742` bytes.

| sample | wall (s) | user (s) | system (s) | max RSS (KB) |
| --- | ---: | ---: | ---: | ---: |
| 1 | 0.03 | 0.01 | 0.01 | 13672 |
| 2 | 0.03 | 0.01 | 0.01 | 13408 |
| 3 | 0.03 | 0.02 | 0.01 | 13692 |
| 4 | 0.03 | 0.02 | 0.01 | 13604 |
| 5 | 0.03 | 0.02 | 0.00 | 13460 |

The focused 409 smoke measured wall `0.04s`, user `0.01s`, system `0.02s`,
and maximum RSS `7356KB`. No allocation measurement was collected; all timing
figures are representative smoke/scale evidence only.

## checkpoint ledger

| checkpoint | status |
| --- | --- |
| PA16 typed parameterized class-constructor boundary | Complete: focused constructor matrix `17/17`; course controls 400--409 pass with syntax checks; new 409 covers self-pointer hidden-destination typing, protected/private base-constructor access, and defaulted/deleted aggregate field initialization without constructor calls/helpers; aggregate handout controls retain only the known LowIR address/bool shape comparison differences. Final PA16 is `91/243` with `152` failures and `243/243` coverage; failure and coverage identity additions/removals are both `∅`/`∅`. Through-PA15 is `1167/1167`; file audit passes with five pre-existing warnings; diff-check passes. |
| PA16 typed class-object construction boundary | Prior completed milestone retained as history; its earlier stage and audit claims are not current-gate evidence. |
| PA16 static data storage/access milestone | Completed bounded audit/repair; final full stage is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`; focused 16-fixture evidence is `11/16`. |
| PA16 typed static member-function lookup/emission | Previously completed and retained; selected static-function, inherited, protected, and demand controls remain green. |
| PA16 protected object access / inherited member boundary | Previously completed and retained; course 405 now checks the superseding static-data boundary. |
| PA16 shared semantic DAG / typed owner complexity correction | Completed; shared default facts and repeated transparent casts pass without singleton-parent rejection, and direct redeclaration ownership is BindingId-indexed. |
| Through PA15 | Prior exact required gate (historical): completed at `1167/1167`; the final current result is recorded in the PA16 checkpoint row above. |
| PA16 file audit | Prior exact audit evidence (historical): passed with five pre-existing `bad-division` warnings after the bounded helper move and declaration-header extraction; the final current result is recorded in the PA16 checkpoint row above. |
