# PA16 implementation plan

## Stage Design

Spec alignment (§§1, 2, 4, 5, and 7): PA10 remains the syntax boundary and
PA12 publishes one typed expression fact; PA15 consumes that fact directly
through the existing call and construction owners.  No source rendering,
reparse, parallel expression analyzer, or second LowIR model is introduced.

For the supported ordinary non-array placement-new form, PA12 validates a
complete named non-union non-polymorphic class type, synthesizes one typed
allocation-size literal, preserves an explicit global qualifier, and sends the
size plus placement expressions through the existing typed function/overload
selector.  The selected allocation must have a target-size (`unsigned long`)
first parameter and exact `void*` result.  It publishes a `NewExpression`
whose children are the selected allocation `CallExpression` and the existing
`ConstructorAction`; brace and parenthesis initializers therefore share the
established aggregate/constructor selection, conversion, and demand facts.
The existing C-style cast owner records the two typed conversions needed by
`(void*)buf` (array decay followed by pointer-to-void conversion), while
non-void array casts remain on the ordinary typed path.

PA15 lowers the allocation call once, passes its returned pointer as the
constructor destination, and returns that same pointer with the NewExpression
type.  Constructor calls, aggregate helpers, symbol identity, declaration
demand, and LowIR call emission remain owned by their existing machinery.
For both supported target fixtures, allocation is emitted once before
construction; a throwing constructor propagates through the existing call
boundary.  Neither target selects or declares a matching placement
deallocation function, and delete/placement-delete lookup, deallocation, and
cleanup remain outside this checkpoint; general throwing placement-new
cleanup is not claimed.

## Failure Map

The latest landed-stage authority before this checkpoint is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`211/243` passed, exactly `32` failed, and all `243/243` identities are
covered.  The complete supplied 32-failure authority map is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/200-reference-member-class-init.t`
- `pa16/tests/general/200-string-literal-does-not-convert-to-mutable-void-pointer.t`
- `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
- `pa16/tests/general/300-adl-associated-namespace-does-not-climb-parents.t`
- `pa16/tests/general/300-adl-using-declaration-source-point.t`
- `pa16/tests/general/300-callable-field-hides-private-base-method.t`
- `pa16/tests/general/300-enum-class-nonmember-operator-bitand.t`
- `pa16/tests/general/300-friend-function-definition-skip.t`
- `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
- `pa16/tests/general/300-operator-nullptr-t-from-zero.t`
- `pa16/tests/general/300-overloaded-deref-user-assignment.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The constructor-call placement-new identity is in the supplied authority;
the aggregate placement-new identity is the one known authority-only pass
from the landed implementation.  The fresh full-stage run preserves this
exact map: its sorted comparison has `fresh_only=0` and `authority_only=0`,
with no coverage reduction.

## Active Checkpoint

The implementation is limited to the typed placement-new semantic/lowering
boundary in these files:

- `dev/src/pa11_semantic_model.h` and `dev/src/pa11_semantic.cpp`
- `dev/src/pa12_semantic.cpp`, `dev/src/pa12_semantic_new.cpp`, and
  `dev/src/pa12_semantic_resolution.cpp`
- `dev/src/pa15_lowering.h`, `dev/src/pa15_lowering_flow.cpp`,
  `dev/src/pa15_lowering_construction.cpp`, and
  `dev/src/pa15_lowering_calls.cpp`
- `dev/frontend_source_sets.mk`

The bounded audit repair touches `dev/src/pa12_semantic_new.cpp`,
`dev/src/pa12_semantic_resolution.cpp`,
`dev/src/pa15_lowering_construction.cpp`, and adds the narrow course control
`cppgm.tests/course/pa16/423-typed-placement-new-boundary-regression.sh`.

The exact owner/data flow is
`PA10 NewExpression/NewPlacement` -> `PA12 semantic_new_expression` -> typed
allocation `CallExpression` plus typed `ConstructorAction` -> PA15
`lower_call` -> `initialize_constructor_value` at the allocation result ->
typed pointer value.  Selected bindings, scopes, callable types, conversion
ranges, constructor demand, and LowIR symbols are carried by canonical facts;
no text is used as transport.

Array-new, delete, heap runtime/lifetime ownership, polymorphism, copy/move or
by-value class semantics, unsupported initializer forms, parser changes,
handout tests, `.ref` fixtures, reference/host execution, and the other 31
authority residuals are excluded.  Array-new is rejected at the semantic
entry boundary; it is not implemented.  The adjacent array-decay/pointer-
to-void cast repair is included only because it is the typed placement
argument in the constructor-call target.  The shared declaration planner
adjustment is included because the target's externally declared constructor
requires the existing declaration-owner ABI spelling `%arg0`, `%arg1`;
retained definitions keep `%this` and are probed separately.  Generic
conversion cleanup beyond that placement argument remains excluded.

The audit found and repaired three boundary defects: the array-pointer cast
special case was narrowed to pointer-to-void targets; non-array named-class
shape is checked before allocation publication; and PA12/PA15 now require
typed allocation owner, size-parameter, result, constructor callable, hidden
destination, and pointer-physical consistency.  These are fail-closed checks,
not new semantic breadth.

## Performance Evidence

For a placement with `P` arguments, `F` selected allocation parameters, and
`K` selected constructor parameters, the added work is
`O(P + F + K + Select(C, P))`, where `C` is the candidate set and `Select`
denotes the existing selector's candidate/argument viability and ranking cost;
no stronger selector bound is claimed here.  The new path does not duplicate
that selector work, add a whole-program retry, create a per-node owning cache,
or perform an unbounded scan.  PA15 consumes the two NewExpression child
facts and each selected signature once, so construction is linear in the
emitted action sequence.

Representative structural evidence from both target shapes: each fixture
publishes one allocation call with a synthesized size plus one placement
argument; the aggregate publishes one aggregate constructor action with one
member initializer; the constructor-call publishes one user constructor
action with one argument and two cast conversion facts.  Emitted order is
allocation call, then constructor call, with the returned pointer reused by
the later store/compare; no allocation result is rescanned or reconstructed.

## Validation

Final validation and gate evidence:

- `make -C dev cppgm++ CXX=g++`: passed; the new semantic translation unit
  remains linked through the frontend source set (status `0`; durable log
  directory:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-placement-new-final-20260830`).
- `make -C pa16 check
  TEST='tests/general/200-placement-new-expression-aggregate-brace.t'`:
  status `0`, `PASS (1/1)`.
- The constructor-call target reaches typed allocation and construction in
  order and preserves the external declaration spelling, but its focused
  `make -C pa16 check` returned status `2` for the known one-test LowIR
  mismatch from the unrelated `trunc u8 i64` before `zext i32 u8` truth-width
  shape.
- `make -C pa16 check TEST='tests/general/200-out-of-class-getter-only.t'`:
  status `0`, `PASS (1/1)`.
- `cppgm.tests/course/pa16/423-typed-placement-new-boundary-regression.sh`:
  `sh -n` and the script both returned status `0`; it covers a supported
  non-void array cast, valid `(void*)` placement, rejection of non-`void*`
  allocation results and non-`size_t` first parameters, and the repaired
  array-new rejection.
- `make test-pa16` returned status `2` at `211/243`, with the exact supplied
  32-failure identity set, `fresh_only=0`, `authority_only=0`, and `243/243`
  coverage.  Full output and the sorted identity/coverage comparison are in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-placement-new-final-20260830/make-test-pa16.log`
  and `identity-coverage-comparison.log`.
- The required `n=16` through-stage command returned status `0` with
  `1167 / 1167`; the source file audit returned status `0` with five known
  header-division warnings and no fatal finding; final `git diff --check`
  returned status `0`.  Durable logs are in the same directory above as
  `through-pa15.log`, `file-audit.log`, and `git-diff-check-final.log` with
  matching status files.
- Focused logs/statuses in that directory are `focused-build.*`,
  `focused-aggregate.*` (status `0`), `focused-constructor.*` (status `2`,
  known truth-width comparison only), `focused-getter.*` (status `0`), and
  `course-423-shn.status` plus `course-423.*` (both status `0`).
- The final path audit found only the three bounded source repairs, this plan,
  the audit record, and course 423; no handout test, fixture, `.ref` file,
  exit-status sidecar, harness, comparator, generated oracle, or unrelated
  path changed.

## Next Checkpoint

This completed checkpoint audits `fb4f46ed`; PA16 remains incomplete with the
same 32 residual identities.  Future work must select a separate residual from
the map and must not treat extra passes as compensation for a fresh failure.
Placement delete/deallocation lookup and general throwing placement-new
cleanup remain outside this checkpoint.

## Checkpoint Ledger

| checkpoint | PA16 identities / coverage | delta / evidence | status |
| --- | --- | --- | --- |
| `1694bc3e` retained baseline | `200/243`, 43 failures, `243/243` covered | prior bit-field baseline | prior landed |
| `7e060b28` typed packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior focused bit-field evidence retained | prior landed |
| `d95a6fe7` local-class checkpoint start | `202/243`, 41 failures, `243/243` covered | prior local-class selection | prior checkpoint |
| `d83e927f` typed local-class materialization | `206/243`, 37 failures, `243/243` covered | prior focused matrix, through-PA15, and audit passed | prior landed |
| `70327e4d` typed exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | typed destructor-body suffix and cleanup work | prior landed |
| `3b2b4882` checkpoint audit baseline | `208/243`, 35 failures, `243/243` covered | clean turn-start tree and authoritative stage log | prior baseline |
| `ee8f44d5` per-throw typed array cleanup checkpoint audit | `209/243`, 34 failures, `243/243` covered | exact prior authority comparison preserved all identities and coverage; focused constructor/array evidence passed with mapped residuals | prior landed |
| `08472cce` typed pragma-pack record layout (parent `0ff3fdef`) | prior landed pack-layout checkpoint | typed ordered pack/layout path; superseded as current parent by `9f7101ac` | prior landed |
| `9f7101ac` typed pack-layout audit | `210/243`, 33 failures, `243/243` covered | former HEAD and parent baseline; hardened typed pack validation/layout path; broad authority is the 33-failure baseline for this increment | prior landed |
| `fb4f46ed` typed placement-new semantic/lowering checkpoint (parent `9f7101ac`) | `211/243`, 32 failures, `243/243` inventoried; fresh-only `0`; authority-only aggregate placement-new | focused aggregate `PASS (1/1)`; constructor exit and typed LowIR flow pass with only the documented unrelated `trunc u8 i64` residual; declaration-owner regression `PASS (1/1)`; through-PA15 `1167/1167`; audit no fatals; diff-check pass | landed; PA16 remains incomplete |
| `fb4f46ed` placement-new checkpointAudit completed | supplied authority and fresh result `211/243`, 32 failures, `243/243` covered; exact 32 identities listed above; `fresh_only=0`, `authority_only=0` | completed bounded repairs: narrowed array-pointer casting, rejected array and non-named placement targets early, enforced exact allocation signatures, and validated PA15 cross-fact/physical-pointer continuity; focused build, aggregate/getter controls, course 423, through-PA15, file audit, and final diff-check pass; constructor target retains only the known truth-width LowIR mismatch | complete; PA16 remains incomplete |
