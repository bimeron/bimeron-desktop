# Tray interaction

The tray icon opens the main window when the desktop reports a primary click or
double click. Its menu contains connection status, traffic, profiles, Open
Bimeron, and Quit. The menu must be populated before the icon is registered
with the desktop, because Linux status notifier hosts can read it without
opening a local Qt menu. Profile and connection changes update its contents.
Ubuntu GNOME uses the icon variant for a dark panel even when the application's
Qt palette is light. GNOME and Ubuntu session identifiers select this variant;
on Ubuntu with no desktop identifier, the dark-panel variant is the fallback.
Other Linux desktops continue to use the Qt color scheme.

Acceptance criteria: on Windows and Ubuntu GNOME, clicking the icon provides a
working route to Open Bimeron and Quit; selecting a profile starts or switches
to it. On desktops that report a primary activation, clicking the icon opens
the window directly. Closing the window keeps the tray icon available. On the
dark Ubuntu GNOME top bar, both connected and disconnected icons are legible.

The profile check mark reflects the core's reported connection state, not a
menu click. Clicking a checked profile requests a stop; the check remains until
the core reports that the session has ended. Clicking an unchecked profile
requests a start or a switch, and it is checked only after the core reports a
working connection. A failed start leaves it unchecked. A profile switch starts
the next profile only after a successful stop; a failed stop cancels the queued
start.

Acceptance criteria: disconnecting from the tray stops the active tunnel and
keeps the check during cleanup; a stop error leaves the next profile unstarted.
After a failed start, the menu shows no connected profile and retrying from the
tray follows the same path as the GUI. Repeated clicks during stopping do not
start another session.

On Linux, the core must fully release the TUN interface before its successful
stop response. A second start of the same profile must not encounter an
interface left by the previous session or enter a retry loop over that error.
The GUI logs a persistent core `LastError` once per distinct occurrence rather
than once per status poll.
