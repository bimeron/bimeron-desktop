# Windows VM check for Desktop 0.2.0 and Core v0.2.0-beta.1

Run these commands **inside the disposable Windows 10 amd64 VM**, in an elevated PowerShell window. Never run the service commands on the host. The host has copied the MSI, Core ZIP, manifest, SHA256SUMS, and a reference Core executable to `C:\Users\Public\Documents\Bimeron-VM-Test`. Record results before changing the guest service. The MSI is a local test artifact, not a release.

## Recorded local VM result, 2026-10-23

The Windows amd64 test MSI is `dist/bimeron-desktop-0.2.0-windows-amd64.msi`, 29,044,736 bytes, SHA-256 `c68cd76971f41cdfce39442ed7240213ae6db210107c17d57f9cf49128fd900e`. MSI metadata reports ProductVersion `0.2.0` and the established UpgradeCode. Administrative MSI extraction yielded a bundled `bimeron-core.exe` with SHA-256 `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`, identical to the verified Core candidate executable. The bundled helper recognizes `service update -source`.

In the `Windows 10 STD` VM, the original running automatic service was Core `0.1.0-beta.4`, executable SHA-256 `b162c2ce459dcdf3a238a23e9638b647981cc2bf73413853ca71d74db0dbacbc`. A working VM VPN profile was connected. The MSI installed without changing the service executable. A wrong executable SHA-256 was rejected before replacement. A correct source with an intentionally wrong expected version caused IPC health-check failure and restored the original executable, running state, and old IPC version. A subsequent update with the correct version succeeded: the service remained running and automatic, its command path stayed the same, its executable hash became `75e55422ff50ce53d65870f774def68d6d754fa60dee6c77758b3b213b2f7b87`, and IPC `hello` reported `0.2.0-beta.1` at revision `78aca2aa5537f55365c7c7b0452600d69338e49f`. The existing VM profile passed `validate` and reconnected through an explicit Core CLI `start` after the direct helper update. This was **not** GUI-managed reconnection.

The manifest still has `gui_reviewed: false` and no signature, so GUI release discovery and GUI-managed update/reconnection cannot pass yet. The exact GUI build `0.2.0` has passed local tests and the installed package/helper checks above. The proposed range `[0.2.0, 0.2.1)` is not approved by this direct-helper test alone.

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
