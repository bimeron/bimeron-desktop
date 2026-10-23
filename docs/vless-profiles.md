# VLESS profiles

The Add profile dialog accepts a complete `vless://` URI and stores it as the sole `[AccessKey] VLESS` source. Its fragment suggests the profile name. The visual editor can edit the URI source and explicit `Type = vless` parameters. The raw editor remains available for configurations outside the visual editor. The installed core's `validate` command is authoritative; the GUI does not reimplement VLESS transport validation.

Explicit VLESS settings cover TCP with TLS or REALITY (with or without Vision), WebSocket with TLS, gRPC with TLS or REALITY (including authority and TunMulti), and XHTTP with TLS or REALITY in the core's supported modes. XHTTP over HTTP/3 is selected by `ALPN = h3` with TLS and no fingerprint. The GUI leaves core to validate combinations and details such as custom gRPC method paths. One access-key source is written at a time. The GUI sends the validated profile's absolute path through the existing `start` RPC and follows the core's reported session state.

Extended XHTTP settings (`XHTTPExtra` and separate `XHTTP...` option fields) are edited in Raw config. When an explicit VLESS profile contains settings the visual form cannot represent, switching to Raw config must keep the original text, and saving from the visual form must be blocked. Editing Raw config is the explicit way to save such a profile. URI sources remain opaque and are not parsed or rewritten by the visual editor.

Acceptance criteria:

- Adding a valid VLESS URI, including an IPv6 server or encoded fragment, creates a `.conf` accepted by a VLESS-capable core. Invalid links are rejected by `core validate` without exposing credentials in diagnostics.
- Opening and saving VLESS URI and explicit profiles retains their source and all fields represented by the visual editor. Extended settings are never removed by switching tabs or saving; the user must edit them in Raw config.
- The visual form represents the supported TCP, WebSocket, gRPC and XHTTP combinations listed above. `core validate` rejects invalid combinations. Existing Shadowsocks and SOCKS5 profiles continue to work.
- A service without VLESS support may reject `start` even when the bundled CLI validates a profile. The GUI reports the service error and its authoritative state; installing a VLESS-capable service is required to connect.

Desktop source version 0.2.0 includes VLESS support. The currently pinned core `v0.1.0-beta.5` predates VLESS; packages built with that pin do not provide VLESS service support. The local Core `v0.2.0-beta.1` candidate from commit `78aca2aa5537f55365c7c7b0452600d69338e49f` passes the Windows amd64 GUI profile validation tests, but its proposed GUI range `[0.2.0, 0.2.1)` remains unreviewed and its local manifest has no signature. Pin and package a signed, reviewed Core release separately.
