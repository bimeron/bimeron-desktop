# Disabled IKEv2 GUI integration plan

## Authorized offline connection outcome slice (2026-11-05)

Specification recorded before implementation. This separate Desktop worktree is
`feat/ikev2-gui-connection-feedback`, based on local
`plan/ikev2-gui-integration` at `1fb7157e6cb51b17d87a79bd85ca2e1d3c16892d`,
including D3 `4edb869`. Initial Git state was clean and T3 binding matched.

Existing MainWindow presentation already covers connecting, connected, stopping,
failed, unknown protection and owner closure. Session already prevents repeated
activation; MainWindow retains its sticky block even after closure. Reuse these
paths and their fixtures instead of adding a connection state machine or adapter.

The missing bounded presentation is Windows error 775 with unknown local cause.
Transport currently strips LastError/ErrorStage and has no supported structured
775 or EAP-UI-required result. Do not parse diagnostics or extend the wire schema,
safe-code transport allowlist, Session contract or capabilities. A hypothetical
775 snapshot injected directly by the private MainWindow fixture may exercise a
literal presentation-only branch; clearly label that branch and captures as
fixture-only, unreachable through the current transport. Say that the connection
failed with Windows error 775 and its local cause is unknown; do not assert wrong
password, server rejection or a need for EAP UI. No supported explicit EAP UI
evidence exists in this Desktop contract, so no EAP-required presentation is added.

Acceptance:

- Assert existing connecting/connected/disconnecting/failed labels through the
  Session-to-MainWindow fixture path, with disabled Connect and no packet dispatch.
- Exercise repeated Connect intents during an attempt without another launch or
  activation, and preserve unknown-cleanup blocking after error and controller loss.
- Check the hypothetical 775 text in English and Russian, unknown-code fallback,
  no raw diagnostic leakage, no EAP/password/server-rejection inference, and no
  request, retry, cleanup or readmission caused by rendering it.
- Capture and inspect actual Qt 775 presentation using the existing preview seam;
  keep generated evidence under ignored build/. Preserve native OFF, packet and
  GeoData behavior. Run focused Windows amd64 Release build/CTest and translation
  and whitespace checks; report untested targets honestly. No performance-sensitive
  work is planned, so benchmarks/profiling are not applicable.

No Core/lab/VM/server/network/service/credential access, other-worktree changes,
runtime wiring, OS prompts, package/release work, staging or commits are authorized.
The supported build/packaging flow is unchanged.

### Offline outcome implementation and validation

The only product delta is a guarded literal 775 presentation in
`MainWindow::showNativeSnapshot` and its Russian translation. It requires a
fixture-injected authoritative failed snapshot. Unknown/stale snapshots retain
generic unknown presentation, and arbitrary diagnostics retain the generic error.
Transport and Session production sources, Actions, wire fields, approval gates,
capabilities and packet/GeoData code are unchanged. There is no 775 producer in
the current transport, and this presentation is not runtime error handling.

`ConnectionManagementTest.h` now verifies existing Session-to-window transitions,
disabled Connect/direct toggle refusal, exact EN/RU 775 text, no authentication
diagnosis, stale evidence and generic fallback, and no rendering-induced requests.
`NativeIKEv2SessionTest.h` adds repeated Connect intents during the existing
one-activation fixture. Existing cleanup/retained-protection/controller-loss,
closure, D3 provisioning, packet and GeoData regressions remain in the suites.

One implementation child ran with confirmed `codex_petr/gpt-6-astra`, reasoning
`low`, service tier `default`; coordinator review completed bilingual/state
acceptance assertions. No provider fallback or additional worker was used.

Local Windows amd64, Qt 6.8.3 / MSVC 19.51 Release checks passed:

- Configure: `cmake --preset windows-amd64-release -DBIMERON_REQUIRE_CORE=OFF
  -DBIMERON_DEPLOY_QT=OFF -DBUILD_TESTING=ON`.
- Build: `cmake --build --preset windows-amd64-release --target bimeron-desktop
  developer-mode-test profile-vless-test geodata-selection-test
  geodata-download-test --parallel 2`.
- Focused CTest with `-C Release --output-on-failure --timeout 60`:
  developer-mode-test 10.16 s; profile-vless-test 3.51 s;
  geodata-selection-test 5.21 s; geodata-download-test 5.20 s. These are
  CI-equivalent focused targets, not the complete CI/platform matrix.
- Tests used Qt bin on PATH, the matching platform plugin directory,
  `QT_QPA_PLATFORM=offscreen`, `QT_QPA_FONTDIR=C:/Windows/Fonts`, and cleared
  optional `BIMERON_*CORE` / `BIMERON_GEOSITE_IDS` inputs.
- Strict UTF-8 Python JSON parsing and `git diff --check` passed.

Four actual Qt 720x600 offscreen images were generated and inspected under
ignored `build/ui-evidence/fixture-775-{en,ru}-{light,dark}.png`. The existing
capture invocation is `developer-mode-test --visual-check --connection-preview
<png> en|ru --native-preview --native-ui-state fixture-775`, with `--ui-dark`
for dark captures. Text wraps within the existing labels and Connect is disabled.
These hypothetical fixture images do not establish native Windows DPI or VPN
behavior. A shell-loop capture command was blocked by execution policy before
running; explicit individual fixture invocations succeeded.

Final review found no raw diagnostic disclosure, new authority, retry, cleanup,
readmission, secret persistence, package or shared-style changes. No performance
path changed; benchmarks/profiling are not applicable. Linux and ARM64 builds or
runtimes were not exercised in this bounded Windows offline slice and are not
claimed unaffected or release-validated. Full CI, packaging and live native
acceptance remain outstanding for future release work. Core, active lab, VM,
services, credentials and other worktrees were not accessed. No staging, commit,
push, merge or release was performed.

Planning snapshot: 2026-11-04. The user subsequently authorized the UI-only stage below. Native enablement remains outside scope.

## Post-D3 bounded runtime backlog (2026-11-04)

This is the current planning boundary and supersedes older next-step suggestions
below. D1 and D3 are complete offline increments, not work to repeat. This update
authorizes no implementation, production composition, gate/pin change or native
operation. Only this English plan document changes; build/packaging flow is unchanged.

### Exact source and evidence boundary

Desktop HEAD is `4edb869b644568e5cbdbe3f49fe94b1237cc30ec`, following D1
`49263d5b273f9a3420cef27056a16f942739272b`. The worktree was clean before this
update. T3 and Git both bind this work to `plan/ikev2-gui-integration` at
`C:/Users/PC/.t3/worktrees/bimeron-desktop/plan-ikev2-gui-integration`;
the planning selection is `codex_petr/gpt-6-astra`, MEDIUM.

Core was inspected read-only in `feat/ikev2-auth-contract`, HEAD
`498596eabe4ec734cb0bda72059dd74a66c9dc49`, with uncommitted auth work. This is
a source snapshot, not an approved binary or the active lab's working source.
The concurrent Core writer owns further corrections and commit preparation.
Exact checkout SHA-256 references for the auth delta inspected here:

- `docs/IKEV2_AUTH_CONTRACT.md`: `30367457f19ec4f615e453bf652029d61074adaf20460a756b17d4222cd4e239`
- `core/capabilities.go`: `4247f26be1d20c2d196b529d8280eabc98590b360f0149380931648c3289c11f`
- `internal/control/ikev2_auth.go`: `8909063adc8bdca89692c649d35316bf6baaccd5a816cea853a3f9156a108a37`
- `internal/control/foreground.go`: `83172aa42e7c761a5cc295bea0228cf4716aab5755e842e0711536a31a53e833`
- `internal/control/server.go`: `a513ed5a05c47d43417ad4acd09e3c82fdb9038976bc8151becd5c64824f66fa`

Provisioning, native config and RAS references in this checkout remain at that
Core HEAD. Its Windows protection capability initializer retains baseline true
values; the five new auth flags default to false. This differs from the historical
dirty lab protection-flags observation below. Neither value approves protected
activation or demonstrates live capability. Reconcile the eventual accepted Core
revision and exact artifacts before any consumer pin changes.

Use the recorded D3 Windows amd64 Release/offline CTest 4/4 result (15.67 seconds)
and Qt fixture captures below as saved GUI evidence, without rerunning them for
this documentation change. Core's auth document records focused tests/vet/race
checks; these were not rerun or independently certified here. Supplied lab C1
finite-12/exact-2 PASS does not establish exact-pair approval, owner-death recovery,
live IKE or product readiness. First Connect/Stop remains under HIGH review at
this planning snapshot; no outcome is assumed. The active lab checkout, VM,
services, network, credentials and GeoData work are outside this task.

### Actual API to GUI obligations

1. **Admission and Connect.** `NativeIKEv2Session::connectProfile(absolutePath,
   reviewedSha256)` exists; `NativeIKEv2Actions` has no Connect method and its
   private `bind` accepts only a fixture Session. MainWindow has no production
   caller. `launchAllowed()` consumes four gates per attempt and still pins
   revision `66d84d8f4dcfc6ada625eb11894b19feea51b7ba`. Transport `Approval`
   describes executable/hash/revision/architecture, not an approval producer.
   Required inputs are independently approved exact Core/Desktop artifacts,
   authoritative fresh managed-host compatibility/inactivity, owner/protection
   absence, protected-installation admission, and selected-Core validation of the
   exact configuration bytes whose digest Transport rechecks. Define the producer,
   lifetime and invalidation of each input before composition. Core/profile/byte
   changes, intervening operations, cancellation and uncertainty must invalidate
   pending authority; no cached bool, successful local Save, static capability,
   missing process or missing pipe grants it. No new wire/schema is selected here.
2. **Windows-profile review and mutation.** Actions `select` -> `preview` delegates
   Session `previewProfile(name, server, "apply"|"remove")`; Transport launches
   the provisioning owner and `hello` begins its one-shot preview. `review()`
   exposes local consent; `confirm(consent)` delegates `confirmPreview(token)`.
   The existing request sends command `apply`/`remove`, `accept_revision`, bound
   `instance` and `plan_token` equal to preview `state_token`. Review all public
   fields: action, windows_profile, server_name, authentication, ipsec, ipv6_routes,
   credentials and trust. D3 already binds generation, selection, instance, token
   and the complete plan and consumes confirmation before dispatch. Core rechecks
   the token before mutation. Stale preview needs a new explicit review with a
   fresh admitted owner; there is no same-owner re-preview or automatic retry.
3. **Cancel, rollback and close.** Actions `cancel()` delegates local
   `cancelExchange()` only. Provisioning has no Stop command; Core mutation runs
   through rollback and rejects `close_owner` during previewing/applying. Terminal
   failed provisioning can close without proving repair. Keep rolled-back failure,
   incomplete rollback, journal/manifest cleanup failure and unknown outcomes
   visible after closure. D3 preserves those safe codes; do not repeat its completed
   allowlist work or display raw elevated diagnostics. Missing ownership/conflict
   detail may receive a separately bounded safe-code mapping, with generic fallback.
4. **Foreground Stop and observation.** Session `stop()` suppresses activation
   and queues Stop while launch/exchange is pending; discovery is read-only
   `hello`, followed by status/events, never replayed activation. Actions exposes
   `stop`, `rediscover` and `closeOwner`, but no product controls invoke them.
   MainWindow's packet eligibility/Stop/quit routes must later select the native
   lifecycle explicitly without routing native commands through packet CoreClient.
   Process creation or hello is not Connected. Event loss requires status refresh;
   IPC loss/cancel remains unknown, with no replacement launch or automatic retry.
5. **Closure versus readmission.** Transport `processState()` uses its retained
   launched handle or authenticated discovered-peer process handle. Discovery
   therefore can observe exit; absence without such a handle is Unknown. Session
   requires accepted close plus observed exit; lost close reply plus exit cannot
   fabricate closure. Foreground close additionally requires its terminal cleanup
   checks; provisioning terminality has the narrower meaning above. MainWindow
   `showNativeSnapshot()` keeps `nativeOwnerBlocked_` true even on `ownerClosed`,
   and `requestShutdown()` refuses unresolved native exit. A contract for fresh
   admission/recovery must precede clearing that block, returning to packet use,
   or constructing a new one-attempt controller. Neither UI destruction nor peer
   exit proves global owner/protection absence or surviving rollback completion.
6. **Authentication remains unavailable.** Core `beginIKEAuth`/`answerIKEAuth`
   are private in-process fixture seams, not callable Desktop APIs. All five
   `auth_submission`, `auth_attempt_only`, `auth_windows_persistence`,
   `auth_certificate_selection` and `auth_smart_card_interaction` flags are false.
   The seam binds installation/instance/session/attempt/request/profile/server
   plus private exchange authority, uses bounded expiry and one-shot consumption,
   and retires on lifecycle/authority loss. Its synchronous fixture acceptance is
   neither authentication success nor storage success. Native routes still reject
   generic challenge/answers; no credential wire, config key or production consumer
   exists. Existing saved Windows credentials/EAP-26 identity use does not prove
   attempt-only input. Keep local GUI drafts unsaved/unsubmitted and preview Save
   blocking. Before enabling prompts, Core must establish supported Windows method
   UI or credential preparation, correct user/thread context, protected authority
   across exchanges, cancellation and separate storage consent/result semantics.
   Remember defaults off; unsupported persistence must not silently save, and a
   save failure cannot report remembered credentials. Certificate/card selection
   remains a separate later capability. No PIN field, arbitrary EAP blob, secret
   IPC extension, private-key export or promise of QString zeroization is proposed.

### Dependency-ordered slices and acceptance

These are proposed later tasks, not implementation authorization. Keep each slice
small and amend its acceptance before code if a contract changes.

1. **Independent disabled/offline slice.** Preserve D3 and the unavailable default.
   The smallest useful follow-up is a focused fixture/readout change for one
   demonstrated gap, such as safe ownership-conflict presentation or false/missing
   auth capability refusal. First inspect existing fixtures and add only missing
   cases. Acceptance: no live controller, native request, OS chooser, credential
   storage or serialization; missing/false auth flags never enable submission;
   unknown codes remain generic; stale/cancelled consent never revives. Existing
   one-shot and closure cases are saved coverage, not unfinished implementation.
   This can proceed independently of lab/Core work after separate authorization.
2. **Authority contract handoff (runtime prerequisite).** Parent/Core owners must
   deliver exact approved artifacts and revision compatibility, admission freshness
   and invalidation rules, exact-byte validation/digest provenance, and authoritative
   closure/recovery/readmission evidence. Acceptance is source-backed producer and
   consumer mapping with expired, changed-artifact/config, conflicting-owner and
   unknown-protection refusal cases. A mock demonstrating these rules is not a
   native authority producer. Retain consumer-B scope; no mandatory managed-host
   installation or installer redesign is inferred from legacy gate names.
3. **Provisioning runtime slice (after 2).** Add only the narrow approved production
   composition and editor preview/confirm/remove bindings using the D3 adapter.
   Review the fixture-only bind restriction explicitly; do not merely remove it
   to enable runtime. Acceptance: admitted create/unchanged/remove and ownership
   conflict, one exact consent, token change, selection/cancel/late callback,
   interrupted apply, rollback failure, close refusal while busy, and terminal
   closure without false recovery. A second preview requires fresh admission and
   a fresh controller; failure cannot unlock Connect. Actual native acceptance is
   a separately authorized parent-owned step, not an offline fixture claim.
4. **Foreground lifecycle and recovery slice (after 2 and accepted Core lifecycle
   review; provisioning 3 only if that workflow is required).** Add the missing
   Connect delegation and bind button/double-click/Enter/tray/Stop/startup discovery
   to one native eligibility path. Keep saved-Windows authentication as the only
   existing path; new input is not a prerequisite if approved Core supports the
   preconfigured path. Before enablement, accept Stop during launch/hello/connect,
   single activation, status after dropped events, identity mismatch, IPC loss,
   retained protection, accepted-close-only, exit-only, lost close reply, discovered
   peer exit and unresolved recovery. Prove approved readmission before releasing
   the sticky block; preserve packet Stop while native is merely selected, no
   auto-reconnect, no native statistics, and explicit quit/close-to-tray behavior.
   First Connect/Stop review and owner-death evidence remain distinct gates.
5. **Optional auth runtime slice (after 2/4 and a separately accepted callable Core
   auth contract).** Integrate only supported methods. Acceptance binds prompt and
   reply to exact attempt/profile/authority, with missing/rejected credentials,
   expiry, cancellation, duplicate/late answer, selection change, Stop and IPC
   loss. Verify attempt-only semantics or explicitly return the persistence-only
   constraint for product decision; test remember off/on/failure independently of
   authentication outcome. Synthetic secrets must not appear in config, settings,
   logs, arguments, previews or captures. Private seam tests do not satisfy native
   provider/storage acceptance. Certificate/card integration follows its own
   capability/eligibility/provider contract, without blocking a supported saved path.

For a later implementation, run focused existing fixtures for changed behavior
and relevant CI-equivalent checks, with actual Qt captures only for presentation
changes. No full release matrix, live capture, build or native call is needed for
this plan update. Benchmarking/profiling are inapplicable. Documentation validation
is source mapping, exact Git/T3 identity, snapshot hashes, scope review and
`git diff --check`; no release readiness is claimed. Proposed commit scope, for
later authorization only: `docs(ikev2): bound post-D3 runtime wiring backlog`.

## D2 source resolution and D3 offline specification (2026-11-04)

This section supersedes the earlier unanswered D2 list only where source evidence
answers it. The user authorized D2 planning at MEDIUM and D3 implementation at
LOW, without runtime enablement, Core edits, commits, push or release. Verified
Desktop baseline is `49263d5b273f9a3420cef27056a16f942739272b`, clean: D1 was
committed by the previous GUI thread with separate user permission. Preserve it.
Core was read only at `498596eabe4ec734cb0bda72059dd74a66c9dc49` with dirty
Step 5-9 work. Source evidence is not approval of any executable or final pair.

### D2 implemented, unsupported and unresolved contracts

- **Approval/admission: unresolved.** Desktop `NativeIKEv2Transport.h:14-19`
  accepts explicit artifact approval; `NativeIKEv2Session.cpp:78-90` consumes
  launch gates once and pins its historical accepted revision. There is no
  production authority producer for the approved pair, fresh admission/absence,
  or reviewed selected-Core config digest. D1 validation success does not supply
  these. Keep runtime composition absent and show a concrete unavailable reason.
  Do not turn on gates, change revision/digest pins, or reinterpret consumer B as
  mandatory managed-host installation.
- **Provisioning: implemented scaffold.** Core
  `internal/control/server.go:22-40` defines request fields;
  `internal/systemvpn/profile.go:13-24` defines the public plan. Review profile,
  server, action, authentication, IPsec, IPv6 routes, credentials and trust.
  `profile_control.go:72-107` accepts `apply`/`remove` with exact `accept_revision`,
  `instance`, and `plan_token` matching the preview's `state_token`.
  `profile_windows.go:174-176` rechecks state immediately before mutation and
  returns `ikev2_profile_preview_changed` on mismatch. No in-place preview retry
  exists: `profile_control.go:42-70` starts preview once. A stale result consumes
  that intent; a fresh owner/admission/review is a later explicit operation.
- **Cancellation/rollback: bounded support.** Core
  `profile_windows.go:25` keeps mutation synchronous through rollback;
  `profile_control.go:108-120` refuses close while previewing/applying but accepts
  terminal provisioning close, including failure. There is no provisioning stop
  command. Local cancel invalidates confirmation and cancels local exchange only;
  it cannot promise rollback or stop an applied operation. Error codes in
  `profile_control.go:130-137` distinguish changed preview, rolled-back creation,
  incomplete rollback, journal/manifest cleanup failure and ownership conflicts.
  Desktop must preserve the useful safe provisioning codes and render stale,
  failure and manual-review/unknown outcomes without displaying privileged text.
  Owner close is not proof that a failed provisioning transaction was repaired.
- **Credentials: unsupported native submission.** Core
  `foreground.go:20-23` and `profile_control.go:29-32` reject challenge/answers
  fields on these routes. `ras_windows.go:267` requires EAP method 26;
  `ras_windows.go:327-370` reads saved Windows credentials and EAP identity.
  `systemvpn/config.go:15-20` permits only WindowsProfile, ServerName, KillSwitch;
  `core/config.go:35` aliases that type. No native username/password, attempt-only
  submission, remember-consent/result, certificate selection or smart-card/PIN
  contract exists. Keep existing local authentication drafts and Save blocking;
  no new config keys, credential IPC, logs or storage. Future prompts must bind
  exact attempt/profile and ignore cancelled/late replies; this is a requirement,
  not an implemented backend feature.
- **Closure: observation exists; admission release unresolved.** Desktop
  `NativeIKEv2Transport.cpp:595-607` observes either its launched handle or the
  authenticated peer process handle retained during discovery. Thus discovery
  does not necessarily lack an exit handle. Session `closeOwner`, `clean` and
  `poll` require accepted close plus observed exit; snapshot/pipe disappearance
  alone is insufficient. `MainWindow.cpp:1563-1565` deliberately retains its
  sticky owner block even after ownerClosed. Do not release it or automatically
  replace/reconnect a controller: fresh authority and recovery remain absent.
- **Certificate/card capabilities: unsupported.** Core
  `core/capabilities.go:27-38` has no authentication-method/identity capabilities;
  working line 47 reports nested protection flags false. Static supported=true
  does not mean credentials exist or protected activation is admitted. Preserve
  current preview-only method UI and unavailable OS choosers. No Core redesign.

### D3 accepted offline implementation and acceptance

Implement one small production-useful native action/state adapter around the
existing Session, with a default unavailable composition and private fixture
injection. Integrate its state with existing native rendering/confirmation seams;
do not add a general command bus or a second lifecycle engine. Production must
construct no live native transport/controller and perform no native discovery,
launch, apply/remove, connection, credential or OS operation. A compiled adapter
may delegate existing Session methods only when privately supplied by fixtures
at this stage. Do not grant launch gates in product code.

Bind each local intent to selected profile/server/action and an exact generation;
bind consent additionally to the current owner instance and token. Profile change,
cancel, stale/invalid preview, IPC loss and controller destruction invalidate
consent. Repeated/late consent cannot apply; duplicate callbacks must not restore
cancelled intent. Read-only observation may update unknown state but never replay
mutation or automatically reconnect. Reuse Session's transport correlation and
identity checks rather than inventing another wire API.

Expose only existing preview, confirm, local cancel, stop, read-only rediscovery
and close-owner behavior. Distinguish unavailable, previewing/reviewable, applying,
success, stale review, failed/manual-review, unknown and closure pending/observed
where the existing API supplies evidence. Never classify unknown cleanup as safe,
or terminal rollback failure as success. Fresh connection/retry remains refused
without fresh admission and a fresh controller; sticky MainWindow blocking stays.

Before implementation, this specification accepts these focused fixture cases:
default composition refuses without dispatch; exact preview-confirm dispatches
once; mismatched profile/server/action/token/instance and malformed preview refuse;
cancel before/after preview, selection change, repeated and late callbacks cannot
apply; changed token requires new review; IPC loss/destroyed controller retains
unknown cleanup; rollback/manual-review remains visible after close; close during
operation refuses; accepted-close-only and exit-only do not fabricate closure;
discovery with authenticated peer exit can observe closure without granting new
admission. Test actual adapter and Qt rendering, not a mirrored model. Preserve
D1 and packet/GeoData regressions, English/Russian conventions and shared styles.

Build Windows amd64 Release and run developer-mode-test, profile-vless-test,
geodata-selection-test and geodata-download-test with offline Qt fixtures. Capture
actual Qt screens for materially changed presentation. Review security, secret
handling, callback lifetime and unnecessary complexity; run whitespace and
translation checks. No performance-sensitive work is planned, so benchmarking
and profiling are not applicable. Linux/ARM64 and release packaging are deferred
under this bounded task, not claimed unaffected or release-validated. Record
results and exact remaining runtime dependencies here, then stop.

### D3 bounded UI refinement

Confirmation renders only the public plan fields as wrapping plain text in a bounded scroll area with a persistent footer. Provisioning rendering consumes adapter state, preserves failure evidence after close, and retains sticky connection blocking. Production explains the missing approved Core/Desktop pair and fresh admission.

### D3 offline implementation evidence (2026-11-04)

Added header-only `NativeIKEv2Actions`: default unavailable, private binding accepts
only the existing Session Hooks fixture, and a controller cannot be replaced.
Local consent binds generation, profile, server, action, instance, token and the
complete public plan. Typed, bounded plan fields are required; malformed or
mismatched plans invalidate intent even if a later observation looks valid.
confirmation is consumed before dispatch. Selection changes, cancel, unknown
observations, owner/token changes and destruction invalidate intent. Session still
owns transport validation, operation refusal and closure evidence. Read-only
rediscovery does not restore consent. Closure preserves safe failure/recovery
information in the adapter snapshot. No credentials or additional configuration
fields are serialized. Authentication drafts remain local and runtime remains OFF.

MainWindow observes adapter snapshots, retains its sticky owner block and routes
its private confirmation seam through the existing Qt confirmation dialog and the
adapter. Profile selection invalidates consent. No production action is wired to
this seam and no live controller is constructed. The production unavailable reason
now names the missing approved artifact pair and fresh admission. Offline native
presentation distinguishes preview, apply, success, stale, failed and unknown;
the confirmation displays all public plan fields as plain text. English/Russian
labels are included. Native status wraps without changing packet styles.
The Session and dialog grant narrowly scoped friendship for this integration;
there are no build registration, bundle pin, Core or packaging changes.

Windows amd64 Release built in the existing VS18/Qt 6.8.3 build with
`BIMERON_DEPLOY_QT=OFF`. Adapter fixtures exercise unavailable dispatch, exact
one-shot consent, all consent identity mismatches, cancel, selection changes,
IPC timeout, changed token or summary, late/repeated responses, read-only
provisioning rediscovery, destruction/rebind refusal, applying-close refusal and
accepted-close versus observed-exit evidence. Failure codes survive status/close
and unknown observations. Actual Qt modal tests cover consent, cancellation and
selection/plan changes during review. Existing Session/Transport and native
rendering fixtures retain lifecycle, correlation and malformed wire coverage.

Validation uses `QT_QPA_PLATFORM=offscreen`, Qt bin on PATH, with optional
`BIMERON_GEODATA_CORE` and `BIMERON_GEOSITE_IDS` removed. Initial build failure
was private access to the existing dialog factory, corrected with MainWindow
friendship. Initial new Qt fixture placement entered a modal loop while another
test had queued legacy callbacks; isolating the fixture before that scenario
fixed the test, without changing production callback behavior. Overlapping worker
builds caused C1083/LNK1104; coordinator-owned sequential builds succeeded.
A trailing comma in ru.json passed PowerShell's permissive parser but broke Qt
translation-dependent fixtures. Strict Python UTF-8 json.load rejected it;
removing the comma and rebuilding restored all four tests (4/4, 15.65 seconds).
This supersedes earlier partial-worker validation claims. Final native wrapping
verification and capture results are recorded below. No performance-sensitive
change was added, so benchmarking/profiling are not applicable.

Security review: no authority producer, gates enabled in product, live fixture
transport, credential storage, privileged diagnostic output or automatic retry.
Runtime dependencies remain approved exact artifacts, fresh admission and digest
authority, plus the unsupported native authentication contract described above.
Linux/ARM64 and release packaging were not run and are not claimed validated.
No commit, push, merge or release was performed.

### Final D3 offline acceptance

Coordinator reran the final Windows amd64 Release application and all four fixture
builds sequentially, with no parallel writer/build. Final CTest passed 4/4 in
15.67 seconds: developer-mode-test 8.23 s, geodata-download-test 4.38 s,
geodata-selection-test 0.11 s, profile-vless-test 2.95 s. Strict UTF-8 Python JSON
parsing and git diff --check passed. D1 creation/validation/rollback and packet/
GeoData regressions remain in those suites; no test was removed to obtain a pass.

Actual Qt offscreen captures and localized state assertions passed using
`developer-mode-test --visual-check --profile-geometry <absolute-png> --d3-only`
with `--geometry-english --geometry-theme light`, and without the English flag
with `--geometry-theme dark`. The 12 ignored images are
`build/ui-evidence/d3-reviewed-en-light.png*` and `d3-reviewed-ru-dark.png*`:
complete public plan, review, stale plan, incomplete rollback, generic failure
and unknown after cancellation. Representative confirmation/review/unknown
images were inspected. The first captures exposed a clipped Russian upper
status; native-only word wrapping fixed it and the new captures were checked.
The plan values are synthetic public fixture text, not a live Core response;
these images prove Qt rendering only, not Windows DPI or VPN operation.

Final diff review found no Core, package, dependency, bundle pin, shared QSS,
credential persistence or native authority changes. Four additional source-backed
provisioning codes are allowlisted; privileged diagnostic text remains excluded.
Private mock binding is the only active composition. The sticky MainWindow block
is deliberately not released by closure observations. No new product choice is
needed for this offline slice. A later separately authorized runtime stage must
first resolve the exact approved pair, fresh admission/recovery and reviewed
digest producer; authentication APIs remain unsupported. Do not infer authority
from these tests, the Core working tree or lab success. Stop at this boundary.

## Historical D1 roadmap and remaining runtime boundaries

Baseline: commit `2632568` (`feat(ikev2): prepare native profile and authentication UI`).
This section records the earlier roadmap; the D2/D3 section above is current.
At this point the user authorized the bounded implementation stage: D1 local
creation and D2 contract preparation. Runtime enablement is not implied. The
initial UI-only stage is complete, not a completed native VPN feature.

### Current implementation increment

In an unlocked developer session the Add profile dialog offers Create IKEv2
profile; ordinary sessions do not show it until native IKEv2 is a finished user
workflow. The action opens the existing
editor with empty name/profile/server fields and protection requested. Empty
native fields remain visually editable; unsupported documents retain Raw-only
preservation. Cancel creates no stored profile. Save validates a protected
temporary draft with the selected Core, imports it under a fresh UUID, validates
the final path and registers it only after the index save succeeds. Failures
return to the same editor. No Windows profile or credentials are created.

Raw backend reclassification now adds/removes native scrolling while preserving
the existing form. Focused fixtures cover cancel, missing/rejecting validator,
index-write failure with rollback, duplicate display names with distinct paths,
save/reopen/rename and local deletion confirmation. The native deletion prompt
explicitly says the Windows VPN profile remains. Existing import and packet
editing flows are reused, not replaced. Tests use an offline validator fixture;
passing them is not native operational or production-artifact acceptance.

Validation for this increment (Windows amd64, 2026-11-04): application and both
UI test targets built; `developer-mode-test`, `profile-vless-test`,
`geodata-selection-test` and `geodata-download-test` passed. Both UI suites were
rerun after the final layout-only correction. Translation JSON and `git diff
--check` passed. Initial new-test timeouts were caused by reentrant modal-dialog
handling in the test timer; deferred Save/Cancel clicks fixed the fixture. No
benchmark applies. Linux/ARM64, packaging and native runtime were not exercised;
this is local UI acceptance, not a release matrix or release readiness claim.

### D2 handoff: concrete existing contract and required answers

Read-only source check: Core HEAD `498596eabe4ec734cb0bda72059dd74a66c9dc49`;
`internal/control/server.go` and `internal/systemvpn/profile.go` have no working-tree
diff in this check. This is schema evidence, not approval of the dirty Core tree
or an artifact pair. Desktop references: `NativeIKEv2Session.h/.cpp`.

| Existing call or observation | Status and next contract answer |
| --- | --- |
| `connectProfile(absolutePath, reviewedSha256)` | Implemented Session entry; production caller absent. Who supplies exact-artifact approval, fresh admission observations and the selected-Core digest? |
| `previewProfile("Fixture", "vpn.invalid", "apply")` | Implemented Session/Transport scaffolding; gated. Core `ProfilePlan` returns `action`, `windows_profile`, `server_name`, `state_token`, `authentication`, `ipsec`, `ipv6_routes`, `credentials`, `trust`. Confirm which fields must be reviewed and the authority for closing an interrupted provisioning owner. |
| `confirmPreview(token)` | Existing request fields are `accept_revision`, `plan_token`, and bound `instance`; response token is named `state_token`. Session verifies profile/server/action/token and rejects duplicate confirmation. Confirm stale-preview recovery and partial rollback terminal observations before UI binding. |
| `stop()` / `closeOwner()` / `cancelExchange()` | Existing separate operations; local cancellation marks unknown and preserves the owner. Confirm evidence sufficient to release the sticky Desktop block, especially discovered owners without a process handle. Pipe/process disappearance alone is insufficient. |
| Username/password, certificate/card selection | No agreed native credential submission API in this Desktop contract. Need attempt/profile binding, cancel/late-result behavior, storage consent/result, attempt-only support and nonsecret config fields. Generic control `answers` fields do not establish native auth support. |

Offline adapter acceptance after answers: stale token requires a new review;
cancel before confirmation sends no mutation; IPC loss retains unknown cleanup;
late auth replies cannot reach another attempt; remember failure cannot claim
saved credentials; owner-close acceptance cannot alone claim authoritative
absence. No additional managed host or replacement protocol is proposed.

### Ownership and completion boundary

This thread owns Desktop profile creation/editing, authentication presentation,
consent, adapters to agreed Core APIs, connection/stop/closure presentation,
startup rediscovery, tests and Desktop documentation. The main thread coordinates
Core API/schema changes, Windows credential/EAP/certificate/card implementations,
installation/admission, protection/recovery and lab evidence. This thread consumes
those results; it does not duplicate the lab or implement privileged operations.
No mandatory managed host or new generic backend abstraction is introduced.

Done here means a user can create a supported profile, prepare its Windows side,
provide supported credentials, explicitly connect/stop and understand unresolved
cleanup through the existing Desktop interface, backed by accepted Core contracts.
Unsupported authentication modes remain honestly unavailable. Full product release
and installer acceptance remain separate. No push, PR, merge, package or release
is authorized by this planning request.

### D1 — finish the local profile workflow (independent)

Original gap: users had to hand-create/import a `.conf`; a GUI-only build without
Core opens the import-correction path. A manually supplied lab file is not the
product creation workflow.

- Add a distinct “Create IKEv2 profile…” action inside the existing Add profile
  dialog, alongside the current URI workflow. Keep the existing Add key button,
  default URI behavior and other protocol forms unchanged. The new action opens
  the current native editor; do not create a second native form or add a wizard.
- Fields: display name, Windows profile name, server, current native protection
  request and authentication section. New local profiles do not claim that the
  Windows profile or credentials exist. Never seed real server/user data by default.
- Preserve the distinction between Save local configuration and Apply Windows
  changes. Use existing selected-Core validation and protected atomic profile
  writes before registering a usable profile. Missing validator must preserve an
  editable draft and explain why it cannot yet be saved; no fake validation pass.
- Complete create/cancel/import/edit/reopen/rename/local-delete paths. Local Remove
  must not remove the Windows profile or credentials. Document that distinction
  where the destructive action is confirmed, without adding repeated warnings.
- Preserve the user's immediate Raw display, compact native layout, existing
  window size and scrolling. Native-to-packet and packet-to-native Raw edits must
  update visible fields, draft state and scrolling correctly. Keep the packet
  UI unchanged, including its credentials and GeoData behavior.

Acceptance: all entry points reach the same native editor; cancelling writes
nothing; valid local save round-trips; invalid/missing validation retains the
original file and draft; duplicate/colliding names and file errors are explicit.
Tests must cover reclassification and unsupported fields without data loss.

### D2 — agree the Desktop/Core integration contract (main-thread handoff)

Prepare one small list of request/response examples and unresolved fields from
actual current APIs. Mark each item implemented, missing, or awaiting acceptance;
do not design a disconnected replacement protocol.

Required answers before dependent code:

- Exact selected Core/artifact approval and validation/digest source; production
  admission/owner/protection observations, their freshness and invalidation.
- Provisioning preview action/profile/server/token, stale-preview handling,
  apply/remove terminal states, safe errors and cleanup/closure requirements.
- Credential acquisition/storage API in the interactive user's context, prompt
  ownership, save consent and whether attempt-only password use is supported.
- Attempt/profile binding, cancellation, late reply and IPC-loss semantics for
  authentication; what nonsecret preferences belong in the accepted config schema.
- Certificate/card capability, eligible identity representation and supported
  provider UI. No Desktop PIN checking or private-key export.
- Authority for releasing MainWindow's sticky native block, including discovery
  of an existing owner without a launched-process handle and unresolved cleanup.

Output: agreed examples plus focused adapter acceptance cases. No production
activation from a boolean supplied by Desktop or from process/pipe absence alone.
D1 and offline presentation tests do not wait for lab completion.

### D3 — wire Windows-profile preparation (after its contract)

Reuse `previewProfile`, `confirmPreview` and the existing confirmation surface.
Bind the exact reviewed profile/server/action and opaque token; reject stale or
changed drafts and duplicate clicks. Render previewing/applying/success/failure
separately from VPN connection. A successful local Save is not OS provisioning.
Explicit Remove targets only the reviewed owned Windows profile; no adoption or
removal of an unrelated same-name profile. Cancellation before confirmation sends
no mutation; dismissing a pending operation does not claim rollback. Close the
provisioning owner only under its accepted terminal/cleanup contract.

Acceptance: unchanged preview, create/remove, conflict, stale token, changed
selection, user cancel, interrupted apply, partial rollback and IPC loss, using
existing mocks first. Production actions stay disabled until authority exists.

### D4 — make username/password interaction functional (after auth contract)

Replace the current nonpersistent UI draft with the accepted auth flow. Reuse
saved Windows credentials without reading/displaying the stored password. Support
explicit input or a connection-time prompt only as permitted by Core. Remember
requires separate explicit consent and a confirmed storage result; unchecked must
not silently persist. If Core requires persistence, show that constraint instead
of promising a nonexistent attempt-only mode.

Bind prompts and results to the selected profile/server and one attempt. Cancel
ends that attempt; errors permit explicit retry, not a credential retry loop.
Changing method/profile clears secrets and invalidates pending input. Credentials
must not reach `.conf`, QSettings, logs, arguments, screenshots or preview tokens.
Persist only accepted nonsecret settings; never silently discard an apparently
saved authentication selection. Reuse safe localized errors, not raw diagnostics.

Acceptance: saved/missing/rejected credentials, remember on/off/failure, cancelled
prompt, changed profile, duplicate submit, late answer, timeout and IPC loss.
Use synthetic secrets and assert no persistence/logging or cross-profile reuse.

### D5 — connect foreground lifecycle to MainWindow (after admission contract)

Add the missing production caller and a narrow native action adapter around the
existing Session/Transport. Do not route native actions through packet CoreClient.
Discovery is read-only; Connect validates exact bytes, obtains fresh authority,
launches once and lets Session perform authenticated hello/activation. Buttons,
double-click, Enter and tray actions must share the same eligibility rules.

Connect → verifying/connecting → authoritative connected; Stop during launch
suppresses activation, otherwise requests stop → cleanup → owner closure. Never
translate launch success, a terminal connection state, accepted close or process
exit alone into proven cleanup. Display connection and protection separately.
Keep native stats unavailable and automatic connection/reconnection disabled.
A later explicit attempt needs fresh authority and a fresh controller.

Handle selecting another profile during native work without overwriting native
state, and selecting native during packet work without breaking packet Stop.
Prevent conflicting operations through existing controls, without inventing new
service/installation policy. A blocked action should have one actionable reason.

Acceptance: one activation, queued Stop, identity/revision mismatch, malformed
reply, event loss/status refresh, timeout/cancel, safe errors, retained protection,
cleanup failure and no packet fallback. Preserve all current native transport
identity checks and packet lifecycle/GeoData regressions.

### D6 — reopen, recovery presentation and exit (after closure/recovery contract)

On GUI reopen, discover and attach without replaying activation. Recover current
state from authoritative status; never infer safety from missing IPC. Show retained,
unknown and recovery-required states with the accepted user action. Only call an
existing approved Core recovery operation; no GUI-side WFP/DNS cleanup.

Define close-to-tray versus explicit Quit consistently with existing app behavior.
If a user requests disconnection and exit, complete Stop/cleanup/close first;
uncertainty keeps a visible unresolved result. Local UI destruction does not stop
an owner. Release the native UI block only on the agreed evidence; do not leave
it permanently latched after proven completion. Provisioning shutdown uses its
own pending-operation semantics rather than VPN Stop.

Acceptance: restart while connected/stopping, owner loss, stale owner/session,
GUI loss during auth/provisioning, rejected/lost close reply, observed exit,
unknown cleanup and returning to packet use after proven completion.

### D7 — certificates and smart cards (separate optional capability stage)

Desktop implements identity selection, summaries and cancellation only after Core
and server support is agreed. Integrate actual eligible identities and provider
interaction; distinguish no certificate, unsuitable/expired identity, missing or
removed card, cancelled PIN UI and authentication failure. Keep secrets out of
profile storage and never weaken server trust checks. Test with mocks, then use
separately authorized integration evidence. Do not block delivery of a supported
password-based first version on unsupported card/certificate functionality.

### D8 — validation, review and handoff for each bounded stage

Specify acceptance before each implementation, run focused tests, inspect real Qt
screens (EN/RU, light/dark, minimum size and 100/150% scaling), review the diff and
record actual results. Preserve existing profile/connection/native/GeoData suites.
Benchmark/profile only if an implementation introduces a performance-sensitive
path. Keep generated captures and synthetic profiles out of Git.

Before production integration is considered complete, run relevant CI-equivalent
Windows x64/ARM64 and Linux amd64/arm64 builds/tests, explicitly distinguishing
cross-compilation from runtime evidence. Native functionality remains Windows-only;
Linux must keep its unavailable state and existing packet behavior. An unavailable
target is a reported validation gap, not assumed success. Full package matrix and
live installation/VPN acceptance belong to separately authorized release/product
work coordinated by the main thread.

Handoff includes Desktop commit(s), exact Core contract/artifact used, tests and
screenshots, supported/disabled capabilities and remaining gates. Commit/push/PR
operations require the user's authorization for that stage. Stop after the stage,
not after silently extending into Core or lab work.

### Recommended execution sequence

Next: D1, plus preparation of the D2 contract handoff. Then D3 and D4 as their APIs
become available; D5 and D6 are one lifecycle acceptance chain before enablement.
D7 follows separately when supported. D8 applies to every stage. If Core supplies
only a subset, integrate and test that subset offline without claiming full readiness.

## Historical UI-only stage: specification and evidence

The user requested implementation planning for the UI alone, followed later by
runtime work. This section replaces slices 1–2 below as the immediate implementation
brief. The earlier HTML mockups are not accepted visual specifications: they
changed unrelated profile presentation and did not reproduce the actual Qt UI.
Use existing Qt widgets as the baseline. The user authorized this UI-only implementation with a subsequent instruction.

### Scope and fixed decisions

- Keep MainWindow size, sidebar, navigation, profile delegate, list spacing,
  icons, selection styling, button order, shortcuts and shared QSS unchanged.
  Keep non-native profile text, traffic counters, editor tabs, tray behavior,
  connect/reconnect and deletion behavior unchanged. Do not rename shared buttons
  or replace the page with a new layout. No global theme changes.
- Reuse `nativeStatusLabel_` for native-only explanatory text. Native selection
  alone must not overwrite the status or statistics of an active packet session.
  Existing `showNativeSnapshot` ownership blocking remains authoritative; selection
  is not ownership. Keep that block across selection changes, stale packet replies
  and controller destruction. This stage cannot clear it or authorize another start.
- Keep native Connect, provisioning and removal unavailable in production.
  No new production controller construction, `setLaunchGates`, command dispatch,
  Core process, IPC, OS inventory, service or credential operation. No timer-driven
  simulated connection and no fake success in the shipped UI.
- Preserve native config serialization, field order, Raw-config concealment and
  explicit reveal, validation and Save/Cancel behavior. Saving the document never
  means installing a Windows profile. Native fields already exist; do not rebuild
  them or add native protocol selection to the shared Add key workflow.
- Use existing `tr()` and `assets/translations/ru.json`; keep source strings and
  documentation English. Use existing QPushButton/QLabel/QDialog styles, plain
  text for supplied values, tab order and accessible labels. No new dependencies.

### UI-1: main-window presentation

Files: `src/MainWindow.h/.cpp`, translations, existing connection fixtures.
Limit changes to the existing native presentation methods and native-only branches.

Production native selection shows: “Native IKEv2 is not available in this build.”
and a short explanation that connection and Windows profile operations are not
enabled. Do not display artifact hashes, revision gates, IPC vocabulary or lab
requirements to the ordinary user. Preserve detailed engineering reasons in the
planning documentation, not the status label.

For supplied offline snapshots, render the following states in the existing
connection status/native detail labels. Do not add a new status card:

- Connecting: connection is being established; protection is unconfirmed unless
  separately authoritative. A launch acknowledgement is not Connected.
- Connected: show the authoritative connection state, and traffic/DNS protection
  separately. Connected alone must not imply either protection is active.
- Stopping: disconnecting; cleanup pending. Terminal connection state with retained
  protection remains unresolved, rather than returning to the normal idle view.
- Unknown: connection state cannot be confirmed; protection Unknown. Never retain
  a stale Active claim after loss of authority.
- Failed: a bounded localized error and separately known protection/cleanup state.
  Unknown error codes use a generic message; never display raw Core diagnostics.
- Clean terminal / owner closure pending: connection ended, closure still pending.
  Accepted closure without observed exit is not completed closure. Even completed
  closure does not grant admission or clear the existing block in this UI stage.

Native statistics remain unavailable without modifying packet statistics.
Keep native action buttons disabled, including in presentation fixtures: enabled
Stop/Connect behavior belongs to the later adapter stage. Existing native tray
status may mirror the same text; do not change packet tray logic or quit policy.
Do not modify Session's safe-code allowlist in this stage.

### UI-2: native editor and confirmation surface

Files: `src/ProfileEditorDialog.h/.cpp`, translations, existing editor fixtures.
Append a compact native-only form row/group below the existing native fields in
the General tab, headed “Windows profile”. Show an unavailable reason and two
disabled buttons: “Review Windows profile changes…” and
“Remove Windows profile…”. Local document validity is not Windows profile inventory;
never claim that the OS profile exists, is absent or is prepared from config alone.
The group must be hidden without consuming space for every non-native profile,
including after Raw-config reclassification. Respect `assessRepresentation` rather
than assuming the initial constructor's profile type never changes.

Prepare a small private confirmation-dialog factory in the same implementation
file, accessible to the existing test seam. No public generic backend interface,
new source target or production action handler is needed. A supplied presentation
record contains only profile name, server, proposed action and a reviewable flag;
the UI must not interpret authority tokens. Tests call the factory directly.
The real preview/token adapter is deferred.

The dialog shows exact profile/server and Create, Unchanged or Remove wording.
Create/Remove require explicit confirmation; Cancel is the default, Escape and
window close cancel. Unchanged is informational with Close only. Pending, failed
or invalid previews have no enabled confirmation. Confirm returns a local UI
result once and performs no mutation. Test malicious markup as literal text,
long values and keyboard activation. Do not show passwords, raw configs, tokens,
installation IDs or internal command names. Removal wording distinguishes the
Windows profile from the existing local-list Remove action.

### UI-3: real Qt evidence and focused regression checks

Before editing, capture a baseline using the existing isolated visual fixtures,
with synthetic profiles and temporary settings, from the unchanged application.
Capture the same cases after the change using real Qt widgets, not HTML or AI
images. Reuse `tests/ConnectionManagementTest.h` preview facilities and
`tests/ProfileVlessTest.cpp` editor coverage. Keep generated screenshots ignored
under this worktree's build directory. Do not launch an ordinary configured app,
use the user's saved profiles, or execute native Core to obtain screenshots.
If no verified local fixture executable exists, build the supported local fixture
using the canonical build instructions; do not borrow or modify parent lab binaries.

Acceptance cases:

1. A representative packet profile main window and editor retain baseline layout,
   labels, available actions and serialization. Selecting native while a packet
   session is active does not replace the active session status/counters. Returning
   to a packet selection leaves no native-only editor rows or reserved blank space.
2. Native unavailable, connecting, connected, stopping, failed, retained cleanup,
   unknown and closure-pending presentations use synthetic inputs. Exercise
   changed signals, controller destruction and queued legacy replies using the
   existing native fixtures; none grants admission, sends a command or releases
   the native ownership block. Update old label-equality assertions to check the
   same safety invariant if concise status and detail text now differ.
3. Production native controls remain disabled through button, double-click,
   Enter, tray and profile-switch paths. Save does not call provisioning.
   A signal/request spy proves zero newly introduced native dispatches.
4. Native editor field round-trip, native section precedence, hidden credentials,
   explicit Raw reveal, cancelled edits and Raw/native reclassification preserve
   current behavior. Non-native routing and GeoData controls are unchanged.
5. Confirmation fixtures cover Create, Remove, Unchanged, pending/error,
   cancellation, default focus, duplicate confirmation and long/markup values.
   This tests local dialog results only, not token freshness or OS rollback.
6. Inspect real light/dark, English/Russian screenshots at normal and minimum
   window sizes (main 720x600; editor minimum 620x460), and 100%/150% scaling.
   No clipped text, enlarged shared minimum size, overlapping controls or hidden
   focus. Do not claim native platform/DPI evidence from a synthetic rendering.

Run the existing local connection/editor regression target and relevant native
session/transport fixtures, translation validation and diff checks. Run existing
GeoData selection/download checks as regression checks, without changing their
implementation. Identify exact supported commands from build documentation before
running them; report unavailable toolchains honestly rather than changing pins.
No benchmark is needed for label/dialog work. Full release packaging and live
network validation are outside this UI-only stage. Report actual platforms tested.

### Implementation order and stop condition

Capture baseline → UI-1 and focused checks → UI-2 and focused checks → UI-3 visual
comparison/regressions → update this document with actual results. The bounded
implementation may run on low effort; unresolved lifecycle decisions must remain
deferred rather than being guessed. Stop with reviewable native-only UI changes
and real Qt screenshots. No commits, push, PR, Core/lab edits or next-stage work
without further authorization.

Later work owns validation/digest sourcing, actual preview and token binding,
Connect/Stop/close dispatch, cancellation/IPC recovery, owner-block release,
admission, artifact approval and production enablement. UI completion proves none
of those contracts. This plan does not depend on lab restoration succeeding.

## Sources and boundaries

Desktop branch `plan/ikev2-gui-integration`, base `38e020eb41bfb619f2a794e9628e3d0fc6002a40`. T3 binding and Git HEAD agree. Parent planning documents were inspected read-only in `C:/Users/PC/.t3/worktrees/bimeron-desktop/plan-ikev2-installation-contract/docs`: installation contract status/GUI boundaries, profiles GUI boundaries, and lab handoff. The historical handoff is not the current lab status. Consumer direction B supersedes mandatory managed-host proposals; installer service and runtime remain separate.

Core was inspected read-only at `C:/Users/PC/.t3/worktrees/bimeron-core/plan-ikev2-installation-contract`, HEAD `498596eabe4ec734cb0bda72059dd74a66c9dc49`. Its working tree contains uncommitted lifecycle, RAS, network/WFP and test changes. `internal/control/foreground.go` and `profile_control.go` are unchanged against HEAD; the working `core/capabilities.go` changes nested kill-switch/DNS-protection flags from true to false. Working protected start refuses `ikev2_safe_bootstrap_unavailable`. Do not attribute those working changes to the committed API or approved binaries. Bundled beta2/66d84 is not approval of a final artifact pair.

## Exact existing integration points

- Core `internal/control/foreground.go`, `validateForeground`, `bindForegroundSession`, `foregroundDispatcher`: installation/instance/session binding; read-only `hello`; activation by a second `hello` carrying `accept_revision`; `status`, cursor-based `events`, `stop`, `close_owner`. There is no foreground `start` command. Stop before activation cancels activation; stop timeout returns `cleanup_pending`. Close requires terminal state and inactive session; that alone is weaker than Desktop cleanup checks.
- Core `internal/control/profile_control.go`, `bindProfileControl`: separate provisioning owner. First `hello` schedules preview; `status` exposes `previewing`, `previewed`, `applying`, `succeeded`, `failed`. Confirmation sends `apply` or `remove`, accepted revision and `plan_token` copied from preview `state_token`. Close rejects an operation in progress. This endpoint has no Stop/cancel mutation.
- Desktop `src/NativeIKEv2Transport.h/.cpp`: immutable Approval, endpoint discovery, protected launch, request serialization and authenticated response decoding. `launchSession` takes an absolute config path and reviewed SHA-256; `launchProvisioning` takes profile/server/remove. Local cancellation and destruction do not stop the owner. Process exit is not cleanup proof.
- `src/NativeIKEv2Session.cpp`, `connectProfile`, `received`, `poll`: consumes fresh launch gates once, launches, authenticates hello, activates once, alternates status/events; event drops require status refresh. `stop` queues behind pending exchange. `confirmPreview` checks exact token/name/server/action and confirms once. `clean` requires authoritative terminal cleanup; `closeOwner` then waits for accepted close plus observed process exit. Identity changes poison the controller. Gates and revision literals are existing constraints, not production authority producers.
- `src/MainWindow.cpp:1479`, `nativeUnavailableReason`, `bindNativeController`, `showNativeSnapshot`: binding has no production caller; rendering sets `nativeOwnerBlocked_` unconditionally and shows protection status with no statistics. It does not clear the block after closure or render a complete provisioning/error workflow. `toggleConnection` (1651) and `updateConnectionButton` (2176) remain packet-oriented/disabled for native; no native Stop or closeOwner caller exists. A pointer binding is not completed UI integration.

## Small dependency-ordered implementation slices (future authorization)

1. Extend existing offline MainWindow fixtures to render controller snapshots and bounded safe error messages. Specify button, tray and close-window behavior for the scenarios below. Keep production binding absent and all native operations unavailable. No new service, generic backend framework or gate provider is needed.
2. Add only the native action adapter and preview confirmation presentation under the existing test seam: dispatch Connect/Stop/preview/confirm/close to the existing controller, separate from packet CoreClient. Test exact selected profile and cancellation. Keep the production path disabled; fixture gates are not production gates.
3. After separately accepted admission/cleanup contracts, implement the production caller, reviewed config validation/digest source and gate authority input. This is a later scope, not part of slice 1 or 2. Do not reinterpret the legacy `managedHostsCompatibleAndIdle` field as a requirement to install a managed host.

Enabling remains gated on exact approved GUI/Core artifacts, installation/admission authority, protection and recovery evidence and supported capability contract. Lab success is unnecessary for slices 1–2 and insufficient by itself for slice 3. No dependency pins or packaging changes are proposed here.

## UI flows and offline acceptance

Use `tests/NativeIKEv2SessionTest.h` Hooks/Harness and `tests/NativeIKEv2TransportTest.h` wire fixtures, plus `tests/ConnectionManagementTest.h` MainWindow fixtures. These are proposed tests, not results of this planning task.

- Foreground: selected native profile remains unavailable in production. Fixture Connect progresses through launch/verification/connecting to authoritative connected. Process creation and hello alone must never display Connected. Repeated clicks cause one activation; no packet start, SSH prompt or statistics polling occurs.
- Stop: while launching or awaiting hello, Stop suppresses activation and queues a single stop. During connection it requests stop; `cleanup_pending`, retained protection or recovery-required stays visibly unresolved and prevents another connection. Do not display Disconnected as proof of safe cleanup.
- Provisioning: preview is distinct from connection. Show profile, server and proposed action; require explicit confirmation of that exact preview. Changed/stale token, changed selection, duplicate confirmation and mismatched action must not apply. Cancel before confirmation sends no apply/remove; close only after preview is terminal. During apply, dismissing UI does not claim cancellation or rollback.
- Errors: map allowlisted codes to plain localized messages; never show raw privileged text, credentials or profile contents. Unknown codes get a generic failure. Exercise approval refusal, consent cancellation, launch failure, malformed response and identity mismatch separately. Current Session safe-code allowlist collapses many Core provisioning codes; agree the useful safe subset before expanding it.
- IPC loss/local cancel: render unknown protection and cleanup, retain the owner block, and never auto-launch a replacement. Read-only rediscovery may recover authoritative status. Dropped events trigger status before restoring certainty; late replies must not reactivate cancelled intent.
- Reconnect: no automatic native connection/reconnection. A later explicit attempt needs fresh authoritative absence/admission and a fresh controller after prior owner closure; existing one-attempt controller must not be recycled. Reopening the UI discovers an existing owner without replaying activation.
- Owner close: terminal status alone is insufficient. Test active session, failed cleanup, unknown WFP/DNS, accepted close without exit, exit without accepted close, lost close reply, and never-activated idle owner. Window/controller destruction releases local resources only. UI must not silently stop or advertise cleanup on exit.
- Regression boundaries: retain existing packet connection/reconnection/stop behavior and `GeoDataSelectionTest.cpp`/`GeoDataDownloadTest.cpp` expectations. Preserve native serializer precedence and credential hiding from Desktop base; extend `ProfileVlessTest.cpp` coverage only where the adapter touches selection/serialization. No native credential collection, packet fallback or fabricated statistics.

## Questions reserved for later Core/contract work

1. What accepted consumer-B authority supplies fresh admission/absence to Desktop, replacing assumptions behind the four scaffold gates? No new authority mechanism is selected here.
2. What exact final revision/artifact pair and capability semantics authorize protected activation? Current working flags and safe-bootstrap refusal cannot satisfy that requirement.
3. What evidence permits MainWindow to release its owner block after discovery-only attachment, where this transport may not own a launched-process handle? Keep unknown blocked until answered.
4. Which provisioning failure codes should survive the Desktop allowlist, and what user action is valid after incomplete rollback/manual-review errors? Do not treat provisioning terminality as successful recovery.

## Validation and handoff

Read-only source mapping and Git/T3 identity checks completed. No product code, parent/Core paths, lab state or package metadata changed. Documentation-only diff reviewed for scope and whitespace. Compilation, runtime tests, performance profiling and four-target packaging are not applicable to this plan-only change; none are claimed. Release readiness remains unproven. Next bounded stage proposed: slice 1 only, after explicit authorization.


## UI-only implementation evidence (2026-11-04)

Implemented native-only unavailable/status text in MainWindow, a hidden-for-packet
Windows-profile action group in the existing editor, and a private local-consent
dialog exercised by fixtures. Production buttons remain disabled. No new runtime
binding, native commands, launch gates, Session/Transport changes, package changes
or shared style changes were made. English/Russian strings are included.

Real Qt captures are in ignored `build/ui-evidence/final-*`: Windows platform,
English/Russian, light/dark, scale factors 1 and 1.5, seven supplied connection
states, minimum 620x460 editor and create/remove/unchanged/invalid confirmations.
Inspected representative Russian light/dark captures and the 1.5 editor: controls
fit and supplied markup remains literal. These are synthetic UI states, not native
VPN evidence. Screenshot dimensions 620x460 and 930x690 reflect those editor scale
runs. The unchanged packet-page offscreen baseline and after image had identical
SHA-256 A6C24BE37354FD8B09BC6007EE4CE0D6F802570E8B68DD281A6406F2E80DB305
before correcting the capture fixture's missing dark palette initialization.
A complete original editor pixel baseline was not captured; packet-editor absence
of the new group is asserted separately. Do not claim exhaustive visual parity.

Review: native selection does not alter packet status/statistics; existing owner
blocking and stale-callback protection remain. Config serialization and credential
reveal code are unchanged. Confirmation returns local consent only and cannot
provision. Unknown/invalid previews have no Accept action. Tests retain existing
packet, serializer, GeoData and native-controller coverage. A build overlap caused
an initial stale-object link failure; a final clean application build removes that
ambiguity. A test invocation during clean build found removed executables and was
not a test failure in product code; fixtures are rebuilt before the final run.

Validation is local Windows amd64 only. Linux and ARM64 runtime/compile matrix,
release packaging, installation, live networking and performance profiling were
not run for this bounded UI stage. No performance-sensitive operation was added.
Later adapter/admission/recovery questions remain deferred. No commit/push/PR.

Final Windows amd64 Release result: clean bimeron-desktop build passed; rebuilt developer-mode-test passed (7.26 s), profile-vless-test (2.52 s), geodata-selection-test (0.10 s), geodata-download-test (4.20 s) passed. Translation JSON parsing and git diff --check passed. Optional external Core validation was not supplied; this is offline Desktop coverage.


## User-requested Raw editor simplification

The user explicitly superseded native Raw concealment for this UI: show the source
text immediately on its tab and remove Show/Hide and Validate buttons. Keep the
existing validation on switching edited Raw to visual fields and the caller
validation before persistence. Raw-only preservation and unrelated credential
fields are unchanged. Tests now assert visible Raw and validation through tab
navigation, including failed validation retaining the draft.

## Next proposed stage: IKEv2 authentication UI

Planning only, requested after the user reviewed the native editor. No credential
implementation, Core change or native operation is authorized by this section.
This supersedes the earlier suggestion to treat saved Windows credentials as an
authentication method: reuse/storage and the authentication method are separate
choices. A client certificate is also distinct from server certificate validation.

### Current evidence

Core working source `internal/systemvpn/ras_windows.go`, `validateEntry`, accepts
only IKEv2 with EAP method 26; `dial` obtains saved Windows credentials and saved
EAP identity without interactive auth UI. Desktop Session/Transport expose no
credential submission, certificate selection or authentication-challenge API.
There is no current support contract for password prompts, EAP-TLS or smart cards.
Neither nested IKEv2 support nor successful lab provisioning proves those APIs.
Do not modify those restrictions as part of UI construction.

### Product interaction to prepare

Use the existing native General tab and QFormLayout. Add a native-only
“Authentication” section below the existing identity/name fields and before the
Windows-profile operations. Keep all existing field order and shared styles.
Use a compact method combo and conditional rows, not a separate wizard or changes
to Add key, packet profiles, Raw layout, or global navigation.

Methods and fields:

1. **Username and password.** Credential source: “Use saved Windows credentials”
   or “Enter credentials”. Saved source shows no recovered username/password and
   never claims credentials were found without an authoritative response. Enter
   source shows Username, masked Password, “Ask at connection” (default on), and
   “Remember in Windows” (default off). Asking later disables the editor password
   field and clears its draft. A username is optional only when the later prompt
   or saved identity supplies it; an empty password is never implicitly anonymous
   authentication. Remember is explicit consent to later Windows storage, not a
   promise made by the UI-only implementation.
2. **Client certificate.** Show a read-only selection summary and “Choose
   certificate…”. Future selection lists eligible identities with subject, issuer
   and expiry; it returns an opaque reference, never exports the private key.
   Client authentication must not be confused with trusting the VPN server.
3. **Smart card.** Show card/certificate selection summary and “Choose…”. PIN is
   not a profile field. The intended connection flow delegates any PIN interaction
   to the supported Windows/provider UI. No generic PIN textbox, PIN retention,
   arbitrary certificate import or invented fallback is included.

The method is chosen according to server support. Password and smart card are
alternatives, not optional switches that can be combined arbitrarily. Existing
server trust/name checks remain unchanged for every method.

### Exact UI-only behavior for the next low-effort implementation

- Create a small native-only widget group inside `ProfileEditorDialog.cpp`, with
  narrowly scoped draft state and stable object names for tests. Do not reuse
  packet `username_`/`password_` or their serializer. No general backend framework.
- The method/source selectors may switch presentation so the user can inspect
  each proposed form. Username/password input, remember controls and OS chooser
  actions remain disabled until a supported backend exists. Use a visible short
  explanation: “Authentication setup is not available in this build.” Do not
  accept real secrets into a form that cannot apply them.
- Default presentation for current native profiles is Username/password with
  saved Windows credentials, labeled as the current supported path, not detected
  credentials. Do not infer a certificate or saved credential from WindowsProfile
  or ServerName. Imported unknown authentication settings remain Raw-preserved.
- If the user selects an unimplemented method/source, clearly mark that selection
  as a preview and disable Save with “This authentication choice cannot be saved
  yet.” Returning to the original source restores normal existing Save behavior.
  Do not silently drop an apparently saved choice, persist an invented config key,
  or globally prevent editing existing supported native documents.
- Keep native-only controls hidden without space in every packet editor and after
  Raw reclassification. Clear the auth preview on profile identity/reclassification
  changes. The existing visible Raw source contains only the document; these
  controls never append credentials, PINs or new auth settings to it.
- Preserve minimum editor geometry 620x460. If the expanded group does not fit,
  use a scroll area only around the native authentication group, with Save/Cancel
  always visible; do not enlarge the shared dialog or restyle packet forms.
- English source strings and Russian translations use existing `tr()` patterns.
  No Show/Hide/Validate buttons are reintroduced. Ordinary method/source selection
  controls which rows are visible.

### Later runtime flows (not implemented by this stage)

Password connection: validate profile/admission → identify missing credentials →
prompt once → submit through the accepted private mechanism → connecting/result.
Cancel ends that attempt without retries or saving. Authentication failure may
offer an explicit retry after authoritative terminal handling; no password retry
loop. A save failure is not reported as successful remembered authentication.

Certificate/card connection: obtain eligible identity → obtain required provider
interaction → connect/result. Missing card, no eligible certificate, PIN cancellation,
rejected identity and removed card have distinct bounded presentation. Do not
invent retry limits or implement PIN validation in Desktop. IPC loss/late auth
responses stay bound to the original attempt and must not start a replacement.

Storage contract: no password/PIN in `.conf`, QSettings, logs, process arguments,
preview summaries or screenshots. “Remember” requires a supported Windows-backed
storage contract and error reporting. Without remember, desired credentials are
attempt-scoped; feasibility must be confirmed with Core before claiming support.
Clear widget drafts on cancellation, method/profile changes and dialog destruction;
do not claim QString clearing provides guaranteed memory zeroization.

### Focused acceptance and implementation order

1. Add the native-only method/source group and EN/RU strings. Capture actual Qt
   forms for all three methods and both password sources; no HTML substitutes.
2. Test row visibility, disabled secret/chooser controls, Save blocking for preview
   choices and restoration of unchanged-profile Save. Prove no new native request,
   process, OS enumeration or credential-store access from any control.
3. Test profile/Raw reclassification, Cancel, keyboard focus and unchanged document
   round-trip; confirm packet username/password fields and serializer remain intact.
   Source inspection and test assertions must show no auth preview persistence.
4. Inspect actual light/dark EN/RU screenshots at normal/minimum geometry and
   scale factors 1/1.5. Run existing profile and connection suites plus translation
   and whitespace checks. No package, dependency, Core or lab edits. Benchmarking
   is inapplicable to this bounded form change.
5. Stop with reviewable UI and evidence. No runtime work, commits or PR actions
   automatically follow. The next implementation can use low effort under this
   fixed scope; contract changes return to planning.

### Questions that gate runtime, not UI construction

- Which supported API prepares/supplies EAP-MSCHAPv2 credentials and identity in
  the correct interactive user context? Can it support attempt-only credentials,
  or does it require Windows persistence? Do not offer working “do not remember”
  semantics until answered.
- What request/response and cancellation contract binds credential prompts to
  the exact profile, server and attempt without placing secrets in ordinary IPC
  diagnostics? No wire schema is selected here.
- Which certificate authentication method and Windows/provider interaction are
  actually supported by Core and the server? What authoritative capability and
  eligibility result enables certificate/card selection? Current EAP-26 validation
  must not be relaxed merely to populate the UI.
- What nonsecret settings, if any, belong in the reviewed config schema for
  method/identity/storage preferences? Existing profiles must remain compatible.

These questions require a later focused Core contract decision. No lab access or
new workers are needed to construct or review the proposed UI.


### Authentication UI implementation (user authorized)

Added a native-only Authentication group with method/source presentation,
masked disabled password input, disabled Windows/certificate actions and explicit
preview-only Save blocking. Returning to the saved-Windows source restores Save.
Identity edits and representation refresh reset the preview. No auth keys or
secrets are serialized; packet controls are separate and unchanged. The group
scrolls locally without increasing the shared minimum editor size. No Core,
Session/Transport, package, dependency or lab changes.

Extended existing profile tests for method/source switching, Save restoration,
masked/disabled secret inputs and unchanged config bytes. Connection capture
fixtures assert packet auth-group invisibility and capture every method/source.
Windows Qt screenshots use EN/RU, light/dark and scale factors 1/1.5; generated
files stay in build/ui-evidence/auth-editor-*. Linux/ARM64 and actual authentication
remain untested. No performance-sensitive change; profiling is not applicable.

Geometry correction before continuing: Qt's native General form cannot fit all existing fields plus authentication and Windows actions at 620x460. An ignored-size authentication scroll collapses completely. Use scrolling for the native General page instead, with normal-height fields and footer outside it. Non-native documents retain their existing page construction. This supersedes the authentication-group-only scrolling choice.

Final authentication UI checks: Windows amd64 Release application/fixture builds passed; profile-vless-test passed (4.45 s), developer-mode-test passed (7.46 s). Real Qt captures in build/ui-evidence/auth-reviewed-* supersede the collapsed intermediate auth-final captures. Minimum page scrolling verified visually; credentials remain disabled, no runtime auth operations. Translation JSON and whitespace checks passed. No commit or push.


User visual correction: remove fixed-height/nested auth scrolling and inherited viewport background; keep native content top-aligned on the dialog base. Place compact Windows-profile operations adjacent to the profile/server fields. Allow local username/password/ask/remember draft interaction; choices still cannot be saved or submitted. Real OS chooser/provisioning remains disabled. Clear local drafts on method/source/identity changes; no serialization. This supersedes disabled secret inputs for UI-only interaction.

Compact layout verified with actual Qt light/dark minimum-size captures in build/ui-evidence/auth-compact-ru-*. Background now matches dialog; Windows actions follow server fields; one page scrollbar appears only when needed. Windows amd64 build and profile-vless-test (3.96 s), developer-mode-test (7.61 s) passed. No shared styles/packet handlers changed. User explicitly retained scrolling and existing window size.
