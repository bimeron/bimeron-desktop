# Session-scoped developer mode

Status: implemented; validation results and remaining release checks are recorded below.

## User-visible behavior

- Count five left clicks on the Desktop version label in About, not the Core version label. Count within the current application process, without a timing requirement; leaving About does not reset the count. Double-click events must not add extra synthetic clicks.
- The label retains its current appearance, cursor, focus behavior, and accessibility role. No hover effect, tooltip, click animation, counter, or activation notification is added.
- On the fifth click, restart the GUI and open a new Development tab. Do not interrupt the system-service VPN merely to unlock the tab. Defer the restart while a service update or another privileged operation is in progress.
- Unlock state is passed only to the replacement process, never persisted in settings or autostart entries. A normal subsequent launch always starts without Development and targets the system service.
- Minimizing or closing to the tray is not an application exit. Explicit Quit ends the developer session. An application-requested theme/language restart ends this session too: clean up the local daemon first, then restart in normal mode. Only the initial unlock restart enables Development.
- Development contains a backend selector: System service (initial selection) or Local core executable; an absolute-path field with Browse; and an explicit Apply action. Selecting a file alone does not launch it. Show the active backend, effective path, running Core version, and transition/error status.
- Developer state and the selected path are session-only. Existing profiles and ordinary application preferences retain their current persistence behavior.

## Existing integration and constraints

- `src/main.cpp` already supports `--restart` with a bounded single-instance lock wait.
- `src/MainWindow.cpp` builds navigation and About, performs automatic service startup, manages privileged service commands, and has separate theme/language restart paths.
- `src/CoreClient.cpp` currently targets only `Bimeron.Core.Service` on Windows and `/run/bimeron/control.sock` on Linux.
- The sibling Core checkout supports `bimeron daemon`, requiring Administrator/root privileges. It is a foreground process and accepts IPC `shutdown` for network cleanup and exit.
- Windows console-daemon IPC is `Bimeron.Core.<hex>`, where hex is the first 16 bytes of SHA-256 of the elevated user's SID. Core CLI IPC prefers the system service, so invoking arbitrary CLI commands is not a substitute for selecting the GUI's IPC endpoint.
- Linux console daemon and service share `/run/bimeron/control.sock` and a daemon lock. Its control owner uses `BIMERON_CONTROL_UID` or `SUDO_UID`; a root-only socket would prevent an ordinary GUI from connecting.
- Recheck this contract against the selected binary and the pinned release during implementation. Unsupported binaries must fail clearly. Do not assume the sibling checkout describes every historical binary.

## Backend lifecycle

Use a small explicit state model: System, Switching, Local, and Failed. Keep backend state, process ownership, endpoint, and service restoration information together. Avoid a general plugin/backend framework.

1. Validate an absolute existing executable path and probe its version without elevation, asynchronously with a timeout. Preserve paths with spaces and Unicode; pass arguments without a command shell. Windows requires matching Wintun alongside the selected core. Architecture or missing-runtime failures must produce actionable errors.
2. Before applying a backend change, explain that the active VPN will disconnect and that a local executable runs with elevated privileges. Cancellation retains the current backend and connection. Do not persist this consent or silently elevate on file selection.
3. Suspend automatic service startup, reconnection, background update application, and regular IPC polling during transition. Finish or cancel pending requests and invalidate old responses so they cannot update the new backend's state.
4. Record whether the system service was running. Disconnect its VPN, stop the service if running, and verify it stopped before starting a local daemon. Use the existing bundled helper for service management, never replace the installed service executable or change its registration/startup configuration. An active signed update blocks switching.
5. Launch only the explicitly selected executable with `daemon`, using UAC or polkit and retaining process identity/lifecycle information. Keep the GUI unelevated. On Linux, arrange the original GUI UID through the existing Core control-owner contract; do not rely on polkit preserving arbitrary environment variables. On Windows, derive and verify the actual elevated account's pipe identity; reject an unsupported different-account elevation rather than connecting to the wrong daemon.
6. Connect directly to the intended daemon endpoint and require a successful compatible `hello`. Verify endpoint ownership against the launched process using platform facilities; do not adopt an already running unrelated daemon or treat its hello as launch success. Record the returned instance identity. A timeout, UAC cancellation, process exit, or incompatible response rolls back the attempted transition.
7. Commit the UI backend only after readiness. Do not reconnect a profile automatically after a backend switch; let the developer explicitly connect using the selected core. Profile requests, statistics, events, capabilities, About version, and diagnostics must all reflect the active backend. Diagnostics that cannot explicitly target the local daemon must be unavailable with an explanation, rather than reporting on the service.
8. While Local is active, disable system-service install/start/stop/remove/update actions and suppress automatic service recovery. Signed updates must not replace the chosen local executable or use it as the trusted service-update helper. Background check results must not offer an actionable update against the local backend.
9. Returning to System sends shutdown only to the owned local daemon, waits for network cleanup and process exit, restores the previous service running state, and selects the service endpoint. If the service was originally stopped or absent, leave it so and show that state; do not let automatic startup immediately undo restoration. Switching to another local file first completes the same owned-daemon cleanup.
10. Explicit Quit always cleans up an owned local daemon, regardless of the normal stop-core-on-exit preference, and restores service availability if this session stopped a running service. Do not automatically reconnect the old system VPN. Preserve existing quit behavior when no local transition has occurred.
11. Failures remain visible and retain enough in-memory ownership/restoration state for retry. Never report a successful return while the local daemon is still active; never silently fall back to the service after local-core failure. Use bounded waits and keep the UI responsive. Do not force-kill a daemon during network cleanup as a normal shutdown path.

Abrupt GUI termination or OS failure cannot guarantee cleanup using the current foreground-daemon contract. A subsequent normal GUI launch must still hide Development and select the system service; it must not adopt or kill an unrelated leftover daemon. Report a conflict when service startup fails. Parent-death supervision requiring Core changes is outside this first implementation and must be documented as a limitation, not claimed as solved.

An elevated service command that outlives its two-minute watchdog remains tracked, with a visible waiting status and switching disabled until it exits. The GUI does not kill a privileged helper mid-operation or abandon its handle and start a conflicting rollback. Startup probes and daemon readiness/cleanup have bounded timers. On Linux an already-root process launches directly; ordinary GUI sessions use polkit with an explicit control-owner UID. Windows elevation under a different account is rejected and may require manually closing the launched console daemon before retrying.

After a failed transition, the error remains visible and normal polling stays suspended until the developer explicitly retries or applies System service. This prevents a local-core failure from silently turning into a connection through the system service.

## Implementation sequence

1. Implement session activation and restart handoff in `main.cpp` and `MainWindow`, including failure handling when the replacement process cannot start. Reuse the existing lock protocol and ensure exactly one window survives.
2. Add the Development page and English source strings/Russian translations. Keep selected values in memory and distinguish pending selection from the active backend.
3. Add narrowly scoped backend lifecycle handling and explicit endpoint selection/reset to CoreClient. Centralize runtime backend routing while keeping the trusted bundled service helper separate.
4. Integrate privileged daemon launch, readiness, rollback, restoration, diagnostics, and all quit/restart paths. Audit every existing `coreExecutablePath`, service startup, and updater caller.
5. Add focused behavioral tests and perform native Windows/Linux manual checks. Review the complete diff and update README plus relevant development documentation. Update the canonical build document only if the implementation changes its described flow, after reading it completely.

If endpoint ownership, elevation, or cleanup cannot be implemented with the existing Core contract on a supported platform, update this specification with the concrete Core dependency before proceeding. Do not ship a file picker that only changes a helper path while the GUI still talks to the system service.

## Acceptance and validation

- Clicks one through four produce no visible change; the fifth restarts once and opens Development. Other About labels do not activate it. Double-click counting, reopening About, and restart launch failure are covered.
- Quit followed by normal launch removes the tab and local selection. Tray hiding preserves the session. Theme/language restart cleans up local mode and returns to normal mode. Autostart never inherits developer mode.
- An active system VPN survives the unlock restart. Applying Local requires explicit disconnect/elevation consent and actually exchanges IPC with the chosen executable; verify with distinguishable Core builds and instance identities.
- System-to-local-to-system and local-to-local transitions pass with the service initially running, stopped, and absent. Installed service bytes, registration, and startup configuration remain unchanged.
- Cover nonexistent files, unsupported/incompatible binaries, wrong architecture, missing Wintun, spaces/Unicode, cancelled elevation, occupied endpoints, timeout, daemon crash, stale IPC replies, pending signed update, and failed service restoration. No unintended system IPC fallback or update occurs.
- Test Quit with both values of stop-core-on-exit. Local VPN cleanup completes, the owned daemon exits, and the service's prior running state is restored. Exercise close-to-tray separately.
- Run existing `core-update-test` and `profile-vless-test`, plus focused activation/routing/lifecycle tests. Use fakes for transition failures and real elevated processes in a disposable Windows/Linux environment for integration; mocked IPC alone is insufficient.
- Build Windows amd64/ARM64 and Linux amd64/arm64 for compile coverage. Run native tests and manual UI/lifecycle checks where runners exist; explicitly report unavailable ARM64 runtime coverage. Check Russian/English text and 100%/150% scaling.
- This is not a throughput change: benchmark/profiling is not required unless launch, polling, or UI responsiveness regresses. Check bounded startup/shutdown and no blocking GUI-thread waits.
- No release or package publication is requested. Before a later release, run the full supported test/build/package matrix required by the repository instructions.

## Validation record (2026-10-24)

- Windows amd64 and ARM64, Linux amd64 and arm64 configured and compiled locally with the supported presets. ARM64 binaries were cross-compiled, not executed on ARM64 hardware.
- Windows amd64 and Linux amd64 passed `developer-mode-test`, `core-update-test`, and `profile-vless-test`. Activation tests use isolated preferences and a replacement test process; they check the silent four-click threshold, fifth-click restart, and the new tab's visibility/selection. IPC tests cover matching/mismatched peer PIDs, endpoint replacement, and stale timeout suppression.
- A real pinned Linux amd64 Core daemon passed startup, PID-checked hello, shutdown, and local-to-local replacement inside private mount, network, and PID namespaces. Service management used a fixture, covering initially running/stopped/absent states, startup failure rollback, failed service restoration, and restoration retry. These checks do not validate actual systemd transitions or polkit prompts; the isolated process was already root.
- The Development page was rendered and inspected offscreen at 150% scaling in English. Native Windows/Russian visual checks remain outstanding.
- Review addressed stale socket callbacks after switching, fail-closed daemon identity checks, background monitoring during an asynchronous replacement probe, and update/switch dialog reentrancy. `git diff --check` passed. No performance benchmark is applicable; polling and transition timers remain asynchronous.
- Real Windows UAC, Linux polkit, live VPN cleanup/reconnection boundaries, actual service restoration, and ARM64 runtime checks remain release validation requirements. No MSI/DEB release was prepared. Forced GUI termination can leave the daemon running, as documented above.

The opt-in Linux integration mode of `developer-mode-test` requires an isolated
`/run/bimeron-desktop-integration-service` sentinel and a real Core path:

```bash
sudo unshare --mount --net --pid --fork --mount-proc bash -c '
  mount --make-rprivate / &&
  mount -t tmpfs tmpfs /run &&
  mkdir -p /run/user/0 && chmod 700 /run/user/0 &&
  touch /run/bimeron-desktop-integration-service &&
  QT_QPA_PLATFORM=offscreen build/linux-amd64/developer-mode-test \
    --integration-core "$PWD/.tools/core/linux-amd64/bimeron"
'
```
