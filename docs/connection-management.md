# Desktop connection management

## Contract

The active protected Core endpoint is authoritative. Startup performs IPC 1 hello
and status before enabling controls. An unavailable endpoint is not disconnected;
an unknown profile is not the selected profile. Runtime Core identity and the
selected validation CLI identity are separate. Missing identity is unknown.

All connection entry points share one action path: button, profile double-click,
Return/Enter and tray. Activating the active profile disconnects it; activating
another profile stops the old session, waits for successful cleanup and starts
the selected profile once. Selection alone never changes the connection.

Validate asynchronously with the bundled CLI in System mode or the selected CLI
in Development mode. Send only the absolute profile path to start. An accepted
start is not connected. After an ambiguous timeout, inspect status without
automatically repeating start. Stop timeout cancels pending profile switching;
the user can explicitly retry stop. Cleanup errors prohibit automatic start and
recover. Emergency recovery remains a separately confirmed administrative action.

Observe connecting, connected, reconnecting, stopping and failed from Core.
Reconnection belongs to Core. Check instance on every response, including errors;
on restart reset hello/capabilities, event cursor, session and speed baselines and
cancel old intentions. Dropped events or invalid cursors require status resync.
Unknown or malformed state never means disconnected. Unknown diagnostics remain
redacted according to the profile-preservation contract.

## Scheduling and lifecycle

Keep one IPC request in flight. Prioritize user actions and resynchronization;
poll events approximately every second, status every two seconds and stats every
second while connected. Coalesce overdue work. Ordinary requests time out after
five seconds, stop/shutdown after two minutes. Retry read-only hello after IPC
failure at 1, 2, 5 and then 10 seconds; never install/start a service or switch
endpoints as an IPC error fallback. Permission and compatibility failures have
distinct messages. CLI validation and service-status probes must not block UI.

Compute payload speed using monotonic elapsed time between snapshots. Reset on
instance/session change, counter decrease or IPC loss. The first sample has no
rate. Missing statistics and stale speed are unavailable, not zero.

Adopt an already active startup session. Auto-connect is opt-in, once after the
initial synchronized disconnected state; never after IPC failure, instance or
backend change, cleanup failure or an administrative operation. DevelopmentCore
continues to own local process cleanup, VPN-lock verification and restoration of
the previous service availability. Backend transitions suspend ordinary control
and invalidate asynchronous results. They never reconnect a profile automatically.

Signed service updates retain their existing trust gate and explicit consent.
Restore the previous known profile once only after successful helper completion,
expected-version hello and clean disconnected status. Failure, cancellation,
rollback, missing profile or mismatched version never reconnects.

Closing to tray does not stop Core. Explicit Quit leaves the system service and
session running by default. The existing stop-Core-on-exit preference is explicit
and defaults false. Owned Development daemon cleanup keeps its separate policy.
Do not use shutdown or recover for ordinary disconnect.

## Boundaries and acceptance

SSH profiles and transient authentication are specified in [SSH profiles](ssh-profiles.md).
Native IKEv2 foreground backend is not added. Preserved opaque profiles are not a support claim;
native IKEv2 is not started through packet controls. Bundle pins, signed manifests,
dependencies, packaging and release policy are unchanged.

Isolated fake IPC/CLI checks cover startup adoption, all lifecycle states,
accepted/rejected/timed-out start, delayed/failed stop, profile switching, repeated
actions, removed targets, instance changes in all responses, dropped events,
invalid cursor, access/version/correlation/size failures, stale replies and CLI
results, speed resets and secret sentinels. They also check backend/update gates,
auto-connect and exit policy without using the installed service or system network.
Keep navigation and profile-preservation regression coverage.

Run focused tests followed by existing CTest and the four-platform CI build
matrix. Windows ARM64 CI is compile-only. Check English/Russian UI at 100/150%
scaling. Check request bounds and UI heartbeat under delayed responses; profile
CPU/allocations if a polling regression is observed. Real service, ACL/elevation,
update and network checks require a separately authorized disposable environment;
fake tests and compilation are not runtime interoperability evidence.

## Verification (October 29, 2026)

Windows amd64 and Linux amd64 passed `developer-mode-test`, `core-update-test`
and `profile-vless-test`. Windows ARM64 and Linux arm64 compiled; no local ARM64
runtime result is claimed. GUI-only configurations used the existing supported
presets without downloading or changing the bundled Core. No packages were made.

The connection fixtures verified a responsive UI heartbeat during delayed stop
and validation, serialized requests and bounded polling under frequent timer ticks.
No polling or UI-responsiveness regression was observed that warranted a separate
profiler or benchmark run. English/Russian fixture renders at 720x600 logical pixels were
inspected at 100% and 150%; status and connection controls fit. The test executable
does not include the application's resource icons or AppStyle, so these renders
do not replace installed-package appearance checks.

Diff review covered ambiguous command completion, stale callbacks, cleanup gates,
startup-profile identity, update restoration, secret handling and scope boundaries.
Live VPN/proxy, service-update, UAC/polkit and multi-user ACL checks were not run.
They remain separately authorized disposable-environment checks; this change
does not establish new protocol interoperability or authorize a release.
