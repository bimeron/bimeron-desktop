# Bimeron for macOS: port specification for Swift and SwiftUI

Status: Draft specification for product owner review. macOS implementation
GUI has not yet been executed. Date of analysis: October 12, 2026.

## 1. Purpose and boundaries

Create a separate native Bimeron application for macOS on Swift/SwiftUI,
preserving the behavior of the Qt desktop application: management of
profiles, connection, routing editor, settings, log and menu bar
menu. The network logic is performed by the existing `bimeron-core`.

“Same application” in this document means functional continuity.
Exact reproduction of sizes, colors and Qt widgets is not required. Offered
native macOS design with support for light/dark theme and keyboard. This
a proposal rather than an already approved design.

The document distinguishes:

- **Fact** - behavior found in current sources or core documentation.
- **Requirement** - the proposed behavior of the macOS application and the criterion for checking it.
- **Open Decision** is a choice that cannot be reliably derived from the repositories.

This document does not change the supported builds and packages of Qt Desktop. Their canonical
the description remains in [build-and-packaging.md](build-and-packaging.md).

## 2. Basis and sources

Local sources of two repositories were studied:

| Repository | Fixed HEAD |
| --- | --- |
| `bimeron-desktop` | `3d36f6a08d268f7afb83303393052be23983a399` |
| `bimeron-core` | `eaeb74029260e164f47620e0f4b1f09f5f886582` |

There was already a local change in desktop `AGENTS.md`; it is not part of this
specifications. Core had no changes in the working tree at the time of analysis.
The specified revisions are the basis for the analysis and are not recommended versions of the future release.

Main desktop sources:

- [MainWindow.cpp](../src/MainWindow.cpp): screens, connections, settings, menus,
  diagnostics and service maintenance.
- [ProfileEditorDialog.cpp](../src/ProfileEditorDialog.cpp): visual and text editors.
- [ProfileStore.cpp](../src/ProfileStore.cpp): creation, import, storage and identity of profiles.
- [CoreClient.cpp](../src/CoreClient.cpp): local RPC.
- [main.cpp](../src/main.cpp): localization and one GUI instance.
- [ru.json](../assets/translations/ru.json): UI localization resource.

In the `bimeron-core` repository, the agent needs to read the following files regarding
its root; they are not nested here to avoid creating an outdated copy:

| Source core | Destination |
| --- | --- |
| `docs/GUI_INTEGRATION.MD`, `docs/CONTROL.MD` | Daemon RPC contract and state semantics |
| `docs/CONF.MD`, `internal/config/config.go` | Syntax and Validation `.conf` |
| `docs/APPLE_NETWORK_EXTENSION.MD` | Implemented Apple integration and its limitations |
| `docs/APPLE_PAID_DEVELOPER_SPEC.MD` | Signature, system startup and pending checks |
| `apple/Control/main.swift`, `apple/Control/SystemActivation.swift` | Example of control via Apple API |
| `apple/Shared/ProfileStore.swift` | Storing configuration in Keychain |
| `apple/PacketTunnel/PacketTunnelProvider.swift` | Provider and messages `status`/`events` |
| `cmd/bimeron-apple/main_darwin.go` | Real C ABI and limitations of Apple bridge |
| `core/session_info.go`, `internal/control/server.go` | Session IDs and exact JSON fields |
| `docs/MACOS.MD`, `docs/DOCTOR.MD`, `docs/DNS_METADATA.MD` | Additional required sources when implementing relevant functions |

The analysis was performed using the source code, without launching the interface and without network checks on a Mac.
Screenshots and visual reference are not prepared. Information about previously run Mac tests
below are taken from the core documentation and are not meant to be re-tested as part of this work.
When updating the core, you need to check the contract with the new revision. When it diverges
documents, check the implementation and tests, then correct the specification.

## 3. macOS architecture and limits of availability

### 3.1. Two existing control methods

| Property | macOS daemon / native utun | Apple NetworkExtension |
| --- | --- | --- |
| Management | JSON RPC via `/var/run/bimeron/control.sock` | `NETunnelProviderManager` and `NETunnelProviderSession` |
| VPN engine | Privileged daemon | `NEPacketTunnelProvider` with shared Go core via C ABI |
| State | `hello`, `status`, `events`, `stats` | System `NEVPNStatus` and messages to the provider |
| Profile | Absolute path `.conf` | Inline configuration from Keychain with `startTunnel(options:)` |
| SOCKS5 | Supported by core | Current `RLCreate` rejects `Mode=proxy` |
| Kill switch | Not implemented | Not implemented; `KillSwitch=true` rejected |
| Confirmed readiness | Experimental backend; documentation describes real ARM64 tests | Unsigned builds and ABI are checked; signed system launch is still open |

**Suggestion:** select NetworkExtension as the basis for the user application and
reuse existing components from core. Daemon is considered only as
separately selected prototype version. You cannot run both VPNs at the same time;
automatic transition between them in case of an error is prohibited.

Selecting NetworkExtension does not yet mean functional parity: SOCKS5,
automatic recovery after restarting the extension and part of the diagnostics
require separate solutions. These restrictions must remain visible in the plan.

### 3.2. Sharing of Responsibility

- SwiftUI GUI: screens, local profiles, user settings, commands and
  display of confirmed status.
- Apple host integration: OS permissions, VPN registration, Keychain access,
  start/stop and exchange messages with the provider.
- Provider and core: packages, Outline transport, DNS, routing, reconnect,
  network settings and cleaning.

The development of the provider and ABI remains in `bimeron-core`, as provided by its
Apple specification. The new GUI must not contain an independent copy of the network core.
Method for connecting versioned core sources/artifacts and embedding
The provider in the final app bundle must be determined before building the integrated product.
The current `BimeronCore.app` is a technical shell, not a ready-made GUI component.

Don't design a universal layer for two backends in advance. Implement selected
control method; for tests, it is enough to replace the answers at its border.

### 3.3. What cannot be considered ready-made

The core documentation clearly leaves signed activation, real packages open
via `packetFlow`, excluding outgoing Go sockets from the tunnel, DNS compatibility,
system lifecycle, persistent restart and kill switch. The GUI does not eliminate these tasks.

The current system extension does not read the custom Data Protection Keychain.
Host transmits the secret configuration with an explicit `start`. Self-launch
extensions after reboot/crash, without such transfer, are not yet provided. Therefore
You cannot promise Always-on, auto-connection before user login, or reliable startup
system switch without a working host until this problem is solved.

## 4. Screens and user behavior

### 4.1. Main window - “Profiles”

Fact: The Qt application contains sections “Profiles”, “Settings”, “Log”, “About the program”.
The main screen shows the status, server addresses when connected, traffic, list
profiles and actions of adding, importing, deleting and connecting.

Requirements:

1. Keep these four sections accessible; settings and information about the program
   It is permissible to place it in standard macOS windows.
2. Show the selected profile separately from the actually active one. Select the line yourself
   does not connect VPN by itself.
3. Show the name and current profile mode. In Qt the signature "System VPN" is now
   hardcoded for all rows; this is not a portability requirement.
4. Denote activity with text/symbol, and not just color. States:
   disconnected, connected, connected, reconnected, stopped, error.
5. If the list is empty, “Add” and “Import” are available, the connection is not available.
6. Double click or Return launches the selected inactive profile or shift
   profile. Reopening the application shows the existing window.
7. If the backend reports an unknown GUI profile, show “Unknown profile”;
   allow the session to be stopped without necessarily selecting a known profile.

`Session.server_tcp/server_udp` addresses and original server names are shown
only with confirmed `connected`. If the TCP/UDP endpoints are the same, it is enough
one address. This is local interface data, not public diagnostics.

### 4.2. Adding by key

Fact: `ss://` and `ssconf://` are supported. The name is suggested from the URL fragment
`#...` after percent-decoding; a manually changed name is not overwritten.
Saving is available with a non-empty name and a pre-valid key.
The final check is performed by core. Saving does not connect the VPN.

Requirements:

- The user can insert the key manually or as a separate action from the buffer.
  For macOS, it is proposed to read the buffer by explicit action, without background monitoring.
- Do not display the full key in tips, notifications and the log. After closing
  form its secret values are not saved in user settings.
- Support static key formats that core accepts, including base64;
  The UI check should not reject a key valid for core due to simplified heuristics.
- Limit the name to 128 Unicode characters, remove control characters; correct
  Serialize values containing quotes and special characters.
- Do not save the profile in the user directory until successful validation and recording.
  A write error should not make it appear that the profile was successfully added.

The initial parameters of the new profile correspond to Qt: `Version=1`, `Mode=vpn`,
`Name=Bimeron` for interface, `MTU=1420`, `DNS=1.1.1.1`, `IPv6=disable`,
`KillSwitch=false`, `Routing.Default=proxy`. The profile name is specified by the user.
Important: the default value of `IPv6` for the core parser is `enable`; The GUI explicitly records
`disable` when created, rather than changing the semantics of the missing parameter when imported.

### 4.3. Import

Fact: Qt offers `.conf`/`.ssconf` files but expects configuration contents
Bimeron with sections; a file with only a URL does not become `.conf` due to the extension.
The profile is copied to internal storage and verified by CLI `validate`.

Requirements:

- Accept UTF-8 configuration no larger than 64 KiB, with or without BOM, LF/CRLF.
  The full syntax is determined by the core, not the file extension.
- Create import as a separate profile. The error does not leave a record in
  list, damaged profile or extra secret in Keychain.
- `AccessKey.File` resolve relative to the original `.conf` before transfer. For NE
  read the available key file and convert the source to inline form, since
  `LoadConfigData` prohibits `File`. The key file limit is 64 KiB.
- Maintain correct semantics of quotes, escapes, comments and relatives
  ways. Simply searching for the string `File =` is not enough for a full import.
- If there is no access to the key file, show the reason and offer to select it.
  Do not treat the relative path relative to the application directory.
- Take the name from `[Profile] Name`, if not, from the name of the imported file.
  Duplicate names are acceptable: identity is not determined by name.

The user's original file remains in place. Automatic migration of Qt settings
and profiles from another OS are not included in the first transfer; explicit import is used.

### 4.4. Profile editor

Save four logical tabs:

| Tab | Contents |
| --- | --- |
| Basic | Name; key source type; hidden key; server, port, cipher, password and prefix parameters for the corresponding source; mode; SOCKS5 listen, if supported |
| Network | Logical interface name, MTU, DNS, IPv6, kill switch availability |
| Routing | Default Action, Local Network Exception, Ordered Rules |
| Source config | Editable `.conf` monospace font |

In current Qt, the source type is shown as text, the switch is hidden. For source
`SS` parsed server parameters are read-only; for explicit group
they are edited. The password is hidden, the link is shown as a separate action.
The compatible core `Key` parameter must also be preserved when importing/editing,
even if there is no separate visual form for it.

Field restrictions: port `1..65535`, MTU `1280..1500`, from one to four different
unicast IPv4 DNS. Ciphers: `chacha20-ietf-poly1305`, `aes-128-gcm`, `aes-192-gcm`,
`aes-256-gcm`. Prefix restrictions are checked by core. On macOS the interface name is
logical label: real `utunN` selects OS.

Features that the selected backend does not have are disabled with an explanation. Import
`KillSwitch=true` should not silently change it to `false`. For NE `Mode=proxy`
save as an unsupported profile or reject with a clear reason; launch
It's prohibited as a VPN. The choice of storage policy for such profiles is specified in section 12.

Saving checks the entire final config without a network connection. Cancel leaves
the original profile remains unchanged. Validation/Keychain/Disk error retains previous working
version and entered draft for correction. The active profile change is applied
after disconnecting and reconnecting; this is communicated to the user.

In Qt, after manually changing the source text, it becomes the source
conservation; further changes in form do not replace it. Proposed Requirement
for macOS: explicitly indicate the source of the edit. After changing the original
text, update the visual form only after successful parsing, or make it
unavailable with explanation. Losing recent edits due to tab switching is unacceptable.

Parameters and comments that are unknown/unrepresentable on the form cannot be silently deleted.
The minimum permissible is to leave editing of such a profile only in text
mode. A full-fledged editor must take into account the syntax of `docs/CONF.MD`, including
quotes and escapes; do not transfer the simplified Qt parser as a normative grammar.

### 4.5. Routing

- Actions: `proxy` (“Via VPN”), `direct` (“Directly”), `block` (“Block”).
- The rules are checked from top to bottom until the first match. A longer CIDR is not
  gets priority automatically.
- The rule contains an inclusion flag, type `CIDR` or `Domain`, purpose and action.
  Add, delete, move up/down are available.
- CIDR allows a comma-separated list and separate IPv4/IPv6 addresses; single
  addresses are normalized to `/32` and `/128`. Empty elements are rejected.
- Domain includes subdomains, case-insensitive comparison; `*` is not used.
- Qt stores disabled rules as commented blocks `# [Rule]`. Save
  the ability to disable a rule without losing its content and position.
- “Ignore local network” creates `direct` before custom rules
  for `10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 169.254.0.0/16, fc00::/7, fe80::/10`.

Show a brief explanation of the limitations: The domain split of the system VPN depends on
DNS metadata core; direct IPs, old DNS cache and native DoH/DoT applications
can use CIDR/default. Application routing is not supported.
VPN DNS is routed through Outline regardless of `Routing.Default`.

### 4.6. Removal

Deletion requires confirmation with the profile name. Active lifecycle profile
(`connecting`, `connected`, `reconnecting`, `stopping`) cannot be deleted: first
stop the session. Saved `Session` with `active=false` itself should not
permanently disable deletion after stop.

Deleting removes the internal profile, its secret and the auto-connect link;
the original imported file is not affected. The deletion error should be shown,
and the remaining secret/record is taken into account for a second attempt. In Qt, some of the errors are file
transactions are not processed; this is not an example of the required behavior.

## 5. Connection and states

The backend is the source of truth. The GUI can show the command being sent,
but does not declare the VPN connected based on one successful return of `start`.

| State | Presentation and available action |
| --- | --- |
| Status not yet received / backend unavailable | Separate message; no “VPN disabled” statements; re-read status |
| `disconnected` | "Connect" for a valid selected profile |
| `connecting` | “Connection...”; cancel via stop; attempt number is not a percentage |
| `connected` | "Connected"; stop active or change selected profile |
| `reconnecting` | “Reconnecting...”; stop available; server availability is not guaranteed |
| `stopping` | “Disconnection...”; new starts are blocked until completion |
| `failed` | Visible error and safe code; restart only after confirmation of completed cleaning |

Change A → B: remember B's intention, stop A, wait for completion, check B
and run it once. If stop fails, the start of B is aborted. Repeated presses
should not generate multiple parallel start/stops or get lost silently due to busy.

After launch timeout, reread the status: the request could have been accepted. Don't repeat
start blindly. When reopening the GUI, join an already existing session;
do not run the saved profile over it. After control break state
The tunnel remains unknown until the backend confirms.

On NE, take into account two states: system and internal core. `invalid` means
missing/invalid VPN configuration; `disconnecting` corresponds
waiting for a stop; `reasserting` - recovery. Receive system changes
via NetworkExtension notifications. With system `connected` and internal
`reconnecting` show recovery; if core status is unavailable, do not show
outdated addresses and statistics as current. Separately display pending permission,
activation failure and the need to reboot.

## 6. Settings, menu and log

### 6.1. Application Settings

| Setting | Current default | macOS requirement |
| --- | --- | --- |
| Language | Selected from the system locale, with English as the fallback | UI localization is separate; core codes are not translated |
| Disable VPN when leaving | Off | Only an explicit exit, not closing the window; on NE use stop, not daemon shutdown |
| Notifications | Included | Consider OS resolution; failure does not interfere with VPN management |
| Detailed statistics | Off | Switches additional counters |
| Auto-connect when GUI starts | Off | Explicitly selected profile, one attempt after successful initialization in the absence of an active session |
| Save previous journal entries | Included | Do not clear the current log on a new connection; it's not a promise of storage between runs |
| Launch application upon login | Qt only implements Windows | Login item is offered using macOS; disabled by default |

GUI autostart, system component startup, and VPN auto-connection are different actions.
GUI auto-connection does not mean on-demand/restart system extension without host.
If the selected profile has disappeared or Keychain is unavailable, please provide the reason; don't choose
different profile automatically and do not repeat the permission request in an endless loop.

Fact: Changing the Qt language requires a restart; canceling the dialogue returns the previous language.
On macOS it is possible to change without restarting. If a restart is needed, it saves the VPN
and does not enforce the "disconnect on exit" policy as a side effect of changing the language.

### 6.2. Window, exit and menu bar

Closing the window leaves the application in the menu bar and saves the VPN. Restart
or "Open Bimeron" returns the window. Cmd+Q/Quit exits the GUI; default
The VPN continues to work. With output shutdown enabled, wait for the result
stop; if there is an error, show that the stop is not confirmed and let you try again
or explicitly exit with a clear VPN state.

The menu contains status, traffic, a list of profiles marked active, opening a window
and exit. It is proposed to add an explicit "Disconnect" for the active session, including
with an unknown profile. During the shutdown, start commands are blocked.
Reuse `assets/icons/tray/png/macos-template-*.png` as source material
template-icons; Check readability in both themes and Retina.

Keyboard: Cmd+N - add, Cmd+, - settings, Cmd+Q - exit, Cmd+W - close
window, Return—action of the selected profile, Escape—cancel the dialog. All elements
have accessibility labels and are accessible from the keyboard. Color is not the only thing
carrier of the state.

### 6.3. Traffic and log

Main counters - `bytes_up`, `bytes_down`; speed - positive difference
the sum of the counters divided by the actual elapsed monotonic time. First shot
does not give reliable speed. Change session/instance and count down counters
a new reference point, without negative speed or surge.

Additionally: `tcp_total/active/failed`, `udp_total/active/failed`,
`dns_queries/failed`, `routed_proxy/direct/block`. These are core counters, not exact ones
bytes of a network card, encrypted overhead or proof of the effectiveness of protection.

The log contains time, safe events and diagnostic codes. There is cleaning and
explicit export `.log`/`.txt`. Fact: Qt stores the log in window memory; setting
"keep previous" applies to new connections in this run.
It is proposed to preserve this semantics, limit the log to the last 5000 records
and don't add disk storage until a separate solution. Connection notifications,
error and shutdown are sent on a state transition, without duplicates of each poll.

### 6.4. Diagnostics and maintenance

Qt runs `doctor -output <new-file.json>`; read-only operation, existing
the file is not overwritten. The report should not contain keys, dynamic key URLs,
server addresses and profile paths/names. The user explicitly selects the save location.

For NE, an existing doctor of its own utun backend cannot be automatically considered
diagnosing expansion. Define a report with GUI/provider/core versions, status
registrations/permissions and secure codes; mark unavailable checks. Not
add an arbitrary dump of `status.Session` to a public report.

Windows/Linux buttons for installing, starting, updating and deleting services cannot be transferred
literally. NE requires extension activation and VPN registration states, explained
macOS requests and retry after failure. Deleting registration is allowed
after stopping and only affects the Bimeron configuration.

`recover` is not in the RPC and is a legacy backend administrative operation.
Do not run it automatically on any error or apply it to NE without a separate
contract. A safe NE recovery path must be defined in core.

## 7. Storage and secrets

Fact: Qt stores index `profiles.json` with `name/path`, files `.conf` in data directory
applications, file rights only to the owner. ID is calculated as SHA256 normalized
absolute path; on Windows, the path is additionally converted to lowercase.
This is a path ID, not a content ID. Core does not expose symbolic links.

NE offer:

- List of profiles - own stable UUID and unclassified metadata in
  Application Support; UI settings - UserDefaults.
- Full inline configuration with a key - Data Protection Keychain. In VPN preferences
  only unclassified `profile_id` and service parameters.
- UUID GUI/Keychain, `Session.id` and daemon `Session.profile_id` are different entities.
  With `LoadConfigData` there may be no file ID. Active NE profile link
  with registered `providerConfiguration.profile_id`, do not calculate from name
  and do not pass off the SHA256 UUID as a daemon identity.
- Profile changes should consistently update the Keychain and index with a rollback when
  error. The existing Swift `ProfileStore.write` can add via `SecItemAdd`,
  but by itself does not implement updating, profile directory and full processing
  deletion errors. This is a GUI integration job, not an out-of-the-box storage capability.
- Multiple local profiles do not require multiple system VPN configurations.
  One Bimeron-owned configuration is offered, updated after stop.
- For the active profile, save the semantics “changes will be applied the next time”
  connection" without changing the configuration of a running provider.

Keys and passwords are prohibited in UserDefaults, process arguments, log, analytics,
diagnostics, fixtures and Git. Sample profiles contain only synthetic data.
Keychain, validation, and VPN management operations do not block SwiftUI's main thread.
General Keychain and sandbox rights must match the selected host/provider
bundle; current values ​​of the technical shell cannot be transferred without verification.

## 8. Management contracts

### 8.1. Leading Candidate: NetworkExtension

The GUI calls the Apple API directly, using the core technical wrapper as a reference.
Parsing human-readable stdout `BimeronControl` is not a proposed API.

| Operation | Implemented mechanism / limitation |
| --- | --- |
| Activate system extension | `OSSystemExtensionManager` and result `OSSystemExtensionRequest` |
| Get/Save Registration | `NETunnelProviderManager.loadAllFromPreferences` / `saveToPreferences`; select by provider bundle ID |
| Run | `NETunnelProviderSession.startTunnel(options: ["profile_data": NSData])`; data from Keychain |
| Stop | `stopVPNTunnel()`; return does not confirm completed cleaning |
| Get core state | `sendProviderMessage` with UTF-8 `status`; JSON `{ "status": ..., "stats": ... }` |
| Get events | Message UTF-8 `events`; JSON array of up to eight events retrieved from the queue |
| Built-in core version | `RLVersion()`; does not prove the version of the old provider actually working after the update |

This channel **is not daemon RPC**: no shell `version/id/instance`, no
`after/cursor/dropped`, no messages `start/stop/hello/capabilities/doctor`.
`events` has best effort semantics; don't run competing event readers
from different parts of the GUI. Restore the state from `status`, not from the completeness of the log.
For an unsupported message, provider returns no data.

It is required to limit the wait for provider messages, handle nil, damaged JSON
and ending the session during the request. Suggested reading timeout is 5 seconds,
status/statistics period when UI is open - about 1 second, without overlapping
requests. After sleep, do a new reading. Interface timeout should not occur without permission
interrupt network cleaning or initiate a new session.

The provider now waits for `connected` core up to 90 seconds at start. `RLStop` waits until
60 seconds and returns an error with the handle saved if the cleanup is not completed.
In the current Swift `abort` the result of `RLStop` is ignored; completion `stopTunnel`
therefore cannot be considered verified evidence of successful cleaning. Processing
of this case and understandable provider errors relate to core integration tasks.

`RLCreate` checks the config without starting the network and returns handle or 0. Null
the result does not contain a detailed error code. Editor requires verified
way of secure detailed validation: existing CLI `validate` taking into account sandbox
or a separate contract core. Don't reinvent the missing C ABI function.
When using `RLCreate` for verification, release handle and take into account `RLStop`.

NE capabilities cannot be taken from daemon `PlatformCapabilities()` without adjusting for
bridge restrictions. In the current bridge: only VPN, without kill switch; IP/domain routing
implemented in the general core, but require real NE checks. Need explicit consistency
set of features of the integrated version; unknown feature is not enabled
optimistically. The provider API extension is implemented as a separate change in core.

### 8.2. Alternative: daemon RPC v1

This section is only needed if daemon is selected. Regular user GUI
connects to `/var/run/bimeron/control.sock`; socket `0600`, server checks
UID. Daemon launched via sudo uses `SUDO_UID`; direct root launch without
the desired owner does not give regular GUI access. Installing launchd in the current core
not implemented. LaunchDaemon/SMAppService-helper - separate job, not available
ready function. SwiftUI GUI itself should not be run as root.

One request and one response per connection: UTF-8 JSON + LF. Request maximum 16 KiB
including LF, the answer is to limit it to 256 KiB. Read the stream up to LF, take partial reads into account.
Send only known fields, check `version=1`, match between `id` and `error`;
Ignore unknown response fields. ID up to 64 bytes, no deduplication mechanism.

```json
{"version":1,"id":"mac-1","command":"hello"}
{"version":1,"id":"mac-2","command":"start","config":"/absolute/profile.conf"}
{"version":1,"id":"mac-3","command":"events","after":0}
{"version":1,"id":"mac-4","command":"stop"}
```

Teams: `hello`, `version`, `capabilities`, `status`, `start`, `events`, `stats`,
`stop`, `shutdown`. `config` is applicable only to `start`, `after` is applicable to `events`.
`stop` leaves the daemon running; `shutdown` terminates it after cleaning.

JSON register saved: `status.State/Attempt/Since/LastError/Session`,
`event.Time/State/Code/Message/Attempt/LastError`; shell and statistics fields -
snake_case. RFC3339 time can contain a fractional part and a time zone.

When starting GUI: `hello`, get capabilities/instance, read authoritative
state. When changing `instance`, reset the cursor and statistical database, update
hello/status/stats. For `events` store UInt64 sequence/cursor; story contains
128 latest events. At `dropped=true`, re-read status. At `invalid_cursor`
check restart and reset after=0. Do not define logic from the text `Message`.

Timeout reference: reading 5 seconds, stop/shutdown up to 120 seconds; I/O outside the UI thread.
CoreClient Qt keeps one request and silently ignores a new one when busy - this is technical
a limitation of the current client, not the desired semantics of a Swift client.

Errors that the GUI should distinguish: `unsupported_protocol`, `invalid_request`,
`unknown_command`, `invalid_cursor`, `absolute_config_path_required`,
`invalid_configuration`, `session_already_running`, `session_start_failed`,
`session_stop_failed`, `daemon_shutting_down`. Unknown code show how
general error with the code. After cleanup failure, do not perform automatic start/recover.

## 9. Known differences and parity limits

| Function desktop | Carry Requirement/Open Gap |
| --- | --- |
| Multiple local profiles | Necessarily; technical Apple shell now installs one selected profile |
| System VPN | Main scenario; required real signed NE lifecycle |
| Local SOCKS5 | Available in Qt/core daemon, not in Apple bridge; solution to the promise of full parity |
| Kill switch | Not available on macOS; do not simulate an on switch |
| SCM/systemd service management | Replaced by macOS activation/enrollment when selecting NE |
| Closing the GUI while saving the VPN | Must check on a real Mac |
| Auto-connect when GUI starts | It is suggested to keep; not equal to automatic restart system extension |
| Doctor and disaster recovery | An NE contract is still required; legacy commands are not a ready-made replacement |
| Text and visual editors | Mandatory, preventing the loss of unrepresentable parameters |
| Localization, log, statistics, menu | Preserve with native macOS behavior |

Not included unless specifically requested: iOS, new VPN protocol, routing by
applications, cloud synchronization, subscriptions/accounts, automatic updates
applications, Always-on and publication in the Mac App Store. This does not eliminate the need
Decide which existing desktop features can be deferred in the first macOS release.

## 10. Acceptance criteria

All points below are **checks of future implementation that have not yet been completed**, and not a report
about past tests. Use synthetic configs in automated tests.

| ID | Script | Verified result |
| --- | --- | --- |
| P01 | Add valid `ss://` / `ssconf://` | The name and defaults are correct, the profile is saved, the VPN does not start |
| P02 | Invalid key / entry error | Clear error, no ghost entry and no residual secret |
| P03 | Import with BOM/CRLF, Unicode, quotes, relative `File` | The semantics correspond to core; file resolved from original `.conf` |
| P04 | Profile/key larger than 64 KiB, no access to File | Import rejected, UI remains responsive |
| P05 | Save form/source text, validation error, cancel | Previous version saved on error/cancellation; edits are not lost between tabs |
| P06 | Rules, order, disabled blocks, local network | The resulting policy matches the expected first match |
| P07 | Import `Key`, unsupported field, `KillSwitch=true`, `Mode=proxy` | No silent deletion of parameters or weakening of the profile; restrictions explained |
| P08 | Delete active/paused/auto-connect profile | Active is protected; stopped is deleted; autolink cleared; source file not deleted |
| C01 | Startup, long connection, server error | No false connected; shows actual status and safe error |
| C02 | Cancel during start/reconnect | One stop, completion confirmed; no subsequent spontaneous start |
| C03 | Change A → B, including stop error | B starts only after successful completion of A |
| C04 | Quick repeat commands, timeout start | No parallel sessions; state is reread before restart |
| C05 | Restarting GUI with VPN active | The GUI joins, recognizes the profile or shows an unknown one; stop available |
| C06 | Loss of control during a running session | "Backend unavailable", without stating that VPN is disabled |
| C07 | New session, reset counters, sleep | No negative/false speed and old server addresses |
| N01 | Permission, refusal, waiting for confirmation, reboot activation | Various understandable states; retry after user action |
| N02 | NE status/events: nil, corrupted response, timeout, break | UI does not freeze, there is no fictitious RPC cursor, the state is restored |
| N03 | Inaccessible Keychain / Disappeared Profile | No fallback to someone else's profile and no entry of the key in preferences |
| N04 | Stopping with error core/provider | Successful cleanup is not reported; repeated start is blocked until the error is resolved |
| D01 | Daemon only: version/id mismatch, fractional read, large response | Incompatible/corrupt response rejected without hanging |
| D02 | Daemon only: change instance, dropped, invalid_cursor | Cursor reset if necessary, status reread, statistics synchronized |
| U01 | Closing the window, relaunching, Cmd+Q with both settings | Window/GUI/VPN is completed according to Section 6 |
| U02 | Auto-connection on/off, already active session | Exactly the required attempt; existing session is not replaced |
| U03 | Menu, localization, dark theme, keyboard, VoiceOver | All basic actions are available, the status is clear without color |
| U04 | Log/Export, Notification Failure, Diagnostics | There are no secrets, the UI works when the OS fails, the report does not produce unverified data |
| I01 | Signed provider: TCP/UDP, DNS, IPv4/IPv6, split | Real traffic matches the profile, outgoing sockets are not looped |
| I02 | Wi-Fi/Ethernet, network loss, sleep/wake, provider crash | Defined state and verified recovery/error, no stuck handles |
| I03 | Clean installation, update, uninstall | User data is saved according to the adopted policy; other people's VPNs are not affected |

For NE, paragraphs D01–D02 do not apply; for daemon, N01–N04 in their NE formulation.
This is a choice of option, not a reason to skip the equivalent error checks of the chosen one
backend Integration criteria are specified according to the Apple core specification.

## 11. Implementation and verification procedure

1. Approve the decisions from section 12 and update the status of the document. Commit
minimal size of the first version and explicitly deferred desktop functions.
2. Connect the existing core/provider of the agreed version; on Mac confirm
short end-to-end registration script, start, status, stop. UI in parallel is possible
develop with dummy states, but this is not proof of VPN integration.
3. Implement directory/Keychain, add/import and main screen with lifecycle.
Then editor, settings, menu bar, log and diagnostics.
4. Check each stage with focused Swift tests of format, storage, transitions
status and errors. Check the validity of the exported configs with the real core.
5. Perform macOS build and tests via Xcode/`xcodebuild`, ABI and integration checks
from core. Write down specific project/scheme/commands after creating the project;
there are none now, so the document does not contain fictitious GUI build commands.
6. Go through real network and system acceptance scenarios. Check GUI versions,
built-in and running core/provider; check the old provider separately
after updating the bundle and reactivating it.
7. Check diff for regression, working with secrets, correctness of errors and unnecessary
complexity. Update documentation and prepare a release only for a specific task.

Profiling is needed when the packet path changes, polling or detected load:
measure CPU/GUI memory at rest, with window open/closed and VPN active, latency
UI response and memory stability during repeated start/stop. Numerical promises
establish performance based on the measured baseline, and not invent it now.

Product Matrix: Apple Silicon and, if approved, Intel; selected minimum and
current macOS. Amd64 compilation does not replace network tests on Intel. Changes
of a common core additionally require its relevant cross-platform CI checks.
The documentation stage does not require building MSI/DEB: their code and flow do not change.

Direct distribution will require a suitable Apple Developer team, permanent
bundle ID, signing/provisioning, verification of nested signatures, notarization and
Gatekeeper on a clean Mac. The current status of the account/certificates in this work is not
checked. Selecting the final packaging and publishing remains a separate release task.

## 12. Open solutions before implementation

| Solution | Suggestion/why choice is required |
| --- | --- |
| Backend first version | NetworkExtension; daemon only if you deliberately need an experimental prototype |
| Completeness of parity and SOCKS5 | Record whether SOCKS5 is required in the first release; the current NE bridge does not implement it |
| Unsupported profiles | It is proposed to save for editing with a launch ban; do not change parameters automatically |
| Minimal macOS and Intel | Select supported OS/architectures; verified SDK core is not deployment target GUI |
| Distribution | Direct distribution of system extension is offered; App Store - a separate solution |
| Identifiers and core integration | Permanent host/provider bundle ID, Keychain group, version and method of delivery of components |
| Autorecovery without GUI | For the first version, it is proposed not to promise until the completion of the secret storage model system extension |
| NE Validation and Diagnostics | Approve available core contract, including detailed errors and running provider version |
| Design | A functionally similar native SwiftUI is offered; the accuracy of visual copying must be confirmed separately |

Before making these decisions, you can use the document for assessment and planning.
They should not be silently assumed to be selected just because a recommendation is listed here.

## 13. Transfer packet to the next agent

Submit this specification, both repositories with committed revisions, instructions
`AGENTS.md` of each project and the decisions taken from section 12. For visual parity
add anonymized screenshots of the main screens and states, if required.
Do not transmit real keys, profiles and signing secrets as part of the job.

Statement of the task for the next stage:

> Develop a separate macOS GUI Bimeron on Swift/SwiftUI according to the approved
>`macos-app-spec.md`. First, check that the contracts of both repositories are up to date
> and the selected backend. Reuse Apple core integration in a consistent way.
> Save acceptance criteria identifiers in the plan and report. Unsupported features
> and do not pass off incomplete system checks as completed. Requirement changes
> reflect in the specification before implementation. Perform the release only as a separate task.
