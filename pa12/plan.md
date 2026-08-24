# PA12 functional-cast/type-name checkpoint

## Stage Design

`PA11SemanticModel` remains the sole semantic owner in the forward PA10 ->
PA11 -> PA12 pipeline. `TypeKey`/`TypeId` is the only semantic type identity;
canonical type construction stays in `pa11_semantic_types.cpp`, and
target-directed selection stays in `pa12_semantic_resolution.cpp`. There is no
parallel parser/type model, rendered-string recovery, or cold-renderer input.

The retained member-pointer work uses typed `TypeKey.named` ownership,
function cv flags, sparse static-member sidecars, reverse suffix binding, and
recursive array qualification. Its prior structural/declarator evidence and
ledger are retained below; this checkpoint is the functional-cast/type-name
increment.

PA10 preserves the call-shaped boundary. The bounded
`classify_function_style_cast` result in `pa10_parser_support.cpp` owns
None/legacy/typed classification, the contiguous builtin/cv scan, indexed
`decltype` close lookup, and exact `charged_work`; `PA10Parser` charges that
result once and consumes it. A single-token built-in cast keeps its legacy
identifier form. A contiguous multi-keyword fundamental type or
`decltype(expr)` target remains a typed `TypeId` child. In PA12,
`pa12_semantic_resolution.cpp` owns functional target resolution, support
validation, and source-to-target cast construction through `builtin_cast_target`,
`type_from_type_id`, or relevant-scope typed lookup. Value lookup precedes alias
lookup, so ordinary calls and hiding remain intact.

## Spec Alignment

- Root spec §1: PA10's call-shaped `PA10Ast` feeds the sole PA11 semantic model;
  PA12 extends that path without a parser, type model, or renderer-input
  duplicate.
- Root spec §§2-3: canonical `TypeId`/`TypeKey`, typed `NamePath` lookup,
  `SemanticFactId`, and owner-attached `ConversionFact` values carry the
  classified facts. Rendering is cold and one-way; value lookup still precedes
  type-alias lookup.
- Root spec §4: delimiter indexes are built once, cast-prefix work is charged
  once per parser classification, and functional-cast work is bounded by the
  contiguous specifier prefix plus one operand walk. The increment adds no
  persistent hot record fields or retry loop.
- Root spec §7: focused outputs are deterministic and the retained immutable,
  interleaved cast-family measurements below are reported as bounded
  observations, not asymptotic claims.

## Failure Map

The fresh final checkpoint result covers all 166 paths and passes `163/166`.
Its exact residuals are:

1. `pa12/tests/general/300-local-extern-function-declaration.t`
2. `pa12/tests/general/300-reference-binding-pointee-const-pointer.t`
3. `pa12/tests/general/300-static-cast-overloaded-function-template-argument.t`

The earlier `155/166` baseline and its five fixed member-pointer/array paths,
followed by the landed functional-cast `160/166` baseline, remain historical
evidence. The retained audit history is `90/166`, `103/166`, `113/166`,
`120/166`, `142/166`, `146/166`, `149/166`, `155/166`, `160/166`, then the
fresh final `163/166` result.

The landed increment removes the three functional-cast/type-name residuals;
the current exact residual set is the unchanged local-extern,
reference-binding, and overloaded-function-template set above. Fresh
through-PA12 is `848/851` with that same set; fresh
prior-through-PA11 is `685/685`. No additional failure is permitted,
and additional passes do not compensate for a new failure.

## Active Checkpoint

- Commit `115aff98b17774925ae64e53c1ec3260a0b1e6ca` is the reviewed landed
  increment; the audit found no direct regression requiring a source repair.
- `decltype(expr)(arg)`, multi-keyword fundamental casts such as
  `unsigned long(e)`, visible aliases, and supported scalar zero-initialization
  share one call-shaped AST boundary and one typed semantic target path. PA10's
  bounded classifier is owned by `pa10_parser_support.cpp`; PA12's typed cast
  target/validator and conversion builder are owned by
  `pa12_semantic_resolution.cpp`.
- Supported one-argument casts reuse explicit-cast validation, including the
  scoped-enum-to-integral case. `semantic_cast_to_target` records the selected
  source-to-target `ConversionFact` on the resulting `CastExpression` for both
  explicit and functional casts.
- When a later context asks for the exact type of an already-created cast
  prvalue, `record_builtin_conversion` performs no additional identity
  conversion. This preserves the cast owner’s contiguous conversion range; it
  does not drop the selected cast conversion. The sibling functional/explicit
  cast probe is `/tmp/pa12-sibling-cast-range.cpp` and succeeds.
- Zero-argument supported scalar targets produce typed prvalue zero literals;
  unsupported class construction is rejected. Existing ordinary calls,
  invalid arity, name hiding, cast-to-void, and static-cast behavior remain
  covered by focused checks.

## Performance Evidence

Retained member-pointer structural measurements are: `TypeKey 80`, `Scope
440`, `DeclaratorOp 40`, `FunctionFact 48`, `BindingSidecar 32`, and `SpecFact
32` bytes. The functional-cast increment adds no persistent record fields.

The retained declarator-depth measurement used five interleaved rounds over
200 typedef declarations; every run exited `0`:

| depth | wall samples (ms) | median wall (ms) | median user (ms) | median sys (ms) | median max RSS (KiB) |
|---:|---|---:|---:|---:|---:|
| 32 | 10, 10, 10, 10, 10 | 10 | 0 | 0 | 10464 |
| 128 | 60, 50, 50, 50, 60 | 50 | 20 | 30 | 26892 |
| 512 | 230, 220, 230, 220, 220 | 220 | 120 | 100 | 91860 |

Retained cast-family evidence used an immutable copy of the newly built
compiler and five interleaved rounds over equivalent generated inputs
containing 128, 512, and 2048 functions; each function included
`unsigned long(x)`, `decltype(x)(1)`, and zero-argument `size_t()`. Medians
were:

| functions | median wall | median max RSS |
|---:|---:|---:|
| 128 | 0.02 s | 11520 KiB |
| 512 | 0.09 s | 34312 KiB |
| 2048 | 0.41 s | 124884 KiB |

Repeated hashes matched at every size: 128=`03c34dd8c84bf3bdbd3d1e83d29f3b8a05cd71349f51da2c0c395eb4e424e93f`,
512=`c940bc38dc8c51c8bc094df6c332539d29f8e5080b3d74b1f4dfc0f9e48723fb`,
and 2048=`40c86b4490f5e3a50fc9fd92849c0c5a640bd28dc52a53e1eb0dc9bd0ca02137`.
These are bounded observations, not a broader timing coefficient or
asymptotic claim. The parser risk itself is bounded by the contiguous O(k)
specifier scan, indexed `decltype` close lookup, relevant-scope lookup, and
one operand semantic walk. A fresh focused six-case repeat produced six
status `0/0` pairs and six `cmp` exits `0`; that is determinism evidence, not a
new performance claim.

## Checkpoint Ledger

| checkpoint | retained evidence | outcome |
|---|---|---|
| PA12 canonical declarator/member-target | landed `4f890322`; focused PA12 `20/20`; PA10 `4/4`; 12 probes; two rendering assertions; broad PA12 `160/166`; through-PA11 `685/685` | complete; exact six residuals remained for the next family |
| PA12 functional-cast/type-name | focused PA12 `11/11`; PA10 `5/5`; sibling-cast probe success; broad PA12 `163/166` with 166 covered; through-PA11 `685/685`; through-PA12 `848/851`; final audit exit `0` with two retained header-division warnings; diff check clean | complete for the owned family; residuals 2, 3, and 5 remain outside scope |
| PA12 `checkpointAudit` for `115aff98` | Complete ownership trace; no source repair; focused PA12 `3/3 + 8/8`, PA10 `4/4`, six deterministic repeated dumps; fresh PA12 `163/166` with all 166 covered, fresh through-PA11 `685/685`, fresh through-PA12 `848/851`, fresh file audit with 2 retained warnings, and clean diff/path checks | complete audit record; exact three residuals remain outside scope |

## Validation Status

Focused evidence: `make -C pa12 -j2` exit `0`; the
three active PA12 cases passed `3/3`, eight PA12 call/cast/static-cast controls
passed `8/8`, and four PA10 call-shaped parser controls passed `4/4`. Six
repeated direct dumps had tool statuses `0/0` and `cmp` exit `0` for every
pair. The three residual checks reproduce only the final residual set and
remain intentionally excluded.

Fresh final stage evidence:

- `make test-pa12`: exit `2`, `163/166`, all 166 covered, exactly the three
  residuals in the Failure Map.
- Exact command `n=12; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`: exit `0`, `685/685`.
- `make test-report-through-pa12`: exit `2`, `848/851`, exactly the same three
  PA12 residuals and no earlier regression.
- `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src`: exit `0`,
  with the two retained header-division warnings
  for `dev/src/cpp_semantic_core.h` and `dev/src/pa11_semantic_model.h`; no
  fatal findings.
- `git diff --check`: exit `0`.
- Exact authorized-path audit: pass; changed paths are exactly
  `pa12/audit.md` and `pa12/plan.md`, with no tests/refs/harnesses/grammar/
  scripts changed.

## Next Checkpoint

The first remaining residual family is
`pa12/tests/general/300-local-extern-function-declaration.t`. A future
checkpoint may investigate that local-extern declaration path with separate
authorization. The pointee-const-pointer reference-binding residual and the
overloaded function-template selection residual remain separately out of this
checkpoint; no work on either is implied here.

## Remaining Scope

The final residuals are `300-local-extern-function-declaration.t`,
`300-reference-binding-pointee-const-pointer.t`, and
`300-static-cast-overloaded-function-template-argument.t`. They remain outside
this checkpoint. No general class-aware calls, class construction, or template
semantics were added.
