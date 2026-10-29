# Core/Desktop integration contract

## Scope and evidence

This specification defines the integration boundary and acceptance criteria for
future Desktop changes. It does not implement those changes or restore connection
controls by itself. It is not a roadmap or a release approval.

The source review on October 29, 2026 used these exact baselines:

- Desktop: `2c3feaa5c8e55439bd77e6bd36f333cfd2c12e92`.
- Core: `9f527efe0a4959786084675f1ff795edb8a0d910`, matching remote `master`
  and the commit referenced by annotated tag `v0.3.0-beta.1`.
- Desktop bundle pin: `v0.2.0-beta.1`, Core revision
  `6e0c171505be8e349d5f42570f7ceb3e50ac6697`.

A tag is not evidence of published release assets, a signed compatibility manifest,
or runtime interoperability. The GitHub release-by-tag API returned HTTP 404 for
`v0.3.0-beta.1` during this review; no release assets or compatibility approval for
that tag were verified. Do not assign it a reviewed GUI range from source support.

Core source references below are fixed to the reviewed SHA:

- [Configuration](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/CONF.MD),
  [GUI integration](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/GUI_INTEGRATION.MD),
  and [README](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/README.md).
- [Config parser](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/internal/config/config.go),
  [access-key parser](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/internal/accesskey/accesskey.go),
  [runtime](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/core/runtime.go),
  and [control server](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/internal/control/server.go).

Distinguish four kinds of evidence: implemented Core source support, published
release support, implemented Desktop import/editor behavior, and runtime results
for an exact GUI/Core/platform/server combination. Independent Core reference
tests do not establish Desktop or privileged system-VPN interoperability.

The published bundle supports SS/SSConf, SOCKS5 and VLESS; its source predates
the other upstreams and extensions below, including SS2022 and Hysteria2.
The [recorded VM checks](vm-core-0.2.0-beta.1-check.md) establish specific older
Windows amd64 update/reconnection results, not connectivity of the current
navigation baseline or support for the new protocols. No new runtime checks
were performed for this specification.

## Current Desktop boundary

At the Desktop baseline, Add profile accepts `ss://`, `ssconf://`, `socks5://`,
`vless://`, `hysteria2://` and `hy2://`. Visual forms cover SS, SOCKS5 and VLESS
explicit parameters and the recognized URI sources; Hysteria2 has URI editing
but no explicit form. Generic `Key` and existing `File` sources remain available.
See [VLESS](vless-profiles.md), [SOCKS5](remote-socks5.md),
[Hysteria2](hysteria2-profiles.md) and [profile labels](profile-list.md).

`.conf` import copies the file and then validates the destination with the selected
CLI. It requires `[Profile]` and `[AccessKey]`, so it does not import native
`[IKEv2]` profiles. Being copied and accepted by a new CLI does not imply that the
list recognizes the protocol, the visual editor preserves it, or a service can
connect it. New URI schemes are not accepted by Add profile merely because Core
supports them.

Connection controls, tray profile actions and regular IPC polling were intentionally
disabled at the reviewed navigation baseline. They are now restored according to
[connection management](connection-management.md), using authoritative IPC state
and isolated regression tests. This does not establish real network interoperability
for the protocol matrix or change the reviewed bundle range.

Source review found these risks at the Desktop baseline in `ProfileEditorDialog.cpp`:

- An unknown explicit SS `Method` selects the first cipher combo item.
- An unrecognized explicit `Type` with `Server` selects the SS parameter form.
- Visual save can remove unknown settings after a warning. Raw-only blocking is
  specific to VLESS and does not protect arbitrary new sections such as ShadowTLS.

At that baseline, `CoreDiagnostic.h` redacted only the older URI/source set plus
Hysteria2; it did not establish safe handling of the new credential formats.
The requirements below are acceptance criteria, not runtime evidence.

## Current preservation behavior

[Profile preservation and secret handling](profile-preservation.md) defines the
implemented editor/storage guard and its acceptance tests. Unrepresented profiles
use protected Raw editing, no-op saves preserve bytes, profile replacement is
atomic, and untrusted diagnostic text is hidden by default. This does not expand
Add URI, native IKEv2 import, connection controls, or the reviewed bundle range.
The format and coverage tables below describe the reviewed baseline; source
support is still distinct from Desktop forms and runtime evidence.

## Formats, transport and authentication

Choose exactly one access source: a dedicated URI field, `Key`, `File`, or an
explicit parameter group. Preserve the source rather than converting it on save.
`File` contains a URI, not a JSON subscription document. SSConf is the supported
dynamic source, not a general subscription framework. Core validates duplicates,
unknown options, syntax and combinations; Desktop must not silently remove them.

The matrix describes the reviewed Core source, not the old bundle. Every row
supports TCP in its stated mode. VPN UDP means proxy-routed application UDP;
captured DNS and explicitly direct-routed traffic are separate. Local proxy means
the local SOCKS5 listener, not the remote upstream protocol.

| Upstream / dedicated source | URI and explicit parameters | VPN UDP and DNS | Local proxy | Authentication / verification |
| --- | --- | --- | --- | --- |
| Shadowsocks / `SS`, dynamic `SSConf` | `ss://`, `ssconf://`; explicit `Type=shadowsocks` (alias `ss`), `Server`, `Port`, `Method`, `Password`, optional `Prefix` | Native UDP without wrapper/plugin; DNS uses UDP with TCP fallback | TCP CONNECT | Inner cipher authenticates traffic; SSConf fetched over verified HTTPS |
| Remote SOCKS5 / `SOCKS5` | `socks5://`; explicit `Type=socks5`, `Server`, `Port`, paired optional `Username`/`Password` | Upstream UDP ASSOCIATE required, including DNS | TCP CONNECT | No auth or username/password; upstream is unencrypted |
| VLESS / `VLESS` | `vless://`; explicit fields described below | XUDP over supported transport | TCP CONNECT | UUID; verified TLS or REALITY public-key authentication |
| VMess AEAD / `VMess` | Base64 JSON `vmess://`; no explicit form | UDP over supported transport | TCP CONNECT | UUID, `aid=0`, `scy=auto`, `aes-128-gcm` or `chacha20-poly1305`; TLS or REALITY |
| Trojan / `Trojan` | `trojan://`; explicit `Type=trojan`, `Server`, `Port`, `Password`, optional `ServerName` and transport fields | UDP-over-stream | TCP CONNECT | Password; verified TLS required |
| Hysteria2 / `Hysteria2` | `hysteria2://`, `hy2://`; no explicit form | QUIC relay; server must allow UDP | TCP CONNECT | Password in userinfo or `auth`, never both; verified TLS |
| AnyTLS / `AnyTLS` | `anytls://`; no explicit form | UDP-over-TCP v2 | TCP CONNECT | Password userinfo; verified TLS; session reuse/padding |
| TUIC v5 / `TUIC` | `tuic://`; no explicit form | `quic` stream-mode relay, not native datagram mode | TCP CONNECT | UUID and password; verified TLS |
| SSH / `SSH` | `ssh://`; explicit `Type=ssh`, `Server`, `Port`, `Username`; optional `IdentityFile`, `KnownHostsFile` | TCP-only policy | TCP CONNECT | Key/agent or interactive authentication; matching known_hosts entry required |
| HTTP CONNECT / `HTTP` | `http://`, `https://`; explicit `Type=http` or `https`, `Server`, `Port`, paired optional credentials | TCP-only policy | TCP CONNECT | Optional Basic auth; HTTP plaintext, HTTPS verified TLS before credentials |
| Naive-compatible / `Naive` | `naive+https://` H2, `naive+quic://` H3; explicit `Type=naive`, endpoint, `Username`, `Password`, optional `ServerName` selects H2 only | TCP-only policy, even with H3 | TCP CONNECT | Basic auth; verified TLS and selected H2/H3 ALPN |
| MASQUE / `MASQUE` | `masque://`; explicit `Type=masque`, endpoint, optional paired credentials, `ServerName`, `CAFile`, `MASQUEPath` | H3 CONNECT-UDP, QUIC DATAGRAM; DNS through tunnel | TCP CONNECT and UDP ASSOCIATE | Optional Basic auth; verified TLS 1.3/H3; optional additional CA |

TCP-only policy means captured DNS uses TCP through the upstream and other
proxy-routed UDP is dropped. There is no fallback to direct traffic. Explicit
`direct` routes retain their physical-network behavior. This policy applies to
SSH, HTTP CONNECT, Naive, SS plugins and wrapped SS. It does not mean all traffic
is protected: existing OS routes and application-owned DNS retain their documented
limitations.

### Desktop import and editor coverage

Connection entries were unavailable in the reviewed navigation baseline. The table
below concerns import and editing only, not restored runtime support. "Conditional" means `.conf` copy/import
requires a selected CLI accepting the profile; it does not imply safe visual save.

| Protocol/configuration | Add URI | Import `.conf` | Visual editing at baseline |
| --- | --- | --- | --- |
| SS/SSConf | Yes | Yes, after CLI validation | Existing URI and legacy SS fields; SS2022/plugin/wrapper fields are not fully represented |
| SOCKS5 | Yes | Yes, after CLI validation | URI and explicit parameters |
| VLESS | Yes | Yes, after CLI validation | URI inspection/editing and represented explicit fields; advanced explicit settings use Raw |
| Hysteria2/hy2 | Yes | Conditional; bundle CLI rejects it | URI source; no explicit form |
| VMess, Trojan, AnyTLS, TUIC, SSH, HTTP/HTTPS, Naive, MASQUE | No | Conditional; bundle CLI rejects them | No dedicated forms or list protocol recognition; generic Key/File is not feature support |
| ShadowTLS section | Only as part of a profile, not a standalone URI | Conditional; bundle CLI rejects it | Not represented; current warning-based save risks dropping the section |
| Native IKEv2 | No | Rejected by current AccessKey import requirement | No form; outside the packet connection flow |

Common URI rules are protocol-specific: SOCKS5/HTTP require explicit ports;
SSH defaults to 22, and AnyTLS/TUIC/Naive/MASQUE default to 443. Do not apply one
scheme's path, fragment, query or fingerprint rules to another. Core configuration
is UTF-8 with optional BOM, at most 64 KiB; section/option names are case-sensitive,
unknown or duplicate fields are rejected, and only Rule sections may repeat.

### Shadowsocks extensions and ShadowTLS

[SS transport details](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/SHADOWSOCKS_TRANSPORTS.MD)
and the Core config parser define these constraints:

- SS methods are `chacha20-ietf-poly1305`, `aes-128-gcm`, `aes-192-gcm`,
  `aes-256-gcm`, `2022-blake3-aes-128-gcm`, `2022-blake3-aes-256-gcm` and
  `2022-blake3-chacha20-poly1305`. SS2022 keys are standard base64: 16 bytes
  for AES-128, 32 for AES-256/ChaCha20. AES accepts SIP023 colon-separated
  identity chains ending in the user key; ChaCha20 accepts one key only.
  Preserve colons and base64 padding. SS2022 rejects `Prefix`.
- Explicit SS uses `Plugin` and `PluginOpts`; URI sources carry their own encoded
  `plugin` value. Never combine explicit plugin fields with URI/File/SSConf.
  Simple SSConf JSON/YAML can include plugin settings; separate-endpoint `tcpudp`
  YAML cannot combine with them.
- Embedded `v2ray-plugin` supports WS/WSS only: `mode=websocket`, presence flag
  `tls`, `host`, `path`, `mux`. Defaults are `host=cloudfront.com`, `path=/`,
  `mux=1`; `mux=0` disables Mux. `tls=false` still enables TLS by presence.
  `host` controls HTTP Host and TLS SNI/certificate name, independently of endpoint.
- Embedded `obfs-local` requires `obfs=http|tls`; optional `obfs-host` defaults
  to `cloudfront.net`. HTTP alone accepts `obfs-uri` and `http-method`.
  TLS-shaped obfuscation is not cryptographic TLS and verifies no certificate;
  inner SS provides encryption/authentication. It is not WSS or ShadowTLS.
- Both plugins are TCP-only in VPN and need matching server settings. They need
  no external executable, plugin installer or plugin framework. Unknown options,
  executable paths and combinations with ShadowTLS are rejected.

[ShadowTLS](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/SHADOWTLS.MD)
is a separate optional `[ShadowTLS]` section requiring `ServerName` and `Password`.
Only strict v3 with verified TLS 1.3 is supported; there are no version, insecure,
outer fingerprint or ALPN selectors. Its handshake name/password are independent
of the inner protocol. The access endpoint is the public ShadowTLS listener;
Core does not expose a second backend endpoint. Failure never falls back unwrapped.

| Inner upstream | Allowed wrapper combinations | VPN UDP |
| --- | --- | --- |
| SS AEAD/2022 | TCP; excludes AES-192-GCM and plugins | TCP-only policy |
| VLESS / VMess | Their supported encrypted TCP-based transports, including H2 XHTTP; no H3 on either split leg | Retains inner UDP-over-stream |
| Trojan / AnyTLS | Existing TLS transports | Retains inner UDP framing |
| Naive / SSH | H2 CONNECT / encrypted SSH with host verification | TCP-only policy |

Plain SOCKS5, HTTP CONNECT, plaintext inner modes, Hysteria2, TUIC and HTTP/3 are
not wrappable. Local SOCKS5 remains TCP-only for wrapped profiles. Dynamic/File
sources can use the section; resolved combinations are revalidated by Core.

### Protocol-specific fields and limits

- [VLESS](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/VLESS_TRANSPORTS.MD):
  TCP TLS or REALITY (Vision only with TCP REALITY), WS TLS, gRPC TLS/REALITY,
  XHTTP TLS/REALITY. Explicit fields include `ID`, `Encryption=none`, `Network`,
  `Security`, `Flow`, `ServerName`, `Fingerprint`, `PublicKey`, `ShortID`,
  `SpiderX`, `ALPN`, `WSPath`, `WSHost`, `WSEarlyData`, `GRPCServiceName`,
  `GRPCAuthority`, `GRPCMultiMode`, `XHTTPPath`, `XHTTPHost`, `XHTTPMode` and
  `XHTTPExtra` or separate extended fields. Preserve URI `extra`, custom gRPC
  paths and split-download settings; do not flatten them into a partial form.
  H3 requires TLS and no fingerprint; REALITY XHTTP uses H2. Core's exact mode
  and ALPN combinations remain authoritative.
- [VMess](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/VMESS_TRANSPORTS.MD):
  TCP, WS, HTTPUpgrade, gRPC and XHTTP with TLS; REALITY on TCP/gRPC/H2 XHTTP.
  Preserve JSON fields including `net`, `tls`, `sni`, `fp`, `alpn`, `host`,
  `path`, `mode`, `extra`, `pbk`, `sid`, `spx`. WS/HTTPUpgrade use HTTP/1.1,
  gRPC H2, TLS XHTTP H2 or H3 without `fp` for H3. Reject legacy alter IDs,
  `scy=zero`, plaintext and unsupported fields through Core validation.
- [Trojan](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/TROJAN_TRANSPORTS.MD):
  `Network=tcp|ws|grpc`, optional `Security=tls`; `ServerName`, `ALPN`,
  `WSPath`, `WSHost`, `GRPCServiceName`, `GRPCAuthority`, `GRPCMultiMode`.
  URI equivalents include `type`, `security`, `sni`, `alpn`, `path`, `host`,
  `serviceName`, `authority`, `mode=gun|multi`. WS uses HTTP/1.1 and gRPC H2.
  No REALITY, insecure TLS, fingerprints, WS early data or custom gRPC paths.
- Hysteria2 preserves `sni`, `insecure=0`, `alpn=h3` and `fp` exactly.
  Fingerprints are `firefox`, `firefox-116`, `firefox-116b`, `firefox-116c`,
  `chrome`, `chrome-115`, `chrome-146`. Omission remains omission; `chrome`
  is not an alias for a versioned preset. No insecure TLS, obfuscation or
  bandwidth options. The fingerprint list differs from VLESS/VMess.
- AnyTLS accepts password, optional `sni` and `insecure=0`; no custom
  fingerprint, ALPN or explicit server form. TUIC accepts UUID/password,
  optional `sni`, `alpn` (default `h3`) and `udp_relay_mode=quic`; no native
  datagram mode, 0-RTT or insecure verification.
- HTTP/HTTPS CONNECT requires an explicit URI port and no path/query/fragment.
  HTTPS uses HTTP/1.1 and system trust, with no CA-file or SNI override and no
  plaintext fallback. Naive H3 is selected explicitly and has no H2 fallback or
  UDP relay; Naive compatibility does not promise Chromium-identical traffic.
- [MASQUE](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/MASQUE.MD):
  URI `sni`, `udp_path`, `ca` map to verified name, UDP template and additional
  CA. URI CA paths are absolute; explicit relative `CAFile` resolves against the
  `.conf` directory. System roots remain enabled. The default path is
  `/.well-known/masque/udp/{target_host}/{target_port}/`. Both ordinary CONNECT
  and CONNECT-UDP are required. No H2/TCP fallback or fragmentation; UDP payload
  is at most 1500 bytes and can be smaller with QUIC MTU. Local UDP ASSOCIATE
  requires its TCP control connection to stay open; SOCKS fragments are rejected.

## Preservation and secrets requirements

These requirements apply to future import/editor work across all protocols:

1. Preserve the complete URI/source, encoded credentials, query options,
   SS2022 chain, plugin grammar, wrapper section and extended XHTTP data.
   Unsupported values must never become a default Method, Type or transport.
   Exactly one source remains exactly one source, including generic `Key`.
2. Opening/canceling changes no bytes. Unedited Raw content remains unchanged.
   Visual editing is allowed only when every configured value can be represented
   without loss; otherwise retain Raw and block visual save. A warning granting
   permission to discard unknown settings is not sufficient protection.
3. Save atomically and retain the previous profile on validation/write failure.
   Temporary drafts and final profiles require appropriate owner permissions/ACLs
   while allowing the intended service account to read required files. Existing
   permission-setting calls are not proof of effective Windows ACL protection.
4. Preserve reference meaning: `File`, SSH identity/known_hosts and explicit
   relative CA paths resolve against the profile directory, not GUI cwd. If import
   relocation would change a relative reference, refuse it without retaining a
   new profile rather than silently rebinding it. Do not copy private keys as an
   implicit repair. Service readability is distinct from GUI readability; mapped
   drives, EFS and service-account home directories may differ.
5. Mask credentials by default. Never put complete access URIs, SSConf URLs,
   passwords, UUID/access identifiers, SS2022 chains, ShadowTLS passwords,
   credential-bearing XHTTP headers or SSH answers in logs, diagnostics, settings,
   telemetry or crash attachments. Redact before display and truncation, including
   invalid input and Core stdout/stderr. A scheme-only regex is insufficient for
   unknown schemes, explicit sections or nested secrets.
6. Store SSH identity/known_hosts paths, not private-key bytes. SSH passwords,
   passphrases and keyboard-interactive answers are transient IPC inputs, never
   profile fields or a persistent cache. Existing protocol credentials remain in
   protected plaintext profiles; this contract introduces no credential vault or
   storage migration.

## Validation, compatibility and IPC

The integration remains protected `.conf` + selected Core `validate` + existing
local IPC. Desktop does not reproduce the network implementation or full Core
validator. Static validation makes no network changes and cannot prove server
authentication, dynamic SSConf contents or a working data plane. Core `check`
requires a server and is a separate opt-in diagnostic, not automatic validation.

In system-service mode the baseline validates with the bundled CLI; in local
developer mode it uses the selected executable. The running service can differ
from either. Record CLI identity separately from `hello.core` version/revision/
modified/platform/architecture of the active endpoint. Missing identity means
unknown, not compatible. Do not infer protocol support from a development version
string, broad packet capabilities or the bundle pin alone. Core has no general
per-transport capability list; do not invent one or bypass a service rejection.

Use IPC version 1, correlated requests, UTF-8 JSON plus LF, bounded reads and
asynchronous UI handling. Preserve the existing protected pipe/socket and
[developer backend lifecycle](developer-mode.md). Never silently change endpoint
after access denial, incompatibility or local-daemon failure.

After compatible `hello`, read authoritative status; send `start` once with the
absolute profile path. Accepted start is not connected. Follow status/events and
stats, reset cursors and speed baselines on instance/session changes, and reread
status after dropped events. On ambiguous start timeout inspect status before
retrying. IPC unavailable is distinct from disconnected. Stop waits for cleanup;
profile switching waits for stop before a new start. Do not use shutdown or recover
as ordinary disconnect/error recovery. Unknown error codes remain safe generic
failures. Protocol additions do not require transport-specific start commands.

The signed update gate in [core-updates.md](core-updates.md) remains authoritative.
`gui_reviewed` certifies reviewed compatibility for the specified GUI range and
evidence, not a complete GUI editor for every Core feature. Core may release
independently of GUI feature parity. Neither this document nor the new tag changes
a manifest, bundle pin, update trust decision or package payload.

### SSH authentication

Follow the existing
[SSH contract](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/SSH.MD).
URI `identity`/`known_hosts` are overridden by explicit `IdentityFile`/
`KnownHostsFile`; save GUI-selected absolute paths. URI passwords are rejected.
Never auto-trust an unknown or changed host key. A service does not search its
account's default identity files; agent access must actually be available.

While connecting, poll `auth_pending` on the active protected endpoint. Optional
`auth` contains `id`, `kind`, `prompts`, `echo`; kinds are `key_passphrase`,
`password`, `keyboard_interactive`. Treat prompts as untrusted plain text and
honor masking. There are at most eight prompts of 256 bytes each. Send one answer
per prompt with `auth_reply`, `challenge_id`, `answers`; each answer is at most
1024 bytes and the request remains within the 16 KiB IPC limit.

Challenges expire after two minutes and are one-use. Stop cancels them. Close
prompts and discard transient answers on stop/failure, instance/backend change or
expiry; keep status polling responsive. Wrong/expired IDs produce
`invalid_auth_challenge`, invalid answers `invalid_auth_response`.
`unknown_command` from an old Core means interactive authentication is unavailable,
not permission to cache a password or assume success. Noninteractive selected-key
authentication remains a separate possibility.

The final Limits list in Core SSH.MD still excludes system VPN, contradicting its
earlier sections. The reviewed `TCPOnly` policy, runtime and tests establish the
implemented TCP-only VPN path; they do not establish a full UDP VPN or new Desktop
runtime evidence. Local SSH proxy does not automatically reconnect after failure.

### Native IKEv2 boundary

[IKEv2](https://github.com/bimeron/bimeron-core/blob/9f527efe0a4959786084675f1ff795edb8a0d910/docs/IKEV2.MD)
is an experimental Windows foreground backend using a pre-provisioned current-user
RAS profile, saved Windows credentials/EAP identity and Windows certificate trust.
It bypasses Wintun and the packet stack. `[IKEv2]` is exclusive with `[AccessKey]`;
it requires `WindowsProfile` and `ServerName`, with optional native `KillSwitch`.
Only `Mode=vpn` is allowed in Interface; packet options/rules are rejected.

Daemon start returns `ikev2_requires_foreground_user_session`. Read the separate
`capabilities.ikev2` object; compiled support is not profile/credential readiness.
Native protection differs from packet KillSwitch, activates after bootstrap and
can retain policy after a crash. Stats are unavailable, not zero. Do not offer
normal packet controls, local proxy, traffic charts or daemon connection for this
backend. Its GUI/process/provisioning design and live validation are separate
work; the packet-backend integration must not accidentally activate it.

## Acceptance and verification

The following are tests required of future implementations, not completed tests:

| Scenario / input | Required outcome | Verification |
| --- | --- | --- |
| Each matrix URI, explicit group where supported, `Key` and `File` | One preserved source; correct labels and no unsupported-form invention | Offline round trips plus exact-Core validate fixtures |
| Encoded passwords/fragments, IPv6 endpoints, SS2022 AES chains, plugin escapes, all Hysteria2 fingerprints | Values and omitted defaults retained; no normalization to another transport/preset | Compare saved source/Raw content and validate |
| Unknown Method/Type/fields/sections, ShadowTLS or extended XHTTP outside the form | No fallback or destructive visual save; cancel leaves original bytes | Editor regression tests and file comparison |
| Duplicate/unsupported options, wrong key lengths, mixed sources, incompatible wrapper | Core rejection leaves no new profile and no overwritten original | Negative validate fixtures and atomic-save failure tests |
| Relative file reference relocated on import; unreadable service dependency | No silent rebinding/copying of private keys; actionable safe failure | Temporary-directory import tests and isolated account checks |
| New/malformed URI, explicit/nested credentials and SSH answers in errors | No secret reaches preview, log, diagnostics or settings | Sentinel-secret tests across failure paths; ACL inspection |
| New CLI + older service, older CLI + new service, unavailable local backend | Validation and runtime identities remain distinct; no false compatibility or endpoint fallback | IPC fixtures and exact-binary integration checks |
| Accepted start, failed start, timeout, changed instance, dropped events, reconnect and stop | Authoritative state, responsive UI, no duplicate start or premature cleanup success | IPC lifecycle tests; isolated live service checks |
| SS plugins/wrapped SS, SSH, HTTP and Naive in VPN | DNS through TCP; other proxied UDP dropped; explicit direct rules unchanged | Controlled TCP/UDP/DNS endpoints and packet capture |
| MASQUE local UDP ASSOCIATE and VPN UDP | UDP works only with required server capabilities; no fragmentation or protocol fallback | Controlled reference server, control-channel close and oversized datagram tests |
| TLS name/chain failure, bad auth, changed SSH host key | Fail without insecure/unwrapped/plaintext fallback | Controlled trusted/untrusted servers; no production credentials |
| SSH one/multiple prompts, masking, timeout, stop, reused ID, old Core | One-use transient replies, no stale prompt or secret cache | IPC fixtures and controlled SSH authentication |
| Native IKEv2 profile imported/started through packet flow | No accidental SS conversion or daemon launch; explicit unsupported boundary | Parser/list fixtures and daemon rejection test |

Live evidence must identify Desktop SHA, CLI and runtime Core identities,
OS/architecture, server version/transport and scenario without credentials.
Windows/Linux amd64/arm64 results are recorded separately; compilation is not a
runtime pass. Existing Core reference tests and VM reports retain their original
scope and are not upgraded by this specification.

Acceptance of this documentation requires source-backed coverage of every matrix
row, consistent existing protocol docs, valid links and a documentation-only
diff. Check `git diff --check`, local links/anchors and fixed-SHA Core targets;
review for secrets, unsupported claims and accidental release instructions.
No build, package, benchmark or profiling run is required for documentation-only
changes. Full release testing remains governed by
[build-and-packaging.md](build-and-packaging.md); this document authorizes no release.

Desktop explicit SS extension editing is specified in [Shadowsocks profiles](shadowsocks-profiles.md). Runtime support still requires a compatible Core.
