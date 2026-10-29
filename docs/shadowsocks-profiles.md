# Shadowsocks extensions

The Core/Desktop integration contract is authoritative. The integration target is Core `v0.3.0-beta.1`, revision `9f527efe0a4959786084675f1ff795edb8a0d910`. Explicit Shadowsocks supports the three SS2022 methods, AES SIP023 identity chains, and embedded `v2ray-plugin` / `obfs-local`. The selected CLI validates key lengths, combinations and SIP003 options. URI sources remain opaque and are never regenerated from previews.

The cipher selector includes the four legacy methods and `2022-blake3-aes-128-gcm`, `2022-blake3-aes-256-gcm`, `2022-blake3-chacha20-poly1305`. SS2022 keys use standard base64: 16 decoded bytes per AES-128 key, 32 for AES-256 and ChaCha20. AES accepts colon-separated identities ending in the user key; ChaCha20 accepts one key. Padding and colons are preserved. Invalid keys are rejected by Core without replacing the profile.

Plugin options are preserved literally, without inserting defaults. `tls=false` enables v2ray-plugin TLS by presence. Its host controls HTTP Host and verified TLS SNI independently of the endpoint. Simple-obfs TLS is camouflage, not verified cryptographic TLS. Plugins require matching servers; VPN captured DNS uses TCP and other proxied UDP is dropped. Direct routes remain unchanged. No external executable is launched.

The plugin selector offers no plugin, `v2ray-plugin`, and `obfs-local` (the Core name for simple-obfs). The masked options field accepts the SIP003 string; use protected Raw Show/Edit to inspect it. Options belong only to explicit SS; URI, Key, File and SSConf carry their own plugin configuration.

- v2ray-plugin accepts `mode=websocket`, presence flag `tls`, `host`, `path`, `mux`; defaults are `cloudfront.com`, `/`, and `1`. Mux `0` disables multiplexing. Omitting options retains Core defaults.
- obfs-local requires `obfs=http|tls`; `obfs-host` defaults to `cloudfront.net`. Only HTTP accepts `obfs-uri` and `http-method` (defaults `/` and `GET`). Unknown/duplicate options, malformed escapes, executable paths and plugin + ShadowTLS are rejected by Core.

SS2022 rejects Prefix. Invalid combinations retain their draft. Unknown methods/plugins, mixed sources and ShadowTLS remain protected Raw configurations. No-op saves preserve bytes; validation or atomic-write failure preserves the original. Secrets remain masked and diagnostics redacted.

Acceptance: round-trip all seven ciphers, AES identity chains, plugin options and escapes; preserve URI encoding and source; reject incompatible configurations through Core; retain Raw guards and existing connection lifecycle. Test with the exact reviewed Core via BIMERON_PRESERVATION_CORE. The bundled v0.2.0-beta.1 does not support these extensions. CLI acceptance is not runtime interoperability evidence.

Stage 4 owns SS editor branches. Stage 5 owns generic URI dispatch/import integration; stage 7 owns SSH. Integrate shared editor guards, tests and translations sequentially, recommended order 5, 4, 7. No shared importer/model refactor, bundle update or release is included.

Before release, coordinate one shared bundle upgrade for all stages: published assets, four archive hashes, signed manifests and compatibility gates. This is a separate dependency, not a stage 4 change. Desktop editor support, target Core validation and actual runtime interoperability are separate evidence.

## Verification (October 29, 2026)

Windows amd64 and Linux amd64 passed all three existing CTest executables. Windows ARM64 and Linux arm64 were cross-compiled; no native ARM64 runtime pass is claimed. Windows contract fixtures passed with Core v0.3.0-beta.1, revision 9f527efe0a4959786084675f1ff795edb8a0d910, modified=false, covering AES chains, encoded plugin URI sources, defaults, escapes and invalid combinations. The older bundled v0.2.0-beta.1 CLI rejected the same synthetic SS2022/plugin URI accepted by the target CLI. No installed service or system network was changed.

EN/RU editor fixture renders at 100% and 150% fit the fields and buttons with credentials masked. Generate a synthetic preview with `profile-vless-test --ss-preview <image.png> en|ru` and `QT_SCALE_FACTOR=1|1.5`. These fixtures do not include application resources/styles and do not replace installed-package appearance checks.

Diff review checked source preservation, unknown-plugin Raw protection, Prefix conflicts, option omission and secret handling. No data-plane or polling change requires a benchmark. Live WS/WSS/obfs connectivity, VPN DNS/UDP policy, native ARM64 tests and release compatibility remain unverified. No package or release was produced.
