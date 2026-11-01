#requires -Version 5.1
# Experiment-only support. Dot-source only; never launches a process or constructs an operation.
function Get-ObservationHash {
 param([byte[]]$Bytes)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 $sha=[Security.Cryptography.SHA256]::Create()
 try { ([BitConverter]::ToString($sha.ComputeHash($Bytes))).Replace('-','').ToLowerInvariant() } finally {$sha.Dispose()}
}
function Import-ObservationPinTypes {
 param([byte[]]$FrozenRunner)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 if ((Get-ObservationHash $FrozenRunner) -cne 'f773983c67c06e79ff422350d64324fa9a1e6ca6b3e1bd7b326e5a02d68765cc') {throw 'Frozen runner hash refused'}
 $text=(New-Object Text.UTF8Encoding($false,$true)).GetString($FrozenRunner)
 $tokens=$null; $errors=$null
 $ast=[Management.Automation.Language.Parser]::ParseInput($text,[ref]$tokens,[ref]$errors)
 if($errors.Count) {throw 'Runner parse refused'}
 $functions=@($ast.FindAll({param($a) $a -is [Management.Automation.Language.FunctionDefinitionAst] -and $a.Name -ceq 'Initialize-CheckpointLab'},$false))
 if($functions.Count -ne 1) {throw 'Exact initializer required'}
 # Exact hash precedes evaluation. Only the function definition executes, not runner top level.
 . ([scriptblock]::Create($functions[0].Extent.Text))
 Initialize-CheckpointLab
}
function Get-ObservationVolumePath {
 param([string]$Path)
 Set-StrictMode -Version Latest
 $ErrorActionPreference='Stop'
 [CheckpointNative]::VolumePath($Path)
}
function New-ObservationChainPin {
 param([string]$Path,[bool]$Directory,$Role)
 Set-StrictMode -Version Latest
 $ErrorActionPreference='Stop'
 [CheckpointPin]::new($Path,$Directory,$Role)
}
function Open-ObservationProtectedChain {
 param([string]$Path,[string]$Anchor,$Owners,[switch]$Directory,[switch]$TrustedOS)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 $pathVolume=(Get-ObservationVolumePath $Path)
 $anchorVolume=(Get-ObservationVolumePath $Anchor)
 if(-not ($pathVolume -ceq $anchorVolume -or $pathVolume.StartsWith($anchorVolume+'\',[StringComparison]::Ordinal))) {throw 'Outside fixed anchor'}
 $end=$pathVolume.IndexOf('}\',[StringComparison]::Ordinal)+2
 if($end -lt 3) {throw 'Volume path required'}
 while($true) {
  $part=$pathVolume.Substring(0,$end)
  $leaf=$end -eq $pathVolume.Length
  $owned=$null
  if(-not $TrustedOS){
   foreach($name in @('BimeronCheckpointClientLabBoundary','BimeronProtectedAckLab','BimeronCheckpointObservationBoundary')){
    $candidate=$anchorVolume+'\'+$name
    if($pathVolume -ceq $candidate -or $pathVolume.StartsWith($candidate+'\',[StringComparison]::Ordinal)){$owned=$candidate;break}
   }
   if($null -eq $owned){throw 'Fixed owned lab root required'}
  }
  $role=[CheckpointOperation]::ChainRole($part,$anchorVolume,$owned)
  try {$pin=New-ObservationChainPin $part (-not $leaf -or $Directory.IsPresent) $role}
  catch {throw [InvalidOperationException]::new(('Fixed chain pin refused: '+$part+' role='+$role),$_.Exception)}
  [void]$Owners.Add($pin)
  if($leaf){return $pin}
  $end=$pathVolume.IndexOf('\',$end+1); if($end -lt 0){$end=$pathVolume.Length}
 }
}
function Initialize-ObservationSinkType {
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 if('ObservationDurableSink' -as [type]) {return}
 Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
using System.ComponentModel;
using System.Runtime.InteropServices;
using Microsoft.Win32.SafeHandles;
public sealed class ObservationDurableSink : IDisposable {
 [StructLayout(LayoutKind.Sequential)]
 struct SecurityAttributes { public int Length; public IntPtr Descriptor; public int InheritHandle; }
 [DllImport("advapi32.dll", CharSet=CharSet.Unicode, ExactSpelling=true, SetLastError=true)]
 [return: MarshalAs(UnmanagedType.Bool)]
 static extern bool ConvertStringSecurityDescriptorToSecurityDescriptorW(string sddl,uint revision,out IntPtr descriptor,out uint size);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, ExactSpelling=true, SetLastError=true)]
 static extern SafeFileHandle CreateFileW(string path,uint access,uint share,ref SecurityAttributes security,uint disposition,uint flags,IntPtr template);
 [DllImport("kernel32.dll", ExactSpelling=true)] static extern IntPtr LocalFree(IntPtr memory);
 [DllImport("kernel32.dll", CharSet=CharSet.Unicode, ExactSpelling=true, SetLastError=true)]
 static extern bool CreateDirectoryW(string path,ref SecurityAttributes security);
 public static void NewDirectory(string path) {
  IntPtr descriptor=IntPtr.Zero;
  try {
   uint size;
   if(!ConvertStringSecurityDescriptorToSecurityDescriptorW("O:BAG:BAD:P(A;OICI;FA;;;SY)(A;OICI;FA;;;BA)",1,out descriptor,out size))throw new Win32Exception(Marshal.GetLastWin32Error());
   var security=new SecurityAttributes {Length=Marshal.SizeOf(typeof(SecurityAttributes)),Descriptor=descriptor,InheritHandle=0};
   if(!CreateDirectoryW(path,ref security))throw new Win32Exception(Marshal.GetLastWin32Error(),"Protected directory CREATE_NEW failed");
  }finally{if(descriptor!=IntPtr.Zero)LocalFree(descriptor);}
 }
 readonly FileStream stream;
 public ObservationDurableSink(string path) {
  IntPtr descriptor=IntPtr.Zero; SafeFileHandle handle=null;
  try {
   uint size;
   if(!ConvertStringSecurityDescriptorToSecurityDescriptorW("O:BAG:BAD:P(A;;FA;;;SY)(A;;FA;;;BA)",1,out descriptor,out size))
    throw new Win32Exception(Marshal.GetLastWin32Error(),"Sink descriptor conversion failed");
   var security=new SecurityAttributes { Length=Marshal.SizeOf(typeof(SecurityAttributes)),Descriptor=descriptor,InheritHandle=0 };
   // Atomic CREATE_NEW: no inherited-security window or after-create ACL repair.
   handle=CreateFileW(path,0xc0000000u,1,ref security,1,0x80000000u,IntPtr.Zero);
   int error=Marshal.GetLastWin32Error();
   if(handle.IsInvalid)throw new Win32Exception(error,"Protected sink CREATE_NEW failed");
   stream=new FileStream(handle,FileAccess.ReadWrite,4096,false);
   handle=null; // FileStream owns the retained synchronous write-through handle.
  } finally {
   if(handle!=null)handle.Dispose();
   if(descriptor!=IntPtr.Zero)LocalFree(descriptor);
   // Failed creation/wrapping never deletes or reopens a pathname.
  }
 }
 public void Write(byte[] bytes) {
  stream.Position=0;stream.Write(bytes,0,bytes.Length);stream.Flush(true);
  stream.Position=0;var actual=new byte[bytes.Length];int n=stream.Read(actual,0,actual.Length);
  if(n!=bytes.Length || stream.Length!=bytes.Length || Convert.ToBase64String(actual)!=Convert.ToBase64String(bytes))throw new IOException("Protected readback failed");
 }
 public void Append(string frame) {
  byte[] bytes=new UTF8Encoding(false,true).GetBytes(frame);
  long start=stream.Length;stream.Position=start;stream.Write(bytes,0,bytes.Length);stream.Flush(true);
  stream.Position=start;var actual=new byte[bytes.Length];int offset=0;
  while(offset<actual.Length){int n=stream.Read(actual,offset,actual.Length-offset);if(n==0)throw new IOException("Short readback");offset+=n;}
  if(stream.Length!=start+bytes.Length)throw new IOException("Length changed");
  for(int i=0;i<bytes.Length;i++)if(actual[i]!=bytes[i])throw new IOException("Readback mismatch");
  stream.Position=stream.Length;
 }
 public void Dispose(){stream.Dispose();}
}
"@
}
function Open-ObservationSinks {
 param([string]$Root,$Owners)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 Initialize-ObservationSinkType
 $sinks=@{}
 # Retain partially prepared writers on any failure, too.
 foreach($name in @('status','success','error','warning','verbose','debug','information')) {
  $sink=New-Object ObservationDurableSink (Join-Path $Root ($name+'.frames'))
  [void]$Owners.Add($sink); $sinks[$name]=$sink
  $sink.Append("0`t4`tdGVzdA==`n")
  $sink.Append("0`t5`tdGVzdDI=`n")
 }
 return $sinks
}
function Read-ObservationFixedConfig {
 param([byte[]]$Bytes,[string]$ExpectedHash)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 if((Get-ObservationHash $Bytes) -cne $ExpectedHash){throw 'Config digest refused'}
 # Canonical line format deliberately excludes JSON duplicate-key/coercion ambiguity.
 $value=(New-Object Text.UTF8Encoding($false,$true)).GetString($Bytes)
 $lines=$value.Split("`n")
 if($lines.Count -ne 8 -or $lines[7] -cne '' -or $lines[0] -cne 'ObservationHost002' -or $lines[1] -cne 'boundary-v1' -or $lines[2] -cne 'f773983c67c06e79ff422350d64324fa9a1e6ca6b3e1bd7b326e5a02d68765cc' -or $lines[3] -cnotmatch '^[0-9a-f]{64}$' -or $lines[4] -cnotmatch '^[0-9a-f]{64}$') {throw 'Fixed config schema refused'}
 foreach($index in @(5,6)){if((ConvertTo-ObservationTaskSddl $lines[$index]) -cne $lines[$index]){throw 'Canonical descriptor required'}}
 [pscustomobject]@{FolderSddl=$lines[5];TaskSddl=$lines[6];Experiment=$lines[1];RunnerHash=$lines[2];HostHash=$lines[3];TaskXmlHash=$lines[4]}
}
function Assert-ObservationExecutionPolicy {
 param([string]$MachinePolicy,[string]$UserPolicy)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 if($MachinePolicy -cne 'Undefined' -or $UserPolicy -cne 'Undefined'){throw 'Configured execution policy refused'}
}
function Assert-ObservationTaskXml {
 param([string]$Xml,[string]$ExpectedHash,[string]$Image,[string]$HostPath,[string]$Root)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 if($Root -cne (Join-Path ([Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)) 'BimeronCheckpointObservationBoundary') -or $HostPath -cne (Join-Path $Root 'Checkpoint-ObservationHost.ps1')){throw 'Fixed task host path required'}
 if((Get-ObservationHash ([Text.Encoding]::UTF8.GetBytes($Xml))) -cne $ExpectedHash){throw 'Exact task XML refused'}
 $doc=New-Object Xml.XmlDocument; $doc.XmlResolver=$null
 if($Xml -match '<!DOCTYPE'){throw 'DTD refused'}
 $doc.LoadXml($Xml)
 $ns=New-Object Xml.XmlNamespaceManager($doc.NameTable);$ns.AddNamespace('t','http://schemas.microsoft.com/windows/2004/02/mit/task')
 $checks=@{
 't:Principals/t:Principal/t:UserId'='S-1-5-18';'t:Principals/t:Principal/t:LogonType'='ServiceAccount';
 't:Settings/t:MultipleInstancesPolicy'='IgnoreNew';'t:Settings/t:ExecutionTimeLimit'='PT0S';
 't:Settings/t:AllowHardTerminate'='false';'t:Settings/t:StopIfGoingOnBatteries'='false';
 't:Settings/t:DisallowStartIfOnBatteries'='false';'t:Settings/t:RunOnlyIfIdle'='false';
 't:Settings/t:IdleSettings/t:StopOnIdleEnd'='false';'t:Settings/t:IdleSettings/t:RestartOnIdle'='false';
 't:Actions/t:Exec/t:Command'=$Image;'t:Actions/t:Exec/t:Arguments'=('-NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "'+$HostPath+'"');
 't:Actions/t:Exec/t:WorkingDirectory'=$Root
 }
 foreach($key in $checks.Keys){$nodes=$doc.SelectNodes('/t:Task/'+$key,$ns);if($nodes.Count -ne 1 -or $nodes[0].InnerText -cne $checks[$key]){throw ('Task setting refused: '+$key)}}
 foreach($path in @('t:Triggers/*','t:Settings/t:RestartOnFailure','t:Settings/t:DeleteExpiredTaskAfter')){if($doc.SelectNodes('/t:Task/'+$path,$ns).Count){throw 'Task lifecycle refused'}}
 if($doc.SelectNodes('/t:Task/t:Actions/*',$ns).Count -ne 1 -or $doc.SelectNodes('/t:Task/t:Principals/*',$ns).Count -ne 1){throw 'Task cardinality refused'}
}
function Get-ObservationProcessIdentity {
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 $process=[Diagnostics.Process]::GetCurrentProcess()
 $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
 try {
  $image=$process.MainModule.FileName
  [pscustomobject][ordered]@{
   Image=$image;ImageHash=(Get-ObservationHash ([IO.File]::ReadAllBytes($image)))
   Pid=$PID;CreationUtcFileTime=$process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString('x16')
   Sid=$identity.User.Value;Session=$process.SessionId;Bitness=([IntPtr]::Size*8)
   PowerShellVersion=$PSVersionTable.PSVersion.ToString();Runspace=[runspace]::DefaultRunspace.InstanceId.ToString()
   # Diagnostic only: elevation/integrity must be verified natively before Ready.
   Admission='Unavailable'
  }
 } finally {$identity.Dispose();$process.Dispose()}
}
function Write-ObservationConsumedIntent {
 param($State,[string]$Root,[string]$Record)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 Read-ObservationRecord $Record Dispatch $State.Binding
 if($State.Dispatched){throw 'Dispatch already consumed'}
 Initialize-ObservationSinkType
 # Existing intent (including partial intent from a crash) refuses; never truncate or retry.
 $intent=New-Object ObservationDurableSink (Join-Path $Root 'invocation.intent')
 [void]$State.Owners.Add($intent)
 $State.Dispatched=$true
 $intent.Append("InvocationIntent`t$($State.Binding)`n")
}

function Initialize-ObservationIdentityType {
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 if('ObservationToken' -as [type]){return}
 Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Security.Principal;
public static class ObservationToken {
 [DllImport("advapi32.dll",SetLastError=true)] static extern bool GetTokenInformation(IntPtr token,int kind,IntPtr data,int size,out int needed);
 public static int Value(IntPtr token,int kind) {
  int needed;GetTokenInformation(token,kind,IntPtr.Zero,0,out needed);
  if(needed<4 || needed>65536)throw new InvalidOperationException("Token bounds");
  IntPtr p=Marshal.AllocHGlobal(needed);
  try {if(!GetTokenInformation(token,kind,p,needed,out needed))throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error());
   if(kind==25){var sid=new SecurityIdentifier(Marshal.ReadIntPtr(p));string s=sid.Value;return int.Parse(s.Substring(s.LastIndexOf('-')+1));}
   return Marshal.ReadInt32(p);
  }finally{Marshal.FreeHGlobal(p);}
 }
}
"@
}
function ConvertTo-ObservationTaskSddl {
 param([string]$Sddl)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 $sd=New-Object Security.AccessControl.RawSecurityDescriptor $Sddl
 $sd.GetSddlForm([Security.AccessControl.AccessControlSections]14)
}
function Assert-ObservationTaskDescriptors {
 param($Folder,$Task,$Config)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 $folderSddl=$Folder.GetSecurityDescriptor(7)
 $taskSddl=$Task.GetSecurityDescriptor(7)
 Assert-ObservationTaskSecurity $folderSddl
 Assert-ObservationTaskSecurity $taskSddl
 if((ConvertTo-ObservationTaskSddl $folderSddl) -cne $Config.FolderSddl -or
    (ConvertTo-ObservationTaskSddl $taskSddl) -cne $Config.TaskSddl){throw 'Frozen task/folder descriptor mismatch'}
}
function Assert-ObservationTaskSecurity {
 param([string]$Sddl)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 $sd=New-Object Security.AccessControl.RawSecurityDescriptor $Sddl
 # Task mutation rights differ from file rights: permit no untrusted allow ACE.
 if($sd.Owner.Value -cnotin @('S-1-5-18','S-1-5-32-544') -or $null -eq $sd.DiscretionaryAcl -or
    -not ($sd.ControlFlags -band [Security.AccessControl.ControlFlags]::DiscretionaryAclProtected)){throw 'Task descriptor refused'}
 foreach($ace in $sd.DiscretionaryAcl){
  if($ace -isnot [Security.AccessControl.CommonAce] -or $ace.IsCallback){throw 'Unknown task ACE'}
  if($ace.AceQualifier -eq [Security.AccessControl.AceQualifier]::AccessAllowed -and
     $ace.SecurityIdentifier.Value -cnotin @('S-1-5-18','S-1-5-32-544')){throw 'Untrusted task access'}
 }
}
function Open-ObservationRegisteredTask {
 param($State,$Config,[string]$Image,[string]$HostPath,[string]$Root)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 $service=New-Object -ComObject 'Schedule.Service';[void]$State.Owners.Add($service);$service.Connect()
 $folder=$service.GetFolder('\BimeronLab');[void]$State.Owners.Add($folder)
 $task=$folder.GetTask('CheckpointObservationBoundary');[void]$State.Owners.Add($task)
 if($task.Name -cne 'CheckpointObservationBoundary' -or $task.Path -cne '\BimeronLab\CheckpointObservationBoundary'){throw 'Task identity refused'}
 Assert-ObservationTaskDescriptors $folder $task $Config
 Assert-ObservationTaskXml $task.Xml $Config.TaskXmlHash $Image $HostPath $Root
 return [pscustomobject]@{Folder=$folder;Task=$task}
}
function Get-ObservationVerifiedIdentity {
 param($State,[string]$Image,$ImagePin)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 Initialize-ObservationIdentityType
 $process=[Diagnostics.Process]::GetCurrentProcess();[void]$State.Owners.Add($process)
 # Force a retained process handle before reading creation time and image.
 $handle=$process.Handle
 if($handle -eq [IntPtr]::Zero -or $process.MainModule.FileName -cne $Image -or [IntPtr]::Size -ne 8 -or $PSVersionTable.PSEdition -cne 'Desktop'){throw 'Native Windows PowerShell required'}
 $identity=[Security.Principal.WindowsIdentity]::GetCurrent();[void]$State.Owners.Add($identity)
 $elevated=[ObservationToken]::Value($identity.Token,20);$integrity=[ObservationToken]::Value($identity.Token,25)
 if($identity.User.Value -cne 'S-1-5-18' -or $elevated -ne 1 -or $integrity -ne 16384 -or $process.SessionId -ne 0){throw 'Effective SYSTEM token required'}
 [pscustomobject][ordered]@{Image=$Image;ImageHash=(Get-ObservationHash ($ImagePin.Read(16777216)));Pid=$PID;
 CreationUtcFileTime=$process.StartTime.ToUniversalTime().ToFileTimeUtc().ToString('x16');Sid=$identity.User.Value;
 Elevation=$elevated;Integrity=$integrity;Session=$process.SessionId;Bitness=64;
 PowerShellVersion=$PSVersionTable.PSVersion.ToString();Runspace=[runspace]::DefaultRunspace.InstanceId.ToString()}
}
function New-ObservationRecord {
 param($State,[string]$Path,[string]$Kind)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 Initialize-ObservationSinkType
 $sink=New-Object ObservationDurableSink $Path
 [void]$State.Owners.Add($sink)
 $sink.Append($Kind+"`t"+$State.Binding+"`n")
 $pin=[CheckpointPin]::LogAlias((Get-ObservationVolumePath $Path))
 [void]$State.Owners.Add($pin)
}
function Start-ObservationHost {
 param($State,[string]$HostPath)
 $ErrorActionPreference="Stop"
 Set-StrictMode -Version Latest
 # Bootstrap only exact hash-verified initializer bytes. No runner top level executes.
 $known=[Environment]::GetFolderPath([Environment+SpecialFolder]::ProgramFiles)
 $runner=Join-Path $known 'BimeronCheckpointClientLabBoundary\Run-CheckpointLab.ps1'
 Import-ObservationPinTypes ([IO.File]::ReadAllBytes($runner))
 if([CheckpointNative]::ProgramFiles() -cne $known){throw 'Known folder mismatch'}
 $anchor=[CheckpointNative]::VolumePath($known)
 $root=Join-Path $known 'BimeronCheckpointObservationBoundary'
 if($HostPath -cne (Join-Path $root 'Checkpoint-ObservationHost.ps1')){throw 'Fixed host path required'}
 $independent=[CheckpointPin]::new($known,$true,[CheckpointPinRole]::TrustedOS)
 [void]$State.Owners.Add($independent)
 $rootPin=Open-ObservationProtectedChain $root $known $State.Owners -Directory
 $matched=$false
 foreach($owner in $State.Owners){if($owner -is [CheckpointPin] -and $owner.BoundPath -ceq $anchor -and $owner.Same($independent)){$matched=$true}}
 if(-not $matched){throw 'Known folder anchor identity refused'}
 foreach($name in @('ready.record','dispatch.record','invocation.intent')){
  $probe=[CheckpointNative]::CreateFileW([CheckpointNative]::VolumePath((Join-Path $root $name)),0x80,7,[IntPtr]::Zero,3,0x02200000,[IntPtr]::Zero)
  $errorCode=[Runtime.InteropServices.Marshal]::GetLastWin32Error()
  [void]$State.Owners.Add($probe)
  if(-not $probe.IsInvalid -or $errorCode -ne 2){throw 'Fresh handshake absence required'}
 }
 $pins=@{}
 foreach($name in @('Checkpoint-ObservationHost.ps1','ObservationHost-Native.ps1','host.config','task.xml')){
  $pins[$name]=Open-ObservationProtectedChain (Join-Path $root $name) $known $State.Owners
 }
 $configBytes=$pins['host.config'].Read(4096)
 $configHash=Get-ObservationHash $configBytes
 $config=Read-ObservationFixedConfig $configBytes $configHash
 if((Get-ObservationHash ($pins['Checkpoint-ObservationHost.ps1'].Read(1048576))) -cne $config.HostHash){throw 'Host hash refused'}
 # The reviewed host embeds the support digest; freeze support first, then host/config.
 $supportHash=Get-ObservationHash ($pins['ObservationHost-Native.ps1'].Read(1048576))
 if($supportHash -cne $script:ObservationSupportSha256){throw 'Support hash refused'}
 $runnerPin=Open-ObservationProtectedChain $runner $known $State.Owners
 if((Get-ObservationHash ($runnerPin.Read(1048576))) -cne $config.RunnerHash){throw 'Runner pin refused'}
 foreach($pair in @(@('run.manifest','0972de05644b5ac2a5210be9ad35efb596545dc1634a051b052f2dc0f4695330'),@('lab-protected-ack-Checkpoint.msi','860949118c7ac1ed93376e91576735ee7de2a67f924f0be323c172cf272c355a'))){
  $pin=Open-ObservationProtectedChain (Join-Path (Split-Path $runner) $pair[0]) $known $State.Owners
  if((Get-ObservationHash ($pin.Read(16777216))) -cne $pair[1]){throw 'Artifact refused'}
 }
 $image=Join-Path ([Environment]::SystemDirectory) 'WindowsPowerShell\v1.0\powershell.exe'
 $imagePin=Open-ObservationProtectedChain $image ([Environment]::SystemDirectory) $State.Owners -TrustedOS
 $identity=Get-ObservationVerifiedIdentity $State $image $imagePin
 $xml=(New-Object Text.UTF8Encoding($false,$true)).GetString($pins['task.xml'].Read(65536))
 Assert-ObservationTaskXml $xml $config.TaskXmlHash $image $HostPath $root
 $registered=Open-ObservationRegisteredTask $State $config $image $HostPath $root
 $State.Binding='boundary-v1:'+$config.HostHash+':'+$supportHash+':'+$configHash+':'+$identity.Pid+':'+$identity.CreationUtcFileTime
 $sinks=@{}
 $runnerContext=New-ObservationRunnerContext $runner $sinks
 $prepare={param($s)
  $script:ObservationSinks=Open-ObservationSinks $root $s.Owners
  foreach($key in $script:ObservationSinks.Keys){$sinks[$key]=$script:ObservationSinks[$key]}
  foreach($name in $script:ObservationSinks.Keys){
   $pin=[CheckpointPin]::LogAlias([CheckpointNative]::VolumePath((Join-Path $root ($name+'.frames'))))
   [void]$s.Owners.Add($pin)
  }
  $script:ObservationSinks.status.Append("0`t"+[Text.Encoding]::UTF8.GetByteCount(($identity|ConvertTo-Json -Compress))+"`t"+[Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes(($identity|ConvertTo-Json -Compress)))+"`n")
 }
 $ready={param($binding) New-ObservationRecord $State (Join-Path $root 'ready.record') Ready}
 $dispatch={
  $path=Join-Path $root 'dispatch.record'
  while(-not [IO.File]::Exists($path)){Start-Sleep -Milliseconds 250}
  $pin=Open-ObservationProtectedChain $path $known $State.Owners
  (New-Object Text.UTF8Encoding($false,$true)).GetString($pin.Read(4096))
 }
 $intent={param($binding)
  foreach($owner in $State.Owners){if($owner -is [CheckpointPin]){$owner.Revalidate()}}
  [CheckpointNative]::Mapping($known,$anchor,{[CheckpointNative]::ProgramFiles()},{param($p)[CheckpointNative]::VolumePath($p)})
  Assert-ObservationTaskDescriptors $registered.Folder $registered.Task $config
  Assert-ObservationTaskXml $registered.Task.Xml $config.TaskXmlHash $image $HostPath $root
  New-ObservationRecord $State (Join-Path $root 'invocation.intent') InvocationIntent
 }
 $invoke=${function:Invoke-ObservationRunnerCallback}
 $status={param($message) if($script:ObservationSinks){Write-ObservationFrame $State status $message {param($n,$f)$script:ObservationSinks.status.Append($f)} {}}}
 Invoke-ObservationCycle $State $prepare $ready $dispatch $intent $invoke $status ${function:Wait-ObservationResident} $runnerContext
}
