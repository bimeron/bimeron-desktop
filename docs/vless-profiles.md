# VLESS profiles

The Add profile dialog accepts a complete `vless://` URI and stores it as the sole `[AccessKey] VLESS` source. Its fragment suggests the profile name. The visual editor can edit the URI source and explicit `Type = vless` parameters. The raw editor remains available for configurations outside the visual editor. The installed core's `validate` command is authoritative; the GUI does not reimplement VLESS transport validation.

Explicit VLESS settings cover TCP with REALITY and Vision or TLS, WebSocket with TLS, gRPC with TLS, XHTTP `packet-up` with TLS, and XHTTP `stream-one` with REALITY. The fields and valid combinations follow the core's `docs/VLESS_TRANSPORTS.MD` contract. One access-key source is written at a time. The GUI sends the validated profile's absolute path through the existing `start` RPC and follows the core's reported session state.

Acceptance criteria:

- Adding a valid VLESS URI, including an IPv6 server or encoded fragment, creates a `.conf` accepted by a VLESS-capable core. Invalid links are rejected by `core validate` without exposing credentials in diagnostics.
- Opening and saving VLESS URI and explicit profiles retains their source and all supported fields. Unsupported visual settings trigger the existing warning before a visual save; editing the raw configuration preserves them.
- All supported transport combinations can be represented with explicit parameters. Unsupported combinations fail validation. Existing Shadowsocks and SOCKS5 profiles continue to work.
- A service without VLESS support may reject `start` even when the bundled CLI validates a profile. The GUI reports the service error and its authoritative state; installing a VLESS-capable service is required to connect.

The currently pinned core `v0.1.0-beta.5` predates VLESS. Development verification may use a local build of `feature/vless-transports`. Bundled-core and package changes require a published, signed, GUI-compatible core release and are separate from this GUI change.
