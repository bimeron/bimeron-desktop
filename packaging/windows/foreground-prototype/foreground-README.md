# Bounded elevated-client and settlement prototype

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
`C:/Users/Public/Documents/BimeronInstallerLab` tree after resolving paths and
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

Offline only, based on Desktop 2183b74 and Core 66d84d8f / 4366a5c9.
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

Remaining gates: independently established initiating MSI client SID/session/logon
and effective elevated callback token; protected native-view key/ancestor access
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
