# GeoData implementation verification

Date: 2026-11-01. Desktop issue: [#18](https://github.com/bimeron/bimeron-desktop/issues/18).
Work is local and uncommitted on Desktop `feat/geodata-routing` and Core
`feat/67-geodata-routing` (base `0b9158edccc2d188a4cb889af92678b46cbf9f85`).
No push, PR, package release or installed-service change was performed.

## Implemented boundary

Core owns document mutation, Go-string field decoding, bounded V2Ray protobuf
catalog enumeration and structured file validation. Desktop reuses Qt Widgets,
protected profile files, existing CLI selection and existing start/stop IPC.
There is no new RPC, packaged helper, downloader, dependency or bundle-pin change.

The editor preserves original bytes and rule-block identity; disabling uses one
outer comment layer. GeoData selectors retain stable IDs separately from localized
summaries. Catalog processes are bounded/cancelled, and destruction disconnects
callbacks before killing/waiting. File changes trigger catalog revalidation and
restart notices without changing the active session. Host validation retains
conflicting source/import drafts and blocks runnable save/start.

## Checks

- Core: `go test ./...` and `go vet ./...` passed on Windows amd64.
- Core CLI cross-builds passed for Windows amd64/arm64 and Linux amd64/arm64,
  including the final decoded-fields adapter extension.
- Desktop isolated Release builds passed for Windows x64/ARM64 and Linux
  amd64/arm64 (WSL cross-toolchain for Linux arm64).
- Windows x64 CTest: developer-mode-test, core-update-test, profile-vless-test
  passed with BIMERON_GEODATA_CORE pointing to the modified Core build.
- Linux amd64 CTest: the same three tests passed with the native Linux build of
  the modified Core bridge. Linux tests use the offscreen platform.
- Regression fixtures cover BOM/CRLF/no-final-newline, comments/unknown sections,
  mixed-rule order, repeated toggles, disabled rules, imported local-network
  positions, quoted credentials/numbers/paths, host capability/schema/protection
  gates, relative-reference relocation, popup search/hidden selection/Space,
  and closing with pending catalog processes. Core fixtures additionally cover
  atomic batch failure, insert/delete/field ambiguity and decoder resource limits.
- Diff whitespace and translation JSON checks passed. An offscreen screenshot
  was inspected with view_image for layout. It is a structural preview using
  synthetic data and default Qt styling, not installed-product visual acceptance.

The initial isolated Windows test launch lacked Qt DLLs; rerunning with matching
Qt bin on PATH and plugin path fixed the environment. Initial link errors occurred
while the editor agent was still writing methods and were resolved by completing
that source. Regression failures during review identified outdated assertions for
localized summaries and now-valid quoted absolute paths; the assertions now check
stable serialized IDs and decoded-path safety instead.

## Performance

`go test ./internal/routing -run '^$' -bench BenchmarkDecodeGeoData -benchmem
-benchtime=100ms` on Windows amd64 / Intel i7-12700K measured approximately
1.947 ms/op, 163.80 MB/s, 2,602,941 B/op and 10,177 allocations/op. The fixture has
10,000 CIDRs and 10,000 suffix domains. This measures the reused decoder, not
end-to-end catalog search or production routing latency. No new runtime matcher
or index was introduced. Large real-world catalog UI profiling remains separate.

## Review and remaining limits

Independent review found and drove fixes for quoted nonrouting values, quoted
absolute database import paths, source-selector restrictions, structured Start
validation and quoted Interface capability checks. Credentials are decoded by
Core for controls; original draft bytes remain authoritative.

Changing key sources or removing configured fields explicitly requires Raw config;
no unsafe general field-deletion operation is exposed. Go escape spellings outside
the host's conservative protection-value interpretation may require explicit
source correction; Core remains the full validator.

Native ARM64 execution (Windows and Linux), installed-service permissions,
real SOCKS/VPN data-plane behavior, screen-reader/high-contrast and complete
DPI/theme/manual GUI acceptance were not run on this host. Current CI-equivalent
coverage therefore does not include the native Linux arm64 test runner. No target
is declared unaffected: ARM64 builds passed, but runtime evidence is outstanding.

No packages were built because this is implementation work, not an authorized
release. A future release must obtain reviewed published Core artifacts, update
pins/hashes deliberately, and complete the supported package/runtime matrix under
[build-and-packaging.md](build-and-packaging.md). Existing bundle pins do not
include evidence of this new CLI adapter.


## Native profile-dialog geometry correction

A later user check disproved the initial constructor-only geometry acceptance.
The real modal MainWindow::editProfile path at native Windows 150% first opened
at 680x520, then grew to 680x850 after queued layout events. Size-only diagnostic
snapshots traced this to the hidden VLESS form (minimum height 753) and the
routing table's preferred viewport height propagating through heightForWidth.

The VLESS form now has a widget-resizable scroll viewport. The routing table
uses a flexible vertical policy, an explicit 100-pixel usable minimum and layout
stretch. Whole-tab size-policy suppression and repeated resize calls were removed.
The developer-mode regression opens the real modal editor with Russian strings,
waits 500 ms, switches every enabled visible tab, and checks 680x520 throughout.
Native Windows checks at DPR 1.0, 1.5 and 2.0 passed, as did the three Windows
CTest cases with the real Core bridge. Synthetic native screenshots were visually
inspected. This supersedes the earlier offscreen-only geometry claims; it is not
complete screen-reader/theme or installed-service acceptance.


## Explicit shared downloads continuation — 2026-11-02

Implementation was explicitly resumed at low effort in the shared Desktop
`feat/geodata-routing` checkout. All preceding dirty changes and geometry fixes
were preserved. No commit, push, PR, package, installed-service modification or
additional Core edit was performed. The existing uncommitted sibling Core bridge
was used only as a test executable.

### Implemented files and behavior

- `src/GeoDataDownload.{h,cpp}`: fixed production GitHub release metadata, exact
  same-tag data/checksum assets, per-hop HTTPS allowlist, five redirects, streamed
  bounds/hash, deadline/inactivity timers, cancellation/stale-generation safety,
  task-owned staging and immutable atomic content-addressed publication.
  Publication hashing/copy runs in an interruptible filesystem worker; no updater
  installer, executable downloader, new library or environment URL override exists.
- `src/GeoDataSelection.{h,cpp}`: one settings value for both global paths, checked
  persistence/rollback, active-group validation across readable affected managed
  profiles, application-owned asynchronous catalogs/cache/file invalidation, and
  Core document materialization plus protected same-directory validation drafts.
  Unreadable unusable entries remain untouched and cannot pass later preparation;
  unrelated readable non-GeoData damage does not block global Apply.
- `src/GeoDataDialog.{h,cpp}` and `ProfileEditorDialog.{h,cpp}`: native compact
  manager, application font/icon, explicit Download/Update and Choose file,
  source link, short states/progress, collapsed bounded Details with group IDs,
  recheck/clear, application-wide scope, Cancel/Apply and scoped async observers.
  Global Apply neither writes profiles nor commits unrelated editor fields.
- `MainWindow.{h,cpp}` and `ProfileStore.cpp`: Save/import/Start synchronize active
  database references, preserve the external import source and original managed
  profile path/identity, reject invalid UTF-8 and read-only replacements, and
  retain prior bytes on validation/write failure. Shared selection changes during
  asynchronous validation or before pending IPC Start cancel the stale start.
  Non-GeoData and disabled-only blocks remain unchanged.
- `tests/GeoDataDownloadTest.cpp`, `tests/GeoDataSelectionTest.cpp`, existing
  profile/developer/connection fixtures, source wiring in `CMakeLists.txt`, and
  Russian strings in `assets/translations/ru.json` cover the new behavior.

### Final checks

- Windows x64 Release: `build/geodata-download-amd64`, all five CTest cases passed
  (`developer-mode-test`, `core-update-test`, `geodata-download-test`,
  `geodata-selection-test`, `profile-vless-test`), 12.96 seconds final run.
  Every Windows test used the matching Qt bin PATH, platform-plugin path,
  offscreen QPA and Windows font directory. `BIMERON_GEODATA_CORE` pointed to
  `bimeron-core/build/desktop-bridge/bimeron.exe`.
- Linux amd64 Release (WSL): `build/geodata-linux-amd64`, the same five CTest cases
  passed, 10.80 seconds final run, with the sibling `bimeron-linux-amd64` bridge
  and offscreen QPA.
- Final application builds passed for Windows ARM64 in
  `build/geodata-arm64-buildtools` and Linux arm64 in
  `build/geodata-linux-arm64`. Neither ARM64 executable was run on this x64 host.
  No shared-Qt platform was declared unaffected.
- Fixtures exercise release pinning/missing or mixed metadata/checksums, accepted
  and refused redirects/loops/ports, TLS/HTTP/network failures, deadlines,
  unknown/false lengths, exact/over-limit streaming, duplicate/concurrent kinds,
  stale replies/retry, cancel/destruction, async publication and failed cache
  writes. Synthetic protobufs exercise wrong-kind decoding, shared settings
  failure, two profiles reusing a file, missing active groups, disabled blocks,
  relative paths, full Core validation, original bytes and identity, stale-ref
  correction, unrelated edits, read-only replacement, manager Cancel and manual
  file mutation invalidating Ready/Apply. A queued-cache/forced-inspection
  regression guards against consuming a stale empty result.
- Native Windows QPA: final captures at measured DPR 1.0, 1.5 and 2.0 waited
  500 ms for the real editor and another 500 ms after its manager opened through
  the actual action. The editor retained 680x520 across visible tabs; the manager
  retained 480x300, Segoe UI 9 pt, a real application icon and native-titlebar
  flags. Details stayed below 500 logical pixels. Tab/Shift+Tab focus and Escape
  cancellation passed. Russian captures at all three scales, English at 150%,
  and light/dark captures were inspected. Dark source-link contrast was fixed.
  Generated evidence is under `build/geodata-download-amd64/final-*`.
- Linux light/dark offscreen client captures were inspected under
  `build/geodata-linux-amd64/final-*`. Offscreen propagateSizeHints warnings are
  platform limitations, not native Linux titlebar evidence.
- Translation JSON/placeholders, local documentation links and `git diff --check`
  passed. Build documentation and both plans now describe explicit downloads,
  shared selection, source wiring, test environment and transaction boundaries.

### Performance and remaining acceptance

The exact 64 MiB Windows async-publication fixture measured 769 ms total, 0 ms
start overhead, 767 UI timer ticks and a 3 ms maximum tick gap. A Linux rerun
measured 631 ms, 630 ticks and a 2 ms maximum gap. `/usr/bin/time -v` reported
150,732 KiB maximum RSS for the entire synthetic network suite; the fixture itself
holds complete 64 MiB response bodies, so this is not a production downloader
memory estimate. Production data streaming/hash/copy uses 64 KiB chunks and a
bounded reply buffer; Core catalog stdout is separately limited to 32 MiB. No
routing benchmark was needed: this increment changes acquisition/selection/UI,
not Core's evaluator. Shared Apply and ordinary Save/import profile validation
remain bounded synchronous CLI operations; transfer/catalog inspection/publication
are asynchronous.

Native ARM64 runtime/native Linux arm64 CTest, screen-reader/high-contrast
acceptance, actual installed-service file readability and live VPN/SOCKS traffic
remain unverified. A desktop screen grab of the decorated native frame returned a
blank host-desktop image; native client renders, flags and geometry were verified,
but this does not establish visual titlebar acceptance on an unlocked desktop.
No real public database was installed or service session restarted for testing.
A reviewed published Core with the required bridge and endpoint capabilities is
still a release dependency; downloading data does not establish compatibility.


### Independent review P2: Raw Save validation ordering (2026-11-02)

Fixed the early editor Save callback in `MainWindow::editProfile`: materialize
current shared paths before routing/Core validation of the protected draft in the
profile directory. Final publication still requires dialog acceptance, final
validation, original-byte concurrency checking and the atomic profile writer.
Corrected Raw import validation and its final draft write also materialize the
shared references, retaining the external source. These paths use the existing
selected validation executable consistently, including the real-Core test hook.

`ConnectionManagementTest::sharedGeoDataPreparation` now drives actual
`MainWindow::editProfile`, selects Raw, edits `rawConfig` and clicks Save. Both
absent GeoIPFile and a missing stale database reference save successfully with a
valid shared selection. Assertions cover shared references, active rules, comments
and disabled unknown content; invalid groups retain the exact draft and original.
Active unknown sections are rejected by the real Core and remain intact in the
draft/original (they cannot truthfully be called valid runnable profiles). Cancel
is exercised through its button and destructive discard confirmation, preserving
original bytes. The prior post-acceptance preparation tests remain in place.

Final checks after the correction:

- `cmake --build build/geodata-download-amd64 --config Release`: passed.
- `ctest --test-dir build/geodata-download-amd64 -C Release --output-on-failure`:
  all five passed, 14.34 seconds; developer-mode-test 7.15 seconds. Used Qt 6.8.3
  x64 bin PATH, matching platforms plugin path, offscreen QPA, Windows font
  directory and `BIMERON_GEODATA_CORE` = sibling
  `build/desktop-bridge/bimeron.exe`.
- WSL `cmake --build build/geodata-linux-amd64 -j 4` and
  `QT_QPA_PLATFORM=offscreen BIMERON_GEODATA_CORE=/mnt/c/Users/PC/Documents/dev/bimeron/bimeron-core/build/desktop-bridge/bimeron-linux-amd64 ctest --test-dir build/geodata-linux-amd64 --output-on-failure`:
  all five passed, 11.82 seconds; developer-mode-test 5.93 seconds.
- `cmake --build build/geodata-arm64-buildtools --config Release`: passed;
  final changed fixture rebuilt with `--target bimeron-desktop developer-mode-test`.
- WSL `cmake --build build/geodata-linux-arm64 -j 4`: passed, including the final
  changed fixture. ARM64 runtime/native CTest remain unavailable on this x64 host.
- `python build/check-geodata-docs.py` and `git diff --check`: passed.

An initial fixture used an active unknown section and was correctly rejected by
Core; coverage now distinguishes valid commented unknown content from retained
invalid active content. An initial Linux run pointed at the older `bin` Core;
rerunning with the reviewed desktop-bridge binary passed. No checks were bypassed.
The focused diff review found no additional defect; import final-write ordering
was corrected alongside Save. No benchmark/profile rerun is warranted for this
validation-order correction; acquisition/runtime performance is unchanged.
No commit, push, PR, package, service change, Core edit or user GUI restart occurred.
Existing native UI/service/ARM64 and published-Core release limitations above remain.


### Download feedback corrections — 2026-11-02

Specification amended in geodata-download-plan.md before implementation. Preserved
all earlier dirty checkout changes; no Core edits, dependency changes, commits,
PRs, packages, service changes or user-app termination.

Changes: progress text is disabled and the bar expands across the entire row.
Pending actions become one per-kind Cancel. Cancel disconnects its catalog observer,
invalidates its generation, stops only attempt-owned candidate catalog processes,
aborts network/publication and removes temporary files. Shared/manual files and
existing immutable cache hits are retained. Publication no longer starts an
unnecessary second catalog inspection after assigning the candidate path; the
already validated candidate is rehashed by the publisher before selection.
Keyboard-focusable (?) beside a failed status opens native plain-text diagnostics.
Network messages contain fixed source/phase/status/numeric codes, never raw reply
error strings, bodies, signed redirect queries or credentials. TLS diagnostics
retain numeric certificate error codes. Catalog diagnostics retain bounded
whitelisted structured Core codes plus process/JSON/timeout information.

Actual upstream diagnosis (isolated requests, temporary files cleaned):

- GitHub metadata HTTP 200, tag `202611012203`, 33,415 bytes. Exact release-asset
  prefix and SHA-256 parser both match current upstream; no permissive parser or
  host-allowlist change was justified. Assets redirect to
  `release-assets.githubusercontent.com` over HTTPS.
- GeoIP: HTTP 200, 16,549,675 bytes, SHA-256
  `fbf747a085be25eb61ecac8c27140b4ab0bf8181af6fdb104caa325bd93cf117`.
  The real Qt downloader verified it and removed its candidate. The supplied
  desktop-bridge Core exited 0, version 1, 260 catalog groups.
- GeoSite: HTTP 200, 11,323,876 bytes, SHA-256
  `704a1d584d330445350a39d140cf019d06c184b22c59bc4aebaa660b1b6c841f`.
  The supplied Core exited 0 with `version:1`, `error:"request failed"`, empty
  groups and `details.code:"geodata_identifier_invalid"`. Read-only inspection of
  Core's groupID validator found it permits only letters/digits/underscore/hyphen.
  A bounded protobuf field scan found 1,561 upstream groups, 11 containing `!`,
  beginning with `CATEGORY-GAMES-!CN`. This is a confirmed Core/data compatibility
  failure at catalog validation, not HTTP/TLS/hash failure. Desktop now exposes
  that exact code and keeps prior selection. The previous child was under a parent-imposed no-Core scope, not a user
  prohibition: the user had explicitly authorized Core changes. Desktop does not
  rewrite/filter data. The compatibility continuation below supersedes this blocker.
- The installed `C:/Program Files/Bimeron/bimeron-core.exe` exited 1 with no JSON
  and `error: unknown command` for both catalog requests. It lacks this CLI bridge;
  testing uses the explicitly supplied new bridge, never modifies the service.
- The user's attached screenshot shows GeoIP Download failed. That historical
  network failure did not reproduce in the isolated Qt request. Its original
  phase/status was not retained by the old UI, so no exact historical cause can
  truthfully be assigned. In particular the confirmed GeoSite incompatibility is
  not asserted to explain the screenshot's GeoIP failure. New diagnostics preserve
  the required evidence on a recurrence. TLS/checksum/64 MiB/release pin remain.

Regression evidence:

- Transfer fixtures cover HTTP/TLS/redirect/size/hash failures, held data transfer
  cancellation and staging cleanup, cancel/retry late reply, candidate discard,
  interrupted publication with prior cache preservation, changed-candidate refusal
  and exact 64 MiB publication. Injected network error strings contain synthetic
  credentials/query tokens; emitted errors and popup exclude them.
- Actual modal fixtures exercise previous manual file/shared selection preservation,
  single Cancel visibility, full painted progress groove alignment, catalog-process
  cancellation and candidate deletion, retry followed by stale catalog emission,
  HTTP-429 popup opened with keyboard Space, and error-help adjacency/accessibility.
  A synthetic `category-games-!cn` catalog retains the exact Core rejection code.
- Native Windows QPA/application-stylesheet captures at measured DPR 1/1.5/2:
  `build/feedback-0.6666667.png.*`, `build/feedback-1.png.*`,
  `build/feedback-1.3333333.png.*`. Transfer/failure/error-popup, collapsed/expanded
  Details and normal editor captures inspected. Managers remain 480x300 logical
  pixels, Segoe UI 9 pt, native titlebar flags; progress groove right edge matches
  row controls within 1 logical pixel. Screenshots are ignored local evidence.
- One native run during concurrent checks failed the existing editor-tab size
  assertion; a subsequent sequential full three-DPI run passed without weakening
  the assertion. This transient was not reproduced or attributed to a root cause.
  The new transfer/alignment checks themselves passed. Initial capture timing was
  corrected to wait 500 ms before native screenshots. A Linux test initially
  assumed a visible QTemporaryFile name; corrected it to assert transferred bytes
  and cleanup because Linux may keep the open temporary file unnamed.

Full Windows x64 and Linux amd64 five-test CTest suites passed using the explicit
Qt environment and reviewed desktop-bridge Core. All four application/test build
configurations passed: geodata-download-amd64, geodata-arm64-buildtools,
geodata-linux-amd64 and geodata-linux-arm64. Final fixture-only check results are
recorded below. ARM64 binaries were cross-compiled, not executed. JSON translation
placeholders, local documentation links and git diff --check passed. Review covered
stale callbacks, catalog observer cleanup, deletion ownership, immutable-cache
preservation, credential redaction and unnecessary second validation; fixes above
addressed the findings. No new routing benchmark is warranted; the existing 64 MiB
publication heartbeat remains the responsiveness check. Native screen-reader,
ARM64 runtime, unlocked decorated-frame appearance, installed-service readability,
the transient editor geometry assertion and historical GeoIP network failure
remain acceptance gaps. GeoSite compatibility requires a separate Core fix.


Final checks after the last fixture change:

- Windows x64 CTest: all five passed, 15.37 s (developer-mode 8.13 s).
- Linux amd64 CTest: all five passed, 13.08 s (developer-mode 7.17 s).
- Both ARM64 fixture rebuilds passed; earlier full application/test builds passed.
- Final native Russian light 100/150/200% capture runs passed, including explicit
  error-button adjacency. English dark 150% and Linux offscreen English captures
  additionally passed (local feedback-dark-en/feedback-linux outputs).
- 64 MiB publication: Windows 811 ms, start 0 ms, 810 heartbeat ticks, max gap 3 ms;
  Linux 623 ms, start 0 ms, 622 ticks, max gap 2 ms. No routing performance changes.
- Documentation links, translation placeholders and git diff --check passed.


## Literal bang identifier compatibility fix — 2026-11-02

### Scope and source evidence

The user explicitly authorized Core changes (including the earlier instruction
"including core changes" and approval to proceed). The preceding implementation
child's no-Core restriction came from its parent, not the user. The attribution
above and in the download plan is corrected. All pre-existing dirty changes in
both checkouts were preserved. The accepted compatibility amendment was written
before implementation. No commit, push, PR, package, release, service change,
user-app termination or restart occurred. Updated Core executables remain only in
sibling `build/desktop-bridge`; none was copied into the Desktop application or
installed service. Existing CMake Linux builds ran their unchanged copy step for
the already pinned `.tools/core` payload, not the new bridge.

The [V2Ray v4 schema](https://github.com/v2fly/v2ray-core/blob/v4.45.2/app/router/config.proto)
stores GeoSite.country_code as a string (field 1), separate from Domain.attribute
(field 3). [Loyalsoldier's source documentation](https://github.com/Loyalsoldier/v2ray-rules-dat#geositedat)
uses bang-bearing category names. A bounded scan of the exact pinned files found
only ASCII letters/digits/underscore/hyphen/bang in their group IDs. Thus the
minimal alphabet expansion is `!`, not arbitrary punctuation or selector syntax.

Core `internal/routing/geodata.go` now accepts literal bang in dataset and rule
IDs, preserving the 1–256 byte limit and lowercase ASCII lookup/enumeration.
It validates ASCII before lowercasing, preventing Unicode compatibility letters
(such as Kelvin sign) from becoming accepted ASCII. No negation, filtering,
rewriting, attribute selection or matcher algorithm was added. ConfigDocument
now enforces the same syntax, including rejecting formerly accepted `@` selectors.
Both synchronous and asynchronous Desktop catalog validators accept lowercase
bang IDs, with absolute regex anchors rejecting trailing control characters.

Core tests cover decode/enumerate, uppercase lookup, matching stored domains and
leaving unrelated domains at the default action, document serialization, and
rejection of empty, oversized, control, non-ASCII and attribute-selector IDs.
Existing empty-comma rejection remains covered. Desktop's former bang rejection
fixture now expects a valid catalog in both paths; shared selection and full
materialize/serialize/validate pass with the exact ID preserved. A separate
`sites@cn` malformed fixture retains the structured catalog-error/help regression.
The existing progress/Cancel/(?) source was preserved and its regressions passed.

### Final actual checks

- Core Windows amd64: `go test ./internal/routing ./core ./cmd/bimeron`,
  `go test ./...`, and `go vet ./...` passed. Full tests/vet were repeated after
  the final ASCII guard. No reference-server environment was supplied; optional
  external interoperability/network-namespace tests are not claimed.
- Relevant Core CI commands passed: `go test ./androidcore ./core
  ./internal/config ./internal/routing -count=1`; decoder fuzz with `-fuzztime=5s
  -parallel=4` passed, 912,820 executions, 41 new interesting inputs, no failure.
- Core Geo benchmark smoke, Windows amd64/i7-12700K, `-benchtime=200ms`:
  decode 2,133,487 ns/op, 149.48 MB/s, 2,602,848 B/op, 10,177 allocs/op;
  matching domain 47.91 ns/op and miss 161.7 ns/op, both 0 B/op/0 allocs.
  This is no before/after speedup claim. No additional profiling was warranted:
  the evaluator algorithm and data structures did not change.
- Final `CGO_ENABLED=0 go build ./cmd/bimeron` cross-builds passed for
  windows/amd64, windows/arm64, linux/amd64 and linux/arm64. Outputs are
  `bimeron.exe`, `bimeron-windows-arm64.exe`, `bimeron-linux-amd64` and
  `bimeron-linux-arm64` under sibling `build/desktop-bridge`.
- All four Desktop application/test builds passed: Windows Release
  `build/geodata-download-amd64`, `build/geodata-arm64-buildtools`; WSL
  `build/geodata-linux-amd64`, `build/geodata-linux-arm64` (`-j 4`).
- Final Windows x64 CTest: all five passed, 15.65 s. Final Linux amd64 WSL CTest:
  all five passed, 14.42 s. Cases: developer-mode, core-update, geodata-download,
  geodata-selection and profile-vless. Both used the final matching sibling
  desktop-bridge executable through `BIMERON_GEODATA_CORE`. Windows used
  `C:/Qt/6.8.3/msvc2022_64/bin` prepended to PATH, matching
  `plugins/platforms`, `QT_QPA_PLATFORM=offscreen`, and
  `QT_QPA_FONTDIR=C:/Windows/Fonts`. Linux used offscreen QPA.
- No widget, layout or UI selector implementation changed in this continuation;
  native DPI capture reruns were not needed. Existing modal/feedback fixtures ran
  in the five-test suites; earlier native DPI evidence retains its original scope.
- Documentation links/translation placeholders and both `git diff --check`
  checks passed. Core GEODATA.MD, GUI contract, Desktop document API and ADR 0030
  now describe literal identity, bounds, normalization and rejected selectors;
  Desktop download specification and user routing guide reflect compatibility.

### Isolated pinned public-file verification after the final fix

An HTTPS Python probe used exact release tag `202611012203`, 30-second request
socket timeouts, 16 KiB checksum and 64 MiB data limits, subprocess deadlines and
a newly owned temporary directory. It fetched each exact-tag `.dat.sha256sum`
and `.dat`, compared SHA-256, scanned protobuf group names and compared every
lowercase sorted name with the rebuilt Windows Core catalog. This was an isolated
public-file/Core check, not a claim of an installed GUI or service session test.

- GeoIP: 16,549,675 bytes; SHA-256
  `fbf747a085be25eb61ecac8c27140b4ab0bf8181af6fdb104caa325bd93cf117`;
  Core exited 0 with version 1, 260 groups, no error.
- GeoSite: 11,323,876 bytes; SHA-256
  `704a1d584d330445350a39d140cf019d06c184b22c59bc4aebaa660b1b6c841f`;
  Core exited 0 with version 1, 1,561 groups, no error. All 11 bang IDs survived:
  CATEGORY-GAMES-!CN, CATEGORY-AI-CHAT-!CN, GEOLOCATION-!CN,
  CATEGORY-SOCIAL-MEDIA-!CN, CATEGORY-BROWSER-!CN, CATEGORY-NETDISK-!CN,
  BYTEDANCE-AI-!CN, CATEGORY-SCHOLAR-!CN, CATEGORY-AI-!CN, CATEGORY-CDN-!CN,
  TLD-!CN.
- A synthetic Shadowsocks profile referencing both downloaded databases and
  `GeoSite=category-games-!cn` passed `validate --json`: exit 0, exactly
  `{"version":1}`. No connection was started. The owned temporary directory,
  both downloads/checksums and synthetic profile were removed automatically;
  shared/manual/user files were untouched. The ignored local reproduction script
  is `build/bang-live-verification.py`.

Review traced the identifier from protobuf through catalog, Desktop selection,
Core document serialization, full file validation and runtime matching. The
additional findings (ConfigDocument's loose selector syntax and Unicode folding)
were corrected and regression-tested; no further defect was found in this scope.
The confirmed GeoSite blocker is resolved in these sources. The historical GeoIP
network failure was not reproduced and is not claimed fixed. Native ARM64 runtime,
Linux Core full/race CI (no Linux Go toolchain found), full external transport CI,
installed-service readability/capabilities and live VPN/SOCKS remain unverified.
Published/bundled Core artifacts still require separate reviewed release work;
this source/test fix neither changes the installed Core nor establishes that older
published binaries support the bridge or bang IDs. Parent review remains pending.


## Editor and shared-manager polish — 2026-11-02

Implemented the approved polish in geodata-download-plan.md: immediate editable
Raw tab with automatic transition/Save validation, collapsed empty routing status,
compact application checkboxes and checked-first group picker, destination and
Details chevrons, full translated matcher width, Settings manager entry point,
and passive Checking state without download progress or transfer cancellation.
Cached catalogs retain Ready without a new process; explicit downloads retain
progress, cancellation, cleanup and diagnostic behavior.

Core document edits now insert blank separators only at affected section/rule
boundaries, including changed move adjacencies. Untouched bytes, comments, BOM,
line endings and disabled blocks survive; parse, toggle and no-op moves do not
normalize whitespace. The Core child ran focused/full uncached Go tests and
Windows/Linux amd64 vet, rebuilt the local Windows/Linux amd64 bridges, and
updated DESKTOP_DOCUMENT_API.MD. No new bridge operation was introduced.

Parent integration results:

- All four Desktop application/test configurations built: Windows x64/ARM64 and
  Linux amd64/arm64. Final translation resources rebuilt in all four.
- All five Windows x64 CTests passed with the rebuilt Core bridge (16.79 s).
- All five Linux amd64 CTests passed with the rebuilt Linux bridge (15.60 s).
- Focused fixtures cover Raw visibility and transitions, retained invalid drafts,
  empty status, checked-first reopening/filtering, actual Settings entry point,
  passive/cached readiness and existing download cancellation/error behavior.
- Implementation children verified native Windows DPR 1/1.5/2 after event settling:
  editor 680x520 and collapsed manager 480x300. Parent inspected the manager Details
  capture with the small product chevron. Native decorated-window visual approval
  and screen-reader/high-contrast checks remain manual acceptance.
- Russian strings updated; profile-preservation.md and geodata-routing.md now
  describe immediate Raw editing and shared-manager behavior. Diff checks passed.
- No new performance-sensitive algorithm; benchmark/profile reruns are not
  applicable to these layout/state/formatting changes. Earlier download
  responsiveness measurements retain their original scope.

Review found no remaining blocker in this scope. ARM64 runtime, installed-service
read access/capabilities and live VPN/SOCKS acceptance remain unverified. No
package, release pin, installed service, user profile, commit, push or PR changed.
The existing GUI was not restarted as part of this polish.


## Local launch payload correction — 2026-11-02

The user reported disabled visual tabs and a redacted error after editing Raw.
Reproduction against the executable adjacent to the launched GUI found an older
bundled Core: `document` exited 1 without JSON. The reviewed local development
bridge accepted the same synthetic quoted-routing/GeoData document and exposed
version 1 fields/rules. Thus the conservative Raw-only guard was correct for that
payload; the prior launch paired the new GUI with the wrong Core CLI.

Replaced only ignored build/geodata-download-amd64/Release/bimeron-core.exe with
bimeron-core/build/desktop-bridge/bimeron.exe. SHA-256 matched exactly:
C6A0D8B88C68A4308BF2B8DF6092E4C3CE92E47363463786FC63AFBD73928290.
The profile-vless-test suite passed against this exact adjacent executable (2.80 s).
No source guard, dependency/release pin, package or installed service changed.
Future local builds may restore the pinned Core; explicitly verify the adjacent
CLI supports document before launching this development GUI. The installed
service still independently controls runtime capability gating.


## Picker rendering and inline-warning correction — 2026-11-02

Root causes: the list delegate rendered a styled checkbox widget into the smaller
item-indicator rectangle, clipping its contents; QLineEdit scaled a non-square
9x5 icon as an action icon. Removed that widget-render delegate, styled standard
list indicators at 16x16, and centered the chevron on a transparent square canvas.
The inline routing warning region is hidden as requested. Save/Start gating,
rule state and source preservation remain unchanged.

Independent read-only review found no material issue. Windows x64 all five
CTests passed (16.22 s), Linux amd64 profile-vless-test passed (2.61 s), and all
four application configurations compiled. ARM64 runtime remains untested.

The first optional profile capture lacked application resources/style and was
rejected as visual evidence. The actual MainWindow geometry fixture now captures
.routing.png, .groups.png and .geosite-groups.png with inherited application style
and embedded check/chevron resources. Optional picker capture requires
BIMERON_GEODATA_CORE; ordinary CI retains its no-Core path. Parent corrected its
initial missing-bridge fixture and explicit QDialog rejection after simulated
edits, then ran native DPR 1/1.5/2 captures successfully. Parent inspected the
actual styled picker/table images at all three scales: full indicators/checks,
compact rows, undistorted chevrons and no inline warning region. Space toggling,
filter retention and checked-first reopening passed. The final developer-mode
test rerun passed (8.57 s); diff checks passed.

No performance-sensitive algorithm changed; benchmarks are not applicable.
Updated the approved plan and user routing guide. No package, installed-service,
release pin, commit, push or PR change. The launched local GUI is paired with the
reviewed development Core rather than the older pinned build payload.


## English GeoIP labels and special-group priority — 2026-11-02

Added GeoIPLabels.h using existing QtCore territory names and explicit labels for
the ten special IDs from the external Bimeron-GeoIP-Analysis verification. GeoIP
picker and summary show English names with uppercase codes; GeoSite labels and
serialized IDs remain unchanged. A stable special-first partition follows the
existing selected-first partition, retaining selected-first behavior inside each
category without reorder while filtering/toggling. Unknown IDs stay visible.

Read-only subagent audit compared all 250 catalog territory codes to Qt 6.4 source:
none missing, including XK. Windows Qt 6.8.3 and Linux Qt 6.4 profile-vless-test
passed with BIMERON_GEOIP_COUNTRY_CATALOG pointing to the external catalog (2.97 s
and 2.64 s respectively). Tests check all250 labels, Russian-default-locale English
names, uppercase suffixes, special priority over a selected country, picker search,
lowercase IDs, and existing GeoSite/unknown-selection paths. A misplaced prior
status assertion initially failed after the new scenario changed the matcher;
it was moved before that scenario, preserving the original assertion.

All four application configurations built. Native actual-style Russian-UI captures
show English country names and special-group labels before Russia, complete
checkboxes and unchanged compact geometry. Parent inspected the captures.
Updated approved plan and user guide; diff checks passed. No new dependency,
Core edit, package, release pin or installed-service change. ARM64 runtime remains
unverified; this is no release. Benchmark/profile work is not applicable to this
small lookup/partition over the existing catalog. Local launch retains the exact
reviewed Core CLI adjacent to the GUI.


## Global checked-first priority — 2026-11-02

User clarified that checked groups outrank all unchecked groups, including special
GeoIP groups. Reversed the two stable partitions: special-first, then globally
checked-first. Within checked/unchecked sets, specials still precede countries.
No live reorder during selection/search and no identifier or serialization changes.
A delegated test update adds a real synthetic cloudflare catalog entry and checks
selected countries before unchecked specials, special ordering among unchecked
rows, filter/toggle stability and saved IDs. Windows x64 profile-vless-test passed
(3.02 s); Linux amd64 passed (3.59 s). Both native application builds passed and
diff checks passed. ARM64 rebuilds were not repeated for this platform-independent
partition-order change; prior Qt/API cross-build coverage remains applicable.
Updated specification and user guide. Benchmarks are not applicable. No Core,
package, service, commit, push or PR changes.


## Localized GeoIP labels and bilingual search — 2026-11-02

Country display now passes English Qt territory names through the active app
translator; the existing Russian catalog covers all250 reviewed codes and both
special network labels. Added33olderQt6.4-English-name aliases because Qt6.8 uses
newer CLDR wording. The cached embedded Russian JSON catalog supplies alternate
search names even with English UI; each GeoIP item matches visible text, English,
Russian and original ID case-insensitively. GeoSite labels, stable IDs, checked-first
priority and row stability during filtering remain unchanged. JsonTranslator now
reports isEmpty consistently with its populated custom map.

The translation child verified250unique country mappings, original556translations
preserved, UTF8/placeholder integrity and supplied the Qt6.4 mismatch list. Parent
added all33aliases and resource-backed coverage checks. The earlier read-only
review preferred ID-keyed data to avoid wording drift; the implemented existing
translation architecture instead explicitly covers both supported Qt versions,
with English/ID fallback for future unknown entries.

- Windows x64 all5CTest passed17.41s; Linuxamd64 all5passed17.13s, with external
  BIMERON_GEOIP_COUNTRY_CATALOG coverage250 and bilingual matching checks.
- Both ARM64Release builds passed including final translation-resource rebuilds;
  runtimeARM64 remains untested.
- Native actual-style English and Russian captures passed compact geometry,
  active-translator label checks and search for Russia, its Russian name and RU
  under both UI languages. Parent inspected both picker captures.
- Tests embed the same ru.json resource; docs/build-and-packaging.md records this
  test-only build change. No new runtime dependency or installed file is added.
- Updated approved specification and routing guide; diff checks passed. The static
  translation map is read once, then existing bounded catalog filtering is reused;
  no new benchmark/profile run was applicable.

No Core, package, release pin, installed-service, commit, push or PR changes.
Final independent read-only review found no remaining blocker; native
assistive-technology acceptance and prior service/live-traffic limits remain
unchanged.
