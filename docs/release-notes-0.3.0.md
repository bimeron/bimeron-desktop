Bimeron Desktop 0.3.0 pre-release for Windows and Debian/Ubuntu, amd64 and ARM64.

## Highlights

- Unified profile import and creation, with explicit protocol settings and structured Core problems that navigate to the affected editor field.
- One-click Core repair through a protected helper included in the package: no separate helper download, with a single elevation prompt per repair attempt.
- Shared GeoData routing with categorized group selection, bundled Loyalsoldier databases and automatic database updates.

## Changes

- Explicit profile settings cover `shadowsocks`, `socks5`, `http`, `https`, `vless`, `ssh`, `trojan`, `naive`, `masque`, `hysteria2`, `tuic`, `anytls`, `vmess` and `naive_quic`. The new Hysteria2, VMess, TUIC, AnyTLS and Naive QUIC forms require a Core advertising the corresponding capabilities.
- Service diagnostics show repair progress, details and an exportable report.
- Simplified startup selection and database management in Settings.
- Developer mode can convert access sources between links and explicit fields. About identifies non-release Core builds.

## Fixes

- Preserve ShadowTLS drafts when editing profiles.
- Bring Windows UAC consent to the foreground.
- Avoid a profile editor crash on Qt 6.4.2 when changing visible form rows.
- Fill missing Russian translations and clarify database validation states.

## Bundled Core

Bimeron Core `v0.4.0-beta.1`, commit `e1cada7d`.

## Packages

Choose the MSI for Windows or DEB for Debian/Ubuntu matching your architecture. SHA-256:

| File | SHA-256 |
|---|---|
| `bimeron-desktop-0.3.0-windows-amd64.msi` | `21eacb49c9223b72fd52bed1ec53dfce23d0b27682a998205186ad14e2cad988` |
| `bimeron-desktop-0.3.0-windows-arm64.msi` | `f09e3b13e816d706a82eeb61d7417c9579fce8a87514f0820f3249644f84a248` |
| `bimeron-desktop-0.3.0-linux-amd64.deb` | `058228b8bfe190eb9f0289f15ffc4d0e16ec355d77a70bf5eff0cca845029bb2` |
| `bimeron-desktop-0.3.0-linux-arm64.deb` | `375a8a36a8923952b80c6963e31f22a281e3dc191318df42c60a62f440881275` |

## Upgrade notes

- Upgrade from Desktop 0.2.0 by installing the matching 0.3.0 package over it.
- Installing the Desktop package does not itself replace the independently installed Core service.
- The bundled Core's signed update metadata accepts the GUI range `[0.3.0, 0.3.1)`. Desktop 0.2.0 is outside that range; install Desktop 0.3.0 first. Core updates must also pass signature and compatibility checks.

## Known limitations

- This is a pre-release. Live Windows x64 UAC and Linux amd64 polkit repair acceptance, including upgrade from 0.2.0, remains pending; publication is blocked until it passes.
- ARM64 packages are built and compile/package-verified, but have not been live-tested.
- Windows x64 and Linux amd64 local test suites passed (10/10 each); package payloads and architectures were inspected. These checks do not establish live installation, VPN or repair behavior.
- The bundled Core candidate is frozen but not yet tagged or published; publication remains subject to release acceptance.
