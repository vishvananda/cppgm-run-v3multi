# PA10 Final Architecture Audit

## Audit scope and state

This is the final full-stage audit of the PA10 path rooted at stage base
`e22448c18a79875e6c18566f0db8eb624e31c158` (`PA10: complete residual
structured syntax`).  It records the final audit repair (this commit): the
`::delete` grammar-boundary repair, its earliest public regression, final
post-repair measurements, and the consolidated records.

The final changed-file set is:

- `dev/src/pa10_ast.cpp`
- `cppgm.tests/course/pa10/300-global-scope-delete.t`
- `cppgm.tests/course/pa10/300-global-scope-delete.ref`
- `cppgm.tests/course/pa10/300-global-scope-delete.ref.exit_status`
- `pa10/plan.md`
- `pa10/audit.md`

No grammar, harness, source-set, or existing fixture was changed.  The final
validation counts and gate results are recorded below; earlier residual
identities are historical checkpoint evidence and are not final findings.

## Contract and architecture conclusion

The PA10 command surface is the README contract:

```text
cppgm++ --emit-ast -o <outfile> <srcfile1> [<srcfile2> ...]
```

The driver creates one fresh `PPPreprocessingSession` per source operand,
parses one phase-7 buffer, emits one translation-unit tree, and then lets that
TU's session, token vector, and parser storage die before the next operand.
Output wrappers and child traversal are deterministic.  Parse, preprocessing,
tokenization, and output failures return `EXIT_FAILURE`; partial output on a
failure remains unspecified as permitted by the handout.

There is one PA10 production implementation and one canonical output route:

```text
PPTokenBuffer
  -> posttokenize_cpp_tokens / PA10PostTokenCollector
  -> PA10Token
  -> build_indexes and PA10ParserSupport facts
  -> PA10Parser / PA10Ast typed nodes and side arenas
  -> pa10_renderer
```

`dev/frontend_source_sets.mk` links the four PA10 implementation units and
does not link a parallel PA6 parser.  The implementation does not invoke a
host compiler, reference binary, previous solution, assembler, linker, or
external parser.  It has no source-text reparse, trial AST, rollback,
parallel parser, lookup, semantic classification, or whole-program retry.

## Representative ownership traces

| fact | first owner and continuity | requested boundary |
| --- | --- | --- |
| identifier / qualified name | `PPSpellingId` in the producer table; posttoken preserves the ID; `PA10NameComponent` owns each component; AST snapshots the producer table | renderer joins components once for a name leaf or node label |
| fixed vocabulary / rshift | tokenizer/posttoken `SimpleTokenType`; the collector splits `OP_RSHIFT` into typed close pieces; index facts own close consumption | fixed-token renderer uses enum plus cold spelling |
| decoded literal | posttoken `LiteralData` carries type, count, and bytes; `PA10Token` copies it; literal AST nodes retain it; linkage labels read bytes | literal source is rendered only as cold dump presentation |
| contextual identifier | posttoken classifies `override`, `final`, and attribute introducers once; parser support consumes typed predicates | virt-specifier and attribute boundaries render/skip from typed facts |
| template ownership | `build_indexes` records close, rshift nesting, top-level `||`, and comma facts; parser owns template arguments and qualified components | renderer prints structured argument ranges, never splits joined text |
| parenthesized/declarator shape | delimiter closes and reverse group facts are indexed once; support publishes bounded member-pointer/new/declarator facts; parser consumes them | declarator, type-id, and new-expression nodes are built by the same parser |
| lambda capture | support scans the introducer once into default, capture kind, producer ID, and pack facts; AST owns a capture side range | renderer derives `[this]`, `[&,x]`, and pack spellings on demand |

This is typed fact continuity under spec §2.  Presentation IDs and producer
spelling snapshots are cold storage, not semantic identity.  Operator
conversion type-ids, semantic children, template arguments, name prefixes,
operator labels, and lambda captures have one typed owner and range-based
side-arena representation.

## Indexed work and bounds

`build_indexes` sizes and resets every output index before its forward pass.
Template-angle stacks are scoped by ordinary delimiters; valid `>` and split
`>>` closes are recorded, and top-level logical-or/comma facts are attached to
their owning angle.  Delimiter closes use a typed stack.  Parenthesized groups
are classified in reverse token order so nested results are available before
their enclosing group.  The parser charges the exact returned index-work
count.  Support scans publish their consumed or charged counts, and the
parser charges those counts under `96 * token_count + 2048`.

The parser's monotonic cursor, work budget, `PA10_MAX_AST_NESTING == 1024`,
and recursion/angle/non-angle guards bound malformed inputs.  Renderer node,
name, and declarator traversal has the same structural ceiling and validates
all sidecar ranges before dereference.  Sentinel indexes and explicit size
checks make truncated lookahead fail closed rather than inventing a token.

## Findings and repair

The final-stage audit found one correctness blocker not represented by the
checked fixtures.  `pa10.gram` has both optional-global-scope delete forms:

```text
OP_COLON2? KW_DELETE unary-expression
OP_COLON2? KW_DELETE OP_LSQUARE OP_RSQUARE unary-expression
```

`parse_unary_expression_base` previously entered the delete owner only for
`KW_DELETE`, and `parse_delete_expression` did not preserve a leading scope
marker.  The repair recognizes `OP_COLON2 KW_DELETE`, stores the existing
typed `GlobalScope` node, and leaves operand/array-delete parsing unchanged.
The renderer already has the canonical global-scope leaf path.  This is an
earliest-owner semantic fix, not a test-specific branch, and no reference
regeneration is involved.

The focused matrix after the repair passed `17/17` exact tests.  An
implementation-only probe for `::delete p; ::delete [] p;` exited `0` and
showed both `global-scope` nodes.  A malformed `::delete;` probe exited `1`.
Malformed `[=,]` and `[&,]` lambda probes each exited `1`.  No current
correctness, architecture, self-containment, or fail-closed blocker remains.

The earliest public regression is
`cppgm.tests/course/pa10/300-global-scope-delete.t`; its checked-in `.ref`
contains the exact AST for both `::delete p` and `::delete [] q`, and its
`.ref.exit_status` contains `EXIT_SUCCESS`.  The focused course check passed
`1/1`.  The `.ref` bytes are hand-authored and contract-derived from the
`pa10.gram` delete productions, the README translation-unit/tree output
contract, and the established standalone `global-scope` child shape in the
checked placement-new references.  The implementation output was compared
against those independently derived bytes only as a behavioral check.

The checked fixture's anonymous built-in non-type parameter presentation and
the attribute-skipping boundary remain documented syntax conventions.  They
do not introduce lookup, semantic classification, or opaque placeholder AST
nodes.  Unsupported syntax still fails during parsing.

## Performance and storage review

The final post-repair executable used for timing was `dev/cppgm++`, size
`729560`, mtime `2026-08-24 04:24:18.089163072 +0000`, SHA-256
`c726f7df44651e70b40ce3682d56e5de4ac2298c8914cee33a86a5f25b4b80e2`.
Five interleaved rounds over one TU, equivalent generated declaration inputs,
and output to `/dev/null` on Linux x86_64 gave these median wall/RSS samples:

| generated declarations | wall | peak RSS |
| ---: | ---: | ---: |
| 256 | 0.01 s | 7492 KiB |
| 1024 | 0.03 s | 17548 KiB |
| 4096 | 0.15 s | 58444 KiB |

The supplied separate-TU characterization was 32/128/512 TUs at
`0.01/0.03/0.14 s`, RSS `4588/4652/4632`, `4644/4576/4576`, and
`4900/4832/4832 KiB`; it is limited to proportional per-TU throughput and
TU-lifetime reclamation.

The structural index probe corroborated aggregate scaling.  Function
declarations measured `321 -> 20481` tokens and `1796 -> 114692` index work
for `32 -> 2048` declarations, with `32 -> 2048` classified groups.
Pointer-function declarations measured `385 -> 24577` tokens and
`2564 -> 163844` work; nested-parenthesis expressions measured
`70 -> 4102` tokens and `856 -> 53272` work.  The returned work is linear in
the token facts; no unexplained cost required profiling.

The final-shape storage probe measured literal-bearing inputs as follows;
`a/b` means retained bytes/capacity:

| declarations | AST nodes | child edges | literal bytes | transient PA10Token source | output bytes |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 641 | 640/640 | 256/256 | 940/5775 | 15677 |
| 128 | 2561 | 2560/2560 | 1024/1024 | 3876/23055 | 62773 |
| 512 | 10241 | 10240/10240 | 4096/4096 | 16164/92175 | 251701 |
| 2048 | 40961 | 40960/40960 | 16384/16384 | 67412/368655 | 1009509 |

`sizeof(PA10Token) == 240` and `sizeof(PA10AstNode) == 256` in the probe.
Ordinary grammar child vectors, per-literal byte vectors, and transient token
source strings retain value ownership, as the `18a19d60` durable audit
recorded.  The final-shape measurements re-evaluate rather than
silently inherit that exception: index work, AST nodes/edges, literal storage,
and renderer output all scale linearly; token scratch is reclaimed after the
parse.  This remains an explicit `--emit-ast` cold-boundary exception.  It is
not claimed as a future hot-stage representation, and that future migration
must be measured when a later PA consumes the AST.  It is justified and not a
PA10 blocker.

## Validation and uncertainties

Final focused and broad validation is recorded here:

- `make -C dev cppgm++` — exit `0`;
- pre-existing focused PA10 matrix — exit `0`, `17/17`;
- new global-delete course fixture — exit `0`, `1/1`;
- `::delete p; ::delete [] p;` probe — exit `0`;
- malformed `::delete;` probe — exit `1`;
- malformed `[=,]` and `[&,]` probes — exit `1` each;
- `perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src` — exit `0`,
  pass, with only the known nonfatal `cpp_semantic_core.h:1` bad-division
  warning;
- `make test-report-through-pa10` — exit `0`, with final discovered/pass
  counts `617/617` through PA10;
- `git diff --check` — exit `0`.

No correctness, architecture, self-containment, performance, timeout, or
file-audit uncertainty remains as a blocker.

## Compact checkpoint ledger

| hash | landed state |
| --- | --- |
| `375ae19d1b5bc58f1b75378980aa1d6c7a70bd10` | initial structured PA10 AST checkpoint; historical baseline |
| `18a19d60e2e9d5014f91227732b11bbe2c615a9b` | architecture, storage, bounds, and output audit; durable exception recorded |
| `a2b82dcb56245406f695c271a44ca55ca82f3949` | structured template IDs and qualified names |
| `27623d646279d867e58039af60a1cc52e09e090e` | unified declarator/member ownership |
| `08c38115a64397ae7170a53a81b74a1c36e0a9fb` | structured names, special members, and support ownership |
| `25f784873f2a852fd825316b2188d9f157f8eae5` | typed expression/postfix ownership |
| `d24f8e1689130b0449e19654ffd9e9f3dfc3b853` | structured new-expression/type-id ownership |
| `c16e04ef82e93bb0c628d2f495cc7132d47dd749` | elaborated-type boundary |
| `90e9687c759a39d9b4844cdc01deed3ab1e80250` | declaration ambiguity boundary |
| `87f0b94bc50c0f3658c94d1dbb9215ace5296140` | trailing parameter attributes |
| `ad7423f6bf9cb5469ad774d53cdef2949f1c24ac` | indexed attribute-wrapper correction |
| `e22448c18a79875e6c18566f0db8eb624e31c158` | final residual structured syntax; stage base for the final audit |
| `final audit repair (this commit)` | typed global-scope delete repair, earliest public regression, final measurements, and consolidated records |

Intermediate audit/implementation commits between the compact rows above are
landed in the supplied stage history; none is an active residual state.
