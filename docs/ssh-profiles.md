# SSH profiles and interactive authentication

## Status and scope

This is the stage 7 specification and implementation contract. Acceptance criteria below are not
runtime evidence. The baseline is Desktop 33788a8 and reviewed Core
9f527efe0a4959786084675f1ff795edb8a0d910. The target integration version is Core v0.3.0-beta.1 at that exact SHA.
The bundled v0.2.0-beta.1 does not
support SSH. Do not change the bundle pin or signed update gate for this work.

## Profiles

Import ssh://user@host[:port], defaulting the port to 22 and new profiles to
local proxy mode. Recognize SSH, generic Key and explicit Type=ssh without
converting their sources. Provide URI and explicit Server, Port, Username forms
and identity/known_hosts file pickers storing absolute paths only. Explicit
IdentityFile and KnownHostsFile override URI identity/known_hosts parameters.
Core validate remains authoritative. URI passwords are forbidden.

Preserve all stage 2 guarantees: byte-identical no-op saves, protected Raw for
unrepresented settings, atomic replacement, owner/SYSTEM protection and refusal
of imports that relocate relative references. Never copy private keys.

Require an explicit known_hosts reference for GUI-managed control sessions.
Check GUI readability without claiming service-account readability. Core must
reject unknown or changed host keys. No automatic trust, insecure mode or host
key database editor is added. A reachable Core agent is distinct from the GUI
user's agent. Passwords, passphrases and challenge answers are never persisted.

Both proxy and TCP-only VPN modes are supported by the reviewed Core contract.
Captured DNS traverses SSH over TCP; other proxy-routed UDP is dropped. Explicit
direct routes retain their behavior. Do not add proxy auto-reconnection.

## Interactive control

Use the existing protected endpoint and single-request scheduler. Poll
auth_pending approximately once a second during connecting/reconnecting,
without starving status/events or delaying stop/resynchronization. Polling applies
to connecting packet sessions, including adopted sessions whose protocol is not
known to Desktop; Core returns no challenge for other protocols. Use a
nonmodal dialog and continue polling while it is open.

Accept key_passphrase, password and keyboard_interactive only. Require matching
prompts/echo arrays, at most eight prompts and 256 UTF-8 bytes per prompt.
Render prompts as plain text. Mask passwords and passphrases; honor echo for
keyboard-interactive. Send one auth_reply with challenge_id and answers, one
per prompt, at most 1024 UTF-8 bytes each and 16 KiB for the serialized request.

Never retry an ambiguously completed reply. Suppress repeated challenge IDs.
Cancel dismisses without replying; stop cancels through Core. Close and discard
transient answers when the challenge disappears, after two minutes, on failure,
stop, IPC loss or instance/session/backend change. Clear references promptly;
do not claim guaranteed erasure of implicit Qt memory copies.

unknown_command disables interactive polling until instance/backend change,
without declaring authentication successful or blocking noninteractive key use.
invalid_auth_challenge and invalid_auth_response cause safe messages and state
resynchronization. Never log request payloads or untrusted diagnostic text.

## Ownership and integration

Stage 5 owns generic URI dispatch/import integration in ProfileStore::createConfig,
ProfileStore::connectionInfo and MainWindow::addProfile. Stage 7 supplies SSH
branches only. Stage 4 owns Shadowsocks branches. Coordinate source choices,
loadText, buildText and assessRepresentation in ProfileEditorDialog rather than
refactoring the shared editor/model independently.

Stage 7 owns auth lifecycle additions to MainWindow::poll, handleReply,
handleStatus, invalidateConnection and connectionFailure, preserving stage 3.
CoreClient::request already accepts the required arguments and bounds requests;
no new transport or Core API is required. Keep shared regression fixtures.
Stage 7 completes independently on top of stages 1-4; stage 5 integrates SSH
branches afterward. Stage 5 completion is not a prerequisite for stage 7.
Ownership recommendations require coordination before overlapping implementation.
ShadowTLS remains protected Raw; no wrapper editor is added here.

## Acceptance and verification

- Cover URI/default port/IPv6/encoded paths, SSH/Key/File/explicit recognition,
  file-path precedence, byte preservation, Raw fallback and failed writes/imports.
- Cover all challenge kinds, multiple prompts, masking, UTF-8 limits, malformed
  arrays, request size, repeated IDs, expiry, cancel, stop and ambiguous replies.
- Cover old Core, stale callbacks, instance/session/backend changes, IPC loss,
  one in-flight request and UI heartbeat while authentication is open.
- Sentinel answers must not reach profiles, settings, logs, previews or exports.
- Run existing native CTest and four-platform GUI CI builds. Windows ARM64 is
  compile-only; Linux arm64 CI runs natively. Do not build packages.
- Check EN/RU UI at 100/150 percent scaling and minimum window size. Profile
  only if heartbeat or bounded polling checks expose a regression.

Live checks need a separately authorized disposable environment, exact CLI and
runtime identities and a controlled forwarding-enabled SSH server. Test keys,
password and keyboard-interactive authentication, known/unknown/changed host
keys, proxy TCP and VPN TCP/DNS/UDP policy. Compilation and fake IPC tests do
not establish runtime interoperability. No release work is authorized.

Bundle pin, hashes, signed manifests and compatibility gates are a single shared
pre-release integration dependency across stages, outside stage 7 implementation.

Local validation can set `BIMERON_SSH_CORE` to a CLI built from the exact target SHA.
This extends profile-vless-test without changing package contents or build flow.

## Verification (October 29, 2026)

Windows amd64 and Linux amd64 GUI builds and all three native CTest executables
passed. The Windows profile test additionally ran Core validate using an existing
binary whose Go build metadata reports the exact target SHA and vcs.modified=false.
Windows ARM64 and Linux arm64 cross-builds passed; no native ARM64 runtime test
is claimed. The local host has no native ARM64 runner or qemu-aarch64 executable.
No packages were created and no installed service or network configuration changed.

SSH fixtures cover source recognition, default port, no-op editor preservation,
invalid URI passwords/options/ports, local dependency checks, all auth kinds,
echo/masking, plain-text prompts, malformed and oversized prompts, UTF-8 answer
bounds, duplicate/canceled IDs, backend invalidation, responsive UI heartbeat,
private-endpoint reply serialization, one-use scheduling and old-Core rejection.
The existing stage 2/3 regression suites remain enabled. Auth dialog renders in
English/Russian at 100/150 percent were inspected without clipping. The fixture
uses synthetic server prompts (not translated) and does not replace installed
application/editor visual checks. Eight long prompts use a bounded scroll area,
wrap as literal text and expose their full text as input accessible names.

Diff review addressed URI known_hosts precedence (no implicit home-file override),
secret lifetimes, Raw fallback, stop/session/backend cancellation and shared-file
boundaries. No measured regression warranted separate performance profiling.
Password/keyboard-interactive authentication, two-minute server expiry, service-account
readability, VPN traffic policy and native ARM64 execution remain live-check gaps.
They require the separately authorized environment described above.

## Integration with stage 4

The SSH worktree was fast-forwarded to main `5c70f56` (stage 4, PR #12), then
its uncommitted SSH changes were restored. Conflicts in the editor fields,
AccessKey field set, translations and documentation were resolved by retaining
both SS2022/plugin and SSH behavior. Stage 4 tests were retained unchanged.

After integration, Windows amd64 and Linux amd64 passed all three CTest
executables; Windows ARM64 and Linux arm64 cross-builds passed. Windows tests
used both BIMERON_PRESERVATION_CORE and BIMERON_SSH_CORE with the exact target
Core, including SS2022/plugin positive and negative contract fixtures. Linux
SS editor renders in English at 100 percent and Russian at 150 percent retain
plugin controls, masked credentials and fitting buttons, with SSH-only fields
hidden. No polling/data-plane change was introduced by conflict resolution, so
additional benchmarking was not applicable.

Stage 5 is a subsequent integration task, not a blocker for completing stage 7.
Its generic URI dispatch must incorporate these SSH branches without duplicating
them, then repeat common regressions. No ownership agreement with that independent
session is claimed here.

Final local acceptance includes fair auth polling under overdue read work, visible
size-limit errors and a session-owned two-minute expiry timer. The expiry handler
is exercised without waiting two minutes; actual Core expiry remains a live check.

## Authorized live SSH checks (Windows amd64)

The user supplied an existing remote server and existing local key. No server
configuration, users, keys, installed service or system routes were changed.
The server banner identified OpenSSH_8.9p1 Ubuntu-3ubuntu0.17. Server address,
account, local private-key path and key contents are intentionally absent here.

The target Core executable reports revision
`9f527efe0a4959786084675f1ff795edb8a0d910`, vcs.modified=false; its SHA-256 is
`974d29cd7c9c1199766a383814b6b78c9363ee5c50d013535088142c956726ae`.

- Strict OpenSSH key authentication using existing trusted known_hosts passed.
- Target Core `check -config` passed its HTTPS TCP/TLS request through SSH.
- Target Core `run` with a loopback-only SOCKS5 listener passed a curl
  SOCKS5-hostname HTTPS request to example.com; the foreground process was stopped.
- Separate empty and mismatching local known_hosts fixtures both failed with
  `SSH handshake or host-key verification failed`. No trust bypass was used.

Profiles and public host-key fixtures stayed in ignored build state; private
keys were referenced in place and were not copied. These results establish
foreground Core proxy/key/host-verification behavior, not a Desktop-controlled
daemon, interactive server authentication, privileged VPN policy or service ACLs.
Those remaining live checks do not block delivering stage 7 for review; they
remain explicit runtime/release limitations. Stage 5 integration follows stage 7.
