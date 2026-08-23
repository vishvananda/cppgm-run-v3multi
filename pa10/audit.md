# Current Checkpoint Review

This is the bounded audit of landed checkpoint
`25f784873f2a852fd825316b2188d9f157f8eae5`, `PA10: unify typed expression
postfix parsing`, whose parent is `017eb658f7c464c429fff858cbaae744c46fc01f`.
The review covered the parent-to-commit diff, the current support/parser/AST/
renderer sources, the five selected fixtures and their checked-in references,
the RShift/template siblings, and the PA10 grammar boundary.  The bounded
repairs below stay on that same ownership path and are included in the audited
checkpoint commit.  No grammar, handout test, existing reference, harness, or
residual-family implementation was widened.

## Contract and specification alignment

The production path remains one forward model:

```text
phase-3 source buffer -> typed posttoken stream and indexes
    -> PA10Parser expression seed -> one postfix-suffix consumer
    -> typed PA10Ast -> cold deterministic renderer
```

This matches the Purpose and §§1-4 and §7 of root `spec.md`: the support layer
classifies tokens and owns indexed delimiter/template facts, the parser owns
canonical syntax nodes, and the renderer is the requested text boundary.  No
host compiler, reference binary, previous solution, text reparse, retry loop,
or parallel AST/parser path is used.

The new RShift fact is typed continuity rather than a spelling convention.
The posttoken collector emits `OP_RSHIFT` as adjacent
`RShiftPiece1`/`RShiftPiece2` tokens.  `build_indexes` now sizes and clears all
four result arrays, marks Piece1 when the adjacent Piece2 closes another
indexed angle, and leaves ordinary pairs unmarked.  This is a typed
structural/index fact; it does not by itself settle template-id-versus-`<`
semantic ambiguity, which remains outside PA10.  The parser constructor
retains a token-indexed byte side array; `template_follow_is_valid`, template
declaration lookahead, member-pointer lookahead, and special-member lookahead
all consume the typed marker.  `close_angle` consumes one logical close, while
binary-expression parsing consumes both pieces only as one shift operator.

## Ownership-path findings and bounded repairs

The landed implementation had four same-path semantic gaps and one final
file-audit size correction:

1. `builtin_function_style_cast_start` and the renderer used the broad
   `is_type_keyword` predicate.  That predicate includes `KW_AUTO` for
   declaration-specifier parsing, but PA10’s authoritative
   `simple-type-specifier` production admits exactly the 13 fixed keywords
   `bool`, `char`, `char16_t`, `char32_t`, `double`, `float`, `int`, `long`,
   `short`, `signed`, `unsigned`, `void`, and `wchar_t`; it excludes `auto`.
   A shared typed `is_builtin_function_style_cast_keyword` predicate now owns
   that exact domain.  `auto(1)` is fail-closed and has a reduced checked-in
   status regression.
2. `build_indexes` assumed all result vectors had already been allocated and
   cleared.  An empty/reused support call could write out of bounds.  The
   builder now initializes each index to the exact token domain and sentinel
   state before its monotonic pass.  This adds no hot AST field and no storage
   beyond the existing indexed tables plus the one byte-per-token RShift fact.
3. The synthetic builtin callee is a typed `IdExpression` with
   `SimpleTokenType` identity and cold `token_spelling`; it is not a generic
   text node.  The renderer now validates the exact keyword domain, all
   identity/default fields, sidecar range starts/counts, and nonempty cold
   spelling before rendering it.  It never reparses the spelling.
4. Three older bounded lookaheads still skipped Piece2 from adjacency alone.
   They now require the typed indexed-angle marker, so an ordinary shift pair
   cannot be treated as an indexed-angle close by a declaration or
   member-pointer probe.
5. The final file audit measured `pa10_ast.cpp` at 3003 lines after the bounded
   routing changes.  The same calls and conditions were compacted without a
   semantic change; the final source is exactly 3000 lines and passes the size
   check.

The landed seed/suffix split itself is sound: `typeid` is a seed, builtin
function-style casts are a typed seed, and `parse_postfix_suffixes` is the sole
owner for call, member, subscript, and post-increment/decrement ordering.  The
typeid fixture reaches `typeid(int).name()[0]` through that one loop.  No
placement-new, lambda, declaration/declarator, qualified-name, or other
residual family was entered.

## Representative evidence

The final focused support probe passed with empty and reused output vectors and
printed:

```text
5 3 2 1
4 1 0
rshift probe exit=0
```

The first line has two indexed angle closes: the outer open closes at Piece2
(index 3), the inner open at Piece1 (index 2), and the Piece1 marker is set.
The second line is an ordinary pair: the close index is Piece1 (index 1) and
the marker is clear.  This structural result does not settle template-id-
versus-`<` semantic ambiguity.  Representative expression parsing additionally
passed the selected builtin/typeid/conditional fixtures, ordinary shift and
relational/template siblings, and the member-pointer/special-member lookahead
cluster.  The checked-in relational/ordinary-shift probe was four tests and
passed 4/4 before the broad gate.

The renderer invariant probe rejected both a synthetic `KW_AUTO` callee and a
synthetic node with an invalid operator field (`1 1`, exit 0).  Four malformed
or truncated stdin probes—nested template close, typeid suffix, builtin
parentheses, and `auto(1)`—all exited 1.  The checked-in focused results were:

```text
make -C dev cppgm++ CXX=g++                                  exit 0
warning-clean syntax compiles of pa10_ast/parser_support/renderer  exit 0 each
make -C pa10 check [14 postfix/RShift/typeid/malformed cases] exit 0; 14/14
make -C pa10 check [12 template/member-pointer lookahead cases] exit 0; 12/12
make -C pa10 check [new cast-domain regression]               exit 0; 1/1
make -C pa10 check [relational/ordinary-shift siblings]       exit 0; 4/4
git diff --check                                             exit 0
```

## Broad validation and final gate evidence

Fresh final `make test-pa10` exited 2 with **159 discovered, 142 passed, and 17
failed**.  The 159th test is the added status-only course regression, which
passed; the original 158-test universe therefore remains 141/158 plus that
new pass.  Extracting sorted failure identities from the turn-start log and
the fresh final log produced 17 entries in each file and `diff -u` exited 0.
The exact current residual set is the following, unchanged from the turn-start
17:

The 17 identities are:

```text
pa10/tests/general/200-elaborated-enum-member-declarators.t
pa10/tests/general/200-forward-unknown-nested-template-in-ctor-body.t
pa10/tests/general/200-friend-function-template-declaration.t
pa10/tests/general/200-friend-type-declaration.t
pa10/tests/general/200-global-struct-paren-declaration.t
pa10/tests/general/200-lambda-capture-forms.t
pa10/tests/general/200-local-typedef-paren-declaration.t
pa10/tests/general/200-member-template-parameter-value-vs-template-name.t
pa10/tests/general/200-mock-type-declaration-ambiguity.t
pa10/tests/general/200-parenthesized-new-type-vs-placement.t
pa10/tests/general/200-placement-new-identifier-led-initializer.t
pa10/tests/general/200-placement-new-pack-init.t
pa10/tests/general/200-qualified-enumerator-call-argument.t
pa10/tests/general/200-sizeof-elaborated-class-type-id.t
pa10/tests/general/200-template-member-definition-inherited-typedef-cast.t
pa10/tests/general/200-trailing-parameter-carries-dependency-attribute.t
pa10/tests/general/200-trailing-parameter-vendor-attribute.t
```

Fresh execution of the exact `n=10` through-PA9 command exited 0 with
**457/457**.  Fresh execution of
`perl scripts/cppgm_file_audit.pl --stage pa10 --paths dev/src` exited 0 and
reported one pre-existing warning at `dev/src/cpp_semantic_core.h:1`; there
were no fatal issues or new warnings.

## Performance and bounded-work evidence

The final focused executable was immutable during characterization and had
SHA-256
`e98aa88ab7f577b7b3435db10860e34c100bb2829854170d00800a898e91e863`.
Thirty-two repeated invocations per representative input in the same
environment measured:

| input | elapsed | user | sys | peak RSS |
| --- | ---: | ---: | ---: | ---: |
| `200-typeid-postfix-member-suffix.t` | 0.10 s | 0.04 s | 0.05 s | 4428 KB |
| `200-conditional-simple-type-shift-return.t` | 0.10 s | 0.03 s | 0.07 s | 4428 KB |

These are reused single-executable characterization measurements dominated by
process launch, not a comparative performance claim.  The final rebuild
retained the same SHA-256, so the measurements apply to the committed
executable.  Structurally, index construction
is one monotonic O(n) pass; seed classification is constant-lookahead;
postfix consumption is monotonic in suffix count; and all lookaheads are
bounded by indexed ranges and the existing parser work, recursion, angle, and
nested-delimiter limits.  There is no whole-program retry, text downgrade, or
new recursive path.

## Risks and next checkpoint

The final full-stage and file-audit results are recorded above.  The existing
residual families, including placement-new, lambda capture, declaration and
qualified-name ambiguity, remain risks outside this checkpoint.  Multi-`<`
relational/template ambiguity remains governed by the PA10 out-of-scope
semantic-disambiguation boundary.  The current RShift fact is fail-closed for
the audited indexed-angle and ordinary-pair cases; future changes must preserve
the single typed marker owner.

The next checkpoint is a supervisor-selected residual-family audit.  Do not
widen into placement-new, lambda, general declaration/declarator, or qualified
name work unless the supervisor selects that owner and the evidence proves the
failure belongs there.

## Historical evidence

The following values are retained as history, not current baseline claims:

- `a2b82dcb` established typed template components/sidecars and bounded angle
  ownership; its historical baseline was 106/157.
- `27623d64` unified the declarator/member boundary and bounded declarator
  shape.
- `b9b58b9c` audited that boundary at 123/157 with 34 failures and retained
  through-PA9 457/457 plus the pre-existing file-audit warning.
- `08c38115` routed structured names and special members and removed 12 prior
  residuals; its course boundary fixture remains in the suite.
- `017eb658` was the clean turn-start at 158/136 with 22 failures before the
  landed expression checkpoint.

The historical through-PA9 and file-audit values above are retained as
historical evidence; the fresh final results are recorded in the broad-gate
section.

## Audit ledger

| checkpoint | review result | owner action | validation state |
| --- | --- | --- | --- |
| `a2b82dcb` template/angle ownership | historical | retain typed template components and bounded close ownership | historical 106/157 baseline |
| `27623d64` declarator/member boundary | historical | retain unified declarator/member path and bounded shape | historical focused evidence |
| `b9b58b9c` declarator audit | historical | retain nearest-derived-operator and member-pointer bounds | historical 123/157; through-PA9 457/457 |
| `08c38115` structured names/special members | historical | retain one typed name/special-member path and validated sidecars | historical local 135/157; course 1/1 |
| `25f784873f2a852fd825316b2188d9f157f8eae5` typed postfix checkpoint | audited and committed | use one exact cast-keyword predicate, initialize indexes, route all RShift consumers through the marker, validate synthetic renderer nodes, and retain the 3000-line source bound | fresh 142/159 with exact original 17 failures; fresh through-PA9 457/457; file audit exit 0 with one pre-existing warning; focused 14/14 + 12/12 + regression 1/1; immutable performance characterization |
