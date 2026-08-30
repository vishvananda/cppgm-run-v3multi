# PA16 implementation plan

## Stage Design

This checkpoint closes the typed exception-safe suffix boundary for generated
destructors. PA12 is the sole semantic owner of ordered member/base teardown:
`FunctionFact.destructor_action_begin/count` names the contiguous typed
`DestructorActionFact` range. PA15 resolves the active owner through
`active_destructor_record_` and `active_destructor_this_`, validates the
destructor binding and exact range, and lowers that range without source-text
recovery or unrelated-model scans.

For a nonempty typed suffix, `lower_function` allocates one lowering-only
cleanup block and emits `eh_cleanup` immediately after storing `this`, before
the user body or synthetic empty body. A normal fallthrough emits `eh_end`,
emits the ordered normal suffix plus explicit remaining-suffix cleanup
prefixes, and joins at a return block. The cleanup block replays the same
typed sequence, emits `eh_end`, and `resume`s. A reachable return path lowers
active local lifetimes while the body handler is active, emits `eh_end` to
close it, then lowers the normal typed suffix before terminating that path.
Leaf destructors with an empty range retain their existing shape.

The complete ordered action range is flattened into a local typed
`DestructedElement` replay list. A non-array action contributes one terminal;
an array action contributes reverse element terminals. Each entry retains the
original action, typed array-index path, and terminal record. Normal emission
protects every remaining terminal suffix with typed cleanup blocks; body-unwind
cleanup replays the complete sequence. Addresses are recomputed from the
active typed owner and path, so no SSA value or textual spelling crosses a
cleanup edge.

This follows spec sections 2, 3, and 5: facts have one canonical typed owner,
the `(begin, count)` range is preserved, and source-to-LowIR lowering stays in
the typed in-memory model. It follows section 4 by accounting for work in
consumed facts and emitted IR, and section 7 by recording structural evidence
without timing claims. Explicit destructor calls, local automatic lifetime
ordering, constructor cleanup, label/return lowering, demand-driven helper
emission, and the closed typed bit-field/local-class boundaries remain
unchanged.

## Failure Map

The final stage authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-test-rerun-20260830.log`:
`208/243` passed, `35` failed, and all `243/243` identities were covered.
The exact final failure set is:

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

Compared with the supplied baseline
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
(`206/243`, 37 failures, `243/243` covered), baseline-only is exactly:

- `pa16/tests/general/200-destructor-body-local-before-base-destruction.t`
- `pa16/tests/general/200-member-object-lifetime.t`

Final-only is empty. The failure count therefore decreases by two without a
coverage change; the PA16 stage still has unrelated residual failures.

## Active Checkpoint

The reviewed implementation is limited to these four PA15 lowering files:

- `dev/src/pa15_lowering_flow.cpp` installs and closes the destructor-body
  handler, keeps it active through return-owned local destruction, and emits
  normal/unwind suffix paths.
- `dev/src/pa15_lowering_construction.cpp` validates each exact action,
  flattens scalar and array terminals in action order, recomputes typed
  addresses, and emits normal remaining-suffix cleanup prefixes.
- `dev/src/pa15_lowering.h` carries the lowering-only cleanup block and
  transient terminal declarations.
- `dev/src/pa15_lowering.cpp` initializes the new lowering state.

The exact owner/data flow is
`FunctionFact.destructor_action_begin/count` ->
`checked_destructor_function` -> `model_.destructor_actions_` ->
`lower_destructor_action` (one exact action flattened into typed terminals) ->
one ordered terminal sequence -> `destructor_subobject_address` plus typed
array-path replay -> emitted destructor calls. `active_destructor_cleanup_` is
only a block identity; `DestructedElement` is transient lowering data, not
duplicate semantic ownership. The same sequence serves normal emission with
explicit remaining-suffix prefixes and body-unwind emission without borrowing
normal-path SSA values.

Exclusions are the remaining 35 identities, tests and fixtures, reference
execution, source-text reconstruction, semantic-fact redesign, broad
whole-program passes/caches, and unrelated constructor/local-lifetime changes.
No source sets or generated oracle data were changed.

The lowerer now orders a reachable destructor-body return as local lifetime
destruction under the body handler, `eh_end`, then the typed suffix. An
end-to-end early-return destructor probe remains unvalidated because the
existing PA12 semantic check rejects it before lowering with
`PA12 retained return owner is not definition-owned`; this is the bounded
control-flow uncertainty for this checkpoint.

## Performance Evidence

The exact semantic range is read once to build the transient terminal sequence,
then emission intentionally revisits terminals. Compiler work is
O(actions + emitted IR): terminal collection is linear in the consumed typed
range/elements, and each cleanup-prefix replay costs the address/call IR it
emits. The public LowIR contract has no shared cleanup cursor or implicit
remaining-suffix state, so every potential throw point must carry an explicit,
independently recomputed typed remaining suffix. For N terminals, normal
cleanup prefixes contain `N(N-1)/2` terminal calls and address instructions,
and the body-unwind block adds O(N); generated cleanup-prefix IR is therefore
intentionally O(N^2). This is the semantic/ABI reason for the broader output
bound. No timing claim is made.

Structural evidence from the durable probe log
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-structure-20260830.log`:

- `Derived___Derived`: 3 blocks, 1 body cleanup handler, and 3 calls
  (one local `Guard` call plus the typed `Base` call on each normal/unwind
  path).
- `YB___YB`: 3 blocks, 1 body cleanup handler, and the typed `YA` call on
  each normal/unwind path.
- `Holder___Holder`: 7 blocks, 3 cleanup handlers, 3 resumes, and 9 element
  destructor calls. Its canonical diff has no destructor-function hunk; only
  the pre-existing constructor array shape differs.
- Scaling probes: N=1 has 3 blocks, 1 handler, 2 destructor calls, and 13
  instructions; N=3 has 7, 3, 9, and 44; N=8 has 17, 8, 44, and 174.
  The counts expose the triangular normal prefixes plus the linear unwind
  copy.

## Durable Validation

- Full PA16 stage: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-test-rerun-20260830.log`
  (`make test-pa16`, exit 2 because residual failures remain; 208/243).
- Exact 243-identity/coverage delta:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-identity-delta-rerun-20260830.log`.
- Exact prior gate:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-through-pa15-rerun-20260830.log`
  (`1167/1167`, exit 0).
- File audit:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-file-audit-rerun-20260830.log`
  (exit 0, five known header-division warnings).
- Diff check:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-diff-check-final-20260830.log`
  (exit 0).
- Bounded changed-file audit:
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-changed-file-audit-20260830.log`
  (exit 0; exactly the four reviewed lowering files plus this plan).

The focused 9-test matrix remains `7/9`: the two named destructor-body
targets pass, explicit destructor-call and constructor regressions pass, and
the two remaining focused failures are the known array-presentation cases.
Its durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-destructor-final-focused-20260830.log`.

## Checkpoint Ledger

| checkpoint | PA16 identities / coverage | delta / evidence | status |
| --- | --- | --- | --- |
| `1694bc3e` retained baseline | `200/243`, 43 failures, `243/243` covered | prior bit-field baseline | prior landed |
| `7e060b28` typed packed bit-field boundary | `202/243`, 41 failures, `243/243` covered | prior focused bit-field evidence retained | prior landed |
| `d95a6fe7` local-class checkpoint start | `202/243`, 41 failures, `243/243` covered | prior local-class selection | prior checkpoint |
| `d83e927f` typed local-class materialization | `206/243`, 37 failures, `243/243` covered | prior focused matrix, through-PA15, and audit passed | prior landed |
| `PA16 typed exception-safe destructor suffix` | `208/243`, 35 failures, `243/243` covered | baseline-only exactly the two fixed destructor targets; final-only empty; prior gate `1167/1167`; audit and diff check pass; N=1/3/8 evidence recorded | validated final; committed in handoff |

The remaining 35 identities are outside this checkpoint boundary. Future work
must preserve the canonical typed action range, active owner flow, explicit
cleanup semantics, and the recorded output-complexity rationale.
