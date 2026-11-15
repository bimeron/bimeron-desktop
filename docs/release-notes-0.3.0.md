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
- A profile can be deleted after its connection failed or ended.
- The service dialog keeps its title bar on screen on Windows 11.
- Stopping after a failed reconnection completes cleanly, so service repair works afterwards.
- Repair replaces a damaged Core service executable, works on the first attempt after the service crashed, and completes when another administrator approves the elevation prompt.
- Repair restores a missing repair helper from the protected copy inside the application.
- Connecting no longer fails with "Tunnel name is already in use" after an interrupted session left a stale Bimeron adapter behind.
- Profiles saved with a UTF-8 byte order mark import correctly.
- Preserve ShadowTLS drafts when editing profiles.
- Bring Windows UAC consent to the foreground.
- Avoid a profile editor crash on Qt 6.4.2 when changing visible form rows.
- Restore the Settings background and form spacing on Qt 6.2.
- Fill missing Russian translations and clarify database validation states.

## Bundled Core

Bimeron Core `v0.4.0-beta.1`, commit `5fcadd71`.

## Packages

Choose the MSI for Windows or the DEB for Debian/Ubuntu matching your architecture. SHA-256:

| File | SHA-256 |
|---|---|
| `bimeron-desktop-0.3.0-windows-amd64.msi` | `feafe5d3c0dfe45a226c8a05b9280ae1a84c2b98e995c982cbf4131beddac3f8` |
| `bimeron-desktop-0.3.0-windows-arm64.msi` | `e3b420d92c74193930a808d6edf207959836a403a4ed33f191fc368c29de8758` |
| `bimeron-desktop-0.3.0-linux-amd64.deb` | `65e2ab890c2197dd73c54b4e762373db98a2d9f27b378ca279e973055b44a747` |
| `bimeron-desktop-0.3.0-linux-arm64.deb` | `03f8c490f651b74b4208af5120aedc5c25eba8a02157591bd5c3c9b1716e25e4` |

The packages are not code-signed. Windows may show an unknown-publisher warning; verify the SHA-256 above before installing.

## Upgrade notes

- Upgrade from Desktop 0.2.0 by installing the matching 0.3.0 package over it. Profiles and settings are kept.
- If the installed Core service is older than the bundled Core, the installer updates it to the bundled version. An equal-version service with different executable bytes is also updated (full revision is the fallback when its hash is unavailable). Identical builds and unknown identities are skipped; a newer service is left unchanged. An active VPN connection is interrupted during this update; reconnect afterwards. Windows may ask to restart after the upgrade, but this is not required for Bimeron to work.
- Core `v0.4.0-beta.1` update metadata accepts Desktop `[0.3.0, 0.3.1)`. Desktop 0.2.0 does not receive this Core; upgrade Desktop first.

## Uninstalling

Removing the Desktop package keeps the Bimeron Core service installed and running, as in 0.2.0. To remove the service too, run `bimeron-core service uninstall` from an administrator command prompt (Windows, in `C:\Program Files\Bimeron`) or with `sudo` (Linux) before removing the Desktop package. User profiles and settings are kept.

## Known limitations

- This is a pre-release.
- The Windows ARM64 package was tested in a Windows 11 ARM64 virtual machine. The Linux arm64 package is built and package-verified but has not been tested on ARM64 hardware.

## Licenses

Bimeron Desktop is MIT-licensed. Windows packages include Qt 6.8.3 under the LGPL-3.0; see `QT-NOTICE.txt` in the installation directory.
