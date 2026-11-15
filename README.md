# <img src="assets/brand/bimeron.png" alt="" width="32" height="32" align="absmiddle"> Bimeron Desktop

Bimeron Desktop is the Windows and Linux app for Bimeron: add an access profile, then connect
your computer through it in one click.

[![CI](https://github.com/bimeron/bimeron-desktop/actions/workflows/ci.yml/badge.svg)](https://github.com/bimeron/bimeron-desktop/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/bimeron/bimeron-desktop?include_prereleases)](https://github.com/bimeron/bimeron-desktop/releases)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

The app is a Qt 6 interface for [Bimeron Core](https://github.com/bimeron/bimeron-core). Core runs
as a system service and handles the tunnel, routing and DNS; the app manages profiles and the
connection without administrator rights. Each package includes a matching Core.

**Start here:** [Install](#install) · [Features](#features) · [Build from source](#build-from-source) · [Documentation](docs/README.md)

## Install

Download a package from [Releases](https://github.com/bimeron/bimeron-desktop/releases). Current
versions are pre-releases.

| System | Architecture | Package |
| --- | --- | --- |
| Windows 10 or 11 | x64 | `bimeron-desktop-<version>-windows-amd64.msi` |
| Windows 10 or 11 | ARM64 | `bimeron-desktop-<version>-windows-arm64.msi` |
| Ubuntu 22.04 or newer | amd64 | `bimeron-desktop-<version>-linux-amd64.deb` |
| Ubuntu 22.04 or newer | arm64 | `bimeron-desktop-<version>-linux-arm64.deb` |

The packages are not code-signed, so Windows may warn about an unknown publisher. Compare the
file's SHA-256 with the value in the release notes before installing.

- **Windows:** run the MSI. The options page adds a Start menu shortcut and launches the app
  after installation by default; a desktop shortcut is optional.
  The default location is `C:\Program Files\Bimeron`.
- **Linux:** install with `sudo apt install ./bimeron-desktop-<version>-linux-<arch>.deb`. The
  Core service needs systemd and polkit. Newer Debian-based systems with compatible libraries
  are expected to work but are not tested.

On first launch, Bimeron checks the Core service. If the service is missing or not working,
the service dialog shows the cause and restores it after one administrator prompt.

### Upgrade

Install the newer package over the existing one. Profiles and settings are kept. If the
installed Core service is older than the Core in the package, or is the same version but a
different build, the installer updates it; an active VPN connection is interrupted, so reconnect
afterwards. A newer service is left unchanged.

### Uninstall

Removing the app keeps the Core service installed and running, as well as your profiles and
settings. To remove the service, run `bimeron-core service uninstall` before removing the app:
on Windows from an administrator command prompt in `C:\Program Files\Bimeron`, on Linux with
`sudo`.

## Features

- **Profiles from links or files.** Paste a link or open a `.conf` profile. Supported links:
  `ss://`, `ssconf://`, `socks5://`, `vless://`, `hysteria2://`, `hy2://`, `vmess://`,
  `trojan://`, `tuic://`, `http://`, `https://`, `anytls://`, `naive+https://`,
  `naive+quic://`, `masque://` and `ssh://`.
- **Profile editor.** Create a profile from scratch or edit settings field by field; a raw
  editor is always available. Core checks every profile before it is saved, and problems point
  to the field that needs a fix.
- **Routing by country and site.** Send traffic through the tunnel, directly or block it using
  GeoIP and GeoSite groups. The databases are bundled and can update automatically.
- **Reliable connection.** After a dropped connection, Bimeron retries up to three times and
  explains the last error.
- **Service repair.** One dialog diagnoses the Core service, restores it and exports a
  diagnostic report.
- **Signed Core updates.** The app checks GitHub Releases for a newer compatible Core, verifies
  its signature and asks before replacing the running service. Beta Core releases are offered
  only after you enable them.
- **Tray and startup.** Connect, switch profiles and quit from the tray. Start with the system
  and connect a chosen profile automatically. Closing the window keeps the connection running.
- **English and Russian**, with light, dark and system themes.

The app talks to the Core service over a local channel (a named pipe on Windows,
`/run/bimeron/control.sock` on Linux). Profiles are stored in the user's application data
directory and readable only by the owner.

## Build from source

The app is C++20 with Qt 6 Widgets and builds with CMake presets for four targets:

```powershell
cmake --preset windows-amd64-release
cmake --build --preset windows-amd64-release
```

```bash
cmake --preset linux-amd64-release
cmake --build --preset linux-amd64-release
```

The `windows-arm64-release` and `linux-arm64-release` presets build ARM64. Windows needs
Visual Studio Build Tools and Qt 6.8.3; Linux builds against Ubuntu 22.04's system Qt 6.2.
To build all four MSI and DEB packages, run `./scripts/package-all.ps1` on Windows with WSL.

Requirements, the package layout and release checks are in
[Building and packaging](docs/build-and-packaging.md).

## Documentation

The [documentation index](docs/README.md) lists feature descriptions, the build guide and
release notes. Internal specifications and plans are in [docs/dev](docs/dev/README.md).

## License

Bimeron Desktop is licensed under the [MIT License](LICENSE).

Packages also contain third-party components under their own licenses:

- [Bimeron Core](https://github.com/bimeron/bimeron-core) (MIT, with parts under MPL 2.0) and its
  `THIRD_PARTY_NOTICES.txt`;
- Wintun on Windows, under its Prebuilt Binaries License;
- Qt 6 libraries on Windows, under LGPL-3.0;
- Monocypher and miniz, compiled into the app;
- country and site databases from
  [Loyalsoldier/v2ray-rules-dat](https://github.com/Loyalsoldier/v2ray-rules-dat), under GPL-3.0.

The packages install the license files of Core, Wintun, Monocypher, miniz and the databases.
See [third_party](third_party/README.md) for the sources of the vendored components.
