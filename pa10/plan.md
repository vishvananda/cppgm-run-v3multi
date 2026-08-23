# 1. Stage Design

The phase-7 boundary is one `PPPreprocessingSession` per CLI source list,
one canonical `PPTokenBuffer`, then posttoken facts: fixed
`SimpleTokenType`, producer `PPSpellingId`, source spelling, and decoded
literal data.  `PA10Ast` snapshots producer spellings, so identifier and
qualified-name components retain their producer IDs after the session ends.
`PA10AstNode` also owns typed unqualified-id facts: destructor marker plus
producer name ID, operator-function enum/fixed token (including `[]`/`()`) and
structured conversion target `type-id` in a semantic sidecar.  Individual
presentation pieces are separate hash-indexed cold data; qualified names and
operator labels are composed only by the renderer.

The AST is structured for later stages, with value-owned child vectors.  All
consumed local wrapper operands now move into child vectors and wrapper
replacement, avoiding subtree-copy amplification; no speed claim is made.
Token consumption is monotonic, charged once under `96 * token_count + 2048`,
with no whole-program retry or backtracking.  Unary prefixes fold iteratively;
parser recursion and renderer traversal are bounded by
`PA10_MAX_AST_NESTING = 1024`, with deterministic depth failure.

# 2. Failure Map

Baseline was 157 failures: 150 expected-success fixtures and 7
expected-failure fixtures, all returning `EXIT_NOT_IMPLEMENTED`.  The final
broad checkpoint run discovered all 157 and passed 76, leaving 81.  Residual
families (75 status failures and 6 expected-success dump mismatches) are:

- `tests/general/100` — 3 qualified-`decltype`, member operator-call, and
  template-condition seams.
- `tests/general/200` — 69 advanced operator/conversion and qualified
  declarators, special members, pointers, casts/new/initializers,
  lambda/attribute/exception/linkage, expression/control, and
  template/dependent/namespace/using/alias/angle-token seams.
- `tests/general/300` — 2 local-typedef and namespace-alias shadow cases.
- `tests/spec/200` — 6 bit-field, class-base/constructor-initializer,
  explicit-instantiation/specialization, non-type-template-parameter, and
  qualified-special-member cases.
- `tests/spec/300` — 1 template-id-less-expression case.

The counts account for all 81 residuals; no tests, refs, harnesses, or
grammar fixtures were edited.

# 3. Active Checkpoint

Scope is the coherent `--emit-ast` boundary, move-owned AST construction,
typed unqualified-id/operator/destructor/conversion facts, and the
foundational declarations, declarators, function body, namespace/using/alias,
expression/statement, and special-member slice.  Exact evidence: forced
warning-free build exit 0; spec/100 `13/13` exit 0; general/100 `31/34` exit
2; checked-in `100-bad.t` `1/1` exit 0; `make test-pa10` `76/157` pass,
`81` fail, discovered `157`, exit 2; prior-through-PA9 `457/457` exit 0;
file audit exit 0 with only the pre-existing
`dev/src/cpp_semantic_core.h:1` bad-division warning; and diff checks clean.
This is not full PA10.

# 4. Performance Evidence

No general speed or asymptotic AST-construction claim is made.  The immutable
executable hash was
`1a1d44ccb03b09e68b9fadb9ebaae6f2e7e6d6a6021a6c5f121596320d7068cf` before
and after outside-repository characterization.  Equivalent temporary unary
inputs with 128/256/512/768 prefixes (137/265/521/777 tokens) all exited 0;
single-sample `/usr/bin/time` readings were respectively `0.10s` and peak
RSS `6996/7032/7036/7040 KB`.  These are structural, non-comparative samples
only.  Two 1100-parenthesis inputs (2209 tokens) both exited 1 with
`PA10 parser recursion limit reached at token 261`, `0.10s`, and peak RSS
`8088/8312 KB`, exercising deterministic depth rejection.

# 5. Checkpoint Ledger

- Baseline: `d895a4a3`, clean tree; 157 PA10 failures (150 success + 7
  failure fixtures), discovered count 157.
- Correction milestone: 76/157 pass, 81 remain, so 76 baseline failures are
  removed; PA1--PA9 remain 457/457.  The existing five-path PA10 checkpoint
  is amended in place with this move/typed-fact correction.
- Final commit record: the existing checkpoint was amended in place with
  subject `PA10: add structured emit-ast checkpoint` on 2026-08-23;
  staged diff checks and post-commit clean-status verification passed.
  Identify it by content, subject, and date rather than a prewritten self
  hash.
- Later final: full PA10 completion, if pursued, requires a later checkpoint.
