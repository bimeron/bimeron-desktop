# Windows VM check for Desktop 0.2.0 and Core v0.2.0-beta.1

## Published GitHub release through normal Desktop package, 2026-10-23

In `Windows 10 STD`, the running service was Core `0.1.0-beta.3` (SHA-256 `bd0c239e8d76ed13ee1efa198778c11a96db30255ebe2675aff96e3f038cc1d0`) with the disposable VPN profile connected. The normal Windows amd64 Desktop `0.2.0` MSI (SHA-256 `3dffc30f569a055a50a52b32bb83bda8263c044b0409ce05f19703b903ec7e71`) installed successfully. Its bundled Core SHA-256 was `174c288a5771d55c48e446fce3deef1f6fc7092d615f8277b5cc2aac75486f0f`. MSI installation left the service binary and VPN unchanged.

The installed normal GUI, with beta updates enabled, discovered published Core `v0.2.0-beta.1` through GitHub Releases and displayed the verified update prompt. The user accepted the VM update and UAC request. The Settings page then displayed that the service update completed, and About reported Core `0.2.0-beta.1`. Independent VM checks found service `Running/Auto` with the original command path, the installed executable SHA-256 `174c288a5771d55c48e446fce3deef1f6fc7092d615f8277b5cc2aac75486f0f`, and IPC revision `6e0c171505be8e349d5f42570f7ceb3e50ac6697`. Core status reported `connected (attempt 1)` with the same profile ID `a2b29dd3a1febcea4b0f48bc5323af78c9431cd28864820e1ba988a663207c48`. VM checkpoint `Published GitHub Core update succeeded Desktop 0.2.0` preserves the successful state. This completes the positive normal-source Windows amd64 update scenario. The final-byte negative cases, helper rollback repeat, and ARM64/Linux runtime checks remain separate work.

## Detailed GUI completion result, 2026-10-23

From the old-Core checkpoint, the normal Desktop `0.2.0` MSI was installed again and its GUI executable was replaced inside the protected installation by a local build containing the detailed result change from commit `21b7eba`. That build also included concurrent uncommitted profile-list edits, so it is not an exact build of that commit. This tests the new GUI code in an installed, protected path; a rebuilt MSI containing this change was not installed for this repeat. The GUI executable SHA-256 in the VM was `c4daee60f480acf928ac96af1ae9134dd33761f0f04213f33a4f65e132b05d3a`. The old service executable SHA-256 remained `bd0c239e8d76ed13ee1efa198778c11a96db30255ebe2675aff96e3f038cc1d0` and the disposable profile remained connected during preparation.

The updated GUI discovered the published release, the user approved UAC, and Settings displayed `Core 0.2.0-beta.1 installed and verified; VPN reconnected` (localized in Russian). Independent VM checks found service `Running/Auto` at its original command, installed Core SHA-256 `174c288a5771d55c48e446fce3deef1f6fc7092d615f8277b5cc2aac75486f0f`, IPC revision `6e0c171505be8e349d5f42570f7ceb3e50ac6697`, and `connected (attempt 1)` with the same profile ID `a2b29dd3a1febcea4b0f48bc5323af78c9431cd28864820e1ba988a663207c48`. VM checkpoint `Detailed GUI Core update result verified 0.2.0` preserves the result. A package rebuild and installation check for this GUI change remain before release.

## Final Core assets, 2026-10-23

The final Core assets at `dist/v0.2.0-beta.1/final-assets` differ from the earlier `gui-reviewed-test` bytes. The final manifest SHA-256 is `00816386cf49e072f6a75d9ffb6e9b0a5f5b5c4694e8026a0577cffe3ca96695`; signature SHA-256 is `544f8a22719cbbad819e95e9aca436189c0fd7daa43d323866e34b61739fabc3`. The pinned-key GUI verifier accepted the manifest and all four Windows/Linux amd64/arm64 ZIPs. The final Windows amd64 ZIP SHA-256 is `0b402618b934310b98934b8917832c34c9369f143044e97670710e401b92d22f`; its Core executable SHA-256 is `174c288a5771d55c48e446fce3deef1f6fc7092d615f8277b5cc2aac75486f0f`.

The Windows amd64 local-feed test MSI remains SHA-256 `8e11b03f5a32e2462b5c4354505eaed76f0917fd0b7e9fd4a6d114d3fe6a6fb8` and contains the previously verified trusted `-source` helper. From the old-Core VM checkpoint, its installed GUI accepted the final signed feed, displayed the `0.2.0-beta.1` update prompt, and warned about reconnecting the active VPN. After GUI confirmation and VM UAC approval, the service remained `Running/Auto` with its original command path. Its installed executable matched the final SHA-256 above; IPC `hello` returned version `0.2.0-beta.1`, revision `6e0c171505be8e349d5f42570f7ceb3e50ac6697`. Core status reported `connected (attempt 1)` with profile ID `a2b29dd3a1febcea4b0f48bc5323af78c9431cd28864820e1ba988a663207c48`, exactly the ID recorded before the update. The VM checkpoint `Final signed GUI update succeeded 0.2.0` preserves this state.

Tampered-manifest, tampered-signature, tampered-ZIP, and fault-injected rollback scenarios were completed against the earlier signed test set, **not** against the changed final bytes. An attempted repeat against `final-assets` did not produce results; those four scenarios remain pending for the final set. The normal GitHub Releases source, a package bundling the final Core binary, and other platform installations also remain untested. The VM was returned to the successful final-assets checkpoint. No host service or VPN was changed.

## Signed GUI update result, 2026-10-23

The isolated Windows amd64 GUI package `bimeron-desktop-0.2.0-windows-amd64-local-update-test.msi` (SHA-256 `8e11b03f5a32e2462b5c4354505eaed76f0917fd0b7e9fd4a6d114d3fe6a6fb8`) was installed in `Windows 10 STD`. The signed Core input came from `gui-reviewed-test`: manifest SHA-256 `136065518d43ac019484d70329677a6b706374020bd8dd36b16495d55457a515`, signature SHA-256 `918edee1dae40dcfe81fa5a4a36164589eb4666109234229051bf934083ad315`, key ID `7370a2d8cf890e5e`. The Desktop verifier accepted the signature and all four Windows/Linux amd64/arm64 ZIPs. The manifest says `gui_reviewed: true`, GUI `[0.2.0, 0.2.1)`, IPC 1, and profile format 1. The local release-list override exists only in this test build; normal builds retain GitHub Releases and the same pinned key and verification/install code.

From a VM snapshot with a running Core `0.1.0-beta.3` service and connected disposable VPN profile, installing the test MSI preserved the service executable, running/automatic state, command path, and VPN. The installed GUI executable SHA-256 was `de8837dd9df7e4f25b2e01530508f8926a021b0326e5821c7fac14c37f423620`; its protected bundled Core SHA-256 was `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`. With beta updates enabled, the GUI displayed the verified `0.2.0-beta.1` update prompt and warned that the active VPN would be interrupted and reconnected. After GUI confirmation and VM UAC approval for the bundled Core, the service was still running and automatic at the same command path. Its executable SHA-256 became `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`; IPC `hello` reported `0.2.0-beta.1`, revision `78aca2aa5537f55365c7c7b0452600d69338e49f`. Core status reported the test profile connected again (`attempt 1`, profile ID `a2b29dd3a1febcea4b0f48bc5323af78c9431cd28864820e1ba988a663207c48`). The positive result is preserved in VM checkpoint `Signed GUI update succeeded 0.2.0`.

After restoring the old-Core snapshot, GUI checks against separate copies of the assets rejected a one-byte-modified manifest, signature, and Windows amd64 ZIP. The first two showed no compatible signed update; the ZIP showed `Update archive size or hash mismatch`. None displayed an install prompt. The old service executable hash `bd0c239e8d76ed13ee1efa198778c11a96db30255ebe2675aff96e3f038cc1d0` and connected VPN status were unchanged. A separate invocation of the same protected bundled helper with a valid candidate source and deliberately wrong expected IPC version failed its health check and restored the old executable, running/automatic state, command path, and IPC version. This rollback fault was injected through the helper, not the GUI; VPN reconnection after a failed update was not established by that check. The VM was then restored to the successful GUI-update checkpoint.

The exact installed GUI version exercised end to end is `0.2.0` on Windows amd64. These results support compatibility of that version with the local signed Core candidate. The normal GitHub discovery/download path, Windows ARM64 and Linux installed-service paths, and the rest of the proposed GUI version interval have not been exercised; a production release claim for the entire `[0.2.0, 0.2.1)` interval needs those checks. No host service or VPN was changed, and nothing was pushed or published.

Run these commands **inside the disposable Windows 10 amd64 VM**, in an elevated PowerShell window. Never run the service commands on the host. The host has copied the MSI, Core ZIP, manifest, SHA256SUMS, and a reference Core executable to `C:\Users\Public\Documents\Bimeron-VM-Test`. Record results before changing the guest service. The MSI is a local test artifact, not a release.

## Recorded local VM result, 2026-10-23

The Windows amd64 test MSI is `dist/bimeron-desktop-0.2.0-windows-amd64.msi`, 29,044,736 bytes, SHA-256 `c68cd76971f41cdfce39442ed7240213ae6db210107c17d57f9cf49128fd900e`. MSI metadata reports ProductVersion `0.2.0` and the established UpgradeCode. Administrative MSI extraction yielded a bundled `bimeron-core.exe` with SHA-256 `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`, identical to the verified Core candidate executable. The bundled helper recognizes `service update -source`.

In the `Windows 10 STD` VM, the original running automatic service was Core `0.1.0-beta.4`, executable SHA-256 `b162c2ce459dcdf3a238a23e9638b647981cc2bf73413853ca71d74db0dbacbc`. A working VM VPN profile was connected. The MSI installed without changing the service executable. A wrong executable SHA-256 was rejected before replacement. A correct source with an intentionally wrong expected version caused IPC health-check failure and restored the original executable, running state, and old IPC version. A subsequent update with the correct version succeeded: the service remained running and automatic, its command path stayed the same, its executable hash became `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`, and IPC `hello` reported `0.2.0-beta.1` at revision `78aca2aa5537f55365c7c7b0452600d69338e49f`. The existing VM profile passed `validate` and reconnected through an explicit Core CLI `start` after the direct helper update. This was **not** GUI-managed reconnection.

The manifest still has `gui_reviewed: false` and no signature, so GUI release discovery and GUI-managed update/reconnection cannot pass yet. The exact GUI build `0.2.0` has passed local tests and the installed package/helper checks above. The proposed range `[0.2.0, 0.2.1)` is not approved by this direct-helper test alone.

## Isolated local GUI feed, 2026-10-23

The separate MSI `dist/bimeron-desktop-0.2.0-windows-amd64-local-update-test.msi` is 29,044,736 bytes, SHA-256 `8e11b03f5a32e2462b5c4354505eaed76f0917fd0b7e9fd4a6d114d3fe6a6fb8`. Its installed GUI executable has SHA-256 `de8837dd9df7e4f25b2e01530508f8926a021b0326e5821c7fac14c37f423620`. Its bundled Core matches the candidate executable SHA-256 `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`. MSI administrative extraction found no test executables or private key material. The GUI package uses the pinned production Ed25519 public key `bk6SfABmDG88uhUupYT+ia6dtSrpHbnGaBCfYvihJF0=` (key ID `7370a2d8cf890e5e`).

The local-feed option is compiled only into this test build. `BIMERON_TEST_RELEASES_JSON` must name an absolute local GitHub-shaped release-list file. All asset URLs must be local files under that list's directory. The normal build has no local-feed override and continues to fetch HTTPS GitHub Releases. The test build also retains the normal manifest verifier, archive verifier, protected helper, and GUI install/reconnection path.

An isolated `core-updater-feed-test` generated signed fixtures using an ephemeral local test key, public key `GDGP4uDy+RNl3qPim4ZdwSCQzifrr8on4uBnM+hDsVU=` (key ID `6de142a4d6c79fa5`). Its private seed was stored only in ignored local build state and removed from the VM after testing; this key is not in the MSI. In the VM, a validly signed `gui_reviewed: false` fixture failed with `Unreviewed or incomplete update compatibility`; a wrong signature failed with `Update signature verification failed`; and a changed ZIP failed with `Update archive size or hash mismatch`. A correctly signed, reviewed fixture reached the ready/staging state without invoking service installation. The VM service executable remained SHA-256 `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`, and its VPN remained connected.

Windows Installer did not replace the previous GUI executable when installing another `0.2.0` MSI over it. In the VM, the prior local GUI test MSI was removed, then the local-feed MSI was installed. The service executable, command, startup mode, running state, and connected VPN were unchanged. A checkpoint named `Before local GUI update feed test 0.2.0` preserves the prior VM state.

### Repeat with a production-key signed, reviewed manifest

1. Core supplies `update-manifest.json` and `update-manifest.json.sig` signed by the key ID `7370a2d8cf890e5e`, with `gui_reviewed: true`, GUI range `[0.2.0, 0.2.1)`, IPC 1, profile format 1, and unchanged exact sizes and SHA-256 values for the four desktop ZIPs. Do not send or copy the private key. Verify the exact manifest bytes, signature, and all four archive hashes with the GUI verifier before VM installation.
2. Restore a disposable VM snapshot with an older running Core service and a working test profile. The current VM already runs `0.2.0-beta.1`, so its present state cannot demonstrate discovery of a newer candidate. Copy and install the local-feed MSI in the restored VM; verify its SHA-256 and that the protected bundled Core matches the candidate. Record the original service hash, startup mode, command, IPC version, and active profile ID. Package installation must leave the service and VPN unchanged.
3. Copy the signed manifest, signature, and unmodified Windows amd64 ZIP into `C:\Users\Public\Documents\Bimeron-VM-Test\feed` in the VM. Keep the existing `releases.json` there; confirm its single release is `v0.2.0-beta.1` and its `file:` asset URLs resolve only inside that directory. Verify the copied files' hashes. First substitute a one-byte-changed ZIP while keeping the valid signed manifest: the GUI must refuse to offer installation and leave the service/profile unchanged. Restore the verified ZIP.
4. From a PowerShell window in the **interactive VM desktop**, launch the GUI with the local list for that process:

   ```powershell
   $env:BIMERON_TEST_RELEASES_JSON = 'C:\Users\Public\Documents\Bimeron-VM-Test\feed\releases.json'
   Start-Process 'C:\Program Files\Bimeron\bimeron-desktop.exe'
   ```

   Enable beta Core updates, select **Check for core update**, and record that the GUI offers `0.2.0-beta.1` only after signature, compatibility, and ZIP verification. Accept the warning and UAC prompt. Record that the elevated executable is `C:\Program Files\Bimeron\bimeron-core.exe` and its arguments include `service update -source`, `-version 0.2.0-beta.1`, and the extracted executable SHA-256.
5. Confirm the installed service executable SHA-256 is `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`, startup mode and command are preserved, and IPC `hello` reports `0.2.0-beta.1` at revision `78aca2aa5537f55365c7c7b0452600d69338e49f`. The GUI must reconnect the **same** previously active test profile once. Check a disconnected-service variant separately: it must stay disconnected. Record any failure and rollback without treating a mere version-range match as compatibility evidence.

Do not run these steps on the host. A signed local test manifest with `gui_reviewed: true` is for the isolated VM only until these checks finish; it is not an instruction to publish a release.

## 1. Verify the guest and inputs

```powershell
$model = (Get-CimInstance Win32_ComputerSystem).Model
if ($model -ne 'Virtual Machine') { throw "Not a VM: $model" }
$work = 'C:\Users\Public\Documents\Bimeron-VM-Test'
$msi = Join-Path $work 'bimeron-desktop-0.2.0-windows-amd64.msi'
$zip = Join-Path $work 'bimeron-v0.2.0-beta.1-windows-amd64.zip'
$manifest = Get-Content (Join-Path $work 'update-manifest.json') -Raw | ConvertFrom-Json
$asset = $manifest.archives | Where-Object { $_.os -eq 'windows' -and $_.arch -eq 'amd64' }
if ($asset.name -ne (Split-Path $zip -Leaf) -or (Get-Item $zip).Length -ne $asset.size -or
    (Get-FileHash $zip -Algorithm SHA256).Hash.ToLower() -ne $asset.sha256) { throw 'Core ZIP mismatch' }
if ((Get-FileHash $msi -Algorithm SHA256).Hash.ToLower() -ne 'c68cd76971f41cdfce39442ed7240213ae6db210107c17d57f9cf49128fd900e') { throw 'MSI mismatch' }
if ($manifest.compatibility.gui_reviewed -ne $false -or (Test-Path (Join-Path $work 'update-manifest.json.sig'))) { throw 'Candidate state changed; stop and review' }
Expand-Archive -LiteralPath $zip -DestinationPath (Join-Path $work 'core-zip') -Force
$source = Join-Path $work 'core-zip\windows-amd64\bimeron.exe'
$digest = (Get-FileHash $source -Algorithm SHA256).Hash.ToLower()
if ($digest -ne '75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87') { throw 'Core executable mismatch' }
& $source version
```

## 2. Record the original service and install the GUI package

```powershell
$service = Get-CimInstance Win32_Service -Filter "Name='BimeronCore'"
if (-not $service) { throw 'Install an older test Core service in the VM first' }
$beforeState = $service.State
$beforeStartMode = $service.StartMode
$beforeCommand = $service.PathName
$installedCore = 'C:\Program Files\Bimeron Core\bimeron.exe'
$beforeHash = (Get-FileHash $installedCore -Algorithm SHA256).Hash.ToLower()
"Before: $beforeState $beforeStartMode $beforeCommand $beforeHash"
& $installedCore version -daemon
& $installedCore status
```

If `status` reports an active VPN, record its profile and test only if the VM has a working disposable profile. Do not copy or log credentials. Install the MSI, then compare the package helper with the verified candidate and confirm the service has not changed:

```powershell
$install = Start-Process msiexec.exe -ArgumentList @('/i', "`"$msi`"", '/qn', '/norestart') -Wait -PassThru
if ($install.ExitCode -ne 0) { throw "MSI exit $($install.ExitCode)" }
$trusted = 'C:\Program Files\Bimeron\bimeron-core.exe'
if ((Get-FileHash $trusted -Algorithm SHA256).Hash.ToLower() -ne $digest) { throw 'Bundled helper mismatch' }
& $trusted service
if ((Get-FileHash $installedCore -Algorithm SHA256).Hash.ToLower() -ne $beforeHash) { throw 'MSI changed installed service' }
```

## 3. Test rejection, rollback, and successful update

The following steps interrupt a running **VM** service. Run them only after recording its initial state. Use the installed, protected package helper as the command executable; the extracted candidate is only the `-source` data file.

First pass an incorrect digest. This must fail before the service is stopped and must preserve its hash and state:

```powershell
& $trusted service update -source $source -version '0.2.0-beta.1' -sha256 ('0' * 64)
if ($LASTEXITCODE -eq 0) { throw 'Wrong digest accepted' }
if ((Get-FileHash $installedCore -Algorithm SHA256).Hash.ToLower() -ne $beforeHash -or
    (Get-Service BimeronCore).Status.ToString() -ne $beforeState) { throw 'Wrong digest changed service' }
```

If the original service is running, request an intentionally wrong expected version with the **correct** digest. The helper must restart the candidate, reject its IPC `hello` version, roll back, and restore the old running service:

```powershell
if ($beforeState -eq 'Running') {
    & $trusted service update -source $source -version '0.2.0-beta.1-rollback-test' -sha256 $digest
    if ($LASTEXITCODE -eq 0) { throw 'Wrong IPC version accepted' }
    if ((Get-FileHash $installedCore -Algorithm SHA256).Hash.ToLower() -ne $beforeHash -or
        (Get-Service BimeronCore).Status -ne 'Running') { throw 'Rollback failed' }
    & $trusted version -daemon
}
```

Now perform the real update. A previously running service must remain running and answer IPC `hello` with Core `0.2.0-beta.1`; a previously stopped service must remain stopped. The service start mode and command must remain unchanged:

```powershell
& $trusted service update -source $source -version '0.2.0-beta.1' -sha256 $digest
if ($LASTEXITCODE -ne 0) { throw "Update exit $LASTEXITCODE" }
$after = Get-CimInstance Win32_Service -Filter "Name='BimeronCore'"
if ($after.State -ne $beforeState -or $after.StartMode -ne $beforeStartMode -or
    $after.PathName -ne $beforeCommand -or
    (Get-FileHash $installedCore -Algorithm SHA256).Hash.ToLower() -ne $digest) { throw 'Updated service state or payload mismatch' }
if ($beforeState -eq 'Running') {
    $hello = & $trusted version -daemon | ConvertFrom-Json
    if ($hello.version -ne '0.2.0-beta.1') { throw 'IPC hello version mismatch' }
    & $trusted status
}
```

When a working disposable VM profile exists, validate it with the installed package helper before the update and check connection through the GUI after a signed release makes the GUI update flow available. The direct helper command does not perform GUI reconnection. Keep the profile path and credentials out of shared logs.

## 4. GUI update path after signing

The current local manifest has no `.sig` and sets `gui_reviewed: false`; the GUI must reject it. A signature alone does not make the release discoverable: the GUI currently fetches GitHub Releases. After Core review, verify the exact signed manifest and all four ZIPs with `core-update-test --verify-release` for GUI `0.2.0`, then exercise GUI discovery and installation through an approved release or a controlled test endpoint. Check the GUI prompt, protected bundled helper invocation, IPC `hello`, and reconnection of the same active test profile. Keep this VM isolated from the host VPN.
