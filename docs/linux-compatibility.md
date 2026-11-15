# Linux package compatibility

## Specification

Desktop 0.3.0 Linux release packages target Ubuntu 22.04's system Qt 6.2,
glibc 2.35 and GCC 11 runtime baseline on amd64 and arm64. Build release DEBs
against that baseline rather than lowering metadata on binaries built on a newer
system. Keep system Qt, DEB packaging, Core pins and the existing privileged
repair model; no bundled Qt, new package format or third-party runtime repository.

Newer Debian/Ubuntu systems with compatible system libraries are intended targets;
this does not claim Ubuntu 18.04/20.04 support, other distribution package formats
or validation of every derivative. Windows retains its existing Qt/toolchain.

## Acceptance

- Configure and compile with the stock Ubuntu 22.04 Qt 6.2.4 and GCC 11.
- Both DEBs declare dependencies compatible with the baseline and include actual
  ELF runtime dependencies; no newer Qt/glibc/libstdc++ symbol requirement is hidden.
- Install the amd64 DEB using ordinary Ubuntu 22.04 repositories, launch the real
  GUI under the existing desktop user and run the native test suite.
- Check Core, repair broker, plugins, translations and GeoData, including their
  pinned bytes and protected paths. Real polkit/repair acceptance remains required.
- Build/package arm64 against the same baseline; report runtime limits explicitly.
- Run Windows regression checks for any shared source changes; preserve MSI flow.

This is a compatibility/build change, not a performance-sensitive algorithm.
Benchmarks/profiling are not applicable unless a regression appears. Existing
release artifacts are superseded only after successful new package verification.

Qt 6.2 lacks QFormLayout::setRowVisible. On that version the widget-row helper
hides both label and field and empties the row's cells until it is shown again
(see "Layout corrections" below); newer Qt retains the current API and the
6.4.0-6.4.2 upstream-bug workaround. Existing editor geometry/toggle fixtures
must pass on both Qt generations. Linux packaging checks ELF symbol versions
before staging and derives runtime library dependencies with dpkg-shlibdeps.
Native amd64 CI uses Ubuntu 22.04; arm64 CI on Ubuntu 24.04 additionally checks
newer system Qt compatibility, without claiming baseline package acceptance.

## Verification (2026-11-14)

Implementation: 3ebe2cd on release/0.3.0; Core remains 0e08a5c8. The native
Ubuntu 22.04 VM used stock Qt 6.2.4, GCC 11.4 and glibc 2.35. CMake 3.31.10
was installed as a build tool for schema-6 presets. Both amd64 and arm64 DEBs
built successfully there. Source/packaging overlay hashes on the VM match the
local implementation files. Generated state and tools remain outside Git.

Native Ubuntu 22.04 amd64 CTest passed 10/10 (18.32 s); Windows x64 passed
10/10 (28.44 s); newer system Qt on Ubuntu 24.04 WSL passed 10/10 (18.15 s).
The Ubuntu 22.04 binary also stayed running against Ubuntu 24.04 system libraries
in an isolated offscreen smoke run (intentional timeout exit 124). Both Windows
architectures were rebuilt and packaged; both ARM64 targets are compile/package
only. Hosted CI was updated but not pushed or executed for this change.

The DEB installs/configures on Ubuntu 22.04 without third-party repositories.
Its generated dependencies include libc6 >=2.34 and libstdc++6 >=11, not the
newer host's runtime. ldd has no missing libraries. GUI launched as the ordinary
test user; a native X11 window capture shows the rendered application. Root CLI
installation/start of the packaged Core service for UID 1000 then established
GUI IPC readiness (Disconnected), with matching Core/broker hashes. This is not
real polkit consent, repair/bootstrap, VPN-server interoperability or upgrade
acceptance. The VM is left with the GUI and service running, VPN disconnected.

Both DEBs passed the ELF ABI gate; the previously built Qt 6.4 binary was
correctly rejected for requiring Qt_6.4. Fresh MSI/DEB extraction verified every
file against final staging (43/40 Windows files; 15 per Linux target), PE/ELF
architectures, Core/broker pins, Wintun and GeoData/license hashes, executable
modes, version/architecture metadata and the retained MSI UpgradeCode. No test
or backup binaries were packaged. The existing ARM64 Qt DX compiler DLL warning
remains; native ARM64 GUI behavior is unverified. Diff/security/regression review
and git diff --check passed; no unresolved implementation finding was identified.
No benchmark/profile was applicable. Final packages/hashes are in release-0.3.0.md.
Ignored evidence is under build/ubuntu2204-compat, including native GUI captures.

## Polkit launch-context correction (2026-11-15)

The earlier SSH/runuser launch reached IPC after root CLI service setup but was
not in the graphical login session. Its Restore exited 127 because pkexec had
no agent in that remote root session and could not open /dev/tty. Supplying
DISPLAY/XAUTHORITY/session-bus environment does not transfer login-session
membership. Relaunching the unchanged installed GUI from the local GNOME terminal
allowed real polkit dismissal/retry and successful Restore of the stopped service,
followed by verified IPC Disconnected. See service-actions.md for exact evidence
and the separate diagnostic-retention fix. This resolves the stopped-service
launch-context failure, not the other release repair scenarios.

## Layout corrections (2026-11-15)

Two Ubuntu 22.04 (Qt 6.2.4) regressions were reported from the installed GUI:

- Settings: the scrolled page content was grey and did not match the window.
  `QScrollArea::setWidget()` enables background filling with the palette's
  Window colour. The page background comes from the style sheet, and the Linux
  "System" theme leaves the palette untouched, so the content painted the stock
  Qt grey. Windows always sets Window to the style-sheet colour, which hid it.
  The Settings content and its viewport, and the profile editor's VLESS and
  routing-status scroll areas, no longer fill their background, as the other
  editor scroll areas already did.
- Profile editor, Protocol tab: the ShadowTLS section was pushed down by a blank
  band. Qt 6.2's QFormLayout skips only rows with empty cells; a row whose
  widgets are hidden still receives vertical spacing. The parameter form has 24
  rows that are usually hidden, which reserved 156 px. The same extra gaps
  affected every form toggled by the helper, such as the General tab. On
  Qt < 6.4 the helper now removes a hidden row's widgets from their cells
  (parent, buddy and visibility are kept) and puts them back when the row is
  shown, matching setRowVisible. While a row is hidden, itemAt(),
  labelForField() and getWidgetPosition() do not report its widgets, and rows
  must not be inserted or removed above it. Qt 6.4+ is unchanged.

Acceptance: the Settings background behind and between cards matches the
window in the System and Dark themes; with no protocol parameter rows visible,
the parameter form is only as tall as its margins and ShadowTLS starts at the
top of the Protocol tab; hidden rows between visible ones add no spacing;
`formRowVisibility` and `protocolTabLayout` in profile-vless-test cover this and
pass on Qt 6.2 and Qt 6.8.

Verification: Ubuntu 22.04 VM, stock Qt 6.2.4, staged sources outside the
`/opt` snapshot. profile-vless-test failed with the previous helper and passed
with the fix. CTest passed 8/9; geodata-download-test fails with "failure
handling mismatch" both with and without this change (also in the existing
`/opt` baseline build), so it is unrelated. Windows x64 (Qt 6.8.3) CTest passed
10/10. Before/after renders of the real MainWindow and new-profile editor on the
VM's X display (xcb, System and Dark themes) confirm both fixes and evenly
spaced General-tab rows. They were made with a temporary uncommitted capture
mode. Not a performance-sensitive change; no benchmark is applicable.

## Installed current GUI on Ubuntu 22.04 (2026-11-15)

Layout fix 8a4f863 was integrated into release/0.3.0 as 4b48df1, together with
existing repair diagnostics and friendly display-name changes. Rebuilt the amd64
DEB on the Ubuntu 22.04 VM and passed native CTest 10/10 (18.16 s), the Ubuntu
22.04 ELF ABI gate and pinned repair-payload verification. APT's downloaded-package
cache occupied 1.3 GB and was cleared with apt-get clean; available space recovered
without deleting source, user data or test evidence. The earlier GeoData test
failure does not reproduce in this full run.

Reinstalled the DEB (dpkg exit 0), verified installed GUI bytes against staging,
then launched it through the existing local GNOME terminal server as user test,
not as a child of the SSH/root login. Native screenshot of the installed Settings
page confirms the corrected uniform background and responding running-service
indicator. Profiles/index hashes stayed unchanged and the Core service retained
PID 50730. Core reports failed (attempt 4), so VPN connectivity is not accepted
by this installation check; no profile connection/start/stop was requested.

GUI SHA-256: 6d60975b1592a56f8bca1ee336f911378f2c9bf521654d3103ee79f8e57fbbec.
DEB SHA-256: 3074d7b58887281522f363440110dd2c151efea95acd545f12520760b48d8ae3.
Package, logs and native screenshot are retained under ignored build/linux-vm-current.
This is an amd64 VM refresh, not a final four-package release freeze; ARM64 and
Windows packages were not regenerated for this installation request.
