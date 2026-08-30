# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10 declaration-specifier tokens and
PA11 layout become PA12 typed semantic facts and conversion facts, and PA15
lowers those facts into LowIR.  PA12 owns the `BitFieldFact` and its
`BindingId`; PA15 owns the `BitFieldAddressProjection` and `ProjectionId` used
by its typed `LoweredValue`.  The packed-field path carries declared type,
storage type, operation type, width, mask, and signedness through those owners.
Member reads, conversions, encodes, stores, inc/dec, and aggregate
initialization consume that same identity.  The selected implementation-defined
plain-`int` policy is unsigned storage/value signedness; a narrow field still
promotes to `int` when `int` represents its range, while a full-width field
promotes to `unsigned int`.  Explicit signed integral fields and
signed-underlying enum fields remain signed.  This checkpoint adds no text
transport, parallel analyzer, rescan/cache, retry, or second lowerer.

## Failure Map

Turn-start authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`215/243` passed, exactly `28` failed, and `243/243` identities are covered.
The complete turn-start failure map is:

- `pa16/tests/general/100-function-pointer-nested-param-name-shadow.t`
- `pa16/tests/general/200-aliased-base-mem-initializer-match.t`
- `pa16/tests/general/200-const-subobject-member-call.t`
- `pa16/tests/general/200-elaborated-member-forward-type.t`
- `pa16/tests/general/200-external-ctor-overload-nonfirst-argument.t`
- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-friend-intermediate-derived-protected-base-method.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-nested-braced-member-aggregate-init.t`
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
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

Relative to the previous `214/243` / `29`-failure checkpoint, the landed
`a5c8e1664e5059e2453e3252021f3843d0ab23b6` increment has exactly one
baseline-only recovery, `400-bitfield-aggregate-init.t`, and no fresh-only
identity.  Coverage remains `243/243`; extra passes do not compensate for a
new failure.  Fresh `make test-pa16` exits `2` at `215/243` with exactly `28`
failures.  The independent comparison reports authority `28`, fresh `28`,
authority-only `0`, fresh-only `0`, inventory `243`, run total `243`, and
coverage `243/243`; the full-stage failure set is unchanged.

## Active Checkpoint

This audit reviews `a5c8e1664e5059e2453e3252021f3843d0ab23b6` relative to
`7f4fe2d4` and records one coherent repair in
`dev/src/pa12_semantic_facts.cpp`.  The landed declaration-specifier token
check legitimately distinguishes unqualified plain `int` from explicit
`signed`/`unsigned`; it does not alter physical storage layout.  The repair
continues the selected plain-`int` signedness into `operation_type`: narrow
fields use `int` when their range fits, and a full-width unsigned-selected
field uses `unsigned int`.  Explicit signed integral and signed-underlying enum
facts remain signed for extraction, promotion, assignment, initialization, and
updates.  This is a semantic policy applied through one fact, not a fixture
special case.

The PA15 trace remains one typed path.  Direct aggregate initialization starts
the first direct bit-field projection at `ROOT_STORAGE_ADDRESS` of the
canonical aggregate storage; nested paths first replay their ordinary
pointer/array projections and use `ROOT_POINTER_VALUE`, while constructor
subobjects use `ROOT_POINTER_LOAD` from the active typed `this` storage.  Each
initializer is evaluated once.  Packed-unit stores load/clear/OR/store the
existing unit after the first field, preserving neighbors; saved typed
projections are replayed instead of reusing a transient pointer.

`lower_binary_expression` passes `suppress_bit_field_copy` only at operand
conversion boundaries.  `materialize_lvalue_value` consumes it only in the
bit-field branch to suppress the redundant publication `IK_COPY`; ordinary
non-bit-field lvalues still emit their load.  The focused branches retain
floating conversion, bool-context, pointer, reference, and value-category
materialization, so no required copy or boundary is skipped.  No API narrowing
was necessary after tracing all call sites; the default remains false outside
this binary-operand optimization.

Inc/dec preserves the PA12 operation type and PA15 projection: each update
extracts the old field value once, then its preserving RMW store reloads the
packed unit once before clear/OR/store.  Prefix returns the updated lvalue
category and postfix returns the old prvalue.  The two signed-read references
omit required sign extension,
and the prefix reference requests an unsigned comparison despite the PA12
`int` promotion for its narrow fields.  Their checked-in LowIR shapes remain
oracle tensions; changing typed lowering to reproduce them would violate the
README/standard contract and is out of scope.

## Performance Evidence

The structural evidence is bounded: fact/projection lookup is O(1) per typed
bit-field operation, width/mask work is bounded, and path replay is O(depth).
Focused output shows the expected projections, packed-unit RMW operations, and
no redundant bit-field publication copies; the 412 regression covers
neighbor-preserving initialization/assignment and single evaluation.  No
rescan, retry, parallel lowerer, or fixture-dependent branch was added.  No
timing, RSS, allocation, or code-size measurement was taken, so no numerical
performance claim is made.

## Validation

Final validation is complete:

- `make -C dev cppgm++ CXX=g++`: status `0`.
- The focused 10-test PA16 matrix covering prefix/postfix, aggregate
  initialization, signed reads, layout, zero-width layout, constructor and
  sparse initialization, address rejection, and member-access rejection:
  status `2`, `PASS (7/10)`; the three residual identities are the prefix and
  two signed-read oracle tensions.
- `sh cppgm.tests/course/pa16/412-typed-bit-field-initialization-root-regression.sh`:
  status `0`.
- `sh cppgm.tests/course/pa16/422-typed-pack-wide-bitfield-layout-regression.sh`:
  status `0`.
- `sh cppgm.tests/course/pa16/424-typed-plain-int-bitfield-promotion-regression.sh`:
  status `0`; it covers narrow/full-width plain `int`, explicit signed, packed
  neighbors, assignment, and prefix/decrement updates.
- Focused non-bit-field, floating, bool-boundary, pointer/reference, and
  bit-field-publication probes: status `0`.
- `git diff --check`: status `0`.

Durable raw logs and status sidecars are in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-plain-int-bitfield-checkpoint-audit-20260830/`.
- `make test-pa16` is `2` at `215/243` with `28` failures; its raw log/status
  are `final-make-test-pa16.log` and `.status`.
- The exact `n=16` through-PA15 gate is `0` at `1167/1167`; its raw
  log/status are `final-through-pa15.log` and `.status`.
- The required file audit is `0` with five known warnings; its raw log/status
  are `final-file-audit.log` and `.status`.
- The independent inventory/failure comparison is `0`; its summary is
  `final-identity-comparison.log` and `.status`, with inventory/run total
  `243/243`, coverage `243/243`, authority/fresh failures `28/28`,
  authority-only/fresh-only `0/0`, and unexpected failures `0`.
- Final diff/path checks are recorded as `final-git-diff-check.*` and
  `final-changed-paths.*` after the record updates.

No test, handout, reference, harness, comparator, or coverage surface was
changed; `424` is the sole focused regression added under the permitted course
directory.

## Next Checkpoint

PA16 remains incomplete at `215/243`, with the exact unchanged 28-failure map
and `243/243` coverage.  The next coherent source boundary is the residual typed
constructor/member-initialization and lifetime flow.  The signed-read and
prefix oracle tensions remain contract questions, not reasons to weaken typed
lowering.

## Checkpoint Ledger

| checkpoint | result | status |
| --- | --- | --- |
| `1694bc3e` bit-field baseline | `200/243`, 43 failures, `243/243` covered | prior landed |
| `7e060b28` packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior landed |
| `d95a6fe7` local-class start | `202/243`, 41 failures, `243/243` covered | prior checkpoint |
| `d83e927f` local-class materialization | `206/243`, 37 failures, `243/243` covered | prior landed |
| `70327e4d` exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | prior landed |
| `3b2b4882` checkpoint audit baseline | `208/243`, 35 failures, `243/243` covered | prior baseline |
| `ee8f44d5` typed array cleanup | `209/243`, 34 failures, `243/243` covered | prior landed |
| `08472cce` typed pragma-pack layout | prior landed pack-layout checkpoint | prior landed |
| `9f7101ac` pack-layout audit | `210/243`, 33 failures, `243/243` covered | prior landed |
| `fb4f46ed` placement-new semantic/lowering | `211/243`, 32 failures, `243/243` covered | prior landed; historical |
| `85b819b7` pre-increment authority | `211/243`, 32 failures, `243/243` covered | prior baseline |
| `typed truth-width continuity (parent 85b819b7)` | final `214/243`, 29 failures, `243/243` covered; authority-only 3 named identities; fresh-only 0; through-PA15 `1167/1167`; audit 0 with five known warnings; diff-check 0 | landed in this checkpoint commit |
| `96e80152` truth-width checkpointAudit | Focused build `0`, PA16 `7/7`, PA15 `5/5`; fresh PA16 status `2` at `214/243` with authority/fresh `29/29` failures, baseline-only/fresh-only `0/0`, and `243/243` coverage; through-PA15 `1167/1167`; file audit `0` with five pre-existing warnings; final diff/path audits `0`; exact-pointee class-pointer guard repaired | completed audit |
| `a5c8e166` typed packed-bit-field value/update checkpointAudit | Final PA16 status `2` at `215/243`, exactly `28` failures and `243/243` covered; independent comparison authority/fresh `28/28`, authority-only/fresh-only `0/0`, inventory/run total `243/243`; landed delta is exactly baseline-only `400-bitfield-aggregate-init.t`; through-PA15 `0` at `1167/1167`; file audit `0` with five known warnings; focused 412/422/424, probes, diff-check, and path audit pass. Durable evidence is under `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-plain-int-bitfield-checkpoint-audit-20260830/`; no forbidden surface changed | completed audit |
