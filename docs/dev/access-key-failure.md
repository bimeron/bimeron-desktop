# Dynamic key access denied

When receiving an HTTP 401, 402, 403, 404, or 410 response from address `ssconf://`, core considers the key unavailable (used or revoked). The session enters state `failed` with code `access_key_unavailable` without retrying. The GUI shows a warning about the need to replace the key. The warning appears once when entering this state and does not depend on the tray notification settings.

Transient errors (including HTTP 408, 429 and 5xx, network failures) still cause reconnections. Responses and errors should not reveal the URL or key content in events, status, and interface.

Acceptance criteria: the behavior is the same for VPN and proxy, after a failure the retry counter does not increase and a new `reconnecting` state is not published, a successful new launch is possible after replacing the key, and all four supported packages contain an updated core and GUI.