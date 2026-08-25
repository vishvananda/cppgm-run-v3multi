# PA14 Checkpoint Plan

## 1. Stage Design

The line reader in `dev/abimangle.cpp` remains an adapter: it decodes source
vocabulary and normalized context records into one canonical typed
`AbiFactCase`.  `dev/src/abi_mangle.cpp` owns the append-based reusable
name/type/function encoder.  Its per-case state owns canonical ABI tags,
member qualifiers, semantic operator/conversion/special-member terminals,
local and lambda contexts, TLS/thunk special names, and typed name-prefix
substitutions.  The only raw text retained at this boundary is the explicitly
normalized raw local-context fragment.

The flow follows spec.md §§1--4 and §7: one production model, typed fact
continuity after parsing, deterministic per-case state, and no rendered-name
reparsing.  It follows Itanium Chapter 5.1 for ABI-tag order, nested names,
member qualifiers, operator and conversion terminals, local/closure names,
TLS wrappers, thunk call-offsets, and substitution numbering.  Special-member
vocabulary is decoded by the line adapter into
`AbiFunctionSpecialTerminalKind`; ordinary source-name terminals remain
source strings.  The encoder consumes those typed fields directly.

Substitution identity is a typed per-case path: component spellings are
interned once, qualified-name prefixes are parent-path nodes in a trie, and
only the (usually short) canonical ABI-tag ID vector is used as a leaf key.
This avoids copying a growing component vector for every prefix.  Append-based
recursive name/type/function work therefore costs O(n log k) for ordinary
qualified paths (and O(q log q) for q tags), within the O(n log n) target for
fact/type size; map comparisons still account for component spelling length.
Each qualified prefix walk selects the deepest existing leading untagged
candidate once, then source-encodes and registers only the unmatched suffix in
ABI insertion order.  The state is deterministic, and no recursive path
performs a whole-case scan.

## 2. Failure Map

Turn-start baseline: 35/111 passed, 76 failures.  Family counts were
100=25/25, 200=3/25, 300=7/37, 400=0/4, 500=0/13, 600=0/7.

Post-change full-stage counts: 100=25/25, 200=25/25, 300=7/37,
400=0/4, 500=0/13, and 600=0/7.  Thus all 111 tests were covered, 57
passed, 54 failed, and the baseline failure count fell by 22 with no coverage
loss.  The focused 100 and 200 checks were each PASS (25/25).

## 3. Active Checkpoint

This increment owns the complete checked-in `pa14/tests/abi/200-*.t` family:
ABI tags on functions, members, and special types; canonical member
qualifiers; semantic operators and conversions; builtin transform types;
local classes and local/lambda/namespace-lambda contexts; TLS wrappers;
ordinary, virtual-base, and covariant thunks; and the first substitution order
needed by those cases.  It also preserves the checked-in 100 family and the
reusable typed substitution boundary needed by the 200 cases.

Explicit nonclaims: this increment does not implement or validate broad
300--600 behavior, construction-vtable extensions, entity/template-template
coverage, dependent expressions, dependent owners, or later standard-library
cases beyond whatever was already present.  The observed 300--600 counts are
reported for coverage and regression accounting only, not as a claim of
completion.

## 4. Performance Evidence

Focused and stage commands and results:

- `make -C pa14 check TEST='tests/abi/200-*.t'`: PASS (25/25).
- `make -C pa14 check TEST='tests/abi/100-*.t'`: PASS (25/25).
- Generated probe, `printf '%s\n' 'thunk -8 virtual-result 16 -32 function path ::C::f' |
  /tmp/abimangle-pa14-candidate -o /dev/stdout /dev/stdin`: emits
  `_ZTchn8_v16_n32_N1C1fEv` (fixed result 16 and virtual offset -32).
- Generated substitution probe, `printf '%s\n' 'function path ns::C::operator' 'conversion-terminal named:ns::C::D' |
  /tmp/abimangle-pa14-candidate -o /dev/stdout /dev/stdin`: emits
  `_ZN2ns1CcvNS0_1DEEv`, with one longest-prefix `S0_` substitution and no
  consecutive substitution tokens.
- `make test-pa14`: all 111 tests covered; 57/111 passed and 54 failed
  (22 fewer failures than the 76-test baseline).
- `n=14; ... make test-report-through-pa$((n - 1))`: PASS,
  `947 / 947` through PA13.
- `perl scripts/cppgm_file_audit.pl --stage pa14 --paths dev/src`: PASS;
  four pre-existing header-division warnings are reported.
- `git diff --check`: PASS.
- Direct `g++ -std=gnu++11 -Wall -Wextra -Werror -Idev/src -c` compilation
  of both `dev/src/abi_mangle.cpp` and `dev/abimangle.cpp`: PASS.

The material risk is substitution and recursive encoding.  The immutable
candidate was built once before timing with:

    make -B -C dev abimangle && cp -f dev/abimangle /tmp/abimangle-pa14-candidate && chmod a-w /tmp/abimangle-pa14-candidate && sha256sum /tmp/abimangle-pa14-candidate

It was mode 0555, size 284840 bytes, with SHA-256
`269c9eeddb0acc543fa511a62fb74793e0c3d40da57b44decb5cb0115f2c6116`.
The exact generated-input measurement used the frozen candidate, a
process-substitution Perl generator, and `/usr/bin/time`:

    run_measurement() { depth_value=$1; parameter_value=256; printf 'depth=%s params=%s ' "$depth_value" "$parameter_value"; /usr/bin/time -f 'elapsed=%e user=%U sys=%S maxrss=%M' /tmp/abimangle-pa14-candidate -o /dev/null /dev/stdin < <(perl -e '$depth_value=shift; $parameter_value=shift; @name_parts=map { "n$_" } (0 .. ($depth_value - 1)); $qualified_name=join("::", @name_parts); print "function path ::${qualified_name}::f\n"; for($index=0; $index<$parameter_value; ++$index) { print "param named:${qualified_name}::C\n"; }' "$depth_value" "$parameter_value"); }
    for depth_value in 256 512 1024 512 256 1024 1024 256 512 512 1024 256 512 256 1024; do run_measurement "$depth_value"; done

Each size used 256 repeated named-type parameters.  The generated input
sizes were 370084, 764836, and 1560508 bytes for depths 256, 512, and 1024.
The 15 interleaved samples (five per size) had median elapsed/user/sys/RSS
results of:

| Owner depth | Parameters | Input bytes | Median elapsed | Median user | Median sys | Median max RSS |
|---:|---:|---:|---:|---:|---:|---:|
| 256 | 256 | 370084 | 0.02 s | 0.01 s | 0.00 s | 10256 KiB |
| 512 | 256 | 764836 | 0.04 s | 0.03 s | 0.01 s | 13652 KiB |
| 1024 | 256 | 1560508 | 0.08 s | 0.06 s | 0.01 s | 20292 KiB |

As a structural counter, `if rg -q 'NameKey|name_key|substitution_indexes_' dev/src/abi_mangle.cpp; then ...; else ...; fi`
reported `old-rendered-or-vector-key-symbols=0`, while `NameTrieNode` occurs
four times in the implementation.  A depth-d path contributes one interned
trie edge per distinct component, not a copied prefix vector for each edge;
repeated named-type parameters traverse those identities and only compare
their complete typed leaf/tag key.  The near-doubling timings track the
near-doubling input sizes, supporting the intended ordinary O(n log n)-class
behavior.  This is representative evidence for the substitution risk; it is
not a performance or behavior claim for 300--600.

## 5. Checkpoint Ledger

| Checkpoint | Starting point | Result | Status |
|---|---|---|---|
| PA14 bounded typed-foundation audit | `a95729060db60598a9e1f490346d093db7e99c3e` | Numeric IDs, one append type path, dense cycle state, explicit wide-value boundary, focused/broad validation, through-PA13, and source audit complete | Broad validation complete; bounded five-path repair finalized |
| PA14 complete typed 200-family ABI boundary | `16d775c44d9daf7b1b852e0d14f6c672595ec186` | Complete 200 family and preserved 100 family pass; typed special/operator/conversion terminals, canonical qualifiers/tags, local/lambda/namespace names, TLS/thunk spelling, covariant fixed-result offset, longest-prefix trie substitution, builtin transform; stage 57/111 with 54 failures; through-PA13 and audit pass | Trie correction validated and committed in the amended checkpoint |
