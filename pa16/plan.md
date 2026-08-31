# PA16 typed user-defined-literal checkpoint

## Stage Design

The posttoken collector owns decoded `UserDefinedLiteralData` (kind, suffix,
prefix/value).  PA10 transfers each occurrence once into a sparse typed
sidecar range; source spelling remains presentation text only.  PA11 owns
literal-operator declaration identity and suffix matching through the shared
namespace/value indexes and ordinary using-directive lookup.  PA12 forms the
cooked string array/size arguments, applies normal typed call selection and
conversions, and records the selected call.  PA15 consumes that call and its
conversion/address facts through existing typed call and literal-storage
lowering.  No suffix/value reparsing, parallel semantic path, retained whole
program scan, or host/reference invocation is introduced.  This follows
`spec.md` §§1–4: one forward pipeline, typed fact continuity, canonical
owners, demand-driven lookup, and bounded work.

## Failure Map

Turn-start authority was 230/243 with complete 243/243 coverage and 13
failures.  The active UDL identity now passes; the exact current 12 residual
outputs are:

- syntax ingress: `200-nested-braced-member-aggregate-init`;
  resolved: `300-user-defined-string-literal-operator`
- reference/value-category/conversion:
  `200-reference-indexed-pointer-member-access`,
  `200-reference-member-class-init`, `300-overloaded-deref-user-assignment`
- lifetime/lowering: `200-local-default-class-array-lifecycle`
- linkage/emission/demand: `200-unnamed-namespace-hidden-friend-single-definition`,
  `300-friend-function-definition-skip`
- ADL/evaluation scheduling: `300-nested-enum-hidden-friend-bitmask-adl`
- overload lookup: `300-using-base-static-same-signature-derived-preferred`
- bit-field lowering/presentation: `400-bit-field-prefix-postfix-increment`,
  `400-signed-bit-field-read`, `400-signed-enum-bit-field-read`

The PA16 run reports 231/243; discovered tests and exit-status artifacts are
243/243/243 (`.t`/reference-status/fresh-status).  The current output has no
new failure identity; the other residuals remain outside this checkpoint.

## Active Checkpoint

Complete the C++11 cooked string UDL expression pipeline for
`operator ""_pick(const char*, size_t)`, including block `using namespace`
visibility, suffix-aware merging in the shared literal-operator lookup bucket,
normal selection/conversion facts, and ABI suffix emission.  The focused
course regression also checks a distinct `_other` suffix and a 64-suffix
visible scale leg selecting late `_slot63`.

## Performance Evidence

The implementation performs O(1) typed sidecar transfer per UDL, O(scope
lookup + visible candidate count) suffix filtering/selection, and existing
linear-in-argument lowering; it adds no persistent transitive closure or
whole-program retry.  Regression 427 generated 64 declaration/definition
pairs (`_slot00` through `_slot63`), observed 64 unique scale ABI suffix
symbols, selected the late `_slot63`, and executed successfully.  This is
structural bounded-work evidence only; no timing or RSS claim is made.

## Focused and Broad Evidence

The final serial checks before commit are:

```text
make -C pa16 check TEST=tests/general/300-user-defined-string-literal-operator.t  PASS (1/1)
make -C pa10 check TEST=tests/general/200-literal-operator-id.t                  PASS (1/1)
sh cppgm.tests/course/pa16/427-typed-user-defined-literal-boundary-regression.sh PASS
make test-pa16                                                                      exit 2; 231/243
n=16; ... make test-report-through-pa15                                              exit 0; 1167/1167
perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src                  exit 0; 6 warnings
git diff --check                                                                    exit 0
```

## Checkpoint Ledger

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
| committed checkpoint | Typed cooked-string UDL pipeline plus 427 scale regression; focused and broad gates pass, committed in this checkpoint. |
