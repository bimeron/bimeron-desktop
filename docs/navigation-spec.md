# Desktop navigation acceptance criteria

The main window opens on Profiles. Its persistent sidebar switches between
Profiles, Settings, Log, and About, with the active section highlighted.
The platform command modifier selects Profiles with 1, Settings with comma,
Log with 3, and About with 4. Command/Control+N opens Add key only on Profiles.

Selecting a profile highlights it. Shift+Up and Shift+Down move the selected
profile one row, preserve its selection, and do nothing at either boundary.
Delete requests confirmation naming that profile. The row pencil edits that
profile. Add key and Import .conf open their respective dialogs. VPN connection
is unavailable in this navigation phase: Connect stays disabled, status reads
Disconnected, and traffic remains zero.

Add key keeps its dialog open after a validation or write error; Show/Hide
controls key visibility and Paste reads the clipboard. The editor has Basic,
VLESS, Network, Routing, and Raw config tabs. Routing commands affect rules
within the profile, and unavailable commands are disabled. Switching from
edited raw text to a visual tab runs the same core CLI validation used by Save;
invalid text keeps Raw config active and displays Error with OK. Enter saves;
Escape cancels and asks whether to discard edits if the draft changed.

Theme and language remain available in Settings. Pending preferences remain
disabled. Save log and Clear are disabled while the log is empty. Tray profile
entries are informational; Open Bimeron and Quit remain available.
