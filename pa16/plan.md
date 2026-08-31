# PA16 nested braced aggregate-member checkpoint

## Stage design and spec alignment

`PA10` recognizes `Value{...}` as one `CallExpression` containing one
`BracedInitList`; the renderer preserves braces while ordinary parenthesized
calls retain parentheses.  `PA12`'s `AggregateAppertainer` accepts that shape
only when its typed functional target is the exact destination class record,
then forwards the existing AST argument nodes to
`semantic_aggregate_constructor_value` and `select_constructor`.  The result
is one typed `ConstructorAction` with selected binding, scope, callable type,
and converted semantic argument edges.  `PA15` consumes those edges directly,
projects the aggregate member/subobject address before any permitted call
elision, and emits every observable argument or constructor body.

This is one forward pipeline under `spec.md` Purpose and §§1–5,7: no source
text reparse, second semantic model, broad retry, host/reference compiler, or
untyped class-value transfer.  Unsupported or malformed shapes fail closed.
The PA16 README boundary still excludes general temporary materialization,
copy/move transfer, by-value parameter/return semantics, virtuals, and eager
helpers.

## Exact failure map

Turn-start authority is `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`make test-pa16` exited `2` at `235/243`, with complete `243/243` identity
coverage and exactly these eight residual owners:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t`
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
3. `pa16/tests/general/200-unnamed-namespace-hidden-friend-single-definition.t`
4. `pa16/tests/general/300-friend-function-definition-skip.t`
5. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`
6. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
7. `pa16/tests/general/400-signed-bit-field-read.t`
8. `pa16/tests/general/400-signed-enum-bit-field-read.t`

The nested-braced target is outside that residual set and passes focused
validation below.  Prior-through and file-audit results were reused at turn
start, then freshly rerun after the audit repair.  No added pass changes the
failure ceiling or coverage requirement.

## Current audit milestone

The landed checkpoint is `2e48cd6dd87726828ee329d6b47e89435d964fc0`, relative
to parent `67d8f53b`.  The reviewed implementation boundary is
`dev/src/pa10_ast.cpp`, `dev/src/pa10_renderer.cpp`,
`dev/src/pa12_semantic_aggregate.cpp`, `dev/src/pa15_lowering.h`,
`dev/src/pa15_lowering_aggregate.cpp`,
`dev/src/pa15_lowering_construction.cpp`, and regression 429.  This audit's
documentation boundary additionally includes this plan and `pa16/audit.md`.

The repair makes the constructor no-op memo explicitly two-dimensional:
`(FunctionFactId, require_empty_parameters)`.  The old single vector was not
call-order independent: an empty user constructor with parameters is false
when the policy requires an empty signature and true when typed parameters are
allowed, while the old complete-cache branch ignored the policy on later
calls.  The policy-separated dense caches preserve cycle invalidation for both
queries.  The aggregate suppression proof now also requires exact action type,
valid binding/layout ownership, and the selected binding's hidden-object
callable signature and parameter edges.

The side-effect-free recursion validates fact/child ranges and types, rejects
volatile facts, calls, assignments, increment/decrement, identifiers, and
unknown kinds conservatively, and treats cycles as effectful.  Only a complete,
memberless, non-union class with a non-synthetic empty-body constructor and
typed side-effect-free arguments can be suppressed.  Member projection/address
calculation remains before that final suppression decision; arrays, ordinary
zero-initialization, nonempty members, volatile reads, effectful arguments, and
effectful constructor bodies remain observable.

## Structural performance evidence

The parser creates one braced AST node and PA12 performs one typed argument
forwarding pass.  Constructor selection retains its existing `O(C*A)`
candidate/argument work.  PA15 uses one memo/visiting map-set traversal over
reachable typed facts, structurally `O((A+E) log(A+E))` with `O(A+E)` temporary
storage; the constructor no-op policy caches are dense `O(F)` per policy and
the zero-initialization cache is dense `O(T)`.  Regression 429 uses a fixed
64-argument member constructor and checks pure projection retention,
side-effecting argument evaluation, an effectful constructor body, and a
volatile read.  These are structural bounds and bounded scale evidence only;
there is no timing or RSS claim.

## Focused and final-gate evidence

The following commands were run after the repair:

```text
make build                                                        # exit 0
sh -n cppgm.tests/course/pa16/429-nested-braced-aggregate-member-regression.sh
                                                                    # exit 0
sh cppgm.tests/course/pa16/429-nested-braced-aggregate-member-regression.sh
                                                                    # PASS
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-nested-braced-member-aggregate-init.t'
                                                                    # PASS (1/1)
make -C pa16 CPPGM_SKIP_DEV_REBUILD=1 check TEST='tests/general/200-aggregate-class-member-subobject-init-target.t tests/general/200-member-initializer-aggregate-member.t tests/general/200-aggregate-array-member-brace-elision.t tests/general/300-value-init-empty-functional-cast-aggregate.t tests/spec/200-direct-list-init-explicit-ctor.t tests/general/200-copy-list-init-explicit-ctor-bad.t'
                                                                    # PASS (6/6)
sh cppgm.tests/course/pa16/409-typed-constructor-boundary-regression.sh
                                                                    # exit 0 (silent)
sh cppgm.tests/course/pa16/429-nested-braced-aggregate-member-regression.sh
                                                                    # PASS
make test-pa16                                                     # exit 2; 235 / 243
n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
                                                                    # exit 0; 1167 / 1167
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
                                                                    # exit 0; 6 warnings, no fatals
git diff --check                                                   # exit 0
```

The fresh PA16 failure set equals the turn-start authority: `8` versus `8`,
with `authority-only=0` and `fresh-only=0`.  Identity coverage is complete at
`243/243`; missing and unexpected identities are `0/0`.  The file-audit
warnings are the six existing `bad-division` findings in
`dev/src/abi_mangle.h`, `dev/src/cpp_semantic_core.h`, `dev/src/lowir_model.h`,
`dev/src/pa11_semantic_model.h`, `dev/src/pa12_semantic_selection.h`, and
`dev/src/pa15_lowering.h`.  No handout test, fixture, reference,
exit-status sidecar, harness, comparator, generated output, source-set file,
or unrelated residual owner was changed.

## Uncertainties and next checkpoint

The full eight-owner failure map is unchanged by fresh comparison.  The no-op
proof deliberately does not cover unsupported temporary/copy paths or
constructors with bodies/actions, and no claim is made for unrelated residuals.
The six file-audit warnings are known header findings, not checkpoint defects;
no in-scope correctness uncertainty remains.  PA16 remains incomplete.  Next
checkpoint: `200-local-default-class-array-lifecycle.t`, the first residual
owner.

## Checkpoint ledger

| checkpoint | compact result |
|---|---|
| d54e32d1 | Authority was 224/243 with 19 failures and complete identity coverage. |
| b58ddd2a | Typed `nullptr_t` carrier path completed through PA11/PA12/PA15. |
| e09d8223 | Recorded nullptr state: 225/243 authority, 18 residual failures, 243/243 inventories. |
| d5bf2600 | Typed constructor-overload/lifetime audit reached 227/243 with 16 residual failures. |
| 29d9c4ce | PA10 elaborated-member parameter repair/PA15 ABI ownership reached 228/243 with 15 residuals. |
| 69bbe800 | Empty-base layout/address projection reached 230/243 with 13 residuals. |
| 75f7944a | Empty-base identity validation audit completed; through-PA15 remained 1167/1167. |
| 2ca2323a | Clean turn-start state; baseline 230/243 and exact 13-item map. |
| 2cfa1111 | Final committed UDL checkpoint: PA16 231/243, exact 12 failures, 243/243 coverage. |
| 4a5bbdd5 | Clean turn-start baseline: PA16 231/243 with complete 243/243 coverage. |
| `617c137a` typed object-call boundary checkpointAudit | Completed committed audit/repair at 234/243 with the exact unchanged nine residual identities and complete coverage. |
| `2e48cd6d` nested-braced aggregate-member checkpoint | Completed bounded audit/repair and documentation: fresh PA16 is exit `2` at `235/243` with the exact unchanged eight residual identities and `243/243` coverage; authority-only/fresh-only are `0/0`; through-PA15 is `1167/1167`; file audit exits `0` with six known header warnings; focused target/control checks are `1/1` and `6/6`, course 409 exits `0`, and regression 429 plus syntax pass. PA16 remains incomplete. |
