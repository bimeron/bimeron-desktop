# Experimental native IKEv2 Desktop integration

## Status and implementation gate

This is the accepted stage 8 specification, not implemented support or runtime
evidence. Desktop baseline: `82943b87a36acfe6ab041902ea02bad917579691`.
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
