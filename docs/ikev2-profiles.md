# Experimental native IKEv2 Desktop integration

## Status and implementation gate

This is the accepted stage 8 specification. Partial offline implementation covers
profile handling, authenticated transport, lifecycle fixtures and guarded UI routing.
Production native connection, provisioning and owner discovery remain unavailable;
the installer prototype remains fail-closed and outside production packaging.
Recorded offline checks do not establish protected-install or native runtime support.
Historical planning Desktop baseline: `82943b87a36acfe6ab041902ea02bad917579691`.
Core baseline: `v0.3.0-beta.1`, revision
`9f527efe0a4959786084675f1ff795edb8a0d910`. The annotated tag resolves to that
commit. At planning time its release-by-tag API returned 404. The actual Desktop
bundle remains `v0.2.0-beta.1`; it does not establish native IKEv2 support.

The accepted Core implementation source is
`66d84d8f4dcfc6ada625eb11894b19feea51b7ba`, whose tree matches independently
reviewed PR #71 head `83683e4b5c1a673f0d29d667c09b116f5805027b`.
Both hosted CI runs passed. This satisfies the source/API prerequisite for offline
Desktop implementation, not protected-install or release acceptance. Those require
approved architecture-specific final Core executable digests, Desktop caller live
evidence and demonstrated MSI transaction safety. Do not parse logs or use daemon
start. Historical tags and executable identities remain unchanged; the recorded
`14a3da9a87e07dfe78600c702a3b2f3e7c48e7dbd76baae1c4778c1826067795`
digest identifies only the earlier `6a9bf825ecc0391e2440638b67ffe61b45a1e9c7`
live-tested executable.

## Approved installation and owner contract

The user accepted explicit, separate native enrollment on 2026-10-30. Ordinary
packet-only installations and their existing signed update/rollback behavior must
remain unchanged. Trusted absence of enrollment means native operations are
unavailable; it must not become a new global packet-start prerequisite. Pending,
unsafe or incomplete enrolled installation state must fail closed for participating
protected admissions. Abnormal native-installation failure may require explicit
administrative recovery. Routine successful installation must not silently leave
the product indefinitely locked pending manual settlement.

This accepts the limited product boundary, not a particular registry schema or
an unproven MSI completion protocol. The global missing-state refusal and proposed
settle-before-service-restart updater redesign were rejected. Native execution
remains disabled until safe terminal settlement, migration/participating-host
exclusion and preserved update/rollback are demonstrated. No setup wrapper,
installed general-purpose helper or automatic administrative recovery is added.

<a id="trusted-invocation-procedure-selected-specification-review-pending"></a>

### Trusted invocation procedure (historical; superseded proposal)

Everything in this subsection records the earlier proposal and lab scope. Its
procedural authorization, callback settlement and embedded-action lifetime route
are superseded by the selected proposed Core controller below. Retained future-tense
requirements describe that historical proposal, not current production approval.

Historical option 1 proposed permission to revise this specification and obtain
independent review, not permission to enable native enrollment. For enrollment
and every modification of an enrolled installation, the intended same-user
interactive user must start the approved native MSI already fully elevated, using
an exact independently approved package and protected preload selection. This is
a trusted invocation procedure, not cryptographic or process attestation by MSI
callbacks. Administrators, SYSTEM and the exact TrustedInstaller SID specified
below are trusted; a malicious privileged administrator is outside this boundary.

Residual risk: callbacks cannot prove that this procedure was followed or that
the original initiating process was already fully elevated. That proposal would have relied on correct procedure after independent review;
it is not the current selected authorization contract. The revised procedural contract has independent review for the disconnected
observation only, not production acceptance. The original
`verifyInitiatingClient` remains Unknown; do not rename it Verified. Any future
implementation is separate work. Ordinary nonenrolled packet installation and
signed update/rollback remain unchanged, with no new Clean or admission gate.

Each callback must independently inspect its effective THREAD token first, using
the PROCESS token only for ERROR_NO_TOKEN. All other query errors and filtered
thread tokens refuse; never use RevertToSelf to obtain process authority. Require
full administrator authority: elevation, high integrity, enabled Administrators,
unrestricted token, adequate impersonation level where applicable, and actual
protected-object access. Elevation type alone is insufficient, including for
nonsplit administrators. SYSTEM deferred work can prove privileged access, not
the identity of an interactive user.

The accepted option-1 callback split is: future Preflight and PostFinalize require
full administrator authority and the CURRENT effective interactive principal.
Deferred, Commit and Rollback require effective privileged protected-object access;
SYSTEM is allowed there but supplies no interactive identity and cannot publish Clean.
The intended same-user invocation is procedural. No receipt self-comparison is an
authenticated reference: another full administrator may pass local eligibility.
This does not machine-enforce a designated user or continuity between callbacks.
If required, protected designated-user/reference and continuity gates are future work.

The next disconnected observation reads the selected token's SID, nonzero session
and AuthenticationId, then LsaGetLogonSessionData for that exact LUID. Require valid
bounded data, matching LUID/SID/session and exactly Interactive (2) or
RemoteInteractive (10). Reject SYSTEM, service principals, Session 0, unknown LSA,
query failures, restricted/AppContainer tokens and inadequate impersonation.
Elevation type Full alone never suffices; nonsplit administrators can qualify.
An in-memory explicit-token AccessCheck is only descriptor evidence, not an actual
protected-object open. No observation authorizes mutation, Clean or admission.

Independent approval before preload must bind the exact package, Binary DLL and
protected selection/loading path. DLL self-checks cannot retroactively establish
preload provenance. Preserve exact effective database/Binary, sequence, conditions,
mutation-property allowlist, transforms/MSP refusal, cached maintenance and old
nested-package identities, and external payload pins. Unsupported routes refuse
before mutation. No callback success grants authority by itself. The next
disconnected fixture must meet the [focused acceptance criteria](../packaging/windows/foreground-prototype/foreground-README.md#selected-trust-revision-and-next-disconnected-fixture).

The prototype must constrain its own approved package: old-product removal after
InstallExecute and before InstallFinalize, all destructive/rollback/commit work
inside that transaction, and no later resource mutation. Post-Finalize settlement
is a separate protected journal acknowledgment, not rollback-covered resource
work or proof that the caller received a successful MSI return. Effective token,
required interactive user binding, approved effective package, exact generation,
final inventory and quiescence must be reverified before durable clean publication. Unsupported
transforms, patches, maintenance baselines or mutation-driving properties refuse.
Filtered callback tokens, missing callbacks, failed verification/flush, interrupted
or reboot-incomplete outcomes retain pending for explicit administrative recovery.
Actual callback identity, code-loading provenance, ordering and durable host reads
remain unproved. The current continuous-mutex contract is not silently replaced
by a callback assertion; production stays disabled pending reviewed amendments
and disposable-VM evidence.

Each approved Windows GUI build embeds the exact reviewed Core revision and
architecture-specific executable digest from verified final artifacts. Authenticate
the protected Core image before initial hello, then independently verify the OS
pipe peer PID, creation time, elevated token, same SID/interactive session, image
hash and file identity. Retain verified handles and use identification-only SQOS.
Claimed build metadata is not executable authentication.

The installer creates protected `bimeron-foreground.json` beside Core, with only
`schema:1`, a unique 32-lowercase-hex public `installation` ID and the 64-lowercase-hex
`gui_sha256` of the actual final installed GUI. Preserve the ID across repair and
upgrade. Runtime policy writing and writable development installations are unsupported.

Session activation is a second authenticated hello with instance and exact
`accept_revision`, never a start command. Preactivation stop permanently cancels
activation. Handle `activation_cancelled`, `owner_closing`, `cleanup_pending` and
`session_not_terminal`. Accepted close irreversibly closes admission even when its
response is lost. Cleanup confirmation, stop, terminal state, owner closure and
process exit are separate observations. Provisioning uses a separate owner with
immutable launch parameters and confirmation of its exact `profile.state_token`
as `plan_token`; omit session. Resolve ambiguous confirmation through status.

Missing pipes never prove absence. Unknown ownership, retained/modified protection,
incomplete cleanup or incompatible participating managed hosts block native launch.
Never automatically stop, upgrade, restore or recover another backend. Existing
signed packet-service updates remain available; a mismatching foreground executable
is disabled until an approved Desktop/Core pair is installed.

The MSI implementation must hold the reviewed Core installer exclusion primitive
on one owning thread across all destructive actions, major-upgrade removal, policy
publication, commit and completed rollback. A preflight check is insufficient.
Validate named-object security; refuse abandoned mutexes, active/unknown markers,
unsafe paths and incomplete durable journals. Preserve prior policy and identity
for rollback. Do not schedule unguarded reboot replacement or kill owners.
An embedded native MSI action may implement this, but its actual callback/process
lifetime, journal and nested-uninstall behavior must be demonstrated in a prototype
before claiming installable native support. No such evidence exists yet.

### Proposed Core installation controller (selected architecture; review pending)

**PROPOSED, review pending; not production approved.** This selects a proposed
Core-owned narrow installation command applying a protected approved plan. It is
not an existing Core capability or a setup wrapper/general-purpose installer helper.
Conditional permission to use an installer as part of another route if otherwise impossible does not relax
procedural bypass protection. The preceding trusted-invocation procedure and its
callback settlement/state-record experiments are historical, superseded proposals,
not current authorization or terminality mechanisms. Their evidence is retained.
Core `66d84d8f4dcfc6ada625eb11894b19feea51b7ba` requires an independently reviewed
amendment; it does not already implement this controller contract. No new released
command, plan schema or authorization interface is specified here.

- **Unenrolled:** trusted, complete observation establishes no native enrollment.
  Native admission is unavailable; ordinary packet-only installation/start and
  existing signed updater rollback keep their current behavior. Missing state is
  not a new global packet admission gate.
- **Settled:** authenticated durable evidence binds the exact enrolled installation,
  generation, approved inputs and verified terminal inventory, with no outstanding
  mutation or rollback work. Only matching participating admissions may proceed,
  subject to all existing owner/protection checks; settlement alone is no launch
  authority.
- **Pending:** a durably published enrollment or enrolled-modification intent
  excludes new conflicting participating protected admissions before any resource
  mutation; it does not itself revoke the exact admitted operation described below.
  It remains pending through commit, rollback, nested removal and possible late
  server work until trusted terminal settlement for that exact generation.
- **Unsafe:** contradictory, corrupt, incomplete or unverifiable enrolled evidence,
  failed reads/security checks or uncertain recovery prevents protected admission.
  It cannot be interpreted as Unenrolled or cleared by retry, timeout or deletion.

Absence is a distinct successful trusted read result, never an alias for access,
I/O, decoding, identity, durability or authority errors. Absence alone cannot
prove Unenrolled after enrollment may have occurred; the reviewed migration and
authority contract must establish that distinction. No global missing-state gate
is proposed. Administrative recovery remains explicit, separately specified work.

Admission and publication must share a reviewed serialization boundary so that
no host passes a Settled check while another starts mutation. Before first
enrollment or any enrolled mutation, close new participating admissions, establish
quiescence of existing conflicting work, and durably publish Pending. Mutation
must not begin unless that publication succeeds and is visible to every relevant
participant. Failure/crash at any boundary must leave exclusion effective, even
if the publishing process dies. A preflight snapshot or mutex acquired only for
that snapshot cannot supply this guarantee. The controller must supply this protected serialization and durability; the lab
records and existing model do not implement it.

Every operation must bind the same installation ID, a fresh nonreused generation
and exact operation; every continuation and settlement retains those bindings.
First enrollment instead binds an explicit trusted no-predecessor baseline: a
complete authorized Unenrolled observation, inventoried pre-enrollment resources
and exact rollback baseline, with migration/quiescence obligations satisfied. It
cannot require a prior Settled generation that does not exist. Subsequent enrolled
operations bind their exact prior Settled generation/inventory. Neither missing
records nor a caller assertion supplies the no-predecessor baseline. Bind the
independently approved MSI bytes/identity, effective database and Binary,
architecture, Core/GUI
and external payload digests, protected preload selection, allowed mutation inputs
and rollback baseline. Revalidate these bindings at the privileged boundary and
terminal publication; mutable properties or self-reported hashes are insufficient.
Reject stale, mismatched, replayed or substituted inputs without mutation or
settlement. Preserve exact prior policy, installation identity and rollback data.

Pending blocks new conflicting admissions, not continuation of the exact already
admitted installation/generation/operation with its approved inputs, bounded work
scope and rollback baseline. Such continuation includes bound nested work,
rollback and service restoration needed to finish that operation while Pending.
Each privileged continuation must independently verify its membership in that
still-outstanding admitted work; matching an installation ID alone is insufficient.
Unrelated work, stale generations, substituted operations and continuations after
terminal closure must refuse before mutation. This exception grants no new native
session or unrelated packet start. Its enforceable authorization and revocation
mechanism remains unresolved; Pending is neither a blanket prohibition of required
rollback nor general permission to bypass exclusion.

Every host capable of conflicting work must participate: foreground/native owners,
packet service/start paths where enrollment affects them, signed update helpers,
installer/repair/uninstall and nested old-product hosts. Migration must inventory
already installed/running and cached older hosts, prove they honor exclusion or
are quiescent and cannot resume conflicting work, and refuse enrollment when this
cannot be established. Merely installing a new reader does not retrofit an old
host. The migration and update ordering must preserve the existing signed updater's
verification, replacement and rollback, including service restoration; no proposed
settle-before-service-restart redesign is accepted. Pending cannot make the very
rollback needed to reach a safe terminal outcome impossible.

The selected controller sequence is:

1. Authenticate Core/controller and independently approved inputs; copy and pin
   the exact MSI, payloads, plan and rollback baseline in protected staging outside
   every tree the operation may replace or remove. Validate effective package,
   transforms/patch refusal, mutation inputs, migration and old cached packages.
   Close/drain conflicting participating admissions under the Core exclusion guard.
2. Core `BeginInstallationTransaction` retains that guard on one owning OS thread
   through final release. On that thread call `MsiBeginTransaction`; refuse an
   existing transaction or any failure, never Join an outer transaction. Ownership
   of this outer transaction establishes membership of the exact invoked operation;
   independent research found no callback detector proving enclosing-transaction
   absence. Callback observations cannot replace ownership.
3. Bind installation, fresh generation, operation, exact inputs and baseline; write,
   flush and read back protected Pending before the exact synchronous
   `MsiInstallProduct` call can cause any destructive work. If Begin succeeds but
   Pending write, flush or readback fails, prohibit Install and attempt
   `MsiEndTransaction` rollback on the SAME owning thread. Uncertain partial
   publication or failed/unknown End retains unsafe exclusion; do not infer
   absence, return to Unenrolled or release the guard. A successful End alone
   cannot clear uncertain publication; trusted reconciliation remains required.
4. Call `MsiEndTransaction` on the owning thread with the selected commit or
   rollback disposition, including rollback after install failure. Require its
   authoritative completion for the bound transaction, not just an install return,
   callback, process exit or quiet inventory. Commit and rollback both require
   independently verified final inventory and durable settlement. Release the
   exclusion guard last, after settlement is flushed and verified.

`MsiEndTransaction` is authoritative only for the explicitly bounded transaction
mutation domain, not arbitrary future tasks. Approved packages must be synchronous
and bounded, with no detached workers, reboot/deferred replacement, chainer or
out-of-transaction mutation. Enumerate nested old-product removal, rollback/commit,
services, policy and payload resources; authenticate old cached package identities
and migration controls before allowing their destructive work. Refuse unsupported
baselines/routes. No native MSI callback publishes settlement.

An Install failure is not itself terminal evidence. A failed installation followed
by authoritative successful End rollback, exact verified restoration of its bound
baseline, complete domain closure and durable verified settlement is a safely
restored FAILED operation, not a successful install. Failed or unknown End
(including access denied, install failure or in-progress End results), disabled
rollback, reboot-required/initiated results, incomplete inventory, outstanding
work or failed durability remain unresolved and must not settle. An Install
failure without the full successful rollback evidence remains unresolved. A timeout
is uncertainty, never permission to terminate workers or infer completion.

After durable Pending, controller death fences NEW conflicting participating work
even after mutex abandonment; it cannot revoke a standard MSI execution segment
that has already been authorized. That segment may continue, including nested
work. Bound rollback/service-restoration authority must survive controller death
to finish the exact admitted operation. This survival and enforceable domain
closure are explicit disconnected-model assumptions, not a claimed native MSI
revocation or surviving authorization capability. Death never supplies settlement;
uncertain outcomes require explicit separately reviewed recovery. A dead controller's
held-thread End protocol cannot be assumed to have completed.

Direct enrolled MSI bypass remains a mandatory unresolved production gate. EVERY
participating independently approved effective package must contain mandatory
execute-sequence preflight AND a first deferred admission gate before any
resource mutation. This includes direct enrollment, enrolled install/repair/
maintenance/uninstall, cached packages and nested old-product removal. Neither
gate may be skipped through UI routing, sequence conditions or caller-controlled
properties. Independently approve the exact effective database, Binary DLL,
sequence/conditions and trusted preload selection before loading actions; self-
checks after loading cannot establish provenance. Old ungated baselines must
refuse enrollment/upgrade participation rather than relying on the new package's
gates. Preserve unchanged direct packet behavior only for trusted Unenrolled state.

Both gates require independently grounded controller/operation authorization,
with exact package/action and installation/generation/input bindings; every
privileged destructive continuation must retain those bindings. An authenticated
local pipe proves its peer PROCESS, not the executing DLL export, action, MSI
session or effective sequence. Independent approved preload provenance and exact
effective sequence are therefore required even with peer authentication.
CustomActionData (CAD) may locate bound data; it is never authority. A public
property, nonce, claimed PID, action name or matching plan ID is insufficient.
Native proof tying that approved preload/action/session to the admitted operation,
transport and cached-old-package support remains unresolved. Outer transaction
ownership alone neither authorizes direct enrolled execution nor an arbitrary
outer caller. Conditional permission to use a controller when necessary accepts
neither procedural bypass nor unenforced enrolled-package gates.

Bound signed-updater rollback/restoration must finish while Pending using narrowly
verified authorization for the existing operation. It must neither reacquire a
mutex held by its controller nor require Settled before restarting/restoring the
bound service. Define a continuation route that also preserves bound rollback/
restoration authority after controller death, and reject unrelated starts; test it
before accepting participation. Existing signed verification and
rollback guarantees remain mandatory.

The Core amendment must cover this ownership/thread lifetime, protected admission
and durable publication, authenticated destructive-action authorization, migration,
bound restoration and crash recovery. Native evidence and independent review are
required before enabling production. This documentation supplies neither.

Minimal disconnected acceptance is an exploratory finite logical-state model,
with explicit events and assumptions, no production reader/writer or MSI wiring.
The first model covers enrollment and enrolled modification with commit or exact
restored rollback, including failed enrollment returning to its bound baseline;
it excludes successful native-enrollment removal. A later separately reviewed
extension must define removal terminal inventory, durable enrollment-history and
authority evidence, exclusion retirement and when Unenrolled may safely be
reestablished. Successful removal must not be inferred from deleted state or a
missing record; nested old-product removal within an admitted upgrade remains
bound work in the first model.

Model terminal events are explicitly assumed evidence with no native producer.
Each event binds installation ID, admitted generation, exact operation and inputs,
expected and observed terminal inventory (commit or restored baseline), and the
complete outstanding-work domain, requiring no outstanding mutation, rollback,
nested or delayed work. The model assumes enforceable closure against future
continuation for that domain; it does not derive closure from finite observations.
Reject missing/mismatched bindings, wrong inventory, outstanding work and replayed
terminal events without settlement. Reject late continuations after assumed
terminal closure, as well as unrelated/stale continuations while Pending. These
are conditional model assertions, not evidence that a real host can enforce them.

Cover trusted no-predecessor first enrollment and rejection of a fabricated or
unknown baseline; Unenrolled packet behavior; pending-before-mutation ordering;
competing new admission versus exact bound continuation; bound rollback/service
restoration while Pending; crash/failed publication at each boundary; read errors
versus absence; stale/replay and input mismatch; old-host nonparticipation; commit
and restored rollback; late server work; and rejection of callback/exit/ACK/
checkpoint-only settlement. Assert that no conflicting new admission overlaps
mutation and no uncertain outcome becomes Settled. Model passes support exploration
only, not acceptance of this proposal or production terminality.

#### Smallest disconnected controller implementation acceptance

Keep the reviewed enrollment model unchanged: its terminal events still assume
closure, durability and migration, with no native producer. Add only a disconnected
controller model with injected MSI calls, protected-store/inventory outcomes and an
owning-thread guard; no real MSI, registry, service, native settlement callback or
released CLI/schema. Reuse the logical bindings above rather than duplicating the
generic enrollment model.

Acceptance must assert ordered authentication/staging, guard acquisition, Begin
(no existing transaction/Join), Pending write/flush/readback before the exact
Install call, End commit or rollback, inventory, durable settlement and release
last. Exercise wrong-thread entry/continuation/End/release and exceptions. Require:

- Both mandatory gates for every participating effective package, including direct
  and quiet/UI-skipped routes, maintenance, cached and nested old removal; reject
  skipped/conditioned/property-bypassed gates and old ungated baselines.
- Forged peer process, action/export/session claims and CAD/property/nonce/plan
  substitutions refuse. A valid pipe peer with unapproved preload/Binary/sequence
  still refuses; injected approved provenance is an assumption, not native proof.
- Begin success followed separately by Pending write, flush and readback failure
  never calls Install; attempts End rollback on the same thread. Partial/uncertain
  publication and failed/unknown End retain unsafe exclusion without absence
  inference or guard release.
- Successful commit and failed Install plus successful End rollback, exact baseline,
  complete domain closure and durable settlement have distinct outcomes: successful
  operation versus safely restored failed operation. Failed/unknown End (access
  denied, install failure, in progress), rollback disabled, reboot, incomplete or
  mismatched inventory and durability failures never settle.
- Death after authorization permits the already-authorized MSI segment to survive
  while Pending fences NEW work. Model surviving bound rollback/restoration
  authority explicitly, without inventing native revocation or automatic settlement.
  Bound signed restore avoids mutex reacquisition and settle-first deadlock.
- Reject stale/substituted continuation, unrelated new work, outstanding/out-of-domain
  work and callback/exit-only settlement; preserve Unenrolled packet compatibility.

Keep successful authorization, surviving restoration authority, durability and
closure explicitly assumed; disconnected passes provide no native proof.
Successful enrolled removal remains a separate specification/acceptance extension.

Native acceptance separately requires immutable approved inputs, real protected
publication/read durability and security, competing participating hosts, migration
from old/cached hosts, nested upgrade/rollback, crash/interruption and late-server
observations, plus independently verified terminal inventory, enforceable closure
of the bounded mutation domain and signed updater rollback preservation. Record
actual unavailable/error outcomes and architecture limits. Independent specification review, Core amendment and review of concrete
evidence precede production approval; model passes or a single MSI run cannot
satisfy these gates. This docs-only proposal changes no build or packaging flow.

Current parent checkpoint VM experiment outcome is **UNKNOWN**. The old client
root was moved to `prelaunch-c260f52f`. Review found nested inventory records
without individual file hashes, so equality across that move is unproved. Exact
`85930a2` runner/MSI/manifest guest hashes and protected ACLs were separately
verified after staging.
One Invoke-Command AsJob was dispatched, but parent collection failed because
Tee-Object combined incompatible LiteralPath and Append, and the host exited.
No result was captured; immediate read-only observation found no MSI logs or
Pending/checkpoint. That absence proves neither nonexecution nor terminality.
VM orchestration is stopped; this record authorizes no retry or VM access and
claims no successful launch, interruption, rollback or settlement.

## Core prerequisite and security acceptance

Core must specify and implement an elevated interactive foreground owner under
the same Windows SID as the current-user RAS profile. Reject elevation under a
different account before network mutation. Do not add a separate Engine helper
or reimplement native networking in Desktop.

- Provide a private foreground endpoint with IPC 1, correlated bounded requests,
  owner/SYSTEM ACLs, peer identity, PID and instance verification. Never fall back
  to the system daemon. Publish the exact wire schema before Desktop coding.
- Provide hello, status, events and stop. Start exactly one session from an
  explicit absolute config path; reject packet start and arbitrary execution.
  Return Core identity and capabilities.ikev2. Distinguish requested, active and
  retained protection, failed cleanup and unavailable statistics.
- Stop cancels startup or disconnects and waits for ordered cleanup. Process
  exit alone is not proof of restored networking. Lost GUI/IPC does not remove
  protection; allow reconnection only to a verified owner. Unknown ownership
  blocks new sessions. Do not force-kill to resolve a timeout.
- Return bounded structured provisioning preview/apply/remove results through
  a protected channel. Preserve Core ownership/GUID/settings checks, active or
  modified profile refusal, collision checks, journals and rollback semantics.
  Apply must use the exact reviewed parameters, not mutable GUI state.
- Recheck executable/config identity at the privileged boundary and prevent
  substitution between validation, preview and mutation. Do not accept shell
  fragments or arbitrary command paths over the endpoint.
- Keep recovery explicit and administrative, after stopping the owner. Never
  adopt/disconnect foreign RAS sessions or delete foreign WFP/NRPT policy.

Core tests must cover wrong SID/peer, malformed and oversized messages, stale
instances, cancellation, lost IPC, GUI/owner crashes, startup/cleanup failure,
retained policy, collision/modification, interrupted journals and recovery.
An isolated Windows VM must verify cleanup and preservation of foreign policy.
Record the agreed replacement SHA, protocol schema and sanitized evidence here
before lifting the Desktop implementation gate.

## Desktop behavior

Import protected UTF-8 native .conf files with Profile and IKEv2 sections and no
AccessKey. Keep the 64 KiB bound and selected-Core validation. Reject mixed
sources through Core validation; invalid imports leave no retained profile.
Preserve bytes, BOM/line endings, relative-reference safety, atomic replacement
and effective ACLs. No-op saves remain byte-identical.

Use an explicit native backend discriminator in ProfileConnectionInfo, without
reinterpreting packet connectable semantics. The native form contains
WindowsProfile, ServerName and optional native KillSwitch only. Interface allows
Mode=vpn only. Unknown/unrepresented content remains protected Raw. Never
convert a native profile to SS or drop settings to enable visual editing.

Use a small NativeIKEv2Session controller and the existing CoreClient transport,
with a Windows implementation and unsupported Linux stub. Keep DevelopmentCore
and packet connection management separate. Require authoritative absence of a
conflicting managed session; never automatically stop/restore the service.

Expose provisioning as Preview followed by explicit Apply, and separate explicit
Remove. Start never provisions or repairs a profile. Certificates, credentials
and initial saved EAP identity remain Windows-owned. Never persist their bytes.

Read capabilities.ikev2 rather than inheriting packet flags. Hide proxy, packet
rules, ShadowTLS, DNS/MTU overrides and charts. Show statistics as unavailable.
Explain unprotected bootstrap, LAN blocking and retained WFP/NRPT protection.
New native KillSwitch defaults off; enabling requires explicit confirmation.
Preserve imported intent and distinguish it from active protection.

No native autostart or automatic reconnect. Close-to-tray preserves the owner.
Explicit Quit offers graceful stop with cleanup or cancellation of exit. Unknown
or timed-out cleanup blocks switching/start; recovery is never automatic.
Linux may preserve/edit profiles, subject to successful selected validation,
but cannot connect or provision. Provide safe English/Russian messages.

## Implementation and verification

Integrate stage 6 first, then rebase stage 8. Coordinate ProfileEditorDialog
loadText/buildText/assessRepresentation, ProfileStore importConfig/connectionInfo,
MainWindow connection/tray/exit/update transitions, translations and tests.
Do not independently refactor shared importer/editor/connection contracts.

Extend ProfileVlessTest.cpp for native import, labels, mixed/invalid sources,
Raw fallback, no-op bytes and validation/write/ACL failures. Add isolated
NativeIKEv2SessionTest.h fixtures to the existing test executable for UAC cancel,
SID/PID mismatch, stale replies, bounded polling, cancellation, crash, cleanup and
recovery gates. Extend ConnectionManagementTest.h to prove native profiles never
reach daemon start and existing SSH/packet/update behavior remains intact.
Use secret sentinels across settings, diagnostics and logs.

Run all existing CTest executables on native Windows x64 and Linux amd64/arm64,
and all four GUI builds. Windows ARM64 remains compile-only. Check translations,
links, diff hygiene, UI heartbeat and EN/RU layouts at 100/150 percent scaling.
Profile only if responsiveness/polling regresses. In separately authorized live
checks compare foreground start/stop latency with the same Core without GUI;
repeat throughput measurements only if Desktop shows data-plane impact.

Live acceptance requires a disposable Windows 10/strongSwan environment, exact
Desktop/Core identities, same-user elevation, credential/certificate rejection,
provisioning ownership guards, IPv4/internal IPv6/DNS, protection activation,
loss/crash retention and explicit recovery with baseline/recovered captures.
Core issue #56 and PR #59 establish only their recorded Core milestone; Windows
11, RRAS and physical mobility remain deferred in Core issue #66. Public IPv6
and Windows ARM64 native interoperability are not established.

Published assets, four hashes, bundle pins, signed compatibility metadata and
the full MSI/DEB release matrix are one separate shared pre-release dependency.
This specification does not change packaging or authorize release.

## Offline implementation evidence

The following dated results and earlier fixture acceptance records are historical
evidence with their original scope. They do not implement or approve the
[historical trusted invocation procedure](#trusted-invocation-procedure-historical-superseded-proposal).

Windows x64 Release tests used VS 18 2026 BuildTools and Qt 6.8.3 with the
explicit offscreen platform (no native-platform fallback). Baseline
`7f44c7309fc4cf9e91554f6bed8f167cefcda35d` and stage 8 profile revision
`a8024a2a73ccc848488ad3b2c2f796db4581b304` both passed
`developer-mode-test --visual-check` and `profile-vless-test` once without retries.
The clipboard assertion and subsequent packet/developer transport, transient SSH,
Quit and connection lifecycle fixtures completed. Qt offscreen uses a process-local
clipboard; this does not validate native Windows clipboard behavior or rendered UI.

Optional Core validation variables were cleared for these runs. Native-profile
CLI checks require `BIMERON_IKEV2_CORE` to point to the exact reviewed Core build;
those checks remain outstanding. The newly added foreground transport was not
registered in the tested targets and is not covered by these results. Protected
handshake, elevated launch, MSI transactions and live native networking remain
separate acceptance gates. No final executable approval or release is implied.

After registering the native sources and applying controller fix `ab2cd7c8`,
integrated source `9f82f97` built successfully on Linux amd64 under WSL Ubuntu
with its supported system Qt. CTest passed all three targets: developer-mode,
core-update and profile-vless. These runs include the registered native transport
and controller offline fixtures, not a protected Windows handshake or native VPN.
The supported system Qt remains eligible for Linux testing; Windows offscreen
clipboard evidence above is specific to the inspected Qt 6.8.3 platform plugin.

Controller re-review closed the delayed-discovery and legitimate terminal-cleanup
findings at `9f82f97`. Follow-up fixture `a9c1f35` removes hello-only metadata from
stop responses and asserts retained verified capabilities for preactivation and
activated sessions. Registered CTest passed 3/3 on both Windows x64 Release with
explicit Qt 6.8.3 offscreen and Linux amd64 after this fixture was added. These
results remain offline-only and do not establish native installation acceptance.

The optional `BIMERON_IKEV2_CORE` profile validation fixtures subsequently passed
on Windows x64 with a validate-only CLI built from the exact Core `66d84d8f`
Git archive (tree `33dcda328133a36b0f103b4a0599a09e20afce4d`). Its SHA-256 is
`34c43e7b4e5a14737cbc9c5f105a92cad8ff2f0eb4a5b5634c70f2e45a78ac39`.
Build inputs: Go 1.26.9, CGO disabled, readonly modules, trimpath and buildvcs=false;
Version records the complete source SHA with an archive suffix. Native
`vcs.revision` is absent and `modified=false` is a default, not clean-VCS evidence.
This artifact was used only for static validation, never authenticated foreground
execution, service installation or release approval. Earlier outstanding-validation
statements above retain their historical scope; this closes the profile CLI fixture
check only, not artifact approval or live integration.

The final integrated offline pass at
`21acc2789ad0a6eaa4b7286a3e78ca03751f39e9` built the GUI and all test targets
for Windows amd64/ARM64 and Linux amd64/arm64. Windows x64 Qt 6.8.3 offscreen
and Linux amd64 system Qt 6.4.2 offscreen each passed CTest 3/3. The hash-verified
validate-only Core archive profile run passed 1/1. Both ARM64 results are
compile-only; native ARM runtime/CI was not run locally. Diff hygiene and
case-sensitive translation JSON parsing passed. No package generation,
protected installation, native GUI connection, enrollment or live VPN acceptance
is established by this matrix.

### Offline protected-ack access fixture acceptance

A separate, default-off standalone Windows lab target may observe effective-token
access without authorizing enrollment. Its callbacks must fail closed, report
initiating-client correlation as Unknown, and never publish clean state. The
fixture must not write a pending or acknowledgment file unless a fixed private
lab directory, its ancestors and pinned handles have been validated against
reparse and owner/DACL substitution. A complete implementation additionally
requires an isolated validated mutex, CREATE_NEW and flushed bounded records,
immutable generation matching, and external exact-package and client-process
pins. Until those requirements are implemented and independently checked, no
MSI is suitable for guest execution. Offline tests must distinguish actual OS
observations from injected failure models; no installation, registry, network,
service, credential, elevation or guest action is authorized by this amendment.

## Bounded offline UI controller binding acceptance

Bind an externally supplied concrete NativeIKEv2Session privately to MainWindow,
using QObject-context connections and QPointer lifetime guards, including its initial
snapshot. Production must not construct, discover, launch or provision a controller
until a protected approval provider exists. No runtime setting can enable native
Connect, Preview, Apply or Remove. The daemon-observed nativeSession flag remains
separate from the bound foreground controller.

Controller signals must update the native status label and tray through connected,
stopping, retained/modified and unknown states with statistics unavailable. Packet
status replies, profile selection and tray rebuilding must preserve that display
and its sticky block. Controller disappearance becomes unknown and cannot prove
cleanup or absence. Switching, service mutation, update, restart, autostart and
reconnect remain blocked. Close-to-tray sends no command; explicit Quit cancels
with the existing localized explanation, without native stop/close orchestration.

Acceptance uses the real controller with existing offline hooks and actual changed
signals, including initial state and destruction. Unbound packet and SSH fixtures
must retain their behavior without constructing or discovering a native controller.
Run focused offscreen baseline and post-change tests, all three existing CTest
executables on Windows x64/Linux amd64, and four GUI compilations where available.
Report ARM64 runtime and EN/RU 100/150 percent presentation evidence honestly;
compilation or fixture generation alone is not visual or native activation proof.
No timer changes, benchmark, packaging, enrollment, network mutation or release.

Queued-callback acceptance refinement: callbacks queued before real-controller
binding must recheck the sticky native authority boundary when delivered.
Post-update refresh and Development changed delivery must preserve native
presentation, tray state and the block, send no daemon hello and cause no native
activation, including after controller destruction leaves Unknown. Development
status and orphan-cleanup controls must still refresh. Unbound packet update
refresh and Development notification behavior must remain unchanged. Reproduce
with queued callbacks before the fix, then rerun focused offline tests and all
four GUI compilations. No timer or performance behavior is changed.

Offline UI binding evidence (2026-10-30): based on parent `e5f00d3`, acceptance
was committed before implementation. Windows x64 Qt 6.8.3 and Linux amd64 Qt 6.4.2
passed all three existing CTest executables with the offscreen platform and optional
Core validation variables cleared. Windows required the explicit Qt plugins path;
an initial missing-plugin process was terminated and the configuration corrected.
All four GUI/test builds compiled; both ARM64 targets are compile-only (no ARM64
runtime or qemu-aarch64 available). No packaging flow or controller/transport
implementation changed. No timers changed, so benchmarks/profiling were inapplicable.

The real-controller fixture verifies initial snapshot, changed propagation through
connected/stopping/retained-modified, Russian translation, packet-status and profile
refresh containment, tray rebuilding/Quit, settings/autostart/restart guards, service
and update refusal, and controller destruction retaining Unknown. Existing packet
and SSH tests pass. Production has no controller construction or binding caller,
no native hello/discovery/launch/provisioning, and no approval setting. The sticky
block deliberately has no UI recovery or native stop/close orchestration in this batch.
Diff review found no packaging, pin, credential, or network-mutation changes.

EN/RU 100/150 percent bound-Unknown presentation PNG fixtures were generated and
inspected in ignored build state. Windows offscreen rendered missing-glyph boxes,
so these images do not establish readable text, clipping or visual acceptance.
Native-platform presentation and real tray/close behavior remain manual acceptance
gates. This is partial offline GUI integration, not native activation, protected
installation, enrollment, live networking or release acceptance.

Queued-callback fix evidence (2026-10-30), isolated baseline `3cf0f89`:
acceptance above was recorded before the fix. The Windows x64 reproducer failed
before production edits with "queued legacy callbacks preserve native presentation
tray and authority". The fix checks the sticky native block inside both shared
callback handlers; Development status and orphan-cleanup controls still refresh.
Queued callbacks across real-controller binding and destruction now preserve the
native labels, tray status and connection generation, with no daemon request or
native activation. Explicit unbound update refresh and invalid Development
selection checks retain packet synchronization and failure presentation.

Final Windows x64 (Qt 6.8.3) and Linux amd64 (Qt 6.4.2) CTest passed 3/3 each,
including developer-mode-test --visual-check, with explicit offscreen and optional
Core variables cleared. All four GUI targets compiled; Windows/Linux ARM64 were
compile-only. Review covered every refreshCoreAfterUpdate/updateDevelopment caller
and sibling hello/invalidation authority paths; no additional finding remains in
this fix scope. No transport, controller, pins, packaging or timer changes; no
benchmark/profiling or release applies. EN/RU glyph/layout and native tray visual
acceptance remain open as recorded above.

### Disconnected pending-file fixture acceptance (bounded batch)

This batch permits only a disconnected pending-file primitive, not MSI authority.
The standalone default-off FOREGROUND_LAB_PROTECTED_FILE target has no install
rules. The only native directory is LABProtectedFiles beside the harness image;
no command line, environment or arbitrary path is accepted. It must already exist.
The harness must be strictly below the native Program Files known folder returned
by SHGetKnownFolderPath. Resolve both local fixed-drive paths once to the same
volume GUID, reject malformed, relative, stream, dot and sibling-prefix paths,
and retain read-sharing-only non-reparse disk directory handles from the volume
root through LABProtectedFiles. Ancestors above Program Files are traversal and
metadata pins only; Program Files and every descendant require protected non-null
DACLs and trusted ownership. Trust is exactly SYSTEM, Administrators and the
TrustedInstaller SID S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464;
other service SIDs remain untrusted. Reject unknown ACEs and untrusted mutation
rights. Record the Program Files pin index and apply the same boundary on every
operation. Do not repair ACLs or ownership, or accept an alternative anchor.
File opens deny all sharing and check disk type, identity, single link,
non-directory/non-reparse attributes, owner and protected DACL before I/O.

The exact 48-byte record is ASCII BLPF, version byte 1, pending byte 1, two zero
reserved bytes, 16 installation bytes, 16 generation bytes, a little-endian
32-bit acknowledgment counter (0..1024), and four zero reserved bytes. Both IDs
must be nonzero and immutable. No other size, version, state or reserved value is
accepted. CREATE_NEW writes counter zero; acknowledgment rereads exact bytes,
compares both IDs and increments only below the bound, then writes completely
and FlushFileBuffers. Errors preserve the file, including partial writes; no
clean state, delete, recovery, retry or settlement API exists. Exclusive handles
serialize operations without a named mutex. Security is rechecked before writes.

Acceptance checks separate pure record/ACL models and injected short-write/flush
failure from native protected-path observations. Only owned ignored build fixtures
may be written. Native positive unavailability is reported, not bypassed. Compile
x64/ARM64; run x64 tests. Linux/performance/release checks are inapplicable to this
Windows-only disconnected target. MSI/client correlation, effective database trust,
package pins, participating host exclusion and durable clean publication remain
unimplemented gates; prospective exports must continue unconditionally failing.

Path-pinning refinement: resolve the module's local drive root once to its volume
GUID using GetVolumeNameForVolumeMountPointW, then use only that canonical volume
path for every pin and leaf open. This prevents later DOS drive-letter remapping
from redirecting the fixed leaf. Caller-supplied device/UNC paths remain absent.


#### Anchor identity and case namespace correction acceptance

Resolve the shared local fixed-drive root once, then derive both volume-GUID
paths from that mapping. Open and retain the actual native known-folder anchor
with read sharing only, non-reparse metadata and protected ownership/DACL checks.
Text containment only locates a candidate: the traversal anchor must have the
same volume serial and file ID as that held known-folder object. Recheck this
identity and both handles' security before each operation. Preserve all existing
root pins, authority-index, mutation, descriptor and reparse checks.

Every pinned directory, including the volume root and known-folder handle, must
successfully report FileCaseSensitiveInfo with zero flags. Enabled or unknown
flags and all query errors refuse admission; there is no unsupported-filesystem
or older-Windows fallback. This intentionally sacrifices availability where the
query is unsupported. Tests must reject same-prefix/different-anchor identities
and enabled/unknown/error case flags. An owned temporary-directory fixture must
attempt actual SetFileInformationByHandle case sensitivity, then demonstrate
actual differently cased opens and identity results if enabled. Permission or
support refusal is reported as native unavailable, never replaced by model claims.
No elevated host setup, ACL repair or guest harness is part of this correction.

Offscreen font-discovery follow-up (2026-10-30): the parent rerendered the four
bound-Unknown previews from the author checkout at `6b236c0`, using child-only
QT_QPA_FONTDIR=C:/Windows/Fonts with Qt 6.8.3 offscreen and explicit plugin paths.
All four preview processes exited 0. Visual inspection of EN/RU at scale 1 and 1.5
showed readable ASCII/Cyrillic, complete wrapped Unknown explanations and visible
button labels without observed clipping in these frames. Generated files remain
ignored as native-fontdir-{en,ru}-{1,1.5}.png in that checkout's build directory.
No fonts were installed and no product/global configuration changed. This resolves
the missing-glyph artifact for those offscreen previews, not native Windows font
selection, tray/close behavior or visual acceptance of every GUI state. The later
queued-callback fix changes guards rather than layout; these images do not claim
a fresh binary build of that later revision.

### Protected ACK access-only MSI specification and acceptance

This isolated batch supersedes the earlier token-only fixture gate only for a
new access experiment, never production enrollment. Implement a default-OFF
FOREGROUND_LAB_PROTECTED_ACK standalone option and explicit excluded-from-ALL
target; no install rule, production CPack, host MSI action or guest execution.
Existing fail-closed exports, pending-file harness pin() and frozen artifacts
remain unchanged. The only new path is native Program Files\BimeronProtectedAckLab\LABProtectedFiles,
resolved with the existing volume/anchor/case/DACL/pinned-handle checks. No
caller supplies a path. The guest directory and state must be new and separately
authorized; the existing pending protected-file guest experiment is untouched.

Build Success, CommitFail and PostFinalizeFail DLL/MSI pairs for x64 and ARM64,
with fresh product, upgrade and package GUIDs, empty Component and
FeatureComponents EnsureTables and zero installed resources. Five synchronous
checked Binary callbacks run at 1499 (Preflight), 1501 (Rollback registration),
1502 (Deferred), 1503 (Commit) and 6601 (PostFinalize), around Initialize 1500
and Finalize 6600. CommitFail returns 1603 in Commit; PostFinalizeFail returns
1603 before any acknowledgment. Commit and Rollback never acknowledge.

An externally prepared immutable protected run.manifest is generated after MSI
linking. It binds nonzero 16-byte installation/generation IDs, architecture,
compiled variant, product/package IDs, exact MSI and Binary SHA-256 and the
canonical actual database table/sequence expectations. Parent verification pins
the original MSI bytes externally; callbacks cannot attest their original MSI
source hash. Preflight validates protected storage/manifest and effective active
database logical content/Binary, rejects transforms, patches and unsupported
maintenance. Deferred independently rereads protected manifest and CREATE_NEW
writes/flushed pending counter zero using its IDs. PostFinalize independently
rereads manifest, checks effective thread token (process fallback exclusively on
ERROR_NO_TOKEN), protected write access and effective database, then requires
matching IDs and exact counter zero, increments once and flushes counter one.
Any existing record blocks creation; counter one refuses replay without writing.
All failures retain residue: no Clean, delete, repair, retry or settlement claim.
No ID/path property, client PID/ancestor/SID/session authority, production mutex,
host reader or verified boolean is introduced. Diagnostics remain bounded and
state initiatingClientUnknown. Trusted-engine extraction of Preflight code is an
unresolved boundary; production approval remains denied.

Acceptance: /W4 /WX x64/ARM64 compilation, six cached-WiX builds with unsuppressed
ICE, read-only actual table/Binary/PE/flags/identity/import checks, x64 actual token
precedence and owned ignored temporary replay checks. Label stale-ID/ACL/write/
flush models separately from protected native positives. Freeze source SHA,
twelve artifact hashes, eighteen GUIDs and exact manifest outputs/logs in ignored
build state. A READY GUEST REVIEW BUNDLE contains one Success MSI and matching
canonical protected manifest with enumerated effects; it grants no permission.
ARM64 runtime, Linux, performance and the production matrix are inapplicable.

### ACK zero-resource freshness and diagnostics follow-up

Before another candidate run, the disconnected lab may accept only empty REMOVE
or exact uppercase ALL, and only after its protected manifest, active package
header/Binary, exact canonical database and explicit zero Component,
FeatureComponents and File rows are validated. Query the OS product state for
the manifest ProductCode and require UNKNOWN (-1), retaining all other freshness
and rollback-enabled checks. This is lab-only eligibility, not authenticated
caller intent: always report removeOriginUnknown=1 and initiatingClientUnknown=1.

Each callback must report a bounded first-failure stage and numeric detail/status:
directory pin, manifest read/format, header, zero-resource query, canonical query
or first differing category, freshness predicate, rollback Boolean, effective
token, record create or acknowledgment. Never print property values, account
names, environment or arbitrary errors. Preserve all prior frozen artifacts and
guest residue. Offline tests must cover exact ALL versus lowercase/other values,
nonfresh product state, nonzero resources and first-stage preservation. Any new
bytes require static review before the next VM run; enrollment stays disabled.


### Bounded checkpoint-only ACK variant acceptance

This accepted offline chunk adds default-OFF FOREGROUND_LAB_PROTECTED_ACK_CHECKPOINT,
explicit foreground-lab-protected-ack-checkpoint, x64 only, excluded from ALL,
installation and production. Its fresh Checkpoint identities and compiled variant 3
reuse the ACK source, exact WiX sequence, protected run.manifest/pins and 48-byte
Pending format. The three existing ACK variants and trace contracts are unchanged.
Historically the trusted invocation procedure was selected for specification review;
it is superseded by the proposed Core controller above; this checkpoint establishes neither its runtime binding nor continuous
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

### Read-only current-context CLI acceptance

Add only exact `foreground-test.exe --observe-context-only` to the existing test
executable. Dispatch before all fixtures, temporary files, child processes and
crash cases; call the reviewed `observeCallbackInteractiveContext` exactly once
with no descriptor. Print one fixed numeric summary of completeness, current
principal eligibility, token source/type/elevation/authority/session, LSA validity,
logon type and refusal/status. Never print account names, SIDs, logon/token IDs,
environment or arbitrary input. Only logon types 2/10 remain eligible.

Exit 0 means complete observation, including policy refusal, never a security test
pass. Token/LSA query failure or invalid/missing LSA data is incomplete and exits 1,
even if an earlier policy refusal masks that failure. Invalid arguments exit 2
without running fixtures; exact internal --probe/--crash forms retain behavior.
The mode performs native token/LSA queries only, with no owned file writes, child
process, network, SCM, registry or ACL mutation. No new executable, hook, export,
initiating-client correlation, arbitrary-path option or build flow is introduced.

Acceptance: existing x64 focused CTest, x64 exact-mode output/exit and invalid-mode
checks demonstrating no full-suite output; x64/ARM64 existing target /W4 /WX builds,
ARM64 static checks only. Freeze committed source, executable/required DLL hashes
and logs in new ignored build/context-observer-review; preserve all MSI bytes.
Host observations and models remain separate. Parent alone owns later disposable
VM Elevated/Limited/Session-0 observations; those need separate review and scope.
No result proves authority, protected-object open, designated-user identity,
callback continuity or preload provenance. Production remains disabled. No GUI,
Linux/package matrix, VM execution, benchmarks or profiling applies to this mode.

Read-only CLI evidence (2026-10-30): the acceptance above preceded implementation.
Only foreground-test.cpp changes behavior. Existing callback-context-x64 and
callback-context-arm64 targets built Release /W4 /WX, without WiX. x64 CTest
passed 2/2; ARM64 foreground-sequence passed 1/1 as host static validation only.
The inherited PE checker passed both DLLs; DLL hashes remain exactly those from
reviewed observation source 0af7a0ee. No MSI target was built or MSI bytes changed.

Exact host mode returned 0 with complete=1, eligible=1, refusal=0, status=0,
lsaStatus=0, source=2 (process), type=1 (primary), elevationType=2 (Full), elevated=1,
enabledAdmin=1, adminDenyOnly=0, restricted=0, appContainer=0, servicePrincipal=0,
integrity=12288, session=1, lsaDataValid=1, lsaSession=1 and logonType=2.
This native host observation had no refusal; it does not substitute for native
Limited or Session-0 runs. Existing full-suite native checks separately refused
identification/restricted tokens and anonymous query (1347); malformed LSA,
SYSTEM/session and exact 2/10 coverage remain models, not CLI native positives.

The ignored mode check executes the exact CLI and four invalid forms, checks
single-line numeric output under 1024 bytes, exit/completeness agreement and no
MODEL/NATIVE/PASS suite output. Extra observer arguments, misspelling and malformed
internal forms exit 2 with fixed diagnostics. Existing exact internal forms pass
through the full regression suite. Diff review confirms early dispatch and one
observer call, with no fixture side effects on that path; no production authority,
protected open, continuity, designated-user or preload proof is claimed.

Frozen review location: `build/context-observer-review`. Minimal x64 VM input is
only `x64/foreground-test.exe` and adjacent `x64/foreground-prototype.dll` (static
CRT; remaining imports are Windows system DLLs). From that directory run:

```powershell
.\foreground-test.exe --observe-context-only
```

x64 executable SHA-256:
`4f42af033b00d0524756f6f9cd3f0aeeba30dbf711dbc60df00f6c43a26a5246`.
ARM64 executable SHA-256 (compile/static only):
`63c9df759540a37798ee48d46d1b5dda79acb83a6ae21ef2684775170ea61d9c`.
The post-commit inventory freezes source commit/bytes, both architectures, logs,
mode check and existing MSI inventory, without later documentation edits. Parent
VM scope remains separately reviewed Elevated/Limited/Session-0 observation only;
no MSI, protected writes or installation is needed. Canonical build documentation
is unchanged because this is one mode in the same existing target and build flow.
No review finding remains within this bounded change; native context coverage,
loading trust and production/ARM64 runtime acceptance remain open. GUI/Linux,
release matrix, benchmarks and profiling are inapplicable; no release is prepared.

### Context observer VM acceptance

Run immutable 24ea7958 x64 EXE and adjacent DLL from a fresh protected native
Program Files lab directory. Hash both inputs before execution. Observe Session 0
through existing administrative PowerShell Direct, then the logged-on interactive
user through separate demand-only InteractiveToken Limited and Highest tasks.
The parent may use fixed lab PowerShell capture scripts, with no arbitrary input,
to save bounded stdout/exit status. This is temporary test orchestration, not an
installed product launcher. Give that user read/execute access to immutable inputs
and write access only to a separate output directory. Verify actual active user
SID/session immediately before RunEx; no password, S4U or logon fallback.
No MSI, native activation, protected-record writes or token modification is involved.
Keep task execution unlimited/no forced termination, but bound parent observation
to 60 seconds. Timeout preserves task/evidence and stops later cases. Remove only
each exact task after confirmed terminal state. Report local context eligibility,
not human UAC consent, intended-user causality or protected-object access.

### Native context-only observations (2026-10-30)

The parent ran immutable 24ea7958 x64 observer bytes in the disposable VM:
EXE SHA-256 4f42af033b00d0524756f6f9cd3f0aeeba30dbf711dbc60df00f6c43a26a5246;
adjacent DLL 3416a65ab196d89c3d7d4b2b7c9238b9ed509072d5de940fd96eb39fa2b181d4.
Hashes were checked in the guest. A fresh protected Program Files lab held inputs;
the desktop SID had read/execute access and write access only to its output folder.
The initial console-user query returned no user before provisioning, despite an
Explorer session. The parent instead resolved the unique Explorer session/owner SID
through OS queries and rechecked it immediately before each InteractiveToken task.
This selects the test user; it is not MSI initiating-client authentication.

All observations were complete with token/LSA status zero and observer exit 0:
- Session 0 administrative PowerShell Direct: eligible 0, refusal 4, high integrity,
  elevated enabled administrator, token/LSA session 0, logon type 2.
- Task Scheduler InteractiveToken Limited: eligible 0, refusal 3, medium integrity,
  elevation false, administrator deny-only, restricted, token/LSA session 1/type 2.
- Task Scheduler InteractiveToken Highest: eligible 1, refusal 0, high integrity,
  elevated enabled administrator, unrestricted, token/LSA session 1/type 2.

Both demand-only tasks completed with result 0 and no running instances, then were
removed by exact name. Fixed capture scripts/output and observer inputs are retained;
no MSI or full fixture suite was run. Private output is under ignored
build/vm-lab-evidence-20261030/context-observer-*. Eligibility is a local context
observation only, not human UAC consent, intended-user causality/continuity,
protected-object access, preload provenance or actual MSI callback authority.
ARM64 runtime and RemoteInteractive type 10 were not exercised.

### Callback context trace acceptance

The accepted bounded batch is specified in the foreground README under
"Accepted callback context trace batch". Default-OFF context tracing reuses the
reviewed selected-token/LSA observer without protected access or authority. The
original trace schema, MSI sequence and zero-resource contract remain unchanged.
New immutable evidence and independent review precede parent-only VM observation;
Historical option 1 relied on procedural same-user invocation without original-
client attestation or continuity proof; it is superseded by the
[proposed Core controller](#proposed-core-installation-controller-selected-architecture-review-pending). Incomplete/early-failure results are valid
unavailable evidence. Production enrollment remains disabled.

Callback context trace offline evidence is recorded in the foreground README.
Specification 10cd3ac precedes immutable source 751426da89c586ae66ab79b7b7f37fc2ed03e48d.
All six context DLL/MSI pairs compiled/linked with unsuppressed ICE and passed
actual read-only table/Binary/PE/API checks; x64 focused CTest passed 4/4, ARM64
host statics 2/2 without ARM64 execution. Context-OFF historical regressions pass.
Ignored build/trace-context-review-751426d freezes source from Git before evidence
append, with consistent archive/source hashes, artifacts/18 GUIDs and logs/effects.
This is NOT VM-READY; independent review precedes parent-only Success observations.
No MSI actions/protected opens/writes/network/VM or production change occurred.
Incomplete/early failure remains unavailable evidence. Historical option 1 relied
on procedural same-user invocation without original-client attestation or continuity
proof; it is superseded by the [proposed Core controller](#proposed-core-installation-controller-selected-architecture-review-pending).
