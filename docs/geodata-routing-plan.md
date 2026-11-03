# GeoIP / GeoSite routing implementation plan

Tracking: https://github.com/bimeron/bimeron-desktop/issues/18.

Status: implementation authorized, 2026-11-01, at low reasoning effort. Core changes and low-effort subagents are authorized. No release, push or PR is authorized.

The explicit download/shared-selection increment is authorized by
[the download plan](geodata-download-plan.md), which supersedes earlier local-only
and per-profile selection wording. Core remains local-only; its bridge and active
endpoint capabilities are still required. No additional Core edits or release work
are authorized for this increment.

## Scope and evidence

Implement the approved Routing tab with lossless profile editing, local dataset management and capability-aware validation. Follow AGENTS.md and the smallest-change approach; reuse Qt Widgets and existing IPC/CLI infrastructure. Update this specification before implementing any discovered scope change.

Core dependency: https://github.com/bimeron/bimeron-core/issues/67, local branch `feat/67-geodata-routing`, clean revision `0b9158edccc2d188a4cb889af92678b46cbf9f85` (not pushed). Read and use that revision's `docs/GEODATA.MD`, `docs/GUI_GEODATA_ROUTING.MD`, `docs/GUI_INTEGRATION.MD`, `docs/adr/0030-explicit-geodata-host-bindings.md`, and `core/config_document.go`. Local source is authoritative; remote fixed-revision links may be unavailable until Core publication.

Approved visual reference, inspected with view_image:
`C:/Users/PC/.t3/userdata/attachments/f4d1de94-31ef-40d5-803c-51929bab1fcc-a40d8166-9a32-4de5-ad78-b467d0268a70.png`.
The attachment is an external local reference, not a tracked asset; its complete functional layout is specified below so this plan remains useful without that path.

### Actual desktop integration

- `src/ProfileEditorDialog.{h,cpp}`: QTabWidget, existing Routing page, `defaultRoute_` (HiDpiComboBox), `ignoreLocalNetwork_` (HiDpiCheckBox), four-column QTableWidget `rules_`, Add/Remove/Move up/Move down buttons, QPlainTextEdit `raw_`, Save/Cancel. The current Rule has a boolean domain discriminator; it cannot represent GeoIP/GeoSite. `loadText`, `buildText`, `syncRaw`, `applyRaw`, `assessRepresentation` currently parse/rebuild a restricted model and protect unsupported content with Raw-only behavior. Do not merely extend its recognized-field whitelist and thereby enable destructive reconstruction.
- `src/MainWindow.cpp`: `editProfile` validates a protected temporary draft in the profile directory, then writes via ProfileStore. `validateConfig` runs the bundled CLI (system mode) or selected Development executable with `validate <absolute-profile-path>`; errors use safeCoreDiagnostic. The connection path also performs asynchronous CLI validation before IPC start. Extend both paths, not only the editor callback.
- `src/CoreClient.{h,cpp}`: existing protected named-pipe/Unix-socket IPC; MainWindow consumes `hello.capabilities`. Start passes an absolute .conf path; the service reads files. There are no new GeoData RPCs.
- `src/DevelopmentCore.{h,cpp}` and `docs/developer-mode.md`: session-only selection of a local Core executable, with an isolated daemon and lifecycle safeguards. Later local testing can select `C:/Users/PC/Documents/dev/bimeron/bimeron-core/dist/windows-amd64/bimeron.exe`; do not copy it into the desktop payload or start/switch a daemon during planning.
- Read-only CLI probes of that executable returned `0.3.0-beta.2-dev`, revision `0b9158edccc2d188a4cb889af92678b46cbf9f85`, modified=false, windows/amd64; `capabilities` returned explicit geoip_split=true, geosite_split=true, geodata_schema=v2ray-protobuf. These are CLI build facts, not installed-service verification.
- `cmake/core-release.cmake` pins v0.3.0-beta.2 candidate archives; build documentation identifies older candidate revision `66d84d8f4dcfc6ada625eb11894b19feea51b7ba`. A version label or bundle pin does not establish GeoData support. Record CLI and running endpoint identities separately; invalidate support on endpoint/instance changes.
- `ProfileStore::importReferencesSafe` currently checks AccessKey references only; it misses GeoData relative-path relocation. `importConfig` copies profiles into the managed directory. `writeConfig` supplies atomic replacement and profile permissions: reuse it. Current MainWindow import rejects invalid profiles; add a separate non-runnable draft path to retain conflicting imports for correction without adding them as validated runnable profiles.
- Localization uses Qt tr() and `assets/translations/ru.json`; keep serialized IDs independent from labels and follow the existing translator in `src/main.cpp`.

### Bridge gap and dependency gate (first implementation milestone)

Current desktop is C++/Qt, not an embedded Go host. Core `ParseConfigDocument`, Bytes/RuleCount/Rule/RulesJSON/SetRuleEnabled/SetRule/MoveRule, `GeoDataGroups`, and `ValidateConfigDataWithGeoData` exist in Go. Android wrappers and Apple C ABI are not a Windows/Linux desktop binding. Current Core CLI dispatch has no document editing or group-enumeration command. `validate <path>` already validates file-based active GeoData configuration but provides generic CLI diagnostics, not the typed document/group bridge required by the UI.

Preferred next boundary: coordinate a small, non-networking Core-owned CLI/helper adapter around these existing Go APIs, returning bounded versioned JSON and exact draft bytes. This is a proposed dependency, NOT an existing command or RPC. Keep service control on existing IPC and runnable validation on existing CLI. Do not invent calls such as RPC geodata_groups or reuse the Apple runtime library on Windows.

Before UI implementation, agree ownership and executable distribution with Core maintainers. Adapter operations must cover document inspect/edit/toggle/move plus explicit add/delete and section-field patching (ConfigDocument currently has no AddRule/DeleteRule or general GeoData/Default setters), dataset enumeration and typed validation. Use core APIs directly where available; extend their contract deliberately for missing mutations rather than reserialize the whole profile. Define protocol version, limits, cancellation, stdin/stdout framing and protected resource access. Never put profile secrets on command lines or logs. If an additional helper is required, its build/distribution is a separate explicit dependency; a pure CLI extension avoids another packaged executable.

Exit gate: working Windows/Linux amd64/arm64 adapter fixtures with exact Core identity and no networking; failure is visible and retains Raw drafts. Until this is delivered, preserve raw editing and disable unsupported GeoData visual editing with a visible explanation. Do not ship a separate C++ protobuf decoder or approximate lossless parser as a shortcut. The user subsequently authorized changes to the sibling Core checkout. Use a Core-owned CLI adapter, not a new RPC or packaged helper.

## Specification and acceptance criteria

### Profile model and lossless edits

The original bytes plus document-block identity are authoritative. Keep executable policy separate from editor state. A table row represents one whole [Rule] block, including disabled state and attached comments. Preserve BOM, LF/CRLF, missing final newline, unknown sections/fields, comments and block order across table/source/table and unrelated General/Network edits. Unknown/malformed rows remain visible or Raw-only with a reason; refuse unsafe mutations. Opening/canceling and unedited saves preserve exact bytes.

```ini
[GeoData]
GeoIPFile=data/geoip.dat
GeoSiteFile=data/geosite.dat
[Routing]
Default=direct
[Rule]
GeoIP=ru,by
Action=direct
[Rule]
GeoSite=github,youtube
Action=proxy
# [Rule]
# GeoSite=category-ads-all
# Action=block
```

Exactly one CIDR/Domain/GeoIP/GeoSite matcher and Action per active block. Comma-separated GeoIP/GeoSite IDs mean OR; rules share top-to-bottom first-match order. Actions: Direct/direct, Via VPN/proxy, Block/block. IP entry may normalize a literal address to /32 or /128 using the existing helper; document this edited-value normalization.

Never serialize Enabled. Disable by commenting every nonblank block line with one outer `#` layer, including existing comments; enable removes exactly that layer. Only structurally valid commented candidates become disabled rows. Malformed candidates stay ordinary comments. Unknown fields block unsafe edit/enabling; unchanged disabled rules do not block unrelated supported edits. Map typed error rule numbers (one-based ACTIVE rules) back to document/table IDs including disabled rows.

### Existing local-network checkbox

Current `localNetworkCidrs()` is `10.0.0.0/8, 172.16.0.0/12, 192.168.0.0/16, 169.254.0.0/16, fc00::/7, fe80::/10`. Current load recognizes any enabled direct CIDR block with that normalized set and hides it in the checkbox; build emits one such block before all table rows. This can change precedence or collapse duplicates.

Keep the existing ranges and direct CIDR semantics; introduce no .conf option. The checkbox is a convenience for an explicit rule, not an unconditional OS bypass. For new opt-in insert that block first with visible precedence explanation. For imported profiles retain all blocks and their positions; bind the checkbox only to an unambiguous leading exact-match block, leave later/duplicate matches as visible table rows and explain mixed state. Toggling must be explicit and must not remove user-authored blocks silently. Retain comments when disabling the bound block. Confirm this desktop policy against the separate cross-platform local-network contract before changing shared behavior; Core GUI contract explicitly leaves that contract open. Test first-match interaction with earlier block/proxy/GeoData rules; never automatically hoist imported rules.

### Components matching the approved sketch

Extend ProfileEditorDialog rather than adding a second editor:

1. Top: default action plus local-network checkbox using existing HiDpi controls.
2. Four columns: On / Type / Destination / Action. Type choices IP/CIDR, Domain, Countries (GeoIP), Sites (GeoSite). Retain QTableWidget for now; use explicit matcher IDs instead of bool domain. Move block identity, not cell strings.
3. IP/Domain use QLineEdit. GeoData uses a focused popup built from Qt search QLineEdit, checkable list/model and summary field. Search matches localized names AND stable IDs; filter does not clear hidden selections. Countries include Russia (ru), Belarus (by); sites include GitHub (github), YouTube (youtube), Ads (category-ads-all). Populate only IDs enumerated from the selected validated file; localized name map has ID fallback, never a hardcoded available-group list. Store IDs in item data, serialize IDs only. Keep selected missing IDs visible with errors.
4. Below table: explicit first-match explanation, existing Add/Remove/Up/Down actions, textual dataset status with an optional colored indicator, and Manage databases button. Status distinguishes selected/validated/available from loaded in the active session; CLI inspection alone must not claim loaded runtime data.
5. Preserve Save/Cancel and existing raw-source tab. Save/Apply and Start require valid runnable content; keep invalid source/import edits as separate drafts until corrected. Cancel never writes the draft to the original.
6. Keyboard: logical Tab/Shift+Tab order, arrows in table/list, Space toggles, Enter commits selection without accidentally saving dialog, Escape closes popup then dialog, accessible labels/checked state and visible focus. Provide keyboard actions for add/delete/reorder with discoverable shortcuts; restore selected row after reorder. No color-only status or tooltip-only restriction reasons. Verify screen-reader names, high contrast, English/Russian labels, dark/light themes, minimum window size and 100/150/200% scaling on Windows; Linux native themes too.

### Local database management

One local V2Ray protobuf file per type; no auto-download or update service. Manage view: paths, browse/change/clear, schema validation result, available searchable groups, errors and restart-required status. Extension alone proves nothing. Enumerate/validate through shared Core decoder; honor its 64 MiB/file and resource budgets. Do file work asynchronously with bounded responses/cancellation and stale-result suppression. Cache catalog by resource identity/content changes, not just localized display text.

Resolve relative paths against final .conf directory. The existing same-directory temporary validation pattern preserves this base. Show absolute resolved location and explain host-local paths. GUI readability does not prove LocalSystem/root service readability; start remains authoritative and must surface access failures. No automatic ACL widening, elevation or protection changes. Handle Windows mapped drives/EFS/ACLs and Linux permissions/symlinks deliberately.

Import must detect GeoData relocation before publishing a runnable profile. Offer a correction draft retaining source bytes/base path and missing-dependency details. Initially refuse unsafe relocation; portable profile+dataset export/import must either explicitly copy chosen public datasets with collision/overwrite checks and preserved relative layout or report that referenced files must be transferred separately. Never silently rewrite or copy dependencies. Disabled-only rules must remain editable without installed files.

Datasets are immutable for a session, including reconnects. File/path/content replacement marks restart required; it does not mutate the active session. Restart uses existing stop/wait-for-cleanup/start sequencing only after explicit user action. Do not promise automatic runtime file-change detection beyond what the UI actually verifies.

### Validation and capability gating

Use explicit active-endpoint geoip_split/geosite_split and recognized geodata_schema=v2ray-protobuf, not version inference, ip_split or domain_split. Missing/false flags or unknown schema prevent new/enabled GeoData rules with a visible reason. Keep imported values. Also verify selected validator/adapter capability and identity; newer CLI cannot certify an older running service. Reevaluate on backend/instance changes and before Save/Start.

- IKEv2: no split routing; retain unsupported imported content and block Apply/Start. This task does not implement desktop IKEv2.
- Packet VPN KillSwitch=true permits only CIDR: active Domain/GeoIP/GeoSite conflict. Changing either side shows a conflict and requires explicit cancel or disable-affected-rules choice. Never silently turn protection off or delete rules. Proxy mode disables packet kill-switch controls without silently rewriting imported conflicts.
- Active unknown IDs, invalid identifiers, missing/unreadable/invalid files block Apply/Start. Disabled rules need no databases; validate on enable. IPv6=disable remains dominant.
- GeoSite VPN also needs domain-routing support. Explain DNS metadata dependence, TTL/shared-IP limits, DoH/DoT, old caches and bare IP limitations. SOCKS GeoIP uses only known destination IPs; never resolve domains locally implicitly. Unknown IP falls through to later rules/Default.
- Use typed details (code/kind/group/rule) via the agreed adapter; generic validator/host errors need no fake field mapping. Do not parse prose into stable error codes. Retain drafts on all failures and redact secrets through existing diagnostics.

## Ordered implementation milestones (manual continuation)

1. Resolve and document bridge ownership/protocol and local-network precedence contract; write bridge contract fixtures before UI changes. Confirm exact CLI and endpoint identities. Stop dependent work if the bridge is unavailable.
2. Introduce document-backed routing mutations and non-runnable draft retention; implement and test add/delete/section patching with agreed Core adapter. Replace restricted routing reconstruction while preserving non-routing content; extend import reference safety.
3. Extend existing Routing page to four matcher types and popup selection; add group labels/translations, first-match text and database-management view. Keep availability/error state separate from saved IDs and active-session state.
4. Centralize restrictions for editor, imports/raw drafts and connection entry points; connect asynchronous enumeration/validation and stale-result handling. Reuse atomic save, selected Development CLI and authoritative IPC lifecycle.
5. Run focused tests below, profile only new expensive operations, then relevant CI equivalents. Review diff for data loss, ordering, permission escalation, secret exposure, unsupported capability inference and unnecessary dependencies. Update user/integration docs with evidence and unresolved limits.
6. Only under a later explicit release request: obtain reviewed published Core artifacts, update pins/hashes and canonical build documentation together, then perform full package/runtime acceptance. No release is part of this task.

## Meaningful tests and verification

- Extend `tests/ProfileVlessTest.cpp` with mixed-type order, table/raw/table exact-byte fixtures (BOM, CRLF, missing newline, comments/unknown sections), repeated disable/enable, nested comments, malformed commented examples, unknown fields, non-routing edits, add/delete/move and cancel/failed-save preservation. Differential fixtures against Core ConfigDocument; test active-rule diagnostic index mapping after disabled rows.
- Precedence regression: imported local-network block after GeoSite/Block, duplicate local blocks, disabled block, checkbox changes and mixed CIDR/Domain/GeoIP/GeoSite first-match order. Runtime policy behavior belongs to Core-backed fixtures, not duplicated C++ matchers.
- Temporary-directory imports: profile-relative files, relocation refusal and retained correction draft, explicit portable transfer/missing dependencies, collisions, invalid files/groups, disabled-only rules, no silent path rebinding. Protect original bytes on failed import/validation/write.
- Dataset fixtures: valid small protobufs, malformed/oversized/empty files, unknown groups/selectors, changed files and restart indication, cancellation/stale completion, service-vs-GUI access, decoder limit errors. No real production credentials or databases in Git.
- All gating combinations including both directions of kill-switch conflict, raw/import bypass attempts, old/missing flags/schema, newer CLI/older service and reverse, endpoint changes, IKEv2, IPv6, proxy unknown-IP and VPN DNS limitations. Ensure no implicit DNS lookup.
- Qt UI checks for ID-preserving localized selection, search across hidden selected items, missing selected IDs, table selection/reorder, keyboard/popup semantics, accessible names, visible reasons and status text. Extend existing Qt tests; no new test framework needed. Extend DeveloperModeTest/ConnectionManagementTest fixtures for blocked Start and endpoint changes where appropriate.
- Run the existing full CTest suite (core-update-test, profile-vless-test, developer-mode-test) after focused fixtures pass. Exact geodata-capable Core/adapter contract tests must name executable revision; existing BIMERON_PRESERVATION_CORE is a preservation hook, not proof of a new bridge. Document any new explicit optional test hook when introduced.
- CI equivalents from `.github/workflows/ci.yml`: configure/build all Windows x64/ARM64 and Linux amd64/arm64 configurations; CTest native Windows x64 and both native Linux runners, Windows ARM64 compile-only in current CI. Use existing Qt/MSVC versions and offscreen Linux tests. Native ARM64 UI/runtime evidence remains separate.
- Measure catalog decode/search latency, memory and UI responsiveness with representative large datasets; profile only if these show a bottleneck. Avoid per-keystroke file decode or one heavyweight widget per group. Record dataset size/group count, machine and cold/warm timings; no invented benchmark result or arbitrary unsupported SLA.

## Platform, packaging and documentation boundaries

Supported desktop packages are Windows MSI and Debian DEB for x64/amd64 and ARM64. Qt source portability does not establish macOS product/package support; macOS spec is separate. No new setup wrapper, AppImage, WinGet or APT repository. No dependency changes, binary copies, downloads or pin changes now. Later bridge distribution and Core update compatibility must work on all four targets. Keep build/ compiler state, stage/ exact payload, dist/ distributables; no generated binaries, databases, secrets or downloaded tools in Git.

Read/update `docs/build-and-packaging.md` in the same change whenever the agreed bridge alters build/dependencies/payload flow. Update `docs/profile-preservation.md`, `docs/core-desktop-integration-contract.md`, `docs/developer-mode.md` and README links where the implementation actually changes their behavior. Convert this plan to verified feature documentation without claiming unrun acceptance checks. Use Conventional Commits for later commits.

Before an explicitly requested release, complete all four supported test/build/package targets, inspect payload/metadata and matching Core hashes, and test installed service dataset access and lifecycle on Windows/Linux. Record platform runtime gaps honestly, including ARM64; compilation alone is not runtime acceptance.

## Planning handoff

Planning checks: repository AGENTS.md and canonical packaging guide read; approved image inspected; Core source/contracts/API and CLI dispatch inspected; local CLI version/capabilities queried without network/session changes; all existing desktop issues searched for duplicates; clean worktree verified before feature-branch creation. No product implementation, live network tests, builds/packages, benchmarks or profiling were performed: not applicable to this documentation-only stage.

Outstanding dependencies: desktop-callable Core document/catalog adapter and missing mutation API; agreement on checkbox precedence contract; published/reviewed Core payloads for all architectures; actual installed-endpoint capability/identity and service file-access evidence. These are implementation gates, not assumed RPCs or existing guarantees.


## Authorized implementation decisions

The user authorized autonomous implementation and Core changes after planning.
The chosen adapter is a Core CLI extension, preserving the existing IPC surface
and package shape. Two low-effort implementation agents own the Core adapter and
Qt editor separately; focused host/import checks run independently. No push,
PR, package release or installed-service modification is authorized.

Bridge request: `bimeron document` receives one JSON request over stdin with
`version: 1`, `profile` text and ordered `operations`. Operations are `set_rule`
(index, matcher, value, action), `toggle` (index, enabled), `move` (index, to),
`add` (index, matcher, value, action, enabled), `delete` (index), and `set_field`
(section, key, value). Response contains version, exact profile text and rules;
errors are explicit. Core owns framing/size/structural validation. Dataset
catalog uses `bimeron geodata groups <geoip|geosite> <absolute-file>` with JSON
version/groups/error/details. This is a new CLI surface, not an existing RPC.
See the implementation contract in Core `docs/DESKTOP_DOCUMENT_API.MD` when
available. Document text is never passed on process arguments.

The desktop local-network convenience preserves imported rule order. Only an
unambiguous leading exact-range block can bind to the checkbox. Newly requested
bypass is an explicit first rule with a visible precedence explanation; later
or duplicate imported local rules remain visible. This is a desktop convenience
policy, not a new shared Core field or an implicit OS routing exemption.


## Planned explicit database downloads

On 2026-11-02 the user approved planning of a compact database dialog with
button-initiated downloads from Loyalsoldier and retained manual file selection.
See [download dialog implementation plan](geodata-download-plan.md). This is a
future desktop-only scope extension to the original local-files-only requirement;
no automatic downloads or Core changes are implemented by that planning step.


The download-manager plan was clarified to use one application-wide selection per
database kind, reused by all Desktop-managed profiles using GeoIP/GeoSite. The
per-profile Core format remains unchanged; validated Desktop synchronization writes
references to the shared files. See the shared-selection/transaction contract in
[the download plan](geodata-download-plan.md). This remains planned work.
