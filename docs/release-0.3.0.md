# Desktop 0.3.0 release acceptance

## Scope

Prepare Desktop 0.3.0 as a GitHub pre-release from `main` after `516ea4b`,
including the changes made since `v0.2.0`:

- shared GeoData routing with categorized group selection, default Loyalsoldier
  databases and automatic database updates;
- unified profile import and editor creation;
- one-click Core recovery and service diagnostics;
- structured Core problems with field navigation;
- explicit Hysteria2, VMess, TUIC, AnyTLS and Naive QUIC options;
- developer-mode access source conversion and non-release Core marking in About;
- the fixes, translations and test hardening merged since `v0.2.0`.

This is release preparation; publication (tag, GitHub Release, push of release
artifacts) requires separate final approval.

Use the existing MSI/DEB packaging flow. No setup wrapper, new package format,
service architecture, dependency upgrade or unrelated feature is in scope.

## Native IKEv2

The native IKEv2 code from the merged stage 8 branch ships in the binary but is
not a user feature in 0.3.0:

- profile creation is offered only in developer mode;
- new drafts never show IKEv2 fields;
- an imported `[IKEv2]` profile is refused at connection with
  "Native IKEv2 requires a separate foreground backend";
- transport test hooks are compiled only with `NATIVE_IKEV2_TRANSPORT_TESTING`;
- nothing under `packaging/windows/foreground-prototype/` is built or installed by
  the main project.

Release notes do not advertise IKEv2. Safe Core update and MSI maintenance for
native IKEv2 remain deferred (issue #23).

## Core input

The previously bundled Core `v0.3.0-beta.2` lacks the `explicit_access` capability
and structured problems, so with it the explicit protocol forms stay hidden and
Core problems cannot navigate to fields. Desktop 0.3.0 therefore pins Core
`v0.4.0-beta.1`, which is being prepared separately.

Use a verified frozen candidate for local acceptance; publication requires signed metadata and:

- reports `explicit_access` and structured problems through the bundled CLI and
  service;
- has `gui_reviewed: true`, GUI bounds `[0.3.0, 0.3.1)`, IPC 1 and profile
  format 1, signed by the production key `7370a2d8cf890e5e`;
- has four archive hashes verified against exact source/version, architecture,
  payload, licenses and Wintun digests.

Candidate status (2026-11-14): Core `v0.4.0-beta.1` from commit
`0e08a5c8a114060ac0b53bbcc534b18c76d1aed4` is built and frozen but not tagged.
It supersedes `e1cada7d` by adding bounded reconnect recovery, and candidate `272e6027`, whose repair broker rejected its own signed
manifest because it accepted only GUI bounds `[0.2.0, 0.2.1)` (bimeron-core#133).
Its assets are published unchanged only after VM acceptance of this Desktop
candidate. The signed manifest (SHA-256
`7117c15801a3d99fbcc919e31fccd49e4036959cfb407f99352138d977041c63`) has GUI
bounds `[0.3.0, 0.3.1)`, IPC 1, profile format 1 and key `7370a2d8cf890e5e`;
its four archive hashes and four extracted executable hashes match `cmake/core-release.cmake`. The local candidate
ZIPs populate `.tools/core`, where `scripts/fetch-core.cmake` verifies them
offline. The Windows amd64 CLI reports revision `0e08a5c8` and lists all explicit
protocols in `explicit_access`.

Desktop 0.2.0 is not inside these bounds and keeps its current Core until the
user installs Desktop 0.3.0. Test keys and local-update builds are not release
inputs. Core publication is not authorized by this document.

## Repair broker

Packages ship the pinned Core executable as the protected repair broker and embed
it in the GUI for bootstrap. Release is **BLOCKED** until Windows x64 real UAC
and Linux amd64 real polkit VM acceptance passes:

- Clean install and upgrade from brokerless 0.2.0.
- Repair of removed/corrupted service executable.
- Missing separate broker bootstrapped from the protected GUI without a second elevation.
- Denial/retry/repeated clicks/close-reopen with prompt counting.
- Alternate-administrator credentials on Windows.

Any wrong attribution, duplicate mutation or failed rollback blocks release.
ARM64 remains compile/package-only. The pinned Core `0e08a5c8` includes bounded reconnect recovery and fixes the online repair-acquisition window bug;
packages must be rebuilt from these pins before VM acceptance.

## Version

Set the project version to `0.3.0`. The MSI keeps the existing UpgradeCode with a
higher ProductVersion; the DEB version is higher than `0.2.0`.

## Acceptance

- Build Windows x64/ARM64 and Linux amd64/arm64 from the release branch. Run the
  native Windows x64 and Linux test suites; use hosted Linux arm64 CI. Windows
  ARM64 and Linux arm64 are compile/package-only unless an ARM64 VM becomes
  available; report that limitation explicitly.
- Produce exactly two MSI and two DEB files with version 0.3.0. Inspect metadata,
  architecture of every PE/ELF file, installed paths, licenses (including the
  GPL-3.0 notice for the GeoData databases) and Core/Wintun/GeoData payload
  hashes. Reject test executables, lab files, local feeds, test keys and private
  data.
- In a disposable Windows x64 VM and a Linux amd64 VM:
  - clean install and in-place upgrade from 0.2.0 preserve profiles and settings;
  - explicit VMess/TUIC/AnyTLS/Naive QUIC/Hysteria2 forms are offered and a
    created profile validates and connects with the bundled Core;
  - an invalid profile shows a structured Core problem that navigates to the
    field;
  - default GeoData databases load, and the automatic update replaces them with
    verified downloads;
  - signed Core update discovery respects the `[0.3.0, 0.3.1)` bounds, and a
    failed update restores the previous Core.
- Without developer mode, no IKEv2 creation entry is visible.
- Review English/Russian presentation of new and changed screens.
- Freeze artifact hashes, source commits, commands, results and remaining risks;
  prepare release notes and obtain final publication approval.

No performance-sensitive product change is planned, so new benchmarks/profiling
are inapplicable unless an actual regression requires investigation. No build,
package or runtime result is claimed by this acceptance document.

## Local candidate evidence (2026-11-14)

Source: `release/0.3.0` at `81abc6c` (translation update after `d12265c`), Core candidate `e1cada7d`.
`scripts/package-all.ps1` produced exactly four packages:

| Package | Size | SHA-256 |
|---|---|---|
| `bimeron-desktop-0.3.0-windows-amd64.msi` | 60010496 | `21eacb49c9223b72fd52bed1ec53dfce23d0b27682a998205186ad14e2cad988` |
| `bimeron-desktop-0.3.0-windows-arm64.msi` | 37642240 | `f09e3b13e816d706a82eeb61d7417c9579fce8a87514f0820f3249644f84a248` |
| `bimeron-desktop-0.3.0-linux-amd64.deb` | 36370590 | `058228b8bfe190eb9f0289f15ffc4d0e16ec355d77a70bf5eff0cca845029bb2` |
| `bimeron-desktop-0.3.0-linux-arm64.deb` | 33692644 | `375a8a36a8923952b80c6963e31f22a281e3dc191318df42c60a62f440881275` |

- Windows x64 and Linux amd64 (WSL) CTest: 10/10 each, including
  `repair-resource-test` and `repair-payload-test` (32 configure-gate cases).
- MSI administrative extraction and `dpkg-deb -x`: bundled Core and repair
  broker equal the executable pins for every target; Windows brokers have the
  architecture-matched `wintun.dll`; DEB brokers are mode 755 ELF of the target
  architecture; DEB metadata reports 0.3.0 and the target architecture; no
  `*-test*` files in any package.
- ARM64 is compile/package-only. VM acceptance has not been run.

An earlier package set from `469be43` was rejected: its GUI embedded the
superseded `272e6027` broker because Core archives use a fixed file time and
the resource was not regenerated. `d12265c` embeds digest-named copies.

## Reconnect recovery release blocker

Desktop #48 and Core #136 require [bounded reconnect recovery](reconnect-attempts.md).
The frozen e1cada7d candidate does not satisfy this gate. Core must merge the
fix with green CI, then the release thread freezes a new candidate from master.
Pins, all four packages and VM acceptance must be renewed separately.

## Reconnect candidate supersedes earlier evidence

The current Core candidate is 0e08a5c8a114060ac0b53bbcc534b18c76d1aed4, merged through Core PR #137 after green CI. It supersedes e1cada7d; the earlier package hashes above are historical and must not be used for this candidate. The signed manifest SHA-256 is 7117c15801a3d99fbcc919e31fccd49e4036959cfb407f99352138d977041c63. Archive and extracted executable pins are updated together. Compatibility and production signing key are unchanged. Rebuild all four packages and repeat pair/VM acceptance before publication.

## Reconnect candidate package verification (2026-11-14)

Source: release/0.3.0 at f6c002e (including reconnect merge c674446), Core
0e08a5c8a114060ac0b53bbcc534b18c76d1aed4. The final full package-all.ps1
run produced exactly two MSI and two DEB packages. Windows x64 CTest passed
10/10 (33.06 s), WSL Linux amd64 CTest passed 10/10 (26.63 s), including the
new reconnect fixtures, payload gates and embedded repair resource hashes.
Both ARM64 targets compiled and packaged; neither ARM64 runtime was executed.

Fresh administrative MSI extraction and dpkg-deb extraction matched every file
against stage (43 Windows amd64, 40 Windows arm64, 15 per Linux target).
All PE/ELF target machines, four Core/broker executable pins, adjacent Windows
Wintun/license bytes and bundled GeoData hashes/license passed. Both MSI metadata
projections retain ProductVersion 0.3.0 and the existing UpgradeCode; DEB metadata
is version 0.3.0 with the expected architecture, and archive binaries are mode 755.
No test executable, backup binary or local update feed is in the final payload.

The first MSI was rejected because two pre-existing local backup executables in
the Windows amd64 build output were copied by the existing directory install rule.
Those files were preserved outside the compiler output in ignored build evidence,
then the complete packaging script was rerun. Only the final hashes below apply.
The directory-copy rule still requires output hygiene and explicit inventory review.
ARM64 Qt deployment warns about missing dxcompiler.dll/dxil.dll; builds pass, but
native ARM64 GUI behavior remains unverified. Diff review and git diff --check
passed. No performance-sensitive change requires benchmarks/profiling.

Logs, inventory projections and verification.json are under ignored
build/release-0.3.0-reconnect. No installed service or host network was changed.
New-candidate VM reconnect, repair/UAC/polkit, clean-install and upgrade acceptance
remain outstanding and block publication. Previous VM results do not accept this pair.

| Package | Size | SHA-256 |
|---|---|---|
| `bimeron-desktop-0.3.0-linux-amd64.deb` | 36369758 | `37a9d7e7d8735e406040540dd45f32c0371182c3127e590eb5081abb961dd432` |
| `bimeron-desktop-0.3.0-linux-arm64.deb` | 33700408 | `6569f2180a874e7dd64850abfe77bd5ff3399970231ebc1fb178316c927f3a40` |
| `bimeron-desktop-0.3.0-windows-amd64.msi` | 49078272 | `5e981ffecbbac323c9b13c101c907592b58dbd0f9f84256ec5ba173d64d88eb8` |
| `bimeron-desktop-0.3.0-windows-arm64.msi` | 37646336 | `09272394f4b27e8e8893da65d1facde201dc8f536d96ff47ac5c61eacb9c89d3` |

## New-candidate VM acceptance (2026-11-14, partial)

Windows 10 STD: verified final MSI SHA-256
5e981ffecbbac323c9b13c101c907592b58dbd0f9f84256ec5ba173d64d88eb8
before installation. Removed the prior local 0.3.0 package, installed the published
0.2.0 MSI, then upgraded to this 0.3.0 package. All successful MSI operations
returned 0. The first old-MSI invocation with a forward-slash path returned 1619;
retrying with the native Windows path succeeded. No dependency or check was bypassed.
The original service executable hash and connected session survived package changes;
all three profile files and the profile index retained their exact bytes.
Installed GUI/Core/broker hashes match the final package payload. No VM checkpoint
was restored or deleted, and no other Windows VM was used.

Updated the VM service through the installed protected CLI with the reviewed
executable digest. The service now reports 0.4.0-beta.1, revision 0e08a5c8,
modified=false and Service state: 4 (running); its executable equals the bundled
Core/broker pin. The original VM profile validated and connected through CLI.
This is an administrator CLI update and manual CLI reconnection, not GUI signed-feed,
UAC consent or automatic GUI reconnection acceptance.

A separate VLESS TLS profile targeting closed loopback port 1 validated, then
produced IPC reconnect progress 1/3, 2/3 and 3/3 with transport_check_failed.
It terminated in failed with Attempt=4, ReconnectAttempt=3, ReconnectLimit=3 and
the same cause. A subsequent start of the unchanged original profile connected
and omitted both reconnect fields, with empty LastError. The temporary test profile
was removed; the VM service remains running with the original profile connected.
The test script used process-only execution-policy bypass after confirming that
MachinePolicy/UserPolicy were Undefined; no machine policy was changed.

Observation: Stop after terminal failure returns session_stop_failed because
Engine.Stop returns the completed run's original error. The subsequent Start works;
this is not evidence that cleanup failed. GUI recovery/quit behavior for this
case remains unverified. Neither connection interruption after success nor native
GUI counter/tooltips, transient presentation and service-dialog appearance were
observed in this run. The existing interactive GUI process survived installation,
so its in-memory version is not accepted as the new GUI merely from the file hash.

Linux: recovered the previously configured SSH access to the dedicated
Bimeron Test Ubuntu 22.04 LTS VM and booted it. The copied DEB matched the final
SHA-256. dpkg unpacked it but could not configure it: libqt6widgets6 and
libqt6network6 require >=6.4, whereas the VM's stock repositories offer 6.2.4;
qt6-translations-l10n is also absent. Core/broker version commands reported the
correct candidate. Removed the incomplete package afterwards; no service was
installed and no dependency requirements were weakened. The VM remains running.
A suitable Qt >=6.4 Linux VM is required for GUI/polkit acceptance. The existing
WSL package build and tests are not a substitute for this VM gate.

Evidence: ignored build/release-0.3.0-reconnect/vm contains MSI logs, upgrade,
service-update, retry IPC projections and final-state results. No secret/profile
contents are included in tracked documentation. Real UAC/polkit, alternate-admin,
repair/bootstrap failure paths, protocol server interoperability, new-GUI visual
checks and complete Linux install/upgrade acceptance remain release gates.

## Ubuntu 22.04 compatibility package set (2026-11-14)

This set supersedes the previous package hashes. Source implementation: 3ebe2cd.
See linux-compatibility.md for specification, exact test results and limitations.
Both Linux packages are now built against Ubuntu 22.04 Qt 6.2/GCC 11/glibc 2.35,
with generated runtime dependencies and a mandatory packaging-script ABI check.
Windows x64 and Ubuntu 22.04/24.04 amd64 tests passed 10/10 each. Both ARM64
packages built without runtime execution. All four extracted payloads, architectures,
pins, modes, metadata and licenses were verified. The ordinary-user Linux GUI
launched natively and established IPC after root CLI service installation.
This resolves the earlier Qt dependency installation blocker. Native polkit/repair,
new-package upgrade scenarios and all other outstanding release gates remain;
there is no publication approval or full release acceptance claim.

| Package | Size | SHA-256 |
|---|---|---|
| `bimeron-desktop-0.3.0-linux-amd64.deb` | 38368604 | `6dc42da2d9d3fd59ca4427a7adc10e708f973ac38868b96853ad7298ee596ac1` |
| `bimeron-desktop-0.3.0-linux-arm64.deb` | 35592224 | `107a90cee150bac10ef9369e0372a05d980f54b851f3cdc57b9e439836a6366d` |
| `bimeron-desktop-0.3.0-windows-amd64.msi` | 49078272 | `29efce252fa90ee0db8b3ec890d03dc34ad6f903fb3f091ead2b124894f03ee9` |
| `bimeron-desktop-0.3.0-windows-arm64.msi` | 37646336 | `130dea205b9141b41094b1fa85e5c29df70794a7c7be6a3c34a96535c155a55f` |
