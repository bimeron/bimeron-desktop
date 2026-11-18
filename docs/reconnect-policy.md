# Reconnect policy presentation

Specification for Desktop 0.3.1; issue [Desktop #60](https://github.com/bimeron/bimeron-desktop/issues/60).
It extends [bounded reconnect recovery](reconnect-attempts.md), which stays the
behaviour with a Core that does not report the new policy.

Core owns reconnection. Desktop only presents what Core reports and sends
explicit user commands; it never schedules attempts, counts them or releases
protection on its own. Routing, GeoData rules and the per-profile kill switch
setting (off by default) are unchanged.

## Core policy being presented

- Initial connect: up to three retries.
- After an established session is interrupted: endless cycles of ten attempts,
  separated by a 60-second pause.
- Errors a retry cannot fix (for example, a rejected key) follow the same cycle.
- With the kill switch on, Core keeps protection between attempts, during pauses
  and after an initial-connect failure until the user disconnects.

## States and texts

English is the source text; Russian is in `assets/translations/ru.json`.

| Core report | Status line | Colour | Extra line | Controls |
|---|---|---|---|---|
| Initial connect, attempt n of 3 | `Connecting… (n/3)` | yellow | held line | Disconnect (Disconnect VPN while held) |
| Interrupted, attempt n of 10 | `Reconnecting… (n/10)` | yellow | held line | Disconnect VPN |
| Interrupted, pause | `Server unavailable · retrying in 0:59` | red | cause, held line | Retry now (if advertised), Disconnect VPN |
| Initial-connect failure, protection held | `Couldn't connect. Check your internet connection and access key. Internet access stays paused until you select Disconnect VPN.` | red | none | Disconnect VPN |
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
- Retry now is next to the status line. It appears only during the pause and
  only when Core advertises the capability. It sends the retry command once and
  then refreshes the status.

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
that reads IPC field names. Until the Core contract is final, it uses these
provisional additive IPC 1 names:

| Meaning | Provisional name |
|---|---|
| Cycle after an interruption, from 1 | status `ReconnectCycle` |
| Phase | status `ReconnectPhase`: `attempting` or `waiting` |
| Next attempt time | status `NextAttemptAt`, RFC 3339 |
| Protection held | status `ProtectionHeld`, boolean |
| Retry now | capability `retry_now`, boolean; command `retry_now` |

A status reports the new policy when it has the phase, the held marker or the
cycle. A cycle means an interruption; otherwise it is the initial connect.
Invalid numbers and times are ignored as in Desktop 0.3.0.

A Core without these fields keeps the Desktop 0.3.0 presentation:
`Reconnecting… (n/3)` from `ReconnectAttempt`/`ReconnectLimit`, no extra line,
no Retry now and the plain Disconnect button.

## Acceptance criteria

- Model tests (`tests/ReconnectPolicyTest.h`) cover the mapping, invalid input,
  every state in English and Russian, the countdown format and ticking, button
  flags, the capability gate, the notification sequence and the presence of
  every Russian translation.
- Window tests (`ConnectionManagementTest::reconnectPolicyView`) use a private
  IPC endpoint. They cover the waiting view, Retry now sending one command, the
  held-failure button stopping without a profile switch, Connect after release,
  the unheld failure and the unchanged old-Core view.
- `developer-mode-test` passes on Windows x64; CI runs the full suite on
  Windows x64 and Linux and builds all four targets.
- After the final contract: update `fromIpc()` and its fixtures, then check the
  states visually with a Core build on the Windows test VM.

Benchmarks and profiling are not applicable: the change adds a one-second label
refresh during the pause only.
