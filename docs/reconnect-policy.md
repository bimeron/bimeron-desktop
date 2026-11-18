# Reconnect policy presentation

Specification for Desktop 0.3.1; issue [Desktop #60](https://github.com/bimeron/bimeron-desktop/issues/60).
It extends [bounded reconnect recovery](reconnect-attempts.md), which stays the
behaviour with a Core that does not report the new policy.

The Core contract is [bimeron-core #162](https://github.com/bimeron/bimeron-core/issues/162)
(pull request [#163](https://github.com/bimeron/bimeron-core/pull/163)):
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
| Initial connect, attempt n of 3 | `Connecting… (n/3)` | yellow | held line | Disconnect (Disconnect VPN while held) |
| Interrupted, attempt n of 10 | `Reconnecting… (n/10)` | yellow | held line | Disconnect VPN |
| Interrupted, pause | `Server unavailable · retrying in 0:59` | red | cause, held line | Retry now (if advertised), Disconnect VPN |
| Initial-connect failure, protection held | `Couldn't connect. Check your internet connection and access key. Internet access stays paused until you select Disconnect VPN.` | red | none | Retry now (if advertised), Disconnect VPN |
| Failure without held protection | `Connection failed` | red | none | Connect |

- The countdown is the time until Core's next attempt, as `m:ss`, rounded up
  and refreshed every second. Without a valid time the pause reads
  `Server unavailable`.
- The held line `Internet access is paused until the VPN connects.` appears
  only while Core reports that protection is held.
- The cause is the localized catalog text of `LastError`. It is shown during the
  pause, so an error a retry cannot fix stays visible. The status tooltip keeps
  the catalog code and text as in Desktop 0.3.0.
- Disconnect VPN is the main connection button. Its tooltip reads
  `Internet will work without VPN protection.` A proxy-mode session keeps the
  plain Disconnect label unless protection is held.
- While protection is held, the main button stays Disconnect VPN even when
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
  handover to another kill switch profile, but Desktop offers only Disconnect VPN
  and Retry now in that state. A `kill_switch_held` start error is shown through
  the catalog like other start errors.

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
extra line, no Retry now and the plain Disconnect button.

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
  - the unheld failure and the unchanged view after a restart into an old Core.
- `developer-mode-test` passes on Windows x64; CI runs the full suite on
  Windows x64 and Linux and builds all four targets.
- Before release: check the states visually with a Core build of the contract
  on the Windows test VM.

Benchmarks and profiling are not applicable: the change adds a one-second label
refresh during the pause only.
