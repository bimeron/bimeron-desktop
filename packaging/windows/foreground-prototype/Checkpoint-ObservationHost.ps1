#requires -Version 5.1
# Disconnected experiment host. No provisioning or task lifecycle operations.
$script:ObservationSupportSha256='769c041245f5026f5861af41408baed402d7dca86b32985953939a1cb87162e8'
function New-ObservationState {
 param([string]$Binding)
 if ([string]::IsNullOrWhiteSpace($Binding)) { throw 'Binding required' }
 @{ Binding=$Binding; Sequence=0L; Bytes=0L; Started=$false; Dispatched=$false;
    Failed=$false; Owners=(New-Object System.Collections.ArrayList); Phase='New' }
}
function Write-ObservationFrame {
 param($State,[string]$Stream,[string]$Value,[scriptblock]$Append,[scriptblock]$Status)
 # Never throw into the runner pipeline, including when independent status fails.
 try {
  if ($Stream -cnotin @('success','error','warning','verbose','debug','information','status')) { throw 'Stream refused' }
  $utf8=New-Object System.Text.UTF8Encoding($false,$true)
  $bytes=$utf8.GetBytes($Value)
  if ($bytes.Length -gt 65536 -or $State.Bytes+$bytes.Length -gt 8388608) { throw 'Capture bound exceeded' }
  $State.Sequence++; $State.Bytes+=$bytes.Length
  $frame=('{0}' -f $State.Sequence)+"`t"+$bytes.Length+"`t"+[Convert]::ToBase64String($bytes)+"`n"
  # Adapter must append, Flush(true), and exact-readback before returning.
  & $Append $Stream $frame | Out-Null
 } catch {
  $State.Failed=$true
  try { & $Status 'CaptureUnavailable' | Out-Null } catch { }
 }
}
function Read-ObservationRecord {
 param([string]$Record,[string]$Kind,[string]$Binding)
 # One complete record only, with no whitespace normalization or extra fields.
 $expected=$Kind+"`t"+$Binding+"`n"
 if ($Record -cne $expected) { throw 'Partial, duplicate or foreign record refused' }
}
function Invoke-ObservationCycle {
 param($State,[scriptblock]$Prepare,[scriptblock]$Ready,[scriptblock]$Dispatch,
       [scriptblock]$Intent,[scriptblock]$Runner,[scriptblock]$Status,
       [scriptblock]$Resident,$RunnerContext)
 # All callbacks are trusted offline fixtures, never data from a config file.
 try {
  if ($State.Started) { throw 'Duplicate start refused' }
  $State.Started=$true
  & $Prepare $State | Out-Null
  if ($State.Failed) { throw 'Sink preparation failed' }
  $State.Phase='Ready'; & $Ready $State.Binding | Out-Null
  $record=& $Dispatch
  Read-ObservationRecord $record 'Dispatch' $State.Binding
  $State.Dispatched=$true
  & $Intent $State.Binding | Out-Null
  $State.Phase='Invoking'
  # Direct call in this runspace. No process, job, operation constructor or disposal.
  & $Runner $State $RunnerContext | Out-Null
  $State.Phase='Returned'
  try { & $Status 'RunnerReturned; completion and server state unknown' | Out-Null }
  catch { $State.Failed=$true }
 } catch {
  $State.Phase='Unavailable'
  try { & $Status 'Unavailable; completion and server state unknown' | Out-Null } catch { }
 }
 # Only tests inject a returning resident. Production never releases owners.
 & $Resident $State
}
function Wait-ObservationResident {
 param($State)
 $script:CheckpointObservationOwner=$State
 while ($true) { try { Start-Sleep -Seconds 30 } catch { } }
}
function New-ObservationRunnerContext {
 param([string]$Path,$Sinks)
 # ReadOnlyDictionary prevents replacement of the frozen path or sink reference.
 $values=New-Object 'System.Collections.Generic.Dictionary[string,object]'
 $values.Add('Path',$Path); $values.Add('Sinks',$Sinks)
 New-Object 'System.Collections.ObjectModel.ReadOnlyDictionary[string,object]' (,$values)
}
function Invoke-ObservationRunnerCallback {
 param($State,$Context)
 Invoke-ObservationCapturedRunner $State $Context['Path'] $Context['Sinks']
}
function Invoke-ObservationCapturedRunner {
 param($State,[string]$RunnerPath,$Sinks)
 $append={param($name,$frame) $Sinks[$name].Append($frame)}
 $status={param($message) $Sinks.status.Append("0`t"+[Text.Encoding]::UTF8.GetByteCount($message)+"`t"+[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($message))+"`n")}
 # Preserve the exact script invocation and runspace. Stream conversion is bounded capture only.
 & $RunnerPath -LabOnlyAllow DisposableWindowsVM -ExpectedRunnerSha256 'f773983c67c06e79ff422350d64324fa9a1e6ca6b3e1bd7b326e5a02d68765cc' -ExpectedManifestSha256 '0972de05644b5ac2a5210be9ad35efb596545dc1634a051b052f2dc0f4695330' -ExpectedMsiSha256 '860949118c7ac1ed93376e91576735ee7de2a67f924f0be323c172cf272c355a' *>&1 | ForEach-Object {
  try {
   $name=if($_ -is [Management.Automation.ErrorRecord]){'error'}elseif($_ -is [Management.Automation.WarningRecord]){'warning'}elseif($_ -is [Management.Automation.VerboseRecord]){'verbose'}elseif($_ -is [Management.Automation.DebugRecord]){'debug'}elseif($_ -is [Management.Automation.InformationRecord]){'information'}else{'success'}
   Write-ObservationFrame $State $name ([string]$_) $append $status
  } catch {$State.Failed=$true;try{& $status 'CaptureUnavailable'}catch{}}
 }
}

# Entry is last: all functions exist before the nonreturning residency path.
if ($MyInvocation.InvocationName -ne '.') {
 $ErrorActionPreference='Stop'
 $state=New-ObservationState 'Unverified'
 $script:CheckpointObservationOwner=$state
 try {
  $supportBytes=[IO.File]::ReadAllBytes("$PSScriptRoot/ObservationHost-Native.ps1")
  $sha=[Security.Cryptography.SHA256]::Create()
  try {$digest=([BitConverter]::ToString($sha.ComputeHash($supportBytes))).Replace('-','').ToLowerInvariant()}finally{$sha.Dispose()}
  if($digest -cne $script:ObservationSupportSha256){throw 'Frozen support digest refused'}
  . "$PSScriptRoot/ObservationHost-Native.ps1"
  Start-ObservationHost $state $PSCommandPath
 } catch {
  try {[Console]::Error.WriteLine($_.Exception.Message)}catch{}
  Wait-ObservationResident $state
 }
}
