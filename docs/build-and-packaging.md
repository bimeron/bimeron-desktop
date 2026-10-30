# Building and packaging Bimeron Desktop

This document is the canonical description of building and packaging Bimeron Desktop. Update it in the same commit when CMake presets, dependencies, package structure, installer behavior, or packaging scripts change.

## Supported results

The project produces four system packages:

| Platform | Architecture | CMake-preset | Result |
|---|---|---|---|
| Windows | amd64 | `windows-amd64-release` | `bimeron-desktop-<version>-windows-amd64.msi` |
| Windows | ARM64 | `windows-arm64-release` | `bimeron-desktop-<version>-windows-arm64.msi` |
| Debian/Ubuntu | amd64 | `linux-amd64-release` | `bimeron-desktop-<version>-linux-amd64.deb` |
| Debian/Ubuntu | arm64 | `linux-arm64-release` | `bimeron-desktop-<version>-linux-arm64.deb` |

The version is specified once in `project(BimeronDesktop VERSION ...)` of the `CMakeLists.txt` file. Before release, change it there, rebuild all four packages and make sure that the version appears in their names and metadata. Windows uses `amd64` in project-owned names while Visual Studio and WiX still use their required `x64` architecture identifier.

The desktop source version is `0.2.0` for the VLESS GUI changes. Normal packaging pins published Core `v0.2.0-beta.1` from commit `6e0c171505be8e349d5f42570f7ceb3e50ac6697` and its four Windows/Linux release ZIP hashes. Its production-key-signed manifest has `gui_reviewed: true`, GUI `[0.2.0, 0.2.1)`, IPC 1, and profile format 1. Before approving a Desktop release, build and inspect all four packages against those published archives, then test the normal GitHub release source. A package upgrade does not itself replace the installed service.

The Windows amd64 `0.2.0` VM package, its Core payload, signed GUI-driven update, installed-service rollback, IPC `hello`, and automatic VM profile reconnection results are recorded in [the VM check](vm-core-0.2.0-beta.1-check.md). The positive signed GUI path was repeated with the final Core assets through a local release-list override in the test build. Tampering and direct-helper rollback with those final bytes passed; the other installed package targets remain untested. The normal installed GUI discovered and installed the published Core through GitHub Releases in the Windows amd64 VM; see the VM check.

For isolated GUI update testing, configure a separate build with `-DBIMERON_LOCAL_UPDATE_TESTING=ON`. With `BUILD_TESTING=ON`, also provide `-DBIMERON_LOCAL_UPDATE_TEST_PUBLIC_KEY=<ephemeral-test-public-key>` for the optional feed fixture executable; its private seed stays in ignored local build state and never enters the MSI. Its MSI name carries `-local-update-test`, and it reads only the local release list named by `BIMERON_TEST_RELEASES_JSON`; it accepts only local assets beneath that list's directory. Never use this test build for release packaging. The normal configuration keeps GitHub Releases and the pinned production public key. The Windows amd64 test MSI hash and VM results are in [the VM check](vm-core-0.2.0-beta.1-check.md). See [the signed update specification](core-updates.md) for acceptance criteria.

The normal `0.2.0` build pinned the published `v0.2.0-beta.1` archives and produced both MSI and both DEB packages. Windows amd64 and Linux amd64 `core-update-test` and `profile-vless-test` passed; both ARM64 targets compiled and packaged without runtime testing. The staged Windows amd64 Core SHA-256 is `174c288a5771d55c48e446fce3deef1f6fc7092d615f8277b5cc2aac75486f0f`, matching the verified release archive. A normal installed-GUI update through GitHub Releases passed in the Windows amd64 VM. The detailed outcome text was tested from a protected install; all four packages were then rebuilt and checked, and the clean Windows amd64 MSI was installed in the VM. The later profile-list changes require another package build before release.

The local `0.1.5` build produced all four packages with `scripts/package-all.ps1`. Windows amd64 and Linux amd64 tests passed; Windows and Linux ARM64 packages compiled without runtime testing. MSI metadata reports ProductVersion `0.1.5`; both DEBs report Version `0.1.5` and their target architecture. Visual validation of the 720 by 600 minimum client area at 150% scaling and a live MSI upgrade from `0.1.4` remain outstanding.

After pinning published core `v0.1.0-beta.5`, the full four-package script completed again. Windows amd64 and Linux amd64 `core-update-test` passed; both ARM64 targets compiled and packaged without runtime testing. The published manifest signature verified with the pinned key, and its four ZIP hashes agreed with `SHA256SUMS`. Extracted MSI and DEB core payload hashes match the verified archives for each architecture. Both MSI files report ProductVersion `0.1.5` and retain the Bimeron UpgradeCode; both DEBs report Version `0.1.5` and the correct architecture. A live GUI-driven update to `beta.5`, GUI visual validation, installer upgrade testing, and ARM64 runtime checks remain outstanding.

The local `0.1.4` build produced all four packages with `scripts/package-all.ps1`. Windows amd64 and Linux amd64 tests passed; Windows and Linux ARM64 packages compiled without runtime testing. MSI metadata reports ProductVersion `0.1.4`; both DEBs report Version `0.1.4` and their target architecture. Visual checks at 100% and 150% scaling and a live MSI upgrade from `0.1.3` remain outstanding.

The local `0.1.3` build produced all four packages with `scripts/package-all.ps1`. Windows amd64 and Linux amd64 tests passed; both ARM64 targets compiled and packaged without runtime testing. Both DEBs report Version `0.1.3` and their target architectures. Windows MSI upgrade and theme appearance on a Windows 11 device still require manual validation.

The local `0.1.2` build produced all four packages. Windows amd64 and Linux amd64 tests passed; Windows and Linux ARM64 packages compiled without ARM64 runtime testing. MSI metadata reports ProductVersion `0.1.2` and the unchanged UpgradeCode `{E93DE42D-1F4F-4227-8056-1086581CCE51}`. Both DEBs report Version `0.1.2` and their target architecture. A live `0.1.1` → `0.1.2` MSI upgrade on Windows 10 STD replaced the GUI executable, preserved the installed core service executable and running service, and left VPN connected. The unattended install closed the GUI; reopen it for visual confirmation. DEB upgrade behavior and ARM64 runtime remain untested.

AppImage, `setup.exe`, WinGet manifests and APT repositories are not currently supported. They should not be considered part of the release process.

## Purpose of directories

All directories are generated locally and excluded from Git:

- `build/<platform>-<architecture>` - CMake cache, object files and intermediate compilation results;
- `stage/<platform>-<architecture>` - the exact file tree that is installed by the corresponding package;
- `dist` - only ready-made `.msi` and `.deb` intended for publication;
- `.tools/wix` - automatically downloaded portable WiX Toolset;
- `.tools/core` - downloaded and verified core release archives and extracted payloads;
- `/tmp/bimeron-desktop-packages` inside WSL/Linux - temporary CPack platform for DEB.

`build` and `stage` are not published and should not be included in the installer as separate directories. `dist` should not contain `_CPack_Packages`, unpacked payloads or old Bimeron packages. The full Windows script clears old Bimeron packages before building. `-WindowsOnly` also clears them and leaves two MSI files. The Linux script replaces old Linux DEBs but leaves any existing MSI files; check their versions before publishing a combined set.

## Core release inputs

Both packaging scripts use the pinned Bimeron Core release in `cmake/core-release.cmake`. Published `v0.2.0-beta.1` includes the required `-source` helper. Its four Windows/Linux ZIP hashes match the published manifest and `SHA256SUMS`; pin the actual published hashes and keep the helper guard enabled. `scripts/fetch-core.cmake` verifies the pinned archives on Windows and Linux. A sibling core checkout is not required. The release tag and hashes must be updated together when upgrading core.

Acceptance criteria: a clean checkout can build the two DEBs on native Linux without PowerShell or WSL; Windows with WSL can build all four packages; Windows without WSL can build two MSI packages with `-WindowsOnly`; an archive that fails the pinned hash check cannot be packaged. Each installed core binary must match the verified published archive for its architecture.

Expected input files:

| Build | Bimeron Core | Additional files |
|---|---|---|
| Windows amd64 | `.tools/core/windows-amd64/bimeron.exe` | `wintun.dll`, licenses |
| Windows ARM64 | `.tools/core/windows-arm64/bimeron.exe` | ARM64 `wintun.dll`, licenses |
| Linux amd64 | `.tools/core/linux-amd64/bimeron` | third-party notices |
| Linux arm64 | `.tools/core/linux-arm64/bimeron` | third-party notices |

When core is present, the build bundles it as `bimeron-core.exe` on Windows or `bimeron-core` on Linux. The GUI looks for this exact name. GUI-only builds can omit core; package builds use `BIMERON_REQUIRE_CORE=ON` and fail if any required payload file is missing.

The fetch script checks cached archive hashes on every run, downloads missing or changed archives, verifies the pinned SHA-256, extracts them, and checks required files. A clean checkout needs access to GitHub. With verified archives already in `.tools/core`, the fetch step can run offline. The installer does not download core. Separately, a GUI built with a trusted production release key can download signed core updates at runtime; see [Signed core service updates](core-updates.md).

### Updating the bundled core version

1. Inspect the desired published release of `bimeron/bimeron-core` and its `SHA256SUMS` file. Confirm it contains Windows and Linux archives for amd64 and arm64 with the expected binaries, Wintun files and licenses.
2. Change the tag and four matching SHA-256 values in `cmake/core-release.cmake`. Verify that the published helper supports `-source`, then set `BIMERON_CORE_RELEASE_SUPPORTS_SOURCE_UPDATE` to `TRUE`. Update the repository URL, target lists or filenames there only if the release format changed. Do not use a floating `latest` URL; prereleases may not be returned by GitHub's latest-release endpoint.
3. Fetch on both platforms, or run the full package scripts. Check that the extracted core binaries and Wintun DLLs match their target architectures.
4. Build all four packages and verify their payloads and metadata using the review steps below. Record the chosen core tag in the release notes. A core release does not change an already installed desktop package automatically. The installed service can be updated independently only from a compatible signed release after the user confirms installation.

### Signed service updates

`BIMERON_CORE_UPDATE_PUBLIC_KEY` is a CMake cache string containing the standard-base64 32-byte Ed25519 public key for `bimeron/bimeron-core` update manifests. Its default is the published `v0.1.0-beta.3` key (`bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0=`, key ID `7370a2d8cf890e5e`); the private key is never included. Existing build directories can cache an old empty override, so verify this cache value when rebuilding them. An empty override disables signed updates. Key rotation requires a GUI release that trusts the successor before core switches signing keys; the current implementation accepts one key and must be extended for overlapping keys before rotation.

The normal package scripts pin published core `v0.2.0-beta.1` from commit `6e0c171505be8e349d5f42570f7ceb3e50ac6697`. This helper supports `-source`. An older installed service can be replaced by the new bundled helper. The GUI refuses direct updates when its bundled helper lacks `-source` or its package path is writable by ordinary users. Earlier local packages bundled `v0.1.0-beta.3` from commit `ff489b8ee0a0f4394f381d5273d6a3d92dab48c1`; their service-update compatibility excludes GUI `0.1.5`.

The `ff489b8` prepared files were published without rebuilding. The downloaded GitHub assets matched the prepared bytes, the signed manifest, and `SHA256SUMS`. The GUI verified the exact manifest signature and extracted all four desktop archives. At that time, `cmake/core-release.cmake` pinned the four hashes of those published ZIPs. A normal package build must reset `BIMERON_CORE_ROOT` to `.tools/core` so a cached local override cannot package an older candidate. The four GUI `0.1.1` packages built after this correction contain core `ff489b8`; MSI and DEB payload hashes match the published binaries.

Local verification on Windows with WSL: the full four-package script completed; Windows amd64 and Linux amd64 `core-update-test` passed; both ARM64 targets compiled but were not run on ARM64 hardware. MSI extraction and DEB payload inspection matched the published core binaries. The package key was present in all four CMake caches, an empty-key package configuration failed, and the clean staging trees contained no test executable or library. A separate Windows 10 VM successfully updated a running old service to published `v0.1.0-beta.3` through GUI `0.1.1`; see `docs/core-updates.md`. Installer UI, a live GUI package upgrade, wrong-digest rejection, and IPC-failure rollback still require separate manual checks.

The GUI checks GitHub Releases, verifies `update-manifest.json.sig` against the exact bytes of `update-manifest.json`, checks release compatibility, then verifies and extracts the selected archive. It elevates only the protected core binary from the installed MSI/DEB and passes the downloaded executable as data with `service update -source <absolute-path> -version <version> -sha256 <executable-sha256>` using UAC or `pkexec`/WSL. The bundled core copies and verifies it before replacing the service. This update does not replace Wintun. The GUI reconnects the same active VPN profile after a successful service update and verified IPC hello. `docs/core-updates.md` defines the selection and failure policy. The GUI vendors Monocypher 4.0.2 for Ed25519 and miniz 3.0.2 for ZIP extraction; their licenses are included in each package.

## Environment requirements

### Windows

Current presets are designed for:

- Windows 10/11;
- PowerShell 7 or Windows PowerShell with support for the script used;
- CMake 3.21 or later;
- Visual Studio Build Tools 2026 with C++ toolchain installed in `C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools`;
- Qt 6.8.3:
  - x64: `C:/Qt/6.8.3/msvc2022_64`;
  - ARM64: `C:/Qt/6.8.3/msvc2022_arm64`;
- host tools for ARM64: `C:/Qt/6.8.3/msvc2022_64`;
- WSL with a Debian/Ubuntu-compatible distribution for the four-package Windows script;
- access to GitHub when the pinned core archives or WiX 3.14.1 are not cached locally.

WiX does not need to be installed globally. `scripts/package-all.ps1` downloads the official WiX 3.14.1 archive in `.tools/wix` and adds it to `PATH` for the current process only.

If Visual Studio/Qt versions or paths change, you must first update and check `CMakePresets.json`, then this document.

### Linux/WSL

The Linux presets are designed for an amd64 Debian/Ubuntu host. They build amd64 natively and cross-compile arm64. For this host you need at least:

```bash
sudo apt update
sudo apt install cmake ninja-build g++ qt6-base-dev qt6-base-dev-tools \
  qt6-translations-l10n dpkg-dev fakeroot
```

For ARM64 cross-compilation you additionally need:

```bash
sudo dpkg --add-architecture arm64
sudo apt update
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu \
  qt6-base-dev:arm64
```

The specific set of Qt multiarch packages may differ between Debian/Ubuntu versions. The main requirement of the preset: the ARM64 CMake configuration of Qt must be located through `/usr/lib/aarch64-linux-gnu/cmake`, and the `aarch64-linux-gnu-gcc` and `aarch64-linux-gnu-g++` compilers must be available in `PATH`.

Shell scripts are checked out with LF line endings through `.gitattributes`, including on Windows hosts configured with `core.autocrlf=true`; WSL Bash requires this for `scripts/package-linux.sh`.

The two-DEB script has not been configured for a native arm64 host. On that host, use a suitable toolchain and adjust the presets before relying on the result.

## Choose a build command

Run these commands from the repository root:

| Host and result | Command | Expected `dist` contents on a clean checkout |
|---|---|---|
| Windows with WSL, four packages | `./scripts/package-all.ps1` in PowerShell | Two MSI and two DEB |
| Windows without WSL, Windows packages | `./scripts/package-all.ps1 -WindowsOnly` in PowerShell | Two MSI |
| amd64 Debian/Ubuntu, Linux packages | `bash scripts/package-linux.sh` | Two DEB |

The scripts fetch core, explicitly configure CMake with `BIMERON_CORE_ROOT=.tools/core` and `BIMERON_REQUIRE_CORE=ON`, build, clear the target staging tree, stage and package. Explicitly setting the root prevents an earlier local candidate override cached in a build directory from entering a normal package; clearing the staging tree removes stale files from earlier manual installs. Package configuration rejects a cached empty public key; clear that override or pass the pinned key explicitly. For local candidate testing, configure the four presets manually with `BIMERON_CORE_ROOT` pointing to the verified payload. Run `cmake --preset <preset>` and `cmake --build --preset <preset>` without fetching core only when you want a GUI-only development build. If the same build directory was previously configured for packaging, pass `-DBIMERON_REQUIRE_CORE=OFF` to reconfigure it for GUI-only work.

## Complete build of all packages

From the repository root in PowerShell:

```powershell
./scripts/package-all.ps1
```

The default Windows script requires WSL and a working Linux distribution. It checks that requirement before touching `dist`. Without WSL, run `./scripts/package-all.ps1 -WindowsOnly` to build the two MSI packages. That mode fetches only the Windows core archives and leaves only the two MSI packages in `dist`. Build the DEBs on a Linux machine with `scripts/package-linux.sh`; if publishing all four together, copy its two finished DEBs into the Windows `dist` directory and verify that all four packages have the intended desktop and core versions.

The script performs the following process:

1. checks WSL unless `-WindowsOnly` was requested, then fetches and verifies the required core archives;
2. creates `dist` and removes old Bimeron `.msi`, `.exe` and `.deb` packages;
3. downloads portable WiX if necessary;
4. configures, builds, stages and packages Windows amd64 and ARM64 with `BIMERON_REQUIRE_CORE=ON`;
5. unless `-WindowsOnly` was requested, runs `scripts/package-linux.sh` in WSL for the two DEBs;
6. removes temporary `dist/_CPack_Packages` and displays the finished packages.

On native Linux, build both DEBs from the repository root with:

```bash
bash scripts/package-linux.sh
```

This fetches and verifies only the Linux core archives, builds both architectures and copies the final DEBs from `/tmp/bimeron-desktop-packages` into `dist`. It requires the Linux toolchains and Qt packages listed above; it does not require PowerShell or WSL. The script replaces old Linux DEBs in `dist` after both new packages are built.

If there is any error, the script ends and should not be considered a successful release. The full Windows script checks WSL and core downloads before clearing `dist`; later failures can leave only some new packages. The Linux script keeps existing `dist` files until both DEBs have been built, but a failure can still leave temporary packages under `/tmp`.

## Building one configuration

Fetch core first with `cmake -DBIMERON_CORE_PLATFORM=windows -P scripts/fetch-core.cmake` on Windows or `cmake -DBIMERON_CORE_PLATFORM=linux -P scripts/fetch-core.cmake` on Linux. Add `-DBIMERON_REQUIRE_CORE=ON` to the `cmake --preset` configure command for package builds. The commands below assume the core files have already been fetched.

The default payload directory is `.tools/core`. If it must be elsewhere, pass the same absolute `-DBIMERON_CORE_ROOT=<path>` to the fetch command and the CMake configure command. The automated package scripts use the default directory.

### Windows amd64

```powershell
cmake --preset windows-amd64-release -DBIMERON_REQUIRE_CORE=ON
cmake --build --preset windows-amd64-release
cmake --install build/windows-amd64 --config Release
cpack --preset windows-amd64-release -C Release
```

### Windows ARM64

```powershell
cmake --preset windows-arm64-release -DBIMERON_REQUIRE_CORE=ON
cmake --build --preset windows-arm64-release
cmake --install build/windows-arm64 --config Release
cpack --preset windows-arm64-release -C Release
```

To manually launch `cpack`, WiX 3 must be available in `PATH`. Run `package-all.ps1 -WindowsOnly` to prepare it without WSL, or temporarily add `.tools/wix` to `PATH` of the current PowerShell session.

### Linux amd64

```bash
cmake --preset linux-amd64-release -DBIMERON_REQUIRE_CORE=ON
cmake --build --preset linux-amd64-release
cmake --install build/linux-amd64
cpack --preset linux-amd64-release
```

### Linux ARM64

```bash
cmake --preset linux-arm64-release -DBIMERON_REQUIRE_CORE=ON
cmake --build --preset linux-arm64-release
cmake --install build/linux-arm64
cpack --preset linux-arm64-release
```

The manual Linux `cpack` commands create packages in `/tmp/bimeron-desktop-packages`, not directly in `dist`. Copy the finished DEB to `dist` after inspecting it, or use `scripts/package-linux.sh` to do that automatically. Staging on the Linux filesystem prevents a Windows filesystem mounted in WSL from reporting `0777` permissions for DEB contents.

## Windows MSI content

The `stage/windows-amd64` or `stage/windows-arm64` tree is the source of the MSI content. Next to `bimeron-desktop.exe` are installed:

- `bimeron-core.exe` of the same architecture;
- `wintun.dll` of the same architecture;
- necessary DLLs and Qt plugins;
- architecture-matched MSVC redistributable DLLs, including `VCRUNTIME140_1.dll`, copied from the installed Visual Studio toolchain;
- `translations/qt_ru.qm` for localized Qt buttons and dialogs;
- `WINTUN-LICENSE.txt`;
- `CORE-LICENSE.txt`;
- `THIRD_PARTY_NOTICES.txt`.
- `MONOCYPHER-LICENSE.txt` and `MINIZ-LICENSE.txt`.

The install rule excludes `core-update-test.exe`, `core-update.lib`, `profile-vless-test.exe`, and `developer-mode-test.exe` from the MSI even though they share the Visual Studio output directory with the GUI. Use a fresh `stage/upgrade-windows-*` prefix for local upgrade-test packages so previously staged test artifacts cannot survive an incremental install.

Wintun is not downloaded during installation or application launch: the required `wintun.dll` is included in the MSI in advance. ARM64 MSI uses the DLL from the ARM64 core release archive.

Installer:

- has an English interface;
- registers the product under the name `Bimeron`;
- offers installation directory;
- shows page `Additional options`;
- by default creates a shortcut in the Start menu;
- by default does not create a shortcut on the desktop;
- by default launches Bimeron after successful installation;
- uses the Bimeron-specific persistent `CPACK_WIX_UPGRADE_GUID` needed to update an existing Bimeron installation.

The MSI must default to `C:\Program Files\Bimeron\` and must not detect an unrelated RouteLine installation as an upgrade. An older local MSI accidentally reused RouteLine's upgrade GUID and offered `C:\Program Files\RouteLine\`; do not install that package. Bimeron has no published desktop releases, so the GUID was replaced before its first release. Do not change the new GUID after release: a different GUID makes Windows treat the package as a different product. A disposable Windows Sandbox test confirmed that the specific local `0.1.0` MSI replaces the bundled core when installed over the cached installed `0.1.0` MSI. GUI `0.1.1` also replaces it and has a higher ProductVersion, providing a clear package upgrade on Windows and Debian.

The `v0.1.5` GitHub pre-release distributes unsigned MSI packages with an explicit warning that Windows may show an unknown-publisher prompt. A future signed release requires a code signing certificate and a separate secure `signtool` step; the certificate and private key must not be stored in the repository.

## Linux DEB contents

DEB installs:

```text
/usr/bin/bimeron-desktop
/usr/bin/bimeron-core
/usr/share/applications/bimeron.desktop
/usr/share/icons/hicolor/64x64/apps/bimeron.png
/usr/share/doc/bimeron-desktop/THIRD_PARTY_NOTICES.txt
/usr/share/doc/bimeron-desktop/CORE-LICENSE.txt
/usr/share/doc/bimeron-desktop/MONOCYPHER-LICENSE.txt
/usr/share/doc/bimeron-desktop/MINIZ-LICENSE.txt
```

The Linux window uses the bundled PNG icon (Qt's system ICO image plugin is not guaranteed to be installed) and declares the `bimeron` desktop file name so the dock can match the window to `bimeron.desktop`.

In Settings, `Start Bimeron with Linux` enables application launch at user login by writing `bimeron.desktop` to the user's XDG autostart directory (`~/.config/autostart` by default). Disabling it removes that file. The checkbox reflects the file's presence and reports write errors. Autostart launches the GUI with `--autostart`, which keeps the window hidden when a system tray is available; clicking the tray icon opens it. If the configured startup profile exists, the normal auto-connect flow still connects it. A manual launch opens the window, including when the application is already running. Existing Bimeron autostart entries are updated to the quiet launch command when Settings opens. This is separate from the system service boot setting.

Qt does not nest in DEB. The package declares system dependencies:

```text
libqt6widgets6 (>= 6.4)
libqt6network6 (>= 6.4)
qt6-translations-l10n
iproute2
nftables
pkexec
systemd
```

Because of this, the package is designed for Debian/Ubuntu-like systems with Qt 6, systemd and `pkexec` available. Depend on `pkexec` directly: Ubuntu 25.04 has it but no installable `policykit-1` package. Core is included inside DEB and is not downloaded after installation.

Acceptance criterion: `apt-get -s install ./bimeron-desktop-<version>-linux-amd64.deb` resolves dependencies on Ubuntu 25.04, and installation leaves both desktop and core executables available.

Installation and update:

```bash
sudo apt install ./bimeron-desktop-<version>-linux-amd64.deb
```

or:

```bash
sudo dpkg -i bimeron-desktop-<version>-linux-amd64.deb
sudo apt-get install -f
```

A package with a higher version upgrades the installed package `bimeron-desktop`. User settings are located outside `/usr` and are not deleted during a regular update.

DEB is not yet subscribed or published to the APT repository. A typical APT signature refers to the repository's metadata; it should be designed in conjunction with the future publication process.

## Review before publishing

GitHub Actions `Desktop CI` runs on pushes, pull requests and manual dispatch. It configures and compiles the GUI for Windows amd64 and ARM64 (MSVC 2022 and Qt 6.8.3; ARM64 cross-compiles on an x64 runner) and Linux amd64 and arm64 (Ubuntu 24.04 and system Qt 6). CI does not download core, create installation packages or publish them. It also runs CTest on native Windows x64 and both native Linux runners. Windows ARM64 remains cross-compile-only. On Windows ARM64, CI sets `BIMERON_DEPLOY_QT=OFF` because the ARM64 deployment tool cannot run on the x64 runner. Local package builds use the default `ON`. Without downloaded core, CMake uses CPack's default license text while configuring CI; package builds require the actual core notices and licenses.

`ctest` runs `profile-vless-test`, covering profile preservation, atomic replacement, permissions/DACLs, relative-reference import refusal, and secret redaction as well as existing protocol round trips. It uses Qt's offscreen platform on Linux and the Windows platform plugin on Windows. Set `BIMERON_VLESS_CORE` to a VLESS-capable core CLI path to additionally validate each generated profile against the core. Set `BIMERON_HYSTERIA2_CORE` for Hysteria2 validation and `BIMERON_PRESERVATION_CORE` to the exact reviewed newer CLI for SS2022, plugin, wrapper, and opaque-source preservation fixtures. These variables are for local contract testing and do not affect packages.

`developer-mode-test` checks session activation/restart, local IPC routing and peer-process rejection, canceled requests, and invalid executable handling. It uses isolated settings and the offscreen platform on Linux, never elevates or changes the installed service. Real elevated daemon switching still requires integration checks. The session-only Development tab accepts a local Core executable at runtime without changing the bundled package payload; see [developer mode](developer-mode.md).

The same executable includes `ConnectionManagementTest.h` fixtures for authoritative
startup state, profile stop/cleanup/start ordering, ambiguous start and stop
timeouts, instance and session changes, dropped events, cursor recovery, monotonic
speed resets, asynchronous CLI cancellation/rejection, update restoration and
explicit quit policy. Transport fixtures cover fragmented/malformed/oversized
responses, correlation/version failures and the 16 KiB request bound. They use
private local endpoints and the test executable as a validation CLI, not the
installed Core or system network. Test extensions require no additional build
target or package payload. See [connection management](connection-management.md)
for the runtime and separately authorized live-check boundaries.

After the complete script, `dist` should have exactly four current files:

```text
bimeron-desktop-<version>-linux-amd64.deb
bimeron-desktop-<version>-linux-arm64.deb
bimeron-desktop-<version>-windows-arm64.msi
bimeron-desktop-<version>-windows-amd64.msi
```

Minimal Git and name checking:

```powershell
git status --short
Get-ChildItem dist -File | Sort-Object Name | Select-Object Name, Length
```

Validate DEB metadata on Linux or in WSL:

```bash
dpkg-deb -I dist/bimeron-desktop-<version>-linux-amd64.deb
dpkg-deb -c dist/bimeron-desktop-<version>-linux-amd64.deb
dpkg-deb -I dist/bimeron-desktop-<version>-linux-arm64.deb
dpkg-deb -c dist/bimeron-desktop-<version>-linux-arm64.deb
```

Check the DEB architecture (`amd64` or `arm64`), desktop version, dependencies, paths and executable rights of both binaries. Compare each staged core binary, Wintun DLL and license with the extracted file under `.tools/core/<platform>-<architecture>`. For MSI, manually check a clean install and an upgrade from a previous version on the appropriate architecture, all three `Additional options` page options, launching the application, closing the installer, and uninstalling the program. MSI creation alone does not validate installer behavior.

The user performs GUI checks separately; successful compilation does not in itself confirm the correctness of the interface or VPN session.

## Cleaning and reassembly

A normal rerun does not require deleting `build`: CMake updates the changed files. If the toolchain, Qt path, generator or architecture has changed, remove only the specific problematic directory, for example `build/windows-arm64`, then re-run the corresponding preset.

Do not delete the entire working directory or use `git clean -fdx` as a standard cleanup method: this may remove local tools and other untracked data.

## Common errors

### CMake does not find Qt

Check for the path from `CMAKE_PREFIX_PATH` of the corresponding preset. For Windows ARM64 you need both target Qt ARM64 and x64 host tools. For Linux ARM64, check multiarch packages and `/usr/lib/aarch64-linux-gnu/cmake`.

### Core or Wintun not found

Run `cmake -DBIMERON_CORE_PLATFORM=linux -P scripts/fetch-core.cmake` on Linux or use `windows` on Windows, then check the input paths from the table above. Do not replace ARM64 files with x64 versions. Packaging requires all payload files. A GUI-only build can omit core.

### WSL is not installed

The default `scripts/package-all.ps1` exits before clearing `dist`. Run `./scripts/package-all.ps1 -WindowsOnly` for both MSI packages, and run `scripts/package-linux.sh` on a Linux machine for DEBs.

### WiX or CPack does not create MSI

Run `scripts/package-all.ps1` or `scripts/package-all.ps1 -WindowsOnly` so that WiX is loaded and added to `PATH`. When manually packing, make sure that `candle.exe` and `light.exe` are accessible from the current session.

### Linux files in DEB have permissions `0777`

Do not create DEB directly in the directory on `/mnt/c`. The current configuration specifically uses `/tmp/bimeron-desktop-packages` on the Linux filesystem.

### During Windows build the old GUI is running

Before linking, the script refuses to terminate the GUI if its adjacent local
Core is running (or its path cannot be inspected). Return to the system backend
and quit Development normally before rebuilding; this preserves daemon cleanup.
Otherwise, `scripts/stop-build-app.ps1` completes only the `bimeron-desktop.exe` instance launched from the build directory being updated. The installed application and core service should not terminate with this script.

### Only some of the packages remain in `dist`

The complete script cleans up old packages before building begins. If one of the following steps fails, correct the error and repeat the entire `package-all.ps1`; Don't publish a mixed set of packages from different runs.

## Files defining the process

- `CMakePresets.json` - architectures, toolchain and build directories;
- `CMakeLists.txt` - assembly, install tree, CPack, dependencies and package names;
- `scripts/package-all.ps1` - full orchestration of four packages;
- `scripts/package-linux.sh` - native Linux orchestration of two DEBs;
- `scripts/fetch-core.cmake` - portable core release download and SHA-256 verification;
- `cmake/core-release.cmake` - core release URL, tag, hashes and payload filenames;
- `scripts/stop-build-app.ps1` - safe completion of the GUI from a specific build directory;
- `cmake/linux-arm64-toolchain.cmake` — Linux ARM64 cross-toolchain;
- `packaging/windows/options.wxs` - MSI interface and behavior;
- `packaging/linux/bimeron.desktop` — desktop entry Linux;
- `.gitignore` - exclusion of generated directories and local secrets.

SSH profile fixtures can additionally use `BIMERON_SSH_CORE` pointing to Core
`v0.3.0-beta.1` built from `9f527efe0a4959786084675f1ff795edb8a0d910`.
This optional validation path does not change the bundled Core. The existing
connection test executable also covers transient SSH challenge UI and private IPC.
Protocol URI import tests use `BIMERON_PRESERVATION_CORE` targeting Core
`v0.3.0-beta.1` revision `9f527efe0a4959786084675f1ff795edb8a0d910`.
The current bundle pin is not evidence of support for these new protocols.

## Foreground installer feasibility prototype (not enabled)

Status: the [trusted invocation procedure revision](ikev2-profiles.md#trusted-invocation-procedure-selected-specification-review-pending)
is selected for independent specification review, not production acceptance.
This docs-only revision changes no supported build, packaging or installer flow.

Specification and acceptance for this isolated prototype: follow stage 8
`e4e6d05` and Core `66d84d8f4dcfc6ada625eb11894b19feea51b7ba`'s complete
`docs/IKEV2_FOREGROUND_CONTROL.MD` and internal `windowsnet` transaction guard.
Before production registration, demonstrate a dedicated OS thread retaining the
connect mutex through destructive actions, final policy publication, and settled
commit/rollback, with DLL lifetime pinned. Offline acceptance covers competing
threads/processes, abandoned ownership, strict owner/DACL and marker decisions,
flushed create-new journal refusal and preserved rollback metadata. Test journals
and Local objects are isolated surrogates, never installed policy or Global locks.
Generated WiX must embed an architecture-matched Binary, schedule admission before
RemoveExistingProducts/InstallFiles and settlement after all mutations, and fail
closed without approved final Core/GUI inputs. Sequence inspection alone cannot
prove execution order across nested transactions or callback process lifetimes.

The prototype must not register in production CMake/CPack. Its MSI entry point
always refuses: secure cross-process callback routing, MSI server lifetime through
rollback/commit and nested major-upgrade uninstall are unresolved prerequisites.
No installed helper, policy writer, reboot replacement, process termination or
network operation is authorized. A successful offline test is not MSI acceptance.

Offline result (2026-10-30): the standalone project at
`packaging/windows/foreground-prototype` builds with MSVC for x64 and ARM64.
Native x64 CTest exercises descriptor fixtures, actual user-owned object refusal,
absent/present/wrong-type markers, different callback threads and child-process
exclusion, nested-worker refusal, completed commit/rollback, incomplete settlement,
teardown, process crash, journal preservation and abandonment refusal. The generated
WiX fixture and five invalid variants are checked for both architectures; cached
WiX 3 candle accepts both generated sources. This is syntax/sequence inspection,
not a linked or executed MSI transaction. ARM64 runtime is untested. The default
VS Insiders instance lacked ARM64 configuration; explicit installed Build Tools
selection compiled it successfully. Linux targets are unaffected: no production
CMake, install rules, packet updater or package flow changed. No full GUI/package
matrix, performance benchmark, network operation or release was run for this
standalone Windows-only experiment.

Blocker established by the experiment: a second independent worker/process cannot
acquire the mutex while the transaction worker owns it; creating another static
handle does not route a callback to that owner. In particular the nested-worker
uninstall fixture refuses admission. DLL pinning prevents unload but cannot keep
an MSI server process alive; deliberate child-process death leaves a flushed
journal and abandoned mutex and subsequent admission refuses even after abandonment
is consumed. There is no authenticated cross-process transport or demonstrated
MSI host lifetime. Production integration therefore remains blocked.

The worker accepts only test fixture handles and writes opaque rollback metadata
into a private temporary journal with CREATE_NEW, write-through and FlushFileBuffers.
It does not claim protected journal durability across power loss, validate protected
Program Files ancestors/reparse points/effective ACLs, retain real image handles,
parse/publish schema 1 policy, generate an installation ID, compute the final GUI
hash, or restore installed bytes. These security requirements cannot be replaced
by the temporary fixture. Settlement is an explicit simulated completion signal,
not evidence that MSI completed rollback. Module pinning lasts until process exit.
Do not wire this DLL into production: its sole MSI export always returns failure.

Integration instructions and remaining acceptance are in
`packaging/windows/foreground-prototype/foreground-README.md`. Final artifacts and
approved architecture-specific Core digests are still absent; future GUI approval
must bind final installed GUI bytes, preserve the random public 16-byte installation
ID and previous exact policy/rollback metadata through repair/upgrade. Keep existing
signed packet installation/update behavior. Do not enable native packaging by
default until the live transaction and trust prerequisites above are satisfied.

## Offline native IKEv2 build integration

The GUI source list includes `NativeIKEv2Transport` and `NativeIKEv2Session`,
including unsupported Linux stubs. Compiling these sources does not instantiate
or approve a native controller, enable IKEv2, or supply a default approval digest.
Windows targets compiling them link `ole32` alongside `advapi32` and `shell32`.

The existing `developer-mode-test` target includes both native fixture headers
and runs their transport and session fixtures before the connection-management
suite. Only this test target defines `NATIVE_IKEV2_TRANSPORT_TESTING`, for both
transport and fixture compilation; the session seam remains a private friend.
No additional executable or package payload is introduced; existing install
exclusions still apply. Run offline fixtures with Qt 6.8.3 and
`QT_QPA_PLATFORM=offscreen` explicitly, without native-platform fallback. Its
clipboard is process-local and provides no native clipboard acceptance evidence.
Clear optional `BIMERON_*CORE` validation variables for offline-only runs.

`BIMERON_IKEV2_CORE` is an optional exact reviewed Core CLI validation input for
native profile fixtures when those fixtures are integrated. It does not select
or change the bundled Core. This build registration alone does not establish
integrated profile-test coverage. Protected executable/peer authentication,
same-user elevation, architecture-specific final executable approvals, MSI
transaction safety and live native networking remain separate acceptance gates.

## Bounded elevated-client prototype follow-up

The standalone foreground prototype now targets the accepted 2183b74 offline
contract. Its owned README records acceptance and evidence limits before code.
It remains outside production CMake/CPack; all MSI exports fail closed. No registry
reader/writer, enrollment, helper or Core admission change is authorized. Linked
MSI/table and token-descriptor fixture evidence is separate from actual initiating
MSI client correlation, outer completion, protected journal durability and VM gates.

Follow-up offline evidence: MSVC x64/ARM64 standalone compilation and cached WiX 3
candle/light linking completed. x64 CTest passed 3/3; ARM64 host-side WiX/MSI/PE
checks passed 2/2 (no ARM64 execution). ICE emitted ICE71 for the empty Media table;
it was not suppressed. Read-only table reports live in each build's
`Release/fixture.tables.json`, beside `fixture.msi`. The completion boolean was
removed; only bounded PostFinalize fixture evidence can acknowledge a temporary
journal. Commit callbacks cannot settle. Current-client correlation remains
Unknown; the current-token reader and privileged positive AccessCheck are not
actual MSI callback proof. Both MSI exports always refuse. Core does not read
this fixture journal, so durable host exclusion and native enrollment remain
unimplemented. See the owned README for commands and remaining VM gates.

The standalone offline foreground prototype now authors a single empty-payload
Media row and verifies the linked WiX MajorUpgrade and downgrade condition rows.
Its two architecture fixtures passed normal ICE validation without warning or
error diagnostics; ICE09 could not check the absent Component table. This does
not change or validate production packaging. See the prototype README for hashes
and remaining authorization and runtime gates.

## Separate offline foreground trace lab (not a release package)

The standalone foreground project has a default-OFF `FOREGROUND_LAB_TRACE`
option. With explicitly supplied cached WiX, build only the explicit
`foreground-lab-trace` target for Success, CommitFail and PostFinalizeFail empty
per-machine LABTRACEONLY MSIs. No production CMake/CPack or install rule references
them. Keep artifacts in ignored `build/foreground-lab-*`, never stage/dist.
Each DLL compiles a fixed failure point; no runtime command/property selects it.
The separate observer does not link the worker/journal and cannot enroll or
authorize native operation. Existing fail-closed exports are unchanged.

MSVC x64/ARM64 compilation, unsuppressed ICE for six actual MSIs and read-only
MSI/Binary/PE checks passed. x64 standalone CTest passed 4/4; ARM64 host-side checks
passed 3/3, including 42 negative database mutations per architecture. ICE09 notes
the absent Component table; ICE71 passes the explicit empty Media row. No callback,
installation, VM or release was run. Linux and production matrix/benchmarks are
inapplicable to this disconnected Windows-only target. Commands, trace schema,
identities, hashes and remaining runtime limits are recorded in
`packaging/windows/foreground-prototype/foreground-README.md`.

The corrected lab uses `EnsureTable` to create an empty Component table. The
read-only checker requires all six columns with the MSI schema and zero rows;
its prior strict allowlist and negative checks remain in force. The earlier ICE
pass did not establish runtime behavior. The first Success VM attempt failed at
InstallInitialize after error 2205 for the missing table; causality remains a
hypothesis pending independent review and a parent-led repeat.

## CostInitialize discrimination experiment (specification only)

The separate foreground lab specifies four frozen-MSI costing cases and strict
isolation and offline verification gates in its README. This is not a release
package, product change, or authorization to run MSI actions on the host. Guest
execution requires independent review and proof of the documented isolation gate.

Implementation appendix: the standalone
[bounded costing probe](../packaging/windows/foreground-prototype/lab-costing-probe/README.md)
originally used a flag-1 restricted MSI handle and literal CostInitialize. Its cached-tool
x64 build, four frozen-input copies, actual compound/logical comparisons, hash
negatives and compiled MSI API allowlist passed offline. It is outside production
CMake/CPack and does not change the supported package flow. No host MSI actions, release or architecture matrix were performed. The restricted
mode was subsequently run in a disposable x64 VM; review and guest execution
of the new mode remain pending.

The accepted follow-up specification in the probe README adds a separately
selected normal machine-dependent handle, with the same fixed inputs and bounded
CostInitialize call path. The normal handle itself does not guarantee immutability.
The saved restricted-mode guest run returned 0 for all four cases; a full `/qn`
run of the original source MSI failed with 2728 at CostInitialize. This is a
discrimination experiment, not a claimed fix. Independent review precedes the
parent's guest run of the new mode.


The parent-operated disposable x64 VM full-server FeatureComponents-only
discriminator passed CostInitialize, InstallInitialize and InstallFinalize, with
four successful intended callbacks and no Rollback; the both-changes case also
passed. Component-stream-only and the original Component-only MSI failed with
2728 at CostInitialize. Client probes all passed and did not discriminate. The
empty FeatureComponents database table is sufficient for that fixture; no internal
engine cause is established. ProductState -1 before and after is expected for
the zero-resource lab and is not production installation acceptance.

The accepted correction adds WiX EnsureTable for FeatureComponents, retaining
Component and all strict payload and sequence gates. Source commit
`a761106bd03f3710fbd2b092943cc493727289d1` produced six fresh MSIs and DLLs;
x64 CTest passed 4/4 and ARM64 host checks 3/3, with unsuppressed ICE and actual
read-only table/Binary/PE checks. The old Component-only MSI is rejected for the
missing FeatureComponents table. Exact hashes, identities, paths and VM limits
are in the prototype README. These fresh bytes remain runtime-untested pending
independent review and a parent-operated VM run. No host MSI actions, production
integration or release matrix were performed; Linux, performance and profiling
are inapplicable to this disconnected Windows fixture.

### Offline protected access observation

The separate standalone foreground prototype offers the default-off
`FOREGROUND_LAB_PROTECTED_ACCESS` option and explicit
`foreground-lab-protected-access` target. It is a fail-closed token-source
observation only, with no install rule or distributable MSI. See the prototype
README for its missing security and guest-execution gates. It does not alter
supported build or packaging flows.

### Disconnected pending-file build option

The standalone foreground project adds default-off FOREGROUND_LAB_PROTECTED_FILE
and explicit foreground-lab-protected-file target, with no install rule or MSI.
Configure separate ignored build/foreground-file-amd64 and arm64 directories with
that option; build that target in Release and run only its x64 CTest. It uses
installed MSVC and Windows SDK only. Strict protected-path positive availability
is reported separately from codec/ACL models and owned temporary I/O; passing
models cannot establish installer authority. Supported package flow is unchanged.

Pending-file offline evidence: both Windows architectures compiled the standalone
non-injected library and injected harness with /W4 /WX. x64 CTest passed 1/1;
ARM64 is compile-only. Strict native protected-path admission was unavailable on
the host. Codec/ACL/identity models and owned temporary write/flush checks do not
establish protected storage or installer trust. See the fixture README for the
exact limits. Linux/performance/package checks are inapplicable to this isolated
Windows-only option; no release artifacts were generated.

The standalone pending-file follow-up uses the native Program Files known folder
as its authority boundary and exact TrustedInstaller SID in addition to SYSTEM and
Administrators. It retains root-to-leaf traversal pins and revalidates descriptors
from that boundary. x64/ARM64 /W4 /WX compilation and x64 offline CTest passed;
protected native positive was unavailable on the host. No guest bundle or guest
execution is authorized by this result. See the prototype README for the minimal
separate guest-mode plan. Supported packaging and MSI exports are unchanged.


Pending-file anchor correction appendix: the standalone primitive now retains a
separate native known-folder handle, compares its volume/file identity with the
traversal anchor, and rejects every case-sensitive or unqueryable directory pin.
There is no unsupported-query fallback. The single volume-GUID mapping, existing
DACL/mutation/reparse/root pins and unconditional MSI export failures remain.
MSVC x64/ARM64 /W4 /WX builds passed; x64 protected-file/access CTest passed 2/2.
An owned temporary native case-sensitive directory demonstrated actual differently
cased open behavior and rejection; protected Program Files positive remained
unavailable. See the prototype README for model/native evidence boundaries.
No build option, supported packaging flow, guest harness or release changed.

### Offline protected-file guest harness

The optional standalone `FOREGROUND_LAB_PROTECTED_FILE` build also exposes the
explicit, excluded-from-ALL `lab-protected-file-guest` target. It has no install
rule and does not enter packages. With existing Visual Studio x64 and ARM64 C++
toolchains and an existing `dumpbin.exe`, run from the repository root:

```powershell
./packaging/windows/foreground-prototype/lab-protected-file-guest-check.ps1 -Dumpbin '<absolute path to dumpbin.exe>' -FrozenBundlePath '<existing frozen guest-bundle directory>'
```

The script performs static-only frozen x64 hash and explicit DLL/API allowlist
inspection, including negative parser fixtures; it never executes the harness or
builds either architecture. It preserves the old provisional bundle and writes a
final x64-only permission manifest under new `build/guest-bundle-final-x64-reviewed`.
ARM64 remains unbuilt for this bundle and completeArchitectureMatrix is false;
this does not block x64 permission preparation or claim full release validation.
Independent final review and explicit approval of the concrete bundle precede
guest provisioning/execution. Preparation-only edits need no executable rebuild. It does not provision or run the guest experiment. See
[the guest acceptance criteria](../packaging/windows/foreground-prototype/lab-protected-file-guest-README.md).
Rebuild and rehash after primitive changes; native success requires separate
permission and a disposable Windows VM.

### Isolated protected ACK MSI access fixture

The standalone foreground project adds default-OFF FOREGROUND_LAB_PROTECTED_ACK
and explicit `foreground-lab-protected-ack`, excluded from ALL and installation.
It produces three compiled Success/CommitFail/PostFinalizeFail Binary DLL/MSI pairs
per Windows architecture using explicitly supplied cached WiX 3, with normal
unsuppressed ICE. Empty Component and FeatureComponents tables use EnsureTable;
there are no installed resources or production CPack changes. Its only protected
path is native Program Files\BimeronProtectedAckLab\LABProtectedFiles, separate
from all earlier guest state. The original pending-file pin/counter behavior and
fail-closed exports remain unchanged.

See the foreground README for exact configure/build/CTest commands, canonical
post-build protected manifest encoding, external MSI hash pinning, offline coverage,
frozen review-bundle paths and proposed guest effects. The x64 native test opens
actual MSI databases read-only and exercises token precedence and owned temporary
record I/O; it never opens an installer session. ARM64 is compile/static-check only.
No MSI actions, host protected writes, elevation, VM operation or release are part
of this build flow. A review bundle grants no guest permission or production trust.
Linux, performance and full production package checks are inapplicable to this
disconnected Windows-only target. Supported release packaging is unchanged.

Protected ACK offline evidence: all six x64/ARM64 /W4 /WX DLL/MSI variants built;
normal verbose ICE passed without warning/error diagnostics. x64 focused CTest
passed 4/4; ARM64 host read-only checks passed 1/1, with 57 database negative models
per architecture. C++ read-only MSI projection matched the generated manifest;
actual token precedence/error refusal and owned temporary record I/O passed.
No host MSI action or guest execution occurred. Review and runtime limitations,
source/artifact evidence and the permission-not-granted bundle are in the foreground
README and ignored build/protected-ack-review state.

ACK freshness/diagnostic source follow-up adds an OS product-state query and fixed
numeric stage reporting. The lab-only REMOVE=ALL allowance requires an exact
canonical zero-resource database and UNKNOWN product state; it grants no caller
provenance or production authority. The standalone import checker must explicitly
allow the new MSI API entry after inspecting the linked image. Fresh source builds,
read-only checks, manifests and review are required; never overwrite frozen inputs.
Supported production packaging and original fail-closed exports remain unchanged.

ACK expanded offline verification follow-up: after the new native resource-query
negative test's Unicode sentinel and owned-database commit corrections, the parent
rebuilt its x64 harness and the previously unbuilt base fixture targets. The full
configured x64 standalone CTest set passed 7/7, including missing/nonempty resource
queries without installer actions. The revised test target also compiled for
ARM64; ARM64 actual read-only ACK checks passed 1/1. These later test-only source
changes do not change the frozen source-a1ab617 MSI/DLL behavior bytes. Initial
unbuilt-target and test-build/cleanup failures were resolved before this pass;
none was skipped to report success. Frozen Success artifact review and a new VM
run remain separate gates. No product runtime or release matrix is established.

ACK active-table diagnostic appendix: the accepted standalone README specification
adds only fixed private active-tables.txt on Success Preflight stage 41/detail 1.
It retains strict canonical projection, original 1603 and protected CREATE_NEW
semantics. Fresh cached-WiX bundles are required; old frozen inputs remain intact.
Supported production packaging is unchanged. Parent alone performs VM checks.

ACK runtime metadata projection correction: the accepted foreground README limits
exclusion to nine exact engine metadata row-owner names with persistence FALSE,
only in _Tables/_Columns. Persistent owners require TRUE; all other owners refuse.
Canonical persistent bytes and source MSI strict allowlists remain unchanged.
New x64/ARM64 cached-WiX builds and immutable review evidence are required; no
host MSI actions or protected writes are authorized. Windows 10 engine 5.00.10011
is the sole parent runtime observation; ARM64 runtime and production authority
remain unproved. Supported release packaging is unchanged.

ACK runtime metadata correction offline result: fresh build/ack-projection-x64 and
arm64 compiled /W4 /WX and linked six cached-WiX MSIs with unsuppressed ICE.
x64 focused CTest passed 4/4; ARM64 read-only checks passed 1/1, never executing
ARM64 code. Owned MSI copies verified exact baseline/temporary metadata projection
and unknown/persisted collision/missing/schema refusal. Source allowlists and
manifest encoding remain strict. Stages 42/43 distinguish persistence query/policy
refusal from mismatch. New ignored build/ack-projection-review freezes source,
identities, hashes, manifests and logs for independent review and parent VM work;
it grants no guest permission or production authority. Existing admission and
snapshot gates are unchanged. See the foreground README for evidence limits.

The isolated protected ACK Success fixture from a6d93adc was observed once in the disposable x64 VM on 2026-10-30: exit 0, all four successful callbacks, retained Pending counter 1, and no diagnostic snapshot. See the prototype README's Native x64 protected ACK Success observation for exact artifact hashes and limits. This is access evidence only; production packaging, enrollment and ARM64 runtime remain unavailable.
The two frozen x64 protected ACK failure variants were subsequently observed once each in the disposable VM: CommitFail rolled back with Pending counter 0; PostFinalizeFail failed after successful Finalize without rollback and retained counter 0. The prototype README records the acceptance and evidence limits; no production packaging behavior changed.

### Bounded offline ACK checkpoint build

The separate default-OFF FOREGROUND_LAB_PROTECTED_ACK_CHECKPOINT option exposes
only explicit foreground-lab-protected-ack-checkpoint on x64, outside ALL/install
and production packaging. It reuses the protected ACK source/sequence with fresh
identities and variant 3. See the foreground README acceptance for exact 80-byte
checkpoint encoding, fixed 60-second fail-closed wait and unconditional pre-ACK
PostFinalize refusal. This is partial offline fixture preparation; no runner,
termination or VM implementation belongs here. Existing ACK/trace contracts and
supported release packaging remain unchanged. Shared code requires existing six
ACK static checks, focused x64 regressions and ARM64 compilation; the checkpoint
constructor refuses ARM64. Use cached WiX and unsuppressed ICE, freezing all new
review evidence in a fresh ignored build directory. No guest-ready bundle claim.

Checkpoint offline result: fresh x64 checkpoint plus three existing ACK pairs and
three ARM64 ACK regression pairs compiled /W4 /WX and linked cached WiX with
unsuppressed ICE. x64 focused checks passed 6/6; ARM64 read-only checks passed 1/1,
with no ARM64 execution. Explicit ARM64 checkpoint configure refuses; default
options stay OFF. The fresh build/ack-checkpoint-review bundle is partial offline
evidence only, without a runner or VM execution. See the foreground README for
commands, fixed encoding, model/native evidence separation and review limits.
Supported production packaging remains unchanged.

Checkpoint owned-client runner blocker: the separate Run-CheckpointLab.ps1 refuses
before launch because the requested hard no-termination-after-deadline contract
cannot be enforced atomically by TerminateProcess(HANDLE, UINT). The prototype
README specifies the scheduling witness and explicit safe-API blocker fallback.
Offline PS 5.1/7 tests and a fresh ignored blocker-only evidence bundle are required;
this is not a guest-ready implementation and must not trigger parent MSI execution.
Frozen checkpoint inputs, supported packaging and existing runners stay unchanged.
The strict-deadline blocker witness passed offline on PS 5.1 and 7.6.9. The new
ignored build/ack-checkpoint-runner-blocked-review contains unchanged frozen
checkpoint inputs and refusal-only source/evidence, pending independent review.
Operational interruption acceptance is unimplemented; this bundle must not be
used to authorize parent MSI execution. No build/package flow changed.

### Disconnected callback interactive-context observation

The standalone foreground-prototype DLL now links Windows secur32 for
LsaGetLogonSessionData/LsaFreeReturnBuffer; its DLL and test compile /W4 /WX.
Configure fresh ignored build/callback-context-x64 and callback-context-arm64 with
the existing standalone VS commands, omitting WIX_TOOL_DIR. Build only
--target foreground-test; run x64 CTest and ARM64 foreground-sequence only.
No MSI is built. This observation has no production caller or install rule.
The original MSI PE checker still permits only its historical three imports;
new MSI preparation would require a separately scoped secur32 allowlist review.
Supported production packaging, checkpoint, trace, runner and package pins are unchanged.

### Checkpoint client commitment-deadline runner

The accepted foreground README resolution supersedes the refusal-only runner
contract while retaining its historical hard-deadline witness. The separate runner
uses a 300-second monotonic commitment deadline and one Eligible/Cancelled/Committed
gate; API entry/return/death and observer latency are unbounded after commitment.
Only the owned retained launch handle can receive one direct TerminateProcess call.
Frozen reconciled checkpoint MSI/DLL/manifest bytes are reused without rebuilding;
Run-Lab and the three-variant contract remain unchanged. Offline PS 5.1/7 mocks and
interop compilation precede a new ignored READY FOR REVIEW bundle. Parent VM
execution, retained protected state and possible late server work are separate
review gates. This disconnected Windows x64 runner changes no supported release
build or package flow; product matrix, benchmarks and profiling are inapplicable.

Checkpoint commitment runner offline result: PS 5.1 and 7 each passed 106 managed
assertions and AST checks with exact interop compiled only, no native API calls or
real process/MSI execution. Frozen index/inventory/sums, 180 immutable source ZIP
entries and seven MSI/DLL/manifest bindings verified. The separate native-x64-only
runner and exact committed source evidence are frozen under ignored
build/ack-checkpoint-runner-ready-review-cf12d14, READY FOR REVIEW only. See the
foreground README for path/pin/decision semantics, exact effects and untested VM
boundaries. Historical hard API-entry deadline evidence remains superseded, not
deleted. No MSI rebuild, original Run-Lab/ACK/context/CMake change or release flow
change occurred; all protected/isolated state and late-server uncertainty remain.

Final checkpoint log-reservation review uses metadata-only CREATE_NEW access to
avoid competing with native logger write access. The fresh final review bundle is
build/ack-checkpoint-runner-ready-review-final; earlier bundles remain intact.
Both PS suites rerun on exact committed ZIP-extracted sources, and source hashes
cover those same immutable Git-blob bytes. Native runtime remains a parent gate.

Checkpoint P1 ownership correction: final wait/query failure or unconfirmed death
now retains the worker, actual process handle and input pins in an explicit sticky
pending owner until host exit. Observers report Unresolved, Completed false and
possibleLateOperation true; observer completion is not resource release. No retry,
polling or recovery API was added. Confirmed death releases once; failure before
any process exists can release. PS 5.1/7 each pass 150 exact-worker assertions and
AST/interop checks without native execution. Fresh ignored
build/ack-checkpoint-runner-retention-review freezes exact committed source and
unchanged MSI/DLL/manifest with updated tests/effects for a fresh parent independent
review before VM work. Earlier bundles remain preserved. No supported packaging
flow, product matrix, MSI build/action, VM, benchmark/profile or release change.

Checkpoint fixed-system-image correction acceptance: only the internally selected
GetSystemDirectoryW-validated System32 msiexec pin may have >=1 links. Lab leaves
remain single-link. Explicit trusted OS owner/DACL checks replace the OS bypass;
safe inherited OS DACLs are allowed, protected lab DACLs remain mandatory. See the
foreground README for the MEDIUM specification, separated model/native checks,
immutable review freeze and parent review gate. No MSI rebuild or supported flow
change; no VM, launch, termination or product matrix belongs to this correction.

Fixed-system-image offline evidence: PS 5.1/7 pass 150 unchanged worker regressions
plus 34 descriptor/metadata/role models. Separately selected native checks verify
the exact two-link host system image, trusted owner/DACL/ancestor metadata/identity,
retained read-open compatibility and owned hardlink sharing/refusal. No operation
constructor, launch, termination, MSI API/action or VM ran. New ignored
build/ack-checkpoint-system-image-review freezes exact committed sources and
unchanged artifact pins for fresh parent review. Native guest/loader/logger and
server behavior remain unproved; supported package flow is unchanged.

### Optional callback context trace build

The standalone project may enable default-OFF FOREGROUND_LAB_TRACE_CONTEXT only
with FOREGROUND_LAB_TRACE. Configure fresh ignored build/trace-context-x64 and
build/trace-context-arm64 using the standalone VS 18 BuildTools commands and
explicit cached WIX_TOOL_DIR, adding both options ON. Build foreground-lab-trace,
foreground-test and lab-context-record-test. Run native x64 tests and ARM64 host
source/static tests only. Only context trace DLLs link the shared observer and
secur32; historical trace builds remain context-OFF. Use the explicit checker
context profile; default invocation keeps the historical profile. Normal WiX ICE
remains unsuppressed. No production install/CPack flow changes. See the foreground
README acceptance for immutable evidence, review and parent-only VM limits.

Implementation uses shared callback-context.cpp only for token/LSA capture and
policy; foreground retains its descriptor wrapper. Explicit context checker API
sets and .imports.json reports cover actual direct symbols, while default
historical profile still refuses secur32. The fixed numeric context record is
bounded to 2048 wchar including termination; its complete/eligible values are
observations only. Build only explicit targets and use the focused test filters
in the foreground README. Context-OFF regressions use separate ignored
build/trace-historical-x64 and arm64 directories. No production flow changes.

Callback context trace offline result: immutable source
751426da89c586ae66ab79b7b7f37fc2ed03e48d built six fresh x64/ARM64 context pairs
with cached WiX and unsuppressed ICE; actual tables/Binary/PE/API checks passed.
x64 focused CTest passed 4/4, ARM64 host statics 2/2 (compile-only runtime).
Context-OFF regression pairs passed historical checks on both architectures,
including default-profile invocation. Default-OFF/dependency refusal, fixed record
bounds/errors/logging return preservation and selected-token/LSA regressions pass.
The foreground README records resolved preparation errors and exact evidence.
Ignored build/trace-context-review-751426d freezes immutable source/archive,
artifact hashes/18 context GUIDs, tables/imports/logs/effects before evidence append.
This is offline review material, NOT VM-READY; parent review/execution remains.
No supported package flow changed. Linux/product matrix and performance checks
are inapplicable; no host MSI actions, protected writes, VM or release occurred.
