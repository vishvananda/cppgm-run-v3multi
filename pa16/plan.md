# PA16 typed bit-field value-boundary checkpoint

## Stage Design

PA11 publishes one canonical `BitFieldFact` per named field: declared,
storage, and operation types; signedness; value/storage widths; masks; and
packed offsets.  PA12 owns promotion and builtin operation typing from that
fact.  PA15 owns typed lvalue projection/replay, extraction and sign
extension, prefix/postfix update, value encoding, and packed-unit
read/modify/write.  This checkpoint audits that single path through typed
LowIR and its PA13 consumer.

This aligns with `spec.md` Purpose and §§1–5/§7: one forward typed pipeline,
canonical identity and ownership, bounded deterministic work, typed LowIR at
the phase boundary, and no textual reconstruction or unsupported performance
claim.

## Failure Map

The supplied turn-start authority is `make test-pa16` exit 2 at `239/243`,
with complete coverage and exactly these four residuals:

1. `pa16/tests/general/200-local-default-class-array-lifecycle.t` — array
   lifetime/destruction owner; outside this checkpoint.
2. `pa16/tests/general/200-reference-indexed-pointer-member-access.t` —
   reference/index lowering owner; outside this checkpoint.
3. `pa16/tests/general/400-signed-bit-field-read.t` — signed extraction shape
   disagreement; preserved as a residual.
4. `pa16/tests/general/400-signed-enum-bit-field-read.t` — signed underlying
   enum extraction shape disagreement; preserved as a residual.

## Active Checkpoint

The PA15 equality carrier is limited to the canonical case where a non-signed
bit-field has narrow unsigned `u32` storage and PA12 selects `i32` as its
operation comparison type.  This fact-based boundary also covers an
unsigned-underlying enum when those typed facts select the same endpoints.  The
checked LowIR shape spells `cmp eq/ne u32` while the extracted producer may
remain `i32`; equality is representation invariant for the field's represented
value.  The PA15 producer and PA13 consumer now recognize only that typed
direction.  Other widths, reverse signedness, relational predicates, pointers,
and non-field operations remain exactly typed and fail closed.

Value-first encoding is semantically equivalent to mask-first `AND`, but is
the deterministic public LowIR order required by the existing prefix/postfix
oracle.  Initialization retains mask-first order.  Packed stores still clear
only the canonical field mask and OR the encoded value into the loaded unit.

Direct, implicit-`this`, pointer, and nested member roots retain canonical
binding lookup.  Cast and const-reference wrappers use their own typed
conversion/reference-temporary paths and do not need to borrow field identity.

## Performance Evidence

The affected path uses a fixed number of canonical fact lookups and LowIR
actions per access/update; the equality carrier examines the two operands and
does not scan by field width.  Masking, shifting, sign extension, and packed
read/modify/write use bounded-width integer operations.  These are structural
bounds only; no timing, allocation, RSS, or generated-code performance claim
is made.

## Checkpoint Ledger

- Baseline: clean `177b845f` relative to parent `ab4fa405`; supplied PA16 is
  `239/243` with the exact four residual identities above.
- Audit repair: PA15's carrier producer is restricted to `u32` storage versus
  `i32` operation typing, and the PA13 consumer replaces broad same-width
  equality acceptance with that one `i32 -> u32` carrier pair.  The wording
  covers non-signed canonical fields, including the focused unsigned-underlying
  enum case when the facts select those endpoints.
- Focused evidence: the five affected checks preserve the four supplied
  failures and pass the prefix test; courses 412, 422, 424, and 433 pass;
  valid carrier and malformed-consumer probes pass their expected outcomes.
- Final stage evidence: `make test-pa16` is exit 2 at `239/243`, with an exact
  retained four-identity failure set; tests/reference/fresh sidecars are
  complete at `243/243/243`, with zero content mismatches and matching
  `224/19` status distributions.
- Required earlier-stage/file gates: `make test-report-through-pa15` exits 0
  at `1167/1167`; the exact PA16 file audit exits 0 with six known nonfatal
  header warnings.
- Completed disposition: bounded PA16 checkpoint audit and direct consumer/path
  repair are complete.  No handout, fixture, reference, sidecar, harness,
  comparator, generated output, source-set, or unrelated residual owner
  changed.
- Next checkpoint: separately scoped
  `pa16/tests/general/200-local-default-class-array-lifecycle.t`.
