# PA16 implementation plan

## Spec alignment and typed ownership

This checkpoint is bounded to the PA16 Purpose and `spec.md` sections 1, 2,
5, and 7.  The maintained path is:

```text
PA12 FunctionFact + ConstructorActionFact + TypeId/BindingId
  -> PA15 memoized constructor-graph and zero-initialization summaries
  -> member-function demand + aggregate/construction lowering
  -> typed LowIR root/subobject address paths
```

PA10 owns syntax.  PA11 owns canonical types, bindings, records, scopes,
layout, access, and publication.  PA12 owns semantic facts, conversions,
lifetime facts, constructor selections, and typed constructor actions.  PA15
consumes those facts directly; it does not recover source text or redo lookup.

The audited proof is conservative.  A constructor wrapper is omitted only when
its canonical function identity, void/arity boundary, scope/implicit object,
defined non-union record, complete layout, sidecars, body, enclosing actions,
and reachable constructor graph all prove no-op.  Semantic constructor actions
also require the typed aggregate, storage-backed, or temporary call shape; an
extra child cannot be silently dropped.  Value-initialization retains its zero
stores.  DMI, destructors/lifetime work, scalar/value stores, argument
evaluation, explicit calls, throwing/cleanup paths, and incomplete facts stay
effectful.

The automatic-local address cache is keyed by `BindingId` and current `BlockId`,
cleared per lowered function, and stores only an already-emitted root address.
Constructor and zero-initialization caches are dense by immutable `FunctionFactId`
or `TypeId`; in-progress recursion is conservative and completed invalid
results remain non-prunable.

## Authority and exact failure map

The authoritative turn-start log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
Its exact authority at landed HEAD
`24d555c882a3e15ea3ffe5be42ed5d9953084df6` is `222/243`, with `21` failures
and `243/243` identities covered.  The final fresh run must not add a failure
identity or reduce coverage.

```text
pa16/tests/general/100-function-pointer-nested-param-name-shadow.t
pa16/tests/general/200-elaborated-member-forward-type.t
pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t
pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t
pa16/tests/general/200-local-default-class-array-lifecycle.t
pa16/tests/general/200-nested-braced-member-aggregate-init.t
pa16/tests/general/200-reference-indexed-pointer-member-access.t
pa16/tests/general/200-reference-member-class-init.t
pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t
pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t
pa16/tests/general/300-callable-field-hides-private-base-method.t
pa16/tests/general/300-enum-class-nonmember-operator-bitand.t
pa16/tests/general/300-friend-function-definition-skip.t
pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t
pa16/tests/general/300-operator-nullptr-t-from-zero.t
pa16/tests/general/300-overloaded-deref-user-assignment.t
pa16/tests/general/300-user-defined-string-literal-operator.t
pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t
pa16/tests/general/400-bit-field-prefix-postfix-increment.t
pa16/tests/general/400-signed-bit-field-read.t
pa16/tests/general/400-signed-enum-bit-field-read.t
```

The landed increment's two selected controls,
`200-const-subobject-member-call.t` and
`200-friend-derived-private-base-defaulted-constructor.t`, remain outside
that residual map.

## Checkpoint findings

The bounded repair covers three related facts:

1. `demand_constructor_fact` now includes
   `constructor_action_call_shape_is_noop` in the semantic `ConstructorAction`
   no-op decision while preserving the separate storage-backed `CallExpression`
   path.  The initializer consumer applies the same proof; valid zero-argument
   temporary value-initialization remains elidable, while malformed or
   argumented shapes remain effectful.  `constructor_action_is_noop` rejects
   temporary objects as a lowering shortcut.
2. `constructor_function_is_noop` now rejects inconsistent
   `NamedRecord.has_base`/`direct_base` metadata, incomplete or contradictory
   complete-layout base metadata, invalid immediate base records, and
   nonzero direct-base offsets.  Member actions remain tied to the enclosing
   complete layout and callable return types remain tied to the constructor
   signature.
3. No unrelated call lowering, test, fixture, reference, sidecar, harness,
   comparator, generated output, coverage rule, or source-set file changed.

## Final evidence

Durable command logs are outside the repository under
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-24d555c8-checkpoint-audit-20260830/`.

- `make -C dev cppgm++ CXX=g++`: status `0`; log
  `rebuild-after-demand-fix.log`.
- The landed two-test target: status `0`, `PASS (2/2)`; log
  `final-focused-landed-targets.log`.
- Course 404 and 409 typed regressions: status `0` each; logs
  `final-focused-course-404.log` and `final-focused-course-409.log`.
- The ten-test constructor/aggregate/lifetime matrix: status `0`,
  `PASS (10/10)`; log `final-focused-constructor-matrix.log`.
- The five known residual controls: status `2`, `0/5`; log
  `final-focused-authority-controls.log`.  All five identities are in the
  authority map.
- Exact prior-through command with `n=16`: status `0`,
  `ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)`; log
  `gate-1-prior-through-pa15.log`.
- `make test-pa16`: status `2`, `222/243` passed; log
  `gate-2-test-pa16.log`.  Its 21 failure identities exactly match authority.
- Exact identity comparison against the authority log: authority failures
  `21`, fresh failures `21`, authority-only `0`, fresh-only `0`; discovered,
  reference-sidecar, and fresh-sidecar inventories are each `243`, with every
  missing/unexpected comparison `0`; log `gate-4-identity-comparison.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`
  with only the five pre-existing header-division warnings; log
  `gate-3-file-audit.log`.
- Final `git diff --check` and clean-tree verification are recorded in
  `final-diff-check.log` and `final-clean-status.log`.

## Structural and performance evidence

The shared no-op leaf worklist is intended to visit each reachable constructor
fact/action once, with O(F+E) traversal and O(F) visitation storage for `F`
function facts and `E` constructor edges.  A 128-independent-derived-local
probe completed with status `0`, `WALL=0.01`, `RSS_KB=8928`, 128 root
addresses, 0 derived wrapper definitions, 0 derived calls, and 128 retained
base-entry definitions.  This is representative structural/spot evidence,
not a benchmark, timing, or timeout claim; the self-contained command,
input, process, observation, and counter record is in `final-scale-probe.log`.

## Final checkpoint

This audit is complete for `24d555c8`, while PA16 remains incomplete with its
unchanged 21 residual failures.  The next separately bounded checkpoint is
the first residual identity, `100-function-pointer-nested-param-name-shadow.t`;
this audit does not audit or repair it.

## Checkpoint ledger

| Commit | Status |
| --- | --- |
| `24d555c8` | Completed PA16 checkpoint audit; final focused and broad evidence, exact 21-identity comparison, full 243-identity coverage, file audit, and clean-tree verification recorded above. |
