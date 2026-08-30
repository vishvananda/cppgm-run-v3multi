# PA16 implementation plan

## Stage Design

PA16 keeps one production pipeline: PA10/PA11 syntax and names become PA12
typed semantic facts and conversion facts, and PA15 lowers those facts into
LowIR.  This checkpoint uses the existing `SemanticFact` truth metadata,
`ConversionFact` source/target and truth policy, and `LoweredValue` semantic
`type`/physical `physical_type`; it adds no text transport, parallel analyzer,
rescan/cache, retry, or second lowerer.  The relevant LowIR contract is that a
comparison carries its operand type and produces canonical integer truth, and
that an explicit conversion owns the destination type.

## Failure Map

Turn-start authority is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
`214/243` passed, exactly `29` failed, and `243/243` identities are covered.
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
- `pa16/tests/general/400-bitfield-aggregate-init.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

The three identities recovered by the landed increment and therefore absent
from this authority are `200-nonliteral-field-condition-not-folded.t`,
`200-placement-new-expression-constructor-call.t`, and
`300-pragma-pack-followed-by-endif.t`.  Fresh comparison is exact: authority
and fresh failures are both `29`, baseline-only and fresh-only are both `0`,
the independent inventory and run total are both `243`, and coverage is
`243/243`.

## Active Checkpoint

The landed increment repairs the typed truth/comparison physical-width
continuity boundary for the three recovered identities:
`200-placement-new-expression-constructor-call.t`,
`300-pragma-pack-followed-by-endif.t`, and
`200-nonliteral-field-condition-not-folded.t`.  This bounded audit found and
repairs one over-broad class-pointer classifier: the object-layout helper
unwraps arrays, so it cannot be used directly to classify a pointer's
pointee.  The finalizer now strips cv qualification only, requires an exact
`Named` pointee, and then validates the class record.  Thus `Class*` retains
the typed truth policy while `Class (*)[N]`, scalar pointers, invalid ranges,
and non-pointer operations remain materialized.

The shared owner/data flow is:

1. `semantic_binary_expression` publishes comparisons as semantic `bool`
   facts with `canonical_truth=true` and records their owned conversions.
2. `CanonicalTruthFinalizer::publish_conversions` preserves a bool-origin
   canonical truth value only at typed boundaries represented by the fact:
   member-derived truth, `sizeof`/size-type-derived truth, and exact
   class-object-pointer comparisons.  Plain procedural comparisons retain
   `Materialize`, and bool storage/ABI destinations remain u8.
3. `finish_member_call` publishes the same bool-result
   `bool_context_operand` and `direct_bool_boundary` metadata already emitted
   by all other relevant call owners, for both static and non-static results.
4. PA15 keeps canonical truth's internal physical carrier at i64 while the
   semantic result remains bool/u8 at storage boundaries.  `cmp` retains the
   actual typed operation operand, and conversion policy is consumed once
   without redundant truncation at the explicitly typed boundary.

The placement-new and pragma-pack/parser/layout machinery named by the
recovered fixtures is not changed; only their shared truth-conversion
boundary is in scope.  No focused regression, test, fixture, harness,
comparator, or reference/host execution is added or changed.

## Performance Evidence

The finalizer remains linear in semantic facts, owned conversions, and result
edges; the new exact-pointee classifier is O(1) per canonical-truth fact with
bounded range checks.  PA15 still emits each expression fact/instruction once;
there is no rescan, cache, whole-program retry, or duplicate lowering.
Focused structural probes show `Class*` uses the typed direct boundary while
`Class (*)[2]` and scalar pointers use ordinary materialization.  No timing,
RSS, or code-size measurement was taken, so no measured performance claim is
made.

## Validation

The focused and broad checks passed with the expected incomplete-stage result:

- `make cppgm++ CXX=g++` from `dev/`: status `0`.
- PA16 focused target/control set: status `0`, `PASS (7/7)`.
- PA15 condition, call, sizeof, and conversion controls: status `0`,
  `PASS (5/5)`.
- Direct probes exit `0`: `Class*` is direct `zext i32 u8`, while
  `Class (*)[2]`, `int*`, and `int (*)[2]` retain ordinary truncation followed
  by zext.
- `make test-pa16`: status `2`, `214/243`, exactly the authority's 29
  failures, no baseline-only or fresh-only identity, and `243/243` coverage.
- The exact `n=16` through-PA15 command: status `0`, `1167/1167`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`
  with five pre-existing header-division warnings and no fatal finding.
- Final changed-path audit: status `0`, limited to the three scoped files.
- `git diff --check` after all source and record edits: status `0`.

Raw logs and statuses for these results are in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-truth-width-audit-final-20260830/`.

## Next Checkpoint

PA16 remains incomplete with the exact 29 residual identities above.  The next
checkpoint should select the largest coherent remaining typed owner while
preserving the distinction between typed truth boundaries and plain procedural
materialization.

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
