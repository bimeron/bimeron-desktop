# Desktop 0.3.1 release acceptance

Tracking issue: [#61](https://github.com/bimeron/bimeron-desktop/issues/61).

## Scope

Desktop 0.3.1 is a GitHub pre-release built from `release/0.3.1` at `4286781`
(`42867818940c916fa188994fdbbd0cfafa2d436e`). It contains:

- the version bump to 0.3.1;
- the control server identity check (#59, PR #63), see
  [control server identity](control-server-identity.md);
- the reconnect policy and kill switch hold presentation (#60, PR #62), see
  [reconnect policy](reconnect-policy.md);
- the Core `v0.4.0-beta.2` pin (PR #64);
- the kill switch warning in the Core update dialog (PR #66);
- three trusted Core update signing keys (#65, PR #67), see
  [the trust set](core-updates.md#desktop-031-trust-set-and-signing-policy).

Supported packages are the two MSI and two DEB files; no other format is in scope.
No performance-sensitive code changed for the release itself, so benchmarks and
profiling are not applicable.

## Core input

Core `v0.4.0-beta.2` from commit `5251bd09084adf4fc1048a3def5a93f19f3a08ec`, the
frozen candidate that is published unchanged. Its signed manifest has GUI bounds
`[0.3.0, 0.3.2)`, IPC 1, profile format 1 and key `7370a2d8cf890e5e`. The four
archive hashes and the four extracted executable hashes in
`cmake/core-release.cmake` match the frozen candidate (`evidence/desktop-pins.json`):

| Target | Archive SHA-256 | Executable SHA-256 |
|---|---|---|
| windows-amd64 | `23b5fbf914e377e7291a4b17be287ec6e08f2ef2656e492ea6c30b506f0d95af` | `99cae551b82aa95f7f59ec5c7b9122d9e6c06d01ac9303c7d7d7356f7bfa9668` |
| windows-arm64 | `142a18609638080f657cc4c084c0eb88fd42c9940db10dffb328a40c0ba469e2` | `027f19073789f55c50accf5b7fc1aca1c598620359438ffaafafd4190ef0a41d` |
| linux-amd64 | `06479bafc1217edae5c9402dfa0d616d62f402951d006f932db37d9e355ffdd6` | `4dc244d72c0e51f038bfe4365497691f9edbb919d8f37216fd580ec70ad15868` |
| linux-arm64 | `e86e90a30d02db7b548bf72df6e734f22363abe1c6f4bce78e84fda4483f9843` | `8e00b06b3ee99e330b169c694aa5d6fa12fc859ee7bef8d313725d40da7d77e2` |

## Packages (2026-11-19)

| Package | Size | SHA-256 |
|---|---|---|
| `bimeron-desktop-0.3.1-windows-amd64.msi` | 49563226 | `9d76b824e0df1a4f9110976ab110c4b08bf93d6347419c6d3493e28200bd73d7` |
| `bimeron-desktop-0.3.1-windows-arm64.msi` | 38106407 | `8664c06ed1d6dc84cc4d906ec75d7ef89842ae1c8ba46ad398163cf77ef96149` |
| `bimeron-desktop-0.3.1-linux-amd64.deb` | 38663318 | `2133c4915f876c518dd7be2a7c37abfd847bc757d11cc3b8986dc9bee7027cbc` |
| `bimeron-desktop-0.3.1-linux-arm64.deb` | 35845618 | `f7d5bc4081d58495dc68d9aad6ef90f0dcbfbbccf051fc2a4db00867a405c0eb` |

How they were built:

- Windows: the steps of `scripts/package-all.ps1 -WindowsOnly`, run from a local
  copy whose only difference is the Core root: `.tools/core-candidate`, filled with
  the two frozen Windows ZIPs and verified offline by `scripts/fetch-core.cmake`.
  With a non-default root, CMake does not run the pinned `bimeron.exe version` on
  the build host. The pinned revision check was replaced by hashing the staged
  `bimeron-core.exe` and `repair/bimeron-repair.exe` against the executable pins.
  The published Core assets are the same bytes, so the packages equal a build
  from the default `.tools/core`.
- Linux: `scripts/package-linux.sh` on the Ubuntu 22.04 build VM from a
  `git archive` of `4286781` and the two frozen Linux ZIPs, with
  `CMAKE_BUILD_PARALLEL_LEVEL=2`.
- Configuration printed the trusted Core update key IDs `7370a2d8cf890e5e`,
  `e6fb20d55656904a` and `095d295f5fe9a4e2` for all four targets.

Package inspection:

- MSI administrative extraction: `bimeron-core.exe` and `repair/bimeron-repair.exe`
  equal the executable pin of their architecture; `wintun.dll` beside the GUI and
  beside the broker equals the ZIP's Wintun of that architecture; every PE file
  has the target machine (x64 or ARM64); `QT-NOTICE.txt`, `QT-LGPL-3.0.txt` and
  `QT-GPL-3.0.txt` are present; no `*-test*` file. ProductVersion `0.3.1` and the
  existing UpgradeCode `{E93DE42D-1F4F-4227-8056-1086581CCE51}`. 46 files (amd64)
  and 43 files (ARM64).
- `dpkg-deb -x`: `/usr/bin/bimeron-core` and
  `/usr/libexec/bimeron/repair/bimeron-repair` equal the executable pin, mode 755,
  ELF of the target machine; Version `0.3.1` with the target architecture; the
  license files are present; no `*-test*` file; 15 files each.
- Tests: Ubuntu 22.04 amd64 CTest 13/13 on the build VM. Windows test executables
  were not run on the build host; Desktop CI passed on all four targets for
  `4286781`.

## VM acceptance

Windows 10 STD (Hyper-V, x64). Start state: Desktop 0.3.0 with its Core
`v0.4.0-beta.1` service (`14c88367…`), three profiles. A checkpoint
`bimeron-rel031-pre-0.3.0-beta1` was taken first. UAC consent and credential
prompts were driven through the Hyper-V virtual keyboard; a SYSTEM watcher
recorded every `consent.exe` and Bimeron process start. Evidence stays in the
git-ignored `build/rel/vm-win`.

| Scenario | Result | Evidence |
|---|---|---|
| b+ 1. Upgrade 0.3.0 → 0.3.1 with a beta.1 service | PASS | `msiexec /i /qn` exit 0 in 5 s; log `Bimeron installer Core: service updated; reconnect VPN in GUI`. Service, bundled CLI and broker are `99cae551…`, service reports `0.4.0-beta.2` / `5251bd09`, `modified=false`. Profile files byte-identical. After a GUI restart: connected (`Подключено · 66.90.90.98:9108`). Repair with the service stopped: one consent prompt, service image not replaced (same hash and modification time), ready. Reconnected. |
| b+ 2. Upgrade with a service already on beta.2 | PASS | Uninstalled 0.3.1 (service kept), reinstalled 0.3.0: `newer installed version; skipped`, service PID unchanged. Upgrade to 0.3.1: `equal version, identical or unknown build; skipped`, service PID 1056 unchanged, no service events. Connect, Repair (one prompt, image unchanged), reconnect. |
| c. Clean install, corrupted image, alternate administrator | PASS | Service and package removed, no Bimeron directories left. Clean MSI install: `absent or untrusted service/payload; skipped`, no service. Restore installed the service with one prompt (pin, ready). Corrupted image (512 bytes XOR at 0x1000): Restore replaced it with the pin, one prompt. Alternate administrator: standard user `bimstd` signed in on the console; Restore showed the credential prompt for `bimeron-repair.exe`, credentials of administrator `test` entered. Installation and then repair of a corrupted image each took one prompt; the broker ran as `test` with the `bimstd` GUI as parent; service registered for the `bimstd` SID and ready within 1 s. |
| d. Wrong digest | PASS | `bimeron-core service update -source <beta.2 exe> -version 0.4.0-beta.2 -sha256 <last digit changed>`: exit 1, `staged executable SHA-256 mismatch`; service PID, state and image hash unchanged, no service or SCM events. |
| Identity: trusted service | PASS | Every connection above ran through the verified service pipe. |
| Identity: squatted pipe | PASS | Service stopped, `\\.\pipe\Bimeron.Core.Service` created by an administrator process. The GUI connected 8 times and wrote 0 bytes; log: `control_server_untrusted … service pipe owner S-1-5-32-544, expected S-1-5-18`, request not sent. |
| Reconnect: counter and pause | PASS | Outbound path to the server blocked by a firewall rule: loss detected after 84 s, `Переподключение… (1/10)` to `(10/10)` with Disconnect VPN, then `Сервер недоступен · повтор через 0:59` with the catalog cause and Retry now. Retry now started the next cycle at once; removing the block reconnected within 5 s. |
| Kill switch hold | PASS | Same profile with `KillSwitch = true` (restored to its original bytes afterwards). After the loss: `Интернет приостановлен до подключения VPN.` with Disconnect VPN. During the hold DNS outside the tunnel failed and an HTTP request bound to the physical interface could not connect (curl exit 7); routed requests reached only the tunnel. Disconnect VPN removed every Bimeron WFP filter; HTTP and DNS worked again. |
| Three trusted keys | PASS | Printed by configuration for all four targets (see Packages). |
| Update dialog warning | See scenario (a) | Needs a newer signed Core than the installed service. |

Observations, not defects:

- A standard user cannot repair a service that another user installed: Restore
  from `bimstd` while the service was registered for `test` ended with
  `Service ownership could not be established`, and nothing changed. The Core
  repair broker accepts only a service whose SCM command line names the calling
  user's SID (`cmd/bimeron/repair_windows.go`); the code and the Core
  `docs/REPAIR_SESSION.md` text ("Windows matches SCM path, account and owner")
  are identical in `v0.4.0-beta.1` and `v0.4.0-beta.2`, and Desktop 0.3.0 maps the
  same result to the same text. This is the documented ownership model: the
  service belongs to the user who installed it. The release notes list it under
  Known limitations.
- `bimeron-core service uninstall` refuses a running service (`stop service before
  uninstall`); `bimeron-core service stop` first. The release notes say so.
- The installed GUI process survives an MSI upgrade; it was restarted before each
  GUI check, as in 0.3.0.
- Earlier package builds left `repair-bootstrap-<digest>` cache directories under
  `C:\Program Files\Bimeron`; they are documented as not garbage-collected and
  were removed only to make the clean install clean.

Linux amd64 (Ubuntu 22.04, Qt 6.2, GNOME session). Start state: Desktop 0.3.0
with the Core `v0.4.0-beta.1` service (`5e584c9f…`).

| Scenario | Result | Evidence |
|---|---|---|
| f. DEB upgrade 0.3.0 → 0.3.1 with service update | PASS | `dpkg -i` exit 0, `Bimeron installer Core: service updated; reconnect VPN in GUI`; `dpkg -V` clean; service, `/usr/bin/bimeron-core` and broker equal `4dc244d7…`; service `0.4.0-beta.2` / `5251bd09`, active, NRestarts 0; profile files byte-identical. |
| f. Repair through polkit | PASS | Service stopped and 16 bytes of its image zeroed. GUI Restore → one polkit prompt for `bimeron-repair` → image replaced with the pin, service active, `Всё готово к подключению`; exactly one `pkexec` invocation. `/run/bimeron` is root `0711`, the socket `0600` owned by the GUI user. |
| Reconnect, initial connect | Observed | The VM's only profile points to a placeholder server, so a connection cannot succeed there; the GUI showed `Подключение… (2/3)` from the Core policy. The session was stopped through the CLI; no nftables rules were left. |

Linux arm64: package inspection only; there is no arm64 hardware.

Windows 11 ARM64 (**e**): NOT RUN. The Mac that hosts the Windows 11 ARM64 UTM
guest was unreachable on 2026-11-19 (no ping reply, SSH reset during the key
exchange). See Publication before scenario (e) below.

## Publication before scenario (e)

On 2026-11-19 the user decided to publish 0.3.1 with all four packages, before
scenario (e). The Windows ARM64 MSI (`8664c06e…`) is the final build from
`4286781` with Core `5251bd09`. Its package inspection passed: Core and broker
equal the ARM64 executable pin, Wintun equals the ARM64 archive, every PE file
targets ARM64, the Qt notices are present and no test file is included. It has not
been run on a device; the release notes say so. Scenario (e) runs when the Mac
is available, and its result is added here. If (e) finds a defect, a fixed
package needs a new Desktop release, because a published asset is not replaced
in place.

Desktop has no self-update. In both `v0.3.0` and `4286781` the only network
clients are `CoreUpdater` (`bimeron/bimeron-core` releases) and `GeoDataDownload`
(Loyalsoldier `v2ray-rules-dat`). Installed GUIs therefore never read this
release; users install Desktop packages manually.

## Scenario (a): Desktop 0.3.0 users

Run on Windows 10 STD after Core `v0.4.0-beta.2` was published
(tag `v0.4.0-beta.2` → `5251bd09`, signed manifest SHA-256
`31c7f51e62d23608c6440071813b05f74185cb953a33953e8c911ec613153bca`, key
`7370a2d8cf890e5e`), from the checkpoint with Desktop 0.3.0 and its beta.1
service. The installed, unmodified 0.3.0 read the real GitHub release.

| Step | Result | Evidence |
|---|---|---|
| In-app update 0.3.0 → beta.2 | PASS | Profile connected; About → "Включить бета-обновления ядра" → "Проверить обновление ядра": `Загрузка проверенного ядра 0.4.0-beta.2…`, then the dialog `Установка ядра 0.4.0-beta.2 прервёт работу VPN. После успешного обновления Bimeron снова подключит тот же профиль.` One consent prompt; the bundled beta.1 CLI ran `service update -source <staged> -version 0.4.0-beta.2 -sha256 99cae551…`. Service image `99cae551…`, `0.4.0-beta.2` / `5251bd09`; `Service ready`; GUI `Ядро 0.4.0-beta.2 установлено и проверено; VPN переподключён`, status connected to the same profile. About shows `Core: 0.4.0-beta.2 · CLI проверки: 0.4.0-beta.1`. |
| Repair in 0.3.0 after the update | Expected downgrade | Disconnected, service stopped, Restore: one consent prompt, the service image became the 0.3.0 broker bytes `14c88367…` (`0.4.0-beta.1`), ready. With beta updates still enabled, About again offered `Ядро 0.4.0-beta.2 готово к установке`. The release notes describe this. Reinstalling 0.3.0 over a beta.2 service does not downgrade (scenario b+ 2). |
| Upgrade 0.3.0 → 0.3.1 from this state | PASS | `service updated; reconnect VPN in GUI`, service `99cae551…`; after a GUI restart the profile connected. |
| 0.3.1 with a pre-identity service | PASS | Test setup only: the beta.2 helper refused to install the beta.1 image (`updated service IPC health check failed control_server_untrusted … previous executable restored`, service kept beta.2), so the beta.1 CLI installed it. Desktop 0.3.1 then reported `Службу нужно обновить или восстановить`, About `Core: неизвестно`. Restore with the service still running: one consent prompt, image replaced with `99cae551…`, ready. This is the GUI-driven old-service route that the identity check left open. |
| Update dialog kill switch warning | Not run on a VM | A real dialog needs a signed Core newer than the installed service that Desktop 0.3.1 also trusts. Beta.2 is the newest release, and 0.3.1 rejects the older services. The text and its conditions are covered by `developer-mode-test` in CI (PR #66); the first real occurrence will be the update to Core `v0.4.0-beta.3`. |

## Restored state

- Windows 10 STD: rolled back to the acceptance checkpoint (Desktop 0.3.0, Core
  `v0.4.0-beta.1` service `14c88367…` owned by `test`, the original four profile
  files), then the checkpoint was deleted. Removed afterwards: the acceptance
  watcher tasks and working directory, the temporary firewall rule, the standard
  test user `bimstd` and its profile, and the `C:\BimeronDesktopTest60` and
  `C:\BimeronIdentityTest` directories from earlier Desktop tests. No Bimeron WFP
  filters remain; `dontdisplaylastusername` is back to 0.
- Ubuntu 22.04 VM: left on Desktop 0.3.1 with the Core `v0.4.0-beta.2` service,
  stopped session, its one profile unchanged. Build state under
  `/opt/bimeron-compat/build` was removed to free disk space; `dist/` keeps the
  two DEBs.

## Remaining risks

- Windows ARM64: published before scenario (e); package-verified only until it
  runs. Linux arm64 is package-verified only.
- The Core update dialog warning was not observed with a real update (see scenario a).
- The packages are not code-signed.
