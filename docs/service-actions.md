# Service action visibility

The service panel must show actions supported by the last confirmed service state:

| State | Visible actions |
|---|---|
| Checking, unavailable, or unrecognized | None |
| Confirmed not installed | Install and boot option |
| Stopped | Start, check for signed core update, remove |
| Running | Stop, check for signed core update |
| Paused | Stop |
| Starting, stopping, resuming, pausing | None; recheck shortly |

An action in progress disables the visible controls. After success, cancellation, or failure, the panel rechecks service state. The status label always reflects the latest service status result, even after an action fails. The action failure and its diagnostic remain in the GUI log. Emergency network recovery is shown after a failed action and remains available until a later successful action. A failed status command must not be interpreted as an uninstalled service unless its diagnostic explicitly says the service is not installed. Labels must remain readable in the Russian locale at the supported window width.

The update action checks signed GitHub releases and only offers service replacement after the new executable has been downloaded and verified. It elevates the core from the protected GUI package with `-source`, never the downloaded executable. A writable package location or bundled core without `-source` requires a GUI package update. Without a production signing key it reports that signed updates are unavailable.

Acceptance criteria: no contradictory service actions appear before the first check or after a failed action; an unavailable core offers no install action; the stopped, running, paused, and transitional states show only the actions above; after a failed action, a successful status check displaying state 4 shows running while the failure remains in the log; Russian button labels are not clipped.
