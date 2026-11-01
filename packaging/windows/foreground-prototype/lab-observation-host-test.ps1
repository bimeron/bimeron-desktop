#requires -Version 5.1
$ErrorActionPreference='Stop'
. "$PSScriptRoot/Checkpoint-ObservationHost.ps1"
$count=0
function Assert($Condition) { if (-not $Condition) { throw 'Assertion failed' }; $script:count++ }
foreach($mode in @('success','refusal','partial','pre-loss','post-loss','unresolved','prepare-error','ready-error','intent-error')) {
 $s=New-ObservationState 'experiment:hosthash:confighash:pid:creation'
 $s.CallbackFailed=$false; $s.Calls=0; $s.Residency=0; $s.Frames=New-Object System.Collections.ArrayList
 $s.Owners.Add((New-Object object)) | Out-Null
 $owner=$s.Owners[0]; $runspace=[runspace]::DefaultRunspace.InstanceId; $hostPid=$PID
 $prepare={param($x) if($mode -eq 'prepare-error'){throw 'sink'} }
 $ready={if($mode -eq 'ready-error'){throw 'ready'} }
 $dispatch={if($mode -eq 'pre-loss'){throw 'transport'}; if($mode -eq 'partial'){return 'Dispatch'}; "Dispatch`t$($s.Binding)`n"}
 $intent={if($mode -eq 'intent-error'){throw 'intent'} }
 $runner={param($x)
  $x.Calls++
  if($PID -ne $hostPid -or [runspace]::DefaultRunspace.InstanceId -ne $runspace){$x.CallbackFailed=$true;return}
  if($mode -eq 'refusal'){throw 'refused'}
  foreach($stream in @('success','error','warning','verbose','debug','information','status')) {
   Write-ObservationFrame $x $stream "exact`nUnicode: $([char]0x03bb)" {param($name,$frame) $s.Frames.Add($frame)} {throw 'status lost'}
  }
  if($mode -eq 'post-loss'){Write-ObservationFrame $x success 'lost' {throw 'sink lost'} {throw 'status lost'} }
 }
 $resident={param($x) $x.Residency++}
 Invoke-ObservationCycle $s $prepare $ready $dispatch $intent $runner {} $resident
 Assert ($s.Residency -eq 1 -and [object]::ReferenceEquals($owner,$s.Owners[0]))
 Assert ($s.Calls -eq $(if($mode -in @('partial','pre-loss','prepare-error','ready-error','intent-error')){0}else{1}))
 Assert ($s.Frames.Count -eq $(if($s.Calls -eq 1 -and $mode -ne 'refusal'){7}else{0}))
 Assert (-not $s.CallbackFailed)
 Assert ($s.Failed -eq ($mode -eq 'post-loss'))
 $before=$s.Calls
 Invoke-ObservationCycle $s $prepare $ready $dispatch $intent $runner {} $resident
 Assert ($s.Calls -eq $before -and $s.Residency -eq 2)
 $sequence=0
 foreach($frame in $s.Frames) {
  $sequence++
  $parts=$frame.TrimEnd("`n").Split("`t")
  Assert ($parts.Count -eq 3)
  Assert ($parts[0] -ceq [string]$sequence)
  Assert ([Text.Encoding]::UTF8.GetString([Convert]::FromBase64String($parts[2])) -ceq "exact`nUnicode: $([char]0x03bb)")
  Assert ([Convert]::FromBase64String($parts[2]).Length -eq [int]$parts[1])
 }
}
foreach($record in @("Dispatch`tx`nDispatch`tx`n",'Dispatch',"Dispatch`ty`n")) {
 $refused=$false; try {Read-ObservationRecord $record Dispatch x} catch {$refused=$true}; Assert $refused
}
$s=New-ObservationState x
Write-ObservationFrame $s success ('x'*65537) {throw 'must not append'} {throw 'lost'}
Assert $s.Failed
"PASS: $count offline assertions; synthetic control flow only; no native operation."

# Owned temporary protected-leaf I/O fixtures; not protected-root acceptance.
. "$PSScriptRoot/ObservationHost-Native.ps1"
$temp=Join-Path ([IO.Path]::GetTempPath()) ('observation-host-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($temp)
$owners=New-Object Collections.ArrayList
# Reconstruct canonical LF Git-source bytes; the strict frozen hash still gates evaluation.
Import-ObservationPinTypes ([Text.Encoding]::UTF8.GetBytes([IO.File]::ReadAllText((Join-Path $PSScriptRoot 'Run-CheckpointLab.ps1')).Replace("`r`n","`n")))
try {
 $sinks=Open-ObservationSinks $temp $owners
 $state=New-ObservationState fixture
 $append={param($stream,$frame) $sinks[$stream].Append($frame)}
 foreach($stream in @('success','error','warning','verbose','debug','information','status')) {
  Write-ObservationFrame $state $stream "Unicode $([char]0x03bb)`n" $append {throw 'status lost'}
  Assert (-not $state.Failed)
  $readerStream=[IO.File]::Open((Join-Path $temp ($stream+'.frames')),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
  $reader=New-Object IO.StreamReader $readerStream
  try {$lines=$reader.ReadToEnd().TrimEnd("`n").Split("`n")}finally{$reader.Dispose()}
  Assert ($lines.Count -eq 3)
  Assert ($lines[2].StartsWith(([string]$state.Sequence)+"`t"))
  $path=Join-Path $temp ($stream+'.frames')
  $acl=Get-Acl -LiteralPath $path
  $raw=New-Object Security.AccessControl.RawSecurityDescriptor ($acl.Sddl)
  Assert (($raw.ControlFlags -band [Security.AccessControl.ControlFlags]::DiscretionaryAclProtected) -ne 0)
  Assert ($raw.Owner.Value -ceq 'S-1-5-32-544' -and $raw.Group.Value -ceq 'S-1-5-32-544')
  Assert ($raw.DiscretionaryAcl.Count -eq 2)
  foreach($ace in $raw.DiscretionaryAcl){
   Assert ($ace.AceQualifier -eq [Security.AccessControl.AceQualifier]::AccessAllowed -and $ace.AceFlags -eq 0 -and $ace.AccessMask -eq 0x1f01ff -and $ace.SecurityIdentifier.Value -in @('S-1-5-18','S-1-5-32-544'))
  }
  [CheckpointNative]::ValidateDescriptor($raw,[CheckpointPinRole]::LabProtected)
  $alias=[CheckpointPin]::LogAlias([CheckpointNative]::VolumePath($path))
  try {$alias.Revalidate();Assert ($null -ne $alias)} finally {$alias.Dispose()}
  $before=$lines -join "`n"
  $refused=$false;try{[ObservationDurableSink]::new($path).Dispose()}catch{$refused=$true};Assert $refused
  $verifyStream=[IO.File]::Open($path,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
  $verify=New-Object IO.StreamReader $verifyStream
  try {Assert (($verify.ReadToEnd().TrimEnd("`n").Split("`n") -join "`n") -ceq $before)}finally{$verify.Dispose()}
 }
 $refused=$false;try {Open-ObservationSinks $temp $owners | Out-Null}catch{$refused=$true};Assert $refused
 foreach($sddl in @('O:BAG:BAD:(A;;FA;;;SY)(A;;FA;;;BA)','O:BAG:BAD:P(A;;FA;;;WD)')){
  $raw=New-Object Security.AccessControl.RawSecurityDescriptor $sddl
  $refused=$false;try{[CheckpointNative]::ValidateDescriptor($raw,[CheckpointPinRole]::LabProtected)}catch{$refused=$true};Assert $refused
 }
 $closedPath=Join-Path $temp 'closed.frames'
 $closed=[ObservationDurableSink]::new($closedPath)
 try {$closed.Append("preserved`n")}finally{$closed.Dispose()}
 $refused=$false;try{[ObservationDurableSink]::new($closedPath).Dispose()}catch{$refused=$true};Assert $refused
 Assert ([IO.File]::ReadAllText($closedPath) -ceq "preserved`n")
 $recordState=New-ObservationState fixture
 try {
  New-ObservationRecord $recordState (Join-Path $temp 'ready.record') Ready
  Assert ($recordState.Owners.Count -eq 2)
  $recordState.Owners[1].Revalidate()
  Assert ((Get-Acl -LiteralPath (Join-Path $temp 'ready.record')).AreAccessRulesProtected)
 }finally{foreach($owner in $recordState.Owners){$owner.Dispose()}}
 $descriptor=ConvertTo-ObservationTaskSddl 'O:SYG:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)'
 $config="ObservationHost002`nboundary-xml-v1`n4e60cab47b730cda2e724af15d68cf1558464c231b5e13b4cf026b762af0387c`n$('a'*64)`n$('b'*64)`n$descriptor`n$descriptor`n"
 $bytes=[Text.Encoding]::UTF8.GetBytes($config)
 $parsed=Read-ObservationFixedConfig $bytes (Get-ObservationHash $bytes);Assert ($parsed.Experiment -ceq 'boundary-xml-v1')
 foreach($bad in @($config.Replace('002','003'),$config.Replace('4e60c','f700b'),($config+"`n"),$config.Replace("`n","`r`n"))) {
  $badBytes=[Text.Encoding]::UTF8.GetBytes($bad)
  $refused=$false;try{Read-ObservationFixedConfig $badBytes (Get-ObservationHash $badBytes)|Out-Null}catch{$refused=$true};Assert $refused
 }
 $refused=$false;try{Read-ObservationFixedConfig $bytes ('0'*64)|Out-Null}catch{$refused=$true};Assert $refused
 $refused=$false;try{Import-ObservationPinTypes $bytes}catch{$refused=$true};Assert $refused
 $refused=$false;try{Assert-ObservationTaskXml '<Task />' ('0'*64) x y z}catch{$refused=$true};Assert $refused
 $xml='<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task" />'
 $refused=$false;try{Assert-ObservationTaskXml $xml (Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($xml))) x y z}catch{$refused=$true};Assert $refused
} finally {
 foreach($owner in $owners){$owner.Dispose()}
 # Only this GUID-named, owned temporary fixture is removed.
 Remove-Item -LiteralPath $temp -Recurse -Force
}
"PASS: $count assertions including actual protected-leaf CREATE_NEW/Flush(true)/readback; no operation construction or launch."
Initialize-ObservationIdentityType
Assert ($null -ne ('ObservationToken' -as [type]))
Assert-ObservationTaskSecurity 'O:SYG:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)'
$count++
foreach($sd in @('O:BUG:SYD:P(A;;GA;;;SY)','O:SYG:SYD:(A;;GA;;;SY)','O:SYG:SYD:P(A;;GR;;;BU)','O:SYG:SYD:NO_ACCESS_CONTROL')){
 $refused=$false;try{Assert-ObservationTaskSecurity $sd}catch{$refused=$true};Assert $refused
}
"PASS: $count assertions; token interop compile only and task descriptor policy; no task/process/runner operation."

function Test-ObservationWrapper {
 param([string]$Fixture)
 $probe=New-ObservationState fixture
 $probe.Calls=0; $probe.Residency=0; $probe.CallbackFailed=$false
 $probe.Frames=New-Object Collections.ArrayList
 $probe.Streams=New-Object Collections.ArrayList
 $probe.ExpectedPid=$PID; $probe.ExpectedRunspace=[runspace]::DefaultRunspace.InstanceId
 [void]$probe.Owners.Add((New-Object object)); $held=$probe.Owners[0]
 $capture=@{};foreach($name in @('success','error','warning','verbose','debug','information','status')){
  $sink=New-Object psobject -Property @{Probe=$probe;Name=$name}
  $sink | Add-Member ScriptMethod Append {param($frame) [void]$this.Probe.Frames.Add($frame);[void]$this.Probe.Streams.Add($this.Name)}
  $capture[$name]=$sink
 }
 $context=New-ObservationRunnerContext $Fixture $capture
 $immutable=$false;try{$context['Path']='invalid'}catch{$immutable=$true};Assert $immutable
 Invoke-ObservationCycle $probe {} {} {"Dispatch`tfixture`n"} {} ${function:Invoke-ObservationRunnerCallback} {} {param($x)$x.Residency++} $context
 Assert ($probe.Calls -eq 1)
 Assert (-not $probe.CallbackFailed -and -not $probe.Failed)
 Assert ($probe.Residency -eq 1 -and $probe.Phase -ceq 'Returned')
 Assert ([object]::ReferenceEquals($held,$probe.Owners[0]))
 Assert ($probe.Frames.Count -eq 6)
 Assert (($probe.Streams -join ',') -ceq 'success,error,warning,verbose,debug,information')
 $expected=@('fixture-success','fixture-error','fixture-warning','fixture-verbose','fixture-debug','fixture-information')
 for($i=0;$i -lt 6;$i++){
  $parts=$probe.Frames[$i].TrimEnd("`n").Split("`t")
  Assert ($parts.Count -eq 3 -and $parts[0] -ceq [string]($i+1))
  $decoded=[Convert]::FromBase64String($parts[2])
  Assert ($decoded.Length -eq [int]$parts[1])
  Assert ([Text.Encoding]::UTF8.GetString($decoded) -ceq $expected[$i])
 }
}
$fixtureRoot=Join-Path ([IO.Path]::GetTempPath()) ('observation-wrapper-'+[guid]::NewGuid().ToString('N'))
[void][IO.Directory]::CreateDirectory($fixtureRoot)
try {
 $fixture=Join-Path $fixtureRoot 'inert.ps1'
 [IO.File]::WriteAllText($fixture,@'
param($LabOnlyAllow,$ExpectedRunnerSha256,$ExpectedManifestSha256,$ExpectedMsiSha256)
$probe.Calls++
if($PID -ne $probe.ExpectedPid -or [runspace]::DefaultRunspace.InstanceId -ne $probe.ExpectedRunspace -or
 $LabOnlyAllow -cne 'DisposableWindowsVM' -or
 $ExpectedRunnerSha256 -cne '4e60cab47b730cda2e724af15d68cf1558464c231b5e13b4cf026b762af0387c' -or
 $ExpectedManifestSha256 -cne '0972de05644b5ac2a5210be9ad35efb596545dc1634a051b052f2dc0f4695330' -or
 $ExpectedMsiSha256 -cne '860949118c7ac1ed93376e91576735ee7de2a67f924f0be323c172cf272c355a'){$probe.CallbackFailed=$true}
'fixture-success'
Write-Error 'fixture-error' -ErrorAction Continue
Write-Warning 'fixture-warning'
Write-Verbose 'fixture-verbose' -Verbose
$DebugPreference='Continue'; Write-Debug 'fixture-debug'
Write-Information 'fixture-information' -InformationAction Continue
'@)
 Test-ObservationWrapper $fixture
 $originalCycle=${function:Invoke-ObservationCycle}.ToString()
 $originalCallback=${function:Invoke-ObservationRunnerCallback}.ToString()
 foreach($mutant in @('delete-invocation','substitute-path')){
  try {
   if($mutant -eq 'delete-invocation'){
    $changed=$originalCycle.Replace('& $Runner $State $RunnerContext | Out-Null','# invocation deleted')
    Assert ($changed -cne $originalCycle)
    Set-Item function:Invoke-ObservationCycle ([scriptblock]::Create($changed))
   }else{
    $changed=$originalCallback.Replace("$"+"Context['Path']", "'missing-inert-fixture.ps1'")
    Assert ($changed -cne $originalCallback)
    Set-Item function:Invoke-ObservationRunnerCallback ([scriptblock]::Create($changed))
   }
   $detected=$false;try{Test-ObservationWrapper $fixture}catch{$detected=$true}
   Assert $detected
   "MUTANT DETECTED: $mutant SHA256=$(Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($changed)))"
  }finally{
   Set-Item function:Invoke-ObservationCycle ([scriptblock]::Create($originalCycle))
   Set-Item function:Invoke-ObservationRunnerCallback ([scriptblock]::Create($originalCallback))
  }
 }
}finally{
 $resolved=[IO.Path]::GetFullPath($fixtureRoot)
 if((Split-Path $resolved) -cne ([IO.Path]::GetTempPath().TrimEnd('\')) -or (Split-Path $resolved -Leaf) -notmatch '^observation-wrapper-[0-9a-f]{32}$'){throw 'Fixture cleanup boundary'}
 Remove-Item -LiteralPath $resolved -Recurse -Force
}
"PASS: $count assertions including exact wrapper and two detected in-memory mutants."


# Readback adapters only: no Task API. Check initial and predispatch helper calls.
$canonical=ConvertTo-ObservationTaskSddl 'O:SYG:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)'
$descriptorConfig=[pscustomobject]@{FolderSddl=$canonical;TaskSddl=$canonical}
$folderFixture=New-Object psobject -Property @{Sddl=$canonical;Reads=0}
$taskFixture=New-Object psobject -Property @{Sddl=$canonical;Reads=0}
foreach($adapter in @($folderFixture,$taskFixture)){
 $adapter | Add-Member ScriptMethod GetSecurityDescriptor {param($flags) if($flags -ne 7){throw 'flags'};$this.Reads++;$this.Sddl}
}
Assert-ObservationTaskDescriptors $folderFixture $taskFixture $descriptorConfig
Assert-ObservationTaskDescriptors $folderFixture $taskFixture $descriptorConfig
Assert ($folderFixture.Reads -eq 2 -and $taskFixture.Reads -eq 2)
foreach($adapter in @($folderFixture,$taskFixture)){
 foreach($changed in @('O:BAG:SYD:P(A;;GA;;;SY)(A;;GA;;;BA)','O:SYG:BAD:P(A;;GA;;;SY)(A;;GA;;;BA)','O:SYG:SYD:P(A;;GR;;;SY)(A;;GA;;;BA)')){
  $adapter.Sddl=$changed
  $refused=$false;try{Assert-ObservationTaskDescriptors $folderFixture $taskFixture $descriptorConfig}catch{$refused=$true};Assert $refused
 }
 $adapter.Sddl=$canonical
}
"PASS: $count total assertions; both descriptor readbacks and exact owner/group/DACL mismatch refusal."

foreach($file in @('Checkpoint-ObservationHost.ps1','ObservationHost-Native.ps1','lab-observation-host-test.ps1')){
 $tokens=$null;$errors=$null
 [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $file),[ref]$tokens,[ref]$errors)
 Assert ($errors.Count -eq 0)
}
$nativeSource=[IO.File]::ReadAllText((Join-Path $PSScriptRoot 'ObservationHost-Native.ps1'))
Assert ($nativeSource.Contains('$invoke=${function:Invoke-ObservationRunnerCallback}'))
Assert ($nativeSource.Contains('${function:Wait-ObservationResident} $runnerContext'))
Assert ($nativeSource.Contains('Assert-ObservationTaskDescriptors $folder $task $Config'))
Assert ($nativeSource.Contains('Assert-ObservationTaskDescriptors $registered.Folder $registered.Task $config'))
Assert ((Get-ObservationHash ([IO.File]::ReadAllBytes((Join-Path $PSScriptRoot 'ObservationHost-Native.ps1')))) -ceq $script:ObservationSupportSha256)
"PASS: $count final assertions including parser, production wiring and embedded hash binding."

# Return status follows invocation and precedes residency, even on sink failure.
foreach($failReturnSink in @($false,$true)) {
 $returnState=New-ObservationState return-fixture
 $returnState.Order=New-Object Collections.ArrayList
 $returnState.Owners.Add((New-Object object)) | Out-Null
 $retained=$returnState.Owners[0]
 $returnStatus={param($message)
  $returnState.Order.Add($message) | Out-Null
  if($failReturnSink){throw 'owned synthetic sink failure'}
 }
 Invoke-ObservationCycle $returnState {} {} {"Dispatch`treturn-fixture`n"} {} `
  {param($x) $x.Order.Add('Invoked') | Out-Null} $returnStatus `
  {param($x) $x.Order.Add('Resident') | Out-Null}
 Assert (($returnState.Order -join '|') -ceq 'Invoked|RunnerReturned; completion and server state unknown|Resident')
 Assert ($returnState.Phase -ceq 'Returned')
 Assert ($returnState.Failed -eq $failReturnSink)
 Assert ([object]::ReferenceEquals($retained,$returnState.Owners[0]))
}
"PASS: $count assertions including durable-return adapter ordering and sink-failure retention."

# Exact experimental action acceptance; XML hashes are recomputed for semantic negatives.
$root='C:\Program Files\BimeronCheckpointObservationBoundaryXml'
$image='C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe'
$hostPath="$root\Checkpoint-ObservationHost.ps1"
$xml=@"
<Task version="1.3" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"><Triggers/><Principals><Principal id="System"><UserId>S-1-5-18</UserId><RunLevel>HighestAvailable</RunLevel></Principal></Principals><Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><AllowHardTerminate>false</AllowHardTerminate><StartWhenAvailable>false</StartWhenAvailable><RunOnlyIfNetworkAvailable>false</RunOnlyIfNetworkAvailable><IdleSettings><StopOnIdleEnd>false</StopOnIdleEnd><RestartOnIdle>false</RestartOnIdle></IdleSettings><AllowStartOnDemand>true</AllowStartOnDemand><Enabled>true</Enabled><Hidden>false</Hidden><RunOnlyIfIdle>false</RunOnlyIfIdle><WakeToRun>false</WakeToRun><ExecutionTimeLimit>PT0S</ExecutionTimeLimit><Priority>7</Priority></Settings><Actions Context="System"><Exec><Command>C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe</Command><Arguments>-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File &quot;$root\Checkpoint-ObservationHost.ps1&quot;</Arguments><WorkingDirectory>$root</WorkingDirectory></Exec></Actions></Task>
"@
Assert-ObservationTaskXml $xml (Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($xml))) $image $hostPath $root
$count++
foreach($args in @('-NoLogo -NoProfile -NonInteractive -File',
 '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy RemoteSigned -File',
 '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -Command',
 '-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File -Extra')){
 $bad=$xml.Replace('-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File',$args)
 $refused=$false;try{Assert-ObservationTaskXml $bad (Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($bad))) $image $hostPath $root}catch{$refused=$true};Assert $refused
}
$bad=$xml.Replace('Checkpoint-ObservationHost.ps1','arbitrary.ps1')
$refused=$false;try{Assert-ObservationTaskXml $bad (Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($bad))) $image "$root\arbitrary.ps1" $root}catch{$refused=$true};Assert $refused
Assert-ObservationExecutionPolicy Undefined Undefined
$count++
foreach($policy in @('Restricted','AllSigned','RemoteSigned','Unrestricted','Bypass','Default','', 'undefined')){
 foreach($scope in @('MachinePolicy','UserPolicy')){
  $m='Undefined';$u='Undefined';if($scope -ceq 'MachinePolicy'){$m=$policy}else{$u=$policy}
  $refused=$false;try{Assert-ObservationExecutionPolicy $m $u}catch{$refused=$true};Assert $refused
 }
}
foreach($file in @('ObservationHost-Native.ps1','Checkpoint-ObservationHost.ps1')){
 $source=[IO.File]::ReadAllText((Join-Path $PSScriptRoot $file))
 Assert ($source -notmatch '(?i)Set-ExecutionPolicy|Set-ItemProperty|New-ItemProperty|RegistryKey|gpupdate')
}
"PASS: $count final assertions including exact process-only action and configured GPO refusal."


# Inject only native adapters into the exact production chain helper. No native pins.
$chainAst=[Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot 'ObservationHost-Native.ps1'),[ref]$null,[ref]$null)
$chainFn=$chainAst.Find({param($n)$n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Open-ObservationProtectedChain'},$true)
$injected=$chainFn.Extent.Text.Replace('Open-ObservationProtectedChain','Open-InjectedBoundaryChain').Replace('[CheckpointNative]::VolumePath($Path)','$Path').Replace('[CheckpointNative]::VolumePath($Anchor)','$Anchor').Replace('(Get-ObservationVolumePath $Path)','$Path').Replace('(Get-ObservationVolumePath $Anchor)','$Anchor').Replace('New-ObservationChainPin $part (-not $leaf -or $Directory.IsPresent) $role','New-InjectedBoundaryPin $part $role')
. ([scriptblock]::Create($injected))
function New-InjectedBoundaryPin($Part,$Role){
 $script:boundaryCalls.Add($Role) | Out-Null
 if($script:boundaryFailure -ceq 'constructor'){[void][int]::Parse('fail')}
 if($script:boundaryFailure -ceq 'descriptor' -and $Role -eq [CheckpointPinRole]::TrustedOS){
  [CheckpointNative]::ValidateDescriptor((New-Object Security.AccessControl.RawSecurityDescriptor 'O:SYG:SYD:(A;;FW;;;BU)'),[CheckpointPinRole]::TrustedOS)
 }
 [pscustomobject]@{Path=$Part;Role=$Role}
}
$volume='\\?\Volume{fixture}\';$anchor=$volume+'Program Files'
foreach($failure in @('none','constructor','descriptor')){
 $script:boundaryCalls=New-Object Collections.ArrayList;$script:boundaryFailure=$failure
 $held=New-Object object;$owned=New-Object Collections.ArrayList;[void]$owned.Add($held)
 $savedPreference=$ErrorActionPreference;$ErrorActionPreference='Continue';$caught=$false
 try{Open-InjectedBoundaryChain ($anchor+'\BimeronCheckpointClientLabBoundaryXml\leaf') $anchor $owned | Out-Null}catch{$caught=$true;Assert ($_.Exception.Message -match $(if($failure -ceq 'constructor'){'role=TraversalOnly'}else{'role=TrustedOS'}))}
 finally{$ErrorActionPreference=$savedPreference}
 Assert ([object]::ReferenceEquals($held,$owned[0]))
 if($failure -ceq 'none'){
  Assert (-not $caught)
  Assert (($script:boundaryCalls -join ',') -ceq 'TraversalOnly,TrustedOS,LabProtected,LabProtected')
  Assert ($owned.Count -eq 5)
 }else{Assert $caught;$expected=if($failure -ceq 'constructor'){1}else{2};Assert ($script:boundaryCalls.Count -eq $expected);Assert ($owned.Count -eq $expected)}
}
"PASS: $count assertions including injected first-boundary constructor/descriptor failure under Continue; owners retained."
# Protected binary staging uses the actual sink implementation on an owned temp fixture.
$stageFixture=Join-Path ([IO.Path]::GetTempPath()) ('boundary-stage-'+[guid]::NewGuid().ToString('N'))
[ObservationDurableSink]::NewDirectory($stageFixture)
$stagePath=Join-Path $stageFixture 'payload.bin'
$stageWriter=New-Object ObservationDurableSink $stagePath
try {
 $stageWriter.Write([byte[]](0,255,13,10,128))
 Assert ((Get-Acl -LiteralPath $stagePath).AreAccessRulesProtected)
 $reader=[IO.FileStream]::new($stagePath,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::ReadWrite)
 try{$actual=New-Object byte[] 5;Assert ($reader.Read($actual,0,5) -eq 5);Assert ([Convert]::ToBase64String($actual) -ceq 'AP8NCoA=')}finally{$reader.Dispose()}
 $refused=$false;try{$unexpected=New-Object ObservationDurableSink $stagePath}catch{$refused=$true};Assert $refused
 $refused=$false;try{[ObservationDurableSink]::NewDirectory($stageFixture)}catch{$refused=$true};Assert $refused
}finally{$stageWriter.Dispose()} # Only the inert owned test writer, never production owners.
"PASS: $count assertions including exact binary CREATE_NEW staging and existing-directory refusal; fixture retained."

# Real compiled selector and unmodified production chain; only native adapters mocked.
Assert ([CheckpointOperation]::ChainRole($volume,$anchor,[NullString]::Value) -eq [CheckpointPinRole]::TraversalOnly)
Assert ([CheckpointOperation]::ChainRole($anchor,$anchor,[NullString]::Value) -eq [CheckpointPinRole]::TrustedOS)
Assert ([CheckpointOperation]::ChainRole($anchor+'\image',$anchor,[NullString]::Value) -eq [CheckpointPinRole]::OSContent)
foreach($empty in @('', $null)){
 foreach($part in @($volume,$anchor,($anchor+'\BimeronCheckpointClientLabBoundaryXml'))){
  $pinCalls=0;$refused=$false
  try{$role=[CheckpointOperation]::ChainRole($part,$anchor,$empty);$pinCalls++;New-ObservationChainPin $part $true $role | Out-Null}catch{$refused=$true}
  Assert $refused;Assert ($pinCalls -eq 0)
 }
}
$savedVolume=${function:Get-ObservationVolumePath};$savedPin=${function:New-ObservationChainPin}
try {
 function Get-ObservationVolumePath([string]$Path){$Path}
 function New-ObservationChainPin($Path,$Directory,$Role){[pscustomobject]@{Path=$Path;Role=$Role}}
 foreach($case in @(@(($anchor+'\image'),'TraversalOnly,TrustedOS,OSContent'),@($anchor,'TraversalOnly,TrustedOS'))){
  $owners=New-Object Collections.ArrayList
  Open-ObservationProtectedChain $case[0] $anchor $owners -TrustedOS | Out-Null
  Assert (($owners.Role -join ',') -ceq $case[1])
 }
 $owners=New-Object Collections.ArrayList
 Open-ObservationProtectedChain ($anchor+'\BimeronCheckpointClientLabBoundaryXml\leaf') $anchor $owners | Out-Null
 Assert (($owners.Role -join ',') -ceq 'TraversalOnly,TrustedOS,LabProtected,LabProtected')
 $refused=$false;try{Open-ObservationProtectedChain ($anchor+'\Unowned') $anchor $owners | Out-Null}catch{$refused=$true};Assert $refused
}finally{${function:Get-ObservationVolumePath}=$savedVolume;${function:New-ObservationChainPin}=$savedPin}
"PASS: $count assertions including compiled PowerShell null-string binding and actual trusted/owned chain roles."

# COM-shaped principal injection only; no Schedule.Service creation.
function PrincipalFixture($user,$logon=5,$level=1){
 [pscustomobject]@{Definition=[pscustomobject]@{Principal=[pscustomobject]@{UserId=$user;LogonType=$logon;RunLevel=$level}}}
}
foreach($user in @('S-1-5-18','SYSTEM','NT AUTHORITY\SYSTEM')){
 Assert-ObservationTaskPrincipal (PrincipalFixture $user);$count++
 $normalized=$xml.Replace('<UserId>S-1-5-18</UserId>',('<UserId>'+$user+'</UserId>'))
 Assert-ObservationTaskXml $normalized (Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($normalized))) $image $hostPath $root;$count++
}
foreach($logon in @(0,1,2,3,4,6,7,-1,$null)){
 $refused=$false;try{Assert-ObservationTaskPrincipal (PrincipalFixture SYSTEM $logon)}catch{$refused=$true};Assert $refused
}
foreach($user in @('S-1-5-19','S-1-5-20','S-1-5-32-544','BimeronUnresolvablePrincipalXmlFixture','', ' SYSTEM ')){
 $refused=$false;try{Assert-ObservationTaskPrincipal (PrincipalFixture $user)}catch{$refused=$true};Assert $refused
}
foreach($level in @(0,2,$null)){
 $refused=$false;try{Assert-ObservationTaskPrincipal (PrincipalFixture SYSTEM 5 $level)}catch{$refused=$true};Assert $refused
}
$refused=$false;try{Assert-ObservationTaskPrincipal ([pscustomobject]@{Definition=[pscustomobject]@{Principal=[pscustomobject]@{UserId='SYSTEM'}}})}catch{$refused=$true};Assert $refused
foreach($token in @('ServiceAccount','S4U','Password','InteractiveToken','InteractiveTokenOrPassword','')){
 $bad=$xml.Replace('</UserId>',('</UserId><LogonType>'+$token+'</LogonType>'))
 $refused=$false;try{Assert-ObservationTaskXml $bad (Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($bad))) $image $hostPath $root}catch{$refused=$true};Assert $refused
}
foreach($bad in @($xml.Replace('S-1-5-18','S-1-5-19'),$xml.Replace('HighestAvailable','LeastPrivilege'),$xml.Replace('BoundaryXml','Boundary'))){
 $refused=$false;try{Assert-ObservationTaskXml $bad (Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($bad))) $image $hostPath $root}catch{$refused=$true};Assert $refused
}
# XML absence is checked with SelectNodes under StrictMode, never missing-property access.
$doc=[xml]$xml;$ns=[Xml.XmlNamespaceManager]::new($doc.NameTable);$ns.AddNamespace('t','http://schemas.microsoft.com/windows/2004/02/mit/task')
Assert ($doc.SelectNodes('/t:Task/t:Principals/t:Principal/t:LogonType',$ns).Count -eq 0)
"PASS: $count assertions including XML omission, resolved SYSTEM identity and independent COM principal refusal."
