# Application display name

## Specification and acceptance criteria

Windows friendly-name surfaces must present the desktop application as Bimeron.
The existing app.rc has only an icon, so Windows falls back to the executable
basename in Task Manager Processes and Startup (both supplied screenshots).
Add PE FileDescription/ProductName and version metadata derived from the project
version, retaining InternalName and OriginalFilename as bimeron-desktop(.exe).
Set Qt applicationDisplayName to Bimeron without changing applicationName,
organization, settings paths, single-instance IPC or repair/bootstrap trust.

The Windows Run value is already Bimeron with the quoted executable path and no
arguments. Retain it byte-for-byte: no migration is needed and no StartupApproved
state is reset. Existing enabled and disabled entries use the new executable's
metadata after replacement; never rename entries or create duplicates. Linux
launcher/autostart Name and MSI shortcuts/product already say Bimeron and stay.
Details/image-name surfaces intentionally continue to show bimeron-desktop.exe.
No publisher/signing claims are added. Core pins and resources stay untouched.

Acceptance: both Windows architectures compile with correct PE description,
product, numeric/string versions, original filename and architecture. Focused
checks protect startup command/value and identity contracts. Run native x64 CTest;
ARM64 is compile/static only on this host. Real Task Manager Processes/Startup
validation requires exclusive coordinated VM handover; report separately if not
available. Shared Qt API must support Qt 6.2. No package/release is requested.
Benchmarks/profiling are inapplicable to static display metadata.

## Validation (2026-11-15)

- Isolated Windows amd64 and ARM64 Release builds completed with Qt 6.8.3,
  BIMERON_DEPLOY_QT=OFF, no package, Core download or dist modification.
- Native amd64 CTest passed 10/10; ARM64 host-side metadata CTest passed 1/1
  without executing ARM64 code. The test reads actual PE strings, numeric file
  version and machine type. Initial developer-mode-test failed at the existing
  Loyalsoldier status-line UI fixture; after supplying the documented
  QT_QPA_FONTDIR=C:/Windows/Fonts, its focused retry and the full suite passed.
- Windows test VM (Virtual Machine, build 19044) exclusive testing window:
  temporarily replaced only the installed GUI with the GUI-only amd64 build,
  retaining an exact original backup. Processes > Apps displayed Bimeron;
  Details retained bimeron-desktop.exe. Startup displayed Bimeron after restarting
  Task Manager (the already-open instance retained its old label).
- Actual initial StartupApproved state was Enabled, bytes
  020000000000000000000000. Temporarily set Disabled with the same value name,
  restarted the candidate GUI, checked unchanged Run command and Disabled bytes,
  and captured Startup showing Bimeron / Disabled. Restored exact initial bytes
  and the original GUI, hash-checked restoration and restarted the original GUI.
  Run remained exactly the quoted installed bimeron-desktop.exe path throughout.
  No migration is needed; neither production Run nor StartupApproved code changed.
- Local ignored screenshots: build/display-name-Processes.png,
  build/display-name-Startup.png, build/display-name-Startup-disabled.png and
  build/display-name-Details.png. These are native captures, not mockups.
- Guest test task/files removed, Task Manager closed, all PSSessions removed;
  exclusive window released to parent. Final original GUI PID 3596; Core service
  remained Running PID 1116. Core and repair broker SHA-256 both remained
  e1fb6792bac77f131ed0d7ffc554c060209637fd4fb042d800da578c115b7ce3.
  No profile selection, service restart, UAC, networking or MSI action occurred.
  VPN reconnection was not tested.

Diff review found no changes to executable paths, settings identity, IPC,
bootstrap/repair trust, Core pins, startup implementation or shortcut targets.
Canonical build documentation describes generated PE metadata and validation.
No performance-sensitive code changed; benchmarks/profiling are inapplicable.
Linux full builds/packages were not repeated: the only shared change uses the
Qt applicationDisplayName API supported by Qt 6.2; no Linux build/install flow,
launcher or autostart file changed. Linux native UI and ARM64 runtime remain
unverified. No new release package was prepared; new GUI bytes require normal
package/release validation before shipping. No publisher/signing state is inferred.

## Publisher metadata

The GUI PE resource adds CompanyName Bimeron Network and LegalCopyright
© 2026 NOOB RP LTD (brand as publisher, legal entity as copyright owner).
Task Manager Startup shows CompanyName as Publisher; Programs and Features
already shows the MSI Manufacturer Bimeron Network. File Properties > Details
shows Copyright but not CompanyName. The strings make no signing claim: UAC
keeps showing an unknown publisher until Authenticode signing exists, and a
future certificate subject will be the legal entity. Once the executable is
signed, UAC will show NOOB RP LTD as the verified publisher while CompanyName
still reads Bimeron Network; this split between brand and legal entity is
intended. The LegalCopyright string is a wide literal, so it does not depend on
the code page of the build machine. The metadata test checks
both strings for the amd64 or ARM64 build it is given; CI runs it on Windows x64.
