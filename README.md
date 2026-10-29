# Bimeron Desktop

Cross-platform Bimeron GUI client built with C++20 and Qt 6 Widgets.

Draft specification for a separate native macOS client on Swift/SwiftUI:
[`docs/macos-app-spec.md`](docs/macos-app-spec.md). It describes application behavior,
core integration options, and acceptance criteria. The macOS GUI is not implemented yet.

## Build

Click the Desktop version in About five times to restart into a session-only
Development tab. Choose System service or browse to a local Core executable and
click Apply. Windows asks for UAC once when Development opens and keeps that GUI
elevated for the session; Linux elevates individual Core operations. Switching
disconnects the VPN. Windows needs
the matching `wintun.dll` beside the local executable. Quit shuts down the local
daemon and restores a service that this session stopped. Closing to the tray
keeps the session; the next normal launch hides Development. Theme/language
restarts also end the developer session. The last valid selected Core path is
remembered for the next unlock, but never started automatically. See [developer mode](docs/developer-mode.md)
for lifecycle details and crash limitations.
The Development tab shows the selected local Core file's modification date and time, not its compilation time.

```powershell
cmake --preset windows-amd64-release
cmake --build --preset windows-amd64-release

cmake --preset windows-arm64-release
cmake --build --preset windows-arm64-release
```

All build artifacts are stored in separate directories by platform and architecture:
`build/windows-amd64`, `build/windows-arm64`, `build/linux-amd64` and
`build/linux-arm64`. Qt libraries for Windows are copied automatically.
GUI builds work without Bimeron Core. To bundle it for local testing, run
`cmake -DBIMERON_CORE_PLATFORM=windows -P scripts/fetch-core.cmake` on Windows
or use `linux` on Linux before configuring CMake. The packaging scripts do
this automatically and require the complete core payload. Core is named
`bimeron-core.exe` on Windows or `bimeron-core` on Linux. Windows packages
also include the matching Wintun DLL and license files from the core release.
Before linking, the build stops a GUI instance running from the executable
being updated. Unsaved changes in the open editor are lost.
The core service and VPN session continue to work.

Linux uses the same CMake project and the Qt 6 system package (`qt6-base-dev`).
The commands are executed inside Linux or WSL:

```bash
cmake --preset linux-amd64-release
cmake --build --preset linux-amd64-release

cmake --preset linux-arm64-release
cmake --build --preset linux-arm64-release
```

Linux ARM64 cross compilation also requires `g++-aarch64-linux-gnu` and
ARM64 Qt 6 development packages. Windows ARM64 requires Qt 6.8.3 kit
`msvc2022_arm64`.

## Packages

Full description of the environment, artifact structure, MSI and DEB composition, updates,
testing and diagnostics is in
[`docs/build-and-packaging.md`](docs/build-and-packaging.md).

```powershell
./scripts/package-all.ps1
```

On Linux, run `bash scripts/package-linux.sh` for the two DEBs. The Windows
script collects all four packages and requires WSL. Without WSL, use
`./scripts/package-all.ps1 -WindowsOnly` for both MSI packages. Clean installation contents
falls into `stage/<platform>-<architecture>`, and ready-made MSI and DEB - into `dist`.
Portable WiX for MSI creation is downloaded automatically into an ignored directory
`.tools`; System installation and administrator rights are not required.
The names of ready-made packages are all lowercase, for example
`bimeron-desktop-0.1.0-windows-amd64.msi`.

MSIs use a permanent upgrade GUID, so the package with the newer version
updates an existing installation. DEB is updated by standard installation of the package with
higher version via `apt` or `dpkg`. User settings are
separately from the program directory and are not deleted during updating.

The installed core service can update independently from a compatible signed
`bimeron/bimeron-core` GitHub Release. The GUI checks and downloads quietly,
then asks before replacing the service because an active VPN will disconnect.
Package builds include the production Ed25519 public key and a core with the
trusted `-source` update helper. Beta core updates require opting in for the
current GUI session. The installed package directory must not be writable by
ordinary users. Existing unsigned releases cannot be installed.
See [the update contract](docs/core-updates.md).

When you first start the GUI automatically installs the missing core service,
enables its launch along with the system and starts it. On Windows, UAC is used,
On Linux bundled core is installed as a systemd service via `pkexec`. Service
runs as root, and the Unix socket with mode `0600` belongs to the one who installed it
user; Normal VPN management does not require elevation. Service autostart
does not connect VPN without explicitly selecting a profile. To install on Linux you need systemd
and polkit. In WSL, the GUI uses the standard `wsl.exe -u root`, since the owner
Windows already administers its distribution; separate request for Linux password there
not shown. You can manage the service manually on the settings page.

Windows registers the application under the name `Bimeron`. Before installing MSI
shows English page of advanced options: shortcut in Start menu
and launch after installation are enabled by default, the desktop shortcut is disabled.

AppImage is not a system installer and is not yet created: it has
separate delivery and update model. It should be added when accepted
decision to support distributions outside of Debian/Ubuntu.

The application connects to the `BimeronCore` service via a Windows named pipe or to
`/run/bimeron/control.sock` on Linux. Profiles are located in the data directory
applications and receive only the rights of the owner. Closing the window does not stop the VPN,
unless the user has explicitly enabled the appropriate setting.

The profile dialog accepts `ss://`, `ssconf://`, `socks5://`, `vless://`, `hysteria2://`, and `hy2://` links. Import, editing, and runtime support are separate: see the [Core/Desktop integration contract](docs/core-desktop-integration-contract.md). The current navigation baseline disables connection controls and regular IPC polling. The bundled Core `v0.2.0-beta.1` supports VLESS but predates Hysteria2; validation by a different selected CLI does not establish support in the installed service.

See [Shadowsocks extensions](docs/shadowsocks-profiles.md) for SS2022 and embedded plugin editor support and Core requirements.

SSH integration targets Core v0.3.0-beta.1; see [SSH profiles](docs/ssh-profiles.md)
for Desktop behavior, the older bundled Core limitation and verification scope.
See [Protocol URI import](docs/protocol-uri-import.md) for stage 5 URI coverage, Raw editing and target Core requirements.

Stage 6 adds explicit Trojan, HTTP(S), Naive H2 and MASQUE forms and a separate
ShadowTLS v3 block for represented compatible explicit profiles. URI sources and
extended XHTTP remain protected Raw. See [Parameter forms and ShadowTLS](docs/parameter-forms-and-shadowtls.md).
This does not change the bundled Core or establish runtime interoperability.
