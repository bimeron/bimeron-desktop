# IKEv2 lab handoff — 2026-11-01

Work is paused at the user's request after finishing the currently running design
analysis. Do not start another experiment automatically when resuming this thread.

## Repository state

Branch: `plan/stage-8-ikev2`, PR #16 (draft). Product and lab source changes through
`54e247e` are committed and pushed. Native GUI connection and provisioning remain
disabled. The proposed Core installation controller has not been implemented.
Reviewed finite models do not establish native authority, durability or closure.

The only pre-existing untracked source-tree files are two Python cache directories.
Generated review bundles, artifacts, test logs and native evidence remain ignored
under `build/`. Do not add them or credentials to Git.

## Latest native outcome

The most recent prepare-only attempt used the ignored
`build/boundary-xml-snapshot-review` bundle in a retained PowerShell module.
The corrected SYSTEM XML registered task
`\BimeronLab\CheckpointObservationBoundaryXml`, but preparation stopped at exact
task security-descriptor comparison, before Task.Run, host launch or dispatch.

Read-only native task observations:

- Principal resolves to SYSTEM; COM LogonType is 5.
- State is Ready (3); LastRunTime is the never-run sentinel, 1999-11-30.
- Task SDDL is `O:BAG:BAD:(A;;FA;;;SY)(A;;FA;;;BA)(A;;FR;;;SY)`.
- The folder SDDL is `O:BAG:BAD:PAI(A;;FA;;;SY)(A;;FA;;;BA)`.
- The expected task descriptor was the folder-style descriptor. The engine's
  missing protected/auto-inherited flags and additional SYSTEM read ACE have not
  been accepted as a replacement contract.

Do not run the task or dispatch the runner until the descriptor contract is
resolved, exact native XML/config/settings/principal/SDDL are frozen and reviewed,
and fresh admission gates pass. Do not repair the descriptor or silently adopt
native readback as its own approval.

Earlier resumed attempts exposed orchestration defects and stopped during
preparation. The first resumed attempt lost its result after a collector error;
its execution and terminal outcome remain Unknown. Later absence observations
do not establish historical nonexecution or MSI server quiescence. No resumed
attempt has supplied a successful checkpoint interruption result.

## Preserved live state

Parent PTY session 86808 remains open. Its retained variables hold earlier
PowerShell Direct sessions, jobs and native handles. The current isolated module
is `SnapshotPreparationModule`; it retains the latest preparation session and
owners. Do not reload source into that scope, reset owners, dispose handles,
terminate processes, restart the VM, remove tasks or delete/overwrite lab roots.
These references are live-process state, not durable recovery across application
exit or host reboot.

The Windows 10 STD VM contains old and partial client/observation roots plus the
new BoundaryXml roots. Preserve them. Native MSI/DLL/manifest hashes and protected
ACK behavior were unchanged by the observation-host changes. The task has not
been started, and the new observation runner/MSI has not been invoked.

The encrypted credential file remains outside the repository in LOCALAPPDATA.
Never print or copy its contents into evidence or the repository.

## Checks and next work

Recorded focused checks include checkpoint 159+41+21 assertions, host 393
assertions on Windows PowerShell 5.1 and PowerShell 7, model fault/mutation checks,
official Task Scheduler XSD validation and parent orchestration adapter tests.
The latest snapshot candidate records 48 passing suite executions. Inspect its
frozen manifests and logs; rerunning tests in a frozen bundle changes the logs.

The immediate unresolved issue is native Scheduler task-descriptor compatibility.
A design-only analysis was already running when the user requested this pause.
That analysis has now completed without edits or native operations. Its deferred
recommendation is to review a narrowly bounded lab-only amendment accepting the
exact observed task form together with the exact protected folder form, using
separate validators and repeated readback. It does not claim that the task itself
has inheritance protection or that rechecks eliminate validation-to-use races.
The extra SYSTEM read ACE is redundant with SYSTEM full access; suppressing it
through registration flags would not establish preservation of descriptor flags.
No implementation or security-contract acceptance follows from that analysis.
Any future host/support change requires newly frozen bytes and, under the current
no-overwrite rules, a fresh variant. Preserve the existing never-run task.

After that issue, the lab still needs exact prepared-readback review and one
bounded observation. Production work remains: Core-owned installation transaction,
durable participating-host admission, authenticated package gates, old-package
migration, rollback/service restoration, GUI lifecycle/provisioning integration,
live VPN cleanup/recovery and package upgrade/rollback acceptance. Release is not
authorized by this handoff.
