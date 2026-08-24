# PA12 checkpoint plan

## 1. Stage design and spec alignment

The production route remains one typed owner:

`PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 typed facts -> cold deterministic dump`

PA10 owns structured statement identities and child order. PA11 owns canonical
types, bindings, ordinary scopes, and lookup. PA12 extends that same model with
pointer-keyed `StatementFact` preparation, hidden control/substatement scopes,
condition/for-init `DeclarationFact` records, contextual `ConversionFact`
records, and typed `SemanticFact` nodes for compound, if/else, switch/case/
default, while, do, for, break, and continue.

The reviewed path preserves typed identity and cold presentation boundaries:
producer spellings are retained by PA10, no rendered text is reparsed, and no
reference or host tool participates in required output. Preparation and
semantic traversal are linear in consumed statements, declarations, and
expressions; lexical lookup follows relevant parent scopes; switch labels use
one local typed `FlatIndex` for expected-O(1) duplicate checks. No sibling or
whole-arena scan, retry-until-stable loop, or hash-order output is present.

This checkpoint repairs adjusted switch-condition promotion and case-label
conversion legality, preserves full unsigned case values for canonical
synthesized labels, compacts that payload, and omits invalid placeholders for
valid empty control substatements. The fixed Linux x86_64 promotion helper
covers bool, the narrow character/integer types, char16_t, and wchar_t to int,
and char32_t to unsigned int; fixed-underlying unscoped enums reuse it while
scoped enums remain unpromoted. A case value must be representable in the
promoted target range before duplicate comparison; no out-of-range modulo
normalization is accepted. It does not expand the independent residual PA12
call, class, pointer, namespace, parser, or broader expression families.

## 2. Failure map and coverage

The clean turn-start full-suite baseline supplied for this checkpoint is
**103/166 passing, 63 failures**, with all 166 tests covered. Its partition is:

| checked-in partition | total | passed | failed |
| --- | ---: | ---: | ---: |
| `tests/spec/100-*.t` | 12 | 12 | 0 |
| `tests/general/100-*.t` | 25 | 25 | 0 |
| `tests/spec/200-*.t` | 9 | 8 | 1 |
| `tests/general/200-*.t` | 33 | 19 | 14 |
| `tests/spec/300-*.t` | 10 | 6 | 4 |
| `tests/general/300-*.t` | 77 | 33 | 44 |
| **all PA12 tests** | **166** | **103** | **63** |

The prior checkpoint comparison recorded 13 baseline-only repairs and 0
current-only regressions against the 90/166, 76-failure start. The authorized
final rerun is unchanged from the supplied baseline: 63 unique baseline
failures, 63 unique current failures, 0 current-only paths, 0 baseline-only
paths, and **103 + 63 = 166** covered tests. The complete 166-test inventory
remains unchanged.

## 3. Active checkpoint

At `47ca58bec4e11a5defd67a7ca44db7145ba936ff` relative to `eee242c6`, the
structured-statement boundary is audited as follows:

- condition declarations bind in their control scope; for-init declarations
  bind in the for scope; unbraced branches and iteration bodies get distinct
  hidden child scopes;
- contextual bool and switch-condition conversions are recorded at the
  expression/condition fact, and switch labels use adjusted integral promotion
  types for legality and duplicate comparison;
- scoped enum labels require the same enum type, unscoped enums and integral
  labels use the supported implicit conversion path, and synthesized labels
  retain one uint64_t payload with signed/unsigned/negative metadata without
  changing ordinary literal source spelling;
- case values are checked against the exact mathematical range of the promoted
  fundamental target before canonicalization, so a negative-to-unsigned or
  other narrowing value is rejected rather than modulo-normalized;
- case/default facts, child order, lexical loop/switch validation, deterministic
  rendering, and empty control substatements are checked without invalid child
  IDs.

Focused validation after the repair:

`make -C pa12` passed, with only the pre-existing warning in the unchanged
cast path. The focused checked-in command
`make -C pa12 check TEST='tests/spec/200-if-control-flow.t tests/spec/200-do-statement.t tests/spec/200-for-loop.t tests/spec/200-switch-statement.t tests/spec/300-condition-declaration-scope.t tests/spec/300-nullptr-pointer-conversion.t tests/general/200-condition-declaration.t tests/general/200-if-substatement-sibling-declaration-scopes.t tests/general/200-loop-jumps.t tests/general/200-switch-case-declaration.t tests/general/200-switch-default-declaration.t tests/general/200-while-loop.t tests/general/300-bad-default-outside-switch.t tests/general/300-bad-scoped-enum-if-condition.t tests/general/300-break-outside-loop-bad.t tests/general/300-continue-outside-loop-bad.t tests/general/300-nonconstant-case-label-bad.t tests/general/300-qualified-using-directive-enumerator-case.t tests/general/100-string-literal-array-type.t tests/general/100-variadic-call-fixed-prefix.t tests/general/200-paren-argument-list-call.t tests/general/300-floating-literal-classification.t'` passed **22/22**.

Temporary probes outside the repository passed **17/17 expected outcomes**:
the new promotion/representability matrix was **9/9** (three valid outputs,
including full-width unsigned and same-type scoped-enum cases, plus six
expected rejections, including the former modulo case), existing
condition/for/switch scope checks were **3/3**, and empty control-substatement
checks were **5/5**. Two cold dumps of the same input were byte-identical. The
compact layout probe measured the landed `SemanticFact` at 144 bytes and the
repaired layout at 136 bytes. The known
`general/300-switch-scoped-enum-condition` ordinary-enumerator dump mismatch
remains a residual and was not expanded.

Final authorized validation passed as follows: `make test-pa12` reported
**103/166** with **63 failures** (exit 2); the exact through-PA11 command
reported **685/685**; the PA12 file audit passed with the two known
header-division warnings; and `git diff --check` passed. The five changed paths
are exactly the approved three implementation files plus `pa12/plan.md` and
`pa12/audit.md`; no tests, refs, harnesses, grammar, or scripts changed.

## 4. Performance evidence

The preserved immutable evidence belongs to the landed
`47ca58bec4e11a5defd67a7ca44db7145ba936ff` checkpoint binary and its copy; it
is not the hash of the repaired build. The landed source executable and
immutable copy were both 1,133,096 bytes with SHA-256
`d9683d357e23c632502bc9c6b43ae4f2f9fde423274d8e18d0d0a34ce713a6aa`; the
source mode was 775 and the immutable copy mode was 555.

| workload | source lines | source bytes | functions | statements | control nodes | dump lines |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 200 functions | 4,200 | 89,090 | 200 | 4,600 | 2,200 | 20,404 |
| 800 functions | 16,800 | 356,690 | 800 | 18,400 | 8,800 | 81,604 |

Seven interleaved `/usr/bin/time -f '%e'` samples were 0.09, 0.09, 0.09,
0.09, 0.09, 0.09, 0.09 seconds for 200 and 0.39, 0.39, 0.39, 0.39, 0.39,
0.40, 0.39 seconds for 800; medians were **0.09s** and **0.39s**. The
roughly 4x structural increase produced roughly 4.3x median time. The repaired
build is a separate executable from that landed immutable copy. The
source/workload shape and the smaller hot `SemanticFact` layout keep the landed
measurements representative by analysis; no new timing probe is required, and
these timings are not claimed as measurements of the repaired binary.

## 5. Next checkpoint and ledger

The structured-statement checkpoint is validated and recorded by this audit
commit. Future residual-family work requires separate authorization; this
record does not claim the PA12 stage is complete.

| row | result |
| --- | --- |
| turn-start baseline | Clean `47ca58be`; PA12 **103/166**, 63 failures, all 166 covered; through-PA11 evidence **685/685**. |
| completed validation | `make test-pa12` exit 2 with **103/166**, **63 failures**; through-PA11 **685/685**; file audit pass with exactly two known header-division warnings; `git diff --check` pass; normalized paths 63 baseline/63 current, 0 current-only, 0 baseline-only, coverage **166/166**. |
| final result / commit | This audit commit records the bounded structured-statement checkpoint. Future residual-family work requires separate authorization; PA12 remains an incomplete stage with documented residual families. |
