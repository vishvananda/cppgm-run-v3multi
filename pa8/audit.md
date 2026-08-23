# PA8 Final Boundary Audit

## Scope and result

This audit covers the full PA8 production path at entry commit
be6b2300, including the stage chain 8b56021c through be6b2300. The final
worker gates report file-audit exit 0, PA8 94/94, through-PA8 433/433, and
8/8 stages.

One real performance defect was found and repaired in
dev/src/pa8_semantic.cpp: string-array conversion copied the complete
PA8Value::bytes aggregate once per element. No handout test or .ref fixture
changed.

## Spec and architecture alignment

- Section 1 has one production path: nsinit preprocesses each source,
  cpp_declaration_syntax parses the typed posttoken stream, PA8 publishes
  canonical semantic facts, and nsinit.cpp writes the resulting image.
  frontend_source_sets.mk already wires exactly this path for nsinit.
- Sections 2--4 are represented by spelling/literal identities, NameId,
  TypeId, NamespaceId, EntityId, TypeKey, typed lookup buckets, generation
  marks, category epochs, and declaration-owned records. No classified PA8
  fact is rendered and reparsed.
- PA8's N3485 obligations are owned at the relevant boundary: 3.4.3p3
  qualified declarator scope, 3.5 linkage and redeclaration ownership, 5.19
  constant-expression truth, 7.1.5 constexpr cv, and 8.5.2 string-array
  completion and element initialization.
- Section 7 is addressed by the immutable before/after executable comparison
  below. The measurement is explicit about its process-level limits; it is
  not presented as phase, allocation, or asymptotic proof evidence.

## Representative typed ownership traces

1. PPTokenBuffer retains token kinds, fixed identities, spelling IDs, and
   source locations. posttokenize_cpp_tokens emits decoded LiteralData into
   CppSyntaxToken; the streaming declaration parser carries typed
   expressions, CppSyntaxDeclaratorOp bounds/layers, and spelling IDs to
   PA8SemanticActions. No source spelling is reconstructed for semantics.
2. process_declarator resolves a qualified target before
   model_.declarator evaluates bounds and parameter types. It requires
   is_enclosing_namespace(target, current_), applies DeclaratorShape to the
   canonical base TypeId, and then declare_entity uses external NameId
   buckets or (NameId, translation_unit) internal buckets.
   variable_types_compatible and merge_types walk only parallel array
   layers; the declaration owner publishes initializer facts through
   initialize_variable_entity.
3. PA8Value keeps known bytes separate from is_constant_expression, and
   separately carries named entity, referent, relocation, and addend
   identities. EntityRecord is the durable owner of constant bytes,
   constant-expression truth, and relocations; id-expression evaluation
   reads those records without textual recovery.
4. Reference binding preserves typed referents or materializes typed
   temporaries. String literals become typed const-code-unit array entities.
   build_image performs the three entity-order passes—named variables and
   functions, temporaries, then string literals—and a later relocation pass
   patches typed offsets after all targets are planned. dev/nsinit.cpp writes
   that image.
5. Canonical type formation is indexed by TypeKey; lookup is bounded by
   typed buckets/ranges, generation marks, and category epochs. Linkage
   candidates are bucket-bounded, array compatibility is depth-bounded, and
   image planning is three linear entity scans plus one relocation scan.

## Finding and actual change

The old loop in convert_value did PA8Value element = source for every string
element. Since the value owns the full literal byte vector, an N-element
literal copied O(N) bytes N times before replacing them with a one-element
slice.

The repaired loop creates one reusable scalar PA8Value and one reusable
conversion buffer outside the loop. It preserves the source element type,
known-byte fact, constant-expression fact, and zero element count. The
synthetic scalar view explicitly leaves lvalue and null_pointer false; it does
not inherit those aggregate value-category flags. Each iteration copies only
the element slice and clears entity, referent, relocation, and addend
identities because the scalar view is not a separately stored entity. The
adjacent operations audit found no second aggregate-copy or
canonical-ownership defect.

Recursive layout was specifically checked with external equivalent typedef
chains of 256, 1,024, 2,048, 4,096, 8,192, and 16,384 nested one-element
array layers. All completed successfully, with wall times of approximately
0.00, 0.01, 0.02, 0.04, 0.07, and 0.16 seconds and the same 8-byte image
hash `255c6cc22bb64c29ce77b776beec5c6ec861d0f9a61d65b1420baf52f040520a`.
This is consistent with one O(depth) layout walk. The recursive path has no
remaining unexplained cost at the measured depths, so no speculative redesign
or profile was required.

## Scaling evidence

The focused harness used immutable executable copies with identical size
613,552 bytes:

- before repair:
  998c304da07f5ec87b373bac42853f14323cd5c0110a5576126902110eb775b1
- after repair:
  c14ee5ea24804f18123ac18df526faf7591c0e76e9468fa1285100de0199f88a

Each generated input was char value[] = "aaa...", with the terminating code
unit included in the listed unit count. The four-input manifest hash is
fbd2485a35164eceac0367a9f0c1004797c18a3e9cafbb1a82acae50e7aa03cb. Each
before/after pair ran three times in an interleaved order under
/usr/bin/time -f '%e %U %S %M'; output size and SHA-256 were checked on every
run.

| code units | input bytes | output bytes | output SHA-256 | before median wall/user/RSS | after median wall/user/RSS |
| ---: | ---: | ---: | --- | --- | --- |
| 4,096 | 4,114 | 8,196 | `cdac3601f1358632aeada5edbea223c6d34df5cd89fb948f648629be6cc177f5` | 0.00 / 0.00 / 3,828 KB | 0.00 / 0.00 / 3,844 KB |
| 16,384 | 16,402 | 32,772 | `7ef283cec4e42473d1bc04f0c03bbaa151ff968c1781dc159bd612768c525090` | 0.00 / 0.00 / 4,488 KB | 0.00 / 0.00 / 4,472 KB |
| 65,536 | 65,554 | 131,076 | `9d7d71b1fad15f4e0bf166b0c8757253e3b8aa79fe3f264dc025c1952c17c053` | 0.18 / 0.18 / 6,968 KB | 0.01 / 0.01 / 6,984 KB |
| 262,144 | 262,162 | 524,292 | `75f0248ba6fd9e86d7436decaad4a6e69718cc34da19505c79b41023a6f503f8` | 2.65 / 2.63 / 16,856 KB | 0.05 / 0.04 / 16,872 KB |

The small cases are below the 0.01-second timer resolution. The larger
medians corroborate removal of the aggregate copy and preserve deterministic
output, but the harness has no phase/allocation/work counters, no profiler,
and no generated-program quality measurement beyond image size/hash. It
therefore supports the observed scaling repair, not a formal complexity
proof. The repaired string-array curve and the alias-depth curve have no
remaining unexplained cost; profiling was not required.

## Focused validation and limits

- `make -C dev -B nsinit -j2`: exit 0.
- Exact focused selection:

  ```sh
  make -C pa8 check TEST='tests/310-array-str-lit.t.1 tests/340-array-const.t.1 tests/500-static-assert.t.1 tests/500-static-assert3.t.1 tests/600-qualified-redeclaration.t.1 tests/600-qualified-redeclaration2.t.1 course/pa8/120-constexpr-pointer-cross-tu.t.1 course/pa8/120-constexpr-qualified-pointer.t.1 course/pa8/200-char-before-function-alignment.t.1 course/pa8/300-function-typedef-definition-bad.t.1 course/pa8/430-reference-to-array-valid.t.1 course/pa8/431-reference-array-function-layer-valid.t.1 course/pa8/431-reference-function-layer-valid.t.1'
  ```

  Exit 0; `pa8 check: PASS (13/13)`.
- `perl scripts/cppgm_file_audit.pl --stage pa8 --paths dev/src`: exit 0;
  `File audit passed for pa8 with 1 warning(s).` The warning is
  `dev/src/cpp_semantic_core.h:1 [bad-division] header contains substantial
  implementation body; prefer .cpp ownership`. It is non-blocking
  organizational debt at this PA8 boundary: there is one shared production
  implementation, no duplicate model or path, and the required audit exits 0.
- `make test-report-through-pa8`: exit 0; `ALL TESTS PASSED SUCCESSFULLY!
  (433 / 433)` with 8/8 stages.
- `git diff --check`: exit 0.

Evidence remains limited by process-timer resolution and the absence of
phase/allocation counters; it is not a formal asymptotic proof. No correctness,
architecture, performance, self-containment, timeout, or file-audit blocker
remains. No new reference or fixture divergence was introduced or investigated.

## Checkpoint ledger

| checkpoint | durable scope |
| --- | --- |
| 8b56021c | shared typed PA8 semantic core, streaming syntax boundary, and real nsinit production wiring |
| 8ee86ae7 | cross-TU source/link identity, declaration ownership, generation-marked lookup, and first durable audit |
| bf249b24 / affab90c | namespace occupancy and bounded using-entity/type conflict ownership |
| 657e5559 / 2fd0f353 / fc6c4eb4 | typed reference formation/binding, lifetime temporaries, array/reference invariants, and nested-layer correction |
| 97a0f724 | compact PA8 checkpoint totals and failure-map refresh |
| be6b2300 | typed initialization, linkage inheritance/merging, string entities, relocation facts, and image ordering |
| final worker checkpoint | reusable linear string-array element conversion with scalar value-category correction, final-boundary audit consolidation, focused 13/13 validation, file audit exit 0, through-PA8 433/433, and clean final review |
