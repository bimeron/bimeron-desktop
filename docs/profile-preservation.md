# Profile preservation and secret handling

The selected Core CLI remains the configuration validator. This document defines
Desktop editing and storage behavior, not connection support for new protocols.
It implements the preservation requirements in the Core/Desktop integration contract.

## Editing

Opening, canceling, switching tabs, and saving without changes must not rewrite
the profile, including its UTF-8 BOM, line endings, comments, and whitespace.
The existing SS, SOCKS5, VLESS, and Hysteria2 source forms remain available.
URI sources are opaque values: never regenerate them from decoded previews.

Visual editing is permitted only if every configured field, value, source, and
ordered rule survives the existing serializer. Unknown types, methods, options,
sections, duplicate/mixed sources, quoted values, and constrained values that
cannot round-trip use Raw config instead. There is no discard-unknown-settings
confirmation. Raw is authoritative until validation and representability checks
both succeed. Unsupported protocols do not acquire forms or connection support.

Raw contains secrets and requires an explicit Show/Edit action. Masked previews
are never used as saved configuration. Validation failure retains the draft.

## Storage and references

Validate drafts in the profile directory, then replace profiles atomically.
Never fall back to truncating the original. Validation, protection, and write
failures retain the previous profile. Clean up failed imports and temporary drafts.
An index failure after profile replacement is reported and retryable; the profile
remains the authoritative document.

Protect files before writing credentials: owner-only permissions on Unix and a
protected current-user/SYSTEM DACL on Windows. A protection failure prevents the
write. This is plaintext profile storage, not a credential vault.

Import preserves bytes and refuses relocation of relative File, SSH identity/
known_hosts, and CA references, including references inside supported URI/file
sources. Ambiguous references are refused rather than rewritten. Private keys
are never copied. Editing an existing profile keeps its directory as the base.

## Untrusted output

CLI and IPC free text is not trusted. Display only known safe diagnostic templates
and known error codes; otherwise show a generic redacted diagnostic. Redact before
truncation, rendering, logging, or exporting. This covers malformed/unknown URI
schemes, explicit credentials, VMess JSON, XHTTP headers, SS2022 chains, ShadowTLS,
and transient SSH answers. SSH authentication follows [SSH profiles](ssh-profiles.md); answers remain transient.

Diagnostic JSON exports retain only verified fields from the existing report
schema; unknown fields and free text are excluded. The projection retains
recognized build identities, platform, architecture, numeric counters, known
checks, states, and error codes. Unsupported report fields are deliberately
omitted. Failed/invalid reports are not published, and existing reports are not
overwritten.

## Acceptance

- Unknown Method/Type and unrepresented fields never become defaults or disappear.
- Import, edit, save, and reopen preserve the sole source and credential semantics.
- No-op operations preserve bytes; invalid Raw remains editable without data loss.
- CLI rejection, atomic replacement failure, or permission failure preserves the original.
- Relative-reference relocation leaves no imported profile or copied dependency.
- Sentinel secrets never reach previews, errors, IPC logs, log exports, or reports.
- Existing supported forms still permit intentional edits and pass regressions.
- Verify actual Windows DACLs and Unix modes, not just permission API return values.

Tests extend the existing profile and developer-mode executables. Run focused
regressions, full native CTest, and the four-target GUI compilation matrix.
Cross-compilation is not an ARM64 runtime pass. No network benchmark, connection
test, package build, release, or installed-service modification is required here.

Represented explicit SS2022 and embedded plugin fields use the visual editor; unknown plugins and incompatible Prefix combinations remain protected Raw. See [Shadowsocks extensions](shadowsocks-profiles.md).

Stage 6 adds explicit Trojan, HTTP(S), Naive H2 and MASQUE forms and a separate
ShadowTLS v3 block for represented compatible explicit profiles. URI sources and
extended XHTTP remain protected Raw. See [Parameter forms and ShadowTLS](parameter-forms-and-shadowtls.md).
This does not change the bundled Core or establish runtime interoperability.
