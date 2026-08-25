# PA15 Checkpoint Plan

PA15 lowers the supported typed scalar/function/if/direct-call slice from the
existing PA10 and PA11/PA12 owners into the shared `lowir_model::Program`.
Each command-line input remains an independent translation unit and semantic
owner; the driver appends each independently lowered result to one output
program. This checkpoint does not claim full PA15.

## Spec alignment

- Spec §1: one forward typed production path is preserved per input:
  preprocessing, PA10, PA11/PA12, `Pa15Lowerer`, shared LowIR model, and cold
  serialization.
- Spec §2: linkage, storage, type, value, slot, block, terminator, and callee
  identities remain typed facts. No rendered AST, semantic dump, LowIR text,
  fixture, or reference output is consumed as semantic identity.
- Spec §4: the repair adds no cross-TU retry/coordinator or repeated subtree
  merge. Existing per-input lowering and shared-program accumulation remain
  bounded by their owned model sizes; no new superlinear merge claim is made.
- Spec §5: the existing LowIR model remains authoritative for typed symbols,
  metadata, operands, values, slots, blocks, operators, and types.
- Spec §7: final broad status, through-PA14 coverage, audit, focused raw
  regression, independent-input probe, and reconstructed scaling evidence are
  recorded separately below.

## Current checkpoint

PA11 `Binding` owns decoded `LanguageLinkage` and namespace-scope
`internal_linkage`. `extern "C"` is decoded once from PA10 literal data.
`Pa15Lowerer` emits `linkage=c` for external C functions, `binding=internal`
for internal functions, and uses the PA14 ABI encoder for object symbols;
internal C functions intentionally retain C++ local ABI spelling. The driver
serializes only after all per-input lowering and entry validation have
completed, and computes the serialized string before opening the output file.
Write/flush failures after opening may still leave partial output.

Same-context repeated declarations/definitions within one input retain
compatible linkage facts. Declaration-introduced linkage inheritance outside
the active linkage specification is not implemented and is a next-checkpoint
nonclaim. Cross-input declaration/name/type coordination is likewise not
claimed. Independent same-named static inputs remain distinct and resolve
their own calls.

## Exact final failure map

The final `make test-pa15` result is `13/109` passing, `96` failing, exit `2`.
All 109 local cases ran; the added raw regression is a direct public driver
script and does not replace or alter the local fixture suite.

| Category | Failures | Boundary |
|---|---:|---|
| Control flow, loops, switch, goto, condition declarations, short circuit | 17 | Only basic if/else is in this checkpoint |
| Globals, arrays, pointers, address/decay, subscripting | 40 | Not implemented |
| Calls, ABI/reference parameters, overloads, linkage declarations, using/namespace lookup | 18 | Direct resolved scalar calls only; cross-input coordination is not claimed |
| Enums, `sizeof`, floating cases, character literals, conversions | 13 | Supported scalar conversion subset only |
| Extended lvalues, compound assignment, inc/dec, generated slots, other scalar extensions | 8 | Not implemented |
| **Total** | **96** | **No coverage reduction; no added failure** |

## Final validation

- `make -C pa15 -j2`: passed.
- `cppgm.tests/course/pa15/400-raw-lowir-linkage-regression.sh`: passed.
- Existing raw/normalized extern-C/internal focus: passed `1/1`.
- Focused scalar/linkage public set: passed `10/10`.
- Focused PA11 namespace and PA12 namespace-call checks: passed `1/1` each.
- Independent-static multi-input probe passed with distinct `@helper` and
  `@helper__2` symbols; cross-input declaration/definition failed with the
  expected unsupported direct-call-target diagnostic.
- `make test-pa15`: `13/109`, `96` failures, exit `2`.
- `n=15` through-PA14 report: `1058/1058` passed.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src`: passed with
  four existing header-division warnings.
- `git diff --check`: passed.

Earlier valid evidence is retained: the prior model/scalar probes, serializer
round-trip plus `lowir2cy86` check, and prior through-PA14 result remain
consistent with the final focused results. No fixture, `.ref`, or harness
change was made.

## Performance evidence

The historical scaling script was unavailable, so a matching scalar workload
was reconstructed outside the repository: N two-parameter functions, one
local per function, a direct-call chain, and `main`; the current executable
was not rebuilt during measurement. Runs were interleaved in order
`[8,32,16,16,8,32,32,16,8]`, with three observations per size. The generated
source differs from the historical byte counts, so this is final equivalent
workload evidence rather than a byte-for-byte rerun.

| N | median ms | input B | output B | functions | slots | calls | value instructions |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 8 | 12.094 | 551 | 3121 | 9 | 24 | 8 | 39 |
| 16 | 12.845 | 1083 | 6091 | 17 | 48 | 16 | 79 |
| 32 | 14.569 | 2155 | 12043 | 33 | 96 | 32 | 159 |

Artifacts were kept outside the repository at `/tmp/pa15-scale-final-ns.tsv`
and `/tmp/pa15-scale-final-ns.hEzUc1`. The independent-input design adds no
new AST merge path; remaining complexity claims are limited to the existing
per-owner lowering and cold serialization passes.

## Next checkpoint

The next bounded work should establish a correct cross-translation-unit
declaration/name/type view and linkage-inheritance owner before extending
other PA15 groups. Globals, arrays, pointers, references, indirect calls,
loops, switch, short circuit, enums, floats, and extended lvalues remain
separate nonclaims.

## Checkpoint ledger

| Status | Completed work |
|---|---|
| Complete | Preserved independent-input lowering, repaired typed linkage/storage ownership and LowIR metadata, added raw public coverage, corrected serialization ordering, ran all required gates, and documented exact remaining failures and performance evidence. |
