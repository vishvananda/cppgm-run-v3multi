# PA14 Checkpoint Plan

## 1. Stage Design

The line reader in `dev/abimangle.cpp` remains an adapter: it decodes source
vocabulary and normalized context records into one canonical typed
`AbiFactCase`.  `dev/src/abi_mangle.cpp` owns the append-based reusable
name/type/function encoder.  Its per-case state owns canonical ABI tags,
member qualifiers, semantic operator/conversion/special-member terminals,
local and lambda contexts, TLS/thunk special names, and typed name-prefix
substitutions.  Generic terminal fields retain only ordinary source text and
literal suffix payloads; fixed typed vocabulary is rendered by the encoder or
cold serializer.  The only raw ABI fragment retained at this boundary is the
explicitly normalized raw local-context fragment.

The flow follows spec.md §§1--4 and §7: one production model, typed fact
continuity after parsing, deterministic per-case state, and no rendered-name
reparsing.  It follows Itanium Chapter 5.1 for ABI-tag order, nested names,
member qualifiers, operator and conversion terminals, local/closure names,
TLS wrappers, thunk call-offsets, and substitution numbering.  Special-member
vocabulary is decoded by the line adapter into
`AbiFunctionSpecialTerminalKind`; ordinary source-name terminals remain
source strings.  The encoder consumes those typed fields directly, and the
adapter/serializer does not create a parallel fixed-vocabulary terminal owner.

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

Checkpoint-start authoritative result after landed commit
`0c6543189f0c505f6dea9ecb64ec23631b12d8d6`: all 111 tests are covered,
57 pass and 54 fail.  Family counts are 100=25/25, 200=25/25, 300=7/37,
400=0/4, 500=0/13, and 600=0/7.  The current bounded repairs preserve the
focused 100=25/25 and 200=25/25 results.  Final validation covers the same
111 tests with 57 pass and 54 fail in the same family counts.  The exact
current failure set equals the 54 failures in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`: no
previously passing checked-in test regressed and no current-only failure was
introduced.

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

### Spec alignment

Sections 1--4 and 7 are satisfied on this ownership path by one typed
adapter-to-encoder pipeline, dense per-case definition IDs, enum terminals,
stable trie/tag substitution keys, append-based recursion, and measured
deterministic behavior.  The repaired `assign`, unary/binary, terminal-source,
multi-digit discriminator, and typed-continuity cases close semantic gaps in
the Itanium Chapter 5.1 vocabulary without adding a second representation or
a recursive whole-case scan.  Cold serialization renders fixed spellings from
typed enums/types and preserves the affected parse/serialize path.

### Next checkpoint

This checkpoint's review, focused regression, broad gates, through-PA13 gate,
file audit, exact failure-set comparison, and commit are complete.  The next
implementation checkpoint is the separately scoped PA14 300-family boundary.

## 4. Performance Evidence

Current focused checks:

- `make -C pa14 check TEST='tests/abi/100-*.t'`: PASS (25/25).
- `make -C pa14 check TEST='tests/abi/200-*.t'`: PASS (25/25).
- `make -C pa14 check TEST='../cppgm.tests/course/pa14/200-typed-terminal-regression.t'`:
  PASS (1/1); exact output is hand-derived from the PA14 contract and
  Itanium requirements.
- Direct C++11 `-Wall -Wextra -Werror -Idev/src -fsyntax-only` checks for
  `dev/src/abi_mangle.cpp` and `dev/abimangle.cpp`: PASS.
- The typed parse/serialize probe finds no generic raw terminal for fixed
  operator, conversion, or special-member vocabulary and preserves all tested
  mangled outputs; direct probes emit `_ZN1CaSEi`, `_ZN1CpsEv`,
  `_ZN1CngEv`, `_ZZN2ns4makeEvEN1X__10_3runEv`, and preserve
  `terminal-source` as a source terminal.
- `make test-pa14`: 57/111; family counts 100=25/25, 200=25/25, 300=7/37,
  400=0/4, 500=0/13, 600=0/7.
- Exact `n=14` through gate: PASS (947/947).
- Exact PA14 file audit: PASS with four existing `bad-division` warnings.
- `git diff --check`: PASS after the complete bounded diff.

The final immutable mode-0555 candidate was 285264 bytes with SHA-256
`a7ce5a388a49bfd338b3d15499a88d3b0a386d11b2b492cf6fba3713fe5b5cdb`.
Five interleaved samples used 256 repeated named-type parameters and
equivalent qualified-name inputs at each owner depth; the input sizes were
397597, 792349, and 1581854 bytes.

| Owner depth | Parameters | Input bytes | Median elapsed | Median user | Median sys | Median max RSS |
|---:|---:|---:|---:|---:|---:|---:|
| 256 | 256 | 397597 | 0.02 s | 0.01 s | 0.00 s | 9208 KiB |
| 512 | 256 | 792349 | 0.04 s | 0.03 s | 0.00 s | 12460 KiB |
| 1024 | 256 | 1581854 | 0.07 s | 0.06 s | 0.01 s | 18764 KiB |

Structural counters remain `old-rendered-or-vector-key-symbols=0` for
`NameKey|name_key|substitution_indexes_`, with `NameTrieNode` present in the
typed trie implementation.  Each depth-d path contributes one interned edge
per distinct component, and repeated named-type parameters traverse those
identities with complete tag keys.  The near-doubling timings track the
near-doubling input sizes; this is representative evidence for ordinary
O(n log n)-class substitution work, not a claim for 300--600.

The full-stage, through-PA13, and file-audit results above are final evidence;
the exact failure-set comparison against `last-test.log` found zero
current-only regressions and zero baseline failures that newly pass.

The bounded changed surface is `dev/abimangle.cpp`,
`dev/src/abi_mangle.h`, `dev/src/abi_mangle.cpp`, this plan, `pa14/audit.md`,
and one course regression with its exact `.ref`/`.ref.exit_status`.  No PA14
handout fixture/reference, generated artifact, harness, or unrelated stage
surface changed.

## 5. Checkpoint Ledger

| Checkpoint | Starting point | Result | Status |
|---|---|---|---|
| PA14 bounded typed-foundation audit | `a95729060db60598a9e1f490346d093db7e99c3e` | Numeric IDs, one append type path, dense cycle state, explicit wide-value boundary, focused/broad validation, through-PA13, and source audit complete | Broad validation complete; bounded five-path repair finalized |
| PA14 complete typed 200-family ABI boundary | `16d775c44d9daf7b1b852e0d14f6c672595ec186` → landed `0c6543189f0c505f6dea9ecb64ec23631b12d8d6` | Complete 200 family and preserved 100 family pass; typed special/operator/conversion terminals, canonical qualifiers/tags, local/lambda/namespace names, TLS/thunk spelling, covariant fixed-result offset, longest-prefix trie substitution, builtin transform; typed-continuity cleanup and durable course regression; final stage 57/111 with exact 54-failure preservation; through-PA13, file audit, diff-check, compile, and focused gates complete | Checkpoint-audit commit complete; next implementation checkpoint is PA14 300-family |
