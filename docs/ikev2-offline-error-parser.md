# Disconnected IKEv2 draft failure parser

Specification and acceptance recorded before code, 2026-11-05.

This isolated Desktop task starts clean at `dbcf2dc` on
`feat/ikev2-offline-error-parser`. Core owns the shared draft schema. The
read-only proposal in `plan-ikev2-error-contract/docs/ikev2-error-contract-plan.md`
is context, not permission to independently freeze a format. Implementation waits
for Core's exact frozen fixture path and SHA-256; record them here before code.
This is a draft fixture contract, not a published protocol.

Existing Transport strictly validates its v1 status/events and strips LastError,
ErrorStage and Message. Session exposes symbolic safeError without numeric attempt
evidence. D3 provisioning consent and cleanup/closure rules already exist. Leave
all those production paths, pins, gates, capabilities and UI unchanged.

Implement a pure, disconnected parser with typed output and absent/valid/invalid
results. It must clear output on every parse, retain only schema-defined numeric
evidence, and validate the frozen version/domain/stage/slot/attempt rules. Numeric
evidence is neither authentication diagnosis nor lifecycle/cleanup authority.
775 has unknown local cause; 668 cleanup failure cannot prove successful operation.

Acceptance (apply the exact Core draft, without adding a competing format):

- Consume Core's frozen synthetic fixtures and verify their byte hash.
- Preserve operation and cleanup independently, including simultaneous 775/668,
  unknown valid codes and numeric limits; distinguish absent from malformed.
- Reject malformed JSON, wrong types/ranges, unknown fields/version/domain/stage,
  wrong slot, oversize and attempt mismatch. Never retain an earlier result after
  a failed/absent parse. Do not reconstruct evidence from diagnostic strings.
- Demonstrate no raw diagnostic sentinel survives in typed output; no logs, UI,
  persistence, network, native API or authority-bearing behavior is introduced.
- Integrate focused pure fixtures with the existing test harness where feasible;
  run a parser-only entry point to avoid unrelated native/IPC fixture execution.
- Review the final diff and run whitespace checks. No production build/packaging
  flow change, dependency, release matrix, performance benchmark or profiling is
  warranted for this bounded parser. Report actual host checks and limitations.

No staging, commits, push, merge, release, active lab/Core installation-contract
checkout, VM, SSH, runtime network, service, task or credential access is allowed.
Native stays OFF. No subagents are necessary for this bounded worker task.

Frozen handoff verified before implementation: Core fixture SHA-256
`946B21470B8E39DF4AC55FB7762D1CF6D90B91D17D623E0A22B0DA45F97E8E2E`,
spec `8A82427A00AB18AD982DD3507DB6490FE6E47A5D358978D1DD3BC5B11361A28B`
from `bimeron-core/feat-ikev2-offline-error-model/core/testdata/ikev2-error-draft-v1.json`
and `docs/ikev2-offline-error-draft.md`. Implement the exact member as frozen.
The parser accepts member bytes plus an explicit enclosing attempt; absence uses
an explicit presence flag. It enforces the 512-byte limit before JSON decoding.

Validation: standalone Windows x64 MSVC 19.51 Release/Qt 6.8.3 Core-only
harness built and passed (exit 0), including byte-identical hashed Core fixtures,
independent operation/cleanup preservation, malformed/bounds/type/attempt cases,
and output clearing. No GUI/native/IPC suite was executed. Harness uses
`tests/OfflineIKEv2FailureTest.cpp` and accepts the fixture path as its only
argument; local CMake driver and artifacts remain ignored in build/offline-parser.
No production build registration or packaging flow changed. Diff whitespace check
passed. Review found no production references, diagnostic text retention or new
authority. Linux/ARM64 and full CI remain untested; no release claim is made.
Benchmark/profiling is not applicable. All changes remain unstaged/uncommitted.

## Raw preflight follow-up design (MEDIUM, before code)

Verified frozen addendum SHA-256
`AE86BA2537FD1E22C19612E7A1EF03700290D45958D24605D11E8BB2B3D0C451`
and raw corpus `FACD9603E45A910235D38326C1271AC52ABC1AC5E69DAAA55544F0B6D13A8A9B`.
Original frozen files remain unchanged. Add a fixed-schema scanner with a cursor
bounded by 512 bytes, root at depth one and records at depth two only. Object
loops allow at most three/four properties respectively and reject duplicate Qt-
decoded names and unknown names before values. No arrays or deeper objects.
Locate string tokens by consuming escaped bytes together; decode each token via
Qt in an array wrapper, validate resulting UTF-16 surrogate pairs, and validate
input UTF-8 using QStringDecoder without repair. This delegates escape/Unicode
parsing to Qt. Only numeric fields accept raw [1-9][0-9]* tokens; semantic ranges
remain checked by the existing typed decoder. Require exact end after JSON
whitespace. Retain final Qt document parsing. No recursion beyond the single
explicit root-to-record call, no general JSON framework or runtime wiring.

Acceptance adds the exact shared raw corpus and local escaped quote/backslash,
Unicode-equivalent key, UTF-8/surrogate, nesting/trailing-data, range and 512/513
byte boundary cases. Independent focused review follows implementation; no paired
PASS until Core confirms the same corpus. Expected scanner size: about 80 lines.

Raw follow-up implementation: `OfflineIKEv2FailureRaw.h` adds a bounded fixed-shape
preflight (70 lines), with Qt UTF-8 decoder, Qt string-token decoding, decoded
surrogate validation, decoded-name duplicate rejection and positive integer
lexemes. Final Qt JSON/semantic validation is retained. Windows x64 MSVC Release
build and parser harness passed all 33 hash-verified raw cases, original 15
cases and local output-clearing, malformed UTF-8/surrogate, escaped quote/backslash,
object/array, trailing-data and 512/513-byte checks. Frozen files unchanged.
Independent focused review is requested from parent before paired closure; this
is not production readiness, runtime validation or full CI. No benchmark applies.


Review r1 correction specification (before code): independent Sol HIGH review
found Qt accepts illegal backslash escapes that the scanner had delegated to Qt.
Add the JSON escape allowlist and exactly four hexadecimal digits after u before
Qt token decoding. Retain existing surrogate and size checks. Add key/value
regressions and legitimate escaped-backslash/Unicode token cases. Preserve r1
findings; request fresh r2 rather than claiming approval. Remove extra EOF blank
lines in owned code/doc only; frozen fixtures remain byte-identical.

Correction validation: x64 Release standalone parser rebuilt and passed original
15/shared33/local cases plus strict escape key/value regressions and legitimate
backslash, quote, Unicode and surrogate-pair preflight cases. Explicit untracked
git diff --no-index --check against NUL passed for all four owned code/doc files
(no whitespace errors; only configured LF-to-CRLF notices). Both frozen fixture
hashes remain unchanged. Fresh independent r2 review requested; not self-approved.

Review r2 accepted implementation but found generated escape tests semantically
invalid even with the old scanner. Added exact raw key backslash+version and
value d+backslash+ial regressions, otherwise valid. Current Release harness passes.
An ignored copy under build/offline-parser/mutation restores old escape skipping;
the same persistent harness fails with FAILED: exact illegal escape regression,
exit -1073740791. No production source or frozen fixture changed in this correction.
Parent narrow verification remains pending; no self-approved review claim.

Parent narrow verification completed: exact test/semantic/raw source hashes
BF544AAD/5D857D58/026EA969 matched; persistent exact raw cases and failure message
were inspected. Parent ran current parser against original and raw fixtures
(exit 0) and the retained old-scanner mutant (exit -1073740791 with the exact
illegal-escape regression message). The r2 test gap is closed, with implementation
unchanged since HIGH code acceptance. Shared-corpus parity covers the original
15 and supplemental 33 synthetic cases only; no production wire, live native,
full CI or complete cross-platform acceptance is claimed. Changes remain unstaged
and uncommitted. Thread settlement retains history and review evidence.
