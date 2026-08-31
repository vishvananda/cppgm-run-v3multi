# PA16 typed binary conversion-boundary checkpoint

## Stage Design

PA10 preserves explicit cast syntax.  PA12 publishes ordered typed
`SemanticFactId` children and appends each selected `ConversionFact` to the
owning fact's contiguous range.  PA15's
`Pa15Lowerer::lower_binary_expression` is the single builtin-binary owner:
the immediate operand kind `SemanticFactKind::CastExpression` identifies the
typed cast boundary.  A complete validated typed-range walk finds the
cast-owned prefix ending at the cast result; any parent contextual suffix
remains after both operand evaluations.  Nested binary expressions reuse this
boundary, while overloaded calls, comma, and short-circuit operators retain
their existing owners.

The repair keeps the PA10 -> PA12 -> PA15 -> typed LowIR flow.  It adds no
textual reconstruction, second semantic owner, retry, or cache shortcut.
`validate_conversion_range` validates the complete fact range before a
subrange is used: identity and bounds are checked, an empty range must have an
invalid begin, and adjacent typed records must connect exactly except for the
known PA12 reference-temporary retarget from the reference target back to the
fact's value type.  The binary boundary validates the cast child source and
result continuity, accepts a valid zero-owned wrapper whose first contextual
edge starts at the cast result, and rejects malformed/discontinuous ranges.

This is the compact `spec.md` Purpose and §§1-5/§7 alignment: one production
pipeline, typed fact continuity and ownership, deterministic bounded work, and
typed LowIR without an unsupported performance claim.

## Failure Map

Fresh final `make test-pa16` exits 2 at 238/243 and retains exactly the
turn-start five residual identities:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t` — reverse
   array destruction; outside this checkpoint.
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t` —
   possible explicit index widening omission; outside this checkpoint.
3. `pa16/tests/general/400-bit-field-prefix-postfix-increment.t` — typed
   bit-field carrier/operand orientation; outside this checkpoint.
4. `pa16/tests/general/400-signed-bit-field-read.t` — existing signed read
   boundary; outside this checkpoint.
5. `pa16/tests/general/400-signed-enum-bit-field-read.t` — existing signed
   enum read boundary; outside this checkpoint.

The landed increment resolved
`pa16/tests/general/300-nested-enum-hidden-friend-bitmask-adl.t`.  The final
fresh failure identity set equals the supplied authority set: no residual was
added, removed, or reassigned by this checkpoint.

## Active Checkpoint

The landed code correctly recognized immediate typed cast operands for the
active hidden-friend body, but applied their entire shared conversion range
before the sibling operand.  PA12 can append a parent promotion to that same
range, so `static_cast<short>(left_value()) + right_value()` previously
emitted the `short -> long` promotion before `right_value()`.

The repair applies the cast-owned prefix contiguously with the operand and
applies the remaining contextual suffix only after both operands.  The
member-pointer const-typedef return regression remains protected: its two
implicit `char -> int` promotions still occur after both pointer loads.
Nested/effectful operand lowering remains left then right; comma and logical
operators return through their existing lowering owners, and overloaded
operators remain call boundaries.

## Focused Evidence and Performance

- Final `make build`: exit 0.
- Active nested-enum test plus
  `200-member-pointer-const-typedef-return.t`: PASS (2/2).
- PA15 conversion/binary matrix: PASS (6/6).
- PA16 operator/member/friend/chained matrix: PASS (6/6).
- Course 432 cast-boundary regression: PASS; shell syntax also passes.
- Typed zero-owned `reinterpret_cast<const int&>(value) + right_value()`
  probe: PASS, preserving left load, right call, then binary add.
- Final `git diff --check`: exit 0.

The structural bound is linear in the number of conversion records consumed:
the complete-range validator and cast-boundary walk scan typed records, with no
fixed edge-count assumption, and the fixed number of split/application calls
keeps each binary boundary O(number of consumed conversions).  LowIR emission
remains deterministic and each selected record is applied in range order.  No
timing, RSS, allocation, generated-code, or other material performance claim
is made.

Fresh broad PA16 is exit 2 at 238/243 with exactly the five identities above.
The discovered `.t`, checked-in `.ref.exit_status`, and fresh
`.my.exit_status` identity sets are each complete and equal at 243/243/243;
status mismatches are 0, and both reference and fresh distributions are 224
`EXIT_SUCCESS` / 19 `EXIT_FAILURE`.  The exact prior-through gate is
`1167/1167`.  The exact file audit passes with six known nonfatal header
`bad-division` warnings and no fatal findings.

## Checkpoint Ledger

- Start: clean `71a40cfd`; supplied PA16 authority was exit 2 at 238/243 with
  the exact five residuals and complete 243/243 source/reference/status-sidecar
  coverage.
- Investigation: the immediate `CastExpression` kind is the correct typed
  discriminator for this special boundary, but its shared range can contain
  parent contextual conversions.  The typed result and child source establish
  the cast-owned prefix; typed result-source continuity identifies valid
  zero-owned reference wrappers.
- Repair: `lower_binary_expression` now discovers that prefix by a complete
  linear walk; `apply_conversions` validates the whole typed range before
  consuming validated subranges and rejects empty ranges with a begin.  Course
  432 is the sole added public regression because the ordering defect is
  observable in LowIR.
- Final focused result: build 0; active/member pair 2/2; PA15 matrix 6/6;
  PA16 matrix 6/6; zero-owned reference probe pass; course 432 pass; shell
  syntax and diff-check 0.
- Final broad result: PA16 238/243 with the exact unchanged five residuals;
  coverage 243/243/243, equal identity sets, zero status mismatches, and
  matching 224/19 status distributions; prior-through 1167/1167; file audit
  pass with six known warnings.
- Completed disposition: the bounded typed binary conversion-boundary audit
  and directly caused repair are complete.  No array-lifetime, pointer-index,
  or bit-field residual owner was changed.
- Next checkpoint: separately scoped
  `pa16/tests/general/200-local-default-class-array-lifecycle.t`.
