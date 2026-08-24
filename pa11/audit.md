# PA11 final architecture audit

## Final status and route

The PA11 architecture audit and repair are complete on branch `v3multi`,
starting from clean ledger commit `5a062586`. The audited full-stage
production route is:

`PPPreprocessingSession -> PPTokenBuffer -> parse_pa10_ast -> PA11SemanticModel -> deterministic dump`

`cppgm++` extends the typed PA10 AST boundary and has one PA11 semantic owner.
Future `cppgm++` semantic stages must extend this owner and its typed arenas;
they must not introduce a parallel production semantic model. The owner is
self-contained after `PA10Ast`: it does not invoke a reference binary,
previous solution, host compiler, or external semantic producer.

PA7 and PA8 remain actively required earlier-assignment executables with
disjoint staged contracts. `CppSemantic::SemanticCore` is used by the
`nsdecl` and `nsinit` earlier-stage paths; it is not dead code. The audited
PA11 full-stage production route is `cppgm++` only, and this record does not
claim repository-global semantic sharing between those owners.

## Architecture findings and corrections

- Domain identities are defined in the cohesive
  `dev/src/pa11_semantic_storage.h` module: names, types, named records,
  scopes, bindings, dump views, array bounds, source indexes, and generated
  ordinals cannot be mixed as raw `size_t` aliases. The old single `Id`
  namespace is gone.
- The same storage module owns the contiguous insertion-ordered open-addressed
  `FlatIndex`. Scope, namespace, alias, value, name, and type indexes are
  flat; hot records do not own node-based containers. The unused duplicate
  `using_values` index was removed.
- `lookup_namespace_graph`, `lookup_type_graph`, and `lookup_value_graph`
  use one reusable typed `LookupFrame` vector plus generation marks. Each
  reachable scope is marked and entered once, and each traversed using or
  inline-namespace edge is examined once: the graph bound is
  `O(reachable scopes + reachable edges)`. The walk has no input-sized C++
  call stack and does not allocate a visited set per query.
- Lookup priority is unchanged and deterministic: direct candidates precede
  imported candidates; using directives are traversed in reverse lexical
  order; type/value lookup traverses directives before reverse lexical inline
  children. Lexical vectors, never hash-table iteration, determine dump order.
- A canonical enum `NamedRecord` now carries a typed `DumpScopeViewId` link.
  `has_dump_scope_view` is O(1), eliminating the former scan of every global
  dump view for each canonical enum binding. Qualified enum components stay
  as `NamePath`/`NameId` facts, and the canonical enum `NamedRecordId`/`TypeId`
  remains the only lookup-visible identity.
- Qualified display text is rendered only at the dump boundary. The cold
  `DumpBindingView`/`DumpScopeView` sidecars do not create semantic scopes or
  bindings. Anonymous unions retain typed generated kind, owner, source
  interval, and ordinal facts; `__anonymous_union_type__B_E` is derived only
  while rendering.
- The indentation regressions in `make_cv` and `make_reference` were cleaned;
  a complete source audit found no stale untyped identity alias, node-based
  PA11 index, recursive graph self-call, qualified-string semantic intern, or
  old `using_values` path.

## Representative ownership traces

1. Producer spelling IDs become `NameId`s; qualified components remain a
   `NamePath`; typed scope lookup reaches `NamedRecordId`/`TypeId` and
   `BindingId`; the required spelling is rendered at dump time.
2. Typed declarator nodes become typed declarator operations. Pointer,
   reference, array, and function operations are hash-consed into `TypeId`s;
   variable, function, and parameter bindings retain those IDs.
3. Decoded `LiteralData` and typed expression operators produce `ConstValue`,
   which flows into enumerator/constexpr binding facts and then the dump.
4. An anonymous union AST source interval becomes a typed generated identity;
   its canonical record and scope own injected bindings, while only the dump
   derives the required generated display name.

## Immutable performance evidence

The immutable baseline was preserved before rebuilding:

```text
/tmp/pa11-final-audit.vqSxUC/cppgm++-baseline
sha256 99ba035fc6c69ad71746c1719b1ee27222f9829bdd5004f96e367b95f95d387e
size   894744 bytes
```

The final immutable executable is:

```text
/tmp/pa11-final-audit.vqSxUC/cppgm++-after-final6
sha256 228402ff10a504878af5d2272d0027ced6472f0fa3a2725f0bfa193f12b7427c
size   927816 bytes
```

`dev/src/pa11_semantic.cpp` is 2,997 lines and
`dev/src/pa11_semantic_storage.h` is 230 lines. The file audit therefore
passes the source-size limit.

All inputs were generated outside the repository, chmod read-only, and held
unchanged for three interleaved baseline/after samples. Sizes and hashes:

```text
same-small.cpp       9488  733f53fb639e6e08b440e0e32c5ef0fc5f66cad0f82d204236c3880f41be7fae
same-large.cpp      40592  3fff46825ebd9673344f714106b51efbd645148c80086195f0133653c3ccdef7
lookupq-small.cpp    4452  fd34ae6cbadbf4adb35ec7d82d84baa90a6054d40116b7232030f54c0fb89e32
lookupq-large.cpp   18396  60440b6bc7fcd38af94aaff5b350a941d22e9ef6649f2985f222127967d50dc9
deep2-small.cpp       562  02daa3e8ddecc981f93062d0542358a4c1a86718e523236beb11664b9d615e33
deep2-large.cpp      2218  747ea46e7522d0c416ff608e69687faafc9e67eaebf39533cbac18eb8d05fce9
enumviews-small.cpp 15298  758a7f68497289b7b5a38e8e9bcfa4429724cd9f7a780036eb2d087e552705e8
enumviews-large.cpp 63250  9f3b9ef94289abdd1344260751ee7d90c1b3e2101955dfe4f73f8b90e2b62608
lookupq-deep-small.cpp 46138  aafb74f419e4acd8c43d138ff992a79da222908bdf59d35c42d0ef0f671be8ce
lookupq-deep-large.cpp 182586 ec31da20a67e3169505ee8e39f4bbbccd1c944bca6c015644a29eb52c85151e6
```

Each median below is `wall, user, sys, peak RSS`, measured with
`/usr/bin/time -f 'wall=%e user=%U sys=%S rss_kb=%M'`:

| workload | structural output counts | preserved baseline median | corrected after median |
| --- | --- | --- | --- |
| same-small | 1 scope, 512 bindings | 0.01s, 0.00s, 0.00s, 7680 KB | 0.01s, 0.00s, 0.00s, 7688 KB |
| same-large | 1 scope, 2048 bindings | 0.04s, 0.02s, 0.01s, 18436 KB | 0.03s, 0.02s, 0.01s, 18464 KB |
| lookupq-small | 66 scopes, 129 bindings | 0.00s, 0.00s, 0.00s, 5132 KB | 0.00s, 0.00s, 0.00s, 5372 KB |
| lookupq-large | 258 scopes, 513 bindings | 0.02s, 0.01s, 0.00s, 7828 KB | 0.01s, 0.01s, 0.00s, 7816 KB |
| deep2-small | 25 scopes, 2 bindings | 0.00s, 0.00s, 0.00s, 4092 KB | 0.00s, 0.00s, 0.00s, 4132 KB |
| deep2-large | 97 scopes, 2 bindings | 0.00s, 0.00s, 0.00s, 4588 KB | 0.00s, 0.00s, 0.00s, 4840 KB |
| enumviews-small | 193 scopes, 704 bindings, 64 views | 0.01s, 0.00s, 0.00s, 6596 KB | 0.01s, 0.00s, 0.00s, 6588 KB |
| enumviews-large | 769 scopes, 2816 bindings, 256 views | 0.03s, 0.02s, 0.01s, 14460 KB | 0.03s, 0.02s, 0.01s, 14484 KB |
| lookupq-deep-small | 1026 scopes, 257 bindings | 0.05s, 0.03s, 0.01s, 11836 KB | 0.03s, 0.02s, 0.01s, 11836 KB |
| lookupq-deep-large | 4098 scopes, 513 bindings | 0.28s, 0.25s, 0.03s, 34236 KB | 0.14s, 0.10s, 0.03s, 34248 KB |

The deep pair uses flat 1,024- and 4,096-step namespace/using chains, rather
than AST nesting. The enum pair uses 64 and 256 qualified scoped-enum
definitions with eight enumerators each and directly exercises the former
`has_dump_scope_view` scan. The corrected output matched the preserved
baseline byte-for-byte for every workload and every sample.

Corrected after output hashes and sizes:

```text
same-small       18614  1457cbaea8dcb78974ec19037773b34c6f69720f7c9a01c7a45fa48398fa97d3
same-large       75446  13f0baacbab04fc3b314fba86e85db69bf030695bf1716b8827a9995ac88381d
lookupq-small     5162  a34643a426ac562bdbab0a196dcfcf0cb82739edbcbe2b555f92f4af9b548117
lookupq-large    20678  1620d9bbb9a6256225616729264b7e6a93f77a69820f4a058c1fa5aa5c05ca89
deep2-small       1323  9056e73efe9a4d8a2a3ba984a6956f26740b72e05eda18b56f4b94dd6442f9ff
deep2-large      11763  c3f9e2453a33dc63ec699588a84f9b7f01b9800ca4288ee7a58f581771f305d0
enumviews-small  45254  518e1774fbf04bfdf340bcbcb00afe0b898d140dc381474c04ae90ac5a00647f
enumviews-large 187382  66e6b57322d25243ce8f0211e441638253606c1879ea864fc36b1e61861c40e5
lookupq-deep-small 32990 944269193c1654ec0da24cefce2354e007553cd3e2c2665cbc21326668f491cf
lookupq-deep-large 120286 1cd33ca7e4fdb11b0ee647bebf713b74b62660848ae304a37ed0884067910cff
```

The measured deep-chain improvement and the enum structural counts explain
the remaining cost at this scale. Timing is quantized to hundredths of a
second, but no unexplained curve or outlier remained, so profiling was not
necessary. This is scaling evidence, not a production benchmark.

## Validation and ledger

- `make -C dev cppgm++`: passed.
- The focused PA11 ownership set passed `28/28`, covering the affected
  qualified-enum, anonymous-union, lookup/using, declarator, constant,
  `decltype`, bounds, and 100-family paths.
- `perl scripts/cppgm_file_audit.pl --stage pa11 --paths dev/src`: exit 0.
  The sole warning is the pre-existing
  `dev/src/cpp_semantic_core.h:1` substantial implementation-body warning.
- `make test-report-through-pa11`: exit 0, `685 / 685` tests across PA1–PA11.
- No tests or `.ref` files were edited and no reference regeneration was used.
- The durable 68-test PA11 inventory/failure map was not rebuilt.
- `git diff --check` is part of the final handoff check before staging.

The earlier commits remain unchanged: `1154916b` implemented the PA11 owner
and `5a062586` recorded the supplied validation ledger. The final handoff is
one new Luna-authored commit containing the audited source, storage module,
and these final plan/audit records.
