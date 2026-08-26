# PA15 typed declaration and literal-address checkpoint

## Stage Design

The production path is source -> PA10 typed syntax -> PA11 canonical
bindings/types -> PA12 semantic facts and conversions -> PA15 typed LowIR ->
PA13 serialization/validation.  Each residual fact is retained and consumed
at its owning boundary:

- PA10 keeps the operator enum/token on `OperatorFunction` identifiers and
  routes C-style casts by scanning only the local parenthesized token range;
  `parse_type_id` remains the validity check.  String code units continue to
  come only from decoded `LiteralData`.
- PA11 stores operator kind/token, bare `noexcept`, and the
  `Normal`/`Defaulted`/`Deleted` `FunctionDeclarationKind` in the binding
  sidecar.  Special forms are inserted as definitions, so a deleted or
  supported defaulted form must be first and cannot be repeated; an ordinary
  compatible redeclaration preserves the canonical special state.  A
  fixed-vocabulary `NameId` string is only a one-way lookup/presentation
  adapter derived from the PA10 enum/token, never a canonical semantic key.
- PA12 treats special function initializers as declaration properties, lets a
  deleted candidate participate in overload ranking, then rejects a call that
  selects it.  Array-to-pointer semantic context publishes the typed literal
  constant-address relation.
- PA15 maps the typed operator sidecar directly to the centralized PA14 ABI
  terminal, carries nonthrowing to `unwind=no`, preserves generated ordinal
  names for unnamed parameters, and consumes a literal array address through
  the normal cached global/index/load path exactly once.

The ordinary work is bounded by local declarator/token ranges, semantic-fact
children, and emitted IR: there is no textual reparsing, rendered-name ABI
reconstruction, body rescan, or fixture-specific branch.  The scale samples
below support deterministic, linear-looking growth for these paths but do not
prove an asymptotic bound.

## Failure Map

The clean turn-start baseline at `83d60e96` was `106/109`, with all `109`
tests covered.  The complete residual map is now resolved:

| test | owning repair | final disposition |
|---|---|---|
| `100-const-integral-lvalue-overload-category` | PA11 inserts `= delete` as a typed definition with `Binding.has_definition`; PA12 skips it as an expression and rejects a selected deleted overload | PASS |
| `100-string-hex-escape-code-unit` | PA10 accepts the multi-token cast; PA12 carries typed decoded bytes/address; PA15 lowers one cached string global and its ordinary subscript | PASS |
| `100-unnamed-parameter-storage` | PA10 operator identity reaches the PA11 sidecar; PA15 emits typed delete ABI, two generated parameter/slot identities, and `unwind=no` | PASS |

PA15 coverage is `109/109`.

## Active Checkpoint

The implementation is complete in `dev/src/pa10_ast.cpp`,
`dev/src/pa11_semantic_model.h`, `dev/src/pa11_semantic_core.cpp`,
`dev/src/pa11_semantic.cpp`, `dev/src/pa12_semantic.cpp`,
`dev/src/pa15_lowering.h`, `dev/src/pa15_lowering.cpp`,
`dev/src/pa15_operator_abi.cpp`, and `dev/frontend_source_sets.mk`.
`pa15/plan.md` records this checkpoint and its evidence; no tests or reference
fixtures were changed.

Focused validation passed with normal harnesses:

- `make -C pa15 check TEST='tests/general/100-const-integral-lvalue-overload-category.t tests/general/100-string-hex-escape-code-unit.t tests/general/100-unnamed-parameter-storage.t tests/general/100-subscript-sizeof.t tests/general/200-global-array-element-address-initializer.t tests/general/200-const-cast-reference-array-subscript.t tests/general/200-address-of-local-const-integral-uses-storage.t'` -> `PASS (7/7)`.
- `make -C pa10 check TEST='tests/general/100-c-style-cast-expression.t tests/general/200-cast-parenthesized-identifier-shift.t tests/general/200-cast-parenthesized-identifier-shift-or.t tests/general/200-sys-types-minor-cast-expression.t'` -> `PASS (4/4)`.
- Ephemeral declaration probes show: first deleted definition `rc=0`; deleted
  then ordinary compatible redeclaration `rc=0`; its call fails with `PA12
  call selects deleted function`; ordinary then deleted fails with `deleted/defaulted
  function must be first declaration`; duplicate deleted definitions and a body
  after deleted fail with `duplicate function definition`.  A competing
  nondeleted lvalue overload exits `0` and is selected.  Repeated compatible
  operator declarations merge to one `_ZdlPvS_` symbol with `unwind=no`.
- The parser matrix has two typed casts (including cv-qualified/multi-token
  scalars) and two ambiguous parenthesized-expression nodes; AST emission
  succeeds and the adjacent PA10 cases pass.

## Performance Evidence

Recipe: `/tmp/pa15_perf_input.pl COUNT` emits `COUNT` repeated functions with
`(const unsigned long)` casts and `COUNT` string subscripts of
`(unsigned char)"\\xab"[0]`, plus one deleted definition followed by one
ordinary compatible redeclaration per generated signature and repeated
compatible operator declarations.  It was run at `COUNT=32` and `COUNT=256`; each generated
LowIR output was checked by the normal PA15 harness from
`/tmp/pa15-perf-check` (`PASS (2/2)`, successful exit sidecars).

| count | input bytes / lines | LowIR bytes / lines | globals / functions / instructions / slots | SHA-256 of LowIR | time/RSS samples (s/KB) |
|---:|---:|---:|---:|---|---|
| 32 | 6,000 / 356 | 16,753 / 741 | 32 / 65 / 192 / 32 | `f4c89127448b65d0e939aa7c257992cf5ee1eec771dc70782314e551d946cba3` | `0.01/6652`, `0.00/6820`, `0.00/6636` |
| 256 | 48,064 / 2,820 | 134,775 / 5,893 | 256 / 513 / 1,536 / 256 | `42242a80f56f8c28943ca5f2661dd07000e1c90b582af56b886a2db0bf647b55` | `0.05/17884`, `0.05/18152`, `0.05/17888` |

The structural counts and output hashes are stable for each recipe.  These
two runs are representative evidence of deterministic growth with the
number of typed facts and emitted IR; the coarse timings and RSS samples are
not a proof of asymptotic complexity.

## Checkpoint Ledger

| checkpoint | result | durable value |
|---|---|---|
| prior typed conditional-array checkpoint `b7eaf9d8` | retained; supplied baseline `106/109` | typed PA12 category/conversion ownership through PA15 address lowering |
| current typed declaration/literal-address boundary checkpoint | complete; all focused and full gates green; no test/ref changes | typed declaration status, operator identity/ABI, bounded cast routing, and decoded string-address continuity through LowIR |

## Final State

PA15 is complete at `109/109`; through PA14 is `1058/1058` and through PA15
is `1167/1167`.  The source audit passes with five pre-existing header
warnings, `git diff --check` passes, and the committed worktree is clean.
