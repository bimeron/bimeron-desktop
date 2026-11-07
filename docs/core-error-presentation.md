# Core error presentation

Tracking: issue #24. Core contract: Core `docs/ERRORS.MD` and ADR 0035.

## Problem

Saving or importing a profile showed raw Core text such as
`Core rejected the configuration: error: [Interface] Name must start with ...`,
or the editor's generic "Unsupported parameter combination" message. Users could
not tell which field was wrong. Connection failures were logged as bare codes.

## Behavior

- The GUI loads the error catalog of the bundled or selected Core CLI with
  `bimeron errors --json` and caches it per executable path, size and
  modification time. A Core without the command yields no catalog; every flow
  then keeps its previous behavior.
- With a catalog, profile validation always uses `validate --json` and reads
  `problems`. Decisions use the problem `code` and location only, never the
  English `message`.
- The profile editor runs Core validation before its own combination check, so
  the user sees the precise Core problem. The editor's generic check remains as a
  last guard for combinations Core accepts but the visual editor cannot represent.
- A problem dialog shows `<field label>: <catalog text>` in the interface
  language. The label is the editor's own form label for the mapped field.
  Problems without a field show the catalog text alone; a known profile line is
  added when no field can be shown.
- Go to field opens the tab containing the field, scrolls to it, focuses it and
  outlines it in red until the user edits it. Rule problems select the active rule
  row (Core counts active rules; disabled rows are skipped). Problems without a
  visible field open Raw config at the reported line.
- Secret fields listed by the catalog show a fixed mask of eight bullets when
  Core reports `value: present`, or "not set" otherwise. Show value reveals the
  value from the editor field only; Core never sends it.
- Details shows the code, the location and the English Core message.
- Messages outside the editor (import, post-save checks) use the same catalog
  texts with generic field labels.
- A rejected control `start` (`invalid_configuration`) appends the described
  problems to the error dialog and log.
- `Status.LastError` is logged as `Core: <catalog text> (<code>)`. Unknown codes
  keep the previous diagnostic text.

## Field mapping

`ProfileEditorDialog::problemWidget` maps `Section.Field` keys to editor
widgets. Several widgets may represent one canonical field (for example
`AccessKey.ServerName` for VLESS and for generic parameters); the first visible
and enabled widget wins. Related fields are tried next, then the URI/key field
when the problem names a `source`. Canonical names follow Core `docs/ERRORS.MD`.

## Acceptance criteria

- Parsing ignores malformed problems, bounds sizes and accepts only identifier-
  shaped locations.
- Catalog texts fill `{name}` placeholders and fall back to `unclassified` for
  unknown codes; a catalog without `unclassified` is rejected.
- Secret masks never depend on the value length and never contain the value.
- Save with a Core problem shows the problem dialog, keeps the draft and does
  not accept the dialog; Go to field selects the tab, outlines the field and the
  outline clears on edit; unknown locations open Raw config at the line.
- Older Cores without `errors --json` keep the previous validation and messages.

`profile-vless-test` covers these criteria with injected validators and an
injected catalog; it needs no Core executable.
