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

## Spec Alignment

- §§1-2: one forward model and typed fact continuity; display strings are
  rendered only at the requested PA12/PA11 dump boundary.
- §3: template candidates are indexed by typed `NameId`; explicit arguments,
  direct `T` deduction, specialization keys, and selected bindings remain
  typed. Every complete specialization key has explicit not-started,
  in-progress, complete, and failed states; failed keys are retained and do
  not repeat substitution. The narrow bridge covers only the demanded
  `hello<stream>` target and `take(T)` call; no general template-aware overload
  machinery is added.
- §4: parser work is indexed and charged once; candidate work is limited to
  the relevant same-name template list and visible scopes; specialization reuse
  performs one typed flat-index lookup on the function ID plus exact `TypeId`
  argument vector, rather than scanning prior facts.
- §7: focused exact-output checks, deterministic alias-order probes, broad
  stage totals, file audit, and diff/path checks are recorded below. Timing and
  work statements are bounded observations, not unsupported asymptotic claims.

## Failure Map

Turn-start baseline: `make test-pa12` exited 2 with `163/166` paths passing and
all 166 covered. Exact residuals were local extern declaration parsing,
reference-binding pointee-const-pointer lifetime output, and static-cast
overloaded function-template target selection.

Current focused decision gate: exact residual trio `3/3`. The reviewed parser
and declarator increment preserves the local-extern decrement. The lifetime
fix emits an action and synthetic default constructor only for a defined,
empty, local block-scope non-union class object; it does not perform constructor
overload resolution. The template fix selects only typed explicit type-argument
function-template targets and direct type-parameter calls; ordinary lookup and
non-template preference remain first.

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
fixture requires this deliberately narrow exception; general template
semantics, template-template arguments, non-type arguments, dependent lookup,
and template-aware overload ranking remain out of scope.

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

Representative focused evidence is: exact residuals `3/3`, six nearby
PA12 controls `6/6`, and three alias/canonical rendering probes with matching
canonical paths. Before/after sizes are `SemanticFact 144 -> 136`,
`TemplateSpecializationFact 40 -> 48`, and unchanged sparse `BindingSidecar
48`; the typed lookup key is 32 bytes. An immutable built compiler was run on
equivalent generated inputs with 4/8/16/32 distinct types and 8/16/32/64
specialization demands. Each scale emitted exactly the demand count, with
three identical output hashes per scale: `c67642f7b400d36b809f75dd327f9f3a29a4fd0f7f53336b1dc5b9d2a09f6fb7`,
`b6f7d363f10191c9fad9d0680dd1f4f1a968c0480a8415dab249360e085b65d2`,
`9891c0451a0aceb90ebb336b7379fcfed79c63aebc30706030d59a311b8fb587`,
and `4964386a9dc043005b73951f4486988283c801acd3f2c35bc0e6f9741de0897`.
Thus the direct structural count is `2n` exact-key lookups and `2n` retained
facts, with no global-vector scan; no timing/asymptotic claim is made.

## Checkpoint Ledger

| checkpoint | evidence | outcome |
|---|---|---|
| PA12 start | `163/166`, all 166 covered; PA1-PA11 `685/685` | three exact residuals |
| declaration boundary | local extern fixed; abstract prefix/suffix and parameter adjustment; display owner constrained | retained |
| lifetime/template residuals | reference-binding and static-cast template residuals fixed; exact trio `3/3`; nearby controls `6/6` | retained after scope review |
| architecture correction | four-state facts; typed exact-key index; `SemanticFact` 144 -> 136; immutable 4/8/16/32 scale outputs with exact `2n` counts and stable hashes | focused correction pass |
| final validation | PA12 `166/166`; through-PA11 `685/685`; through-PA12 `851/851`; audit green with 2 retained warnings; path/diff checks clean | complete |

## Validation Status

The architecture correction focused check passes `6/6`, including the exact
residual trio. The generated untracked PA10 check artifacts created during
diagnosis were removed explicitly; no checked-in tests, refs, harnesses,
grammar, or scripts were modified. Final `make test-pa12` passed `166/166`,
the exact through-PA11 command passed `685/685`, and
`make test-report-through-pa12` passed `851/851`. File audit passed with the
two retained header-division warnings; `git diff --check` and the commit-level
changed-path audit passed, and the 3000-line PA10 AST limit remains satisfied.

## Next Checkpoint

No further PA12 implementation expansion is planned. This corrected PA12
checkpoint is amended and the worktree is clean.

## Remaining Scope

No current PA12 test residual remains. Future work may generalize template and
class lifetime semantics only under their later assignment boundaries.

Final commit: corrected PA12 residual checkpoint; ID is reported in the handoff.
