# Unified profile creation

## Accepted scope

Replace the separate Add key and Import .conf actions with Add profile. The
dialog accepts a supported protocol URI or a configuration path in one field,
with Browse on its right and the existing supported-format help retained.
Keep clipboard insertion and SSH-specific authentication controls.

Show the name field for URI input only. File selection and typed file paths use
the same import pipeline and never request an override name. Read the profile
name from the document; use the original filename when the name is absent.
Support native Windows drive/UNC paths, Linux absolute paths and ~/ paths,
spaces, Unicode, paired outer quotes and local file URLs. Reject relative paths
with an actionable message. Never treat an unknown URI as a local filename.

Create in editor opens an independent unsaved draft. Its protocol selector must
offer parameter-based choices, not URI/key choices. Cover every protocol already
supported by desktop import: Shadowsocks, SOCKS5, VLESS, Hysteria2, VMess, Trojan,
TUIC, HTTP, HTTPS, AnyTLS, Naive HTTPS/QUIC, MASQUE and SSH. SSConf is a dynamic
subscription source, not a separate connection protocol; retain its import.
Every protocol is saved as explicit `[AccessKey]` options: each typed value has
its own option in the profile, and no generated share link becomes the source.
Hysteria2, VMess, TUIC, AnyTLS and Naive QUIC require a Core that lists them in
the `explicit_access` capability; see [explicit access protocols](#explicit-access-protocols).

Raw configuration remains available and shares the draft with visual controls.
Unsupported or ambiguous documents retain protected Raw editing. Cancellation
returns to the add dialog without losing its input. Saving validates through Core
and atomically registers one profile; failures retain the draft and roll back
partial files/index changes. Never modify an imported source file.

## Implementation constraints

Preserve the existing GUI editing contract, reference safety, GeoData correction,
UTF-8/64 KiB checks, secret redaction and protected temporary writes. Do not
change Core, dependency pins, packaging or unrelated in-progress recovery work.
Use existing Qt styles and EN/RU localization. Do not infer complete protocol
forms from the mockup's abbreviated protocol dropdown.

## Acceptance and verification

- One entry point; Browse remains right of the shared input; format help lists
  every existing URI scheme and supported configuration extensions.
- URI/file transitions show/hide the name correctly and retain user edits.
- Browsed and typed paths behave identically on Windows and Linux.
- Every protocol can be created through fields and through Raw; the creation
  selector contains no link-based options. Reopen preserves the saved values.
- Invalid input, failed validation/writes and cancellation leave no new profile.
- Existing imported-profile editing and preservation tests remain green.
- Exercise light/dark EN/RU at 100/150/200 percent and keyboard navigation.
- Run focused tests followed by CI-equivalent builds/tests for Windows x64,
  Linux amd64/arm64 and the Windows ARM64 compile-only job.
- No benchmark or release packaging is required for this bounded GUI change.

## Status

Implemented in `feat/unified-profile-creation`. The existing editor's nine
explicit protocol forms are joined by five parameter sources: Hysteria2, VMess,
TUIC, AnyTLS and Naive QUIC. New drafts keep protocol selection enabled and
exclude URI sources. VMess exposes the Core-supported transport/TLS fields;
Core remains authoritative for accepted combinations.

Raw changes that cannot be represented losslessly remain Raw-only;
opening or saving such documents does not convert or discard their contents.
Untouched switches between forms and Raw retain the creation draft.

The unified input accepts native absolute paths, quoted paths, home paths and
local file URLs; invalid/relative input disables Save and explains the expected
format in the input tooltip. Missing or unreadable files use import errors.
No Core, package or dependency changes are included in this feature.

ARM64 runtime checks require ARM64 hardware; this x64 host performs cross-builds
only. Package and live connection tests are not part of this implementation task.

## Explicit access protocols

The first implementation saved Hysteria2, VMess, TUIC, AnyTLS and Naive QUIC
drafts as share links built from the typed fields, because Core accepted these
protocols only as links. Core now accepts them as explicit options, so the
editor saves and reopens them as fields:

| Protocol | Saved options | Capability |
| --- | --- | --- |
| Hysteria2 | `Type = hysteria2`, `Server`, `Port`, `Password`; optional `ServerName`, `ALPN`, `Fingerprint` | `hysteria2` |
| TUIC | `Type = tuic`, `Server`, `Port`, `ID`, `Password`; optional `ServerName`, `ALPN` | `tuic` |
| AnyTLS | `Type = anytls`, `Server`, `Port`, `Password`; optional `ServerName` | `anytls` |
| Naive QUIC | `Type = naive`, `Server`, `Port`, `Username`, `Password`, `Network = quic`; optional `ServerName` | `naive_quic` |
| VMess | `Type = vmess`, `Server`, `Port`, `ID`, `Encryption`, `Network`, `Security`; TLS, REALITY and transport options | `vmess` |

The VMess form keeps one field for the transport path, one for the host and one
for the mode. On save they become the option of the selected transport:
`WSPath`/`WSHost` for `ws` and `httpupgrade` (Core rejects them for `tcp`,
so a stale value is reported instead of dropped), `GRPCServiceName`/
`GRPCAuthority`/`GRPCMultiMode` for `grpc` (mode `multi` is `true`, `gun` is
`false`), and `XHTTPPath`/`XHTTPHost`/`XHTTPMode`/`XHTTPExtra` for `xhttp`.
Reopening maps them back. A profile that sets two options for one field, such as
both `WSPath` and `XHTTPPath`, stays Raw-only. Core problems at any of these
options mark the VMess field that holds the value.

The creation selector offers these five protocols only when the running Core
lists them in `explicit_access`. Otherwise it hides them and explains that Core
must be started or updated; an older Core would reject the saved profile.
Existing profiles open regardless of the capability.

Existing profiles with VMess, Trojan, TUIC, AnyTLS, HTTP, Naive or MASQUE links
are not converted. They open in the form as link sources when the link scheme
matches its field; the link stays hidden by default and is saved verbatim.
Unrecognized links remain Raw-only.

Regression tests cover every protocol's saved options, the absence of links and
editor-only names in saved profiles, reopening into a populated form, all VMess
transports, ambiguous VMess documents, VMess problem navigation, link profiles
and the capability gate. With `BIMERON_PRESERVATION_CORE` set to a Core that
reports `explicit_access`, each created profile is also validated by Core.

## Verification evidence

The isolated feature-only staged snapshot was also built on Linux amd64: all
five CTest executables passed without the unrelated recovery changes.

- Windows x64: Release build and all five CTest executables passed, including
  generated profiles validated by the local Core. Windows ARM64 and Linux arm64
  Release cross-builds passed; no ARM64 runtime execution is claimed.
- Linux amd64: Release build and all five CTest executables passed with the local
  document-capable Core used for the generated-protocol validation fixtures.
- New regression cases cover all five URI-backed parameter sources, all fourteen
  creation choices, Core validation, accepted saves, Raw round trips, reopened
  document preservation, default interface mode, protocol switching, absolute/
  quoted/home/file-URL inputs, conditional name visibility and editor cancellation.
- Actual styled Qt captures on native Windows and offscreen Linux verified the
  shared input, footer alignment and default creation editor. Set
  `BIMERON_CREATION_CAPTURE` to an ignored absolute output basename when running
  `developer-mode-test` to reproduce `.add.png` and `.editor.png` captures.
- Review corrected the missing VMess TLS default, empty interface mode before
  service capability discovery, and footer/hint layout. Inactive protocol fields
  remain in the creation draft without leaking into another protocol's output.
- `git diff --check`, UTF-8 decoding and translation JSON validation passed.
- No benchmark is warranted for this bounded GUI workflow. Full EN/RU light/dark
  visual acceptance at every requested DPI, ARM64 runtime and live protocol
  connectivity remain separate checks before release. No packages were produced.
- Existing uncommitted recovery work was retained; this feature does not modify
  build or packaging flow. Recovery changes are excluded from the feature commit.
