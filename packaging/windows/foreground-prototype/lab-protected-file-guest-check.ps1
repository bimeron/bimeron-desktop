param([Parameter(Mandatory=$true)][string]$Dumpbin,
      [Parameter(Mandatory=$true)][string]$FrozenBundlePath)
$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$bundle = Join-Path $root 'build/guest-bundle-final-x64-reviewed'
if (Test-Path -LiteralPath $bundle) { throw 'Final bundle already exists; preserve it and review separately' }
if (Get-ChildItem -LiteralPath (Join-Path $FrozenBundlePath 'x64') -Filter '*.dll') { throw 'App-local DLLs forbidden' }
$old = Get-Content -Raw -LiteralPath (Join-Path $FrozenBundlePath 'manifest.json') | ConvertFrom-Json
$exe = Join-Path $FrozenBundlePath 'x64/lab-protected-file-guest.exe'
$expected = '9F864EB8B5E8018F945DD30036464B3DDA9271D197C4F379AAEC357C1584B5C2'
if ((Get-FileHash -LiteralPath $exe).Hash -cne $expected) { throw 'Frozen executable changed' }
$allow = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'lab-protected-file-guest-imports.json') | ConvertFrom-Json
function Assert-Imports([string[]]$Lines) {
    $dll = $null; $count = 0; $seen = @{}
    foreach ($line in $Lines) {
        if ($line -match '(?i)delay') { throw 'Delay imports forbidden' }
        if ($line -match '^    (\S+\.dll)\s*$') {
            $dll = $Matches[1]
            if ($dll -notin $allow.dlls.PSObject.Properties.Name) { throw "Unlisted DLL: $dll" }
            $seen[$dll] = $true
        } elseif ($line -match '^\s+[0-9A-Fa-f]+\s+(\w+)\s*$' -and $dll) {
            $api = $Matches[1]
            if ($api -cnotin $allow.dlls.$dll) { throw "Unlisted API: $dll!$api" }
            $count++
        } elseif ($line -match '(?i)ordinal') { throw 'Ordinal imports forbidden' }
    }
    $expectedCount = ($allow.dlls.PSObject.Properties.Value | ForEach-Object { $_.Count } | Measure-Object -Sum).Sum
    if ($count -ne $expectedCount -or $seen.Count -ne @($allow.dlls.PSObject.Properties).Count) { throw 'Incomplete import inspection' }
}
# Static parser negatives, never launch the inspected executable.
foreach ($fixture in @(@('    unknown.dll'), @('    KERNEL32.dll','  1 UnknownApi'), @('delay load imports'))) {
    $rejected = $false
    try { Assert-Imports $fixture } catch { $rejected = $true }
    if (!$rejected) { throw 'Negative import fixture accepted' }
}
$imports = & $Dumpbin @('/imports', $exe)
if ($LASTEXITCODE) { throw 'Import inspection failed' }
Assert-Imports $imports
$sourceHashes = [ordered]@{}
foreach ($property in $old.sourceSha256.PSObject.Properties) {
    $file = $property.Name
    $hash = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot $file)).Hash
    if ($file -notin @('lab-protected-file-guest-README.md','lab-protected-file-guest-check.ps1') -and $hash -cne $property.Value) { throw "Behavior/build source changed: $file" }
    $sourceHashes[$file] = $hash
}
$sourceHashes['lab-protected-file-guest-imports.json'] = (Get-FileHash -LiteralPath (Join-Path $PSScriptRoot 'lab-protected-file-guest-imports.json')).Hash
$manifest = [ordered]@{
    provisional = $false; scope = 'x64-only'; completeArchitectureMatrix = $false
    architectureLimit = 'ARM64 unbuilt for this bundle; no full release matrix claim'
    executableSha256 = @{ x64 = $expected }; appLocalDlls = @(); sourceSha256 = $sourceHashes
    unchangedBehaviorSources = @('CMakeLists.txt','lab-protected-file-guest.cpp','lab-protected-file.cpp','lab-protected-file.h')
    preparationOnlySources = @('lab-protected-file-guest-README.md','lab-protected-file-guest-check.ps1','lab-protected-file-guest-imports.json')
    fixedGuestPaths = @('%ProgramFiles%\BimeronProtectedFileLab','%ProgramFiles%\BimeronProtectedFileLab\lab-protected-file-guest.exe','%ProgramFiles%\BimeronProtectedFileLab\LABProtectedFiles','%ProgramFiles%\BimeronProtectedFileLab\LABProtectedFiles\pending.bin')
    expectedState = 'Pending'; expectedCounter = 1; automaticCleanup = $false
    command = '& "$env:ProgramFiles\BimeronProtectedFileLab\lab-protected-file-guest.exe" --guest-pending-file-once --lab-only DisposableWindowsVM'
    executionCount = 1
    refusalPolicy = 'Preserve existing and partial bytes; stop on failure; no automatic retry, repair or cleanup'
    remainingGates = @('Independent final review','Explicit user approval of this concrete bundle','Disposable Windows VM isolation and protected ACL provisioning')
    evidenceLimit = 'Static preparation only; historical host argument/path refusals were not rerun; direct imports do not establish dynamic-loading absence'
}
New-Item -ItemType Directory (Join-Path $bundle 'x64') | Out-Null
Copy-Item -LiteralPath $exe -Destination (Join-Path $bundle 'x64/lab-protected-file-guest.exe')
$path = Join-Path $bundle 'manifest.json'
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Encoding utf8 -LiteralPath $path
Write-Output "Executable SHA256: $expected"
Write-Output "Manifest: $path"
Write-Output "Manifest SHA256: $((Get-FileHash -LiteralPath $path).Hash)"
