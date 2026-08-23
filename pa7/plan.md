# PA7 final full-stage architecture audit

## Final alignment and ownership path

PA7's production path is one typed ownership chain:

source bytes -> PPPreprocessingSession::preprocess -> canonical PPTokenBuffer -> posttokenize_cpp_tokens -> CppSyntaxTokenCollector -> CppDeclarationSyntaxParser -> PA7SemanticActions/PA7SemanticModel::Impl -> render_namespace/render_type -> nsdecl.

CppSyntaxCore owns cursor look, charged token movement, EOF classification, work accounting, and nesting bounds. CppDeclarationSyntaxParser is the one production declaration syntax owner for declarations, namespaces, qualified names, declarators, parameter clauses, type-ids, and literal bounds. It exposes only policy queries and actions at ambiguous name points.

PA7 constructs one CppSyntaxTokenCollector with no observer and runs one CppDeclarationSyntaxParser. Its actions query the current semantic namespace, intern only action-owned names, and build QualifiedName { global, vector<NameId> }, canonical TypeId values, EntityId/NamespaceId ownership, and deterministic first-declaration vectors. It never runs PA6 and never renders/reparses token text.

PA6 supplies the same parser an acceptance policy for the common grammar. The policy is exact for the subset it claims: a final type component must have lexical C/E/Y category; a namespace component must have N; non-root nested-name prefixes must be C/E/Y/N; a bare T-only template category is rejected because template-id productions remain in PA6's legacy extension. If the common parser or policy rejects, PA6 alone falls through to its existing complete parser, using the observer-built PA6 token view. Thus a successful common PA6 input runs one parser, and ordinary PA7 input runs exactly one parser.

Representative typed traces:

- namespace A { namespace B { typedef int T; } } using namespace A; A::B::T x; collects once, looks up A/B as namespaces and T as a type in the current semantic scope, and stores the resulting declaration with one canonical TypeId.
- int a[3]; int a[]; consumes the decoded integer LiteralData through charged consume_literal(). The first declaration interns array(int, 3); the second interns/merges the unknown bound to the existing canonical array identity.
- typedef int& LR; void f(int a[3], LR&&, ...); retains source-order parameter tasks, adjusts the array parameter to a pointer, collapses LR&& to the lvalue-reference identity, and sets the variadic function bit.
- Rendering accepts typed owner facts only: render_type(TypeId) and render_namespace(NamespaceId) use explicit bounded task stacks and append text only at the requested semantic-dump boundary.

## Corrective architecture changes

- CppSyntaxTokenCollector is the sole posttoken owner. Canonical tokens retain fixed enums, PPSpellingId identifiers, decoded LiteralData, an explicit ordinary-vs-UDL distinction, and EOF. A cold CppSyntaxTokenObserver supplies PA6-only mock categories, override/final, empty-string/zero categories, UDL-to-literal adaptation, and RSHIFT splitting only when recog asks for it. nsdecl supplies no observer, performs no C/T/Y/E/N or special-spelling scans, and stores no PA6-only fields.
- The PA6 compatibility vector is made from that one collection event; it is not a second posttoken collector. CppDeclarationSyntaxParser no longer has known_type_spellings_ or a global linear type guess. PA6CommonSyntaxPolicy answers lexical mock questions; PA7SemanticActions asks actual namespace/type lookup without interning unknown spellings.
- The prior ae1d309b20a2ac6261550fa2b3212b9a3cb5199d shared-cursor milestone was reviewed as an intermediate, not papered over: it still had parallel token/grammar ownership, global type memory, and no broad typed lookup cache. This correction removes those blockers while retaining PA6's legacy extension and all PA1-PA7 contracts.
- PA7's earlier node-map replacement remains the storage owner. The compact FlatHashIndex now has explicit clear/overflow paths and the lookup cache uses the same deterministic insertion-order ownership model.

PA6 earliest-owner regressions are 275-shared-type-category-bad.t, 275-shared-typedef-not-symbol-table-bad.t, 275-shared-namespace-alias-category-bad.t, and 275-shared-using-namespace-category-bad.t. They cover lowercase type false positives, typedef-memory leakage, namespace aliases, and qualified using-namespace categories. Existing pa6 template tests cover valid T-category angle handling; existing PA7 220-namespace-name-lookup-shadowing.t and 350-parenthesized-parameter-declarators.t cover semantic scope/ambiguity.

## Bounds, cache, and storage rationale

CppSyntaxCore charges every shared-parser consume before advance(). parse_array_bound() now uses the charged literal consumer; no raw parser helper moves position_ directly. PA7 syntax work is 1024 + 64 * token_count with nesting bound 4096. Lookup uses an iterative DFS worklist and a reusable generation-mark vector: every reachable anonymous/inline/using namespace is marked once per uncached lookup, child order precedes using-directive order, and cycles terminate without a namespace-count zero-fill allocation.

Each lookup cache key is complete: (start NamespaceId, NameId, LookupCategory, LookupMode {in-namespace, unqualified}, category epoch). Namespace creation/reopening topology changes, namespace aliases, and using directives advance all category epochs. Type aliases and using-type declarations advance only the type epoch; values and using-entity declarations advance only the entity epoch. Caches are cleared on invalidation, so stale generations cannot amplify storage; a redeclaration that keeps the same EntityId does not invalidate entity identity. An uncached lookup is bounded by reachable graph size; a cache hit is expected O(1), with bounded linear probing.

FlatHashIndex uses power-of-two capacity (minimum 8), a 70% load threshold, insertion-order entries_, slot-to-entry indices, bounded probes, explicit capacity/entry overflow failures, and no hash iteration in output. Key equality/hash fields are complete for LookupCacheKey and TypeKey; collision/full-table paths terminate with errors. Empty per-namespace indexes allocate no slots. The three lookup caches are also empty until used and the measured repeated-query case retained only two entries/eight slots. Canonical TypeKey vectors remain intentionally owned by renderable TypeRecords and one canonical index entry; no node-map or empty-slot key copies remain.

PA7 emits a semantic dump, not generated code or objects. Code size, instruction count, object size, and link metrics are therefore inapplicable; the applicable evidence is time, RSS, semantic output bytes/hashes, and structural owner counters.

## Scaling and profile evidence

All audit binaries below used Linux x86_64, g++ 15.2.0, -std=gnu++11 -Wall -O3, equivalent environment, and -DPA7_AUDIT_COUNTERS:

~~~sh
AUDIT=/tmp/pa7-correction-audit.bZLkto
mkdir -p "$AUDIT/ae1-source"
git archive ae1d309b20a2ac6261550fa2b3212b9a3cb5199d | tar -x -C "$AUDIT/ae1-source"
make -C "$AUDIT/ae1-source/dev" nsdecl CPPGM_STDLIB_FLAGS=-DPA7_AUDIT_COUNTERS -B -j2
make -C dev nsdecl CPPGM_STDLIB_FLAGS=-DPA7_AUDIT_COUNTERS -B -j2
cp "$AUDIT/ae1-source/dev/nsdecl" "$AUDIT/ae1-nsdecl-audit"
cp dev/nsdecl "$AUDIT/candidate-nsdecl-audit"
sha256sum "$AUDIT/ae1-nsdecl-audit" "$AUDIT/candidate-nsdecl-audit"
~~~

The immutable ae1d309b executable hash was 31c100f375f958d1f36dccd5e5cf0d42da5cdf0dc8a147d7cca7d5518be30421; the corrected candidate hash was b52312b2c43fe78022c63ad9deac95cd361dabac0a93cb6860197e5d97442127. The c75b6088 node-map-era audit executable used for storage characterization was ac2ee6ca75ffb953bd4f06a9288c9ff8a536f9700c36f503f523d1e03416974d.

Lookup inputs were temporary /tmp files with this exact shape, for N = 50, 100, 200, 400, 800, 1600, 3200, 6400:

~~~text
namespace N0 { typedef int T0; }
namespace Ni { using namespace N(i-1); }  for i = 1 .. N-1
using namespace N(N-1);
T0 v0; ... T0 v(N-1);
~~~

measure_lookup.sh interleaved five baseline/candidate runs per N with /usr/bin/time -f 'elapsed=%e user=%U sys=%S rss_kb=%M' binary -o output input, then cmp'ed every pair. Median fields are elapsed/user/system seconds and maximum RSS KB; these are characterization measurements, not a claim of statistically significant timing precision.

| N | ae1 elapsed/user/sys/RSS | corrected elapsed/user/sys/RSS | ae1 visits | corrected visits / cache hits / misses |
|---:|---:|---:|---:|---:|
| 50 | 0/0/0/4092 | 0/0/0/4076 | 2,649 | 150 / 149 / 151 |
| 100 | 0/0/0/4344 | 0/0/0/4328 | 10,299 | 300 / 299 / 301 |
| 200 | 0/0/0/4844 | 0/0/0/4840 | 40,599 | 600 / 599 / 601 |
| 400 | .01/0/0/5828 | 0/0/0/5824 | 161,199 | 1,200 / 1,199 / 1,201 |
| 800 | .02/.02/0/8020 | .01/0/0/8016 | 642,399 | 2,400 / 2,399 / 2,401 |
| 1600 | .07/.06/0/12172 | .02/.01/0/12180 | 2,564,799 | 4,800 / 4,799 / 4,801 |
| 3200 | .27/.25/.01/20808 | .05/.03/.02/20828 | 10,249,599 | 9,600 / 9,599 / 9,601 |
| 6400 | 1.13/1.09/.03/37832 | .10/.07/.03/37820 | 40,979,199 | 19,200 / 19,199 / 19,201 |

Candidate query counts are 6N and the two stable cache entries are reported at every row; the old path's visits are quadratic in this N-chain/N-query family, while the corrected structural work is exactly 3N. Baseline and candidate output bytes/hashes were identical at every row: N=50 d6150962c115b71a9913559e9b6168bd9b8e5634d2011878a0ac8946b59a2fd4; N=6400 b2108e572a5cf6fc11b949658135aecfc971964e834333853c3f7af485333fe5.

For the FlatHashIndex storage check, one-run characterization on the same inputs compared c75b6088 (node maps), ae1d309b (flat indexes, no new cache), and the corrected candidate. At N=100/400/1600, c75/ae1/candidate elapsed/user/sys/RSS were respectively 0/0/0/4304, 0/0/0/4316, 0/0/0/4356; 0.01/.01/0/5848, 0.01/0/0/5808, 0/0/0/5844; and .12/.11/0/12172, .08/.06/.01/12156, .02/.01/.01/12164. All outputs matched (N=1600 hash 63e3983c5ee737468a5a51441ff7060d52ade0310c362b01c802d0c3e3412b2b). These single runs are storage/time characterization, not a controlled isolated index benchmark; they show no material representative RSS loss from the flat ownership layout.

Nested compound rendering used actual type wrappers, not redundant parentheses:

~~~text
typedef int B0;
i mod 3 == 1: typedef B(i-1) *B(i);
i mod 3 == 2: typedef B(i-1) B(i)[1];
i mod 3 == 0: typedef B(i-1) B(i)();
B(N) value;
~~~

measure_nested.sh ran three interleaved baseline/candidate repetitions for N = 32, 64, 128, 256, 512, 1024, 2048, 4096, with pairwise cmp. The 4096 candidate median was .03/.02/.01 seconds/RSS 14900 KB and 64,334 output bytes; N=32 was 0/0/0/RSS 3816 KB and 658 bytes. All 24 pairs were byte-equivalent. Representative candidate output hashes were N=32 79b86effd66a793d35a3240acf2bd58e59a3db6cd063ce7c28f86eb8ff0f0051, N=2048 c13e9e9bfc4eb051a80ad769d2b1fbc66e739d36efbf68e8e61d991117a13259, and N=4096 8be573cedb522eda86f3a0f69442b8c570ccd31320361474d84619b50cc87a8c.

Function task order and variadic output were checked by pa7/tests/310-varargs.t, pa7/tests/360-function-typedef.t, and cppgm.tests/course/pa7/350-parenthesized-parameter-declarators.t; output preserves parameter order, nested function/array/pointer adjustment, and ... placement.

Token-size check compiled the same sizeof_tokens.cpp against ae1d309b and the corrected source: sizeof(CppSyntaxToken) changed from 64 to 56 bytes; sizeof(CppSyntaxTokenCollector) is 40 vs 48 bytes because the latter now contains one optional observer pointer, not PA6 per-token fields. PA7 passes null, so the observer is cold and no compatibility facts are constructed.

## PA6 reference provenance

Before adding the four regressions, TESTING_AND_REFERENCES.md was read in full. The documented command was run exactly as make -C pa6 ref-test TEST='course/pa6/275-shared-*.t'. The pinned recog-ref reports OK for these four inputs, but the PA6 README's lexical-category rule and the direct repaired/legacy dev/recog result are BAD. The generated reference files are retained as provenance; their expected per-file output was corrected to BAD for this real late defect so the fixture cannot mask the earliest-owner regression. No unrelated test or reference was touched, and no reference binary is used by production code.

## Validation

- make test-pa6: 52/52, including all four new earliest-owner regressions.
- make test-pa7: 41/41.
- Focused PA7 scope/ambiguity, array completion, reference collapse, function/parameter, and variadic cases passed through the existing pa7 test harness.
- Required make test-report-through-pa7: 338/338.
- Required perl scripts/cppgm_file_audit.pl --stage pa7 --paths dev/src: passed.
- git diff --check: passed before staging.
- No new dev/src/*.cpp file was added; dev/frontend_source_sets.mk therefore required no change.

## Checkpoint ledger

1. c75b6088c15eec119842e083ab23bbc8f10a6408: clean PA7 semantic model, but node maps, recursive lookup/render paths, and parallel PA6/PA7 collectors/parsers remained.
2. ae1d309b20a2ac6261550fa2b3212b9a3cb5199d: reviewed intermediate; shared cursor and earlier iterative/flat repairs were accepted, but PA6 policy exactness, PA7 cold-sidecar discipline, scope soundness, and broad lookup scaling remained blockers.
3. Corrective milestone: canonical observer-backed token owner, policy/action grammar boundary, semantic type queries, typed generation cache/invalidation, FlatHash guard, and four earliest-owner PA6 regressions.
4. Final handoff: required validation, staging, one worker commit, and empty git status --short; the commit containing this audit is the correction commit whose exact hash is reported with the final handoff.
