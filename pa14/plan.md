# PA14 Checkpoint Plan

## Stage Design

`dev/abimangle.cpp` is the line-oriented adapter.  It validates the normalized
fact vocabulary and constructs one typed `AbiFactCase`; it does not pass
rendered ABI fragments to semantic code.  `dev/src/abi_mangle.h` is the shared
typed fact boundary, and `dev/src/abi_mangle.cpp` is the sole append-based
encoder.

The encoder owns deterministic per-case substitution state.  A
`StructuralKey` contains a typed domain/kind, scalar operands, interned source
components, and canonical child identities.  Definition identities are cached
once, recursive definitions are cycle-checked, and a map interns complete
keys.  Qualified-name edges use an interned trie; each trie node stores the
typed identity formed from its parent identity and one edge, so prefix
registration never copies a growing component vector.  Tagged names use
sorted typed tag IDs.  No rendered mangled spelling is used as a semantic key,
and there is no opaque index reservation path
(`add_opaque_substitution_candidate` is gone).

The 300 boundary registers complete typed candidates for compound types,
template IDs and prefixes, function-template prefixes, member-template
entities, values, standard substitutions, and external entity addresses.  CV
wrappers flatten adjacent const/volatile nodes for identity and emission;
named definition references and direct named/reference spellings share the
same structural identity.  Member-template candidates include their typed
owner and member rather than an arbitrary unqualified component.  An
owner-position name-template publishes its complete typed template-prefix
before its operands and its complete specialization after them; the latter
matches the corresponding typed class specialization for the one-owner mixed
300 model.  Multiple template owners use a complete composed key rooted in
the prior specialization, preserving earlier owner arguments without making
a flattened cross-domain alias.  An ordinary template name, whether qualified
or unqualified, uses one dedicated typed template-entity/prefix identity for
the owner-position prefix and a template-entity argument; that identity is
distinct from the generic name trie, so unrelated unqualified components do
not become candidates.  Entity symbol encoding uses an isolated
nested substitution state, so a member's internal `C::m` spelling cannot
pollute the enclosing function's substitution order.
Entity-symbol isolation swaps the trie/index state object with O(1)
vector/map swaps and restores it on success or exception.  Ordinary path type,
result, and variadic operands remain typed operands; only path template
operands become function template arguments.  A result type is emitted only
for an identified function-template encoding.

The current dependent boundary extends that pipeline instead of adding a text
fallback.  The adapter maps every checked-in `let-expr` form to typed operator,
cast, access, source-name, type, expression-reference, and argument fields,
with explicit arity, operator-shape, and vocabulary validation.  Member-template type records
retain their owner, member source component, and argument range; decltype types
retain the Dt/DT category.  Expression identity includes all such fields and
typed children, including literal type/value, trait operands, member owner,
access mode, and close-owner state.  Encoding is a direct prefix traversal;
type operands share the case substitution state, while direct template
parameter expression leaves do not create a symbol-table candidate.  Recursive
expression and definition walks are cycle-checked, and no rendered expression
fragment is used as identity.  The public fact serializer emits canonical,
parseable typed forms for these 400/500 records, including expression
definitions, expression template arguments, member/member-template owners,
decltype category, and function-template records.

Name/template operands are published in left-to-right ABI order: owner
template-prefix, its operands, then complete specialization.  The pending
function-template prefix is published only at the explicit function-template
argument emission site, never while walking owner or nested argument lists;
final function/operator names remain excluded from ordinary name candidates.
Mixed plain/template owner components are walked as one ordered sequence; a
plain suffix after a template owner is not sent through a disconnected scope
prefix.  Standard-substitution enums own reusable-encoder identity and
emission; raw `Sa`/`So`-style text is adapter-only validation metadata.  Empty
argument-list restrictions were not added as fixture-driven policy.  The
normal work is plausibly O(n log n) for ordinary distinct interned facts/edges
(with O(q log q) tag canonicalization): every qualified edge is interned once,
and each structural map operation is logarithmic.  Structural key comparison
still carries the size of its typed operands, so this is not a claim for
arbitrarily wide keys or later families.  Entity isolation adds O(1) state
swaps per nested symbol.  There are no whole-case rescans or quadratic
rendered/vector keys.

## Failure Map

The prior typed-300 checkpoint began at HEAD `12eaf37b`: all 111 tests were
covered, with 57/111 passing and 54 failing.  Its authorized result was
88/111, with 100=25/25, 200=25/25, 300=37/37, 400=1/4, 500=0/13, 600=0/7.
The seven baseline 300 passes were construction-vtable,
distinct-array-bound-substitution, minimum-signed-integral-value,
std-allocator-substitution, std-initializer-list-member-parameter,
std-initializer-list-parameter, and template-template-argument.

At this turn start, clean HEAD `623abbe4` had that same 88/111 result and
exactly 23 listed failures.  Final validation reports 104/111: 100=25/25,
200=25/25, 300=37/37, 400=4/4, 500=13/13, and 600=0/7.  The focused checked-in
400/500 command is 17/17, so all three 400 and thirteen 500 turn-start
failures are resolved without reducing coverage.  The public serializer
round-trip harness also passes all 17 400/500 inputs with identical mangles,
plus a `decltype-id` smoke case.

The complete pre-milestone failure set was:

- 400: `dependent-alias-type-id`, `dependent-owner-member-template`,
  `dependent-rebind-other`.
- 500: `dependent-bitset-words`, `dependent-call-expression`,
  `dependent-cast-expression`, `dependent-expression-type-substitution-order`,
  `dependent-function-parameter-decltype-param`,
  `dependent-object-member-expression`, `dependent-pack-expression`,
  `dependent-sizeof-type-expression`, `dependent-type-trait-expression`,
  `distinct-integral-decltype-substitution`,
  `distinct-type-trait-expression-substitution`,
  `equivalent-dependent-expr-substitution`,
  `equivalent-integral-decltype-substitution`.
- 600: `function-local-class-template-arg`, `function-template-local-class-arg`,
  `function-template-local-lambda-arg`, `inline-namespace-basic-string-param`,
  `nested-helper-owner`, `template-param-template-type-substitution`,
  `template-parameter-pack-reference-constructor`.

The final prior-stage gate is `make test-report-through-pa13` at 947/947.
`make test-pa14` covers all 111 tests and reports the seven exact 600
nonclaims below.  The PA14 file audit passes with the four known nonfatal
header `bad-division` warnings; `git diff --check` also passes.

## Active Checkpoint and Spec Alignment

This checkpoint now actively owns the checked-in dependent 400/500 boundary:
dependent aliases and member-template owners; typed dependent expressions;
operator trees; casts and calls; type traits and sizeof(type); packs; object
and unresolved-member forms; function-parameter references; decltype category;
and equivalent-expression substitution.  The prior typed 300 family remains
the preserved foundation.

The implementation follows spec.md §§1--4 and §7 by keeping one typed
adapter/model/encoder pipeline, validating malformed fact combinations at the
adapter boundary, representing symbolic substitutions with complete typed
keys, and encoding ABI structure directly in left-to-right order.  It follows
Itanium Chapter 5.1 for dependent types and decltype, function-parameter
references, template arguments, prefix expression traversal, unresolved names,
and compression.  Direct expression leaves remain distinct from type
operands: the latter publish candidates in grammar order, while expressions
themselves are never string-keyed or substituted as rendered text.  The
prior 300 substitutions and multi-owner identity remain preserved; the
remaining 600 local/lambda, inline-namespace, and pack/template-parameter
cases are explicit nonclaims.

The seven 600 identities remain explicit nonclaims for this milestone:
`function-local-class-template-arg`, `function-template-local-class-arg`,
`function-template-local-lambda-arg`, `inline-namespace-basic-string-param`,
`nested-helper-owner`, `template-param-template-type-substitution`, and
`template-parameter-pack-reference-constructor`.  Member-external raw symbols
remain a deliberate external-symbol boundary; this work does not reconstruct
or cross-check them from typed owner/member facts.  No handout fixture,
reference, generated artifact, or harness was changed.  The 600 nonclaims are
unchanged; no shared 400/500 fix is being claimed for them.

## Performance Evidence

Final dependent-expression evidence used immutable candidate
`/tmp/abimangle-pa14-candidate`, SHA-256
`691ba38a34eafc26424b6512f3bd1da5cd8933b63d3d89e5e30df28763437e86`, with
seven interleaved runs per row (`/usr/bin/time -f '%e %U %S %M'`) and median
wall/user/sys/max-RSS results.  The chain has one final expression selected
from a 256/512/1024-deep unary chain.  The shared DAG has one shared leaf,
256/512/1024 unary nodes that each reference it, and one final call listing
all nodes; its output is deliberately linear rather than an exponential
recursive fan-out.

| workload | facts | expression nodes | output bytes | median wall | median user | median sys | median max RSS |
|---|---:|---:|---:|---:|---:|---:|---:|
| chain-256 | 259 | 257 | 526 | 0.00 s | 0.00 s | 0.00 s | 6364 KiB |
| chain-512 | 515 | 513 | 1038 | 0.01 s | 0.00 s | 0.00 s | 8828 KiB |
| chain-1024 | 1027 | 1025 | 2062 | 0.02 s | 0.01 s | 0.01 s | 13948 KiB |
| shared-256 | 260 | 258 | 1039 | 0.00 s | 0.00 s | 0.00 s | 6408 KiB |
| shared-512 | 516 | 514 | 2063 | 0.01 s | 0.00 s | 0.00 s | 8836 KiB |
| shared-1024 | 1028 | 1026 | 4111 | 0.02 s | 0.01 s | 0.01 s | 13976 KiB |

The 1024-deep recursive walk is safe in this bounded check.  Structural
definition/expression identities are memoized in typed maps, but encoded
expression occurrences are intentionally re-emitted; the shared workload's
larger output measures that unavoidable output-sensitive cost.  This is
evidence for these representative linear inputs only, not a claim of
arbitrary-DAG linearity or unlimited recursion depth.  The 0.01 s timer
resolution limits fine-grained conclusions at 256 nodes.

Prior typed-300 checkpoint evidence used an immutable mode-0555 candidate
copied before measurement; it was 405800 bytes with SHA-256
`7bb13fa11ae1c0cac9d69a17e01df57552e84037873c6e971c852d6b3121299d`.
Generated equivalent inputs used a 64-component common qualified prefix and
one distinct `HolderN` template specialization per scale.  Each specialization
contains the same nested entity-function address and is passed as one function
parameter, so the run exercises deep prefix reuse, nested symbol isolation,
and repeated entity-valued arguments without changing checked-in fixtures.
Seven runs per size were interleaved in the order 512, 1024, 256 with
`/usr/bin/time -f '%e %U %S %M'`; the table reports medians.

| input | bytes / fact lines | median wall | median user | median sys | median max RSS | wall ratio vs previous |
|---|---:|---:|---:|---:|---:|---:|
| 256 | 273,040 / 1,792 | 0.14 s | 0.12 s | 0.01 s | 15,596 KiB | -- |
| 512 | 546,960 / 3,584 | 0.28 s | 0.25 s | 0.03 s | 27,340 KiB | 2.00x |
| 1024 | 1,094,992 / 7,168 | 0.55 s | 0.48 s | 0.06 s | 51,320 KiB | 1.96x |

The generated cases contain 64 shared prefix edges plus 256/512/1024 leaf
edges, and exactly 256/512/1024 encoded nested entity-valued arguments.  Source
inspection confirms that `ensure_path_child` interns one `(parent identity,
edge)` key, `append_mixed_plain_component` looks up a complete parent/component
key, and `SubstitutionState::swap` uses container swaps rather than copying
the outer tables.  Thus the near-doubling is consistent with the measured
input growth and the O(n log n) map-bound implementation; the coarse 0.01 s
timer resolution makes the 1024 wall ratio conservative.  This evidence is
not a claim that the explicit 400--600 nonclaims are complete or that timing
alone proves the asymptotic bound.

## Checkpoint Ledger

| checkpoint | starting result | measured result | status |
|---|---|---|---|
| PA14 typed 300 boundary audit | landed HEAD `490d1ec7` from `12eaf37b`, turn-start 88/111 with 23 failures | Final 100/200/300 focused behavior remains 25/25, 25/25, 37/37; nine hand-derived course test files pass (eight added in this audit); final through-PA13 gate is 947/947; final PA14 is 88/111 with exactly the same 23 failures and no regression; file audit passes with four nonfatal pre-existing header warnings; ordinary qualified and unqualified typed template-entity/prefix reuse, owner template-prefix/complete-specialization order, explicit function-prefix timing, composed multi-owner identity, typed CV/alias unsigned normalization, dependent-wide rejection, nested entity isolation, member-template owner identity, enum-only standard substitution, and RAII cleanup audited | checkpoint-audit changes committed; final worktree clean |
| Remaining PA14 work | 400--600 families were outside the 300 ownership boundary | 3 named 400 failures, 13 named 500 failures, 7 named 600 failures remain | explicit nonclaim; next checkpoint |
| PA14 dependent-type/expression checkpoint | clean HEAD `623abbe4`, 88/111 and 23 listed failures | focused 400/500 is 17/17; parse→serialize→parse preserves all 17 mangles; final PA14 is 104/111 with 100/200/300/400/500 at 25/25, 25/25, 37/37, 4/4, 13/13 and the seven exact 600 nonclaims; through-PA13 is 947/947; audit passes with four known warnings; diff check passes | validated checkpoint increment |
