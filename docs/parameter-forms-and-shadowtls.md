# Parameter forms and ShadowTLS

## GUI configuration writing contract

The reusable platform-neutral behavior is defined in the
[GUI profile editing contract](gui-profile-editing-contract.md).

Desktop owns formatting of edited fields; Core remains unchanged and validates
the resulting configuration. Changed/new fields use `Key = Value`, quoting only
when needed to retain exact text. Untouched bytes and comments remain unchanged.
Cleared optional fields are removed, never emitted as empty assignments; required
fields still fail validation. Zero and false are values, not empty settings.
An empty Profile Name or Prefix may be omitted (their existing default is empty).
Disabling populated ShadowTLS comments its header and populated fields. Empty
disabled blocks disappear; partial disabled drafts survive save and reopen.
Enabling restores the block without duplicates. Ambiguous duplicate/unknown
wrapper blocks retain Raw protection. Tests must exercise the real document
bridge and save/reopen, not only the fallback serializer.

### Serialization verification

Windows x64 and Linux amd64 builds and all five CTest executables passed with
`BIMERON_GEODATA_CORE` pointing at existing document-capable Core binaries under
the sibling checkout's ignored `build/desktop-bridge` directory. No Core code or
documentation changed. Regression coverage includes active-to-disabled and back,
partial/new disabled drafts, empty removal, repeated cached edits, Unicode and
quoted whitespace/backslash values, required active values, reversible nested
comments, optional Prefix removal, canonical edited rules, and insertion of SSH
IdentityFile before a disabled ShadowTLS block at EOF.

Boundary follow-up: Windows x64 and Linux amd64 profile tests passed with real
document-capable Core binaries, including fallback and bridge editor cases for
a first ShadowTLS section with BOM and an EOF comment without a newline.
No-op opens, routing edits and reopen preserve valid section boundaries. The
Windows application was rebuilt after the fix; Core remained unchanged.

Unsupported Go-only quoted escape forms in a ShadowTLS block remain protected
Raw rather than being reinterpreted by the desktop decoder. Ambiguous commented
section boundaries and mismatched rule-block indexing also retain Raw protection.
Only changed rule blocks are reformatted on the Core document path. The legacy
no-document routing serializer retains its existing rule-rebuild behavior; the
original ShadowTLS comments are now carried through that fallback rebuild.
ARM64 execution/builds and live protocol checks were not repeated for this
serializer iteration. No benchmark is warranted for bounded configuration edits.

## Approved visual refinement

Rename the tab Protocol. Use the existing dialog surface without a scroll frame.
Place inner options above a separate ShadowTLS heading, About action, checkbox
and two fields with concise hints. Preserve disabled drafts. Show UDP guidance
only while enabled; retain incompatibility reasons. Move encryption details to
About ShadowTLS and remove the permanent CLI/service warning from the form.
Verify light/dark EN/RU layouts at 100/150 percent and existing preservation tests.
Both ShadowTLS field labels must remain associated with their inputs so assistive
technology exposes their names after moving them out of QFormLayout.

### Visual refinement verification

Windows x64 and Linux amd64 builds and all five CTest executables passed.
The Linux profile test also passed ten consecutive runs. ARM64 builds were not
repeated for this presentation-only iteration; prior results are historical.
No benchmarks, package or live connection checks are warranted here.

Styled synthetic previews use the real MainWindow stylesheet and application
palette, with isolated test settings and synthetic credentials:
`developer-mode-test --connection-preview <image.png> en|ru --shadowtls-preview`
(optionally `--preview-dark`), with `QT_SCALE_FACTOR=1` or `1.5`.
These replace the earlier unstyled profile fixture screenshots for appearance
review. About opens local explanatory text; it does not launch a browser.

## ShadowTLS guidance specification

Keep the existing explicit SS, VLESS, Trojan, Naive H2 and SSH forms and Raw
protection boundaries. Explain the public listener endpoint, independent outer
credentials and retained inner encryption/verification. Explain unavailable
combinations without discarding an incompatible draft.

When enabled, distinguish TCP-only SS/Naive/SSH (captured DNS over TCP, other
proxy-routed UDP dropped, direct routes unchanged) from VLESS/Trojan's existing
UDP-over-stream. Selected CLI validation does not establish installed-service
support. Source compatibility is described by Core docs/SHADOWTLS.MD and
internal/accesskey/shadowtls.go; historical revision claims below are not claims
about the currently installed service.

Acceptance: localized guidance follows protocol and wrapper state; secrets stay
masked; incompatible edits preserve values; service start rejection after CLI
acceptance reports failure without changing the profile. Check existing editor
and connection tests, EN/RU layouts at 100/150 percent and translation validity.
No new protocol forms, capability system, dependency, package or release changes.

Keep wrapping guidance outside the dynamically hidden QFormLayout rows, within
the same scroll content: Qt 6.4.2 height-for-width calculation on that form can
corrupt memory. Connection fixtures must tolerate asynchronous clipboard updates
and clipboard-dependent Paste captions.

## Accepted stage 6 specification

Target Core is v0.3.0-beta.1, revision
9f527efe0a4959786084675f1ff795edb8a0d910. Desktop form support,
selected-CLI validation, active-service support and the bundle pin are separate.
This stage does not update packages, signed manifests or compatibility gates.

Add explicit Trojan TCP/WS/gRPC TLS, HTTP/HTTPS CONNECT, Naive HTTP/2 and
MASQUE forms. Preserve optional settings and their omission. HTTP(S) has no
SNI/CA override; Naive HTTP/3 remains an opaque URI/Raw source. MASQUE exposes
ServerName, CAFile and MASQUEPath with existing reference safety.

Add a separate optional ShadowTLS v3 block with ServerName and masked Password.
Outer settings are independent of inner credentials and SNI. Offer it only for
represented compatible explicit SS, VLESS, Trojan, Naive H2 and SSH. Reject
AES-192-GCM, SS plugins, SOCKS5, HTTP(S), HTTP/3 and native IKEv2 combinations.
Other wrapped sources remain protected Raw; Core validates resolved sources.
Never automatically remove wrapper settings when changing an inner transport.

Audit existing VLESS fields against the exact Core transport matrix. Vision is
TCP REALITY only; WS requires TLS and HTTP/1.1, gRPC TLS/REALITY and H2;
XHTTP H3 requires TLS without a fingerprint and cannot use ShadowTLS.
Extended XHTTP and split-download settings remain protected Raw.

## Acceptance

- Visual edits retain every represented value and source; unsupported settings
  retain Raw protection rather than becoming defaults.
- Open, cancel and unchanged save preserve original bytes including BOM/CRLF.
- Forbidden combinations are not offered; incompatible edits retain the draft.
- Selected-Core rejection, atomic-write or protection failure retains originals.
- Relative references retain their meaning; file selection uses absolute paths.
- Secrets remain masked and absent from diagnostics, logs and settings.
- Existing SS extensions, SSH authentication and connection management regressions
  remain enabled. No URI-to-parameter conversion or shared model refactor.

Extend profile-vless-test with positive/negative round trips and exact-Core
validation via BIMERON_PRESERVATION_CORE. Run native CTest on Windows x64 and
Linux amd64/arm64, compile all four supported targets, inspect EN/RU forms at
100/150 percent, validate translation JSON, documentation links and diff hygiene.
Compilation and static validation do not establish live interoperability.
No benchmark is required for bounded form/serialization changes.

## Integration and release boundary

Integrate stage 6 before rebasing stage 8 native IKEv2 changes. Coordinate shared
editor/header, profile tests, translations and contract documentation sequentially.
Keep IKEv2 outside packet forms and daemon start. Preserve stages 2/3/4/5/7.

The current v0.2.0-beta.1 bundle does not implement these new protocols. Published
target assets, four archive hashes, signed compatibility metadata, runtime checks
and the complete MSI/DEB release matrix remain a separate pre-release dependency.

## Fields and editing

Trojan represents Server/Port/Password, optional ServerName, Network tcp/ws/grpc,
Security tls, ALPN, WSPath/WSHost and GRPCServiceName/GRPCAuthority/GRPCMultiMode.
Empty transport/security preserve omission; Core defaults apply. HTTP/HTTPS
represent endpoint and optional paired credentials. Naive H2 requires paired
credentials and optionally ServerName. MASQUE represents endpoint, optional
paired credentials, ServerName, CAFile and MASQUEPath. The CA picker stores an
absolute path; existing relative CAFile remains relative to the profile directory.
No CA or private-key dependency is copied.

General contains endpoint/credentials; Protocol contains protocol
options and independent outer credentials in a scrollable form. Inactive populated
Trojan fields remain visible and block incompatible saves until explicitly cleared.
Invalid imported settings remain Raw-only. The selected Core is the final validator;
GUI constraints are a conservative representation guard, not a duplicate parser.

The ProfileStore connection label recognizes these explicit types; start continues
through existing asynchronous CLI validation and authoritative Core IPC. This
small label addition does not add protocol-specific connection commands.

## Verification (October 29, 2026)

### Guidance follow-up (November 3, 2026)

Windows x64 and Linux amd64 builds and all three CTest executables passed.
Windows ARM64 and Linux arm64 cross-builds passed; native ARM64 execution remains
unverified. EN/RU synthetic form previews were generated with scale factors 1
and 1.5 and inspected for wrapping and masked passwords. Translation JSON,
local documentation links and diff whitespace checks passed.

Added coverage for wrapper guidance state, incompatible SS draft preservation,
and CLI acceptance followed by service rejection with unchanged profile bytes
and redacted diagnostics. Review corrected UDP guidance on incompatible drafts.
The Qt 6.4.2 form-layout failure was reproduced before the layout fix; the fixed
Linux profile test passed 20 consecutive runs. Focused Valgrind checking reported
zero invalid accesses with undefined-value reporting disabled (not a blanket
Valgrind-clean claim). Clipboard fixtures now await state changes and account
for clipboard-dependent captions and geometry.

No benchmark is warranted for these bounded UI changes. No package, release,
installed-service mutation or live ShadowTLS interoperability test was performed.

Windows amd64 and Linux amd64 GUI builds and all three CTest executables passed.
Both platforms ran BIMERON_PRESERVATION_CORE fixtures against clean source-built
Core revision 9f527efe0a4959786084675f1ff795edb8a0d910 (modified=false).
Positive fixtures cover explicit forms, wrapped SS/SSH/Trojan/Naive and VLESS
H2/H3; negative fixtures cover credentials, transports and wrapper exclusions.
The VLESS assertion checks configured values rather than requiring input field
order, because intentional visual edits use the existing serializer order.

Windows ARM64 and Linux arm64 cross-builds passed; native ARM64 execution remains
CI/device evidence, not a local runtime claim. EN/RU synthetic previews were
rendered at scale factors 1 and 1.5. Inspected Trojan and MASQUE layouts fit and
mask credentials; these fixtures omit the installed application's resources/styles.
Use profile-vless-test --parameters-preview <image.png> en|ru with
BIMERON_PREVIEW_TYPE=trojan|http|https|naive|masque and optionally
BIMERON_PREVIEW_PAGE=general to reproduce them.

Review covered secret output, unchanged/Raw preservation, inactive draft retention,
separate outer credentials, unknown fields, source labels and unchanged connection
lifecycle. Translation JSON, local documentation links and git diff --check passed.
No benchmark is warranted for form/serialization changes. No package, release,
installed-service change or live protocol connectivity test was performed.
