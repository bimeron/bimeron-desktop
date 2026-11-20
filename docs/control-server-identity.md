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

The pipe owner applies to the whole pipe, and `GetNamedPipeServerProcessId`
reports the process that created the pipe, also for a later instance that
another process serves. Neither identifies the server of one instance, so the
client checks below cannot detect an extra instance. Core prevents extra
instances with protected pipe DACLs: clients get `0x0012019B`
(`FILE_GENERIC_READ | FILE_GENERIC_WRITE` without `FILE_CREATE_PIPE_INSTANCE`,
`WRITE_DAC` or `WRITE_OWNER`), and only the server principal can create
instances. The client checks bind the endpoint to the right principal and
service; the process ID cross-check is an extra layer.

Service pipe:

1. The owner from `GetSecurityInfo(handle, SE_KERNEL_OBJECT,
   OWNER_SECURITY_INFORMATION)` is exactly `S-1-5-18`.
2. `GetNamedPipeServerProcessId` succeeds.
3. After the pipe is open, `OpenSCManager(SC_MANAGER_CONNECT)`,
   `OpenService("BimeronCore", SERVICE_QUERY_STATUS)` and `QueryServiceStatusEx`
   must succeed, report `SERVICE_RUNNING` and a nonzero process ID, and that ID
   must equal the pipe server process ID. Any other answer is untrusted: a failed
   or denied query, a service that is not installed, process ID 0, or a stopped,
   start-pending, stop-pending or paused service. A pipe can outlive its service:
   an extra instance held by another process keeps the pipe and its SYSTEM owner
   after the service stops. This was possible with services built before
   Core #159, whose DACL let the owner user create instances.
4. A nonzero expected process ID from the caller must also match.

Desktop never opens the service process token: a standard user cannot open a
LocalSystem token.

Console daemon pipe (developer mode only):

1. The owner is exactly `S-1-5-32-544`.
2. Desktop opens the server process once with
   `PROCESS_QUERY_LIMITED_INFORMATION | SYNCHRONIZE` and its token through that
   handle with `TOKEN_QUERY`. The token user equals the GUI user SID and
   `TokenElevation` is true, and the process is still running after the
   inspection. Desktop keeps the handle until the connection is closed, so the
   process ID cannot be reused in between.
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

Both identity failures below report `control_server_untrusted` and write
nothing. The log records `Core: <catalog text> (control_server_untrusted)`,
followed by the observed mismatch. The catalog text comes from the bundled
Core (`bimeron errors --json`): "Could not verify the background VPN service.
Update Bimeron Core to match this version of the application, then try again."
Without a catalog, Desktop uses the same English text and the catalog's Russian
translation.

- `CoreClient::Failure::Untrusted` replaces the former `PeerMismatch` value. The
  status line shows "The service needs to be updated or repaired", the Bimeron
  service window shows the catalog text and offers Restore. Desktop does not
  start or install the service automatically for this failure.
- `CoreClient::Failure::ServiceNotRunning` is a service pipe for which SCM
  reports no running `BimeronCore` process. Desktop presents it like an absent
  service pipe: the existing service-state guidance ("Connection service is
  stopped", "not installed", "changing state") and Restore, and startup keeps
  waiting for the service. Core creates the pipe while the service is
  start-pending and reports running immediately afterwards, so a valid service
  can briefly fall into this case during start; the next attempt succeeds.

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
- VM checks on the Windows test VM, as a standard user and as an administrator:
  actual pipe owner (old service `S-1-5-32-544`, #159 service `S-1-5-18`),
  `READ_CONTROL` on the client handle, `QueryServiceStatusEx` access, rejection
  of a squatted pipe with nothing written, and the old-service route to repair.
- With a #159 service, a standard user who owns the service cannot create an
  extra instance of its pipe (access denied).
- A service pipe that outlives its stopped service is rejected with nothing
  written: against an old service, the standard user holds the extra instance;
  against a #159 service, only SYSTEM can.
- Console daemon checks with an elevated administrator.
- Linux VM checks of the socket directory, socket type and peer credentials.

## Verification (2026-11-18)

Builds: Windows amd64 Release (MSVC, Qt 6.8.3) on the host, compile only;
Linux amd64 Release (Ubuntu 22.04, Qt 6.2) on the Linux VM. Tests and probes
ran only on the VMs. The Core #159 test binary was built from bimeron-core
`d08b2afe` with version `0.4.0-beta.2.identity159`.

Windows test VM. The installed Desktop 0.3.0 GUI and its Core `5fcadd71`
service stayed in place; the service binary, `ImagePath` and state were
restored afterwards (SHA-256 `14c88367...acb1e`).

- `control-identity-test`, `core-identity-test`, `installer-core-test`,
  `profile-vless-test` and `developer-mode-test --visual-check` (offscreen,
  elevated administrator) passed. `developer-mode-test` reads `ru.json` from its
  compile-time source path, so a copy was placed at that path on the VM.
- Old service, elevated administrator (service owner): pipe owner
  `S-1-5-32-544`, rejected as `Untrusted` with the "needs to be updated or
  repaired" diagnostic.
- Old service, standard user `bimstd` (interactive logon, Medium integrity,
  temporarily set as the service owner): `READ_CONTROL` read the owner
  `S-1-5-32-544`, `QueryServiceStatusEx` returned the PID, and the service was
  rejected. A standard user that is not the service owner gets
  `AccessDenied` from the pipe DACL, as before.
- Update route: the installed 0.4.0-beta.1 helper ran
  `service update -source <#159 build>` against the old service and succeeded.
  The updated service replied to both the administrator owner and the
  standard-user owner (pipe owner `S-1-5-18`, SCM PID matched).
- Squatted service pipe (service stopped): a `bimstd` squatter and an
  administrator squatter (owner `S-1-5-21-...-1001`) were each rejected by a
  probe running as the other user. The squatters' per-client accounting showed
  0 bytes from the probe, while the installed Desktop 0.3.0 GUI sent its
  72–144 byte requests to them: this is the exposure the change removes.
- Console daemon (elevated administrator): an existing elevated daemon of the
  same user replied with and without its launched PID, and was rejected with a
  wrong launched PID.

Linux VM (Ubuntu 22.04, installed Desktop 0.3.0 and Core 0.4.0-beta.1 service):

- CTest: 10 of 11 passed, including `control-identity-test`. In
  `developer-mode-test`, the new squatted-socket check passed, but a later
  fixture fails with "save isolated native selector fixture". The unchanged
  base commit `3f9ad05` fails the same way on this VM, so this is a pre-existing
  environment issue.
- Real socket (`/run/bimeron` root `0711`, socket, root listener): replied.
- `/run/bimeron` mode `0731`: rejected before connecting.
- Service stopped, `test` squatter: a symlinked runtime directory and a
  world-writable root directory were rejected before connecting. A root `0711`
  directory with a socket from a UID 1000 listener was rejected after connecting
  by `SO_PEERCRED`, with 0 bytes from the probe. The installed Desktop 0.3.0 GUI
  sent 72 bytes to each squatter.

Not covered: a full GUI-driven Restore of an old service. It needs a Desktop
package whose repair broker carries a #159 Core, and that Core is not frozen
yet. Windows ARM64 and Linux arm64 were not built or run; CI covers their
compilation.

### Revised contract (2026-11-18, Core #159 `aad59396`)

The Core test binary was rebuilt from `aad59396` (tightened DACLs) with version
`0.4.0-beta.2.identity159b`. The window started from Automatic/Running on the
original service file and ended in exactly that state: original `ImagePath`,
start type, running state, file hash and directory listing, and no Bimeron WFP
filters. The probes use `developer-mode-test --control-probe`; extra instances
use `--control-hold`, which reports the bytes each client process wrote.

- Old service, administrator owner: `Untrusted`, owner `S-1-5-32-544`.
- Old service, `bimstd` as owner: `bimstd` held an extra instance and the
  service was stopped; the pipe stayed. The `bimstd` probe was `Untrusted`, and
  the holder received 0 bytes from it.
- The installed 0.4.0-beta.1 helper updated the old service to the #159 build
  with `service update -source`; the administrator probe was trusted.
- #159 service, `bimstd` as owner: trusted. `bimstd` could not create an extra
  instance of the pipe (`CreateNamedPipe` error 5, access denied).
- #159 service with an extra instance held by SYSTEM (the only principal the new
  DACL allows), then stopped: the pipe stayed, owned by `S-1-5-18`. The `bimstd`
  probe was `ServiceNotRunning` with `control_server_untrusted` ("SCM reports
  BimeronCore state 1, not running"), and the holder received 0 bytes from it.

The console daemon handle retention and the catalog guidance were added after
this window. They are covered by `control-identity-test` and
`developer-mode-test` in CI and still need a VM run.
