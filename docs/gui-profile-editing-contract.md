# Cross-platform GUI profile editing contract

This document defines GUI editing behavior for Bimeron profiles. It is intended
for implementations on any platform or UI toolkit. It is a GUI contract, not a
change to Core syntax, validation, protocol support or its document API.

## Ownership and preservation

- Keep the original document and a separate editable draft. Opening, canceling,
  or saving without changes must preserve the original bytes.
- Apply explicit user edits to represented fields. Do not materialize omitted
  defaults merely because a control displays a default value.
- Preserve untouched values, comments, unknown content, ordering and line endings.
  Refuse visual editing of ambiguous or unsupported content rather than dropping it.
- Compare decoded values when determining changes, not their quote spelling.
- Raw text and visual controls represent the same draft. Failed conversion or
  validation must retain that draft and explain why saving is blocked.

## Writing fields

New and changed assignments use `Key = Value`, including routing-rule fields.
Ordinary values are written without quotes. Quote only when needed to preserve
the exact value, for example leading/trailing whitespace, a leading double quote
or control characters. Use encoding accepted by the selected Core; never assume
that all JSON escapes and all Core string escapes are interchangeable.

Do not trim passwords or reinterpret backslashes, Unicode, `#`, `;` or `=` inside
values. Comments occupy their own lines. Preserve the spelling of untouched
quoted values. If an implementation cannot safely decode a value, retain Raw
editing instead of guessing.

Clearing an optional field removes its assignment. An absent required field
blocks saving; it must not silently turn into a default. `0` and `false` are
values, not empty fields. Omission of an explicit empty value is permitted only
when Core gives omission the same meaning (currently empty Profile Name and
Shadowsocks Prefix). Whitespace-only secrets are not automatically empty.

## Disabled ShadowTLS drafts

The enable flag is editor state, not a new Core configuration key.

| State | Serialized result |
| --- | --- |
| Enabled, complete and compatible | Active `[ShadowTLS]` with its populated fields |
| Enabled, missing required data or incompatible | Keep draft; block saving |
| Disabled, one or both fields populated | Commented header and populated fields |
| Disabled, both fields empty | No ShadowTLS section or empty assignments |

For example, a disabled draft is stored as:

```ini
# [ShadowTLS]
# ServerName = handshake.example.com
# Password = example-only
```

Opening this document restores the unchecked control and its values. Enabling it
restores one active section. Repeated toggles must not duplicate sections, fields
or comment prefixes. Disabling must not discard values. A partial disabled draft
is allowed; it does not become a valid active configuration until completed.

When disabling an active block, existing comments inside it receive one additional
outer `# ` layer. Enabling removes that layer and preserves the original comment.
For example `# note` becomes `# # note`, then returns to `# note`. Unknown fields,
duplicate active/disabled blocks and ambiguous boundaries require protected Raw
editing. Do not identify active state with a substring search for `[ShadowTLS]`.

Commenting is not encryption. Disabled credentials remain sensitive profile data
and receive the same file protection, masking and diagnostic redaction as active
credentials.

## Document boundaries

- Preserve an existing BOM exactly once at the document start. A BOM belongs to
  the document, never to a movable section.
- Preserve LF/CRLF for untouched content; new lines follow the document convention.
- Joining blocks must create a line boundary when needed. A final comment without
  a newline must never consume the following section header.
- New fields must be inserted in their active section, outside disabled blocks.
  Commented headers do not change Core's active section context, but can delimit
  editor-owned disabled drafts. Handle both interpretations safely.
- Do not globally reformat a file to fix one field. Any legacy fallback that
  rebuilds more text must document its limitation and retain sensitive drafts.

## Core integration and saving

Core remains the authority for configuration validity. A GUI may use its document
API for supported operations and perform bounded local text edits for GUI-only
presentation behavior. Apply edits against the returned document, not stale line
offsets. Do not require changes to Core for this contract.

Validate the completed draft before replacing the profile, then use an atomic,
protected write. Failure leaves the saved original intact. Static CLI validation
does not establish support in the running service or prove live connectivity.
Service start rejection must not rewrite the profile or expose credentials.

## Portable acceptance cases

Run these through the actual editor save/reopen path, with and without any document
API path the implementation supports:

1. No-op and cancel preserve bytes, BOM, CRLF and a missing final newline.
2. Disable, save, reopen and re-enable a populated ShadowTLS block; verify exact
   values, inactive state, comments and no duplicates.
3. Save a new partial disabled draft, edit it again, clear both fields and verify
   removal. Reject activation with missing required data.
4. Preserve quoted whitespace, Unicode, quotes, backslashes and delimiters in
   credentials; unsupported encodings remain safely editable in Raw.
5. Clear optional fields; retain explicit zero/false and omitted defaults.
6. Combine field edits with rule insertion, deletion, reordering and toggling;
   format changed/new assignments while retaining untouched content.
7. Move a first ShadowTLS block in a BOM-prefixed document, and move an EOF block
   ending in a comment without a newline. The result must retain valid boundaries.
8. Add a field before a trailing disabled block and reopen successfully.
9. Reject ambiguous sections without changing bytes; validation/write/service
   failure must preserve the original and redact diagnostics.

Desktop implementation details and current verification limits are recorded in
[Parameter forms and ShadowTLS](parameter-forms-and-shadowtls.md). Other GUI
implementations should record their own evidence against these cases.
