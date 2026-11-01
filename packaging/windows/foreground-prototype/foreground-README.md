# Bounded elevated-client and settlement prototype

## Selected trust revision and next disconnected fixture

The [trusted invocation procedure](../../../docs/ikev2-profiles.md#trusted-invocation-procedure-selected-specification-review-pending)
is selected for specification revision and independent review only. Production
remains disabled. Callbacks independently check their own effective authority;
they cannot attest that the original client followed the already-fully-elevated,
same-user interactive invocation procedure. Reliance on that procedure is a
deliberate residual risk, acceptable only after independent review. The original
`verifyInitiatingClient` stays Unknown. No code or fixture result is relabeled.

Accepted bounded implementation (specification committed before code):

- Add CallbackInteractiveContextObservation, never Verified, with bounded refusal
  and numeric query status, token source/type/authority, SID/session/authentication
  LUID, selected TokenId and LSA status. No plaintext account names.
- Reuse THREAD-first selection, PROCESS only on ERROR_NO_TOKEN; retain that exact
  handle through token queries, LSA comparison and explicit-token AccessCheck.
  Never RevertToSelf or override a filtered/error thread with process authority.
- Require elevation, high integrity, enabled non-deny-only Administrators,
  unrestricted/non-AppContainer and adequate impersonation; Full type alone is
  insufficient and nonsplit administrators are positive models.
- Validate LSA data bounds and exact LUID/SID/nonzero-session match; allow only
  logon types 2 and 10. SYSTEM/service principals and unknown data refuse.
- Future Preflight/PostFinalize inspect the CURRENT interactive principal.
  Deferred/Commit/Rollback permit privileged SYSTEM access without an interactive
  claim or Clean. Intended same-user invocation remains procedural; another full
  administrator can pass local eligibility. Protected designated-user/continuity
  reference gates, if required, remain future work. No receipt self-comparison.
- Keep original initiating-client Unknown, both always-failing exports and existing
  eligibility/settlement semantics. No production caller, MSI or preload work.
  Package/Binary/loading trust and future pins remain separate requirements.
- Test actual current/impersonated/restricted/anonymous/identification contexts
  where available, exact selected TokenId, query/fallback/full-field failures,
  malformed LSA and SID/session/LUID mismatches, SYSTEM with nonzero session,
  type allowlist, descriptor access denial and nonsplit positive models. Report
  unavailable native contexts explicitly; models do not establish native positives.
- Build x64/ARM64 /W4 /WX, run focused x64 and existing fail-closed regressions and
  ARM64 source/PE statics. No MSI builds, VM, GUI/Linux matrix or benchmarks.

All runtime/readers, enrolled updater/rollback compatibility, continuous mutex,
terminal settlement and native-proof gates remain. Core's 64-byte record and
Desktop's 48-byte Pending fixture are distinct formats, not interchangeable proof.
The dated acceptance/evidence sections below are historical records of their own
fixtures, including Session 0 access-only labs; they do not approve this revision.

## Acceptance for the separate foreground trace lab

This section specifies a proposed disposable-VM experiment; it does not authorize
production enrollment or change the existing fail-closed fixture. The lab is off
by default, built explicitly in ignored `build/foreground-lab-*` directories, and
has no install rule or production CPack connection. Its three separately identified
and hashed per-machine empty MSIs compile fixed Success, CommitFail, and
PostFinalizeFail behavior. Product, upgrade, and package identities are distinct
from one another, production, and the original fixture; no upgrade removes another
product. No files, components, application registry, service, network or other
payload are installed.

Five synchronous checked Binary exports observe preflight before Initialize,
rollback and deferred begin within the transaction, commit within the transaction,
and post-finalize after Finalize. Deferred, rollback and commit run without
impersonation. Only the compiled failure point returns installation failure. The zero-resource lab may remain unregistered: ProductState -1 before and after is expected. An explicit uninstall is only meaningful for a registered lab product and runs no extra actions in an unsuitable context. Read-only table and PE inspection must check exact action
types, conditions and order, identities, embedded digest, architecture, system
imports, static CRT, no delay imports and SYSTEM32 dependent-load flags. Normal ICE
validation remains enabled. Negative fixtures must reject modified flags, order,
actions, payload, identities and hashes.

Trace output is fixed, bounded and versioned through MSI messages. It records UTC,
process and thread identifiers and process creation time, effective thread token
or process token only on ERROR_NO_TOKEN, token class and impersonation level,
elevation, integrity, session, administrator group state, restricted status,
SHA-256 SID hash, bounded logon identifier, numeric status and intended return.
It never records account names, environment, properties, arguments, exceptions or
credentials. Query or logging failure means incomplete evidence. Similar runner
and callback fields do not authenticate the initiating client: that remains Unknown.

The manual guest runner accepts only the literal disposable-VM acknowledgment,
closed case and starting-token choices, a parent-communicated manifest SHA-256,
and optional exact hexadecimal expected hashes. It operates only in the exact
native Program Files `BimeronTraceClientLab` tree after resolving paths and
rejecting reparse points in ancestors. Positive sanitized CIM manufacturer/model
evidence is required but does not authenticate the VM; the parent confirms it.
The fixed-schema manifest binds variants, identities and artifact hashes. Its hash
is an external trust anchor; the parent separately verifies the runner hash before
invocation. Recheck context and hashes after the user opens an elevated shell;
never automatically elevate or accept arbitrary commands, properties or paths.

Invoke the verified native system msiexec with fixed quoted package and log paths,
fixed arguments and `/norestart`. Record runner identity, hash, context, time and
the returned process identity as observations. Wait at most five minutes; timeout
or an unresolved UAC/installer state stops all further cases and cleanup without
killing processes. Never automatically run a failure case after Success or uninstall
an ambiguous result. Explicit uninstall uses only the known registered variant.
Summaries contain sanitized case, hash, token classes, callback order, exit or
timeout, and MSI registration-query state; full logs remain in the guest. Validate
syntax and offline predicates without invoking the guarded runner on the host.

The empty fixture can only observe callback token fields and ordering. It cannot
prove real resource rollback, exact initiating-client authentication, outer MSI
authority, a durable barrier, production approval or native enrollment. Parent
static review precedes any VM execution. No signing, credentials, network policy,
VM access, native clipboard, installation or release action belongs to this task.

## Acceptance recorded before implementation

Historical contract, before the selected trust revision above. Offline only, based on Desktop 2183b74 and Core 66d84d8f / 4366a5c9.
Native enrollment remains unavailable. Own only this directory and the canonical
build document appendix. No host registry, installation, elevation or networking.

Acceptance: effective thread token takes precedence; process fallback is legal
only for ERROR_NO_TOKEN. Descriptor fixtures require same SID/session/logon,
interactive, elevated high-integrity enabled administrator, unrestricted token and
adequate impersonation. Full elevation type alone is insufficient; nonsplit
administrators are allowed. No guessed client PID, parent or MSI property attests
identity. Real initiating-client correlation remains Unknown, never a permit.
Protected registry descriptor fixtures require native 64-bit view, safe ancestors,
trusted owner, protected non-null DACL, expanded registry rights and AccessCheck
against an explicit impersonation token. No host key is opened or written.

The exact own MSI must bind synchronous checked Binary DLL exports, removal after
InstallExecute before InstallFinalize, rollback/commit work inside Initialize to
Finalize and only a nonmutating acknowledgment after Finalize. Refuse disabled
rollback, patches, transforms, unsupported maintenance and mutation properties.
Inspect linked MSI tables and Binary bytes read-only if cached WiX is available. Require exactly the WiX 3 MajorUpgrade rows, downgrade launch condition, matching product identity and secured action properties. Reject missing, empty or altered upgrade and downgrade rows on both architectures.
External payload and signature trust are absent; effective runtime database trust
and production enrollment must refuse regardless of offline fixture results.

Replace completed booleans with bounded fixture outcomes: commit callback never
settles; missing callback, filtered context, death, failed flush, stale generation,
unknown inventory/protection/outer completion retain Pending. A successful local
Finalize and fixture acknowledgment do not prove eventual caller MSI success.
Fixture cleanup is not protected durable clean publication. Core still does not
read this journal; its continuous mutex contract has not changed. Worker/pin are
not crash safe. No helper, wrapper or IPC protocol is introduced.

Required checks: x64/ARM64 compilation, x64 model/worker tests, negative descriptor,
context, sequence and database cases, PE imports/delay imports, cached candle/light
and read-only MSI inspection. ICE unsupported is reported explicitly. Actual token
correlation, callbacks, privileged key access, durable publication, enrolled updater
and participating-host barrier, death/rollback and ARM64 runtime remain VM gates.
No benchmark is applicable to this disconnected offline gate.

## Build and check

Use installed VS 18 Build Tools and Python. From the repository root:

```powershell
cmake -S packaging/windows/foreground-prototype -B build/foreground-prototype-amd64 -G "Visual Studio 18 2026" -A x64 "-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools" "-DWIX_TOOL_DIR=C:/Users/PC/Documents/dev/bimeron/bimeron-desktop/.tools/wix"
cmake --build build/foreground-prototype-amd64 --config Release
ctest --test-dir build/foreground-prototype-amd64 -C Release --output-on-failure
```

Repeat configure/build with `-A ARM64` and a separate
`build/foreground-prototype-arm64-tools` directory. On x64, run only
`ctest --test-dir build/foreground-prototype-arm64-tools -C Release -R "foreground-sequence|foreground-msi-readonly" --output-on-failure`.
`WIX_TOOL_DIR` is optional and must name already cached WiX 3 tools; no download.
With it, the default build invokes candle and light (including normal ICE), and
CTest opens `Release/fixture.msi` with MSIDBOPEN_READONLY. No install session opens.
Generated `Release/fixture.tables.json` records table and embedded Binary digest
proof. Build output is an inert test fixture, not a distributable release.

## Implementation and evidence limits

The C++ context evaluator and settlement evaluator consume explicit offline
observations, not authorization credentials. `readEffectiveToken` is a read-only
current-context adapter, not a client locator: thread errors other than NO_TOKEN
refuse, and filtered thread tokens cannot inherit process privileges. It does not
query parent processes or MSI properties. `verifyInitiatingClient` always returns
Unknown because no exact client correlation primitive has been established.
Its OS adapter and positive privileged AccessCheck remain unproved in this task.
Journal access checks consume an in-memory descriptor and supplied token only;
there is no host key access. Null token, wrong view and unsafe descriptors refuse.
No arbitrary verified=true production argument exists.

The worker is retained only as an exclusion mechanics fixture. One OS thread
acquires/releases a borrowed test mutex. It writes CREATE_NEW temporary pending
bytes with write-through and FlushFileBuffers, never a production enrollment key.
`settleFixture` can delete only this temporary journal after the full bounded
PostFinalize fixture outcome; commit and rollback callbacks cannot do so. Teardown
and deliberate isolated child death retain pending. The worker cannot authorize
real mutation. DLL pinning protects code lifetime, not host process survival.
Core neither reads this journal nor implements the hypothetical registry barrier.
A verified local Finalize acknowledgment is distinct from eventual caller success.

The MSI has no installed file, registry, service or external payload. Its only two
custom actions are synchronous checked immediate type-1 Binary DLL exports; both
always return ERROR_INSTALL_FAILURE, independently of fixture model values.
The unconditional preflight is before Initialize, so disabled rollback, transforms,
patches or arbitrary properties can never obtain enrollment through this DLL.
The offline effective-input model explicitly rejects these and all maintenance
except the fresh fixture, with an empty mutation-property allowlist. It is not a
runtime effective-database verifier. The table verifier checks the exact execute
sequence, custom actions, source/target/type, conditions, directories, features,
properties, exact Upgrade and LaunchCondition rows and embedded digest, and rejects negative variants. The former validator accepted empty Upgrade and LaunchCondition rows in a baseline mutation; the revised validator rejects them and missing or altered rows. Synthetic settlement booleans are model inputs, not authenticated authority. Actual external
payload provenance/signature and a trusted effective runtime database are absent.
Static CRT, system-only imports, no delay imports and SYSTEM32 dependent-load flags
bound this DLL's dependencies; code-loading provenance in MSI still needs VM proof.

## Recorded offline result (2026-10-30)

Both MSVC Release architectures compiled. Cached WiX 3 candle/light linked both
MSIs. The initial MediaTemplate produced an empty Media table and ICE71. The
revised fixture uses an explicit Media row with DiskId 1 and LastSequence 0.
Normal verbose light validation ran ICE, including ICE71, on both revised MSIs
with no warning or error diagnostics; ICE09 noted that the Component table is
absent and cannot be checked. No ICE was suppressed. x64 CTest
covers worker/process exclusion, descriptor/context/settlement negatives, generated
WiX and actual read-only MSI/PE inspection. ARM64 runs only host-side source/MSI/PE
checks, never its executable. No performance-sensitive path changed; benchmarks
and profiling are not applicable. Linux and full GUI/package checks are omitted
because the standalone Windows project is not registered with production builds.
The new source-sequence checker also rejects the actual 2183b74 WiX source at
its old afterInstallInitialize schedule (build-local baseline-regression.log).
No production code, updater, transport, spec or production WiX was changed.

Historical remaining gates (the selected procedure above revises the original
initiating-client proof premise): required runtime interactive binding and
effective elevated callback token; protected native-view key/ancestor access
and durable flush; exact trusted runtime package and approved final signed Core
artifacts; real rollback/commit/nested-uninstall ordering, outer completion,
missing callbacks, filtered contexts, host death/reboot/sharing failures; enforced
participation of all runnable native/network/recovery hosts and enrolled updater
rollback compatibility. All require a separately authorized VM/contract phase.
Implementation is partial and production enrollment remains unavailable.

Revised fixture SHA-256 (local builds, not release artifacts):

| Architecture | MSI | Binary DLL |
|---|---|---|
| x64 | `14fe4bab14d0c5461dfdf0c1d55457dd8d32f92504eeb2a1477cd65a6be5562f` | `490fd505c74f3f76255dce226d2d4cb57acea9ff28f73e3c33e883be0cbe4625` |
| ARM64 | `aa8936c47484f5aca029552eb51bb591e10804356b24d6ce6cc309404789f5a9` | `6a13ef9aef96c9a23774cd7a11acec1c6b30eacbb130598811887f1a45faa7e8` |

## Phase 1 offline trace artifacts (2026-10-30)

Implemented the accepted separate lab; no runner belongs to this phase. The
`FOREGROUND_LAB_TRACE` option defaults OFF. Only the explicit
`foreground-lab-trace` target builds three observation-only DLL/MSI pairs; neither
these DLLs nor packages are in ALL or install rules. No root production registration
changed. The original DLL, worker, journal and always-failing exports are unchanged.
Success means observation return 0 only, never enrollment or authorization.

Configure the existing standalone project with the commands above, substituting
`build/foreground-lab-amd64` and adding `-DFOREGROUND_LAB_TRACE=ON`. Build with:

```powershell
cmake --build build/foreground-lab-amd64 --config Release --target foreground-lab-trace
ctest --test-dir build/foreground-lab-amd64 -C Release --output-on-failure
```

For existing worker regression tests, first build the standalone default target.
Repeat configure/build for `build/foreground-lab-arm64` with `-A ARM64`; run only
`foreground-sequence|foreground-msi-readonly|foreground-lab-static` on x64.
Use cached WiX only. Files are named `lab-trace-Success.msi`,
`lab-trace-CommitFail.msi`, `lab-trace-PostFinalizeFail.msi` and matching DLLs in
`Release/`. Product/upgrade UUIDs are unique per variant and build directory,
preserved in CMakeCache; WiX generates a fresh package UUID when linking. Once
hashed for review, preserve the exact bytes; rebuilding requires new evidence.
There is no Upgrade table or RemoveExistingProducts action.

Every normal message begins `BIMERON_LAB_TRACE_V1 ` followed by bounded flat JSON
(schema 1). `callback` is an exact export name; `utc` is an ISO UTC timestamp;
`pid`, `tid`, `processSession`, `tokenSession`, `integrityRID`, `status` are unsigned
32-bit values. `processCreated` is a 16-hex FILETIME and `logonId` a 16-hex LUID.
`tokenSource` is 0 unknown, 1 thread, 2 process; fallback is only ERROR_NO_TOKEN.
`tokenType` is 0 unknown, 1 primary, 2 impersonation; `impersonation` is -1 when
not applicable, otherwise 0..3. `elevationType` is 0 unknown or 1..3;
`elevated`, `adminEnabled`, `adminDenyOnly`, `restricted` are 0/1.
`sidSHA256` is 64 lowercase hex digits or `unknown`. `evidence` is `observed` or
`incomplete`; numeric status is the first query failure. All fields in an incomplete
record are observations only and may be unknown defaults. `initiatingClient` is
always `Unknown`; `intendedReturn` is 0 or 1603. Fallback records contain only
schema, callback, evidence=incomplete, numeric status, initiatingClient and
intendedReturn. A missing/unparseable callback or failed message means incomplete
evidence, even when the fallback cannot be logged. No names, properties,
environment, command lines, exception text or credentials are logged.

MSVC x64 and ARM64 Release compilation passed with /W4 /WX, static CRT,
SYSTEM32 dependent-load flags, exactly five exports, no delay imports and only
KERNEL32/ADVAPI32/MSI/BCRYPT imports. Normal unsuppressed verbose ICE passed all
six linked MSIs without warning/error diagnostics. ICE09 cannot inspect the
intentionally absent Component table; explicit Media row passed ICE71. x64 CTest
passed 4/4; ARM64 host-side CTest passed 3/3. The lab check validates actual tables,
summary architecture/package code, per-machine properties, exact custom action
types and sequences, empty payload and embedded DLL SHA-256, with 42 negative
mutations per architecture (flags, exports, order, payload, production identities,
extra actions/tables and altered Binary digest). Reports are `Release/*.tables.json`
and `Release/lab-trace-evidence.json`; verbose ICE is in `build.log`.

No callbacks/MSI installation, VM access, registry access, elevation, network,
signing, secrets, guest runner, manifest publication or release was performed.
ARM64 was compiled, never executed. Linux/full GUI/package matrix and performance
benchmarks/profiling are inapplicable: this disconnected Windows-only target changes
no production code or packaging. Static review found no production integration or
application resources. Runtime callback identity/order/rollback and logging remain
unproved until a separately authorized disposable VM phase. The empty fixture
cannot prove real resource rollback, outer completion or authenticated initiator.

Final artifact evidence (SHA-256; identities read from linked MSI, checked against
configured lab identity; these are inert local fixtures, never release artifacts):

### amd64

```json
{
  "Success": {
    "file": "lab-trace-Success.msi",
    "sha256": "7fc52c25dedc5a68031311f9e3bad0c2b48e339df0813be4a4c3c29e4fee35e2",
    "dllSha256": "d87301a4433667e1ebe5b3becfec535f1c30953b2fa6886c52715ba2e35d9414",
    "productCode": "{A96FF6D5-FB68-44DF-BE50-6199ED3EC9C3}",
    "upgradeCode": "{42A71F28-A5D9-4860-82B2-517166C98E42}",
    "packageCode": "{846D28F8-B92B-46B5-B836-DB2D696FB38E}"
  },
  "CommitFail": {
    "file": "lab-trace-CommitFail.msi",
    "sha256": "0970fcd8c86464bd54ea7fd9d5a1ed520e2c05b1146df9b1247da474ca36c309",
    "dllSha256": "85464ac58903bb23cfff6199646da202d11970a4832eebec5b9b15569ddde865",
    "productCode": "{948171C5-070E-49FF-9332-E3A0AEE27701}",
    "upgradeCode": "{AFA8D094-C57F-4C94-87F1-FF4E06D741E0}",
    "packageCode": "{F807B7EF-BAAF-4F4D-B34B-1B4881936F64}"
  },
  "PostFinalizeFail": {
    "file": "lab-trace-PostFinalizeFail.msi",
    "sha256": "e2431fc8ac9c3472cf9890aca3ce3b5c0e390a099c0872d494031a48ed10b841",
    "dllSha256": "1c0cf5067a457c0b1a1dafb6f2ac301773892b5be2f561de99b1e490d7e9075a",
    "productCode": "{16C37902-2E8D-4618-8D68-2EBAB4CC556A}",
    "upgradeCode": "{C1D8B2C2-1779-431C-9980-CAC6785FFE7E}",
    "packageCode": "{E106B694-1318-4DDF-B167-976C4839BAE1}"
  }
}
```

### arm64

```json
{
  "Success": {
    "file": "lab-trace-Success.msi",
    "sha256": "812aa440a96d160b932bd26571e3532ba863602022b7c648acdc0a02e6295fe0",
    "dllSha256": "24888eea032d5f3330b8de776969a09705f93462e8f5bb7347faa2f4bf6627ab",
    "productCode": "{50D8651B-A1F6-43CE-9228-CCA41F3F1750}",
    "upgradeCode": "{FB9B1631-ECED-4801-AC9B-0F07064F256C}",
    "packageCode": "{3CA46ED5-DCE8-4B04-A94C-FB294C4ECE8F}"
  },
  "CommitFail": {
    "file": "lab-trace-CommitFail.msi",
    "sha256": "a3fff4b6d95627de2fd3bca083a42aee241068dd71287f17f2b99b75f7be462a",
    "dllSha256": "4e6f2695409300b6c34b3f318e58d5118b3742a17c1b77b14816e26ec9e9b672",
    "productCode": "{F606DB2D-0307-4C65-89FA-C21860510E1C}",
    "upgradeCode": "{7A6CFC69-0138-48D4-AAD7-3C8B6D777E66}",
    "packageCode": "{3A81D373-6467-428E-9655-B0955F9C0758}"
  },
  "PostFinalizeFail": {
    "file": "lab-trace-PostFinalizeFail.msi",
    "sha256": "81b02173fd11c91be0de66a3224c4e39cd7e012e2db778957d9b3c9c4d8f3a00",
    "dllSha256": "908c80be2d82c838f30499fd49fe0b3a82a82608ec252680c4fb529bd5e2b130",
    "productCode": "{2A2C0686-33CC-4D2A-80E6-5E4C08527BDE}",
    "upgradeCode": "{21757994-5169-48BB-9D35-82B41F0740B5}",
    "packageCode": "{D5596412-5029-4F35-87E9-54D685C89847}"
  }
}
```

## Accepted Component-table correction (2026-10-30)

The first Success VM attempt returned 1603 at InstallInitialize immediately after
error 2205 for the missing Component table. The frozen MSI lacks that table;
its absence is a strong hypothesis, not a proven cause. Add a schema-correct,
physically present, empty Component table using WiX EnsureTable. Do not add a
component, payload, registry, service, or change callbacks, sequencing or runner.
Acceptance: inspect all six actual Component columns and their schema, require
zero rows, preserve every existing strict gate and 42 negative cases, and reject
missing, malformed and nonempty tables. The frozen old MSI must fail specifically
for the missing table. Rebuild all three variants in fresh x64 and ARM64 directories
with cached WiX and unsuppressed ICE; run x64 tests and ARM64 host checks. Record
fresh identities, hashes and provenance. ICE success alone is not runtime proof;
independent review and a parent-led VM repeat remain necessary.

## Component-table correction offline evidence

Source commit e583169a6d677736077f93e1465334b05511f9c7 produced fresh
independent x64 and ARM64 lab builds. All three variants linked with cached WiX
and normal unsuppressed ICE. x64 CTest passed 4/4; ARM64 host checks passed 3/3.
Read-only inspection checked the actual six-column schema and empty table, Binary,
PE, identities and the existing allowlist; 51 negative mutations passed per
architecture. The old frozen 071b609 x64 MSI was rejected specifically for its
missing Component table. Prior ICE success was not runtime validation. No install
or VM repeat was performed; causality and callbacks remain unproved. No benchmark
or profiling applies to this disconnected offline fixture. Production enrollment
remains fail closed. Independent review precedes any parent-led VM repeat.

## CostInitialize discrimination experiment specification

This isolated x64 lab experiment compares four separately hashed copies of the
frozen e583169 Success MSI (SHA-256
`0c73788944a2d520628e7dd619df2eb507821021fd76cef4dd879851586d0dbf`):
unchanged, an explicit zero-length encoded Component compound stream only, an
empty persistent FeatureComponents logical table only, and both changes. The
source bytes must remain unchanged. This is a discrimination experiment, not a
candidate fix or enrollment. No dummy component or resource is authorized.

Acceptance before any guest execution: inspect actual compound storage and MSI
logical data for all four copies, prove only the intended differences, zero
Component rows, unchanged Component primary key/schema, no new component or
resource, and distinct pinned hashes. Reject wrong case/hash and unrecognized
inputs. The guest probe has four fixed cases and bounded numeric statuses and
sanitized hashes only; no arbitrary path, command, package selection, account or
environment output. Its only installer session calls are MsiOpenPackageEx and
MsiDoActionW for CostInitialize, followed by handle closure. No installation,
other standard action, custom action, callback, registration, payload, network,
registry or service operation is authorized. Establish from authoritative API
semantics that this isolation is guaranteed before execution; otherwise stop.
Independent review and a disposable x64 VM operated by the parent are mandatory.
Never run MSI actions on the host. Keep the tool outside production CMake/CPack,
with no dependencies or generated files committed. If a valid zero-length stream
cannot be represented or verified, report the limitation instead of inventing a fix.
Linux, ARM64, benchmark, profiling and release matrix are inapplicable to this
standalone discrimination fixture.

## Implemented bounded costing probe

The accepted 776ea9a experiment is implemented in
[lab-costing-probe/README.md](lab-costing-probe/README.md), with fixed hash pins,
standalone build commands, physical/logical offline checks and the parent-only
guest command. Official restricted-handle API semantics establish the isolation
boundary. Offline x64 checks passed; independent review and disposable-VM results
remain pending. No MSI actions ran on the host and no production gate changed.

## Accepted FeatureComponents correction (2026-10-30)

The parent-operated disposable x64 VM full-server `/qn` run of the FeatureComponents-only discriminator (SHA-256 prefix `443858`) returned 0, passed CostInitialize, InstallInitialize and InstallFinalize, and reported four successful Preflight, DeferredBegin, Commit and PostFinalize callbacks with no Rollback. The both-changes case also returned 0; Component-stream-only (`99a` prefix) and original EnsureTable-Component-only (`0c737` prefix) failed with 2728 at CostInitialize. Client probes all returned 0 and did not discriminate. Creating the empty FeatureComponents database table is sufficient for that fixture; these results do not identify an internal engine cause. ProductState -1 before and after is expected for this zero-resource lab and is not product-install acceptance.

Acceptance before implementation: add only WiX EnsureTable for FeatureComponents alongside Component. Require both actual logical tables, exact Component schema and the two nonnullable primary-key FeatureComponents columns, and zero rows in each. Preserve strict allowlists, payload and sequence gates. Add missing, malformed and nonempty FeatureComponents negatives and reject frozen e583169 Component-only MSI specifically as missing FeatureComponents. No physical Component stream patch, dummy resources, production integration, C++ or runner changes. Build all three variants in fresh x64 and ARM64 directories with cached WiX and unsuppressed ICE; run x64 CTest and ARM64 host checks, inspect actual tables, Binary and PE, and record source commit, hashes and identities. Preserve frozen bytes. Fresh artifacts are untested at runtime until independent review and a parent-led VM run. No host MSI actions are authorized.


## FeatureComponents correction offline evidence

Source commit `a761106bd03f3710fbd2b092943cc493727289d1` produced these fresh artifacts in `build/foreground-lab-featurecomponents-x64/Release` and `build/foreground-lab-featurecomponents-arm64/Release`. All six linked with cached WiX and normal unsuppressed ICE. x64 CTest passed 4/4 and ARM64 host checks passed 3/3. Actual read-only MSI table, Binary and PE inspection passed, including 60 negative mutations per architecture. A copy of the frozen e583169 Success MSI, hash `0c73788944a2d520628e7dd619df2eb507821021fd76cef4dd879851586d0dbf`, was rejected by the checker specifically for missing FeatureComponents. No MSI actions ran on the host. Previous frozen bytes were retained. These fresh bytes have not run in a VM; independent review and a parent-led run remain required. ARM64 execution, production installation, enrollment, benchmarks, profiling and release packaging are outside this offline fixture.

### x64

```json
{
  "Success": {
    "file": "lab-trace-Success.msi",
    "sha256": "c9ef15c85efdd28181ed4048fae661e0330970f05db4480b5823a6108cd604c4",
    "dllSha256": "d46d4dc948f44ac25d5cc00a6b0cede7e64d2407f70a10151d05c38bed942885",
    "productCode": "{CE9008DD-B756-44CD-8476-1C4BE01CC575}",
    "upgradeCode": "{C1E57450-91F2-4B89-AE8F-66D47D3EE21B}",
    "packageCode": "{8FCD8560-9EE5-4C64-B326-0344FE7EDDAF}"
  },
  "CommitFail": {
    "file": "lab-trace-CommitFail.msi",
    "sha256": "db756bb22271ca839b9f5e72bdf75f785c2f2de423c38d7b3fed93ff56aea5fa",
    "dllSha256": "6da5244d0f9f4fb739de4f92dd915fe7b938fd9543d9ed70f0bcabfaf798499e",
    "productCode": "{691221DA-B50B-458F-B5D5-94F8422ECCE8}",
    "upgradeCode": "{60AB3D9D-062E-4EF8-86E4-2F4FBB46D367}",
    "packageCode": "{7724ECBB-E126-44B4-8BD9-847461A921E9}"
  },
  "PostFinalizeFail": {
    "file": "lab-trace-PostFinalizeFail.msi",
    "sha256": "e29f8e43e261926bca496ca1d8140c9c2c4a69d54b8855f116ecbc126eacbfa9",
    "dllSha256": "669de02b10ce5dd442431636cc1e615f89aac74331bbdb64d2f112bf28a5b0b3",
    "productCode": "{166442D2-DEBA-42CD-A80E-D37DC11E35A7}",
    "upgradeCode": "{E9B1A76B-170C-435A-9348-C668940DD32B}",
    "packageCode": "{3DC3F934-2F7C-4188-894C-C94AF3EA4A75}"
  }
}
```

### arm64

```json
{
  "Success": {
    "file": "lab-trace-Success.msi",
    "sha256": "e1002ffa17a9278f26fce4e341cafaa4d8953ef094ed219b1c1f63cc57fa04f2",
    "dllSha256": "7907b9720b479f9bd8f8b3e5174fdf65b8a651080756ad976f3ac4b8237a841b",
    "productCode": "{3F8223D1-6CD4-4369-AAEF-78C255C6291E}",
    "upgradeCode": "{9B2B52EA-EE11-4105-A2C2-2412B6ED3D2A}",
    "packageCode": "{48509725-C4A6-40B9-8D59-2B5EE3A7938B}"
  },
  "CommitFail": {
    "file": "lab-trace-CommitFail.msi",
    "sha256": "5021145d935798908ffb69186a325ea8e771eb535ee760eec45c8d13f519b5fb",
    "dllSha256": "8e1fbe68e1d8bb326eb4f8030e732ccc42a11a2e6655d472002810a7b49736c5",
    "productCode": "{43C44411-622A-4EFB-8D3B-6AB3655622BA}",
    "upgradeCode": "{D7344906-9227-46A7-8C06-B28829E08F18}",
    "packageCode": "{173E560B-6A57-4E82-B67F-3F88BAD06A23}"
  },
  "PostFinalizeFail": {
    "file": "lab-trace-PostFinalizeFail.msi",
    "sha256": "2c9cc5d38f0fa642c8b51e6bee6feaf31706b438b70ca0b024091ce26da2f2f7",
    "dllSha256": "d2b54c174a0b4162f00ec5db2315af1585f08fd5cd2759bfdd57e1fa4beee05d",
    "productCode": "{A9B3F99B-A06D-40E6-8B10-5546CCFA2C93}",
    "upgradeCode": "{09DABFCB-497D-45F2-86E1-6DA0FA36628E}",
    "packageCode": "{4FFFF894-F28C-4C21-AA24-B4D16971E77C}"
  }
}
```

### Fresh supported-authoring VM observations (2026-10-30)

The frozen x64 artifacts from source `a761106` were run separately in the
checkpointed Windows 10 STD VM through native x64 msiexec `/qn /norestart`, from
an elevated noninteractive PowerShell Direct session (session 0). Hashes were
checked before execution. Each case had a fresh private verbose log.

| Case | Exit | Observed callback order | Finalize | Product state |
| --- | --- | --- | --- | --- |
| Success | 0 | Preflight, DeferredBegin, Commit, PostFinalize | succeeded | -1 |
| CommitFail | 1603 | Preflight, DeferredBegin, Commit, Rollback | failed | -1 |
| PostFinalizeFail | 1603 | Preflight, DeferredBegin, Commit, PostFinalize | succeeded before injected failure | -1 |

All observed records were complete with status 0. The selected failure callback
reported intendedReturn 1603; other callbacks reported 0. No rollback callback
was observed in Success or PostFinalizeFail. Immediate observations used an
elevated high-integrity user process token; deferred/commit/rollback observations
used a SYSTEM process token. These are OS token observations, not authenticated
initiating-client evidence. No timeout, reboot or process termination occurred.

The zero-resource fixture left all three ProductCodes unregistered; uninstall
was therefore not run. These results establish callback/order observations only,
not resource rollback/restoration, interactive UAC, durable exclusion, privileged
settlement, host-death behavior or native/production compatibility. Raw logs are
retained under ignored private evidence and are not committed.

### InteractiveToken observation plan

The parent may run the frozen source-a761106 x64 Success fixture through two
sequential, temporary Task Scheduler tasks: InteractiveToken Limited, then Highest.
Revalidate the actual logged-on explorer owner SID and positive session ID before
each launch. Use RunEx with TASK_RUN_USE_SESSION_ID and no action parameters,
credentials, triggers, recurrence or fallback logon. Fix the absolute native
msiexec path, hash-pinned MSI, `/i /qn /norestart` and distinct private log paths.
Keep payload directories administrator/SYSTEM writable with user read access;
only the separate log directory grants the selected user write access.

Record exact task/action/session metadata before registration. Require product
state -1 before the next install; unexpected state blocks progression pending
explicit verified lab cleanup. Observe for at most five minutes without task/process
termination. Disable scheduler execution timeout and hard termination. On timeout
retain the task/evidence and stop. After confirmed completion capture task result,
MSI log/callback observations and product state, then delete only that exact task.
Label results Task Scheduler-launched InteractiveToken Limited/Highest; human UAC
consent remains not tested and authenticated human initiator remains unknown.

### InteractiveToken observations (2026-10-30)

Two separate demand-only tasks ran the frozen x64 Success MSI in the revalidated
interactive desktop session 1. Both used the fixed native msiexec action and
private log paths; neither stored credentials, triggers or recurrence.

- Limited: task/Installer result 1603. Complete Preflight observed a filtered
  medium-integrity user process token, session 1, elevation false. Initialize
  succeeded; Finalize failed. No deferred, commit or PostFinalize records appeared.
- Highest: task/Installer result 0. Complete Preflight and PostFinalize records
  observed the same elevated high-integrity user process identity in session 1.
  DeferredBegin and Commit observed SYSTEM process tokens in session 1. Initialize
  and Finalize succeeded; the four callbacks had status 0 and intended return 0.

Product state remained -1. The parent confirmed no running task instances and
removed each exact temporary task only after completion. Limited's instance ended
before its first Refresh call (0x8004130B); the parent then confirmed terminal task
state, result and completed MSI log without rerunning it. No timeout, termination,
retry, uninstall or reboot occurred. These results are Task Scheduler-launched
InteractiveToken Limited/Highest observations. Human UAC consent was not tested;
authenticated human/initiating-client identity and protected-journal access remain
unproved. Raw logs and task plans stay in private ignored evidence.

### Protected access observation (incomplete; no guest package)

The default-off `FOREGROUND_LAB_PROTECTED_ACCESS` option adds only the explicit
`foreground-lab-protected-access` target to this standalone project. Its DLL
observes the current effective token source (thread first, process only on
`ERROR_NO_TOKEN`); both prospective callbacks always fail. The native test
observes process and impersonation token selection and the failure gate. It does
not exercise filesystem access or simulate flush, ACL, stale generation, or
input substitution. No MSI template or guest bundle is provided: protected
file ancestry, pinned handles, isolated mutex, records, external package pin,
and actual client-process correlation remain unimplemented. This fixture makes
no client identity or authority claim. Do not execute it in a guest or use it
for installation. The existing trace and production exports are unchanged.

### Disconnected pending-file fixture

Acceptance is recorded in docs/ikev2-profiles.md under the distinct disconnected
pending-file subsection. Build with FOREGROUND_LAB_PROTECTED_FILE=ON and explicit
target foreground-lab-protected-file; run CTest -R foreground-lab-protected-file.
The record never represents Clean. Strict ancestor validation may reject the
host build tree; the harness reports native positive unavailable separately from
pure models and owned temporary I/O. No MSI or production integration is added.

Pending-file evidence (2026-10-30): MSVC 19.51 / Windows SDK 10.0.26100 x64
and ARM64 Release compiled the non-injected static primitive and injected harness
with /W4 /WX. x64 CTest foreground-lab-protected-file passed 1/1. Output labels
pure codec/stale-ID/counter/ACL/reparse/link/identity cases MODEL; short write and
flush failure are narrow MODEL injection into the actual writer. The owned
build-local temporary file exercised real WriteFile/FlushFileBuffers and was
removed after closing. The strict native directory pin refused the host path:
protected create/acknowledge, real reparse/substitution attacks and protected
flush are unavailable, not established by these results. ARM64 runtime was not
run. No host ownership changes, protected-state writes, MSI, VM, installation,
network, production build, Linux build, benchmark or release occurred.

The primitive uses protected SYSTEM/Administrators-only file DACLs at creation;
creation cannot change owner or grant the caller privilege. An untrusted default
creator owner therefore causes refusal with the created file retained. Read and
acknowledgment require exact persisted identity IDs and an exclusive file handle.
Failures do not retry, remove the file, or claim recoverability or Clean. The
non-injected library is compiled separately; no installed code links it. Existing
LabProtectedBegin and LabProtectedAcknowledge remain unconditional failures.

Review refinement: native path opens use one resolved volume GUID, avoiding DOS
drive-letter remapping after admission. Reparse components remain forbidden.
The existing token-source/unconditionally failing callback regression also passed:
x64 CTest foreground-lab-protected-(file|access) completed 2/2. Source review
found no production registration or callback changes. Protected positive evidence
and actual filesystem substitution attacks remain unavailable on this host.

### Program Files anchor follow-up

The disconnected fixture now anchors authority at the native Program Files known
folder, retaining metadata/traversal pins above it and protected descriptor checks
at and below it. Exact TrustedInstaller, SYSTEM and Administrators SIDs are trusted;
other service SIDs are not. Both paths must resolve to the same local fixed-drive
volume GUID and the harness must be strictly below the anchor. No ACL or owner
repair is performed. MSVC x64 and ARM64 /W4 /WX builds passed; x64 CTest passed
1/1. The host harness is outside Program Files, so native protected positive and
real attack cases were unavailable. ARM64 execution was not performed.

A guest harness exceeds this anchor-only implementation: no guest-ready bundle or
manifest is claimed. A separate reviewed implementation must provide a default-disabled
fixed guest mode, requiring an explicit fixed flag, with a hash-pinned manifest and
fixed harness destination under native Program Files\\BimeronProtectedFileLab. Its
review must enumerate creation of LABProtectedFiles and pending.bin, create and
acknowledge operations, negative fixtures and retained failure residue. Normal tests
must never invoke that mode. Parent authorization for those guest file effects,
provisioning and execution remains necessary; no guest operation occurred here.
MSI exports continue to fail unconditionally and no production integration is added.


### Anchor identity/case correction evidence

Acceptance was extended before implementation in the distinct pending-file
subsection of docs/ikev2-profiles.md. A single fixed-drive volume mapping derives
both paths. A separately held, protected native known-folder handle must match
the traversal anchor's volume serial/file ID at admission and before operations.
Text comparisons locate the boundary but cannot authorize a different object.
Every directory pin, including the root, must successfully return zero
FileCaseSensitiveInfo flags; enabled/unknown flags and every query error refuse.
No unsupported-query fallback exists, including on older Windows/filesystems.
All previous root pins, authority index, DACL, mutation and reparse checks remain.

MSVC 19.51 / SDK 10.0.26100 x64 and ARM64 Release /W4 /WX compilation passed
for the non-injected primitive and injected harness. x64 focused CTest passed
2/2 for protected-file and the unchanged protected-access fail-closed exports.
Models cover same path text with differing anchor IDs, case flags and query
failure. The owned build-directory native fixture successfully enabled case
sensitivity with SetFileInformationByHandle, verified the queried flag, and
proved actual normal Win32 OPEN_EXISTING of an absent differently cased name
failed; CREATE_NEW then opened that name as a distinct file ID. The directory
was rejected by the actual case-query guard. This is native namespace/guard
evidence, not protected-path admission evidence. Only those owned temporary
fixtures were removed; protected pending-file behavior is unchanged.

The harness remains outside Program Files: protected positive, protected stale
record and actual protected substitution/reparse attacks were unavailable.
ARM64 is compile-only. Review found no removed security checks or production,
GUI, MSI/export changes. No elevation, ACL repair, registry/network mutation,
VM, guest harness, package/release build, benchmark or profiling occurred.
Linux and full product/package checks are inapplicable to this disconnected
Windows-only correction. Guest acceptance and production integration remain open.

### Protected ACK access-only fixture

The specification and acceptance criteria are in `docs/ikev2-profiles.md` under
"Protected ACK access-only MSI specification and acceptance". This is a separate
access experiment. It does not replace the existing pending-file guest harness,
its frozen bundles or retained guest state. Existing prospective exports still
fail unconditionally. `Directory::pin()` and the original counter range keep their
behavior; `pinAck()` resolves only native Program Files\BimeronProtectedAckLab\LABProtectedFiles,
and `acknowledgeOnce()` accepts only counter zero. No caller-selected path exists.

Configure the standalone project in a new ignored build directory using installed
MSVC and an explicitly supplied cached WiX 3 directory:

```powershell
cmake -S packaging/windows/foreground-prototype -B build/foreground-ack-x64 -G "Visual Studio 18 2026" -A x64 -DFOREGROUND_LAB_PROTECTED_ACK=ON -DWIX_TOOL_DIR=<cached-WiX-directory>
cmake --build build/foreground-ack-x64 --config Release --target foreground-lab-protected-ack
ctest --test-dir build/foreground-ack-x64 -C Release -R foreground-lab-protected-ack --output-on-failure
```

Repeat in a separate directory with `-A ARM64`; select an installed VS instance
with ARM64 support if needed. On x64 run only its `foreground-lab-protected-ack-readonly`
test. The option defaults OFF, the target is not ALL, and there are no install or
production CPack rules. Product/upgrade GUIDs are cached per variant/build directory;
WiX generates a new package GUID on linking. Freeze bytes after review preparation;
a rebuild invalidates the bundle and requires new external hash verification.

`lab-protected-ack-check.py <Release-directory> <x64|arm64>` opens actual databases
read-only, checks exact zero-resource tables/schema/sequences and summary identity,
embedded Binary SHA-256, PE machine, five exports, no delay imports, SYSTEM32 load
flags, static CRT and explicit direct DLL/API allowlists. It reuses the existing
trace table validator without changing that validator. Nineteen negative database
models per variant reject extra resources/actions and altered callback conditions
or sequences. The manifest is generated only after this inspection and MSI build.
No installer session is opened. Direct imports do not prove absence of dynamic
loading; static CRT imports include loading, process termination, command-line and
environment APIs, but fixture sources do not use these for callback authority or
diagnostic dumps. The DLL's own MSI imports contain no install/action APIs.

The canonical `run.manifest` is binary, bounded to 65,536 bytes, with no padding:

- Offset 0: eight ASCII bytes `BLACK001`.
- Offsets 8 and 24: nonzero 16-byte installation and generation IDs (opaque bytes).
- Offsets 40 and 44: little-endian uint32 architecture (1 x64, 2 ARM64) and compiled
  variant (0 Success, 1 CommitFail, 2 PostFinalizeFail).
- Offsets 48 and 64: nonzero product and package GUIDs in Windows memory byte order.
- Offsets 80, 112 and 144: SHA-256 of the externally pinned original MSI, Binary
  DLL and the remaining canonical database projection, respectively.
- Offset 176 onward: the exact actual table/sequence expectations. In the fixed
  `COLUMNS` order in the checker, encode table name, uint32 row count, sorted rows,
  uint32 field count per row, then every field. Strings are uint32 byte length
  followed by UTF-16LE without terminator. Binary Data is lowercase SHA-256 text.
  Append summary template, package GUID and decimal flags as the same strings.

IDs are deterministically unique to each fresh package GUID (UUIDv5 installation
and generation domains); they are public labels, never credentials. The native
x64 test compares the actual C++ database projection byte-for-byte with Python's
manifest, checks GUID/Binary header consistency and malformed manifests. It opens
only an MSIDBOPEN_READONLY database, never an installation session. Additional
actual OS tests select the process token with no thread token, then the actual
impersonation token by token ID, and refuse an anonymous thread token without
process fallback. Owned ignored temporary I/O writes/flushes counters zero and one,
refuses CREATE_NEW on existing state and refuses the shared exact-zero transition
on reread counter one. These are not native protected-path positive observations.
The unchanged protected-file harness separately labels ACL/stale-ID/identity and
injected short-write/flush failures as models.

Preflight reads and pins the protected manifest/ancestors, compares its effective
active database projection, product/package identity and Binary digest, and rejects
transforms, PATCH, mutation properties, maintenance and disabled rollback. Deferred
independently rereads the protected manifest and creates a flushed 48-byte Pending
record with its IDs and counter zero. Commit and Rollback never touch records.
PostFinalize independently rereads the manifest, uses thread-token precedence
(process fallback only ERROR_NO_TOKEN), rejects nonelevated/restricted tokens,
rechecks active database and actual protected file access, then increments only
matching counter zero to one and flushes. PostFinalizeFail returns 1603 before ACK.
Any failure or existing record is retained; there is no automatic retry, deletion,
repair, Clean state or settlement. The manifest read handle prevents write/delete
sharing until that callback's operation ends. No callback derives IDs or paths
from MSI properties. MSI hash is an external source expectation only; callbacks do
not self-attest original MSI bytes. Diagnostics contain only bounded callback,
result, access-only and initiatingClientUnknown fields.

Review preparation freezes all six MSIs/DLLs, six manifests, eighteen actual GUIDs,
source SHA/source hashes, table/import reports and verbose unsuppressed ICE/test
logs under `build/protected-ack-review/`. Its `guest-x64-success/` directory contains
one Success MSI, the exact `run.manifest`, and an effects/permission record. This
is READY GUEST REVIEW BUNDLE, with permission not granted. No provisioning or
execution belongs to preparation. Parent review must externally pin the original
MSI and manifest bytes, then separately obtain permission for a fresh disposable
VM lab directory and state. Never reuse or modify BimeronProtectedFileLab.

Proposed effects, only after that separate permission: provision the two new ACK
lab directories with trusted owner and protected SYSTEM/Administrators DACLs;
copy only the matching manifest to LABProtectedFiles\run.manifest; place the pinned
MSI in a new review-owned guest input directory; verify both hashes; invoke that
one Success MSI once with a separately reviewed fixed log path. The engine runs
its five fixed callbacks and may create pending.bin at counter zero and write it
once to counter one. Retain the manifest, pending record and MSI log on success
or failure. No retry, uninstall, repair, cleanup or failure-variant run is included.
MSI's own engine logging/cache/registration effects must be reviewed by the parent;
zero installed resources does not mean the engine performs no system writes.

Remaining runtime questions are availability of the active effective database and
summary information in these exact callbacks, token/write behavior after Finalize,
actual callback order and trusted-engine extraction/loading of Preflight code.
Offline results do not resolve these boundaries or establish production authority,
client identity, participating-host exclusion, outer completion or power-loss
durability. Production approval stays denied. ARM64 runtime, Linux, GUI/product
matrix, benchmarking and profiling are inapplicable to this isolated Windows lab.

Offline result (2026-10-30): x64 MSVC 19.51 Insiders and ARM64 MSVC 19.51
BuildTools / SDK 10.0.26100 compiled all three variants and the native test with
/W4 /WX. The initially auto-selected Insiders ARM64 configuration was unavailable;
explicit installed BuildTools selection resolved it without installation. Six
cached WiX builds completed normal unsuppressed verbose ICE without warning/error
diagnostics. x64 focused CTest passed 4/4 (unchanged protected-access/file plus
ACK read-only/native); ARM64 host ACK read-only passed 1/1. Each architecture's
three actual MSI inspections passed 57 negative database models. Default configure
omits the ACK project entirely. Diff/security review confirmed fixed paths, unchanged
old fail-closed exports, no production registration and no record cleanup path.
Host protected positive and all MSI callback execution remain untested by design.

### First protected ACK VM observation (2026-10-30)

The parent ran the frozen x64 Success MSI from source `270a674` once in the
checkpointed Windows 10 STD VM, after the user authorized further VM checks.
The pinned MSI and protected run.manifest hashes matched before launch. The
installer returned 1603 at AckPreflight (callback 0); no pending.bin was created.
The existing standalone protected-file Pending record remained byte-identical.
All new input, manifest and log residue was preserved without retry or cleanup.

The engine log establishes a first-time install without a caller REMOVE argument,
then InstallValidate adds REMOVE=ALL before Preflight. The current fresh() rejects
that value, a confirmed blocker if manifest validation reached it; the generic
callback result does not establish the first failed internal gate. Bounded stage
diagnostics and a separate, explicitly scoped zero-resource lab predicate are
required before another candidate run. No production admission rule is relaxed.

ACK diagnostic follow-up: the zero-resource lab permits only empty REMOVE or exact
ALL after canonical database/Binary/header and zero-row resource checks, and an
OS MsiQueryProductState query must report UNKNOWN. Other freshness and rollback
gates remain. REMOVE provenance and initiating identity remain Unknown. The
bounded trace now reports stage/detail/api numeric fields; api=0 means no API
status was captured, not proof that an earlier operation succeeded. Stages 1/2/3
identify path/manifest gates, 10-14 freshness/rollback, 30/31 header/resources,
40/41 projection query/mismatch (detail indexes the fixed table order), 50 token,
60/61 create/ack, 70 compiled failure and 90 exception. No property values are
logged. Build new immutable artifacts after this source change; old bytes and
failed VM state remain evidence, not release candidates.

### ACK diagnostic offline build evidence

Source `a1ab61778e8f6c2013392e94a0b30615c5b7b9fd` built in fresh
`build/ack-diagnostic-x64` and `build/ack-diagnostic-arm64` directories using
VS 18 BuildTools, SDK 10.0.26100 and cached WiX. All six MSIs linked with
normal unsuppressed ICE and no warning/error diagnostics. x64 focused CTest
passed 4/4; ARM64 host read-only passed 1/1. Each architecture checked actual
MSI tables, Binary and PE imports and 57 negative database models. The checker
change `fd03e308873d4e79dc96b6b2e9675360cbe5601f` allows only ordinal 113,
observed in the linked DLL and mapped to MsiQueryProductStateW in system msi.dll.
No C++ source changed in that checker commit.

Frozen source archives, six MSI/DLL pairs, manifests, 18 GUIDs, reports, logs,
and a proposed Success-only guest bundle are under
`build/ack-diagnostic-review-a1ab617`; its SHA256SUMS.txt SHA-256 is
`712e10ffcc707b1dac8e878b496f858cda0125c4ed5a707016e71e6a3d477efc`.
No MSI actions, protected writes, VM execution or release were performed. The
bundle is review material; callback behavior and resource state remain unobserved.
ARM64 runtime, Linux, product packaging, benchmarks and profiling are outside
this disconnected Windows fixture.

### Accepted active-table diagnostic specification

Only the Success build (LAB_FAILPOINT=0), AckPreflight callback 0, and original
stage 41/detail 1 may attempt one diagnostic. It reads the active database's
SELECT Name FROM _Tables, sorts UTF-16 names, and records each name with the raw
MsiDatabaseIsTablePersistentW enum, including error values. Names use quoted
ASCII with every non-printable/non-ASCII code unit and quote/backslash escaped
as \uXXXX. Collection refuses more than 4096 rows or 65536 total output bytes
before writing. No Property values, Binary payload, or _Columns are collected.
No MsiDatabaseExportW is permitted inside a custom action.

The already pinned ACK Directory alone may create the fixed active-tables.txt
leaf, with protected SYSTEM/Administrators DACL, CREATE_NEW, no sharing, disk,
single-link, no-reparse and identity checks, complete WriteFile and flush.
Anchor, descriptor, case and volume identity checks remain mandatory. Existing
files refuse; every failure preserves residue. No generic path writer is exposed.
The original failure and 1603 remain unchanged regardless of diagnostic outcome.
The numeric snapshotStatus is 0 not attempted, 1 written, 2 collection refused,
3 write refused. MSI logs contain no names. Canonical acceptance is unchanged.

Acceptance: native owned temporary MSI permanent and HOLD temporary tables,
sorted escaped bounded output, trigger selection, owned temporary file complete
write/flush and existing-destination refusal; actual MSI/Binary/import checks and
fresh GUID/hash manifests. No host installer actions/protected writes, VM actions,
production changes, cleanup, enrollment or release. Parent owns VM execution.

Implementation evidence: all three x64 and ARM64 /W4 /WX variants linked with
cached WiX and unsuppressed ICE. x64 focused CTest passed 4/4; ARM64 actual
MSI/Binary/PE checks passed 1/1 (compile/static only). Ordinal 165 was mapped
by system msi.dll dumpbin to MsiDatabaseIsTablePersistentW. The native temporary
MSI test uses an all-TEMPORARY column with HOLD; HOLD alone does not make a
persistent column temporary. See [Microsoft SQL syntax](https://learn.microsoft.com/en-us/windows/win32/msi/sql-syntax)
and [custom-action database restrictions](https://learn.microsoft.com/en-us/windows/win32/msi/accessing-a-database-or-session-from-inside-a-custom-action).
No acceptance filter or temporary-table exception was added. Diff/security review
found no production registration, arbitrary path API, overwrite or cleanup route.
Host protected-path success and MSI execution remain untested. Linux/product
matrix and performance profiling are inapplicable to this bounded Windows lab.
Frozen review evidence is under build/ack-active-tables-review; parent independent
review and VM execution remain outstanding.

### Accepted ACK runtime metadata projection correction

The parent observed Success active _Tables with 24 names versus 15 source names
on Windows 10 engine 5.00.10011 only. ARM64 runtime remains unproved.
Before implementation, accept this bounded correction: classify only _Tables.Name
and _Columns.Table row owners, sharing one tiny exact case-sensitive classifier.
Exclude only #_BaselineCost, #_BaselineData, #_BaselineFile, #_FolderCache,
#_MsiRedirectedFile, #_PatchCache, CostAdjuster, _Property and __MsiPatchMedia,
and only when MsiDatabaseIsTablePersistentW returns FALSE. Known metadata with
TRUE/NONE/ERROR refuses; unknown owners with any persistence refuse. The fifteen
canonical persistent owners require TRUE. Query row owners, never the virtual
catalogs _Tables/_Columns/_Streams/_Storages themselves (their NONE is expected).
Preserve every retained schema/row, Binary, Property, summary, stream and manifest
encoding byte exactly. No other normalization or source-checker allowlist change.
Persisted runtime-name collisions must be rejected by runtime and source checks.

Keep freshness, zero-resource, REMOVE, rollback and token gates unchanged;
initiating client remains Unknown and records remain Pending only. Snapshot is
still selected only by Success Preflight stage 41/detail 1; successful projection
must not snapshot. Bounded numeric diagnostics distinguish query/policy refusal
from byte mismatch without logging names or values.

Acceptance: actual owned MSI databases without installer actions compare baseline
exactly, normalize an all-TEMPORARY-column CostAdjuster HOLD table, refuse unknown
temporary/persistent tables, persisted CostAdjuster, missing persistent tables and
changed schema. Exercise _Columns owner filtering with rows where exposed, without
assuming temporary columns are visible; separately check NONE/ERROR classification.
Retain a source collision negative. Build x64 and ARM64 /W4 /WX, six fresh cached
WiX MSIs with unsuppressed ICE, focused x64 CTest and ARM64 static-only checks.
Freeze new ignored evidence with identities/hashes/manifests/source/build logs;
never overwrite older bundles. Independent review and parent VM execution remain
separate gates. No host MSI action, protected write, production authority, release,
benchmark or profiling belongs to this disconnected Windows-only correction.

### Runtime metadata correction offline evidence

The accepted correction is implemented only at catalog row-owner projection.
Stages 42 (persistence query ERROR) and 43 (owner/persistence policy refusal),
with raw persistence enum in api, are distinct from stage 40 query/encoding failure
and stage 41 byte mismatch. No names or values enter MSI messages. Successful
projection does not select a snapshot; the original Success Preflight 41/1 trigger
and its protected CREATE_NEW writer remain unchanged.

Fresh build directories are build/ack-projection-x64 and build/ack-projection-arm64.
Use the configure command above with these directories, explicit VS 18 BuildTools,
cached WiX, and FOREGROUND_LAB_PROTECTED_ACK=ON. The x64 configure also enables
FOREGROUND_LAB_PROTECTED_FILE and FOREGROUND_LAB_PROTECTED_ACCESS; build those two
explicit targets alongside foreground-lab-protected-ack. Run x64 CTest with
-R 'foreground-lab-protected-(ack|file|access)' and ARM64 with
-R '^foreground-lab-protected-ack-readonly$'. No ARM64 executable runs on x64.

Both architectures compiled /W4 /WX; all six cached-WiX MSIs passed normal verbose
unsuppressed ICE without warning/error diagnostics. x64 focused CTest passed 4/4;
ARM64 host read-only MSI/Binary/PE/import checks passed 1/1. Each architecture
retains 57 database negative models plus three source CostAdjuster collision
negatives. Native owned copies verified baseline byte identity, temporary
CostAdjuster normalization, unknown temporary/persistent and persisted CostAdjuster
refusal, missing Component and changed Property schema refusal. The actual
_Columns catalog exposed zero temporary owner rows and one persistent CostAdjuster
row; the latter exercised refusal directly through rows(). The shared guard's
exclusion and NONE/ERROR states are checked separately without claiming native
visibility of temporary columns. Existing token, resource, freshness, counter and
snapshot checks passed. Protected native-path positive remained unavailable.

Diff/security review found no changes outside the specified projection and tests,
no broader source allowlist, production integration, record cleanup or caller
identity claim. No performance-sensitive path changed; benchmark/profiling, Linux
and full product package matrix are inapplicable to this standalone Windows lab.
No host MSI actions, protected writes, VM, push, release or production acceptance
occurred. The only prior runtime evidence is the parent's Windows 10 engine
5.00.10011 observation; these new bytes and ARM64 runtime remain untested.

New immutable review material is frozen under build/ack-projection-review, with
six MSI/DLL pairs, six manifests, eighteen GUIDs, actual table/import reports,
source archive/hashes, build/configure/test logs and SHA256SUMS.txt. Its
 guest-x64-success directory is a READY DIAGNOSTIC REVIEW BUNDLE only: independent
review and a separately authorized parent VM run remain required. Prior bundles
are preserved. The artifact index carries exact source commit and hashes.

### Native x64 protected ACK Success observation (2026-10-30)

The parent ran the independently reviewed Success bytes from source `a6d93adc`
exactly once in the authorized disposable Windows 10 STD VM, through the existing
Session 0 administrative PowerShell Direct context. MSI SHA-256 was
`e4e08ee585dc4e269330382744c3fa22a8148c2103c44283d5197679eb0b5cf0`;
Binary SHA-256 `f30cc4bdbf381b8c2621a8211d38752548ccaed3cf6324872d0793b95a62a109`;
protected manifest SHA-256
`2569f0970d834c701015e3df6858120a39fd826a8ed9239fc30f06fd05b9ecd2`.
Guest input hashes were checked before launch. The previous ACK diagnostic directory
was archived intact; the separate BimeronProtectedFileLab Pending record was unchanged.

Exit was 0; callback order was Preflight (0), Deferred (2), Commit (3),
PostFinalize (4), each result/stage/detail/API 0. No rollback callback or
active-tables.txt was observed. ProductState remained -1, as expected for this
zero-resource fixture. The retained 48-byte BLPF record has Pending state,
installation `0eaecf426f4356279c75a5c4d1634673`, generation
`37f925c468305e66979de9382c65865e`, and counter 1; its SHA-256 is
`5934cb0459e36ac4ad72a4b1b5b0675d1f6cbf6285fa0c5d5991dd2795d24000`.
The record remains at the fixed protected ACK lab path. No retry, failure variant,
uninstall, repair or cleanup was run. Private logs/result/copied record are retained
under ignored build/vm-lab-evidence-20261030/ack-projection-a6d93adc-*.

This establishes observed protected-file access and acknowledgment through the
reviewed callbacks in this context. Successful per-step validation/flush is inferred
from frozen control flow and successful returns, not an independent OS I/O trace.
Initiating client and REMOVE provenance remain Unknown. No Clean publication,
interactive UAC proof, real-resource rollback, crash/power-loss durability, ARM64
runtime, production admission or enrollment authority is established. Failure
variants and live identity/settlement proofs remain separate gates.

### Protected ACK failure-variant acceptance (2026-10-30)

Run the frozen a6d93adc x64 CommitFail once, then PostFinalizeFail once only after
unambiguous acceptance of the first case. Archive each previous fixed ACK root
intact without overwrite; verify retained bytes and security metadata. Preserve the
separate protected-file lab record. Reprovision only the fixed ACK lab directories,
verify exact frozen MSI/manifest hashes and fresh ProductState -1 before launch.
Each run has its own private input/log path and a 300-second observation limit.
No retry, uninstall, repair, cleanup or process termination is permitted.

CommitFail requires exit 1603, callbacks 0/2/3/1, callback 3 stage 70/detail 1,
other callback diagnostics zero, failed Finalize and rollback, no callback 4,
and canonical matching Pending counter 0. PostFinalizeFail requires exit 1603,
callbacks 0/2/3/4, callback 4 stage 70/detail 0, other diagnostics zero,
successful Finalize before callback 4, no rollback, matching Pending counter 0.
Manifest bytes and prior records must remain unchanged. Any earlier refusal,
wrong order/state, missing evidence or timeout stops subsequent execution.
PostFinalizeFail occurs before token/projection/ACK and cannot prove those gates.
The frozen inventory supplies exact hashes and identities. This remains access-only
lab evidence, with no Clean, real-resource rollback, client authority or ARM64 claim.

### Protected ACK failure observations (2026-10-30)

The parent executed each accepted frozen x64 a6d93adc failure variant once in the
same authorized disposable VM context, CommitFail first and PostFinalizeFail only
after the first result met acceptance. Exact input hashes/identities are retained
in build/ack-projection-review/artifact-index.json. Guest hashes and ProductState -1
were checked immediately before launch. Previous ACK directories were archived
intact to the fixed sibling suffixes success-a6d93adc and CommitFail-a6d93adc;
file bytes and file/directory security descriptors were compared after each move.

CommitFail returned 1603 with callbacks 0/2/3/1: callback 3 reported stage 70,
detail 1; others reported zero result/stage/detail/API. InstallFinalize failed,
rollback callback executed, and PostFinalize did not. Its retained canonical
Pending has matching manifest IDs and counter 0.

PostFinalizeFail returned 1603 with callbacks 0/2/3/4: callback 4 reported stage 70,
detail 0; others reported zero result/stage/detail/API. The log records successful
InstallFinalize before callback 4 and no rollback callback. Its retained canonical
Pending has matching manifest IDs and counter 0. This injected failure precedes
token/projection/acknowledgment checks; it does not independently exercise them.

Both final ProductState values were -1. Both manifests and the separate standalone
protected-file Pending record remained unchanged. Logs, results and copied records
are retained privately under build/vm-lab-evidence-20261030/ack-a6d93adc-*.
No retries, uninstall, repair, deletion or cleanup occurred. These zero-resource
observations establish callback/failure ordering and retained Pending only, not
resource restoration, crash/power-loss durability, initiating-client authority,
Clean settlement, production exclusion or ARM64 runtime acceptance.

### Accepted early launched-client identity capture

Immediately after Process.Start returns, the existing pin-owning worker retains
its actual process handle and queries GetProcessId and raw creation FILETIME from
that handle before WaitForExit. Publish one immutable tuple; query failure leaves
explicit incomplete identity with no PID lookup or guessed fallback. Still wait
for the owned process and retain pins through late launch/completion. The single
five-minute launch-plus-wait observation deadline and PossibleLateOperation remain.
Compare each canonical callback PID/creation pair with that tuple as Equal,
Different or Unknown observations only. SID/session/token remain separate;
authenticatedInitiator stays Unknown. No discovery, ancestry, callback-host open,
termination, arbitrary command/path, ACK edit or crash variant is authorized.

Acceptance: PS 5.1 and 7 offline tests demonstrate capture before wait, launch and
query failure, late launch, timeout retention, and pair comparison including
incomplete records. No executable/MSI/guest dispatch in the harness. Freeze the
revised runner and original source-a761106 trace bytes in a new ignored review
bundle for one parent Success run. The fixed root/three-variant manifest contract
must be honored; report conflicts with a fresh private guest directory rather than
bypassing it. Parent Preflight/PostFinalize comparisons are discriminators only,
never caller authority or termination permission. Product CI/matrix, performance
and profiling are inapplicable to this disconnected runner-only correction.

### Early identity capture offline evidence

Specification commit 10ad4fe precedes implementation on exact base
6ccbe07807698c4b1ede66f64c3d20852e8a195e. PS 5.1.19041.6456 and 7.6.9 offline
harnesses pass, including 40 negative checks, early capture before blocked wait,
query failure continuing to wait, null launch, late launch and timeout pin retention.
The only native process query reads the current harness handle; no executable,
MSI or guest harness dispatch occurs. Static diff/security review found no new
process discovery, callback-host opening, termination or arbitrary command path.
The existing five-minute budget, foreground worker and pin ownership remain.
No supported build/package flow changed; canonical build documentation is unchanged.

New ignored review bundle: build/trace-client-identity-review-6ccbe07. Its inputs
retain hash-verified original a761106 x64 trace MSI/DLL bytes, actual table reports,
three-variant lab-manifest.json and revised runner/tests. Only one Success run is
proposed; no ACK bytes are reused or relabeled. Frozen runner SHA-256:
885f8999f66d32f6e991cb34f7de3842eda8b089611c57902799fc94cde6ed5f.

Historical preparation gap (resolved by the fixed-root specification below): Assert-LabPath and PSCommandPath require the exact
old Public Documents fixed root and runner leaf. A fresh private
guest directory is incompatible with that contract. The bundle is not runtime
ready and supplies no bypass or permission. Parent must resolve this policy before
execution. All three MSI inputs remain mandatory even for one Success invocation.
Actual launched-msiexec capture and callback comparison await parent VM proof;
Equal/Different/Unknown are observations, SID/session/token stay separate, and
initiating-client authority remains Unknown. No crash variant, host MSI/guest-mode
harness, VM, termination, release, product matrix or benchmark was run.

### Accepted fixed trace-client lab root specification

On base 1ac5d353e567f59e4c0e5739ca1bf1d7861454a7, move only the manual
trace runner's fixed root to native Program Files\BimeronTraceClientLab.
Resolve Program Files through SHGetKnownFolderPath, with no environment or
arbitrary-path fallback. Require a 64-bit process and IsWow64Process2 evidence
of process machine UNKNOWN (native) and native machine AMD64; 32-bit, emulated,
ARM64, unknown or unavailable queries refuse before path admission or dispatch.
Retain every ancestor/reparse refusal, exact runner leaf, three-variant manifest,
input filename/encoding, hash pin, retained launch handle/FILETIME and deadline.

Parent provisions and verifies the Program Files anchor and fixed lab ACLs before
one separately reviewed Elevated Session 0 Success observation. The runner does
not repair or validate ACL policy. Do not change Public ancestor ACLs. This fixed
trace lab is separate from protected ACK and standalone Pending records; neither
is read, modified or reused. No ACK, CMake, native production or MSI byte changes.

Acceptance: PS 5.1 and 7 offline harnesses cover native root derivation, old Public
root/arbitrary/traversal refusal and unsupported architecture decisions, retaining
all prior identity/deadline/pin checks without executable/MSI/VM dispatch. Freeze
new ignored build/trace-client-protected-root-review with revised runner/tests,
original a761106 manifest and all three original MSI bytes, carried-forward actual
table/Binary/import evidence, test logs and hash inventory. No rebuild, VM, push,
watch, benchmark, profiling or release/product matrix is applicable here.

### Fixed trace-client root offline evidence

Specification 7bb03db precedes this runner-only implementation. PS 5.1 and 7
harnesses pass 47 negative checks, including lexical refusal of the old Public
root, arbitrary siblings and traversal before filesystem checks; native known-folder
lookup and native-x64/unsupported architecture decisions are checked offline.
Existing early handle/FILETIME capture, incomplete identity, late launch, deadline
and retained-pin checks pass. IsWow64Process2 failure or missing entry point throws
or returns refusal with no fallback; no unsupported guest was executed.

New ignored bundle build/trace-client-protected-root-review carries the unchanged
original a761106 manifest/MSI/DLL bytes and actual table reports. Prior independent
review's actual MSI/Binary/import checks carry forward by unchanged artifact hashes;
no native rebuild or MSI action was run. Only runner/tests/docs changed. Diff review
found no arbitrary root, environment path fallback, ACL repair, production or ACK
change. Parent must provision and verify the native Program Files anchor/fixed lab
ACLs and review this bundle before the Elevated Session 0 Success observation.
No Public ancestor ACLs or protected ACK/standalone Pending records were touched.
Canonical supported build/package flow is unchanged, so its document is unchanged.
Product CI/matrix, ARM64/Linux runtime, benchmarks/profiling and release are not
applicable to this disconnected native-x64 runner change. VM observation remains
outstanding; Equal/Different/Unknown comparisons remain observations only.

### Retained client identity observation (2026-10-30)

The parent provisioned the separate fixed native Program Files BimeronTraceClientLab
with protected Administrators/SYSTEM permissions after inspecting ancestor SDDL.
The earlier Public/Documents location was not used because its untrusted delete-child
rights cannot provide the intended boundary without broader ACL changes. Program
Files/root ACLs and both existing protected Pending fixtures were unchanged.
Two preparation checks failed before provisioning because of SID translation and
PowerShell casting errors; inspection then used raw SDDL. The initial script invocation
was blocked by ExecutionPolicy before MSI launch. Only the subsequent process-scoped
Bypass invocation ran the reviewed runner; no persistent policy was changed.

Runner source 31018c3f, SHA-256
`da99ca4e0990f0480ad68114217da0fc0aef0d36b62991ca8ae5d9cd7858c78f`,
and manifest `61f15a4bfd277612acd323012b743008e31a4fc91a3bd2bbcb3fb0ba2830536a`
launched the unchanged a761106 Success MSI
`c9ef15c85efdd28181ed4048fae661e0330970f05db4480b5823a6108cd604c4`
once in native x64 Elevated Session 0. Exit was 0, no timeout or late operation,
ProductState -1 before/after, complete Preflight/DeferredBegin/Commit/PostFinalize.
The retained launched-process identity was PID 7580, raw creation FILETIME
`01dd685c3fc9a342`. All four callback identity comparisons were Different.
This disproves direct callback-host equality with this launched client in this
particular experiment. It does not authenticate an interactive initiator, prove a
general MSI placement rule, or identify a safe custom-action termination target.
No process was discovered or terminated for this comparison. No failure variant,
retry, uninstall or cleanup was run. Private summary, receipt, log and result are
retained under ignored build/vm-lab-evidence-20261030. Production/enrollment and
crash authority remain unavailable; no new wrapper requirement is inferred.


### Bounded checkpoint-only ACK variant acceptance

This accepted offline chunk adds default-OFF FOREGROUND_LAB_PROTECTED_ACK_CHECKPOINT,
explicit foreground-lab-protected-ack-checkpoint, x64 only, excluded from ALL,
installation and production. Its fresh Checkpoint identities and compiled variant 3
reuse the ACK source, exact WiX sequence, protected run.manifest/pins and 48-byte
Pending format. The three existing ACK variants and trace contracts are unchanged.
The trusted invocation procedure is selected with independent specification review
pending; this checkpoint establishes neither its runtime binding nor continuous
exclusion. Production remains disabled.

After Deferred create(i,g) returns success (including Pending write/flush), write
and flush fixed checkpoint.bin through narrowly named Directory::writeAckCheckpoint.
Use the diagnostic writer's CREATE_NEW, protected SYSTEM/Administrators DACL,
exclusive sharing, reparse/link/disk/identity and before/after revalidation checks.
The exact 80 bytes are ASCII BLCP, little-endian uint32 version 1, 16-byte
installation ID, 16-byte generation ID, SHA-256 of the complete pinned manifest,
little-endian uint32 code 1 meaning only Pending create returned success, and four
zero reserved bytes. No PID, token, commands, authority or release handshake.
Only then perform compiled non-alertable SleepEx(60000,FALSE), require return zero,
and return ERROR_INSTALL_FAILURE. Create/checkpoint/write/flush/wait failure or
exception fails closed, retaining all residue. No delete, cleanup or retry.

Most importantly, Checkpoint PostFinalize unconditionally returns failure before
any ACK path, including unexpected server continuation. Commit/Rollback remain
nonmutating; there is no event, file or property release mechanism. Tests use the
existing test-only compile hook for bounded wait models, never a real 60-second
sleep, and check ordering, short write/flush/checkpoint/wait failure, exceptions
and zero ACK through every callback/continuation path. Constructor is 64-bit x64
only; no ARM64 checkpoint support is claimed.

Build the new DLL/MSI /W4 /WX with cached WiX and unsuppressed ICE, inspect actual
tables/Binary/identities/exports/imports/sequence and zero extra resources. Recheck
six existing ACK MSIs and focused x64 regressions; compile shared code for ARM64.
Freeze fresh ignored source/hash/MSI/DLL/manifest/GUID/log/effect evidence as a
partial offline fixture, not a guest-ready runner. Parent owns VM work; no runner,
termination, host MSI actions, native production or existing bundle/state changes.
Linux, benchmarks, profiling and release matrix are inapplicable.

### Checkpoint partial offline fixture evidence

Specification commit 1e3936c precedes implementation on dba44bc385e4bdab6db04005617a4278277612bd.
Configure a fresh ignored build directory with the existing VS 18 x64 command,
explicit cached WIX_TOOL_DIR and -DFOREGROUND_LAB_PROTECTED_ACK_CHECKPOINT=ON.
Build --config Release --target foreground-lab-protected-ack-checkpoint, then run
CTest -C Release -R foreground-lab-protected-ack-checkpoint --output-on-failure.
The original ACK option is independent and need not be enabled. The checkpoint
checker invocation is lab-protected-ack-check.py <Release-directory> x64 Checkpoint.
Only this mode permits the observed SleepEx import; existing API allowlists stay
unchanged. Variant 3 extends only the manifest's compiled-variant field.

Fresh build/ack-checkpoint-x64 produced four x64 DLL/MSI pairs; shared-source
regression build/ack-checkpoint-regression-arm64 produced the original three ARM64
pairs and compiled their harness plus the protected-file primitive/harness.
All used /W4 /WX and cached WiX with normal verbose unsuppressed ICE, without
warning/error diagnostics. x64 focused ACK/checkpoint/file/access CTest passed 6/6;
ARM64 host read-only ACK checks passed 1/1 (no ARM64 execution). Actual tables,
Binary hashes, identities, sequence, empty resources, exports, architecture and
imports passed, including 57 existing negative models per architecture and 19
checkpoint negatives plus persisted-metadata collision refusals. Default configure
keeps both ACK options OFF; explicit ARM64 checkpoint configuration refuses.

The model seam checks ordered create-success, checkpoint-success then wait,
each failure and each exception, without a real 60-second sleep. Owned ignored
native file I/O round-trips the exact 80-byte checkpoint, observes 79-byte short
write residue and 80-byte injected flush-failure residue, and checks CREATE_NEW
collision. These are models/owned I/O, not protected positive or callback evidence.
Checkpoint PostFinalize refuses before storage/token checks even after modeled
Commit/Rollback continuation; its ACK call path is compiled out. Commit/Rollback
remain nonmutating. Stages 62/63 identify checkpoint/wait refusal; stage 70/detail 3
is the planned failure, and stage 90 exceptions. No runtime switch controls them.
An initial test signed-comparison warning was corrected before final passing builds.

New build/ack-checkpoint-review contains frozen source, seven MSI/DLL pairs and
manifests, identities/hashes, table/import reports, build/test logs and effect list.
It is PARTIAL OFFLINE FIXTURE ONLY, with no guest-ready runner or termination
implementation. Existing bundles and states were untouched. Diff/security review
found no production/runner changes, cleanup route, release handshake or initiating
client claim. Host protected positive, actual MSI callbacks/wait behavior and VM
execution remain untested; parent owns VM work. No host MSI actions, protected
writes, termination, push, watch, merge or release occurred. Linux/product matrix,
benchmarks and profiling are inapplicable to this isolated Windows fixture.

### Owned checkpoint client runner: strict deadline blocker specification

The requested separate Run-CheckpointLab.ps1 contract is restricted to the frozen
Checkpoint MSI/manifest from reconciled index SHA-256
6ddb8fe82ad514fd0613637e2186513705290580ae885bb1faf13e503108e966.
It must not change the three-variant runner, checkpoint fixture, protected state,
package hashes, authority or initiating-client Unknown/no-Clean conclusions.

Implementation acceptance includes a hard prohibition on a TerminateProcess call
after the shared 300-second launch/observation deadline, including a scheduling
race immediately before the call. Win32 TerminateProcess(HANDLE, UINT) has no
deadline/cancellation parameter. A final monotonic clock read and cancellation
check cannot be atomic with kernel entry: preemption can occur between them.
An observer lock either permits that late call or blocks expiry cancellation;
another thread/timer adds another scheduling boundary. Retaining the launch
handle solves identity reuse, but does not solve this deadline condition.
Reference: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminateprocess

Use the task's explicit safe-API/deadline blocker fallback. Before code, specify:
Run-CheckpointLab.ps1 is an unconditional refusal entry point, with literal VM
acknowledgment and runner/manifest/MSI hash parameters only; it performs no launch,
termination, native API invocation, file writes or protected-path access. It does
not pretend that fixed-path admission, checkpoint validation or a worker is
implemented. Its structured result states BlockedStrictDeadline,
initiatingClient Unknown, clean Unknown, guestReady false and zero launch/kill.
No flag bypasses refusal. Exit nonzero. No arbitrary path/command/case input.

lab-checkpoint-runner-test.ps1 must run offline under PS 5.1 and 7, extract the
refusal function by AST without executing the runner, and compile/run a pure
managed scheduling witness (no native imports or real processes). Demonstrate
that even a retained-handle identity model and final clock check can authorize
one simulated call before expiry yet execute it after expiry under preemption;
compare ordinary in-budget and already-expired cases. Also assert every input
combination remains refused and the production entry point has no execution or
termination escape. These are blocker witnesses, not passing implementation
acceptance for the requested runner/worker/checkpoint/pin tests.

Freeze a new ignored blocker review bundle with exact unchanged MSI/DLL/manifest,
source, test logs, inventory, effects and exact refusal-only guest command.
Independent review remains pending; parent must not execute the MSI from this
bundle. No guest execution, rebuild, recovery, cleanup, retry or uninstall.
Parent owns preservation of all old ACK/standalone states. Scoped
-ExecutionPolicy Bypass applies only to a new PowerShell process; never change
persistent execution policy. Linux/ARM64/product package matrix and benchmarks
are inapplicable to this refusal-only standalone Windows runner. A runnable
interrupting worker remains blocked unless an independently reviewed mechanism
satisfies the original deadline condition; this specification relaxes none of it.

### Checkpoint runner blocker evidence

Specification commit 2df51d7 precedes the refusal-only implementation. Offline
lab-checkpoint-runner-test.ps1 passed on Windows PowerShell 5.1.19041.6456 and
PowerShell 7.6.9. It checks the refusal entry point/unused inputs and managed
controls, and reproduces a scheduling gap from a final 299999 ms check to a
simulated call at 300001 ms. No native interop is invoked. Fake pin retention
is only witness state, not an implemented worker ownership test.

The runnable interruption implementation and its operational acceptance suite
(retained actual launch handle, bounded binary parsing, fixed path pins,
freshness, late launch/early exit, actual worker pin lifetimes and same-handle
death confirmation) remain unimplemented due to the strict deadline blocker.
Do not interpret passing blocker checks as passing those requested tests.

New ignored build/ack-checkpoint-runner-blocked-review freezes unchanged inputs
from the verified reconciled index, current source, test logs, hash inventory,
effects and a refusal-only exact guest command. Status: BLOCKED, pending independent
review, NOT guest-ready; parent VM/MSI execution must not follow this bundle.
The runner does not read or mutate any protected path, state, ACL, log or package.
Diff review found no launch/termination/native import or cleanup path, production
change, authority relaxation or package hash rewrite. No benchmark/profile or
production matrix is relevant to this refusal-only Windows lab entry point.

### Disconnected callback context observation evidence (2026-10-30)

Base: 50e811525d56f243f25d5b9eb8391e43d4cedcb1, including revised contract
59c566631dab25e2844e8e82997e5ea98d88b56e. Specification cb3b5b1 precedes code.
The independently reviewed option-1 revision is implemented only as an observation;
no production callback invokes it. Original client correlation remains Unknown,
both MSI exports unconditionally fail, and old eligibility/settlement are unchanged.

Build without WIX_TOOL_DIR in build/callback-context-x64 and
build/callback-context-arm64 using the standalone VS 18 BuildTools commands.
Build --config Release --target foreground-test. Both architectures compile /W4 /WX
with MSVC 19.51.36248 / SDK 10.0.26100. Run x64 CTest -C Release -V (2/2 passed),
ARM64 CTest -C Release -R '^foreground-sequence$' -V (1/1 passed on x64).
Logs: each directory's build.log and test.log. No MSI was built.

Native x64 observed the current process token with exact TokenId, matching LSA
SID/session/authentication LUID and local refusal None. This is current principal
eligibility only, not intended-user identity or callback continuity. Actual
impersonation and identification tokens were selected by exact TokenId;
identification refused. Anonymous query refused with ERROR_CANT_OPEN_ANONYMOUS
(1347), without process fallback; its token fields were unavailable. An actual
restricted thread token refused. Actual AccessCheck denied an in-memory descriptor,
and the adapter refused it. No protected object was opened. Models separately
cover full-field failures, query failure/fallback, malformed LSA structure size,
null/invalid SID, LUID/SID/session mismatch, SYSTEM with nonzero session, service
principals, exact 2/10 type allowlist, nonsplit positive and Full-only negative.
LSA Size bounds the fields read; SID validation bounds decoding. The native LSA
allocation/pointers are OS-owned, not a caller-provided serialized buffer, and
there is no invented total allocation length or protected-object access claim.

Ignored build/callback-context-pe-check.py reuses the existing pe_check function
with only SECUR32.DLL added to its in-memory import allowlist. Both DLLs passed
machine, hard-gate exports, static CRT/system imports, no delay imports and
SYSTEM32 dependent-load flags. Actual direct imports are ADVAPI32, SECUR32 and
KERNEL32. Log: build/callback-context-pe-check.log. The committed MSI checker is
unchanged and would need an explicit secur32 allowlist review before any new MSI.

SHA-256 DLLs: x64
3416a65ab196d89c3d7d4b2b7c9238b9ed509072d5de940fd96eb39fa2b181d4;
ARM64 86c0b9e23d43d374b03c0e78b60f9d7cc28b33553fd16f6b1eb8da5576fb8d56.
Final source/artifact/log hash inventory: build/callback-context-evidence.json.
Initial /W4 warnings (exported unique_ptr and shadowed fixture variable) were
resolved by exporting Worker methods and renaming the fixture local. The first
native run exposed TokenHasRestrictions returning one byte on this host; the
reader now accepts its bounded 1..sizeof(DWORD) output, with zero initialization.
Final focused checks pass after these corrections.

Diff/security review found no process override, RevertToSelf, receipt/property
reference, account-name logging, protected writes, Clean/admission or production
caller. Only the listed foreground files and documentation changed; checkpoint,
runner, trace and package pins are untouched. No unresolved finding in this
bounded implementation; independent review is the next gate. ARM64 runtime,
actual MSI callbacks, designated-user/continuity enforcement, protected opens and
preload trust remain unproved. No VM-ready or guest-ready bundle exists.
No VM, MSI, push, watch, GUI/Linux matrix, benchmark/profile or release was run:
this Windows-only disconnected fixture changes no product or performance path.

### Accepted checkpoint commitment-deadline runner specification

This accepted resolution supersedes only the operational blocker contract in
2df51d7/50e8115; their hard API-entry deadline witness and evidence remain valid
historical records. The 300-second monotonic budget begins before worker dispatch
and limits commitment to coordinate termination of the owned launched client.
It does not bound API entry, return, process death, or observer scheduling latency.
Preemption after commitment may delay the direct API call arbitrarily.

Implement only Run-CheckpointLab.ps1 and its offline test, independently of Run-Lab.
One gate protects Eligible/Cancelled/Committed and immutable observer snapshots.
After frozen pins, exact checkpoint/Pending, selected retained-handle identity and
alive checks, the worker enters that gate and commits only if still Eligible and
the final monotonic time is strictly before expiry. Publish Committed before
unlocking. The same worker then directly calls TerminateProcess at most once on
that retained handle: no queued callback, timer, retry, delay or alternate target.
Queries, launches, waits and termination stay outside the tiny decision gate.
Observer timeout under the same gate permanently cancels Eligible; Committed
reports TerminationDecisionInFlight, possibleLateOperation true and unknown
latency. It must never report cancellation/no-future-call for a committed worker.
Retain actual launch handle and all pins through operation completion, including
late launch after cancellation (which must never terminate). Capture PID and raw
FILETIME immediately from the actual launch handle; requery that same handle
before commitment. Missing access/query, early exit or mismatch forbids killing.
API success is asynchronous and distinct from same-handle SIGNALED death; errors
or unconfirmed death never retry. Client death cannot establish server quiescence.

The closed guest contract is native x64, explicit exact DisposableWindowsVM and
positive Microsoft Hyper-V classification; fixed native System32 msiexec and
fixed /i /qn /norestart /L*V arguments, without arbitrary path/case/properties.
Native Program Files/BimeronCheckpointClientLab holds the exact runner, reviewed
lab-protected-ack-Checkpoint.msi and run.manifest. Independent expected runner,
manifest and MSI pins are mandatory; manifest/MSI also have compiled frozen pins.
Parent provisions native Program Files/BimeronProtectedAckLab/LABProtectedFiles
with the same manifest. Runner validates and retains protected ancestors/files,
never repairs ACLs, archives or deletes state. Fresh pending.bin/checkpoint.bin
absence and UNKNOWN product state precede launch. Parse the bounded binary
manifest separately from the original three-variant JSON contract; require exact
frozen public IDs, architecture 1, variant 3, GUIDs, MSI/DLL/projection hashes.
Require exact 80-byte BLCP/version/IDs/whole-manifest SHA/code 1/reserved zero and
48-byte BLPF/version/Pending/IDs/counter zero/reserved zero before commitment.
All source/artifact checks of reconciled index
6ddb8fe82ad514fd0613637e2186513705290580ae885bb1faf13e503108e966 carry
forward by unchanged bytes. No MSI rebuild or original runner edits.

Offline PS 5.1/7 tests extract/compile exact interop but never invoke native APIs
or launch/terminate real processes or MSI. Deterministic mocks exercise cancel
wins, commit wins, exact expiry, accepted postcommit preemption, late launch,
early exit, bad checkpoint/IDs/manifest/identity, API error, unconfirmed death,
pin lifetime and exactly one same-retained-handle call. Preserve the original
hard-deadline witness as a superseded-contract regression. Freeze a new ignored
READY FOR REVIEW bundle with unchanged MSI/DLL/manifest, committed source archive
and hashes from those same immutable bytes, test logs, effects and exact proposed
guest command. Never overwrite older bundles. Parent alone reviews/provisions/runs
a VM; no guest execution is authorized here. Preserve all isolated state and stop
orchestration on timeout or ambiguity, including possible late MSI server work.
No Clean, authenticated initiator, production, full matrix or performance claims.

### Checkpoint commitment runner implementation and offline evidence

Specification cf12d14 precedes the operational implementation. The separate runner
now launches only the frozen Checkpoint client via CreateProcessW and retains that
actual returned handle (never a Process.Handle/PID reopen). One gate publishes the
immutable Committed/Cancelled decision; the same worker directly invokes the sole
TerminateProcess adapter. Its final clock read is strict <300000 ms. Observer wait
uses the same worker clock's remaining budget; scheduling latency is unbounded.
API return and same-handle SIGNALED death are separate fields. Timeout never joins,
stops or disposes the worker. Pins and actual handle survive late launch and wait.
Even completed client death leaves serverQuiescence, initiatingClient and clean
Unknown, and possibleLateOperation true: MSI server work may continue independently.

The exact four-parameter entry point has no case/path/property/test-mode selector.
It requires native x64, literal lab acknowledgment, positive Hyper-V classification,
independent expected runner/manifest/MSI hashes, compiled frozen MSI/manifest hashes,
fixed native System32 msiexec and fixed /i /qn /norestart /L*V. Protected Program
Files paths are mapped once to volume-GUID paths. Retained root-to-leaf handles
reject reparse points, nondisk files, extra links and unknown/nonzero directory case
flags. At/below Program Files require trusted owner, protected DACL and no untrusted
mutation grants. A separate known-folder pin matches the traversal anchor identity.
System executable ancestry is metadata-pinned, relying on the fixed native system
location; no system ACL repair or new system DACL-protection requirement is imposed.
The parent must provision protected files as well as directories before invocation.
No existing Pending/checkpoint may be present; no state is overwritten or deleted.
The only runner-created file is a fresh uniquely named verbose MSI log in the client
root, reserved CREATE_NEW and retained through worker completion. JSON stdout names
that log and records the observed decision, raw identity and uncertainty fields.
CreateProcess inherits the invoking shell's authority; the runner never elevates.

PS 5.1.19041.6456 and PS 7.6.9 each passed 106 managed assertions plus AST closed
surface/forbidden-operation checks, compiling the exact native declarations without
calling them. Tests run the actual worker with deterministic fake-handle barriers:
late launch/cancel wins, validation/cancel race, commit wins, exact final expiry,
postcommit modeled API entry at 900000 ms, early exit/query/identity refusal,
manifest/record negatives, API false/exception, unconfirmed death and one retained
handle call only. A real owned temporary read pin additionally blocks a write open
through cancellation/late launch and is released only at worker completion. Native
struct layout is checked without native calls. The original hard-deadline scheduling
counterexample remains a superseded-contract assertion, not an active requirement.
No real executable launch, process termination, MSI session or VM action occurred.

Frozen input verification passed: pinned reconciled index, all 65 inventory entries,
66 SHA256SUMS entries, 180 exact ZIP/source-hash/Git-blob matches and seven actual
manifest/MSI/DLL/projection bindings. The previous P2 source-inventory mismatch is
closed by exact immutable-byte reconciliation, without changing any frozen input.
No MSI/DLL rebuild occurred; carried-forward source/table/import/build/ICE evidence
is copied intact. Fresh ignored build/ack-checkpoint-runner-ready-review-cf12d14 is
READY FOR REVIEW only, with immutable inputs, new committed source archive, hashes
computed from those same ZIP bytes, test logs, effects and exact proposed guest
command. Source/archive equality is verified after the implementation commit.
Old blocker and checkpoint bundles remain untouched. Parent independent review and
separately authorized provisioning/execution remain outstanding; this is no runtime
or production acceptance. Preserve the isolated VM and every old/new state on any
ambiguity or timeout; do not start another case, uninstall, retry or cleanup.

Focused offline checks and git diff --check are the CI-equivalent checks for this
runner-only change. Diff review found no PID reopen/discovery, host targeting,
queued termination, retry, state cleanup or original Run-Lab/ACK/context/CMake edits.
Native path/ACL/query availability, actual msiexec launch/checkpoint timing and
client/server behavior remain untested pending parent VM work. Windows ARM64 and
both Linux targets cannot execute this native-x64-only disconnected runner and no
product build input changed, so the full product/package matrix is inapplicable.
No performance-sensitive product path changed; benchmark/profiling and release
work are inapplicable. No push/watch, production authority or Clean claim.

Checkpoint final review correction: reserve the CREATE_NEW log through a native
metadata-only handle (desired access zero, read/write sharing, no delete sharing).
A writable reservation could conflict with msiexec's logger sharing mode. This
preserves the exact new-file/no-replacement effect without imposing a competing
read/write access requirement. Exact interop compilation and both offline suites
must pass again; native logger behavior still awaits the parent VM. The initial
f91af47 review bundle remains frozen, superseded by fresh ignored
build/ack-checkpoint-runner-ready-review-final. Its source ZIP is assembled from
exact Git blobs after the final commit and both suites run on those extracted
immutable runner/test bytes. The rejected host-EOL git-archive output stays in the
initial bundle as explicitly labeled preparation diagnostics. No old bundle is
modified or relabeled. Intervening context-observer commits were made by another
writer despite the stated no-other-writer assumption; these checkpoint commits
change only the authorized runner/test/README/canonical document files.

### Checkpoint ownership retention fix acceptance (P1)

The existing accepted retention contract requires confirmed owned-client death
before releasing a launched operation's actual handle and input pins. Repair the
final-wait failure path without changing the commitment gate or deadline contract.
Acceptance: a throwing wait or returned wait with unconfirmed/failed death query
must retain the worker, actual handle and pins in an explicit static pending owner,
report Outcome Unresolved, Completed false and possibleLateOperation true, and
remain sticky across observer timeout and GC. No retry, polling or recovery API
is required: this diagnostic retention lasts until host exit. Observe signals that
the worker has returned, not resource release. A confirmed same-handle death may
release once; launch failure before any process exists may release. Late launch
and in-flight operations retain ownership. Offline exact-worker mocks must cover
these branches on PS 5.1 and 7 without native execution. This is an existing
contract bug fix, not a new termination or packaging contract.

P1 retention fix offline result: PS 5.1.19041.6456 and PS 7.6.9 each pass 150
assertions plus AST/interop compilation checks, including throwing final wait,
returned wait/unconfirmed death, death-query failure, initial/final identity-query
failures, explicit pending-owner/actual-handle retention across GC, sticky timeout,
preprocess launch failure and exactly-once release after confirmed death. The
unresolved worker returns without polling; static pending ownership deliberately
lasts until host exit with no explicit recovery API. Completed is false even after
Observe returns true for that worker. No termination retry or alternate target.

Fresh ignored build/ack-checkpoint-runner-retention-review freezes the full exact
committed Git-blob source ZIP/hash inventory, tests on ZIP-extracted runner/test
bytes, unchanged prior frozen MSI/DLL/manifest and updated effects/command. This
bundle requires a fresh parent independent review round before any VM work. Older
bundles are preserved. Focused PS mocks and diff --check are CI-equivalent here;
no native launch/termination, MSI build/action, VM, push/watch or release ran.
Diff review found the P1 release-on-unknown-death path corrected and no unrelated
ACK/context/MSI/CMake changes. Windows ARM64/Linux builds and benchmarks/profiling
are inapplicable to this disconnected native-x64-only runner with no product build
change. Native path/ACL/launch/death behavior and late server work remain unproved.

### Accepted fixed system image pin correction (MEDIUM)

The parent source-33dd88a9 attempt failed in the operation constructor before MSI
launch: native System32 msiexec had two links. No Pending/checkpoint/log or
termination occurred. A last-error value after a successful API is not failure.
Replace the OS descriptor bypass with explicit TraversalOnly, LabProtected and
TrustedOS roles, and an internal fixed SystemImage role constructed only through
GetSystemDirectoryW validated C:\Windows\System32\msiexec.exe. SystemImage alone
accepts link counts >=1; all lab leaves still require exactly one. No caller path,
PATH lookup, exact-two exception, image copy, ACL repair or signature assumption.

Validate held-handle owner and non-null present DACL at Windows, System32 and
image against exact SYSTEM, Administrators or TrustedInstaller. TrustedOS permits
safe inherited DACLs without SE_DACL_PROTECTED; LabProtected still requires it.
Reject unknown/callback ACEs and effective untrusted mutation/control/delete-child
rights; ignore known inherit-only ACEs for the current object. Preserve root
traversal, disk/type/reparse/case/identity checks, retained read sharing without
write/delete, fixed path binding, all frozen hashes/records/freshness and the
commitment deadline and P1 retention behavior.

Acceptance: PS 5.1/7 exact-source native-free 150 worker regressions plus link,
metadata, role and descriptor models. Separately label owned hardlink identity and
sharing checks (not trusted OS positives), and read-only exact native system-path,
ancestor descriptor/metadata/identity and retained-open compatibility checks.
Never construct CheckpointOperation or launch/terminate/MSI/VM in native checks.
Report native API errors without relaxing sharing. Freeze a NEW ignored bundle
from one immutable commit with exact source archive/hashes, unchanged MSI/DLL/MF,
logs, native evidence, effects and proposed command. Preserve older bundles.
Parent fresh independent review precedes any second new-bytes VM attempt; no retry
is automatic. No product matrix, Core, production, benchmark or release changes.

Fixed-system-image implementation evidence: specification 2c7c900 precedes code.
PS 5.1.19041.6456 and 7.6.9 pass the unchanged 150 worker regressions and 34
additional exact-source role/link/metadata/descriptor assertions. An ObjectAce
SDDL with no GUID normalizes to CommonAce; the unknown-ACE regression therefore
constructs an actual ObjectAce rather than claiming that string tested rejection.
The initial native-fixture C# definite-assignment error was corrected before checks
passed. No check was bypassed. The worker/format/pins/deadline/retention code and
frozen MSI/DLL/manifest constants are unchanged.

Run lab-checkpoint-runner-test.ps1 normally for native-free models; select
-NativeReadOnly separately for native evidence. The latter opens only the exact
GetSystemDirectoryW-selected system image and ancestors, retains read-only sharing,
checks metadata/case/identity and actual handle owner/DACL, compares DOS and
volume-GUID image identity, and tests a compatible second read open. On this host
msiexec has two links and exact TrustedInstaller owner. The owned temporary native
hardlink fixture independently proves shared identity, lab metadata refusal and
write/delete alias sharing violations; it is not an OS trust positive. It deletes
only its own temporary files after closing them. No CheckpointOperation constructor,
process launch/termination, MSI API/action or VM operation runs in these checks.
These host observations do not establish guest API availability, logger/loader
compatibility, actual launch/death, MSI server quiescence or production authority.

New ignored build/ack-checkpoint-system-image-review freezes one immutable committed
source archive and same-byte hash inventory, exact extracted-source PS checks,
separate native-query evidence, unchanged frozen checkpoint inputs and updated
effects/proposed command. Older bundles remain preserved. Parent fresh independent
review is required before a second new-bytes VM attempt; no automatic retry.
Focused checks and diff --check are the relevant CI-equivalent checks. Diff review
found no worker/record/hash/ACK/context/Core/production changes or broader lab-link
admission. Product/architecture matrix and benchmarks/profiling are inapplicable to
this disconnected native-x64-only runner correction; no MSI build or release.

### Accepted callback context trace batch

On immutable base db810bc747cc85d440b73c3cac6cbb9a0ee219b4, add default-OFF
FOREGROUND_LAB_TRACE_CONTEXT, requiring FOREGROUND_LAB_TRACE. Only context-enabled
trace DLLs gain its definition, the small shared token/LSA capture/evaluator and
secur32. Preserve the historical V1 record and unchanged WiX: five synchronous
checked Binary actions, types 1/3329/3073/3585/1, sequences
1499/1501/1502/1503/6601, empty Component/FeatureComponents and zero resources.
No ACK/Pending machinery, runner, production or frozen artifact changes.

Extract only reviewed token selection/capture and LSA evaluator/types. Retain the
exact selected handle through all queries and foreground descriptor AccessCheck;
keep AccessCheck wrapper, worker/journal, exported interfaces, Unknown correlation
and failing exports in foreground. Context trace invokes descriptor-free shared
capture exactly once per callback, independently of historical V1 observation.

Add a separate BIMERON_LAB_CONTEXT_V1 bounded flat numeric record (version 1):
callback ID 0..4 in export order, complete, eligible, process session, source/type/
impersonation, selected TokenId and authentication LUID (16 hex), token session,
elevation type/flag, integrity, enabled/deny-only admin, restricted/AppContainer,
service and SYSTEM flags, token status, LSA status/data validity/session/LUID/logon
type, SID equality and refusal. No SID strings, names or MSI properties. Partial
queries, unknown enums, exceptions and failed logging cannot establish complete
eligible evidence. Fixed compiled MSI returns survive every observer/logging
failure. MSI return and local eligibility grant no authority, original-client
attestation or silent continuity proof; option 1 remains procedural same-user.

Explicit checker context profile enumerates actual DLL/API imports, rejects unknown
symbols/ordinals and all delay imports, permits only LsaGetLogonSessionData and
LsaFreeReturnBuffer from SECUR32, checks exactly five exports, static CRT and exact
SYSTEM32 flag. Historical default profile remains unchanged. Review actual other
imports before fixing API sets; no blind DLL allowance. Test record bounds,
exceptions, unknown enums, incomplete evidence, retained selected-token/LSA matrix,
SYSTEM refusal and exact 2/10 logon allowlist. Recheck context-OFF historical trace.

Build six fresh context DLL/MSI pairs in separate x64/ARM64 directories (/W4 /WX,
cached WiX, unsuppressed ICE), inspect actual tables/Binary/PE and 18 unique GUIDs;
run native x64 tests and foreground regressions, ARM64 compile/host statics only.
Freeze a new ignored bundle from immutable source commit/archive before appending
evidence, with matching source hashes, artifacts, GUIDs, imports/tables/tests/ICE
logs and exact effects. No VM-ready claim before independent review. Parent-only
later Success VM cases are Session0/InteractiveHighest/Limited; incomplete or early
failure is valid unavailable evidence, never authority. No host MSI actions,
protected opens/writes, network, override, VM, push/watch/merge or release.
Linux/product matrix, benchmarks and profiling are inapplicable to this isolated lab.

### Context trace implementation and record contract

Enable both FOREGROUND_LAB_TRACE=ON and FOREGROUND_LAB_TRACE_CONTEXT=ON in fresh
build/trace-context-x64 or build/trace-context-arm64, using the installed VS 18
BuildTools instance and explicit cached WIX_TOOL_DIR shown above. Build Release
--target foreground-lab-trace foreground-test lab-context-record-test.
Run x64 CTest -R 'foreground-worker|foreground-sequence|foreground-context-record|foreground-lab-static'
and ARM64 host CTest -R 'foreground-sequence|foreground-lab-static'; never run ARM64
executables on x64. Build the context-OFF regression in separate
build/trace-historical-x64 and build/trace-historical-arm64 directories with only
FOREGROUND_LAB_TRACE=ON, and check their historical profile. No default product
build or install rules are involved.

lab-trace-check.py <Release-directory> <x64|arm64> context selects the new API
policy. Omitting the profile (or explicit historical) preserves the old DLL policy
and refuses SECUR32. Actual .imports.json reports enumerate every direct symbol;
only the two named LSA functions are accepted from SECUR32. MSI SDK ordinals
8/17/103/125 were mapped through native system msi.dll exports to CloseHandle,
CreateRecord, ProcessMessage and RecordSetStringW, respectively (Msi prefixes).
ADVAPI symbols are token/SID read-only observation; BCRYPT supplies the historical
SID hash. KERNEL32 includes reviewed static-CRT I/O, environment, dynamic-loader,
exception and termination imports; their presence does not mean the callback
uses them for these purposes. Source contains no new file/network/termination or
loader operation. ARM64 CRT additionally imports VerSetConditionMask and
VerifyVersionInfoW. Import checks bound direct dependencies, not dynamic behavior.

The added record begins BIMERON_LAB_CONTEXT_V1 and uses a fixed 2048-wchar buffer.
All values are unsigned 32-bit numbers except impersonation (-1 for primary or
unknown, otherwise 0..3) and three exactly 16-lowercase-hex LUID strings. Callback
IDs are 0 Preflight, 1 Rollback, 2 DeferredBegin, 3 Commit, 4 PostFinalize. Source
is 0 unknown, 1 thread, 2 process; type is 0 unknown, 1 primary, 2 impersonation;
elevationType is 0 unknown or native 1..3. Refusal is 0 None, 1 TokenOpen,
2 TokenQuery, 3 Authority, 4 Principal, 5 LsaQuery, 6 LsaData, 7 LogonType,
8 DescriptorAccess (never selected by descriptor-free trace). Unknown enums
produce incomplete evidence with refusal TokenQuery. LSA status is its unsigned
32-bit NTSTATUS representation. TokenId/authenticationId/lsaAuthenticationId
use high then low halves. Flags are 0/1. SID strings remain internal; only equality,
SYSTEM and service flags leave the observer.

complete requires known field enums, successful process/token/LSA queries and
valid LSA data; it does not imply policy eligibility. Mismatch, SYSTEM, service,
Session 0 and non-2/10 logons refuse eligibility. eligible additionally requires
the shared full-authority evaluator and no recorded refusal. Neither is an
installer authorization result. Capture exceptions emit complete=0/eligible=0
and numeric ERROR_NOT_ENOUGH_MEMORY. Logging errors attempt one complete=0 fallback
with ERROR_WRITE_FAULT without recapture. Missing, duplicate, unparseable or any
incomplete context record makes that callback's context evidence unavailable;
a failed logging fallback can itself be absent. A logged record cannot attest
message delivery or the eventual MSI return. Original V1 messages are unchanged
and independently observed; token observations may differ between those two reads.

Shared callback-context code contains only selection/capture and LSA policy. The
foreground descriptor wrapper retains the same selected token for AccessCheck;
worker/journal and old exported interfaces stay in foreground. The historical
foreground MSI checker remains unchanged and predates baseline SECUR32 linkage;
its foreground-msi-readonly target is not part of these focused regressions.
No old allowlist is broadened to accept the newly linked foreground DLL.

Exact proposed later effects, for parent review only: copy the externally pinned
new x64 Success MSI to a fresh parent-approved guest input location; create distinct
private verbose logs for one Session 0, one InteractiveToken Highest and one
InteractiveToken Limited invocation, sequentially with terminal-state checks.
The parent may register/remove only exact demand-only tasks after confirmed
completion; no runner change is supplied here. Windows Installer performs its own
engine extraction/loading, logs/cache and possible registration writes despite
zero installed resources. Callbacks only read effective tokens/LSA and emit MSI
messages; no ACK, Pending, checkpoint, protected-object opens/writes, network,
cleanup, override, retries or authority publication. Limited/early-failure or
incomplete records are valid unavailable evidence. No VM-ready claim or guest
permission is conveyed; review precedes all parent execution.

### Context trace offline evidence (2026-10-30)

Specification commit 10cd3ac9e70aa9dc80bf758be990c32251844788 is recorded before
implementation.
Immutable implementation source is
751426da89c586ae66ab79b7b7f37fc2ed03e48d, based exactly on
db810bc747cc85d440b73c3cac6cbb9a0ee219b4 in isolated t3/e5d3f004.
The initial checkout was clean and T3-attached, distinct from main and the parent;
only that child branch was reset to the accepted base before edits.

Both context architectures compiled all three DLL/MSI variants and foreground/
record-test targets with MSVC 19.51.36248, SDK 10.0.26100, /W4 /WX and static CRT.
All six context MSIs linked with cached WiX and normal verbose unsuppressed ICE,
without WiX warning/error diagnostics. Actual read-only MSI tables, embedded Binary,
PE machine, exact five exports, direct DLL/API imports, no delay imports, exact
SYSTEM32 flag, zero resources, action types/sequence and 18 globally unique context
GUIDs passed. Each architecture retained 60 negative database models, plus context
API/ordinal/DLL/LSA-set policy negatives and actual PE API/ordinal/delay mutations.
The historical default rejects the new context DLLs.

x64 focused CTest passed 4/4: unchanged worker/context/LSA/AccessCheck/failing-export
regressions, generated source sequence, new record tests, actual lab statics.
ARM64 compiled the same targets and passed 2/2 host source/static tests; no ARM64
code executed. Native x64 selected process/impersonation/identification TokenIds
matched; restricted tokens refused and anonymous query was unavailable (1347)
without process fallback. SYSTEM/nonzero-session refusal, nonsplit positives,
exact logon 2/10, mismatched/malformed LSA and full-field errors remain model
coverage. No protected-object positive is claimed. New tests cover exact capacity,
bounds, unknown enums, incomplete fields, single capture, exception and all three
MSI logging failure stages with unchanged compiled return behavior.

Context-OFF x64/ARM64 builds independently produced six historical-regression pairs
with normal unsuppressed ICE. x64 focused CTest passed 3/3, ARM64 host checks 2/2;
both also passed no-profile checker invocation. Default options were verified OFF;
context ON without trace ON refused configuration. Mechanical source review proved
V1 observation bodies, WiX, shared types/evaluator/LSA parser and query bodies
unchanged by extraction. Descriptor AccessCheck remains in foreground with the
retained selected handle; worker/journal, original Unknown and failing exports
retain their behavior. Review found no new production linkage, protected I/O,
properties, account/SID-string logging, override or authority claim.

Initial new test identifiers collided with Windows small/TokenSource names; renamed
before final passing builds. The strict ARM64 import check first refused two CRT
version-query symbols; actual dumpbin review justified only those exact additions.
Initial archive line-ending conversion was detected before freeze; regeneration
with git -c core.autocrlf=false archive made extracted source exactly match Git
blobs. These resolved preparation failures and original logs are retained.

New ignored immutable bundle: build/trace-context-review-751426d. Source ZIP SHA-256
fdc6d0155650cc9d0ab8e2356216e8f41a5937512d31671fe8db93c3083f654a;
SHA256SUMS.txt SHA-256
aaf63e5f095d6ecd7f63f28c9556273d3c2676a20155c77d2240d497ecdb2ad1.
artifact-index.json binds all context and historical MSI/DLL hashes and actual
GUIDs; source-sha256.json hashes immutable archive bytes. Source and archive were
frozen before this evidence append, with extracted sources verified against that
Git commit. The bundle contains actual tables/imports, caches, build/ICE/test logs,
configuration refusals and exact effects.txt. It is offline review material only,
NOT VM-READY. No old frozen input or allowlist was changed. No host MSI actions,
VM, protected I/O, network, push/watch/merge or release occurred. Parent independent
review precedes any Success Session0/InteractiveHighest/Limited observation.
Linux/product/package matrix and benchmark/profiling are inapplicable to this
isolated Windows lab. ARM64 runtime, actual callback context/delivery, preload trust,
protected access and production authority remain unavailable.

### Context-trace parent execution procedure

Use only frozen source 751426d x64 Success MSI SHA-256
1ab76fde413c54e25e695ad0f4493cd0d86111459b98678a58f9c0f8f11e436b
(ProductCode {2055AE98-AEBD-4FF4-B7E8-2F5219A600CE}). Parent verifies the
integrated implementation is byte-equivalent and input hash in the guest.
Provision fresh protected C:\ProgramData\BimeronTraceContext751426d inputs,
with SYSTEM/Administrators mutation rights and the OS-observed desktop SID read
access. Give that SID write access only to separate per-case log directories.
Preserve all ACK, checkpoint, Core reader and original trace states.

Observe Session0 first via PowerShell Direct, then Highest and Limited using
separate demand-only InteractiveToken tasks selected by revalidated desktop SID
and positive session. Each action is the exact native System32 msiexec path with
/i fixed MSI /qn /norestart /L*V! and its fresh absolute private log. No properties,
transforms, patches, wrappers, triggers or credentials. ProductState must be -1
immediately before each launch. Bound parent observation to 300 seconds, disable
Scheduler hard termination/execution limit, and never kill on timeout. Remove only
the exact completed task, after capturing terminal log/result and zero instances.

Require complete original V1 and separate context records in the expected four
successful callbacks, no rollback and successful Finalize for completed cases.
Inspect current-principal/token/LSA eligibility without interpreting it as client
authentication. Limited may fail before callbacks; that is unavailable callback
evidence, not a substituted standalone positive. Unexpected refusal, incomplete
records or timeout stops subsequent cases. No retry, failure variant, uninstall,
cleanup or production/native operation is part of this procedure. Task launches
are not evidence of human UAC consent. Existing exclusions and all authority gates
remain unchanged; source review and focused tests are not a release matrix.

### Actual context-trace observations (2026-10-30)

The parent integrated the reviewed source with implementation-file equality to
751426d; parent x64 foreground tests passed 2/2, ARM64 compiled and sequence 1/1
passed. Frozen Success MSI 1ab76fde413c54e25e695ad0f4493cd0d86111459b98678a58f9c0f8f11e436b
was hash-checked in the guest and executed once per accepted context. All three
ProductState results remained -1. Original V1 and separate numeric context records
are retained in private build/vm-lab-evidence-20261030/trace-context-* logs.

Session0: exit 0, four complete context records in 0/2/3/4 order. Preflight and
PostFinalize were high user contexts in session 0, refused Principal (4).
Deferred/Commit were SYSTEM, restricted, session 0/logon type 0, refused Authority (3).
Highest InteractiveToken task: result 0, four complete records 0/2/3/4.
Preflight/PostFinalize were unrestricted high interactive session 1/type 2,
eligible 1/refusal 0. Deferred/Commit were restricted SYSTEM session 1/type 0,
eligible 0/refusal 3. Limited task: result 1603, only complete Preflight record,
restricted/filtered user session 1/type 2, eligible 0/refusal 3; later callbacks
are unavailable evidence. No rollback callback was observed in successful cases.

Each temporary task had zero instances at completion and was removed by exact
name. Task configuration records requested InteractiveToken run level; the original
MSI client's primary token was not independently captured in these task launches.
Observed callback tokens establish their current context, not causal initiation,
human UAC consent or designated-user continuity. SYSTEM interactive refusal does
not prove or disprove its separate privileged-access role. No protected-object
open/write, preload provenance, ACK, Pending or production action was exercised.
Inputs/logs and existing lab state were retained; no failure variant, retry,
uninstall or cleanup ran. ARM64 runtime and RemoteInteractive remain untested.

### Checkpoint new-runner early-exit observation (2026-10-30)

After independent c260f52 system-image-policy review, the parent archived the old
prepared client directory intact to BimeronCheckpointClientLab-prelaunch-33dd88a9,
verifying file hashes and descriptors, then provisioned reviewed runner SHA-256
e4466dbd1fbf538311a2b3eda5f84bd84bc5695cd771d5163cd04f22e0be9ba1.
The unchanged MSI/manifest were pinned. One invocation reported Cancelled,
EarlyClientExit, Completed true, same-handle Signaled death and TerminationApi
NotCalled, child PID 5052/creation 01dd68661a44354c. No termination occurred.
Pending/checkpoint were absent, and standalone Pending hash was unchanged.

The retained unique log records MSI error 1314 for the volume-GUID package
directory, followed by server/client MainEngineThread return 2, before any callback.
This establishes an early package/path failure in this experiment, not a successful
interruption, checkpoint or Pending-retention proof. The runner did not capture a
separate native process exit code; engine-log return 2 is the available evidence.
All prepared inputs, archives and log remain retained. No retry, uninstall or cleanup
ran. Private result/log are under build/vm-lab-evidence-20261030/checkpoint-c260f52f-*
and checkpoint-9701525611d543f48da06123a987d391.log. New path-policy diagnosis and
review are required before another invocation; no sharing/security check was bypassed.

### Accepted checkpoint DOS argument identity correction (MEDIUM)

The c260f52f observation is MSI error 1314 (specified path unavailable), not Win32
privilege error 1314. The GUID log was created and GUID package paths reached both
client and server; this does not establish blanket GUID-path incompatibility.
Use native Program Files DOS arguments as a bounded compatibility hypothesis,
not a proven root cause or service-consumption proof.

Only external /i and /L*V arguments change to the native Program Files known-folder
DOS path plus fixed suffixes and an internally generated unique log GUID name.
Keep all GUID traversal, metadata/security, hashes, freshness, lab single-link and
SystemImage distinctions and retained pins. Immediately before CreateProcess,
revalidate the original fixed-drive-to-volume mapping and native known-folder
anchor. Retain strict DOS anchor/MSI aliases and compare volume/file IDs against
original GUID pins. Reserve the GUID log with metadata-only CREATE_NEW access;
open its DOS alias metadata-only with read/write sharing and no delete sharing,
validate metadata/security and compare reservation identity. Any query/error or
mismatch refuses before launch. No caller/environment/PATH/alternate/fallback.
All aliases and original pins remain retained without write/delete sharing except
the two metadata-only logger-compatible handles (read/write sharing, no delete).
Identity is established at validation time, not atomically at service consumption.
Privileged remapping is outside the explicit trusted OS/administrator boundary.

After confirmed same-process-handle signal and before close, query
GetExitCodeProcess exactly once. Publish nullable uint exit code plus bounded
NotQueried/Succeeded/Failed status and nullable numeric error. Query failure does
not alter confirmed-death release; unconfirmed death never queries/reopens by PID
and retains P1 ownership. Keep the 300-second commitment gate, sticky unresolved
retention and at-most-once termination unchanged; no retry/server kill/action.

Acceptance: retain 150 worker and 34 policy assertions on PS 5.1/7; add matching
alias identity, MSI/log/anchor mismatch, changed/failing mapping/fixed-drive/query
refusal, logger flags, signal/query/close same-handle ordering, query-failure
confirmed release once and no query on unconfirmed death. Separate read-only
native DOS/GUID mapping/identity/system opens and owned temporary fixtures are
allowed; no operation constructor, launch, MSI, guest or host protected writes.
Freeze a NEW ignored bundle with exact committed Git-blob source/archive hashes,
unchanged MSI/DLL/manifest, tests/native evidence/effects/command. Preserve old
bundles. Parent fresh independent review precedes future VM work; parent must
archive prepared client root intact under fresh suffix c260f52f without overwrite
before staging. No Core/product/MSI rebuild, benchmark, full matrix or release.

DOS alias implementation evidence: specification 31a238c precedes code. PS 5.1 and
7.6.9 each pass 159 worker/format assertions (all original 150 retained), 34
unchanged policy assertions and 21 alias/mapping assertions, plus AST wiring and
exact interop compilation. New worker cases cover early signal/query/close on the
same fake handle, thrown and native-style failed exit queries, confirmed release
once and no exit query for unresolved/prelaunch failure. Numeric exit error is
nullable; unexpected managed query exceptions report Failed with null error.

The GUID reservation retains desired access zero. The DOS log alias requests only
READ_CONTROL (metadata/security, no data read), with read/write sharing and no
delete sharing. Both undergo same-object identity comparison; the alias applies
strict lab metadata/security checks. Anchor/MSI aliases use existing strict pins.
No ordinary read pin is added to the log. Final mapping/known-folder checks follow
all retained-pin revalidation immediately before native launch dispatch.

Separate PS 5.1/7 native selection passed actual Program Files DOS/GUID identity
and fixed mapping, original system path/descriptor/open checks and owned temporary
log reservation/READ_CONTROL-alias/data-writer compatibility. Owned fixtures do not
prove protected guest positives. No operation constructor, process launch,
termination, MSI API/action or host protected write ran. Model failure injection
and AST wiring are not native service-consumption evidence. No atomic consumption
attestation, privileged-remap protection, actual MSI logger/guest behavior, server
quiescence, initiating-client authority or Clean claim is established.

Fresh ignored build/ack-checkpoint-dos-alias-review freezes exact committed source
and Git/archive hashes, tests on extracted bytes, native logs, unchanged frozen
MSI/DLL/manifest and historical evidence, effects and proposed guest command. Old
bundles remain intact. Parent fresh independent review is required before future
VM work. Parent must archive the existing prepared client root intact to a fresh
BimeronCheckpointClientLab-prelaunch-c260f52f suffix (extend uniquely if occupied),
verify hashes/descriptors, and never overwrite it before staging new reviewed bytes.
No guest changes were performed here. Diff review found no gate/P1/termination
policy relaxation, fallback/PID reopen, retry, server kill or unrelated edits.
Focused PS checks and diff --check are CI-equivalent. Product matrix (Windows x64,
ARM64 and both Linux targets), benchmarks and profiling are inapplicable: this is a
disconnected native-x64-only lab runner and no product/build/package input changed.
No Core/product/MSI rebuild, full matrix, push/watch or release occurred.

### Resume experiment observation (2026-11-01)

Independent review of `31a238c` and `85930a2` found no blocking issue within the
bounded DOS-alias runner delta. PS 5.1 and PS 7 each repeated 159 worker/format,
34 policy and 21 alias assertions; archive/inventory checks matched committed
sources and frozen artifact pins. This does not establish native execution.

The parent accessed the disposable Windows 10 STD VM and confirmed no active MSI
process, no Pending/checkpoint and matching prepared MSI/manifest hashes. The
existing client root was moved intact to
`BimeronCheckpointClientLab-prelaunch-c260f52f`; the before/after comparison matched, but later review found nested inventory
records with missing individual file hashes. It does not prove file-byte equality
across the move. This also does not establish archive corruption. The reviewed runner and unchanged MSI/manifest were
staged with protected SYSTEM/Administrators ACLs and verified guest hashes.

One remote asynchronous invocation was dispatched. The parent collector then
failed because its `Tee-Object` call combined unsupported `-LiteralPath` and
`-Append` parameters, and the collector process exited. No runner result was
captured. Subsequent read-only observation found no new client log, Pending or
checkpoint and no MSI process. This does not prove whether the runner constructor
or launch path ran, explain a refusal, or establish terminal server quiescence.
The experiment outcome is unavailable, not a successful interruption or a repeat
of MSI error 1314. No retry, process termination, uninstall or cleanup followed.

Private evidence is retained under ignored `build/vm-resume-20261101-112745`
(preparation, staged hashes/SDDL and read-only aftermath) and
`build/stage8-resume-tests-20261101` (repeated offline checks). All guest inputs and
the archive remain preserved. A corrected collector needs independent review and
a fresh explicit experiment specification before another invocation; these
observations grant no Clean, enrollment or release approval.

### Next resume experiment: persistent capture host (specification; review pending)

Prepare exactly one new, independently reviewed observation of the unchanged
DOS-alias runner; the lost AsJob/child-PowerShell result remains unavailable.
Nested arrays in the old inventory omitted child hashes: old byte equality is
unproved, without evidence of corruption. Preserve that evidence and every old
archive. This specification is for parent review before implementation. Existing
broad user VM authorization applies after independent review; no new permission
request is required. No VM operation is part of this documentation change.

Before staging, read-only inventory the old archive, current client root and
protected lab state. Emit a flat ordinal-path-sorted list with exactly one record
per object, including root/directories, relative path, kind, length, file SHA-256
and owner/group/DACL SDDL; directory hashes are explicitly inapplicable. Reject
array-valued fields, missing file hashes, duplicate paths, reparse points,
unreadable objects or enumeration errors. Record current identities and absence
checks separately; new hashes cannot reconstruct missing historical evidence.
Refuse dispatch on unresolved prior activity or failed existing freshness gates;
absence of msiexec alone does not establish prior completion/server quiescence.
Archive the current prepared root under a fresh same-Program-Files-parent
BimeronCheckpointClientLab-prelaunch-c260f52f suffix, extending uniquely if needed,
with no replacement, merge or overwrite. Compare complete flat inventories before
and after the move, retaining both. On any mismatch/error stop and preserve the
resulting state; do not repair, undo or retry. Then stage only reviewed fixed bytes
with existing protected ACLs and hash checks; never modify the old archive.

Minimal candidate: one uniquely named, demand-only native Windows Task Scheduler
task, using a fixed native System32 Windows PowerShell host and a separately
reviewed experiment-only host script in protected ignored evidence. No service,
general helper, installed dependency, interactive-logon dependency or credentials.
Use SYSTEM/ServiceAccount explicitly, no triggers/restart policy, IgnoreNew,
unlimited execution time (PT0S), AllowHardTerminate false and no idle/battery stop
conditions, expiration/deletion or execution timeout. Read back the exact task XML
before its one start. These are explicit settings, not defaults; see Microsoft's
[Task Scheduler settings schema](https://learn.microsoft.com/en-us/windows/win32/taskschd/taskschedulerschema-settings-tasktype-element).
SYSTEM is the proposed host principal, not authenticated initiating-client proof;
its compatibility with unchanged runner admission remains a review requirement.
Scheduler ownership should survive remoting/parent loss; reboot, shutdown, guest
failure or privileged termination can still destroy pins. No restart/recovery is
authorized, and these settings do not prove survival by themselves.

The host invokes the exact reviewed Run-CheckpointLab.ps1 in its own PowerShell
process/runspace, with unchanged fixed arguments and runner SHA-256
98004df32700cc1dc24c1a75183533e8ca81143ac50f8188379cee94c315c94c;
reuse the reviewed MSI/manifest pins above. No child PowerShell, remote job,
separate worker process, edited runner, new termination call or disposal path.
After invocation or capture failure the host remains resident indefinitely,
retaining the runspace, loaded static types and sticky ownership; it must also
remain resident while invocation is in flight. No finally/exit/timeout may release
runner resources. The runner alone retains its existing termination authority.

Before any dispatch, test actual collector append, flush and exact readback with
multiple uniquely identified records on the chosen local and guest sinks, using
PS 5.1-compatible explicit writers, not the failed Tee-Object parameter pairing.
Reserve fresh no-overwrite evidence paths. Specify separate success-object,
error, warning, verbose, debug, information/host-message and host-status sinks,
encoding/framing and sequence numbers; preserve runner JSON as bytes plus a
separate parsed copy. Flush each boundary and verify readback; status must never
interpret missing output as success. Host exceptions/capture failures go to an
independent pre-opened status sink when possible; total sink loss is unavailable
evidence and must not cause exit, cleanup or another invocation.

Use a durable one-shot protocol: record flushed intent with experiment ID,
source/artifact hashes, fixed arguments, sinks and task configuration before task
start. The host writes Ready only after sink tests and self-verification, reporting
actual image path/hash, PID/creation time, SID, token elevation/integrity, session,
PowerShell version/bitness and runspace identity. Parent verifies Ready against
the approved identity/configuration, then writes one protected, flushed dispatch
record bound to that identity and experiment. Host consumes it at most once,
flushes invocation intent before entering the runner, and never repeats after an
ambiguous acknowledgment. A failed/missing handshake authorizes no invocation.
Record capture/refusal/return separately from runner completion and server state.

Parent observation is at most 300 seconds total for this experiment; this neither
extends nor changes the runner's own 300-second commitment semantics. Timeout,
transport loss, error or unresolved result ends parent observation only. Preserve
host/task/session, pins, inputs, archive, logs and protected state: no kill,
disposal, task removal, cleanup, retry or recovery. Even confirmed client death
is not server quiescence or Clean. Later inspection is read-only and separately
reported, never an automatic second launch.

Before any VM launch, freeze new source/hash/configuration/effects evidence and
pass offline synthetic acceptance for success, prelaunch refusal, partial/failed
capture, lost transport before/after dispatch, duplicate start/dispatch and
unresolved/inflight retention. Verify one invocation maximum, same host PID and
runspace/static owner, all streams and durable ordering/readback, zero new
termination/disposal calls, and resident ownership after collector/session loss.
Use injected fixtures only, no MSI/native runner operation. Independent review
must resolve exact host lifecycle, sink failure behavior, SYSTEM compatibility,
Scheduler configuration and how synthetic survival evidence limits native claims;
any unmet requirement blocks dispatch, not a silent fallback to AsJob. Native
survival and actual guest identity remain unproved until observed. No product
enabling, build/package flow, signed packet updates, Core/MSI rebuild, release,
benchmark or architecture matrix changes belong here.

#### Capture host implementation clarification (offline contract)

Capture means exact emitted PowerShell strings, encoded once with strict UTF-8,
not original native stdout bytes. Each sink record is ASCII sequence, byte length,
and base64 UTF-8 payload separated by tabs and terminated by LF. Sequence is global
across the seven sinks (success/error/warning/verbose/debug/information/status).
Maximum string payload is 65536 bytes; total capture is 8 MiB. Oversize or failed
capture is unavailable evidence, never runner cancellation. Parsing JSON is a
separate read-only consumer after framing, never part of the pipeline callback.

Synthetic acceptance proves control flow and retained references only. It cannot
prove scheduler survival, native pinning, power-loss durability or SYSTEM admission.
Ready and dispatch must bind experiment, host/config SHA-256, PID and creation time.
Partial, duplicate, preexisting and replayed records refuse. Durable invocation
intent must precede entry; ambiguity never authorizes retry. Every terminal path
enters indefinite residency; observation ends without releasing the runspace,
static owners, pins or in-flight ownership. Capture callbacks contain all errors
and report best effort to an independent preopened status sink.

Native deployment remains blocked until exact task identity/XML/security descriptor
and protected handle verification are implemented and independently reviewed.
The proposed fixed task is path `\BimeronLab\`, name
`CheckpointObservation-c260f52f`, principal SID S-1-5-18, ServiceAccount. Its action
is native System32 WindowsPowerShell\v1.0\powershell.exe with quoted fixed
`-NoLogo -NoProfile -NonInteractive -File "<protected host>"` arguments and fixed
protected experiment working directory. Parent must externally read back image,
arguments, working directory, task security descriptor and every explicit setting
above. No task registration/start is implemented here. Parent must verify and
retain handles for the fixed experiment root, config, dispatch, inputs and all
ancestors, refusing reparse/alias/hardlink or ambiguous owner/DACL/identity state;
path-only ACL checks or caller-supplied boolean attestations are insufficient.
Ready additionally requires independently verified image hash, SID, elevation,
integrity, session, bitness and runspace, plus append/flush/exact readback tests on
all preopened sinks. Config must contain exactly those fixed paths, reviewed source,
manifest/MSI hashes, fixed runner arguments, sink paths, task identity and identity
bindings; canonical config bytes and host bytes are hashed before Ready.

Offline library is not a deployment adapter. Its injected fixture functions must
never be supplied from an untrusted configuration. Production entry deliberately
refuses before Ready/dispatch: the native retained-handle/task/identity verifier
is not yet implemented. Do not stage or start it. This is an explicit unresolved
implementation blocker, not permission to substitute path-based checks. Parent
provisioning and one Task start remain future parent-owned work after exact-code
review. Old outcome remains unknown: read-only activity checks and unchanged
runner freshness cannot establish prior completion. Preserve all old state.

#### Native support follow-up (offline, still not deployable)

ObservationHost-Native.ps1 now supplies actual CREATE_NEW write-through read/write
sinks, Flush(true) and exact same-handle readback; all seven sinks are reserved
before use and partial preparation retains owners. The existing framing bounds
apply. Test records use sequence zero; captured records start at one. Independent
status has its own preopened writer. The scoped runner wrapper merges all six
PowerShell streams, classifies records and contains capture conversion errors.
It invokes the script directly with the frozen runner/MSI/manifest arguments;
no job, child PowerShell, operation constructor or termination is added.

The support library also supplies strict canonical config parsing, exact XML
hash plus explicit setting checks, one-shot CREATE_NEW invocation intent, process
identity diagnostics and retained protected ancestor pin opening. Pin types load
only from runner bytes matching SHA-256
98004df32700cc1dc24c1a75183533e8ca81143ac50f8188379cee94c315c94c:
parse the verified source and evaluate only its single Initialize-CheckpointLab
function definition, then call that initializer. Never dot-source the runner.
The entire unchanged initializer source must also be used by the eventual runner
invocation; compatibility of repeated Add-Type remains a review/test gate.
The checkout runner is not an approved staging source.

Run the bounded checks (no task registration/start or runner invocation):

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File packaging/windows/foreground-prototype/lab-observation-host-test.ps1
pwsh -NoProfile -File packaging/windows/foreground-prototype/lab-observation-host-test.ps1
```

Both pass 108 assertions, comprising existing injected flow tests and real owned
nonprotected temporary sink I/O plus config/hash/schema/XML refusal tests. They
do not exercise protected native pin positive acceptance, task COM readback,
elevation/integrity verification or actual SYSTEM runner admission. No infinite
resident test runs. The temporary fixture alone is disposed/deleted by the test.

Canonical config recipe is strict UTF-8 without BOM, exactly seven LF-terminated
lines: ObservationHost002, c260f52f, frozen runner hash above, approved host hash,
approved exact UTF-8 task XML hash (lowercase 64-digit hex), canonical folder SDDL,
and canonical task SDDL as specified in the correction acceptance below. No extra fields,
CRLF or duplicate lines are accepted. This config is a library codec, not yet a
complete deployable configuration. Before any later staging, freeze copies of
host/support/tests/config/XML and their SHA-256 inventory into a new ignored
build/observation-host-review-<unique> directory using CREATE_NEW/no-overwrite;
copy the runner only from the old reviewed archive and verify the exact hash
above, never from checkout. Preserve all existing archives. This recipe neither
creates a task nor authorizes dispatch.

Historical stage108 remaining implementation gates (superseded by the wiring below):
fixed native-known-folder evidence root selection and full wiring, handle-bound
self image attestation, token elevation/integrity, read-only Task Scheduler COM
XML/security-descriptor verification, protected Ready/dispatch identity binding,
and complete preparation/dispatch/residency orchestration. The current task XML
validator is a component, not a task identity verifier. The process diagnostics
use path-based image hashing and explicitly report Admission Unavailable.
Production entry therefore continues unconditional refusal/residency. No Ready,
deployment or guest-ready claim is made. This follow-up is partial implementation;
independent review and further implementation remain necessary.

#### Persistent host wiring (offline review candidate)

The entry now calls Start-ObservationHost after checking the embedded support
SHA-256. All functions precede entry. It selects native Program Files and fixes
the evidence directory to `BimeronCheckpointObservation-c260f52f`; the host,
support, `host.config`, `task.xml`, seven `*.frames` sinks, `ready.record`,
`dispatch.record` and `invocation.intent` are immediate children. No directory
creation, task registration/start, process launch or runner construction occurs
in preparation. Missing provisioning refuses and remains resident.

The seven-line version 002 canonical config recipe above applies to this fixed
path/argument recipe (no configurable code or paths). Freeze support first,
embed its lowercase SHA-256 in ObservationSupportSha256 in the host, then hash
the resulting host bytes. Freeze the exact COM RegisteredTask.Xml UTF-8 bytes
as task.xml and put their hash on config line five. Line four is the frozen host
hash. Config SHA-256 is included in the handshake. Any edit requires a fresh
review/freeze; never replace existing evidence or regenerate old archives.
Provisioning and one demand start belong exclusively to the parent after review.

Preparation loads only the exact frozen initializer AST, retains protected input
and ancestor pins, verifies artifact hashes, retains current process/token/image
owners, and requires Desktop PowerShell x64, session zero, effective SYSTEM,
TokenElevation=1 and integrity RID 16384. Identity includes retained-image hash,
PID/creation, version and runspace. Task Scheduler COM is read-only: exact fixed
name/path, XML hash and explicit settings/action are checked, together with
protected owner/DACL descriptors for task and folder. This conservative policy
permits task allow ACEs only for SYSTEM and Administrators; a differently
provisioned descriptor refuses rather than being silently broadened.

Seven CREATE_NEW writers perform two exact-readback tests and retain metadata
pins. Ready is a CREATE_NEW flushed bound record, after a separate status frame
containing actual identity JSON. The binding is experiment:hosthash:supporthash:
confighash:pid:creation. Parent must independently compare identity and hashes
before creating exactly `Dispatch<TAB><binding><LF>` at dispatch.record, flushing
and closing its writer. Host opens/pins that file once, refuses partial/foreign
records, revalidates pins/mapping/task, then CREATE_NEW-flushes invocation.intent
before direct same-runspace captured invocation. Preexisting records refuse;
no retry follows ambiguity. Waiting for absent dispatch has no host timeout.
Parent observation remains bounded to 300 seconds and never stops the host.

PS 5.1 and PS 7 pass 114 offline assertions, including actual owned temporary
sink I/O, token interop compilation and task descriptor rejection fixtures.
These tests do not execute production preparation, token queries, Scheduler COM,
protected pin acceptance, runner initialization/invocation or indefinite residency.
Independent exact-code review and parent native observation remain mandatory;
offline evidence does not prove native survival, SYSTEM admission or server
quiescence. The old outcome remains unknown. No VM/task operations, credentials,
deployment, release or production task changes are part of this implementation.

Exact frozen guest-input runner hash 98004df3... was separately verified and its
initializer compiled twice in each fresh PS 5.1/7 process, without executing its
top level or any native API. Repeated Add-Type compatibility passed. The first
PS 5.1 command without ExecutionPolicy Bypass was refused by this machine's
script policy; the corrected offline command with the documented Bypass passed.
The fixed scheduled action intentionally has no policy override: parent must
verify the guest's existing execution policy accepts these reviewed scripts;
this implementation does not change machine policy or the fixed action.

#### Reviewed host correction acceptance (offline only)

Pass the frozen runner path and sinks as an explicit immutable callback context,
never resolve them through dynamic callback scope. Exercise that exact production
callback and captured wrapper with an inert script in the same PID/runspace.
Successful cases require one invocation, exact stream frames and decoded values,
external callback failure flags and retained residency; negative modes require
explicit zero/one counts. In-memory invocation-deletion and path-substitution
mutants must fail the same checks on PS 5.1 and 7.

Config version 002 extends the five original lines with folder and task descriptor
lines, in that order. Each is the exact canonical owner/group/DACL SDDL returned
by RawSecurityDescriptor.GetSddlForm(Owner|Group|Access), ASCII and LF terminated.
Compare both COM descriptors at preparation and immediately before intent; keep
the strict protected-owner/DACL allowlist policy as well. Freeze reviewed canonical
descriptors from independent readback, then support hash, embedded host support
hash, host hash, XML hash and config bytes/hash. No alternate descriptor encoding
or five-line config fallback is accepted. No Task API or native execution belongs
to offline checks. Provisioning and native dispatch remain blocked pending fresh
independent review; the prior VM remains unavailable.

Correction validation: PS 5.1.19041.6456 and PS 7.6.9 each pass 225 final host
assertions, including exact inert wrapper streams/arguments/PID/runspace, retained
owners/residency, explicit invocation counts, descriptor readbacks and both
in-memory mutants. Deletion mutant SHA-256 is
9a82cc3e6e0b08eb71182957b3a23044360a0ce2604536ef79d880f7cde14981;
path substitution mutant is
0ad8812d99910c9a505788b5228318d2857c036922fa55688b02474374586f9d.
PS 5.1 fixture debug emission uses DebugPreference Continue because -Debug prompts
in that shell. Checkpoint regression passes 159 worker + 34 policy + 21 alias
assertions; Run-Lab offline AST/interop/trace/JSON and 47 negatives pass on both.
All three host scripts parse and embedded support hash matches exact bytes.
No native APIs, Task API, VM, credentials, MSI actions or real runner construction
were used by the host suite. The separate existing Run-Lab suite queries its own
harness handle read-only. Product four-target build/package checks and performance
benchmarks are inapplicable to this disconnected host-only correction. Review
found and corrected the SDDL section mask to include group (Owner|Group|Access=14).
Native/provision dispatch remains blocked until independent exact-code review.

Host return-status follow-up (2026-11-01): the host emits a best-effort durable
`RunnerReturned; completion and server state unknown` frame after return and
before residency. Failed status capture preserves retained ownership and marks
capture unavailable. PS 5.1 and PS 7 each pass 233 assertions, including exact
return/status/residency ordering, sink-failure retention and both prior mutants.
Independent review found no remaining scoped code finding. Host SHA-256 is
`b26240488d1b0014a6d549c4e55729ae87575027fb9824b59e574f2855ab4b30`;
support remains `b2b52da454427cb73c8a000889f056b2fe2a01b8e5ff6ddff543e2c136aa4711`.
Earlier hashes/counts above are historical. Freeze these exact bytes before
preparation; no result here proves protected guest pin acceptance, SYSTEM/task
admission, actual runner execution, server quiescence or native survival.


Host atomic sink correction (2026-11-01): acceptance requires CREATE_NEW with
an explicit protected descriptor at creation, never an after-create ACL repair or
weaker LogAlias policy. The shared sink now passes SECURITY_ATTRIBUTES containing
`O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)` to CreateFileW with GENERIC_READ|GENERIC_WRITE,
FILE_SHARE_READ, CREATE_NEW and FILE_FLAG_WRITE_THROUGH. Owner/group are BA;
only SYSTEM and Administrators receive full access. Nonprivileged tokens that
cannot assign this owner fail closed; there is no user-owner fallback. The
synchronous SafeFileHandle transfers to FileStream only after successful wrapping;
failed wrapping closes it, and LocalFree always releases the converted descriptor.
Errors retain created files without pathname deletion/reopening or ACL repair.
Existing State.Owners and ancestor retention remain unchanged on later failures.
New-ObservationRecord shares this constructor, including Ready/invocation records.
No arbitrary-path product API was added.

PS 5.1 and PS 7 each pass 295 assertions (the prior 233 preserved plus 62),
including both existing behavioral mutants. Actual GUID-owned temporary files
verify protected DACL, BA owner/group, two explicit full-access SYSTEM/admin ACEs,
LogAlias admission/revalidation, durable same-handle readback, closed-writer
CREATE_NEW no-overwrite and New-ObservationRecord pin retention. Descriptor policy
rejects inherited and permissive Everyone-write models. The test reconstructs LF
Git-source runner bytes before the unchanged frozen-hash initializer import;
runner top level and operation constructor never execute. Sharing-aware readers
are required while the writer is retained. This privileged host token can assign
BA ownership; this does not establish SYSTEM/task admission, protected ancestors,
guest survival, storage power-loss behavior or native runner execution. Conversion
and wrapping failure cleanup were reviewed structurally, not fault-injected.
No host Program Files writes, Task/MSI/VM actions, real runner constructor/launch,
credentials, release, commit or push occurred. Product build/package matrix and
benchmarks/profiling are inapplicable to this disconnected Windows host fix.

New exact LF source SHA-256 pins (earlier pins above are historical):

- `ObservationHost-Native.ps1`: `d01b6de64586a9154d523b92d1b82ac546d8da442b4544d8526256511c208928`

- `Checkpoint-ObservationHost.ps1`: `a47881ce6b809e4a6cad94ca676392583a9c496a745327cd539401b7a3386442`

- `lab-observation-host-test.ps1`: `544875f5eb3c22bdbf6b08421aaf64a423081bcbb6dd80e4fc815fd5eb691f06`

The host embeds the new support digest. Parent orchestration v2 and its old frozen
bundle are deliberately untouched; freeze new support/host/config/source hashes
and obtain independent exact-code review before any parent provisioning/start.


Process-only observation action amendment (2026-11-01, offline acceptance):
The exact hash-approved experimental Task Scheduler Windows PowerShell action
must use `-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File
"C:\Program Files\BimeronCheckpointObservation-c260f52f\Checkpoint-ObservationHost.ps1"`.
Require MachinePolicy and UserPolicy both exactly Undefined before provisioning;
refuse every configured value, including Bypass. Default Restricted with undefined
scopes is allowed only because this child action explicitly sets process policy.
No Set-ExecutionPolicy, registry/GPO/global setting or parent remoting policy change
is permitted. Reject missing/changed/extra arguments and arbitrary host paths even
with recomputed XML hashes. Preserve all other pins, settings, source selection,
protected state, one-shot dispatch and 300-second bound. Freeze v7 separately,
support first, embedded host digest second, then parent/test pins, task XML and
seven-line config002 hashes; preserve every earlier bundle. Native mode must not
execute during this amendment. Offline PS5.1/7 host, parent and adapter suites plus
DryRun are required. Product packaging/runtime behavior is unchanged.

Reported v6 preparation boundary: effective Restricted, all five scopes Undefined
including MachinePolicy/UserPolicy (no configured policy). HostRoot absent, Client
exists. Stopped before host-root creation, archive, staging, task/MSI/runner launch.
Earlier read-only bootstrap, retained pins/inventory/process queries and local
native-evidence/sink/remoting preparation may already have occurred. Session 86808
and all references remain parent-owned and retained; no cleanup or liveness query
is authorized here. Earlier lost outcome remains Unknown. This is supplied context,
not a fresh guest observation or evidence of quiescence.

Amendment validation: PS5.1/7 each pass 320 host assertions (295 preserved),
parent/adapters and DryRun; checkpoint 159+34+21 and Run-Lab 47 negatives pass.
No native mode, Task APIs, VM, credentials, MSI or real runner launch.
Independent review and native admission remain pending. Product matrix and
performance checks are inapplicable; supported packaging is unchanged.
New exact LF source pins:

- `ObservationHost-Native.ps1`: `e970502dbdd95c5162c84afb0df1093beebb537d353751c66cdf2c9371bb3347`
- `Checkpoint-ObservationHost.ps1`: `9b37fea468f91ca18718c5845c711b774c38a22435eaacff02f76dfd1d9a585b`
- `lab-observation-host-test.ps1`: `a63a073b1173f1ab71253e1230acca76fd198a346af7676925502322c33e34ed`

Ignored frozen candidate: `build/stage8-parent-orchestration-20261101-v7`;
see SPEC.md, REVIEW.md, COMMANDS.md and SHA256SUMS.txt for pins and config recipe.
Actual task XML/config hashes require future verified Scheduler readback; no
guest task/config was created during offline validation. No commit or push.


### Explicit authority boundary correction specification (2026-11-01, offline only)

Supersedes earlier frozen-runner source-selection instructions for this correction.
Volume prefixes strictly above independently resolved Program Files/SystemDirectory
anchors are TraversalOnly: metadata, reparse, case and retained identity checks,
without descriptor authority. Exact anchors are TrustedOS, retaining safe trusted
owner and present non-null DACL validation; inheritance is allowed. TrustedInstaller
already belongs to the unchanged trusted SID set for both OS and lab descriptors.
Only the three fixed owned lab trees are LabProtected, with protected DACLs and
single-link leaves. System image descendant pins use a distinct OSContent role,
with the same safe OS descriptor policy, never lab ownership or anchor authority.
No length comparison selects authority, mutation mask relaxation or ACL repair.
Retain independent known-folder DOS/GUID identity and mapping checks. Original
runner anchor pins and Chain must obey the same roles. Failure reports only the
bounded fixed path/role, never descriptor/SID/environment/account dumps.

Each security helper establishes local Stop and strict semantics even under a
Continue caller; injected constructor and descriptor failures must stop at the
first boundary, retain existing owners and make no later calls. Offline PS5.1/7
checks cover traversal, exact anchor, owned roots, safe TI/inherited OS descriptors,
unsafe ACE/null/callback/unprotected lab refusal. No real operation constructor,
launch, Program Files mutation, Task, MSI, VM, credentials, retry or owner disposal.
Freeze exact normalized LF UTF-8 runner bytes in fresh ignored
build/lab-boundary-review, record raw byte and normalized hashes plus Git diff,
then update support runner pins, embedded support digest and tests in that order.
No circular digest and no MSI/DLL/manifest rebuild; preserve all v7 bundles.
Native Move Access denied remains unresolved, client hostroot onlyclient-before
and all sessions remain retained (reported context, not a fresh observation).
Native preparation remains prohibited pending independent review.


Authority boundary correction offline evidence (2026-11-01): specification above
preceded implementation. Runner Chain uses component equality and explicit owned
roots; exact Program Files and DOS anchor pins use TrustedOS. Host shares that
selector, retains independent DOS/GUID known-folder identity, and uses local Stop
and strict mode in each support helper. OSContent distinguishes safe OS descendant
checks from exact anchor authority; it does not relax descriptor/mutation policy.
Existing trusted SIDs, single-link lab checks and retention behavior are unchanged.

PS5.1/7 each pass checkpoint 159 worker + 41 policy/role + 21 alias assertions,
and host 334 assertions with both existing mutants detected. Constructor injection
fails at traversal; descriptor injection fails at exact OS anchor under Continue,
without later calls or removal of retained owners. git diff --check passes.
Review corrected diagnostic wrapping to preserve native Win32 exceptions used by
checkpoint absence polling. No performance-sensitive path or product build/package
change: benchmarks/profiling and four-target release matrix are inapplicable.

Final source candidate is build/lab-boundary-review/final; PROVENANCE.json records
HEAD blob, exact checkout and frozen normalized LF UTF-8 byte hashes. SOURCE.diff
records the uncommitted changes; this is not claimed to be committed source.
The top-level runner copy is a superseded intermediate candidate, never deploy it.
Final runner SHA-256: 461997fda2094cdd08dcc28ff82917a16350266773959ca36147b38353cebc06.
Fresh host import/config/invocation pins use that final runner. Support is frozen
before the embedded host digest. Earlier archive-only runner instructions and
hashes are historical and superseded solely for this candidate.

No native preparation is authorized: reported Move Access denied is unresolved,
client hostroot onlyclient-before and all sessions remain retained. Parent source
reader/remote Stop correction and parent/config/XML pins require separate review.
No VM, Task, MSI, real operation constructor/launch, credentials, build, commit or
push occurred. No native positive or guest survival evidence is claimed.


### Fresh fixed boundary attempt (2026-11-01, specification only)

Reported v7 preparation continued after ACL errors because remote fail-fast was
missing. The old `BimeronCheckpointObservation-c260f52f` host root contains only
`client-before680`; the old client root remains. Session 86808, all handles and
all other references remain retained. No task or MSI ran. Native Move AccessDenied
cause remains UNKNOWN; this layout choice neither diagnoses Move nor bypasses ACLs.
Explicit authority roles and parent fail-fast corrections are implemented but
pending independent review; no native positive evidence is implied.

For experiment `boundary-v1` only, supersede archive-before-staging with these
new fixed paths beneath independently resolved native Program Files:

- Client root: `BimeronCheckpointClientLabBoundary`.
- Host root: `BimeronCheckpointObservationBoundary`.
- Exact Scheduler task: `\BimeronLab\CheckpointObservationBoundary`.

No caller-configurable path, alternate suffix or fallback is permitted. The parent
adopts this routine reversible lab layout choice under existing broad user
authority, without a new permission request. This specification does not execute
or approve an unreviewed candidate. Inventory old evidence read-only; preserve
ALL old roots, partial files, bundles, pins, sessions, handles and references
byte-for-byte. Do not archive, move, rename, delete, repair or dispose them. The
native parent path for this attempt must contain no archive calls and must not
modify old guest inputs. Source provenance comes from independently approved
local frozen inputs, never from rewriting old guest state.

Acceptance before native admission:

1. Freeze and independently review the exact runner/host/support/parent and tests
   for this fixed layout, including both pending corrections. Every source change
   requires a fresh runner hash and host import/config/invocation pins; freeze
   support before embedding its digest in the host, then freeze host, parent,
   exact task XML and immutable config/manifest bindings. Keep all older pins
   historical and untouched. Require focused PS5.1/7 role/fail-fast/host/parent
   checks and DryRun; inject failures under Continue and prove no later mutation,
   archive, dispatch or release of retained references.
2. Prove both new roots and the exact task freshly absent; uncertainty or any
   existing object refuses. Stage only the newly reviewed frozen runner, host,
   support, unchanged checkpoint MSI and immutable manifest with protected
   CREATE_NEW semantics and strict hash/identity/descriptor pins. No reuse,
   overwrite, ACL repair or retry, including after partial preparation. MSI/DLL
   bytes and protected ACK state paths stay unchanged; require fresh Pending and
   checkpoint absence and UNKNOWN product state under the existing gates.
3. Preserve every prior role and fail-fast gate: TraversalOnly above exact OS
   anchors, TrustedOS safe descriptors at exact anchors, OSContent only for OS
   descendants, and LabProtected for the explicitly owned trees. Retain independent
   DOS/GUID known-folder identity, alias/mapping, single-link lab, descriptor and
   retained-handle checks. No permissive OS-anchor or lab descriptor fallback.
4. Admit at most one bounded native attempt of the same reviewed zero-resource
   checkpoint experiment, with no additional resource or network installation.
   Require the persistent same-process/same-runspace host, independently verified
   bound Ready, one exact bound dispatch and CREATE_NEW invocation intent before
   direct invocation. Keep the process-only action policy and all prior identity,
   task/config, stream capture, commitment and retention gates. Parent observation
   is bounded to 300 seconds; timeout, return or ambiguity proves neither server
   completion nor quiescence. Preserve all references and evidence on every outcome;
   no host stop, retry, cleanup or recovery is introduced.

This addendum changes documentation only: no code, VM, credentials, Task/MSI
operation, commit or push. Native survival and protected admission remain unproved.
Product behavior and final MSI/DEB artifacts are unchanged; the four-target product
build/package matrix and benchmarks/profiling are inapplicable to this spec-only
layout choice. Fresh exact-source review remains the next gate.


Boundary-v1 integrated offline candidate (2026-11-01): fixed new client/host/task
names and experiment binding are implemented. Parent candidate and test adapters
are in ignored `build/boundary-v1-review`. Guest source selection and archive
functions/calls are removed. Only local frozen inputs are staged, with pinned
MSI/DLL/manifest and protected atomic CREATE_NEW file/directory creation; old roots
are inventoried read-only. The existing new-dispatch pending-file publication
remains atomic; it never moves old evidence. Protected ACK paths stay unchanged.
Constructor/descriptor wrappers retain their original inner failures. The parent
receives failed jobs before transport fallback, writes bounded allowlisted durable
diagnostics, and keeps phase/context/HResult and sanitized original inner message.
PS5.1/7 each pass checkpoint 159+41+21, host 339 (334 preserved, two mutants),
parent offline/adapters/fail-fast and DryRun. Actual preparation/chain body faults
under Continue/strict Off prove first-error termination and retained prior owners.
Binary staging and existing-directory refusal use actual owned temp fixtures.
See that candidate's PROVENANCE.json, SHA256SUMS.txt, COMMANDS.md and REVIEW.md.
Independent exact-source/security review and guest task/config readback remain
required; no native positive, VM, credentials, Task/MSI or real runner operation,
commit, push or release is claimed. Session 86808 and earlier references/bundles
remain untouched. Supported packaging is unchanged; the product four-target
build/package matrix and performance profiling are inapplicable to this lab-only
Windows script change.
