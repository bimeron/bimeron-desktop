# Control server identity

Tracking: issue #59. Core contract: Core `docs/CONTROL.MD`, section "Server
identity" (bimeron-core #159).

## Problem

Desktop wrote control requests, including profiles, as soon as a local endpoint
accepted the connection. The Windows service pipe was not authenticated at all,
and the console daemon was checked by process ID only. A local process that
created the endpoint first would receive those requests.

## Behavior

Desktop verifies the server of every Core control endpoint before it writes the
first byte. A failed check closes the connection without writing, reports
`control_server_untrusted` and never falls back to another endpoint. The checks
use only OS-provided identity. Response fields are never proof of identity.

`CoreClient` derives the policy from the endpoint name:

| Endpoint | Policy |
|---|---|
| `Bimeron.Core.Service` (Windows) | Service pipe |
| `Bimeron.Core.<32 hex>` (Windows) | Console daemon pipe |
| `/run/bimeron/control.sock` (Linux) | Unix control socket |
| Any other name | Rejected without connecting |

Only the `developer-mode-test` target defines `BIMERON_CONTROL_FIXTURE_ENDPOINTS`.
There, other names are private test fixtures bound only by the optional expected
process ID, as before. Production builds have no such switch.

### Windows pipes

Desktop opens the pipe itself with `GENERIC_READ | GENERIC_WRITE` (which
includes `READ_CONTROL`) and identification-only SQOS
(`SECURITY_SQOS_PRESENT | SECURITY_IDENTIFICATION`). The server can never
impersonate the GUI user. Only a verified handle is passed to `QLocalSocket`.
A busy pipe is retried within the request deadline, at most 5 seconds.

Service pipe:

1. The owner from `GetSecurityInfo(handle, SE_KERNEL_OBJECT,
   OWNER_SECURITY_INFORMATION)` is exactly `S-1-5-18`.
2. `GetNamedPipeServerProcessId` succeeds.
3. When `OpenSCManager(SC_MANAGER_CONNECT)`, `OpenService("BimeronCore",
   SERVICE_QUERY_STATUS)` and `QueryServiceStatusEx` report a nonzero process ID,
   the server process ID equals it. An unavailable SCM answer skips this step,
   because step 1 already limits the server to SYSTEM.
4. A nonzero expected process ID from the caller must also match.

Desktop never opens the service process token: a standard user cannot open a
LocalSystem token.

Console daemon pipe (developer mode only):

1. The owner is exactly `S-1-5-32-544`.
2. Desktop opens the server process with `PROCESS_QUERY_LIMITED_INFORMATION`
   and its token with `TOKEN_QUERY`. The token user equals the GUI user SID and
   `TokenElevation` is true.
3. When Desktop launched the daemon, the server process ID equals the process ID
   of the retained launch handle.

Desktop never selects the console daemon automatically after a service pipe
failure. It uses that pipe only when the user explicitly selected a local Core
in developer mode, after Desktop stopped the system service or found it absent,
and launched the daemon itself.

### Linux socket

Before connecting, `lstat` checks:

- `/run/bimeron` is a directory, not a symlink, owned by UID 0, with no group or
  other write permission;
- `/run/bimeron/control.sock` is a socket.

A missing directory or socket is an ordinary unavailable Core, not an identity
failure. After connecting, `SO_PEERCRED` must report UID 0. A nonzero expected
process ID (the developer daemon or a live systemd `MainPID`) must also match.
Peer credentials are authoritative; the path checks are defence in depth.

### Native IKEv2 foreground pipe

`NativeIKEv2Transport` already performs the Core `ForegroundClient` checks before
each write: identification-only SQOS, a retained `SYNCHRONIZE` process handle,
an elevated token of the same SID and session, an image under Program Files with
pinned protected ancestors, a SHA-256 (or pinned volume and file index) match,
and a running process after inspection. The response must carry the sent
installation, the `ikev2_foreground` or `ikev2_profile` endpoint and the observed
owner identity. There is no fallback to the service or console daemon. No change
was required; failures keep the existing `IdentityRejected` handling.

### Repair session

Windows: the repair pipe server process ID equals the launched broker process ID
(or an authenticated child of it), and the retained broker handle shows the
process still running. Linux: the runtime directory `/run/bimeron-repair` and
the socket pass the same `lstat` checks, the broker process is still running, and
`SO_PEERCRED` reports UID 0 and the broker PID. A missing socket is retried as
before; any other failure stops the session with the existing broker identity
message.

## Presentation

An identity failure is `CoreClient::Failure::Untrusted`; it replaces the former
`PeerMismatch` value. The status line shows "The service needs to be updated or
repaired", the log records the code and the observed mismatch, and the Bimeron
service window offers Restore. Desktop does not start or install the service
automatically for this failure.

A service built before Core #159 leaves the service pipe owned by
`S-1-5-32-544` and is rejected. This is the expected route for an outdated
service: the installer service update and the repair broker both work with the
old service, because they use SCM and the PID-bound readiness pipe instead of
the control pipe.

## Acceptance criteria

- Unit tests (`control-identity-test`) cover endpoint classification and every
  service, console daemon and Unix decision, including the old-service owner.
- `developer-mode-test` shows that a fixture pipe or socket under the service
  policy receives no bytes, and that the failure is `Untrusted`.
- VM checks on Windows 10 STD, as a standard user and as an administrator:
  actual pipe owner (old service `S-1-5-32-544`, #159 service `S-1-5-18`),
  `READ_CONTROL` on the client handle, `QueryServiceStatusEx` access, rejection
  of a squatted pipe with nothing written, and the old-service route to repair.
- Console daemon checks with an elevated administrator.
- Linux VM checks of the socket directory, socket type and peer credentials.
