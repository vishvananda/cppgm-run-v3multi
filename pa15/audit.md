# PA15 final architecture audit

## Current state

The implementation checkpoint under review is
`fc9f7ffbd6604f579a7734390ef822d786085794` (`PA15 complete typed boundary
checkpoint`). It completed the three formerly reported PA15 residuals:

| surface | first owner to terminal | checked result at fc9f baseline |
|---|---|---|
| const-integral lvalue overload category | PA10 cast/syntax -> PA11 canonical deleted definition -> PA12 ranking and selected-deleted rejection -> PA15 typed call boundary -> PA13 LowIR | pass |
| decoded string code unit | PA10 `LiteralData` -> PA12 typed bytes/constant address -> PA15 cached global/index/load and typed conversion -> PA13 LowIR | pass |
| unnamed operator-delete parameters | PA10 operator enum/token -> PA11 sidecar/declaration state -> PA12 definition -> PA15 PA14 ABI terminal/ordinal slots/unwind -> PA13 LowIR | pass |

The supplied clean baseline reported `make test-report-through-pa15` at
`1167/1167` and the PA15 file audit at exit `0`. Those are preserved as
pre-audit evidence. The final required validation run also passed
`1167/1167` across `pa1` through `pa15`; exact results are recorded below.
The final file list is exactly the source repair below, the durable PA13 shell
regression, and the two consolidated records.

The files are `dev/src/lowir2cy86_backend.cpp`,
`cppgm.tests/course/pa13/400-lowir2cy86-typed-boundary-regression.sh`,
`pa15/plan.md`, and `pa15/audit.md`. The shell regression writes only under a
cleaned `mktemp` directory; no PA13 `.t`/`.ref` fixture or handout was changed.

## Architecture conclusion

The production ownership graph is a single forward pipeline:

```text
PA10 typed syntax
  -> PA11 canonical binding/type and sidecar facts
  -> PA12 selected declarations, conversions, categories, labels, literals
  -> PA15 typed values, CFG, ABI, addresses, and LowIR instructions
  -> PA13 typed LowIR serialization, validation, and backend boundary
```

PA10's operator and cast handling is enum/token based. C-style casts use a
bounded local parenthesized-token scan and then the typed type parser; an
ambiguous parenthesized expression remains an expression. Decoded string
code units are retained in `LiteralData`, not recovered from rendered text.

PA11's `BindingSidecar` carries operator kind/token, bare `noexcept`, and
`Normal`/`Defaulted`/`Deleted` declaration state. Canonical `Binding` records
definition and redeclaration state. The fixed operator `NameId` vocabulary is
only a one-way adapter for presentation/lookup; PA15 consumes the typed
sidecar. Compatible declarations merge through the canonical owner, while
selected deleted functions are rejected by PA12 after overload ranking.

PA12 owns conditional common type/category, fact-local conversions, array
decay/reference binding, label resolution, decoded literal bytes, and
`ConstantAddressFact`. It publishes these once in typed semantic facts. PA15
does not parse source or LowIR text to rediscover them.

PA15's operator path calls the centralized PA14 ABI terminal and carries
nonthrowing as `unwind=no`. Unnamed parameters receive deterministic
function-local ordinals and slots. Literal arrays use one typed constant
address relation, one cached internal global, and the ordinary typed
`addr`/`index`/`load` path. Array conversions are read from the fact-local
range and applied once. Label lowering uses dense function-local `LabelId`
and `BlockId` identities, sparse fact-indexed flow arenas, canonical
continuations, persistent typed loop/switch context, and generation-stamped
scratch state.

PA13's serializer presents the already typed model at the LowIR boundary.
The validator and backend consume typed owners and payloads. The source set
contains `pa15_operator_abi.cpp` in
`FRONTEND_OBJ_BASENAMES_cppgm++`; there is no parallel PA15 producer.

## Findings and repairs in the final change set

The audit found a complete-path issue that the PA15 text-comparison harness
does not exercise: several valid PA15 `.check` files were rejected when fed
through the current PA13 validator. The repair is deliberately limited to
`dev/src/lowir2cy86_backend.cpp`:

1. `IK_COPY` now allows a named integer source and destination with equal
   `integer_width()` but different signedness. This models PA15's
   representation-preserving signedness retag. It does not relax pointer,
   float, object, or unequal-width checks.
2. `validate_global` now accepts `INIT_ZERO` for scalar and pointer globals.
   The backend already emits the corresponding zero storage form.
3. `IK_BINARY` now recognizes only pointer-pointer subtraction as PA15's
   pointer-difference form and records its result as `i64`; pointer addition
   and other pointer binary operators remain invalid.

The durable PA13 shell regression is part of the final change set. It creates all
LowIR inputs under `mktemp`, invokes the built `dev/lowir2cy86` by default,
and removes its generated files on exit. No `.ref` fixture, handout, or
source-set file was changed; no reference binary, previous solution, host
compiler, shell-out, or alternate production pipeline was used.

## Representative evidence

Focused source tests and direct typed-boundary checks on the current tree:

- `make -C dev lowir2cy86` -> pass.
- PA15 declaration/literal/array matrix -> `PASS (7/7)`.
- PA10 bounded-cast matrix -> `PASS (4/4)`.
- Direct `lowir2cy86` validation of the seven representative PA15 outputs ->
  `PASS (7/7)`; the unnamed-operator fragment was combined with a synthetic
  typed entry function because the standalone fragment intentionally has no
  entry point.
- Direct validation of two conditional-array outputs and two label/goto
  outputs -> `PASS (4/4)`.
- Boundary probes accepted equal-width `i8`/`u8` copy, pointer zero global,
  and pointer difference; unequal-width copy and pointer addition were
  rejected.
- `./cppgm.tests/course/pa13/400-lowir2cy86-typed-boundary-regression.sh`
  exited `0`: positive exit `0`; unequal-width named integer copy, `void`
  zero global, and pointer addition each exited `1`. This direct focused
  regression is durable course coverage, separate from the root gate.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited `0`
  with five existing `bad-division` warnings: `abi_mangle.h`,
  `cpp_semantic_core.h`, `lowir_model.h`, `pa11_semantic_model.h`, and
  `pa15_lowering.h`.
- `make test-report-through-pa15` exited `0`; the actual output enumerated
  `pa1` through `pa15` (15 stages) and ended
  `ALL TESTS PASSED SUCCESSFULLY! (1167 / 1167)`.
- The stage-boundary counts represented by that run are PA13 `96/96` and
  through-PA13 `947/947`, PA14 `111/111` and through-PA14 `1058/1058`, and
  PA15 `109/109`, yielding through-PA15 `1167/1167` across `15/15` stages.
- Final `git diff --check` after this documentation update exited `0`.

Representative serialized forms observed in the checks include:

- `function @operatordelete(... %__param0 ..., %__param1 ...) -> void
  [unwind=no, ...]` with `$__param0` and `$__param1` slots;
- one `global @__strlit__1` containing decoded byte `171` and its
  `addr`/`index`/`load` sequence;
- `unary decay ptr` for typed array decay;
- `binary sub ptr` followed by `binary div i64` for pointer distance;
- typed label targets and branch/jump `BlockId` serialization.

## Spec conformance review

The review against `spec.md` and `pa15/README.md` confirms:

- one source-to-typed-LowIR production path and one typed LowIR model;
- typed enum/operator/declaration/type/value/category/label/ABI/address
  continuity, with presentation strings restricted to true boundaries;
- fact-local `(begin,count)` conversion/child ranges, sparse flow arenas,
  deduplicated typed worklists, and generation guards;
- deterministic identity allocation and serialization order;
- no source reparsing, ABI reconstruction from text, duplicate semantic or
  IR models, fixture-specific branching, retry-until-stable pass, broad
  invalidation, stale scratch state, unbounded recovery, or production
  compiler shell-out;
- a central PA14 ABI terminal and direct PA13 typed serialization/validation.

The code review found no additional architecture defect in the four required
ownership paths. The final root full-stage gate and file audit passed after
the focused validator repair; commit and clean-status proof are recorded by
the final handoff.

## Measurement and limits

The prior measurement directories named by the stale plan were absent, so
their numbers are historical/unverifiable in this workspace. Fresh evidence
was collected from immutable mode-0555 candidates in
`/tmp/pa15-final-measure.Wl9uud`, using five interleaved rounds and
`timeout 60s` for both compilation and validation. Candidate hashes are:

```text
cppgm++       10de97087b997429732fded8424fdc47742680b8f19cc22f9042f54366eee4fe
lowir2cy86    7f5d1d67d55289f8cebc39022410ceb57093287ad66c35d950fa136814c85d85
```

The recipe has an ordinary operator-delete definition, a deleted overload
followed by a compatible redeclaration, `N` cast/literal-subscript function
bodies, and one entry function. The measured medians are:

| N | input lines/bytes | LowIR lines/bytes | globals/functions/instructions/slots | compile wall/user/sys/RSS | validate wall/user/sys/RSS | stable LowIR hash |
|---:|---:|---:|---:|---|---|---|
| 32 | 132 / 3,629 | 879 / 21,135 | 32 / 34 / 516 / 66 | 0.10 / 0.00 / 0.00 / 6,948 KiB | 0.10 / 0.00 / 0.00 / 7,032 KiB | `7ca749cf309749486335207dbdf2decccd2292c62bd089435f39bed65716a867` |
| 256 | 1,028 / 27,977 | 6,927 / 167,361 | 256 / 258 / 4,100 / 514 | 0.10 / 0.02 / 0.01 / 14,272 KiB | 0.10 / 0.02 / 0.01 / 8,260 KiB | `5489bc8a911bcfe15c95ffba50ac0591645ebeb1adb2900f3b20c19c9aef7829` |

The address/index/load/copy/convert counts were respectively
`32/32/128/32/128` and `256/256/1024/256/1024`. All ten LowIR outputs were
accepted by the immutable validator. The exact generated counters and raw
time/RSS files are retained in the artifact directory. These measurements
support bounded deterministic growth for this recipe only; they do not prove
an asymptotic bound, generated native-code quality, or allocation behavior.

## Historical ledger and current disposition

The following rows summarize the reviewed PA15 stage sequence without
presenting old checkpoint results as current:

| checkpoint group | durable result | disposition |
|---|---|---|
| `f77219af` -> `c2917518` | typed scalar, linkage, and structured-control foundations | historical |
| `3a7267fa` -> `eba262f5` | typed address/value, relocation, and null initializer ownership | historical |
| `3bf82dbe` -> `ca3c38ca` | enum scalar, evaluator, and relational fact ownership | historical |
| `fbc3cce7` -> `2a10382f` | callable/reference, reinterpret, and floating conversion ownership | historical |
| `98e75ff9` -> `959f9481` | discarded/returned-expression and demand-root ownership | historical |
| `a2f33047` -> `f038141d` | typed label CFG, sparse flow, continuations, and generation isolation | historical |
| `b7eaf9d8` -> `83d60e96` | conditional-array category/conversion ownership; old audit's `106/109` is superseded | historical |
| `fc9f7ffb` plus final change set | current final audit state: the implementation checkpoint reported `109/109` PA15 tests and final validation passed `1167/1167` across `pa1` through `pa15`, with five existing file-audit warnings, three validator repairs, the durable PA13 shell regression, and truthful plan/audit consolidation | current |

The current row is the only final-state row. All required validation commands
are green; the final handoff records the resulting commit and clean status.
