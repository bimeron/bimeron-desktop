# Core availability and recovery

Status: implementation specification and ordered plan; no runtime changes yet.
Requested on 2026-11-03. Implement one step at a time with focused validation.
This document specifies the intended change; existing service/developer contracts
remain the description of shipped behavior until their implementation is updated.

## Problem and evidence

Profiles can show Core unavailable while Settings remains at Service: checking
without an available resolution. This can happen after developer backend changes,
but recovery must also cover ordinary startup, service failure and IPC failure.

Observed in source (not yet a reproduced live failure):

- `MainWindow::connectionFailure()` calls `invalidateConnection()`, which cancels
  `serviceStatusProcess_` as well as connection work. IPC retries do not themselves
  restart service status inspection. A canceled initial inspection can therefore
  leave the initial checking label indefinitely.
- `refreshServiceStatus()` uses `connectionGeneration_`; a connection failure can
  invalidate a valid service result even when the selected backend is unchanged.
- `DevelopmentCore::restoreService_` exists only in memory. An abrupt GUI exit
  loses the reason a previously running service was stopped.
- Existing service actions, trusted helper execution, peer checks, cleanup gates,
  signed updates and emergency network recovery already exist. Reuse them.

Read before implementation: [service actions](service-actions.md),
[developer mode](developer-mode.md), [connection management](connection-management.md),
[core updates](core-updates.md), and the relevant Core integration contract.
Read the complete [build documentation](build-and-packaging.md) before changing
build files, dependencies, scripts, packaging or architecture paths. No such flow
change is planned. Verify current startup/autostart behavior before modifying it.

## Scope and invariants

Provide a shared availability assessment and explicit recovery entry points on
Profiles and Settings for Windows and Linux. Keep source/documentation English
and provide Russian translations. Preserve profiles and preferences.

- Passive diagnosis never starts/stops/installs a service, selects another backend,
  elevates, downloads an update, reconnects a profile or changes network settings.
  Preserve separately configured existing startup behavior, subject to recovery
  conflict gates; do not broaden it into failure-triggered recovery.
- Runtime Core readiness, OS service state and VPN connection state are distinct.
  A running service or successful helper exit is not readiness. IPC failure is
  never proof that the VPN disconnected or network cleanup succeeded.
- Recovery is explicit. Revalidate immediately before mutation; never derive a
  privileged action from a translated label or an old UI snapshot.
- One mutating operation at a time, shared with service/update/development/quit
  operations. Avoid a new generic task framework.
- Preserve endpoint/process ownership checks, disconnected acknowledgement,
  process exit and VPN lock checks. No fallback to another backend after failure.
- Never execute a persisted local path with elevation on startup. Never kill or
  adopt a process based only on a journal, filename, PID or responding endpoint.
- Emergency `recover` remains separate, with its existing direct-network warning.
  No automatic reinstall, forced kill, network reset or automatic VPN reconnection.
- No Core protocol change, package replacement mechanism, release or publication
  is included. Unsupported repair must explain the limitation and offer diagnostics.

## Availability model and presentation

Keep typed facts: backend (system/local/transition), service observation
(unknown/not installed/stopped/transitional/running/paused/failed), IPC observation
(checking/ready/transport failure/timeout/access denied/incompatible/invalid or
untrusted response), and cleanup/conflict gate. Retain observation freshness,
operation identity and diagnostic cause. Derive display state and actions centrally;
do not combine these dimensions into a large Cartesian-product enum.

Both pages consume the same assessment. Profiles shows a short cause, explanation,
primary action and Details link to the service/recovery section of Settings.
Settings also shows backend, service and IPC facts, last failure and retry. Never
paint a running service as proof of overall health. A ready disconnected Core is
healthy; VPN connection remains an independent status.

| Assessment | Primary action | Restrictions |
|---|---|---|
| Initial inspection | Checking | Bounded; do not hide diagnostics indefinitely |
| Confirmed missing service | Install service | Explicit consent; existing trusted installation path |
| Confirmed stopped service | Start service | Recheck conflict/cleanup gate before start |
| Service transition | Check again after bounded wait | No overlapping mutation; timeout explains latest state |
| Service running, IPC unavailable | Retry check; offer Restart service | Restart requires interruption warning and safe cleanup |
| Status command failed or unknown output | Retry check | Never offer Install based on an ambiguous error |
| Access denied | Show permission guidance and retry | No generic elevation claimed to fix IPC permissions |
| Incompatible protocol | Explain required compatible GUI/Core | Existing signed update path only if its prerequisites pass |
| Invalid response or peer mismatch | Details and retry | No automatic mutation of an untrusted peer |
| Local Core failed / interrupted switch | Return to system service | DevelopmentCore cleanup/ownership gates still apply |
| Cleanup unconfirmed or foreign daemon conflict | Explain blocker and safe next step | No second Core; separate emergency recovery when appropriate |
| Ready | No repair required | Compatible hello plus authoritative fresh status |

Default timing: service probe 5 seconds; retain existing IPC request timeout and
1/2/5/10-second transport retry backoff. Bound a passive transition observation to
30 seconds, then show a retryable transition-timeout result. Periodically refresh
unhealthy system service facts at most once per 10 seconds, without concurrent
probes; explicit retry can refresh immediately. Tests should use controllable time.
Do not discard a pending probe on every IPC failure. Backend changes invalidate
old results. Privileged helpers that exceed their watchdog remain tracked until
exit: show waiting, keep UI responsive and block conflicting mutations.

## Recovery behavior

The entry action diagnoses first, then offers the specific operation. Reuse
`runServiceCommand`, trusted executable selection and existing platform elevation.
Cancellation is a terminal user-visible result, not success or a permanent spinner.
Action errors remain visible even if a subsequent status probe succeeds.

After a mutating command completes, query service state and require a compatible
hello and fresh authoritative status before reporting restored availability. Use
a bounded verification window (30 seconds); failure leaves retry and diagnostics.
No profile reconnect is introduced by this workflow. Recheck operation/backend
identity when accepting asynchronous results.

Restart is not blindly stop/start: warn that it can interrupt VPN/proxy, verify
cleanup using the existing Core lifecycle contract, and never start a replacement
after unconfirmed cleanup. If a nonresponding running service cannot be safely
restarted with the existing helper contract, stop at an actionable blocked result.
Inspect that contract before implementation; document any required Core dependency
instead of weakening the gate or inventing a command.

## Interrupted developer session

Add a versioned, atomically written recovery journal in per-user application data,
separate from the session-only developer unlock preference. Record before the
first service mutation: original service state, transition ID and stage, intended
restoration, backend identity and minimal local ownership evidence available from
the current platform. Do not record profile content, credentials or IPC payloads.
Failure to persist the initial journal prevents the destructive transition.

Journal presence never enables Development or chooses/launches a remembered local
binary. On a normal launch, inspect actual service/process/endpoint/lock state
before existing automatic service startup or auto-connect can race recovery. Show
an interrupted-session banner with explicit recovery; do not silently act on it.
If live inspection proves the system already restored and no local conflict remains,
clear the stale journal without stopping a working service or VPN.

Factor restoration so it is callable outside the hidden Development page. An
originally stopped/absent service remains stopped/absent after restoring that
transition; starting/installing it is a separate user action. Journal deletion is
allowed only after confirmed lifecycle completion/restoration. Retain it across
failures and cancellation. Validate schema, lengths and paths; corrupted/unknown
records produce a diagnostic and fresh inspection, never process control.

After GUI restart, saved PID/path alone cannot authorize shutdown. Reuse live
same-user/executable/peer identity verification only where it proves ownership;
otherwise explain how to close the daemon manually. Linux's shared endpoint makes
service/local identity ambiguous: block mutation unless live evidence resolves it.
Parent-death supervision and guaranteed cleanup after OS failure remain out of scope.

## Ordered implementation checkpoints

1. **Reproduce and fix status starvation.** Add a failing asynchronous fixture for
   IPC loss during a service probe. Separate service-probe invalidation from ordinary
   connection invalidation, retain backend/mutation guards, and ensure every probe
   reaches a terminal state. Cover stale results after backend switch, failed start,
   timeout and unknown output. Do not add recovery mutations in this checkpoint.
2. **Centralize assessment.** Add the smallest typed assessment/reducer and map
   existing service and IPC facts into it. Keep the existing action visibility
   rules until the next checkpoint. Test fact precedence and freshness, including
   running service with failed IPC and ready-but-disconnected Core.
3. **Expose consistent UI and explicit recovery.** Add the shared banner/actions,
   Settings navigation, retry and post-command verification. Route start/install
   through existing commands. Add restart only after auditing its cleanup contract.
   Cover cancellation, command failure, busy operations and no implicit reconnect.
4. **Persist interrupted-switch recovery.** Implement journal I/O and lifecycle
   boundaries in DevelopmentCore; integrate startup inspection and normal-mode
   recovery without exposing the developer tab. Test crash boundaries before/after
   stop, launch, cleanup and restore; include originally absent/stopped services,
   corrupt journal, persistence failure, stale PID and foreign daemon.
5. **Validate and document final behavior.** Update service-actions.md,
   developer-mode.md and connection-management.md alongside their implemented
   changes. Include diagnostics fields with secret redaction. Review the full diff
   for stale callbacks, privilege boundary errors, profile loss and duplicate work.

Each checkpoint must pass focused tests before the next. If existing Core cannot
satisfy a safety condition, update this specification with concrete evidence and
the reduced supported action before implementing that action. Use Conventional
Commits if commits are requested; do not publish a release as part of this plan.

## Acceptance and validation

- Missing Core at startup and IPC loss during status inspection cannot leave
  checking indefinitely. Repeated retries cannot starve service diagnosis.
- Both pages agree on cause/action, while service and VPN facts remain distinct.
- Every nonready state offers retry/details and a truthful supported next step.
- Late replies, repeated clicks, tab changes and backend changes cannot execute
  duplicate actions or overwrite current state. Updates and quit retain exclusion.
- A command returning zero without a ready compatible Core is not reported repaired.
- No failure starts another backend or reconnects a profile. No unknown service
  error becomes not installed. Existing configured startup behavior is preserved
  except while an unresolved recovery conflict safely blocks it.
- Restart/return-to-system cannot bypass cleanup or peer checks. Emergency recover
  is separately confirmed; ordinary retry never removes kill-switch protection.
- Normal launch after interrupted Development offers recovery without unlocking
  Development, launching a persisted executable or trusting a recycled PID.
- Original stopped/absent service state is preserved through transition recovery.
- Exercise EN/RU, light/dark, 100%/150% DPI and minimum supported window size.
  Cause, actions and operation errors must remain readable and keyboard accessible.

Extend existing `tests/ConnectionManagementTest.h` and developer-mode fixtures with
isolated fake probes/helpers/endpoints and temporary journal storage; do not change
the installed service or public network in automated tests. Run focused fixtures,
then existing `developer-mode-test`, `core-update-test` and `profile-vless-test`.
Use disposable Windows/Linux environments for real elevated lifecycle checks;
fakes alone do not prove cleanup or service restoration. Run relevant CI-equivalent
builds for Windows x64/ARM64 and Linux amd64/arm64 and native tests where supported;
report unavailable targets and distinguish cross-compile from runtime coverage.

This is not a throughput change. Benchmark/profiling is not required unless probe
load or UI responsiveness regresses. Check bounded probe count, backoff and no
blocking GUI-thread waits. Before a later release run the full supported
test/build/package matrix; no packaging is requested now.

Planning validation: source-path and existing-contract review only. No product
tests/builds, live service mutations, benchmarks or release checks have run for
this documentation-only change. The reported screenshot sequence remains a
reproduction task for checkpoint 1.

## Implementation record (2026-11-03)

The initial implementation separates service-probe invalidation from IPC failure,
adds a shared typed service observation and availability explanation, explicit
probe retry, Settings navigation and interrupted-session restoration entry point.
Unhealthy probes are rate-limited; short transition polling ends after thirty
seconds. English and Russian strings are provided.

Implementation decisions and limits: a generic restart for a nonresponding service
requires a verified Core cleanup contract and is not offered. Successful explicit
start/install and interrupted recovery of a previously running service begin fresh
hello/status verification with a thirty-second deadline, without reconnecting a
profile. Restoring an originally stopped/absent service preserves that state and
does not require readiness. Stop/remove/emergency recovery do not require readiness.

The assessment reuses existing IPC readiness/failure fields with typed service
facts instead of adding a separate reducer framework. Service probe generations
and mutation/backend invalidation reject obsolete callbacks. Start/Install remain
in Settings, reached through Profiles' Core details action. The journal deliberately
contains no saved executable/PID; cross-process ownership cannot be established
from such records. A remaining local daemon requires manual closure. Interrupted
cleanup needs a separately confirmed successful Core emergency recovery, persisted
before service restoration; endpoint disappearance and a free lock alone never
authorize restoration. Linux reconciles an already healthy service through live
systemd MainPID, peer-checked hello/status with a stable instance and a final MainPID
recheck. It preserves the running VPN and rejects unverified or unhealthy peers.

Live elevated recovery and native EN/RU layout/DPI checks remain required before
release. Compilation and isolated fixtures cannot prove installed-service cleanup.

## Implementation validation (2026-11-03)

- Final Release desktop builds passed: Windows x64, Windows ARM64, Linux amd64
  and Linux arm64. Both ARM64 targets were cross-compiled, not executed.
- All five CTest tests passed on native Windows x64 (13.74 s) and Linux amd64
  under WSL (9.67 s): developer-mode, core-update, geodata-download,
  geodata-selection and profile-vless.
- New isolated fixtures cover a service probe surviving IPC loss, cancellation
  on backend invalidation, shared availability guidance, readiness deadline and
  late recovery, and valid/corrupt journals refusing persisted process adoption.
- Diff/UTF-8/translation JSON checks passed. Review found and corrected stale
  readiness verification after backend changes and inappropriate verification
  when restoration intentionally preserves an originally stopped/absent service.
- No throughput change; benchmarking/profiling not applicable. No installed-service
  mutation, real elevated crash-recovery check, native layout/DPI acceptance,
  packaging or release was performed. These remain release-validation limits.
- Windows Qt deployment reports missing optional dxcompiler/dxil DLLs; compilation
  succeeds. This change does not modify deployment or packaging configuration.

## Review corrections (2026-11-03)

Interrupted recovery now requires persisted successful emergency cleanup before
restoration when no live service can prove readiness. A free/abandoned VPN lock
and missing endpoint are insufficient. The existing network recovery warning
precedes the separate explicit helper operation, and failures retain the journal.
Linux can reconcile an already running system service by live systemd MainPID,
OS peer PID, compatible hello/status with stable instance, and a final MainPID
recheck. It preserves the active connection and never sends stop/start/recover.

All four desktop targets rebuilt successfully; ARM64 remains compile-only.
The new isolated regressions exercise free-lock/unconfirmed cleanup, foreign IPC
peer, changed service PID and healthy-service reconciliation. A fixture event-order
failure was diagnosed and corrected by awaiting the server connection event.
No installed-service or network mutation was performed. Existing release-validation
limits and benchmark applicability remain unchanged.

Final correction checks: developer-mode-test passed on Windows x64 (6.64 s) and
Linux amd64/WSL (4.41 s). The four other CTest suites passed on both platforms
in the correction run. The Windows fixture crash was localized to a test-only
stored callback assignment before reconciliation; replacing that member with a
per-call private query callback and rebuilding resolved it. Diagnostic markers
were removed. Positive reconciliation retains connected VPN state; negative tests
retain the journal for foreign peers and service PID changes. No benchmark is
needed for this control-flow correction. Real elevated recovery remains a
release-validation requirement.

## Accepted startup and presentation revision (2026-11-03)

The user approved the nine-state interactive mockup and implementation. This
revision supersedes the earlier passive-only ordinary-startup rule and inline
recovery banners. Preserve the recovery journal, cleanup and peer-identity gates.

- Ordinary GUI launch prepares the system backend once: inspect IPC/service state,
  start an installed stopped service once, and require fresh hello/status within
  a bounded window. Never restart a running unresponsive service automatically,
  install automatically, repeat elevation after cancellation, or launch a local
  executable. Development and visual/test launches do not auto-start services.
- A pre-existing connected VPN is preserved. Core readiness does not itself start
  VPN; only the existing explicit auto-connect preference may connect after normal
  startup readiness. Manual repair never reconnects a profile implicitly.
- Interrupted recovery/local conflicts/unconfirmed cleanup block automatic start.
  A manual stop in this GUI session is respected; background polling cannot undo it.
- Profiles uses the existing connection status row: Preparing connection while
  initial preparation is pending, ordinary connection state when ready, and one
  concise error plus Fix... on failure. Remove the separate explanatory paragraph,
  Retry core check row and Core details button.
- Settings retains the existing application preferences and a compact service
  card containing service state and Manage.... Its content scrolls when needed;
  the footer and minimum control sizes remain usable at 720x600 and 100/150% DPI.
- Fix... and Manage... open one bounded native Qt management/recovery dialog:
  clear cause and specific primary action; expandable Details contains technical
  diagnostics and advanced service controls. Confirmed missing service offers
  install, stopped service offers start, canceled permission offers explicit retry,
  leftover daemon/cleanup failures retain the existing safe recovery flow. Emergency
  network recovery retains its separate direct-network warning.
- Move signed Core update controls to About. Keep update prerequisites and existing
  command paths intact. Use current application styling, English source strings,
  complete Russian translations and normal keyboard/Escape behavior.
- Acceptance: one startup attempt, no repeated UAC/polkit, no service mutation in
  developer/test mode or on ambiguous service failure, healthy VPN preserved,
  configured startup auto-connect still works, manual recovery never reconnects,
  no duplicated status/buttons, all actions reachable, no untranslated labels,
  no clipped settings/dialogs in native EN/RU at 100/150% scaling.
- Add focused behavior and native capture fixtures using fake service operations,
  private endpoints and isolated settings. Do not mutate the installed service
  in tests. Rebuild four targets and run relevant native CTest checks. No package
  release, dependency or build-flow change is planned.

### Content-sized management correction

The management dialog must have no scroll areas or user resizing. Its fixed width
and content-driven height follow the existing 18/16 margins, 12 spacing, rounded
buttons and section typography. Expanding/collapsing Details recomputes the height.
Advanced actions do not duplicate the primary install/start action. A rejected
start inspection is represented explicitly and offers warned network recovery,
with instructions to close other Core instances first. Successful recovery clears
this state; an ambiguous observation never authorizes installation. Tests must
cover expansion/collapse sizing, fixed bounds, missing-service installation and
blocked-start recovery independently of the latest service observation.
After a readiness timeout, a confirmed missing or stopped service still offers
explicit Install or Start, respectively. Access/identity/protocol errors retain
their read-only retry action; action text and dispatch always share one decision.

Correction checks: all four desktop targets built. Five CTest suites passed on
Linux amd64 and in the initial Windows x64 correction run. After the final timeout
change, developer-mode-test passed on Linux (4.45 s) and Windows offscreen (5.64 s).
The native Windows rerun passed the recovery assertions but failed the separate
Supported clipboard shows Paste assertion; this remains an unresolved native
clipboard test issue, not a clean native-suite pass. Native recovery captures and
fixed-size/no-scroll/expand-collapse assertions passed in EN/RU at DPR 1/1.5 and
Russian dark mode. The final Russian light capture measured 460x180 collapsed,
460x281 expanded, and 460x180 collapsed again. git diff --check passed. No service
or network mutation, packaging, performance-sensitive change or release occurred.

### Revision validation

- Built the desktop target for Windows x64/ARM64 and Linux amd64/arm64. ARM64
  was compile-validated only; no native ARM64 runtime was available. Windows
  ARM64 deployment retained the existing optional dxcompiler/dxil warning.
- All five CTest suites passed on Windows x64 and Linux amd64/WSL. After updating
  obsolete presentation assertions, developer-mode-test passed in 6.56 s and
  4.36 s respectively; the other four suites passed in the preceding run.
- Native Windows captures passed containment checks at 720x600 for English and
  Russian at DPR 1 and 1.5, with an additional Russian dark-theme run. Captures
  cover preparation, readiness, failure, Settings and the management dialog;
  generated PNG/JSON artifacts remain under ignored build/.
- Review corrected action/label divergence: Check again cannot dispatch install
  or start for permission, protocol, identity or verification errors. Regression
  clicks cover both missing and stopped observations. Manual start inspection
  now blocks profile activation and discards its result if Core becomes ready
  or a connection operation starts before the callback.
- git diff --check passed. No performance-sensitive data path changed; benchmarking
  and profiling are not applicable. No release packages were produced.
- Real UAC/polkit cancellation, elevated service recovery and native ARM64 runtime
  remain release-validation requirements. Tests use isolated endpoints and do not
  mutate the installed service or network. Conflict inspection is a point-in-time
  check; Core's own locks remain responsible for external startup races.

## Next implementation plan: guided connection repair

Status: implemented with capability-limited manual restart fallback (see validation below). This section supersedes conflicting UI/action
rules above. Earlier validation records describe earlier implementations only.

### Product contract

The helper must explain what prevents connection, offer a concrete remedy, execute
the complete authorized remedy, and verify readiness. A generic unavailable label,
an unexplained Check again action, or diagnostic export alone is not a solution.
Do not infer missing service, broken installation or leftover network state from
an IPC timeout. In particular, replace the current boolean start-block result:
an occupied endpoint or inaccessible lock does not justify network recovery.

Profiles retains one concise status and Fix connection. Settings offers Manage
service. Both open the same helper, which starts read-only diagnosis immediately.
Ordinary startup retains its one guarded attempt to start a stopped installed
service; developer mode stays explicit. Never reconnect a VPN after manual repair.

### Window and copy

- Title: Connection repair. Show a specific cause, its consequence, and the next
  action. Use service in normal copy; reserve Core/IPC/lock details for diagnostics.
- Show three compact findings: application components, system service, connection
  to service. Each has a readable outcome, not a spinner without a deadline.
  Add a developer-session/conflict finding only when relevant.
- One primary remedy, one optional alternative, and Close. No duplicate actions
  in Details. Refresh diagnostics is a secondary action whose explanation names
  what is checked: installed components, service state and service response.
- During repair show the actual step: Installing service / Stopping service /
  Restoring network / Starting service / Waiting for service response. Disable
  conflicting operations. A timeout ends in a specific failure with a next step.
- Success: Ready to connect. The service is running and responding. Return to
  profiles closes the helper; it does not start a VPN. Preserve an existing VPN.
- Details contains concise evidence, the failed step, error code and report export.
  Full logs belong in the report, not an unbounded label.
- No scrollbars or manual resize. Width follows existing dialog conventions;
  height follows visible content and shrinks on collapse/state changes. Use existing
  theme tokens, 18/16 margins, 12 spacing, rounded buttons and section typography.
  Long details use bounded summaries and report export. Validate minimum window
  size, EN/RU, light/dark and 100/150% DPI with real controls.

### Diagnosis and remedy matrix

| Evidence | Explanation and primary action | Workflow / alternative |
|---|---|---|
| Diagnosis pending | Checking what prevents connection | Bounded probes; show which check is pending; no mutation |
| Packaged helper missing/unusable | Application components are missing — Repair application | Explain installer repair/reinstallation preserving profiles; open verified official download destination; do not offer commands requiring the missing helper |
| Service confirmed missing; trusted helper available | Connection service is not installed — Install and start | Existing elevated install, fresh conflict inspection, start, hello/status verification |
| Service stopped; no conflicts | Connection service is stopped — Start service | Guard, elevation if required, start, verify |
| Elevation canceled | Windows/Linux permission was not granted — Allow service start | Explain the upcoming OS prompt; rerun only the canceled action on explicit click |
| Verified owned service running, IPC unresponsive | Service is running but not responding — Restart service | Explicit disruption warning; graceful service stop, verify exit and cleanup, then guarded start and verify. No forced kill or automatic cleanup |
| Stop fails or times out | Service could not be stopped | Show exact failure, keep start blocked, offer supported manual service-management instructions/open OS manager where available; report export remains secondary |
| Confirmed incompatible version | Application and service versions differ — Update service / Update application | Show both versions; choose only a supported signed update path. If service updater cannot work without IPC, use installer repair/update; never show a dead update action |
| Verified leftover developer daemon | Previous developer session is still running — Return to system service | Confirm disruption, gracefully stop the verified owned daemon, verify cleanup, restore/start system service and verify |
| Interrupted journal without live verified daemon | Previous switch was interrupted — Complete restoration | Existing journal reconciliation; only request network recovery when evidence requires it |
| Confirmed incomplete network cleanup | Previous session left network settings — Restore network and service | Warn about direct access, explicit confirmation, recover, verify cleanup, guarded start, verify readiness |
| Endpoint/lock occupied, owner unknown | Another process may be using the connection service | Identify verified owner if possible; explain how to close the other instance; secondary Refresh diagnostics. Never kill an unknown process or offer blind cleanup |
| Access denied during inspection | Cannot inspect the service with current permissions | Name the denied operation; offer supported elevated inspection if the CLI supports it, otherwise explicit OS service-management/installer repair instructions |
| Unknown/malformed service status | Service state could not be determined | Show failed check and diagnostic; offer manual service-management instructions and application repair if helper integrity is invalid; do not guess installation state |
| Ready | Connection service is ready | Return to profiles; advanced management remains separate from repair |

For unavailable automatic remedies, show a short inline instruction page with a
real supported destination/action, not another generic error. No system reboot,
service removal, emergency cleanup or permission escalation runs automatically.
Restart cannot proceed until stop and cleanup are verified. Explicit recovery
authorization is separate from restart authorization.

### Implementation sequence for low-effort execution

1. Capability inventory and contract tests. Inspect existing CLI install/start/
   stop/recover/status/version behavior and platform identity verification. Record
   which remedies can run directly and which require installer/manual guidance.
   Verify the canonical official download URL from repository release metadata.
   No guessed URL, unsigned download, new CLI command or privilege bypass.
2. Extract a typed diagnosis model from MainWindow. Snapshot includes timestamp,
   generation, helper availability/trust, service state, IPC failure/version,
   process identity, developer journal and cleanup evidence. Replace bool+QString
   inspection with typed outcomes and evidence. Derive cause, explanation,
   recommended action and available alternatives once for both UI and dispatch.
3. Implement a recovery controller using existing platform operations. Explicit
   states: diagnosing, awaiting consent, executing step, verifying, ready, failed.
   One operation at a time; generation/lifetime guards; per-step deadlines; fresh
   inspection before mutation. Preserve healthy sessions and settings. Closing
   the dialog must not abandon an elevated process; reopen shows actual progress.
   Diagnose may be canceled; executing privileged operations remain tracked.
4. Implement workflows in order: start; install+start; return from developer mode;
   recover+start; verified restart; installer/update/manual guidance. At each step
   retain the specific failed operation and choose a meaningful next action.
   Never treat helper exit code alone as readiness. Restore saved running intent
   only within its authorized recovery flow; never enable profile auto-connect.
5. Build the helper from this model. MainWindow only opens it and routes existing
   service APIs. Implement all matrix states with consistent EN/RU copy, findings,
   progress and alternatives. Remove obsolete generic Check again dispatch and
   serviceStartBlocked_ boolean inference. Keep Details strictly diagnostic.
6. Test behavior and native presentation; review the matrix against screenshots
   and actual clicks, not only label assertions. Update service-actions.md and
   connection-management.md to remove historical contradictory rules. No release
   or commit is implied by this plan.

### Acceptance and verification

- Every matrix row has a deterministic fixture. Assert the displayed action and
  exact command sequence, including absence of forbidden commands.
- Cover install/start success and failure, permission refusal, stop timeout,
  incompatible version, missing helper, occupied lock, unknown peer, interrupted
  journal, cleanup failure, stale callbacks, close/reopen, duplicate clicks, and
  service becoming ready while inspection is pending.
- Assert that unknown locks do not select recovery, unknown services do not select
  install, healthy VPN is preserved, manual repair never auto-connects, and restart
  never starts a second core before verified stop/cleanup.
- A screenshot equivalent to the reported unavailable-service case must show
  what was actually learned, what could not be determined and at least one concrete
  supported next step; Save report and Refresh diagnostics alone fail acceptance.
- Native captures cover every remedy type, progress, success, failure and expanded
  details; assert fixed bounds, shrink-on-collapse, no scrollbars, containment and
  reachable controls. Review screenshots before declaring style compliance.
- Build Windows x64/ARM64 and Linux amd64/arm64. Run supported native tests and
  investigate the outstanding Windows clipboard failure rather than counting an
  offscreen pass as a native pass. Exercise privileged workflows in an isolated VM
  when available; explicitly report untested UAC/polkit and ARM64 runtime paths.
- No benchmarks are required for this control/UI change unless implementation
  introduces performance-sensitive work. No host service/network mutations in
  automated tests; no packages or release without an explicit request.


### Guided repair capability inventory (2026-11-03)

Source inspection of the sibling Core checkout establishes these boundaries;
installed helper capabilities still require the existing trust checks.

- Windows `cmd/bimeron/service_windows.go` implements start, stop, status and
  install. Stop waits for SCM Stopped for up to two minutes but does not inspect
  service exit codes. The handler reports cleanup failure through a service-specific
  exit code. Therefore stop helper success alone cannot authorize restart; a live
  SCM cleanup/identity check or manual guidance is required.
- Linux `cmd/bimeron/service_linux.go::stopLinuxService` waits for systemctl stop
  and requires the unit Result to equal success. Restart still requires verified
  service ownership, fresh stopped/endpoint/lock checks and compatible hello/status.
- Read-only start inspection now distinguishes occupied endpoints, access denial,
  unknown inspection failures, occupied locks and abandoned Windows mutexes.
  Occupied/inaccessible locks do not establish leftover network configuration.
  Only the abandoned-mutex result explicitly records unconfirmed cleanup.
- A recovery journal records intent, not ownership or proof of network damage.
  Existing reconciliation preserves a healthy verified Linux service. Otherwise
  the prior restoration contract requires separately confirmed cleanup before
  restoration; the UI must describe that uncertainty honestly, never claim that
  unknown ownership or a journal alone proves damaged network settings.
- Private-endpoint regression fixtures exercise typed conflict evidence,
  asynchronous completion, lifetime cancellation and legacy callback compatibility.
  They do not stop services or mutate host networking. Native OS access-denied
  and lock-permission behavior still requires isolated platform integration tests.


### Guided repair implementation validation (2026-11-03)

The extracted ConnectionRepair snapshot/reducer selects typed causes, actions and
phases. DevelopmentCore distinguishes occupied, inaccessible, unknown and abandoned
locks/endpoints. Neither unknown ownership nor an abandoned lock alone authorizes
network recovery. MainWindow executes existing guarded install/start, recovery/start
and developer restoration paths, preserves failure history, and verifies hello plus
fresh status. Details contains bounded diagnostics and an allowlisted report; advanced
service management has a separate dialog. Unsupported restart uses platform-specific
manual instructions and Verify repair result, which refreshes conflict evidence.

Four Release desktop targets compiled: Windows amd64/ARM64 and Linux amd64/arm64.
ARM64 is cross-compile evidence only. All five CTest suites passed on native Windows
amd64 (14.32 s) and Linux amd64 under WSL (10.51 s). Fixtures cover typed diagnosis,
unknown conflicts, exact fake install/start and recover/start sequences, duplicate
commands, failed install, failed stop blocking replacement start, readiness and
retained failure history. No host service or network was mutated.

The native clipboard failure was localized to unsuccessful clipboard fixture input
acquisition: readback did not equal the requested URI and Paste correctly stayed
disabled. The fixture now bounds/retries acquisition before asserting UI behavior.
A subsequent native full suite passed; external clipboard contention can still fail
the explicit fixture-acquisition assertion rather than masquerading as a UI failure.

Native Windows captures under ignored build/guided-repair/final-* cover English and
Russian, light and dark, measured DPR 1 and 1.5. They include preparation, readiness,
Settings, stopped/missing/running/unknown/failed service remedies, unknown ownership,
manual instructions, incompatibility, verification, success and expanded Details.
Fixed bounds, containment, no scroll areas and shrink-on-collapse assertions passed.
English light and Russian dark manual screens were visually inspected; untranslated
advanced-management text was corrected and the eight-way captures repeated.

Review retained the Windows helper cleanup-contract limitation and Linux ownership
limitation: automatic restart is intentionally unavailable rather than weakening
cleanup gates. Real UAC/polkit, elevated restoration/cleanup, native Linux appearance
and ARM64 runtime remain untested. The fixtures do not exercise every native OS
permission error or real crash boundary. Benchmark/profiling is not applicable to
this bounded control/UI change. No build flow changes, packages, commits or releases.


### Review blocker acceptance addendum (2026-11-03)

Privileged commands have a two-minute presentation watchdog. Expiry retains the
process handle, mutation exclusion and completion tracking; the reopened helper
shows still-running/cleanup-unconfirmed guidance. Failed Windows exit inspection
also retains tracking. A fake held command exercises expiry and close/reopen.
Service probes retain allowlisted outcome category, exit code, duration and UTC
observation time, including timeout and malformed replies. Details and the saved
report expose this evidence without raw output. Reports also retain IPC category,
observation time and bounded version identifiers. Only the canonical Service state
response shape is accepted. Manual guidance identifies supported platform tools,
cleanup limits and the repository's official MSI/DEB release destination.

### Guided repair review-blocker validation (2026-11-03)

Final-source Release builds passed for Windows amd64, Windows ARM64, Linux amd64
and Linux arm64 using the existing build presets. ARM64 results are cross-compiles,
not runtime coverage. All five CTest suites passed with the native Windows platform
plugin (14.82 seconds) and Linux amd64 under WSL (10.40 seconds). The focused
connection fixtures exercise held-helper watchdog expiry, operation exclusion,
close/reopen, actual failed/malformed/successful child-process probes, retained safe
probe evidence in Details and report objects, and exclusion of raw secret-bearing
probe output. An initial probe-fixture failure was traced to the visual-check guard
suppressing explicitly injected probes; the guard now permits only the private
fixture override while ordinary visual captures still suppress installed probing.

Native Windows captures in ignored build/guided-repair/review-dpr-* cover English
and Russian, light/dark, and measured DPR 1 and 1.5. The fixture adds expanded failed
probe evidence and watchdog/reopened-watchdog states. Containment, fixed size,
no scroll areas and shrink-on-collapse assertions pass. English and Russian light
screens were visually inspected, including the translated Russian credential
redaction notice, structured probe evidence, manual guidance, and persistent watchdog
presentation. The screenshots are real Qt windows, not offscreen substitutes.

No host services or network settings were changed; no packages, commits or releases
were produced. Real UAC/polkit and elevated cleanup/restoration still require an
isolated platform environment; native Linux appearance and ARM64 runtime remain
external validation limits. Benchmark/profiling is not applicable to this bounded
control/UI change. The build and packaging flow is unchanged.

Final independent review found no blocking issues. Its minor untranslated probe
category finding was corrected using localized display names while preserving
machine identifiers in reports. Four desktop targets rebuilt; focused native
Windows and Linux/WSL tests passed (6.99 s / 4.82 s). WSL initially retained an
outdated test object; invalidating the header timestamp rebuilt the fixture and
resolved the obsolete assertion. Final Russian native expanded probe capture
under build/guided-repair/localized-final.* was visually checked. Earlier complete
suite/matrix evidence and external privileged/runtime limits remain applicable.

### Inline helper follow-up validation (2026-11-03)

Acceptance: deterministic missing/untrusted helper evidence must agree with the
findings, both collapsed and expanded; advanced controls follow fresh service
observations and operation exclusion, without a nested management dialog.

Added a private friend-test snapshot override without deleting installed files.
Native RU captures cover missing-helper and untrusted-helper collapsed/expanded
states under `build/guided-repair/helper-ru.*.png`; geometry observations are in
`build/guided-repair/helper-ru.json`. Missing collapsed/expanded and untrusted
expanded captures were inspected: no clipping or contradictory Available finding.
Probe fixtures cover missing, malformed/unknown, stopped, running, and return to
missing, asserting actual Stop/Update/Uninstall visibility and idle/busy enabled
states. Hidden Install/Start dispatch is exercised with fake signal handlers;
no nested dialog is allowed. No host service or network action is performed.

Windows Release developer-mode-test rebuilt successfully; focused native CTest
passed in 7.38 seconds (7.39 seconds total). Final native recovery geometry capture
passed. git diff --check passed. Existing shared changes were preserved. No
performance-sensitive behavior changed, so benchmarking/profiling is inapplicable.
No platform flow changes or repeat four-target builds, packages, commits or release.
Real elevation and ARM64 runtime limits from earlier validation still apply.

## Approved compact one-click repair contract (2026-11-03)

Status: implementation plan only. The user approved the three-screen mockup in
this conversation (collapsed, expanded, executing) and explicitly requires fidelity
to both its functionality and the existing style. This section supersedes earlier
manual-instruction/application-download primary actions. Historical implementation
and validation records above do not constitute acceptance of this revision.

### Non-negotiable presentation

- One dialog, Bimeron service. Collapsed: specific cause heading, short explanation
  of impact and what repair will do, then one footer containing Details at left,
  Close and the blue Restore button at right. No checklist, refresh button, service
  management button, installer link or instructions in the collapsed state.
- Expanded: insert the compact three-row findings card between explanation and
  footer. Additional bounded technical evidence and report export are inside
  Details only. Do not reintroduce nested management dialogs or a second footer.
- Executing: specific step and progress indicator; Restore is disabled and reads
  Restoring. Progress must reflect measured stages; use indeterminate progress
  when duration is unknown, never invented percentages.
- Ready: explicit readiness confirmation and Close; no implicit profile connection.
  Failure: specific failed step and cause, retained evidence, Restore for a fresh
  guarded repair attempt. Do not replace the primary action with instructions.
- Close hides the dialog without canceling a tracked privileged operation. Reopen
  shows its current phase. Disable repeated repair while an operation remains live,
  including after watchdog expiry; do not label an unsafe retry as available.
- Native content-sized fixed window, no scrollbars or manual resize, height shrinks
  when Details closes. Match existing palette and theme conversion, 6px buttons,
  9px findings card, typography, spacing and margins; use actual application icon.
  The approved mockup is the reference, not a suggestion to redesign during coding.

### One-click behavior

Restore starts diagnosis and runs the appropriate complete workflow. The user does
not choose CLI commands, visit a download page or reinstall the GUI manually. OS
UAC/polkit consent is allowed. A destructive network-reset/disruption warning may
require separate consent; one click does not waive that existing safety boundary.

| Confirmed cause | Required workflow behind Restore |
|---|---|
| Stopped service | Fresh conflict inspection -> start -> compatible hello/status |
| Missing service | Obtain trusted helper -> install -> start -> hello/status |
| Interrupted development switch | Verify owned daemon/journal -> graceful shutdown if needed -> confirm cleanup -> restore system service -> readiness |
| Confirmed leftover networking | Explicit network-risk consent -> recover -> confirm cleanup -> start -> readiness |
| Owned unresponsive service | Confirm disruption -> graceful stop -> prove process exit and cleanup -> start -> readiness |
| Missing/damaged component | Recover verified compatible component through supported trusted mechanism -> repair service if needed -> readiness; preserve profiles/settings |
| Unprotected GUI location | Use a verified protected repair broker or securely provision one; do not execute a writable helper elevated or falsely diagnose missing components |
| Incompatible versions | Select verified compatible component using supported version policy -> repair/update service -> readiness |
| Permission refusal | Explain canceled consent -> next explicit Restore retries the necessary authorization only |
| Unknown observation | Bounded structured diagnosis -> execute only an evidence-supported remedy; otherwise precise failure and retained diagnostics, never blind network reset/process termination |

Unknown failures cannot be guaranteed fixable. The contract is a real attempted
safe repair with an honest result, not success for every possible OS failure.
Do not claim complete support if any ordinary matrix row still opens instructions.

### Implementation work packages and gates

1. Audit actual capabilities before UI edits. Trace current ConnectionRepair
   Action::Manual/Application, TrustedCore protected path restrictions, platform
   service lifecycle and packaging payloads. Read docs/build-and-packaging.md in
   full before designing changes to trusted deployment/packaging. Inspect sibling
   Core contracts read-only and document exact missing guarantees. Deliver a table
   mapping each required workflow to existing operations or a named missing API.
2. Define the necessary backend contracts. Existing code cannot establish safe
   restart/cleanup or securely bootstrap every missing/untrusted helper scenario.
   Specify structured inspection/repair results, ownership, cleanup proof, stage
   events, error codes, timeouts and restart safety. Define trusted broker delivery,
   authenticity verification, protected destination, permissions, atomic replacement,
   rollback and offline behavior. Do not invent a command, bypass path checks or
   elevate downloaded writable code. Core/installer changes are a real dependency;
   record their exact scope before implementation and flag any unavailable dependency
   as unfinished backend work, never silently substitute manual instructions.
3. Implement and verify backend repair operations first. Reuse supported secure
   paths; extend Core/installation contracts only where required. Preserve settings,
   profiles and credentials; do not silently change installation location or reset
   unrelated networking. Require compatible readiness after operation completion.
4. Implement one repair controller. Snapshot -> bounded diagnosis -> repair plan ->
   consent if needed -> executing stages -> verifying -> ready/failed. Serialize
   operations, reject stale callbacks, revalidate before mutation, retain step/error
   history. Failed or canceled attempts do not loop authorization automatically.
   Preserve ordinary startup's one guarded system start and explicit developer mode.
5. Implement the approved dialog literally. Presentation observes the controller;
   Restore dispatches repair, never a URL or instructions page. Put findings and
   report export inside Details. Remove obsolete alternate primary navigation and
   redundant controls from this flow. Keep EN source and complete RU translations.
6. Validate functionality AND visual parity. Each matrix row needs exact fake command
   sequence assertions, including negative commands and readiness. Then run relevant
   native tests/four builds; exercise new privileged/broker flows in isolated Windows
   and Linux VMs before declaring them operational. No host network/service mutation.
7. Independently review the resulting diff and actual native screenshots against
   this contract. Correct findings, update canonical docs, report genuine runtime
   limits. Do not mark the revision complete because existing builds alone pass.

### Required acceptance evidence

- Restore actually installs/starts/repairs the appropriate backend and confirms
  readiness; hidden Install/Start button clicks alone are insufficient evidence.
- Missing vs damaged vs untrusted vs unavailable are distinct, and title, findings
  and action derive from one snapshot. Healthy existing VPN remains intact.
- Unknown peer/lock never authorizes killing a process or clearing networking.
  A missing executable never results in trying to execute that missing executable.
- Tests cover permission refusal, missing/offline component source, signature
  rejection, partial update/rollback, cleanup failure, hung helper, stale callback,
  repeated click, close/reopen and eventual completion after watchdog expiry.
- Final native captures: collapsed, expanded, running, ready and failed for stopped,
  missing, untrusted, interrupted and unknown cases. EN/RU, light/dark, DPR1/1.5.
  Compare with approved mockup: footer alignment, no extra collapsed controls,
  content-driven sizing, actual stage feedback and translated text. Review images
  before handing off; geometric containment does not prove design fidelity.
- Build four supported architectures; native Windows/Linux tests and isolated
  privileged-flow evidence. Track ARM64 runtime separately from cross-compilation.
- Document code changes, checks, review findings and remaining risks. Benchmarks
  only if a performance-sensitive path changes. No release/commit is implied.

### Change control

Do not reduce required behavior to accommodate current implementation limits.
If a backend constraint changes the promised workflow, expose that mismatch before
claiming completion and update the implementation plan explicitly. No return to
manual instructions/download links as the primary remedy. No unsolicited layout
redesign. Implementation order is backend contract, behavior, approved UI, then
behavioral and visual acceptance; all four are mandatory.

### Single elevation session requirement

The user authorized implementation and explicitly requires a single UAC request
per repair attempt. The elevated broker owns the complete bounded inspect/repair/
verify sequence; it must not relaunch runas or pkexec per step. Authenticate the
requesting user/session, constrain commands to repair operations, validate all
inputs and trusted payloads inside the privilege boundary, retain handles and
stage results, and exit after completion. No reusable arbitrary-command elevation
channel. Consent refusal is terminal for that attempt. Test elevation launch count
and no recursive elevation for multi-step repair and failure/rollback paths.

### Implementation audit and integration gates

The following source audit separates available primitives from required integration.
It does not certify the new repair flow as operational.

| Workflow | Existing primitive | Required privileged-session integration |
|---|---|---|
| Stopped | Core service start | Fresh ownership/conflict inspection and compatible hello/status |
| Missing | Core service install and start | Authenticated original requester identity; one process; rollback after failed readiness |
| Missing helper / unprotected GUI | Protected executable ACL checks | Independently deployed protected broker discovery; verified repair payload |
| Incompatible / damaged | Core source update with rollback | Publisher signature and compatibility verification inside privilege boundary |
| Owned unresponsive | Core service stop | Service identity, process exit, cleanup exit status, bounded restart |
| Interrupted development | Desktop journal and daemon peer checks | Authenticated ownership and cleanup proof carried into the single session |
| Leftover networking | Explicit Core recovery | Explicit risk consent and fresh evidence; no inference from unknown locks |
| Unknown | Desktop structured service/IPC probes | Bounded privileged discovery; fail without mutation if evidence stays ambiguous |

The existing source-update digest is supplied by the caller. It proves copied
bytes match that supplied digest, not publisher authenticity. A repair broker
must verify the signed manifest and compatibility itself before accepting a
replacement. Existing service installation also derives ownership from the
current process identity; alternate administrator credentials must never silently
replace the original requesting user as service owner.

The original Desktop working tree already contained changes to DevelopmentCore,
MainWindow, connection tests, Russian strings and service/development documentation.
Those edits are retained. Coordinated Core scope is the bounded repair runner,
platform ownership/cleanup adapters, authenticated transport, payload verification
and focused fixtures. Package scope is protected broker delivery and discovery;
release pins must not claim a new capability before reviewed artifacts contain it.

Privileged validation must use an isolated guest. A running Windows 10 STD
Hyper-V guest is present, but its existence alone does not establish authorized
guest credentials, a disposable checkpoint or completed UAC/runtime evidence.
Other running lab guests are unrelated and must not be modified.

### Implementation handoff status

This revision is implemented in development source but is **not accepted as the
complete approved contract**. The Desktop now dispatches one authenticated broker
session, consumes bounded measured stage/error events, retains the elevated process
through close/watchdog, and verifies fresh hello/status after successful completion.
The compact bilingual dialog uses one footer and expanded-only findings. There is
no manual/download primary action or per-step elevation chain in this repair flow.

Core adapters implement protected-payload installation, start, owned graceful
restart, scoped confirmed network recovery, compatible readiness, executable/DLL
repair and transactional rollback. Protected broker bytes are the source of repair;
the API accepts no source path, digest or arbitrary command. See sibling Core
`docs/REPAIR_SESSION.md` for the exact supported operations and remaining limits.

Outstanding acceptance blockers:

- Old packages do not contain the new protected broker. Optional reviewed-payload
  packaging and protected discovery are implemented, but migration/bootstrap when
  that trust root is absent is not. The current pinned Core archives lack the new
  API. No writable downloaded executable is elevated as a substitute.
- A live legacy developer daemon cannot be adopted from the v1 journal, which
  contains no durable process identity. New durable privileged launch records and
  authenticated live reconciliation remain unfinished. A dead transition can be
  finalized after broker readiness and successful journal removal.
- No signed external component acquisition is implemented in the privileged API.
  Repair uses the already protected package payload; unavailable source remains an
  explicit failure.
- Real UAC/polkit count, cross-user authorization, service/network mutation and
  rollback have not been exercised in disposable guests. ARM64 builds are compile
  evidence only. No production installation or host network mutation was performed.

Final Desktop Release builds passed for Windows amd64/ARM64 and Linux amd64/arm64.
The five native Windows CTest suites passed in 15.21 seconds; the five native Linux
amd64 suites under WSL passed in 10.84 seconds. Tests include one session dispatch,
repeated click/close exclusion, failure without relaunch, private impostor-server
rejection and guarded journal finalization. They do not measure real UAC prompts.
Both repository whitespace checks passed. No performance-sensitive data path was
changed; benchmarks and profiling are not applicable.

The final consent follow-up adds at most two evidence-driven consent challenges
over the same authenticated connection. Each response is bounded, denial is
terminal, and approval causes fresh inspection before mutation. Tests verify that
a newly healthy service is not stopped and no second elevation is launched.
The Windows application rebuilt and its focused native suite passed in 7.43
seconds; all five Linux/WSL suites passed in 10.76 seconds after this follow-up.
Final Core builds passed for all four targets, its native Windows command suite
passed in 0.355 seconds, and the native Linux focused repair/update fixtures passed.
The Core documentation records exact commands. ARM64 runtime remains untested.

Review corrected broker peer authentication, caller identity under alternate
credentials, operation exclusion before the elevated handle exists, stale callbacks,
bounded diagnostics, compatible-version preservation, process/cleanup proof and
initially-stopped transactional activation. Visual review corrected dark findings
contrast, the progress indicator palette, and a multi-monitor capture DPR mismatch.

Final native captures are `build/compact-repair/accepted-{en,ru}-{light,dark}-{1,1.5}.*.png`.
Eight JSON logs each contain 26 states, with every state's measured DPR equal to
its requested 1 or 1.5 (208 images total). They cover collapsed/expanded findings,
stopped/missing/untrusted/unknown cases, execution, watchdog close/reopen, failure
and readiness. Fixed content bounds, footer geometry and no-scroll assertions pass.
Actual final English light execution and Russian dark expanded images were reviewed
independently; earlier final/review/measured prefixes are superseded evidence.

Files touched by this implementation in Desktop (pre-existing edits preserved):
`CMakeLists.txt`, `assets/translations/ru.json`, `docs/build-and-packaging.md`,
`docs/core-recovery-plan.md`, `docs/service-actions.md`,
`packaging/windows/options.wxs`, `src/ConnectionRepair.h`,
`src/DevelopmentCore.cpp`, `src/DevelopmentCore.h`, `src/MainWindow.cpp`,
`src/MainWindow.h`, `src/TrustedCore.cpp`, `src/TrustedCore.h`,
`tests/ConnectionManagementTest.h`, and `tests/DeveloperModeTest.cpp`.
Existing changes in `docs/connection-management.md` and `docs/developer-mode.md`
were retained. No commits, packages, releases or publication were produced.

Coordinated Core file inventory (the sibling checkout was clean before this task):

- `cmd/bimeron/repair_consent_test.go`
- `cmd/bimeron/repair_observe.go`
- `cmd/bimeron/repair_observe_test.go`
- `cmd/bimeron/main.go`
- `cmd/bimeron/service_linux.go`
- `cmd/bimeron/service_update_files.go`
- `cmd/bimeron/service_update_files_test.go`
- `cmd/bimeron/service_update_verify.go`
- `cmd/bimeron/service_update_windows.go`
- `cmd/bimeron/service_windows.go`
- `internal/control/foreground_identity_windows.go`
- `internal/linuxnet/runtime_linux.go`
- `cmd/bimeron/repair_developer_linux.go`
- `cmd/bimeron/repair_dll_windows.go`
- `cmd/bimeron/repair_linux.go`
- `cmd/bimeron/repair_session.go`
- `cmd/bimeron/repair_session_test.go`
- `cmd/bimeron/repair_source.go`
- `cmd/bimeron/repair_source_test.go`
- `cmd/bimeron/repair_transaction_test.go`
- `cmd/bimeron/repair_transport.go`
- `cmd/bimeron/repair_transport_linux.go`
- `cmd/bimeron/repair_transport_other.go`
- `cmd/bimeron/repair_transport_test.go`
- `cmd/bimeron/repair_transport_windows.go`
- `cmd/bimeron/repair_transport_windows_test.go`
- `cmd/bimeron/repair_windows.go`
- `cmd/bimeron/service_update_context_test.go`
- `docs/REPAIR_SESSION.md`
- `internal/windowsnet/repair_windows.go`

### Protected GUI bootstrap follow-up

The missing-broker path permits a protected updated GUI containing the reviewed
broker resource to provision those bytes and enter the same repair session with
one elevation. Acceptance requires no GUI/plugin/settings startup in the elevated
entry, protected output ancestry, exact byte verification, no arbitrary commands,
Linux PID preservation, and Windows retained-parent/child image proof. Portable
writable GUIs and old installed GUIs fail closed. This is not complete historical
package migration or real UAC/polkit acceptance. Prior build evidence does not
cover this follow-up; final matrix and embedded-resource checks are required.

Bootstrap follow-up validation: all four Desktop Release targets built after the
atomic-provisioning, bounded-hash and Windows elevation checks. Five Windows x64
CTest suites passed in 13.37 seconds; five Linux amd64 WSL suites passed in 10.71
seconds. Added negative fixtures reject missing embedded resources, absent parent
identity, relative directories and regular-file output directories. Whitespace
checks passed. These default builds contain no embedded broker resource, so they
do not establish positive embedded-payload provisioning or actual elevated child
acceptance. ARM64 remains compile-only. No packages or host services were changed.

### Parent bootstrap integration check

The parent corrected Linux bootstrap to open the payload with O_NOFOLLOW, check
owner/mode/size and hash that descriptor, then fexecve the same inode. Windows
child authentication now returns an owned process handle retained until the
repair socket is destroyed. These changes still need independent security review
and positive runtime/privileged acceptance; inode binding does not prevent a
privileged writer from modifying bytes in place.

Fresh Core manifest hashes were used for a separate ignored Windows amd64 build:
build/bootstrap-reviewed-windows-amd64. Configure and Release desktop compilation
passed with embedded broker and runtime resources. This is positive development
embedding BUILD evidence only, not a protected installation or UAC execution.
The source artifacts remain uncommitted development builds, not signed release
payload acceptance. Normal Desktop builds passed for all four architectures.
Linux focused developer-mode-test passed in 5.29 seconds. Native Windows
--bootstrap-auth-rejection passed after warming Qt/security initialization before
measuring repeated rejection handles. The subsequent full developer-mode-test
failed at native clipboard fixture acquisition; no clean full Windows pass is
claimed. Rejection tests do not prove successful child authentication or PID-reuse
handling. Positive retained-handle, Linux inode-execution and actual UAC/polkit
checks remain outstanding. No host repair, package or release occurred.

### Quit ownership regression acceptance

Quit during delayed repair authorization must leave repair ownership intact even
before a launcher handle exists. With development cleanup pending, Quit and its
scheduled retry must not enter development selection, change the development
phase, or launch another elevated command. The focused native Windows fixture
`developer-mode-test --repair-quit-ownership --visual-check` passed after rebuilding
the Release target. It exercises both cleanup states through a 300 ms retry window.
The successful native embedded-child authentication fixture is still outstanding;
this result makes no claim about successful authentication, retained child-handle
lifetime, protected ACLs, UAC, or ARM64 runtime behavior.

### Native bootstrap authentication fixture

Acceptance: a same-image parent launches a harmless test-executable child serving
a user-only local pipe. Verify its actual pipe server PID, process ancestry,
creation times, image digest and live state. Authentication returns an independent
owned handle that survives closing the launch handle and is closed on socket
destruction. Reject unrelated parents, exited children, denied image paths and
mismatched payload bytes.

The private per-call payload/path-policy seam replaces only embedded-payload
selection and protected-path policy for this fixture; production retains both
original policies. No global bypass, Core invocation or elevation is involved.
Windows amd64 Release developer-mode-test --bootstrap-auth-success passed.
Other platforms, packaging and UAC acceptance are outside this fixture's scope.

Parent follow-up verified the production authenticatedChild wrapper always uses
its embedded payload and TrustedCore::protectedExecutable; only the private
per-call test helper accepts a fixture policy. Native Windows success, rejection
and repair-quit-ownership modes all passed again. Windows ARM64, Linux amd64 and
Linux ARM64 rebuilt successfully after the fixture/ownership refactor; all five
Linux/WSL CTest suites passed. These checks do not cover production protected-path
ACL deployment, real elevation count or privileged rollback. Prior full Windows
clipboard-acquisition failure remains distinct from passing isolated fixtures.

### Windows repair without a developer record

A fresh machine need not have a protected developer recovery record directory.
Acceptance: absence of that optional directory/file must not become a service
identity mismatch; all other protected-path failures remain terminal. An existing
unrecorded local daemon still blocks automatic installation. Windows native
error propagation and a real missing-service repair must be checked separately
from Desktop broker authentication fixtures. No dialog redesign is required.

Windows bootstrap runtime completeness: the embedded and provisioned payload must
include WINTUN-LICENSE.txt beside wintun.dll. Core service installation copies both
files; a broker/DLL-only bootstrap cannot install a missing service. Use the same
verified package license input and exact-byte provisioning as the existing DLL.

Readiness follow-up: the Core broker and updater must use a standard status
request and compare response instance IDs after hello. The ordinary Core server
rejects foreground-only request fields; readiness fixtures must use its real
dispatcher rather than fabricated successful response objects alone.

### Windows missing-record repair outcome (2026-11-04)

The reported identity failure was reproduced after the old local daemon exited:
Windows optional-record directory absence lost its native error in protected-path
locking. Core now preserves not-found causes through both directory and image
locks while retaining identity rejection. Native missing-file/parent and sharing
violation regressions pass; existing ACL and redirected-path fixtures remain in
place. Standard readiness now omits foreground-only request fields and compares
nonempty response instance IDs. A real standard IPC server with an inert engine
covers repair and update handshakes. Review also fixed a malformed JSON tag found
by go vet in the existing acquisition implementation.

The manually prepared protected test installation lacked WINTUN-LICENSE.txt.
Its payload was completed, and Windows GUI bootstrap now embeds/provisions that
same package license beside the DLL so missing-service installation has both.
No new release inputs, dependencies, package formats or release pins were added.

Live authorized host recovery: verified the previous local daemon's actual pipe
PID, retained OS process handle, executable path, creation time and current-user
ownership; status was disconnected. Normal IPC shutdown acknowledged disconnected
and the retained process exited without forced termination. Before the Core fix,
repair still returned identity-mismatch. After the fix and runtime completion,
the broker emitted inspect/install/start/verify/ready and done ok=true. The later
final broker repeat emitted inspect/verify/ready without restarting the healthy
service. SCM reports BimeronCore Running, manual start. Direct service-pipe OS PID
and matching hello/status instances were checked; state remains disconnected.
The updated protected Desktop is running and native accessibility inspection
shows Disconnected and an available Connect button. Profiles and their settings
were not edited. This is host acceptance for this specific reported case, not
full disposable-guest, alternate-credential or UAC-count acceptance.

Validation: five native Windows Desktop CTest suites passed (15.75 s); twenty
configure-gate negative fixtures passed. Core go test for cmd/bimeron,
internal/control and internal/windowsnet and go vet passed. Focused native Linux
repair/developer/update/readiness tests passed under WSL. Core built for Windows
amd64/ARM64 and Linux amd64/arm64. Desktop Windows amd64 and ARM64 built with the
correct final embedded broker and complete Windows runtime resource. Linux
Desktop rebuilds were not required: its changed bootstrap/CMake branches are
Windows-only; the shared Core readiness change was built/tested on Linux.
ARM64 is compile evidence only. No packages, release, commit or publication were
made. No performance-sensitive data path changed, so profiling/benchmarks do not
apply. Diff review and both whitespace checks passed; broader existing repair
migration/legacy-daemon limitations remain documented above.

### Registered broker from a development GUI

Acceptance: the service header displays Running (Russian: Работает) in the
existing success palette, including its dark-theme counterpart. Other service
states retain their established colors.

A GUI launched from a writable development directory may restore a stopped
service through an independently installed protected broker registered by the
machine installer. Findings must account for that broker rather than diagnosing
the writable GUI path as the repair trust failure. The selected executable is
still revalidated before elevation; no writable executable or HKCU/PATH override
is accepted. Test preparation must register the protected development installation
using the same HKLM InstallDirectory contract as MSI. Exercise the real Restore
action from the development GUI with a stopped service and verify fresh IPC.

Terminal repair results must refresh the independent service observation so that
fresh IPC readiness cannot be displayed beside a stale pre-repair Stopped value.
The regression fixture must complete a broker attempt from Stopped and consume a
new private CLI observation of Running, without host service/network operations.

Registered-broker follow-up validation: the reported process was the ordinary
build/windows-amd64/Release GUI, not the protected staged GUI. The manually staged
broker had no HKLM InstallDirectory value. The test installation is now registered
using the MSI machine-registration contract, with a protected HKLM key and existing
protected broker ancestry. The writable GUI now recognizes that independent trust
root in its assessment; execution still uses the same protected discovery and
revalidation. No trust check was removed and no writable helper was elevated.

Running now renders as capitalized Running / Работает using #16803c in light mode
and the existing #76d49a dark palette mapping. After terminal repair, an independent
fresh service-status probe replaces stale stopped observations. A private fixture
starts from Stopped, completes a successful broker result and observes Running;
it also ensures that the observation alone does not establish IPC readiness.
The fixture cancels its unrelated pending IPC before waiting for the CLI probe,
so asynchronous backend callbacks cannot contaminate this service-only assertion.

The disconnected owned Windows service was force-terminated once for the user's
reported scenario; SCM observed exit code 1067 and the build GUI Restore action
was invoked with the registered protected component. SCM recovery can race this
attempt; the service returned Running. A separate normal stop and subsequent
Restore interaction also returned Running with fresh matching Running/Responding
findings in the build GUI. This does not certify crash cleanup under an active
VPN or attribute every restart to the broker rather than SCM recovery.

All four Desktop targets built. Five native Windows CTest suites passed in 15.76 s;
the final isolated developer-mode suite passed in 8.10 s after tightening fixture
isolation. All five native Linux/WSL suites passed in 11.81 s. ARM64 remains
compile-only. The protected staged Windows GUI was also rebuilt and updated.
Whitespace review passed. No performance-sensitive path changed; no benchmark or
profiling was needed. No package, release, commit or publication was produced.

### Visible diagnostic details and repair logging

The user explicitly requests that diagnostic free text be shown even when it
contains credentials. Remove the generic credentials-hidden placeholder entirely.
Keep bounded plain-text rendering and existing translated explanations, but retain
other diagnostic text in the expanded repair details and exported repair report.
Failed/malformed service probes retain their actual bounded output; structured
Core report schema filtering is separate from this free-text display policy.

The existing Journal must record repair stages, helper completion, final verified
readiness, cancellation/start failures and terminal repair errors. Log every
observed service-state transition, including unavailable/unknown observations,
but do not repeat identical states on periodic probes. Successful helper exit
must be logged as awaiting verification, not as established service readiness.
Tests must cover visible credential-bearing diagnostics, bounded output, failure
reports, service-transition deduplication and final verified repair results.

Visible-diagnostics/logging implementation validation: the generic hidden-details
message and its Russian catalog entry are removed. Diagnostic free text is bounded
to 16 KiB and rendered as plain text or escaped Journal content, including URI and
credential-bearing text. Profile-validation popups use explicit plain text. Core
error detail is available in the error dialog and Journal. Structured Core report
field filtering remains a separate contract; the repair report retains actual
bounded failed-probe output, last Core error, startup and repair diagnostics.

Journal entries now cover broker progress (including replacement), start failure,
terminal failure/cancellation, helper completion awaiting readiness and final
verified readiness/failure. Observed service state and native pending-state code
are deduplicated together, so Starting -> Stopping is logged even though both are
transitional in the availability model. Existing log save/clear behavior is reused.

Regression fixtures verify supplied credential text in dialog/details/report/log,
plain-text formatting, size bounds, failed-probe retention, successful and failed
repair results, no premature success, unchanged-state deduplication and distinct
pending-state transitions. Obsolete redaction assertions were updated to the
user's explicit visibility policy; profile preservation and SSH-answer isolation
remain tested. Five final Windows CTest suites passed in 15.25 seconds and five
Linux/WSL suites in 12.10 seconds. All four Desktop targets built; ARM64 remains
compile-only. The protected embedded-broker Windows GUI was rebuilt and updated,
and the ordinary build GUI was launched with Journal open. The host service
remained Running; no VPN/network mutation was needed for this follow-up.

Both Desktop and sibling Core are checked out on feat/core-recovery, retaining the
combined working-tree changes from this thread and thread
718b11e6-7e4c-4387-9295-4578eb5155df. No commit, package or release was produced.
Diff review checked plain-text rendering, protocol stage coverage and duplicate
logging; whitespace checks passed in both repositories. Documentation was updated
in service-actions.md and connection-management.md. No performance-sensitive path
changed; profiling/benchmarks were not applicable. Broader privileged repair and
ARM64 runtime acceptance limits documented above remain unchanged.


### PR snapshot isolation

The Desktop PR snapshot excludes concurrent unified-profile-creation edits from
the shared checkout. A separate GUI-only Windows build found that the service
availability fixture implicitly relied on a bundled local Core executable.
The fixture now explicitly supplies available/trusted components for service-state
scenarios; missing-component scenarios retain their own separate coverage.
This is a fixture precondition correction, not a production behavior change.
The snapshot uses the original Add key / Import actions and profile APIs.

The isolated GUI-only Windows Release snapshot compiled successfully. All five
native CTest suites passed across the focused runs; developer-mode-test passed
in 8.51 seconds after correcting both component preconditions. Twenty production
configure-gate negative fixtures also passed. No concurrent profile-creation code
or documentation is part of this PR snapshot.
