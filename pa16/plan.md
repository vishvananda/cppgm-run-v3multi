# PA16 implementation plan

## Stage Design

This checkpoint owns the constructor-time class-array exception boundary.
PA12 remains the sole semantic owner: a synthesized constructor's
`FunctionFact.constructor_action_begin/count` selects its ordered typed
`ConstructorActionFact` range, including the constructor binding, object type,
arguments, and value-initialization bit. PA15 lowers that fact range and does
not reconstruct source text or create a second semantic owner.

For an array construction, `ArrayAddressRoot` identifies either the active
constructor subobject (`ConstructorActionFact` plus its owner) or the placement
storage binding. `emit_constructor_elements` walks array dimensions in forward
lifetime order and appends each completed class terminal to a transient
`vector<ConstructedElement>`. Each terminal retains only the typed root, array
index path, canonical record, and `model_.destructor_binding(record)`.

The first terminal can use its normal destination SSA. Once a completed prefix
exists, a potentially throwing later terminal carries only its typed root/path:
PA15 emits a fresh `eh_try`, recomputes the destination inside that protected
block, evaluates the typed constructor call, and closes the edge with
`eh_end`. Its fresh cleanup block replays the completed vector in reverse and
`resume`s. Thus every throw point owns exactly its already-constructed prefix;
no normal-path address SSA crosses an exception edge. Nested arrays replay all
indices from the same root. Nothrow and no-op constructors retain the direct
path, and synthetic value-initialization still emits its required zeroing.

The persistent `ArrayCleanupChain` is removed because shared cleanup nodes do
not satisfy the checked per-throw-point LowIR contract. Normal destructor
suffix lowering, automatic-lifetime ordering, aggregate initialization, and
semantic resolution remain outside this increment. The checked local default
array fixture still presents destruction in forward order; C++ lifetime rules
require reverse destruction, so it is documented as a residual rather than
used to drive a semantic reversal.

## Failure Map

The authoritative turn-start log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`208/243` passed, `35` failed, and all `243/243` identities were covered.
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
- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-aggregate-brace.t`
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
- `pa16/tests/general/300-packed-class-layout.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`
- `pa16/tests/general/300-prvalue-derived-base-friend-operator.t`
- `pa16/tests/general/300-synthesized-array-member-lifecycle.t`
- `pa16/tests/general/300-user-defined-string-literal-operator.t`
- `pa16/tests/general/300-using-base-static-same-signature-derived-preferred.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The focused post-change matrix below covers 35 constructor/array/lifetime
neighbors. It passes `30/35`: the primary
`300-synthesized-array-member-lifecycle.t` failure is cleared; the five
remaining selected failures are the already-mapped local-array presentation,
value-init aggregate, friend-access, and two placement-new identities. The
fresh full-stage residual is exactly the 34 entries above except for the
primary. The fresh identity comparison reports baseline-only exactly the
primary, fresh-only `0`, unrecognized `0`, and `243/243` coverage; its durable
log also records the sorted full residual set. The exact selected residuals
are:

- `pa16/tests/general/200-friend-derived-private-base-defaulted-constructor.t`
- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-placement-new-expression-aggregate-brace.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/300-value-init-aggregate-with-nontrivial-member.t`

## Active Checkpoint

The implementation is limited to:

- `dev/src/pa15_lowering_construction.cpp`: typed array-terminal collection,
  throw classification, destination recomputation, and per-throw cleanup.
- `dev/src/pa15_lowering.h`: the transient terminal declaration and lowering
  helper interfaces; the persistent `ArrayCleanupChain` is gone.

The exact owner/data flow is
`FunctionFact.constructor_action_begin/count` -> ordered
`ConstructorActionFact` -> `lower_constructor_action` or placement-array
lowering -> `ArrayAddressRoot` -> recursive `emit_constructor_elements` ->
transient `ConstructedElement` prefix -> `constructor_elements_may_throw` and
`emit_constructor_call_with_cleanup` -> fresh typed root/path replay -> the
canonical `model_.destructor_binding(record)` calls. Constructor actions use
the active constructor record and action as the root; placement construction
uses the semantic storage binding. The vector is lowering-only data, not
duplicate semantic ownership, and no address SSA is retained across blocks.

Scope exclusions are the other 34 turn-start failures, the local-array fixture
whose checked presentation conflicts with reverse C++ destruction, aggregate
and placement-new semantic gaps, destructor-suffix/local-lifetime changes,
tests and `.ref` files, source-text recovery, host/reference execution, and
whole-program retries or caches. No new source file or source-set entry was
needed. The exact runtime throwing behavior of user constructors is represented
by LowIR EH edges here; the focused fixtures validate the emitted contract,
not a host exception implementation.

## Performance Evidence

For `N` flattened terminals and maximum array-path depth `D`, completion
collection is `O(N)` for a flat array and `O(ND)` for nested paths. Each of
the `N-1` later potentially-throwing terminals gets an independent handler
and reverse prefix, so explicit cleanup output is intentionally `O(N^2D)`;
with fixed `D`, the destructor-call count is the triangular
`N(N-1)/2`. This is required by the per-throw-point LowIR contract. No
persistent graph, whole-program retry, textual fallback, or unbounded cache
was added.

Measured structural evidence: the checked primary `Holder__Holder` (N=3)
has 5 blocks, 2 `eh_try` handlers, 2 resumes, 3 constructor calls, 3 cleanup
destructor calls, and 46 instructions. A direct nested `Element elements[2][3]`
probe (N=6, D=2) has 11 blocks, 5 handlers, 5 resumes, 6 constructor calls,
15 cleanup destructor calls, and 190 instructions. A flat N=1 probe has 1
block, 0 handlers, 1 constructor call, 0 cleanup destructor calls, and 7
instructions. A three-element noexcept probe has 1 block, 0 handlers, 3
constructor calls, 0 cleanup destructor calls, and 17 instructions. An
implicit-default no-op probe is demand-elided while its nontrivial destructor
lifetime remains represented. These probes show forward construction, reverse
cleanup, and fresh root/path address projections.

## Validation

- `make -C dev cppgm++ CXX=g++`: exit `0`, and the focused 35-test
  constructor/array/lifetime matrix is `30/35`; the primary fixture passes and
  all five selected residuals are already in the turn-start map. The exact
  focused command, probes (nested N=6, N=1, noexcept, and no-op), and output
  are in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-array-focused-20260830.log`.
- Fresh `make test-pa16` exits `2` at `209/243`, with `34` residual failures;
  the exact output is in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-array-final-test-20260830.log`.
  The sorted identity and coverage comparison is in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-array-final-identity-coverage-20260830.log`:
  authority `35`, fresh `34`, baseline-only the primary, fresh-only `0`,
  inventory `243`, covered `243`, missing `0`, and unexpected `0`.
- The exact required through-PA15 gate exits `0` at `1167/1167`; its durable
  output is in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-array-final-through-pa15-20260830.log`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0`
  with five pre-existing header-division warnings; its output is in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-array-final-file-audit-20260830.log`.
  `git diff --check` exits `0`; its exact output is in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-constructor-array-final-diff-check-20260830.log`.
- The per-throw-point body shape is the checked canonical LowIR
  presentation/fixture contract; each body still replays the already-built
  prefix in reverse, which is the C++ lifetime requirement. Stale generated
  `*.lowir.compare` files were not used as evidence.

## Checkpoint Ledger

| checkpoint | PA16 identities / coverage | delta / evidence | status |
| --- | --- | --- | --- |
| `1694bc3e` retained baseline | `200/243`, 43 failures, `243/243` covered | prior bit-field baseline | prior landed |
| `7e060b28` typed packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior focused bit-field evidence retained | prior landed |
| `d95a6fe7` local-class checkpoint start | `202/243`, 41 failures, `243/243` covered | prior local-class selection | prior checkpoint |
| `d83e927f` typed local-class materialization | `206/243`, 37 failures, `243/243` covered | prior focused matrix, through-PA15, and audit passed | prior landed |
| `70327e4d` typed exception-safe destructor suffix | `208/243`, 35 failures, `243/243` covered | typed destructor-body suffix and cleanup work | prior landed |
| `3b2b4882` checkpoint audit baseline | `208/243`, 35 failures, `243/243` covered | clean turn-start tree and authoritative stage log | turn start |
| current PA16 increment | `209/243`, 34 failures, `243/243` covered | exact delta is baseline-only `300-synthesized-array-member-lifecycle.t`, fresh-only `0`; focused `30/35`; nested N=6 gives 11 blocks, 5 handlers, 15 cleanup calls; full/prior/audit/diff logs recorded above | finalized in the checkpoint commit |

The other 34 turn-start identities are outside this checkpoint boundary. Future
work must preserve the canonical typed action range, active owner flow,
forward construction, reverse destruction, explicit per-throw cleanup, and the
recorded output-complexity rationale.
