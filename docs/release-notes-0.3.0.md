Bimeron Desktop 0.3.0 pre-release for Windows and Debian/Ubuntu, amd64 and ARM64.

## Highlights

- Unified profile import and creation, with explicit protocol settings and structured Core problems that navigate to the affected editor field.
- One-click Core repair through a protected helper included in the package: no separate helper download, with a single elevation prompt per repair attempt.
- Bounded reconnection: after a connection drop Bimeron retries up to three times, shows the attempt as `Reconnecting… (1/3)` and explains the last error, including its code, in a tooltip.
- Shared GeoData routing with categorized group selection, bundled Loyalsoldier databases and automatic database updates.

## Changes

- Explicit profile settings cover `shadowsocks`, `socks5`, `http`, `https`, `vless`, `ssh`, `trojan`, `naive`, `masque`, `hysteria2`, `tuic`, `anytls`, `vmess` and `naive_quic`. The new Hysteria2, VMess, TUIC, AnyTLS and Naive QUIC forms require a Core advertising the corresponding capabilities.
- Service diagnostics show repair progress, details and an exportable report.
- Simplified startup selection and database management in Settings.
- Developer mode can convert access sources between links and explicit fields. About identifies non-release Core builds.
- Linux packages now target Ubuntu 22.04 (Qt 6.2) and newer.
- The application appears as "Bimeron" in operating system menus and app lists.

## Fixes

- Service diagnostics no longer report a running service as not installed or as an invalid response.
- Connecting no longer flashes a "Failed to prepare the connection" error before succeeding.
- Repair keeps elevation failure details in diagnostics.
- Preserve ShadowTLS drafts when editing profiles.
- Bring Windows UAC consent to the foreground.
- Avoid a profile editor crash on Qt 6.4.2 when changing visible form rows.
- Restore the Settings background and form spacing on Qt 6.2.
- Fill missing Russian translations and clarify database validation states.

## Bundled Core

Bimeron Core `v0.4.0-beta.1`, commit `0e08a5c8`.

## Packages

Choose the MSI for Windows or the DEB for Debian/Ubuntu matching your architecture. SHA-256:

| File | SHA-256 |
|---|---|
| `bimeron-desktop-0.3.0-windows-amd64.msi` | `6e9d18b64aa581a5dd56b961a077bb0679072bb22b8ae7d87d2577eaaa766140` |
| `bimeron-desktop-0.3.0-windows-arm64.msi` | `2d1c26e897daa3b91817f1e59c9ccf6c8c2a6a26a7a8b603d0863c239f00a600` |
| `bimeron-desktop-0.3.0-linux-amd64.deb` | `543030fb1f511f375c49dd846d6fc4909dc9a65dd277e0252c6c132a023d4489` |
| `bimeron-desktop-0.3.0-linux-arm64.deb` | `2fe7adaa13616ea7562d891dd5ccabc197527473b94d696fc5b6f9a49499fe19` |

The packages are not code-signed. Windows may show an unknown-publisher warning; verify the SHA-256 above before installing.

## Upgrade notes

- Upgrade from Desktop 0.2.0 by installing the matching 0.3.0 package over it. Profiles and settings are kept.
- Installing the Desktop package does not replace the running Core service. Bimeron offers the Core update through its signed update check; Windows may ask to restart after the upgrade, but this is not required for Bimeron to work.
- Core `v0.4.0-beta.1` update metadata accepts Desktop `[0.3.0, 0.3.1)`. Desktop 0.2.0 does not receive this Core; upgrade Desktop first.

## Uninstalling

Removing the Desktop package keeps the Bimeron Core service installed and running, as in 0.2.0. To remove the service too, run `bimeron-core service uninstall` from an administrator command prompt (Windows, in `C:\Program Files\Bimeron`) or with `sudo` (Linux) before removing the Desktop package. User profiles and settings are kept.

## Known limitations

- This is a pre-release.
- ARM64 packages are built and package-verified but have not been tested on ARM64 hardware.
