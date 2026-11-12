# Desktop navigation acceptance criteria

The main window opens on Profiles. Its persistent sidebar switches between
Profiles, Settings, Log, and About, with the active section highlighted.
The platform command modifier selects Profiles with 1, Settings with comma,
Log with 3, and About with 4. Command/Control+N opens Add profile only on Profiles.

Selecting a profile highlights it. Shift+Up and Shift+Down move the selected
profile one row, preserve its selection, and do nothing at either boundary.
Delete requests confirmation naming that profile. The row pencil edits that
profile. Add profile opens the unified link/file dialog with Create in editor. Connection
controls follow [connection management](connection-management.md): selection alone
does not connect; Connect, double-click, Return/Enter and tray activation share
the same start/stop/switch path. Status and traffic come from the active Core.

Add profile keeps its input after a validation or write error. Paste accepts
supported links and absolute file paths. See [profile creation](profile-creation.md). The editor has Basic,
VLESS, Network, Routing, and Raw config tabs. Routing commands affect rules
within the profile, and unavailable commands are disabled. Switching from
edited raw text to a visual tab runs the same core CLI validation used by Save;
invalid text keeps Raw config active and displays Error with OK. Enter saves;
Escape cancels and asks whether to discard edits if the draft changed.

Theme and language remain available in Settings. Pending preferences remain
disabled except the restored auto-connect and explicit stop-Core-on-exit options.
The Settings header holds the Country and site databases action, because the
databases are shared by every profile. The Application card groups appearance,
general behavior, and startup preferences with thin dividers; launch at login is
followed by a single Auto-connect selector (see
[connection management](connection-management.md)).
Save log and Clear are disabled while the log is empty. Tray profile entries
follow the same availability gates as the connection button; Open Bimeron and
Quit remain available. Closing to tray does not disconnect.
