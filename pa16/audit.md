# PA16 checkpoint audit

## Current Checkpoint Review

This review covers the active ordinary, non-template overloaded-operator
checkpoint.  The implementation span began at landed commit
`23a26df5299ef51ed5ff1b419ca7e05888e46e9e`, relative to
`20f14d30c8aa3ee71a2ebdb11a36d1f785d85adc`, and was tightened at the latest
landed commit `2d93a5e90f383652ffd22469620476248d639e8d`, relative to
`23a26df5299ef51ed5ff1b419ca7e05888e46e9e`.  The bounded audit repairs in
this checkpoint are limited to the ordinary operator ownership path in
`pa11_semantic.cpp`, `pa11_semantic_core.cpp`, `pa12_semantic.cpp`,
`pa12_semantic_calls.cpp`, `pa12_semantic_facts.cpp`,
`pa12_semantic_member.cpp`, `pa12_semantic_resolution.cpp`, and the focused
course regression
`cppgm.tests/course/pa16/411-typed-operator-lexical-base-access-regression.sh`;
no handout, fixture, reference, comparator, or generated output was changed.

The contract and exclusions are the PA16 ordinary operator boundary: templates,
class by-value/copy/move/assignment semantics, conversion operators, member
pointers, virtual or multiple inheritance, and unrelated lifetime or global
initialization work remain outside this audit.  The constructor-path support
below is only the narrow implicit construction of a class reference argument
needed by an operator call; it does not open general value construction.

The ownership trace is:

```text
PA10 operator token/name metadata
  -> PA11 typed operator kind/token, canonical BindingId/ScopeId, friend
     relation, declaration-point visibility, and sparse (namespace ScopeId,
     NameId) hidden-friend key
  -> PA12 member + ordinary lookup + ADL/hidden-friend union, deterministic
     candidate identity, implicit-object cv/base/access ranking, enum identity
     and promotion ranking, fallback, converted arguments, one canonical
     CallExpression, and expression-owned bool provenance
  -> PA15 typed call demand and operator ABI lowering, bool materialization or
     value-producing logical behavior, and LowIR call/result
```

### Findings and bounded repairs

- Friend definitions retain their namespace-owned canonical binding and scope,
	while parameter types and the body retain the introducing class as their
	lexical type scope.  PA11 records one typed
	`Function ScopeId -> (class ScopeId, NamedRecordId)` relation only when
	`process_function_definition` sees the in-class friend definition.  The
	ordinary type/value lookup path consumes that exact relation; it never scans
	`BindingSidecar::friend_records`, which remains the access-friend set merged
	across redeclarations.  Direct declarations, inherited type lookup,
	declaration-point filtering, hidden-only visibility, visible redeclaration
	identity, and malformed relation identity all remain typed and fail closed;
	no parent-scope rewrite or whole-scope rescan was added.  Course 411 proves
	that two access friends do not broaden the defining class's nested type/value
	lookup, while the defining friend still accesses the other class's private
	member.
- `conversion_for` now preserves canonical named-enum identity before applying
  integral promotion: an exact enum target wins its matching overload, while a
  different enum or integer-to-enum conversion is nonviable.  Unscoped enum
  promotion retains a worse rank than exact identity and still distinguishes
  the promoted representation from other integral destinations.  A targeted
  PA15 probe caught the first rank formulation's `int`/`unsigned` regression;
  the final formulation restores both unscoped-enum promotion cases and the
  earlier scalar overload behavior.
- Reference binding uses the existing typed qualification and
	`member_object_convertible` access/base machinery.  PA11 retains each
	direct base's parsed `MemberAccess` on the canonical `NamedRecord`; PA12
	checks that fact on the bounded base path with the actual access scope.
	Public edges are always viable, private edges require the edge-owning class
	or its access friend, and protected edges additionally require an enclosing
	or friend class proven by typed single-inheritance ancestry to derive from
	the edge owner.  Unrelated namespace code and malformed, ambiguous, or
	invalid paths fail closed.  Same-class cv is exact while a base path is
	ranked after it; member/nonmember selection therefore uses one typed
	implicit-object comparison.
- Operator candidate discovery unions the relevant member, ordinary-lookup,
  ADL, and hidden-friend candidates without a category shortcut.  Unary,
  postfix, binary, subscript, call, and shift forms retain their operator-kind
  and token metadata.  Nonmember class/enum requirements, hidden-only
  qualified-lookup rejection, visible friend redeclaration reuse, and
  nonviable built-in fallback are checked before one selected call is formed.
- A failed direct conversion to a class reference parameter may use one
  non-explicit, accessible, single-parameter constructor selected through the
  existing constructor index and `Copy` context.  The selected operator's
  argument is then represented by the existing constructor-action/call fact
  path.  Class-by-value constructor parameters, variadic/multi-parameter
  constructors, explicit/deleted/inaccessible constructors, and general
  copy/move/value transfer remain excluded.
- PA12 keeps direct typed bool call/comparison results at their expression-owned
  boundary, while built-in `&&`/`||` still materialize the established logical
  representation and retain short-circuit lowering.  Overloaded logical
  operators remain ordinary calls and are not short-circuited.  Reference
  return chaining and address-of fallback use object types, not references;
  literal constant addresses are recorded at the owning declaration boundary.
- The complete path was checked for deterministic bounded behavior.  The
	hidden-friend lookup is keyed by exact namespace scope/name identity, the
	lexical relation is sparse, candidate vectors are bounded by language-
	relevant declarations, and lookup state is guarded by existing bounded
	traversal.  Base-access ancestry is likewise bounded by the typed scope and
	named-record counts.  There is no rendered-name or function-category
	branching, fixture-specific branch, whole-scope scan, retry lowering,
	incomplete key, unbounded cache, or shortcut in this ownership path.

### Focused and full evidence

The final source build is `make -B -C dev cppgm++`, exit `0`, recorded in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-final-build.log`.
`sh -n cppgm.tests/course/pa16/411-typed-operator-lexical-base-access-regression.sh`
and the course script both exit `0`; the durable course log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-course411.log`.
The final direct focused matrix and its command description are in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-focused-final.log`.
It covers 32 status rows: the original 29-row operator matrix matches `28/29`
(the sole mismatch is the documented `nullptr_t` parser holdout), while the
added public/private/protected and inherited-access controls preserve their
expected statuses.  The complete focused status count is `29/32`, with all
three mismatches being pre-existing documented holdouts.  The semantic rows
also cover member/nonmember cv/base ranking, enum identity and ADL, nested
friend lookup, friend visibility/redeclaration, reference-result chaining,
fallback, logical operators, and shift/string chains.

The full final command `make test-pa16` exited `2`, with
`127/243` passed, `116` failed, and all `243/243` identities covered.  Its
durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-test-pa16.log`.
The exact identity comparison against the authoritative turn-start
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` is in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-identity-compare.log`:
the baseline was `122/243` with `121` failures, baseline-only is exactly
these five repaired identities,

```text
pa16/tests/general/200-inherited-member-overload-set.t
pa16/tests/general/300-basic-operator-overloads.t
pa16/tests/general/300-enum-operator-adl-selects-matching-overload.t
pa16/tests/general/300-hidden-friend-operator-nullptr-compare.t
pa16/tests/general/300-stream-shift-selection-chain.t
```

and final-only is `0`.  Thus the final failure count is no greater than the
`121` no-regression baseline; the five extra passes do not mask a new failure.

The required command
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
exited `0` at `1167/1167`; its log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-through-pa15.log`.
The final file audit exits `0` with only the five known header-division
warnings (`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
`pa11_semantic_model.h`, and `pa15_lowering.h`); its durable log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-file-audit.log`.
The final `git diff --check` log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-diff-check.log`.

### Performance and residual boundaries

The state-matched immutable executable is mode `0555` at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-perf-followup-v5/cppgm++-immutable`.
It byte-matches the final `dev/cppgm++`; both SHA-256 values are
`e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`.
Five interleaved final/immutable rounds for small, large, and same-name-noise
inputs measured whole compiler invocations with `/usr/bin/time`, including
parsing and LowIR output.  Raw inputs and rows are preserved in the `input/`
and `timing.tsv` files; medians are in `medians.tsv`, and structural counts
are in `structure.tsv` under that directory.

| input | lines | target decls | unrelated same-name hidden friends | target expressions | LowIR functions/calls | wall median (range) | RSS median (range) |
| --- | ---: | ---: | ---: | ---: | ---: | --- | --- |
| small | 268 | 2 | 0 | 128 | 131 / 256 | `0.02s (0.02..0.02)` | `9352 (9336..9544) KiB` |
| large | 1046 | 12 | 0 | 512 | 525 / 1024 | `0.11s (0.11..0.11)` | `21932 (21892..22076) KiB` |
| same-name-noise | 1804 | 2 | 256 | 128 | 387 / 256 | `0.08s (0.08..0.08)` | `17668 (17560..17832) KiB` |

The same-name-noise case preserves the target expression/call counts while
adding 256 unrelated same-name hidden friends, which structurally corroborates
exact-key bounded discovery.  These whole-compile timings are representative
evidence, not an isolated phase or timeout proof.

The three direct focused status mismatches are honest pre-existing holdouts:
`pa16/tests/general/300-operator-nullptr-t-from-zero.t`,
`pa16/tests/general/200-private-base-static-cast-member.t`, and
`pa16/tests/general/200-friend-derived-access-inherited-protected-field.t`.
The first is not an operator-resolution failure.  Its reference expects
success, but the final compiler reports `ERROR: unexpected fixed token at
token 13` while parsing the `nullptr_t` declaration, before PA11/PA12
operator lookup.  PA12 already has the typed `nullptr_t` fallback and
integer-zero-to-nullptr conversion; making this declaration parse requires a
PA10 grammar/token repair outside this checkpoint's authorized ownership and
would widen conversion/value semantics.  The other two are unchanged legacy
parser/access controls also present in the turn-start failure map; course 411
separately proves the public/private/protected operator-reference boundary,
including further-derived protected access and external rejection.  The
nullptr classification command, source text, and parser result are preserved
in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-operator-followup-nullptr-classification.log`.
The focused relaxed LowIR presentation differences likewise are not converted
into unsupported standard-coverage claims.

The next checkpoint is a separately authorized PA16 residual audit: first
classify the remaining full-stage identities by ownership, keeping this
ordinary-operator map frozen; only a PA10 `nullptr_t` checkpoint may address
the holdout if PA10 scope is explicitly opened.  Do not advance this path to
PA17 on the unchanged full-stage failure map alone.

## Historical Fixed-Bound Array-Lifetime Checkpoint Review

This historical review was retained from the preceding fixed-bound array
lifetime checkpoint.  Its original content follows unchanged.

This review covers landed commit `0a6be82d9bf17db2585772f2be28d45e6af781de`
(`PA16: add typed array lifetime cleanup`) relative to parent
`5d91986f166e000daddecaf112e0cb58df6a8e8b`, plus bounded audit repairs and
the focused course regression in
`cppgm.tests/course/pa16/410-typed-lifetime-activation-control-exit-regression.sh`.
The scope is fixed-bound local automatic arrays of class objects and recursive
synthesized array-member lifetime: typed array shape, canonical class and
destructor identity, PA12 lifetime/action facts, PA15 recursive construction
and destruction, completed-prefix EH cleanup, lexical/control-exit state, and
LowIR serialization.  Global/static/TLS lifetime and guards, copy/move or
by-value transfer, virtual/multiple inheritance, templates, new/delete, and
unrelated operator/access/temporary machinery remain outside this review.

The authoritative checkpoint-turn-start full-stage state was `93/243` passed,
`150` failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` command exited `2` with log
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-test.log`:
`93/243` passed, `150` failed, and all `243` tests were covered.  The
normalized failure map is exactly the baseline map: `150` identities in each
log, baseline-only `∅`, and final-only `∅`; the `93` passing complement is also
unchanged.  The test inventory contains exactly `243` identities, and the
baseline and final runs each report every identity, so coverage additions and
removals are both `∅`.  The normalized set/count record is preserved at
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-identity-compare.log`.

The affected ownership path is:

```text
PA10 local class-object/array declaration and synthesized-member syntax
  -> PA11 canonical TypeId array shape, NamedRecordId, BindingId, and
     destructor FunctionFact ownership
  -> PA12 automatic LifetimeFact plus ordered ConstructorActionFact and
     DestructorActionFact ranges (base/member order and recursive arrays)
  -> PA15 typed destructor demand, constructor/destructor lowering, checked
     array strides, active-lifetime state, and completed-prefix EH chains
  -> LowIR constructor/destructor calls, lexical/control-exit cleanup,
     eh_try/eh_cleanup/eh_end/resume, and truthful unwind metadata
```

### Findings and bounded repairs

- The original lowering path had a per-function scan of every lifetime fact and
  its scope ancestry to decide whether `goto` must fail closed.  The repair
  adds `index_lifetime_facts()`, called once from `index_binding_facts()`.  It
  builds a dense `ScopeId`-indexed byte flag after walking each lifetime's
  ancestry once.  `lower_function` validates its typed FunctionFact scope and
  performs one O(1) flag lookup; any nontrivial lifetime in that function still
  conservatively blocks `goto`.
- The one-time lifetime index enforces exact typed continuity: the object's
  `Binding.type` equals `LifetimeFact.object_type`; the object is a variable
  owned by the fact's scope; the array/object type resolves to a class record;
  the fact destructor equals `model_.destructor_binding(record)`; and
  `checked_destructor_function` validates the complete destructor FunctionFact
  and action range.  Every scope ID in the ancestry is range-checked, the walk
  is bounded by the total scope count, it must reach a Function scope with a
  valid non-self parent, and malformed or cyclic ancestry fails closed.
  Duplicate lifetime bindings and declaration lifetime ranges are rejected.
- The index's ancestry walk is now in `pa15_lowering_construction.cpp`, keeping
  the affected `pa15_lowering.cpp` under the 3000-line file-audit limit.  This
  is a source-ownership correction, not a behavior change: indexing remains
  once per completed semantic model, with O(S) dense flags and O(L log L) map
  publication for `L` lifetime facts and `S` scopes, plus bounded ancestry
  work `O(sum depth) <= O(L*S)`.
- Constructor/destructor actions remain canonical typed ranges.  PA12 publishes
  base-first and declaration-order member construction, reverse member/base
  destruction, and recursive array actions.  PA15 validates member owner
  bounds and base record identities before lowering an action, rechecks the
  active destructor FunctionFact, and uses typed demand worklists without
  textual recovery.
- Array element paths validate the bound and checked `ordinal * type_size(child)`
  offset before converting the index.  Completed elements retain a typed root
  and path; cleanup recomputes their addresses, so arena growth or later
  LowIR emission cannot invalidate a saved temporary.  The shared prefix chain
  materializes each completed element once and emits one reverse destructor
  call per chain node before transferring to its predecessor and finally
  `resume`.
- Automatic lifetimes activate only after initialization.  Lexical scope
  markers, unbraced substatement cleanup, branch-state restoration, loop
  condition/iteration joins, for-init normal exit, switch-arm recovery, return,
  fallthrough, break, and continue all preserve only the active typed suffix;
  unsupported `goto` remains fail closed.  Destructor-body early return still
  emits remaining base destruction.
- The affected implementation is deterministic and bounded: typed fact/action
  ranges are snapshotted before recursive demand can grow arenas, demand scans
  reachable typed facts once, lowering performs bounded path/layout checks, and
  no reference binary, host compiler, whole-scope retry, or test-specific
  output shortcut is used.

## Focused Evidence

`make -C dev cppgm++` exited `0`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-build.log`.
Syntax checks and focused course controls 408, 409, and 410 all exited `0`;
the durable focused log is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-focused.log`.
Course 410 specifically verifies activation before later declarations,
unbraced if/while/for statement cleanup, for-init normal exit, destructor
early-return base cleanup, loop and switch join state, branch exits, nested
array reverse addressing, and the E=8/16/32 structural scale controls.  Its
exact output was:

```text
PA16 structural flat E=8 cleanup_calls=7 main_lines=129
PA16 structural flat E=16 cleanup_calls=15 main_lines=257
PA16 structural flat E=32 cleanup_calls=31 main_lines=513
```

The full run still reports the four affected-path handout comparison
identities `200-destructor-body-local-before-base-destruction.t`,
`200-local-default-class-array-lifecycle.t`,
`200-member-object-lifetime.t`, and
`300-synthesized-array-member-lifecycle.t` in the unchanged baseline failure
map.  Their checked-in fixtures and references were not changed; no current
pass or failure claim is inferred from a reference-only shape difference.

The exact prior gate command (`n=16` followed by
`make test-report-through-pa15`) exited `0` at `1167/1167`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-through-pa15.log`.
The required file audit exited `0` and reported five existing
header-division warnings; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-file-audit.log`.
The warnings are `abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`,
`pa11_semantic_model.h`, and `pa15_lowering.h`; there were no fatal issues.
`git diff --check` exited `0`; durable log:
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-diff-check.log`.

### Performance Evidence

Course 410's E=8/16/32 cleanup calls are exactly `E-1`, with main-line deltas
`128` and `256`; its nested `[2][3]` control verifies six reverse destructor
calls, outer strides `1,0`, and inner indices `2,1,0,2,1,0`.  These are
structural scale controls, not a timing claim.

The refreshed smoke/scale run used the immutable `0555` executable
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-perf/cppgm++-immutable`
with SHA-256
`be89e2a8efdc723f3e2947f8df48bf00b72333f52750f61dddbc6bd61539ad14`.
For each E, the same generated input was used for five interleaved batches of
20 compiler invocations; `/usr/bin/time` measured the batch and the reported
values are medians and ranges across the five batches.  The complete output,
including current input/output hashes, is
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/pa16-audit-0a6be82-final-performance.log`.

| E | wall median (range) | user median (range) | system median (range) | RSS median (range) |
| --- | --- | --- | --- | --- |
| 32 | `0.09s (0.09..0.09)` | `0.04s (0.04..0.04)` | `0.05s (0.04..0.05)` | `6516 (6448..6564) KiB` |
| 128 | `0.18s (0.18..0.19)` | `0.09s (0.09..0.10)` | `0.09s (0.08..0.09)` | `8500 (8496..8628) KiB` |

The input hashes are E=32 `f37786713c510300b1a9e5884285f7ae4ae7e16a5c1337616a087aac9bf79e54`
and E=128 `c2a66edb4b0088e64e49a70b57dda93c935a0469a853c2a621de72fcc9422c0f`.
Final output hashes are E=32
`67b17d7e3f7b2a3507dd795ed9cd05285dc1050c1eec600d15f92b70a6b16d0b` and
E=128 `cc0554ce1ed562f67be832da79110737001cbf9b96aa40c406960803c3e96399`.
The outputs have main-line counts `513` and `2049`, cleanup nodes/calls
`31/31` and `127/127`; the fourfold element increase gives fourfold main-line
growth and cleanup calls remain `E-1`.  These are representative smoke/scale
measurements, not a benchmark comparison or an allocation claim.

### Next Implementation Checkpoint

PA16 is not complete.  The next implementation checkpoint remains within
PA16: resolve the remaining local automatic/synthesized lifetime reference
shape and semantic cases after separately scoping unrelated PA16 failures.
Global/static/TLS lifetime, value transfer, virtual/multiple inheritance, and
the other exclusions above remain deferred; do not advance this path to PA17
on the unchanged full-stage map alone.

## Historical Static Member-Function Checkpoint Review

This review covers landed commit `021ef63927293f62e13a29b5b8265c7105fb35a9`
relative to parent `15e133af`, plus the bounded audit repairs and one focused
course regression in that checkpoint.  It is limited to typed
static member-function lookup and reachable emission: qualified and
unqualified calls, class/base hiding and overload filtering, access, canonical
owner/binding/type continuity, PA15 demand, declaration/definition emission,
recursion, and the raw static ABI.  Static data storage, constructors and
lifetime, operators/ADL, broad initialization, friends/using, and
multiple/virtual inheritance remain outside that review.

The authoritative turn-start full-stage state was `55/243` passed, `188`
failed, and `243/243` covered, from
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`.
The final `make test-pa16` exited `2` at the same `55/243`, with `188`
failures and `243/243` coverage; sorted failure-identity comparison was exact,
with no additions or removals.

The affected ownership path was:

```text
PA10 qualified/unqualified IdExpression or parenthesized call
  -> PA12 typed qualifier/class-scope and MemberLookup selection
  -> first owning class ScopeId + canonical BindingId + raw function TypeId
  -> static-only candidates without an implicit object, or one mixed
     static/non-static member set when a non-static body supplies this
  -> access check at the selected owner; raw static or hidden-object fact
  -> PA15 namespace roots and reached static bodies walk typed call facts
  -> FunctionFactId identity chooses definition demand or declaration plan
  -> LowIR symbol uses the retained owner and raw source parameters
```

### Findings

- The landed helper correctly resolves a qualified class type through
  `lookup_type_path`, uses `member_lookup` so the first direct/base
  declaration set hides later bases.  With no implicit object, it filters that
  set to static functions; in a non-static member body, current/base-qualified
  and unqualified calls retain both static and non-static functions in one
  typed overload set.  PA12 call facts keep the selected raw callable `TypeId`
  for a static winner and the owner-qualified hidden-object type for a
  non-static winner; the inherited owner remains the selected `ScopeId`/
  `BindingId`.  PA15's `function_binding_fact_index_` then follows that same
  identity into a definition or declaration boundary, and
  `function_components`/`abi_function_symbol` retain the declaring owner.
- The mixed-set comparator follows N3485 §13.3.1 and §13.3.3: a static
  candidate's implicit-object ICS1 matches any object but establishes no
  conversion sequence, so it is neither better nor worse than another
  candidate on that dimension.  Object qualification is therefore compared
  only between two non-static candidates; explicit argument conversion ranks
  remain the common ranking criteria for static/non-static pairs.
- The audit found a fail-open class-qualified case: if the selected class name
  had only a non-static function, a type, a blocked name, or no static
  candidate, the old boolean result could reopen ordinary value lookup and
  manufacture a no-object call.  The repair separates “class-qualified name
  claimed” from the static candidate vector, unwraps parenthesized callees,
  and rejects the claimed spelling when no static target exists.  A valid
  current/base-qualified call in a non-static member body instead leaves the
  class claim to the unified member selector, so a viable static or non-static
  candidate can win without the first category suppressing the other.
- Static-body unqualified lookup now searches nearer block/function lexical
  declarations first, then the enclosing class's typed direct/base member set.
  An inherited static function therefore retains its base owner even when an
  outer namespace function has the same spelling; a direct non-static
  declaration still hides base declarations before static-only filtering in a
  static body.  In a non-static body, the first owning set supplies both
  categories to the same argument/object ranking.  The lookup is bounded by
  the scope vector and never reopens an outer value set after a class member
  has claimed the name.  Parenthesized callees use the same typed branches.
- The landed access gate covered qualified static candidates only.  The audit
  adds the same `member_accessible` check after selection for every
  class-owned static binding, including an unqualified static-body call.
  Protected inherited static access is granted by the derived access-class
  proof without a non-static object relation; private or unrelated access
  fails closed.
- The landed PA15 demand branch treated every indexed static `FunctionFact` as
  a definition.  Static declarations can have a valid fact with no body, so
  the repair validates binding/owner identity and distinguishes a complete
  function/body scope (definition demand) from a bodyless declaration
  (declaration demand).  Missing or contradictory definition facts, scopes,
  body ranges, owner records, and callable types fail closed.  Recursive
  static edges still use the existing visited function/fact worklists and are
  emitted once.  Before traversal, PA15 builds one dense
  `BindingId -> class ScopeId` index from class-scope-owned bindings; duplicate
  or out-of-range ownership fails closed, and each reached static fact checks
  that index in O(1) instead of scanning the owner's values.
- The added course regression covers parenthesized qualified and unqualified
  static calls, qualified non-static rejection, both directions of mixed
  static/non-static overload ranking in qualified and unqualified member
  bodies, tied-explicit-rank neutral-ICS ambiguity in each of those spellings,
  inherited static-body lookup against an outer same-spelled function,
  protected/private access, an inherited declaration-only static call,
  same-binding redeclaration identity, recursive demand deduplication, and
  raw parameter-only static ABI.  The existing handout matrix covers
  static/non-static filtering and qualified inherited owner retention.

## Focused Evidence

`make -C dev cppgm++` exits `0`.  The focused handout command

```sh
make -C pa16 check TEST='tests/general/100-static-member-qualified-call.t tests/general/100-static-member-overload-skips-nonstatic-this.t tests/general/200-inherited-static-member-qualified-call.t tests/general/200-static-nonstatic-same-pointer-signature.t tests/general/100-builtin-prefix-static-member-call.t tests/general/100-member-methods.t tests/general/200-protected-base-method.t'
```

exits `0` with `7/7` passing.  Course regressions 401--406 each exit `0`; 402's
`PA12 inherited member name is not callable` line is from its expected
negative case.  Course 406 independently rejects the qualified and
unqualified tied-explicit-rank mixed calls with `PA12 ambiguous member call`.
`sh -n` on the new script exits `0`.  No handout test, fixture, or `.ref` file
changed.

Five measured invocations of the new bounded regression after the neutral-ICS
repair (including its recursive/inherited static demand chain) took
`0.31--0.34s`, with RSS `7,064--7,268KB`; the seven-test handout probe took
`0.20s` and `9,824KB`.  These are representative smoke measurements, not a
formal benchmark.  The
new lookup performs only bounded lexical/class/base walks, and PA15 performs
one class-binding owner-index setup followed by O(1) selected-owner checks
and dense visited worklists; no whole-program retry, textual recovery, or
generated-artifact dependency was added.

The existing `pa16/tests/general/200-out-of-class-member-default-argument.t`
still fails in that tree before reaching the static declaration audit; that
pre-existing default-argument merge gap was not part of the landed static
ownership increment.  The focused declaration control consequently used a
bodyless static declaration without a default argument.  The required
through-PA15 command exited `0` at `1167/1167`; the PA16 file audit exited `0`
with five pre-existing header `bad-division` warnings.  `git diff --check`
passed before the final commit, and no handout test, fixture, or `.ref` file
changed.

## Historical Protected-Access Checkpoint Review

This review covers landed commit `8b445ee6e7b7090a1f2d19edebbc96d756f438ad`
relative to parent `9f158daa4cf5fd8326123ca9ccded1b4c59df382`, plus one
narrow source repair, one lexical-access repair, and the course regression
expansion in this audit.  It is limited to protected member access-scope
handling for field and method expressions: protected static object spelling
uses only the access-class/owner proof, while protected non-static members
also impose the object-expression rule.  Lookup, owner-path lowering,
constructors/lifetime, friends/using, operators/ADL, and other PA16 failures
are not re-audited here; PA15 is traced only where it consumes the affected
typed facts or reports the existing static projection boundary.
The bounded audit is complete and committed, and final validation leaves the
working tree clean.

The complete owned path is:

```text
PA10 MemberExpression/CallExpression or member-body IdExpression syntax
  -> PA12 typed actual-record lookup over class/direct-base scopes
  -> selected BindingId + owner ScopeId + typed base path
  -> actual object TypeId (this, dot object, or arrow pointee)
  -> member_accessible(binding, owner, access scope, object)
  -> semantic MemberExpression/CallExpression fact with selected owner
  -> PA15 typed owner/path validation and ordered base-subobject projections
  -> LowIR field address or non-static call with the hidden object pointer
```

### Findings

- The landed call-site changes carry the actual object into all three affected
  non-static paths: implicit member fields pass the typed `this` record,
  explicit fields pass the dot record or arrow pointee, and selected calls pass
  the normalized dot/arrow object.  The selected `BindingId` and owner
  `ScopeId` remain the semantic facts consumed downstream.
- `member_accessible` walks the bounded lexical scope chain and records each
  class scope from innermost to outermost.  For protected members it obtains
  each candidate's canonical named `TypeId` from the typed `TypeKey` index and
  accepts the first candidate that derives from the declaring owner with
  `member_base_path` and, for non-static members, also has the actual object
  type derived from that candidate.  This gives a nested class inside
  `Derived` the enclosing `Derived` access rights required by N3485 §11.7,
  while same-owner access still returns at the existing scope boundary and
  private/unrelated access remains rejected.
- For protected non-static members, the second proof strips the supported
  reference/cv layers from the actual object and requires a named record whose
  direct-base path contains the access class.  Thus a `Derived` or
  further-derived object is accepted in a `Derived` body, while a `Base&`,
  `const Base&`, or `const Base*` object is rejected.  The check is identity-
  based and does not render or recover a type name.
- The audit found one narrow exception in the landed helper: the new object
  proof was also applied to protected static members.  The repair returns
  after an eligible access-class/owner proof for a static binding, because
  C++'s additional object-expression restriction is only for non-static
  members.  The existing PA15 static-member projection boundary remains
  outside this checkpoint.
- The nested-class probe initially reached `PA12 record member is
  inaccessible` with only the innermost `Nested` class considered.  A
  constructor-free out-of-class `Derived::Nested` member definition reaches
  the same helper without widening PA15 nested-function emission; the lexical
  candidate walk then accepts its enclosing `Derived` access class for both a
  protected field and method through `Derived&`.  The corresponding `Base&`
  object reduction remains rejected by the second proof.  An inline nested
  call still encounters the pre-existing `PA15 direct call target was not
  emitted` boundary, so it is not used as a lowering claim here.
- The lexical walk is explicitly bounded by the scope-vector size and now
  requires an invalid cursor on exit.  A valid out-of-range cursor or a valid
  cursor left after cycle exhaustion fails closed before any collected class
  can grant protected access; ordinary invalid-parent termination and the
  same-owner return inside the walk are unchanged.
- Access is checked after member-call overload/cv selection, and field/call
  facts retain their selected binding and owner.  The existing semantic tail
  guard rolls back failed member-call probes; the new helper is const and its
  path walks use only local vectors, so failed accessibility does not publish
  a fact, demand edge, or fallback ordinary-name lookup.
- PA15 independently validates the selected actual-object-to-owner relation
  and complete zero-offset layouts before emitting each typed base-subobject
  projection.  This preserves the existing fact continuity from PA12 through
  the field/call LowIR consumers.

## Focused Evidence

`sh cppgm.tests/course/pa16/405-protected-object-access-regression.sh` exits
`0`.  Its positive source covers same-owner access, implicit and qualified
`this`, explicit dot and arrow on `Derived`, const-reference and pointer
normalization, dot and arrow on a further-derived object, and both field and
method paths.  It checks the expected typed projection counts and
`@Base__protected_method` calls.  Its constructor-free nested `Nested` member
source accepts both protected field and method access through `Derived&`; the
parallel nested `Base&` source returns `EXIT_FAILURE` with the exact PA12
inaccessible diagnostic.  Its separate ordinary field-through-`Base&` and
method-through-`const Base*` sources also return `EXIT_FAILURE`.

The checked-in protected positive control
`make -C pa16 check TEST='tests/general/200-protected-base-method.t'` exits
`0` with `1/1` passing.  The existing course controls
`401-typed-member-projection-boundary-regression.sh`,
`402-typed-member-call-demand-roots-regression.sh`,
`403-typed-inherited-member-field-regression.sh`, and
`404-typed-implicit-default-demand-regression.sh` each exit `0`; 402's
`PA12 inherited member name is not callable` line is the expected diagnostic
from its negative reduction.  No handout test, fixture, or `.ref` file was
changed.

The permanent course-405 protected-static object-spelling source returns
`EXIT_FAILURE` at the pre-existing `PA15 static member projection is
unsupported` boundary, rather than at `PA12 record member is inaccessible`;
this verifies that the access gate no longer imposes the non-static object rule
on static bindings without expanding the static lowering surface.

The turn-start authoritative log records full-stage PA16 at `49/243` passed,
`194` failures, and `243/243` covered.  The authorized final `make test-pa16`
also exits `2` at `49/243`, with `194` failure identities; sorted identity
comparison against the turn-start log gives added `∅` and removed `∅`, and
the `243`-test inventory remains fully covered (`243/243`).  The earlier
pre-increment history is preserved: the plan records the `48/243` to `49/243`
improvement from removing `pa16/tests/general/200-protected-base-method.t`.
The required through-PA15 command exits `0` at `1167/1167`.  The required
file audit exits `0` with the same five existing header `bad-division`
warnings; no handout fixture or `.ref` file changed.

## Performance and Boundaries

Protected access performs one bounded lexical scope walk of depth `S`, records
`L` class candidates, and performs at most two typed direct-base walks of depth
`D` per candidate; its worst-case check is `O(S + L*D)` (with the ordinary
non-nested case `L=1`).  Same-owner access returns before a base walk, and
protected static access can short-circuit after the owner proof.  Selection
remains bounded by the walked scope/inheritance depths and candidate set; the
new check adds no cache, whole-program retry, textual recovery, or mutation on
a failed probe.  PA15 performs one independent typed owner/layout check.

The representative temporary three-level state-free chain timing sample used
five compiler invocations per size with `/usr/bin/time`: for 1, 128, and 512
local declarations, maximum elapsed times were respectively `0.02s`, `0.01s`,
and `0.02s`; maximum RSS was approximately `5.4MB`, `6.1MB`, and `8.7MB`.
A separate temporary nested-access sample placed 256 protected-field
expressions in one out-of-class nested member and used lexical class depths
`L=1`, `8`, and `32`, with three invocations per depth; all exited `0`, with
maximum elapsed time `0.01s` and maximum RSS `7.4MB`.  These are small
bounded-behavior samples, not formal benchmarks or asymptotic timing claims.
Remaining uncertainties are the pre-existing static and inline-nested-call
lowering boundaries, and unrelated protected typedef/friend/using and
broader PA16 surfaces.

## Historical Previous Checkpoint Review

This review covers landed commit `b1a9e58959cb47835362a654283200831e7b99d6`
relative to parent `25e80541`, plus four narrow audit repairs included in
this checkpoint.  It is limited to direct and inherited unqualified
non-static member calls.  Inherited fields, qualified-base calls,
protected/friend/using access, operators/ADL, constructors/lifetime, virtual
or ref-qualified methods, and general conversion work remain outside it.

The owned path is:

```text
PA10 CallExpression(MemberExpression or unqualified IdExpression) syntax
  -> PA12 typed lexical/class/direct-base lookup and member selection
  -> exact Function-scope implicit-object BindingId as semantic child zero
  -> selected BindingId, owner ScopeId, callable Function TypeId, and args
  -> reachable FunctionFact demand edge
  -> PA15 ABI/owner validation and ordered base-subobject projections
  -> LowIR call with the owner pointer followed by explicit arguments
```

### Findings

- `semantic_call_expression` probes the typed member path before functional
  casts and ordinary direct lookup.  The probe unwraps supported
  parenthesized callees, accepts only a plain unqualified id for the new path,
  and never asks namespace lookup or ADL for member candidates.  Its lexical
  walk checks nearer block/function declaration sets, then the direct class
  and ordered direct-base declaration sets.  At every set the value graph is
  probed before the type graph, so a same-scope ordinary method hides a
  same-spelled class/enum tag.  A value-owned class/base set suppresses
  unrelated enclosing candidates.  A base-owned value set with no supported
  non-static method is blocked by its nonempty typed base path, while a
  `ValueRef` origin from an unsupported import returns explicit `Blocked` and
  cannot silently reopen outer value lookup; nearer lexical/direct-class values
  retain the ordinary resolver's existing fallback.  A using-view remains with
  the ordinary resolver.  A type-only first set returns an explicit typed `TypeId` outcome and is
  consumed by the existing functional-cast producer, so it cannot silently
  reopen outer value lookup.  The separate type probe uses a fresh lookup
  generation after the value probe.
- `member_function_candidates_in_scope` retains only ordinary non-static
  functions from the selected class scope.  The selected `BindingId` and
  `ValueRef` owner remain canonical; the callable `Function TypeId` is built
  with the selected owner and its cv-qualified hidden object pointer.  Object
  qualification is checked before the existing explicit-argument conversion,
  default, overload, access, and deleted-function logic.  Equal best choices
  remain ambiguous rather than depending on traversal order.
- `prepare_pa12_member_parameter` owns one synthetic first parameter and its
  exact `BindingId` in the member Function `Scope`.  The unqualified helper
  now passes the already-validated `BindingId` into `semantic_this_expression`,
  rather than resolving the enclosing `this` binding a second time.  Thus the
  successful call has one stable implicit-object fact at child zero, followed
  by converted/defaulted explicit arguments.
- Inherited fields remain deferred, but an inherited value declaration set is
  still owned by the first base scope that contains the spelling.  Empty
  non-static member candidates and unsupported imported-base value origins are
  therefore blocked by the typed base ownership signal and fail closed;
  ordinary outer/ADL lookup cannot be reopened.  A nearer lexical or
  direct-class value still reaches the existing ordinary resolver, preserving
  its direct/static behavior.
- `direct_base_chain` walks `NamedRecordId` edges with Floyd cycle detection.
  The audit repair validates every class-scope back-reference, rejects a
  non-invalid base on `has_base == false`, rejects virtual and union base
  metadata, and preserves the existing single-direct-base parser rejection.
  The semantic path is bounded by inheritance depth and is passed to the
  shared selector without a second semantic walk; same-owner conversion stays
  constant-time.
- A successful typed member call is the only member demand edge.  PA15 follows
  its selected binding through the existing `FunctionFactId` index, validates
  the selected class owner and hidden ABI, and plans declaration-only members
  from the typed callable boundary.  It scans reachable facts once with typed
  worklists; failed guarded probes publish no fact or demand edge.
- `lower_call` independently reconstructs the actual-object-to-owner path as
  a safety check.  For every edge it validates the current class relation and
  a complete `RecordLayout` whose direct base is the expected record at offset
  zero, then emits ordered `IPK_BASE_SUBOBJECT` projections.  Dot takes one
  address and arrow one pointer expression; free and indirect calls retain
  their existing lowering paths.

### Historical Focused Evidence

The six-test handout probe
`make -C pa16 check TEST='tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-implicit-member-call-suppresses-adl.t tests/general/200-member-call-implicit-this-cv-overload.t tests/general/200-local-class-direct-init-inherited-member-call.t tests/general/200-parenthesized-member-call.t tests/general/200-single-inheritance.t'`
exits `2` with `1/6` passing.  The inherited outer-type control passes.  The
five remaining failure identities are unchanged prerequisite blockers:
`200-implicit-member-call-suppresses-adl.t`,
`200-member-call-implicit-this-cv-overload.t`,
`200-local-class-direct-init-inherited-member-call.t`,
`200-parenthesized-member-call.t`, and `200-single-inheritance.t`.

The focused control
`make -C pa16 check TEST='tests/general/100-member-methods.t tests/general/200-inherited-member-call-hides-outer-type.t tests/general/200-member-call-return-type-overload-arity.t'`
exits `0` with `3/3` passing.  The existing course regressions
`400-typed-layout-boundary-regression.sh`,
`401-typed-member-projection-boundary-regression.sh`, and
`402-typed-member-call-demand-roots-regression.sh` each exit `0`.
The extended 402 script asserts typed LowIR ownership for a base tag/method
collision (zero-offset projection and `@Base__f`) and for direct and inherited
type-only first declaration sets (typed zero cast in `Derived__call`, with no
outer `@f` call).  It also rejects an inherited `Base::f` data member in
`Derived::call` and confirms that no outer `@f` call is emitted.  `sh -n` over
those scripts and `git diff --check` also exit `0`.

Bounded stdin reductions (not additional suite coverage) compile successfully
for direct, inherited, and parenthesized unqualified calls.  The inherited
and parenthesized outputs each contain one zero-offset
`projection=base_subobject` before the base call; a three-level reduction
contains two ordered projections before `@A__f`.  The new 402 reductions show
that a same-scope tag does not hide an ordinary base method, while direct and
inherited type-only declarations return the typed functional-cast zero rather
than calling the unrelated outer function.  The inherited non-callable-value
reduction fails closed rather than reaching the outer function.  A nearer block
variable named like
the method still fails at the local non-callable target and does not fall
through to the base method.

The required through-PA15 command
`n=16; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi`
exits `0` with `1167/1167` passing.  The required file audit
`perl scripts/cppgm_file_audit.pl --stage pa16 --paths dev/src` exits `0` with
these five warnings: `dev/src/abi_mangle.h:1`,
`dev/src/cpp_semantic_core.h:1`, `dev/src/lowir_model.h:1`,
`dev/src/pa11_semantic_model.h:1`, and `dev/src/pa15_lowering.h:1`, each
`bad-division` for a substantial implementation body in a header.
`make test-pa16` exits `2` with `48/243` passing, `195` failures, and
`243/243` coverage.  Comparing the exact failure identities with the
turn-start map in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` gives
added set `∅` and removed set `∅`; all `195` identities are unchanged.  No
fixture, reference, or coverage identity was changed.

### Historical Performance and Boundaries

The lexical/type/value probes inspect only the walked scopes.  Base metadata
validation and lookup are bounded by inheritance depth; same-owner conversion
returns before a base walk.  For `C` candidates, `A` explicit arguments, and
`P` parameters, member viability/ranking remains approximately
`O(C * (P + A))`.  PA15's typed function/fact worklists scan each reachable
node once, and lowering performs one independent bounded path/layout check.
No whole-program retry or textual recovery was added.

No timing, RSS, allocation, or structural-counter measurement was collected;
these are structural bounds only.  Unsupported inherited type construction
and inherited non-callable value calls are fail-closed, while the focused
scalar type-only cases use the existing functional-cast producer.  The
remaining uncertainties are the unchanged
`195`-identity PA16 failure map and the explicitly deferred
protected/friend/using, inherited-field, qualified-base, static,
constructor/lifetime, virtual/ref-qualified, operator/ADL, and broader
conversion slices.

## Audit ledger

| checkpoint | result and disposition |
| --- | --- |
| `fb4348b6` typed parameterized class-constructor checkpointAudit | Complete: bounded PA10--PA15 constructor audit repaired canonical hidden-destination callable typing, protected-constructor access, shared candidate owner validation, and aggregate copy/direct-list dispatch for explicitly-defaulted/deleted constructors. The focused constructor matrix is `17/17`; course controls 400--409 pass with syntax checks, including new self-pointer/protected/private and aggregate field/helper coverage. The two aggregate handout controls retain the known LowIR address/bool shape comparison difference. Final PA16 is `91/243` with `152` failures and `243/243` coverage; failure and coverage identity additions/removals are both `∅`/`∅`. Through-PA15 is `1167/1167`; the file audit passes with five pre-existing warnings; diff-check passes; representative scale smoke is recorded above. No handout, fixture, reference, or `.ref` changed. |
| `32c45463` typed class-object construction checkpointAudit | Completed bounded audit of the landed typed construction increment relative to `a2ac5256`: repaired canonical empty named-class constructor identity, fail-closed FunctionFact ownership, value-initialization zeroing semantics, aggregate DMI fallback, typed range/owner/index validation, demand-driven empty-helper elision, and the course-404 ordering controls. Focused copied handout comparison is `10/11`; course controls 400--407 are green; final PA16 is `80/243` with `163` failures and `243/243` coverage, with exact failure and coverage additions/removals `∅`/`∅`; construction stress smoke is five successful `0.00s` runs with RSS `5824--6056KB` (timings in `/tmp/codex-pa16-stress-final.Tn9MSH/stress-1.time` through `stress-5.time`), 14 constructor helpers, 14 constructor calls, 13 base projections, 45 field projections, and 45 stores. Through-PA15 is `1167/1167`; the file audit passes with five pre-existing header-division warnings; no handout, fixture, reference, or `.ref` changed. |
| `2f130396` typed static-data storage/access checkpointAudit | Completed bounded audit/repair: canonical direct class-owner merging, inherited/nested typed owner retention, initializer-fact preservation, demand-aware class-static/TLS emission, access checks, exactly-once static object evaluation, and PA12 fail-closed class claims are traced and repaired. `make -C dev cppgm++`, course controls 400--407, the focused probe, and exact through-PA15 gate pass their bounded criteria; full PA16 is `61/243` with `182` failures and `243/243` coverage, with failure-identity additions/removals `∅`/`∅`. The file audit exits `0` with five pre-existing warnings; no handout or reference changed. |
| `021ef639` typed static member-function lookup/reachable-emission checkpointAudit | Completed bounded audit/repair: class-qualified lookup fails closed, current/base-qualified and unqualified member-body calls rank one mixed static/non-static set, static-body lookup preserves inherited owner/hiding, access and raw-vs-hidden-object facts remain typed, and PA15 uses a dense class-binding owner index with O(1) selected-owner checks. The focused handout matrix is `7/7`, course controls 401--406 exit `0`, final PA16 is `55/243` with the exact turn-start `188` failure identities and `243/243` coverage, through-PA15 is `1167/1167`, and the file audit passes with five pre-existing warnings. |
| `8b445ee6` protected object access scope checkpointAudit | Completed and committed bounded audit/repair: static protected object spelling stops after the typed access-class/owner proof, nested protected access considers eligible enclosing class scopes while retaining the non-static object proof, and malformed valid scope ancestry fails closed after the bounded walk. Course 405 covers the field/method matrix, nested `Derived&`/`Base&` controls, and the exact existing PA15 static boundary. Final PA16 is `49/243` with `194` failures and `243/243` coverage, with exact failure additions/removals `∅`/`∅`; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and the final working tree is clean. |
| `b1a9e589` direct + inherited unqualified member-call checkpointAudit | Bounded audit completed with four narrow fixes: exact synthetic-`this` BindingId reuse, value-before-type lookup with explicit `Type`/`Blocked` outcomes, fail-closed direct-base metadata validation, and inherited value-set ownership blocking. Direct/inherited/parenthesized reductions, the three focused controls, and course regressions pass; the six-test handout probe remains `1/6` on the same five prerequisite identities. Through-PA15 is `1167/1167`, the file audit exits `0` with five pre-existing warnings, and full PA16 is `48/243` with `195` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `37265733` typed member projection audit/repair | Direct/nested dot and arrow ownership is traced through PA12, PA11 `RecordLayout::member_offsets` keyed by the object's canonical `NamedRecordId`, and PA15 LowIR; the reference-cv and class anonymous-injection defects are repaired. Broad validation and exact identity/coverage checks pass their bounded invariants; PA16 remains incomplete with the existing 205 failures. |
| `0b534f2f` typed direct member-call checkpointAudit | Completed bounded audit/repair: implicit-object cv subset ranking, N3485 variadic comparison, single-owner typed reachable member demand, dense PA15 reachability metadata, declaration-only member declarations with hidden-object/cv ABI boundaries, hidden-object call formation, and source-file sizing are repaired. Focused PA16/PA15 controls and all relevant course regressions pass; through-PA15 is `1167/1167`, the file audit passes with five pre-existing warnings, and full PA16 remains `47/243` with `196` failures and `243/243` coverage, with zero failure-identity additions or removals. |
| `0a6be82d` typed fixed-bound local/synthesized array lifetime checkpointAudit | Completed bounded audit/repair: typed lifetime ownership and destructor continuity are validated once, dense `ScopeId` flags replace the former per-function lifetime scan, checked array paths/actions and arena-safe recursive cleanup are retained, and lexical/control-exit/EH state is covered by course 410. Final PA16 is `93/243` with the exact turn-start `150` failure identities and `243/243` coverage; through-PA15 is `1167/1167`; the file audit passes with five existing warnings; diff-check passes; current structural and interleaved smoke/scale evidence is recorded above. |
| `2d93a5e9` ordinary non-template overloaded-operator checkpointAudit | Completed bounded audit/repair of the `20f14d30` -> `23a26df5` implementation span as tightened at `2d93a5e9`: the follow-up corrects exact friend-definition lexical ownership and typed private/protected/public base-reference accessibility while retaining enum identity/promotion ranking, narrow converting-constructor participation, reference/address facts, and typed bool boundaries through PA10--PA15. Final PA16 is `127/243` with `116` failures and `243/243` coverage; exact comparison to the `122/243` turn-start map has five baseline-only repaired identities and zero final-only identities. Through-PA15 is `1167/1167`, final file audit has five known warnings, focused status is `29/32` with three documented pre-existing holdouts, course 411 passes, and state-matched performance is in `pa16-operator-perf-followup-v5` with final/immutable SHA-256 `e5ffb4e9869c619552f193e16ef063ab2feba7c27f809887ebdd187960196580`. No handout, fixture, reference, comparator, or generated output changed. |
