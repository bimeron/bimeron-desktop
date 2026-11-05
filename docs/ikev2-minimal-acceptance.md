# Minimal Windows IKEv2 acceptance proposal

Status: PLAN ONLY, 2026-11-05. No execution or product enablement is authorized.
Native remains OFF. This checklist defines a finite future acceptance slice,
not a claim that Windows IKEv2 works. Parent-supplied current context is limited
to unresolved error 775 and failed normal cleanup with error 668/unknown outcome.
No cause, recovery, quiescence or subsequent success is inferred.

Desktop planning baseline: `dbcf2dc109d63366e3e3d526d4e4c29c04089089`, initially
clean, branch `plan/ikev2-minimal-acceptance`. T3 confirms the matching isolated
worktree and `codex_petr/gpt-6.1-sol`, medium reasoning. Only committed sources
in this checkout were used; no active Core/lab checkout or runtime was inspected.

## Scope and precedence

Reuse [GUI integration obligations and dependency-ordered slices](ikev2-gui-integration-plan.md),
[profile requirements](ikev2-profiles.md),
[installation lifecycle decisions](ikev2-installation-contract.md),
[connection management](connection-management.md),
[signed Core updates](core-updates.md), and the complete canonical
[build and packaging flow](build-and-packaging.md). This proposal does not
supersede their gates, settle pending consumer-B/security decisions, change pins,
or supply an approval/admission producer. Existing production composition is absent.

The minimum functional statement requires an approved exact pair to connect in
one specified Windows context, authenticate a real tunnel, carry declared allowed
traffic with correct DNS/routes, and positively clean up on Stop. Failure,
cancellation, honest GUI state and safe repeatability are part of that minimum.
Protection required by the selected profile/current contract remains mandatory.

Advanced kill-switch fault coverage, owner death/crash/reboot recovery, exhaustive
legacy servicing races, full release acceptance, Windows ARM64 and other OSes are
separate evidence scopes. Excluding those experiments from this finite functional
slice does not waive any existing enablement/security/release prerequisite. In
particular, unresolved bootstrap protection interval, owner recovery or servicing
authority cannot be relabeled optional to unlock the GUI. No KillSwitch=false,
dynamic-filter substitution, packet fallback or weakened gate is proposed.

## Entry record and dependencies

Every field below must be concrete before a separately authorized live case;
an empty field means BLOCKED, not an implicit default:

- Exact Desktop and Core source commits, executable SHA-256, architecture,
  revisions/protocol compatibility, package identity/hash and independent exact-pair
  approval reference. A source commit, fixture gate or signed update manifest
  alone is insufficient. Reconcile the historical accepted revision and bundle
  mismatch using the existing contract before changing any consumer pin.
- Windows edition/version/build and native architecture; launch/install paths;
  actual interactive user/session and elevation/provisioning ownership context.
  Keep identity evidence private and reference it without logging credentials.
- Exact validated configuration digest, Windows profile identity and owned profile
  baseline; endpoint/server identity, supported authentication method and credential
  source, IPsec policy, IPv4/IPv6 route expectations, DNS servers/namespaces and
  protection policy. Freeze approved traffic destinations and expected results.
- Use only a separately approved supported authentication path. Preconfigured
  saved-Windows credentials may suffice if supported by the exact Core; this
  does not validate new prompts, attempt-only submission, certificate/card input
  or persistence. No new credential/trust-root installation is implied.
- Fresh authoritative admission, exact-byte validation, authenticated peer and
  one-attempt ownership; independently established absence of conflicting owned
  RAS/DNS/WFP state. Missing process/pipe/marker is not sufficient. Record the
  baseline of unrelated profiles, credentials, DNS/routes and network resources.
- Accepted protection interval and lifecycle/maintenance decision; supported
  closure and fresh readmission producers; independently reviewed evidence and
  separate runtime authorization. The active recovery environment is excluded.
- Predeclare one finite observation deadline per case, bounded traffic samples,
  evidence owner/location and safe stop procedure. Timeout means UNKNOWN; it
  grants no process termination, filter removal or automatic recovery authority.

## Finite future case checklist

All live cases below are REQUIRED LIVE / NOT RUN here. Complete offline review
first. Each case records exact entry-record ID, expected and observed GUI/Core/OS
state, timestamps, bounded sanitized evidence references and PASS/FAIL/BLOCKED/
UNKNOWN. A skipped or unavailable case is not PASS. Use no secrets in captures.

1. **Unavailable/refused entry.** Missing approval/admission, changed config/pair,
   unsupported auth or conflicting ownership leaves Connect disabled with a useful
   safe reason. Keyboard, double-click, tray, startup and update restoration cannot
   bypass eligibility or dispatch packet/native activation. Provisioning, if needed,
   follows existing exact preview/consent and ownership rules; cancellation of a
   local exchange does not promise rollback of an already applied operation.
2. **One Connect and authenticated tunnel.** From the admitted clean baseline,
   one explicit Connect produces one attempt. GUI progresses Connecting to
   Connected only after authoritative authenticated-session and required protection
   evidence. Correlate the exact owned RAS session/peer and assigned interface
   with independent OS/network evidence; process start, hello, capability flags
   or a connected label alone fail this criterion. No automatic reconnect.
3. **Allowed traffic, DNS and routes.** While that same session is active, one
   bounded application exchange to each declared allowed destination succeeds
   through the intended tunnel. Verify the observed path and assigned IPv4/IPv6
   routes against the frozen policy, and resolve declared names via the expected
   DNS/namespace policy without unintended resolver use. Explicitly verify the
   declared behavior of an unused/unsupported address family; do not infer it from
   IPv4 success. Required protection is independently observed throughout its
   accepted interval; route/GUI status alone is insufficient. Any leak is FAIL.
4. **Stop with positive cleanup.** Explicit Stop shows Disconnecting and prevents
   new activation. Independently confirm termination of the exact owned RAS
   session, then owned DNS restoration and owned WFP cleanup in the required
   order. Compare with the baseline, preserving unrelated resources. Only after
   authoritative terminal cleanup may the GUI report the clean terminal state;
   required accepted owner closure and observed exit are separate from cleanup.
   Stop reply, process exit, MSI success or missing marker alone is insufficient.
5. **One safe repeat.** Only after case 4 and independently fresh admission,
   explicitly perform cases 2-4 once more with new attempt identity. Require no
   duplicate owner/session/profile, stale consent, reused admission, leftover
   DNS/routes/protection or automatic retry. Current GUI sticky ownership blocking
   must not be cleared merely to make this case runnable; absent approved
   readmission implementation means BLOCKED.
6. **Cancel during pending Connect.** In a separately admitted attempt, request
   Stop during launch/hello/connecting at one preselected boundary. Require no
   late activation, no stale callback changing the selected intent and positive
   terminal cleanup as in case 4. IPC/local exchange cancellation is not native
   Stop or proof of cleanup. Loss of observation stays Unknown and blocks retry.
7. **Failed connection / 775.** In a separately authorized controlled failure case,
   show safe Failed only for authoritative failure; never Connected. If 775 is
   actually returned, report the Windows code with local cause unknown, without
   diagnosing password, EAP or server rejection. Require cleanup/readmission as
   above before another attempt. If 775 cannot be safely produced within the
   frozen case, record its live handling as BLOCKED and retain fixture-only
   presentation coverage; do not provoke account lockout or endlessly redial.
8. **Cleanup failure / 668 or lost observation.** A separately scoped controlled
   case must show retained/unknown protection and recovery required, block Connect,
   backend switching, update and destructive servicing, and preserve evidence.
   Error 668 from failed normal cleanup is not successful Stop or proved absence
   of a connection. Do not infer a cause or successful cleanup from its number.
   Use existing offline failure injection until a safe live setup is separately
   authorized. Stop this sequence immediately on ambiguity; no repeat/recovery
   execution is part of this checklist.

GUI observations accompany each case: accurate Connecting/Connected/Disconnecting/
Failed/Unknown, explicit retained protection/recovery information, safe localized
errors without raw privileged diagnostics, and one eligibility path for every
entry point. Native Windows sizing/DPI, close-to-tray and explicit quit behavior
need actual GUI evidence; offscreen captures do not prove them. Quit cannot hide
or silently abandon unresolved native ownership, and a presentation change cannot
dispatch cleanup, clear admission blocks or authorize packet fallback.

## Critical installation and update boundaries

These are dependencies for supported use, not a new installer design or permission
to run MSI. Apply the existing MSI/update acceptance and historical-route inventory:

- Exact approved clean install remains protected against ordinary-user replacement;
  installation or package rollback never enrolls/approves a native pair by itself.
- While connecting, connected, stopping or Unknown, supported MSI maintenance
  (including direct/quiet/cached repair, self-heal, nested old removal and uninstall)
  and signed updates must be serialized/refused before conflicting mutation.
  A GUI-only status check is insufficient. Preserve existing packet-update behavior.
- Before offline update/remove, positively quiesce owned runtime state, retain
  exclusion through mutation/rollback, and retain verified recovery bytes/state
  until cleanup is confirmed. Missing recovery or an ordinary-user bypass blocks
  enablement. Preserve unrelated profiles, credentials, certificates and trust roots.
- After successful update or restored rollback, validate the actual pair/schema
  afresh before new native admission. No automatic native reconnection, stale
  approval or packet updater restoration; failed/unknown servicing preserves the
  recovery boundary. Existing full historical/race coverage remains required by
  its contract, without expanding this checklist into that matrix.

## Existing evidence versus missing evidence

Inspected existing `tests/NativeIKEv2TransportTest.h`,
`tests/NativeIKEv2SessionTest.h` and `tests/ConnectionManagementTest.h` plus the
GUI plan rather than adding duplicate fixtures. They cover injected transport/
session replies, consent invalidation, one-shot actions, cancellation/uncertainty,
closure distinctions, blocked packet dispatch and GUI outcome rendering. The
latest GUI fixture directly injects hypothetical 775 for EN/RU cause-unknown
wording, stale-result refusal and generic unknown-error fallback. It is not a live
775 wire contract, native dial or authentication result. Synthetic cleanup fields
do not establish actual RAS/DNS/WFP state, and fixtures that block repeated Connect
do not demonstrate implemented readmission.

The GUI plan records prior focused Windows offline builds/tests and Qt captures.
Those are saved evidence, not rerun or independently certified here. Existing
MSI models/table/ACL fixtures establish only their stated offline scope, not live
installer authority, rollback survival or runtime exclusion. No lab handoff/status
was refreshed. All live cases above and critical servicing evidence remain absent
from this planning task. Parent-supplied 775/668 outcomes remain unresolved.

## Stop criteria and finite completion

Stop at the first missing prerequisite, mismatched bytes/context, failed peer or
profile identity, leaked traffic, damaged unrelated state, unconfirmed RAS/DNS/WFP
cleanup, retained protection, observation timeout or servicing bypass. Preserve
state and bounded evidence; report BLOCKED/FAIL/UNKNOWN and keep native OFF.
Do not retry against ambiguity, repair permissions, clear protection, disconnect
unrelated sessions, kill owners, reuse the active lab or ask its operators to run
cases. A changed artifact/config/context requires a new frozen entry record and
explicitly scoped review, not an unbounded expansion of this sequence.

Functional completion requires cases 1-8 with the evidence limits above resolved
and applicable installation/update gates satisfied. Report only acceptance of the
named exact pair/Windows context; no general Windows, ARM64 or release claim.
Advanced protection/recovery and the full four-target release matrix keep their
separate existing gates. Failure to complete this finite slice is a useful result,
not permission to weaken requirements.

This documentation-only handoff runs whitespace/relative-link checks and scope
review. Product tests, CI builds, runtime cases, package matrix, benchmarks and
profiling are not applicable to the prose change and are not passing evidence.
No code, package flow, pin, dependency or installer behavior changes. Proposed
Conventional Commit, without staging/committing:
`docs(ikev2): define finite Windows minimum acceptance checklist`.
