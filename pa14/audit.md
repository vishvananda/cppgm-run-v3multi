# PA14 Audit

## Current Checkpoint Review

This review audits landed commit `490d1ec79877424ca537b522d81885eca049e81f`
(`pa14: complete typed 300 ABI boundary`) from parent
`12eaf37b894f60474c190542736e220ee87e93b4`.  The bounded ownership is the
complete checked-in `300-*` family and preservation of the earlier `100-*` and
`200-*` families:

```text
fact-file line -> dev/abimangle.cpp adapter -> canonical AbiFactCase
                 -> dev/src/abi_mangle.cpp FactEncoder -> exact ABI spelling
```

The turn-start full-stage baseline covers all 111 handout tests and passes
88/111: `100=25/25`, `200=25/25`, `300=37/37`, `400=1/4`, `500=0/13`, and
`600=0/7`.  The exact 23 failures are retained as the bounded nonclaim:

```text
400-dependent-alias-type-id
400-dependent-owner-member-template
400-dependent-rebind-other
500-dependent-bitset-words
500-dependent-call-expression
500-dependent-cast-expression
500-dependent-expression-type-substitution-order
500-dependent-function-parameter-decltype-param
500-dependent-object-member-expression
500-dependent-pack-expression
500-dependent-sizeof-type-expression
500-dependent-type-trait-expression
500-distinct-integral-decltype-substitution
500-distinct-type-trait-expression-substitution
500-equivalent-dependent-expr-substitution
500-equivalent-integral-decltype-substitution
600-function-local-class-template-arg
600-function-template-local-class-arg
600-function-template-local-lambda-arg
600-inline-namespace-basic-string-param
600-nested-helper-owner
600-template-param-template-type-substitution
600-template-parameter-pack-reference-constructor
```

The final broad validation reproduced that result exactly.  The required
through-PA13 gate passed `947/947`; `make test-pa14` covered all 111 handout
tests and passed `88/111` with `100=25/25`, `200=25/25`, `300=37/37`,
`400=1/4`, `500=0/13`, and `600=0/7`.  Its failure identities are exactly the
23 names above, with no turn-start passing test newly failing.  The required
file audit passed with four nonfatal pre-existing `bad-division` warnings in
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`, and
`pa11_semantic_model.h`; it reported no fatal findings.

### Representative typed-300 traces

- `300-function-template-prefix-result` parses `name-source`, a typed
  `function-template-prefix`, one typed function-template argument, a typed
  result, and parameters.  `collect_function_facts` preserves those fields;
  `function_prefix_identity` is a structural key, and the pending prefix is
  published immediately before the function's own template-argument list;
  the complete name emits
  `_Z9addressofIiEPT_RS0_`.  Conversion prefixes and operator prefixes use
  the corresponding typed terminal shape and are checked against the actual
  function facts.
- `300-abi-tagged-function-template` and the canonical CV/name-substitution
  cases retain ABI tags, adjacent CV wrappers, and qualified-name components
  as typed data.  `type_identity` flattens CV order and resolves canonical
  definition references; `append_type` then emits one spelling path, so
  equivalent `const volatile`/name forms share a candidate without using a
  rendered mangled string as a key.
- `300-std-allocator-substitution` and
  `300-std-ostream-member-template-result` carry a typed standard-substitution
  enum and typed argument references.  The enum owns identity and emission;
  retained `Sa`/`So` text is validated at the adapter boundary only.  The
  reusable encoder reads no standard-substitution spelling.  A mismatched
  code/name pair now fails instead of emitting a plausible abbreviation.
- A direct two-owner probe with `C1::fn` and `C2::fn` initially exposed a
  substitution keyed only by the unqualified `fn` component.  The member
  template argument key now includes the typed owner, member component, and
  typed member flags; the repaired hand-derived output is
  `6HolderIN2C12fnEN2C22fnEE`.  The member name is no longer independently
  inserted into the substitution table.  This follows the Itanium compression
  rule in `../doc/itanium-mangling.txt` §5.1.10 that qualified-name prefixes,
  not arbitrary unqualified components, are substitution candidates.
- A mixed qualified-owner probe encoded `ns::Outer<int>::f` and then passed the
  already-encoded `ns::Outer<int>` as a typed parameter.  The owner-position
  name path now publishes `ns` as `S_`, the dedicated typed `ns::Outer`
  template-prefix as `S0_` before `IiE`, and the complete `ns::Outer<int>` as `S1_`
  after its operands, exactly as the local Itanium §5.1.10 example requires.
  The hand-derived output is `_ZN2ns5OuterIiE1fES1_`; the complete typed
  specialization still crosses the name/type boundary.  Plain components
  after a template owner continue through the mixed-prefix path.
- `300-function-prefix-after-owner-template-regression` places the owner
  `Outer<int>` argument list before the actual function-template argument
  `float`; its hand-derived output is `_ZN2ns5OuterIiE1fIfEES1_`.  The
  pending function-prefix candidate is published only by the explicit
  terminal-function argument role, never by the owner's or a nested
  argument's list.  `300-multiple-template-owner-composition-regression`
  emits `_ZN2ns5OuterIiE5InnerIfE1fEv`; the second owner key is composed from
  the complete first specialization, so its earlier template arguments are
  not flattened away.
- `300-template-entity-prefix-reuse-regression` adds a function-template
  argument naming the ordinary qualified template entity `ns::Outer` after
  the structured owner has emitted `ns::Outer<int>::f`.  Its hand-derived
  output is `_ZN2ns5OuterIiE1fIS0_EES1_`: the template entity reuses the
  already-published typed `ns::Outer` prefix slot `S0_`, while the complete
  owner parameter remains `S1_`; the pending function-prefix slot is therefore
  still published only immediately before the actual function argument list.
- `300-unqualified-template-entity-prefix-reuse-regression` covers the same
  ordering for a global `Holder<int>::f`.  Its hand-derived output is
  `_ZN6HolderIiE1fIS_EES0_`: the typed global `Holder` prefix is `S_`, the
  complete specialization is `S0_`, and the later template-entity argument
  reuses `S_` without making an arbitrary unqualified source component a
  candidate.
- `300-member-function-pointer-nttp`, the external/member entity cases, and
  the direct nested probe trace typed owner/member/entity facts through
  `argument_identity`, `entity_identity`, `encode_entity_argument`, and the
  isolated nested symbol encoder.  Nested entity functions now collect and
  emit only their own target path/signature facts; they cannot reuse the
  enclosing function's `param`, name, or template records.  The regression
  output for an outer `param H` is
  `_ZN1p3useENS_6HolderIXadL_ZN1p1C1fEiEEEE`.
- The new hand-derived value regression exercises `const uint`, a `uint`
  alias, and a dependent `uint` value of `-1`, emitting respectively
  `6HolderILKj4294967295EE`, `6HolderILj4294967295EE`, and
  `6HolderITnT_Lj4294967295EE`.  Builtin resolution follows CV and type
  definition references, and the same boundary rejects dependent `int128` or
  `uint128` values because the canonical stored value is 64-bit.  Signed
  minimum handling and standard substitutions remain green in the checked-in
  family.
- `member-external-address` is an intentional raw external-symbol boundary,
  consistent with the PA14 README's raw external-symbol fact contract: the
  stored symbol is emitted verbatim for the `L...` entity address, while
  the typed owner/member/function fields validate the shape and participate in
  the complete non-rendered argument identity.  The raw spelling is not fed to
  the qualified-name trie or looked up as an independent substitution
  candidate.  The encoder does not reconstruct or cross-check an already-known
  external symbol against those typed facts; producer consistency at this
  explicit boundary is a nonclaim.

### Ownership, identity, ordering, and exception safety

The adapter builds one canonical `AbiFactCase` with dense numeric definition
IDs.  `StructuralKey` retains domain/kind, scalar flags and values, interned
source components, and ordered child identities.  Template IDs include typed
arguments; ordinary template prefixes and template-entity arguments share one
dedicated typed identity for both qualified and unqualified names, while
function-prefix keys include owner template-name arguments and exclude later
function-template operands; entity/member keys retain typed owner, member,
function-qualifier, parameter, and address facts.  No rendered mangled spelling
is a semantic key.  Raw spelling fields remain only as cold metadata or true
external-symbol/context boundaries.

Candidate publication follows the ABI's left-to-right structure: an ordinary
owner template-prefix is published before that component's template operands,
and its complete specialization is published only after those operands.  A
function-template prefix is published immediately before the function's own
template-argument list through an explicit typed emission role; owner and
nested argument lists cannot trigger it.  Final function and operator names
remain excluded from ordinary name substitution candidates.  The pending
function-prefix candidate is scoped with RAII, as are structural-identity and
type-definition active marks.  Nested entity symbols swap the complete
substitution state and restore it on both success and exception; nested
function fact collection is separately bounded to its typed target.  There is
one production `FactEncoder` path, no whole-case retry or rescan, no
host/reference/compiler shell-out, and no hardcoded test answer.  Qualified-
name edges use trie/map identities and tag canonicalization is the only
sorted operand; the source structure supports plausible O(n log n) map-backed
work for ordinary consumed fact/name sizes rather than rendered-string or
growing-vector keys.  This is not a theorem for arbitrarily wide structural
keys or later families.

### Bounded repairs and focused evidence

The audit repairs are limited to the owned paths: CV/alias-aware unsigned
normalization, dependent-wide-value rejection, nested entity target isolation,
RAII cleanup of active/pending state, complete function-prefix identity fields,
typed standard-substitution validation,
function-prefix consistency checks, owner template-prefix ordering, ordinary
typed template-entity/prefix identity for qualified and unqualified names,
explicit function-template
argument-role publication, composed multi-owner identity, member-external
data/function validation, and the short local/lambda target adapter bounds
check.  The only added test surface is the small set of
hand-derived course regressions and one negative rejection under
`cppgm.tests/course/pa14/`; no handout test, `.ref`, harness, wrapper, or
reference was regenerated or modified.

The focused evidence is:

```text
make -B -C dev abimangle                              PASS
g++ -std=c++11 -Wall -Wextra -Werror -Idev/src        PASS
  -fsyntax-only dev/src/abi_mangle.cpp dev/abimangle.cpp
make -C pa14 check TEST='tests/abi/100-*.t'           PASS 25/25
make -C pa14 check TEST='tests/abi/200-*.t'           PASS 25/25
make -C pa14 check TEST='tests/abi/300-*.t'           PASS 37/37
course PA14 focused regressions                        PASS 9/9
typed mismatch/cycle/wide probes                      PASS (rejected)
```

The final broad gate commands were:

```text
n=14; ... make test-report-through-pa13                  PASS 947/947
make test-pa14                                             88/111, exit 2;
                                                           exact authorized 23-failure set
perl scripts/cppgm_file_audit.pl --stage pa14 --paths dev/src
                                                           PASS, 4 nonfatal warnings
```

For performance, the corrected post-repair executable was copied to an
immutable mode-0555 temporary binary of 405800 bytes, SHA-256
`7bb13fa11ae1c0cac9d69a17e01df57552e84037873c6e971c852d6b3121299d`.  Seven
interleaved runs used equivalent generated cases with a 64-component
qualified prefix, one nested entity-function address, one template owner, and
one function parameter per case.  The inputs were 256/512/1024 cases,
273040/546960/1094992 bytes and 1792/3584/7168 fact lines.  `/usr/bin/time`
medians were:

```text
cases   wall   user   sys   max RSS
  256   0.14   0.12  0.01   15596 KiB
  512   0.28   0.25  0.03   27340 KiB
 1024   0.55   0.48  0.06   51320 KiB
```

The measured wall ratios are 2.00x and 1.96x for the two doublings; the
near-doubling follows equivalent input growth and is consistent with the
trie/map and structural-key facts above.  This is full parse/retain/encode
process evidence, not an allocation proof or a claim about the incomplete
400--600 families.

The bounded checkpoint changes are committed in the authorized normal
checkpoint commit, and final verification leaves the worktree clean.  No
handout test/ref, harness, wrapper, or unrelated stage surface was changed.

## Prior Checkpoint Review (historical): typed 200 family

This review audits landed commit `0c6543189f0c505f6dea9ecb64ec23631b12d8d6`
from `16d775c44d9daf7b1b852e0d14f6c672595ec186`.  The bounded ownership is
the complete checked-in `200-*` family and preservation of the `100-*` family:

```text
fact-file line -> dev/abimangle.cpp adapter -> canonical AbiFactCase
                 -> dev/src/abi_mangle.cpp FactEncoder -> ABI spelling
```

The 200 facts are decoded once into `AbiQualifiedName` components, typed
operator/special-terminal enums, typed conversion types, qualifier vectors,
contexts, and thunk adjustments.  Ordinary source-name terminals remain
source strings; `terminal-source` is not decoded as a special terminal.  Fixed
operator, conversion, and special-member vocabulary is not duplicated in the
generic `terminal` fields, including direct local/lambda/namespace-lambda
targets.  The only raw ABI fragment retained at this boundary is an explicitly
normalized raw local-context fragment.

### Representative fact traces

- `200-abi-tagged-function` parses `ns::f` into components and `cxx11` into a
  function tag record; the encoder sorts the tag vector at the ABI spelling
  boundary and emits `_ZN2ns1fB5cxx11Ei`.  The `foo`, `bar` case emits
  `_Z1fB3barB3foov`, confirming canonical tag order.  Tagged type definitions
  feed the same typed name path before the `TI`/`TV` special prefixes.
- `200-member-function-cv-qualifier-order` stores `const volatile` as enum
  qualifiers, and the nested-name encoder emits the ABI order `VK`:
  `_ZNVK9cv_member4callEdd`.  It does not use source qualifier order as an
  identity key.
- Operator records use `AbiOperatorTerminalKind`: `plus`, `plus-assign`,
  literal `_digits`, and `assign` reach `pl`, `pL`, `li7_digits`, and `aS`
  respectively.  Unary versus binary `plus`/`minus` uses the explicit
  parameter count and member shape.  Conversion records retain a typed
  `AbiType`; `C::operator C` emits `_ZN1CcvS_Ev` after the owner prefix has
  established the `S_` substitution.  Special-member records use only the
  typed `C1`--`C3`/`D0`--`D2` enum at emission.
- `200-builtin-transform-type` parses `__remove_reference` and its `ptr:int`
  operand as a typed transform tree, emitting `u18__remove_referenceIPiE`.
  No rendered transform name is reparsed.
- Function-context local classes, lambdas, and namespace lambdas all retain
  typed context/reference fields.  Function and raw contexts emit the same
  local-name prefix shape, while namespace-scope lambda closures emit
  `N2ns3$_0E`; the two local lambda forms emit the checked-in `cl` call
  operator names.  Direct target call and special-member terminals use their
  typed enums rather than parallel raw words.  Local entity discriminator 11
  now follows the ABI `__10_` form, while source terminals such as `run` stay
  source names.
- `200-tls-wrapper` emits `_ZTWN2ns1xE` by applying `ZTW` to the typed data
  name.  Ordinary, virtual-base, covariant, and virtual-result thunk records
  append typed `h`/`v` call-offset components before one function-target
  encoding, producing the four checked-in offset shapes without reparsing a
  rendered target.
- Qualified-prefix substitutions use interned component IDs in a per-case
  trie.  The leaf key includes the complete canonically sorted ABI-tag ID
  vector, so a tagged and untagged name cannot alias.  The local-context
  lambda substitution case emits `NS_1CE`, demonstrating prefix identity and
  insertion order through the typed path.

### Architecture, complexity, and evidence

`AbiFactCase` is the one production semantic model for this path.  The
encoder consumes its typed records directly; `join_qualified_name` is limited
to adapter serialization/external-symbol boundaries, and no rendered ABI name
is fed back into parsing or substitution lookup.  Definition references are
dense per-case IDs, operator and special-member vocabularies are enums,
conversion types are typed trees, and ABI tags remain arbitrary source
identifiers rather than a replacement for a semantic type.  Cold fact
serialization renders typed terminal spellings from enums/types and preserves
the affected function/context forms without making them a second semantic
owner.  The only whole-case scans are the bounded target/fact
collection steps at function entry; recursive type, context, and thunk paths
do not rescan the case.  Trie/map lookup and tag sorting keep ordinary work in
the O(n log n) class for the consumed fact/name size.

The final affected build was copied to an immutable mode-0555 candidate
(285264 bytes, SHA-256
`a7ce5a388a49bfd338b3d15499a88d3b0a386d11b2b492cf6fba3713fe5b5cdb`).  With
256 repeated named-type parameters and equivalent qualified-name inputs,
interleaved five-sample runs used 397597, 792349, and 1581854 bytes for owner
depths 256, 512, and 1024.  Median `/usr/bin/time` results were:

```text
depth   input bytes   elapsed   user   sys   max RSS
  256       397597      0.02s  0.01s 0.00s    9208 KiB
  512       792349      0.04s  0.03s 0.00s   12460 KiB
 1024      1581854      0.07s  0.06s 0.01s   18764 KiB
```

The near-doubling timings track the equivalent input-size increase.  This is
representative encoder-process evidence for the substitution path, not a
claim about later PA14 families.

### Findings and bounded repairs

- Restored the missing typed `assign` operator terminal (`aS`) lost during the
  enum migration.
- Corrected member unary `operator+()`/`operator-()` classification; a member
  with no explicit operand is unary, whereas one with an explicit operand is
  binary.
- Kept `terminal-source` as an ordinary source terminal instead of allowing
  a spelling such as `constructor-complete` to select a special-member code.
- Implemented the two-digit local discriminator production required for
  occurrences whose encoded number is at least 10.
- Made typed operator, conversion, and special-member terminals canonical in
  the adapter; fixed vocabulary is rendered on demand by the cold serializer,
  while ordinary source terminals and literal suffix payloads remain raw
  source data.
- Added `cppgm.tests/course/pa14/200-typed-terminal-regression.t` with exact
  hand-derived output for assignment, member unary operators, a
  `terminal-source`/special-spelling collision, and the `__10_` discriminator.

Focused `100-*` and `200-*` checks both pass 25/25, and the course regression
passes 1/1 through the PA14 check harness.  The final `make test-pa14` covers
all 111 checked-in tests and passes 57: 100=25/25, 200=25/25, 300=7/37,
400=0/4, 500=0/13, and 600=0/7.  Its 54-test failure set is exactly equal
to the 54-test set in `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`:
there are no current-only regressions and no baseline failures that newly
pass to mask a regression.  The exact through-PA13 gate passes 947/947.  The
stage file audit passes with four existing `bad-division` warnings for
`abi_mangle.h`, `cpp_semantic_core.h`, `lowir_model.h`, and
`pa11_semantic_model.h`; `git diff --check` and the warning-clean C++11
compile pass.  No handout fixtures, handout references, harnesses, or other
stage surfaces were modified; the only added test surface is the one course
regression and its hand-derived `.ref` pair.  Generated `.my`/`.check`
artifacts were not retained.

### Uncertainties and exclusions

The audit does not claim new 300--600 behavior, broader dependent/template
substitution, construction-vtable coverage, or compiler-owned PA15 use.  The
small checked-in 200 family cannot exercise every possible ABI tag, local
discriminator, operator, or offset magnitude, so the focused regression and
direct probes cover the new repaired edges while the fixture suite covers the
landed contract.  The performance sample includes parsing and retained fact
storage and is not an encoder-only allocation profile.

## Earlier Checkpoint Review (historical): typed 100 family

This review is bounded to the complete checked-in `100-*` family and the
ownership path introduced by `a95729060db60598a9e1f490346d093db7e99c3e`:

```text
fact-file line -> dev/abimangle.cpp adapter -> canonical AbiFactCase
                 -> dev/src/abi_mangle.cpp FactEncoder -> ABI spelling
```

The implementation surface is `dev/abimangle.cpp`, `dev/src/abi_mangle.h`,
and `dev/src/abi_mangle.cpp`; the existing `abi_mangle` source wiring was
already correct and was not changed. No tests, references, harnesses, or
other stage surfaces were modified.

### Representative fact traces

- A qualified function such as `function ::ns::f int ptr:char` is parsed once
  into name components `ns, f`, builtin enum `int`, and a pointer child
  `char`; the encoder emits `_ZN2ns1fEiPc` without joining and reparsing the
  qualified name.
- `variable x`, `variable ::ns::x`, `variable std::nothrow`, and
  `variable std::__1::cerr` retain component vectors. They emit `x`,
  `_ZN2ns1xE`, `_ZSt7nothrow`, and `_ZNSt3__14cerrE` respectively.
- `named:ns::C` owns `ns, C`; member-pointer, cv/pointer, array, and function
  facts emit `MN2ns1CEi`, `PKi`, `A3_i`, `FivE`, and `FidzE` through the same
  append traversal. The generic vendor-qualified type also reaches this
  single path.
- `let-arg Maximum value uint -1` is a typed builtin/value pair. Its dense
  definition index is used by a template type and emits
  `6extentILj4294967295EE`. The signed minimum fixture retains the signed
  magnitude path and emits `3BoxILxn9223372036854775808EE`.
- Constructor and destructor terminals are parsed into
  `AbiFunctionSpecialTerminalKind` and selected by that enum alone, retaining
  the `C1`, `C2`, `C3`, `D0`, `D1`, and `D2` ABI spellings. A C-linkage fact
  carries `AbiLinkageKind` and emits its external name without `_Z`.

### Findings and repairs

- `AbiQualifiedName` owns ordered components. The adapter strips an optional
  leading `::` and splits once; the reusable encoder consumes the vector
  directly. `split_qualified_name` and the join-then-split
  `encode_components` route are gone. Joining remains only at cold text
  serialization or the raw external-symbol boundary.
- `AbiDefinitionId` is numeric-only. The adapter's per-case
  `DefinitionInterner` assigns deterministic dense indices, tracks which
  labels were defined, reports duplicate and unknown binder spellings, and
  copies labels only into `AbiFactCase::definition_labels` as a cold
  diagnostic/serialization sidecar. The reusable encoder has no string-bearing
  definition ID and indexes its definition owner directly.
- Type emission has one production traversal: `append_type` has one explicit
  case for every 23 represented `AbiTypeKind` values and appends to one output
  buffer. `encode_type_impl`, the duplicate `encode_*` compound routes, and
  recursive serialized type keys are absent. A dense byte-vector
  `ActiveDefinitionScope` provides exception-safe enter/leave cycle state.
- Builtins, linkage, and special terminals are typed enums; array bounds are
  parsed to `size_t`. Supported integral values use the typed builtin and
  signed/unsigned normalization. Typed `int128` and `uint128` values are
  rejected at the reusable encoder's typed-value emission boundary because
  the accepted value storage is 64-bit; they can no longer silently produce a
  wrong successful name.
- The write-only substitution table and recursive `type_key` were removed.
  They had no lookup and emitted no substitution, so this checkpoint makes no
  substitution-support claim. Unary children are moved into parent nodes and
  function records are read from the case owner rather than copied into a
  second semantic record vector.

### Determinism, ownership, and risk review

Case and record order remain input order. Definition indices are assigned by
the adapter's deterministic first-reference interning order within each case;
binder spelling is not semantic identity. ABI tags are sorted only at their
ABI spelling boundary. The encoder owns no separate semantic model: its dense
definition table points into the canonical `AbiFactCase`, and cycle state is
keyed by the same canonical index.

The corrected path is ordinary O(n) in consumed type/fact structure, apart
from bounded ABI-tag sorting and map-backed adapter interning/lookups. It does
not reparse typed facts, recursively serialize structural keys, retry through
exceptions, shell out to a host/reference compiler, or emit dormant
substitutions. Retained parsed cases account for most measured RSS; the
performance sample below measures the complete parse-and-encode process.

### Focused and broad evidence

- Forced `make -B -C dev abimangle` and direct C++11
  `-Wall -Wextra -Werror -fsyntax-only` checks for both affected translation
  units completed without warnings.
- `make -C pa14 check TEST='tests/abi/100-*.t'` passed 25/25. The ten
  pre-existing exact 200/300 passes were rerun individually and all passed.
- Generated probes reject `let-arg Wide value uint128 -1` with status 1 and
  diagnostic `128-bit integral ABI values are unsupported by the stored value
  representation`; the output file stayed empty. A generated signed-minimum
  probe passed. Generated duplicate and unknown binder probes retained their
  respective diagnostics.
- The final `make test-pa14` covered all 111 tests and reproduced 35 passes /
  76 failures: 100 is 25/25; 200 is 3/25; 300 is 7/37; 400 is 0/4; 500 is
  0/13; and 600 is 0/7. The 76 failures are 22, 30, 4, 13, and 7 in
  families 200 through 600 respectively.
- The required through-PA13 gate passed 947/947. The source file audit passed
  with four `bad-division` warnings, including the substantial-body policy
  warning for `abi_mangle.h`; no unauthorized source path was found.
  `git diff --check` passed.
- Structural counters in the corrected source are: 23 `AbiTypeKind` cases in
  one `append_type`; zero `encode_type_impl`, `encode_cv_type`, `encode_array`,
  `encode_function_type`, `split_qualified_name`, `encode_components`,
  `type_key`, `SubstitutionTable`, substitution-table state, `std::set`, or
  node-set active-state operations; and no `std::string` field in
  `AbiDefinitionId`.

### Corrected performance evidence

The immutable mode-555 candidate was copied from the final affected build to
`/tmp/abimangle-pa14-corrected-final2.03RnDp`; its SHA-256 is
`7d99b0c57154421f3405bb95dea31d439032de5e818a7f83cd34b78434029b40`.
Each generated case used the same three fact lines (a named definition and a
nested pointer/cv/array/member-pointer target); output went to `/dev/null`.
Input sizes were 5,000 cases / 438,893 bytes, 10,000 / 878,894 bytes, and
20,000 / 1,768,894 bytes. Five repetitions were interleaved in 5k, 10k, 20k
order. Median wall/user/system/RSS were respectively:

```text
cases   wall(s)  user(s)  sys(s)  RSS(KiB)
 5000     0.14     0.08    0.05      62808
10000     0.27     0.17    0.10     122072
20000     0.57     0.35    0.21     240604
```

These are representative full-process medians with equivalent generated
inputs and structural counters, not a comparison ratio or an encoder-only
measurement. RSS includes retained parsed cases and the sample does not claim
later-family scalability.

### Uncertainties, limits, and later-family exclusions

The broad result remains at the checkpoint boundary: this repair preserves
the ten later exact passes but does not implement new 200--600 grammar or
behavior. Typed substitution, dependent expressions, local contexts, lambda
contexts, and later-family ownership require a separate authorized checkpoint.
128-bit typed integral values are an explicit supported-range rejection, not
an encoding claim. Cold serialization supports only the currently represented
fact subset and may use deterministic synthetic binder labels when no sidecar
label is available.

### Audit ledger

| Audit | Starting point | Bounded result | Working-tree status |
|---|---|---|---|
| Prior PA14 typed-foundation checkpoint | `a95729060db60598a9e1f490346d093db7e99c3e` | Numeric canonical IDs, one append type path, dense cycle state, typed terminal/linkage/builtin ownership, explicit 128-bit rejection, focused and broad validation complete | Broad validation complete; bounded five-path repair finalized |
| Current PA14 complete typed 200-family checkpoint audit | `16d775c44d9daf7b1b852e0d14f6c672595ec186` → landed `0c6543189f0c505f6dea9ecb64ec23631b12d8d6` | Complete 100/200 focused behavior preserved; typed ABI tags, qualifiers, terminals, contexts, TLS/thunks, and qualified-prefix substitutions traced; four bounded semantic repairs plus typed-continuity cleanup made; durable course regression added; full-stage, through-PA13, file-audit, diff-check, compile, and exact failure-set preservation validated | Checkpoint-audit changes committed; worktree clean |
| Current PA14 typed-300 checkpoint audit | `12eaf37b894f60474c190542736e220ee87e93b4` → landed `490d1ec79877424ca537b522d81885eca049e81f` | Complete 300 focused behavior preserved; typed substitution keys, CV/alias value normalization, dependent-wide rejection, enum-only standard continuity, complete member-template identity, ordinary qualified and unqualified typed template-entity/prefix identity, owner template-prefix/complete-specialization order, explicit function-prefix timing, composed multi-owner identity, mixed qualified-owner identity, template-prefix validation, and nested entity-target isolation audited and repaired; no speculative empty-argument restriction added; nine hand-derived course regressions and refreshed representative immutable interleaved performance evidence recorded; final through-PA13 947/947, PA14 88/111 with the exact 23 authorized nonclaim failures, and file audit pass with four nonfatal warnings | Checkpoint-audit changes committed; final worktree clean |
