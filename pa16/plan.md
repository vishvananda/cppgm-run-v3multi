# PA16 implementation plan

## 1. Stage Design

This checkpoint is bounded to the PA16 purpose and `spec.md` Purpose plus
sections 1, 2, 5, and 7.  N3485 [expr] p11 and [expr.static.cast] p6 make an
explicit conversion to `void` a discarded-value expression: the operand is
evaluated, but ordinary nonvolatile lvalue-to-rvalue conversion is not
generally implied.  The residual PA16 fixtures require a stable O0 read at
one typed boundary, so the implementation keeps that exception narrow.

The production data flow remains one typed pipeline:

```text
PA10 syntax
  -> PA11 canonical types/bindings/scopes
  -> PA12 semantic_cast_to_target: CastExpression + ToVoid conversion fact
  -> PA15 CastExpression validation + typed discarded lowering
  -> typed LowIR
```

PA12 is the producer and owner of the selected `ConversionKind::ToVoid` fact.
For a normal explicit cast it publishes one conversion whose source is the
operand's `expression_object_type`, whose target is the cast target, and
whose kind is `ToVoid`; `add_conversion` and `set_fact_conversion` keep that
fact typed and contiguous.  The PA12 array-to-void-pointer special case is a
different, two-record pointer conversion and does not enter the void-target
consumer.

PA15 first validates the cast child and the single conversion range, kind,
source, and target before passing an explicit
`DiscardedExpressionContext::ExplicitToVoid` to the existing discarded-value
consumer.  The extra scalar read is enabled only for a direct `IdExpression`
that is an lvalue of scalar object type, has a valid non-reference
`BindingKind::Parameter` binding, and therefore has initialized formal storage
at function entry.  Ordinary local variables and other lvalue-producing facts
remain non-materializing.

The existing function/reference early exits, volatile reads, class-lvalue
address materialization, comma and void-conditional sequencing, and
assignment/increment/decrement effect paths remain intact.  No source spelling,
name recovery, lookup retry, or parallel semantic path is introduced.  The
invariants are typed conversion ranges, direct fact/binding ownership, source
evaluation order in LowIR, and no redundant assignment or increment result
load.  The decision reads one fact, binding, and type tuple: O(1) time and
O(1) additional storage per discarded fact, with no cache or whole-program
scan.

## 2. Failure Map

The parent/increment provenance is separate from the checkpoint authority:
parent `14cadc0c135156ed20583e3b5adb07b1260cabe2` was recorded at `222/243`
passing with `21` failures and `243/243` identities covered.  The supplied
turn-start authority for landed commit
`6d2ed09cd4b3daf55ab28282addcf3a878a8adba`, in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`, is
`224/243` passing with exactly `19` failures and `243/243` identities
covered.  The `222/243` result is not the current regression budget.

The authoritative current failure set is:

```text
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

The landed increment's exact parent-to-checkpoint delta is the two repaired
identities `100-function-pointer-nested-param-name-shadow.t` and
`300-enum-class-nonmember-operator-bitand.t`; fresh-only identities are `0`.
The final primary stage log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/test-pa16.log`:
status `2`, `224/243` passed, and exactly the 19 identities above.  The exact
comparison at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/identity-comparison.log`
reports authority/fresh failures `19/19`, retained `19`, authority-only `0`,
fresh-only `0`, and discovered/reference/fresh inventories `243/243/243` with
all missing/unexpected counts `0`.  No residual identity is reclassified.

## 3. Active Checkpoint

The checkpoint audit follows the PA12 `semantic_cast_to_target` producer into
the PA15 `CastExpression` consumer.  The producer-side facts are complete for
valid input; no PA12 owner change is required.  The genuine consumer defect
was a missing fail-closed source/target check after the existing kind/range
check.  The repair is confined to `dev/src/pa15_lowering_flow.cpp`: a void
cast now requires exactly one in-range `ToVoid` record whose source matches the
child fact's object type and whose target matches the cast fact's type.

Discarded lowering then preserves the following typed boundaries:

- direct non-reference scalar formal parameters may receive the narrow O0
  read required by the checked-in PA16 fixtures;
- ordinary nonvolatile locals and ordinary scalar/reference lvalues do not
  receive an implied read;
- volatile scalar lvalues are loaded, while function ids and reference-bound
  ids remain evaluation no-ops;
- explicit class lvalues materialize their address only;
- comma and void-conditional expressions preserve sequencing and lower their
  children in ordinary discarded context; assignment and increment/decrement
  paths retain their side effects without a redundant result load.

Validation and final gates:

```text
make -C dev cppgm++ CXX=g++
  status 0; log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/reconfirm-rebuild.log

make -C pa16 check TEST="tests/general/100-function-pointer-nested-param-name-shadow.t tests/general/300-enum-class-nonmember-operator-bitand.t tests/general/200-derived-pointer-member-init.t"
  PASS (3/3); log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/reconfirm-focused-pa16.log

make -C pa15 check TEST="tests/general/200-literal-logical-short-circuit-omits-unreachable-call.t tests/general/200-for-iteration-discards-void-comma-rhs.t tests/general/200-comma-expression-xvalue-reference-return.t tests/general/200-return-void-call-expression.t"
  PASS (4/4); log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/reconfirm-focused-pa15.log

n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi
  status 0; ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167); log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/prior-through-pa15.log

make test-pa16
  status 2; TEST SUMMARY: 224 / 243 TESTS PASSED; log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/test-pa16.log

perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src
  status 0; five pre-existing header-body warnings; log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/file-audit.log

exact failure/inventory comparison against last-test.log
  status 0; failures 19 -> 19, authority-only 0, fresh-only 0;
  discovered/reference/fresh 243/243/243; all missing/unexpected 0;
  log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/identity-comparison.log

git diff --check
  status 0; log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/final-diff-check.log

bounded changed-path/file audit
  status 0; only the three approved paths were changed and the index was empty;
  log /home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/changed-path-audit.log
```

The durable log directory is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-checkpointAudit-6d2ed09c-final-20260830/`.
The earlier O0 structural probe remains at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/checks/pa16-checkpoint-to-void-probe.log`;
it covers explicit pointer/enum parameters, an uninitialized local, ordinary
scalar/reference lvalues, and assignment/increment controls without adding a
checked-in test.

## 4. Performance Evidence

The risk is one extra scalar load and corresponding LowIR growth at the
explicit typed boundary.  The prior O0 probe compiled eight functions and
emitted 22 instruction lines; the seven checked functions emitted 21.  Its
structural counters (instruction lines, `load ptr`, `load i32`) are:

| Probe function | Instructions | `load ptr` | `load i32` |
| --- | ---: | ---: | ---: |
| `explicit_pointer` | 3 | 1 | 0 |
| `explicit_enum` | 3 | 0 | 1 |
| `explicit_uninitialized_local` | 1 | 0 | 0 |
| `ordinary_scalar` | 2 | 0 | 0 |
| `ordinary_reference` | 2 | 0 | 0 |
| `assignment_control` | 4 | 1 | 0 |
| `increment_control` | 6 | 1 | 1 |

The selected generated functions have `do_start_op`: 11 instructions and
four loads (`3` pointer, `1` i32), including exactly one `on_immediate` load;
the enum operator has 5 instructions and exactly two i32 loads.  These are
structural IR counters, not a timing claim.  The new invariant checks and the
discarded decision are bounded constant work per explicit cast/fact, with no
allocation, cache, whole-program traversal, dependency edge, or repeated
lowering.  No material timeout or performance risk is indicated by the
representative evidence; no benchmark conclusion is drawn from one sample.

## 5. Checkpoint Ledger

| Commit | Status |
| --- | --- |
| `24d555c8` | Completed the prior PA16 checkpoint audit; its focused and broad evidence, exact 21-identity comparison, full 243-identity coverage, file audit, and clean-tree verification remain historical record. |
| `6d2ed09c` | Completed checkpoint audit: PA12 ToVoid ownership is sound, PA15 now fail-closes in-range typed source/target mismatches, and the narrow discarded-value behavior and controls are preserved. Reconfirmation is build `0`, PA16 `3/3`, PA15 `4/4`; prior-through is `1167/1167`; final PA16 is status `2` at `224/243` with exactly the same 19 failures; identity comparison is `19 -> 19`, authority-only/fresh-only `0/0`, and discovered/reference/fresh `243/243/243`; file audit is status `0` with five pre-existing warnings; diff-check and final path audit pass. Evidence is in the durable final checkpoint directory. |
