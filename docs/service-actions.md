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

## Linux elevation diagnostics acceptance (2026-11-15)

A GUI started through SSH/root runuser retains the remote root login session,
 even when its effective UID, DISPLAY and session-bus environment match the desktop.
The local GNOME polkit agent belongs to the graphical login session. Such a remote
launch is not native polkit acceptance: pkexec can exit 127 while attempting to
create a textual agent without a controlling terminal. Launch the installed GUI
from the local desktop/terminal for authorization testing; never bypass polkit.

Before implementation: retain bounded elevation-process diagnostics in Details
and the repair report on terminal failure, using the existing diagnostic policy.
The short failure remains explicit; untrusted stderr must not become a broker
stage, consent, successful result or readiness evidence. Acceptance covers exit
127 diagnostics, cancellation (126), a fresh retry, and unchanged requirement for
successful broker result plus exit plus hello/status. No Core/pin change is needed
for the remote-session launch defect. Existing historical probe evidence remains
historical, not a fresh missing-service finding.

Verification: the original installed GUI (DEB SHA-256
6dc42da2d9d3fd59ca4427a7adc10e708f973ac38868b96853ad7298ee596ac1)
was relaunched from the local GNOME terminal. Native journal records startup
consent dismissal, repair consent dismissal at 09:45:02, then one authorized
repair broker execution at 09:45:06. The GUI records inspection, service start,
readiness verification and final ready state; systemd reports active and the GUI
shows Disconnected. Both Core and broker retain executable digest 5bebf5e3.
No root CLI repair substituted for this test. The installed GUI was not replaced;
the diagnostic-only source candidate was built and tested separately at
/opt/bimeron-linux-repair from HEAD 353d31e plus hash-verified source overlays.

Ubuntu 22.04 Qt 6.2.4 candidate CTest passed 9/9 (18.46 s), including the new
exit-127/cancellation fixtures and existing broker/readiness/consent regressions.
This GUI-only configuration omits repair-resource-test; embedded Core bytes and
pins are unchanged. A full-disk environmental failure also reproduces in the
parent's old GeoData test: only 53 MiB free for a >64 MiB fixture. Final tests used
an isolated private mount namespace mapping only root's test data directory to
/run/bimeron-repair-tests (RAM); no global mount or existing data was changed.
Only this task's compiler object files were removed to recover space; logs remain.
Windows x64 compiled; the focused developer fixture passed with the documented
offscreen font environment. ARM64 rebuilds/packages and WSL repeats are not part
of this diagnostic-only validation; this is not release acceptance. Performance
benchmarking/profiling is inapplicable. Diff/security review found no changed
trust, consent, identity, cleanup or readiness boundary. Evidence and captures
are in ignored build/linux-repair and VM /tmp/bimeron-repair-investigation.
Any release including the diagnostic improvement needs Desktop rebuild/repack;
no Core re-freeze or pin change is required by this cause. Other release gates
remain outstanding.

Final Windows x64 CTest passed 9/9 (23.10 s) with QT_QPA_PLATFORM=offscreen,
QT_QPA_PLATFORM_PLUGIN_PATH pointing to Qt 6.8.3 platforms, and
QT_QPA_FONTDIR=C:/Windows/Fonts. The initial missing-font run failed an unrelated
GeoData label fixture; the corrected environment passed the focused fixture and
full suite. No product change was made for that environment issue.
