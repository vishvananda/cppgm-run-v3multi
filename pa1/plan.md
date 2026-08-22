# PA1 architecture audit

## Contract and specification alignment

PA1 owns translation phases 1--3 and the N3485 preprocessing-token boundary.
`dev/src/pp_tokenizer.cpp` is the sole production implementation;
`dev/pptoken.cpp` and `DebugPPTokenStream` are the CLI/presentation adapter.
`IPPTokenStream` retains category-by-method calls and token spelling because
that spelling is required at PA1's requested textual output boundary.  No
ordinary production path invokes a host compiler, reference binary, or second
tokenizer.

The pipeline decodes strict UTF-8 and removes the BOM, creates trigraph-aware
physical `Unit` spans, compacts UCN conversion and line splicing in place,
adds the implied final LF, and performs one forward token scan.  A `Unit` owns
decoded `cp`, physical `[begin,end)`, and `from_ucn`.  Raw prefixes are matched
in logical units; the opening quote's physical `end` owns delimiter/body/close
scanning, and the physical close maps once to the logical suffix.  Raw body
spelling is the intentional physical-source exception.

## Typed ownership traces

`TokenKind` is the canonical category owner through centralized `emit_token`
dispatch.  `data_from_units`/`data_from_physical` render only for the
`IPPTokenStream` call; no rendered result participates in classification or
state.

| fact | ownership trace |
| --- | --- |
| UCN identifier | `phase1_ucns` produces a decoded `Unit` such as `cp=0x03C0, from_ucn`; Annex E ranges classify it as `TokenKind::Identifier`, then the boundary renders UTF-8. Basic-source UCNs remain rejected. |
| exact operator identity | decoded `and`, `new`, or `delete` units match the corresponding `PreprocessingOperator` enum value and emit `TokenKind::PreprocessingOpOrPunc`; identity is never recovered from rendered text. |
| include header context | exact `PreprocessingOperator::Hash` or `AlternativeHash` (`#`/`%:`) enables directive state at line start; `IdentifierRole::IncludeDirective` enables header context; one header span emits `TokenKind::HeaderName`. Whitespace/comments preserve the state. |
| normal literal | `LiteralKind` selects character/string grammar; logical escape and UCN checks consume the span, then the selected literal `TokenKind` emits once. |
| raw literal with spliced prefix | `RawPrefixKind` matches post-phase logical units; `Unit.end` locates the physical opening quote; raw delimiter comparison is bounded by 16 code points; the physical close maps to the logical suffix without changing raw body spelling. |
| whitespace/comment/newline | the scanner emits `TokenKind::WhitespaceSequence`; `TokenKind::NewLine` resets directive/header state. |
| maximal-munch punctuator | the ordered pattern table maps each spelling to a distinct `PreprocessingOperator`; the `<::` exception emits exact `Less` and then exact `Scope`. |

## Finding, disposition, and changes

The review finding was confirmed.  The old implementation rendered an
identifier before comparing identifier-like operators and passed rendered data
to state logic that compared `#`, `%:`, and `include`.

The repair introduces one exact `PreprocessingOperator` enum for all 57
punctuator spellings and all 13 identifier-like operators.  The pattern tables
map decoded-unit spellings to those exact values; string-literal array bounds
derive pattern lengths, and the existing longest-first order is preserved.
`PunctuatorRole` and the collapsed `IdentifierRole::AlternativeOperator` are
removed.  Only the separate typed `IdentifierRole::IncludeDirective` remains
for directive context.  Header state consumes exact `Hash`/`AlternativeHash`
enum values.  Token categories still flow through `TokenKind` to the adapter.

Changed files are `dev/src/pp_tokenizer.cpp` and this audit record.  No fixture,
reference, CLI adapter, stream interface, or source-set change was needed.

## Performance evidence

The final executable was rebuilt with the PA1 `g++` C++11 `-O3` build and had
SHA-256
`f46395ec25c22b9c2896f2fe645c609bdf162b26f7743f04589eb46f09c8782b`.
For reproducibility, temporary inputs were constructed outside the repository
as follows:

```sh
yes 'int x;' | head -c 1048572 > one-mib.cpp
yes 'int x;' | head -c 4194302 > four-mib.cpp
```

Each size had one warmup run; then five interleaved `one-mib`, `four-mib`
runs used `/usr/bin/time -f '%e %U %S %M'`, with compiler output redirected to
`/dev/null`.  The host-scoped medians were:

| input | wall | user | system | peak RSS |
| --- | ---: | ---: | ---: | ---: |
| 1,048,572 bytes | 1.22s | 0.69s | 0.52s | 34,308 KB |
| 4,194,302 bytes | 4.88s | 2.69s | 2.18s | 126,468 KB |

The approximately fourfold time scaling supports the linear pass structure;
no profile was warranted.  Earlier RSS samples were from different historical
build layouts and are not compared with this final measurement.

## Validation and checkpoint ledger

- Starting audited state: `8047559c`, clean; prior through-PA1 evidence was
  54/54 and 1/1 stage.
- `make -C dev pptoken`: PASS.
- Focused operator/include, maximal-munch, raw-prefix, literal, UCN, comment,
  trigraph, and E2 checks: PASS, 11/11.
- `perl scripts/cppgm_file_audit.pl --stage pa1 --paths dev/src`: exit 0,
  12 files checked.
- `make test-report-through-pa1`: exit 0, exactly 54/54 and 1/1 stage; no
  timeout.
- `git diff --check`: PASS.
- No checked-in fixture or reference changed.  Generated check artifacts were
  removed from the worktree.
- Final repair commit and clean-status confirmation are recorded in the
  follow-up ledger entry below.
