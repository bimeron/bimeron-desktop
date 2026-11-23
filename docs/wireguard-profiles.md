# WireGuard and AmneziaWG profiles

Tracking: [Desktop #74](https://github.com/bimeron/bimeron-desktop/issues/74). Core work:
[bimeron-core#202](https://github.com/bimeron/bimeron-core/issues/202); Core design: ADR 0043
(`docs/adr/0043-wireguard-amneziawg-client.md` in bimeron-core).

Desktop imports, stores and edits WireGuard (WG) and AmneziaWG (AWG 1.0, 1.5, 2.0, 3.0 and
3.1) profiles on Windows and Linux. Core implements the protocol and is the only parser and
validator: Desktop never interprets WG/AWG syntax. It uses the Core `bimeron wireguard`
document bridge (Core `docs/WIREGUARD.MD`, section "Desktop document bridge") and the
existing `bimeron validate --json`. WG/AWG support in Core is experimental.

## Core boundary

`bimeron wireguard` follows the `bimeron document` framing: one JSON request on stdin, one
JSON response on stdout, no arguments, files, network or DNS. Desktop uses these operations:

| Operation | Desktop use |
| --- | --- |
| `capabilities` | Whether the selected CLI has the bridge: `supported` and `document_api` 1 or later (mobile packet backends report `document_api` 0, which offers no editing). An older CLI answers `unknown command` with a nonzero exit; WG/AWG import and editing are then unavailable. The result is cached per executable path, size and modification time. |
| `detect` | Decides whether an imported `.conf`, a clipboard text or a referenced `File` is a Bimeron profile or a WG/AWG configuration. |
| `inspect` | Decoded fields, protocol and AWG version, AllowedIPs coverage and validation problems for the editor and the profile list label. |
| `generate_keypair` | Creates the client key pair when the user first selects WireGuard for a new profile. |
| `public_key` | Derives the read-only client public key for display and copying. |
| `generate_preshared_key` | Creates a preshared key on request. |
| `edit` | Applies `set_field` operations to the WG/AWG text; an empty value removes the key. A refused operation returns `error`, its problem and the original text. |
| `import_uri` | Converts `vpn://`, `wireguard://` and `wg://` links to WG/AWG text and a suggested name. A link that cannot be converted returns problems only. |

Requests and responses travel only through stdin/stdout; they are secret-bearing and never
logged. Core problems carry `document: "wireguard"` when their section, field and line
refer to the WG/AWG text; Desktop maps them to the WireGuard tab or to the WG/AWG config tab.
Problems about the reference itself (`file_unreadable`, `file_not_regular`,
`file_path_not_local`, `file_too_large`, `file_not_utf8`) are reported at
`[AccessKey] File` without that marker.

The running service advertises support with `hello.capabilities.wireguard.supported`.
A selected CLI with the bridge does not imply that the service supports WG/AWG.

## Import

Add profile accepts:

- `.conf` files. Core detects the format by content. Bimeron profiles keep the existing
  import path. WG/AWG configurations (`[Interface]` with `PrivateKey`, `[Peer]`) create a
  new pair. A document that looks like both is refused with `document_format_ambiguous`.
- `vpn://` (Amnezia), `wireguard://` and `wg://` links in the link field. The name field is
  optional for these links: an empty name uses the name from the link, otherwise `WireGuard`.
- A WG/AWG configuration on the clipboard. When the clipboard holds multi-line text up to
  64 KiB with a section header in any spelling, Paste becomes "Paste WireGuard configuration" and imports
  it directly; Core `detect` decides the format, so a partial configuration reaches the
  correction editor as from a file. This check is a presentation hint only.
- A Bimeron profile whose `[AccessKey] File` references a WG/AWG file, relative or absolute.
  Both files are copied into the profile store, which supports moving a pair to another
  computer. Other references keep the existing relocation rules.

Desktop reads `[AccessKey] File` as Core does: a quoted value is decoded with Go
`strconv.Unquote` rules (a decoded leading U+FEFF stays part of the name); a name that is
not valid UTF-8 cannot be represented by Desktop and is never looked up.

Referenced files are read only when they are local regular files: Desktop never opens a
network path (UNC, extended-length or mapped network drive on Windows, or a path through a
symbolic link or junction, which could lead to a share) or a FIFO; Core then
reports `file_path_not_local` or `file_not_regular`. Only a valid bridge reply, or an older
CLI's normal `unknown command` refusal, is a definitive answer. A failed Core probe (start
failure, timeout, crash, malformed output, or a reply without the envelope and result its
operation promises) is transient and never cached as "not WireGuard",
and a transient failure during live field validation is retried on the next check or Save: opening or importing such a
profile, or starting it, is refused with a retry message instead of falling back to the ordinary path. A
managed pair opens only when Core positively identifies its key file; with an unreadable key
file or a CLI without the bridge it does not open. Links longer than 64 KiB are refused.

The supported-format help lists the new schemes and both `.conf` formats. Core problem
codes are rendered from the Core error catalog (`errors --json`) in the interface language.
A configuration that converts but fails validation opens the editor with both documents as
an import correction draft; cancelling stores nothing. Rejected imports leave no files or
index entries.

A new pair's Bimeron profile is generated by Desktop:

```ini
[Profile]
Version = 1
Name = <file name, link name or WireGuard>

[AccessKey]
File = <managed key file>

[Interface]
Mode = vpn

[Routing]
Default = proxy
```

The Network tab reads DNS and MTU from the WireGuard document. Leaving either field
empty uses Core defaults (DNS 1.1.1.1 and MTU 1420). Core also uses the WG MTU for the system adapter within 1280–1500. Import preserves an omitted MTU. Saving the profile removes any legacy Bimeron DNS and MTU overrides, including
values that differ from the WireGuard document. Direct import preserves existing overrides until the first editor Save. Opening a profile leaves both files intact.

## Storage

A WG/AWG profile is two files in the profile store:

- the Bimeron profile `<uuid>.conf`, listed in `profiles.json` as before;
- the managed key file `<uuid>.<revision>.wg.conf` beside it, referenced by the relative
  `[AccessKey] File` value. Relative references resolve against the profile directory.

The key file is stored byte for byte: BOM, line endings, comments and a missing final
newline are kept, and Core never rewrites it. It must be valid UTF-8 and at most 64 KiB.
Both files use the protected writer (owner-only on Unix, protected current-user/SYSTEM DACL
on Windows). `QString::fromUtf8` drops a leading BOM, so key files are decoded with
`ProfileStore::keyFileText`, which keeps it.

Saving is a pair transaction. When the WG/AWG text changes, the new text is written under
a fresh revision name first, then the profile is atomically replaced to reference it, and
only then the previous managed revision is removed. A failure at any step leaves the
previous pair intact and removes the new key file. A profile-only change keeps the
reference. The editor refuses to save when either file changed on disk during validation.

Deleting a profile removes the profile and then its managed key file. Only a reference
matching `<profile base name>.<8 hex>.wg.conf` without a directory is ever deleted; an
absolute or external key file stays. Renaming changes `Name` only; file names do not change.
Desktop has no duplicate or export function; the WG/AWG config tab provides the standard
configuration for other clients.

## Editor

A WG/AWG profile shows:

- **General**: the source is "WireGuard / AmneziaWG configuration" and cannot be switched;
  the key file path is not shown. Desktop owns the `File` reference: Save refuses a Bimeron
  config whose `File` differs from the stored reference (a new draft keeps the placeholder
  `wireguard.conf`, replaced when the pair is stored). When the running service lacks WG/AWG support, a notice
  explains that the profile can be edited but connects only after Core is started or updated.
- Key operations are enabled individually through the CLI's `capabilities.key_operations`.
  Imported configurations retain their private and preshared keys. Older CLIs without
  these operations still support manual entry. Only the client public key has a Copy action.
- **WireGuard**: private key and preshared key (masked, with Show), addresses, server public key, endpoint, AllowedIPs and keepalive. A collapsed "AmneziaWG"
  section lists every AWG parameter grouped by version: `Jc`, `Jmin`, `Jmax`, `S1`, `S2`,
  `H1`-`H4` (values or `lo-hi` ranges), `I1`-`I5`, `S3`, `S4`, `HeaderProtectionKey`,
  `ContentPaddingAddition`, `RekeyAfterTime`, `RekeyTimeout`, `RejectAfterTime`,
  `KeepaliveTimeout`, `MaxHandshakeAttempts`, `RandomTrailers` and `DisableCookies`. Its
  header shows how many parameters are set. An absent key stays absent; the 3.1 booleans
  `RandomTrailers` and `DisableCookies` are Not set / On / Off choices
  (Not set removes the key, On and Off write `1` and `0`; any spelling Core accepts loads, and a
  value Core rejects shows Not set with the problem on the field). A status line below the fields shows Core's validation of the current draft a
  moment after each change.
- **Reserved** appears below **Keepalive, seconds** and above **AmneziaWG** for plain
  WireGuard. Tab moves through the three byte inputs in order. Enter three decimal
  values from 0 to 255, or clear all three inputs to remove `[Peer] Reserved`.
  Three zeros are a value. Partial input stays in the form and blocks Save and transfer
  to **WG/AWG config** until completed or cleared. Complete or clear the row before
  selecting an AmneziaWG version, which hides the row and preserves its contents.
- **WG/AWG config**: the WG/AWG text, editable.
- **Bimeron config**: the Bimeron profile (the former Raw config tab; other profiles keep
  the Raw config title and show no WireGuard tabs).

Fields, both raw tabs and Save use one draft per document. A field edit becomes a Core
`set_field` operation against the loaded text; untouched lines, comments, BOM and CRLF keep
their bytes. Clearing a field removes its line. Leaving an edited WG/AWG config tab inspects
the text and reloads the fields; a document Core cannot show as fields (`fields_error`:
duplicate keys, several `[Peer]` sections, malformed lines) stays in that tab and the
WireGuard tab is disabled. While Core refuses a field edit, the WG/AWG config tab does not
open, so the pending field draft is never replaced by the original text. Raw edits of a CRLF
document keep CRLF. Opening and cancelling preserve both files. Saving an otherwise unchanged pair
only updates the Bimeron file if it contains legacy DNS or MTU overrides.

Save first requires Core `detect` to classify the WG/AWG text as `wireguard`: Core also accepts
URI-backed `File` profiles, so another text would otherwise replace the managed key file.
It then validates the pair as Core reads it: protected drafts of both files are written beside
the profile (or in the import draft directory), the draft profile references the draft key
file, and `validate --json` checks both. Problems in the WG/AWG text name the WireGuard
field (expanding the AmneziaWG section when needed) or a line of the WG/AWG config tab.
Secret values are masked in problem text.

Create in editor offers "WireGuard / AmneziaWG" only when the running service advertises
WG/AWG support and the selected CLI has the bridge; otherwise a notice explains that Core
must be started or updated. A new draft proposes `AllowedIPs = 0.0.0.0/0, ::/0` and clears
the default DNS. The new key file is built by Core from the fields (`[Interface]` before
`[Peer]`).

## Routing

The Routing tab of a WG/AWG profile adds "AllowedIPs priority", stored as
`[Routing] AllowedIPsMode`. The key is written only when it differs from the default or was
already present:

| Option | Value | Behavior |
| --- | --- | --- |
| Direct outside AllowedIPs | `direct` (default) | Bimeron rules decide; tunnel traffic outside AllowedIPs goes directly. |
| Block outside AllowedIPs | `block` | The same, but that traffic is blocked. |
| Ignore AllowedIPs | `ignore` | All tunnel traffic goes into the tunnel. |
| AllowedIPs only | `only` | As in plain WireGuard; Bimeron rules and the default action are not applied but stay in the profile. |

For a new or imported pair with active GeoData rules, validation uses a protected draft
of the WG/AWG sibling. Other relative dependencies remain subject to the import checks.
In `only` mode Desktop keeps the rules and skips their service capability and GeoData
availability checks. Quoted values in the Bimeron config use Core's Go string escapes.
Core validates the complete profile before a connection.

Block rules win in the first three options and direct rules apply inside AllowedIPs (Core
behavior). Beside the selector the tab shows the option's hint and the AllowedIPs list
(read only, from the WireGuard tab). Notes use Core's `allowed_ips.coverage`, never a
Desktop calculation: with full coverage the first three options behave the same. With IPv4
only coverage and the `direct` or `only` option, the note explains that IPv6 traffic that would
go directly only because it is outside AllowedIPs is blocked; an explicit direct rule still
applies. With `only` the rules area is disabled and the note explains why.

## Profile list and connection

The list shows "WireGuard → System VPN" or "AmneziaWG 2.0 → System VPN" from Core
inspection, cached in memory per key file identity and selected CLI. Probes run off the GUI thread,
so a stalled Core never freezes the list; a transient failure is retried after 30 seconds. Start refuses a WG/AWG
profile with a notice when the running service lacks WG/AWG support. Start validation is
unchanged: Core reads the referenced file.

Connection failures use Core codes with en/ru texts from `errors --json` and Core's
`LastErrorPermanent` flag, so no Desktop mapping is needed: `wireguard_handshake_timeout` and
`wireguard_socket_failed` are retried; `wireguard_platform_limit` (AWG paddings, junk sizes or
MTU too large for the platform engine, notably about 2 KB per datagram on Windows) and
`invalid_wireguard_transport` are not. These codes are kept in exported diagnostics. Start
progress events (`wireguard_handshake`, then `transport_check` in VPN mode) appear in the log.

## Verification

`profile-vless-test` covers the pair storage (exact bytes, revisions, failed replacement,
removal and external files) and the editor through a fake bridge: tabs and titles, no-op
round trips with BOM and CRLF, field edits and removals, WG/AWG raw edits, ambiguous
documents, AllowedIPs priority and notes, refused edits, problem labels without secrets,
service and CLI capability gating, creation and the bridge helpers. With
`BIMERON_WIREGUARD_CORE` pointing to a Core with the bridge it repeats detect, inspect,
edit, link import and an editor round trip against the real Core.

Build the WG/AWG integration CLI from Core commit
`f052ebdef9756b4ce1ed89bada26c65f457ddc9f` (the merge of bimeron/bimeron-core#256)
and set `BIMERON_WIREGUARD_CORE` to the resulting executable. The packaged Core
has a separate release archive pin in `cmake/core-release.cmake`.

Live connection checks run only on the test VMs. Ordinary Desktop CI does not download Core.

[Desktop #99](https://github.com/bimeron/bimeron-desktop/issues/99) tests Reserved
with a fake API v1 bridge that returns `Peer/Reserved` in its decoded fields. The test
covers import, keyboard traversal, partial input, form and raw synchronization, and
the saved profile pair. It also times repeated draft reads and checks that they
reuse the cached edit response.
Set `BIMERON_WIREGUARD_RESERVED_CORE` to a build of Core's `feat/wireguard-reserved-208`
branch to run the document smoke test (import, inspect, edit, reopen and remove).
The Reserved capability and service admission rules are tracked in
[Core #208](https://github.com/bimeron/bimeron-core/issues/208).
Successful document editing verifies the CLI path. Transport requires a separate check.
