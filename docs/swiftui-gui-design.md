# Source-derived Qt design for the SwiftUI implementation

## Evidence and acceptance scope

Baseline: Desktop `7f44c7309fc4cf9e91554f6bed8f167cefcda35d`.
Companion: [screen/state reference](current-gui-reference.md) and
[macOS architecture](macos-app-spec.md). This is a source inspection, without
running a GUI or comparing screenshots. Values below are code values in Qt
logical pixels, not measurements of a rendered window. SwiftUI points are a
proposed starting mapping, not a pixel-perfect guarantee. Preserve compact
hierarchy and interaction rather than introducing a new product design.

## Typography and layout ledger

Source: [MainWindow.cpp](../src/MainWindow.cpp), constructor stylesheet,
`profilesPage`, `settingsPage`, `logPage`, `aboutPage`, `developmentPage`;
[ProfileEditorDialog.cpp](../src/ProfileEditorDialog.cpp), constructor.

| Element | Exact source value | Suggested SwiftUI implementation |
| --- | --- | --- |
| Main content window | Initial and minimum 720 × 600; root margins/spacing 0 | Minimum content size; HStack(spacing: 0); title bar is extra native chrome |
| Sidebar | Fixed 150; margins left/top/right/bottom 10/14/10/10; brand gap 14 | Fixed-width VStack, no automatic sidebar collapse at reference size |
| Base text | QMainWindow 12px; no family forced here | System font starting at 12pt, verify inherited dialog text separately |
| Brand | Literal ◈  Bimeron; 15px, weight 700, padding 3 | Text with bold system font; preserve wordmark rather than invent logo |
| Page title | 17px, weight 700 | System 17 bold |
| Section title | 13px, weight 700 | System 13 bold |
| Tabs / headers | Selected tab 600; header 600 | Semibold; selected underline |
| Connection status | Connected/transitional/error weight 500; neutral no extra weight | Medium state text, distinguish by words as well as color |
| Profiles page | Margins 18/16/18/16; spacing 10 | Padded VStack; list expands; actions stay at bottom |
| Settings page | Margins 20/18/20/18; spacing 12 | Same compact cards, not a redesigned preference grid |
| Application card | Margins 14/12/14/12; spacing 7 | Rounded bordered VStack |
| Service card | Margins 14/12/14/12; spacing 8 | Separate backend-dependent card |
| Log/About/Development | Margins 18/16/18/16 | Match page alignment |
| Theme/language | Minimum width 125 | Trailing picker; label on left with spacer |
| Auto-connect row | Spacing 10, profile selector stretches | Toggle plus enabled-dependent picker |
| Profile editor | Initial 680 × 520, minimum 620 × 460 | Resizable sheet/window; preserve minimum content size |
| Add profile | Fixed width 440; dynamic sizeHint height; URI/name min height 30 | Sheet width target 440, intrinsic conditional rows; never clip RU |
| Raw | Fixed system font, no wrapping | Monospaced editor with horizontal scrolling and explicit reveal |
| VLESS XHTTP extra | Fixed height 72 | Multiline field, though unrepresentable extended values remain Raw |
| Routing table | On 48, Type 120, Destination stretch, Action 136; row 30 | Table with ordered stable IDs; selected row actions |

Unspecified form margins, layout spacing, font families and dialog button sizes
come from Qt/platform style defaults. They are not zero and are not measured
here. Do not infer absolute baselines/line heights from font-size declarations.
Native SF metrics differ from Windows/Linux fonts; inspect actual Qt reference
font and line wrapping before accepting point-size substitutions.

## Palette ledger

Source: `themedStyle` in [MainWindow.cpp](../src/MainWindow.cpp) and
`applyThemePalette`, `appDarkMode` in [WindowsTheme.h](../src/WindowsTheme.h).
Hex values are sRGB code literals. QSS token substitution and palette roles are
separate mechanisms; do not assume every custom-painted element uses the same
substitution. Store named light/dark SwiftUI colors for the corresponding roles.

| Role / QSS token | Light | Dark substitution |
| --- | --- | --- |
| Window | #f8fafc | #181e27 |
| Sidebar | #f1f6fc | #202937 |
| Card / base white | #ffffff | #202937 |
| Alternate field/header | #f7f9fc | #263244 |
| Hover/selection surfaces (#f2f7fd, #edf5ff, #f7fbff) | As named | #30445d |
| Pressed/selected (#e1edfb, #dcecff) | As named | #365579 |
| Disabled background / row separator | #edf1f5 | #2b3544 |
| Border (#dce5ef, #d6dee7, #e5ebf2) | As named | #40516a |
| Control border | #ccd8e5 | #52647c |
| Checkbox border | #b8c6d6 | #657b95 |
| Main / section text (#152238, #26364a) | As named | #e8edf5 |
| Secondary text | #526174 | #c1cedd |
| Hint / neutral (#718096, #66768a) | As named | #aab8ca |
| Disabled text | #95a2b2 | #8493a7 |
| Accent (#1267d6, #1677ff) | As named | #79b4ff |
| Pressed text | #0d56b4 | #a4ccff |
| Failure (#c62828, #b42318) | As named | #ff9a92 |
| Success | #16803c | #76d49a |
| Pending/warning (#d69200, #b54708) | As named | #f3cf72 |
| Danger hover text/background | #912018 / #fff5f4 | #ffb5ae / #493039 |

Palette exceptions: Button dark base #263244, Highlight #315a88 (light #1677ff),
HighlightedText white, PlaceholderText dark #aab8ca (light #95a2b2).
System theme on Windows uses AppsUseLightTheme; other platforms use the palette.
Qt applies theme/language on restart. Native appearance adaptation is a possible
macOS deviation that must be explicitly approved/tested, not silently changed.

## Controls, lists, popups and assets

The constructor QSS specifies buttons min-height 27, horizontal padding 10,
1px border, radius 6. Normal white/#ccd8e5; hover #f2f7fd/#77aef4 with
#1267d6 text; focus accent border; pressed #e1edfb/#1267d6 with #0d56b4 text;
disabled #edf1f5/#d6dee7 with #95a2b2 text (dark substitutions above).
Navigation has no border, left-aligned padding 7/9, radius 6; selected
#dcecff with #1267d6. Cards radius 9, 1px #dce5ef border.

Inputs/pickers/spinners min-height 27, radius 6, border #ccd8e5; text input
horizontal padding 8; picker/spinner right padding 30, left 8. Their hover/focus
borders are #77aef4/#1677ff; disabled shares button disabled tokens. Spinner
buttons and dropdown are 26 wide, arrows 9 × 5. Dropdown rows min-height 26,
padding 0/6, selected #edf5ff/#1267d6. `HiDpiComboBox::showPopup` removes
native frame/shadow, squares top corners and rounds bottom corners (6), tracks
popupOpen so the input bottom corners square while open. Native macOS Picker
menus may retain OS geometry; record this deviation rather than drawing a
nonaccessible imitation. [HiDpiComboBox.h](../src/HiDpiComboBox.h) is authority.

Checkbox min-height 27, text spacing 7; indicator 15 × 15, radius 4,
normal white/#b8c6d6, checked accent border + check asset; focus accent text.
[HiDpiCheckBox.h](../src/HiDpiCheckBox.h) has a Windows high-DPI painter with
palette-based fills; it is not proof of identical low/high-DPI dark rendering.

Tab min-height 32, horizontal padding 13, selected 2px accent bottom border;
pane has 1px top border and no other border. Table header min-height 28,
padding 0/7; grid #e5ebf2; focused cell editor has a 2px accent bottom border.
Lists/text views radius 7 and 1px #dce5ef border, accent focus border. Rows
min-height 48 + vertical padding 2, two lines, separator #edf1f5. The row edit
button is 30 × 30, radius 6. `updateProfileIndicators` paints active success
background #e8f7ed/#1d4235, pending #fff4bf/#493e23, failure #fdecec/#492d35;
dots #19c15f/#f4c20d/#dc3545, idle #94a3b8. Selection remains distinct from
active. Preserve both state channels in a SwiftUI row model.

`ConnectionButton::paintEvent` uses a custom Retina/high-DPI path: filled
connect blue #1677ff, hover #0e6fe9, pressed #0c5fc8, white text; disconnect
outline blue with palette Base/AlternateBase/Highlight fills. Disabled uses
explicit dark tokens. This differs from QSS substitution: use the actual
reference at the chosen scale rather than assuming one accent for all states.
`AppStyle` in [main.cpp](../src/main.cpp) suppresses Qt dotted focus frames;
the port must retain visible native keyboard focus/accessibility indication.

Assets: [edit.xpm](../assets/icons/edit.xpm),
[check.xpm](../assets/icons/check.xpm),
[chevron-down.xpm](../assets/icons/chevron-down.xpm),
[chevron-up.xpm](../assets/icons/chevron-up.xpm),
[bimeron.ico](../assets/icons/bimeron.ico). Status dots are painted, not assets.
Tray source assets are in `assets/icons/tray/png/`: light/dark connected and
disconnected variants, and macos-template-connected/disconnected at 18, 22,
36 and 44px. Use template images in the macOS menu bar; do not reuse a colored
Windows ICO as an unverified template. Menu status/traffic are disabled entries;
profile checks follow confirmed connection, Open Bimeron and Quit remain available.
Qt popup geometry, file chooser chrome and message-box metrics are platform
style dependent and have no fixed values in this source inventory.

## SwiftUI component and state implementation contract

- Root HStack + fixed navigation VStack + page enum reproduces ordering. Keep
  normal Profiles initial page and explicitly activated Development initial page.
- Profiles uses a scrollable list with selectedID separate from activeID, a
  connection status model, optional stats and fixed bottom actions. All activation
  entry points dispatch one reducer/action path, including menu bar commands.
- Use explicit backend states unknown/unavailable/connecting/connected/
  reconnecting/stopping/disconnected/failed, busy operation, synchronized identity
  and cleanup gates. Never derive successful state from a button click.
- Editor uses a draft with original bytes, representability and Raw-authoritative
  flags. Conditional tabs/forms bind only represented values; preserve omitted
  optional values. SecureField/reveal state must not replace stored text with masks.
- Form rows map to TextField, SecureField, Picker, Toggle and numeric controls;
  routing uses Table with boundary-disabled move/remove. Do not convert source
  URI to explicit fields as a side effect of presentation.
- Use native sheets for modal editor/add/confirmation and a separate nonmodal
  auth surface that leaves IPC polling and stop available. NSOpenPanel/NSSavePanel
  replace file dialogs; keyboard Cancel and default action remain explicit.
- Scope Return/Enter and Delete to focused profile list; Shift-Up/Down reorders.
  Main commands use Command-1/comma/3/4 and Command-N only on Profiles. Editor
  shortcut scope must not collide with main navigation. Save/Cancel and dirty
  discard behavior must preserve drafts; invalid Raw-to-form shows an Error/OK
  dialog and stays on Raw. Do not rely on unspecified Qt default Tab order:
  verify focus in visible control order and skip hidden/disabled controls.
- MenuBarExtra or NSStatusItem substitutes the tray. Closing the window preserves
  session; Quit is separate. A macOS login item replaces platform startup entry;
  NetworkExtension permissions replace service elevation where applicable.
  These are platform substitutions needing backend implementation, not existing
  macOS runtime features. Preserve warnings, consent and cleanup semantics.

## Visual verification checklist (not executed)

At baseline Qt and implemented macOS revision, capture synthetic EN/RU light/dark
screens at 720 × 600 and editor minimum, 100/150% reference scaling and Retina.
Check every row of the [screen matrix](current-gui-reference.md), including six
logical editor sections, conditional hidden tabs, plugin/SSH/parameter controls,
Raw reveal and invalid drafts, long names, eight long auth prompts and errors.
Record actual OS/font/DPI, content versus frame size and runtime Core identity.
Inspect baseline alignment, wrapping, row/button heights, popup attachment,
selected versus active colors, focus ring, disabled contrast, icon sharpness,
scroll reachability and minimum-size clipping. Exercise keyboard-only navigation,
default/cancel/destructive confirmations, dark menu bar templates and notifications.
Use VoiceOver labels and plain text status in addition to color. Record each
intentional native deviation with before/after captures. No screenshots or
visual acceptance evidence are supplied by this documentation-only change.
