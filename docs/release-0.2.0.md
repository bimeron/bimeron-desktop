# Desktop 0.2.0 release acceptance

## Scope

Prepare Desktop 0.2.0 from main commit
`7f44c7309fc4cf9e91554f6bed8f167cefcda35d`, including its existing profile,
protocol, SSH, connection-management and recovery features. Do not merge the
`plan/stage-8-ikev2` branch or ship its native UI, enrollment or installer labs.
The IKEv2 work is parked in draft PR #16 and a separately retained Git bundle.
This is release preparation; publication requires separate final approval.

Use the existing MSI/DEB packaging flow. No setup wrapper, new package format,
service architecture, dependency upgrade or unrelated feature is in scope.

## Core input

The current published Core 0.2 bundle does not implement all protocols exposed by
this Desktop baseline. The candidate is Core `v0.3.0-beta.2` from immutable
reviewed commit `66d84d8f4dcfc6ada625eb11894b19feea51b7ba`.
The old `v0.3.0-beta.1` tag remains at `9f527efe`; its packet test exposed the
already fixed MASQUE shutdown error-precedence bug. Do not move that tag or
substitute a newer development binary. Pin the four final
archive hashes only after their exact source/version, architecture, payload,
licenses and signed update metadata have been verified. The production update
key and GUI compatibility interval `[0.2.0, 0.2.1)`, IPC 1 and profile format 1
must agree with the actual Desktop verifier. Test keys and local-update builds
are not release inputs. Core publication is not authorized by this document.

"Without IKEv2" describes Desktop functionality, not a claim that every Core
source file or CLI capability is absent. No native functionality is added to
Desktop by selecting this Core candidate.

## Acceptance

- Build Windows amd64/ARM64 and Linux amd64/arm64 from this release branch.
  Run the existing native Windows amd64 and Linux test suites; use native hosted
  Linux arm64 checks where local execution is unavailable. Windows ARM64 is
  compile/package-only unless suitable runtime hardware is available; report
  that limitation explicitly.
- Run protocol/profile validation against the exact candidate Core where the
  existing tests support it. Preserve ordinary signed update, health check,
  failed-update restoration and profile reconnection behavior.
- Produce exactly two MSI and two DEB files with version 0.2.0. Inspect metadata,
  architecture, installed paths, licenses and Core/Wintun payload hashes. Reject
  test executables, lab files, local feeds, test keys and private data.
- Check installation/upgrade and ordinary GUI/update behavior in the authorized
  disposable environment without deleting or resetting the parked native labs.
  Record unexecuted architecture-specific runtime checks rather than claiming
  cross-compilation proves them.
- Review English/Russian presentation and relevant existing connection/profile
  flows. Do not expose or advertise the deferred Desktop IKEv2 feature.
- Freeze artifact hashes, source commits, commands, results and remaining risks;
  prepare release notes and obtain final publication approval.

No performance-sensitive product change is planned, so new benchmarks/profiling
are inapplicable unless an actual regression requires investigation. No build,
package or runtime result is claimed by this acceptance document.

## Windows deployment architecture correction

Before rebuilding the rejected ARM64 candidate, require deployment to query the
selected target Qt installation explicitly, even when windeployqt runs from the
x64 host installation. Disable its compiler-runtime copy: the existing CMake
runtime install rule supplies the target MSVC redistributables. Acceptance is an
inspection of every packaged EXE/DLL: x64 MSI contains x64 PE files and ARM64 MSI
contains ARM64 PE files, including Qt plugins and vcruntime140_1.dll. Remove only
stale generated deployment files before rebuilding; preserve other packages and
all parked work. No GUI behavior or dependency version changes are intended.

The installed MSVC ARM64 redistributable directory also contains an x64
vcruntime140_1.dll compatibility file. Actual dependency inspection of every
staged ARM64 EXE/DLL finds no import of it. Exclude that unused compatibility
file from ARM64 installation; retain it for x64. ARM64 runtime acceptance requires
vcruntime140.dll and architecture-correct remaining runtime files.

## Local candidate evidence (2026-10-30)

The four-package flow passed after correcting Windows target-Qt deployment.
The first ARM64 MSI was rejected by independent payload review and is superseded.
The correction passes target Qt paths to host windeployqt, omits its runtime copy,
and excludes the unused x64 compatibility runtime from ARM64. Packaging now
rejects mismatched PE machines before CPack. The check accepted both corrected
stages and rejected an x64 GUI deliberately supplied as an ARM64 stage fixture.
Actual cabinet extraction confirms all 67 Windows payload files match staging
and every PE has its intended architecture. Both DEBs' eight-file payloads match
staging, permissions and ELF architectures. Dependency simulation passed on
Ubuntu 24.04.4; Ubuntu 25.04 was not tested.

Windows amd64 and Linux amd64 CTest passed 3/3. Candidate-backed protocol/profile
validation passed on each (1/1). Windows tests passed again after deployment
changes. Windows ARM64 compiled and packaged; Linux arm64 cross-compiled and
packaged. Native Linux arm64 hosted checks and ARM64 runtime remain pending.
English/Russian offscreen previews at 100%/150% were inspected; native tray and
installed UI checks are not established by those previews.

Frozen local package SHA-256:

| Package | SHA-256 |
|---|---|
| Windows amd64 MSI | `6f11d7b240ccd78843d676c9ee489d9f924d685254d8a7e8b431930a89e987fd` |
| Windows ARM64 MSI | `db2c925afe2ec39fcf142002427a014d61880ec364bcb60982005de7cbf36ccb` |
| Linux amd64 DEB | `3bb9e970097ef7f48e26edfd7be887f885d32f50017e8e7a7c6dbf71df4e2d72` |
| Linux arm64 DEB | `cdf844ff4308b4cea36f682a660732b539287e6eff98151c78f824d604a85642` |

Evidence is retained in ignored `build/package-all-final.log`,
`build/package-audit-corrected/` and the test logs. Prior rejected artifacts'
review remains in `build/package-audit/`; its rejection must not be interpreted
as the corrected candidate's result. New benchmarks are inapplicable to this
packaging-only correction.

Not ready for publication: production Core manifest signature and exact signed
GUI verification, native hosted Linux arm64 checks, installed upgrade/smoke
checks, final artifact review and publication approval remain open. The VM
currently has an unpublished Desktop 0.2.0 and a running independent Core service;
ordinary upgrade testing must preserve those service/profile settings and all
parked lab directories. No installer action was performed during inventory.
