# Offline only: parse runner AST, extract pure helpers; never invoke its entry point.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$path=Join-Path $PSScriptRoot 'Run-Lab.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'Runner syntax failure.' }
foreach ($name in @('Get-LabRoot','Compare-LabProcessIdentity','Get-LabTokenClass','Initialize-LabHelpers','Convert-LabJsonObject','Read-LabJson','Test-LabTraceComplete','Assert-JsonKeys','Assert-LabPath','Assert-Fields','Assert-Hash','Read-LabManifest','Read-LabTrace','Get-LabOutcome')) {
 $fn=$ast.Find({param($node) $node -is [Management.Automation.Language.FunctionDefinitionAst] -and $node.Name -eq $name},$true)
 . ([scriptblock]::Create($fn.Extent.Text))
}
$helperAdd=$ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -eq 'Add-Type'},$true) | Where-Object {$_.CommandElements[2].Value.Contains('public sealed class BimeronLabJson')}
$fakeSource=@"
public static class LabFakeLauncher {
 public static BimeronLabLaunch Worker(IDisposable[] pins,bool fail){return new BimeronLabLaunch(Returned(fail?0:100),pins,fail?(Func<Process,BimeronLabIdentity>)QueryFailure:Query,Wait);}
 public static int WaitCalls; public static bool Captured, WaitEntered; public static readonly ManualResetEvent Release=new ManualResetEvent(false);
 public static Func<Process> Returned(int delay){return delegate{Thread.Sleep(delay);return new Process();};}
 public static BimeronLabIdentity Query(Process p){Captured=true;return new BimeronLabIdentity(1,"01dd000000000001");}
 public static BimeronLabIdentity QueryFailure(Process p){throw new InvalidOperationException();}
 public static void Wait(Process p){Interlocked.Increment(ref WaitCalls);if(!Captured)throw new Exception("Wait before capture");WaitEntered=true;Release.WaitOne();throw new InvalidOperationException();}
 public static Func<Process> Delayed(int delay){return delegate{Thread.Sleep(delay);return null;};}
}
"@
Add-Type -TypeDefinition ($helperAdd.CommandElements[2].Value + "`n" + $fakeSource)
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
function Reject-LabPath([string]$Value) {
 $message='';try {$null=Assert-LabPath $Value} catch {$message=$_.Exception.Message}
 if ($message -cne 'Noncanonical lab path.') {throw 'Expected lexical path refusal before filesystem checks.'}
 $script:count++
}
Reject-LabPath 'C:\Windows\System32'
Reject-LabPath 'C:\Users\Public\Documents\BimeronInstallerLab'
$fixedRoot=Get-LabRoot
if ($fixedRoot -cne ([IO.Path]::Combine([BimeronLabLaunch]::ProgramFiles(),'BimeronTraceClientLab'))) {throw 'Fixed known-folder root mismatch.'}
Reject-LabPath ($fixedRoot+'\..\escape')
Reject-LabPath ($fixedRoot+'Other')
if (-not [BimeronLabLaunch]::SupportedArchitecture(8,0,0x8664)) {throw 'Native x64 refused.'}
foreach ($pair in @(@(4,0,0x8664),@(8,0x14c,0x8664),@(8,0x8664,0xaa64),@(8,0,0xaa64),@(8,0,0))) {
 Reject {if ([BimeronLabLaunch]::SupportedArchitecture($pair[0],$pair[1],$pair[2])) {return};throw 'Unsupported architecture.'}
}
# The closed parameter contract must not admit a caller-selected root.
if (@($ast.ParamBlock.Parameters.Name.VariablePath.UserPath) -contains 'Root') {throw 'Arbitrary root parameter.'}
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
# Exact native adapter queries only this already-running harness process handle.
$own=[Diagnostics.Process]::GetCurrentProcess()
try { $nativeIdentity=[BimeronLabLaunch]::Capture($own);if ($nativeIdentity.Pid -ne $PID -or $nativeIdentity.Created -cnotmatch '^[0-9a-f]{16}$') {throw 'Native handle identity failure.'} } finally {$own.Dispose()}
$identity=[BimeronLabIdentity]::new(1,'01dd000000000001')
if ((Compare-LabProcessIdentity $trace $identity) -cne 'Equal' -or (Compare-LabProcessIdentity $trace ([BimeronLabIdentity]::new(2,$identity.Created))) -cne 'Different' -or (Compare-LabProcessIdentity $trace ([BimeronLabIdentity]::new(1,'01dd000000000002'))) -cne 'Different' -or (Compare-LabProcessIdentity $fallback $identity) -cne 'Unknown' -or (Compare-LabProcessIdentity $trace $null) -cne 'Unknown') {throw 'Pair comparison failure.'}
$temp=[IO.Path]::GetTempFileName()
try {
 $pin=[IO.File]::OpenRead($temp)
 $worker=[LabFakeLauncher]::Worker([IDisposable[]]@($pin),$false)
 $worker.Start()
 if ($worker.Observe(10) -or $null -ne $worker.Identity -or -not $pin.CanRead) {throw 'Late launch retention failure.'}
 $deadline=[Diagnostics.Stopwatch]::StartNew()
 while (-not [LabFakeLauncher]::WaitEntered -and $deadline.ElapsedMilliseconds -lt 3000) {Start-Sleep -Milliseconds 10}
 if (-not [LabFakeLauncher]::WaitEntered -or $null -eq $worker.Identity -or $worker.Observe(10) -or -not $pin.CanRead) {throw 'Early capture/wait timeout retention failure.'}
} finally { [LabFakeLauncher]::Release.Set() | Out-Null; if (-not $worker.Observe(3000)) {throw 'Fake wait did not finish.'};[IO.File]::Delete($temp) }
$waitCalls=[LabFakeLauncher]::WaitCalls
$failedQuery=[LabFakeLauncher]::Worker([IDisposable[]]@(),$true)
$failedQuery.Start()
if (-not $failedQuery.Observe(3000) -or -not $failedQuery.IdentityFailed -or $null -ne $failedQuery.Identity -or [LabFakeLauncher]::WaitCalls -ne ($waitCalls+1)) {throw 'Query failure guessed identity.'}
Write-Output "PASS: PowerShell $($PSVersionTable.PSVersion); AST, exact interop, canonical trace, bounded JSON, deadline/pins and $count negative checks. No runner dispatch, executable launch, token query, MSI or VM operations; current harness handle queried read-only."
