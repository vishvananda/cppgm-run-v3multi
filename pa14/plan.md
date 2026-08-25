# PA14 Checkpoint Plan

## Stage Design

The line adapter in `dev/abimangle.cpp` parses normalized facts into the
typed model in `dev/src/abi_mangle.h`; `dev/src/abi_mangle.cpp` is the shared
append-based encoder. Structural substitution keys contain typed domains,
components, scalars, and child identities. Rendered ABI text and adapter
spellings are never semantic keys; name edges use the existing trie and
structural candidates publish in ABI order.

The 600 boundary emits local names directly, tracks template-argument depth
with one counter, and distinguishes direct `template-param` from explicit
`template-param-subst` publication without changing identity. It represents
`template-param-template <index> <arg-ref>...` as a typed specialization,
does not register its complete dependent prefix-plus-arguments form as a
type candidate, and handles an owner-template constructor terminal after the
owner arguments. The public enum has an explicit validity predicate; the
encoder and cold serializer reject invalid values. Empty template-template
argument lists are rejected at both typed boundaries.

The new state is O(1) per case. Existing `std::map` structural/trie lookups
remain O(log n), with typed key comparison proportional to operand width; no
retry, rescan, rendered key, or growing candidate-vector path was added.

## Failure Map

Clean HEAD `bf99cd9c` started this checkpoint at 104/111: 100=25/25,
200=25/25, 300=37/37, 400=4/4, 500=13/13, and 600=0/7. The complete 600
failure set was:

1. `function-local-class-template-arg`: got `N2ns4WrapIZNS_4makeEvEN5LocalEEE`, expected `N2ns4WrapIZNS_4makeEvE5LocalEE`.
2. `function-template-local-class-arg`: got `_Z1gIZ1fvEN1XEEiv`, expected `_Z1gIZ1fvE1XEiv`.
3. `function-template-local-lambda-arg`: got `_Z5applyIZ4hostiEN3$_0EEiT_`, expected `_Z5applyIZ4hostiE3$_0EiT_`.
4. `inline-namespace-basic-string-param`: got `_ZSt7getlineIT_ERNSt7__cxx1112basic_stringIS0_St11char_traitsIS0_ESt9allocatorIS0_EEE`, expected `_ZSt7getlineIT_ERNSt7__cxx1112basic_stringIT_St11char_traitsIT_ESt9allocatorIT_EEE`.
5. `nested-helper-owner`: got `N2ns5OuterIT_NS_5AllocIS1_EEE12_Guard_allocE`, expected `N2ns5OuterIT_NS_5AllocIT_EEE12_Guard_allocE`.
6. `template-param-template-type-substitution`: the adapter rejected `template-param-template` with `unexpected extra ABI fact fields`; expected `_ZN2ns3useERT0_IT_ES1_`.
7. `template-parameter-pack-reference-constructor`: the encoder rejected `ABI function has no name components`; expected `_ZN5ownerIT_EC1EDpRKT_`.

## Active Checkpoint and Spec Alignment

All seven 600 outputs now match their checked-in refs. The adapter/model/
encoder boundary remains typed: local entities, template arguments,
template-parameter specializations, owner terminals, and substitution modes
are represented as fields and enums. Validation occurs before malformed facts
can acquire ABI output, while the serializer preserves both
`template-param-template` and `template-param-subst` spellings.

The implementation follows `spec.md` §§1--4 and §7 and Itanium Chapter 5.1
for template arguments, local names, template parameters, compression, and
left-to-right candidate publication. The enum mode is not part of structural
identity. The complete dependent specialization is not a type candidate;
its parameter prefix and argument references retain normal ordering.

Durable public coverage is in
`cppgm.tests/course/pa14/400-public-typed-model-regression.{cpp,sh}`. It
checks both happy-path mangles, exact parse/serialize/parse stability for
`template-param-template` and `template-param-subst`, empty-argument
rejection in the encoder and serializer, and invalid-enum rejection in both.

Validation results:

| command | result |
|---|---|
| `make -C pa14 check TEST='tests/abi/600-*.t'` | PASS (7/7) |
| `make -C pa14 check TEST='../cppgm.tests/course/pa14/*.t'` | PASS (10/10) |
| `CXX=${CXX:-g++} sh cppgm.tests/course/pa14/400-public-typed-model-regression.sh` | PASS |
| `g++ -std=c++11 -Wall -Wextra -Werror -Idev/src -fsyntax-only dev/src/abi_mangle.cpp dev/abimangle.cpp` | PASS |
| `make test-pa14` | PASS (111/111) |
| `n=14; if [ "$n" -le 1 ]; then echo '===== ALL TESTS PASSED SUCCESSFULLY! (0/0) ====='; else make test-report-through-pa$((n - 1)); fi` | PASS (947/947) |
| `make test-report-through-pa14` | PASS (1058/1058) |
| `perl scripts/cppgm_file_audit.pl --stage pa14 --paths dev/src` | PASS; 4 known nonfatal header warnings |
| `git diff --check` | PASS |

No handout test, reference, harness, or generated repository file changed.

## Performance Evidence

The immutable candidate was copied after the final source build to
`/tmp/pa14-typed600-bench-final.CCFpU8/candidate`, mode 0555, size 484,664
bytes, SHA-256
`ae56d2130e54a63fca117b624230f309bcdeaaf097b9821a9775f91b63c800d0`.

Inputs were generated at 1024, 2048, and 4096 scales with fixed-width
components `Owner0000`/`Spec0000` and repeated direct `T_` references nested
in each `I...E` template argument list. Each unique fixed-width owner
specialization was passed by reference to `ns::use`, growing typed owner and
specialization state without widening names. Seven samples per size were
interleaved in 1024/2048/4096 order; each used
`/usr/bin/time -f '%e %U %S %M'` around the immutable candidate. Medians:

| scale | fact lines | input bytes | output bytes | wall | user | sys | max RSS |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1024 | 3,075 | 93,256 | 19,468 | 0.09 s | 0.05 s | 0.03 s | 24,724 KiB |
| 2048 | 6,147 | 186,440 | 38,924 | 0.19 s | 0.11 s | 0.08 s | 45,648 KiB |
| 4096 | 12,291 | 372,808 | 77,836 | 0.39 s | 0.21 s | 0.16 s | 87,500 KiB |

Source inspection confirms the depth counter is a scalar in
`SubstitutionState`, enum-mode handling is a constant comparison, and
`StructuralKey`/name-trie child tables retain `std::map` O(log n) operations.
This workload shows near-doubling with fixed-width inputs and outputs; it is
not an allocation or general asymptotic proof beyond this workload.

## Checkpoint Ledger

| checkpoint | starting result | concise outcome |
|---|---|---|
| PA14 typed-300 boundary | 88/111 with 23 failures | 100/200/300 preserved at 25/25, 25/25, 37/37; through-PA13 947/947; audit and typed identity review recorded. |
| PA14 dependent 400/500 boundary | 104/111 with seven 600 failures | 400/500 focused coverage 17/17; 100–300 preservation 87/87; serializer and public typed-model checks passed. |
| PA14 typed 600 boundary | 104/111; exactly the seven failures above | 600=7/7, PA14=111/111, through-PA13=947/947, through-PA14=1058/1058; public boundary and benchmark evidence recorded. |

Final handoff uses commit message `PA14: complete typed ABI name boundary`;
the post-commit `git status --short` result is empty.
