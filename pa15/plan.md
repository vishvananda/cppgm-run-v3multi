# PA15 final typed-boundary audit

## Status and scope

This record covers the complete PA15 implementation checkpoint at
`fc9f7ffbd6604f579a7734390ef822d786085794` (`PA15 complete typed boundary
checkpoint`) and the approved audit/repair change set on top of that commit.
The supplied clean baseline was `1167/1167` through PA15. The final required
validation run also passed `1167/1167` across the fifteen stages; the exact
commands and warnings are recorded below.

The final source-to-LowIR contract is:

```text
source -> PA10 typed syntax -> PA11 canonical bindings/types
       -> PA12 semantic facts/conversions -> PA15 typed LowIR
       -> PA13 serialization and validation
```

There is one production producer for this path. Text is used only at source,
diagnostic/dump, external-name, and LowIR serialization/input boundaries.

## Final change-set file list

The final change set contains exactly:

- `dev/src/lowir2cy86_backend.cpp` — three narrow PA13 validator repairs;
- `cppgm.tests/course/pa13/400-lowir2cy86-typed-boundary-regression.sh` — a
  durable direct validator regression with temporary inputs and outputs;
- `pa15/plan.md` and `pa15/audit.md` — the consolidated final-stage records.

No PA13 `.t`/`.ref` fixture or PA15 handout file was changed; the regression
cleans its `mktemp` directory and commits no generated output.

## Ownership trace

| first typed owner | semantic owner | PA15/PA13 terminal evidence |
|---|---|---|
| PA10 operator enum/token and bounded C-style-cast scan | PA11 `Binding`/`BindingSidecar` declaration kind, operator identity, nonthrowing state, and definition/redeclaration state; PA12 overload selection and deleted rejection | PA15 maps the typed sidecar through the centralized PA14 ABI terminal, emits `unwind=no` and ordinal parameter names, and PA13 validates the serialized function; no ABI reconstruction from rendered names |
| PA10 decoded `LiteralData` and array `TypeId`/value category | PA12 owns decoded bytes, `ConstantAddressFact`, common conditional type/category, and fact-local array/reference conversions | PA15 publishes one cached literal global/address/index/load path and applies each conversion once; PA13 validates typed decay and loads |
| PA10 label/goto spelling | PA12 owns `NameId`, dense function-local `LabelId`/`LabelTableId`, duplicate checks, and resolved semantic facts | PA15 lowers typed `BlockId` CFG edges with sparse fact-indexed flow arenas, canonical continuations, persistent control context, and generation-stamped scratch; PA13 validates the resulting blocks |

The reviewed paths preserve one typed value through lowering. No source
reparsing, rendered-name ABI reconstruction, parallel semantic/IR model,
fixture-specific branch, reference/host compiler shell-out,
retry-until-stable loop, broad invalidation, stale generation state, or
unbounded recovery was found.

## Audit repairs in the final change set

The focused cross-stage audit found three PA13 validator omissions exposed by
valid PA15 output. They are repaired in `dev/src/lowir2cy86_backend.cpp`:

1. `copy` accepts a named integer source with the same storage width when the
   signedness spelling changes. PA15 uses this bit-preserving typed retag for
   conversions such as `i8` to `u8`; different widths and non-integer types
   remain rejected.
2. A scalar `global : <type> = zero`, including `ptr`, is accepted as the
   typed zero initializer required by the LowIR contract.
3. PA15's `binary sub ptr` pointer-difference form validates its two pointer
   operands and publishes an `i64` result consumed by the following element
   distance arithmetic. Other pointer binary operators remain rejected.

`dev/frontend_source_sets.mk` already registers
`dev/src/pa15_operator_abi.cpp` in the `cppgm++` source set; no new source
file was added. The durable PA13 shell regression is listed in the final
change-set file list above.

## Focused evidence

- `make -C dev lowir2cy86` passed after the validator repair.
- The PA15 residual/adjacent source matrix passed `7/7`; the PA10 cast matrix
  passed `4/4`.
- The direct PA13 validator matrix passed `7/7`, including the standalone
  unnamed-parameter function combined with a synthetic entry function.
  The conditional-array and label validator matrix passed `4/4`.
- Positive and negative boundary probes passed: equal-width `i8` to `u8`
  copy accepted; `i8` to `u16` copy rejected; `global ptr = zero` and
  pointer-difference LowIR accepted; pointer addition remained rejected.
- `./cppgm.tests/course/pa13/400-lowir2cy86-typed-boundary-regression.sh`
  exited `0`: its one positive LowIR input exited `0`, while unequal-width
  copy, `void` zero global, and pointer addition exited `1`. This is a direct
  PA13 validator regression, separate from the root through-stage gate.
- `perl scripts/cppgm_file_audit.pl --stage pa15 --paths dev/src` exited `0`.
  It reported five existing `bad-division` warnings for
  `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
  `pa11_semantic_model.h`, and `pa15_lowering.h`.
- `make test-report-through-pa15` exited `0`. Its output enumerated `pa1`
  through `pa15` (15 stages) and ended `ALL TESTS PASSED SUCCESSFULLY!
  (1167 / 1167)`.
- The stage-boundary counts represented by that run are PA13 `96/96` and
  through-PA13 `947/947`, PA14 `111/111` and through-PA14 `1058/1058`, and
  PA15 `109/109`, yielding through-PA15 `1167/1167` across `15/15` stages.
- The final `git diff --check` exited `0` after the documentation update.

## Bounded measurement

The previous `/tmp` artifacts named by the older record were unavailable, so
the old performance numbers are not treated as current proof. A fresh
immutable-candidate sample is in
`/tmp/pa15-final-measure.Wl9uud`. The candidate and validator were mode `0555`
with SHA-256 values:

- `cppgm++`: `10de97087b997429732fded8424fdc47742680b8f19cc22f9042f54366eee4fe`
- `lowir2cy86`: `7f5d1d67d55289f8cebc39022410ceb57093287ad66c35d950fa136814c85d85`

Each generated input contains an ordinary and a deleted-compatible operator
declaration path, `N` functions with a bounded `(const unsigned long)` cast
and decoded `"\\xab"` byte subscript, and one entry function. Five rounds
alternated the 32 and 256 inputs. Every compile and validator invocation used
`timeout 60s`; all outputs validated and each size produced one LowIR hash.

| N | input lines/bytes | LowIR lines/bytes | globals/functions/instructions/slots | addr/index/load/copy/convert | compile median wall/user/sys/RSS | validate median wall/user/sys/RSS | LowIR SHA-256 |
|---:|---:|---:|---:|---:|---|---|---|
| 32 | 132 / 3,629 | 879 / 21,135 | 32 / 34 / 516 / 66 | 32 / 32 / 128 / 32 / 128 | 0.10 / 0.00 / 0.00 / 6,948 KiB | 0.10 / 0.00 / 0.00 / 7,032 KiB | `7ca749cf309749486335207dbdf2decccd2292c62bd089435f39bed65716a867` |
| 256 | 1,028 / 27,977 | 6,927 / 167,361 | 256 / 258 / 4,100 / 514 | 256 / 256 / 1,024 / 256 / 1,024 | 0.10 / 0.02 / 0.01 / 14,272 KiB | 0.10 / 0.02 / 0.01 / 8,260 KiB | `5489bc8a911bcfe15c95ffba50ac0591645ebeb1adb2900f3b20c19c9aef7829` |

These are bounded structural and timing observations for this recipe, not
an asymptotic or machine-independent claim. No unexplained timing jump was
observed; the structural counters and stable hashes corroborate the measured
growth. Phase/allocation instrumentation is not exposed by the current
frontend executable, so no stronger allocation claim is made.

## Final ledger

The earlier PA15 commits were reviewed in order as the scalar/linkage,
structured-control, typed address/relocation/null, enum/evaluator/relational,
callable/reinterpret/floating, discard/return, label/CFG, and conditional-array
ownership increments. The durable stage sequence is:

| checkpoint | disposition |
|---|---|
| `f77219af` through `c2917518` | historical typed scalar/linkage and structured-control foundations |
| `3a7267fa` through `eba262f5` | historical typed address, relocation, and null ownership foundations |
| `3bf82dbe` through `ca3c38ca` | historical enum, evaluator, and relational typed facts |
| `fbc3cce7` through `2a10382f` | historical callable/reference, reinterpret, and floating conversion paths |
| `98e75ff9` through `959f9481` | historical discarded/returned-expression ownership |
| `a2f33047` through `f038141d` | historical typed label CFG, sparse flow, continuation, and generation repair |
| `b7eaf9d8` and `83d60e96` | historical conditional-array checkpoint; prior audit reported `106/109` and is not current |
| `fc9f7ffb` plus final change set | current final audit state: the implementation checkpoint reported `109/109` PA15 tests, and final validation passed `1167/1167` across `pa1` through `pa15`, with five existing file-audit warnings, three validator repairs, the durable PA13 shell regression, and truthful plan/audit consolidation |

The prior `b7eaf9d8`/`106/109` row is retained only as history. The current
audited state is the fc9f implementation plus the explicitly listed final
validator repair, durable regression, and records; no stale residual map is
presented as final.
