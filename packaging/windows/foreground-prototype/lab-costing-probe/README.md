# Bounded CostInitialize probe

Accepted contract: parent specification commit 776ea9a. This standalone x64
experiment is disconnected from production builds and installation rules.
It is a discrimination experiment, not a fix or native enrollment evidence.

## Isolation proof and execution boundary

Microsoft documents that [MsiOpenPackageExW flag 1](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msiopenpackageexw)
creates a machine-state-independent restricted handle incapable of changing
machine state, excludes DLL/EXE/script custom actions, and permits CostInitialize.
It requires COM initialization on the same thread. [MsiDoActionW](https://learn.microsoft.com/en-us/windows/win32/api/msiquery/nf-msiquery-msidoactionw)
permits CostInitialize outside an initialized installation transaction.
The original restricted mode initializes COM on its sole calling thread, opens flag-1 handles,
passes only literal `CostInitialize`, closes each successful handle, and balances
COM cleanup. There is no sequence, default action, installation or other action
entry point. The source and compiled MSI import allowlist are checked offline.

Independent review of the new mode is pending. Only the parent may run the guest command after
review in a disposable x64 Windows VM. Never run the guest mode on the host.
The restricted mode was run in the disposable VM; no guest run of the new mode has occurred.

## Build and offline checks

Use cached Visual Studio 18 Build Tools and CMake, without dependencies:

```powershell
cmake -S packaging/windows/foreground-prototype/lab-costing-probe -B build/lab-costing-probe -G "Visual Studio 18 2026" -A x64 "-DCMAKE_GENERATOR_INSTANCE=C:/Program Files (x86)/Microsoft Visual Studio/18/BuildTools"
cmake --build build/lab-costing-probe --config Release --target lab-storage
python packaging/windows/foreground-prototype/lab-costing-probe/prepare.py
cmake --build build/lab-costing-probe --config Release
python packaging/windows/foreground-prototype/lab-costing-probe/check.py
```

Preparation refuses existing output MSIs. It verifies the exact frozen input
path/hash before copying and again afterward. Existing copies are retained as
evidence; use a separately reviewed fresh build directory if preparing again
(the preparation script currently fixes the documented build directory).
Regenerated artifacts must be reviewed and repinned; do not assume byte-for-byte
reproducibility of Windows Installer database commits.

`prepare.py` reuses the read-only checker to compare every persistent table,
actual columns and data (including Binary hashes). Component schema includes its
primary-key type and remains exactly equal with zero rows. FeatureComponents has
its two nonnullable primary-key columns and no rows. The feature-only change may
alter only the table/column/string metadata compound streams; all other stream
bytes are equal. The explicit Component change adds exactly one zero-byte stream,
leaving every existing stream unchanged. Summary, Binary, sequence, identities,
resource and validation data remain unchanged. Physical container allocation is
not required to be identical after native storage/database commits.

Table stream encoding is derived from Wine's primary implementation,
https://github.com/wine-mirror/wine/blob/master/dlls/msi/table.c
(`encode_streamname`, `read_stream_data`): 0x4840 prefix, the MSI 64-character
alphabet and paired six-bit encoding. The mapping is checked against existing
nonempty Feature, Directory, Property, _Tables and _Columns streams in the frozen
MSI before adding Component. Thus the name is not inferred from an absent stream.
`lab-storage` uses native IStorage only; it is an offline preparation utility,
not part of the guest probe. It never opens an MSI session.

The generated `Release/offline-report.json` records all logical tables and
physical stream sizes/hashes for each copy. `check.py` passes corrupt-hash tests
for each fixed case, swapped-case rejection, three invalid argument tests,
verify-only mode, and compiled MSI import inspection. The Windows SDK links MSI
by ordinal; the check resolves these against the system DLL exports and requires
exactly MsiOpenPackageExW, MsiDoActionW and MsiCloseHandle. No delay imports occur.
An initial name-only import check failed because of ordinal imports; the corrected
check resolves the actual ordinals (232, 34, 8). No action was invoked to test this.

## Frozen artifacts and guest command

Artifacts: `build/lab-costing-probe/Release/`. Copy only the probe executable and
four MSI files together to a private directory in the disposable guest.
The original executable accepted `--verify-only` or `--guest-cost-initialize`,
locates the four fixed names beside itself, pins all four hashes before opening
any session, and retains non-write/non-delete-shared file handles until cleanup.
There is no case, path, property, action or environment override. Output contains
only numeric case/status fields and pinned hashes; a failed action is a measured
result, not a probe process failure. Close status is reported separately. No logs
containing accounts or environment are requested.

Fixed parent-operated guest command, from that directory:

```powershell
.\lab-costing-probe.exe --guest-cost-initialize
```

SHA-256:

```text
lab-costing-probe.exe 3e13664c8430a2b3af7e6eee94b11ac850659b06fac3bc92e339272ddc263b67
lab-storage.exe       03d9f89861543de3a179e02f57c5c95caa4c99e46867dd7a3bf8a83b60ba0cb5
original.msi          0c73788944a2d520628e7dd619df2eb507821021fd76cef4dd879851586d0dbf
component-stream.msi  99a09d53f05af7c82c8eede7c64a3200731eeee87bdde9bc86dc0aedc1290561
feature-components.msi 4438582783be142a9eceae71bf9f53bfdc124747eacd27f938f6c626bd739447
both.msi              0eda7262ab1fe7e365e1db8cbd374a1aa00d5fb4442816172f1da316226cb2df
```

MSVC x64 standalone compilation and all described offline checks passed.
Diff review found no production registration or resource changes. No performance
path changed, so benchmarks/profiling are inapplicable. Windows ARM64 and both
Linux targets are unaffected by this disconnected x64 Windows experiment; the
release build/package matrix was not run. Remaining risks:
independent review and guest results pending; results cannot establish native
installation safety, callback behavior, outer transaction settlement or enrollment.

## Accepted machine-state experiment specification

Add exactly `--guest-cost-initialize-machine-state` to select a normal
`MsiOpenPackageExW` handle with literal option `0`. Preserve `--verify-only`
 and the existing flag-1 `--guest-cost-initialize` mode. The normal handle is
machine-dependent and is not an immutability guarantee; the bounded call path
is the safeguard. The four frozen inputs have no embedded UI. Initialize COM on
the calling thread, validate all four pinned hashes before any MSI open, retain
non-write/non-delete-shared read handles through cleanup, call only literal
`CostInitialize`, and close every successfully opened MSI handle. The only MSI
imports are `MsiOpenPackageExW`, `MsiDoActionW`, and `MsiCloseHandle`; no install,
sequence, transaction, default or null action, callback, property setter,
user-selected input, or embedded UI is allowed. Output identifies the fixed
mode and options and retains bounded numeric statuses and hashes.

Acceptance: an offline x64 build in a new directory uses the exact four frozen
MSIs without regenerating them; the checker verifies both option branches,
argument and hash rejection, and compiled MSI imports without host MSI actions.
Independent review precedes any parent-operated disposable VM run. The saved
restricted-mode guest evidence at `build/vm-lab-evidence-20261030/costing-probe-66bb4b28.txt`
reports all four actions returned 0. A full `/qn` run of the same original source
MSI failed with 2728 at CostInitialize; neither result establishes a fix.
Microsoft documents [normal and restricted open options](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msiopenpackageexw)
and [CostInitialize outside an installation transaction](https://learn.microsoft.com/en-us/windows/win32/api/msiquery/nf-msiquery-msidoactionw).

## Machine-state build and offline results

The new executable is `build/lab-costing-probe-machine-state/Release/lab-costing-probe.exe`,
SHA-256 `e9c74d70bf3e4993ff0f52b385687e9fe12c700383cc3b7ff070cf9901abb642`.
The four pinned MSIs and the original executable in the parent build directory
were retained untouched; copies of the MSIs and offline report were used in the
new directory. The checker accepts that directory as its sole optional argument:

```powershell
python packaging/windows/foreground-prototype/lab-costing-probe/check.py build/lab-costing-probe-machine-state/Release
```

It checks the two fixed open branches and literal action in source, the compiled
MSI import allowlist, all four hashes and logical/stream comparisons, invalid
arguments, and corruption rejection for verify-only and both guest arguments.
No host MSI action was invoked. After independent review, the parent may run
`lab-costing-probe.exe --guest-cost-initialize-machine-state` only in the
disposable guest with the four pinned files beside it. The new mode remains
untested in the guest; neither the restricted result nor the full-install failure
proves a cause or fix.

## Accepted logged machine-state experiment specification

Add only `--guest-cost-initialize-machine-state-logged`, using option 0 and the same four pinned inputs, retained read handles, COM lifecycle and literal action. Before any open for each case, reserve its fixed `C:\ProgramData\BimeronCostingLab-66bb4b28\costing-engine-caseN.log` with CREATE_NEW, then enable verbose and extra-debug logging with flush-each-line. Abort before opening on reservation or logging failure; disable logging after each close or failed open and abort on disable failure. Report only bounded mode, options, numeric logging and existing statuses; never print the log contents. The fixed existing directory must be owned by SYSTEM or Administrators, have a protected DACL granting access only to those principals, and be held without delete sharing; reject reparse points in it and its ProgramData parent. Reserve each file with the inherited private ACL, close it before MSI logging, and disclose the resulting non-atomic gap: privileged principals can replace the reservation. No user-selected paths or additional MSI operations are permitted. Logs can contain sensitive properties and stay private.

Acceptance: offline checks reject invalid arguments and each invalid hash before logging, inspect the fixed paths, ordering and boundaries in source, and require exactly the four MSI imports including `MsiEnableLogW`. Build a new x64 executable without regenerating or changing frozen inputs or older binaries. Independent parent review precedes any disposable VM execution. This is a diagnostic discriminator, not a fix; both previous probe modes returned action 0 in all four cases while the full msiexec server run reported 2728. No host MSI actions or log execution are authorized. [MsiEnableLogW](https://learn.microsoft.com/en-us/windows/win32/api/msi/nf-msi-msienablelogw) requires an existing directory and documents the logging options and disable call.

## Logged build and offline results

The x64 executable at `build/lab-costing-logged/Release/lab-costing-probe.exe` has SHA-256 `de8170d4d44e8e1c4b383dbcc828ee242d8c1bed30e51696e26777f7c9943223`. The four frozen MSIs and offline report were copied to this new directory without regeneration; older executables and MSIs were retained. The checker passed source-boundary, exact four MSI-import, invalid-argument, all four corrupt-hash checks for every mode, swapped-input and verify-only checks. No host MSI action or logging was executed. The fixed directory and each reserved file must pass the stated ownership and ACL checks; the directory and ProgramData handles are held without delete sharing. The reservation is closed before the engine opens the path, leaving a non-atomic gap for privileged principals. Logs may contain sensitive properties and must remain private.

Independent parent review and a separately operated disposable-guest run remain pending. The earlier flag-0 and flag-1 guest probes returned action 0 for all four cases; the full msiexec server run reported CostInitialize 2728. Logging is a discriminator, not a fix. This standalone x64 experiment does not alter production packaging; ARM64, Linux, performance benchmarks, profiling and release packaging are inapplicable.

## One-shot full-server discriminator

Permit exactly one parent-operated full-server installation of frozen `both.msi`,
SHA-256 `0eda7262ab1fe7e365e1db8cbd374a1aa00d5fb4442816172f1da316226cb2df`,
in the authorized disposable x64 VM. Recheck the guest hash and require ProductCode
`{BF4DEC35-F5C3-4809-976C-112E597FCF6A}` to be unregistered immediately before launch.
Use `/i /qn /norestart` with a fresh private verbose log, no transforms or property
overrides. This permits lab registration and eligible observation callbacks, beyond
isolated costing. Preserve the input, log, process result, callback evidence and
registration state. Stop after this case: no automatic uninstall, retry, reboot or
other variant. A timeout leaves completion unknown; never kill Installer as recovery.

Passing server CostInitialize without 2728 is the discriminator. Full lab success
additionally requires exit 0, successful InstallFinalize, expected registration and
Preflight -> DeferredBegin -> Commit -> PostFinalize success observations, without
executed rollback. Any later failure is recorded separately. Combined success does
not identify which structural addition matters and grants no production/native
installation or enrollment approval.

### Observed one-shot result (2026-10-30)

The authorized Windows 10 STD VM ran the pinned `both.msi` through native x64
`msiexec /i /qn /norestart` from an elevated, noninteractive PowerShell Direct
session. Exit code was 0. Server CostInitialize, InstallInitialize and
InstallFinalize succeeded. Observed callbacks were Preflight, DeferredBegin,
Commit and PostFinalize, each with complete status-0 records and intended return 0;
no rollback callback executed. Product state was -1 both before and after.
Consequently this is a positive server-costing/callback discriminator, not the
specified full lab success with registered product state. No uninstall or retry
followed. The combined change does not identify which empty-table/stream addition
is necessary. Interactive UAC, initiating-client authentication and production
transaction safety remain unproved. Raw logs remain in ignored private lab evidence.
