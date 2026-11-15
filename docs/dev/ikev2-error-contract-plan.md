# IKEv2 safe error transport proposal

Status: specification only, 2026-11-05. Implementation requires separate approval
of this contract by Core and Desktop owners. No callable API, wire implementation,
production composition, capability enablement, approval pin or build flow changes
are authorized. Native remains OFF. Windows 775 remains unresolved; supplied 668
normal-cleanup failure evidence does not establish successful IKEv2 operation.

## Source baseline and existing behavior

Desktop baseline is `dbcf2dc` on `plan/ikev2-error-contract`, initially clean.
T3 confirmed this checkout/branch and `codex_petr/gpt-6.1-sol`, MEDIUM. Core
inspection used only Git objects at local committed auth revision
`8ef0110326c86c1621a0c274dfb06a30b123324b` (parent
`498596eabe4ec734cb0bda72059dd74a66c9dc49`) via the separate
`feat-ikev2-auth-contract` checkout. This revision is source evidence, not an
approved runtime/artifact pair; its disabled auth seam supplies no callable wire
auth or native approval. The active installation-contract checkout was not read.

- Core `internal/systemvpn/ras_windows.go`: entry inspection, dial, status and
  hangup receive Windows numeric results; `status` also returns the RAS status
  error number. No native calls were executed during inspection.
- Core `internal/systemvpn/session.go`: `waitConnected` converts status errors
  through `rasError`. Its allowlist maps 691, 798/13801/13806, 623, 5 and 809 to
  symbolic errors, with special handling for 6. Other numbers, including 775 and
  668, collapse to `ikev2_native_operation_failed`. Failed startup invokes Close;
  a cleanup error replaces the startup error. Close reports cleanup failed or
  timeout and does not preserve the underlying number in its public error.
- Core `core/ikev2_windows.go`: Open wraps most failures with `fault(err.Error(),
  false)`. `fault` does not populate `stageError.stage`. Cleanup/protection failures
  can replace earlier errors. `core/native_observed.go` separately reports RAS
  cleanup pending/succeeded/failed; this is lifecycle evidence, not a diagnosis.
- Core `core/engine.go`: transitions copy `err.Error()` to Status.LastError and
  stageError.stage to ErrorStage; Event copies those values and Attempt. Attempt
  increments for Open/reconnect. Connected/disconnected clears error strings.
- Core `internal/control/foreground.go`: read-only hello/status/events return
  engine status; accepted second hello schedules Start once. Synchronous Start
  failure becomes `session_start_failed`. Stop returns `cleanup_pending` on its
  five-second timeout or `session_stop_failed`; it refreshes status. Installation,
  instance/session mismatch, cancelled activation, closing and nonterminal close
  have symbolic response errors. These errors differ from asynchronous engine
  failure. No foreground start or auth command is introduced here.
- Desktop `src/NativeIKEv2Transport.cpp`: `payload` validates then removes
  LastError/ErrorStage from status, and Message/LastError/ErrorStage from events.
  Event Code and response error pass a symbolic allowlist. `decodeResponse`
  binds version 1, request ID, installation, peer identity and instance; hello
  checks exact Core identity/capability shape. Unexpected fields are rejected.
- Desktop `src/NativeIKEv2Session.cpp`: `received` checks pending command and
  retained owner/installation/instance/session, copies lifecycle evidence and
  allowlists response error into Snapshot.safeError. It does not expose Attempt
  or numeric diagnostics. Invalid identity poisons the controller. IPC failure,
  cancellation and event loss remove authority. `clean()` independently checks
  native cleanup/protection evidence before close; process exit is not cleanup.
- `tests/NativeIKEv2TransportTest.h` already asserts diagnostic stripping and
  source-derived event fixtures. Session tests cover lifecycle/closure and
  identity failures. `tests/ConnectionManagementTest.h` injects safeError `775`
  privately into MainWindow; this is hypothetical and unreachable through the
  committed producer/transport. Reuse these suites rather than duplicate them.

## Proposed bounded record

Retain symbolic response error and lifecycle fields for their existing purposes.
Never parse LastError, ErrorStage, Message, formatted errors or OS diagnostic text
to reconstruct numeric evidence. Introduce a typed Core-internal carrier at the
numeric observation site before `rasError` destroys the number; propagate it
through explicit typed fields rather than string matching.

Proposed optional status member `native_failure` has exactly `version: 1` and
optional `operation` and `cleanup` records, at least one present. Each record has
exactly `domain`, `code`, `stage`, `attempt`; no message or extension bag.
Domain is the literal `windows_ras`. Code is an integer in 1..4294967295; zero
means no error and must be represented by absence. Reject strings, booleans,
fractions, negatives, null, overflow and nonfinite values. Attempt is an integer
in 1..2147483647 and must equal enclosing Status.Attempt. Bound the complete
member to 512 UTF-8 bytes and two records. No arrays/history or raw text survives.

Stage is one of `profile_inspection`, `dial`, `connection_status`,
`cleanup_hangup`, `cleanup_status`. Operation permits only the first three;
cleanup permits only the last two. These name local observation sites, not
authentication causes. Unknown stage/domain/version or additional fields are a
protocol error in the negotiated contract. Unknown valid numeric codes are
preserved and displayed as a generic Windows error number, with unknown cause.
Non-RAS failures continue using symbolic/generic errors without this member.

Retain the first operation failure and first cleanup failure separately for the
same attempt. Cleanup remains independently failed/unknown even when an operation
number is present. Clear current diagnostic records on a new attempt or confirmed
successful lifecycle transition; never carry them into a replacement attempt.
If cleanup fails after a successful connection, only cleanup is populated. A
non-numeric cleanup error retains lifecycle failure without inventing a number.
Capturing both failures requires a Core producer change: current strings cannot
recover discarded evidence. This is a dependency, not implemented behavior.

## Version compatibility and binding

Keep the legacy protocol and strict decoder unchanged. A future separately
reviewed foreground protocol version 2 must select this member and its exact
schema. A v2 client first performs read-only v1 discovery, verifies exact approved
Core identity, then uses v2 only when that reviewed pair explicitly supports it.
Define the support advertisement and dual-version Core behavior in that future
change; existing exact capability-count checks prohibit casually adding a flag.
Do not send an extension to old v1 clients. New clients consuming v1 show existing
generic errors; absence is no proof of success. Unsupported version/identity fails
closed, with no downgrade of a mutating request or automatic activation retry.
No revision pins or capability gates change in this planning task.

Only status carries the new record initially; events remain wake-up/history data
and trigger authoritative status refresh. This avoids a second diagnostic schema
and event replay authority. Event drops retain the existing unknown/status path.

Transport must retain existing authenticated peer, request ID, installation,
instance and session validation. A future Session representation must keep an
optional typed diagnostic separate from safeError and bind it to endpoint,
owner/installation/instance/session, local attempt generation, exchange generation
and enclosing Attempt. Before a new attempt is known, expose no numeric result.
After a higher Attempt is accepted, lower-attempt responses cannot restore an
error. Within an attempt, discard old exchange completions, duplicates and
out-of-order status. Cancellation/timeout, identity change, closure and controller
replacement invalidate current presentation authority. A retained historical
diagnostic must be explicitly stale and cannot be rendered as the current result.

The diagnostic grants no launch, retry, credential request, EAP prompt, Stop,
cleanup, owner-close or readmission authority. Existing clean/protection checks
remain independent and conservative. 775 may only say Windows error 775, local
cause unknown. It proves neither bad password, server rejection nor EAP UI need.
668 observed during cleanup must be labelled cleanup failure at its observed
stage, without turning an earlier connection state into a success claim.

## Offline acceptance for a future authorized implementation

1. Core fake-driver tests preserve 775 at dial/status and 668 at cleanup, both
   together and independently; startup rollback cannot erase the operation record.
   Test cancellation races, cleanup timeout/non-numeric failure, attempt reset,
   successful transition clearing and bounded generic fallback. No Windows calls.
2. Extend existing Transport serialization fixtures: valid 775/668, unknown valid
   number, absent record, v1 generic behavior and exact v2 shape. Reject wrong
   types/ranges, zero, oversized member, unknown fields/version/domain/stage,
   wrong slot/stage and mismatched Attempt. Raw diagnostic sentinel text must
   remain stripped from emitted JSON and never reach logs, widgets or settings.
3. Extend Session fake hooks: delayed old attempt/exchange, duplicate reply,
   cancellation, identity/session replacement, timeout, events dropped, recovery
   and close/controller loss cannot revive numeric presentation or authority.
   A malformed extension follows InvalidResponse/unknown behavior; it must not
   leave a previous numeric error authoritative. Cleanup failure still blocks
   close/readmission, and rendering sends no commands.
4. Extend the existing MainWindow fixture tests for typed authoritative 775,
   cleanup 668, both records, unknown code, absent/stale evidence, EN/RU text and
   redaction sentinels. Keep existing private 775 fixture explicitly hypothetical
   until a supported producer exists. Assert no password/server/EAP inference.
5. Test old/new version combinations using serialized fake responses only.
   Neither unsupported v2 nor absent evidence may replay activation. Native OFF,
   packet/GeoData behavior and all approval gates must remain unchanged.

## Proposed targets and handoff

Core owner: `internal/systemvpn/session.go`, `ras_windows.go`, their fake-driver
tests, `core/ikev2_windows.go`, `engine.go`, and foreground/control serialization
tests. Agree typed propagation and version negotiation before Desktop consumption;
keep disabled auth separate. Desktop follow-up: `src/NativeIKEv2Transport.{h,cpp}`,
`src/NativeIKEv2Session.{h,cpp}`, existing Transport/Session/ConnectionManagement
test headers, then `src/MainWindow.cpp` and translation assets only as required.
Update this plan with accepted schema/compatibility decisions before coding.

Suggested future commit title: `feat(ikev2): preserve typed foreground failure evidence`.
Planning commit proposal: `docs(ikev2): specify safe foreground error transport`.
No commits are made here. Build/packaging, dependencies, installation contracts,
approval production, lab recovery and runtime enablement are outside this change.

This documentation-only task uses source review and whitespace/content checks.
Compilation, runtime tests, benchmarks/profiling and all four release targets are
not applicable because no executable or build flow changes. Future implementation
needs focused offline suites and relevant CI checks; release validation remains
separate. No live diagnosis, successful IKEv2 or release readiness is established.
