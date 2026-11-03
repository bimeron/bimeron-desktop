# Connection repair and service actions

Profiles and Settings open the same native Bimeron service dialog. It presents a
specific observed cause and short explanation. The collapsed footer contains
Details, Close and Restore. Findings, bounded technical evidence and report export
appear only inside Details; there are no manual-instruction or download primary
actions. The window is fixed, content-sized and uses the application theme.

## One repair session

Restore launches the protected separately installed repair broker once with a
fresh nonce and the GUI process ID. The GUI authenticates the endpoint against
the elevated process ID before sending a bounded request. The broker authenticates
the requesting OS process and user. The request contains only protocol version and
explicit disruption/network-recovery consent; it is not an arbitrary command or
path execution interface. Linux uses protected /usr/bin/pkexec directly, not a
PATH-selected executable or the WSL root-launch shortcut.

If fresh privileged inspection discovers a need for additional consent, the same
authenticated socket may request disruption or network-recovery confirmation once
each. The GUI presents the explicit warning asynchronously and replies with the
matching consent identifier and approval boolean. Denial is terminal. No second
elevation is launched; the broker bounds each consent wait to sixty seconds within
its overall repair deadline. Stale dialog responses cannot affect another attempt.

The broker owns inspection, evidence-supported changes, verification and rollback
inside that one elevated process. Measured stage events update the indeterminate
progress indicator. Only allowlisted stage/error identifiers are displayed; raw
privileged stderr is not exposed. The GUI accepts at most 32 events per attempt
and retains at most 64 history entries. Successful terminal result and process
exit are both required, followed by compatible hello and fresh status. A helper
exit alone never establishes readiness. Repair does not connect a profile.

The Journal records repair stages and failures, helper completion pending readiness,
and the final verified result. It also records observed service-state changes,
including distinct pending states, without repeating unchanged polling results.
Expanded diagnostic details and the repair report retain bounded CLI output and
Core free text even when credentials are present, as explicitly requested by the
user. No generic credentials-hidden placeholder is displayed. Plain-text rendering
and escaped log entries prevent diagnostic content from becoming markup. The
compact dialog previews diagnostics; the report retains their full bounded text.

Close hides the dialog without canceling the operation. Reopening retains its
state; watchdog expiry does not release the process handle or permit another
attempt. Canceled OS consent is terminal for that attempt. Socket retry callbacks
are bound to the attempt nonce and cannot attach to a later attempt. A failed
repair remains an explicit failure with Restore for a new guarded attempt.

## Safety and current capability limits

The GUI never elevates an adjacent writable executable. Broker discovery is
independent of the GUI location and requires a protected machine installation.
The repair assessment recognizes that installed broker even when the GUI runs
from a development directory. The service header uses the success palette and
capitalized Running text for an observed running service; IPC readiness remains
a separate check.
If that broker is absent, repair reports the missing protected component without
trying to execute a missing file. Secure automatic bootstrap when no protected
broker remains is not implemented; the current UI does not claim that this case
has been repaired. The package's old pinned Core is not proof of broker support.

Unknown ownership, an occupied lock, and a recovery journal are not permission to
kill a process or reset networking. A journal records intent, not network damage.
Disruptive service repair and network recovery require explicit consent and fresh
privileged evidence. Interrupted development reconciliation and compatible signed
component replacement remain dependent on the implemented broker capabilities;
unsupported cases return a specific failure rather than instructions or success.
No caller-selected digest is treated as a release signature.

The legacy guarded service controls remain separate implementation facilities;
the repair dialog does not expose them or chain their per-command elevations.
Signed service updates retain their existing signature and responding-service
prerequisites outside this repair session.

## Ordinary startup and validation

Ordinary GUI startup retains one guarded attempt to start an installed stopped
system service. Development and visual/test launches do not auto-start services.
Missing/ambiguous status, unresolved recovery and local conflicts block automatic
start. A manual stop in this session is respected. Initial preparation preserves
configured startup auto-connect eligibility; explicit repair consumes it.

IPC failures do not cancel independent service probes. Unhealthy service polling
is bounded and rate-limited. Backend changes and mutation generations reject
obsolete callbacks. Findings distinguish observed facts from unknown results: a
running service is not proof of readiness.

Isolated fixtures exercise one broker launch, repeated clicks, close/reopen,
terminal failure and fresh readiness verification. The native Windows private
pipe fixture rejects an impostor before sending any request. These tests do not
mutate host services or networking. Native EN/RU light/dark captures at DPR 1 and
1.5 check containment, fixed bounds, expanded findings and progress. Compilation
and fixtures do not establish actual UAC/polkit, elevated cleanup, missing-broker
bootstrap, or ARM64 runtime acceptance. No package, commit or release is implied.
