# PA16 typed operand scheduling checkpoint

## Stage Design

PA12 owns typed `SemanticFactId` child order and each operand's
`ConversionFact` range.  PA15's `Pa15Lowerer::lower_binary_expression` is the
single builtin-binary lowering owner.  Operand-owned `CastExpression` ranges
are lowered contiguously with their operand; parent-assigned implicit ranges
retain the established schedule of evaluating both operands before applying
their contextual conversions.  Nested expressions reuse this boundary, while
overloaded calls, comma, and short-circuit operators keep their existing
owners.

The distinction is typed semantic ownership, not a rendered-name or test
branch.  It preserves the flow from semantic facts to typed LowIR with no
textual reconstruction, second semantic owner, retry, or added scan.  This
aligns with `spec.md` Purpose and §§1-5/§7: one production pipeline, typed fact
continuity, owner-recorded conversions, deterministic work, and typed LowIR.
The ordinary cost remains O(n) in lowered facts/instructions; the change adds
only constant work per binary node.

## Failure Map

The clean `HEAD c529ea6a` baseline was `make test-pa16` exit 2 at 237/243,
with all 243/243 identities covered and exactly these six residuals:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t` — reverse
   array destruction; unresolved here.
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t` —
   possible explicit index widening omission; unresolved here.
3. `pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t` — active
   typed binary operand scheduling; resolved by this checkpoint.
4. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t` — typed
   bit-field carrier/operand orientation; unresolved here.
5. `pa16/tests/general/400-signed-bit-field-read.t` — existing signed read
   boundary is preserved; unresolved here.
6. `pa16/tests/general/400-signed-enum-bit-field-read.t` — existing signed enum
   read boundary is preserved; unresolved here.

Final residuals are exactly baseline items 1, 2, 4, 5, and 6:

- `pa16/tests/general/200-local-default-class-array-lifecycle.t`
- `pa16/tests/general/200-reference-indexed-pointer-member-access.t`
- `pa16/tests/general/400-bit-field-prefix-postfix-increment.t`
- `pa16/tests/general/400-signed-bit-field-read.t`
- `pa16/tests/general/400-signed-enum-bit-field-read.t`

## Active Checkpoint

The hidden-friend body now emits left `i32` load/cast, right `i32` load/cast,
then `or`.  A first all-conversions reorder exposed
`200-member-pointer-const-typedef-return.t`, whose implicit `char -> int`
promotions must remain after both loads.  The shared boundary was corrected to
pair only typed `CastExpression` conversions and preserve the implicit range
schedule; the regression then passed.  ADL, `BindingId`, `FunctionFact`
demand, and function emission are unchanged.

## Performance Evidence

- Focused repair: `make build` exit 0; active plus regression
  `make -C pa16 check ...` `PASS (2/2)`; PA15 conversion/binary matrix
  `PASS (6/6)`; PA16 operator/member/friend/chained matrix `PASS (12/12)`;
  20/20 focused identities; `git diff --check` exit 0.
- Fresh broad `make test-pa16`: exit 2 as expected for residual LowIR
  mismatches, `238/243` passed.  Its five reported identities are exactly the
  final residual list above; active and member-pointer diffs are absent.  The
  approved six-item comparison is resolved-baseline-only `1`, fresh-only `0`,
  and new `0`.
- Identity audit: 243 source `.t` identities, 243 `.ref.exit_status`
  identities, and 243 `.my.exit_status` identities; all identity deltas are
  zero.  Reference and fresh sidecars are each 224 `EXIT_SUCCESS` and 19
  `EXIT_FAILURE`, with zero status mismatches.  The 19 expected compile-fail
  cases have no LowIR `.my` body, but remain covered by fresh status sidecars.
- The focused `pa16 check` harness writes `.check`; the broad normal harness
  `make test-pa16` regenerated `.my` from the current `dev/cppgm++`.  The fresh
  active `.my` has the required load/cast/load/cast schedule, its compare diff
  is absent, and ignored generated files are not tracked.
- Required prior gate, exactly
  `n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`,
  exited 0 at `1167/1167`.  `perl scripts/cppgm_file_audit.pl --stage pa16
  --paths dev/src` exited 0 with six known `bad-division` warnings.

## Checkpoint Ledger

- Start: clean `c529ea6a`; PA16 `237/243`, six residuals, and 243/243 status
  identities covered.
- Investigation: semantic child order and typed conversion range flow through
  `lower_expression_impl(..., defer_conversions)` and `apply_conversions` to
  typed LowIR; the initial broad run caught the implicit-promotion regression.
- Repair: retained the two-file scope and made the typed `CastExpression` /
  contextual-conversion ownership distinction in
  `dev/src/pa15_lowering_flow.cpp`; no tests, refs, harnesses, or comparators
  were edited, and generated outputs remain ignored and untracked.
- Final evidence: focused 20/20; PA16 238/243 with exact five residuals and
  243/243 status identity coverage; through-PA15 1167/1167; file audit and
  diff check pass.
- Commit: subject `PA16: schedule typed binary operand conversions`, parent
  `c529ea6a`; commit hash is intentionally not recorded in this committed
  plan.
- Remaining boundary: the exact five residual identities listed above remain
  separately scoped; array destruction, pointer-index widening, bit-field
  increment carrier orientation, and signed bit-field read owners are not
  changed here.
