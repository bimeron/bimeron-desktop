param([Parameter(Mandatory = $true)][string]$Executable)

$ErrorActionPreference = 'Stop'
$targetPath = [IO.Path]::GetFullPath($Executable)
$processName = [IO.Path]::GetFileNameWithoutExtension($targetPath)
$localCorePath = Join-Path ([IO.Path]::GetDirectoryName($targetPath)) 'bimeron-core.exe'
foreach ($core in @(Get-Process -Name 'bimeron-core' -ErrorAction SilentlyContinue)) {
    if (-not $core.Path -or [IO.Path]::GetFullPath($core.Path) -eq $localCorePath) {
        throw 'A local Core may still be running. Return to the system backend and quit the Development GUI normally before rebuilding; no process was terminated.'
    }
}
foreach ($candidate in @(Get-Process -Name $processName -ErrorAction SilentlyContinue)) {
    if ($candidate.Path -and [IO.Path]::GetFullPath($candidate.Path) -eq $targetPath) {
        Write-Host "Stopping build executable (PID $($candidate.Id)): $targetPath"
        Stop-Process -Id $candidate.Id -Force
        if (-not $candidate.WaitForExit(10000)) {
            throw "Build executable did not exit: $targetPath"
        }
    }
}
