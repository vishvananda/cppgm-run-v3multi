# PA7 final full-stage architecture audit

## Final alignment

PA7 now has one production syntax owner for the shared PA6/PA7 responsibility. The path is:

`source bytes -> PPPreprocessingSession::preprocess -> canonical PPTokenBuffer -> posttokenize_cpp_tokens -> CppSyntaxTokenCollector -> CppDeclarationSyntaxParser -> CppDeclarationSyntaxConsumer -> PA7SemanticModel::Impl -> render_namespace/render_type -> nsdecl`.

The collector is the single typed posttoken boundary. Each token retains fixed enums, `PPSpellingId` where applicable, decoded `LiteralData`, EOF, and PA6's mock categories/special tokens (`override`, `final`, empty string, zero, split rshift). PA7 uses the canonical stream directly. Its ordinary input runs exactly one `CppDeclarationSyntaxParser`; `PA7SemanticActions` supplies semantic actions only. PA6 uses the same parser with an acceptance consumer for the common grammar and adapts to its legacy token view only for the broader PA6 extension parser. `nsdecl` never runs PA6 and never renders/reparses syntax.

This resolves spec.md §1's shared-core requirement: declaration, namespace, declarator, parameter, type-id, literal-bound, cursor charge, EOF, and nesting production ownership is shared; stage code owns policy/actions and semantic storage. Qualified names remain global plus ordered `NameId` components. The model owns `NameId`, `TypeId`, `EntityId`, and `NamespaceId` identity, namespace/entity/type lookup, canonical type equality, and first-declaration vectors.

Representative traces:

- `namespace A { namespace B { typedef int T; } } using namespace A; A::B::T x;` is collected once, converted to `QualifiedName{global=false, [NameId(A), NameId(B), NameId(T)]}`, resolved by namespace-category lookup followed by type-category lookup, and stored as one canonical `TypeId`.
- For `int a[3]; int a[];`, the shared parser consumes the decoded integer `LiteralData` through its charged `consume_literal()`; the semantic action interns `array(int, 3)`, and redeclaration merge completes the unknown bound to the existing array `TypeId`.
- For `typedef int& L; void f(int a[3], L&&, ...);`, shared declarator operations are applied from the declarator hole outward: function parameters are collected in source order, array parameters adjust to pointers, and `reference()` collapses `L&&` to the lvalue reference represented by `L`; the final function `TypeId` retains the variadic bit.
- Rendering accepts typed owner facts only: `render_type(TypeId)` uses an explicit task stack, while `render_namespace(NamespaceId)` uses an explicit namespace stack and the existing variable/function/child vectors. Text is produced only at the requested semantic-dump boundary.

## Architecture findings and changes

The reviewed milestone's shared cursor was insufficient because PA6 and PA7 still had parallel collectors and declaration parsers. The repair:

- added `cpp_syntax_tokens.h` and one `CppSyntaxTokenCollector`;
- extracted the concrete shared grammar into `cpp_declaration_syntax.h/.cpp`, with a compact action interface and bounded syntax temporaries rather than a retained translation-unit AST;
- made PA6 consume that owner and preserve its legacy extension parser/contracts;
- made PA7 consume the same parser once, then perform semantic actions in source order;
- retained the earlier iterative lookup, merge, type rendering, namespace rendering, and charged literal movement fixes;
- replaced PA7 node maps with a lazy flat index: entry keys/values live once in insertion-order vectors and slots hold entry indices.

No tests or reference files were edited.

## Bounds, cache, and storage rationale

`CppSyntaxCore` owns cursor look, charged advance, EOF classification, work limits, and nesting limits. PA6 uses 1024 nesting and `max(10000, 512 * token_count)` work; PA7 shared syntax uses 4096 nesting and `1024 + 64 * token_count` work. Every shared-parser token consume charges before `advance()`; array bounds no longer move the cursor directly.

Lookup uses a reusable generation-mark vector and an iterative DFS worklist. Each query marks a reachable anonymous/inline/using namespace once, visits children before using directives in the prior deterministic order, and terminates on cycles without namespace-count zero-fill. Its per-query bound is the reachable graph/worklist size; repeated declarations pay the required lookup traversal, not an accidental recursive stack or allocation proportional to all namespaces.

The flat index maintains power-of-two capacity, a 70% load threshold, bounded linear probing, explicit growth/probe overflow failures, and no deletion holes. Equality and `TypeKeyHash` cover every key field, including kind, fundamental, child, cv, bound/unknown-bound, result, variadic, parameter count, and every parameter `TypeId`. Expected lookup is constant-time; worst-case probing is explicitly bounded by table capacity. No hash iteration affects output: all rendered order is carried by vectors.

Each of the six per-namespace indexes is initially empty and allocates slots only after its first entry. Canonical `TypeKey` values are intentionally retained both in the directly renderable `TypeRecord` and in the canonical index entry; the index no longer duplicates keys into empty slots or during rehash, and its slot arrays store only entry indices. The measurements below cover this remaining vector cost and the per-namespace index amplification. No new semantic cache key was added.

## Scaling and profile evidence

PA7 emits a semantic dump, not generated code or objects; generated-code size/instruction metrics are therefore inapplicable. The applicable measures are elapsed/user/system time, maximum RSS, output-byte/hash equivalence, and structural counters.

The immutable before binary was built from the clean c75b6088 archive in `/tmp/pa7-baseline.mwF1LQ`:

```sh
make -C /tmp/pa7-baseline.mwF1LQ/dev nsdecl CPPGM_TEST_RUNNER=0 -j2
sha256sum /tmp/pa7-baseline.mwF1LQ/dev/nsdecl
# aafbec433acce83c683cb3207a95e3def52a018822d5ca4d1a5e789eebacdb83
```

The after binary is the final normal build of the working source:

```sh
make -C dev nsdecl CPPGM_TEST_RUNNER=0 -B -j2
sha256sum dev/nsdecl
# 658d777bf2fb9ffcdfa3365a936a24161f0d5b54247ca4c56e95a2e725093632
```

Both used g++ 15.2.0, `-std=gnu++11 -Wall -O3`, Linux x86_64, and test runner disabled. Inputs were temporary files under `/tmp/pa7-final-audit.1XytN8`; refreshed run records were under `/tmp/pa7-final-audit-rerun.0KS779`. Lookup shape was:

```text
namespace A0 { typedef int T0; }
namespace Ai { using namespace A(i-1); }  for i=1..N-1
using namespace A(N-1);
T0 v0; ... T0 v99;
```

For each N in 50, 100, 200, 400, 800, 1600, 3200, 6400, five interleaved baseline/candidate runs used:

```sh
/usr/bin/time -f 'elapsed=%e user=%U sys=%S rss_kb=%M exit=%x' \
  binary -o /tmp/out input.cpp
cmp -s baseline.out candidate.out
```

All 40 pairs exited 0 and compared equal. The following are medians of the five runs; fields are elapsed/user/system seconds and RSS KB.

| N | baseline e/u/s/RSS | candidate e/u/s/RSS |
|---:|---:|---:|
| 50 | 0/0/0/4060 | 0/0/0/4340 |
| 100 | 0/0/0/4332 | 0/0/0/4596 |
| 200 | 0/0/0/4568 | 0/0/0/4592 |
| 400 | 0/0/0/5516 | 0/0/0/5460 |
| 800 | .01/0/0/7084 | .01/0/0/7096 |
| 1600 | .02/.02/0/10456 | .02/.01/0/10484 |
| 3200 | .05/.04/.01/17108 | .04/.03/.01/17088 |
| 6400 | .11/.08/.03/30712 | .09/.06/.03/30736 |

Output hashes were identical in every row; for example N=50 was `38673b144ce6a7bf448cd25ea2a80e474ba2e31a8789d4181626a290e987abd9` and N=6400 was `d9e4f862d1db59f34d1ae3ee8461d9be4558f74517f76c7b48332931cc91c4b8`. Timings are characterization at this scale, not a claim of statistically significant speedup.

The nested-type shape was an actual compound chain, not redundant parentheses:

```text
typedef int T0;
i mod 3 == 0: typedef Ti *T(i+1);
i mod 3 == 1: typedef Ti (*T(i+1))(int);
i mod 3 == 2: typedef Ti T(i+1)[2];
Tn x;
```

This cycles pointers, pointer-to-functions, and arrays while keeping function parameters valid. Five interleaved runs for N=25, 50, 100, 200, 400, 800, 1600 produced identical output bytes/hashes. Output bytes were 656, 1183, 2182, 4234, 8282, 16434, and 32683 respectively; candidate RSS medians were 4052, 4084, 4388, 4584, 5068, 6312, and 8792 KB. Elapsed medians were 0, 0, 0, 0, 0, .01, and .01 seconds for the candidate. A single N=3000 characterization, still below the 4096 render guard, passed equivalently with 61,159 output bytes and identical hash `45ee7ebf30dbf7b0d13e2e23e33cb27c90e4efc6368ae14fe180a5b3b2f9ba01`; baseline/candidate elapsed was .03/.03 seconds and RSS 13,740/12,128 KB. The explicit variadic check produced identical output:

```text
function f function of (pointer to function of (pointer to int) returning int, lvalue-reference to array of 2 int, ...) returning void
```

For structural corroboration, an audit-only build used `-DPA7_AUDIT_COUNTERS` and had hash `279a5c342f1d2b4fbcc5b3829c6752b9470990588ee29feb139841a832a7d568`. Lookup namespace visits were:

```sh
make -C dev nsdecl CPPGM_STDLIB_FLAGS=-DPA7_AUDIT_COUNTERS CPPGM_TEST_RUNNER=0 -B -j2
sha256sum dev/nsdecl
# 279a5c342f1d2b4fbcc5b3829c6752b9470990588ee29feb139841a832a7d568
```

| N | lookup queries | namespace visits | namespaces | namespace index slots/entries |
|---:|---:|---:|---:|---:|
| 50 | 199 | 5199 | 51 | 392/151 |
| 100 | 299 | 10299 | 101 | 520/201 |
| 200 | 499 | 20499 | 201 | 776/301 |
| 400 | 899 | 40899 | 401 | 1288/501 |
| 800 | 1699 | 81699 | 801 | 2312/901 |
| 1600 | 3299 | 163299 | 1601 | 4360/1701 |
| 3200 | 6499 | 326499 | 3201 | 8456/3301 |
| 6400 | 12899 | 652899 | 6401 | 16648/6501 |

The counter grows linearly with the fixed 100 declarations and confirms iterative graph work rather than recursive stack growth. For the type chain, N=1600 had 2153 canonical type entries in 4096 slots, 533 parameter TypeIds, and 1602 namespace index entries in 4104 slots. These counters explain the measured flat storage and the absence of empty-slot key amplification.

## Validation

- Focused PA6/PA7 grammar, lookup, arrays, declarator, reference-collapse, and variadic checks passed.
- `make test-pa7 CPPGM_TEST_RUNNER=0`: 41/41.
- `make test-report-through-pa6 CPPGM_TEST_RUNNER=0`: 293/293.
- Final required `make test-report-through-pa7`: 334/334.
- Non-runner repeat `make test-report-through-pa7 CPPGM_TEST_RUNNER=0`: 334/334.
- `perl scripts/cppgm_file_audit.pl --stage pa7 --paths dev/src`: passed, 32 files checked.
- `git diff --check`: passed at each source checkpoint.
- No tests or `.ref` files were changed.

## Checkpoint ledger

1. c75b6088 baseline: PA7 behavior passed its existing gates, but the architecture still had parallel PA6/PA7 collectors and grammar implementations.
2. Review repair: centralized typed posttoken facts, introduced the shared concrete declaration grammar/action boundary, reused it from PA6, and removed PA7's second parser.
3. Measured correction: the first shared-tree version added material RSS at N=6400; it was replaced with streaming semantic actions. Final candidate RSS is within measurement noise of baseline.
4. Final source audit: flat index invariants/storage, iterative lookup/render/merge, charged token movement, self-containment, and source-set ownership are complete. The worker commit and clean-status result are the final handoff gates.
