# IKEv2 safe update and MSI maintenance: reconciled plan

Status: accepted specification for one offline slice, 2026-11-05. Native IKEv2
remains OFF. Error 775 is unresolved and error 668 normal cleanup failed; nothing
here claims a working native connection, cleanup or recovery. No installer,
service, lab, VM or network operation is part of this work. Supported packages
(MSI/DEB, x64/amd64 and ARM64) and their build flow are unchanged.

## Reconciled sources

This document performs the reconciliation step required by both plans below.
They remain the detailed references; this file records only the agreed
ownership, the decisions taken and the acceptance of the next slice.

| Source | Location | Identity |
|---|---|---|
| Core maintenance specification (uncommitted) | Core worktree `plan-ikev2-safe-update-spec`, `docs/IKEV2_SAFE_MAINTENANCE_SPEC.md`, base `8ef0110` | SHA-256 `1b53c406353877736d3a5cc0ebb4b39720893f402308b9fc767a6de955c93a1e` |
| Desktop maintenance plan (uncommitted) | Desktop worktree `plan-ikev2-safe-maintenance`, `docs/ikev2-safe-maintenance-plan.md`, base `38e020e` | SHA-256 `87ce5054f401fc3432a455a4433ea1c476f3dcf8a26859c6a0de0151242f268c` |
| Offline MSI inventory (slice A) | [ikev2-msi-compatibility.md](ikev2-msi-compatibility.md), `scripts/msi_inventory.py` | commit `e26e2f7` (this branch's base) |

The two plans agree on substance: refusal before the first conflicting mutation,
no maintenance-driven native Stop or force-close, retention of recovery bytes and
state on uncertainty, and no retroactive protection of old cached MSI packages.
Where wording differs, the resolution below applies.

## Ownership

| Concern | Owner | Notes |
|---|---|---|
| Admission decision for service update, install over existing payload, uninstall, reconfiguration and package maintenance | Core | One decision contract. Every privileged participant rechecks at its own effect boundary. |
| GUI preflight and refusal messages | Desktop | Advisory only; a GUI result is never authority. |
| Execute-sequence gate in new MSI packages | Desktop, after Core admission exists | Must precede shutdown, `RemoveExistingProducts`, file and registry mutation, and cover quiet, repair, uninstall and nested removal. |
| Historical and cached MSI routes | Desktop evidence, product decision | Old packages never call a new gate. Harmless isolation, clean migration or native-off remain the only choices. |
| Durable recovery retention and lock continuation | Core | Unresolved dependency. Native maintenance stays blocked until it is accepted. |

## Decisions (2026-11-05)

1. **Active packet session: Stop, then Retry.** When a packet VPN or proxy session
   is starting or active, maintenance refuses with "Stop the VPN, then retry the
   update". The user stops through the existing control and retries from fresh
   admission. An idle running packet service remains eligible: the updater may stop
   and restart it while preserving its prior running or stopped state. This
   supersedes, for the future integrated path, today's interrupt-update-reconnect
   behavior. The current GUI behavior is **not** changed by this slice (see
   "Deferred Desktop slice").
2. **Unknown or failed cleanup** refuses with "Cleanup could not be verified.
   Update blocked; recovery files retained". It is never reported as merely busy.
3. **Native-enrolled baselines** refuse all maintenance until the durable recovery
   retention and restoration-continuation design is accepted. The model must not
   return eligibility for them.
4. **First code slice** is a pure Core eligibility model (Core specification,
   bounded slice 2). The Desktop MSI gate and GUI wiring wait for Core admission
   and an accepted integration destination.

## Accepted slice: pure Core eligibility model

Repository and branch: Core `feat/ikev2-maintenance-decision`, base
`8ef0110326c86c1621a0c274dfb06a30b123324b`. New files only:
`internal/maintenance/decision.go` and `internal/maintenance/decision_test.go`.

### Specification

`Decide(Inputs) (Decision, error)` is a pure, deterministic function using only
the Go standard library. It has no build tags, OS calls, I/O, clocks, globals or
locks, and is not called by production code in this slice. Its result is
eligibility for a later atomic recheck and reservation, never an OS permission,
token or grant.

Every input field is a finite enumeration whose zero value is `Unknown`, so a
zero or partially filled `Inputs` can never be eligible:

| Field | Values besides `Unknown` |
|---|---|
| `Operation` | `ServiceUpdate`, `ServiceInstallOverExisting`, `ServiceUninstall`, `ServiceReconfigure`, `PackageMaintenance` |
| `Baseline` | `PacketOnly` (agreed never-enrolled classification), `NativeEnrolled` |
| `Pair` | `Exact`, `Mismatch` |
| `Lifecycle` | `Idle`, `PacketStarting`, `PacketActive`, `PacketStopping`, `NativeActive`, `Provisioning`, `MaintenanceInProgress` |
| `Cleanup` | `Verified`, `Pending`, `Failed` |
| `Journal` | `Clean`, `Incomplete` |
| `Lock` | `Free`, `Busy`, `Abandoned` |
| `Backup` | `None`, `Unresolved` |
| `Retention` | `Sufficient`, `Insufficient` |

Access-denied, stale or missing evidence is represented as `Unknown`; the model
does not accept Booleans such as `clean=true`. `NativeActive` covers any native
session or still-open terminal foreground owner.

`Decision` carries a `Verdict` and one `Reason`. Verdicts:

- `Eligible` — every field has its single clean value (below).
- `StopThenRetry` — the user must stop an active packet session first.
- `RetryLater` — another admitted lifecycle step is in progress.
- `RecoveryRequired` — uncertain, failed or inconsistent state; recovery
  resources must be retained.
- `Blocked` — retrying cannot help (pair mismatch, native baseline, insufficient
  or unverified retention).

Evaluation order is fixed; the first matching rule wins:

1. Any field outside its defined range returns `ErrInvalidInput` with a
   `RecoveryRequired` decision. An `Unknown` operation is also invalid input.
2. `RecoveryRequired`: unknown baseline, lifecycle, cleanup, journal, lock or
   backup; failed cleanup; incomplete journal; abandoned lock; unresolved backup;
   or `NativeActive` under a `PacketOnly` baseline (inconsistent evidence).
3. `Blocked`: unknown or mismatched pair; `NativeEnrolled` baseline; unknown or
   insufficient retention.
4. `RetryLater`: busy lock; pending cleanup; `PacketStopping`, `Provisioning` or
   `MaintenanceInProgress`.
5. `StopThenRetry`: `PacketStarting` or `PacketActive`.
6. Otherwise `Eligible`.

The only eligible input for each valid operation is: `PacketOnly`, `Exact`,
`Idle`, `Verified`, `Clean`, `Free`, `None`, `Sufficient`. All operations share
this policy in this slice; operation-specific CLI prerequisites (for example
uninstall requiring a stopped service) stay in their existing commands.

The package does not reuse `internal/nativeenrollment` types: their semantics
(update health and inventory evidence) do not match admission, and BIMNENR1 is
not extended. No journal, lock, transport or public API is added.

### Acceptance criteria

1. Exhaustive enumeration of the full valid input space proves: exactly one
   eligible tuple per valid operation; the verdict of every tuple equals the
   ordered rules above; results are deterministic.
2. Changing any single field of the eligible tuple to any other valid value never
   yields `Eligible`.
3. If any of baseline, lifecycle, cleanup, journal, lock or backup is `Unknown`,
   the verdict is `RecoveryRequired`, never `RetryLater` or `StopThenRetry`.
4. `NativeEnrolled` never yields `Eligible`, `StopThenRetry` or `RetryLater`.
5. The zero `Inputs{}` and out-of-range values of every field are refused with
   `ErrInvalidInput`.
6. Named regression cases from the Core specification's negative acceptance map
   to the expected verdict and reason: SCM Stopped with residual DNS/WFP (cleanup
   failed or unknown), empty RAS with incomplete journal, missing owner after
   crash (lifecycle unknown), access denied (unknown), consumed abandonment,
   preexisting backup, insufficient retention, wrong pair, active packet session,
   idle running packet service, native session under a packet baseline.
7. A test asserts the package imports only `errors` (no OS, I/O or sync packages).
8. `go test`, `go test -race` where supported, `go vet` and `gofmt -l` pass for
   the package; `GOOS=linux` (amd64 and arm64) vet confirms it builds off Windows.

Benchmarks and profiling are not applicable: the function is not on a hot path
and has no production caller. The four-target Desktop package matrix is not
applicable: no Desktop source, build or package file changes.

## Deferred slices and blockers

1. **Desktop GUI refusal (Stop, then Retry).** Replace silent refusals in
   `coreUpdatePrerequisite`, the update-ready handler and service removal with the
   agreed messages; refuse instead of interrupting an active packet session and
   drop the post-update reconnect for that case. Blocked on choosing the
   integration destination: `main` (`296130e`) rewrote the same `MainWindow`
   service and recovery code, so implementing on this base would conflict.
2. **Core helper admission.** Call the model from `service_update_windows.go` and
   `service_windows.go` before `protectServiceDir`, staging, SCM or file changes,
   with the lock order from the Core specification. Blocked on producers of
   authoritative lifecycle, cleanup and journal evidence and on checked guard
   release.
3. **New MSI execute-sequence gate.** Blocked on slice 2 and on the Core
   specification's lock-continuation and recovery-retention decisions. The
   always-failing foreground prototype must not be wired into production.
4. **Historical routes.** No 0.1.x MSI bytes, installed caches, transforms or
   patches are available (see the inventory document). Without an enforceable
   answer, native support stays off for that deployment boundary.
5. **Tracking.** Open a scoped issue under the existing enrollment work before
   slice 2; record an ADR once lock continuation and historical-route choices are
   accepted.

## Implementation result

Core commit `5628356abbe87cb59087e0f862372c29ac50379d` on local branch
`feat/ikev2-maintenance-decision` (not pushed) adds only
`internal/maintenance/decision.go` (imports `errors` only) and
`internal/maintenance/decision_test.go`. Constants carry their type prefix
(`BaselinePacketOnly`, `LockAbandoned`, ...) because bare names such as `Unknown`
would collide within one package. The zero `Verdict` is `RecoveryRequired` and
the zero `Reason` is `ReasonInvalidInput`, so a zero `Decision` never means
`Eligible`. Reason values are in-process codes, not a wire or persistence format.
No production code imports the package.

Validation with the cached Go 1.26.9 toolchain and `GOPROXY=off`:

- `go test ./internal/maintenance/ -count=1 -v`: 5 tests pass, covering all
  acceptance criteria. Exhaustive enumeration covers 186,624 tuples against an
  oracle that restates the ordered rules as data.
- `go vet` passes for windows/amd64, linux/amd64 and linux/arm64; `gofmt -l` and
  `git diff --check` are clean.
- `go test -race` could not run on this host: ThreadSanitizer fails to allocate
  memory (error 87). The same host failure occurs for the existing
  `internal/nativeenrollment` package; this is not a code finding.
- Mutation checks: seven implementation mutants (native baseline eligible,
  unknown operation accepted, StopThenRetry before Blocked, inconsistent-evidence
  rule removed, unknown lock as RetryLater, widened lock range, reordered failure
  reasons) and one reviewer mutant (active packet session eligible) were all
  caught; the committed bytes were restored and retested.

Review (planner, high effort) found no blocking issues. Notes: the test oracle
shares the specification, not code, with `Decide`; "idle running packet service"
maps to the eligible tuple because service running/stopped state is deliberately
not an admission input; native-enrolled failures report `RecoveryRequired`
before `Blocked`, which keeps the "cleanup could not be verified" message.

Not run or not applicable: Desktop builds, CTest and the four-target package
matrix (no Desktop source or packaging change), benchmarks and profiling (no
production caller), any installer, service, VM or lab action. Remaining risks
are unchanged: no authoritative evidence producers, no durable recovery
retention, no MSI gate and no answer for historical cached packages, so native
maintenance and native support stay off.
