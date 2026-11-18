# Bounded reconnect recovery

Agreed specification for Desktop 0.3.0; issues [Desktop #48](https://github.com/bimeron/bimeron-desktop/issues/48)
and [Core #136](https://github.com/bimeron/bimeron-core/issues/136). Desktop 0.3.1 presents the newer
Core policy as described in [Reconnect policy presentation](reconnect-policy.md); this behaviour
remains for a Core that does not report it.

Core owns recovery and permits three retries after initial failure or a later
interruption, excluding initial Open. Both ordinary and persistent transport
recovery are bounded. Success resets the budget for the next interruption.
Stop cancels pending retries; nonretryable errors end immediately. After the last
failure, cleanup completes and state becomes failed with the last problem.
Cleanup failure takes precedence.

## IPC and presentation

IPC stays version 1. Optional integer status/event fields `ReconnectAttempt`
(1..3) and `ReconnectLimit` (3) describe the upcoming/current retry, including
its backoff. Zero/reset values are omitted. Success/disconnection/new Start reset
progress; terminal failure retains it. Existing `Attempt` remains cumulative per
Start and includes initial Open. Events may be dropped; status is authoritative.

The Profiles connection status reads `Reconnecting… (1/3)` in English and
`Переподключение… (1/3)` in Russian. Reconnecting and failed statuses have a
tooltip with the textual `LastError` code and the localized catalog message.
Missing catalog entries have a localized generic explanation. Empty error means
no tooltip. Arbitrary legacy error strings are not exposed; only bounded
catalog-style identifiers are displayed as codes. Catalog text is escaped for Qt
rich-text detection. Successful connection and other states clear the tooltip.

Old services, including 0.2.0-beta.1, may omit progress. Missing, noninteger,
nonpositive or out-of-range fields leave the plain reconnecting label. Desktop
never invents a limit, derives recovery progress from cumulative Attempt, or
stops service recovery on its own. The old service retry policy remains its own.

## Acceptance and validation

- Core tests cover initial + three retries, ordinary/persistent exhaustion,
  cleanup priority, progress events and JSON field names, success/reset and Stop.
- Desktop fixtures cover both languages, attempt changes without state changes,
  tooltip localization/code, failed cause, success clearing, missing/invalid
  progress and unsafe legacy errors. Run the Windows x64 CTest suite.
- Review diffs for secrets, stale presentation, compatibility and cancellation.
  No performance-sensitive data path is added; benchmarks/profiling are not
  applicable. Product builds and tests do not authorize release packaging.

## Release gate

This full behavior is required for Desktop 0.3.0. The frozen Core e1cada7d
candidate and Desktop pins remain untouched by this branch. Core must merge via
a PR with green CI and a merge commit; the release thread builds and freezes a
new candidate from that master commit, updates pins separately and repeats the
supported four-target package/build/test matrix and VM acceptance. Current GUI
checks alone do not establish end-to-end acceptance with the new Core.

## Local verification (2026-11-14)

Windows x64 Release configured and built with the windows-amd64-release preset,
BIMERON_DEPLOY_QT=OFF and BIMERON_REQUIRE_PYTHON_TESTS=ON. CTest passed 9/9
in 24.78 seconds, including the bilingual reconnect fixtures. The environment
used Qt 6.8.3 msvc2022_64/bin on PATH, QT_QPA_PLATFORM_PLUGIN_PATH set to its
plugins/platforms, QT_QPA_PLATFORM=offscreen and QT_QPA_FONTDIR=C:/Windows/Fonts.
The first fixture run exposed incomplete simulated Core readiness; the fixture
now establishes readiness and restores language/notification settings. A later
existing GeoData layout assertion failed without the documented font directory;
setting it restored the date label and the complete suite passed. No GeoData
production code or assertion was changed.

Core branch feat/reconnect-attempt-limit at
29b5cd8a7cc84ddc9eccad9b79ac7a42410267e5 is based on upstream master e1cada7d.
Windows amd64 go vet ./..., go test -p 1 ./... and go test -race ./core
./internal/control passed; the Core suite also passed ten consecutive runs.
Diff review and git diff --check passed in both repositories. No unresolved
local correctness/security findings remain. No benchmark/profile is applicable.

This is a GUI-only CI-equivalent build without a configured repair payload, so
the optional embedded-resource test is not registered. No package was produced.
Linux and ARM64 checks were deferred to Core PR CI and the new-candidate release
matrix, rather than treated as unaffected. No live service, UAC/polkit, network
failure VM scenario or new-candidate integration acceptance was exercised.
