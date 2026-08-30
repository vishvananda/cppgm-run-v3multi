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
`211/243` passed, exactly `32` failed, and `243/243` identities were covered.
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

The final fresh PA16 map has `29` failures and `214/243` passes.  It is the
turn-start map minus exactly these three authority-only identities:

- `pa16/tests/general/200-nonliteral-field-condition-not-folded.t`
- `pa16/tests/general/200-placement-new-expression-constructor-call.t`
- `pa16/tests/general/300-pragma-pack-followed-by-endif.t`

The comparison has `fresh_only=0`, `authority_only=3`, inventory `243`, and
`243/243` coverage; no fresh failure or coverage identity was introduced.

## Active Checkpoint

This increment repairs the typed truth/comparison physical-width continuity
boundary for the three named residuals:

- `200-placement-new-expression-constructor-call.t`
- `300-pragma-pack-followed-by-endif.t`
- `200-nonliteral-field-condition-not-folded.t`

The shared owner/data flow is:

1. `semantic_binary_expression` publishes comparisons as semantic `bool`
   facts with `canonical_truth=true` and records their owned conversions.
2. `CanonicalTruthFinalizer::publish_conversions` now preserves a bool-origin
   canonical truth value at the typed PA16 boundaries represented by the fact:
   member-derived truth, `sizeof`-derived truth, and comparisons whose typed
   operation is a class-object pointer.  Plain procedural comparisons retain
   `Materialize`, and a bool target remains a materialization/storage boundary
   (`u8`); the existing member-derived direct-boundary behavior is unchanged.
3. `finish_member_call` now publishes the same bool-result
   `bool_context_operand` and `direct_bool_boundary` metadata already emitted
   by the other call owners.
4. PA15 consequently keeps canonical truth's internal physical LowIR carrier
   at `i64` while its semantic result remains `bool`/`u8`.  The `cmp` operand
   spelling remains the typed consumer/operation width (`ptr`, `i64`, `u8`,
   and so on); this fix does not make every comparison operand `i64`.  The
   non-bool return/condition conversion at those typed boundaries can emit the
   direct `zext i32 u8` shape without a redundant `trunc u8 i64`.  Logical
   lowering also sees a bool-valued member call and chooses an `i64` comparison
   consumer, as the existing typed condition path requires.

The placement-new and pragma-pack/parser/layout machinery in the first two
fixtures is not changed; it is only the shared truth conversion boundary that
is in scope.  The remaining 29 authority identities, general ABI redesign,
unrelated condition semantics, tests, fixtures, harnesses, comparators, and
reference/host execution are excluded.  No focused regression is added because
the three existing public boundary fixtures exercise the owner directly.

## Performance Evidence

The fix performs O(1) work per canonical-truth conversion and per member-call
fact, including constant typed-operation classification, with the
existing finalizer remaining linear in semantic facts, owned conversions, and
result edges.  PA15 still emits each expression fact/instruction once; there
is no rescan, cache, whole-program retry, or duplicate lowering.  Focused
before/after evidence is structural: each of the first two named cases changes
the result tail from two conversions (`trunc u8 i64` then `zext i32 u8`) to one
(`zext i32 u8`), while the field-condition case keeps the same call/compare/
store sequence count and changes only `cmp ne u8` to `cmp ne i64`.  The final
PA16 run confirms no additional identity delta after the scoped correction;
the six initially affected PA15 shape controls all return to their original
materialized form and pass.

## Validation

The mandatory first-stop checks passed.  The first broad trial exposed a
same-owner overreach; the final scoped correction and all required gates are
recorded here:

- `make cppgm++ CXX=g++` from `dev/`: status `0`.
- `make check TEST='tests/general/200-placement-new-expression-constructor-call.t tests/general/300-pragma-pack-followed-by-endif.t tests/general/200-nonliteral-field-condition-not-folded.t tests/general/200-placement-new-expression-aggregate-brace.t'` from `pa16/`: status `0`, `PASS (4/4)`.
- `git diff --check`: status `0`; `git status --short` shows only the two
  PA12 source files and this plan.

- Post-correction `make cppgm++ CXX=g++`: status `0`.
- Post-correction PA16 target/control check: status `0`, `PASS (6/6)`.
- Post-correction PA15 regression-control check: status `0`, `PASS (6/6)`.
- Final `make test-pa16`: status `2` because the expected residuals remain;
  `214/243` passed, `29` failed, and `243/243` identities were covered.  Raw
  output/status: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-truth-width-20260830/final-make-test-pa16.log` and
  `final-make-test-pa16.status`.
- Final identity/coverage comparison against the supplied 32-failure
  authority: `authority_failures=32`, `fresh_failures=29`,
  `authority_only=3`, `fresh_only=0`, `inventory=243`,
  `fresh_passes_plus_failures=243`, and zero missing/unexpected coverage.
  Raw comparison: `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-truth-width-20260830/identity-coverage-comparison.log`.
- The exact required `n=16` through-PA15 command: status `0`,
  `1167 / 1167`; raw output/status are in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-truth-width-20260830/final-through-pa15.log` and
  `final-through-pa15.status`.
- `perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src`: status `0`,
  with five existing `bad-division` warnings in `abi_mangle.h`,
  `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`, and
  `pa15_lowering.h`; raw output/status are in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-truth-width-20260830/final-file-audit.log` and
  `final-file-audit.status`.
- Final `git diff --check`: status `0`; raw output/status are in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-truth-width-20260830/final-git-diff-check.log` and
  `final-git-diff-check.status`.

## Next Checkpoint

PA16 remains incomplete with 29 residual identities.  The next checkpoint
should select the largest coherent typed owner from the remaining map while
preserving the finalizer’s distinction between typed PA16 truth boundaries and
plain procedural materialization.  Do not pivot into placement-new semantics
or pragma-pack layout based only on the fixture names.

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
| `85b819b7` turn-start authority | `211/243`, 32 failures, `243/243` covered | current baseline |
| `typed truth-width continuity (parent 85b819b7)` | final `214/243`, 29 failures, `243/243` covered; authority-only 3 named identities; fresh-only 0; through-PA15 `1167/1167`; audit 0 with five known warnings; diff-check 0 | landed in this checkpoint commit |
