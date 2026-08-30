# PA16 implementation plan

## Stage Design

The maintained pipeline is one typed path:

1. PA10 owns syntax and AST facts for declarations, expressions, names,
   operators, and source structure.
2. PA11 owns canonical types, bindings, records, scopes, layout, access, and
   lookup/publication facts.
3. PA12 owns semantic expression/declaration facts, value categories,
   conversions, lifetime facts, constructor selections, and typed constructor
   action/function facts.
4. PA15 consumes those PA12 facts for typed construction, lowering, LowIR
   address paths, and demand-driven function emission.  It does not recover
   source text or redo lookup/selection.

## Failure Map

Turn-start baseline: `220/243`, `23` failures, and `243/243` identities
covered, at HEAD `d889058c0d159bd4414ffb6e9f5ac75227ce0192`.

This checkpoint targets two LowIR-only residuals:

- `pa16/tests/general/200-const-subobject-member-call.t`: retain the
  automatic `Map` root address and `Map::g`/`Table::f` call path, while
  removing the unused empty-`Table` subobject projection.
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`:
  retain the automatic `D` root address and reachable `B` base-entry
  definition, while removing the no-op defaulted `D` wrapper/call.

Preservation controls include
`cppgm.tests/course/pa16/404-typed-implicit-default-demand-regression.sh`,
`cppgm.tests/course/pa16/409-typed-constructor-boundary-regression.sh`, and
the signed bit-field reads (`400-signed-bit-field-read.t` and
`400-signed-enum-bit-field-read.t`), whose negative-value behavior is not
altered.

## Active Checkpoint

Spec alignment: implement the PA12-fact to PA15 construction/lowering demand
boundary described by the PA16 Purpose and sections 1, 2, 5, and 7.  The
classifier is conservative: incomplete, invalid, unsupported, cyclic, or
otherwise unknown facts are effectful.

Typed owner/data flow is:

```text
PA12 FunctionFact + ConstructorActionFact + TypeId/BindingId
  -> PA15 memoized constructor-graph and zero-initialization summaries
  -> collect_demanded_member_functions / aggregate lowering
  -> constructor-action lowering and typed LowIR address paths
```

The constructor summary validates canonical function identity, explicit
parameter arity, function scope/implicit object, defined non-union class
record, complete layout, sidecars, empty body, and every reachable action.
Only an empty synthetic wrapper whose reachable constructor graph is no-op is
omitted.  A user-provided empty constructor is retained as a reachable leaf
definition when needed by that wrapper chain; a direct user-provided call is
not pruned.  A separate typed zero-initialization summary recognizes only
complete, flat, non-union class/array structures with no DMI, destructor, or
scalar store work.  Value-initialization stores remain explicit even when the
underlying synthetic constructor call is omitted.

The root automatic class address is always emitted for the lifetime boundary.
A function-scoped binding/block cache reuses only that already-emitted typed
root address, so a later member call cannot create a duplicate address.  The
non-goals are constructors with arguments or argument evaluation, nonempty
executable bodies, DMI, scalar zero/store work, effectful base/member
construction, destructor/lifetime effects, possible required actions, unions,
unsupported layouts, incomplete/invalid facts, and cycles.

## Focused Evidence

Durable command logs are in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-noop-construction-checkpoint-20260830/`.
On the final source state:

- `make -C dev cppgm++ CXX=g++`: status `0` (`16-build-after-size-fix.log`).
- The two authoritative targets pass `2/2` with status `0`
  (`17-target-check-final.log`).  The 404 and 409 typed course regressions
  each pass with status `0` (`18-course-404-final.log`,
  `19-course-409-final.log`).
- `make test-pa16` reports `222/243` with `21` failures and denominator
  `243`; its harness exit status is `2` because residual failures remain
  (`22-make-test-pa16-commit-source.log`).  Identity comparison with
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
  finds starting `23`, fresh `21`, fresh-only `0`, and fixed `2`, with
  `243/243` identities covered.  The fixed identities are the two targets
  named in the Failure Map (`23-failure-identity-compare-commit-source.log`).
- The exact `n=16` prior-through command exits `0` with `1167/1167`
  (`24-prior-through-pa15-commit-source.log`).
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`.
  It reports only the five existing header-body warnings for
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`
  (`25-file-audit-pa16-final.log`).
- The final `git diff --check` after source validation is clean
  (`20-diff-check-final.log`).

Structural LowIR evidence from the two target sources:

- `main` for the const-`Map` target contains exactly `%t1 = addr $m` and
  `%t2 = call i32 @Map__g(%t1)`; `Map::g` retains its field projection and
  `Table::f` call.
- `main` for the friend target contains `%t1 = addr $d`; there is no `D::D`
  function/call, and `B__B__base_entry` remains defined.
- The 404/409 controls retain DMI, stateful base/member construction,
  value-initialization stores, argumented construction, and required method
  projections/calls.
- The final wide-root probe compiles with status `0` and has one `main`,
  `128` root addresses, `0` derived wrapper definitions, `0` derived calls,
  and `128` retained `B__B__base_entry` definitions
  (`21-wide-root-probe-final.log`).

## Performance Evidence

Constructor and zero-initialization summaries use dense per-function/type
state, result, and invalidation arrays; an in-progress state makes recursion
effectful, and completed results are memoized.  Each reachable constructor
action/type edge is analyzed once per lowering.  The no-op leaf-preservation
worklist captures one shared dense visitation state for the entire demand pass;
state 1 breaks an unexpected cycle and state 2 permanently records a completed
node, so the whole leaf-preservation pass is O(F+E) time and O(F) memory for F
functions and E reachable constructor edges, rather than O(R*F) dense setup for
R pruned roots.  Root-address reuse uses one function-scoped ordered binding
map.

An ephemeral wide-root probe with 128 independent defaulted-derived locals
over 128 empty user-provided bases compiled with status `0`.  Its LowIR had
one `main`, 128 root addresses, zero derived wrapper definitions, zero derived
constructor calls, and 128 retained `B__B__base_entry` definitions.  This is
structural evidence only; no timing or RSS claim is made.

## Checkpoint Ledger

- Parent/start baseline: HEAD
  `d889058c0d159bd4414ffb6e9f5ac75227ce0192`, clean worktree,
  `220/243`, `23` failures, `243/243` identities.
- Validated checkpoint: typed PA12-fact constructor/zero-initialization
  classification, shared demand pruning, root-address reuse, focused targets
  `2/2`, controls `404` and `409`, broad PA16 `222/243` with `21` failures,
  no fresh-only failure identities, prior-through `1167/1167`, and audit
  status `0` with five existing warnings.  Durable evidence is recorded at
  the path above.
- Changed implementation surfaces are limited to existing PA15 lowering
  files; no tests, fixtures, references, harnesses, comparators, generated
  outputs, coverage rules, or source-set files were changed.
