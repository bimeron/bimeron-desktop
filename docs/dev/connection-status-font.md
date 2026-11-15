# Connection status font weight

The connection status line on the Profiles page uses a medium font weight for
connected, connecting, reconnecting, stopping, and failed states. This keeps
the status readable without the heavy bold appearance seen on Linux with the
previous weight of 600. The neutral disconnected state retains its normal
weight. Status colors and text are unchanged.

Acceptance criteria: the status line remains legible on Linux and Windows,
including a connected endpoint, and switching states does not make the status
line unexpectedly bold.
