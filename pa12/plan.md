# PA12 checkpoint plan

## Stage Design:

Contract: `--emit-semantics` consumes the existing preprocessing and PA10 AST
pipeline, extends the canonical PA11 owner, and renders deterministic PA12
facts.  The production flow is one owner:

`PPPreprocessingSession -> PPTokenBuffer -> PA10Ast -> PA11SemanticModel -> PA12 fact arenas -> cold dump`

`TypeId`, `BindingId`, `ScopeId`, `SemanticFactId`, and `ConversionFactId`
are typed identities.  PA12 facts, children, names, conversions, and
declaration bindings use contiguous insertion-ordered storage; rendering is
the only text boundary.  Canonical type equality is O(1) `TypeId` equality.
Declaration, function, namespace, and compound facts are indexed by typed
IDs through compact pointer-keyed `FlatIndex` tables: average O(1) lookup,
with lexical vectors retained for deterministic order.  Candidate collection
uses relevant scope values only; ranking is O(c*a) for candidates `c` and
arguments `a`.  No rendered semantic text is parsed back.

## Failure Map:

Turn-start baseline: `make test-pa12` was `0/166` passing, with all 166
existing cases failing as `EXIT_NOT_IMPLEMENTED`; coverage is unchanged.
The complete current-stage set is categorized here and excludes unrelated
stages.

| semantic subsystem / checked-in set | total | baseline | final |
| --- | ---: | ---: | ---: |
| foundational declarations, scopes, calls, overloads, arithmetic, conversions — `tests/spec/100-*.t` | 12 | 0 | 12 |
| foundational calls, literals, pointer/reference conversions, assignments, namespace lookup — `tests/general/100-*.t` | 25 | 0 | 25 |
| control-flow, indirect calls, arrays, subscripting, `sizeof` — `tests/spec/200-*.t` | 9 | 0 | 4 |
| broader scopes, conditions, using forms, loops, conversions — `tests/general/200-*.t` | 33 | 0 | 12 |
| specified negative overload, scope, conversion, null-pointer cases — `tests/spec/300-*.t` | 10 | 0 | 4 |
| advanced procedural, enum, pointer, cast, namespace, control-flow cases — `tests/general/300-*.t` | 77 | 0 | 28 |
| **all PA12 tests** | **166** | **0** | **85** |

Thus 81 residual failures remain in excluded or broader families; no tests or
references were removed.

## Active Checkpoint:

Included: empty units; top-level aliases, variables, function declarations and
definitions; named/qualified namespaces and aliases; function/block scopes;
parameters, local declarations, initializers, returns; integer, bool,
`nullptr`, string-array, and straightforward floating functional-cast
classification; id/parenthesized expressions; direct and qualified calls;
basic overload selection; identity, lvalue-to-rvalue, integral, pointer
qualification/void-pointer, null-pointer, array/function decay, and basic
reference-binding conversions; arithmetic, logical, equality, assignment,
pointer-plus, prefix/postfix increment, and conditional facts; and real
rejection for invalid calls/bindings.  The PA10 lookahead correction is scoped
to simple builtin cast types so `(void)--x` parses without changing expression
parenthesis behavior.

Excluded: classes/members, templates, user-defined conversions/operators,
full control-flow statements, target-directed overloaded function-pointer
expressions, deep pointer qualification safety, general enum semantics, and
broader 200/300 diagnostics.  Unsupported successful inputs are rejected,
not emitted as plausible incomplete facts.

Focused validation after modularization and indexing:
`make -C pa12 check TEST='tests/spec/100-*.t tests/general/100-*.t'` — 37/37.
Explicit prior-stage validation: `make test-pa11` — 68/68; the corrected PA10
case — 1/1; exact through-PA11 gate — 685/685.

Supervisor review findings and resolution: the 4,753-line owner exceeded the
3,000-line audit limit, so it was split into self-contained typed core,
PA12-fact, and rendering/entry `.cpp` modules with a shared model header and
registered source set.  Whole-vector declaration/function/compound/namespace
searches were replaced by typed pointer `FlatIndex` indexes; lexical arenas
remain the render order.  The shared model changes were checked by the prior
stage gates above.  The driver route and one-owner boundary were retained.

## Performance Evidence:

Immutable generated inputs were created only under `/tmp/pa12-perf.rRyL68`
and not modified during measurement.  Seven interleaved `/usr/bin/time`
samples were taken; values have 0.01-second resolution, so these are scaling
observations rather than unsupported speed claims.

| workload | structural input / dump counts | median wall |
| --- | --- | ---: |
| facts-200 | 200 lines/18,250 bytes; 200 functions, 200 variables, 1,000 expression records; 2,004 dump lines | 0.01s |
| facts-800 | 800 lines/74,650 bytes; 800 functions, 800 variables, 4,000 expression records; 8,004 dump lines | 0.05s |
| pointer-overloads-244 | 243 candidates + driver, fixed 5 arguments, 2 variables; 246 input lines/25,253 bytes; 262 dump lines | 0.02s |
| pointer-overloads-1025 | 1,024 candidates + driver, fixed 5 arguments, 2 variables; 1,027 input lines/113,872 bytes; 1,043 dump lines | 0.09s |

The 4x fact and dump scales and approximately 4x candidate scales show the
expected linear growth in these representative runs (with startup/output and
timer quantization included).  All pointer candidates are viable, so the
fixed-argument overload runs exercise candidate ranking across the five
argument slots.  Source inspection also confirms no newly added whole-arena
per-node scan; the fact indexes are average O(1), while ranking remains the
specified O(c*a) loop.

The exact audit command passed with two pre-existing/header-shape warnings:
`cpp_semantic_core.h` and the data-oriented `pa11_semantic_model.h` contain
substantial inline type/storage bodies; no fatal audit issue remains.

## Checkpoint Ledger:

- Baseline: clean `8034c734`; `make test-pa12` was `0/166`, all
  `EXIT_NOT_IMPLEMENTED`.
- Uncommitted milestone: driver wiring, shared PA11/PA12 fact owner, PA10
  cast-lookahead fix, three implementation modules, typed structural indexes,
  and this plan.  Full `make test-pa12`: `85/166` pass, `81` residual
  failures; focused `37/37`; prior gate `685/685`; audit passed with 2
  warnings; performance evidence is recorded above.
- Final commit/check evidence: the checkpoint was committed with message
  `PA12: add shared semantic fact foundation` after the exact full-suite,
  through-PA11, audit, performance, and whitespace checks above; its complete
  stat was inspected and the final working tree is clean.  The resulting
  commit hash is the handoff identifier.
