# GeoSite picker trial

## Accepted Other labels and empty sections follow-up — 2026-11-03

Authorized for implementation. This delta supersedes historical labels below.
Use Miscellaneous / Прочее for the stable `without` topic key, after all named
topics and only under the existing meaningful-subset rule. Topicless scopes keep
one disabled All topics option. Use Other / Другое for sidebar and search heading.
Hide all empty content sections using unique available IDs only; unavailable
selections, searches and selection filters never affect sidebar population.
Keep fixed row identities (AI 7, Other 9), All groups always visible, and reset any
hidden current section to All groups before deriving navigation state. Preserve
Cursor's accepted AI placement and third priority, all IDs and serialization.
Acceptance: sparse/empty/full catalogs have the correct visible section subset;
Other appears only for unknown available IDs; Miscellaneous is last and restores
by key; global modes preserve navigation and never restore a hidden section.
Update directly affected fixture expectations, including Cursor's baseline reverse
migration. Per explicit user instruction, run no tests, CTest, visual fixtures,
screenshots, benchmarks or profiling. Incrementally build all four existing Release
caches and launch only the normal Windows x64 GUI with the documented Qt environment.
No package, dependency, settings, service, commit, push or PR changes.

## Cursor placement and priority — 2026-11-03

User-approved delta: move `cursor` from Work and education / Software development
to Artificial intelligence / AI tools (`4-ai`). In the effective AI browsing view,
the priority prefix is ChatGPT (`openai`), Claude (`anthropic`), Cursor (`cursor`),
then the ordinary alphabetical remainder. Search and selected-view ordering retain
their existing rules. Preserve Cursor's label, type, ID and search aliases.
Tests are not run for this delta at the user's explicit request; existing test
expectations may require adjustment before the next validation run.

Status: implemented eight-section taxonomy, 2026-11-03. Baseline: e28d5d3 plus preserved dirty metadata transfer.
Historical specifications and validation reports are retained below.

## Accepted eight-section taxonomy — 2026-11-03

This user-approved agreement supersedes historical fixed-six-section constraints.
The exact section order is Advertising and tracking; Communication and entertainment;
Technology and access; Work and education; Shopping and services; Ecosystems;
Artificial intelligence; General lists. Russian additions are Экосистемы and
Искусственный интеллект; Общие списки remains last. All groups and Unclassified
remain utility views. Keep all 1561 original IDs, labels, kinds, aliases, routing,
rule order and selection serialization. Section and topic navigation is session-only.

Ecosystems requires an affirmatively curated multi-product brand portfolio. Company
sites, support domains, regional mirrors, a thematic association or one product
with supporting domains alone do not qualify. Conservative retained corporate
collections stay General lists / Companies and organizations, including Neuralink.
Standalone products keep their purposes: YouTube video, GitHub development.
Broad portfolios such as Google stay Ecosystems even when they include AI.

Exact Ecosystems migration from the existing 6-organizations topic (no other
entries are eligible in this change):

`adguard`, `adobe`, `alibaba`, `alphabet`, `amazon`, `apple`, `autodesk`, `auxgroup`, `baidu`, `broadcom`, `bytedance`, `canonical`, `cheetahmobile`, `cisco`, `dell`, `didi`, `disney`, `dmm`, `douyin`, `epicgames`, `facebook`, `gmo-internet`, `google`, `haier`, `hkt`, `intuit`, `jd`, `kakao`, `kingsoft`, `kuaishou`, `lenovo`, `line`, `mailru`, `mailru-group`, `meitu`, `meta`, `microsoft`, `mozilla`, `mts-ru`, `naver`, `nbcuniversal`, `netease`, `newscorp`, `oracle`, `pccw`, `progress`, `protonmail`, `rakuten`, `razer`, `rostelecom`, `ruanmei`, `samsung`, `sber`, `sony`, `tencent`, `xiaomi`, `yandex`.

Move every existing 4-ai member and xiaomi-ai into Artificial intelligence.
Also move the existing AI-origin collections category-ai-!cn and its legacy alias
category-ai-chat-!cn there, preserving their absent topic and explanatory mixed
creative/development scope. This does not claim AI-only domain coverage.
Keep the populated stable 4-ai key (AI tools) for compatibility and bilingual
AI/ИИ topic search; xiaomi-ai joins that topic with its existing MiMo/XiaoAI label.
Ecosystems entries have no narrower topic; do not invent a duplicate ecosystem
topic. All five 6-* topic keys remain unchanged in General lists, now index 7.
No other topic assignment changes. There remain 46 topics.

Acceptance: eight ordered sections; exact baseline preservation except the above
section/topic moves and General lists index shift; all section/topic assignments
consistent; Neuralink remains general; product distinctions, bilingual new-section
and AI/Codex/Claude/ID search, topic reset and unavailable/selected restoration;
stable serialized selection and unchanged routing. Extend native EN/RU captures
for Ecosystems, AI, General lists and product distinction. Run four incremental
Release builds and native Windows x64 / WSL Linux amd64 CTest; ARM64 compile-only.
No package, dependency, settings, Core, service, commit, push or PR changes.

External baseline snapshots and every 6-organizations decision are retained in
`C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-eight-sections-2026-11-03`.
Selection uses existing curated labels and cached evidence; no new web research.
Sohu is retained in General lists: its mixed media/website-tool evidence does not
affirm the corporate portfolio criterion. Dropbox likewise remains general because
its label establishes a product and supporting domains, not independent products.

## Eight-section verification — 2026-11-03

The final counts in navigation order are 21 advertising/tracking, 522 communication/
entertainment, 289 technology/access, 293 work/education, 231 shopping/services,
57 ecosystems, 27 artificial intelligence and 121 general lists: **1561 total**.
All 46 stable topic IDs remain, including populated 4-ai; 1156 entries have a topic.
There are 84 semantic section migrations (57 ecosystems and 27 AI), 121 General
lists index-only moves and 1356 byte-equivalent metadata records after parsing.
No label, original ID, collection/service kind or unrelated topic changed.

Reviewed all 145 baseline 6-organizations records using curated labels, prior
catalogue decisions, later cached purpose decisions and the mixed-purpose ledger.
The external organization-decisions.json retains every move/retention, exact
available decision pointers, source excerpts and inherited limitations. Later
explicit portfolio evidence resolves Haier, HKT, JD, NetEase, Rostelecom and
Ruanmei; Proton and Kuaishou use later product portfolio evidence. Sohu and Dropbox
remain general under the conservative criterion above. No new web research.

Validation:

- All four existing incremental Release application/test builds passed: Windows
  x64/ARM64 and WSL Linux amd64/arm64. Existing GUI-only caches retain
  BIMERON_REQUIRE_CORE=OFF and Windows BIMERON_DEPLOY_QT=OFF. ARM64 is compile-only
  on this x64 host. No configure, dependency, Core pin or packaging flow change.
- Final native Windows x64 CTest passed 5/5 in 22.92 seconds; WSL Linux amd64
  offscreen CTest passed 5/5 in 19.88 seconds. Both used the reviewed local
  desktop-bridge Core CLI and real 1561-ID fixture. No clipboard/Paste failure
  occurred in these runs. The first Linux compile exposed a missing explicit
  QCryptographicHash test include; fixed and all subsequent final builds passed.
- Regressions verify all metadata against the pre-task dirty baseline SHA-256
  after reversing only approved moves, section/topic consistency, topic scoping
  across eight sections, Neuralink/general, YouTube/video, GitHub/development,
  aliases, bilingual section/topic/AI/Codex/Claude/literal-ID search, restoration,
  unavailable selections, stable serialization and existing routing preservation.
- Diagnostic construction/filter samples: Windows 36.13/0.81 ms, Linux 25.61/0.84
  ms. Prior transfer samples were 26.50/0.53 and 20.93/0.53 ms. These single samples
  under concurrent build load exclude Core enumeration, parsing and presentation;
  they are not wall-clock opening latency or a statistically controlled regression
  benchmark. Filtering remains below one millisecond. Navigation retains the same
  row scan and cached search text; no new performance-sensitive algorithm or
  additional profiling is justified.
- Reviewed native EN/RU captures with fresh eight-final-en-native.png and
  eight-final-ru-native.png basenames under ignored build/geosite-classification:
  all three new/repositioned sections, AI search, Codex/Claude and Google versus
  YouTube/GitHub/Neuralink placement. EN DPR is 1, RU DPR is 1.5. Long legacy labels
  elide with complete tooltips; section names remain readable. Minimum 590x350
  captures retain scrollable navigation. This is not a new full DPR 1/1.5/2 matrix.
- External exact-field validation and git diff --check passed. Task-relative diff
  review found no unresolved correctness, security, routing, duplicate-row or
  unnecessary-complexity defect. Pre-existing dirty work and parent checkout were
  preserved. The build guide documents the additional fixture outputs only.

Remaining limits are inherited sampled scope/ownership evidence, unproven installed
source revisions and live installed-routing compatibility, plus ARM64 runtime.
No packages, installed settings/services, Core mutations, commit, push or PR.
The external baseline, per-ID ledger, task-relative diffs and validation live in
C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-eight-sections-2026-11-03;
final build/test/diagnostic logs and native captures live in ignored
build/geosite-classification/eight-verified-* and eight-final-* files.

## Current metadata transfer verification — 2026-11-03

The separately authorized implementation transfers **1380 eligible records** from
`geosite-reanalysis-v2/efficient-v3/unknown-final-028/catalogue.json` under the
external research root `C:/Users/PC/Documents/Bimeron-GeoIP-Analysis`.
Its file-byte SHA-256 is
`1d010fc26ef5e3b8cc817711e5c0f60e4984cda2e49e263746558185ccd8563c`.
The draft's 1382 known MAIN figures are not the import count: 18 records lost
labels/kind in the final draft but retain supported exact earlier decisions;
those fields were restored individually, including Apple software updates and
recovery. Neuralink and RedTube have known MAIN but unsupported kind and remain
literal/unclassified alongside the draft's 179 unknown MAIN records: **181 total**.
No missing topic was treated as an eligibility failure.

Against the actual dirty 977-entry product baseline, 403 IDs are new, 83 records
change and 894 remain unchanged, including reused existing Russian translations.
All 977 prior IDs remain. There are 1185 topic assignments and 195 accepted entries
without refinement. The six sections and 44 existing topic keys remain; the two
accepted additions produce 46 topics and 48 All groups utility/topic options.
Business operations and CRM has ten entries; Weather and forecasts has seven.
The primary-purpose policy moves business workflow software to Work and education
and weather information, including weather APIs, to Shopping and services.
Apple updates joins Windows updates in Updates and drivers. IDs, aliases,
selection serialization and routing rules remain unchanged.

The integration ledger, before-file snapshots, exact new/changed/retained/unknown
partition, per-field statuses, source pointers, exclusions, input hashes and
validation are outside the repository in
`C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-desktop-transfer-review-2026-11-03`.
The known MAIN values of category-social-media-ir, Entrust and iTunes are retained
when later mixed-null suggestions conflict. The referenced Quera purpose excerpt
now resolves the earlier forums objection: category-forums-ir becomes a Work
service with no narrower topic. Internet Archive retains no specific topic:
identical library/archive source bytes do not resolve the prior web-only concern.
No new web research or whole-catalog semantic audit was performed. Research files
are unchanged; only curated product metadata and translations are compiled.

Validation and review:

- All application/test targets built in the four existing Release configurations:
  Windows x64/ARM64 and Linux amd64/arm64. Existing GUI-only caches have
  `BIMERON_REQUIRE_CORE=OFF`; Windows has `BIMERON_DEPLOY_QT=OFF`. No build flow,
  dependency, Core pin, package or service change was made. ARM64 is compile-only
  on this x64 host; ARM64 runtime remains unverified.
- Windows x64 CTest passed 5/5 in 31.86 seconds; WSL Linux amd64 passed 5/5 in
  18.49 seconds. Tests used the reviewed local desktop-bridge Core CLI through
  `BIMERON_GEODATA_CORE`, with native Windows Qt and Linux offscreen Qt. Optional
  unrelated protocol CLI overrides were not supplied. Focused fixtures check
  exact metadata/topic counts and memberships, every Russian translation,
  aliases/literal keys, known-MAIN exclusions and topicless eligibility,
  section-specific/full-taxonomy options, reset, bilingual global search,
  selected/unavailable restoration and stable serialization in both languages.
- Real 1561-ID construction/filter diagnostic samples are Windows 26.50/0.53 ms
  and Linux 20.93/0.53 ms, versus prior 30.07/0.58 and 19.49/0.59 ms respectively.
  These single samples exclude Core enumeration, parsing and presentation; host
  load and caches vary. Filtering remains below one millisecond. No timing gate
  or additional profiling was justified for a metadata-only runtime change.
- Native Qt captures in ignored `build/geosite-classification` use fresh basenames
  `transfer-en.png` and `transfer-ru.png`. Reviewed the actual `.4-business-crm.png`,
  `.5-weather.png`, `.3-updates-drivers.png`, `.unknown.png` and `.selected.png`
  images for each language: readable labels/IDs, complete topic text, correct
  headings and distinct unavailable selection. The recorded DPR is 1 for English
  and 1.5 for Russian; these are representative current samples, not a new full
  DPR matrix. Prior DPR 1/1.5/2 captures remain historical evidence. The fixture
  also preserves minimum 590x350 captures and geometry logs.
- External exact-ID/field/topic validation and `git diff --check` passed. Review
  checked correctness, preserved dirty work, translation-key collisions, literal
  IDs, unchanged routing/selection behavior, session-only state, security and
  unnecessary complexity. Existing combo behavior already meets the agreement,
  so this transfer adds no picker implementation logic. The build guide documents
  the extra fixture outputs. No unresolved code-review defect was found.

Remaining limits are sampled purpose evidence and inherited provenance, unproven
installed source revisions/endpoint ownership, 181 unclassified IDs, 195 records
without narrower topics, ARM64 runtime and live installed-routing compatibility.
No package/release, settings mutation, commit, push or PR is part of this transfer.

## Accepted next metadata transfer, 2026-11-03

This agreement defined the transfer implemented above. Historical results below
describe the earlier 977-entry, 44-topic version.
Retain the six sections in their existing order and all 44 existing topics.
Add two topics, bringing the agreed taxonomy to 46:

- Section 4, Work and education: Business operations and CRM /
  Бизнес-процессы и CRM.
- Section 5, Shopping and services: Weather and forecasts /
  Погода и прогнозы.

Education remains covered by Learning and Science and research within section 4.
Wallpapers and connected-device topics are not part of this agreement.
All groups exposes all topics; a selected section exposes only its own topics.
Default to All topics and reset to it on a section change. Unclassified disables
the topic selector. Existing global search and Only selected behavior remains
as specified below. All groups, Only selected and Unclassified are utility views,
not additional sections. A classified entry may have no specific topic.

Classify by the primary user purpose; accounts, updates and support do not alone
change that purpose. Business workflow software belongs in Work and education;
consumer purchases, payments and service booking belong in Shopping and services.
Affirmatively established corporate collections spanning independent purposes may
use General lists / Companies and organizations. Preserve uncertainty when the
source scope does not support a single section; do not use General lists as an
uncertainty fallback. Collections precede services, then localized alphabetical
order. Original IDs and serialized selections remain unchanged.

The external unknown-final-028 draft has 1561 IDs, 1382 proposed known main
sections and 179 unknown main sections. These are research coverage figures,
not accepted import counts. Transfer eligibility still requires supported EN/RU
labels, section and collection/service kind; topics are optional. Preserve
source limitations and review existing-entry changes explicitly. Do not import
research caches or generated research arrays into the repository. This agreement
authorizes the specification only; metadata implementation is a subsequent step.

## Accepted classification extension

Integrate curated product metadata from the external 1561-ID source-purpose
review: 977 accepted records, 584 unresolved records, 44 stable topics. Review
the 180 flagged records before transfer; record corrections separately outside
the frozen evidence cache. Ship only accepted labels, zero-based sections,
collection/service kinds and optional stable topic IDs in GeoSiteLabels.h and
Russian translations. Unknown and unresolved IDs remain literal and unclassified.
The cache is a sampled source-purpose audit; exact installed source revisions
are unproven. No topic implies a narrower domain set or routing recommendation.

Keep All groups, the six fixed sections, then Unclassified as utility navigation.
Unclassified contains available IDs without accepted metadata; unavailable
selected IDs have a separate heading and remain accessible in All groups,
search and Only selected. One labeled Topic combo offers All topics, Without
topic refinement (accepted entries with no topic), and applicable taxonomy
topics. All groups offers the full taxonomy; Unclassified disables refinement.
Changing section resets Topic to All topics. Changing section or topic exits
Only selected. The clickable selection count toggles Only selected, which
ignores section/topic and shows every checked ID, including unavailable ones.
Unchecking there removes the row immediately. Nonempty trimmed search overrides
section, topic and Only selected globally; clearing restores the prior view.
Search matches English and Russian explanatory names and exact literal IDs
independently of interface language. Collections precede services, sorted by
localized name then ID; unclassified and unavailable rows have distinct headings.
Retain original selected ID order and append new IDs in lexical ID order,
independent of language, filters and checkbox click order. Cancel never applies.

Acceptance additionally covers taxonomy/metadata validity, aliases, topic reset,
bilingual global search and restoration, selected/unavailable view transitions,
deduplication, stable serialization across languages, keyboard access, empty
headings and native EN/RU geometry at measured DPR 1/1.5/2, including minimum
590x350. Measure real 1561-ID construction/filtering against the prior Windows
56.13/0.39 ms diagnostic samples without a flaky timing threshold. Build all four
CI targets and run native fixtures where possible; report unavailable runtime
targets explicitly. No packages, settings, services, dependencies or releases.

Use native Qt controls, the application font, existing colors and control sizes.
GeoSite alone gets a resizable 728x473 logical-pixel picker: global search, an
All groups view followed by the six agreed sections, collection headings before
alphabetically sorted services, original IDs, selection count and OK/Cancel.
Selection does not move rows. Search spans the catalog and clearing it restores
the section. Hidden selections survive navigation; Cancel changes nothing.
Keep existing selected ID order and append newly selected IDs in stable order.
Do not reorder routing rules or change GeoIP behavior.

The explanatory-name source is the external source-purpose review described
below. Section assignments are editorial navigation choices, not a full
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

The external research root is
`C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-classification`.
The frozen `themes/catalog-with-topics.json` SHA-256 is
`a7abc235907166c4df82879c8f3f8feff5301dbbfd475d915aaf07073be6ba59`.
It contains 1561 IDs, 977 accepted and 584 unresolved; 788 initially have topics
and 189 have no specific topic. The product retains the exact 977 accepted IDs;
44 topics use stable string keys and zero-based sections. After the reviewed
corrections, 792 entries have topics and 185 retain no specific topic. Unknown
future IDs are never classified by prefix. The database is still enumerated by
Core; product metadata neither supplies IDs nor proves installed domain semantics.

The 180 flagged records were reviewed against cached source evidence and their
scope rationale. The separate external review is
`C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-desktop-integration-review/corrections-and-flag-review.json`.
It records evidence hashes, retained uncertainty and 24 concrete product metadata
corrections. The frozen catalog and evidence cache were not modified. Pirate Bay
and RARBG move to Technology and access / Torrents and P2P; Beisen moves to
Shopping and services / Public and business services; Homebrew moves to
Technology and access / App stores and packages (app distribution).
Internet Archive moves to Technology and access with no narrower topic: its
Open Library and archive scope should not become a web-only claim. The explicit
Iranian-forums comment for category-forums-ir remains stronger than the cache's
unresolved Quera education inference, so its original section and absent topic
are retained. Nineteen label/type corrections broaden misleading product-only
summaries, including ABEMA/Ameba, Next Media, Deepin/UnionTech, RT projects and
ZEE5/Zee TV. Remaining scope flags retain uncertainty rather than force topics.

Names and translations are explanatory, not official localized names. Sources
include cached Loyalsoldier workflow/README and v2fly/domain-list-community
files. This is a sampled source-purpose review; exact upstream revisions used
for installed release 202611012203 remain unproven. Geography expresses source
classification, not physical hosting location. The compatibility key
category-ai-chat-!cn stays visible; !cn never means blocked. Literal
category-urlshortner is preserved; category-pt means private torrent trackers,
category-tm Turkmenistan, and category-cas certificate authorities outside mainland
China. No research data, source files or generated runtime catalogs are shipped.

The dialog is session-local and writes no preferences or data files. It uses
native list checkboxes with a small text delegate for muted IDs and independent
elision; complete labels are available in tooltips. Long section names wrap.

## Classification extension verification — 2026-11-02

- GUI and all existing test targets compile in Release for Windows x64/ARM64
  and Linux amd64/arm64. Used the existing presets with
  `BIMERON_REQUIRE_CORE=OFF`, and `BIMERON_DEPLOY_QT=OFF` on Windows; no build
  flow, dependencies, Core pins, packages or installed services changed.
- Windows x64: all five CTests passed in 20.13 seconds with the native Windows
  platform. Linux amd64 under WSL: all five passed in 19.53 seconds, offscreen.
  Both used the existing local `bimeron-core/build/desktop-bridge` document/catalog
  CLI (read-only executable input), not the installed service. ARM64 targets were
  cross-compiled only because this host is x64; ARM64 runtime remains unverified.
- Metadata checks establish exact coverage of all 977 accepted IDs, absence of
  all 584 unresolved entries, 41 unique valid topics, 792 topic assignments,
  section/topic consistency, all Russian labels, three compatibility aliases,
  and the unchanged frozen catalog SHA-256. Unknown future IDs remain literal.
- Extended profile fixtures cover topic selection/reset, Without topic refinement,
  global bilingual name/ID search in both interface languages, clearing search,
  selected-only and unavailable transitions, duplicate input IDs, hidden selection,
  empty headings, original-order retention with lexical new IDs, editor Cancel,
  keyboard toggling and existing routing/profile preservation regressions.
  Initial failures were heading-count assumptions after reviewed GitHub collection
  metadata; fixtures now distinguish source-ID counts from editorial headings.
- Real 1561-ID diagnostic samples: Windows construction 30.07 ms, synchronous
  filtering 0.58 ms; Linux construction 19.49 ms, filtering 0.59 ms. The prior
  Windows samples were 56.13/0.39 ms. These are single samples, affected by caches
  and concurrent host work, excluding parsing/Core enumeration/presentation.
  Filtering remains below one millisecond; no further profiling or timing gate
  was justified. Earlier Windows construction samples ranged 28.86–47.27 ms.
- Real native Qt captures use the application stylesheet, both languages, measured
  DPR 1, 1.5 and approximately 2 (1.99999995), full 728x473 and minimum 590x350.
  The catalog, topic, selected/unavailable and minimum states produce 24 PNGs in
  ignored `build/geosite-classification`; `screenshots.json` lists absolute paths
  and measured DPR. For example: `en-1.png.topic.png`,
  `en-1.5.png.topic.png`, `en-2.png.topic.png`, `ru-1.png.topic.png`,
  `ru-1.5.png.topic.png`, `ru-2.png.topic.png`. Visual review confirmed legible
  controls and wrapping; the minimum English sidebar scrolls. The active count
  now explicitly says Only selected because the existing stylesheet made the
  pressed-button state subtle. These are synthetic selections, not routing proof.
- Diff review and `git diff --check` passed. Reviewed correctness, search and
  selection regressions, exact-ID handling, session-only state, translation
  collisions and unnecessary complexity. Preserved the unrelated shared Security
  translation. No new dependencies or architecture were introduced. The build
  document now describes the extended native capture outputs.
- Remaining limits: sampled cached purpose evidence, unproven installed source
  revisions, 584 unresolved IDs, 185 accepted groups without a narrower topic,
  ARM64 runtime, and live installed-routing compatibility. No release is prepared.
  Work remains uncommitted in the authorized shared worktree; no push or PR.

## Prior trial verification — 2026-11-02 (before classification extension)

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

## Accepted operating-system topic split

Replace `3-browser-os` with four section-2 topics: `3-operating-systems`
(Operating systems / Операционные системы), `3-updates-drivers` (Updates and
drivers / Обновления и драйверы), `3-app-packages` (App stores and packages /
Магазины приложений и пакеты), and `3-browsers` (Browsers / Браузеры).
The taxonomy has 44 topics (46 All groups combo options); accepted IDs remain
977 with 792 topic assignments and 185 without refinement.

Operating systems contains android, archlinux, debian, fedora, ubuntu and
ubuntukylin. Updates and drivers contains win-update only. App stores and
packages contains apkcombo, apkmirror, apkpure, f-droid, flatpak, homebrew,
snapcraft and category-android-app-download. Browsers contains ceno,
epicbrowser, firefox, librewolf and category-browser-!cn.

These are whole-source purpose labels: OS projects include documentation and
forums, not merely system services. No domain datasets are split and no Android
or iOS update-only group is inferred. Broad Apple/Microsoft portfolios and
ambiguous nearby groups retain their existing metadata; win-spy stays telemetry
and PKI stays security. Frozen evidence and integration corrections remain intact.
The separate `geosite-os-topic-review` directory beside the research cache records
whole-file/include evidence, hashes, mapping rationale and nearby exclusions.

Acceptance: exact assignments and topic counts, bilingual names/search, unchanged
aliases, selection persistence across all four topics and selected-only/search
transitions, native Russian OS/update captures, four available compile targets,
focused native tests, and a safely launched rebuilt Windows GUI. No packages,
installed-service mutations, settings changes or release work.

### OS split verification — 2026-11-02

- Updated only metadata, Russian topic strings, this specification and existing
  fixtures; picker behavior needed no further change. Exact frozen accepted-ID
  coverage remains 977; 44 topics, 46 utility/topic options, 792 assigned and
  185 without refinement. The affected topic sizes are 6/1/8/5. Aliases remain
  unchanged. External `geosite-os-topic-review/review.json` records all 20 cards,
  recursive file/include hashes, custom win-update provenance and nearby exclusions.
- All GUI/test targets compiled in the four existing Release configurations:
  Windows x64/ARM64 and Linux amd64/arm64. Windows x64 CTest passed 5/5 in
  22.57 s; Linux amd64 passed 5/5 in 20.35 s. ARM64 is compile-only on this
  x64 host. Qt 6.8.3 Windows platform/plugin paths and the reviewed local
  desktop-bridge Core were supplied explicitly; Linux used offscreen Qt.
- Profile fixtures cover the exact four topic memberships, both-language global
  update search, search restoration, selected-only/topic transitions, hidden and
  unavailable selection retention, serialization order, topic translations and
  unchanged aliases/telemetry/PKI. Real 1561-ID diagnostic samples: Windows
  construction/filter 26.72/0.51 ms; Linux 18.10/0.49 ms. These single samples
  exclude Core enumeration and presentation; no performance-sensitive logic
  changed and no additional profiling was warranted.
- Native Russian screenshots, using the existing isolated developer fixture,
  are `build/geosite-classification/os-ru-native.png.3-operating-systems.png`
  and `build/geosite-classification/os-ru-native.png.3-updates-drivers.png`.
  Measured DPR is 1.5. Visual review confirmed all six OS entries, only win-update
  under updates, readable names/IDs and no clipped topic/control text. Existing
  captures were preserved under their original names.
- The stop-build safety guard passed before relinking and launch. The rebuilt
  Release GUI opened visibly as PID 45464, title Bimeron, remained alive and
  responding on a later check. Adjacent Core and Wintun SHA-256 hashes are
  unchanged. The bundled helper's read-only `service status` exited 0 and
  reported `Service state: 1`; no service mutation was performed.
- Focused diff review and `git diff --check` passed. No new dependencies,
  architecture/build-flow changes, packages, commits, pushes or PRs. Existing
  authorized dirty implementation was preserved. Remaining limits: cached source
  revisions do not prove installed domain semantics; no ARM64 runtime evidence
  or release/package acceptance is claimed. This split adds one native Russian
  DPR sample; earlier EN/RU DPR 1/1.5/2 captures remain prior-layer evidence.


## Transfer eligibility clarification — 2026-11-03

The authorized transfer evaluates EN/RU labels, section and independently supported
collection/service kind per field. A missing optional topic never blocks an entry.
A known MAIN alone is insufficient. Where the final draft loses fields but names
an exact supported earlier decision, reconcile that concrete conflict against the
referenced decision and record the restored fields externally; do not infer kind
from an ID. Preserve known MAIN for later mixed-null recommendations. Retain
previous product corrections when the later proposal supplies no stronger scope
evidence. Apply the accepted purpose policy to business workflow and weather
refinements. Keep all 44 prior topic keys and add `4-business-crm` and `5-weather`.
No research arrays or evidence are product inputs at runtime. The external
`geosite-desktop-transfer-review-2026-11-03` directory records every transferred,
retained and excluded ID, source decision, field status and editorial exception.
Acceptance uses exact eligible coverage, both languages, category/topic reset,
global search, original-ID serialization, aliases and unavailable selections;
real-catalog timings, four Release compile targets and native EN/RU captures.

## Accepted Mac evidence import and topic search — 2026-11-03

Validate the read-only 179-ID Mac handoff against the original external input.
Import only records with supported nonempty English/Russian names, a main section
and collection/service kind. Resolve known-purpose mixed/null-section proposals under the editorial navigation
rule below; keep evidence gaps unclassified. Preserve neuralink/redtube and all
baseline metadata. Optional topic
refinement is not required for import. Keep six sections and 46 existing topics.
Record exact decisions, exclusions, provenance and source hashes externally.

Reuse 4-ai (AI tools / Инструменты ИИ). Display OpenAI and ChatGPT (Codex) /
OpenAI и ChatGPT (Codex), and add (Claude) to both Anthropic explanatory names.
Do not promote mixed AI/creative/development aliases into the AI topic.
Search every assigned topic's English and Russian names regardless of UI language,
using the existing cached row search text. Preserve global search override/restore,
section topics, selected/unavailable rows and original-ID serialization.

Acceptance: exact imported IDs and exclusions, baseline preservation, complete
bilingual section/topic labels, AI/ИИ/Codex/Claude and generic topic queries in
both languages, navigation restoration and serialization; four Release builds,
native x64 CTests, real 1561-ID timings against prior samples and native EN/RU
AI/search/new-record captures. ARM64 runtime acceptance requires ARM64 hardware.
No dependencies, settings/services/Core changes, packages or repository publication.

### Mac import verification — 2026-11-03

Imported 138 supported records from the exact 179-ID handoff, producing 1518
classified and 43 unclassified real IDs. All imports have EN/RU names, main
section and independently supplied kind; optional topics remain absent. The 40
mixed/null-main proposals and xiaomi-ai evidence gap remain excluded, as do
neuralink/redtube (outside this handoff, unsupported kind). No baseline entry was
reclassified. OpenAI/ChatGPT and Anthropic gained Codex/Claude display text;
4-ai uses AI tools / Инструменты ИИ. Its mixed creative/development aliases stay
without refinement. Six sections, 46 topics and 1185 topic assignments remain.
Every section and topic has an English label and Russian translation; all 1518
entry names have Russian translations. Existing translation keys were retained;
only the explicitly requested AI topic translation changed.

Read-only SSH/SCP import and the audit ledger are external at
`C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-mac-results-import-2026-11-03/`.
`import-ledger.json` records each import/exclusion, source limitations, 209 source
file hashes, 37 cited receipt files inspected, and the exact original-input
comparison. The original JSON bytes and 179 unique ordered IDs match. No downloaded
script was executed and no further web research was performed. Mixed-purpose and
historical/snippet evidence limitations remain explicit; these are explanatory
labels, not an installed-domain audit.

The existing cached row search now includes the assigned topic's English and
Russian labels independently of UI language. Tests exercise every topic in both
languages, AI/ИИ membership, Codex/Claude, generic Security translation, global
search restoration, topic/section transitions, selected/unavailable serialization,
coverage, aliases and all translations. Substring semantics remain unchanged:
ИИ can also match Russian words ending in ии.

All GUI/test targets built in the four documented Release configurations.
Windows x64 CTest passed 5/5 in 14.21 s; Linux amd64 passed 5/5 in 9.07 s.
These runs did not set the optional BIMERON_GEODATA_CORE integration executable;
Core-backed bridge fixtures therefore remain prior-layer evidence, not fresh
acceptance. Both ARM64 targets are compile-only on this x64 host.
Real 1561-ID construction/filter samples: Windows 33.09/0.62 ms versus prior
26.50/0.53 ms; Linux 24.12/0.61 ms versus 20.93/0.53 ms. Single samples include
metadata/sorting and cached topic-text preparation, exclude Core enumeration and
presentation, and impose no flaky threshold. Filtering retains the existing
single row scan; topic expansion occurs only during construction. No additional
profiler was run; these samples do not establish a statistically significant
regression or a performance guarantee.

Native Qt captures are `build/geosite-classification/mac-en.png.*` (light, DPR 1)
and `mac-ru.png.*` (dark, DPR 1.5). Reviewed AI-topic, Codex/Claude and representative
928Net/RU-CENTER search captures show readable labels/IDs and the expected global
search override. Existing captures remain intact. Logs use the `mac-` prefix.
Diff review and git diff --check passed. No new dependency, settings/service/Core
mutation, packaging, commit, push or PR; existing authorized dirty work preserved.
Release/package validation and ARM64 runtime remain outside this task's evidence.

## Accepted mixed-purpose editorial navigation — 2026-11-03

A null research MAIN is not an unknown purpose. Import the 40 editorial_mixed
records with supported bilingual scope labels and independently supplied kinds.
Choose the dominant user purpose including auxiliary features. Work software
belongs in section 4, consumer services in section 5, and coherent entertainment
in section 2. For a cross-section mixture, explicitly choose a navigation priority
without claiming factual dominance: software/developer workflows precede their
community content; consumer payments precede accompanying developer tools;
entertainment viewing precedes associated apparel or event ticketing. Retain the
whole-scope explanatory label and collection kind.

Where independent purposes instead establish a corporate portfolio or thematic
source collection without a defensible common activity, choose section 6 /
Companies and organizations. This expresses affirmative collection scope, never
uncertainty or presumed common ownership. Optional narrower topics may remain
null. Record each decision and its evidence limitations in an external rationale
ledger. Preserve all 1518 existing records, 46 topic keys, AI labels, bilingual
topic search, original-ID serialization and routing. Expected coverage is 1558
of 1561; xiaomi-ai, neuralink and redtube remain unknown without new proof.

Acceptance: exact 40-ID additions and baseline preservation, bilingual labels,
section/topic consistency, aliases, search and serialization regressions; all four
Release builds, native Windows x64/Linux amd64 CTests, and native EN/RU mixed
record captures. ARM64 runtime requires target hardware. A data-only addition may
reuse the prior 33.09/0.62 ms Windows and 24.12/0.61 ms Linux diagnostic samples
with explicit attribution; no new benchmark or profiling result is implied.

### Mixed-purpose placement verification — 2026-11-03

Imported all 40 known-purpose editorial_mixed records from the existing local Mac
cache. Coverage is now 1558 classified and 3 unknown of 1561 real IDs; xiaomi-ai,
neuralink and redtube remain unknown. All 1518 existing metadata rows were
compared before/after and retained verbatim. Six sections and 46 topics remain;
1210 entries have topics. All additions retain the Mac collection kind and exact
EN/RU scope names. Six navigate under communication/entertainment, three under
technology/access, three under work/education, three under shopping/services, and
25 affirmatively mixed corporate/thematic collections under general lists /
Companies and organizations. These are editorial navigation priorities, not
factual rankings of independent products or claims of common ownership.

The external `geosite-mac-results-import-2026-11-03/editorial-placement-ledger.json`
records every ID, bilingual names, kind, section, optional topic, navigation
rationale, original evidence rationale/sources and limitations, plus the input
decisions SHA-256. The original import ledger and downloaded evidence remain
unchanged. No web research, source script execution or runtime research input was
added. Metadata additions are confined to GeoSiteLabels.h and ru.json.

All four documented Release GUI/test configurations compiled with existing
GUI-only caches (`BIMERON_REQUIRE_CORE=OFF`, Windows `BIMERON_DEPLOY_QT=OFF`).
Final Windows x64 native CTest passed 5/5 in 12.44 s; Linux amd64 offscreen CTest
under WSL passed 5/5 in 9.03 s (earlier passing runs: 13.59 s and 8.92 s). These runs leave optional Core-backed bridge integration
unset, as in the preceding Mac-import layer. Both ARM64 configurations are
compile-only on this x64 host; target runtime remains unverified. An initial
failure exposed the old NVIDIA-excluded assertion; it was updated to the accepted
known-purpose placement and the complete native suites passed. Coverage includes
40 explicit sections/kinds, all translations, topic consistency, compatibility
aliases, bilingual topic search, representative mixed-name/ID search in both
languages, navigation restoration and original selection serialization.

No performance-sensitive logic changed: only 40 static metadata rows and
translations were added. Reuse the preceding Mac-import diagnostic samples,
Windows construction/filter 33.09/0.62 ms and Linux 24.12/0.61 ms; these are prior
measurements, not new placement timings. No additional profiling was warranted.

Native light English and dark Russian captures use `mixed-en-native.png.*` and
`mixed-ru-native.png.*` in ignored build/geosite-classification. Representative
CNET, madshi, PayPal and NVIDIA searches were visually reviewed for readable
whole-scope labels, collection headings and original IDs. Search intentionally
overrides the remembered AI topic/section. All 80 per-ID captures were generated at measured DPR 1.5 in both languages.
The fixture captures every new ID as `.mixed-<id>.png`; short ID substring queries may show additional matches.
Use the emitted catalog JSON for actual DPR, rather than the requested factor.
The first capture lacked the reviewed bridge CLI and failed before editor
observations; after supplying it, a fractional-scale attempt hit the existing
fixed editor-geometry assertion. The documented font directory with scale factor
1 passed the complete native fixture; no geometry assertion was removed.

Diff review covered exact-ID additions, evidence limits, null topics, translation
keys, section consistency, unchanged search/routing behavior and unnecessary
complexity. `git diff --check` passed. No dependencies, settings/services/Core
mutations, packages, commits, pushes or PRs. Existing dirty work was preserved and
the parent checkout was untouched. Release/package validation, ARM64 runtime and
installed-domain semantics remain outside this evidence.

## Accepted final Mac delta — 2026-11-03

Compare the new versioned Mac snapshot against the preserved first snapshot by
exact ordered ID and canonical record hashes. Only xiaomi-ai changed; neuralink
and redtube have no new decisions and remain unclassified. Accept the user's
miaibox.com AI/smart-home and xiaoaiassist.com XiaoAI phone/speaker/smart-home
mapping as USER PROVIDED, not independently verified web proof. Retain the
negative static research limits in evidence/xhigh-ai.json.

Add xiaomi-ai as a collection named Xiaomi AI — MiMo tools and XiaoAI device
assistant / ИИ Xiaomi — инструменты MiMo и помощник XiaoAI. MiMo work tools
(section 4) and independently useful XiaoAI device assistance (section 3) form
a mixed branded portfolio without one common activity: apply the existing
section 6 / Companies and organizations editorial rule (6-organizations). This
is navigation, not factual dominance or independently proved endpoint ownership.
Preserve all 1558 prior rows, 46 topics, AI names, bilingual search, IDs, routing
and serialization. Expected coverage: 1559 classified, 2 unknown, 1211 topic
assignments. No runtime logic or research inputs are added.

Acceptance: exact one-ID delta and baseline preservation; EN/RU label/search and
collection/section/topic assertions; retained neuralink/redtube exclusions; four
Release builds, native Windows x64 and Linux amd64 CTests, representative native
EN/RU captures. ARM64 runtime remains unverified on this x64 host. Reuse prior
Mac-import 33.09/0.62 ms Windows and 24.12/0.61 ms Linux construction/filter
samples for this data-only delta; no new benchmark or profiling claim. Record
source/record/file hashes, provenance and limits in the external versioned ledger.

Native Windows validation limitation: repeated CTest attempts failed the existing
Supported clipboard shows Paste assertion. Temporary bounded waits/reacquisition
did not fix it. A boolean-only diagnostic showed a visible dialog but no retained
fixture URI in the system clipboard, with Paste disabled. These unsuccessful
test changes and diagnostic output were removed; no product or assertion bypass
was retained. Native geometry captures passed through their separate fixture
path. Record Windows offscreen CTest separately from native-window acceptance.

### Final Mac delta verification — 2026-11-03

Added only xiaomi-ai. Actual coverage is 1559/1561, with neuralink and redtube
still literal/unclassified; 46 topics and 1211 topic assignments. Exact ordered
179-ID comparison found one changed decision. All previous 1558 metadata rows
and translation keys/values were retained verbatim. The mixed portfolio uses
section 6 / Companies and organizations with complete bilingual MiMo/XiaoAI
scope names. Existing AI labels/search and selection/routing logic are unchanged.

The new external snapshot is
`C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-mac-results-import-2026-11-03-v2`.
All 228 source files match the Mac's exact-byte SHA-256 manifest. Old/new
decisions hashes are `eb4ee85c053a3547eaa7788c709fa4b216eebf0269033f5429e5d3285ec618b7`
and `a54b3bb2a7f8b2750e0f812360cb5227ca59679a79d20dcb8e992f981e48c158`.
`delta-ledger.json` records every ID's canonical before/after hash, the one
placement, original decision, source hashes, user-statement provenance and limits.
The previous snapshot and editorial ledger are preserved. No web research or
downloaded code execution occurred. USER PROVIDED mapping does not become
independent proof: xhigh static findings remain negative/bounded; official
cached material establishes product context, not both helper-domain mappings.

All four Release GUI/test configurations compiled in the existing GUI-only
caches (Core required OFF; Windows Qt deployment OFF). ARM64 is compile-only
on this x64 host. Windows x64 offscreen CTest passed 5/5 in 18.73 s with the
reviewed bridge CLI explicitly set. Native-window CTest failed only the existing
clipboard/Paste fixture (4/5); its failure and diagnosis above remain an open
acceptance limitation, not a passing native suite. All classification/profile
assertions passed. Linux amd64 offscreen CTest also uses the reviewed CLI; the
final result is 5/5 in 14.86 s. Optional unrelated protocol overrides are unset.

Native light EN/dark RU captures use fresh `final-delta-*-native.png.*` basenames
in ignored build/geosite-classification. Both `.search-xiaomi-ai.png` images were
visually reviewed: readable full names, original ID and collection heading;
global search correctly overrides the remembered AI section/topic. Measured DPR
is 1.5 in both emitted catalog JSONs. Earlier captures remain intact.

This is a data-only runtime delta. Reused historical Mac-import diagnostic samples
are Windows construction/filter 33.09/0.62 ms and Linux 24.12/0.61 ms; no new
benchmark or profiling was needed or claimed. Diff review checked exact delta,
translations, evidence limits, taxonomy, baseline preservation and unchanged
routing/search/serialization. No unresolved product-code defect found; the native
clipboard acceptance failure remains explicit. Build documentation describes the
new capture query. No dependency/package/release/commit/push/PR/settings/service
or Core change; existing dirty work and the parent checkout are preserved.

Final exact-delta verification and `git diff --check` passed. Remaining limits:
2 unclassified IDs, user-supplied endpoint mapping, installed-source semantics,
ARM64 runtime and the native Windows clipboard/Paste test failure.


## Accepted final two editorial decisions — 2026-11-03

Before implementation, the user explicitly approves both final kinds and placements:
RedTube is a collection in section 2, Communication and entertainment / Adult
content (`2-adult`), labeled RedTube adult content / Контент для взрослых RedTube.
Neuralink is a corporate-scope collection in section 6, General lists / Companies
and organizations (`6-organizations`), labeled Neuralink in both languages.
These are USER APPROVED editorial choices, not independently proven facts.

Acceptance: add exactly neuralink and redtube; preserve all 1559 existing metadata
records, 46 topics, search, routing and serialization. Expected coverage is
1561/1561 classified, zero known catalog exclusions and 1213 topic assignments.
Keep future unknown IDs literal. Record cached source file hashes and user approval
in a new external ledger without changing research snapshots. Cached Neuralink
contains only neuralink.com; RedTube includes livehdcams.com, redtube.com,
redtube.com.br, redtubepremium.com and rdtcdn.com (CDN). Exact installed-base
semantic correspondence is not proven. No further research is required.

Run focused metadata checks, four incremental Release builds and native-host
CTest where supported. Retain the open native Windows clipboard/Paste failure;
o fixture bypass or unrelated fix is authorized. ARM64 remains compile-only.
Benchmarking/profiling is not applicable to this two-row data import. No packages,
dependencies, settings, services, Core changes, commits, push or PR.


## Final two editorial implementation verification — 2026-11-03

Added exactly neuralink and redtube with the approved collection kinds, labels,
sections and topics above. Exact comparison with the pre-task dirty-worktree
snapshot preserves all 1559 prior metadata records and all prior translations;
coverage is 1561/1561, zero known catalog exclusions, 46 unchanged topics and
1213 assignments. Future unknown IDs remain literal. No search, routing,
serialization or picker implementation changed.

The new external ledger and pre-task snapshots are in
`C:/Users/PC/Documents/Bimeron-GeoIP-Analysis/geosite-final-two-editorial-2026-11-03`.
`decisions.json` records user approval, cached contents and exact-byte SHA-256:
Neuralink `e4612cd1671eb47ac46e1dcd6c713b0d39a56d23792857bfb4810d601421ff1b`;
RedTube `d624e78c4d13a5820b8d921b235c0deb5816a7725eb553e8db55c94d86470bdf`.
`validation.json` records exact-delta checks. Existing research snapshots are
unchanged. Editorial approval is not independent proof of kind/placement or
exact installed-base semantic correspondence.

Focused assertions cover exact counts, collection kinds, sections, topics,
English labels and Russian search; the existing fixtures retain bilingual
translation, taxonomy, search, selection and serialization regression coverage.
Linux amd64 offscreen CTest passed 5/5 in 20.67 seconds using the reviewed local
bridge CLI. Both Linux Release application/test configurations compiled.
Benchmarking and profiling are not applicable to two metadata rows; historical
samples remain historical. No new performance claim or visual matrix is made.


All four incremental Release application/test builds completed successfully in
the existing GUI-only caches: Windows x64/ARM64 and Linux amd64/arm64 (Core
required OFF, Windows Qt deployment OFF). Windows x64 offscreen CTest passed
5/5 in 24.22 seconds; a separate native Windows run passed 5/5 in 20.50 seconds.
Both used the reviewed local desktop-bridge CLI via BIMERON_GEODATA_CORE;
optional unrelated protocol CLI overrides were not supplied. The native pass
does not diagnose or close the previously observed clipboard/Paste failure:
no fixture was changed or bypassed, and its intermittent failure remains an open
acceptance risk. ARM64 builds are compile-only on this x64 host.

Review of the task-only diff found no correctness, security, regression or
unnecessary-complexity defect. Exact baseline preservation, unchanged taxonomy,
translation keys and evidence limitations were checked; git diff --check passed.
Only GeoSiteLabels.h, ru.json, ProfileVlessTest.cpp and this specification/report
were changed by this task. Prior dirty work, including build documentation,
is preserved. The documented build workflow did not change. No packages,
release, dependency, Core, settings, service, commit, push or PR operation occurred.
Remaining limits are installed-source semantic correspondence, user-approved
editorial provenance, ARM64 runtime and the earlier native clipboard/Paste
instability; full catalog coverage does not remove these evidence limits.


## Unified ordering and interaction implementation — 2026-11-03

The accepted `geosite-list-order-plan.md` interaction addendum supersedes all
historical collection-first and search-overrides-selected descriptions above.
The picker now interleaves available entries alphabetically, uses only openai
then anthropic as the effective AI prefix, and groups nonempty global search by
the eight navigation sections, then Unclassified. Unavailable selections remain
in a separate trailing block. Search intersects Only selected. Both global modes
disable section/topic navigation and restore the browsing topic on exit.
Topic options derive from available classified entries; topicless scopes expose
one disabled All topics option. Counts include every checked ID; empty-state
text distinguishes selected, selected-search and ordinary search. Native Qt
clear-button behavior includes localized accessibility/tooltip text and keyboard
activation. Query, checks and serialized ID order survive transitions.

The reviewed label table records 655 Russian brand-first names and the two
English ChatGPT / OpenAI (Codex), Claude / Anthropic names. All former complete
EN/RU labels remain cached search aliases. Exact baseline comparison preserves
1561 IDs, all kinds, sections, topics, compatibility aliases and scope metadata;
there remain eight sections, 46 topics and 1156 topic assignments. Translation
keys and alias IDs are unique. No automatic runtime label transformation exists.

Focused bilingual fixtures cover all four query/selected combinations and both
transition directions, restored topics, global control state, meaningful topic
options, whitespace, unavailable/unknown IDs, zero selection, no matches,
unchecking the last selected match, clear-button Space activation, AI subsets,
alphabetical/group order, current-item preservation, cancellation and deterministic
serialization. Existing profile/routing fixtures remain active. An old fixture
counting collection/service headings was changed to count actual ID rows.
An implementation failure allowing unavailable unknown rows into Unclassified
was fixed by checking availability explicitly; no assertion was bypassed.

Performance diagnosis used real-catalog elapsed timings for construction, first
search, 100 repeated query edits, 100 selected toggles and 100 AI/browse switches.
The initial remove-at-front/reinsert approach measured Linux mode switches at
88.06 ms; removing from the end reduced this to 19.25 ms. The final Qt model sort
with cached localized alphabetical rank measured 0.666 ms per switch. This is
elapsed-time profiling of the changed path, not CPU sampling. Final Linux sample:
construction 20.37 ms, first grouped search 1.76 ms, repeated queries 0.858 ms,
selected toggles 0.317 ms (100 repetitions each). Earlier baseline single samples
were 25.61/0.84 ms construction/filter. First search now includes one regrouping;
ordinary query edits do not sort or recreate rows. Samples exclude Core
enumeration, parsing and presentation, vary with host load, and impose no timing
gate or end-to-end latency guarantee.

The four existing GUI-only Release application/test configurations compiled:
Windows x64/ARM64 and WSL Linux amd64/arm64. No configure, dependency, Core pin,
installer or packaging flow changed. ARM64 is compile-only on this x64 host.
Final Linux amd64 offscreen CTest passed 5/5 in 31.58 seconds using the reviewed
local desktop-bridge Core CLI and real catalog. The optional unrelated protocol
CLI overrides were not supplied.

The full native editor capture at fractional scaling encountered the existing
`switching tabs must not grow the dialog` geometry assertion before picker
acceptance. The picker capture function is now shared by the full fixture and
an explicit `--geosite-only` path using the same MainWindow stylesheet. Normal
CTest retains the original editor/manager geometry checks. This independently
validates picker DPR appearance without claiming to fix unrelated editor scaling.


The first unanchored capture series also exposed monitor migration: a requested
factor 4/3 yielded actual DPR 1.333 on another monitor, and a later factor 2/3
sample ran below DPR 1 and failed the exact minimum-size fixture. The shared
capture now anchors the picker to the primary screen before native creation,
records its screen name/DPR, and prints actual minimum geometry on failure.
The unchanged exact 590x350 assertion passed on the anchored Russian DPR 1 run.
The capture driver verifies every measured DPR against its target. Historical
failed/unanchored evidence remains under `order-verified-*`; final anchored
captures use `order-screen-*`. No product geometry assertion was weakened.


Final native matrix: EN/RU × light/dark × measured DPR 1/1.5/2, all on
LG HDR 4K. `build/geosite-classification/order-screen-manifest.json` records 12
combinations and 96 scenario PNGs: selected/unavailable, topicless Ecosystems,
grouped search (including the multi-section Google query), selected-search,
selected no-match, AI and minimum size. Every combination passed exact 590x350
minimum geometry and measured-DPR validation. Representative EN/RU images across
all scales and both themes were visually reviewed: text/IDs elide independently,
section headings are readable, the native clear icon has contrast and padding,
disabled context is muted, selected mode is visible, and AI starts ChatGPT then
Claude. Native icon/style rendering is retained without a shared input-style
change. This is picker appearance evidence, not installed routing acceptance.


Final Windows x64 native CTest passed 5/5 in 36.66 seconds with Qt 6.8.3 Windows
platform, documented font/plugin paths, the real catalog and reviewed local
GeoData bridge CLI. The native clipboard/Paste fixture passed unchanged; this
run does not explain away its historical intermittent failure. Final Windows
real-catalog samples: construction 34.70 ms, first grouped search 1.69 ms,
repeated queries 0.800 ms, selected toggles 0.339 ms, AI/browse switches 0.651 ms
(100 repetitions each). The initial implementation's Windows switches were
269.46 ms before the model-sort correction. These diagnostic samples share the
limits stated above and are not a statistically controlled performance guarantee.

Final incremental build logs are `order-verified-build-windows-amd64.log`,
`order-verified-build-windows-arm64.log`, `order-verified-build-linux-amd64.log`
and `order-verified-build-linux-arm64.log`, all successful. Final CTest logs are
`order-verified-ctest-windows.log` and `order-verified-ctest-linux.log` under
ignored build/geosite-classification. Exact-field/translation validation is
`order-exact-validation.json`; captures are indexed by `order-screen-manifest.json`.
The build guide documents the shared isolated capture option and screen anchoring;
no supported build/package flow changed. The accepted plan contains all exact
label changes and the cached-rank implementation rationale.

Final diff review checked preserved taxonomy/kinds/scopes, literal unknown IDs,
availability, selection serialization, localized/legacy search, stable model
identity, noninteractive headings, signal blocking, mode restoration, accessibility,
and unnecessary complexity. No unresolved task-code correctness/security defect
was found. `git diff --check` and the untracked plan whitespace check passed.
All changes remain local and uncommitted; previous partial work is preserved.
No dependencies, packages, installed settings/services, Core mutations, commit,
push, PR or subagents were used. Remaining limits are ARM64 runtime, package/
installed-routing acceptance, inherited catalog scope/provenance, and the separate
fractional editor-geometry/historical clipboard fixture risks. No release is prepared.

## Other labels and empty sections implementation outcome — 2026-11-03

Implemented the accepted follow-up above. Sidebar population is computed once
from the unique available IDs, with fixed row indices and a visible All groups
fallback. Other uses the same label in search headings. Miscellaneous retains
`without`, follows all named topics, and preserves the meaningful-subset rule.
Previous dirty work, including Cursor's AI placement and third priority, remains.
Directly impacted fixture expectations now account for hidden sparse sections,
Miscellaneous selection by stable key, and reversing Cursor's earlier migration
for the historical metadata baseline (28 current AI entries).

All four existing incremental Release builds completed with exit code 0:
Windows amd64, Windows ARM64, WSL Linux amd64 and WSL Linux arm64. Independent
build directories ran concurrently; Linux builds used two jobs each. Logs:
`build/geosite-classification/other-build-windows-amd64.log`,
`other-build-windows-arm64.log`, `other-build-linux-amd64.log` and
`other-build-linux-arm64.log`. The documented stop-build-app guard ran before
Windows builds. No reconfiguration, dependencies or packages were needed.
The full existing build targets compiled fixture executables but did not run them.

Only the normal `build/windows-amd64/Release/bimeron-desktop.exe` was launched,
with Qt 6.8.3 msvc2022_64 bin/plugins and QT_QPA_PLATFORM=windows, visible normal
window requested; running PID 23816. This is a launch, not runtime acceptance.
Tests, CTest, screenshots, visual fixtures, benchmarks and profiling were not run,
as explicitly requested. ARM64 runtime and interface behavior remain unvalidated.
Diff review found no additional taxonomy, ID, routing or serialization changes in
this follow-up; fixed AI/Other row mapping is retained. `git diff --check` passed.
Historical reports remain intact. Build/packaging workflow is unchanged; no
settings, installed Core/service, commit, push or PR changes were made.
