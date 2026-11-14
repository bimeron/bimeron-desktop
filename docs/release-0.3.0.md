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
