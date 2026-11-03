# Disabled IKEv2 GUI integration plan

Planning snapshot: 2026-11-04. The user subsequently authorized the UI-only stage below. Native enablement remains outside scope.

## Current next stage: native-only UI, before runtime integration

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
