# Hysteria2 profiles

## Contract and scope

The Core contract is `bimeron-core` branch `feat/hysteria2`,
`docs/GUI_INTEGRATION.MD` and `docs/CONF.MD` as inspected on October 26, 2026.
The desktop GUI supports a complete `hysteria2://` or `hy2://` URI as one
`[AccessKey] Hysteria2` source. Existing generic `Key` and `File` sources
remain supported. The GUI writes the URI unchanged, including percent-encoded
password characters, optional `sni`, and fragment. It never decodes the
password into a second configuration field. Core `validate` is authoritative
for URI options, duplicate parameters, TLS and transport constraints.

Hysteria2 requires a nonempty password and a server port. The password is the
URI username; `sni` is optional and TLS verification stays enabled. VPN mode
uses TCP and UDP through QUIC, and the server must support UDP relay. Local
proxy mode provides TCP CONNECT. The GUI does not offer unsupported obfuscation,
insecure TLS or bandwidth options. There is no explicit Hysteria2 parameter
form in the current Core contract.

## GUI behavior

- Add profile accepts both URI schemes, suggests the decoded fragment as the
  profile name, writes `Hysteria2 = <original URI>`, and validates the resulting
  `.conf` with the selected Core CLI before keeping it. Invalid input or failed
  validation leaves no profile behind.
- The profile list labels `Hysteria2` and generic `Key` URI sources as
  `Hysteria2`; `File` keeps its existing disconnected/connected behavior.
  Unknown or malformed sources stay unconnectable from every entry point.
- The visual editor recognizes the Hysteria2 source, hides the URI by default,
  permits reveal/edit, and saves it as one unchanged source. A recognized
  source follows the existing source-selector lock. Raw config retains
  unsupported fields and remains available for advanced profiles.
- Profile previews, diagnostics and errors redact `Hysteria2 = ...`,
  `Key = ...`, and any password field. URI credentials must not be logged or
  displayed in the profile list. The existing absolute-path `start` RPC,
  service status and connection lifecycle need no protocol-specific change.

## Acceptance criteria

1. Add and edit round trips preserve `hysteria2://` and `hy2://` URIs, encoded
   passwords, optional `sni`, IPv4/IPv6 hosts and fragment text. A generic
   `Key` URI remains a generic `Key` source after editing.
2. A `.conf` with `[AccessKey] Hysteria2` imports and displays the correct
   protocol. A malformed URI cannot be started. Core rejects an absent
   password or port, unsupported or duplicate query options, and unsupported
   TLS settings without retaining a newly added profile.
3. The editor preserves unrelated settings and never silently converts the
   URI into explicit settings. Existing Shadowsocks, SOCKS5 and VLESS profiles
   retain their current behavior. Neither the list nor diagnostic output
   exposes the password.
4. Focused GUI round-trip tests run without network access. Set
   `BIMERON_HYSTERIA2_CORE` to a Hysteria2-capable Core CLI path to validate
   the generated profiles against it. A manual integration test with an
   installed capable service confirms connection, UDP relay, stop, and
   reconnect; an older service
   reports its error without claiming that VPN is connected.

## Implementation and release sequence

1. Extend the existing Add profile validation, `ProfileStore` source mapping
   and protocol detection, editor source choices/load/save, and diagnostic
   redaction. Reuse the current `validate` and IPC flow.
2. Add focused round-trip and redaction coverage, then run the relevant
   desktop tests and CI-equivalent builds. No performance benchmark is needed
   for this profile-format/UI change.
3. Update the profile-list documentation and this specification
   to match the final behavior. If implementation changes the scope, revise
   this specification before implementing that change.
4. The currently pinned published Core `v0.2.0-beta.1` predates Hysteria2.
   A Hysteria2-capable signed and GUI-reviewed release is required before
   updating the package pin. Once published, follow
   `docs/build-and-packaging.md`: update the Core tag and four archive hashes,
   update that document in the same commit, and verify all four packages before
   release. Local candidate Core builds may be used for contract tests only.
