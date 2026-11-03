# GeoSite unified list and AI ordering plan

## Pending follow-up: Other labels and empty sections

Authorized for implementation by the user on 2026-11-03.
This follow-up supersedes the earlier visible labels and validation scope for
this delta only. Existing implementation and validation below are historical.

### Requested behavior

- Rename the topic choice Without topic refinement to Miscellaneous / Прочее.
  Keep its internal data key `without`. All topics remains first; existing named
  topics retain their order; Miscellaneous is always the LAST choice when present.
  Preserve the meaningful-subset rule: offer it only alongside named topics when
  some available classified entries in that scope have no topic. A topicless scope
  still has one disabled All topics choice, not a Miscellaneous-only filter.
- Rename the Unclassified navigation view to Other / Другое. Apply the same label
  to the corresponding search heading. Its meaning remains available IDs absent
  from curated metadata, not entries merely lacking a narrower topic.
- Hide every empty content section, including Other. Determine presence from the
  unique available IDs passed to the picker, not all 1561 metadata entries, current
  query matches, checked entries, or previously selected unavailable entries.
- Keep the relative order of populated sections; Other remains last when present.
  All groups stays visible even with no available entries: it is a utility view
  needed to inspect unavailable selections and the empty-state message.
- Do not hide/reveal sidebar sections as a search query or selected filter changes.
  Existing global-mode disabling/restoration and nonempty search heading rules
  remain in effect. Unavailable selected entries retain their trailing block and
  do not make an otherwise empty content section visible.

### Implementation approach

1. Update accepted trial specification before application edits. Inspect current
   diff and preserve all previous work, including Cursor's move into `4-ai` and
   third priority position after ChatGPT and Claude.
2. The current picker derives section identity from row positions (AI row 7,
   Other row 9, metadata section = row - 1). Prefer setting empty sidebar items
   hidden via QListWidgetItem::setHidden, retaining their underlying fixed rows.
   Do not delete or compact rows without replacing ALL positional assumptions.
   Hidden rows must not be keyboard-selectable; ensure initial/fallback selection
   is the visible All groups item. Never restore a hidden current section.
3. Compute populated sections once from available IDs during construction. The
   available catalog is immutable for this dialog; no query-time recalculation
   or new persisted setting is needed. Preserve existing topic identity/restore.
4. Add the `without` choice after the loop adding named topics. Change only the
   picker-facing English translation keys and Russian strings; inspect consumers
   before deleting old keys to avoid changing unrelated UI.
5. Adjust directly affected fixture expectations for renamed labels, final topic
   position, visible-section subsets and Cursor's already approved metadata/order.
   Do not run CTest, visual test fixtures, full test suites, benchmarks or profiling
   for this follow-up, per the user's explicit request. Report them as not run.
6. Read the complete canonical build document and incrementally rebuild all four
   existing Release configurations (Windows x64/ARM64, Linux amd64/arm64) without
   running tests or producing packages. Honor the documented build-GUI stop guard.
   Diagnose build failures; do not silently skip a requested configuration.
7. Launch ONLY the normal Windows x64 desktop executable from this worktree with
   the documented Qt environment. No fixture, ARM64 executable, installed app or
   Linux GUI launch. Report build results and that runtime testing was omitted.

### Acceptance checklist for implementation review

- Current fully classified database hides Other; a subset containing an unknown
  available ID shows Other with that ID. Unknown unavailable selections alone do
  not show Other. Empty available input keeps All groups usable.
- Sparse catalogs show only populated content sections; AI priority still orders
  ChatGPT, Claude, Cursor regardless of how many earlier sections are hidden.
- Miscellaneous is last when meaningful; topicless Ecosystems still has exactly
  one disabled All topics choice. Saved `without` restores by data key, not index.
- Searching and Only selected neither alter sidebar population nor orphan heading
  visibility; disabled controls and browsing restoration retain previous behavior.
- Labels are Other / Другое and Miscellaneous / Прочее, consistently in EN/RU.
- No catalog IDs, routing, selection serialization or taxonomy assignments change.


Status: implemented and validated, 2026-11-03, following the accepted plan. The interaction addendum is authoritative. Only openai and anthropic receive AI priority.
Checkpoint: acf439a (complete 1561-ID catalog, eight sections, 46 topics).

## Agreed behavior

- Remove Collections and Individual services headings and their sorting priority.
- Use one alphabetical list of available entries, including future unclassified
  IDs; retain the Unclassified navigation view and a separate unavailable-selected
  block at the bottom. Unknown labels fall back to the original ID.
- Preserve all IDs, section/topic assignments, search coverage, check states,
  unavailable selection restoration, cancellation and serialized selection order.
- Put recognizable brand/product names before explanatory text where the current
  label starts with a generic description. Preserve the complete meaning in both
  languages. Do not infer a brand from an opaque ID or strip meaningful words
  automatically. Record the exact label changes for review.
- Ordinary ordering compares localized display names using the existing Qt
  locale-aware comparison, then original ID for ties. Do not change collation,
  transliterate names or introduce a separate unexplained sort key in this task.
- In the Artificial intelligence section, ChatGPT is first (openai), Claude
  second (anthropic). These are navigation priorities, not measured market shares.

## Proposed remaining AI priorities

Proposed short prefix: ChatGPT, Claude, Gemini, DeepSeek, Grok, Perplexity.
Only the first two positions are explicitly specified by the user. Confirm or
accept the remaining proposal before treating it as the agreed ranking; if the
user starts implementation without expanding the ranking, implement only the
two confirmed positions and alphabetize the rest.

Use original IDs for priority, never translated labels. Gemini has two existing
keys (google-deepmind and google-gemini compatibility alias); do not merge them,
invent a narrower domain scope or occupy two popular-prefix slots. If Gemini is
approved, use google-deepmind as canonical priority and leave the compatibility
alias in the ordinary alphabetical remainder with its scope explained. Grok maps
to xai, whose broader xAI scope must remain clear. Label openai as ChatGPT / OpenAI
(Codex), anthropic as Claude / Anthropic, with equivalent Russian labels.

## Context-dependent ordering

Apply the prefix only in the effective AI view: empty search with AI section
selected, or the stable 4-ai topic selected (including All groups plus that topic).
With a nonempty query, use section groups in navigation order and ordinary
alphabetical ordering within each section (see the interaction addendum below).
The Only selected view is also global and alphabetical. All groups and all other
sections stay alphabetical. A hidden/nonmatching priority entry must not appear,
leave a gap or be duplicated. No Popular heading is necessary.

## Implementation sequence

1. Update the accepted specification and acceptance criteria before code changes.
2. Review brand-first labels; keep the historical collection boolean as inert
   metadata for this bounded UI change rather than rewriting all catalog rows.
   Remove its influence on headings, ordering and classification visibility.
3. Separate availability/unclassified state from the old collection buckets.
   Inspect UserRole + 2 consumers and heading filtering before changing them.
4. Implement a small explicit priority lookup and context-aware ordering. Retain
   list items/check states; reorder on effective mode changes, not every check
   toggle or search keystroke when the sorting mode has not changed. Preserve
   focus/current item where possible; avoid recursive itemChanged callbacks.
5. Update EN/RU labels, meaningful regression tests and native capture fixtures.
6. Run focused tests, then existing four incremental Release builds and native
   Windows x64 / Linux amd64 CTests. Read build-and-packaging.md completely before
   build-related work. ARM64 runtime remains unavailable on the x64 host.
7. Review EN/RU AI, ordinary section, global search and unavailable-item captures.
   Measure real-catalog construction/filtering and mode switches because sorting
   now changes with navigation. Report samples and limits, not unsupported speed
   claims. Review diff and git diff --check; document results.

## Acceptance checks

- Exactly 1561 metadata entries, eight sections and 46 stable topics retained.
- Mixed former collection/service entries interleave alphabetically; no old
  headings remain. Unknown available IDs integrate using their literal names.
- Confirmed AI prefix works in EN/RU and with subsets of available IDs; remainder
  is alphabetical. Switching AI/other sections, topics, global search and Only
  selected restores the appropriate order without losing checks or focus.
- Canonical/alias IDs remain separately selectable and serialize unchanged.
- Unavailable selected entries remain removable in the trailing block; heading
  visibility is correct when filters hide all of its rows.
- Original selection ordering, new-ID deterministic serialization, Cancel,
  duplicate-input handling and routing regressions continue to pass.

No packages, dependencies, service/settings changes, push or PR. No implementation
or delegation should start until the user launches the low-effort implementation.

## Interaction addendum — search, selected view and topics

Accepted for implementation. Incorporates the user's five follow-up requirements. This section
supersedes earlier statements implying an ungrouped global search result.

### Existing problem and intended model

Currently the filter checks query first, then Only selected, then section/topic.
Thus typing a query silently ignores an active Only selected button. Section and
topic controls still look usable although neither constrains these global views.
The supplied screenshots demonstrate the misleading retained topic and headings.

Use two independent states: trimmed search query and Only selected toggle. Keep
the last browsing section/topic separately as session-only navigation context.

| Query | Only selected | Visible entries | Ordering |
| --- | --- | --- | --- |
| Empty | Off | Browsing section and topic | Unified alphabetical list; AI prefix in effective AI view |
| Nonempty | Off | Global query matches | Section groups, alphabetical within each |
| Empty | On | All checked entries across the catalog | Unified alphabetical list |
| Nonempty | On | Checked entries matching the query | Section groups, alphabetical within each |

Unavailable selected entries remain a separate trailing block in all applicable
views, filtered by query when present; do not duplicate them in section groups.
When unchecked during Only selected they disappear immediately. Keep the mode
active even at zero selections, with an explicit empty state and usable toggle.
Count always means the total checked entries, including hidden/unavailable ones,
not search matches. Retain the current Selected: N / Selected: N · Only selected
button text and visibly checked styling; tooltip explains that typing searches
within the selection while active. Search placeholder becomes Search selected
names or IDs in this mode, localized in EN/RU.

Toggle Only selected without erasing the query. Clearing search keeps Only
selected active. Turning the toggle off with a query shows all matching entries.
Turning it off with no query restores the remembered browsing section/topic.
Search never automatically toggles selection mode or changes check states.

### Disabled controls and restoration

Disable BOTH section navigation and topic selector whenever a trimmed query is
nonempty OR Only selected is active: they do not filter these global modes.
Retain the previous section as a disabled/muted context, with a tooltip explaining
global search/selected scope. The disabled topic selector shows only All topics;
save its previous stable topic ID separately instead of showing an irrelevant
active-looking topic. Do not force a section change or overwrite saved context.
Restore the saved section/topic when both global modes are inactive; fall back to
All topics if that topic is no longer available. Avoid signal-driven resets
accidentally clearing Only selected, the query, or stored topic selection.

### Sections with no topic refinement

Compute named topics from available classified rows in the browsing scope, not
from current query matches or the selected subset. For All groups use all
available named topics; for a section use that section's available named topics.
If there are none (including Ecosystems, Unclassified or an empty scope), the
selector contains exactly All topics and is disabled. Never offer Without topic
refinement as the only alternative to All topics.
If named topics exist, enable the selector in browsing mode. Offer Without topic
refinement only when available classified entries in that scope actually lack a
topic; preserve stable topic IDs and their established ordering. Global-mode
disabling takes precedence over topic availability.

### Search clear affordance

Prefer QLineEdit's built-in clear button (setClearButtonEnabled) and the current
application Qt style. Inspect the actual light/dark result before adding styling.
Use a small centered cross with a comfortable hit target, no prominent border or
permanent background, and normal hover/focus feedback. Do not use a Unicode cross
as an improvised icon or change shared application-wide input styling.
Show it whenever the raw field text is nonempty, including whitespace; filtering
continues to use trimmed text. Clear through the normal textChanged path, retain
search focus and selection mode, and restore browsing only when appropriate.
Provide localized Clear search tooltip/accessibility text and keyboard access.
Verify icon contrast, text padding, no overlap and DPR 1/1.5/2 in light/dark themes.

### Search section headings

For every nonempty trimmed query, group matching available rows by the eight
sections in navigation order, then Unclassified when needed. Emit only nonempty
headings, including when there is just one matching section. Headings use the
existing compact heading treatment with localized section names, no checkboxes,
and no selectable entry semantics. Within each section use localized alphabetical
ordering and ID tie-break; the AI popularity prefix is inactive during search.
The selected-search intersection follows exactly the same grouping rules.
Maintain the unavailable block last. No collection/service headings survive.
Do not append duplicate section labels to each row when a heading already gives
the context. Empty results have no orphan headings.

### Implementation and verification additions

- Implement visibility, effective sorting mode, heading mode and control enabled
  state from one coherent state calculation; replace the old bucket-dependent
  heading scan. Do not create independent callbacks with contradictory resets.
- Preserve checks, current item/focus where possible and stable selection
  serialization while regrouping; headers must never enter selectedIds().
- Test all four state combinations and transitions in both directions, including
  nonempty search -> selected -> clear -> unselect mode -> restored topic.
- Test whitespace-only input, zero selections, no matches, a missing/disabled
  topic, available unknown IDs, unavailable selected IDs, and unchecking the last
  visible selected match. Distinguish No selected groups, No matching selected
  groups, and ordinary No matching groups in localized empty-state text.
- Verify Ecosystems has one disabled All topics option; named-topic scopes expose
  only meaningful options, and temporary search results do not rebuild that list.
- Test grouped search section order, alphabetic order inside each section, single
  section headings, hidden empty headings and ordinary AI prefix restoration.
- Capture EN/RU light/dark examples corresponding to all three supplied screens:
  selected view with disabled controls, topicless Ecosystems, and grouped global
  search with clear button. Also capture search-within-selected and zero matches.
- Extend performance samples to repeated query edits and toggles; avoid rebuilding
  metadata or full widgets per keystroke. No special network/ranking requests.

Implementation was explicitly authorized for the current low-effort phase.

## Reviewed label changes

All scopes, IDs, kinds and topics are retained. Former EN/RU labels remain search aliases.

| ID | Previous EN | Current EN | Previous RU | Current RU |
| --- | --- | --- | --- | --- |
| `2kgames` | 2K games | 2K games | Игры 2K | 2K — игры |
| `36kr` | 36Kr news | 36Kr news | Новости 36Kr | 36Kr — новости |
| `51job` | 51job recruitment services | 51job recruitment services | Сервисы поиска работы 51job | 51job — сервисы поиска работы |
| `9news` | Nine news and television | Nine news and television | Новости и телевидение Nine | Nine — новости и телевидение |
| `activision` | Activision games | Activision games | Игры Activision | Activision — игры |
| `activision-blizzard` | Activision Blizzard games | Activision Blizzard games | Игры Activision Blizzard | Activision Blizzard — игры |
| `addthis` | AddThis audience tools | AddThis audience tools | Инструменты работы с аудиторией AddThis | AddThis — инструменты работы с аудиторией |
| `addtoany` | AddToAny sharing tools | AddToAny sharing tools | Инструменты публикации AddToAny | AddToAny — инструменты публикации |
| `afp` | AFP news | AFP news | Новости AFP | AFP — новости |
| `alibabacloud` | Alibaba Cloud international | Alibaba Cloud international | Международное облако Alibaba | Alibaba — международное облако |
| `aliyun` | Alibaba Cloud China | Alibaba Cloud China | Облако Alibaba в Китае | Alibaba — облако в Китае |
| `aliyun-drive` | Aliyun Drive | Aliyun Drive | Облачное хранилище Aliyun Drive | Aliyun Drive — облачное хранилище |
| `aligames` | Alibaba games | Alibaba games | Игры Alibaba | Alibaba — игры |
| `anon-v` | Anon-V adult video | Anon-V adult video | Видео для взрослых Anon-V | Anon-V — видео для взрослых |
| `anthropic` | Anthropic AI tools (Claude) | Claude / Anthropic | Инструменты ИИ Anthropic (Claude) | Claude / Anthropic |
| `openai` | OpenAI and ChatGPT (Codex) | ChatGPT / OpenAI (Codex) | OpenAI и ChatGPT (Codex) | ChatGPT / OpenAI (Codex) |
| `ap` | Associated Press news | Associated Press news | Новости Associated Press | Associated Press — новости |
| `aparat` | Aparat video | Aparat video | Видео Aparat | Aparat — видео |
| `apifox` | Apifox API tools | Apifox API tools | Инструменты API Apifox | Apifox — инструменты API |
| `apipost` | Apipost API tools | Apipost API tools | Инструменты API Apipost | Apipost — инструменты API |
| `apple-music` | Apple Music | Apple Music | Музыка Apple Music | Apple Music — музыка |
| `apple-podcasts` | Apple Podcasts | Apple Podcasts | Подкасты Apple | Apple — подкасты |
| `apple-update` | Apple software updates and recovery | Apple software updates and recovery | Обновления и восстановление ПО Apple | Apple — обновления и восстановление ПО |
| `aptoide` | Aptoide app store | Aptoide app store | Магазин приложений Aptoide | Aptoide — магазин приложений |
| `askdiandian` | Diandian AI | Diandian AI | ИИ Diandian | Diandian — ИИ |
| `atlassian` | Atlassian work and development tools | Atlassian work and development tools | Инструменты работы и разработки Atlassian | Atlassian — инструменты работы и разработки |
| `bandcamp` | Bandcamp music | Bandcamp music | Музыка Bandcamp | Bandcamp — музыка |
| `bdsmhub` | BDSMHub adult content | BDSMHub adult content | Контент для взрослых BDSMHub | BDSMHub — контент для взрослых |
| `beisen` | Beisen recruitment tools | Beisen recruitment tools | Инструменты подбора персонала Beisen | Beisen — инструменты подбора персонала |
| `bestore` | Bestore snack retail | Bestore snack retail | Магазин снеков Bestore | Bestore — магазин снеков |
| `bethesda` | Bethesda games | Bethesda games | Игры Bethesda | Bethesda — игры |
| `bilibili-game` | Bilibili games | Bilibili games | Игры Bilibili | Bilibili — игры |
| `blizzard` | Blizzard games | Blizzard games | Игры Blizzard | Blizzard — игры |
| `bluearchive` | Blue Archive game | Blue Archive game | Игра Blue Archive | Blue Archive — игра |
| `bluepoch` | Bluepoch games | Bluepoch games | Игры Bluepoch | Bluepoch — игры |
| `boboporn` | BoboPorn adult content | BoboPorn adult content | Контент для взрослых BoboPorn | BoboPorn — контент для взрослых |
| `bohemia` | Bohemia Interactive games | Bohemia Interactive games | Игры Bohemia Interactive | Bohemia Interactive — игры |
| `brazzers` | Brazzers and Mofos adult content | Brazzers and Mofos adult content | Контент для взрослых Brazzers и Mofos | Brazzers и Mofos — контент для взрослых |
| `btdig` | BTDig DHT torrent search | BTDig DHT torrent search | Поиск торрентов DHT BTDig | BTDig — поиск торрентов DHT |
| `bytedance-ai-!cn` | ByteDance AI tools outside mainland China | ByteDance AI tools outside mainland China | Инструменты ИИ ByteDance за пределами Китая | ByteDance — инструменты ИИ за пределами Китая |
| `canva` | Canva and Affinity design tools | Canva and Affinity design tools | Инструменты дизайна Canva и Affinity | Canva и Affinity — инструменты дизайна |
| `catchplay` | CATCHPLAY video | CATCHPLAY video | Видео CATCHPLAY | CATCHPLAY — видео |
| `cavporn` | CavPorn adult content | CavPorn adult content | Контент для взрослых CavPorn | CavPorn — контент для взрослых |
| `cdn77` | CDN77 delivery network | CDN77 delivery network | Сеть доставки CDN77 | CDN77 — сеть доставки |
| `cdprojekt` | CD Projekt games | CD Projekt games | Игры CD Projekt | CD Projekt — игры |
| `category-forums-ir` | Quera developer learning and recruitment platform | Quera developer learning and recruitment platform | Платформа обучения и найма разработчиков Quera | Quera — платформа обучения и найма разработчиков |
| `clips4sale` | Clips4Sale adult video | Clips4Sale adult video | Видео для взрослых Clips4Sale | Clips4Sale — видео для взрослых |
| `cnbeta` | cnBeta news | cnBeta news | Новости cnBeta | cnBeta — новости |
| `cnn` | CNN news | CNN news | Новости CNN | CNN — новости |
| `coding` | CODING development platform | CODING development platform | Платформа разработки CODING | CODING — платформа разработки |
| `comfy` | Comfy AI creation tools | Comfy AI creation tools | Инструменты ИИ Comfy | Comfy — инструменты ИИ |
| `costco` | Costco retail | Costco retail | Магазин Costco | Costco — магазин |
| `ctyun` | Tianyi Cloud | Tianyi Cloud | Облако Tianyi | Tianyi — облако |
| `cuinc` | CUINC adult content | CUINC adult content | Контент для взрослых CUINC | CUINC — контент для взрослых |
| `cursor` | Cursor coding tools | Cursor coding tools | Инструменты программирования Cursor | Cursor — инструменты программирования |
| `cygames` | Cygames games | Cygames games | Игры Cygames | Cygames — игры |
| `dailymail` | Daily Mail news | Daily Mail news | Новости Daily Mail | Daily Mail — новости |
| `dailymotion` | Dailymotion video | Dailymotion video | Видео Dailymotion | Dailymotion — видео |
| `dart` | Dart programming tools | Dart programming tools | Инструменты программирования Dart | Dart — инструменты программирования |
| `deezer` | Deezer music | Deezer music | Музыка Deezer | Deezer — музыка |
| `digitalplayground` | Digital Playground adult video | Digital Playground adult video | Видео для взрослых Digital Playground | Digital Playground — видео для взрослых |
| `dmm-porn` | DMM adult content | DMM adult content | Контент для взрослых DMM | DMM — контент для взрослых |
| `dola` | Dola AI (formerly Cici) | Dola AI (formerly Cici) | ИИ Dola (ранее Cici) | Dola — ИИ (ранее Cici) |
| `dongjiao` | Dongjiao massage booking services | Dongjiao massage booking services | Сервисы заказа массажа Dongjiao | Dongjiao — сервисы заказа массажа |
| `douyin` | Douyin services | Douyin services | Сервисы Douyin | Douyin — сервисы |
| `dw` | Deutsche Welle news | Deutsche Welle news | Новости Deutsche Welle | Deutsche Welle — новости |
| `dwion` | DiLian delivery network | DiLian delivery network | Сеть доставки DiLian | DiLian — сеть доставки |
| `ea` | Electronic Arts games | Electronic Arts games | Игры Electronic Arts | Electronic Arts — игры |
| `ebuyer` | Ebuyer retail | Ebuyer retail | Магазин Ebuyer | Ebuyer — магазин |
| `embark` | Embark games | Embark games | Игры Embark | Embark — игры |
| `erolabs` | EROLABS adult games | EROLABS adult games | Игры для взрослых EROLABS | EROLABS — игры для взрослых |
| `fans66` | Fans66 adult content | Fans66 adult content | Контент для взрослых Fans66 | Fans66 — контент для взрослых |
| `fastly` | Fastly delivery network | Fastly delivery network | Сеть доставки Fastly | Fastly — сеть доставки |
| `figma` | Figma design tools | Figma design tools | Инструменты дизайна Figma | Figma — инструменты дизайна |
| `firebase` | Firebase developer services | Firebase developer services | Сервисы Firebase для разработчиков | Firebase — сервисы для разработчиков |
| `flutter` | Flutter development tools | Flutter development tools | Инструменты разработки Flutter | Flutter — инструменты разработки |
| `kktv` | KKTV video | KKTV video | Видео KKTV | KKTV — видео |
| `konami` | Konami games and related products | Konami games and related products | Игры и другие продукты Konami | Konami — игры и другие продукты |
| `ku6` | Ku6 video | Ku6 video | Видео Ku6 | Ku6 — видео |
| `kurogames` | Kuro Games titles | Kuro Games titles | Игры Kuro Games | Kuro Games — игры |
| `lastfm` | Last.fm music | Last.fm music | Музыка Last.fm | Last.fm — музыка |
| `likee` | Likee video | Likee video | Видео Likee | Likee — видео |
| `lowiro` | Lowiro games | Lowiro games | Игры Lowiro | Lowiro — игры |
| `microsoft-dev` | Microsoft development tools | Microsoft development tools | Инструменты разработки Microsoft | Microsoft — инструменты разработки |
| `mihoyo-cn` | miHoYo China games | miHoYo China games | Игры miHoYo для Китая | miHoYo — игры для Китая |
| `miniso` | MINISO shopping | MINISO shopping | Магазин MINISO | MINISO — магазин |
| `missav` | MissAV video | MissAV video | Видео MissAV | MissAV — видео |
| `moji` | Moji Weather | Moji Weather | Погода Moji | Moji — погода |
| `momo` | momo shopping | momo shopping | Магазин momo | momo — магазин |
| `msn` | MSN news and entertainment | MSN news and entertainment | Новости и развлечения MSN | MSN — новости и развлечения |
| `mudvod` | Mudvod video | Mudvod video | Видео Mudvod | Mudvod — видео |
| `mvideo` | M.Video shopping | M.Video shopping | Магазин М.Видео | М.Видео — магазин |
| `neowin` | Neowin news | Neowin news | Новости Neowin | Neowin — новости |
| `netflav` | Netflav video | Netflav video | Видео Netflav | Netflav — видео |
| `netflix` | Netflix video | Netflix video | Видео Netflix | Netflix — видео |
| `newegg` | Newegg shopping | Newegg shopping | Магазин Newegg | Newegg — магазин |
| `nexon` | Nexon games | Nexon games | Игры Nexon | Nexon — игры |
| `nike` | Nike shopping | Nike shopping | Магазин Nike | Nike — магазин |
| `nikkan-gendai` | Nikkan Gendai news | Nikkan Gendai news | Новости Nikkan Gendai | Nikkan Gendai — новости |
| `nintendo` | Nintendo games | Nintendo games | Игры Nintendo | Nintendo — игры |
| `nordstrom` | Nordstrom shopping | Nordstrom shopping | Магазин Nordstrom | Nordstrom — магазин |
| `nutaku` | Nutaku games | Nutaku games | Игры Nutaku | Nutaku — игры |
| `okko` | Okko video | Okko video | Видео Okko | Okko — видео |
| `olevod` | Olevod video | Olevod video | Видео Olevod | Olevod — видео |
| `oskelly` | OSKELLY shopping | OSKELLY shopping | Магазин OSKELLY | OSKELLY — магазин |
| `ozon` | Ozon shopping | Ozon shopping | Магазин Ozon | Ozon — магазин |
| `pandanet` | Pandanet games | Pandanet games | Игры Pandanet | Pandanet — игры |
| `pchome` | PChome shopping | PChome shopping | Магазин PChome | PChome — магазин |
| `redtube` | RedTube adult content | RedTube adult content | Контент для взрослых RedTube | RedTube — контент для взрослых |
| `salesforce` | Salesforce properties | Salesforce properties | Сервисы Salesforce | Salesforce — сервисы |
| `sina` | Sina and Weibo services | Sina and Weibo services | Сервисы Sina и Weibo | Sina и Weibo — сервисы |
| `xiaomi-ai` | Xiaomi AI — MiMo tools and XiaoAI device assistant | Xiaomi AI — MiMo tools and XiaoAI device assistant | ИИ Xiaomi — инструменты MiMo и помощник XiaoAI | Xiaomi — ИИ, инструменты MiMo и помощник XiaoAI |
| `xunlei` | Xunlei services | Xunlei services | Сервисы Xunlei | Xunlei — сервисы |
| `c-span` | C-SPAN public affairs video | C-SPAN public affairs video | Видео C-SPAN | C-SPAN — видео |
| `camwhores` | CamWhores adult video | CamWhores adult video | Видео для взрослых CamWhores | CamWhores — видео для взрослых |
| `chinatelecom` | China Telecom group services | China Telecom group services | Сервисы группы China Telecom | China Telecom — сервисы группы |
| `chinaunicom` | China Unicom group services | China Unicom group services | Сервисы группы China Unicom | China Unicom — сервисы группы |
| `docker` | Docker tooling and image distribution | Docker tooling and image distribution | Инструменты Docker и распространение образов | Docker — инструменты и распространение образов |
| `epicgames` | Epic Games games and creator ecosystem | Epic Games games and creator ecosystem | Игры и экосистема для авторов Epic Games | Epic Games — игры и экосистема для авторов |
| `freebuff` | Freebuff coding tools | Freebuff coding tools | Инструменты программирования Freebuff | Freebuff — инструменты программирования |
| `freenode` | Freenode chat network | Freenode chat network | Сеть общения Freenode | Freenode — сеть общения |
| `gaijin` | Gaijin games | Gaijin games | Игры Gaijin | Gaijin — игры |
| `gimy` | Gimy video | Gimy video | Видео Gimy | Gimy — видео |
| `gitlab` | GitLab development platform | GitLab development platform | Платформа разработки GitLab | GitLab — платформа разработки |
| `gog` | GOG games | GOG games | Игры GOG | GOG — игры |
| `google-deepmind` | Google AI tools and research | Google AI tools and research | Инструменты ИИ и исследования Google | Google — инструменты ИИ и исследования |
| `gucci` | Gucci shopping | Gucci shopping | Магазин Gucci | Gucci — магазин |
| `hashicorp` | HashiCorp development tools | HashiCorp development tools | Инструменты разработки HashiCorp | HashiCorp — инструменты разработки |
| `hm` | H&M shopping | H&M shopping | Магазин H&M | H&M — магазин |
| `homedepot` | The Home Depot shopping | The Home Depot shopping | Магазин The Home Depot | The Home Depot — магазин |
| `hooligapps` | Hooligapps adult games | Hooligapps adult games | Игры для взрослых Hooligapps | Hooligapps — игры для взрослых |
| `hoyoverse` | HoYoverse games | HoYoverse games | Игры HoYoverse | HoYoverse — игры |
| `huffpost` | HuffPost news | HuffPost news | Новости HuffPost | HuffPost — новости |
| `huggingface` | Hugging Face AI platform | Hugging Face AI platform | Платформа ИИ Hugging Face | Hugging Face — платформа ИИ |
| `humblebundle` | Humble Bundle digital store | Humble Bundle digital store | Магазин цифровых товаров Humble Bundle | Humble Bundle — магазин цифровых товаров |
| `ikea` | IKEA shopping | IKEA shopping | Магазин IKEA | IKEA — магазин |
| `illgames` | ILLGAMES adult games | ILLGAMES adult games | Игры для взрослых ILLGAMES | ILLGAMES — игры для взрослых |
| `illusion` | Illusion legacy games | Illusion legacy games | Игры Illusion — прежний ключ | Illusion — игры, прежний ключ |
| `iqiyi` | iQIYI video | iQIYI video | Видео iQIYI | iQIYI — видео |
| `itchio` | itch.io games | itch.io games | Игры itch.io | itch.io — игры |
| `java` | Java language platform | Java language platform | Платформа языка Java | Java — платформа языка |
| `jetbrains` | JetBrains development tools | JetBrains development tools | Инструменты разработки JetBrains | JetBrains — инструменты разработки |
| `jetbrains-ai` | JetBrains AI tools | JetBrains AI tools | Инструменты ИИ JetBrains | JetBrains — инструменты ИИ |
| `jfrog` | JFrog artifacts platform | JFrog artifacts platform | Платформа артефактов JFrog | JFrog — платформа артефактов |
| `jiguang` | Jiguang developer services | Jiguang developer services | Сервисы Jiguang для разработчиков | Jiguang — сервисы для разработчиков |
| `kaggle` | Kaggle data science platform | Kaggle data science platform | Платформа науки о данных Kaggle | Kaggle — платформа науки о данных |
| `lagou` | Lagou recruitment | Lagou recruitment | Поиск работы и подбор персонала Lagou | Lagou — поиск работы и подбор персонала |
| `liepin` | Liepin recruitment | Liepin recruitment | Поиск работы и подбор персонала Liepin | Liepin — поиск работы и подбор персонала |
| `linotype` | Linotype font platform | Linotype font platform | Платформа шрифтов Linotype | Linotype — платформа шрифтов |
| `louisvuitton` | Louis Vuitton shopping | Louis Vuitton shopping | Магазин Louis Vuitton | Louis Vuitton — магазин |
| `ltn` | Liberty Times news and yes123 recruitment | Liberty Times news and yes123 recruitment | Новости Liberty Times и поиск работы yes123 | Liberty Times — новости и поиск работы yes123 |
| `mojang` | Mojang games | Mojang games | Игры Mojang | Mojang — игры |
| `mongodb` | MongoDB database platform | MongoDB database platform | Платформа баз данных MongoDB | MongoDB — платформа баз данных |
| `netease` | NetEase company services | NetEase company services | Сервисы компании NetEase | NetEase — сервисы компании |
| `pgyer` | Pgyer, Tracup and FrontJS tools | Pgyer, Tracup and FrontJS tools | Инструменты Pgyer, Tracup и FrontJS | Pgyer, Tracup и FrontJS — инструменты |
| `pixiv` | pixiv company services | pixiv company services | Сервисы компании pixiv | pixiv — сервисы компании |
| `adblockplus` | Adblock Plus privacy and ad blocking | Adblock Plus privacy and ad blocking | Блокировка рекламы и защита приватности Adblock Plus | Adblock Plus — блокировка рекламы и защита приватности |
| `800best` | BEST logistics | BEST logistics | Логистика BEST | BEST — логистика |
| `aamgame` | AAM amusement games | AAM amusement games | Развлекательные игры AAM | AAM — развлекательные игры |
| `abc` | ABC television and news | ABC television and news | Телевидение и новости ABC | ABC — телевидение и новости |
| `abema` | ABEMA and Ameba media services | ABEMA and Ameba media services | Медиасервисы ABEMA и Ameba | ABEMA и Ameba — медиасервисы |
| `accuweather` | AccuWeather forecasts and weather services | AccuWeather forecasts and weather services | Прогнозы и погодные сервисы AccuWeather | AccuWeather — прогнозы и погодные сервисы |
| `acer` | Acer products and corporate services | Acer products and corporate services | Продукты и корпоративные сервисы Acer | Acer — продукты и корпоративные сервисы |
| `acfun` | AcFun video community | AcFun video community | Видеосообщество AcFun | AcFun — видеосообщество |
| `actalis` | Actalis certificates | Actalis certificates | Сертификаты Actalis | Actalis — сертификаты |
| `adblock` | AdBlock privacy and ad blocking | AdBlock privacy and ad blocking | Блокировка рекламы и защита приватности AdBlock | AdBlock — блокировка рекламы и защита приватности |
| `adguard` | AdGuard product portfolio | AdGuard product portfolio | Портфель продуктов AdGuard | AdGuard — портфель продуктов |
| `adidas` | Adidas shopping | Adidas shopping | Покупки Adidas | Adidas — покупки |
| `adjust` | Adjust advertising measurement | Adjust advertising measurement | Рекламная аналитика Adjust | Adjust — рекламная аналитика |
| `adobe` | Adobe ecosystem | Adobe ecosystem | Экосистема Adobe | Adobe — экосистема |
| `adobe-activation` | Adobe license-validation endpoints | Adobe license-validation endpoints | Адреса проверки лицензий Adobe | Adobe — адреса проверки лицензий |
| `aerogard` | Aerogard insect repellent brand | Aerogard insect repellent brand | Бренд средств от насекомых Aerogard | Aerogard — бренд средств от насекомых |
| `afdian` | AFDIAN creator memberships | AFDIAN creator memberships | Подписки на авторов AFDIAN | AFDIAN — подписки на авторов |
| `agora` | Agora real-time communication platforms | Agora real-time communication platforms | Платформы связи в реальном времени Agora | Agora — платформы связи в реальном времени |
| `aiqicha` | Aiqicha company lookup | Aiqicha company lookup | Проверка компаний Aiqicha | Aiqicha — проверка компаний |
| `airbnb` | Airbnb accommodation | Airbnb accommodation | Жильё Airbnb | Airbnb — жильё |
| `airchina` | Air China aviation services | Air China aviation services | Авиационные сервисы Air China | Air China — авиационные сервисы |
| `airwick` | Air Wick home fragrance brand | Air Wick home fragrance brand | Бренд ароматов для дома Air Wick | Air Wick — бренд ароматов для дома |
| `aisiku` | Aisiku and Coolab language learning | Aisiku and Coolab language learning | Изучение языков Aisiku и Coolab | Aisiku и Coolab — изучение языков |
| `aixcoder` | aiXcoder coding assistant | aiXcoder coding assistant | Помощник программиста aiXcoder | aiXcoder — помощник программиста |
| `akamai` | Akamai delivery infrastructure | Akamai delivery infrastructure | Инфраструктура доставки Akamai | Akamai — инфраструктура доставки |
| `alibaba` | Alibaba ecosystem | Alibaba ecosystem | Экосистема Alibaba | Alibaba — экосистема |
| `aljazeera` | Al Jazeera media | Al Jazeera media | Медиа Al Jazeera | Al Jazeera — медиа |
| `alphabet` | Alphabet companies and services | Alphabet companies and services | Компании и сервисы Alphabet | Alphabet — компании и сервисы |
| `amap` | Amap maps and navigation | Amap maps and navigation | Карты и навигация Amap | Amap — карты и навигация |
| `amazon` | Amazon ecosystem | Amazon ecosystem | Экосистема Amazon | Amazon — экосистема |
| `amazontrust` | Amazon Trust certificates | Amazon Trust certificates | Сертификаты Amazon Trust | Amazon Trust — сертификаты |
| `amc` | AMC television | AMC television | Телевидение AMC | AMC — телевидение |
| `amd` | AMD products and services | AMD products and services | Продукты и сервисы AMD | AMD — продукты и сервисы |
| `amplitude` | Amplitude product analytics | Amplitude product analytics | Продуктовая аналитика Amplitude | Amplitude — продуктовая аналитика |
| `anandtech` | AnandTech technology content and forums | AnandTech technology content and forums | Технологические материалы и форумы AnandTech | AnandTech — технологические материалы и форумы |
| `android` | Android operating system | Android operating system | Операционная система Android | Android — операционная система |
| `anexia` | Anexia and netcup infrastructure | Anexia and netcup infrastructure | Инфраструктура Anexia и netcup | Anexia и netcup — инфраструктура |
| `anker` | Anker brands and products | Anker brands and products | Бренды и продукты Anker | Anker — бренды и продукты |
| `annas-archive` | Anna’s Archive books and research library | Anna’s Archive books and research library | Библиотека книг и исследований Anna’s Archive | Anna’s Archive — библиотека книг и исследований |
| `apkmirror` | APKMirror app downloads | APKMirror app downloads | Загрузка приложений APKMirror | APKMirror — загрузка приложений |
| `apkpure` | APKPure app downloads | APKPure app downloads | Загрузка приложений APKPure | APKPure — загрузка приложений |
| `apple` | Apple ecosystem | Apple ecosystem | Экосистема Apple | Apple — экосистема |
| `apple-pki` | Apple certificate infrastructure | Apple certificate infrastructure | Инфраструктура сертификатов Apple | Apple — инфраструктура сертификатов |
| `applysquare` | ApplySquare education | ApplySquare education | Образование ApplySquare | ApplySquare — образование |
| `archiveofourown` | Archive of Our Own fiction | Archive of Our Own fiction | Фанфики Archive of Our Own | Archive of Our Own — фанфики |
| `archivetoday` | Archive.today web archive | Archive.today web archive | Веб-архив Archive.today | Archive.today — веб-архив |
| `arphic` | Arphic fonts and iFontCloud | Arphic fonts and iFontCloud | Шрифты Arphic и iFontCloud | Arphic и iFontCloud — шрифты |
| `artstation` | ArtStation artist portfolios | ArtStation artist portfolios | Портфолио художников ArtStation | ArtStation — портфолио художников |
| `asahi` | Asahi publications | Asahi publications | Издания Asahi | Asahi — издания |
| `asobo` | Asobo flight-simulation games | Asobo flight-simulation games | Авиасимуляторы Asobo | Asobo — авиасимуляторы |
| `asproex` | Asproex cryptocurrency exchange | Asproex cryptocurrency exchange | Криптовалютная биржа Asproex | Asproex — криптовалютная биржа |
| `asus` | ASUS corporate and cloud services | ASUS corporate and cloud services | Корпоративные и облачные сервисы ASUS | ASUS — корпоративные и облачные сервисы |
| `autodesk` | Autodesk ecosystem | Autodesk ecosystem | Экосистема Autodesk | Autodesk — экосистема |
| `autoru` | Auto.ru vehicle marketplace | Auto.ru vehicle marketplace | Автомаркет Auto.ru | Auto.ru — автомаркет |
| `auxgroup` | AUX Group ecosystem | AUX Group ecosystem | Экосистема AUX Group | AUX Group — экосистема |
| `avmoo` | AVMOO adult catalogue | AVMOO adult catalogue | Каталог для взрослых AVMOO | AVMOO — каталог для взрослых |
| `bahamut` | Bahamut gaming community | Bahamut gaming community | Игровое сообщество Bahamut | Bahamut — игровое сообщество |
| `baltamatica` | BALTAMATICA numerical computing | BALTAMATICA numerical computing | Численные вычисления BALTAMATICA | BALTAMATICA — численные вычисления |
| `bangumi` | Bangumi community | Bangumi community | Сообщество Bangumi | Bangumi — сообщество |
| `bbc` | BBC broadcasting | BBC broadcasting | Вещание BBC | BBC — вещание |
| `beats` | Beats audio products | Beats audio products | Аудиопродукция Beats | Beats — аудиопродукция |
| `beget` | Beget hosting | Beget hosting | Хостинг Beget | Beget — хостинг |
| `bestbuy` | Best Buy retail brands | Best Buy retail brands | Торговые бренды Best Buy | Best Buy — торговые бренды |
| `bestv` | BesTV television | BesTV television | Телевидение BesTV | BesTV — телевидение |
| `betboom` | BetBoom betting | BetBoom betting | Ставки BetBoom | BetBoom — ставки |
| `betterexplained` | BetterExplained learning | BetterExplained learning | Обучение BetterExplained | BetterExplained — обучение |
| `bilibili-cdn` | Bilibili delivery domains | Bilibili delivery domains | Домены доставки Bilibili | Bilibili — домены доставки |
| `bilibili2` | Bilibili2 adult site | Bilibili2 adult site | Сайт для взрослых Bilibili2 | Bilibili2 — сайт для взрослых |
| `binance` | Binance cryptocurrency services | Binance cryptocurrency services | Криптовалютные услуги Binance | Binance — криптовалютные услуги |
| `bitauto` | Bitauto automotive portal | Bitauto automotive portal | Автомобильный портал Bitauto | Bitauto — автомобильный портал |
| `bitflyer` | bitFlyer cryptocurrency exchange | bitFlyer cryptocurrency exchange | Криптовалютная биржа bitFlyer | bitFlyer — криптовалютная биржа |
| `bitly` | Bitly link shortening | Bitly link shortening | Сокращение ссылок Bitly | Bitly — сокращение ссылок |
| `bitrix` | Bitrix business software | Bitrix business software | Деловое ПО Bitrix | Bitrix — деловое ПО |
| `bitsquare` | Bisq cryptocurrency exchange | Bisq cryptocurrency exchange | Криптовалютная биржа Bisq | Bisq — криптовалютная биржа |
| `bitwarden` | Bitwarden password manager | Bitwarden password manager | Менеджер паролей Bitwarden | Bitwarden — менеджер паролей |
| `blogspot` | Blogger and Blogspot blogs | Blogger and Blogspot blogs | Блоги Blogger и Blogspot | Blogger и Blogspot — блоги |
| `bloomberg` | Bloomberg corporate portfolio | Bloomberg corporate portfolio | Портфель компаний и сервисов Bloomberg | Bloomberg — портфель компаний и сервисов |
| `bluepoch-games` | Reverse 1999 game endpoints | Reverse 1999 game endpoints | Адреса игры Reverse 1999 | Reverse 1999 — адреса игры |
| `bluesky` | Bluesky social network | Bluesky social network | Социальная сеть Bluesky | Bluesky — социальная сеть |
| `blurams` | Blurams cameras and monitoring | Blurams cameras and monitoring | Камеры и видеонаблюдение Blurams | Blurams — камеры и видеонаблюдение |
| `bmw` | BMW Group portfolio | BMW Group portfolio | Портфель компаний и сервисов BMW Group | BMW Group — портфель компаний и сервисов |
| `boc` | Bank of China financial services | Bank of China financial services | Финансовые услуги Bank of China | Bank of China — финансовые услуги |
| `bongacams` | BongaCams adult streams | BongaCams adult streams | Трансляции для взрослых BongaCams | BongaCams — трансляции для взрослых |
| `booking` | Booking.com accommodation | Booking.com accommodation | Бронирование жилья Booking.com | Booking.com — бронирование жилья |
| `books` | Books.com.tw bookstore | Books.com.tw bookstore | Книжный магазин Books.com.tw | Books.com.tw — книжный магазин |
| `boomerang` | Boomerang television | Boomerang television | Телевидение Boomerang | Boomerang — телевидение |
| `bootstrap` | Bootstrap web framework | Bootstrap web framework | Веб-фреймворк Bootstrap | Bootstrap — веб-фреймворк |
| `boxun` | Boxun and BowenPress content sites | Boxun and BowenPress content sites | Контентные сайты Boxun и BowenPress | Boxun и BowenPress — контентные сайты |
| `braveux` | Brave UX design projects | Brave UX design projects | Дизайн-проекты Brave UX | Brave UX — дизайн-проекты |
| `bridgestone` | Bridgestone products and services | Bridgestone products and services | Продукция и услуги Bridgestone | Bridgestone — продукция и услуги |
| `brightcove` | Brightcove video platform and tools | Brightcove video platform and tools | Видеоплатформа и инструменты Brightcove | Brightcove — видеоплатформа и инструменты |
| `brilliant` | Brilliant learning | Brilliant learning | Обучение Brilliant | Brilliant — обучение |
| `broadcom` | Broadcom and VMware portfolio | Broadcom and VMware portfolio | Продукты Broadcom и VMware | Broadcom и VMware — продукты |
| `bushiroad` | Bushiroad entertainment projects | Bushiroad entertainment projects | Развлекательные проекты Bushiroad | Bushiroad — развлекательные проекты |
| `buymeacoffee` | Buy Me a Coffee creator payments | Buy Me a Coffee creator payments | Платежи авторам Buy Me a Coffee | Buy Me a Coffee — платежи авторам |
| `buypass` | Buypass certificates and identity | Buypass certificates and identity | Сертификаты и идентификация Buypass | Buypass — сертификаты и идентификация |
| `bybit` | Bybit cryptocurrency exchange | Bybit cryptocurrency exchange | Криптовалютная биржа Bybit | Bybit — криптовалютная биржа |
| `bytedance` | ByteDance ecosystem | ByteDance ecosystem | Экосистема ByteDance | ByteDance — экосистема |
| `cabletv` | Cable TV and HOY broadcasting | Cable TV and HOY broadcasting | Телевещание Cable TV и HOY | Cable TV и HOY — телевещание |
| `cainiao` | Cainiao logistics | Cainiao logistics | Логистика Cainiao | Cainiao — логистика |
| `calgoncarbon` | Calgon Carbon and Calgon products | Calgon Carbon and Calgon products | Продукция Calgon Carbon и Calgon | Calgon Carbon и Calgon — продукция |
| `cambridge` | Cambridge education and research | Cambridge education and research | Образование и исследования Cambridge | Cambridge — образование и исследования |
| `canonical` | Canonical ecosystem | Canonical ecosystem | Экосистема Canonical | Canonical — экосистема |
| `capitalonline` | CapitalOnline cloud infrastructure | CapitalOnline cloud infrastructure | Облачная инфраструктура CapitalOnline | CapitalOnline — облачная инфраструктура |
| `carrotquest` | Carrotquest and Dashly customer communication and marketing | Carrotquest and Dashly customer communication and marketing | Общение с клиентами и маркетинг Carrotquest и Dashly | Carrotquest и Dashly — общение с клиентами и маркетинг |
| `casimages` | Casimages file hosting | Casimages file hosting | Хостинг файлов Casimages | Casimages — хостинг файлов |
| `cbs` | CBS and Paramount media collection | CBS and Paramount media collection | Коллекция медиасервисов CBS и Paramount | CBS и Paramount — коллекция медиасервисов |
| `ccb` | China Construction Bank group | China Construction Bank group | Группа China Construction Bank | China Construction Bank — группа |
| `cdek` | CDEK shipping and shopping | CDEK shipping and shopping | Доставка и покупки CDEK | CDEK — доставка и покупки |
| `ceno` | Ceno browser | Ceno browser | Браузер Ceno | Ceno — браузер |
| `cerebras` | Cerebras AI computing platform | Cerebras AI computing platform | Вычислительная платформа ИИ Cerebras | Cerebras — вычислительная платформа ИИ |
| `certinomis` | Certinomis certificates | Certinomis certificates | Сертификаты Certinomis | Certinomis — сертификаты |
| `certum` | Certum certificates | Certum certificates | Сертификаты Certum | Certum — сертификаты |
| `changyou` | Changyou gaming sites | Changyou gaming sites | Игровые сайты Changyou | Changyou — игровые сайты |
| `chaoxing` | Chaoxing learning resources | Chaoxing learning resources | Учебные ресурсы Chaoxing | Chaoxing — учебные ресурсы |
| `chatango` | Chatango live chat | Chatango live chat | Онлайн-чат Chatango | Chatango — онлайн-чат |
| `chatwhores` | ChatWhores adult chat | ChatWhores adult chat | Чат для взрослых ChatWhores | ChatWhores — чат для взрослых |
| `cheetahmobile` | Cheetah Mobile group | Cheetah Mobile group | Группа Cheetah Mobile | Cheetah Mobile — группа |
| `chegg` | Chegg learning | Chegg learning | Обучение Chegg | Chegg — обучение |
| `chinamobile` | China Mobile group | China Mobile group | Группа China Mobile | China Mobile — группа |
| `chinapost` | China Post group | China Post group | Группа China Post | China Post — группа |
| `chinapower` | ChinaPower research project | ChinaPower research project | Исследовательский проект ChinaPower | ChinaPower — исследовательский проект |
| `chinaso` | ChinaSo search and information portal | ChinaSo search and information portal | Поисково-информационный портал ChinaSo | ChinaSo — поисково-информационный портал |
| `chinatower` | China Tower infrastructure | China Tower infrastructure | Инфраструктура China Tower | China Tower — инфраструктура |
| `chinaz` | ChinaZ webmaster resources and tools | ChinaZ webmaster resources and tools | Ресурсы и инструменты для веб-мастеров ChinaZ | ChinaZ — ресурсы и инструменты для веб-мастеров |
| `cisco` | Cisco ecosystem | Cisco ecosystem | Экосистема Cisco | Cisco — экосистема |
| `citic` | CITIC group | CITIC group | Группа CITIC | CITIC — группа |
| `citizenlab` | Citizen Lab research | Citizen Lab research | Исследования Citizen Lab | Citizen Lab — исследования |
| `ciweimao` | Ciweimao fiction | Ciweimao fiction | Художественная литература Ciweimao | Ciweimao — художественная литература |
| `ck101` | CK101 forum | CK101 forum | Форум CK101 | CK101 — форум |
| `clarivate` | Clarivate analytics and information services | Clarivate analytics and information services | Аналитика и информационные сервисы Clarivate | Clarivate — аналитика и информационные сервисы |
| `clearasil` | Clearasil acne care | Clearasil acne care | Средства против акне Clearasil | Clearasil — средства против акне |
| `clearbit` | Clearbit business data services | Clearbit business data services | Бизнес-данные Clearbit | Clearbit — бизнес-данные |
| `cloudcone` | CloudCone cloud hosting | CloudCone cloud hosting | Облачный хостинг CloudCone | CloudCone — облачный хостинг |
| `cloudconvert` | CloudConvert file conversion | CloudConvert file conversion | Конвертация файлов CloudConvert | CloudConvert — конвертация файлов |
| `cloudflare` | Cloudflare infrastructure services | Cloudflare infrastructure services | Инфраструктурные сервисы Cloudflare | Cloudflare — инфраструктурные сервисы |
| `cloudflare-ipfs` | Cloudflare IPFS gateway | Cloudflare IPFS gateway | Шлюз IPFS Cloudflare | Cloudflare — шлюз IPFS |
| `cloudns` | ClouDNS dynamic DNS | ClouDNS dynamic DNS | Динамический DNS ClouDNS | ClouDNS — динамический DNS |
| `clubhouse` | Clubhouse audio community | Clubhouse audio community | Аудиосообщество Clubhouse | Clubhouse — аудиосообщество |
| `cnb` | CNB cloud-native builds | CNB cloud-native builds | Облачная сборка CNB | CNB — облачная сборка |
| `cnblogs` | CNBlogs developer publishing | CNBlogs developer publishing | Публикации для разработчиков CNBlogs | CNBlogs — публикации для разработчиков |
| `cnki` | CNKI scholarly resources | CNKI scholarly resources | Научные ресурсы CNKI | CNKI — научные ресурсы |
| `codeberg` | Codeberg development hosting | Codeberg development hosting | Размещение проектов Codeberg | Codeberg — размещение проектов |
| `coinone` | Coinone cryptocurrency exchange | Coinone cryptocurrency exchange | Криптовалютная биржа Coinone | Coinone — криптовалютная биржа |
| `comfy-ui-launcher` | Comfy UI launcher | Comfy UI launcher | Средство запуска Comfy UI | Comfy UI — средство запуска |
| `comodo` | Comodo security | Comodo security | Безопасность Comodo | Comodo — безопасность |
| `coolapk` | Coolapk community | Coolapk community | Сообщество Coolapk | Coolapk — сообщество |
| `coomer` | Coomer adult archive | Coomer adult archive | Архив для взрослых Coomer | Coomer — архив для взрослых |
| `copymanga` | CopyManga comics | CopyManga comics | Комиксы CopyManga | CopyManga — комиксы |
| `corel` | Corel software | Corel software | Программное обеспечение Corel | Corel — программное обеспечение |
| `coupang` | Coupang shopping | Coupang shopping | Покупки Coupang | Coupang — покупки |
| `coursera` | Coursera online learning | Coursera online learning | Онлайн-обучение Coursera | Coursera — онлайн-обучение |
| `cowlevel` | Cowlevel gaming community | Cowlevel gaming community | Игровое сообщество Cowlevel | Cowlevel — игровое сообщество |
| `cowtransfer` | CowTransfer file transfer | CowTransfer file transfer | Передача файлов CowTransfer | CowTransfer — передача файлов |
| `craigslist` | Craigslist classifieds | Craigslist classifieds | Объявления Craigslist | Craigslist — объявления |
| `creativecommons` | Creative Commons organization | Creative Commons organization | Организация Creative Commons | Creative Commons — организация |
| `csis` | CSIS policy research | CSIS policy research | Исследования государственной политики CSIS | CSIS — исследования государственной политики |
| `ctexcel` | CTExcel mobile connectivity | CTExcel mobile connectivity | Мобильная связь CTExcel | CTExcel — мобильная связь |
| `ctrip` | Ctrip, Trip.com and Qunar travel services | Ctrip, Trip.com and Qunar travel services | Туристические сервисы Ctrip, Trip.com и Qunar | Ctrip, Trip.com и Qunar — туристические сервисы |
| `curseforge` | CurseForge game mods | CurseForge game mods | Модификации игр CurseForge | CurseForge — модификации игр |
| `cuttly` | Cuttly link management | Cuttly link management | Управление ссылками Cuttly | Cuttly — управление ссылками |
| `cybertrust` | Cybertrust Japan infrastructure services | Cybertrust Japan infrastructure services | Инфраструктурные сервисы Cybertrust Japan | Cybertrust Japan — инфраструктурные сервисы |
| `cylink` | CyLink proxy access | CyLink proxy access | Прокси-доступ CyLink | CyLink — прокси-доступ |
| `dandanplay` | DandanPlay media player | DandanPlay media player | Медиаплеер DandanPlay | DandanPlay — медиаплеер |
| `dazn` | DAZN sports video | DAZN sports video | Спортивное видео DAZN | DAZN — спортивное видео |
| `dcard` | Dcard community | Dcard community | Сообщество Dcard | Dcard — сообщество |
| `ddmaicai` | Dingdong grocery delivery | Dingdong grocery delivery | Доставка продуктов Dingdong | Dingdong — доставка продуктов |
| `debian` | Debian operating system | Debian operating system | Операционная система Debian | Debian — операционная система |
| `deepin` | Deepin and UnionTech projects | Deepin and UnionTech projects | Проекты Deepin и UnionTech | Deepin и UnionTech — проекты |
| `deepseek` | DeepSeek AI | DeepSeek AI | ИИ DeepSeek | DeepSeek — ИИ |
| `deppon` | Deppon logistics | Deppon logistics | Логистика Deppon | Deppon — логистика |
| `deribit` | Deribit cryptocurrency exchange | Deribit cryptocurrency exchange | Криптовалютная биржа Deribit | Deribit — криптовалютная биржа |
| `dingtalk` | DingTalk collaboration | DingTalk collaboration | Совместная работа DingTalk | DingTalk — совместная работа |
| `discord` | Discord communication | Discord communication | Общение Discord | Discord — общение |
| `discourse` | Discourse forum software | Discourse forum software | ПО для форумов Discourse | Discourse — ПО для форумов |
| `disney` | Disney corporate portfolio | Disney corporate portfolio | Корпоративный портфель Disney | Disney — корпоративный портфель |
| `disqus` | Disqus website comments | Disqus website comments | Комментарии на сайтах Disqus | Disqus — комментарии на сайтах |
| `divar` | Divar classifieds | Divar classifieds | Объявления Divar | Divar — объявления |
| `dlercloud` | DlerCloud network access | DlerCloud network access | Сетевой доступ DlerCloud | DlerCloud — сетевой доступ |
| `dmit` | DMIT infrastructure services | DMIT infrastructure services | Инфраструктурные сервисы DMIT | DMIT — инфраструктурные сервисы |
| `doi` | DOI identifier infrastructure | DOI identifier infrastructure | Инфраструктура идентификаторов DOI | DOI — инфраструктура идентификаторов |
| `douban` | Douban community and audio | Douban community and audio | Сообщество и аудио Douban | Douban — сообщество и аудио |
| `douyu` | Douyu live streaming | Douyu live streaming | Трансляции Douyu | Douyu — трансляции |
| `dowjones` | Dow Jones publications and business information | Dow Jones publications and business information | Издания и деловая информация Dow Jones | Dow Jones — издания и деловая информация |
| `dslreports` | DSLReports community | DSLReports community | Сообщество DSLReports | DSLReports — сообщество |
| `duolingo` | Duolingo learning | Duolingo learning | Обучение Duolingo | Duolingo — обучение |
| `duyaoss` | DuyaoSS proxy-service reviews and speed tests | DuyaoSS proxy-service reviews and speed tests | Обзоры и тесты скорости прокси-сервисов DuyaoSS | DuyaoSS — обзоры и тесты скорости прокси-сервисов |
| `dyna` | DynaComware fonts and font licensing | DynaComware fonts and font licensing | Шрифты и лицензирование DynaComware | DynaComware — шрифты и лицензирование |
| `dynu` | Dynu dynamic DNS | Dynu dynamic DNS | Динамический DNS Dynu | Dynu — динамический DNS |
| `eastmoney` | Eastmoney investment services and financial information | Eastmoney investment services and financial information | Инвестиционные сервисы и финансовая информация Eastmoney | Eastmoney — инвестиционные сервисы и финансовая информация |
| `easylist` | EasyList ad-blocking and privacy filter project | EasyList ad-blocking and privacy filter project | Проект фильтров блокировки рекламы и защиты приватности EasyList | EasyList — проект фильтров блокировки рекламы и защиты приватности |
| `ebay` | eBay marketplace | eBay marketplace | Маркетплейс eBay | eBay — маркетплейс |
| `economist` | The Economist Group media and business information | The Economist Group media and business information | Медиа и деловая информация The Economist Group | The Economist Group — медиа и деловая информация |
| `edx` | edX online learning | edX online learning | Онлайн-обучение edX | edX — онлайн-обучение |
| `egghead` | egghead developer education | egghead developer education | Обучение разработчиков egghead | egghead — обучение разработчиков |
| `ehentai` | E-Hentai adult comics | E-Hentai adult comics | Комиксы для взрослых E-Hentai | E-Hentai — комиксы для взрослых |
| `electron` | Electron application framework | Electron application framework | Фреймворк приложений Electron | Electron — фреймворк приложений |
| `eleme` | Ele.me / Taobao Instant Commerce delivery | Ele.me / Taobao Instant Commerce delivery | Доставка Ele.me / Taobao Instant Commerce | Ele.me / Taobao Instant Commerce — доставка |
| `elevenlabs` | ElevenLabs AI audio tools | ElevenLabs AI audio tools | Звуковые инструменты ИИ ElevenLabs | ElevenLabs — звуковые инструменты ИИ |
| `elsevier` | Elsevier scholarly publications | Elsevier scholarly publications | Научные издания Elsevier | Elsevier — научные издания |
| `embl` | EMBL molecular biology research and data resources | EMBL molecular biology research and data resources | Исследования молекулярной биологии и научные данные EMBL | EMBL — исследования молекулярной биологии и научные данные |
| `eneba` | Eneba marketplace | Eneba marketplace | Маркетплейс Eneba | Eneba — маркетплейс |
| `enfa` | Enfa maternal and child nutrition brands | Enfa maternal and child nutrition brands | Бренды питания для матерей и детей Enfa | Enfa — бренды питания для матерей и детей |
| `entermediadb` | EnterMedia digital media and knowledge management | EnterMedia digital media and knowledge management | Управление цифровыми медиа и знаниями EnterMedia | EnterMedia — управление цифровыми медиа и знаниями |
| `entrust` | Entrust, AffirmTrust, Trend Micro and Atos domains | Entrust, AffirmTrust, Trend Micro and Atos domains | Домены Entrust, AffirmTrust, Trend Micro и Atos | Entrust, AffirmTrust, Trend Micro и Atos — домены |
| `envybox` | Envybox sales widgets and CRM tools | Envybox sales widgets and CRM tools | Виджеты продаж и CRM-инструменты Envybox | Envybox — виджеты продаж и CRM-инструменты |
| `epochmediagroup` | Epoch media and shopping properties | Epoch media and shopping properties | Медиа и магазины Epoch | Epoch — медиа и магазины |
| `escapefromtarkov` | Escape from Tarkov game | Escape from Tarkov game | Игра Escape from Tarkov | Escape from Tarkov — игра |
| `eset` | ESET security | ESET security | Безопасность ESET | ESET — безопасность |
| `evernote` | Evernote notes | Evernote notes | Заметки Evernote | Evernote — заметки |
| `f-droid` | F-Droid app distribution | F-Droid app distribution | Распространение приложений F-Droid | F-Droid — распространение приложений |
| `faceit` | FACEIT gaming | FACEIT gaming | Игровой сервис FACEIT | FACEIT — игровой сервис |
| `familymart` | FamilyMart retail | FamilyMart retail | Магазины FamilyMart | FamilyMart — магазины |
| `farfetch` | Farfetch shopping | Farfetch shopping | Покупки Farfetch | Farfetch — покупки |
| `fastlane` | fastlane development automation | fastlane development automation | Автоматизация разработки fastlane | fastlane — автоматизация разработки |
| `fcbox` | Hive Box parcel lockers | Hive Box parcel lockers | Постаматы Hive Box | Hive Box — постаматы |
| `fedora` | Fedora operating system | Fedora operating system | Операционная система Fedora | Fedora — операционная система |
| `feishu` | Feishu collaboration | Feishu collaboration | Совместная работа Feishu | Feishu — совместная работа |
| `fflogs` | FF Logs game analysis | FF Logs game analysis | Аналитика игр FF Logs | FF Logs — аналитика игр |
| `fibank` | First Investment Bank financial services | First Investment Bank financial services | Финансовые услуги First Investment Bank | First Investment Bank — финансовые услуги |
| `ficbook` | Ficbook fiction | Ficbook fiction | Художественная литература Ficbook | Ficbook — художественная литература |
| `firefox` | Firefox browser | Firefox browser | Браузер Firefox | Firefox — браузер |
| `flatpak` | Flatpak and Flathub app distribution | Flatpak and Flathub app distribution | Распространение приложений Flatpak и Flathub | Flatpak и Flathub — распространение приложений |
| `flickr` | Flickr photo community | Flickr photo community | Фотосообщество Flickr | Flickr — фотосообщество |
| `flowwow` | Flowwow marketplace | Flowwow marketplace | Маркетплейс Flowwow | Flowwow — маркетплейс |
| `fluxer` | Fluxer communication | Fluxer communication | Общение Fluxer | Fluxer — общение |
| `fontawesome` | Font Awesome design assets | Font Awesome design assets | Ресурсы дизайна Font Awesome | Font Awesome — ресурсы дизайна |
| `forbes` | Forbes publications | Forbes publications | Издания Forbes | Forbes — издания |
| `forza` | Forza racing games | Forza racing games | Гоночные игры Forza | Forza — гоночные игры |
| `fqnovel` | Fanqie fiction | Fanqie fiction | Художественная литература Fanqie | Fanqie — художественная литература |
| `framer` | Framer website creation | Framer website creation | Создание сайтов Framer | Framer — создание сайтов |
| `funpay` | FunPay marketplace | FunPay marketplace | Маркетплейс FunPay | FunPay — маркетплейс |
| `futu` | Futu and moomoo brokerage | Futu and moomoo brokerage | Брокерские сервисы Futu и moomoo | Futu и moomoo — брокерские сервисы |
| `gamersky` | Gamersky gaming media | Gamersky gaming media | Игровые медиа Gamersky | Gamersky — игровые медиа |
| `gannett` | Gannett and USA TODAY media | Gannett and USA TODAY media | Медиа Gannett и USA TODAY | Gannett и USA TODAY — медиа |
| `gateio` | Gate cryptocurrency exchange | Gate cryptocurrency exchange | Криптовалютная биржа Gate | Gate — криптовалютная биржа |
| `gemfury` | Gemfury developer packages | Gemfury developer packages | Пакеты для разработчиков Gemfury | Gemfury — пакеты для разработчиков |
| `gfw` | GFWList domains | GFWList domains | Домены GFWList | GFWList — домены |
| `ggsel` | GGsel marketplace | GGsel marketplace | Маркетплейс GGsel | GGsel — маркетплейс |
| `gitbook` | GitBook documentation | GitBook documentation | Документация GitBook | GitBook — документация |
| `gitee` | Gitee code hosting | Gitee code hosting | Хостинг кода Gitee | Gitee — хостинг кода |
| `gitv` | GITV television | GITV television | Телевидение GITV | GITV — телевидение |
| `globalvoices` | Global Voices media | Global Voices media | Медиа Global Voices | Global Voices — медиа |
| `gmo-internet` | GMO Internet Group portfolio | GMO Internet Group portfolio | Портфель GMO Internet Group | GMO Internet Group — портфель |
| `goodreads` | Goodreads books community | Goodreads books community | Книжное сообщество Goodreads | Goodreads — книжное сообщество |
| `google-gemini` | Google AI tools compatibility alias | Google AI tools compatibility alias | Совместимый псевдоним инструментов ИИ Google | Google — совместимый псевдоним инструментов ИИ |
| `google-registry` | Google Registry | Google Registry | Реестр доменов Google | Google — реестр доменов |
| `google-registry-tld` | Google Registry top-level domains | Google Registry top-level domains | Доменные зоны Google Registry | Google Registry — доменные зоны |
| `google-trust-services` | Google Trust Services certificates | Google Trust Services certificates | Сертификаты Google Trust Services | Google Trust Services — сертификаты |
| `goproxy` | Go module proxies | Go module proxies | Прокси модулей Go | Go — прокси модулей |
| `growingio` | GrowingIO analytics | GrowingIO analytics | Аналитика GrowingIO | GrowingIO — аналитика |
| `haier` | Haier group businesses | Haier group businesses | Бизнесы группы Haier | Haier — бизнесы группы |
| `haitang` | Haitang online literature | Haitang online literature | Онлайн-литература Haitang | Haitang — онлайн-литература |
| `hanyi` | Hanyi font tools | Hanyi font tools | Шрифтовые инструменты Hanyi | Hanyi — шрифтовые инструменты |
| `haskell` | Haskell language ecosystem | Haskell language ecosystem | Экосистема языка Haskell | Haskell — экосистема языка |
| `hdrezka` | HDrezka video service | HDrezka video service | Видеосервис HDrezka | HDrezka — видеосервис |
| `hetzner` | Hetzner hosting | Hetzner hosting | Хостинг Hetzner | Hetzner — хостинг |
| `hikvision` | Hikvision and EZVIZ products and businesses | Hikvision and EZVIZ products and businesses | Продукты и бизнесы Hikvision и EZVIZ | Hikvision и EZVIZ — продукты и бизнесы |
| `hinet-eca` | Chunghwa Telecom certificates | Chunghwa Telecom certificates | Сертификаты Chunghwa Telecom | Chunghwa Telecom — сертификаты |
| `hitun` | Hitun VPN | Hitun VPN | VPN Hitun | Hitun — VPN |
| `hketgroup` | Hong Kong Economic Times media and business services | Hong Kong Economic Times media and business services | Медиа и деловые сервисы Hong Kong Economic Times | Hong Kong Economic Times — медиа и деловые сервисы |
| `hkgolden` | HKGolden forum | HKGolden forum | Форум HKGolden | HKGolden — форум |
| `hkt` | HKT technology and service portfolio | HKT technology and service portfolio | Технологии и сервисы группы HKT | HKT — технологии и сервисы группы |
| `homebrew` | Homebrew package manager | Homebrew package manager | Менеджер пакетов Homebrew | Homebrew — менеджер пакетов |
| `hongkongpost` | Hongkong Post digital certificates | Hongkong Post digital certificates | Цифровые сертификаты Hongkong Post | Hongkong Post — цифровые сертификаты |
| `hp` | HP company domains | HP company domains | Домены компании HP | HP — домены компании |
| `hpe` | Hewlett Packard Enterprise company domains | Hewlett Packard Enterprise company domains | Домены компании Hewlett Packard Enterprise | Hewlett Packard Enterprise — домены компании |
| `hsbc` | HSBC banking | HSBC banking | Банковские услуги HSBC | HSBC — банковские услуги |
| `huawei-dev` | Huawei development projects | Huawei development projects | Проекты разработки Huawei | Huawei — проекты разработки |
| `hugecore` | Hugecore and MOJi learning tools | Hugecore and MOJi learning tools | Учебные инструменты Hugecore и MOJi | Hugecore и MOJi — учебные инструменты |
| `hujiang` | Hujiang learning services | Hujiang learning services | Учебные сервисы Hujiang | Hujiang — учебные сервисы |
| `huobi` | Huobi and HTX exchange | Huobi and HTX exchange | Биржа Huobi и HTX | Huobi и HTX — биржа |
| `ibm` | IBM organizational domain collection | IBM organizational domain collection | Подборка доменов организации IBM | IBM — подборка доменов организации |
| `icbc` | ICBC banking | ICBC banking | Банковские услуги ICBC | ICBC — банковские услуги |
| `identrust` | IdenTrust certificates | IdenTrust certificates | Сертификаты IdenTrust | IdenTrust — сертификаты |
| `idg` | IDG technology media, research and marketing | IDG technology media, research and marketing | Технологические медиа, исследования и маркетинг IDG | IDG — технологические медиа, исследования и маркетинг |
| `ifanr` | iFanr media and cloud products | iFanr media and cloud products | Медиа и облачные продукты iFanr | iFanr — медиа и облачные продукты |
| `ifast` | iFAST investment and banking platforms | iFAST investment and banking platforms | Инвестиционные и банковские платформы iFAST | iFAST — инвестиционные и банковские платформы |
| `ihuman` | Hongen education | Hongen education | Образовательные сервисы Hongen | Hongen — образовательные сервисы |
| `imgix` | imgix image delivery | imgix image delivery | Доставка изображений imgix | imgix — доставка изображений |
| `infowars` | Infowars media | Infowars media | Медиа Infowars | Infowars — медиа |
| `insider` | Insider publications | Insider publications | Издания Insider | Insider — издания |
| `intel` | Intel corporate portfolio | Intel corporate portfolio | Компании и продукты Intel | Intel — компании и продукты |
| `intel-dev` | Intel development projects | Intel development projects | Проекты разработки Intel | Intel — проекты разработки |
| `internet-archive` | Internet Archive libraries and archives | Internet Archive libraries and archives | Библиотеки и архивы Internet Archive | Internet Archive — библиотеки и архивы |
| `intsig` | Intsig products and services | Intsig products and services | Продукты и сервисы Intsig | Intsig — продукты и сервисы |
| `intuit` | Intuit products and services | Intuit products and services | Продукты и сервисы Intuit | Intuit — продукты и сервисы |
| `ipip` | IPIP IP lookup | IPIP IP lookup | Определение IP IPIP | IPIP — определение IP |
| `ixbt` | iXBT technology media | iXBT technology media | Технологические медиа iXBT | iXBT — технологические медиа |
| `jd` | JD group retail, logistics and technology businesses | JD group retail, logistics and technology businesses | Торговля, логистика и технологии группы JD | JD — торговля, логистика и технологии группы |
| `jiemian` | Jiemian and Cailian media | Jiemian and Cailian media | Медиа Jiemian и Cailian | Jiemian и Cailian — медиа |
| `jiyukobo` | Jiyukobo typefaces | Jiyukobo typefaces | Шрифты Jiyukobo | Jiyukobo — шрифты |
| `jquery` | jQuery libraries | jQuery libraries | Библиотеки jQuery | jQuery — библиотеки |
| `jsdelivr` | jsDelivr CDN | jsDelivr CDN | CDN jsDelivr | jsDelivr — CDN |
| `jtexpress` | J&T Express logistics | J&T Express logistics | Логистика J&T Express | J&T Express — логистика |
| `juejin` | Juejin developer community | Juejin developer community | Сообщество разработчиков Juejin | Juejin — сообщество разработчиков |
| `justmysocks` | Just My Socks proxy | Just My Socks proxy | Прокси Just My Socks | Just My Socks — прокси |
| `jwplayer` | JW Player video platform | JW Player video platform | Видеоплатформа JW Player | JW Player — видеоплатформа |
| `kakao` | Kakao corporate portfolio | Kakao corporate portfolio | Компании и сервисы Kakao | Kakao — компании и сервисы |
| `kernel` | Linux kernel resources | Linux kernel resources | Ресурсы ядра Linux | Linux — ресурсы ядра |
| `keybase` | Keybase communication | Keybase communication | Общение Keybase | Keybase — общение |
| `kick` | Kick streaming | Kick streaming | Трансляции Kick | Kick — трансляции |
| `kingsoft` | Kingsoft corporate portfolio | Kingsoft corporate portfolio | Компании и продукты Kingsoft | Kingsoft — компании и продукты |
| `kodi` | Kodi media center | Kodi media center | Медиацентр Kodi | Kodi — медиацентр |
| `konachan` | Konachan anime wallpapers | Konachan anime wallpapers | Аниме-обои Konachan | Konachan — аниме-обои |
| `koolearn` | Koolearn education | Koolearn education | Образовательные сервисы Koolearn | Koolearn — образовательные сервисы |
| `kraken` | Kraken exchange | Kraken exchange | Биржа Kraken | Kraken — биржа |
| `kuaidi100` | Kuaidi100 logistics | Kuaidi100 logistics | Логистика Kuaidi100 | Kuaidi100 — логистика |
| `kuaikan` | Kuaikan comics | Kuaikan comics | Комиксы Kuaikan | Kuaikan — комиксы |
| `kuaishou` | Kuaishou product portfolio | Kuaishou product portfolio | Продукты Kuaishou | Kuaishou — продукты |
| `kucoin` | KuCoin exchange | KuCoin exchange | Биржа KuCoin | KuCoin — биржа |
| `kugou` | Kugou music services | Kugou music services | Музыкальные сервисы Kugou | Kugou — музыкальные сервисы |
| `kyodonews` | Kyodo news publications | Kyodo news publications | Новостные издания Kyodo | Kyodo — новостные издания |
| `landian` | Landian technology news | Landian technology news | Технологические новости Landian | Landian — технологические новости |
| `lantern` | Lantern access | Lantern access | Доступ Lantern | Lantern — доступ |
| `lanzou` | Lanzou file storage | Lanzou file storage | Файловое хранилище Lanzou | Lanzou — файловое хранилище |
| `laracasts` | Laracasts programming courses | Laracasts programming courses | Курсы программирования Laracasts | Laracasts — курсы программирования |
| `lastpass` | LastPass passwords | LastPass passwords | Пароли LastPass | LastPass — пароли |
| `launchpad` | Launchpad software collaboration | Launchpad software collaboration | Совместная разработка Launchpad | Launchpad — совместная разработка |
| `le` | LeEco ecosystem | LeEco ecosystem | Экосистема LeEco | LeEco — экосистема |
| `lenovo` | Lenovo companies and products | Lenovo companies and products | Компании и продукты Lenovo | Lenovo — компании и продукты |
| `letsencrypt` | Let's Encrypt certificates | Let's Encrypt certificates | Сертификаты Let's Encrypt | Let's Encrypt — сертификаты |
| `lg` | LG Electronics portfolio | LG Electronics portfolio | Продукты и сервисы LG Electronics | LG Electronics — продукты и сервисы |
| `lianjia` | Lianjia and Beike housing services | Lianjia and Beike housing services | Жилищные услуги Lianjia и Beike | Lianjia и Beike — жилищные услуги |
| `liberapay` | Liberapay recurring donations | Liberapay recurring donations | Регулярные пожертвования Liberapay | Liberapay — регулярные пожертвования |
| `librewolf` | LibreWolf browser | LibreWolf browser | Браузер LibreWolf | LibreWolf — браузер |
| `lighter` | Lighter exchange | Lighter exchange | Биржа Lighter | Lighter — биржа |
| `lihkg` | LIHKG forum | LIHKG forum | Форум LIHKG | LIHKG — форум |
| `limelight` | Limelight CDN | Limelight CDN | CDN Limelight | Limelight — CDN |
| `line` | LINE products and services | LINE products and services | Продукты и сервисы LINE | LINE — продукты и сервисы |
| `linkedin` | LinkedIn professional network | LinkedIn professional network | Профессиональная сеть LinkedIn | LinkedIn — профессиональная сеть |
| `linuxdo` | LINUX DO forum | LINUX DO forum | Форум LINUX DO | LINUX DO — форум |
| `litv` | LiTV television | LiTV television | Телевидение LiTV | LiTV — телевидение |
| `livejournal` | LiveJournal community | LiveJournal community | Сообщество LiveJournal | LiveJournal — сообщество |
| `liveperson` | LivePerson conversational AI | LivePerson conversational AI | Разговорный ИИ LivePerson | LivePerson — разговорный ИИ |
| `lizhi` | Lizhi audio | Lizhi audio | Аудио Lizhi | Lizhi — аудио |
| `localbitcoins` | LocalBitcoins exchange | LocalBitcoins exchange | Биржа LocalBitcoins | LocalBitcoins — биржа |
| `localizejs` | Localize website localization | Localize website localization | Локализация сайтов Localize | Localize — локализация сайтов |
| `logitech` | Logitech brands and products | Logitech brands and products | Бренды и продукты Logitech | Logitech — бренды и продукты |
| `longbridge` | Longbridge brokerage | Longbridge brokerage | Брокерские услуги Longbridge | Longbridge — брокерские услуги |
| `mailcom` | mail.com email | mail.com email | Почта mail.com | mail.com — почта |
| `mainichi` | Mainichi publications | Mainichi publications | Издания Mainichi | Mainichi — издания |
| `manhuagui` | Manhuagui comics | Manhuagui comics | Комиксы Manhuagui | Manhuagui — комиксы |
| `manhuaren` | Manhuaren and DM5 comics | Manhuaren and DM5 comics | Комиксы Manhuaren и DM5 | Manhuaren и DM5 — комиксы |
| `manorama` | Manorama publications and radio | Manorama publications and radio | Издания и радио Manorama | Manorama — издания и радио |
| `manoto` | Manoto television | Manoto television | Телевидение Manoto | Manoto — телевидение |
| `matrix` | Matrix communication ecosystem | Matrix communication ecosystem | Экосистема общения Matrix | Matrix — экосистема общения |
| `matters` | Matters publishing | Matters publishing | Публикации Matters | Matters — публикации |
| `mcdonalds` | McDonald's restaurants | McDonald's restaurants | Рестораны McDonald's | McDonald's — рестораны |
| `mdn` | MDN developer documentation | MDN developer documentation | Документация разработчика MDN | MDN — документация разработчика |
| `mediachinesegroup` | Media Chinese publications | Media Chinese publications | Издания Media Chinese | Media Chinese — издания |
| `medium` | Medium publishing | Medium publishing | Публикации Medium | Medium — публикации |
| `meipian` | Meipian social publishing | Meipian social publishing | Социальные публикации Meipian | Meipian — социальные публикации |
| `meitu` | Meitu products and services | Meitu products and services | Продукты и сервисы Meitu | Meitu — продукты и сервисы |
| `meituan` | Meituan consumer services | Meituan consumer services | Потребительские сервисы Meituan | Meituan — потребительские сервисы |
| `messenger` | Messenger chat | Messenger chat | Чат Messenger | Messenger — чат |
| `metacritic` | Metacritic reviews | Metacritic reviews | Обзоры Metacritic | Metacritic — обзоры |
| `metart` | MetArt adult brands | MetArt adult brands | Бренды для взрослых MetArt | MetArt — бренды для взрослых |
| `miaomiaozhe` | Miaomiaozhe shopping price comparison | Miaomiaozhe shopping price comparison | Сравнение цен и покупки Miaomiaozhe | Miaomiaozhe — сравнение цен и покупки |
| `microsoft-pki` | Microsoft certificate infrastructure | Microsoft certificate infrastructure | Инфраструктура сертификатов Microsoft | Microsoft — инфраструктура сертификатов |
| `midea` | Midea appliance and technology portfolio | Midea appliance and technology portfolio | Бытовая техника и технологические направления Midea | Midea — бытовая техника и технологические направления |
| `mihoyo` | miHoYo regional game groups | miHoYo regional game groups | Региональные группы игр miHoYo | miHoYo — региональные группы игр |
| `mikrotik` | MikroTik networking | MikroTik networking | Сетевые решения MikroTik | MikroTik — сетевые решения |
| `mindgeek-porn` | MindGeek adult brands | MindGeek adult brands | Бренды для взрослых MindGeek | MindGeek — бренды для взрослых |
| `misskey` | Misskey social instances | Misskey social instances | Социальные серверы Misskey | Misskey — социальные серверы |
| `misskey-universe` | Misskey community instances | Misskey community instances | Серверы сообщества Misskey | Misskey — серверы сообщества |
| `mobile01` | Mobile01 forum | Mobile01 forum | Форум Mobile01 | Mobile01 — форум |
| `mocha` | Mocha JavaScript testing | Mocha JavaScript testing | Тестирование JavaScript Mocha | Mocha — тестирование JavaScript |
| `modrinth` | Modrinth game mods | Modrinth game mods | Игровые модификации Modrinth | Modrinth — игровые модификации |
| `mogujie` | Mogujie fashion shopping | Mogujie fashion shopping | Покупки модных товаров Mogujie | Mogujie — покупки модных товаров |
| `monotype` | Monotype font tools and libraries | Monotype font tools and libraries | Шрифтовые инструменты и библиотеки Monotype | Monotype — шрифтовые инструменты и библиотеки |
| `moonvy` | Moonvy design management | Moonvy design management | Управление дизайном Moonvy | Moonvy — управление дизайном |
| `morisawa` | Morisawa fonts | Morisawa fonts | Шрифты Morisawa | Morisawa — шрифты |
| `motorola` | Motorola companies and services | Motorola companies and services | Компании и сервисы Motorola | Motorola — компании и сервисы |
| `mozilla` | Mozilla web products and projects | Mozilla web products and projects | Продукты и проекты Mozilla | Mozilla — продукты и проекты |
| `mxroute` | MXroute email hosting | MXroute email hosting | Электронная почта MXroute | MXroute — электронная почта |
| `mzed` | MZed filmmaking courses | MZed filmmaking courses | Курсы кинопроизводства MZed | MZed — курсы кинопроизводства |
| `n26` | N26 banking | N26 banking | Банковские услуги N26 | N26 — банковские услуги |
| `n3ro` | N3RO VPN | N3RO VPN | VPN N3RO | N3RO — VPN |
| `nationalgeographic` | National Geographic publications and maps | National Geographic publications and maps | Публикации и карты National Geographic | National Geographic — публикации и карты |
| `naver` | Naver companies and services | Naver companies and services | Компании и сервисы Naver | Naver — компании и сервисы |
| `nbcuniversal` | NBCUniversal brands and destinations | NBCUniversal brands and destinations | Бренды и парки NBCUniversal | NBCUniversal — бренды и парки |
| `netcraze` | Netcraze networking | Netcraze networking | Сетевые решения Netcraze | Netcraze — сетевые решения |
| `netcup` | netcup hosting | netcup hosting | Хостинг netcup | netcup — хостинг |
| `netlify` | Netlify hosting | Netlify hosting | Хостинг Netlify | Netlify — хостинг |
| `newscorp` | News Corp companies and services | News Corp companies and services | Компании и сервисы News Corp | News Corp — компании и сервисы |
| `newsmax` | Newsmax media | Newsmax media | Медиа Newsmax | Newsmax — медиа |
| `nexitally` | Nexitally VPN | Nexitally VPN | VPN Nexitally | Nexitally — VPN |
| `nexo` | Nexo crypto finance | Nexo crypto finance | Криптофинансы Nexo | Nexo — криптофинансы |
| `nga` | NGA and 178 gaming communities and media | NGA and 178 gaming communities and media | Игровые сообщества и медиа NGA и 178 | NGA и 178 — игровые сообщества и медиа |
| `nginx` | NGINX server software | NGINX server software | Серверное ПО NGINX | NGINX — серверное ПО |
| `ngrok` | ngrok tunnels | ngrok tunnels | Туннели ngrok | ngrok — туннели |
| `nicegram` | Nicegram messaging | Nicegram messaging | Общение Nicegram | Nicegram — общение |
| `niconico` | Niconico entertainment | Niconico entertainment | Развлечения Niconico | Niconico — развлечения |
| `nikke` | NIKKE game | NIKKE game | Игра NIKKE | NIKKE — игра |
| `nikkei` | Nikkei publications | Nikkei publications | Издания Nikkei | Nikkei — издания |
| `nodejs` | Node.js runtime | Node.js runtime | Среда выполнения Node.js | Node.js — среда выполнения |
| `nodeseek` | NodeSeek forum | NodeSeek forum | Форум NodeSeek | NodeSeek — форум |
| `noip` | No-IP dynamic DNS | No-IP dynamic DNS | Динамический DNS No-IP | No-IP — динамический DNS |
| `npmjs` | npm packages | npm packages | Пакеты npm | npm — пакеты |
| `nvidia` | NVIDIA and Hugging Face products | NVIDIA and Hugging Face products | Продукты NVIDIA и Hugging Face | NVIDIA и Hugging Face — продукты |
| `oan` | OAN and AWE television brands | OAN and AWE television brands | Телевизионные бренды OAN и AWE | OAN и AWE — телевизионные бренды |
| `ogury` | Ogury advertising | Ogury advertising | Реклама Ogury | Ogury — реклама |
| `okjike` | Jike social network | Jike social network | Социальная сеть Jike | Jike — социальная сеть |
| `okx` | OKX crypto services | OKX crypto services | Криптосервисы OKX | OKX — криптосервисы |
| `onedrive` | OneDrive storage | OneDrive storage | Хранилище OneDrive | OneDrive — хранилище |
| `onekey` | OneKey crypto wallet | OneKey crypto wallet | Криптокошелёк OneKey | OneKey — криптокошелёк |
| `op` | OP.GG gaming | OP.GG gaming | Игровой сервис OP.GG | OP.GG — игровой сервис |
| `openjsfoundation` | OpenJS development projects | OpenJS development projects | Проекты разработки OpenJS | OpenJS — проекты разработки |
| `openrec` | OPENREC streaming | OPENREC streaming | Трансляции OPENREC | OPENREC — трансляции |
| `openx` | OpenX advertising | OpenX advertising | Реклама OpenX | OpenX — реклама |
| `osu` | osu! game | osu! game | Игра osu! | osu! — игра |
| `otpbank` | OTP banking services | OTP banking services | Банковские услуги OTP | OTP — банковские услуги |
| `oup` | Oxford University Press resources | Oxford University Press resources | Ресурсы Oxford University Press | Oxford University Press — ресурсы |
| `overclockers-ru` | Overclockers.ru media | Overclockers.ru media | Медиа Overclockers.ru | Overclockers.ru — медиа |
| `paofuyun` | Paofuyun VPN | Paofuyun VPN | VPN Paofuyun | Paofuyun — VPN |
| `paskoocheh` | Paskoocheh app distribution | Paskoocheh app distribution | Распространение приложений Paskoocheh | Paskoocheh — распространение приложений |
| `pbs` | PBS television | PBS television | Телевидение PBS | PBS — телевидение |
| `pearson` | Pearson learning and assessment resources | Pearson learning and assessment resources | Обучение и оценка Pearson | Pearson — обучение и оценка |
| `perl` | Perl language ecosystem | Perl language ecosystem | Экосистема языка Perl | Perl — экосистема языка |
| `petrochina` | CNPC and PetroChina resources | CNPC and PetroChina resources | Ресурсы CNPC и PetroChina | CNPC и PetroChina — ресурсы |
| `phoenix` | Phoenix media and education | Phoenix media and education | Медиа и образование Phoenix | Phoenix — медиа и образование |
| `picacg` | PicACG comics | PicACG comics | Комиксы PicACG | PicACG — комиксы |
| `pikpak` | PikPak storage | PikPak storage | Хранилище PikPak | PikPak — хранилище |
| `pingan` | Ping An group | Ping An group | Группа Ping An | Ping An — группа |
| `pingcap` | PingCAP database projects | PingCAP database projects | Проекты баз данных PingCAP | PingCAP — проекты баз данных |
| `pinggy` | Pinggy tunnels | Pinggy tunnels | Туннели Pinggy | Pinggy — туннели |
| `pingpe` | ping.pe network diagnostics | ping.pe network diagnostics | Диагностика сети ping.pe | ping.pe — диагностика сети |
| `pingsx` | Ping.Sx network diagnostics | Ping.Sx network diagnostics | Диагностика сети Ping.Sx | Ping.Sx — диагностика сети |
| `pixhost` | Pixhost image hosting | Pixhost image hosting | Хостинг изображений Pixhost | Pixhost — хостинг изображений |
| `polyv` | POLYV enterprise video platform | POLYV enterprise video platform | Корпоративная видеоплатформа POLYV | POLYV — корпоративная видеоплатформа |
| `progress` | Progress Software product portfolio | Progress Software product portfolio | Портфель продуктов Progress Software | Progress Software — портфель продуктов |
| `raiffeisenbank` | Raiffeisen banking services | Raiffeisen banking services | Банковские услуги Raiffeisen | Raiffeisen — банковские услуги |
| `rakuten` | Rakuten service portfolio | Rakuten service portfolio | Портфель сервисов Rakuten | Rakuten — портфель сервисов |
| `rarbg` | RARBG torrent access | RARBG torrent access | Торренты RARBG | RARBG — торренты |
| `razer` | Razer ecosystem | Razer ecosystem | Экосистема Razer | Razer — экосистема |
| `reabble` | Reabble RSS reader | Reabble RSS reader | RSS-ридер Reabble | Reabble — RSS-ридер |
| `reagroup` | REA Group property platforms | REA Group property platforms | Платформы недвижимости REA Group | REA Group — платформы недвижимости |
| `realtype` | REALTYPE web fonts | REALTYPE web fonts | Веб-шрифты REALTYPE | REALTYPE — веб-шрифты |
| `rebrandly` | Rebrandly link management | Rebrandly link management | Управление ссылками Rebrandly | Rebrandly — управление ссылками |
| `regru` | REG.RU domains and hosting | REG.RU domains and hosting | Домены и хостинг REG.RU | REG.RU — домены и хостинг |
| `remnawave` | Remnawave proxy management | Remnawave proxy management | Управление прокси Remnawave | Remnawave — управление прокси |
| `roku` | Roku entertainment services | Roku entertainment services | Развлекательные сервисы Roku | Roku — развлекательные сервисы |
| `rt` | RT media and related projects | RT media and related projects | Медиа и связанные проекты RT | RT — медиа и связанные проекты |
| `rthk` | RTHK broadcasting | RTHK broadcasting | Вещание RTHK | RTHK — вещание |
| `ruanmei` | Ruanmei software and media portfolio | Ruanmei software and media portfolio | Программные и медиасервисы Ruanmei | Ruanmei — программные и медиасервисы |
| `rumble` | Rumble video platform | Rumble video platform | Видеоплатформа Rumble | Rumble — видеоплатформа |
| `samsung` | Samsung ecosystem | Samsung ecosystem | Экосистема Samsung | Samsung — экосистема |
| `sankei` | Sankei publications | Sankei publications | Издания Sankei | Sankei — издания |
| `sb` | SB network utilities | SB network utilities | Сетевые сервисы SB | SB — сетевые сервисы |
| `scaleflex` | Scaleflex media asset tools | Scaleflex media asset tools | Работа с медиаматериалами Scaleflex | Scaleflex — работа с медиаматериалами |
| `schoopia` | Schoopia campus learning | Schoopia campus learning | Учебная платформа Schoopia | Schoopia — учебная платформа |
| `schwab` | Charles Schwab investing | Charles Schwab investing | Инвестиции Charles Schwab | Charles Schwab — инвестиции |
| `snap` | Snap product portfolio | Snap product portfolio | Продукты Snap | Snap — продукты |
| `sogou` | Sogou product portfolio | Sogou product portfolio | Продукты Sogou | Sogou — продукты |
| `spiceworks` | Spiceworks IT community | Spiceworks IT community | ИТ-сообщество Spiceworks | Spiceworks — ИТ-сообщество |
| `suning` | Suning corporate portfolio | Suning corporate portfolio | Корпоративные сервисы Suning | Suning — корпоративные сервисы |
| `tex` | TeX ecosystem | TeX ecosystem | Экосистема TeX | TeX — экосистема |
| `tld-opennic` | OpenNIC top-level domains | OpenNIC top-level domains | Альтернативные доменные зоны OpenNIC | OpenNIC — альтернативные доменные зоны |
| `tmtpost` | TMTPost media, learning and data portfolio | TMTPost media, learning and data portfolio | Медиа, обучение и данные TMTPost | TMTPost — медиа, обучение и данные |
| `uc` | UC browser and news | UC browser and news | Браузер и новости UC | UC — браузер и новости |
| `win-extra` | WindowsSpyBlocker extra rules | WindowsSpyBlocker extra rules | Дополнительные правила WindowsSpyBlocker | WindowsSpyBlocker — дополнительные правила |
| `win-spy` | Windows tracking domains | Windows tracking domains | Домены отслеживания Windows | Windows — домены отслеживания |
| `win-update` | Windows update domains | Windows update domains | Домены обновлений Windows | Windows — домены обновлений |
| `xiaoheihe` | HeyBox gaming community and tools | HeyBox gaming community and tools | Игровое сообщество и инструменты HeyBox | HeyBox — игровое сообщество и инструменты |
| `xiaomi` | Xiaomi ecosystem | Xiaomi ecosystem | Экосистема Xiaomi | Xiaomi — экосистема |
| `xingrz` | XiNGRZ personal technology blog | XiNGRZ personal technology blog | Личный технологический блог XiNGRZ | XiNGRZ — личный технологический блог |
| `yahoo` | Yahoo historical brand portfolio | Yahoo historical brand portfolio | Исторический портфель Yahoo | Yahoo — исторический портфель |
| `yuewen` | Yuewen reading and creative products | Yuewen reading and creative products | Чтение и творческие продукты Yuewen | Yuewen — чтение и творческие продукты |
| `zb` | ZB cryptocurrency exchange | ZB cryptocurrency exchange | Криптовалютная биржа ZB | ZB — криптовалютная биржа |
| `amp` | AMP web framework and delivery | AMP web framework and delivery | Веб-фреймворк AMP и доставка ресурсов | AMP — веб-фреймворк и доставка ресурсов |
| `apple-dev` | Apple developer projects | Apple developer projects | Проекты Apple для разработчиков | Apple — проекты для разработчиков |
| `canon` | Canon corporate and product portfolio | Canon corporate and product portfolio | Компания Canon и её продукты | Canon — компания и её продукты |
| `csdn` | CSDN developer resources | CSDN developer resources | Ресурсы CSDN для разработчиков | CSDN — ресурсы для разработчиков |
| `dmm` | DMM service portfolio and supporting domains | DMM service portfolio and supporting domains | Портфель сервисов DMM и вспомогательные домены | DMM — портфель сервисов и вспомогательные домены |
| `dropbox` | Dropbox product and supporting domains | Dropbox product and supporting domains | Продукты Dropbox и вспомогательные домены | Dropbox — продукты и вспомогательные домены |
| `embedly` | Embedly developer media tools | Embedly developer media tools | Медиаинструменты Embedly для разработчиков | Embedly — медиаинструменты для разработчиков |
| `emojipedia` | Emojipedia reference and emoji celebrations | Emojipedia reference and emoji celebrations | Справочник Emojipedia и праздники эмодзи | Emojipedia — справочник и праздники эмодзи |
| `facebook-dev` | Meta developer projects | Meta developer projects | Проекты Meta для разработчиков | Meta — проекты для разработчиков |
| `hisense` | Hisense appliances and connected-TV services | Hisense appliances and connected-TV services | Техника Hisense и сервисы смарт-ТВ | Hisense — техника и сервисы смарт-ТВ |
| `hupu` | Hupu sports community and Shihuo shopping | Hupu sports community and Shihuo shopping | Спортивное сообщество Hupu и покупки Shihuo | Hupu — спортивное сообщество и покупки Shihuo |
| `kkbox` | KKBOX streaming and KKTIX ticketing | KKBOX streaming and KKTIX ticketing | Стриминг KKBOX и продажа билетов KKTIX | KKBOX — стриминг и продажа билетов KKTIX |
| `lark-global` | Lark office suite and related services | Lark office suite and related services | Офисный пакет Lark и связанные сервисы | Lark — офисный пакет и связанные сервисы |
| `meizu` | Meizu devices and Flyme ecosystem | Meizu devices and Flyme ecosystem | Устройства Meizu и экосистема Flyme | Meizu — устройства и экосистема Flyme |
| `meta` | Meta company and services | Meta company and services | Компания Meta и её сервисы | Meta — компания и её сервисы |
| `microsoft` | Microsoft company and services | Microsoft company and services | Компания Microsoft и её сервисы | Microsoft — компания и её сервисы |
| `mixi` | MIXI company and social platform | MIXI company and social platform | Компания MIXI и социальная платформа | MIXI — компания и социальная платформа |
| `oracle` | Oracle company and products | Oracle company and products | Компания Oracle и её продукты | Oracle — компания и её продукты |
| `pccw` | PCCW company and services | PCCW company and services | Компания PCCW и её сервисы | PCCW — компания и её сервисы |
| `renren` | Renren social network and former parent | Renren social network and former parent | Социальная сеть Renren и бывшая материнская компания | Renren — социальная сеть и бывшая материнская компания |
| `shireyishunjian` | Shireyishunjian community and Yingpianqu video-sharing network | Shireyishunjian community and Yingpianqu video-sharing network | Сообщество Shireyishunjian и видеосообщество Yingpianqu | Shireyishunjian — сообщество и видеосообщество Yingpianqu |
| `sohu` | Sohu media, games and Kuaizhan website tools | Sohu media, games and Kuaizhan website tools | Медиа и игры Sohu и создание сайтов Kuaizhan | Sohu — медиа и игры и создание сайтов Kuaizhan |
| `wistia` | Wistia business video platform | Wistia business video platform | Видеоплатформа Wistia для бизнеса | Wistia — видеоплатформа для бизнеса |

| `58tongcheng` | 58.com service portfolio | 58.com service portfolio | Портфель сервисов 58.com | 58.com — портфель сервисов |
| `5ch` | 5ch forum | 5ch forum | Форум 5ch | 5ch — форум |
| `mts-ru` | MTS ecosystem | MTS ecosystem | Экосистема МТС | МТС — экосистема |
| `qihoo360` | 360 security, software and services portfolio | 360 security, software and services portfolio | Продукты безопасности, программы и сервисы 360 | 360 — продукты безопасности, программы и сервисы |
| `yandex` | Yandex ecosystem | Yandex ecosystem | Экосистема Яндекса | Яндекс — экосистема |
| `falungong` | Falun Gong-related media and services | Falun Gong-related media and services | Медиа и сервисы, связанные с Фалуньгун | Фалуньгун — связанные медиа и сервисы |

## Implementation notes

The localized display-name/ID comparison runs once per picker construction.
Its cached alphabetical rank is reused by Qt's model sorting for the three
ordering modes (ordinary, effective AI, grouped search); this changes neither
collation nor tie-breaking. Native model sorting preserves item identity and
current-item indexes. Potential section headings persist as noninteractive,
hidden rows until matching entries make them visible. Item role +2 is now an
explicit unavailable boolean, independent of the historical collection flag.
Topic choices rebuild only when entering/leaving a global mode or changing the
browsing section; query edits and checkbox changes retain cached search text.

## Validation outcome

All four incremental Release builds passed; Windows x64 native and Linux amd64
CTests passed 5/5. Both ARM64 targets are compile-only on this x64 host. Native
EN/RU light/dark captures passed at measured DPR 1/1.5/2 with exact 590x350
minimum size. Real-catalog repeated query/toggle/mode timings, failure diagnosis,
exact metadata preservation and final review are recorded in
[the trial report](geosite-picker-trial.md#unified-ordering-and-interaction-implementation--2026-11-03).
No release/package, dependency, Core, service, commit, push or PR work occurred.

## Other labels and empty sections follow-up outcome — 2026-11-03

Implemented under explicit user authorization. See the accepted follow-up and
[actual build/launch results](geosite-picker-trial.md#other-labels-and-empty-sections-implementation-outcome--2026-11-03).
All four incremental Release builds passed. Normal Windows x64 GUI launched as
PID 23816. Tests, CTest, screenshots/visual fixtures, benchmarks and profiling were
explicitly not run; historical validation above does not validate this delta.
