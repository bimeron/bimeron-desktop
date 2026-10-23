param([Parameter(Mandatory = $true)][string]$Executable)

$ErrorActionPreference = 'Stop'
$targetPath = [IO.Path]::GetFullPath($Executable)
$processName = [IO.Path]::GetFileNameWithoutExtension($targetPath)
foreach ($candidate in @(Get-Process -Name $processName -ErrorAction SilentlyContinue)) {
    if ($candidate.Path -and [IO.Path]::GetFullPath($candidate.Path) -eq $targetPath) {
        Write-Host "Stopping build executable (PID $($candidate.Id)): $targetPath"
        Stop-Process -Id $candidate.Id -Force
        if (-not $candidate.WaitForExit(10000)) {
            throw "Build executable did not exit: $targetPath"
        }
    }
}
