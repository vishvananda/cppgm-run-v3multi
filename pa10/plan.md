# PA10 Final Audit Plan and Evidence

## Final audit state

The final PA10 implementation is rooted at stage base
`e22448c18a79875e6c18566f0db8eb624e31c158` (`PA10: complete residual
structured syntax`) and includes the final audit repair (this commit): the
typed global-scope delete boundary in `dev/src/pa10_ast.cpp`, its earliest
public regression, and these consolidated records.

The final validation counts and gate results are recorded below.  Historical
intermediate residual counts in prior records are ledger context only, not
final state.

The final audit files are:

- `dev/src/pa10_ast.cpp`
- `cppgm.tests/course/pa10/300-global-scope-delete.t`
- `cppgm.tests/course/pa10/300-global-scope-delete.ref`
- `cppgm.tests/course/pa10/300-global-scope-delete.ref.exit_status`
- `pa10/plan.md`
- `pa10/audit.md`

## Spec alignment and ownership

PA10 has one production `--emit-ast` path:

```text
PPTokenBuffer / PPSpellingId / LiteralData
    -> posttokenize_cpp_tokens / PA10PostTokenCollector
    -> typed PA10Token facts
    -> indexed template, delimiter, angle, and parenthesis facts
    -> one PA10Parser and canonical PA10Ast side arenas
    -> pa10_renderer requested text boundary
```

The driver creates one fresh preprocessing session per source operand, keeps
translation units in command-line order, rejects output/input inode aliases,
and writes deterministic wrappers.  `dev/frontend_source_sets.mk` links
exactly `pa10_ast`, `pa10_declarator_shape`, `pa10_parser_support`, and
`pa10_renderer` for `cppgm++`; the PA6 recognizer and later semantic parsers
are not parallel PA10 production paths.

At the first typed owners, phase-3 tokens retain fixed identity and
`PPSpellingId`; posttoken maps fixed vocabulary to `SimpleTokenType` and
copies decoded `LiteralData` or typed user-defined-literal facts.  The PA10
collector adds `RShiftPiece1/2`, contextual `override`/`final`/attribute
facts, and retains source spelling only as transient presentation data.
Identifiers remain producer IDs.  Literals retain decoded type, element
count, and bytes in their AST owner; source spelling is used only for the
requested dump and decoded linkage labels are rendered from literal bytes.

`build_indexes` owns one forward index pass for template/rshift closes,
delimiter closes, and lexical top-level `||`/comma facts.  It then classifies
parenthesized groups once in reverse token order so nested facts are available
to their enclosing group.  `PA10ParserSupport` owns bounded token-shape
predicates and publishes their work; `PA10Parser` charges every published
step against `96 * token_count + 2048`.  The parser owns all token consumption
and AST construction.  Qualified names and template arguments are component
and range facts; operator conversions, lambda captures, semantic children,
and presentation labels use typed side arenas.  The renderer resolves those
facts and never reparses or feeds rendered text back into production.

This satisfies spec §§1, 2, 4, and 7: there is no source reparse, trial AST,
rollback, second parser, lookup, semantic classification, host/reference/
previous-solution dependency, whole-program retry, or production textual
downgrade after a fact has been classified.  The documented checked-fixture
anonymous built-in non-type parameter (`int = 0`) is represented by a typed
default form; checked attribute syntax is structurally skipped because its
references omit attribute nodes.  These are explicit PA10 syntax-boundary
conventions, not test-name dispatch.

## Correctness and malformed-input boundaries

The audit found one real grammar mismatch.  `pa10.gram` accepts both
`delete p` and `::delete p`, including array delete, but the parser recognized
only the unqualified form.  The repair is at the unary-expression owner: it
recognizes `OP_COLON2 KW_DELETE`, emits the existing typed `GlobalScope` child,
then uses the unchanged delete operand and array-marker path.  The renderer
already owns the requested `global-scope` spelling, so no second output path or
fixture change is needed for the implementation path.  The required earliest
public regression is
`cppgm.tests/course/pa10/300-global-scope-delete.t`, with exact output in its
`.ref` file and `EXIT_SUCCESS` in its `.ref.exit_status` sidecar.  It contains
both `::delete p` and `::delete [] q`; the focused course check passed `1/1`.

Focused validation after the repair was `17/17` exact-green.  A direct
`::delete p; ::delete [] p;` probe exited `0` and emitted both global-scope
nodes; the malformed `::delete;` probe exited `1`.  Malformed `[=,]` and
`[&,]` lambda introducer probes each exited `1`.

The parser and renderer fail closed at missing EOF, invalid posttoken facts,
unmatched/truncated delimiters, invalid indexed ranges, malformed attributes,
malformed lambda capture lists, parser work exhaustion, and the 1024-level
AST/recursion/angle nesting ceilings.  Unsupported statement and expression
forms reach a syntax error rather than an opaque source-span node.  Output
ordering is determined by token and vector order; unordered maps are used
only for presentation-ID lookup, never iteration.

## Performance and storage evidence

Measurements use the final post-repair `dev/cppgm++` executable: size
`729560`, mtime `2026-08-24 04:24:18.089163072 +0000`, SHA-256
`c726f7df44651e70b40ce3682d56e5de4ac2298c8914cee33a86a5f25b4b80e2`.
The single-TU timing run used the same equivalent generated declaration
inputs, `/dev/null`, five interleaved rounds, and medians on Linux x86_64:

| declarations in one TU | median wall | median peak RSS |
| ---: | ---: | ---: |
| 256 | 0.01 s | 7492 KiB |
| 1024 | 0.03 s | 17548 KiB |
| 4096 | 0.15 s | 58444 KiB |

This is a bounded characterization, not a universal speed claim.  The
separate-TU authority characterization also measured 32/128/512 repeated
largest-checked-input TUs at `0.01/0.03/0.14 s`, with RSS ranges
`4588–4652/4576–4644/4832–4900 KiB`; it supports proportional per-TU
throughput and TU-lifetime reclamation only.

An observation-only structural harness measured the exact `build_indexes`
counter returned to the parser.  Representative ordinary function
declarations grew from `321 tokens / 1796 work / 32 classified groups` at 32
declarations to `20481 / 114692 / 2048` at 2048 declarations.  Pointer-function
declarations grew from `385 / 2564 / 64` to `24577 / 163844 / 4096`; deeply
parenthesized expressions from `70 / 856 / 32` to `4102 / 53272 / 2048`.
The work/token ratios remain bounded and the parenthesized-group counts are
linear in the input.  No unexplained cost required profiling.

The final-shape storage probe reported the following literal-bearing results;
the `a/b` columns are retained bytes/capacity:

| declarations | AST nodes | child edges | literal bytes | transient token source | rendered bytes |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 641 | 640/640 | 256/256 | 940/5775 | 15677 |
| 128 | 2561 | 2560/2560 | 1024/1024 | 3876/23055 | 62773 |
| 512 | 10241 | 10240/10240 | 4096/4096 | 16164/92175 | 251701 |
| 2048 | 40961 | 40960/40960 | 16384/16384 | 67412/368655 | 1009509 |

`PA10Token` is 240 bytes and `PA10AstNode` is 256 bytes on this build.  The
ordinary grammar child vectors, per-literal byte vectors, and transient
`PA10Token::source` strings are therefore an explicit `--emit-ast` boundary
exception carried forward from the durable `18a19d60` audit.  Current
measurements re-evaluate that exception: retained and rendered work are
linear, transient token storage is released with the parser, and no hidden
superlinear index or renderer cost appeared.  The exception is not presented
as the future hot-stage representation; a later consumer must re-measure and
move it to compact arenas before treating the AST as a hot compiler record.
Within PA10's requested dump stage this is justified and is not an unresolved
blocker.

## Validation state and checkpoint ledger

The final focused build `make -C dev cppgm++` exited `0`.  The pre-existing
focused matrix exited `0` with `17/17`; the new checked-in global-delete
fixture exited `0` with `1/1`; direct success and malformed-input probes had
exit codes `0`, `1`, `1`, `1`, respectively.  The final source file audit,
through-stage report, and `git diff --check` all exited `0`.  The source audit
reported `File audit passed for pa10 with 1 warning(s)` and only the known
`dev/src/cpp_semantic_core.h:1` bad-division warning.  The root report ended
`ALL TESTS PASSED SUCCESSFULLY! (617 / 617)`.

The landed stage history, in order, is:

```text
375ae19d 18a19d60 43703613 a2b82dcb d1bd5357 27623d64 b9b58b9c
08c38115 017eb658 25f78487 a35dfc17 d24f8e16 d5e24ae2 a7c20b87
c16e04ef 163fa9e1 90e9687c 7a35c9a9 87f0b94b ad7423f6 e22448c1
```

The architecture/storage audit is `18a19d60`; the attribute correction is
`ad7423f6`; the final audit repair (this commit) is the typed global-scope
delete fix, its public regression, and the consolidated final records.  No
uncertainty is a blocker.
