# PA15 typed discarded/returned-expression checkpoint

## Stage Design

The production path remains source -> PA10/PA11 typed facts -> PA12
conversion/category facts -> PA15 typed LowIR -> PA13 serialization and
validation. This checkpoint keeps ownership at the existing boundaries:
PA12 records a typed `ToVoid` conversion (including void-to-void casts),
contextualizes non-void returns, and represents scalar `return {}` as a typed
zero literal; PA15 evaluates discarded sources without materializing their
unused value, preserves comma right-hand lvalue/xvalue addresses, and prunes
only literal short-circuit RHS control flow. Function declarations are
demand-materialized only when a lowered reachable call/address or typed global
relocation consumes them. No textual reconstruction, whole-program DCE/retry,
reference binary, host compiler, or test-specific path is used.

## Failure Map

The authoritative turn-start commit is `2a10382f8abc0ff44ab7712f62c23f530441a63a`.
Its baseline is `98/109` passing, `109/109`
covered, with these exact 11 residuals. The final broad gate is `103/109`
passing, `109/109` covered: the five checkpoint residuals are removed and the
six listed final residuals are unchanged. No replacement failure appeared.

| residual | checkpoint status |
|---|---|
| `100-const-integral-lvalue-overload-category` | turn-start residual; not in checkpoint |
| `100-string-hex-escape-code-unit` | turn-start residual; not in checkpoint |
| `100-unnamed-parameter-storage` | turn-start residual; not in checkpoint |
| `200-comma-expression-xvalue-reference-return` | removed; broad PASS |
| `200-for-iteration-discards-void-comma-rhs` | removed; broad PASS |
| `200-goto-case-block-entry-label` | turn-start residual; not in checkpoint |
| `200-goto-case-block-label-after-statement` | turn-start residual; not in checkpoint |
| `200-literal-logical-short-circuit-omits-unreachable-call` | removed; broad PASS |
| `200-nested-conditional-array-decay` | turn-start residual; not in checkpoint |
| `200-return-void-call-expression` | removed; broad PASS |
| `300-return-empty-braces-scalar` | removed; broad PASS |

## Active Checkpoint

Status: complete for this checkpoint; six named residuals remain. The
focused command passed `5/5`; the adjacent expression matrix passed `7/7`;
the broad PA15 harness result is `103/109` with `109/109` coverage (its exit
status is 2 for the six known residuals):

    make -C pa15 check TEST='tests/general/200-comma-expression-xvalue-reference-return.t tests/general/200-for-iteration-discards-void-comma-rhs.t tests/general/200-literal-logical-short-circuit-omits-unreachable-call.t tests/general/200-return-void-call-expression.t tests/general/300-return-empty-braces-scalar.t'
    make -C pa15 check TEST='tests/general/200-comma-expression-lvalue-address.t tests/general/100-for-loop.t tests/general/200-direct-short-circuit-condition-branch.t tests/general/200-floating-logical-branch.t tests/general/200-scalar-assignment-address-lvalue.t tests/general/200-scalar-reference-static-cast-return.t tests/general/100-simple-call.t'

Changed implementation files are `dev/src/pa11_semantic_model.h`,
`dev/src/pa12_semantic.cpp`, `dev/src/pa12_semantic_facts.cpp`,
`dev/src/pa12_semantic_resolution.cpp`,
`dev/src/pa15_lowering.h`, `dev/src/pa15_lowering.cpp`, and
`dev/src/pa15_lowering_flow.cpp`. No checked-in test or `.ref` file changed.
The exact prior PA1-PA14 gate passed `1058/1058`. The PA15 file audit passed
with five pre-existing header-division warnings, and `git diff --check`
passed. The declaration matrix passed `4/4`; the temporary probes compiled
and validated void-to-void, bool/pointer/floating empty-brace returns, and a
typed external-function global relocation; a non-void return from void was
rejected. The global relocation emitted one external declaration, while the
defined-function address emitted none; the literal unreachable declaration
was omitted.

## Performance Evidence

Artifact: `/tmp/pa15-perf.5j71Us`. It contains `inputs.sha256` for the five
scoped sources plus a generated declaration-heavy source (32 unused and 3
demanded external declarations), `toolchain.sha256`, generated LowIR/CY86,
`structural.tsv`, `validators.tsv`, and `timings.tsv`. All six compile and
`lowir2cy86` validator statuses are zero. Structural counts are:

| input | LowIR lines | declarations | blocks | slots | calls | converts | cmps |
|---|---:|---:|---:|---:|---:|---:|---:|
| comma | 23 | 0 | 3 | 2 | 1 | 0 | 0 |
| for | 60 | 0 | 11 | 4 | 0 | 0 | 4 |
| logical | 22 | 0 | 7 | 0 | 0 | 0 | 0 |
| returnvoid | 16 | 0 | 3 | 0 | 2 | 0 | 0 |
| emptybrace | 57 | 0 | 11 | 3 | 2 | 3 | 3 |
| declheavy | 13 | 3 | 1 | 0 | 3 | 0 | 0 |

Three interleaved rounds measured 20 compilations per input at `--emit-lowir
-O0`. There were 18 samples total; all statuses were zero. Per-input wall
samples were `0.07`–`0.08` seconds per 20 and maximum RSS samples were
`7108`–`7692` KiB. This is candidate-only evidence, not a
comparative or universal performance claim. The local subtree work is linear;
literal pruning is constant work per logical node, and bounded declaration
maps/sets are `O(n log n)` without whole-program retries or rescans.

## Checkpoint Ledger

| checkpoint | result | movement |
|---|---:|---|
| turn-start `2a10382f8abc0ff44ab7712f62c23f530441a63a` (prior lineage includes `d8d92556`; d8 was not the turn-start HEAD) | `98/109`, `109/109` covered, 11 residuals | incoming authoritative baseline |
| validated checkpoint | `103/109`, `109/109` covered, 6 residuals | five scoped residuals removed; final residuals are the three `100-*`, two goto-label cases, and nested conditional array decay; no new/replacement failures |
