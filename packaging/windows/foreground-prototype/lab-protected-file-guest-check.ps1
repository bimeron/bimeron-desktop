param([string]$Generator = 'Visual Studio 18 2026', [Parameter(Mandatory=$true)][string]$Dumpbin,
      [ValidateSet('Both','x64')][string]$Architectures = 'Both')
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$source = Join-Path $root 'packaging/windows/foreground-prototype'
$bundle = Join-Path $root 'build/guest-bundle'
New-Item -ItemType Directory -Force $bundle | Out-Null
$hashes = [ordered]@{}
$targets = if ($Architectures -eq 'Both') { @('x64','ARM64') } else { @('x64') }
foreach ($arch in $targets) {
    $build = Join-Path $root "build/guest-$arch"
    & cmake -S $source -B $build -G $Generator -A $arch -DFOREGROUND_LAB_PROTECTED_FILE=ON
    if ($LASTEXITCODE) { throw "Configure failed: $arch" }
    & cmake --build $build --config Release --target lab-protected-file-guest
    if ($LASTEXITCODE) { throw "Build failed: $arch" }
    $exe = Join-Path $build 'Release/lab-protected-file-guest.exe'
    $imports = & $Dumpbin /imports $exe
    if ($LASTEXITCODE) { throw "Import inspection failed: $arch" }
    if (($imports -join "`n") -match '(?i)MsiDoAction|msi\.dll|CreateProcess|ShellExecute|WinExec|ws2_32|winhttp|wininet') { throw "Forbidden import: $arch" }
    $destination = Join-Path $bundle $arch
    New-Item -ItemType Directory -Force $destination | Out-Null
    Copy-Item -LiteralPath $exe -Destination (Join-Path $destination 'lab-protected-file-guest.exe') -Force
    $hashes[$arch] = (Get-FileHash $exe -Algorithm SHA256).Hash
    if ($arch -eq 'x64') {
        & $exe --verify-only
        if ($LASTEXITCODE -ne 0) { throw 'Verify-only failed' }
        & $exe --guest-pending-file-once --lab-only DisposableWindowsVM
        if ($LASTEXITCODE -ne 3) { throw 'Host path guard failed' }
        & $exe --guest-pending-file-once --lab-only
        if ($LASTEXITCODE -ne 2) { throw 'Argument guard failed' }
    }
}
$files = @('CMakeLists.txt','lab-protected-file-guest.cpp','lab-protected-file.cpp','lab-protected-file.h','lab-protected-file-guest-README.md','lab-protected-file-guest-check.ps1')
$sourceHashes = [ordered]@{}
foreach ($file in $files) { $sourceHashes[$file] = (Get-FileHash (Join-Path $source $file) -Algorithm SHA256).Hash }
$manifest = [ordered]@{
    provisional = $true; completeArchitectureMatrix = ($Architectures -eq 'Both'); executableSha256 = $hashes; appLocalDlls = @(); sourceSha256 = $sourceHashes
    fixedGuestPaths = @('%ProgramFiles%\BimeronProtectedFileLab','%ProgramFiles%\BimeronProtectedFileLab\lab-protected-file-guest.exe','%ProgramFiles%\BimeronProtectedFileLab\LABProtectedFiles','%ProgramFiles%\BimeronProtectedFileLab\LABProtectedFiles\pending.bin')
    expectedState = 'Pending'; expectedCounter = 1; automaticCleanup = $false
}
$manifest | ConvertTo-Json -Depth 5 | Set-Content -Encoding utf8 (Join-Path $bundle 'manifest.json')
Write-Host "Provisional manifest: $bundle/manifest.json"
exit 0
