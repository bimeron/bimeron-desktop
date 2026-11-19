# Reconnect policy presentation

Specification for Desktop 0.3.1; issue [Desktop #60](https://github.com/bimeron/bimeron-desktop/issues/60).
It extends [bounded reconnect recovery](reconnect-attempts.md), which stays the
behaviour with a Core that does not report the new policy.

The Core contract is [bimeron-core #162](https://github.com/bimeron/bimeron-core/issues/162)
(pull request [#163](https://github.com/bimeron/bimeron-core/pull/163), merged as 9521d8b4):
Core `docs/CONTROL.MD`, "Reconnect recovery and kill switch hold", and
`docs/GUI_INTEGRATION.MD`, "Guidance for Desktop 0.3.1".

Core owns reconnection. Desktop only presents what Core reports and sends
explicit user commands; it never schedules attempts, counts them or releases
protection on its own. Routing, GeoData rules and the per-profile kill switch
setting (off by default) are unchanged.

## Core policy being presented

- Initial connect: the first attempt plus up to three retries, then `failed`.
  A nonretryable failure is `failed` at once.
- After an established session is lost: endless cycles of ten attempts,
  separated by a 60-second pause (phase `waiting`). A wait ends early on
  `reconnect_now`, a network change or a resume from sleep.
- After a loss, errors a retry cannot fix (for example, a rejected key) follow
  the same cycle; `LastError` keeps the specific code.
- Native IKEv2 keeps three retries without cycles.
- With the kill switch on, Core keeps protection between attempts, during pauses
  and after `failed` until the user disconnects (`stop` or `shutdown`).

## States and texts

English is the source text; Russian is in `assets/translations/ru.json`.

| Core report | Status line | Colour | Extra line | Controls |
|---|---|---|---|---|
| Initial connect, attempt n of 3 | `Connecting… (n/3)` | yellow | held line | Disconnect |
| Interrupted, attempt n of 10 | `Reconnecting… (n/10)` | yellow | held line | Disconnect |
| Interrupted, pause | `Server unavailable · retrying in 0:59` | red | cause, held line | Retry now (if advertised), Disconnect |
| Initial-connect failure, protection held | `Couldn't connect. Check your internet connection and access key. Internet access stays paused until you select Disconnect.` | red | none | Retry now (if advertised), Disconnect |
| Failure without held protection | `Connection failed` | red | none | Connect |
| Leftover protection, no run (`KillSwitch: "residual"`) | `Internet is blocked by leftover VPN protection.` | red | none | Fix connection (emergency network recovery); Connect blocked |

- The countdown is the time until Core's next attempt, as `m:ss`, rounded up
  and refreshed every second. Without a valid time the pause reads
  `Server unavailable`.
- The held line `Internet access is paused until the VPN connects.` appears
  only while Core reports that protection is held.
- The cause is the localized catalog text of `LastError`. It is shown during the
  pause, so an error a retry cannot fix stays visible. The status tooltip keeps
  the catalog code and text as in Desktop 0.3.0.
- The main connection button reads Disconnect in every state where it
  disconnects: connected, connecting, reconnecting, the pause and a held
  failure. While reconnecting, in the pause and whenever protection is held,
  its tooltip reads `Internet will work without VPN protection.` A proxy-mode
  session has no tooltip unless protection is held. When the button offers a
  profile switch it has no tooltip, and a selected native IKEv2 profile shows
  its unavailability reason instead.
- While protection is held, the main button stays Disconnect even when
  another profile is selected, and pressing it only stops the session. Switching
  profiles would otherwise release and re-arm protection.
- Retry now is next to the status line. It appears during the pause and in a
  held failure, only when Core advertises the capability. It sends
  `reconnect_now` once, guarded with the current `instance` and `session`, and
  then rereads the status. In a held failure Core restarts the initial attempts,
  shown as `Connecting… (n/3)`. An error such as `reconnect_not_pending` (an
  attempt has already started) is written to the log through the catalog and the
  status refresh shows the current state.
- Desktop never sends `start` while protection is held. Core would accept a
  handover to another kill switch profile, but Desktop offers only Disconnect
  and Retry now in that state. A `kill_switch_held` start error is shown through
  the catalog like other start errors.
- `start` waits up to 2 minutes, as Core recommends: with the kill switch it
  returns only after protection is installed, and a handover first waits for
  the previous cleanup. A `session_already_running` reply (a repeated start of
  the running profile) is only logged and the status is reread; the live
  session stays up and no error dialog is shown.

## Leftover protection and network recovery

After a crash, a power loss or a reboot during a protected session, Core can
run with no session while its protection is still installed. It reports
`failed`, `KillSwitch: "residual"`, `LastErrorPermanent: true` and
`LastError: "cleanup_failed_run_recover"`, unless another failure is already
shown; `Session` is absent or inactive.

- Desktop routes to emergency network recovery when `LastError` ends in
  `_run_recover` (for example `cleanup_failed_run_recover`,
  `kill_switch_pending_run_recover` or `kill_switch_arm_failed_run_recover`) or
  when `KillSwitch` is `residual`. A `start` error ending in `_run_recover` does
  the same. Connect is blocked and Fix connection opens the existing Emergency
  network recovery action, as for a failed cleanup.
- With `residual`, the status line reads
  `Internet is blocked by leftover VPN protection.` The text is pending user
  approval and lives only in `ReconnectPolicy::residualProtectionText()`.
- `residual` is not `holding`: it never shows Retry now, the paused line or the
  held Disconnect button.
- Other recovery codes keep the existing `Could not prepare connection` text.

Desktop 0.3.0 sees `cleanup_failed_run_recover` and shows the same recovery
path with its generic text.

## What "Internet access is paused" means

While Core holds protection (`KillSwitch: "holding"`), traffic that the routing
policy does not send `direct` is blocked until the VPN connects or the user
disconnects. DNS on ports 53 and 853 (TCP and UDP) outside the tunnel is
blocked for every process except Core, including a resolver inside a `direct`
range, so names do not resolve outside the tunnel either. See Core
`docs/KILLSWITCH.MD`.

## Known limitations

Core behaviour that Desktop 0.3.1 presents but does not change; these also go
into the 0.3.1 release notes:

- Forwarded traffic (Internet Connection Sharing, WinNAT for WSL2 or Hyper-V
  guests) is not covered by the Windows kill switch
  ([bimeron-core #164](https://github.com/bimeron/bimeron-core/issues/164)).
- mDNS (UDP 5353) stays permitted where a `direct` rule covers it. LLMNR
  (UDP 5355) and NetBIOS name service (UDP 137) are tracked in
  [bimeron-core #166](https://github.com/bimeron/bimeron-core/issues/166).
- A Core service update restarts the service, so protection is not in place
  while the service restarts.
- When the outbound path is blocked rather than closed, Core takes about 80
  seconds to detect the loss before the first retry cycle starts (observed on
  the Windows test VM).

## Core update confirmation

`src/CoreUpdatePrompt.h` builds the text of the "Core update ready" dialog:

- A session counts as active while it connects, is connected or reconnects,
  and in a failed run that holds protection (`failed`, `KillSwitch: "holding"`,
  `Session.active`). The update stops it, and after a verified update Desktop
  starts the same profile again, exactly as for a connected session.
- When the session owns kill switch protection (`KillSwitch` is `active` or
  `holding`), the interrupt text adds: "Leak protection will be removed during
  the update, and internet traffic will bypass the VPN." («На время обновления
  защита от утечек будет снята, и интернет пойдёт без VPN.») The service restart
  releases that protection.
- No warning without a kill switch, in proxy mode (Core reports no
  `KillSwitch` there) or for `residual`. A residual block routes to network
  recovery, and Desktop does not offer the update while recovery is pending.

## Tray notifications

When notifications are enabled:

- one message per retry cycle: `VPN connection lost. Reconnecting…` for the
  first cycle, then `VPN is still not connected. Retrying…`;
- `VPN reconnected` when the session recovers after a cycle, instead of
  `VPN connected`;
- proxy sessions use the same messages with "Proxy".

Stop, disconnection and failure end the sequence. A connect after that is not a
recovery. Existing connected, failed and disconnected notifications are unchanged.

## IPC mapping and compatibility

`src/ReconnectPolicy.h` keeps a small model (`State`), the presentation function
and the notification sequence. `ReconnectPolicy::fromIpc()` is the only code
that reads the policy fields:

| Model | Core contract (additive IPC 1) |
|---|---|
| New policy reported | capability `reconnect_now` or `kill_switch_hold`, or status `ReconnectPhase`, `KillSwitch` or `ReconnectCycle` |
| Interruption or initial connect | `ReconnectCycle` present (from 1) or absent |
| Phase | `ReconnectPhase`: `attempting` or `waiting` (only in `reconnecting`); briefly absent while the previous session closes, shown as plain `Reconnecting…` |
| Attempt n of N | `ReconnectAttempt`/`ReconnectLimit`: 1–3 of 3 or 1–10 of 10, restarting at 1 in each cycle; absent while waiting |
| Next attempt time | `NextAttemptAt`, RFC 3339 UTC, whenever a backoff or pause is scheduled; absent while an attempt runs. The countdown is `NextAttemptAt` minus local now |
| Permanent error | `LastErrorPermanent`: retrying alone will not fix `LastError` |
| Protection held | `KillSwitch` is `holding`; in `failed` also `Session.active` |
| Leftover protection | `KillSwitch` is `residual` |
| Network recovery required | `LastError` ends in `_run_recover`, or leftover protection |
| Retry now | capability `reconnect_now`; command `reconnect_now` |

Retry now is hidden while an attempt runs, so `reconnect_not_pending` appears
only after a race. The cause is shown for every error during the pause;
`causeOnlyWhenPermanent` in `src/ReconnectPolicy.h` limits it to permanent
errors if that is preferred.

`KillSwitch: "active"` (protected and connected) needs no notice. Invalid
numbers and times are ignored as in Desktop 0.3.0. Status is authoritative:
Desktop does not depend on the new progress events.

A Core without these fields and capabilities keeps the Desktop 0.3.0
presentation: `Reconnecting… (n/3)` from `ReconnectAttempt`/`ReconnectLimit`, no
extra line, no Retry now and no tooltip on the Disconnect button.

## Acceptance criteria

- Model tests (`tests/ReconnectPolicyTest.h`) cover the mapping, invalid input,
  every state in English and Russian, the countdown format and ticking, button
  flags, the capability gate, the notification sequence and the presence of
  every Russian translation.
- Window tests (`ConnectionManagementTest::reconnectPolicyView`) use a private
  IPC endpoint that speaks the contract. They cover:
  - the waiting view and Retry now sending one guarded `reconnect_now`;
  - Retry now in a held failure, a `reconnect_not_pending` reply and the
    restarted initial attempts;
  - the held-failure button stopping without a profile switch and Connect after
    release;
  - the unheld failure and the unchanged view after a restart into an old Core;
  - leftover protection and another `_run_recover` code routing to recovery.
- `developer-mode-test` passes on Windows x64; CI runs the full suite on
  Windows x64 and Linux and builds all four targets.
- Before release: check the states visually with a Core build of the contract
  on the Windows test VM.

## Visual check (2026-11-18)

This record predates #76: the button it names Disconnect VPN now reads
Disconnect (`Отключить`); its tooltip is unchanged.

Desktop ef9e880 against Core 874c1cc2 (the reviewed contract) on the Windows 10
test VM, in Russian, driven through UI Automation:

- Initial failure: `Подключение… (1/3)` to `(3/3)`, then the plain failure with
  Connect.
- Lost session (outbound path blocked): `Переподключение… (2/10)` to `(10/10)`
  with Disconnect VPN, then the red pause counting down from `0:58`, with the
  catalog cause and Retry now. Retry now started cycle 2; removing the block
  reconnected.
- Kill switch profile: the paused line while attempting, the held-failure
  text with Retry now and Disconnect VPN. Retry now restarted the three initial
  retries with protection held; Disconnect VPN released every filter.
- Residual protection after the service process was killed while held: the
  leftover-protection text, the recovery button and a blocked Connect, without
  held-protection controls.
- The previous Core with this Desktop: `Переподключение… (2/3)` without the new
  controls.

The VM service, its executable hash and its WFP state were restored and
rechecked afterwards. The tray recovery message and the Disconnect VPN tooltip
are covered by tests only: UI Automation does not expose them.

Benchmarks and profiling are not applicable: the change adds a one-second label
refresh during the pause only.
