# Remote SOCKS5 profiles

The Add profile dialog accepts `socks5://host:port` and `socks5://username:password@host:port` alongside the existing Shadowsocks links. It saves the URI as the sole `[AccessKey] SOCKS5` source. The URI needs a host and port, no path, query, or fragment, and either both credentials or neither. Reserved credential characters must be percent encoded.

The visual profile editor reads and writes both the `SOCKS5` URI source and explicit `Type = socks5`, `Server`, `Port`, and optional paired `Username` and `Password`. For either explicit SOCKS5 or explicit Shadowsocks parameters, the link row is hidden; only relevant parameter fields are shown. The key-file source is omitted from the choices for new profiles, but remains available when editing an existing `File` profile so saving it preserves the source. It keeps the remote protocol distinct from the local SOCKS5 listener selected by `[Interface] Mode = proxy`. The raw editor continues to allow all core-supported configuration fields.

Acceptance: adding a valid SOCKS5 URI produces a core-compatible configuration; opening and saving either SOCKS5 source preserves its protocol and credentials; Shadowsocks sources remain supported. The upstream SOCKS5 connection is unencrypted. In VPN mode the remote server must support TCP CONNECT and UDP ASSOCIATE; local proxy mode needs CONNECT.

Packages use core release `v0.1.0-beta.2`, which supports remote SOCKS5. The earlier `v0.1.0-beta.1` rejects `SOCKS5` as an unknown option.
