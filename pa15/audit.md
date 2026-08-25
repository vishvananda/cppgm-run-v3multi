# PA15 Audit

## Current Checkpoint Review

This review covers the landed PA15 typed scalar lowering increment at
`f77219af1b8aa5dd5e9d2441b580825c593de62b` and its bounded final repair. The
driver retains one independent PA10/PA11/PA12 semantic owner and lowerer per
input file, appending each typed result to the shared `lowir_model::Program`.
There is no cross-translation-unit AST merger or shared semantic coordinator.

PA11 `Binding` owns decoded language linkage and namespace-scope internal
function storage. `extern "C"` is read from PA10's decoded literal fact, not
from a rendered dump. The lowerer maps external C linkage to LowIR
`linkage=c` and the PA14 C object spelling; internal C functions retain the
PA14 C++ local object spelling while LowIR records `binding=internal`.
Function headers, parameters, local slots, `BindingId`-to-`SymbolId` call
targets, `ValueId` producers/owners, if/else blocks, and terminators remain
typed and deterministic. The cold serializer is the only text boundary.

The existing extern-C/internal fixture was a real raw-output defect even
though normalized comparison passed. The public regression
`cppgm.tests/course/pa15/400-raw-lowir-linkage-regression.sh` directly checks
the raw headers for C linkage, internal binding, and both PA14 object symbols,
and rejects the old strong/no-linkage headers. No handout fixture, `.ref`, or
harness was changed.

The driver computes the complete serialized LowIR string before opening the
output path. If opening fails, no output is created; after opening, a write or
flush failure may still leave partial output. This is not an atomic-write
claim.

Within one translation unit, declarations and definitions under the same
linkage context retain compatible typed linkage and merge normally. Linkage
inheritance from a declaration into a later declaration/definition outside
that context is not implemented by the current PA11 contract; it is a
next-checkpoint declaration/linkage nonclaim. Focused PA11/PA12 dump behavior
remains passing.

Separate input files are intentionally not treated as one source program.
Two independent inputs containing same-named static functions lower to
distinct LowIR symbols and each input's call resolves within its own semantic
owner. Cross-input declaration/definition and namespace-view coordination is
not claimed and remains the next checkpoint. No claim is made for globals,
arrays, pointers, references, indirect calls, loops, switch, short circuit,
enums, floats, or the other remaining PA15 groups.

## Final Evidence

- `make test-pa15`: all 109 cases ran; `13 / 109` passed, `96` failed, exit
  `2`, with no coverage reduction or added failure.
- The required through-PA14 report passed `1058 / 1058`.
- The raw public regression passed; the existing normalized extern-C/internal
  check passed `1/1`; the focused scalar/linkage set passed `10/10`.
- Independent-static input probing passed with `@helper` and `@helper__2`,
  and the caller targeted `@helper__2`. A cross-input declaration/definition
  probe failed with `PA15 direct call target was not emitted`, as the stated
  nonclaim.
- Focused PA11 namespace and PA12 namespace-call checks passed `1/1` each.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` passed with
  the four existing header-division warnings. `git diff --check` passed.

## Audit Ledger

| Checkpoint | Evidence and disposition |
|---|---|
| PA15 full-stage / checkpointAudit — typed scalar linkage/storage ownership | Preserved independent-input lowering, repaired canonical PA11 linkage/storage facts and LowIR mapping, added the raw public regression, corrected output-open behavior and documentation, and retained all remaining feature groups as explicit nonclaims. |
