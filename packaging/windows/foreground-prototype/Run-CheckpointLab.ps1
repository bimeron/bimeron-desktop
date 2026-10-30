#requires -Version 5.1
# LAB ONLY: strict deadline blocker fallback; never a guest execution runner.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][ValidateSet('DisposableWindowsVM')][string]$LabOnlyAllow,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedRunnerSha256,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedManifestSha256,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedMsiSha256
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Get-CheckpointRunnerRefusal {
 # Correct input pins cannot satisfy the strict deadline; no validation claim.
 [pscustomobject][ordered]@{
  outcome = 'BlockedStrictDeadline'
  reason = 'No atomic deadline-aware TerminateProcess API; pre-call preemption permits a late call.'
  guestReady = $false
  inputsValidated = $false
  launchCount = 0
  terminationCount = 0
  initiatingClient = 'Unknown'
  clean = 'Unknown'
  serverQuiescence = 'Unknown'
  possibleLateOperation = 'Unknown'
 }
}
Get-CheckpointRunnerRefusal | ConvertTo-Json -Compress
exit 1
