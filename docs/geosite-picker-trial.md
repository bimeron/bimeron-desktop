# GeoSite picker trial

Status: implementation specification, 2026-11-02. Baseline: e28d5d3.

Use native Qt controls, the application font, existing colors and control sizes.
GeoSite alone gets a resizable 728x473 logical-pixel picker: global search, an
All groups view followed by the six agreed sections, collection headings before
alphabetically sorted services, original IDs, selection count and OK/Cancel.
Selection does not move rows. Search spans the catalog and clearing it restores
the section. Hidden selections survive navigation; Cancel changes nothing.
Keep existing selected ID order and append newly selected IDs in stable order.
Do not reorder routing rules or change GeoIP behavior.

The explanatory-name source is the external 136-entry reference prepared in the
previous discussion. Section assignments are trial editorial choices, not a full
domain audit. Unclassified IDs remain literal, explicitly marked, and accessible
in All groups and global search. Missing selected IDs remain visible and marked
unavailable. Regional and legacy keys remain separate, visible entries. No
inference from !cn to blocking, and no conflation with GeoIP.

Acceptance: keyboard operation, global name/ID search, section changes, preserved
hidden/missing selections, cancel, stable serialization and unchanged rule order.
Check native English/Russian geometry at measured DPR 1, 1.5 and 2, plus focused
profile/developer fixtures. Measure opening/filtering against 1561 catalog IDs.
No dependencies, Core, service, packaging, release or settings changes. Rollback
must remove only the diff after the baseline commit. Catalog completeness and
further classification remain outside this trial.


## Catalog provenance and trial limits

The 136 explanatory labels come from the previously prepared external reference,
based on the Loyalsoldier README and v2fly/domain-list-community source files.
They are explanatory translations, not official localized names. The shipped
metadata lists exact keys; unknown future keys are never classified by prefix.
The existing GitHub and YouTube labels are assigned to Work and education and
Communication and entertainment respectively. Other service/company IDs remain
unclassified pending source review. All groups is a utility view, not a seventh
section. Geography expresses source classification, not physical hosting location.
The compatibility key category-ai-chat-!cn stays visible. Literal category-urlshortner
is preserved; category-pt means private torrent trackers, category-tm Turkmenistan,
and category-cas certificate authorities outside mainland China.

The dialog is session-local and writes no preferences or data files. It uses
native list checkboxes with a small text delegate for muted IDs and independent
elision; complete labels are available in tooltips. Long section names wrap.

## Verification — 2026-11-02

- Windows x64 application and test builds passed. All five CTests passed in
  18.12 seconds against the reviewed local document/catalog Core CLI.
- Linux amd64 application and test builds passed under WSL. All five CTests
  passed in 17.93 seconds against the reviewed Linux CLI.
- Windows ARM64 and Linux arm64 application cross-builds passed. ARM64 runtime
  was not tested because this host is x64; no package matrix was requested.
- Native Windows application-style captures passed in English and Russian at
  measured DPR 1, 1.5 and 2. Inspected all six catalog images in
  ignored build/geosite-trial. Long English sections wrap; labels and checkboxes
  remain legible. The capture uses the real catalog IDs with synthetic selection,
  not a user's saved profile or proof of runtime routing.
- Visual review found and fixed horizontal clipping in the English sidebar and
  premature name elision caused by exact text-width rounding. The delegate keeps
  four logical pixels of text-width allowance and preserves native indicators.
- Real 1561-ID catalog picker construction measured 56.13 ms on Windows and
  16.98 ms on Linux; synchronous filtering measured 0.39 ms and 0.33 ms.
  These are single diagnostic samples, excluding file parsing/Core enumeration
  and window presentation, not an end-to-end benchmark. No further profiling was
  justified by these results.
- Focused coverage checks global search, section restoration, hidden/missing
  selected IDs, selection count, keyboard toggling, stable ID serialization,
  unchanged GeoIP behavior and existing profile preservation tests.
- Diff review and whitespace checks passed. No new dependencies, configuration
  writes, service operations, installer changes or release packages. The trial is saved in a separate commit above baseline e28d5d3. Full catalog classification and
  installed-service compatibility remain separate work.
