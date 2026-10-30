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
impersonation. Only the compiled failure point returns installation failure. Lab
uninstall targets only the chosen registered lab product and runs no extra actions
in an unsuitable context. Read-only table and PE inspection must check exact action
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
