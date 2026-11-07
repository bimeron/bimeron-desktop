# Proposed protected installation and first GUI IKEv2 contract

## Status and verified scope

**Step 4 decision proposal:** see [consumer Windows lifecycle](#step-4-decision-proposal-consumer-windows-lifecycle).
It replaces the recommended research/next-work direction, not accepted security
requirements. Steps 2/3 below remain traceable alternative designs; do not implement
their disconnected codecs next or treat the managed-host proposal as accepted.
Under conditional option B, Step 3 authority/WAL/wire interfaces are alternative-only,
not mandatory dependencies; its supersession statements apply within that alternative.

**PROPOSED v1 contract — engineering decisions extended in Step 3, 2026-11-03.**
The Step 3 section below supersedes unresolved design choices in the preserved
Step 2 text. Some mechanisms are implementation-ready; action/session authority,
historical-route enforcement and surviving restoration have explicit feasibility
blockers. This is specification work, not user/security acceptance,
an implementation authorization or native enablement.
The companion Core proposal is `docs/IKEV2_INSTALLATION_CONTRACT.MD` on Core
`plan/ikev2-installation-contract`, based on local master
`6af5d72bd7e1e98461e78f437509a1994f395fab`. Desktop uses the same branch name,
based on local `plan/stage-8-ikev2` at
`943c92d77a9c51587d94db49b3c32bf5b1f2737c`.

This proposal refines the selected Core controller in [IKEv2 profiles](ikev2-profiles.md#proposed-core-installation-controller-selected-architecture-review-pending).
It supersedes the historical trusted-invocation/callback settlement route only as
a proposed architecture. Earlier accepted offline work and frozen evidence retain
their exact scope; no earlier approval transfers to this proposal.

At this Desktop base, `MainWindow::nativeUnavailableReason` still refuses native
operations; `bindNativeController` has no production caller. Session launch consumes
four `Gates` booleans whose production authority producer is absent. Transport and
session require Core revision `66d84d8f4dcfc6ada625eb11894b19feea51b7ba`, whereas
`cmake/core-release.cmake` bundles `v0.2.0-beta.1`. A reviewed final pair is required;
replacing the revision literal alone is not a solution. Prototype MSI exports fail
unconditionally and are outside production CMake. Core's clean-base findings belong
to the companion proposal; the old geodata audit is not evidence about master.

## Authority and ownership

Core owns privileged installation, durable enrollment, participant admission,
operation recovery and settlement. Desktop owns explicit consent, authenticated
status display, exact profile selection and foreground session/provisioning UI.
MSI actions execute only a bounded authorized mutation domain; they never settle.
The installation owner is separate from VPN and provisioning owners. Closing any
client transport, UI or owner is not installation settlement or cleanup evidence.

The threat boundary trusts Windows and authorized Administrators/SYSTEM and the
exact trusted OS identities already specified by the protected-path contract;
malicious privileged administrators are outside it. Ordinary callers, writable
paths, untrusted package properties, stale records and ambiguous observations are
not authority. Elevation alone does not establish approved input or operation identity.
Authenticate protected images and their ancestors, retain identities/handles and
revalidate on use. Preserve same SID/interactive session and OS peer checks for
foreground control. Privileged installation calls additionally require the reviewed
controller/action/preload authorization mechanism; a pipe peer proves a process,
not its executing DLL export, MSI session or effective package sequence.

The protected final artifact approval binds Core revision and executable digest,
GUI digest, architecture and compatible contract versions. It is a build/review
input, never user settings or downloaded self-asserted metadata. Final policy uses
the existing schema-1 installation ID and actual GUI hash. Enrollment history,
Pending and rollback data live outside replaceable/removable payload trees.
Desktop LPF 48-byte lab records and Core BIMNENR1 64-byte observer records are
distinct formats. Neither is a production settlement receipt or interchangeable
durable journal; no implicit conversion or promotion is permitted.

## State, serialization and terminality

`Unenrolled` requires trusted complete no-predecessor evidence and inventory;
missing files alone do not establish it. Native is unavailable, while existing
packet-only install/start and signed update/rollback keep their behavior.
`Settled` binds an exact generation, approved pair and terminal inventory.
`Pending` fences new conflicting participating work for an admitted operation.
`Unsafe` represents contradictory, corrupt, unverifiable or uncertain authority.
Neither Pending nor Unsafe permits new native work. Historical Unknown stays Unknown.

The proposed sequence is:

1. Independently approve and protect/pin the effective package, Binary/preload,
   allowed properties, all payloads, rollback baseline and complete mutation domain.
   Inventory installed, running, registered and cached participants. Close new
   admission under the shared serialization protocol before acquisition can become
   ambiguous; drain existing conflicting work without terminating other owners.
2. Core retains `BeginInstallationTransaction` on one owning OS thread. That thread
   begins a new outer MSI transaction; joining an existing transaction is forbidden.
   Publish bound Pending with flush and trusted readback before Install/mutation.
3. Execute only the exact synchronous approved install. Mandatory execute preflight
   and first deferred admission gates precede mutation on every effective package,
   including quiet, maintenance, repair, cached and nested old-product routes.
   Caller conditions/properties cannot skip either gate. Reject transforms, patches,
   substituted Binary/preload and unsupported mutation properties before execution.
4. End commit or rollback on the same owning thread. Verify the exact final inventory,
   complete bounded-domain closure and durable terminal publication. Release the
   guard last. Successful commit yields `Committed`; failed install with verified
   exact rollback yields `RestoredFailure`, never installation success.

Pending publication failure after Begin prohibits Install and attempts same-thread
End rollback. Partial publication, unknown acquisition/End/release, failed flush or
readback, abandoned ownership, reboot work, disabled rollback or incomplete
inventory cannot return to Unenrolled or Settled by inference. Preserve durable
exclusion/history and report recovery required. A volatile mutex cannot preserve
exclusion after process death by itself; the native durable mechanism is a blocker.

Already admitted segments and narrowly bound rollback/service restoration may
continue while Pending, including after controller death, only under independently
verified surviving authority for that operation. They cannot admit new work or
settle. Restoration must not reacquire the controller-held mutex or require Settled
before restoring the service. Unrelated service starts remain excluded. Preserve
signed updater validation and exact rollback guarantees.

Terminal closure means no admitted nested, rollback, service or late continuation
can mutate afterward. End completion covers only its proven bounded domain; no
detached workers, reboot replacement, chainers or out-of-domain mutations qualify.
Callback success, Install return, process/pipe absence, quiet time, model Clean and
an inventory snapshot are individually insufficient. Death never supplies End or
closure evidence. Recovery is explicit and separately reviewed: reconcile protected
history, exact outstanding domain and restored/committed inventory before any
terminal publication. Never delete a record to unlock. Failed first enrollment
requires durable retirement/history before a trusted Unenrolled result is possible.

## Fixed semantic interfaces for subsequent implementation

These names are design-only contracts shared with Core, not released commands or
wire schemas. The security mechanism and encoding must be specified and reviewed
before production adapters are connected. Unknown versions, enums, fields and
malformed/oversized data fail closed; diagnostics use bounded reason codes, no
credentials, profiles or arbitrary elevated text.

- `InstallationBinding { installationId, generation, pairIdentity, operationId,
  planDigest }`: exact equality across request, action, continuation and outcome;
  fresh nonreused generation/operation IDs; approved plan digest covers canonical
  inputs. IDs and hashes are bindings, not bearer authority.
- `ApprovedInstallationPlan { binding, action, baseline, targetInventory,
  packageSet, mutationDomain, serviceBaseline }`: protected approved reference,
  no arbitrary executable, path, command or mutation properties. First enrollment
  uses a verified no-predecessor baseline; later operations bind prior Settled state.
- `InstallationStatus { availability: Ready|Unavailable|Unsafe,
  phase: Unenrolled|Settled|Pending|Unsafe, binding, reason, outcome }`, with outcome
  `InProgress|Committed|RestoredFailure|RecoveryRequired`. Ready requires verified
  matching Settled state and compatibility, but is never a reusable launch grant.
  Known Pending is Unavailable; unverifiable state is Unsafe. Unenrolled cannot
  report native Ready. A restored outcome is interpreted against its actual baseline.
- `GetInstallationStatus(expectedBinding)` and `GetInstallationOperation(binding)`
  are authenticated observations. Repeated queries cannot mutate, recover or settle.
  `ExecuteInstallationPlan(protectedPlanReference)` admits one exact operation;
  duplicate IDs only return its recorded result, never execute again; different
  data under the same ID refuses. Ambiguous
  transport results require operation lookup, not retry with a new ID.
- `CloseInstallationOwner(binding)` closes only the caller's operation-owner
  lifecycle after verified terminal conditions; it cannot settle, delete records,
  kill continuation or substitute for VPN `close_owner`.
- Core-internal `AcquireParticipatingAdmission(role,binding)`,
  `AuthorizeInstallationAction(actionEvidence,binding)`,
  `ContinueBoundRestoration(continuationEvidence,binding)` and
  `RecoverInstallation(recoveryPlan)` remain privileged and uncallable as GUI
  setters. There is no public Settle/Clean endpoint.

Proposed finite reason vocabulary shared with Core: `ready`, `notEnrolled`,
`operationPending`, `unsupportedContract`, `invalidRequest`, `bindingMismatch`,
`pairMismatch`, `unsafeEvidence`, `incompatibleHost`, `packageRejected`,
`actionUnproven`, `preloadUnproven`, `ownerBusy`, `acquisitionUncertain`,
`durabilityFailed`, `terminalUnproven`, `recoveryRequired`. Encoding and applicability
rules remain part of the blocked paired adapter specification, not implementation
discretion. Unknown reasons fail closed.

Core must atomically acquire/recheck participant admission at actual native launch
and provisioning mutation, serializing with enrollment/modification. GUI status and
the existing four booleans are only UX preconditions. They cannot close the race
between a status read and launch. Production integration must not manufacture true
booleans from capabilities, model state, missing IPC or package success.

Foreground behavior remains read-only authenticated `hello`, then exact
`accept_revision` activation against the pinned config digest. Preserve instance,
session and installation binding; `status`/`events`, `stop` and `close_owner` remain
distinct operations. Provisioning uses its separate owner, preview/state_token and
exact plan_token confirmation with Core rechecks and ownership/rollback. Start does
not create credentials or trust roots or silently repair a Windows profile.

## Smallest first-connection scope and dependencies

The proposed first supported target is Windows amd64 on a freshly inventoried host,
one reviewed Core/Desktop pair, explicit enrollment, one explicit profile preview/
apply/confirmation, one foreground GUI connect, stop and verified cleanup/closure.
ARM64 support must not be implied by x64 success. A clean-host restriction reduces
the migration success cases; it does not exempt historical package bypass controls.

1. **Resolve security contracts before production code.** Core owns a concrete
   durable admission/history protocol, action/preload/session proof, surviving
   restoration and enforceable domain closure. Desktop supplies effective MSI
   sequence/input inventory and lifecycle requirements. Determine how an old
   ungated MSI or cached repair/uninstall is prevented from conflicting after
   enrollment. Merely refusing it in the new controller cannot intercept direct
   old execution. If enforceable prevention is unavailable, first GUI stays blocked.
2. **Implement Core authority with enablement off.** New `internal/installation/*`
   and `cmd/bimeron/installation_windows.go`; integrate existing `internal/windowsnet/`,
   `internal/nativeenrollment/`, control foreground/profile authentication and
   service/update entry paths named by the Core companion. Deliver writer/readers,
   controller, participant gates and bound restoration together; do not ship a
   writer whose possible consumers still ignore Pending.
3. **Implement package participation with enablement off.** Desktop
   `packaging/windows/options.wxs`, `CMakeLists.txt` and separately reviewed new
   production action source must enforce both gates and bounded sequences. Do not
   turn the lab's always-failing exports into production actions by a flag. Preserve
   unenrolled packet behavior. Reject unsupported legacy/maintenance/removal routes
   before mutation; test prevention independently of GUI routing.
4. **Bind artifacts and Desktop adapters.** `cmake/core-release.cmake`, artifact
   approval generation in CMake, `src/NativeIKEv2Transport.{h,cpp}` and a proposed
   narrow `src/NativeInstallationClient.{h,cpp}` consume the reviewed pair/semantic
   contract. Keep existing protected image/peer checks. `CoreUpdater.{h,cpp}` must
   preserve signed update and bound restoration on enrolled hosts. Pair publication
   and supported build documentation change together in a later authorized stage.
5. **Connect GUI lifecycle after authority evidence.** `NativeIKEv2Session.{h,cpp}`
   consumes authoritative status with Core-side atomic admission;
   `MainWindow.{h,cpp}` constructs/binds the controller and connects explicit
   Connect/Stop and provisioning actions. Keep unavailable/unknown states explicit,
   cancel pending activation on Stop, and retain cleanup/closure before exit rules.
   Use the existing profile validation and `CoreClient` contracts, not CLI fallback.
6. **Enable only the proved scope.** Review exact artifacts and complete the
   separately authorized Windows evidence below before first supported GUI use.
   This plan does not itself authorize these steps or enablement.

After contract acceptance, Core authority work and Desktop typed adapter/UI fixture
work can proceed in parallel against the fixed semantics with production disabled.
Package gates depend on the accepted action/preload mechanism; native binding and
enablement depend on the integrated authority evidence. Interface changes amend
both specifications before dependent implementation continues.

Mandatory for the first connection: crash-safe exclusion, failure rollback and
service restoration, rejection of unsupported old/cached routes, durable closure,
approved pair, actual launch admission, authenticated provisioning and session
lifecycle. Later work may add successful legacy migration, supported enrolled
upgrade/repair/removal and automatic recovery only after their own contracts and
evidence. Refusal protections for those routes cannot be deferred. PR completion
must explicitly scope unsupported routes and platform claims and satisfy its
accepted criteria; a single x64 connection does not complete PR #16. Prerelease
requires the supported Windows x64/ARM64 and Linux amd64/arm64 test/build/package
matrix, package inspections and corresponding installation/runtime claims.

Non-goals: Scheduler/observation-host work, new CLI experiments, setup wrappers,
AppImage/WinGet/APT automation, automatic native reconnect/autostart, packet fallback,
credentials/CA provisioning, fabricated manifests, automatic administrative recovery,
general installer helpers and successful unenrollment in this minimal slice.

## Acceptance and evidence plan

Planning validation is limited to source/doc consistency, links, diff review and
`git diff --check`. No test/build/native execution is needed for this docs-only
change; benchmarks/profiling and the four-target product matrix are not applicable.

Future focused offline checks extend `tests/NativeIKEv2TransportTest.h`,
`tests/NativeIKEv2SessionTest.h`, `tests/ConnectionManagementTest.h` and existing
Core authority/control/service tests. Check:

- Each crash/error boundary before/after acquisition, Pending, Begin/Install/End,
  inventory, terminal publication and release; no unknown state becomes Ready.
- Status-to-launch race, replayed generation/operation, wrong pair/architecture,
  stale config/preview token, wrong SID/session/peer, forged action/CAD/nonce,
  approved peer with unapproved preload, skipped/conditioned gates and altered DB.
- Direct/quiet/cached/repair/nested/old package bypass, active old hosts and pending
  reboot refuse before mutation. Positive new-package gating alone is insufficient.
- Commit differs from restored failure; restore while Pending without deadlock;
  controller death plus surviving exact continuation; late continuation after
  closure refuses. Missing/denied/corrupt records remain distinct.
- Lost replies query the exact operation; stopped preactivation never connects;
  UI cannot infer cleanup from process exit; packet-only and unsupported Linux
  behavior remain unchanged. No hidden autoconnect or native-to-packet fallback.

New native evidence, only under a subsequent explicit authorization, must bind exact
source/artifact hashes, OS/architecture and effective MSI databases. Demonstrate
protected storage ACL/path identity and crash durability, competing actual hosts,
action/preload provenance, old-route prevention, rollback/service restoration and
bounded terminal closure. Then exercise protected GUI hello/activation, preview/
confirm, RAS connection, DNS/WFP behavior, Stop, cleanup, close, lost IPC and failures.
Use the actual production sequence; lab exports/models are not substitute producers.
Windows ARM64 runtime requires its own evidence before claiming support. Run relevant
CI-equivalent checks after implementation; perform the complete matrix before release.

Reuse historical evidence only within exact hashes and scope: source/API review,
Desktop CI 8/8, transport/session fixtures, controller/enrollment finite models,
protected ACK/context observations. None proves production enrollment, authenticated
actions, crash closure or a working GUI connection. Retain frozen bundles, VM roots,
tasks, handles and all Unknown results unchanged. No arbitrary repeat reviews:
review the reconciled specification once, then concrete changed authority/artifacts;
repeat only for changed bytes/contracts or unresolved findings.

## Decisions still blocking acceptance

The architecture selects Core ownership; it does not select a proven native security
mechanism. Before enabling any GUI connection, independently review: (1) durable
storage/history and atomic participant acquisition, including death before Pending;
(2) authenticated effective-package/action/preload/session association; (3) enforceable
prevention of ungated historical routes on an enrolled host; (4) bounded-domain
closure and surviving restoration; (5) versioned wire/plan encoding and final
artifact approval production. A restriction to new hosts or a procedural promise
does not resolve (2)-(4). If those cannot be implemented within this boundary,
return for a revised product/security specification; do not weaken the gates.

## Step 2 review record

At the Desktop baseline above, a bounded independent read-only review of this
proposal and its two documentation cross-links found no contradictions or omitted
gates within the requested scope. It did not verify the Core companion or native
feasibility and is not acceptance. Local link/anchor and whitespace checks passed.
The two planners also read the paired proposals and reconciled their semantic
names, states, outcomes and reason vocabulary; this is consistency review only.
No implementation, tests, build, package/native/VM execution, benchmarks, profiling,
commit, push or release was performed for this planning change. The unresolved
security mechanisms above remain the next-stage design gate.

## Step 3 concrete decisions and remaining feasibility boundaries

This section and the paired Core Step 3 section select engineering mechanisms,
not deployment permission. Earlier Step 2 statements that encoding/storage are
unspecified are superseded here. Gates 1 and 5 have selected implementable designs
subject to tests and native validation; gates 2, 3 and 4 have selected fail-closed
interfaces and exact blockers, not a claim that Windows supplies their missing
authority. The complete production installation adapter is therefore **not yet
implementation-ready**. The disconnected codec/history/admission implementation
slice below is ready to scope next. No new released CLI/API is created by this text.

### Gate 1: durable authority, history and atomic participating admission

Use native `FOLDERID_ProgramData/Bimeron/InstallationAuthority/v1`, independent of
Desktop/Core payload replacement and uninstall. `anchor.json` and `history.wal`
are a new format, neither LPF48 nor BIMNENR1. Bootstrap is a privileged, separately
approved operation which inventories the host, establishes the platform profile
below and creates the anchor once. A missing anchor is not a never-enrolled proof.
Missing/inconsistent authority makes native status Unsafe; legacy packet operation
outside an established authority profile retains its existing behavior.

Core owns creation with an explicit protected SYSTEM/Administrators full-control
DACL and trusted owner at creation, never create-then-repair. Ordinary users have
no authority-store access; authenticated Core provides observations. Pin the native
known-folder identity and each descendant handle, reject reparse/case-sensitive/
unqueryable or untrusted writable ancestors, require local NTFS and a single-link
file, deny write/delete sharing while reading/writing. Recheck volume/file identity
and descriptors on use. Keep WAL handles only during serialized I/O; close them
before releasing the admission mutex so another trusted writer can append. No
exclusive lifetime WAL handle. No configurable store root or user-provided absolute path.
Existing protected-path code is a starting point, not evidence for this new root.

Select an append-only WAL with a hard 64 MiB bound, no compaction/reset/pruning.
Each frame is `uint32LE(payloadBytes) || payload || digest`, payload at most 65536
bytes, digest `SHA256(previousDigest || lengthLE || payload)`; the first previous
digest is SHA-256 of exact anchor bytes. Payload is canonical UTF-8 JSON, version
1, with monotonic uint64 decimal-string sequence, operation binding and a finite
record kind. The Core companion fixes record schemas, including a maximum depth
of 8, an anchor maximum of 4096 bytes, at most 32 outstanding leases and 1 MiB
reserved for terminal/closure records. No wrap. Torn tails, unknown
records, bad hashes, missing sequences or extra bytes make the whole authority
Unsafe; do not truncate to the last apparently good record. Reserve terminal-record
capacity before admitting work. Full history refuses new work, never deletes history.

Use retained-file append, `FlushFileBuffers`, then same-handle exact readback before
any dependent effect. Publish neither through rename-overwrite nor a mutable Clean
flag. A terminal transition is one validated complete frame, not two independent
state files. A hash chain detects corruption, not privileged replacement with a
valid old prefix; malicious administrators and offline disk rollback are outside
this threat boundary. Restored machine/disk snapshots require explicit recovery,
never replay-as-fresh. Power-loss/metadata durability still needs native evidence.

All participating admission decisions use protected machine mutex
`Global\BimeronCoreInstallationAdmissionV1`. Core validates owner/DACL and rejects
abandoned/unknown acquisition. Lock order is admission serialization, durable lease
or Intent publication, release admission serialization, then zero-timeout existing
connect lock on its owning OS thread. **Never wait for connect while holding the
admission mutex.** Reacquiring admission briefly while holding connect is allowed
only because no holder waits for connect; no callbacks/waits occur under admission.

A participant appends and flushes its lease before launch or mutation, then takes
the connect lock and holds it for its full admitted lifetime. A live, proven
no-effect connect-acquisition refusal may close its lease; ambiguous acquisition
cannot. Installer Intent atomically fences new leases, then drains existing ones
by bounded status outside the mutex, without killing owners. It takes the existing
`BeginInstallationTransaction` guard only after all relevant leases close. Intent
is flushed **before** guard acquisition or `MsiBeginTransaction`; Pending is flushed
after Begin and before Install. Intent also persists the OS-observed controller
`owner:{pid,created,sid,session,imageDigest}`, using the LeaseOpen identity schema
in Core. Revalidate retained process identity/creation time, SID/session and pinned
image plus active owner control; matching PID alone is insufficient. A restarted
controller is not that owner. Missing/dead/unqueryable ownership makes status Unsafe
but never proves cleanup or permits fence removal. Ready observation never reserves a lease.

Death before Intent permits no installation effect; death during Intent publication
leaves either no authorized effect or unreadable/unfinished history which refuses.
Death after durable Intent but before Pending leaves the fence. Death after lease
publication but before launch leaves a lease requiring recovery; a PID disappearing
does not close it. Intent maps to external Pending/InProgress; abandoned or corrupt
intent/lease maps to Unsafe/RecoveryRequired. No lease expiration or timeout unlock.
After Pending, only already admitted operation segments and exact bound rollback
may run; new admission remains closed. `Terminal` (or `Retire`) is only a prepared
candidate, still externally Pending. After verified closure, hold the admission
mutex across checked connect release, then append/flush/readback `ReleaseComplete`
with `{terminalSequence}` naming that candidate; release admission mutex LAST.
Only ReleaseComplete makes a terminal candidate effective. Death/error before it
leaves a durable fence even if another process consumes mutex abandonment. This
refines Step 2's release-last rule: admission serialization is released last, not
the connect mutex. No uncertain connect release can publish ReleaseComplete.
Participant completion similarly verifies closure, acquires admission, performs
checked connect release, appends/flushes LeaseClose, then releases admission last.
A never-launched, proven never-acquired lease can close without connect release.
Current Core release closures discard native release errors; S2 must introduce
checked-release results rather than treating a void callback as proof.

### Gate 2: effective package and action authority

Select detached signed exact artifact approval, protected immutable staging and a
complete effective MSI database projection. The approval must cover final MSI
bytes, ProductCode/PackageCode/UpgradeCode, every persistent table/column/row and
Binary/cabinet stream, ordered execute sequence, exact action DLL and dependencies,
OS/architecture profile and exact Core/Desktop bytes. The approved projection
defines an explicit list of engine-temporary metadata exceptions; the zero-resource
ACK fixture's exception list is not automatically valid for a product MSI.

The controller chooses the fixed staged MSI and a fixed property dictionary; no
caller path, source URL, transform, patch, external cabinet, embedded UI, chainer,
arbitrary command/property or mutable package source. Approval validation and
protected staging precede Intent. Execute-sequence preflight verifies the actual
database before mutation; a first deferred gate must bind the same operation before
the mutation script. Both gates are mandatory for all supported sequences. Static
conditions, UI choices, maintenance mode or a property cannot skip either.

Do not turn `ForegroundPrototype` into an authorizer: its existing export always
returns failure. `lab-protected-ack.cpp::database` demonstrates immediate database
projection only. An authenticated named-pipe process, public CAD locator, ProductCode,
module filename/hash or successful callback is not proof of the executing export
and effective MSI session. Deferred actions have a restricted handle and can run
after the original session no longer exists; there is no documented deferred
`MsiGetActiveDatabase` route in the supported context API list. An immediate receipt
cannot bridge that trust gap merely by being signed or copied into CAD.

`AuthorizeInstallationAction(actionEvidence,binding)` consequently has no production
success implementation selected. Its required evidence is the exact effective
package digest, loaded DLL/dependency identity, approved action/sequence position,
current operation/transaction association and one-use admission under Core history.
Unproved fields return `actionUnproven` or `preloadUnproven` before mutation. This is
an explicit **platform feasibility blocker**, not a request to implement a permissive
stub. Pinning a DLL file does not prove which dependency was already loaded; Windows
DLL search order includes loaded modules and dependency-name searches.

Bounded alternatives: (A) prove that an enforced managed code-integrity/installer
profile restricts the entire MSI execution environment sufficiently to make a
reviewed action gate trustworthy, including session association; or (B) replace
enrolled MSI mutation with a separate Core-owned transactional writer and make MSI
nonmutating for that domain. B changes installer architecture/rollback ownership and
needs a new accepted scope; it is not a setup-wrapper shortcut. Neither alternative
is accepted or implemented here. Recommend investigating A first together with gate 3;
if it fails, retain GUI disablement rather than replace proof with a callback nonce.

### Gate 3: historical and cached route prevention

Concrete counterexample: after enrollment, an old direct `msiexec /i`, `/f` or `/x`
can execute its own ungated sequence and replace/remove payload while a new native
owner holds the new mutex. The old package does not read this WAL. A fresh initial
inventory, changed UpgradeCode, new-package LaunchCondition, or checking Pending in
the new controller cannot intercept it. Old Core helpers/updaters and already
running old processes are part of the same threat, not only MSI files.

Within the present unmanaged desktop scope there is no selected enforceable hook
covering these routes. Keep enrollment/GUI disabled. The recommended bounded
product alternative is a **managed Windows amd64 host profile established before
enrollment**, with default-deny executable/DLL and MSI/MSP/MST policy, exact approved
package/code allowlisting, no wildcard signed-package/admin/cache exceptions, no
uncontrolled already-running predecessor, and policy identity/digest in the plan.
AppLocker default rules explicitly allow administrators, signed installers and the
Windows Installer cache; enabling those defaults does not satisfy this requirement.
An App Control/UMCI profile with script enforcement is a candidate for evaluation,
not a claimed tested cached-repair or service-host enforcement mechanism.

This is a material product/security scope change: host policy affects software
beyond Bimeron, needs an administrative deployment/support owner, OS-version coverage
and a policy lifecycle. No policy is installed or silently changed by Desktop.
Core must refuse admission if the accepted effective policy is absent, in audit
mode, incompatible, modified or unqueryable. Readback of a policy hash alone cannot
prove enforcement. Policy must remain enforced throughout enrollment, including
rollback and offline/reboot recovery; snapshot checking cannot close a TOCTOU gap
if untrusted code can disable it. Authorized administrators and Windows remain
trusted; ordinary users cannot change policy or create an alternative execution path.

User/security choices: adopt the managed-host prerequisite subject to proof
(recommended research direction), retain unmanaged scope with native GUI unavailable
(safe default until a decision), or authorize a separate architecture redesign.
There is no option here to defer old-route blocking until after first connection.

### Gate 4: closed domain, restoration and recovery

Choose a finite resource manifest, bound into `mutationDomainDigest`: exact payload
files and hashes, registry key/value names/types/bytes, MSI product/component/cache
identities, schema-1 enrollment policy, and service executable/configuration/start
state. Each resource identifies baseline, target and rollback owner; unknown extra
rows/resources refuse. The WAL, anchor, protected backups and approval staging are
outside package-owned/removable trees. No async action, detached process, reboot
replacement, task, custom-action network operation, arbitrary service command,
unlisted nested package or out-of-domain registry/file write is allowed. Existing
`LaunchBimeron` uses `Return="asyncNoWait"`; a future enrolled sequence must suppress
that launch and require explicit launch after settlement. Current packet MSI remains
unchanged. Enrollment does not provision credentials, CA roots or an RAS profile.

Same owning OS thread orders guard -> Begin -> durable Pending -> exact Install ->
End(commit or rollback) -> resource and continuation closure verification -> durable
prepared Terminal -> checked connect release under admission -> durable
ReleaseComplete -> admission release last. Distinguish End access denied, install failure,
already running and rollback disabled; no success coercion. On failure before Install
after Begin, attempt same-thread rollback but preserve the fence on any ambiguity.
End commit success destroys MSI rollback capability; a later inventory/publication
failure is RecoveryRequired, not an automatic request for an impossible MSI rollback.

Standard MSI ownerless rollback may survive controller death, but the documented
rollback trigger does not attest completion or restore arbitrary Core service work.
Custom service changes need separately authored rollback before forward mutation,
exact protected backups and an authenticated surviving executor. Existing updater
mutex and signed `-source` path are not that executor. Bound restoration cannot wait
for Settled or reacquire its controller's connect mutex. New ordinary service start
is excluded; only exact operation-bound restoration may proceed under retained
Pending authority, and it cannot settle or grant new work.

The concrete recovery policy is **fail-safe retention**, not automatic repair:
retain all leases, rollback data and Pending/Unsafe state after death, End uncertainty,
unproved service restoration or missing closure. No public Recover/Settle/Clean API.
An internal recovery plan must bind the same history, interrupted domain and exact
approved recovery code, establish that no earlier continuation can mutate again,
restore or verify inventory, and durably append the decision. Inventory or reboot
alone does not establish that premise. If terminal closure cannot be proved,
recovery refuses and native remains disabled. Successful first-enrollment rollback
prepares Retire directly only when the live controller verifies the original
unenrolled baseline and complete closure, followed by checked release and
ReleaseComplete. Separately accepted recovery may also prepare Retire under those
requirements. Terminal/RestoredFailure is restricted to an actual prior Settled
baseline; it cannot activate a nonexistent first-enrollment baseline. Retirement
never deletes the installation ID or resets history.

Surviving restoration/closure is still a feasibility blocker. A narrow independently
resident Core broker with operation-specific continuation slots is a bounded possible
solution, but adds a privileged lifecycle and needs explicit design/security acceptance
and native evidence. No new service, Scheduler dependency or pretend broker guarantee
is introduced by this stage. First GUI cannot ship while this blocker remains.

### Gate 5: bounded paired plan, wire and artifact producer

Use strict RFC 8785 JCS UTF-8 JSON for protected plans and history payloads: no BOM,
duplicate/unknown keys, floats or invalid UTF-8; uint64 values are canonical decimal
strings without leading zeros and never wrap. Hash exact canonical bytes, never a
parser's reserialization of untrusted input. Binding is exactly `installationId`
(32 nonzero lowercase hex), `generation` (nonzero uint64 decimal string), `pairIdentity`
(64 lowercase SHA-256 hex), `operationId` (32 nonzero lowercase hex), `planDigest` (64 hex).

Plan maximum is 262144 bytes. Fields: `schema:1`, `binding` excluding `planDigest`
(avoid a self-hash), `action` (`enroll|update|repair|remove|recover`),
`baselineDigest`, `targetInventoryDigest`, `packageSetDigest`, `mutationDomainDigest`,
`serviceBaselineDigest`, `policyProfileDigest`; all digest fields are SHA-256 hex.
`planDigest` is SHA-256 of the entire exact canonical plan. Action enum membership
does not enable it: first scope accepts enroll only after all gates; update/repair/
remove/recover refuse until separately approved. Referenced immutable manifests
are individually bounded to 262144 bytes, at most 16 unique objects and 4194304
aggregate bytes with no recursive references; typed schemas must be accepted before
any action uses them. Unknown resource/package/service schemas refuse. Hashes alone
do not approve opaque objects. Never fetch from request-controlled paths.

New installation pipe is
`\\.\pipe\Bimeron.Core.Installation.<sidHash>.<session>`, where sidHash is lowercase
hex of the first 16 bytes of SHA-256 of the UTF-8 SID and session is decimal Windows
session ID. This is separate from foreground/provisioning pipes. Require explicit
pipe ACL, local-only transport, first-instance protection, identification-only SQOS,
exact same SID/logon/session and retained peer PID/creation time/protected image
identity/hash matching the approved pair. Do not grant a client pipe-instance creation
rights via generic-write ACLs. Authenticate OS peer before any request bytes and
recheck on reconnect; matching JSON claims alone never authenticate a peer.

Wire is one UTF-8 JSON object followed by LF, at most 65536 bytes including LF in
either direction; maximum eight simultaneous connections; 5-second bounded I/O,
maximum nesting depth 8, one request/response per connection. Wire JSON need not be
canonical; protected/signed objects must be canonical.
Timeout cancels the exchange, not the operation or authority. Request ID is 1..64
ASCII `[A-Za-z0-9_-]`. Envelope is `{version:1,requestId,command,binding}`; commands
are `getStatus|getOperation|executePlan|closeOwner`. Only getStatus accepts null
binding for initial discovery. executePlan additionally carries
`planReference:planDigest`, a fixed protected lookup key, never an arbitrary path.
Response is `{version:1,requestId,command,status:{availability,phase,binding,reason,outcome}}`
using the Step 2 finite enums. A null binding is allowed only for unbound Unavailable
or Unsafe observations, never Ready. Unknown fields/enums/versions, duplicate keys,
multiple frames or oversized/incomplete messages fail closed with no execution.

Core checks full binding equality at execution; duplicate operationId plus same
planDigest returns the recorded result, different bytes refuse. A lost reply is
resolved with getOperation for the original binding, never a new operation ID.
closeOwner is lifecycle bookkeeping only after verified terminal conditions; it
cannot release an unfinished fence. Known Pending gives operationPending/InProgress;
corrupt authority gives unsafeEvidence/RecoveryRequired. Unknown wire versions or
invalid envelopes close the connection without a status response; unsupportedContract
is only for a valid v1 request encountering an incompatible installed store/plan/pair
contract. State/reason combinations are fixed in the Core companion;
Desktop must not turn any unknown combination into Ready. This new protocol does
not change the current newline-framed foreground protocol or its tests by implication.

Select a detached Ed25519 installation-pair approval, separate from the existing
signed Core update manifest and verified by a dedicated compiled public trust key.
No private key is committed. A future producer
`scripts/prepare-installation-approval.py` reads final frozen build inputs and emits
canonical approval plus an unsigned signing request; it cannot self-approve. A
separate release/security signing step emits a detached signature. Runtime validates
both signature and exact artifact hashes. Existing `gui_reviewed: true` or a new
Core version string is insufficient authority for native installation.

Producer order avoids circular hashes: compile trust-root-enabled Core and Desktop;
freeze final executables/DLLs; build and inspect the exact MSI; compute final pair,
package and evidence manifests; independently review then sign the detached approval.
The detached approval is not embedded into the MSI whose hash it covers. Its protected
import is a bootstrap input, not a fourth supported package format or network setup
wrapper. Never rebuild/change payload after approval; changed bytes require new
approval. Current production CMake/pins/export failure and GUI disablement stay intact.

Pair approval binds schema=1, platform=windows, architecture=amd64, both full source
SHAs plus dirty-source patch hashes if a nonrelease candidate, exact GUI/Core/DLL/MSI
SHA-256, contract/store/plan versions, package projection, resource domain, policy
profile and evidence-index digest. `pairIdentity=SHA256(exact approval payload)`;
signature is outside that payload. Plans reference pairIdentity; approval does not
include planDigest, so there is no cycle. No candidate approval is a release approval.
Evidence index records producer/tool versions, commands, OS build/architecture,
source/artifact hashes and pass/fail/not-run for every acceptance case. Missing
required native evidence is not a passing field. The producer rejects a mixed pair,
unsupported architecture, absent evidence or stale projection; unsigned draft output
cannot authorize bootstrap. Nothing is generated/signed in this documentation stage.

The closed approval-payload field names are `schema`, `purpose`, `platform`,
`architecture`, `keyId`, `coreRevision`, `desktopRevision`, `corePatchDigest`,
`desktopPatchDigest`, `coreDigest`, `desktopDigest`, `msiDigest`, `dependenciesDigest`,
`packageProjectionDigest`, `mutationDomainDigest`, `policyProfileDigest`,
`osProfileDigest`, `evidenceIndexDigest`, `storeVersion`, `planVersion`, `wireVersion`.
All versions are integer 1; platform/architecture are literal windows/amd64;
purpose is `candidate|production`. Revisions are 40 lowercase hex; patches are null
for clean source or SHA-256 hex for candidate-only dirty source. All other Digest
fields are SHA-256 hex. keyId is 1..64 ASCII `[A-Za-z0-9_-]` selecting a compiled
32-byte Ed25519 public key, never a key supplied by the payload. Maximum canonical
payload is 65536 bytes, depth 8. Unknown keys or duplicate keys refuse.
Signature is exactly 64 raw bytes over ASCII `BIMERON-INSTALLATION-PAIR-V1` followed
by a NUL byte and exact canonical payload bytes. pairIdentity hashes payload only.
Production admission refuses purpose=candidate or non-null patch digests; candidate
approval can be examined in separately authorized offline fixtures only.
Referenced manifests obey the 16-object/4 MiB object budget above. Dependency
manifest entries identify final file bytes; they do not recursively authorize code.
Resource/projection/policy/evidence object schemas remain gated on their concrete
platform adapter design; the S1 codec treats their digests as references, not proof
that executable admission is available. No production signer may approve unresolved
required evidence. This freezes the outer producer contract without pretending that
the blocked platform evidence producers already exist.

### Dependency-ordered implementation slices and acceptance

S1 (recommended next, Astra/codex_petr low, **separate authorization required**):
disconnected Core `internal/installation/{contract,codec,replay}.go` and
focused tests; Desktop `tests/NativeInstallationContractTest.h` plus proposed
`src/NativeInstallationProtocol.{h,cpp}` pure parser, no transport/GUI registration.
Share exact positive/negative fixture bytes. Test malformed/duplicate/unknown fields,
all integer/length bounds, self-hash avoidance, replay, torn frames, crash before
Intent/Pending and lease-before-launch, lock inversion and no-effect versus ambiguous
acquisition. Add controller identity to Intent round trips, PID-reuse/owner loss,
live first-enrollment Retire with ReleaseComplete, rejected nonexistent Settled
rollback, and unknown-wire-version versus valid-v1 unsupportedContract fixtures.
Inject storage/locks/clock; no native authority writes, MSI or enablement.
Core codec/history and Desktop parser fixtures can run independently in parallel.

S2 depends on S1 and an accepted bootstrap/storage contract: Core
`internal/installation/{store_windows,admission_windows,bootstrap_windows}.go`.
Controller and existing `internal/windowsnet/network_windows.go`/control/service
production integration additionally depend on gates 2/3/4 acceptance (S3), not merely
storage tests. Test actual protected ACL/path identity, competing processes,
flush/readback failures, crash/power-loss and abandonment; every participant must
fence before a writer is integrated. Implement with production enablement off.
Native testing is a separate explicitly scoped authorization, not part of S1.

S3 depends on material policy decision and resolved gate-2/gate-4 feasibility:
Desktop proposed `packaging/windows/installation/{actions.cpp,manifest-check.py}`,
`packaging/windows/options.wxs`, `CMakeLists.txt`; Core controller/action/restoration
adapters. Exact file additions may be refined before coding, never enable the lab
exports. Negative tests must execute old/cached/direct/quiet/repair/uninstall/nested
routes under the actual enforced profile, substituted DLL/dependency, arbitrary CAD,
skipped gates, disabled rollback, crash with surviving restoration and late mutation
after closure. No policy/authority producer means this slice remains blocked.

S4 depends on S2/S3 evidence: producer above, `cmake/core-release.cmake`,
`src/NativeInstallationClient.{h,cpp}`, `src/NativeIKEv2Transport.{h,cpp}` and
`src/CoreUpdater.{h,cpp}`. Test signature/key mismatch, mixed artifacts, source vs
effective DB changes, peer/PID/session replacement, timeout/replay, approved peer
with unapproved preload and signed-update rollback while Pending. Producer/static
manifest work can proceed independently of controller work after the shared schema
is frozen; it cannot emit accepted approval before evidence exists.

S5 depends on all preceding evidence and explicit GUI scope acceptance:
`NativeIKEv2Session`, `MainWindow`, existing provisioning/connection tests. Preserve
read-only hello -> accept_revision, exact preview/state_token/plan_token confirmation,
stop/close distinctions and actual Core admission at launch. Test stale Ready raced
against Intent, Stop before activation, lost IPC, restored failure, unavailable/unsafe
display and zero packet fallback. First GUI Windows amd64 evidence does not complete
PR16 or the four-target release matrix. No release stage is authorized here.

### Bounded feasibility evidence proposal, not executed

If the managed-host alternative is accepted, use a new disposable Windows amd64
environment with an explicitly pinned OS build and frozen exact sources/artifacts.
One experiment set should test: denied old MSI from original path and cache via
install/repair/remove, transforms/patches, old helper/service routes; accepted new
sequence; loaded-dependency substitution; wrong action/session/CAD; controller death
before/after each gate and during service stop/restore; policy absent/audit/changed;
reboot/rollback-disabled and surviving late work. Preserve each history/evidence
bundle. A single escaped mutation disproves closure for that profile. No retry on
the same ambiguous state, no deletion of old evidence. Scheduler lab is unrelated.

Action attestation must produce a concrete enforceable causal binding, not merely
extra tracing; if documented APIs and the managed trust boundary cannot provide it,
report gate 2 unresolved and choose a scope redesign before native implementation.
Likewise controller death plus ownerless MSI rollback is only trigger evidence until
service restoration and terminal closure are established. This experiment is proposed,
not authorized or run; existing VM sessions/handles and Unknown outcomes are untouched.

### Source and platform justification

Read-only source checks at the recorded bases establish the gap: Desktop
`MainWindow::nativeUnavailableReason` still blocks; transport checks exact
`66d84d8f...` while `cmake/core-release.cmake` bundles beta.1; the four session Gates
lack a producer. Core `BeginInstallationTransaction` pins the OS thread and wraps
the connect mutex, but has no durable Intent/lease/terminal writer. Desktop
`foreground.cpp::ForegroundPrototype` fails; `lab-protected-ack.cpp` projects an
immediate database, not deferred session authority; `options.wxs::LaunchBimeron`
is asynchronous. These are actual code facts, not assumed runtime evidence.

Official documentation consulted read-only on 2026-11-03:

- [MsiBeginTransaction](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msibegintransactiona)
  specifies single ownership and ownerless rollback, not a durable completion receipt.
- [MsiEndTransaction](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msiendtransaction)
  defines owner-only commit/rollback and failure dispositions; commit deletes rollback state.
- [Deferred action context](https://learn.microsoft.com/en-us/windows/win32/msi/obtaining-context-information-for-deferred-execution-custom-actions)
  limits session APIs and explains why the original session can be unavailable.
- [DLL search order](https://learn.microsoft.com/en-us/windows/win32/dlls/dynamic-link-library-search-order)
  documents loaded-module and dependency resolution; a top-level DLL pin is insufficient.
- [AppLocker installer rules](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/applocker/windows-installer-rules-in-applocker)
  documents the unsafe-for-this-contract default cache/admin/signed-package allowances.
- [App Control script enforcement](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/design/script-enforcement)
  is a candidate platform mechanism, not proof of complete legacy-route coverage.
- [Rollback custom actions](https://learn.microsoft.com/en-us/windows/win32/msi/rollback-custom-actions)
  explains explicit reversal for custom mutations and interrupted-action handling.
- [FlushFileBuffers](https://learn.microsoft.com/en-us/windows/win32/api/fileapi/nf-fileapi-flushfilebuffers)
  supplies the chosen flush primitive; exact crash/storage guarantees need validation.
- [Named pipe security](https://learn.microsoft.com/en-us/windows/win32/ipc/named-pipe-security-and-access-rights)
  explains explicit DACLs, logon SIDs and pipe-instance rights.
- [RFC 8785](https://www.rfc-editor.org/rfc/rfc8785) defines canonical JSON;
  this contract further forbids floating-point payload fields.

These references justify chosen primitives and identified limits; they do not certify
the assembled Bimeron system. User/security acceptance, native validation and release
approval remain separate from this design decision record.

### Step 3 review and validation record

One independent read-only paired review (Astra/codex_petr, low) found three defects:
missing durable controller identity in Intent, ambiguous live first-enrollment
rollback retirement, and inconsistent unknown-wire-version behavior. Both planners
corrected the schemas/transitions and added corresponding negative acceptance.
The coordinator also fixed the pre-review release-publication gap with prepared
Terminal/Retire plus ReleaseComplete; reviewer found that ordering coherent.
The review recognized gates 2/3/4 as explicit feasibility blockers, not missing
claims of success. Corrections were checked locally for paired consistency; no
second independent review or native acceptance is claimed. The closed outer approval
field/signature encoding was finalized by the coordinator after that review.

Desktop whitespace and relative file/anchor checks passed, including the untracked
contract; Core whitespace and relative file links passed. Only the original six
documentation drafts are changed across the pair. No product tests, CI builds,
benchmarks, profiling, native/MSI/VM/service operations, commit, push or release ran.
Those checks are inapplicable to documentation-only changes; all four supported
package/runtime targets still need their relevant implementation/release evidence.

## Step 4 decision proposal: consumer Windows lifecycle

**PROPOSED, NOT USER/SECURITY ACCEPTED — 2026-11-03.** Recommend investigating a
consumer Windows lifecycle split: ordinary MSI payload servicing while native work
is quiescent, existing Core foreground/provisioning ownership for runtime effects,
and explicit recoverability before any destructive servicing. No mandatory managed
policy and no new resident broker by default. This is a replacement requirement
proposal, not a solution satisfying the Step 3 five-gate contract. GUI remains off.

### Requirements and authority being decided

Existing source/specification boundaries to retain: Windows and authorized
Administrators/SYSTEM are trusted; ordinary users cannot replace protected code or
authority; approved exact Core/GUI identity, authenticated same-user/session peers,
explicit provisioning consent, owned-resource cleanup and no silent packet fallback
remain required. An approved image is not proof that it is free of vulnerabilities.
Compromised ordinary clients remain untrusted. Compromise of approved elevated Core,
Windows or an authorized elevated installer crosses the trusted computing base;
the application cannot promise containment of equivalent privileged arbitrary code.
Preventing ordinary input from exploiting that code remains a security obligation.

The universal fence against every historical MSI/helper, exact deferred export/
session/preload attestation, global installation WAL and surviving MSI-bound broker
were proposed mechanisms/expanded protections, not user-accepted product promises.
They were motivated by real interference and crash risks. Removing them from the
recommended architecture does not remove those risks or silently accept weaker
network behavior. The smallest material decision is whether to replace universal
privileged-write prevention with supported-path serialization, runtime integrity
checks, fail-closed uncertainty and a recoverable offline maintenance lifecycle.

- **Ordinary user:** protected ACLs, retained object/image checks and authenticated
  narrow IPC can enforce a boundary. Include cached repair, advertised/self-heal
  and uninstall triggered by that user even when Windows Installer executes as
  SYSTEM. Execution privilege alone cannot reclassify these as malicious-admin work.
- **Consenting elevated owner/admin:** may install/update/remove approved software;
  supported Bimeron operations must serialize and preserve recovery. UAC consent
  alone does not approve arbitrary payload, paths or runtime enrollment.
- **SYSTEM installer / concurrent approved updater:** trusted executors can still
  race or crash. All supported direct, quiet, cached and nested maintenance and
  signed-update paths must quiesce or refuse before conflicting mutation, with no
  status-check-to-launch gap. No connection continuity across maintenance is promised.
- **Manual elevated tampering or forced obsolete installation:** universal prevention
  is outside the proposed consumer guarantee only if explicitly accepted. Such work
  can damage availability, binaries and recovery state; later detection cannot undo
  a transient security failure. This exclusion never excuses a standard-user route.
- **Externally managed policy:** default-deny execution/code policy may constrain
  old packages/helpers if maintained by a trusted deployment owner. It adds host-wide
  product/support scope and still needs actual route/enforcement evidence. Default
  AppLocker rules permit admin, signed and cached installers; they do not suffice.

### Concrete alternatives and recommendation

**A. Keep MSI-coupled Step 3 plus managed policy.** Retains the old proposed safety
predicate and exact artifact domain. Requires policy deployment/support, historical
route coverage, action/session trust and surviving restoration evidence. Policy alone
does not supply G2/G4. Appropriate only as a separately chosen managed product;
not recommended as the ordinary consumer prerequisite.

**B. Split package and runtime lifecycles (recommended conditional path).** MSI owns
installed package files/registration and ordinary package rollback. Core owns native
approval/enrollment, profile/RAS/DNS/WFP effects and recovery. No MSI callback grants
runtime authority or settles a network operation. Reuse the existing elevated
foreground and separate provisioning owners; do not move native RAS into the packet
service merely to obtain SCM lifecycle. The present MSI does not own that installed
service: extending SCM ownership would be a separate design change.

Supported maintenance must atomically exclude new foreground/provisioning/update
work, drain existing provisioning, request Stop, verify owned RAS termination and
DNS/WFP cleanup, close owners, then retain exclusion through payload servicing and
rollback. A fresh runtime validates the actual approved pair, schema and recovery
state; neither MSI exit 0 nor a callback receipt enrolls it. Rollback never
automatically reconnects. Standard MSI rollback handles its authored package domain,
not arbitrary Core/network state. Restart Manager and SCM Stopped are lifecycle
signals, not network cleanup or durable closure proofs. A check-and-exit helper or
new GUI-only gate is insufficient for direct/cached routes and crash recovery.

**B2. Core transactional payload writer / resident broker.** Can own a narrower
mutation/recovery domain outside MSI and survive loss of a UI, but requires new
privileged IPC, bootstrap, update, durable recovery and uninstall machinery. It
cannot constrain an equivalent privileged old MSI/helper; a service name or separate
directory is not that boundary. Consider it only if a concrete B experiment shows
that retained recovery/exclusion requires an independently resident executor.
It is not an accepted setup wrapper or a reason to build another disconnected WAL.

**C. Retain native off.** Current safe shipping position, preserving packet behavior.
Choose this if the scope decision or B's lifecycle evidence is not accepted. No
consumer architecture is demonstrated production-ready by this ADR.

### Legacy interference and recovery are first-class gates

Inventory actual supported/released prior MSI databases, component paths, cached
registrations, helpers and updater behavior, including per-user/per-machine contexts.
New package conditions cannot retrofit old cached packages. A fresh host avoids
installed legacy registrations but does not prevent an admin installing an old MSI
later. New paths/UpgradeCodes alone neither prove isolation nor authorize migration.
Unsupported conflicting installed history must block native enrollment until a safe
transition is proved; do not delete caches, patch old packages or impose policy here.
For each ordinary-user-triggerable route, prove either common maintenance exclusion
or harmless/refused behavior before conflicting writes. Any escape blocks B for
that host class; labeling it unsupported is not sufficient protection.

Current Core facts at `6af5d72b...`:

- `internal/control/foreground_windows.go::RunForeground` survives GUI/pipe closure;
  `foreground.go` permits terminal `close_owner`, which alone need not mean successful
  cleanup. `windowsnet/network_windows.go` uses lifetime ownership/marker and a
  thread-bound installation guard, not a complete durable maintenance coordinator.
- `core/ikev2_windows.go` dials RAS before arming WFP/DNS. Connected is withheld until
  protection is installed; that does not prove leak prevention during bootstrap.
  `internal/killswitch/ikev2_windows.go` uses persistent filters and retains them on
  uncertainty. Do not replace them with dynamic filters for convenient teardown:
  dynamic WFP session objects disappear when the session ends.
- `core/ikev2_protection.go` cleans RAS, then DNS, then WFP and retains protection
  on failure. `internal/systemvpn/ras_windows.go::noRASConnections` deliberately
  refuses stale sessions after crash. `cmd/bimeron/connect_windows.go` recovery
  does not reconcile RAS before DNS/WFP recovery. This is a concrete recovery gap.
- Profile PBK and ownership journals are per-user data outside the MSI payload.
  `internal/systemvpn/profile_windows.go` rechecks ownership under its mutation lock.
  Preserve unrelated profiles, saved credentials, certificates and trust roots.

Supported update/remove must retain a verified recovery executable and required
state until positive owned-resource cleanup, or refuse before deleting them. Package
rollback cannot roll back live profile/user data or stand in for network recovery.
After a crash, record/report recovery required; do not clear protection or stale
records to regain connectivity. A bounded recovery design must identify the exact
owned RAS session, avoid adopting/disconnecting unrelated sessions, reconcile DNS
ownership and preserve protection until termination is independently confirmed.
SCM restart, reboot, process absence and absence of one marker do not establish this.
If recovery cannot be safely identified, remain unavailable and preserve evidence;
an offline repair route and its retained payload must exist before supported shipping.

Persistent filters are not boot-time filters. Pre-arm traffic, reboot/BFE startup,
interface reuse and established IPv4/IPv6 flows require explicit analysis. If the
desired guarantee starts before RAS dialing, current sequencing needs redesign;
an experiment cannot prove protection that the code has not installed. No bootstrap
exception, fail-open cleanup or relaxed leak guarantee is accepted by this ADR.

### Minimal next milestone and stop criteria

Next authorization should cover **one bounded lifecycle feasibility slice**, not
G1/G5 codecs or GUI enablement. First accept/reject B's threat/scope delta and fix the
precise network protection interval. Then specify a minimal maintenance exclusion
and owned-RAS recovery change against the existing Core, with a frozen historical
package matrix and exact Windows amd64 OS/Core/Desktop candidate hashes. If any
new broker or weaker guarantee becomes necessary, return for a decision before code.

Proposed later experiment, **not executed or authorized here**: in a fresh isolated
environment, use one approved pair and controlled test endpoint/profile. Exercise
install -> explicit profile preview/apply -> authenticated foreground connection ->
Stop -> verified RAS/DNS/WFP cleanup -> offline update/rollback/remove. Kill only
explicitly owned test processes at pre-arm, post-arm, DNS and cleanup boundaries;
check restart/reboot, interrupted servicing and recovery payload availability.
Race maintenance with connect/provision and test standard-user cached repair,
advertised/self-heal, quiet uninstall and nested old removal using the actual prior
packages; test the signed updater and rollback too. Independently inspect RAS,
NRPT/WFP and existing/new IPv4/IPv6 traffic, not just Core status or model state.
No Scheduler lab dependency, existing VM reuse, credential/trust-root installation
or uncontrolled cleanup is part of that proposed slice.

Stop on leaked traffic outside the explicitly accepted protection interval,
unreconciled RAS/DNS/WFP, ordinary-user bypass, mutation before quiescence, unavailable
recovery bytes, damaged unrelated data, ambiguous rollback or unbounded continuation.
Preserve state and keep native off; do not repeat against an ambiguous environment.
Passing yields lifecycle feasibility for those exact bytes/OS/routes, not a release
or blanket Windows claim. Only then scope the smallest protected GUI end-to-end
integration, final approval producer and compatible bundle revision. Current Desktop
`MainWindow::nativeUnavailableReason`, absent production `bindNativeController`,
four unproduced Gates, `66d84d8f...` revision checks and beta.1 bundle mismatch remain.
Windows ARM64 needs separate runtime evidence; the four-package matrix precedes release.

### Primary evidence and decision status

Microsoft primary documentation consulted on 2026-11-03:

- [Deferred action context](https://learn.microsoft.com/en-us/windows/win32/msi/obtaining-context-information-for-deferred-execution-custom-actions): restricted original-session access; no replacement G2 proof.
- [MSI rollback](https://learn.microsoft.com/en-us/windows/win32/msi/rollback-installation): authored installer rollback and extra obligations for custom effects.
- [Restart Manager integration](https://learn.microsoft.com/en-us/windows/win32/msi/using-windows-installer-with-restart-manager): application shutdown/restart behavior, not VPN cleanup.
- [ServiceControl](https://learn.microsoft.com/en-us/windows/win32/msi/servicecontrol-table) and [service access rights](https://learn.microsoft.com/en-us/windows/win32/services/service-security-and-access-rights): privileged service control is not package-private isolation.
- [Non-admin elevated installation](https://learn.microsoft.com/en-us/windows/win32/msi/installing-a-package-with-elevated-privileges-for-a-non-admin): advertised/per-machine maintenance cannot be excluded solely by execution token.
- [AppLocker installer defaults](https://learn.microsoft.com/en-us/windows/security/application-security/application-control/app-control-for-business/applocker/windows-installer-rules-in-applocker): admin/signed/cache allowances.
- [WFP session flags](https://learn.microsoft.com/en-us/windows/win32/api/fwpmtypes/ns-fwpmtypes-fwpm_session0), [object lifetimes](https://learn.microsoft.com/en-us/windows/win32/fwp/object-management) and [RasHangUp](https://learn.microsoft.com/en-us/windows/win32/api/ras/nf-ras-rashangupw): distinct lifetime/termination mechanisms, not assembled-system crash evidence.

B is an engineering recommendation conditional on explicit product/security
acceptance and native validation. A remains an alternative; C remains current
behavior. Step 3 proposals and their review rationale are preserved above. No
production contract or supported build/package flow changes in this stage.

### Step 4 review and validation record

One bounded independent read-only review (Astra/codex_petr, low, isolated Desktop
review worktree) found no blocking contradiction in conditional B. Its one low
clarity finding was resolved by stating the precedence of Step 4 and alternative-only
status of Step 3 interfaces at the document opening. The reviewer selectively
corroborated Core recovery/bootstrap code; it did not independently revalidate
external links or claim native evidence. No second review round was performed.

The coordinator read the complete canonical Desktop build/packaging document,
selective Step 3 finals, actual paired proposals and focused source evidence;
Core synchronized concise references in its three original dirty docs. Desktop
relative links/anchors and whitespace (including this untracked file), Core reported
link/heading/whitespace checks and `git diff --check` pass. Both planning branch
HEADs remain those recorded above; Step 2/3 drafts are preserved. Only the same six
documentation drafts changed. Product tests, builds/CI matrix, benchmarks/profiling
and all four package/runtime targets are not applicable to this documentation-only
stage, not passing runtime evidence. No native/MSI/VM/policy/service action, GUI
enablement, commit, push, merge or release occurred. PR16 is registered for context.

Outstanding decisions: accept/reject B's ownership/threat-scope change; define the
network protection interval without silently weakening existing requirements;
authorize the bounded feasibility slice only after those decisions. Current GUI
disablement, legacy interference, crash recovery and artifact mismatch remain
release blockers. Stop here until the next stage is explicitly authorized.
