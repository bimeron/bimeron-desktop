# Default country and site databases

Status: specification agreed; tracked in issue #34; implementation not started.

Bimeron does not author the country (GeoIP) and site (GeoSite) databases. It
ships and selects, by default, the third-party databases published by
Loyalsoldier (`Loyalsoldier/v2ray-rules-dat`, GPL-3.0), keeps them current on
request, and lets the user replace or disable them. The interface must attribute
the databases to their author and must not present them as Bimeron's own.

This plan builds on the shared database manager in
[geodata-manager-redesign-plan.md](geodata-manager-redesign-plan.md) and the
routing behavior in [geodata-routing.md](geodata-routing.md).

## Facts that constrain the design

- Loyalsoldier publishes a new release every day at about 22:04 UTC.
- `geoip.dat` is about 16.5 MB and `geosite.dat` about 11.4 MB (about 28 MB per pair).
- The data is licensed under GPL-3.0. Redistributing it inside our packages
  requires shipping the license text and a pointer to the source.
- The pinned Core `v0.3.0-beta.2` cannot check databases (`geodata groups` is
  unknown). `bimeron-core` master supports it but is unreleased.
- Updated downloads are stored content-addressed under
  `QStandardPaths::AppLocalDataLocation/geodata/<sha256>/<kind>.dat`, and
  nothing currently removes superseded copies.

## Agreed decisions

### Source, storage and selection

1. **Initial source.** Packages contain one pinned Loyalsoldier release, verified
   by SHA-256 at build time, like the pinned Core. On first launch the
   application checks once for a newer release.
2. **Bundled location.** Bundled files are read-only package data:
   `C:\Program Files\Bimeron\geodata\` on Windows and
   `/usr/share/bimeron/geodata/` in the DEB. The installer adds, replaces and
   removes them. Updated releases are stored in application data as today. The
   newest verified Loyalsoldier copy is used.
3. **Per-kind source.** Each kind (countries, sites) has one source:
   Loyalsoldier, own file, or none (disabled).
4. **Migration of existing selections.** A saved path inside the application's
   managed database store becomes Loyalsoldier with automatic updates. Any other
   saved path becomes an own file without updates. An empty selection becomes
   Loyalsoldier.

### Automatic updates

5. **One setting.** A single checkbox above the table, "Update Loyalsoldier
   databases automatically", applies only to kinds whose source is Loyalsoldier.
   Own files and disabled kinds are never updated.
6. **Schedule.** Check once a day while the application runs, plus "Update now"
   in the row menu. Download only when a newer release exists.
7. **Failure.** Keep the current database, retry later, and show "Last check
   failed" in the manager. No system notification.
8. **Taking effect.** When no VPN is connected, the new database applies on the
   next connection without a prompt. When a VPN is connected, ask "New databases
   were downloaded. Reconnect now?" with Now and Later.
9. **Old copies.** Keep the current and one previous Loyalsoldier copy; delete
   older managed copies automatically. Never delete own files. There is no manual
   delete action.

### Manager interface

10. **Row actions.** Every row has one "Actions" button with a chevron (the
    application's secondary button), replacing the ellipsis button. Menu items:
    - Loyalsoldier row: Update now, Choose own file, Check again, Disable database.
    - Own file row: Install Loyalsoldier database, Choose another file, Check
      again, Disable database.
    - Disabled row ("Not used"): Install Loyalsoldier database, Choose own file.
11. **Attribution.** No badges. The source appears as text in the row status line:
    "Loyalsoldier · release DD.MM.YYYY · checked" or "Own file · checked". Above
    the table: "Loyalsoldier databases are used by default · GitHub (GPL-3.0)",
    with a link to the source repository.
12. **Restore.** The footer button "Install Loyalsoldier databases" sets both kinds
    to Loyalsoldier and enables automatic updates. "Install Loyalsoldier database"
    in a row menu does the same for that kind, also enabling automatic updates.
13. **Disable.** "Disable database" only removes the selection; files stay on
    disk. The manager does not list affected profiles or warn about them.
14. **Persistence.** Changes are a draft until Apply; Cancel discards them.

### Profiles that need a missing database

15. A profile whose enabled GeoIP or GeoSite rules need a kind that is disabled,
    missing, or fails its check cannot connect. Nothing is shown in the profile
    list.
16. **On connect.** Show the application's ordinary message box titled "Profile
    unavailable" with a plain explanation, for example: "Could not connect
    "Work". The profile has enabled country rules, but no country database is
    selected. Choose a database or disable these rules in the profile." Buttons:
    Manage databases (default), Edit profile, Close. The reason names the actual
    failure, such as "the country database failed its check: file not found".
17. **Automatic connection at startup.** No message box: show a system
    notification and write the reason to the log.
18. **Routing tab.** For a rule whose kind has no usable database, the Target
    field is disabled; the Enabled checkbox and Type stay editable, so the rule
    can be disabled or changed. An orange line under the rule table explains:
    "No country database is selected: while a country rule is enabled, the
    profile will not connect. Choose a database in Manage databases or clear the
    rule's checkbox." Disabled rules are written as comments, as today.
19. **Saving.** A profile with such rules can be saved; only connecting is blocked.
20. **Running connections.** Disabling a database never interrupts an active VPN;
    the block applies to the next connection attempt.
21. **Labels.** "Manage databases" (connect message box and routing tab) and
    "Routing limitations" are written without an ellipsis.

### Prerequisite

22. Release a Core beta from `bimeron-core` master and pin it in Desktop before
    enabling bundled or updated databases. Accepting databases on SHA-256 alone,
    without the Core check, is rejected.

## User-facing strings

| English source | Russian |
| --- | --- |
| Actions | Действия |
| Update now | Обновить сейчас |
| Choose own file… | Выбрать свой файл… |
| Choose another file… | Выбрать другой файл… |
| Disable database | Отключить базу |
| Install Loyalsoldier database | Установить базу Loyalsoldier |
| Install Loyalsoldier databases | Установить базы Loyalsoldier |
| Update Loyalsoldier databases automatically | Обновлять базы Loyalsoldier автоматически |
| Loyalsoldier databases are used by default | По умолчанию используются базы Loyalsoldier |
| Own file | Свой файл |
| Not used | Не используется |
| Last check failed | Последняя проверка не удалась |
| New databases were downloaded. Reconnect now? | Скачаны новые базы. Переподключиться сейчас? |
| Manage databases | Управление базами |
| Routing limitations | Ограничения маршрутизации |
| Edit profile | Изменить профиль |

Final wording of explanatory sentences is reviewed in the actual Qt dialogs in
both languages.

## Open question

- Ellipses: the application otherwise ends dialog-opening button labels with "…"
  (for example "Choose file…", "Save diagnostic report…"). Removing it only for
  the labels in decision 21 makes adjacent buttons inconsistent. Recommendation:
  remove ellipses throughout the application in a separate consistency change.

## Acceptance criteria

- A fresh install selects both Loyalsoldier databases and enables automatic
  updates; country and site rules work offline immediately.
- Upgrades migrate saved selections exactly as decision 4 states.
- The manager shows every state above in English and Russian, light and dark
  themes, and 100%, 150% and 200% scaling, without clipped buttons.
- Choosing an own file or disabling a kind stops updates for that kind only.
  Both install actions restore Loyalsoldier and enable automatic updates.
- An update failure keeps the previous working database and is visible in the
  manager. A database is replaced only after a successful Core check.
- Connected sessions are never interrupted by database changes; the reconnect
  prompt appears only while connected.
- After an update, at most two managed Loyalsoldier copies per kind remain; own
  files are untouched.
- A profile with enabled rules for an unusable kind cannot connect, shows the
  message box from decision 16 on manual connect, and only notifies on
  automatic connection. It can still be edited and saved.
- Packages contain the Loyalsoldier license and source notice; About shows the
  attribution.
- No setting, profile or file outside the documented paths changes merely by
  opening, checking or canceling.

## Implementation sequence

1. Core release: tag a beta from `bimeron-core` master, then pin its tag, revision
   and four SHA-256 values in `cmake/core-release.cmake` (release task; follow
   `docs/build-and-packaging.md`).
2. Row actions: replace the ellipsis button with "Actions" in the current manager.
3. Selection model: per-kind source, the automatic-update setting, migration,
   manager states, install actions and attribution text.
4. Connect gating and routing tab: the message box, startup notification, disabled
   Target field, routing explanation, saving with missing databases, and labels.
5. Packaging: pin a Loyalsoldier release (tag and SHA-256) in CMake, stage the
   files and license into MSI and DEB, and update `docs/build-and-packaging.md`
   in the same change.
6. Automatic updates: daily check, Update now, failure state, reconnect prompt
   and cleanup of superseded copies.
7. Documentation, translations, the CI-equivalent test matrix and native visual
   checks.

Each step is a separate reviewed change with focused tests. Steps 2 and 4 do not
depend on step 1; steps 3, 5 and 6 need a Core that can check databases.

## Risks

- Package size grows by about 28 MB before compression for each of the four
  packages.
- GitHub may be unreachable; the bundled release keeps rules working, and checks
  retry later.
- Each Windows user keeps their own updated copies in application data.
- The service reads databases from per-user application data, as it does today;
  this must be rechecked for the bundled directory on both platforms.
