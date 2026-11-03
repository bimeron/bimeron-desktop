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

Manage databases opens the application-wide country/site database manager, also
available in Settings. Opening it checks saved files without displaying download
progress; valid cached catalogs are reused. The group picker uses compact rows
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
The full-width progress track has no percentage text reservation. Download failed
and invalid-file states expose keyboard-accessible (?) diagnostics with the fixed
source, phase and safe numeric HTTP/Qt/TLS/Core codes; response bodies and signed
redirect query strings are excluded.

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
