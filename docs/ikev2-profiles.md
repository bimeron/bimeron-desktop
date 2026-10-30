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

The user additionally accepted an already-elevated, same-user interactive MSI
client for native enrollment and every modification of an enrolled installation.
Without independently established effective-token/session evidence, refuse before
mutation and request elevated installation. Per-machine server elevation alone
does not satisfy this condition. Ordinary nonenrolled packet installation and its
existing update/rollback retain their behavior. This authorizes only an offline
prototype, not enrollment or a protected installation experiment.

The prototype must constrain its own approved package: old-product removal after
InstallExecute and before InstallFinalize, all destructive/rollback/commit work
inside that transaction, and no later resource mutation. Post-Finalize settlement
is a separate protected journal acknowledgment, not rollback-covered resource
work or proof that the caller received a successful MSI return. Effective token,
same-user/session, approved effective package, exact generation, final inventory
and quiescence must be reverified before durable clean publication. Unsupported
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
