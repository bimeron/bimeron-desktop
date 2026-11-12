# GeoData routing

Tracking: [Desktop #18](https://github.com/bimeron/bimeron-desktop/issues/18).
Implementation and acceptance checklist: [implementation plan](geodata-routing-plan.md).

The Routing tab uses the Core document CLI to preserve profile text while editing
ordered CIDR, Domain, GeoIP and GeoSite rules. This requires a Core containing the
new `document`, `geodata groups` and `validate --json` commands. The existing
bundle pin does not establish support. With an older CLI, preserve the source and
use Raw config; new GeoData controls remain unavailable. The running service must
independently advertise `geoip_split`, `geosite_split` and
`geodata_schema=v2ray-protobuf`. A newer local CLI cannot grant an older service
these capabilities.

## Rules and files

Each row is one complete `[Rule]` block with exactly one matcher and one Action.
Rules run from top to bottom until the first match. Actions are `direct`, `proxy`
and `block`. GeoData selections serialize stable IDs, never translated labels;
comma-separated IDs mean OR. Available IDs come from the selected local database.
Unknown selected IDs stay visible for correction.

Disabling comments the whole block with one outer `#` layer; enabling removes
that layer. There is no Enabled field. Comments, unknown fields, disabled blocks
and ordering stay in the document. Unsafe edits require Raw config. Cancel and
validation failures do not replace the saved profile.

Default databases. Bimeron does not author country and site databases. Packages
ship Loyalsoldier's `v2ray-rules-dat` release pinned at build time (GPL-3.0) and
select it by default for both kinds; the manager names the source in each row
("Loyalsoldier · release DD.MM.YYYY · checked" or "Own file · checked") and links
the author with the license above the table, and About repeats the attribution.
Each kind has one source: Loyalsoldier, own file, or none ("Not used"). With
"Update Loyalsoldier databases automatically" on, the application checks once a
day while it runs and downloads only a newer release; a new file replaces the
current one only after the selected Core reads it and managed profiles' active
groups still exist. A failed check keeps the current file and shows "Last check
failed". Only the current and previous managed copies are kept; own files are
never deleted. If a VPN is connected when a database changes, the application
asks whether to reconnect now; otherwise the next connection uses it. Every row
has an Actions menu (Update now or Install Loyalsoldier database, Choose own
file, Check again, Disable database); "Install Loyalsoldier databases" restores
both kinds and enables automatic updates. On first start of this version, a path
in the managed download store becomes Loyalsoldier, any other path stays an own
file, and an empty selection becomes Loyalsoldier.

A kind may be disabled even when profiles use it. Such a profile can still be
edited and saved: the routing tab disables the rule's Target, keeps Enabled and
Type editable, and explains that the profile will not connect while the rule is
enabled. Connecting it shows "Profile unavailable" with Manage databases, Edit
profile and Close; automatic connection at startup only notifies and logs. A
running connection is never interrupted. Validation copies used while saving
comment out rules of the unavailable kind, so Core still checks the rest.

Manage databases opens the application-wide country/site database manager, also
available in Settings. Opening it checks saved files without displaying download
progress; valid cached catalogs are reused. The manager shows a short scope note,
the Download source link (Loyalsoldier on GitHub, used only by Download and Update),
and a two-row table for Countries and Sites. An empty row shows No file selected
with Download and Choose file. A selected row always keeps its file name and shows
its status beneath it; its row menu offers Update from GitHub, Choose file, Check
again, and Clear selection. Clearing changes only the draft selection and deletes
no file. File information lists each selected file's full path, status, group
count after a successful check, and diagnostics after a failed one; without a
selected file it appears as Error details only after a real failure.

Check results are classified from the validator's own response, never from
translated text:

| Status | Meaning |
| --- | --- |
| Checked | The selected Core listed the database groups. Local only; not proof that the service can read the file. |
| File not found | The saved path does not exist. The selection is kept. |
| Cannot read file | The application cannot read the file; not a corrupt-database verdict. |
| Validation unavailable | The selected Core answers `unknown command` for `geodata groups`. The file stays selected but cannot be verified. |
| Validation failed | The validator could not start, timed out, exceeded its output limit, crashed, or returned an invalid response. |
| Invalid database | The Core rejected the contents with a `geodata_*` code, or listed no groups. |
| Missing group: ID | An active rule uses a group that the database lacks. |

Diagnostics name the operation, the validator path, the exit code, the Core code
and message when present, and the first validator stderr line; they are plain
text with control characters removed and lengths bounded. Cached results are tied
to the file and to the validator executable's identity, so replacing the Core at
the same path forces a new check. A relative selection opened without a profile
context is reported as Cannot resolve path instead of being resolved elsewhere.
A failed or canceled Download, Update, or chosen file never replaces the current
selection or its status; the attempt is reported once, separately. Release and
SHA-256 provenance is shown only for files downloaded in the current session and
is never attributed to manually chosen files. The group picker uses compact rows
and places checked groups first when reopened, without reordering during selection.
Routing explanations are available through Routing limitations and Save/Start
validation; the redundant inline warning panel is hidden.
New or edited section/rule boundaries receive blank separators; untouched source
spacing is preserved. The Raw config tab displays its editable source immediately.
Each
kind has one shared choice for every Desktop-managed profile using it. Download
explicitly fetches Loyalsoldier's latest release metadata, then pins that release's
exact `.dat` and `.dat.sha256sum` assets. Nothing downloads on startup, Save or
Start, and there are no per-profile overrides. Choose file remains available
offline for local V2Ray protobuf files up to 64 MiB.

HTTPS redirects are checked at every hop, with five hops maximum, normal TLS
verification, a 120-second total deadline and 30-second inactivity timeout.
Metadata is limited to 1 MiB, checksum text to 4 KiB and streamed database bytes
to 64 MiB. SHA-256 checks release consistency, not an independent publisher
signature. Core's shared decoder checks the kind and catalog before publication.
Verified downloads use immutable content-addressed paths under
`QStandardPaths::AppLocalDataLocation/geodata/<sha256>/<kind>.dat`. An update
creates a new path. Manual files are never edited/deleted, and no cache collection
occurs. Cancel may leave a verified download cached but never changes selection.

Apply validates affected managed profiles' active groups, then persists both
shared choices together in application settings. It does not rewrite profiles or
save unrelated editor fields. Canceling an editor after global Apply does not
undo Apply. Files are shared; runnable profiles still use existing `[GeoData]`
`GeoIPFile`/`GeoSiteFile` fields. During Save/import/Start, Desktop patches only
active kinds through the Core document bridge, validates a protected draft beside
the original profile, and uses the protected atomic writer before Core receives
the original absolute profile path. A failed preparation preserves profile bytes
and blocks Start. Inactive profiles retain old references until next preparation;
external direct CLI use of those files is outside this feature.

Relative manual selections resolve against each final .conf directory. Downloaded
paths are absolute and host-local. Export still requires transferring referenced
files or correcting paths. Import preserves the external source; only its managed
copy receives shared references. Disabled-only rules keep their original blocks
and do not require a shared database. An absent shared choice shows setup instead
of adopting one profile's old path globally.

The actual Core service must read the files. Local catalog validation does not
prove LocalSystem/root access or that a running session has loaded the database.
Windows mapped drives/EFS and Linux file permissions can differ from GUI access.
Database contents are immutable through a session, including reconnects. Replace
a database only with the understanding that a new session is required; ordinary
reconnect is insufficient. No automatic restart or ACL changes occur.

## Restrictions

Missing capabilities or unsupported schema prevent new active GeoData rules.
IKEv2 does not support split routing. Packet VPN KillSwitch=true permits CIDR
rules only; Domain/GeoIP/GeoSite conflicts require explicit correction. Neither
protection nor rules are silently removed. Imported/source conflicts remain in a
draft and block Apply/Start. Disabled rules do not require databases.

GeoSite VPN routing depends on trusted DNS metadata, including TTL, shared-IP and
cache limitations. Application DoH/DoT, old caches and bare IP traffic may not
match. SOCKS GeoIP uses only known destination IPs; no implicit local domain
resolution occurs. IPv6=disable remains dominant over routing actions.

The local-network checkbox is a convenience for the existing explicit direct
CIDR set, not a new .conf field or unconditional OS exemption. Imported rules keep
their positions; a later local-network rule is not moved ahead of earlier rules.
A newly requested local bypass inserts a leading rule with an explicit precedence
explanation.

## Development verification

Use the session-only Development backend to select a reviewed local Core without
replacing bundled or installed binaries. Core's desktop CLI protocol is documented
in `bimeron-core/docs/DESKTOP_DOCUMENT_API.MD`. No new RPC or helper executable is
introduced. Profile text travels through stdin, not command arguments.

`BIMERON_GEODATA_CORE` enables optional real-bridge regression fixtures in
profile-vless-test. On Windows, put the matching Qt bin directory on PATH and set
QT_QPA_PLATFORM_PLUGIN_PATH to its plugins/platforms directory before CTest.
Qt deployment is disabled in isolated compiler-check builds, so launching their
test executables without that environment produces missing-DLL dialogs.

Package pins and signed-update approval remain separate release work. Supported
packages remain Windows MSI and Debian DEB for amd64/ARM64. No package artifacts or bundled databases are introduced by this feature.
Explicit GUI downloads do not change Core behavior or package pins.

See [verification and remaining limits](geodata-routing-verification.md) for
actual local build/test evidence and release gaps.

The profile dialog retains its existing 680 by 520 initial size (620 by 460
minimum). Long capability diagnostics scroll within the Routing tab instead of
increasing the minimum height of every tab.

Long VLESS fields scroll within their tab; the routing table uses the available
height without promoting its preferred viewport height to a dialog minimum.
The geometry regression opens an ssconf profile through MainWindow::editProfile
with Russian translations and checks both initial and settled modal geometry.


Native modal geometry regression: opening an ssconf profile through
MainWindow::editProfile at 150% scaling initially produced 680x520, then grew to
680x850 after queued layout events. The hidden VLESS form contributed a
753-pixel logical minimum through QStackedWidget. Keep long VLESS forms inside
a widget-resizable scroll area; do not rely on repeated resize calls or ignoring
the whole tab widget's size hint. Acceptance checks must wait for queued layout
and catalog events and verify both initial and settled modal geometry.

The previous offscreen constructor-only size assertions were insufficient: a
native delayed height-for-width pass still enlarged the dialog. The permanent
developer-mode-test regression opens the real modal path, waits 500 ms, and
asserts exact 680x520 geometry. `--profile-geometry <absolute-png-path>` provides
a synthetic screenshot and size-only JSON diagnostics without user profiles or
system IPC. Native Windows at 150% reproduced 680x850 before the fix and retains
680x520 after both the scrollable VLESS form and flexible routing-table fix.


During a transfer or check, each database row offers Cancel. Cancellation removes
that attempt's staging files and stops its network/attempt-owned validation and
publication work; previous selected files and shared validated cache hits remain.
The row progress track has no percentage text reservation. Failed downloads and
checks expose their diagnostics through the keyboard-accessible File information or
Error details disclosure, with the fixed source, phase and safe numeric
HTTP/Qt/TLS/Core codes; response bodies and signed redirect query strings are
excluded.

The pinned Loyalsoldier GeoSite release tested on 2026-11-02 contains group names
with `!`. The updated local Core bridge and Desktop accept them as literal stable
identifiers. Group IDs use 1–256 ASCII letters, digits, `_`, `-` or `!`, with
lowercase lookup/enumeration. Bang does not mean policy negation; attribute
selectors (`@`), controls, non-ASCII and empty IDs remain invalid. The pinned
202611012203 files now pass checksum, catalog and synthetic bang-rule validation.
Older published/installed Core builds still require separate compatibility checks. Desktop does not filter or rewrite checksum-verified data.
See [verification](geodata-routing-verification.md) for exact evidence and limits.


GeoIP labels follow the application language: for example United States (US) in
English and its Russian translation in Russian. Codes stay uppercase. Search
matches English and Russian names and the stable code in either UI language. The ten known service and
special groups appear before countries within each selection set. Checked groups
appear first across the entire picker when reopened. GeoSite uses a separate compact section picker; see [the trial specification](geosite-picker-trial.md). Unknown IDs remain visible,
and labels never replace the original IDs in configuration files.

## New rule focus

Clicking Add selects only the appended rule, makes its Type column current, scrolls it into view and focuses its type selector. Remove must immediately target this new rule. Loading existing rules and rebuilding the table must not steal focus. Acceptance: add with no selection or another row selected, including below the viewport, then remove immediately or change the type with the keyboard.

Validation: Windows amd64 Release GUI build and git diff --check passed. The change is limited to the Add button handler; import and table reconstruction retain their existing behavior. Automated tests were not run by request; interactive acceptance is pending user inspection.

## Removing populated rules

Remove asks for confirmation when the current rule's destination, trimmed of whitespace, is nonempty. GeoIP/GeoSite use their stored group IDs. Empty rules are removed immediately. No is the default confirmation choice; declining or closing preserves the rule and selection. Acceptance: filled CIDR/domain/GeoIP/GeoSite rules prompt, blank/whitespace destinations do not, and confirming removes only the current rule.

Removal validation: Windows amd64 Release build, translation JSON parsing (case-sensitive hashtable) and diff whitespace checks passed. Automated tests were not run by request; confirmation-dialog interaction awaits user inspection.

### Apply and import validation acceptance

Apply remains synchronous. Each invocation inspects each database kind and resolved absolute path at most once, including the initial absolute selection check, and uses set membership for active groups. No catalog survives between Apply invocations.

Unique download-attempt catalogs and watchers are released after success, failure, or cancellation, after signal arguments are no longer referenced. Shared catalogs remain cached. Import validation resolves references against the source but writes protected drafts only in writable temporary storage. Relative references are refused before validation in relocated drafts; source files and directories remain untouched.

### Review-fix verification (2026-11-03)

Windows amd64 Release and Linux amd64 builds passed. CTest passed 5/5 on
Windows (25.89 s) and Linux (17.26 s), with the reviewed sibling desktop-bridge
Core set through BIMERON_GEODATA_CORE and offscreen Qt. Regression coverage
includes uppercase GeoIP name-only saves, commented IKEv2 headers, mixed-case
GeoSite selection, Cancel/Continue/Discard after clearing PluginOpts, one catalog
process for repeated rules/profiles per Apply, fresh inspection on the next
Apply, completed download success/failure cleanup through the actual manager,
cancellation eviction, alternate import validation storage, relative-reference
refusal and temporary-file cleanup. The first run exposed obsolete GeoIP ordering
expectations and an English-only status assertion; both were corrected to the
accepted preservation contract and active translation before the passing run.

The process-count fixture proves elimination of repeated catalog decoding; it is
not a production latency benchmark. Failed-bridge timeout cancellation and OS ACL
read-only import were not independently exercised: field-removal failure and an
absent source directory cover the relevant dirty-state and validation-location
paths. Diff review and whitespace checks passed. No packaging, release, service
changes or commits were performed. ARM64 builds/runtime were not repeated for
this non-release fix; cross-platform runtime evidence is Windows/Linux amd64.
