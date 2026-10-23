param([switch]$WindowsOnly)

$ErrorActionPreference = "Stop"

$sourceRoot = Split-Path -Parent $PSScriptRoot
$wixRoot = Join-Path $sourceRoot ".tools/wix"
$wixArchive = Join-Path $sourceRoot ".tools/wix.zip"
$distRoot = Join-Path $sourceRoot "dist"
$stageRoot = [System.IO.Path]::GetFullPath((Join-Path $sourceRoot "stage"))

if (-not $WindowsOnly) {
    if (-not (Get-Command wsl.exe -ErrorAction SilentlyContinue)) {
        throw "WSL is required for all four packages. Use -WindowsOnly for the two MSI packages."
    }
    wsl.exe -e bash -lc "true" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        throw "A working WSL Linux distribution is required for all four packages. Use -WindowsOnly for the two MSI packages."
    }
    $linuxRoot = (wsl.exe -e wslpath -a $sourceRoot).Trim()
    if ($LASTEXITCODE -ne 0 -or -not $linuxRoot) { throw "Cannot locate the source directory in WSL" }
}

$corePlatform = if ($WindowsOnly) { "windows" } else { "all" }
cmake "-DBIMERON_CORE_PLATFORM=$corePlatform" -P (Join-Path $PSScriptRoot "fetch-core.cmake")
if ($LASTEXITCODE -ne 0) { throw "Core download failed" }

New-Item -ItemType Directory -Force $distRoot | Out-Null
Get-ChildItem -LiteralPath $distRoot -File | Where-Object {
    $_.Name -like "bimeron-desktop-*" -and $_.Extension -in @(".msi", ".exe", ".deb")
} | Remove-Item -Force

if (-not (Test-Path (Join-Path $wixRoot "candle.exe"))) {
    New-Item -ItemType Directory -Force $wixRoot | Out-Null
    Invoke-WebRequest `
        -Uri "https://github.com/wixtoolset/wix3/releases/download/wix3141rtm/wix314-binaries.zip" `
        -OutFile $wixArchive
    Expand-Archive -LiteralPath $wixArchive -DestinationPath $wixRoot -Force
}

$env:PATH = "$wixRoot;$env:PATH"

foreach ($preset in @("windows-amd64-release", "windows-arm64-release")) {
    cmake --preset $preset -DBIMERON_REQUIRE_CORE=ON "-DBIMERON_CORE_ROOT=$(Join-Path $sourceRoot '.tools/core')"
    if ($LASTEXITCODE -ne 0) { throw "Configure failed: $preset" }
    cmake --build --preset $preset
    if ($LASTEXITCODE -ne 0) { throw "Build failed: $preset" }
    $stagePath = [System.IO.Path]::GetFullPath((Join-Path $stageRoot ($preset -replace '-release$','')))
    if (-not $stagePath.StartsWith("$stageRoot$([System.IO.Path]::DirectorySeparatorChar)", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Staging path is outside the project stage directory: $stagePath"
    }
    if (Test-Path -LiteralPath $stagePath) { Remove-Item -LiteralPath $stagePath -Recurse -Force }
    cmake --install (Join-Path $sourceRoot "build/$($preset -replace '-release$','')") --config Release
    if ($LASTEXITCODE -ne 0) { throw "Install failed: $preset" }
    cpack --preset $preset -C Release
    if ($LASTEXITCODE -ne 0) { throw "Packaging failed: $preset" }
}

if (-not $WindowsOnly) {
    wsl.exe -e bash "$linuxRoot/scripts/package-linux.sh"
    if ($LASTEXITCODE -ne 0) { throw "Linux packaging failed" }
}

$cpackTemp = Join-Path $sourceRoot "dist/_CPack_Packages"
if (Test-Path $cpackTemp) {
    Remove-Item -LiteralPath $cpackTemp -Recurse -Force
}

Get-ChildItem (Join-Path $sourceRoot "dist") -File | Select-Object Name, Length
