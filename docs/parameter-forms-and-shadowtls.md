# Parameter forms and ShadowTLS

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

General contains endpoint/credentials; Parameters / ShadowTLS contains protocol
options and independent outer credentials in a scrollable form. Inactive populated
Trojan fields remain visible and block incompatible saves until explicitly cleared.
Invalid imported settings remain Raw-only. The selected Core is the final validator;
GUI constraints are a conservative representation guard, not a duplicate parser.

The ProfileStore connection label recognizes these explicit types; start continues
through existing asynchronous CLI validation and authoritative Core IPC. This
small label addition does not add protocol-specific connection commands.

## Verification (October 29, 2026)

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
