# PA16 unnamed-namespace identity and lifecycle-demand checkpoint

## Stage Design

PA11 is the sole semantic owner of namespace/scope identity and binding
linkage.  An unnamed namespace is a typed `Scope` fact; its parent-keyed
`unnamed_namespace_index_` reuses the same `ScopeId` on reopen, and
`internal_linkage_scope` propagates to each child scope once at creation.
`add_value` and synthesized class special-member bindings fold that typed
owner fact into canonical `Binding::internal_linkage`.  The global namespace
is still a distinct non-unnamed scope.

PA12 creates one typed empty constructor action only for an internal
namespace-scope class default-object demand and marks it
`internal_namespace_default_constructor_demand`; it also publishes the
typed constructor base-entry relation.  PA15 consumes that marker through its
existing global-root demand worklist, emits only the required internal
constructor facts, and keeps the zero-data object path free of a startup call.
At the PA15 symbol boundary, typed namespace components render a fixed
`_GLOBAL__N_1` for every unnamed namespace under its named owner.  No stage
parses or matches rendered names, creates a second semantic owner, retries the
whole program, or eagerly emits helpers.

This is aligned with `spec.md` Purpose and §§1–5/§7: one forward pipeline,
continuity of typed identity/linkage/lifetime/demand facts, and demand-driven
helper emission.  Named namespaces, ordinary namespace/class globals,
hidden-friend lookup/ADL, and nontrivial lifecycle paths retain their typed
owners and prior behavior.

## Failure Map

Turn-start authority was `make test-pa16 = 235/243`, with complete `243/243`
identity coverage and exactly these eight failures:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t` — PA12/PA15
   local default-array construction and cleanup demand.
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t` — typed
   reference/index/member address projection and lowering.
3. `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
   — active cluster: PA11 unnamed-scope identity/linkage, PA15 ABI rendering,
   and internal trivial-constructor demand.
4. `pa16/tests/general/300-friend-function-definition-skip.t` — hidden-friend
   definition selection and function-demand filtering.
5. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t` — nested
   enum/operator identity, ADL, and bitmask lowering.
6. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t` — typed
   bit-field read/modify/write lowering.
7. `pa16/tests/general/400-signed-bit-field-read.t` — signed bit-field
   extraction and represented-value conversion.
8. `pa16/tests/general/400-signed-enum-bit-field-read.t` — signed underlying
   enum bit-field extraction and conversion.

The fresh PA16 gate is `236/243` with complete `243/243` identity coverage.
The exact seven residual identities are items 1, 2, and 4–8 above; item 3 is
resolved.  The residual clusters were not broadened by this checkpoint.

## Active Checkpoint

Scope is limited to typed unnamed-namespace identity/linkage, its ABI symbol
boundary, and the internal namespace-scope default-constructor demand edge.
Reopened anonymous namespaces reuse their parent’s typed scope; distinct named
parents each render `_GLOBAL__N_1`; namespace/class-owned entities retain
internal binding; and duplicate semantic definitions are not emitted.  The
normal PA16 function collection order is retained; no presentation-only
function reorder is present.  The active fixture emits no useless
`__cppgm_init`/`__cppgm_fini` path and emits only its required constructor
helpers.

Changed files:

- `dev/src/pa11_semantic.cpp`
- `dev/src/pa11_semantic_core.cpp`
- `dev/src/pa11_semantic_model.h`
- `dev/src/pa12_semantic_construction.cpp`
- `dev/src/pa12_semantic_facts.cpp`
- `dev/src/pa15_lowering.h`
- `dev/src/pa15_lowering.cpp`
- `dev/src/pa15_lowering_calls.cpp`
- `cppgm.tests/course/pa16/430-typed-unnamed-namespace-per-parent-regression.sh`
- `pa16/plan.md`

The bounded course regression uses two named namespaces, an unnamed namespace
reopen under the first parent, and internal globals/friends/constructors.  It
checks one fixed ABI component, internal bindings, and one definition per
entity.  No handout test, `.ref`, exit-status sidecar, comparator, harness, or
fixture changed.  Remaining uncertainty is limited to the seven pre-existing
residual identities and unmeasured timing/RSS; no C++11/PA16 contract conflict
was found.

## Performance Evidence

New scope work is O(1) per consumed scope: parent-keyed unnamed-scope reuse,
one typed unnamed fact, and one O(1) propagated internal-linkage bit.  PA12
adds one constructor action/base-entry relation per actual qualifying demand.
PA15 uses existing dense binding/function/fact worklists; the new code adds no
O(F) reorder pass, and ordinary collection order remains canonicalized by the
existing top-level entry handling.  New work is therefore linear in consumed
typed scopes/bindings/functions/actions/edges, plus the existing owner-component
rendering path; the fixed ABI component adds no scope/name search.

Measured structural evidence is the bounded two-parent/reopen regression;
`sh -n` and the regression both pass.  No timing or RSS claim is made.

The post-correction commands and results are: `make build` exit 0; active
focused check `1/1`; eight relevant named/unnamed namespace,
lifecycle/hidden-friend controls `8/8`; `make test-pa16` exit 2 with
`236/243` and the seven residuals above; exact PA15 prior gate exit 0 with
`1167/1167`; `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`
exit 0 with six existing warnings; and no prohibited paths in the diff.

## Checkpoint Ledger

| checkpoint | compact state |
|---|---|
| prior `2e48cd6d` | Nested braced aggregate-member checkpoint; historical PA16 authority `235/243`, eight residuals, and `243/243` identity coverage. |
| HEAD `542b136a` | Clean turn-start authority `235/243`, complete `243/243` identity coverage, exactly the eight failures in the map. |
| corrected uncommitted checkpoint | Active target resolved; fresh PA16 `236/243` with exactly seven residual identities and full `243/243` coverage; PA1–PA15 `1167/1167`; build, focused controls, course regression, and file audit pass. Commit-ready after final diff/stage/clean-status evidence. |
