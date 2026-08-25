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

The clean baseline was HEAD `12eaf37b`: all 111 tests were covered, with
57/111 passing and 54 failing: 100=25/25, 200=25/25, 300=7/37, 400=0/4,
500=0/13, 600=0/7.  The seven baseline 300 passes were construction-vtable,
distinct-array-bound-substitution, minimum-signed-integral-value,
std-allocator-substitution, std-initializer-list-member-parameter,
std-initializer-list-parameter, and template-template-argument.

The final broad command `make test-pa14` after the bounded audit correction
covered all 111 tests and reported 88/111: 100=25/25, 200=25/25, 300=37/37,
400=1/4, 500=0/13, 600=0/7.  Thus all 30 baseline-failing 300 cases now
pass, the seven baseline 300 passes remain green, and one 400 case also
passes.  No turn-start passing test regressed; the final failure identities
are exactly the 23 names below.

The complete remaining failure set is:

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

The required final gates are complete: `make test-report-through-pa13` passed
947/947, `make test-pa14` produced the 88/111 result above, and
`perl scripts/cppgm_file_audit.pl --stage pa14 --paths dev/src` passed with
four nonfatal pre-existing `bad-division` header warnings.  The focused
100/200/300 and nine focused course test files (eight added in this audit)
also remain green.

## Active Checkpoint and Spec Alignment

This checkpoint owns the complete checked-in 300 family: typed function/class
template arguments and prefixes; template IDs and standard substitutions;
entity-valued, member, and entity-address arguments; member-template and
template-template arguments; dependent integral values; canonical CV and
named/reference identity; and host-compatible substitution identity/order.

The implementation follows spec.md §§1--4 and §7 by keeping one typed
adapter/model/encoder pipeline, validating malformed fact combinations at the
adapter boundary, representing symbolic substitutions with complete typed
keys, and encoding ABI structure directly in left-to-right order.  It follows
Itanium Chapter 5.1 for nested/template prefixes, qualified template names,
dependent values, address expressions, standard abbreviations, and
substitution numbering.  Fixed standard substitutions are enum values, not
blind output strings; the adapter validates retained spelling metadata before
the enum crosses the boundary.  `name-template`,
`function-template-prefix`, and member-template substitution metadata are
typed declarations and are not semantic keys.  The Itanium compression rule
is applied to the complete typed member owner/name, not to an arbitrary
unqualified member component.  Ordinary qualified and unqualified
template-entity arguments share one dedicated complete typed identity with
their owner template-prefix; arbitrary source-name components remain in the
generic name domain.  The
multi-owner composed key is intentionally
not claimed equivalent to a later-family flattened/nested type record that is
not representable in this 300 model.

Explicit next checkpoint/nonclaims: 400--600 are not complete.  In particular, the remaining
400 dependent-owner cases, 500 dependent expression/decltype/type-trait
families, and 600 local-class/lambda, inline-namespace, and pack/template-
parameter cases remain outside this checkpoint.  Member-external raw symbols
remain a deliberate external-symbol boundary: this work does not reconstruct
or cross-check them from typed owner/member facts.  No PA14 handout test,
reference, generated artifact, or harness was changed.  The completed
checkpoint changes are committed in the authorized normal checkpoint commit,
and the final handoff verifies a clean worktree.  The four file-audit warnings
are existing header-division warnings in `abi_mangle.h`,
`cpp_semantic_core.h`, `lowir_model.h`, and `pa11_semantic_model.h`; there are
no fatal audit findings.

## Performance Evidence

The corrected post-repair candidate was copied to a temporary mode-0555 binary
before measurement; it was 405800 bytes with SHA-256
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
