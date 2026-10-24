# Current desktop GUI reference for a SwiftUI port

Status: source-based reference for the current Qt 6 Widgets application. This
document records the interface to reproduce; [the macOS port specification](macos-app-spec.md)
defines platform integration, security, and open product decisions. It does not
claim that the macOS implementation or a visual comparison has been completed.

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
- The selected page fills the remaining area. The default page is Profiles.
  Qt uses Ctrl+1 through Ctrl+4 for page navigation and Ctrl+N to add a profile.
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

`Add key` opens a dialog for `ss://`, `ssconf://`, `socks5://`, or `vless://`
input and a profile name. Import opens a Bimeron `.conf` configuration. Neither
action connects automatically. Profile removal requires the existing
confirmation and active-session behavior; port the observed behavior rather
than treating deletion as a simple list mutation.

## Profile editor

The `Profile settings` dialog currently opens at 680 x 520 logical pixels and
has a 620 x 460 minimum. Its five tabs, in order, are:

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
5. **Raw config:** editable, unwrapped, monospaced configuration text.

Preserve the distinction between visual fields and raw source. Switching tabs
must not silently discard edits or unrepresentable configuration. Validate the
final configuration through core before replacing a saved profile. See
[VLESS profiles](vless-profiles.md) and the macOS specification for format and
storage rules. Qt uses Ctrl+1 through Ctrl+5 for editor tabs.

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
