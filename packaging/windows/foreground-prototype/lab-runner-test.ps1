# Offline only: parse runner AST, extract pure helpers; never invoke its entry point.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$path=Join-Path $PSScriptRoot 'Run-Lab.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'Runner syntax failure.' }
foreach ($name in @('Get-LabTokenClass','Initialize-LabHelpers','Convert-LabJsonObject','Read-LabJson','Test-LabTraceComplete','Assert-JsonKeys','Assert-LabPath','Assert-Fields','Assert-Hash','Read-LabManifest','Read-LabTrace','Get-LabOutcome')) {
 $fn=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
 . ([scriptblock]::Create($fn.Extent.Text))
}
$count=0
function Reject([scriptblock]$Action) { $rejected=$false;try{& $Action | Out-Null}catch{$rejected=$true};if(-not $rejected){throw 'Expected refusal.'};$script:count++ }
Reject { Assert-JsonKeys '{"schema":1,"schema":1}' }
$manifest=[ordered]@{schema=1;architecture='amd64';traceSchema=1;variants=[ordered]@{}}
foreach($name in @('Success','CommitFail','PostFinalizeFail')) {
 $manifest.variants[$name]=[ordered]@{file="lab-trace-$name.msi";sha256=('a'*64);dllSha256=('b'*64);productCode=('{'+[guid]::NewGuid().ToString().ToUpperInvariant()+'}');packageCode=('{'+[guid]::NewGuid().ToString().ToUpperInvariant()+'}');upgradeCode=('{'+[guid]::NewGuid().ToString().ToUpperInvariant()+'}')}
}
$null=Read-LabManifest ($manifest|ConvertTo-Json -Depth 5)
$manifest.variants.Success.file='../other.msi';Reject {Read-LabManifest ($manifest|ConvertTo-Json -Depth 5)};$manifest.variants.Success.file='lab-trace-Success.msi'
$originalCode=$manifest.variants.Success.productCode
$manifest.variants.Success.productCode=$manifest.variants.CommitFail.productCode;Reject {Read-LabManifest ($manifest|ConvertTo-Json -Depth 5)}
$manifest.variants.Success.productCode=$originalCode
Reject { Assert-Hash ('a'*63) }
Reject { Assert-Hash 42 }
Reject { Assert-LabPath 'C:\Windows\System32' }
Reject { Assert-LabPath 'C:\Users\Public\Documents\BimeronInstallerLab\..\escape' }
Reject { Read-LabManifest '{"schema":"1","architecture":"amd64","traceSchema":1,"variants":{}}' }
Reject { Read-LabTrace 'BIMERON_LAB_TRACE_V1 {"callback":"shell"}' }
Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+('x'*2049)) }
Reject { Read-LabTrace 'BIMERON_LAB_TRACE_V1 {"nested":{}}' }
if ($null -ne (Read-LabTrace 'ordinary MSI line')) {throw 'Nontrace line accepted.'}
# Literal record transcribed from ccf2957a lab-trace.cpp's swprintf contract.
$wire='{"schema":1,"callback":"ObservationPreflight","utc":"2026-10-30T00:00:00.000Z","pid":1,"tid":2,"processCreated":"01dd000000000001","processSession":1,"tokenSource":2,"tokenType":1,"impersonation":-1,"elevated":1,"elevationType":2,"integrityRID":12288,"tokenSession":1,"adminEnabled":1,"adminDenyOnly":0,"restricted":0,"sidSHA256":"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa","logonId":"0000000000000001","status":0,"evidence":"observed","initiatingClient":"Unknown","intendedReturn":0}'
$trace=Read-LabTrace ('MSI (s) [00:00]: BIMERON_LAB_TRACE_V1 '+$wire)
foreach ($bad in @(
 $wire.Replace('"pid":1','"pid":"1"'),
 $wire.Replace('"pid":1','"pid":1.0'),
 $wire.Replace('"pid":1','"pid":4294967296'),
 $wire.Replace('"tokenSource":2','"tokenSource":0'),
 $wire.Replace('"impersonation":-1','"impersonation":0'),
 $wire.Replace('"adminDenyOnly":0','"adminDenyOnly":"0"'),
 $wire.Replace('"evidence":"observed"','"evidence":"complete"'),
 $wire.Replace('ObservationPreflight','Preflight'),
 $wire.Replace('"pid":1','"pid":1,"p\u0069d":2'),
 $wire.Replace('"pid":1','"PID":1'),
 $wire.Replace('2026-10-30','2026-02-30'),
 $wire.Replace('"schema":1','"extra":"secret","schema":1')
)) { Reject {Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+$bad)} }
$fallback=Read-LabTrace 'BIMERON_LAB_TRACE_V1 {"schema":1,"callback":"ObservationCommit","evidence":"incomplete","status":29,"initiatingClient":"Unknown","intendedReturn":0}'
if ($fallback.evidence -cne 'incomplete') {throw 'Fallback lost.'}
$fullIncomplete=Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+$wire.Replace('"evidence":"observed"','"evidence":"incomplete"').Replace('"status":0','"status":5').Replace('"tokenSource":2','"tokenSource":0').Replace('"sidSHA256":"'+('a'*64)+'"','"sidSHA256":"unknown"'))
$sequence=@('Preflight','DeferredBegin','Commit','PostFinalize') | ForEach-Object {Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+$wire.Replace('ObservationPreflight',('Observation'+$_)))}
if (-not (Test-LabTraceComplete $sequence 'Success')) {throw 'Complete source trace rejected.'}
if ((Test-LabTraceComplete @() 'Success') -or (Test-LabTraceComplete @($trace,$fallback,$fullIncomplete) 'Success') -or (Test-LabTraceComplete $sequence 'CommitFail')) {throw 'Missing/incomplete evidence accepted.'}
Reject { Assert-JsonKeys '{"schema":1,"sch\u0065ma":1}' }
Reject { Assert-JsonKeys '{"a":{"x":1,"X":2}}' }
Reject { Assert-JsonKeys '{"a":[{"x":1,"x":2}]}' }
Reject { Assert-JsonKeys ('{"a":'*10+'1'+('}'*10)) }
Reject { Assert-JsonKeys ('{"a":"'+('x'*16384)+'"}') }
$manifestJson=$manifest|ConvertTo-Json -Depth 5 -Compress
foreach ($bad in @($manifestJson.Replace('"schema":1','"schema":1.0'),$manifestJson.Replace('"schema":1','"schema":true'),$manifestJson.Replace('"schema":1','"schema":1,"sch\u0065ma":1'),$manifestJson.Replace('"schema":1','"schema":1,"unknown":1'))) { Reject {Read-LabManifest $bad} }
$withFallback=@($sequence);$withFallback[2]=$fallback
if (Test-LabTraceComplete $withFallback 'Success') {throw 'Fallback passed correct callback order.'}
$withIncomplete=@($sequence);$withIncomplete[0]=$fullIncomplete
if (Test-LabTraceComplete $withIncomplete 'Success') {throw 'Incomplete passed correct callback order.'}
Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+$wire.Replace('"impersonation":-1','"impersonation":-2')) }
Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+$wire.Replace('"tokenType":1','"tokenType":3')) }
Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+$wire.Replace('"elevated":1','"elevated":2')) }

if ((Get-LabOutcome 0 $true $true) -ne 'UnknownPending' -or (Get-LabOutcome $null $false $true) -ne 'UnknownPending' -or (Get-LabOutcome 0 $false $false) -ne 'IncompleteEvidence' -or (Get-LabOutcome 1603 $false $true) -ne 'ObservedOnly') { throw 'Outcome regression.' }
$context=[pscustomobject]@{elevated=$true;integrity=12288;enabledAdmin=$true;restricted=$false}
if ((Get-LabTokenClass $context) -cne 'Elevated') {throw 'Elevated context rejected.'}
$context.restricted=$true
if ((Get-LabTokenClass $context) -cne 'Unknown') {throw 'Restricted context promoted.'}
$context.restricted=$false;$context.enabledAdmin=$false
if ((Get-LabTokenClass $context) -cne 'Unknown') {throw 'Filtered context promoted.'}
$context.elevated=$false;$context.integrity=8192
if ((Get-LabTokenClass $context) -cne 'Unelevated') {throw 'Unelevated context rejected.'}
# Inspect parameter validation by constructing a harmless param-only scriptblock.
$parameterCheck=[scriptblock]::Create($ast.ParamBlock.Extent.Text + "`n 'accepted'")
$valid=@{LabOnlyAllow='DisposableWindowsVM';Case='Success';StartingToken='Unelevated';ExpectedManifestSha256=('a'*64);ExpectedRunnerSha256=('b'*64)}
$null=& $parameterCheck @valid
$valid.LabOnlyAllow='yes';Reject { & $parameterCheck @valid };$valid.LabOnlyAllow='DisposableWindowsVM'
$valid.Case='Arbitrary';Reject { & $parameterCheck @valid };$valid.Case='Success'
$valid.StartingToken='SYSTEM';Reject { & $parameterCheck @valid };$valid.StartingToken='Unelevated'
$valid.ExpectedRunnerSha256='bad';Reject { & $parameterCheck @valid }
# Compile both exact helper and read-only native declarations, never call token APIs.
foreach ($add in $ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -eq 'Add-Type'},$true)) {
 $source=$add.CommandElements[2].Value
 if ($source.Contains('public static class BimeronLabNative')) { Add-Type -TypeDefinition $source }
}
# Fake launcher blocks before returning: no executable, UAC or installer is used.
Add-Type -TypeDefinition @"
using System;using System.Diagnostics;using System.Threading;
public static class LabFakeLauncher {
 public static Func<Process> Delayed(int delay){return delegate{Thread.Sleep(delay);return null;};}
}
"@
$temp=[IO.Path]::GetTempFileName()
try {
 $pin=[IO.File]::Open($temp,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
 $worker=[BimeronLabLaunch]::new([LabFakeLauncher]::Delayed(500),[IDisposable[]]@($pin))
 $watch=[Diagnostics.Stopwatch]::StartNew();$worker.Start()
 if ($worker.Observe(30) -or $watch.ElapsedMilliseconds -gt 400) {throw 'Launch deadline not bounded.'}
 if (-not $pin.CanRead) {throw 'Late-launch pin released early.'}
 Reject { $exclusive=[IO.File]::Open($temp,[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::None);$exclusive.Dispose() }
 if ((Get-LabOutcome $null $true $false) -cne 'UnknownPending') {throw 'Timeout lost uncertainty.'}
 if (-not $worker.Observe(3000) -or -not $worker.Failed -or $pin.CanRead) {throw 'Worker completion/pin ownership failure.'}
 $exclusive=[IO.File]::Open($temp,[IO.FileMode]::Open,[IO.FileAccess]::Write,[IO.FileShare]::None);$exclusive.Dispose()
} finally { [IO.File]::Delete($temp) }
Write-Output "PASS: PowerShell $($PSVersionTable.PSVersion); AST, exact interop, canonical trace, bounded JSON, deadline/pins and $count negative checks. No runner dispatch, executable launch, token query, MSI or VM operations."
