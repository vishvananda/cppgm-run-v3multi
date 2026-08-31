# PA16 typed bit-field value-boundary checkpoint

## Stage Design

PA11 creates the canonical `BitFieldFact`: declared, storage, and operation
types; signedness; value/storage widths; mask; and packed offset.  PA12 owns
promotion and builtin typing from that fact.  PA15 materializes a member
lvalue, extracts and encodes through the fact, performs packed-unit
read/modify/write, and owns prefix/postfix increment.  This checkpoint keeps
that single typed path intact from fact to LowIR value boundary.

## Failure Map

The supplied baseline is `make test-pa16` exit 2, 238/243, with exactly:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t` — array
   lifetime/destruction owner; outside this checkpoint.
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t` —
   reference/index lowering owner; outside this checkpoint.
3. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t` — PA15
   update encoding and comparison-carrier boundary; in scope.
4. `pa16/tests/general/400-signed-bit-field-read.t` — PA11/PA15 signed
   extraction boundary; in scope, with the checked-in fixture omitting the
   required sign extension.
5. `pa16/tests/general/400-signed-enum-bit-field-read.t` — PA11/PA15 signed
   underlying-enum extraction boundary; in scope, with the same fixture
   contradiction.

## Active Checkpoint

Invariant: every bit-field load, operation, comparison, encode, and packed
store consumes the canonical typed/layout fact; semantic promotion remains
the operation type, signed reads retain represented negative values, and
updates preserve unrelated packed bits.

The implementation adds an explicit value-first encoding form for builtin
increment/decrement while initialization keeps its existing mask-first form.
For equality/inequality only, a narrow unsigned field may use its same-width
unsigned storage carrier; this does not change PA12 operation typing and is
representation-invariant for the field's non-negative range.  The PA13
consumer accepts that same-width equality carrier while keeping relational
operand validation exact.  Signed reads remain sign-extending, so the two
contradictory signed fixtures remain residuals unless their oracle is corrected
externally.  No array-lifetime or reference-index code is changed.

Focused validation covered the three 400 bit-field failures, a small PA16
bit-field regression matrix, the three existing typed course regressions, and
explicit PA13 carrier probes.  The remaining uncertainty is limited to the
checked-in signed-read shape discrepancy; the built PA13 consumer accepted
the intended carrier and rejected relational, width, and pointer mismatches.

## Performance Evidence

The boundary performs a fixed number of typed fact lookups and LowIR
operations per access/update.  Masking, shifting, sign extension, and packed
read/modify/write use bounded-width integer operations; the equality-carrier
check visits the two binary operands only, and no loop or cache is indexed by
field width or storage width.  Representative instruction shapes are
mask-first initialization, value-first 1/2-bit increment, signed 3-bit
extraction, signed-enum 2-bit extraction, and packed neighboring-field
preservation.

## Checkpoint Ledger

- Baseline: clean `ab4fa405`; PA16 238/243 with the exact five failures above;
  prior-through PA15 and file audit already pass.
- Changes: PA15 bit-field update encoding order, typed equality carrier,
  same-width equality validation in the PA13 consumer, and this compact plan.
- Focused result: `make -C dev cppgm++ lowir2cy86 CXX=g++` passes; the exact
  prefix `check` and refreshed `my` run/compare pass with no canonical diff;
  the seven local bit-field matrix tests are 7/7; course 412, 422, and 424
  pass; eq/ne same-width carrier probes pass while relational, width, and
  integer/non-integer probes reject.
- Broad/final result: `make test-pa16` exits 2 at 239/243 with no new failure;
  the exact prior-through command passes 1167/1167; file audit passes with
  six pre-existing header warnings; coverage is 243/243/243 with matching
  224-success/19-failure status distributions and zero sidecar mismatches.
- Residual set after fresh broad validation:
  `pa16/tests/general/200-local-default-class-array-lifecycle.t`,
  `pa16/tests/general/200-reference-indexed-pointer-member-access.t`,
  `pa16/tests/general/400-signed-bit-field-read.t`, and
  `pa16/tests/general/400-signed-enum-bit-field-read.t`.
- Commit: final checkpoint commit after `git diff --check`; id reported at
  handoff.
