# 1. Stage design and spec alignment

The `--emit-ast` path is one production flow: the CLI validates the source
list, rejects output/input inode aliases before opening the output, and emits
one deterministic dump. Each command-line source file owns one fresh
`PPPreprocessingSession` and one canonical `PPTokenBuffer`; the session's
source-local macro, include, and conditional state therefore resets at the
translation-unit boundary established by `preproc_session.h` and PA5. The
posttoken facts feed the PA10 parser directly, and `PA10Ast` snapshots
producer spellings before the session is destroyed.

Fixed syntax is carried as `SimpleTokenType`; identifier components retain
`PPSpellingId`; literals retain `LiteralData` (type, element count, and
decoded bytes) beside cold source text. Producer spelling IDs are the
canonical presentation owner for scalar identifiers; `text` is reserved for
synthetic or arbitrary presentation. Operator-function identity is typed,
including call/subscript/conversion and scalar/array new/delete forms.
Destructors retain the tilde marker and producer name ID. Conversion
function-id type-ids are sparse semantic children of the identifier owner,
not a copied field on the special-member wrapper. Operator presentation is
an AST cold range of interned IDs and is composed only by the renderer. A
`LinkageSpecification` retains only the posttoken-decoded `LiteralData`; its
ordinary character payload is rendered on demand at the dump boundary, with
no `PA10LinkageKind` or other semantic linkage classification.

The AST still has value-owned grammar children, but parser wrapper operands
move into their owners. `SpecialInitializer` and `FunctionQualifier` retain
their fixed token identity with cold spellings. Linkage syntax is not
classified: decoded ordinary character bytes are rendered on demand and
arbitrary linkage strings are neither rejected nor stored as derived labels.
No parser retry or backtracking loop exists.
Token consumption and structural entry are charged under
`96 * token_count + 2048`; unary prefixes are folded iteratively.
Declaration, expression, abstract-declarator, braced-initializer, and
renderer name traversal paths are guarded by `PA10_MAX_AST_NESTING = 1024`.

This matches the PA10 README contract: only `--emit-ast -o <outfile>
<srcfile...>` is implemented, translation-unit wrappers are deterministic,
and preprocessing/tokenization/parsing/output failures return `EXIT_FAILURE`.
The requested dump is the only text boundary; no host compiler, reference
binary, source re-tokenization, or semantic classification is used.

Root spec section 4's allocation exception is measured at this explicit-dump
boundary rather than left as an unverified future question. A temporary AST
probe over repeated `int xN = 42;` declarations reported:

| declarations | source bytes | PP tokens | AST nodes | child edges/capacity | literal nodes/bytes/capacity | producer bytes |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 438 | 289 | 289 | 288/288 | 32/128/128 | 295 |
| 128 | 1810 | 1153 | 1153 | 1152/1152 | 128/512/512 | 611 |
| 512 | 7570 | 4609 | 4609 | 4608/4608 | 512/2048/2048 | 2147 |

The parser-owned presentation interning index is destroyed with the parser;
only the cold `presentation_spellings` vector survives in the returned AST.
The probe reports `sizeof(PA10Ast)=312`, `sizeof(PA10AstNode)=216`, and
presentation vector capacity 3 at all three samples. The same final
executable measured full-driver peak RSS/wall samples of
`4116 KB/0.00s`, `4848 KB/0.00s`, and `6736 KB/0.01s` at 32, 128, and 512
declarations. These samples quantify the retained child-vector and literal
byte ownership, while the collector-local `PA10Token::source` strings are
discarded when `parse_pa10_ast` returns. They justify retaining the current
value-owned vectors for this cold dump boundary without a general speed or
asymptotic claim; hot-stage integration is an explicit re-evaluation trigger.

# 2. Exact final failure map

`make test-pa10` evaluated all 157 fixtures and exited 2: 77 passed and 80
failed, consisting of 75 status failures and 5 expected-success dump
mismatches. Status failures are exactly:

- `general/100`: 3 (`decltype` qualified-id, member operator-call, and
  template-condition seams);
- `general/200`: 64 (advanced operators/conversions, qualified declarators
  and special members, pointers/casts/new/initializers, lambda/attribute/
  exception/linkage, expression/control, and template/dependent/
  namespace/using/alias/angle-token seams);
- `general/300`: 2 local-typedef and namespace-alias shadow cases;
- `spec/200`: 5 bit-field, explicit-instantiation/specialization,
  non-type-template-parameter, and qualified-special-member cases; and
- `spec/300`: 1 template-id-less-expression case.

The five dump mismatches are exactly `general/200-global-struct-paren-declaration`,
`general/200-local-typedef-paren-declaration`,
`general/200-member-template-if-less-template-call`,
`general/200-mock-template-name-angle-forms`, and
`general/200-relational-qualified-template-static-calls`. The failure identity
set is exactly the old 81-test set in
`/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log` minus
`spec/200-class-bases-and-ctor-init`; the comparison found zero new failures.
The removed mismatch is the legitimate bounded repair giving base `virtual`
its own node kind. No residual family outside this map was changed.

# 3. Final validation and performance characterization

- `make -B -C pa10 -j2 CPPGM_STDLIB_FLAGS='-Wextra -Werror'`: exit 0.
- `make test-pa10`: exit 2, discovered 157, passed 77, failed 80 (75 status
  and 5 dump); the exact failure-set comparison above found no new identity.
- `n=10; make test-report-through-pa9`: exit 0, 457/457 passed.
- `perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src`: exit 0;
  the only warning is the pre-existing
  `dev/src/cpp_semantic_core.h:1` bad-division warning.
- Focused checked probes for namespace, C/C++ linkage, member declarations,
  function qualifiers, and class bases: 6/6 pass. The arbitrary ordinary
  linkage probe exits 0 and renders `linkage-specification vendor`; the
  ownership probe confirms decoded `LiteralData`, no node text label, no
  derived `vendor` presentation entry, and no semantic linkage category.
- The two-translation-unit output probe exits 0 with two wrappers. `/dev/full`
  exits 1 after finalization, and an output/input alias exits 1 before output
  truncation. A 1100-level abstract declarator fails at token 1025 with the
  nesting limit; a 1100-level parenthesized expression fails at token 261
  with the recursion limit.
- Final executable hash for the measured samples:
  `609b858eefc27234820c3448486a588c66adafe2cd18959a41631d9c7dfbc939`.
  Unary-prefix inputs with 128/256/512/768 prefixes all exit 0; the
  single-sample wall/RSS readings are respectively `0.00s/4024 KB`,
  `0.00s/4136 KB`, `0.00s/4304 KB`, and `0.00s/4368 KB`. These are bounded
  characterization samples, not a general speed or asymptotic claim.

# 4. Next checkpoint

PA10 remains incomplete with the exact 80-test residual map above. A later
implementation checkpoint should choose one residual grammar family
(template-id/angle or declaration/type-context are natural candidates), add
its earliest owner regression, and re-evaluate the measured child/literal
storage when a hot stage begins consuming the AST.

# 5. Completed checkpoint row

| checkpoint | result | evidence |
| --- | --- | --- |
| `375ae19d` structured emit-ast increment plus finalized audit/repair | bounded checkpoint complete; full PA10 remains incomplete by the exact map above | 77/157 pass, 80 residuals, zero new failure identities, 457/457 through PA9, warning-clean build, file audit exit 0, measured storage and limit/output probes |
