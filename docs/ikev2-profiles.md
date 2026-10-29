# Experimental native IKEv2 Desktop integration

## Status and implementation gate

This is the accepted stage 8 specification, not implemented support or runtime
evidence. Desktop baseline: `82943b87a36acfe6ab041902ea02bad917579691`.
Core baseline: `v0.3.0-beta.1`, revision
`9f527efe0a4959786084675f1ff795edb8a0d910`. The annotated tag resolves to that
commit. At planning time its release-by-tag API returned 404. The actual Desktop
bundle remains `v0.2.0-beta.1`; it does not establish native IKEv2 support.

Full GUI implementation is blocked on the Core foreground API prerequisite
below and review of a new, explicitly agreed immutable Core SHA. The baseline
CLI runSession owns an Engine and prints text events; it does not expose the
required foreground control API. Do not parse those logs or use daemon start.

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
