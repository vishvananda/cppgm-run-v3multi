# Current Checkpoint Review

This is the bounded audit of landed checkpoint
`27623d646279d867e58039af60a1cc52e09e090e`, `PA10: unify declarator and
member declaration parsing`. It reviews only the declarator/member boundary
owned by `dev/src/pa10_ast.cpp`, the private typed
`dev/src/pa10_declarator_shape.cpp`/`.h` owner, `dev/src/pa10_ast.h`, and
`dev/src/pa10_renderer.cpp`. The audit repair and final broad gates preserve
the turn-start 123/157 result and its exact 34-identity no-regression failure
set.

## Contract and specification alignment

The reviewed production flow is one forward path:

```text
phase-3 buffer -> posttoken facts -> PA10Token -> PA10Parser
    -> PA10Ast and sidecars -> cold deterministic renderer
```

This matches root `spec.md` §§1-4 and §7. Phase-3 identifiers carry
`PPSpellingId` through the posttoken boundary; the collector classifies the
contextual `override`/`final` vocabulary once into an enum while retaining
source spelling only for the requested dump. Fixed syntax remains
`SimpleTokenType`; names remain component records; template arguments,
decltype prefixes, operator labels, and semantic conversion children use
typed nodes or bounded sidecar ranges. The renderer consumes that model and
does not reparse, invoke a host/reference tool, or recognize test names.

The directly aligned `pa10/pa10.gram` productions are the bit-field list,
declarator and abstract-declarator composition, declarator suffixes,
parameters, non-type/template-template parameter forms, ordinary function
suffixes, lambda-declarator structure, and their declaration/member consumers.
The checked handout/ref contract also includes linkage
specifications such as `extern "C++"` and `extern "C"`, although the current
grammar has no named linkage-specification production (only `KW_EXTERN` in
decl-specifiers and extern-template syntax). It also requires qualified
member-pointer operators such as `C::*` and `n::C::*`, while the current
grammar's `ptr-operator` production literally lists only `*`, `&`, and `&&`.
Likewise the checked throw-specification refs require `throw(...)`, while the
current `function-suffix` text does not list dynamic throw specifications.
These are documented handout/ref extensions, not claims that the grammar was
changed; neither `pa10.gram` nor fixtures was edited.

## Ownership and correctness trace

- The posttoken collector turns one producer `OP_RSHIFT` into two typed close
  pieces, classifies contextual identifiers, and preserves producer spelling
  identity. `PA10Ast` snapshots that producer table before the input session
  ends.
- `parse_decl_or_function` and `parse_class_member` share the declarator path.
  Named and abstract declarators, parenthesized declarators, pointer/reference
  operators, arrays, parameter clauses, default arguments, bit-field lists,
  and member declarations are represented as AST structure rather than source
  spans. Named, unnamed, and zero-width bit-fields take the same checked list
  path.
- `member_pointer_operator_start()` scans only the qualified prefix from the
  current cursor. It bounds every absolute access, uses the existing indexed
  template-close lookup, and charges each material component. The consumer
  `parse_ptr_operator()` consumes the established form directly, so direct
  declarator paths do not rescan the qualifier. The type-id lookahead now
  passes a one-shot `first_member_pointer_checked` fact into
  `parse_abstract_declarator`, removing its former second scan before
  consumption. Nested `C<int>`, `C<C<int>>`, leading-global, and
  `template`-disambiguated qualifiers were exercised; malformed and truncated
  prefixes fail rather than read outside the token vector.
- The former recursive `declarator_has_parameter()` predicate was not a
  correct function-definition/initializer boundary: it could classify the
  parameter clause in `(*p)()` or `(&r)()` as a function and its early
  `Identifier`/nested return could miss an intervening array. The repaired
  `declarator_is_function()` uses a tri-state nearest-derived-operator walk.
  It recurses through a nested declarator only when that layer has an actual
  operator; parentheses alone defer to the enclosing declarator. The first
  operator outward from the identifier decides: `ParameterClause` is a
  function, while `PtrOperator`/reference or `ArraySuffix` is an object.
  Therefore `f()` and `(f)()` with bodies remain function definitions,
  `(*p)()`/`(&r)()` and parenthesized array layers take braced/paren
  initialization, and an inner `h()` parameter clause still identifies a
  function returning a pointer.
- The nearest-operator traversal is now a private typed helper in
  `pa10_declarator_shape.cpp`, declared by its implementation-only header and
  linked only into `cppgm++` through `frontend_source_sets.mk`. The parser
  retains the owning function-definition/initializer decision; no second AST,
  textual boundary, or render/reparse path was introduced.
- Function suffixes have one central path for cv/ref, `noexcept`, dynamic
  `throw(...)`, contextual virt specifiers, and trailing returns. Parameters,
  non-type packs, template-template parameters, linkage declarations, and
  lambda mutable/noexcept/trailing-return pieces retain their checked AST
  nodes. The renderer has explicit output cases for the new qualifier,
  lambda, virtual, and qualified-pointer nodes.
- Renderer entry points validate name-prefix, template-argument,
  operator-presentation, and semantic-child ranges before traversal. Nested
  sidecar nodes re-enter the same validation, and `.at()` access remains a
  fail-closed boundary for invalid spelling IDs. Parser work is one indexed
  pass plus monotonic consumption under the existing work, nesting, and
  recursion ceilings; no whole-input retry, duplicate parser/model, or
  render-and-reparse path was added.

The current source sizes are 2999 lines (`pa10_ast.cpp`), 53
(`pa10_declarator_shape.cpp`), 16 (`pa10_declarator_shape.h`), 361
(`pa10_ast.h`), and 756 (`pa10_renderer.cpp`). The helper is conventionally
formatted and the implementation files remain within their audit limits; the
layout is an ownership decision, not a source-minification target.

## Focused evidence

The focused build and warning checks passed:

```text
make -C dev cppgm++ CXX=g++                              exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_ast.cpp       exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_declarator_shape.cpp exit 0
g++ -std=gnu++11 -Wall -Wextra -Werror ... pa10_renderer.cpp  exit 0
```

The checked-in owner set plus nearby function-pointer/declarator cases passed
23/23 after the repair. The final `/tmp` nearest-operator probe passed and
rendered direct `f()`, plain `(f)()`, and the inner-parameter
function-returning-pointer form as function definitions; `(*p)()`, `(&r)()`,
parenthesized arrays, and arrays of function pointers were simple declarations
with braced or paren initializers. Qualified member-pointer edges, nested
template closes, and `template` disambiguation passed. The two truncated
member-pointer probes plus truncated declarator, `noexcept`, bit-field, class,
and template probes all exited 1 without an out-of-bounds read.

The repaired executable has SHA-256
`13e4d2f60d7bf1a19599d69d55a61bf958cf3af720aa9153e6695a4f168268b6`.
Single bounded characterization runs, not interleaved performance
comparisons, were below the timer resolution:

| input | result | elapsed | peak RSS |
| --- | --- | ---: | ---: |
| qualified member-pointer edge probe | success | 0.00 s | 4116 KB |
| nearest-operator boundary probe | success | 0.00 s | 4116 KB |
| checked qualified-member input | success | 0.00 s | 4116 KB |

These samples support the structural boundedness review only; they make no
timing comparison claim.

## Final validation evidence

The authorized broad commands were rerun against the final source form:

```text
make test-pa10                                      exit 2
===== TEST SUMMARY: 123 / 157 TESTS PASSED =====
n=10; ... make test-report-through-pa9              exit 0
===== ALL TESTS PASSED SUCCESSFULLY! (457 / 457) =====
perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src  exit 0
git diff --check                                    exit 0
```

The PA10 directory contains 157 `.t` files, and the runner reported all 157
discovered. Sorted exact-identity comparison against the turn-start
`last-test.log` found 34 baseline identities, zero added identities, and zero
removed identities. The file audit reported only the pre-existing
`dev/src/cpp_semantic_core.h:1` `bad-division` warning.

## Historical evidence

The following evidence belongs to the earlier
`a2b82dcb56245406f695c271a44ca55ca82f3949` template-id/qualified-name
checkpoint and is retained only as history:

- Its typed angle/name ownership path was closed with historical through-PA9
  457/457 and a file-audit exit 0 carrying the pre-existing
  `cpp_semantic_core.h:1` `bad-division` warning.
- Its old PA10 handoff was 106/157 with 51 failures; that identity set is
  superseded by the landed checkpoint's 123/157 and 34 residual identities.
- That historical audit repaired standalone `decltype` base/mem-initializer
  admission and renderer sidecar-range validation, and retained the rejected
  RShiftPiece2 experiment as a negative result. Its then-current source sizes
  were 2741/346/700 lines for `pa10_ast.cpp`, `pa10_ast.h`, and
  `pa10_renderer.cpp`; those are not current layout claims.
- Its warning-clean `-Wextra -Werror` build passed; the historical focused
  owner set was 11/11, through-PA9 was 457/457, and the file audit exited 0
  with only the same pre-existing warning. Its executable hash was
  `6cf44a43a293399ec829c6423526516cb502ec3b198b74c30209589922d665df`.
- Historical template-index characterization covered 32/128/256 component
  prefixes and malformed truncation; all measured elapsed values were below
  the display resolution except the retained 256-component relational and
  512-pair depth samples. Those values are not measurements of this repaired
  executable.
- Earlier storage characterization measured `sizeof(PA10Ast)=312`,
  `sizeof(PA10AstNode)=216`, presentation capacity 3, and full-driver RSS of
  4116/4848/6736 KB at 32/128/512 declarations. These values describe the
  prior layout only.
- The earlier RShiftPiece2 acceptance experiment was rejected after grammar
  and handout review. The current checkpoint retains the authoritative logical
  close/shift behavior and does not reopen that path.

## Audit ledger

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `27623d646279d867e58039af60a1cc52e09e090e` declarator/member boundary | bounded audit and repair complete; nearest-derived-operator function boundary is precise and member-pointer qualification scan is one-shot | retain the unified typed AST flow; keep grammar/ref extensions documented and defer the 34 unrelated PA10 identities | focused 23/23 plus boundary/truncation probes; private shape helper linked only to `cppgm++`; PA10 123/157 with the exact 34-identity baseline; through-PA9 457/457; file audit exit 0 with one pre-existing warning |

## Next checkpoint

The next checkpoint is a supervisor-selected family from the exact 34 residual
PA10 identities. Keep this declarator/member boundary closed and do not widen
the implementation or failure set without a separately bounded audit.
