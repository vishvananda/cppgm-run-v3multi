# PA14 Audit

## Current Checkpoint Review

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
| Current PA14 typed-foundation checkpoint | `a95729060db60598a9e1f490346d093db7e99c3e` | Numeric canonical IDs, one append type path, dense cycle state, typed terminal/linkage/builtin ownership, explicit 128-bit rejection, focused and broad validation complete | Broad validation complete; bounded five-path repair finalized |
