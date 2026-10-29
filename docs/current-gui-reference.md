# Current desktop GUI reference for a SwiftUI port

Status: source-based reference for the current Qt 6 Widgets application. This
document records the interface to reproduce; [the macOS port specification](macos-app-spec.md)
defines platform integration, security, and open product decisions. It does not
claim that the macOS implementation or a visual comparison has been completed.

## Fixed baseline and reconciliation

Source baseline: Desktop `7f44c7309fc4cf9e91554f6bed8f167cefcda35d`.
This revision reconciles the reference from `67658368c2342ec95547cd0f0579a9d5aca28128`
on `docs/swiftui-gui-parity`, retaining its screen hierarchy and review matrix.
All relative source links refer to this baseline, not a moving runtime claim.
Detailed source-derived geometry, tokens and SwiftUI mappings are in
[GUI design implementation reference](swiftui-gui-design.md).

Desktop import, representable forms, selected CLI validation and active runtime
support are separate axes. The actual bundle remains Core **v0.2.0-beta.1**;
the stages 1–7 target contract is **v0.3.0-beta.1**, Core revision
`9f527efe0a4959786084675f1ff795edb8a0d910`. New forms do not upgrade the service.
Stage 8 IKEv2 is unimplemented: external uncommitted Windows drafts are future
work, not macOS parity. Candidate `62438a9` was rejected after lifecycle race
audit and is not approved or ready.

## Parity target and evidence

Reproduce the current information hierarchy, actions, text, and observable
states in a native SwiftUI application. Use macOS controls, menu conventions,
accessibility, and system dialogs where appropriate. Exact Qt pixel rendering
is not a requirement. The implementation in `src/MainWindow.cpp` and
`src/ProfileEditorDialog.cpp` is the authority when this reference becomes
stale. Related behavior specifications in `docs/` remain authoritative for
their individual changes.

This reference was checked against source, without running the GUI or capturing
screenshots. Visual dimensions, colors, clipping, focus appearance, and dynamic
states still need verification against a running build before visual parity can
be accepted. Use synthetic profiles and redact keys and server addresses in
any reference capture.

## Window and navigation

- The main window has a 720 x 600 logical-pixel minimum. A fixed 150-pixel left
  sidebar contains the Bimeron brand and navigation in this order: Profiles,
  Settings, Log, About. A session-only Development page appears after explicit
  activation; it is not part of the normal navigation.
- The selected page fills the remaining area. The default page is Profiles (Development is initially selected in an explicitly activated development session).
  Qt uses Ctrl+1, Ctrl+comma, Ctrl+3 and Ctrl+4 for page navigation and Ctrl+N to add a profile.
  The macOS equivalent should use standard Command shortcuts where useful.
- The normal window can close to the system tray while the connection persists.
  The macOS counterpart is the menu bar. Exiting is a separate action.

## Profiles page

Top to bottom: page title; connection state; traffic; optional detailed session
statistics; a scrolling profile list; bottom actions. The bottom row places
`+ Add key`, `Import .conf`, and `Remove` on the left and the primary
`Connect`/`Disconnect` action on the right. A selected row exposes an edit
control. Selection and the active connection are distinct: changing selection
does not start a connection. Double-clicking a connectable row starts it or
switches from another active profile; Return activates the selected profile,
and Delete removes it. Unknown or unsupported profiles cannot be started.

Profile rows show the profile identity and mode, with selected, active, and
unavailable states. The connection control and status must reflect the core's
confirmed state, including connecting, connected, reconnecting, stopping,
disconnected, failed, and unavailable. Preserve the visible difference between
the selected row and the active profile. Traffic and optional counters update
while the application runs. See [profile list](profile-list.md),
[tray interaction](tray-interaction.md), and
[access-key failure](access-key-failure.md) for edge cases.

`Add key` opens a fixed-width 440 dialog for ss, ssconf, socks5, vless,
hysteria2/hy2, vmess, trojan, tuic, http/https, anytls, naive+https,
naive+quic, masque and ssh URIs. Input is visible (no reveal toggle); a
question-mark tooltip lists schemes. Name appears above the URI only after a
recognized URI; decoded fragment suggests a name without replacing a manual
name. Paste is hidden for unsupported clipboard content. SSH additionally shows
identity and known_hosts file pickers. Save requires recognized input, name and
SSH dependency checks; selected Core still validates. Errors keep the draft open.
Height follows visible rows; width stays fixed. Import opens a Bimeron `.conf` configuration. Neither
action connects automatically. Profile removal requires the existing
confirmation and active-session behavior; port the observed behavior rather
than treating deletion as a simple list mutation.

## Profile editor

The `Profile settings` dialog currently opens at 680 x 520 logical pixels and
has a 620 x 460 minimum. Its logical tabs, in order (conditional tabs are hidden), are:

1. **General:** name; key source and key or path with reveal and browse;
   server, port, cipher, username, password, prefix; VPN or local SOCKS5 mode;
   SOCKS5 listen address. Available sources include SS, dynamic SSConf, remote
   SOCKS5, VLESS, legacy key/file when present, and explicit settings.
2. **VLESS:** endpoint and encryption; user ID; transport and security; flow,
   SNI, fingerprint, REALITY fields, ALPN, path, host, WebSocket early data,
   gRPC fields, and XHTTP mode/extra. Visibility and editability depend on the
   selected key source and supported combination.
3. **Network:** interface name, MTU, DNS, IPv6, and kill switch. Unsupported
   capabilities appear disabled with an explanation.
4. **Routing:** default action; local-network bypass; ordered rules with On,
   Type, Destination, and Action columns; Add, Remove, Move up, Move down.
5. **Parameters / ShadowTLS:** scrollable conditional protocol fields and separate
   optional ShadowTLS v3 credentials; see the support matrix below.
6. **Raw config:** credential warning, Show/Edit raw config / Hide raw config,
   Validate and use visual editor, initially hidden unwrapped monospaced text.
   Unknown/unrepresentable configurations stay protected Raw; no discard-unknown
   confirmation exists.

Preserve the distinction between visual fields and raw source. Switching tabs
must not silently discard edits or unrepresentable configuration. Validate the
final configuration through core before replacing a saved profile. See
[VLESS profiles](vless-profiles.md) and the macOS specification for format and
storage rules. Qt creates Ctrl+1 through Ctrl+5 shortcuts before inserting Parameters / ShadowTLS; do not assume a sixth shortcut exists. Preserve logical tab commands on macOS and test conditional visibility.

## Settings, log, About, and menu

Settings begins with an Application card: Theme (System, Light, Dark), Language
(English, Русский), disconnect/stop on quit, notifications, detailed statistics,
automatic connection and its profile selector, and launch at login. The Qt
theme and language selectors ask for a restart and revert on Cancel. Below it,
Windows/Linux show a Bimeron service card with status-dependent Install,
Start, Stop, Remove, signed Core update, beta-update opt-in, system-boot, and
emergency recovery controls. Its footer offers diagnostic export and Quit.
The macOS service or extension controls need their own backend contract; do not
copy SCM/systemd actions as decorative controls. See [service actions](service-actions.md)
and [core updates](core-updates.md).

Log has a `Keep previous entries` checkbox, read-only log view, Save log, and
Clear. About presents Desktop and Core version information. The tray menu
shows status, traffic, profiles with the active one identified, Open Bimeron,
and Quit. The menu-bar port should retain these tasks and provide a clear way
to disconnect an active session, including one whose profile is unknown.

## Appearance and state matrix for review

The Qt implementation uses a light/dark palette with custom styled navigation,
cards, buttons, list rows, and focus/disabled states. Existing focused specs
cover [theme](windows-dark-theme.md), [profile-list HiDPI](profile-list-hidpi.md),
[connection button HiDPI](connection-button-hidpi.md), and other controls.
Before calling the SwiftUI port visually close, compare running Qt and macOS
builds at a fixed logical window size and in both themes. Capture and review:

| Surface | Required representative states |
| --- | --- |
| Profiles | Empty list; multiple profiles; selection distinct from active; connecting; connected; reconnecting; failed; unavailable; long names and Russian text |
| Add/import | Each supported key type; invalid input; canceled import; validation error |
| Editor | All five tabs; SS, SSConf, SOCKS5, VLESS, explicit and raw sources; disabled capability; unsaved edits |
| Settings | Default values; auto-connect off/on; service or extension unavailable, stopped, running, busy and failed; update prompt |
| Log/About/menu | Empty and populated log; export; version information; disconnected and active menu |
| Presentation | Light/dark; minimum window; keyboard focus; high DPI/Retina; accessibility labels and status without reliance on color |

Acceptance: each action has the same user-visible result or a documented macOS
substitution; no profile or secret is lost on a canceled or failed edit; backend
state is never inferred solely from a button press; unsupported features are
explained; and every row of the matrix has an inspected reference and port
capture. Record intentional differences and the exact Qt/Core revisions used
for comparison. This matrix is a future verification plan, not a completed test.


## Current protocol and editing support matrix

Authority: [ProfileEditorDialog.cpp](../src/ProfileEditorDialog.cpp)
(`loadText`, `buildText`, `assessRepresentation`, `refreshParameters`) and
[ProfileStore.cpp](../src/ProfileStore.cpp) (`createConfig`, `connectionInfo`).

| Source | Import/form behavior | Boundary |
| --- | --- | --- |
| SS, SSConf, SOCKS5, VLESS, Hysteria2/hy2 | Dedicated source recognition; retain opaque URI rather than regenerating decoded fields | Selected CLI validates; existing source is preserved |
| Explicit SS | Seven cipher choices, masked credentials, Prefix, plugin selector and masked SIP003 options | SS2022 AES SIP023 identity chains preserved; ChaCha20 single key; SS2022 rejects Prefix |
| SS embedded plugins | None, v2ray-plugin, obfs-local | No external executable; unknown options/plugins fall back to protected Raw; plugins exclude ShadowTLS |
| SSH | URI/explicit endpoint, username, identity and known_hosts pickers | No URI password; strict host verification; no trust bypass or private-key copying |
| VMess, Trojan, TUIC, HTTP(S), AnyTLS, Naive, MASQUE URI | Dedicated opaque AccessKey sources, protected Raw | Import recognition is not explicit form support |
| Explicit Trojan | Endpoint/password, optional SNI, TCP/WS/gRPC TLS, ALPN, WS path/host, gRPC service/authority/multimode | Inactive populated fields remain visible and block incompatible save until cleared |
| Explicit HTTP/HTTPS | Endpoint and optional paired credentials | No SNI/CA override |
| Explicit Naive H2 | Endpoint, paired credentials, optional SNI | Naive H3 stays URI/Raw |
| Explicit MASQUE | Endpoint, optional paired credentials, SNI, CAFile, MASQUEPath | Picked CA path absolute; existing relative reference keeps original base |
| ShadowTLS v3 | Independent handshake name and masked outer password for represented explicit SS/VLESS/Trojan/Naive H2/SSH | Excludes AES-192-GCM, SS plugins, SOCKS5, HTTP(S), H3 and IKEv2; never silently delete wrapper on inner edit |
| VLESS | Endpoint, encryption, user ID, transport/security, flow, SNI/fingerprint, REALITY, ALPN, path/host, WS early data, gRPC and XHTTP fields | Vision TCP REALITY only; WS TLS + HTTP/1.1; gRPC TLS/REALITY + H2; XHTTP H3 TLS without fingerprint or ShadowTLS; extended/split-download settings protected Raw |

See [SS](shadowsocks-profiles.md), [URI import](protocol-uri-import.md),
[SSH](ssh-profiles.md), [parameter forms](parameter-forms-and-shadowtls.md),
[VLESS](vless-profiles.md), and [Hysteria2](hysteria2-profiles.md).
Stage 6 narrows the earlier SS/SSH documents' blanket ShadowTLS Raw-only wording
for represented compatible explicit configurations only.

Opening, canceling and unchanged save preserve original bytes, BOM/CRLF,
comments and source. Unknown fields, duplicates, mixed sources and values that
cannot round-trip prohibit visual rewriting. Raw-to-form requires validation
and representation success; failed validation retains Raw and draft. Writes
are atomic and protected before credentials are written. Relative-reference
relocation is refused; dependencies are never copied. Masked previews must never
be serialized. See [preservation](profile-preservation.md) for the storage and
redacted diagnostic contract.

## Authoritative control and transient authentication

Port [connection management](connection-management.md), not optimistic local
button state. Startup hello/status must complete before control is enabled.
Unavailable/malformed IPC is unknown, never disconnected. Button text is Please
wait… during operations, Disconnecting… during stop, Disconnect for an active
session unless switching selection, otherwise Connect. Enablement additionally
requires hello, synchronized status, no blocked cleanup/backend/admin operation,
no native session, and a connectable selection (or an active session to stop).
An accepted start is not connected. Unknown active profiles can still disconnect.
A switch waits for confirmed clean stop; timeout cancels queued start. Never
retry ambiguous writes, recover automatically, or install a service as fallback.
Instance changes invalidate pending intentions, auth and measurements. Speed
uses monotonic deltas; unavailable/stale samples are not zero. Existing active
sessions are adopted. Auto-connect attempts once only after clean initial sync.

SSH authentication is a nonmodal dialog while polling continues: plain-text
server prompts, password/passphrase masked, keyboard-interactive echo honored.
Only matching bounded challenges (up to eight prompts) receive one bounded reply;
Cancel dismisses without replying. Clear answers on expiry (two minutes), stop,
failure, IPC loss and session/instance/backend change. Repeated challenge IDs
are suppressed, ambiguous replies never repeated. Unknown/changed host keys
fail; GUI readability does not prove service-account readability. Answers never
enter profiles, settings, logs or exports. No guaranteed memory erasure claim.

## Settings, development and presentation details

Application defaults: System theme, system-derived EN/RU language, stop Core on
quit off, notifications on, detailed statistics off, auto-connect off. Profile
selector is enabled by auto-connect. Launch-at-login follows actual OS state
(Windows Run entry / Linux desktop entry), independently of service boot and
VPN auto-connect. Theme/language Restart commits the change; Cancel restores
previous value including absence of an override. Other toggles persist directly.
Notifications distinguish VPN/proxy connected, failed and disconnected.

Service cards are platform/backend dependent; controls suspend during local Core
use, backend failure/transition and service/update operations. Signed update
consent/trust checks and beta opt-in must remain distinct from CLI selection.
Only successful helper completion plus expected-version hello and clean status
permits a single previous-profile restore. See [updates](core-updates.md) and
[service actions](service-actions.md); macOS requires a real extension/backend
contract, not decorative Windows/Linux administrative buttons.

About shows Desktop version and runtime Core identity (unknown until learned).
Development is session-only, activated by five left-button releases on the About Desktop version label and
a restart into development mode; see [developer mode](developer-mode.md). Its page has System
service / Local core executable, remembered absolute path, Browse, file modified
time, Apply, plain-text status and conditional Stop remaining local daemon.
Switch requires disconnect/elevation consent, blocks competing admin work,
invalidates old state and never auto-connects. Failure on quit offers Stay
(default), Close GUI only, and conditional separately warned Force stop Core
and close. Closing to tray retains development ownership; quitting cleans up
owned processes under its separate policy.

English strings are `tr()` source text, Russian equivalents are in
[ru.json](../assets/translations/ru.json); `JsonTranslator` in
[main.cpp](../src/main.cpp) falls back to English for absent entries. Preserve
placeholder substitution, literal server prompts and protocol identifiers.
Examples: Profiles / Профили, Settings / Настройки, General / Основное.
Do not translate arbitrary Core output: allowlisted safe codes/templates only.
Key unavailable has a replacement/reconnect warning; validation/write errors
retain drafts, export failures leave existing reports untouched. Verify every
conditional field, error and destructive confirmation in both languages.
