# GeoData download dialog implementation plan

Status: implemented locally at low reasoning effort, 2026-11-02; parent review pending. Continue in
the shared dirty checkout on eat/geodata-routing, preserving existing changes.
No commits, pushes, PRs, release packages, installed-service changes or Core edits.

Related work: [Desktop #18](https://github.com/bimeron/bimeron-desktop/issues/18),
[existing routing plan](geodata-routing-plan.md), [routing documentation](geodata-routing.md).

## Approved scope and visual reference

Use one application-wide country database and one application-wide site database.
The user clarified that selecting/downloading each database once must serve all
Desktop-managed profiles using GeoIP/GeoSite. There is no per-profile dataset
picker or per-profile override in this increment. Replace the verbose
local-database manager with the compact preview approved in
this conversation. Reference: local `build/mockups/geodata-download.html` (ignored,
not a durable tracked asset). The specification below is authoritative even when
the generated preview is unavailable.

The user explicitly expanded the previous local-files-only scope: offer downloads
from Loyalsoldier by default, initiated by a click. No periodic, startup, silent,
background-auto-update or connect-triggered download. Preserve manual file choice.
This changes the old no-download requirement only for this explicit desktop action;
Core itself still performs no downloads. No new RPC or Core API is required.

Use a normal QDialog with the same native title bar and application icon as the
profile editor. Do not draw HTML-style window chrome, enlarge the title/font, or
make the dialog frameless. On Linux use the platform's ordinary window decoration.

Target initial client size: approximately 480x300 logical pixels, content-driven
within a compact range (up to approximately 340 high for the normal translated
state). Use the application font and existing control height, not a smaller global
font. Match the preview's compact spacing; do not copy its CSS pixels literally
into Qt font sizes. Small Download buttons are content-sized, not full-width.
Normal diagnostics must not force a larger window; long details/errors can expand
in a bounded scrolling details area. Preserve resizing, accessible focus and
normal platform scaling. Never use a fixed-height whole dialog to hide overflow.

Layout and user-facing English strings (add natural Russian translations):

1. Native title: `Country and site databases`.
2. One short line: `Download ready-to-use databases or choose your own files.`
3. Source line with a clickable `Loyalsoldier · GitHub` link.
4. Two rows: `Country database` and `Site database`. Each has a short status on
   the left and compact `Download` plus secondary `Choose file…` actions on the
   right. A ready row can show its filename and `Update` instead of Download.
5. Collapsed `Details` for format, resolved paths, permissions, portability,
   upstream release/hash and limitations. Keep optional catalog inspection and
   manual revalidation here instead of additional full-width buttons.
6. Show a short scope label: `Used by all profiles with country or site rules`.
   Footer: Cancel / Apply. Apply is enabled only for an actual, validated shared
   selection change with no pending request or conflicting active rule. A local
   valid file remains usable even when networking is unavailable.
7. After a successful file change, a short notice: `New databases take effect on
   the next connection.` This means a new Core session, not an ordinary reconnect
   within an existing session; do not restart automatically.

Status vocabulary: `Not downloaded`, `No file selected`, `Downloading…`,
`Checking file…`, `Ready`, `Download failed`, `Invalid file`. Display a short useful
error and a retry action; detailed diagnostics belong in Details. Do not claim
runtime-loaded state or service readability from local validation alone.

Simplify the Routing tab status as part of the same UI pass: one actionable reason
for unavailable country/site rules, not a paragraph of decoder/host terminology.
Retain all enforcement. Put technical explanation under Details/Routing limitations.
Do not invent a published compatible Core update or bypass capability gating.

A first-rule wizard, separate Add Rule dialog and automatic suggestion flow were
explored earlier but are not part of this increment. Keep the existing routing
row and searchable multi-select. This plan implements the final compact manager.

## Source-backed implementation audit

- `ProfileEditorDialog::manageDatabases()` currently creates a 620x320 dialog with
  verbose introductory text, editable paths, Browse/Clear, revalidation, group
  inspection and Apply/Cancel. It copies `datasetPaths_` into a dialog draft, but
  its existing validation/catalog state still belongs to the parent editor.
- `inspectDataset()` already runs selected-Core `geodata groups` asynchronously,
  with a generation counter, 15-second timeout, bounded output, typed decoder
  error code, filesystem watchers and session-change tracking. Refactor the
  validation call minimally so the manager can validate a staged candidate
  without changing parent/profile selection. Do not call parent inspectDataset
  on a not-yet-applied candidate as a shortcut.
- `selectGroups()` already supports ID-backed localized search. Keep IDs and
  selection stable after a database change; report missing active groups and
  block Apply rather than dropping them. Disabled rules need no database.
- `CoreUpdater::fetch()` demonstrates QNetworkAccessManager, timeout and bounded
  replies. It is private, tied to signed executable updates and build-specific
  local feeds; do not call it directly or make GeoData depend on updater trust
  keys. Reuse Qt Network and the small established patterns, not the updater's
  executable installation/elevation logic. Check every redirect hop; its current
  initial-host check plus automatic redirects is insufficient for this feature.
- `ProfileStore::writeConfig()` is an atomic protected PROFILE writer. Preserve
  that profile boundary. Use QSaveFile for dataset storage without treating
  database bytes as config or reusing credential-file ACL logic blindly.
- MainWindow already passes the selected Development CLI or bundled CLI and final
  profile path into ProfileEditorDialog. Keep active endpoint capabilities
  separate from CLI support. Data download cannot establish service compatibility.
- Qt Network is already linked. No new library or downloader executable is needed.

## Download and file lifecycle contract

### Fixed sources and transport

Upstream project: https://github.com/Loyalsoldier/v2ray-rules-dat

User-visible download sources:

- https://github.com/Loyalsoldier/v2ray-rules-dat/releases/latest/download/geoip.dat
- https://github.com/Loyalsoldier/v2ray-rules-dat/releases/latest/download/geosite.dat

At implementation, resolve the latest release once per operation through the
repository's GitHub release metadata and pin the actual tag/asset URLs for BOTH
data and checksum. Do not fetch the data and checksum through independent mutable
`latest` redirects; that can mix two releases. Accept exact asset names only.
Prefer the upstream `.sha256sum` assets, or a documented SHA-256 digest supplied
by the same release metadata. Confirm the current metadata/checksum contract with
upstream documentation before coding. If no usable checksum exists, report an
error and keep manual file selection available; do not silently skip checking.

Use HTTPS with normal certificate verification. Limit metadata/checksums to small
bounds (1 MiB metadata, 4 KiB checksum), datasets to 64 MiB each including streamed
bytes, not merely Content-Length. Use explicit redirect handling with a finite
limit (five hops), and validate scheme/host/port on EVERY hop before requesting.
Allow github.com/api.github.com and the documented GitHub release-asset hosts
actually needed by the source; confirm those hosts during implementation. No
arbitrary mirrors, user URL field, HTTP downgrade, executable download or request
credentials. Never accept TLS errors. GitHub rate-limit/offline/proxy errors must
leave the previous working selection intact. Use the normal Qt/application proxy
configuration; do not add a hidden direct-network bypass or send traffic through
a different endpoint. The explicit download is GUI HTTPS traffic, not Core's
routing evaluator.

Keep networking asynchronous, bounded and cancelable. Show progress when total
size is known and an indeterminate state otherwise. Use a total deadline as well
as inactivity timeout (e.g. 120 seconds total / 30 seconds inactivity). Limit one
active job per kind; no unbounded retries. Requests and callbacks are owned by the
dialog/controller, with generation checks and safe abort/disconnect on close.
Do not log profile contents or unrestricted upstream response bodies.

Checksums provide corruption/release consistency checking, not an independent
publisher signature when fetched from the same source. Do not label them as
signed or reuse Bimeron's executable-update signature claims.

### Validate before publication

Per-kind pipeline:

`Not downloaded -> Downloading -> Checking checksum -> Checking with Core -> Ready`

Failure/cancel returns to the prior ready candidate or no-selection state with an
error; it never overwrites the previous valid dataset. Wrong kind, empty catalog,
malformed/oversized protobuf, unknown active groups and missing CLI bridge cannot
become Ready. Verify the new candidate with the selected Core shared decoder and
validate current active rules against it before Apply. Use the existing protected,
same-profile-directory configuration validation callback for full runnable policy;
never start a session as a validation method.

### Shared application selection and profile synchronization

Persist one shared selection per kind in application settings. Downloading or
choosing a file from any profile opens the same application-wide manager. It is
not copied/downloaded again when another profile uses the feature. Use a single
shared controller/state for jobs, catalogs and current selections; editor dialogs
observe it instead of owning independent selections. Normal startup may validate
saved local files but must not download them automatically.

Core still requires GeoIPFile/GeoSiteFile in each runnable .conf. Do not invent a
Core global config or RPC. The Desktop writes references to the shared files into
its managed profile documents, using the document bridge and atomic writer. The
files themselves remain shared. Profiles without GeoIP/GeoSite rules are untouched.
Only the database kinds used by each profile need a reference.

On Apply, inspect affected Desktop-managed profiles and validate their active
GeoData groups against the candidate shared catalogs before committing the shared
selection. Report incompatible profiles/groups without dropping any values. Commit
the two settings together using the existing settings persistence conventions;
on failure retain the previous selection. Do not eagerly rewrite every profile or
introduce a multi-file transaction/recovery journal for this increment.

Materialize the current shared paths into one managed profile at a time during its
normal Save/import/Start preparation. Patch only the relevant GeoData keys through
the document bridge, validate the resulting profile, then atomically replace it
through ProfileStore before handing its original absolute path to Core. Failed
validation/write leaves that profile unchanged and blocks its Start. This preserves
profile identity and avoids generating a temporary runtime path with a different
profile ID. Previously saved references in inactive files can remain until that
profile is next prepared by Desktop; direct external CLI use of those untouched
files is outside this Desktop shared-selection feature.

New/imported profiles use the current shared selection when prepared. Preserve the
external import source; change only the managed copy. Existing per-profile paths
are superseded by the shared choice during the visible global-selection flow and
normal Desktop Save/Start; explain this once in Details. No per-profile exception
UI is offered yet. If no shared selection exists, show setup rather than silently
adopting one profile's database for all the others.

Before Start, always recheck/synchronize references to the current shared files,
then use the ordinary capability/KillSwitch/host and Core validation path. Never
start an unrelated stale database while the UI claims shared data is in use.
Disabled-only rules do not require databases and must not block unrelated work;
retain their blocks when shared files are absent.

An already running session retains its immutable snapshot. Shared Apply changes
future sessions, including other profiles, without stopping or reloading the
current one. Every new Desktop connection uses the current shared selection.

### Storage and transaction boundaries

Use application-local persistent data (QStandardPaths::AppLocalDataLocation), not
Downloads, the install directory, repository dist/, the current working directory
or a temporary path retained in .conf. Stage into a task-owned temporary file in
that storage area, compute SHA-256 incrementally and publish only verified bytes
using QSaveFile with direct-write fallback disabled.

Store immutable content-addressed files, for example
`geodata/<sha256>/geoip.dat` / `geodata/<sha256>/geosite.dat`. Updating must create a
new path rather than replacing bytes referenced by another profile or active
session. Never edit/delete a manually selected file. Handle directory creation,
permissions, disk-full and failed commit as normal visible errors.

A successful explicit download may remain in the managed cache after Cancel;
Cancel does not change the shared application selection, any editor/profile
references, .conf bytes, active rules or session. Aborted/invalid staging files are removed. Do not implement global cache
GC/deletion in this increment; it requires reference tracking across profiles.
Avoid duplicates by content hash. Explain the cache location in Details.

Apply commits the two shared selections after validation. It is an explicit
application-wide operation, independent of which profile opened the manager.
Canceling that profile editor afterwards does not undo global settings; make the
scope visible before Apply. Unsaved unrelated profile edits are never committed
by this operation. An open editor observes the shared selection and materializes
its references only during its own validated Save. No new .conf fields: use only
existing GeoIPFile/GeoSiteFile. Downloaded managed files use explicit
absolute paths; manual relative paths remain profile-relative. Export/import must
continue to report referenced-file portability requirements. Local GUI access is
not proof that the actual Windows LocalSystem/Linux service can read the files;
do not widen ACLs or elevate automatically.

Keep a running session unchanged. Selecting new bytes/path indicates that a new
session is required. Never stop a session, disable KillSwitch, delete rules or
reset capabilities to make a download usable.

## Ordered implementation steps for low-effort continuation

1. Recheck dirty-tree ownership and source/checksum/redirect contracts; read the
   full canonical build guide before any CMake/source-list changes. Update this
   spec first if actual upstream requirements change. No new branch is necessary
   for this continuation of #18; do not switch away from the dirty feature branch.
2. Implement a small GeoData-specific QObject helper (e.g. GeoDataDownload) owning
   QNetworkAccessManager, bounded jobs, temporary files, checksums and immutable
   publication. Keep its production source URLs fixed and expose a narrow network
   injection seam for local test fixtures. Do not create a general download framework.
3. Move current database selections/catalogs/jobs to shared application-owned
   state. Refactor manager-local candidate validation and stale callbacks. Add
   shared settings and per-profile validated path synchronization in the existing
   Save/import/Start preparation path. Preserve unsaved unrelated editor fields. Model global selection,
   transfer state, local validation and active-session state separately.
4. Replace manager layout and simplify the Routing status, with native title bar,
   normal application icon/font, compact buttons, Details, Cancel/Apply and proper
   tab order. Wire Download/Update and Choose file. Preserve clear/group/revalidate
   access in Details where needed. Add English strings and Russian translations.
5. Add focused transfer/lifecycle/storage tests and native modal geometry tests
   before relying on an offscreen preview. Review the diff for data loss, redirect
   bypass, async use-after-free, incorrect Ready states and unnecessary abstractions.
6. Run relevant checks below, inspect real Qt screenshots, update feature/build
   docs to describe explicit downloads and record evidence. Stop before commit,
   push, PR or release unless separately requested.

If new helper .cpp/.h files are added, include them in BIMERON_DESKTOP_SOURCES and
in the explicit profile-vless-test source list as needed. This is source wiring,
not a new package payload. Update docs/build-and-packaging.md in the same change
if that documented flow/source wiring changes; never add a database to MSI/DEB.

## Meaningful tests and acceptance

Network fixture tests (private local server/injected transport, no public downloads
in CI): successful known bytes/checksum, same-release pinning, redirect-loop and
untrusted-hop refusal, TLS errors, HTTP failures/rate limits, timeout, unknown size,
false Content-Length, >64 MiB streamed output, checksum missing/mismatch, canceled
transfer, dialog destruction, duplicate clicks, per-kind stale completions, retry.
The local test seam must not allow arbitrary production download URLs or leak into
release environment overrides.

Storage/validation tests: invalid/wrong-kind/empty database never replaces a ready
file; disk/rename failure preserves prior state; content hashes deduplicate;
manual files remain unchanged; Cancel preserves global settings and profile bytes;
Apply commits both shared selections; Save/Start synchronizes the selected profile; a
second profile reuses the same files without downloading; new/imported profiles
adopt shared references; non-GeoData profiles remain unchanged; invalid active
groups block shared Apply; read-only/invalid selected profiles block Save/Start;
settings/write failure preserves the previous state; stale references are corrected
before Start; open-editor unrelated edits survive synchronization; profile
Cancel does not undo an explicitly completed global Apply; missing active groups
block Apply without dropping selections; disabled rules remain recoverable;
active-session snapshots stay unchanged; cached files are not overwritten.
Use synthetic protobufs, not downloaded binaries/databases in Git.

UI tests: default Download vs manual alternative, progress/check/error/retry/ready
and Update states, disabled Apply during pending/invalid operations, filename and
source links, Details expansion, keyboard Tab/Shift+Tab/Enter/Escape, accessible
names, status not color-only, English/Russian and translated placeholders.

Geometry: open via the actual ProfileEditorDialog::manageDatabases path with the
application stylesheet and native Windows QPA at 100/150/200%. Wait at least 500 ms
and through async status changes; check compact settled geometry and bounded
Details/progress. Confirm the profile editor stays 680x520 on General/Network/
Routing/Raw tabs. Long labels and errors must scroll/wrap rather than inflate the
window or hide actions. Also inspect Linux native/offscreen layout and light/dark
styles. Do not claim native screen-reader/ARM64 evidence from screenshots.

CI-equivalent checks: focused fixtures, then existing CTest cases on Windows x64
and Linux amd64; build Windows x64/ARM64 and Linux amd64/arm64. CI's native Linux
arm64 CTest and actual ARM64 runtime remain separate when hardware is unavailable;
record that gap. Do not skip a platform as unaffected by shared Qt code. Verify
translation JSON/placeholders, local documentation links and git diff --check.
No release/package matrix during planning or implementation without a release task.

No benchmark is needed for copy/layout alone. During implementation verify bounded
memory and responsive UI on near-limit synthetic files; profile only if measured
transfer/validation/search behavior exposes a problem. Record actual results.

## Documentation and stop point

Update docs/geodata-routing.md, the parent plan and relevant build/testing notes
when implementing: explicit button-initiated downloads supersede their old blanket
no-download statement. Keep local-only Core behavior and prior verification history
accurate. Add implementation results to verification documentation rather than
claiming this planned feature already exists.

Planning audit complete: source-backed existing UI/network/file/validation paths
reviewed and current working tree preserved. No implementation, network downloads,
builds, tests or benchmarks performed for this documentation-only step.
The main dependency remains a selected Core exposing the document/catalog/
structured-validation CLI plus a compatible active endpoint for active GeoData
rules; downloads do not solve or bypass that dependency.


Planning amendment: 2026-11-02, user requested application-wide database selection
shared by every managed profile using the feature. This supersedes any earlier
per-profile manager-selection wording. Implementation is now authorized for low-effort continuation; the historical
planning step did not change product code or profile/settings data.


## Implementation contract confirmation (2026-11-02)

Upstream's [release workflow](https://github.com/Loyalsoldier/v2ray-rules-dat/blob/master/.github/workflows/run.yml)
publishes exact `geoip.dat.sha256sum` and `geosite.dat.sha256sum` assets. The
[release metadata](https://api.github.com/repos/Loyalsoldier/v2ray-rules-dat/releases/latest)
inspection resolved tag `202611012203`; checksum assets contain a 64-digit SHA-256,
two spaces and the exact filename. GitHub delivered assets through
`release-assets.githubusercontent.com`. Production allows HTTPS port 443 on only
`api.github.com`, `github.com`, and `release-assets.githubusercontent.com`; every
redirect is checked. This is a consistency check, not publisher authentication
independent of the upstream release.

Shared Apply validates active groups without rewriting or demanding that unrelated
profile settings are already runnable. Full protected draft validation remains in
Save/import/Start preparation. This makes Apply an application-wide selection
operation independent of the editor that opened it. Catalog jobs/cache and file
invalidation are application-owned, with scoped editor/dialog observers.

See [verification](geodata-routing-verification.md) for the actual checks and
remaining acceptance limits. Historical planning-only statements above describe
the earlier planning pass, not the authorized implementation continuation.


Review clarification: global Apply does not repair unrelated unusable profiles.
Unreadable managed entries are left untouched and must pass normal preparation
before any later Start. Readable profiles without active GeoData fields are not
blocked by unrelated size/encoding errors. A readable active GeoData profile must
still pass its document/group checks; invalid active profiles block Apply. This
keeps shared selection independent of unrelated damaged entries while preserving
fail-closed Save/Start preparation.


Implementation and local checks are complete. The exact build/test/DPI evidence,
review fixes and remaining acceptance limitations are recorded in
[verification](geodata-routing-verification.md#explicit-shared-downloads-continuation--2026-11-02).
No release/package acceptance is claimed.


## P2 Raw Save review correction (2026-11-02)

Before editor acceptance, Save must materialize shared references into its protected
same-profile-directory validation draft, including absent and stale GeoIPFile.
Raw import correction uses the same ordering and preserves the external source.
Acceptance: actual MainWindow editProfile Raw Save succeeds with a valid shared
selection, preserves comments/unknown fields, rejects missing groups while retaining
the draft and original, and Cancel leaves original bytes unchanged. Final profile
publication remains after acceptance and final validation.


## Feedback correction acceptance (2026-11-02)

Before code changes: hide progress text and use an expanding full-row track; verify
painted track alignment at native 100/150/200% DPI. Each pending row replaces its
actions with one Cancel action. Cancel invalidates callbacks, aborts the request,
stops attempt-owned catalog validation/publication and removes attempt staging;
it preserves previous selections, manual files and existing immutable cache hits.
Completed validated cache entries remain shared; no global cache deletion.
Download failed has a keyboard-focusable (?) button opening bounded plain-text
safe diagnostics: fixed source, phase, HTTP/network/TLS or Core code, without URL
queries, credentials, response bodies or raw profiles. Retry clears old diagnostics.
Reproduce the actual Qt upstream pipeline with bounded isolated requests before
changing URL/checksum contracts; record exact phase/code and root cause, including
any inability to reproduce the user's historical error. Preserve HTTPS, checksum,
64 MiB bounds, same-release pinning and generation/lifecycle protections.
Add transfer and UI regressions for cancellation/retry, cleanup, previous selection,
progress geometry and error popup. No Core/dependency/service/release changes.

Diagnosis amendment: real pinned release 202611012203 downloads/checksums match
both parsers. GeoIP validates (260 groups). GeoSite Core catalog returns version 1,
error=request failed, details.code=geodata_identifier_invalid: upstream contains
11 identifiers with ! (first CATEGORY-GAMES-!CN), rejected by Core groupID.
No Desktop rewrite/filter or checksum bypass is permitted. The previous implementation child was limited by a parent-imposed no-Core scope,
not a user prohibition. The user authorized Core changes; the continuation below
resolves this confirmed compatibility blocker.
Surface the exact safe Core code and actionable local-file fallback instead of
misdiagnosing this as a network/checksum failure. Installed Core lacks the catalog
command; its exit/non-JSON/process codes must also remain visible.

## Literal group identifier compatibility amendment — 2026-11-02

Accepted scope: expand the existing ASCII group alphabet by `!` only in Core
decoding/rules, ConfigDocument editing and both Desktop catalog paths. Preserve
1–256 byte length, ASCII case normalization and literal identity; `!` is never
policy negation. Do not strip/filter IDs. Reject attribute selectors, controls,
non-ASCII, empty/oversized identifiers and empty comma entries.

Acceptance: synthetic bang groups enumerate, select, serialize, validate and match
only their stored domains; malformed identifiers still fail. Rebuild isolated Core
bridges, run focused/full Core checks, four CLI cross-builds, four Desktop builds
and both five-test suites. Re-download only pinned release 202611012203 in owned
temporary storage, verify exact checksums, enumerate both databases and validate
a synthetic bang-ID rule. Preserve shared/user files and installed processes.
No packages, service changes, commits, push or PR. Historical GeoIP failure remains
unreproduced; no claim that this fix resolves it.

## Approved editor/settings polish — 2026-11-02

The user requested implementation of these refinements, with delegated work:

- Generated/newly edited config sections and Rule blocks have blank-line
  separation. Preserve untouched source bytes, comments, unknown settings,
  disabled rules, BOM and line endings; do not batch-rewrite user profiles.
- Raw config is immediately visible and editable on its tab. Remove Show/Hide
  and Validate/use visual buttons; automatic validation on switching back to
  visual tabs and on Save remains mandatory. Retain invalid drafts and errors.
- Collapse the empty routing-status region when no actionable message exists.
  Keep errors visible without forcing the whole editor to grow.
- Use a normal dropdown chevron for GeoData selection, not a green download
  arrow. Fit translated type names. Make the group popup compact and consistent
  with app controls; checked IDs appear first on reopening, remain stable while
  filtering/toggling, and keep exact serialized IDs including literal bang names.
- Expose the same global database manager in ordinary Settings as well as Routing.
- Opening that manager may inspect saved local files but must not depict this
  passive check as a download. Use a concise checking state, reuse valid cached
  catalogs, and keep unsafe Apply disabled until readiness is established. Real
  explicit downloads retain progress, per-row cancellation and error details.

Acceptance: focused roundtrip/formatting/Raw transition/checked-first/status
fixtures, actual Settings entry point, native post-event geometry at 100/150/200,
English/Russian translation checks, relevant Windows/Linux CTest and supported
cross-builds. Preserve the 680x520 profile editor and compact manager. No release,
commit, push, PR, installed-service change or user-profile migration is requested.

Additional approved polish: replace the oversized filled native Details triangle
with a small product-style chevron, retaining accessible expanded/collapsed state,
keyboard operation and correct 100/150/200% scaling. Avoid global button styling.


## Visual regression correction — 2026-11-02

Replace the clipped checkbox widget-render delegate with Qt list indicators sized
explicitly in the picker stylesheet. Preserve native check state/accessibility,
Space toggling, filtering and checked-first reopening. Give the destination icon
a square transparent canvas so the line-edit action cannot stretch a 9x5 chevron.
Remove the inline routing warning region as requested; retain gating, rule
explanations through Routing limitations, and Save/Start validation. Acceptance:
inspect actual native picker/table captures at DPR 1/1.5/2, run the focused profile
suite and build shared code for supported platforms. No service/package changes.


## English GeoIP country labels — 2026-11-02

Use the externally reviewed local GeoIP analysis (260 groups: 250 countries and
territories, 10 service/special groups) as the coverage reference. GeoIP picker and
destination summary display English territory names with uppercase parenthesized
codes, independently of UI language. Use existing Qt territory names, preserving
exact catalog IDs in item data and serialized rules. Known special groups are
Cloudflare, Amazon CloudFront, Facebook, Fastly, Google, Netflix, Private and
special networks, Telegram, Tor networks, Twitter; these appear before countries.
Retain checked-first reopening within each category and stable ordering during
filtering/checking. GeoSite labels remain unchanged. Unknown IDs remain visible
without inventing a territory. Search matches both English label and code.
Acceptance: cover the external 250-code catalog, service priority over selected
countries, checked-first category behavior, lowercase serialization, unknown IDs,
GeoSite unchanged and English labels under Russian UI. Use focused tests and
native screenshots. No new dependency, download, Core or packaging changes.


## Selection priority correction — 2026-11-02

Checked groups take precedence across the entire picker. On reopening, place all
checked groups first, then unchecked groups. Within each set, GeoIP special groups
precede countries. Keep current rows stable during checking/searching; recalculate
only on reopening. Preserve ID values and GeoSite checked-first behavior.


## Localized country labels and bilingual search — 2026-11-02

Supersedes English-only labels: display country names through the active app
translator, with uppercase stable codes in both Russian and English. Populate the
existing Russian translation catalog for all250 reviewed territories. Search always
matches visible label, English label, Russian label and ID case-insensitively,
regardless of active language. Reuse the embedded Russian catalog for search;
load once, no remote lookup or dependency. Keep global checked-first ordering,
special-group priority inside selection sets, GeoSite behavior and config IDs.
Acceptance: full catalog translation coverage, English/Russian rendering and
cross-language search under both UI languages, filtering/selection preservation,
focused Windows/Linux tests and actual styled native captures. Test executables
embed the same translation resource to exercise production lookup.
