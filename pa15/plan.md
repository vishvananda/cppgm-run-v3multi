# PA15 typed discarded/returned-expression checkpoint

## Spec alignment and stage design

The production path remains source -> PA10/PA11 typed facts -> PA12
conversion/category facts -> PA15 typed LowIR -> PA13 serialization and
validation. The landed increment keeps ownership at those boundaries:

- PA12 records typed `ToVoid` conversions (including void-to-void casts),
  contextualizes non-void returns, and represents supported scalar `return {}`
  as a typed zero literal.
- PA15 evaluates discarded sources without materializing an unused value,
  preserves comma right-hand lvalue/xvalue addresses, lowers void returns and
  typed return conversions, and uses a cached literal-only truth proof for
  short-circuit pruning. For-init and iteration discards use the same discard
  owner as expression statements. A typed scalar volatile lvalue is the
  deliberate exception to value elision: its discarded access emits one
  LowIR load, while a nonvolatile reference identifier remains address-only.
- PA15 demand-materializes declarations only for lowered reachable calls,
  addresses/references, and typed global relocations, in deterministic order.

The boundary is typed end to end: no textual reconstruction, broad DCE,
whole-program retry, reference binary, host compiler, or test-specific output
path is used. The truth cache is per semantic-fact and fails closed for
unknown or malformed facts, keeping recursive nested-logical proof bounded.

## Exact failure map

The parent/turn-start baseline is
`2a10382f8abc0ff44ab7712f62c23f530441a63a`: `98/109` passing, `109/109`
covered, with exactly these 11 residuals. The landed increment at
`98e75ff9dcf02fe6bd0646330dbc121dfc3c6193` made the 11 -> 6 movement before
this audit. This audit began with the six residuals shown below and its final
broad gates preserve that six-name set: `make test-pa15` is `103/109`,
`109/109` covered, with zero added/replacement names and zero missing names.

| residual | checkpoint status |
|---|---|
| `100-const-integral-lvalue-overload-category` | unchanged; out of scope |
| `100-string-hex-escape-code-unit` | unchanged; out of scope |
| `100-unnamed-parameter-storage` | unchanged; out of scope |
| `200-comma-expression-xvalue-reference-return` | removed by landed increment; focused PASS |
| `200-for-iteration-discards-void-comma-rhs` | removed by landed increment; focused PASS |
| `200-goto-case-block-entry-label` | unchanged; out of scope |
| `200-goto-case-block-label-after-statement` | unchanged; out of scope |
| `200-literal-logical-short-circuit-omits-unreachable-call` | removed by landed increment plus bounded value-context repair; focused PASS |
| `200-nested-conditional-array-decay` | unchanged; out of scope |
| `200-return-void-call-expression` | removed by landed increment; focused PASS |
| `300-return-empty-braces-scalar` | removed by landed increment; focused PASS |

No fixture or unrelated residual surface was edited.

## Current checkpoint and focused evidence

Checkpoint under review: landed source at `98e75ff9dcf02fe6bd0646330dbc121dfc3c6193`,
parent `2a10382f8abc0ff44ab7712f62c23f530441a63a`, plus the final bounded
audit repair. The focused five-test matrix passed `5/5` in
`/tmp/pa15-final-focused-5.log`; the representative adjacent matrix passed
`7/7`:

    make -C pa15 check TEST='tests/general/200-comma-expression-xvalue-reference-return.t tests/general/200-for-iteration-discards-void-comma-rhs.t tests/general/200-literal-logical-short-circuit-omits-unreachable-call.t tests/general/200-return-void-call-expression.t tests/general/300-return-empty-braces-scalar.t'
    make -C pa15 check TEST='tests/general/200-comma-expression-lvalue-address.t tests/general/100-for-loop.t tests/general/200-direct-short-circuit-condition-branch.t tests/general/200-floating-logical-branch.t tests/general/200-scalar-assignment-address-lvalue.t tests/general/200-scalar-reference-static-cast-return.t tests/general/100-simple-call.t'

The narrowly named owner regression
`cppgm.tests/course/pa15/405-typed-discarded-return-regression.{source,sh}`
passed again (exit 0; `/tmp/pa15-final-405-regression.log`). It exercises
void-to-void cast chains without a scalar load, for-init
discard of a comma lvalue without an extra load, nested/value-context literal
short circuit without an unreachable call/address declaration, evaluated
runtime call/address demand, direct volatile-object and reference-to-volatile
discard loads, and typed empty-brace bool/integral/f32/f64/f80/pointer return
payloads. Owner probes 400--404 also passed. The five removed tests plus 405
compiled and passed `dev/lowir2cy86`; the semantic probe confirmed PA12's
typed volatile facts, void return facts, and nested cast/logical trees. The
exact prior gate exited 0 at `1058/1058` (`/tmp/pa15-final-checkpoint-through-pa14.log`).
The final root through-PA15 gate exited 2 at `1161/1167`, with the same six
PA15 residuals and all earlier stages passing (`/tmp/pa15-final-checkpoint-through-pa15.log`).
The file audit exits 0 with five known header-division warnings, and
`git diff --check` exits 0; the bounded source move keeps
`dev/src/pa15_lowering.cpp` below the 3000-line limit.

The exact PA15 gate log is `/tmp/pa15-final-checkpoint-test-pa15.log`; the
mechanical name and coverage proof is `/tmp/pa15-final-failure-set-proof.log`.

## Performance evidence

Fresh bounded artifact:
`/tmp/pa15-checkpoint-audit-discarded-final.fRe1n0`.
Its immutable candidate is mode `0555`, SHA-256
`f03860f091c4a646af8937a7e9023e79facd1a6aebb4c885d408d1a81f16d95e`;
`candidate.sha256`, `implementation.sha256`, `inputs.sha256`,
`toolchain.sha256`, `lowir.sha256`, and all validation/measurement files were
verified. Six affected-path inputs (the five removed tests plus 405) used five
interleaved forward/reverse rounds and 20 compilations per sample at
`--emit-lowir -O0`; all compile and LowIR-validator statuses were zero.

| input | LowIR lines | declarations | blocks | slots | calls | converts | cmps | median wall/call | median RSS KiB |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| comma | 23 | 0 | 3 | 2 | 1 | 0 | 0 | 0.0030 s | 5140 |
| for | 60 | 0 | 11 | 4 | 0 | 0 | 4 | 0.0030 s | 5124 |
| logical | 22 | 0 | 7 | 0 | 0 | 0 | 0 | 0.0030 s | 5124 |
| returnvoid | 16 | 0 | 3 | 0 | 2 | 0 | 0 | 0.0025 s | 5364 |
| emptybrace | 57 | 0 | 11 | 3 | 2 | 3 | 3 | 0.0030 s | 5372 |
| owner405 | 169 | 2 | 29 | 6 | 1 | 4 | 4 | 0.0040 s | 5384 |

This is candidate-only, bounded affected-path evidence; it is not a
comparative or universal performance claim.

## Next checkpoint

The next checkpoint is the unchanged residual surface set:
`100-const-integral-lvalue-overload-category`,
`100-string-hex-escape-code-unit`, `100-unnamed-parameter-storage`,
`200-goto-case-block-entry-label`, `200-goto-case-block-label-after-statement`,
and `200-nested-conditional-array-decay`. Those surfaces remain outside this
typed discard/return ownership audit.

## Completed row

| checkpoint | result | movement |
|---|---:|---|
| `98e75ff9dcf02fe6bd0646330dbc121dfc3c6193` + final bounded audit repair | focused `5/5` + adjacent `7/7` + owner regression and LowIR validation pass; `make test-pa15` `103/109`, exact six residuals, `109/109` covered; through-PA14 `1058/1058`; through-PA15 `1161/1167` with the same six | landed increment accounts for 11 -> 6; this audit preserves the turn-start six while fixing for-init/comma discard ownership, recursively bounded literal value-context short-circuit pruning, and typed volatile-scalar discard loads; one earliest-owner regression added; required validation and checkpoint commit complete |
