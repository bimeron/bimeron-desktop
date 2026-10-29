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
