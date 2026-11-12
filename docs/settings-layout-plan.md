# Settings layout and startup selection plan

Status: implemented on `feat/settings-startup-selector`; see Implementation results.

The user selected variant A from the second design review: the database action
in the Settings page header and a single auto-connect selector. This document
records the implementation specification and acceptance criteria. The inline
HTML prototype is a visual reference, not production UI or an additional runtime.

## Existing behavior

- `MainWindow::settingsPage()` puts the shared database action between statistics
  and startup preferences in the Application card.
- Auto-connect uses a checkbox and a separate `HiDpiComboBox`, disabled while the
  checkbox is unchecked. `refreshProfiles()` repopulates that combo box.
- Preferences use `app/autoConnect` and `app/autoConnectProfile`. The latter stores
  a stable profile ID; displayed names come from `profiles_[row].name`.
- Native IKEv2 profiles are excluded from the startup selector.
- Startup connection is already gated by connection readiness and attempted once.
  A missing profile does not cause selection of another profile.
- The database dialog is shared by Settings and the profile editor. Databases
  apply across profiles, rather than belonging to one profile.

## Accepted interface

1. Put Settings on the left and the existing Country and site databases action
   on the right of the page header. Preserve the action's `settingsGeoData`
   object name, text, shared dialog, and passive opening behavior. Use a normal
   secondary Qt button with the application's existing control styling.
2. Keep the page hint below the header and retain the Application and Bimeron
   service cards. Add no navigation destinations or additional cards.
3. Inside Application, keep theme and language first. Separate appearance,
   general behavior, and startup preferences with restrained horizontal dividers
   as shown in the accepted prototype.
4. Keep general behavior preferences in their existing order: stop on exit,
   notifications, and detailed statistics. Place launch with Windows/Linux next,
   followed by auto-connect.
5. Replace the auto-connect checkbox and disabled combo with one always-usable
   `HiDpiComboBox`. Its label is Auto-connect, with the secondary hint When Bimeron
   starts. Right-align the field with the existing appearance selectors; give it
   enough width for profile names without squeezing its label.
6. The selector contains Off, a non-selectable separator, and eligible profiles
   in their existing order. Profile names remain unchanged. Use the existing
   HiDPI popup and keyboard behavior rather than a custom popup implementation.
7. Preserve the service row and bottom diagnostic/quit actions. Match the
   prototype's grouping and alignment using Qt logical sizing and layouts,
   rather than copying HTML pixel dimensions or fixing the page size.
8. Keep the database action reachable when the window is narrow. Allow the
   header action to wrap below the title when required. Long translated labels
   and profile names must not force horizontal overflow or push the arrow out
   of the selector. Truncated profile names must be available in a tooltip and
   readable in the popup.

English source strings and natural Russian translations belong in the existing
translation system. Reuse existing strings where appropriate. No new icon asset
is necessary for this change; the action must remain clear from its label.

Eligible profiles are exactly those the startup path can connect: packet
profiles with a connectable protocol (`MainWindow::packetProfile`). This excludes
native IKEv2, as before, and also unknown-protocol profiles, which the former
list showed although startup silently skipped them.

## Selection and persistence contract

| State or action | Display and effect |
| --- | --- |
| Existing auto-connect is off | Show Off, even if a remembered profile ID exists. |
| Existing auto-connect is on with an eligible profile | Show that profile by name. |
| User chooses an eligible profile | Save its ID and enable auto-connect. Do not connect immediately. |
| User chooses Off | Disable auto-connect and retain the remembered profile ID. |
| Saved enabled profile is missing or no longer eligible | Show a disabled Profile unavailable placeholder with a short explanation. Keep Off and eligible profiles selectable. Preserve saved settings until an explicit user choice. |
| No eligible profiles and auto-connect is off | Show Off and explain that no eligible profiles are available. Do not enable auto-connect automatically when a profile appears later. |
| Profiles are renamed or reordered | Refresh labels/order while preserving selection by ID. |
| Profile list is refreshed or Settings is opened | Do not write settings or start a connection. |

Retain both existing preference keys and their meanings. Off and unavailable
placeholder entries must never be interpreted as profile IDs. Separate user
activation from programmatic list refresh, and block signals during population.
Do not silently fall back to the first available profile. Duplicate display names
must still resolve to their own stable IDs.

The existing startup state machine, readiness checks, native IKEv2 exclusion,
and launch-at-login behavior remain authoritative. Selecting a profile here
configures a future application startup; it does not change the active session.

## Implementation sequence

1. Add focused selection/persistence regression cases to the existing
   `ConnectionManagementTest` harness, using temporary settings and profiles.
   Cover explicit selection/Off, initialization, refresh without writes,
   stale IDs, no eligible profiles, rename/reorder, duplicate names, and native
   IKEv2 exclusion. Retain the existing startup-once and no-fallback tests.
2. Update `settingsPage()` in `src/MainWindow.cpp`: header action, dividers,
   startup preference order, labels, and single selector. Preserve existing
   widget styling and dialog wiring.
3. Update the selector population in `refreshProfiles()` and its user activation
   handler to implement the table above. Introduce a small shared helper only
   if needed to keep initial population and refresh consistent. Change
   `src/MainWindow.h` only if that helper requires a declaration.
4. Update `assets/translations/ru.json` for new user-facing strings and inspect
   both English and Russian rendering.
5. Run focused tests, fix root causes, then run the existing relevant
   CI-equivalent checks using the supported build documented in
   `docs/build-and-packaging.md`. No build-system changes are planned.
6. Inspect native Qt screenshots and keyboard interactions, review the diff,
   and update navigation/settings documentation to describe the final behavior.
   Record actual verification results in this plan.

## Verification and completion criteria

- The relevant `developer-mode-test` coverage passes, including settings
  selection/persistence, existing startup behavior, native-profile exclusion,
  and the shared database action. Run the database-action fixture with its
  required Core prerequisite; report it as unrun if that prerequisite is absent.
- Run `geodata-selection-test` and `geodata-download-test` to check that moving
  the action preserves shared database behavior. Tests use existing local
  fixtures, not unsolicited live downloads or installed-service changes.
- Inspect the actual Qt Settings page, not only the HTML prototype, at the
  reference window size and the supported minimum size. Verify English/Russian,
  light/dark themes, Windows 100%, 150%, and 200% scaling, and Linux normal and
  fractional scaling when those environments are available.
- Inspect the open selector, long and duplicate names, empty/stale selection,
  Tab focus order, keyboard selection, Escape, and access to the header action.
- Opening the database dialog still reaches the existing manager and does not
  start a download or change database selections.
- The final diff contains only the UI, necessary selection handling, regression
  coverage, translations, and documentation. Review settings preservation,
  unintended connection attempts, signal ordering, and layout regressions.
- Benchmarks/profiling are not applicable to this layout and preference change;
  investigate only if verification reveals measurable regressions.
- This task does not prepare a release. Packaging and the complete Windows
  x64/ARM64 and Linux amd64/arm64 release matrix are deferred to a release task;
  unverified platform UI checks must be explicitly reported at handoff.

Suggested implementation commit: `feat(settings): simplify startup selection and move database action`.

## Planning verification

Read the current Settings construction, profile refresh, stored-preference
usage, database action regression test, startup no-fallback coverage, navigation
criteria, and HiDPI combo-box contract. No application source, build configuration,
or packaging files were changed during planning. No tests were run for this
documentation-only planning step.

## Implementation results

Implemented in `src/MainWindow.{h,cpp}` (`settingsPage()`,
`populateAutoConnectSelector()`, `chooseAutoConnect()`), with Russian strings in
`assets/translations/ru.json`. The selector writes settings only from
`QComboBox::activated`; population blocks signals and never writes. The obsolete
`Connect automatically on startup` string was removed.

Deviations from the planned interface, recorded rather than silently changed:

- The header action does not wrap. At the supported 720 by 600 minimum window
  the title and action fit on one row in English and Russian at 100%, 150%, and
  200%, so a wrapping layout would add code without a reachable case.
- The closed field elides long names at the end. The popup keeps the combo-box
  width and Qt's middle elision; every profile entry carries its full name as a
  tooltip, and the closed field shows the selected name as a tooltip.

Verification on Windows 10 x64 (Release, Qt 6.8.3):

- `ctest -C Release` in `build/windows-amd64`: 8/8 passed, including the new
  `ConnectionManagementTest::autoConnectSelector` (default Off without writes,
  separator skipped by the keyboard, duplicate names, Off retaining the ID,
  rename/reorder, missing and native stale IDs, placeholder activation, keyboard
  Off from the placeholder, no eligible profiles, no auto-enable on new profiles,
  header placement of the database action) and the existing startup-once,
  no-fallback, and native-exclusion fixtures.
- `geodata-selection-test` and `geodata-download-test` passed with and without
  `BIMERON_GEODATA_CORE` pointing to a local development Core.
- `settingsGeoDataChecking` passed with that Core when run in isolation. With the
  Core set, the full `developer-mode-test` stops in the earlier, unrelated
  `sharedGeoDataPreparation` Raw editor fixture; unmodified `main` stops at the
  same point with the same Core, so this is pre-existing. The pinned release
  Core lacks the database validation used by these fixtures.
- Native Qt captures through `developer-mode-test --connection-preview <file>
  <en|ru> --settings-preview <off|profile|long|stale|empty>` were inspected in
  English and Russian, light and dark themes, at 100%, 150%, and 200% on a 150%
  display, at the minimum window and at 1000 by 720, including the open
  selector, a long name, duplicate names, and the stale placeholder.
- Linux amd64 (WSL Ubuntu, Ninja, offscreen CTest): 8/8 passed. Linux visual
  inspection, Linux fractional scaling, and both ARM64 targets were not run; CI
  compiles all four targets. No release or packaging was prepared.
- Benchmarks and profiling: not applicable.
