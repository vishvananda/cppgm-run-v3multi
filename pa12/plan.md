# PA12 checkpoint plan

## Stage design and spec alignment

The PA12 production route remains one owner:

`PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 typed facts -> cold deterministic dump`

PA10 owns structured syntax and producer identities; PA11 owns scopes,
canonical types, bindings, lookup, and declaration-derived facts; PA12 extends
that same model with typed expression and conversion facts. `TypeId`,
`BindingId`, `ScopeId`, `SemanticFactId`, and `ConversionFactId` remain typed
identities. Rendering is the only semantic text boundary. Canonical type
equality is O(1) `TypeId` equality, typed indexes remain compact average-O(1)
lookup structures, relevant-scope lookup is retained, and overload ranking is
still O(c*a) for candidates `c` and arguments `a`.

This review confirms the PA12 requirements relevant to the repaired route:

- function declarations and definitions in one scope reuse one compatible
  canonical function binding; parameter top-level cv is normalized for the
  function type, return-type conflicts are rejected, and a second definition
  is rejected;
- recursive pointer qualification checks preserve source cv, compare every
  qualification level, and require the intermediate target `const` needed by
  a deep qualification conversion;
- definition parameter bindings retain their body-visible cv facts while the
  cold dump renders the normalized function-type parameter spelling;
- no rendered AST or semantic dump is parsed back, and the repair adds no
  shell-out, textual shortcut, new owning text model, or whole-arena scan.

## Failure map and coverage

The original implementation baseline was **0/166 passing**. The audit
turn-start/current full-suite evidence was **85/166 passing, 81 failures**;
the final audited result is **90/166 passing, 76 failures**. The complete
166-test inventory and coverage were preserved; no test, reference, harness,
or grammar file changed.

| semantic subsystem / checked-in set | total | original baseline | turn-start | final audited |
| --- | ---: | ---: | ---: | ---: |
| foundational declarations, scopes, calls, overloads, arithmetic, conversions — `tests/spec/100-*.t` | 12 | 0 | 12 | 12 |
| foundational calls, literals, pointer/reference conversions, assignments, namespace lookup — `tests/general/100-*.t` | 25 | 0 | 25 | 25 |
| control-flow, indirect calls, arrays, subscripting, `sizeof` — `tests/spec/200-*.t` | 9 | 0 | 4 | 4 |
| broader scopes, conditions, using forms, loops, conversions — `tests/general/200-*.t` | 33 | 0 | 12 | 13 |
| specified negative overload, scope, conversion, null-pointer cases — `tests/spec/300-*.t` | 10 | 0 | 4 | 4 |
| advanced procedural, enum, pointer, cast, namespace, control-flow cases — `tests/general/300-*.t` | 77 | 0 | 28 | 32 |
| **all PA12 tests** | **166** | **0** | **85** | **90** |

The repaired focused cases are now in the audited checkpoint: deep unsafe
qualification, conflicting function return type, duplicate function
definition, valid deep qualification, top-level-cv redeclaration, function
declarations, parameter renaming, and representative pointer qualification
and void-pointer cases. Broader function-pointer/reference, class/constructor,
and parser gaps remain residual failures and were not expanded here.

## Final validation evidence

Exact commands and final results:

- `make test-pa12` — **90/166 passed, 76 failures**; unchanged 166-test
  coverage and no new test or fixture files.
- `n=12; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi` — **685/685** through PA11.
- `make -C pa12` — build passed; one pre-existing warning in the unchanged
  cast path remains.
- The focused repair and neighboring set — **10/10**:
  `make -C pa12 check TEST='tests/general/300-bad-deep-pointer-qualification-conversion.t tests/general/300-conflicting-function-return-bad.t tests/general/300-bad-duplicate-function-definition.t tests/general/200-deep-pointer-qualification-conversion.t tests/general/200-by-value-parameter-top-level-cv-redeclaration.t tests/spec/100-function-decls.t tests/general/300-function-definition-parameter-renaming.t tests/spec/100-pointer-qualification-conversion.t tests/general/100-pointer-to-const-void-call.t tests/general/300-pointer-to-void-drops-cv-bad.t'`.
- The explicit qualification matrix — **5/5**:
  valid `int** -> const int * const *`, invalid `int** -> const int **`,
  ordinary pointer qualification, pointer-to-const-void, and cv-dropping
  void-pointer rejection.
- Foundational regression set — **37/37**:
  `make -C pa12 check TEST='tests/spec/100-*.t tests/general/100-*.t'`.
- The canonical-binding PA11 regression — **1/1**:
  `make -C pa11 check TEST=tests/general/200-qualified-namespace-function-definition-parameter-type.t`.
- `perl scripts/cppgm_file_audit.pl --stage pa12 --paths dev/src` — passed
  with exactly two pre-existing warnings for `cpp_semantic_core.h` and
  `pa11_semantic_model.h` substantial inline bodies.
- `git diff --check` — passed.

Residual failures are exactly 5/9 in `tests/spec/200-*.t`, 20/33 in
`tests/general/200-*.t`, 6/10 in `tests/spec/300-*.t`, and 45/77 in
`tests/general/300-*.t`; both 100-level sets pass completely. They remain in
unsupported or broader control-flow, function-pointer/reference,
class/constructor, enum, pointer-arithmetic, cast, namespace, and parser
families. The local-`extern` case still fails before semantic analysis in the
unchanged PA10 parser (`expected primary expression at token 49`).

## Performance evidence

The immutable generated-input evidence from the committed checkpoint is
preserved here. It used seven interleaved `/usr/bin/time` samples with
0.01-second resolution and recorded medians:

| workload | structural input / dump counts | median wall |
| --- | --- | ---: |
| facts-200 | 200 lines/18,250 bytes; 200 functions, 200 variables, 1,000 expression records; 2,004 dump lines | 0.01s |
| facts-800 | 800 lines/74,650 bytes; 800 functions, 800 variables, 4,000 expression records; 8,004 dump lines | 0.05s |
| pointer-overloads-244 | 243 candidates + driver, fixed 5 arguments, 2 variables; 246 input lines/25,253 bytes; 262 dump lines | 0.02s |
| pointer-overloads-1025 | 1,024 candidates + driver, fixed 5 arguments, 2 variables; 1,027 input lines/113,872 bytes; 1,043 dump lines | 0.09s |

No new timing sample was needed for this bounded repair. Source-structure
recheck found no change to the fact arenas, `FlatIndex` indexes,
relevant-scope candidate collection, or the O(c*a) ranking loop. The repair
adds only a same-name binding-vector scan for compatible redeclaration
(bounded by that scope's overload set), a lexical dump view keyed by the
canonical binding, and a qualification decomposition proportional to pointer
depth; all retain typed IDs and avoid whole-arena scans.

## Next checkpoint

This bounded audit is complete at 90/166 with 76 documented residual
failures. Any future PA12 residual-family work requires separate authorization;
no broader feature intake is part of this checkpoint.

## Checkpoint ledger

| row | result |
| --- | --- |
| original implementation baseline | Clean parent `8034c734`; PA12 was 0/166 before the shared-fact milestone. |
| committed foundation checkpoint | `a859c671`; known full evidence 85/166 and 81 residual failures, with prior-stage evidence and immutable performance work preserved. |
| current audit row (complete) | Confirmed the single typed ownership route; repaired deep qualification safety and canonical function redeclaration/definition state while restoring lexical PA11 dump views; final PA12 90/166 with 76 residual failures, through-PA11 685/685, file audit passed with two known warnings, and final diff/status checks completed before the required Luna-authored commit. |
