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

The currently pinned Core `v0.3.0-beta.2` lacks the `explicit_access` capability
and structured problems, so with it the explicit protocol forms stay hidden and
Core problems cannot navigate to fields. Desktop 0.3.0 therefore pins Core
`v0.4.0-beta.1`, which is being prepared separately.

Pin it only after it is published with signed update metadata and:

- reports `explicit_access` and structured problems through the bundled CLI and
  service;
- has `gui_reviewed: true`, GUI bounds `[0.3.0, 0.3.1)`, IPC 1 and profile
  format 1, signed by the production key `7370a2d8cf890e5e`;
- has four archive hashes verified against exact source/version, architecture,
  payload, licenses and Wintun digests.

Candidate status (2026-11-14): Core `v0.4.0-beta.1` from commit
`272e6027f8e786c385d5b5509cd80dda155eafd2` is built and frozen but not tagged.
Its assets are published unchanged only after VM acceptance of this Desktop
candidate. The signed manifest (SHA-256
`a1e3132b7d77cee181d4811f022819d0cdbf305d1f573270449b92d648e59a9d`) has GUI
bounds `[0.3.0, 0.3.1)`, IPC 1, profile format 1 and key `7370a2d8cf890e5e`;
its four archive hashes match `cmake/core-release.cmake`. The local candidate
ZIPs populate `.tools/core`, where `scripts/fetch-core.cmake` verifies them
offline. The Windows amd64 CLI reports revision `272e6027` and lists all explicit
protocols in `explicit_access`.

Desktop 0.2.0 is not inside these bounds and keeps its current Core until the
user installs Desktop 0.3.0. Test keys and local-update builds are not release
inputs. Core publication is not authorized by this document.

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
