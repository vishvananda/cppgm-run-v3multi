# Current Checkpoint Review

This review is bounded to the landed PA10 increment and its complete
ownership path: `cppgm++` source operands, preprocessing/session storage,
posttoken facts, PA10 AST ownership, and the requested renderer. It does not
attempt the remaining PA10 grammar families.

## Contract and pipeline

The required surface remains `--emit-ast -o <outfile> <srcfile...>`. The CLI
constructs one fresh `PPPreprocessingSession` and one canonical
`PPTokenBuffer` per translation unit, matching `preproc_session.h` and the PA5
source-local reset contract. The driver rejects an output path that aliases an
input inode before opening or truncating it, checks writes after each
translation unit, and flushes the final stream. Parse failures and stream
failures therefore return `EXIT_FAILURE`; partial output on a parse failure
remains unspecified as allowed by the PA10 README.

`dev/frontend_source_sets.mk` still supplies the single `pa10_ast` production
source to `cppgm++`. No grammar, harness, test, reference, or handout file was
changed.

## Ownership trace

- Identifier and qualified-name identity starts in the producer's
  `PPSpellingId`, passes through `emit_identifier_with_spelling`, and is
  stored as component IDs in `PA10Name`/AST nodes. `PA10Ast` snapshots the
  producer table before the per-TU session ends. Every scalar writer that has
  a producer ID now renders that ID canonically; `text` remains only for
  synthetic or arbitrary presentation. The renderer joins components only at
  the requested text boundary and never reparses a joined name.
- Fixed syntax crosses the posttoken boundary as `SimpleTokenType`. Operator
  names retain a typed alternative and operator token; subscript, call,
  conversion, scalar new/delete, and array new/delete are distinct typed
  cases. Destructor identity retains the tilde token and producer name ID.
  Conversion operators retain their parsed `type-id` in the AST semantic
  sidecar. The former special-member wrapper copy both duplicated identity and
  omitted that sidecar; the renderer now finds the canonical identifier under
  the owned declarator instead.
- Posttoken `LiteralData` (fundamental type, element count, and decoded bytes)
  is copied into the literal AST owner. Its source spelling is a cold
  presentation ID used only for the exact dump. No literal is decoded again
  from rendered text. Linkage specifications retain this decoded payload and
  render typed `C`/`C++` labels from it; they do not strip quotes from token
  source text. User-defined literal input remains outside the focused
  implemented slice and fails parsing rather than becoming an opaque node.
- `SpecialInitializer` and `FunctionQualifier` retain their consumed
  `SimpleTokenType` in the node and keep only the exact spelling as cold
  presentation data. Operator-function and destructor identity remains typed;
  conversion-function `type-id` payload is range-owned by the identifier.
- Grammar children are owned by their parent and consumed operands are moved
  into child vectors. Sparse operator presentation IDs and conversion
  subtrees now live in AST sidecars addressed by ranges, rather than vectors on
  every node. Ordinary grammar-child vectors, transient `PA10Token::source`
  strings, and per-literal byte vectors retain value ownership; their measured
  explicit-dump cost is recorded below rather than left as an uncertainty.

## Boundedness and renderer audit

All parser consumption is monotonic. There is no retry-until-stable loop or
backtracking parser. Consuming tokens and structural entries charge a linear
budget of `96 * token_count + 2048`; unary prefixes are folded iteratively.
The audit found and repaired missing guards in nested abstract declarators and
braced initializer lists. The renderer's recursive name lookup is guarded in
addition to its normal AST indentation limit. A malformed or adversarial
depth sample now fails with a deterministic recursion-limit diagnostic.

The fixed `virtual` base-specifier node is a real syntax kind rather than the
generic `LeafFixed` node. This is one of the six inspected dump mismatches and
the only one in the active base-clause slice. The other five mismatches are
the expected residual declaration/type-context and template-angle seams:
global/local parenthesized declarations, member-template `g<U>` parsing,
mock template-name angle forms, and qualified template static calls.

No source or test names are recognized in the implementation, and no
reference binary, previous solution, or host compiler is invoked to produce
PA10 output.

## Evidence

- `make -B -C pa10 -j2 CPPGM_STDLIB_FLAGS='-Wextra -Werror'` exited 0.
- `make test-pa10` exited 2 after evaluating all 157 fixtures: 77 passed and
  80 failed (75 status mismatches and 5 dump mismatches). Its failure
  identities are exactly the prior 81-test set in
  `/home/vishvananda/work/.ralph/v3multi-gpt-5.6-sol-xhigh/last-test.log`
  minus `pa10/tests/spec/200-class-bases-and-ctor-init.t`; the set comparison
  found zero new failures.
- `n=10; make test-report-through-pa9` exited 0 with 457/457 passing.
- `perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src` exited 0.
  Its sole warning is the pre-existing
  `dev/src/cpp_semantic_core.h:1` bad-division warning.
- Focused checked probes for namespace, both linkage labels, member
  declarations, function qualifiers, and class bases passed 6/6. The
  temporary fact probe built and ran successfully and reported
  `producer_text_duplicates=0`, decoded literal payloads, one C++ linkage
  kind, one `default` initializer, one `noexcept`, three operator nodes, one
  conversion `TypeId`, and `ranges_valid=1`.
- A two-translation-unit probe exited 0 with two wrappers. `/dev/full` exited 1
  after finalization, and an output/input alias exited 1 before truncation.
  The 1100-level abstract declarator exited 1 at token 1025 with the nesting
  limit; the 1100-level parenthesized expression exited 1 at token 261 with
  the recursion limit.

The explicit-dump storage probe reported these retained values:

| declarations | source bytes | PP tokens | AST nodes | child edges/capacity | literal nodes/bytes/capacity | producer bytes |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 32 | 438 | 289 | 289 | 288/288 | 32/128/128 | 295 |
| 128 | 1810 | 1153 | 1153 | 1152/1152 | 128/512/512 | 611 |
| 512 | 7570 | 4609 | 4609 | 4608/4608 | 512/2048/2048 | 2147 |

The same final executable, hash
`856d2c96d45332d7fbea31f408b8f6e67068ece78757bab2ce94c1f982348d0e`, measured
full-driver peak RSS/wall samples of `4148 KB/0.00s`, `4896 KB/0.00s`, and
`6892 KB/0.01s` at 32, 128, and 512 declarations. Unary-prefix samples at
128/256/512/768 prefixes all exited 0 with `4228/4116/4272/4360 KB` peak RSS
and `0.00s` wall time. These are bounded characterization samples, not a
general speed or asymptotic claim. The measured vectors are an explicit-dump
exception justified here; hot-stage integration must re-evaluate them.

## Uncertainties and next checkpoint

There is no unresolved ownership, output, boundedness, or storage question
within this audited increment. PA10 remains incomplete only by the exact 80
test residual map above. A later implementation checkpoint should choose one
residual grammar family and re-evaluate the measured child/literal storage
when a hot stage begins consuming the AST; that trigger is not an exception
left unexplained here.

## Audit ledger

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `375ae19d` PA10 structured emit-ast increment plus finalized audit/repair | bounded audit complete; scalar, linkage, fixed-token, session, base-node, ownership, recursion, and output repairs made | preserve prior PA10 coverage and defer only the exact residual families above | 77/157 pass, 80 residuals, zero new failure identities, through-PA9 457/457, warning-clean build, file audit exit 0, measured storage and limit/output probes |
