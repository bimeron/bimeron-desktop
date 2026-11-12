# Shared database manager redesign

Status: implemented on `fix/geodata-manager-states`; see Implementation results.

This specification records the final table-based mockup accepted in the current
design review. It supersedes the manager layout, explanatory Details copy, and
generic validation-failure presentation in `geodata-download-plan.md`. Existing
download, validation, selection, and profile-safety contracts remain in force.

## Accepted interface

- Keep the existing shared native Qt dialog, reachable from Settings and Routing.
  Keep its ordinary title bar, application icon, font, and theme behavior.
- Use the existing application table appearance: neutral header, thin dividers,
  ordinary text, and existing secondary buttons with six-pixel corner radii.
  Add no decorative cards or new global widget styling.
- Above the table, show the short scope description: these databases serve
  country/site rules across all profiles.
- Directly below that description and above the table, show Download source with
  the existing Loyalsoldier / GitHub link. This describes the provider used by
  Download/Update, not the origin of a manually selected file.
- The table has two fixed rows, Countries and Sites. Columns contain the database
  kind, selected filename with status beneath it when relevant, and actions.
- For an empty row, show No file selected and expose Download and Choose file
  directly. There is no error, passive check, or ellipsis menu to discover first.
- For a populated row, retain the filename in every state, including errors.
  Show actions in an ellipsis menu: Update from GitHub, Choose file, and existing
  revalidation/clear-selection operations. Clearing changes the draft selection
  only; it must not delete a database file.
- Keep Cancel and Apply at the bottom. Apply is disabled without changes or
  while changed selections cannot safely be applied.
- Remove the old explanatory paragraphs about what Download and Choose file do,
  the repeated optional-database hint, inline question-mark buttons, and the
  bottom-right source link.

Use the accepted approximately 520-pixel-wide mockup as a layout reference, not
as literal Qt font sizing or a fixed geometry. Normal content should stay compact;
expanded diagnostics may grow within a bounded scrollable area. Long paths must
not widen the dialog, and table rows must not be clipped. Verify translated button
widths and fractional scaling in the actual Qt implementation.

## File information and diagnostics

- With no selected files and no actual error, show no disclosure or empty panel.
- Once a file is selected, offer File information with the existing small
  chevron, keyboard access, and accessible expanded/collapsed state.
- Expanded information contains structured facts for each relevant database:
  kind, full copyable path, and local validation result. Show the available group
  count after successful inspection. Do not replace a full path with its hash
  directory or a permanently truncated string.
- On inspection failure, add meaningful diagnostic facts: effective validator
  path/version when available, operation, and bounded sanitized error details.
  Display unavailable facts honestly; do not invent a version or successful check.
- A failure without a selected file, such as an initial failed download, still
  needs an accessible Error details disclosure. The absence of selected files
  must not hide real failures.
- Do not assert that a manual file came from Loyalsoldier. Show release/hash
  provenance only where the existing download attempt actually supplies it.
- Local validation does not establish service readability or runtime use.
  Preserve this distinction in diagnostics and validation behavior.

## State and preservation contract

| Condition | User-facing meaning and behavior |
| --- | --- |
| No path selected | No file selected; Download and Choose file available. |
| Existing file being inspected | Filename remains visible; Checking. No download progress. |
| Successful inspection | Filename and Checked, optionally with the group count. |
| Referenced file missing | Filename remains visible; File not found. Preserve the saved path. |
| File cannot be read | Filename remains visible; Cannot read file, with access diagnostics. Do not call it corrupt. |
| Validator lacks the operation | Filename remains visible; Validation unavailable. Explain that the selected Core cannot check databases. |
| Validator unavailable, timed out, or returned an invalid response | Validation failed/unavailable with the specific cause in diagnostics, not an unsupported-format assertion. |
| Validator positively rejects the database content/schema | Invalid or incompatible database with the authoritative reason. |
| Download/update underway | Existing per-row progress and cancellation; preserve the current selection until replacement succeeds. |
| Replacement fails or is canceled | Keep the previous selected file and its known status; report the failed attempt separately. |

Countries and Sites are independent. Support both empty, one selected, both
selected, checking, and mixed success/error states. A successful result for one
row must not clear the other row's error. Apply still enforces all existing
cross-profile and active-rule requirements.

Opening, inspecting, canceling, or failing to inspect must never clear or rewrite
`geodata/paths`, change managed profiles, delete selected files, download anything,
or switch the validation backend. A cached previous success is not evidence that
the currently selected Core supports validation. Only explicit valid Apply may
persist a changed selection. Preserve relative-path resolution against a profile;
an absent resolution context must be reported instead of guessing another file.

## Confirmed diagnosis

Read-only inspection during design established:

- Saved GeoIP and GeoSite paths still reference files in the application's
  content-addressed database storage. Both files exist, and their SHA-256 digests
  match their directory names.
- The local development Core reads 260 GeoIP groups and 1561 GeoSite groups from
  those files successfully.
- The Core bundled in `build/windows-amd64/Release` identifies itself as
  `0.3.0-beta.2`, revision `66d84d8f4dcfc6ada625eb11894b19feea51b7ba`, and returns
  `unknown command` for `geodata groups`.
- `GeoDataDialog.cpp` maps passive catalog failures to Invalid file, including
  validator failures. `GeoDataSelection::requestCatalog()` currently loses stderr
  and communicates a generic diagnostic string, obscuring the actual cause.

The correction must expose this incompatibility accurately. It must not mark
unvalidated replacements as valid, silently use a different Core, or replace the
bundled/installed Core as part of this UI task. These observations describe the
inspected local build, not every installation or a promise of runtime compatibility.

## Implementation sequence

1. Add focused fixtures for catalog result classification: unsupported command,
   validator launch failure, timeout, malformed response, missing/unreadable file,
   authoritative format rejection, and success. Use synthetic executables/files
   and temporary settings rather than real user selections.
2. In `GeoDataSelection.{h,cpp}`, preserve structured failure categories and
   bounded sanitized diagnostics. Extend the existing result/cache interface
   minimally and update consumers consistently. Do not classify by translated
   display text or infer support solely from a version number. Use explicit
   capabilities or authoritative operation responses. Bound stderr capture and
   retain deadline/output limits, cancellation, and generation checks.
3. Ensure cache reuse cannot keep a stale unsupported/success result after the
   validator changes, including replacement of an executable at the same path.
   Keep file invalidation and revalidation behavior intact.
4. Refactor `GeoDataDialog.cpp` to the accepted table, source location, row menus,
   and file-information disclosure. Preserve the shared download controller and
   validated Apply path. Keep selection state separate from replacement-attempt
   status and diagnostics so failures do not falsely erase filenames or readiness.
5. Update English source strings and `assets/translations/ru.json`. Remove obsolete
   strings only after confirming they have no remaining consumers.
6. Extend the existing `ConnectionManagementTest`/GeoData fixtures with empty,
   mixed, error, and replacement states. Exercise both Settings and Routing entry
   points and verify preservation of selections, files, and managed profiles.
7. Run focused tests, diagnose failures, then relevant CI-equivalent checks using
   the existing documented build flow. Inspect native Qt screenshots and keyboard
   interactions; review the diff and update the behavioral documentation.

If result classification or layout work reveals a required scope change, update
this specification before implementing it.

## Acceptance and verification

- `geodata-selection-test`, `geodata-download-test`, and relevant
  `developer-mode-test` fixtures pass. A skipped Core-dependent fixture must be
  reported explicitly rather than counted as successful coverage.
- With the pinned Core, valid existing files show Validation unavailable and
  remain selected. With a compatible development Core, the same fixture files
  pass inspection. Neither case may mutate real user settings or switch services.
- Verify the full state table, safe Apply gating, cancellation/failed replacement,
  close/reopen, stale async callbacks, cache invalidation, and preservation of
  existing active-rule checks. No automatic fallback validator is introduced.
- Inspect English/Russian and light/dark themes on Windows at 100%, 150%, and
  200% scaling, plus Linux normal/fractional scaling where available. Check long
  paths, narrow layouts, menus, tab order, keyboard activation, Escape, expansion,
  and bounded diagnostic growth. HTML previews alone are insufficient evidence.
- Use the existing supported Windows x64/ARM64 and Linux amd64/arm64 build checks;
  report unavailable targets honestly. Packaging and the full release matrix
  belong to a release task. No release, dependency change, or Core replacement
  is included here.
- Review error classification, output bounds/redaction, persistence boundaries,
  asynchronous lifetimes, and accidental validation bypasses. Benchmarks are not
  required for layout/copy; profile only if responsiveness regresses.
- Update `geodata-routing.md` with the final user behavior and record actual test
  results and remaining platform gaps here during implementation.

Planning only: application sources and user settings are unchanged. No build or
test run was performed for this documentation-only step.

Suggested implementation commit: `fix(geodata): clarify validation states and simplify database manager`.

## Implementation results

- `GeoDataSelection` classifies every `geodata groups` run through
  `classifyCatalog()` into `CatalogFailure` (None, Missing, Unreadable,
  Unsupported, ValidatorFailed, Rejected). Missing and unreadable files are
  detected locally before any validator starts. Unsupported means a non-zero exit
  with `unknown command`; Rejected requires a `geodata_*` code or an empty group
  list. Diagnostics keep the operation, validator path, exit code, Core code and
  message, and the first stderr line; stderr capture is bounded to 4 KiB and text
  is stripped of control characters. `catalogResult` and `cachedCatalog` carry
  the category. Cached results also record the validator's file identity, so a
  Core replaced at the same path is checked again. Synchronous `inspect()` and
  `apply()` use the same classifier; the document/validate bridge reports an
  unsupported Core plainly.
- `GeoDataDialog` is rebuilt as specified: scope note, Download source above the
  table, two fixed rows, direct Download/Choose file for empty rows, a row menu
  (Update from GitHub, Choose file, Check again, Clear selection) for selected
  rows, and File information / Error details. The draft selection and its status
  are stored apart from the latest replacement attempt. A relative path without a
  profile context reports Cannot resolve path. Empty rows show a failed attempt
  in their status line only; selected rows keep their status and report the
  attempt once below the table.
- The profile editor leads catalog errors with the same classified explanation.
- 65 strings were added to `ru.json`; 18 strings of the old dialog with no
  remaining consumer were removed.

Deviations, recorded rather than silently changed: the validator version is not
queried (it would need another process per check); diagnostics show the validator
path instead. The ⋯ row button uses the ordinary button style with a larger glyph.

Verification on Windows 10 x64 (Release, Qt 6.8.3):

- `ctest -C Release` in `build/windows-amd64`: 8/8 passed without a Core.
  `geodata-selection-test` adds Core-free classification cases (launch failure,
  timeout, output limit, crash, unsupported, malformed, rejected, unreadable,
  missing, unclassified, success, empty and invalid groups, bounded/sanitized
  text), process-level fixtures through its own synthetic validator, Apply with an
  unsupported validator, and cache invalidation after replacing a validator copy.
  `developer-mode-test` adds `geoDataManagerStates` through a synthetic validator:
  empty, checked, long names, unsupported, rejected, mixed checked/missing,
  clearing, failed update, and a rejected downloaded candidate; every case
  verifies that `geodata/paths` and selected files are unchanged.
- With `BIMERON_GEODATA_CORE` set to a local development Core (`0.1.0-dev`,
  revision `7bac1e01634f`): `geodata-selection-test` and `geodata-download-test`
  passed; in `developer-mode-test`, `profileGeometry` with the real-Core
  `downloadFeedback`, `run`, `geoDataManagerStates`, and `settingsGeoDataChecking`
  (run early in a temporary local build) passed. `sharedGeoDataPreparation` still
  stops in its Raw editor fixture, exactly as on unmodified `main` with the same
  Core, so its updated assertions remain unverified.
- The pinned `0.3.0-beta.2` Core answers `error: unknown command` with exit code 1
  for `geodata groups`; the synthetic unsupported validator reproduces exactly
  that response, and the manager shows Validation unavailable while keeping both
  files selected.
- Native Qt captures (`developer-mode-test --connection-preview <file> <en|ru>
  --geodata-preview <empty|checked|unsupported|mixed|failed>`) were inspected in
  English and Russian, light and dark themes, at 100%, 150%, and 200% on a 150%
  display, collapsed and expanded. Inspection found and fixed an unreadable
  disclosure in the dark theme and a duplicated failure message.
- Linux amd64 (WSL Ubuntu, offscreen CTest): 8/8 passed. Linux visual inspection,
  fractional scaling, and both ARM64 targets were not run; CI compiles all four.
- No release, packaging, dependency change, or Core replacement. Benchmarks and
  profiling: not applicable.
