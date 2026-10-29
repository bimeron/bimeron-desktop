# Protocol URI import

Target Core: v0.3.0-beta.1, revision 9f527efe0a4959786084675f1ff795edb8a0d910.
The v0.2.0-beta.1 bundle pin is obsolete for these protocols.

Import VMess, Trojan, TUIC, HTTP/HTTPS, AnyTLS, Naive HTTPS/QUIC and MASQUE
as dedicated AccessKey sources. Preserve URI bytes after outer trimming.
Core validation is authoritative; new sources remain protected Raw-only.
Reject control characters and profiles exceeding 64 KiB. Preserve atomic writes,
reference safety, secret redaction and authoritative connection management.

Acceptance: each scheme creates the correct source, round-trips unchanged,
receives a protocol label and uses existing CLI validation before start.
Invalid imports leave no profile. Test encoded credentials, IPv6, omitted ports,
VMess Base64 variants, invalid options, CA references and secret sentinels.
Run existing CTest and four-target GUI builds; Windows ARM64 is compile-only.
Use BIMERON_PRESERVATION_CORE with the exact target Core for parser checks.

Stage 5 owns generic dispatch and import integration; stage 4 owns SS branches;
stage 7 owns SSH branches/authentication. Stages 4 (#12) and 7 (#13) are already merged; stage 5 integrates on
main 1afd2c9, preserving SSH signatures, authentication and SS editor behavior. Shared files include ProfileStore.cpp, MainWindow.cpp and profile tests.
Do not broaden editor/model refactors.

Before release, separately verify published target assets, signed compatibility
metadata, CLI/runtime interoperability, update the bundle pin and four hashes,
and complete the supported build/test/package matrix. This stage does not
authorize packaging or release and does not establish live interoperability.

The Add dialog uses a short URI placeholder and fixed width. The profile-name
row appears above the URI only when a recognized URI is entered; the height
adjusts when this row appears or disappears. Hide Paste
for unsupported clipboard content. URI input is always visible; there is no
Show/Hide control. Save requires a recognized URI and a nonempty profile name.

The Add dialog heading has a question-mark help control after its text. Clicking
it shows a tooltip listing all supported URI schemes without exposing clipboard or
profile data. Leaving the help control hides the tooltip.

The VLESS editor tab is visible only for VLESS sources, never other protocols.

## Integrated verification

Integrated on main 1afd2c9 after stages 4 (#12) and 7 (#13). Windows amd64 and
Linux amd64 passed all three existing CTest executables. Windows ARM64 and Linux
arm64 cross-builds passed; no ARM64 runtime pass is claimed. Windows contract
fixtures used Core v0.3.0-beta.1 revision 9f527efe0a4959786084675f1ff795edb8a0d910.
The Add dialog regression covers SSH URI selection with identity/known_hosts
controls retained, conditional name/Paste visibility and stable width.
Review retained SSH authentication, SS editor extensions and stage 2/3 guards.
No performance-sensitive runtime path changed; no benchmark was needed.
Live protocol interoperability, installed-service compatibility and complete
English/Russian 100/150 percent visual verification remain separate checks.
