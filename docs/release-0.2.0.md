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
this Desktop baseline. The candidate is the immutable existing Core tag
`v0.3.0-beta.1`, commit `9f527efe0a4959786084675f1ff795edb8a0d910`.
Do not move that tag or substitute a newer development binary. Pin the four final
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
