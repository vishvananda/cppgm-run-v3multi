# PA12 residual checkpoint

## Stage Design

The production flow remains `PA10Parser -> PA10Ast -> PA11SemanticModel ->
PA12 facts/dump`. `PA11SemanticModel` is the sole semantic owner: canonical
`TypeKey`/`TypeId`, `BindingId`, `ScopeId`, `NamedRecordId`,
`TemplateFunctionId`, `TemplateSpecializationId`, `SemanticFactId`, and
`ConversionFactId` cross the boundary. PA10 owns syntax and indexed lookahead;
PA11 owns declaration/type/scope identity; PA12 owns expression resolution,
conversion, lifetime actions, and the final cold renderer. There is no parallel
parser/model, rendered-name recovery, reference-tool shell-out, or whole-program
retry.

The declaration route uses one bounded support classification for a leading
global-qualified type, charges its reported work once, and leaves consumption to
the normal parser. PA11 keeps a one-pass unnamed abstract-declarator
prefix/suffix split and canonical array/function parameter adjustment. A typed
`NamedRecordId` display sidecar is populated only after the occurrence path is
proved equal to the record's canonical owner/name path. Template
specializations use a typed `(TemplateFunctionId, vector<TypeId>)` key index;
each indexed fact records `NotStarted`, `InProgress`, `Complete`, or `Failed`.
Semantic facts retain the selected `BindingId`; specialization presentation is
recovered from that binding's sparse sidecar at cold render time.
The template exception is deliberately limited to one-component names bound to
global template declarations. The checked-in residual fixture uses the
unqualified template-id spelling; a focused out-of-tree global-qualified
spelling also passed. Namespace-qualified template names and nested same-name
hiding are not claimed by PA12.

## Spec Alignment

- §§1-2: one forward model and typed fact continuity; display strings are
  rendered only at the requested PA12/PA11 dump boundary.
- §3: template candidates are indexed by typed `NameId`; explicit arguments,
  direct `T` deduction, specialization keys, and selected bindings remain
  typed. Every complete specialization key has explicit not-started,
  in-progress, complete, and failed states; failed keys are retained and do
  not repeat substitution. The narrow bridge covers the checked-in unqualified
  `hello<stream>` target and `take(T)` call, both bound to global template
  declarations; a focused out-of-tree global-qualified one-component spelling
  also passed. No general template-aware overload machinery is added.
- §4: parser work is indexed and charged once; candidate work is limited to
  the supported same-name template list and visible-scope ancestry checks;
  specialization reuse performs one typed flat-index lookup on the function ID
  plus exact `TypeId` argument vector, rather than scanning prior facts.
- §7: focused exact-output checks, deterministic alias-order probes, broad
  stage totals, file audit, and diff/path checks are recorded below. Timing and
  work statements are bounded observations, not unsupported asymptotic claims.

## Failure Map

Turn-start baseline: `make test-pa12` exited 2 with `163/166` paths passing and
all 166 covered. Exact residuals were local extern declaration parsing,
reference-binding pointee-const-pointer lifetime output, and static-cast
overloaded function-template target selection.

Final residual decision: the exact trio is `3/3`, and the fresh through-stage
result is `851/851`. The parser and declarator increment
preserves the local-extern decrement. The lifetime fix emits an action and
synthetic default constructor only for a defined, empty, local block-scope
non-union class object; it does not perform constructor overload resolution.
The template fix selects only typed explicit type-argument targets whose
one-component names bind to global template declarations and direct
type-parameter calls bound likewise; the checked-in target spelling is
unqualified, while a focused out-of-tree global-qualified spelling also passed.
Ordinary lookup and non-template preference remain first.

## Active Checkpoint

The display-path concern is closed by `canonical_type_display_path`: a typedef
or namespace-alias spelling cannot populate or overwrite the record sidecar.
Alias-first probes produce `struct S` and `struct n::S`, matching canonical-use
probes in both encounter orders. Anonymous/generated records do not receive
this path.

The retained implementation additions are: PA10 qualified-declaration routing;
PA11 source-type side storage needed to preserve earlier PA11 array/function
parameter dumps while bindings use adjusted canonical types; typed template
facts, four-state specialization selection, and its exact-key index; the PA12
address-of template-id target path; and the narrow local
implicit-default-constructor lifetime fact. The hot `SemanticFact` no longer
duplicates the specialization ID; cold rendering follows its selected binding
sidecar.

The PA12 README labels general template functions out of scope. The current
fixture requires this deliberately narrow exception; namespace-qualified and
nested same-name template lookup, namespace-alias template lookup,
template-template arguments, non-type arguments, dependent lookup, and
template-aware overload ranking remain out of scope. The checked-in template-id
fixture is unqualified and bound to a global declaration; focused probing of a
global-qualified spelling also succeeds. The excluded qualified/nested probes
reject instead of recovering a name from text.

## Performance Evidence

Parser risk is bounded by the existing token indexes, one contiguous qualified
component scan, one indexed parenthesized-group check, and one charged work
account. The unnamed declarator split is one child pass. The display sidecar
walk is bounded by canonical owner qualification depth and is written once.

Lifetime risk is one block/class-kind/empty-scope check and at most one typed
synthetic constructor fact per record; no overload candidate scan is introduced.
Template risk is limited to the indexed same-name list, visible-scope checks,
function-type traversal for direct `T`, and one exact-key flat-index lookup per
demand. No unrelated scope or source-text scan or linear global specialization
scan is performed.

Representative focused evidence is: exact residuals `3/3`, six nearby PA12
controls `6/6`, two PA10 controls `2/2`, four PA11 controls `4/4`, and
four alias/canonical rendering probes with the expected alias/canonical views. Current
sizes are `SemanticFact 136`, `TemplateSpecializationFact 48`,
`BindingSidecar 48`, and `TemplateSpecializationKey 32` bytes. A fresh
immutable built compiler was run three times at each 4/8/16/32-record scale.
The generated shape has two global templates, one explicit `hello<T>` target
and one direct `take(T)` demand per record, so it has exact `2n` demands and
complete specialization declaration lines:

| distinct records `n` | demands / complete facts | output bytes | triplicate SHA-256 |
|---:|---:|---:|---|
| 4 | 8 / 8 | 3046 | `f9c9eb757b400d36b809884e9c6119abbeb01381a282d2097aa54e17c2db603ea4` |
| 8 | 16 / 16 | 5930 | `a9cd17c294bfa227b440c54a87f4d9be173c72f398b0ef7acf119e68990b85e5` |
| 16 | 32 / 32 | 11752 | `58ab8d80f01744073bf9b102dea2b4672fe308d7be6aebc08f5d57d038a38dcc` |
| 32 | 64 / 64 | 23432 | `4ca4ef49f7baf3d315307cffade765e7650c36ccdad3e3963fb7da5008cb9223` |

These are bounded structural/determinism observations for the generated
semantics dump. Source inspection confirms the exact flat-index path and no
global specialization-fact scan; no timing, RSS, asymptotic, generated-code,
or self-hosting claim is made.

## Checkpoint Ledger

| checkpoint | evidence | outcome |
|---|---|---|
| PA12 start | `163/166`, all 166 covered; PA1-PA11 `685/685` | three exact residuals |
| declaration boundary | local extern fixed; abstract prefix/suffix and parameter adjustment; display owner constrained | retained |
| lifetime/template residuals | reference-binding and static-cast template residuals fixed; exact trio `3/3`; nearby controls `6/6` | retained after scope review |
| architecture correction | four-state facts; typed exact-key index; `SemanticFact` 144 -> 136; immutable 4/8/16/32 scale outputs with exact `2n` counts and stable hashes | focused correction pass |
| final source validation | PA12 `166/166`; through-PA11 `685/685`; fresh through-PA12 `851/851` across `12/12` stages; fresh source-baseline file audit exit `0` with exactly 2 retained `bad-division` warnings at `dev/src/cpp_semantic_core.h:1` and `dev/src/pa11_semantic_model.h:1`; `git diff --check` exit `0`; exact changed-path audit found only the two audit documents | source checkpoint `613b9cb6` and Phase 2 documentation gates complete |

## Validation Status

The Phase 1 focused checks pass: exact residual trio `3/3`, six nearby PA12
controls `6/6`, PA10 `2/2`, PA11 `4/4`, and layout/determinism probes. The
authoritative source baseline passed `make test-pa12` `166/166`, through-PA11
`685/685`, and fresh `make test-report-through-pa12` `851/851` across `12/12`
stages. The fresh file audit exited `0` with exactly two retained `bad-division`
warnings at `dev/src/cpp_semantic_core.h:1` and
`dev/src/pa11_semantic_model.h:1`, and `git diff --check` exited `0`. The exact
changed-path audit found
only `pa12/audit.md` and `pa12/plan.md`; no checked-in tests, refs, harnesses,
grammar, scripts, or source were modified. The pre-commit short status listed
only those two documentation files.

## Completion State

No further PA12 implementation expansion is planned. The final audit,
required validation, and two-file documentation commit are complete. The
commit identity and post-commit clean-tree proof are reported in the handoff
rather than self-referenced in this record.

## Remaining Scope

No current PA12 test residual remains. The documented template bridge remains
global and deliberately narrow; future work may generalize template and class
lifetime semantics only under their later assignment boundaries.

Source checkpoint: `613b9cb6`. The final audit/documentation commit identity and
post-commit clean-tree proof are reported in the handoff; this record does not
self-reference the commit's mutable hash.
