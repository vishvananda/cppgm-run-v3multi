# PA9 Final Architecture Audit

## Scope and result

This audit covers the PA9 CY86 production path after base commit `6ee81c4a`
and the directed final-audit repairs. The path is self-contained: it does not
invoke a host compiler, assembler, linker, reference executable, or previous
solution. It writes one native Linux x86-64 ELF load segment directly.

The repaired source audit passed with one pre-existing warning, focused PA9
validation passed 16/16, and the root through-PA9 report passed 457/457.
Six new course regressions and their reference sidecars are included. No
existing handout, test, or reference file was changed.

## Spec and architecture alignment

### Section 1: one production pipeline

The production flow is:

```text
source files
  -> PPPreprocessingSession / PPTokenBuffer per translation unit
  -> posttokenize_cpp_tokens / typed CY86 token arena
  -> CyParser / canonical CyStatement graph
  -> CySemantic / resolved typed operands and constraints
  -> layout_program / dry-run native sizing and label addresses
  -> CyNativeEmitter + CodeEmitter / direct x86-64 bytes
  -> write_elf / one PT_LOAD ELF executable
```

`CyParser` now writes directly into the one canonical `std::vector<CyStatement>`;
it no longer builds a second statement graph and copies it out. The token
vector, parser, and semantic validator are scoped together and destroyed
before empty-entry handling, layout, or native emission. `CySemantic` keeps
only its temporary defined-label set during validation. The removed parser
literal reference, unused `current()` helper, parser label set, and unused
`label_indices()` getter do not leave a second production model.

### Section 2: typed fact continuity

Names are interned once as `NameId`; fixed opcode spellings are recognized only
at the input boundary by an exact 170-entry descriptor table and become typed
`OpcodeInfo` facts. Literals are decoded once by posttokenization into
`LiteralStore` records and a byte arena. `CyExpr` retains `LiteralId`, `NameId`,
offset polarity, and resolved kind rather than rendering and reparsing text.
The backend receives `CyStatement`, `CyOperand`, `CyExpr`, `CyRegister`,
`OpcodeInfo`, and typed fixup values directly.

### Section 4: work, caches, and storage

The passes are forward linear scans over source tokens, statements, labels,
fixups, and emitted bytes, with fixed-size opcode-table lookup per opcode.
There is no whole-program retry-until-stable loop, broad invalidation, or
unbounded worklist. The only temporary semantic label set is bounded by label
count and is released before layout. Literal conversion and red-zone stores
are proportional to payload size. The 170-entry opcode lookup is a bounded
constant per classification; the scaling measurements below did not show an
excessive or nonlinear constant requiring a different vocabulary owner.

### Section 5: typed lowering and native emission

`CyStatement` is the single in-memory low-level model for this assignment.
`layout_program` dry-runs the same `CyNativeEmitter` used by final emission,
so label addresses and statement sizes are derived from the same typed
lowering decisions. `CodeEmitter` writes native instruction/data bytes and
resolves label values directly. `write_elf` adds only the ELF header,
program-header table, padding, and the payload required for execution.

### Section 7: measurement and conformance

The broad conformance result is 457/457. Structural and resource measurements
use immutable executable copies, immutable generated inputs, three interleaved
before/after runs per input, output hashes and sizes, and generated-program
exit status. `/usr/bin/time` supplies wall/user/system time and peak RSS; it
does not supply phase, allocation, or worklist counters. Values displayed as
`0.00` are below the timer’s reported hundredth-second resolution. This is
evidence of the measured curves and deterministic output, not a formal
complexity proof.

## Representative typed ownership traces

1. **Literal payload to ELF bytes.** A posttokenized `LiteralData` carries
   `FundamentalType`, `element_count`, and decoded bytes. `CyTokenCollector`
   stores the metadata and one arena slice in `LiteralStore`, returning a
   `LiteralId`. The parser places that ID in a literal-data statement or a
   `CyExpr`. Semantic validation checks its arithmetic/value category. Layout
   calls `literal_alignment` separately from payload sizing: for example, a
   long-double payload remains 10 bytes while its alignment is 16, and a
   string array uses its element type alignment. Native emission uses
   `literal_value_bytes` or `converted_literal_bytes` for signed/zero
   extension and width truncation, then `CodeEmitter::bytes` appends the
   exact bytes to the ELF payload.

2. **Identifier spelling to fixup.** The posttoken identifier spelling is
   interned once as a `NameId`. Parser labels retain that ID, and references
   retain the same ID in `CyExpr`. Semantic validation rejects fixed-register
   and exact-opcode conflicts, rejects duplicates, and resolves forward or
   backward references against its temporary defined-label set. Layout assigns
   each definition a runtime address before the corresponding dry-run native
   emission. Final `expression_value` consumes the typed label address for a
   rel32 jump/call, an immediate, or a data-address conversion; no spelling
   is recovered for the fixup.

3. **Opcode spelling to native encoding.** The input identifier is compared
   against the exact `kOpcodeDescriptors` vocabulary. The descriptor publishes
   `OpFamily`, width, auxiliary width, signedness, syscall argument count, and
   `CompareKind` in `OpcodeInfo`. `CySemantic::make_constraints` derives the
   descriptor-aligned operand facts, including write/immediate/address/width
   and integer/float/any categories. `CyNativeEmitter` switches on the typed
   family and selects the corresponding x86-64 encoding. Spellings such as
   `not80`, `iadd80`, `ieq80`, `fadd8`, `fadd16`, and invalid conversion widths
   are not recognized as opcodes and can remain labels.

4. **Address expression to safe load/store/x87 bytes.** A register, label, or
   scalar literal memory base is retained in `CyExpr`; a legal `+/-` offset
   retains its own typed `LiteralId` and is semantically checked as integral.
   The parser rejects literal-base offsets in both parenthesized immediate and
   square-bracket memory forms, while retaining label/register offsets. Native
   lowering materializes addresses with scratch registers selected away from
   the requested address, source, destination, and offset roles. Integer and
   move emitters capture source memory addresses before destination writes;
   x87 emitters load all source values before storing the result. Syscall
   lowering loads each required ABI argument register (up to six) before
   loading the syscall number into RAX, protecting it from offset scratch use.

## Findings and actual changes

- Pattern-based `classify_opcode` accepted valid-looking but unspecified
  widths and families. It was replaced by the exact table; a sorted comparison
  against `pa9/cy86-opcode.desc` found 170 names on each side and an empty
  diff.
- `CyParser` previously copied `statements_` and `label_names_` into output
  objects while tokens and parser storage remained alive through emission.
  It now appends moved statements directly to the canonical output, removes
  parser label storage, and scopes token/parser/semantic lifetime before
  layout and emission. Semantic duplicate detection and label lookup use one
  temporary set that is released with `CySemantic`.
- Parenthesized `(-literal)` operands previously deferred rejection of arrays
  and non-arithmetic types until emission. `CySemantic` now validates the
  negated operand literal before resolution and constraint lowering. Integral
  and floating literals remain valid; arrays and non-arithmetic literals are
  rejected at the semantic owner.
- Literal-base `+/-` offsets are rejected in `parse_parenthesized_operand` and
  `parse_memory_operand`, the recognition owners of the two grammar forms.
- The descriptor has no `i`, `s`, `u`, or `f` category on `not`, bitwise
  `and/or/xor`, or syscall operands. Their constraints now use `VALUE_ANY`.
  Reference probes accepted floating-bit literals for representative bitwise
  and syscall cases, confirming the correction.
- Syscall argument loading could use RAX as an offset scratch after the
  syscall number had been loaded. Loading the number last preserves the
  syscall fact while leaving argument registers intact.

## Regression additions and reference note

Added under `cppgm.tests/course/pa9`:

- `300-exact-opcode-vocabulary.t.1`: invalid opcode-like and invalid-width
  spellings used as labels.
- `300-literal-immediate-offset-bad.t.1`: forbidden parenthesized
  literal-base offset.
- `300-literal-memory-offset-bad.t.1`: forbidden square-bracket literal-base
  offset, with a trailing extra operand so the pinned reference also reports a
  compile failure.
- `400-syscall-offset-scratch.t.1`: syscall-number preservation with an
  offset memory argument targeting ABI R10; generated exit status is 42.
- `400-untyped-descriptor-literals.t.1`: floating-bit literals through
  untyped bitwise and syscall operands.
- `400-negated-operand-array-bad.t.1`: non-arithmetic negated operand.

Reference sidecars for exactly these six tests were generated by:

```sh
make -C pa9 ref-test TEST='course/pa9/300-exact-opcode-vocabulary.t.1 course/pa9/300-literal-immediate-offset-bad.t.1 course/pa9/300-literal-memory-offset-bad.t.1 course/pa9/400-syscall-offset-scratch.t.1 course/pa9/400-untyped-descriptor-literals.t.1 course/pa9/400-negated-operand-array-bad.t.1'
```

The pinned reference rejects the isolated parenthesized literal offset but
accepts the isolated `[literal +/- literal]` form. The handout grammar is
authoritative, so the implementation and an isolated temporary probe reject
the memory form; the checked public regression adds a trailing arity error only
to obtain a reference-visible failure status. This limitation is explicit and
does not alter any existing reference fixture.

## Performance and structural evidence

The before executable was preserved before the ownership repair:

```text
before SHA-256  f0c7a333eceb4febb984d59172087268ad6383220a3d82811aae965dcae418e9  528720 bytes
after SHA-256   0cd8720a53b62380aac4ff0c57661c5e7eed2feb71c537158b24e5c7b84f9e3f  528072 bytes
```

Inputs were generated once in `/tmp`, hashed before the runs, and reused for
both immutable executables. The run form was:

```sh
/usr/bin/time -f 'kind=... n=... phase=... run=... wall=%e user=%U sys=%S rss_kb=%M' /tmp/cy86-pa9-audit-... -o /tmp/cy86-pa9-audit-... pa9-input
```

Each row below is the median of three interleaved before/after runs. All 48
compiler invocations across both workload families exited 0. All after-version
generated programs exited 0. Every three-run output set had one unique hash,
and the before and after hashes in each row were identical.

| workload | input bytes / SHA-256 | output bytes / SHA-256 | before median wall/user/sys/RSS KiB | after median wall/user/sys/RSS KiB |
|---|---:|---:|---:|---:|
| 256 data statements (258 total) | 4,796 / `9ec65bbf665e9e4b64a086ad5e71cbca8b20e6bd1e3ad45e304d879317e89fc3` | 4,407 / `fe82df060bc67a8dd60b17aaee5efcb418b2e1a2ebabbd0b89cea30f3370505d` | 0.00 / 0.00 / 0.00 / 4,348 | 0.00 / 0.00 / 0.00 / 4,372 |
| 1,024 data statements (1,026 total) | 19,412 / `e926ac54f0894ecf77f5f670f37edf0a17a4bb7a865523fd2891962120490c69` | 5,175 / `7734a6e3cc6cd277d61cfe71d743bb1afb7570569de5d6a5e5fbbc141d80d373` | 0.01 / 0.00 / 0.00 / 6,400 | 0.01 / 0.00 / 0.00 / 6,404 |
| 4,096 data statements (4,098 total) | 80,852 / `3189a9ffcd51f5472cf8ba0e592ff8849c1e98ff273cae3d1928249b06d34131` | 8,247 / `1a8889086b223f219bf2e6078ca25a3386ff4d97fbb2303af34328a1e028e79a` | 0.03 / 0.02 / 0.01 / 14,020 | 0.03 / 0.02 / 0.01 / 14,016 |
| 16,384 data statements (16,386 total) | 332,996 / `4038ced961b18964fa34c70062c71476223c796a03a6a393c5e5e6683267fb90` | 20,535 / `c6bff9084bac0b29d2331e238e91fee917a0091c4c805f6b5f739278db1f59f4` | 0.15 / 0.10 / 0.05 / 44,920 | 0.14 / 0.10 / 0.04 / 44,920 |
| 4,096 literal payload bytes (3 total statements) | 4,148 / `0044ec3c2debefa17a2fe87dca676aa42ef83ce77b57dbb70fe0b4ecde083e77` | 8,248 / `2dc43e2ba42be35c67848eba120a3a6b7bdf9fe5ba56848001b0beb9782a8174` | 0.00 / 0.00 / 0.00 / 3,868 | 0.00 / 0.00 / 0.00 / 3,856 |
| 16,384 literal payload bytes (3 total statements) | 16,436 / `34d621a84f41c00b4324719aad331f74ede73f77669b09ab2698d8ec384f05c6` | 20,536 / `f9f5b2fb5572a20bbe67fe743582b0f9e1fe22f743bf8cea7f829e4cb0995b64` | 0.00 / 0.00 / 0.00 / 4,504 | 0.00 / 0.00 / 0.00 / 4,504 |
| 65,536 literal payload bytes (3 total statements) | 65,588 / `71538c3aa183f4ed2fe5570343f65c5a5fd82dbd8f0511da5611a20e9494cece` | 69,688 / `355fb648d7e96d7f2949ab798adfb9a8045482528c79559df84fb9e4b9c2d908` | 0.01 / 0.00 / 0.00 / 6,976 | 0.01 / 0.00 / 0.00 / 6,972 |
| 262,144 literal payload bytes (3 total statements) | 262,196 / `485cdd8c0c8cd0df0c4297633cf4e5707cf6c42466777a445761f7660fafb5b7` | 266,296 / `2f3c9d91ff13599860af666c67069e2eb004775f156e597d6d41b39145f65070` | 0.04 / 0.02 / 0.01 / 17,128 | 0.04 / 0.02 / 0.01 / 17,132 |

The curves are ordinary linear growth in the consumed statement or literal
payload facts, with small cases below timer resolution. The ownership repair
reduces retained parser/token duplication and the final executable is 648
bytes smaller, while output remains byte-identical across the paired runs.
No unexplained curve or outlier remained after the structural review, so no
profile was needed. These measurements do not claim phase-level attribution or
an allocation-count improvement.

## Focused and broad validation

Build and focused regression commands:

```sh
make -C dev cy86
make -C pa9 ref-test TEST='course/pa9/300-exact-opcode-vocabulary.t.1 course/pa9/300-literal-immediate-offset-bad.t.1 course/pa9/300-literal-memory-offset-bad.t.1 course/pa9/400-syscall-offset-scratch.t.1 course/pa9/400-untyped-descriptor-literals.t.1 course/pa9/400-negated-operand-array-bad.t.1'
make -C pa9 check TEST='tests/100-noop.t.1 tests/210-reverser.t.1 tests/400-integer-calculator.t.1 tests/500-to-float80.t.1 tests/501-from-float80.t.1 tests/600-float-calculator.t.1 course/pa9/100-empty-program.t.1 course/pa9/300-exact-opcode-vocabulary.t.1 course/pa9/300-literal-immediate-offset-bad.t.1 course/pa9/300-literal-memory-offset-bad.t.1 course/pa9/400-negated-operand-array-bad.t.1 course/pa9/400-syscall-offset-scratch.t.1 course/pa9/400-untyped-descriptor-literals.t.1 course/pa9/400-negated-wchar-sign-extension.t.1 course/pa9/500-long-double-label-alignment.t.1 course/pa9/500-string-literal-element-alignment.t.1'
```

Results: `make -C dev cy86` exit 0; reference generation exit 0 with 6
tests; and the focused check exit 0 with 16/16.

The focused 16-test selection covered existing integer, float, empty-entry,
alignment, and reverser behavior plus all six new regressions. Direct probes
also produced: exact-vocabulary exit 0; allowed label-offset exit 0; isolated
literal immediate and memory offsets compile exit 1; syscall scratch program
exit 42; untyped bitwise and syscall programs exit 0; and negated-array
operand compile exit 1.

Required checks:

```text
perl scripts/cppgm_file_audit.pl --stage pa9 --paths dev/src   exit 0
make test-report-through-pa9                                    exit 0; 457/457
git diff --check                                                exit 0
```

The file-audit warning is:

```text
[warning][bad-division] dev/src/cpp_semantic_core.h:1:
header contains substantial implementation body; prefer .cpp ownership
```

It predates this PA9 work and is outside the touched ownership path; it was
not suppressed or rewritten.

## Limitations and uncertainties

- `/usr/bin/time` reports process-level values only; no phase timers, allocation
  counters, retained-byte counters, or compiler worklist counters were
  available. Small timings are below its displayed resolution.
- The pinned reference accepts the isolated memory literal-offset form, so the
  public sidecar for that regression includes a separate arity failure. The
  handout grammar and direct implementation probe enforce the intended rule.
- Public and focused probes cover representative aliasing, forward-label,
  move80, integer, floating, jump/call, and syscall paths. Hidden combinations
  of descriptors and dynamic targets remain the normal residual uncertainty.
- The single warning above remains because changing the shared PA8 semantic
  header would expand this PA9 audit beyond its owning boundary.

## Checkpoint and commit ledger

1. Base: clean `v3multi` at `6ee81c4a` (`Implement PA9 CY86 native backend`).
2. First audit checkpoint: exact vocabulary, grammar, descriptor, and syscall
   repairs made uncommitted; supervisor approved the base diff.
3. Directed correction: parser canonical ownership/scoping and semantic
   negation validation implemented; six course regressions added; only their
   sidecars regenerated through `make -C pa9 ref-test`.
4. Validation checkpoint: focused 16/16, file audit exit 0 with one warning,
   broad 457/457, and `git diff --check` exit 0.
5. Final worker commit: the commit containing this audit and ledger; its ID is
   reported in the final handoff, and no earlier commit is amended.
