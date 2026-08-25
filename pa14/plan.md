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

The 300 boundary now registers complete typed candidates for compound types,
template IDs and prefixes, function-template prefixes, member/template
entities, values/expressions, standard substitutions, and external entity
addresses.  CV wrappers flatten adjacent const/volatile nodes for identity and
emission; named definition references and direct named/reference spellings
share the same structural identity.  Entity symbol encoding uses an isolated
nested substitution state, so a member's internal `C::m` spelling cannot
pollute the enclosing function's substitution order.  Entity-symbol isolation
swaps the trie/index state object with O(1) vector/map swaps and restores it on
success or exception.  Ordinary path type, result, and variadic operands remain
typed operands; only path template operands become function template
arguments.  A result type is emitted only for an identified function-template
encoding.

Completed name/template operands are published in left-to-right ABI order.
Mixed plain/template owner components are walked as one ordered sequence; a
plain suffix after a template owner is not sent through a disconnected scope
prefix.  The normal work is O(n log n) in the number of distinct interned
facts/edges (with O(q log q) tag canonicalization): every qualified edge is
interned once, and each structural map operation is logarithmic.  Entity
isolation adds O(1) state swaps per nested symbol.  There are no whole-case
rescans or quadratic rendered/vector keys.

## Failure Map

The clean baseline was HEAD `12eaf37b`: all 111 tests were covered, with
57/111 passing and 54 failing: 100=25/25, 200=25/25, 300=7/37, 400=0/4,
500=0/13, 600=0/7.  The seven baseline 300 passes were construction-vtable,
distinct-array-bound-substitution, minimum-signed-integral-value,
std-allocator-substitution, std-initializer-list-member-parameter,
std-initializer-list-parameter, and template-template-argument.

The final broad command `make test-pa14` covered all 111 tests and reported
88/111: 100=25/25, 200=25/25, 300=37/37, 400=1/4, 500=0/13, 600=0/7.
Thus all 30 baseline-failing 300 cases now pass, the seven baseline 300
passes remain green, and one 400 case also passes.  No baseline pass regressed.

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

The checked-in `.my` run was independently repeated with
`make -C pa14 test`: it ran 111 tests and reported `FAIL after 88/111`; the
first reported mismatch was `400-dependent-alias-type-id`.  These are actual
current artifacts, not assumed results from stale `.check` files.

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
blind output strings; `name-template`, `function-template-prefix`, and
member-template metadata are typed declarations and are not semantic keys.

Explicit nonclaims: 400--600 are not complete.  In particular, the remaining
400 dependent-owner cases, 500 dependent expression/decltype/type-trait
families, and 600 local-class/lambda, inline-namespace, and pack/template-
parameter cases remain outside this checkpoint.  No PA14 handout test,
reference, generated artifact, or harness was changed.

## Performance Evidence

The corrected candidate was copied to a temporary mode-0555 binary before
measurement; SHA-256 was
`5109a44d65b46c4c32e8390370ece7f02085cf1e8ae7b376523536f875e8b9f8`.
Generated equivalent inputs used a 64-component common qualified prefix and
one distinct `HolderN` template specialization per scale.  Each specialization
contains the same entity-address argument (`&C::m`) and is passed as one
function parameter, so the run exercises both deep prefix reuse and repeated
entity-valued arguments without changing checked-in fixtures.  Seven runs per
size were interleaved in the order 512, 1024, 256 with
`/usr/bin/time -f '%e %U %S %M'`; the table reports medians.

| input | bytes / fact lines | median wall | median user | median sys | median max RSS | wall ratio vs previous |
|---|---:|---:|---:|---:|---:|---:|
| 256 | 93,230 / 517 | 0.02 s | 0.01 s | 0.00 s | 9,700 KiB | -- |
| 512 | 186,670 / 1,029 | 0.04 s | 0.03 s | 0.01 s | 15,828 KiB | 2.00x |
| 1024 | 373,623 / 2,053 | 0.09 s | 0.06 s | 0.03 s | 28,420 KiB | 2.25x |

The generated cases contain 64 shared prefix edges plus 256/512/1024 leaf
edges, and exactly 256/512/1024 encoded entity-valued arguments.  Source
inspection confirms that `ensure_path_child` interns one `(parent identity,
edge)` key, `append_mixed_plain_component` looks up a complete parent/component
key, and `SubstitutionState::swap` uses container swaps rather than copying
the outer tables.  Thus the near-doubling is consistent with the measured
input growth and the O(n log n) map-bound implementation; the coarse 0.01 s
timer resolution makes the 1024 wall ratio conservative.  This evidence is
not a claim that the explicit 400--600 nonclaims are complete.

## Checkpoint Ledger

| checkpoint | starting result | measured result | status |
|---|---|---|---|
| PA14 typed 300 boundary | HEAD `12eaf37b`, 57/111, 54 failures | 88/111, 23 failures; 100/200=50/50 and 300=37/37; through-PA13=947/947; audit passed with four existing warnings; diff-check passed | coherent checkpoint committed in this handoff |
| Remaining PA14 work | 400--600 families were outside the 300 ownership boundary | 3 named 400 failures, 13 named 500 failures, 7 named 600 failures remain | explicit nonclaim; next checkpoint |
