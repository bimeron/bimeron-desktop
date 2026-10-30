#requires -Version 5.1
# Unsigned LAB ONLY. No product approval. Acceptance: foreground-README.md.
# Empty MSI observations cannot prove rollback, authenticated initiation, a durable
# barrier or native compatibility. Parent review and manual guest execution only.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][ValidateSet('DisposableWindowsVM')][string]$LabOnlyAllow,
 [Parameter(Mandatory)][ValidateSet('Success','CommitFail','PostFinalizeFail','UninstallSuccess','UninstallCommitFail','UninstallPostFinalizeFail')][string]$Case,
 [Parameter(Mandatory)][ValidateSet('Unelevated','Elevated')][string]$StartingToken,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedManifestSha256,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedRunnerSha256
)
if (@('Success','CommitFail','PostFinalizeFail','UninstallSuccess','UninstallCommitFail','UninstallPostFinalizeFail') -cnotcontains $Case -or @('Unelevated','Elevated') -cnotcontains $StartingToken) { throw 'Exact case and starting token required.' }
if ($LabOnlyAllow -cne 'DisposableWindowsVM') { throw 'Literal lab acknowledgment required.' }
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Assert-LabPath([string]$Path) {
 $root = 'C:\Users\Public\Documents\BimeronInstallerLab'
 $full = [IO.Path]::GetFullPath($Path)
 if ($Path -ne $full -or ($full -ne $root -and -not $full.StartsWith($root + '\', [StringComparison]::OrdinalIgnoreCase))) { throw 'Noncanonical lab path.' }
 $item = $full
 while ($item) {
  if (-not [IO.File]::Exists($item) -and -not [IO.Directory]::Exists($item)) { throw 'Missing lab path.' }
  if (([IO.File]::GetAttributes($item) -band [IO.FileAttributes]::ReparsePoint) -ne 0) { throw 'Reparse point refused.' }
  $item = [IO.Path]::GetDirectoryName($item)
 }
 return $full
}
function Assert-Fields($Object, [string[]]$Names) {
 if ($null -eq $Object -or $Object -isnot [pscustomobject]) { throw 'Object required.' }
 $actual = @($Object.PSObject.Properties.Name)
 if (@(Compare-Object $actual $Names -CaseSensitive).Count) { throw 'Unexpected or missing fields.' }
}
function Assert-Hash($Value) {
 if ($Value -isnot [string] -or $Value -cnotmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid hash.' }
}
function Initialize-LabHelpers {
 if ('BimeronLabJson' -as [type]) { return }
 Add-Type -TypeDefinition @"
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.Text;
using System.Threading;
using System.Runtime.InteropServices;
public sealed class BimeronLabJson {
 readonly string text; int pos, nodes;
 BimeronLabJson(string s) { text=s; }
 void White(){while(pos<text.Length && (text[pos]==' '||text[pos]=='\t'||text[pos]=='\r'||text[pos]=='\n'))pos++;}
 Exception Bad(){return new FormatException("Invalid bounded JSON.");}
 char Take(){if(pos>=text.Length)throw Bad();return text[pos++];}
 string String(){if(Take()!='"')throw Bad();var b=new StringBuilder();while(true){char c=Take();if(c=='"')return b.ToString();if(c<32)throw Bad();if(c=='\\'){c=Take();switch(c){case '"':case '\\':case '/':break;case 'b':c='\b';break;case 'f':c='\f';break;case 'n':c='\n';break;case 'r':c='\r';break;case 't':c='\t';break;case 'u':int n=0;for(int i=0;i<4;i++){char h=Take();int v=h>='0'&&h<='9'?h-'0':h>='a'&&h<='f'?h-'a'+10:h>='A'&&h<='F'?h-'A'+10:-1;if(v<0)throw Bad();n=n*16+v;}c=(char)n;break;default:throw Bad();}}b.Append(c);}}
 object Value(int depth){if(depth>8||++nodes>256)throw Bad();White();if(pos>=text.Length)throw Bad();char c=text[pos];if(c=='{'){pos++;var d=new Dictionary<string,object>(StringComparer.OrdinalIgnoreCase);White();if(pos<text.Length&&text[pos]=='}'){pos++;return d;}while(true){White();string k=String();White();if(Take()!=':')throw Bad();if(d.ContainsKey(k))throw Bad();d.Add(k,Value(depth+1));White();c=Take();if(c=='}')return d;if(c!=',')throw Bad();}}
 if(c=='"')return String();
 // This schema admits objects, strings and integers only. No coercion, arrays,
 // floating point, booleans or null are needed at either trust boundary.
 int start=pos;if(c=='-')pos++;if(pos>=text.Length||text[pos]<'0'||text[pos]>'9')throw Bad();if(text[pos]=='0')pos++;else while(pos<text.Length&&text[pos]>='0'&&text[pos]<='9')pos++;long number;if(!long.TryParse(text.Substring(start,pos-start),NumberStyles.AllowLeadingSign,CultureInfo.InvariantCulture,out number))throw Bad();return number;}
 public static object Parse(string s){if(s==null||s.Length>16384)throw new FormatException("JSON too large.");var p=new BimeronLabJson(s);object v=p.Value(0);p.White();if(p.pos!=s.Length)throw p.Bad();return v;}
}
public sealed class BimeronLabIdentity {
 public readonly uint Pid; public readonly string Created;
 public BimeronLabIdentity(uint pid,string created){Pid=pid;Created=created;}
}
public sealed class BimeronLabLaunch {
 [DllImport("kernel32.dll", SetLastError=true)] static extern uint GetProcessId(IntPtr handle);
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool GetProcessTimes(IntPtr handle,out long created,out long exit,out long kernel,out long user);
 public static BimeronLabIdentity Capture(Process process){
  IntPtr handle=process.Handle; // Process retains this actual launch handle through wait.
  uint pid=GetProcessId(handle);long created,exit,kernel,user;
  if(pid==0 || !GetProcessTimes(handle,out created,out exit,out kernel,out user) || created==0)throw new InvalidOperationException();
  return new BimeronLabIdentity(pid,unchecked((ulong)created).ToString("x16",CultureInfo.InvariantCulture));
 }
 readonly Func<Process,BimeronLabIdentity> capture; readonly Action<Process> wait;
 public volatile BimeronLabIdentity Identity; public volatile bool IdentityFailed;
 [DllImport("kernel32.dll")] static extern void GetNativeSystemInfo(IntPtr info);
 public static bool NativeAmd64(){IntPtr info=Marshal.AllocHGlobal(64);try{GetNativeSystemInfo(info);return Marshal.ReadInt16(info)==9;}finally{Marshal.FreeHGlobal(info);}}
 readonly IDisposable[] pins; readonly Func<Process> launch;
 readonly ManualResetEvent done=new ManualResetEvent(false);
 public Process Process; public int? ExitCode; public bool Failed;
 public BimeronLabLaunch(Func<Process> action, IDisposable[] ownedPins):this(action,ownedPins,Capture,delegate(Process p){p.WaitForExit();}){}
 public BimeronLabLaunch(Func<Process> action, IDisposable[] ownedPins,Func<Process,BimeronLabIdentity> query,Action<Process> awaitExit){launch=action;pins=ownedPins;capture=query;wait=awaitExit;}
 public static BimeronLabLaunch ForProcess(ProcessStartInfo info, IDisposable[] pins){return new BimeronLabLaunch(delegate {return System.Diagnostics.Process.Start(info);},pins);}
 // Keep the CLR host alive for late launch and pins; observer never joins this thread.
 public void Start(){var t=new Thread(Run);t.IsBackground=false;t.Start();}
 void Run(){try{Process=launch();if(Process==null)throw new InvalidOperationException();try{Identity=capture(Process);if(Identity==null)IdentityFailed=true;}catch{IdentityFailed=true;}wait(Process);ExitCode=Process.ExitCode;}catch{Failed=true;}finally{foreach(var p in pins){try{p.Dispose();}catch{}}done.Set();}}
 public bool Observe(int milliseconds){return done.WaitOne(milliseconds);}
}
"@
}
function Convert-LabJsonObject($Value) {
 if ($Value -is [Collections.Generic.Dictionary[string,object]]) {
  $result = [ordered]@{}
  foreach ($key in $Value.Keys) { $result[$key] = Convert-LabJsonObject $Value[$key] }
  return [pscustomobject]$result
 }
 return $Value
}
function Read-LabJson([string]$Json) {
 Initialize-LabHelpers
 return Convert-LabJsonObject ([BimeronLabJson]::Parse($Json))
}
function Assert-JsonKeys([string]$Json) { $null = Read-LabJson $Json }
function Read-LabManifest([string]$Json) {
 if ($Json.Length -gt 16384) { throw 'Manifest too large.' }
 $m = Read-LabJson $Json
 Assert-Fields $m @('schema','architecture','traceSchema','variants')
 if ($m.schema -isnot [long] -and $m.schema -isnot [int]) { throw 'Invalid schema type.' }
 if ($m.traceSchema -isnot [long] -and $m.traceSchema -isnot [int]) { throw 'Invalid trace schema type.' }
 if ($m.schema -ne 1 -or $m.traceSchema -ne 1 -or $m.architecture -isnot [string] -or $m.architecture -cne 'amd64') { throw 'Unsupported manifest.' }
 Assert-Fields $m.variants @('Success','CommitFail','PostFinalizeFail')
 $ids = @()
 foreach ($name in @('Success','CommitFail','PostFinalizeFail')) {
  $v = $m.variants.$name
  Assert-Fields $v @('file','sha256','productCode','packageCode','upgradeCode','dllSha256')
  if ($v.file -isnot [string] -or $v.file -cne "lab-trace-$name.msi") { throw 'Invalid MSI filename.' }
  Assert-Hash $v.sha256; Assert-Hash $v.dllSha256
  foreach ($field in @('productCode','packageCode','upgradeCode')) {
   if ($v.$field -isnot [string] -or $v.$field -cnotmatch '^\{[0-9A-F]{8}(-[0-9A-F]{4}){3}-[0-9A-F]{12}\}$' -or $v.$field -eq '{00000000-0000-0000-0000-000000000000}') { throw 'Invalid GUID.' }
   if ($ids -contains $v.$field) { throw 'Repeated lab identity.' }; $ids += $v.$field
  }
 }
 return $m
}
# Canonical contract: lab-trace.cpp at ccf2957a. No alternate schema accepted.
function Read-LabTrace([string]$Line) {
 $prefix = 'BIMERON_LAB_TRACE_V1'
 $index = $Line.IndexOf($prefix, [StringComparison]::Ordinal)
 if ($index -lt 0) { return $null }
 if ($Line.Length -gt 8192) { throw 'Trace line too large.' }
 $json = $Line.Substring($index + $prefix.Length).Trim()
 if ($json.Length -gt 2048) { throw 'Trace too large.' }
 $t = Read-LabJson $json
 $base = @('schema','callback','evidence','status','initiatingClient','intendedReturn')
 $numbers = @('pid','tid','processSession','tokenSource','tokenType','elevated','elevationType','integrityRID','tokenSession','adminEnabled','adminDenyOnly','restricted')
 $full = $base + $numbers + @('utc','processCreated','impersonation','sidSHA256','logonId')
 if (@($t.PSObject.Properties).Count -eq 6) { Assert-Fields $t $base } else { Assert-Fields $t $full }
 foreach ($n in @('schema','status','intendedReturn')) {
  if ($t.$n -isnot [long] -or $t.$n -lt 0 -or $t.$n -gt [uint32]::MaxValue) { throw 'Invalid trace integer.' }
 }
 if ($t.schema -ne 1 -or $t.callback -isnot [string] -or @('ObservationPreflight','ObservationRollback','ObservationDeferredBegin','ObservationCommit','ObservationPostFinalize') -cnotcontains $t.callback -or $t.evidence -isnot [string] -or @('observed','incomplete') -cnotcontains $t.evidence -or $t.initiatingClient -isnot [string] -or $t.initiatingClient -cne 'Unknown') { throw 'Invalid trace enum.' }
 if (@($t.PSObject.Properties).Count -eq 6) {
  if ($t.evidence -cne 'incomplete' -or $t.status -eq 0) { throw 'Invalid fallback evidence.' }
  return $t
 }
 foreach ($n in $numbers) { if ($t.$n -isnot [long] -or $t.$n -lt 0 -or $t.$n -gt [uint32]::MaxValue) { throw 'Invalid trace integer.' } }
 foreach ($n in @('elevated','adminEnabled','adminDenyOnly','restricted')) { if ($t.$n -gt 1) { throw 'Invalid trace boolean.' } }
 if ($t.tokenSource -gt 2 -or $t.tokenType -gt 2 -or $t.elevationType -gt 3 -or $t.impersonation -isnot [long] -or $t.impersonation -lt -1 -or $t.impersonation -gt 3) { throw 'Invalid token enum.' }
 foreach ($n in @('processCreated','logonId')) { if ($t.$n -isnot [string] -or $t.$n -cnotmatch '^[0-9a-f]{16}$') { throw 'Invalid trace identity.' } }
 if ($t.utc -isnot [string] -or $t.utc -cnotmatch '^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$') { throw 'Invalid trace UTC.' }
 $date=[datetime]::MinValue
 if (-not [datetime]::TryParseExact($t.utc,"yyyy-MM-dd'T'HH:mm:ss.fff'Z'",[Globalization.CultureInfo]::InvariantCulture,[Globalization.DateTimeStyles]::None,[ref]$date)) { throw 'Invalid trace date.' }
 if ($t.sidSHA256 -isnot [string] -or ($t.sidSHA256 -cnotmatch '^[0-9a-f]{64}$' -and -not ($t.evidence -ceq 'incomplete' -and $t.sidSHA256 -ceq 'unknown'))) { throw 'Invalid SID hash.' }
 if ($t.evidence -ceq 'observed') {
  if ($t.status -ne 0 -or $t.tokenSource -eq 0 -or $t.tokenType -eq 0 -or $t.elevationType -eq 0 -or $t.pid -eq 0 -or $t.tid -eq 0 -or $t.processCreated -ceq '0000000000000000' -or ($t.tokenType -eq 1 -and $t.impersonation -ne -1) -or ($t.tokenType -eq 2 -and $t.impersonation -lt 0)) { throw 'Unknown observed identity.' }
 } elseif ($t.status -eq 0) { throw 'Incomplete evidence requires error.' }
 return $t
}
function Test-LabTraceComplete($Traces, [string]$VariantName) {
 if ($VariantName -cne 'Success' -or (@($Traces | ForEach-Object {$_.callback}) -join ',') -cne 'ObservationPreflight,ObservationDeferredBegin,ObservationCommit,ObservationPostFinalize') { return $false }
 return @($Traces | Where-Object {$_.evidence -cne 'observed' -or $_.status -ne 0 -or $_.intendedReturn -ne 0}).Count -eq 0
}
function Compare-LabProcessIdentity($Trace, $Identity) {
 if ($null -eq $Identity -or $Trace.evidence -cne 'observed') { return 'Unknown' }
 if ($Trace.pid -eq $Identity.Pid -and $Trace.processCreated -ceq $Identity.Created) { return 'Equal' }
 return 'Different'
}
function Get-LabTokenClass($Token) {
 if ($Token.elevated -and $Token.integrity -ge 12288 -and $Token.enabledAdmin -and -not $Token.restricted) { return 'Elevated' }
 if (-not $Token.elevated -and $Token.integrity -lt 12288) { return 'Unelevated' }
 return 'Unknown'
}
function Get-LabOutcome($ExitCode, [bool]$TimedOut, [bool]$Complete) {
 if ($TimedOut -or $null -eq $ExitCode) { return 'UnknownPending' }
 if (-not $Complete) { return 'IncompleteEvidence' }
 return 'ObservedOnly'
}
function Open-LabPinned([string]$Path, [string]$Hash) {
 $null = Assert-LabPath $Path
 $stream = [IO.File]::Open($Path, [IO.FileMode]::Open, [IO.FileAccess]::Read, [IO.FileShare]::Read)
 try {
  $sha = [Security.Cryptography.SHA256]::Create()
  try { $actual = ([BitConverter]::ToString($sha.ComputeHash($stream))).Replace('-','') } finally { $sha.Dispose() }
  if ($actual -ine $Hash) { throw 'Pinned hash mismatch.' }
  $stream.Position = 0
  return $stream
 } catch { $stream.Dispose(); throw }
}
# No library/test-mode entry point: tests extract only helper AST definitions.
Initialize-LabHelpers
if ($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitProcess -or -not [BimeronLabLaunch]::NativeAmd64()) { throw 'Native amd64 Windows runner required.' }
$root = 'C:\Users\Public\Documents\BimeronInstallerLab'
$null = Assert-LabPath $root
if ($PSCommandPath -cne "$root\Run-Lab.ps1") { throw 'Runner must be at its fixed guest path.' }
$pins = [Collections.Generic.List[IDisposable]]::new()
$pinsTransferred = $false
try {
 $self = Open-LabPinned $PSCommandPath $ExpectedRunnerSha256; $pins.Add($self)
 $manifestStream = Open-LabPinned "$root\lab-manifest.json" $ExpectedManifestSha256; $pins.Add($manifestStream)
 if ($manifestStream.Length -gt 16384) { throw 'Manifest too large.' }
 $reader = [IO.StreamReader]::new($manifestStream, [Text.UTF8Encoding]::new($false,$true), $true, 1024, $true)
 try { $manifest = Read-LabManifest $reader.ReadToEnd() } finally { $reader.Dispose() }
 $variantName = $Case -creplace '^Uninstall',''
 $variant = $manifest.variants.$variantName
 foreach ($name in @('Success','CommitFail','PostFinalizeFail')) { $pins.Add((Open-LabPinned "$root\$($manifest.variants.$name.file)" $manifest.variants.$name.sha256)) }
 $vm = @(Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop)
 if ($vm.Count -ne 1 -or $vm[0].Manufacturer -cne 'Microsoft Corporation' -or $vm[0].Model -cne 'Virtual Machine') { throw 'Positive Microsoft Hyper-V classification required.' }
 Add-Type -TypeDefinition @"
using System;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Security.Cryptography;
using System.Text;
public static class BimeronLabNative {
 [DllImport("kernel32.dll")] static extern IntPtr GetCurrentThread();
 [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
 [DllImport("kernel32.dll", SetLastError=true)] static extern bool CloseHandle(IntPtr h);
 [DllImport("advapi32.dll", SetLastError=true)] static extern bool OpenThreadToken(IntPtr t,uint a,bool s,out IntPtr h);
 [DllImport("advapi32.dll", SetLastError=true)] static extern bool OpenProcessToken(IntPtr p,uint a,out IntPtr h);
 [DllImport("advapi32.dll", SetLastError=true)] static extern bool GetTokenInformation(IntPtr h,int c,IntPtr b,int n,out int needed);
 [DllImport("advapi32.dll")] static extern bool IsTokenRestricted(IntPtr h);
 [DllImport("msi.dll",CharSet=CharSet.Unicode)] public static extern int MsiQueryProductState(string code);
 static IntPtr Info(IntPtr h,int c) { int n; GetTokenInformation(h,c,IntPtr.Zero,0,out n); if(n<=0 || n>1048576) throw new Exception("Token query failed."); IntPtr b=Marshal.AllocHGlobal(n); if(!GetTokenInformation(h,c,b,n,out n)){Marshal.FreeHGlobal(b);throw new Exception("Token query failed.");}return b; }
 static int Number(IntPtr h,int c){IntPtr b=Info(h,c);try{return Marshal.ReadInt32(b);}finally{Marshal.FreeHGlobal(b);}}
 static byte[] SidBytes(string sid){var s=new SecurityIdentifier(sid);var bytes=new byte[s.BinaryLength];s.GetBinaryForm(bytes,0);return bytes;}
 public static object Read(){
  IntPtr h; string source="Thread";
  if(!OpenThreadToken(GetCurrentThread(),8,true,out h)){if(Marshal.GetLastWin32Error()!=1008)throw new Exception("Thread token refused.");source="Process";if(!OpenProcessToken(GetCurrentProcess(),8,out h))throw new Exception("Process token refused.");}
  try {
   string sid; IntPtr b=Info(h,1);try{sid=new SecurityIdentifier(Marshal.ReadIntPtr(b)).Value;}finally{Marshal.FreeHGlobal(b);}
   string hash;using(var sha=SHA256.Create()){hash=BitConverter.ToString(sha.ComputeHash(SidBytes(sid))).Replace("-","").ToLowerInvariant();}
   b=Info(h,25); int integrity;try{string s=new SecurityIdentifier(Marshal.ReadIntPtr(b)).Value;integrity=int.Parse(s.Substring(s.LastIndexOf('-')+1));}finally{Marshal.FreeHGlobal(b);}
   bool admin=false;b=Info(h,2);try{int count=Marshal.ReadInt32(b);int offset=IntPtr.Size==8?8:4;int stride=IntPtr.Size==8?16:8;for(int i=0;i<count;i++){IntPtr row=IntPtr.Add(b,offset+i*stride);string s=new SecurityIdentifier(Marshal.ReadIntPtr(row)).Value;uint flags=(uint)Marshal.ReadInt32(row,IntPtr.Size);if(s=="S-1-5-32-544" && (flags&4)!=0 && (flags&16)==0)admin=true;}}finally{Marshal.FreeHGlobal(b);}
   b=Info(h,11);bool restricted;try{restricted=Marshal.ReadInt32(b)>0 || IsTokenRestricted(h);}finally{Marshal.FreeHGlobal(b);}
   int type=Number(h,8);return new {source,sidHash=hash,session=Number(h,12),elevated=Number(h,20)!=0,elevationType=Number(h,18),integrity,enabledAdmin=admin,restricted,tokenType=type,impersonationLevel=type==2?Number(h,9):0};
  }finally{CloseHandle(h);}
 }
}
"@
 $token = [BimeronLabNative]::Read()
 $actualClass = Get-LabTokenClass $token
 if ($actualClass -cne $StartingToken) { throw 'Starting context mismatch; open the desired shell manually.' }
 $uninstall = $Case.StartsWith('Uninstall', [StringComparison]::Ordinal)
 $before = [BimeronLabNative]::MsiQueryProductState($variant.productCode)
 if (($uninstall -and $before -ne 5) -or (-not $uninstall -and $before -ne -1)) { throw 'Registration state unsuitable or unknown.' }
 $system = [Environment]::GetFolderPath([Environment+SpecialFolder]::System)
 $exe = [IO.Path]::Combine($system, 'msiexec.exe')
 if ($exe -ine 'C:\Windows\System32\msiexec.exe') { throw 'Unexpected native system path.' }
 $id = [Guid]::NewGuid().ToString('N')
 $log = "$root\lab-$Case-$id.log"
 $receipt = "$root\lab-$Case-$id.receipt.json"
 $summaryPath = "$root\lab-$Case-$id.summary.json"
 # CreateNew prevents overwriting an existing file or following a planted output.
 function Write-LabJson([string]$Path,$Value) {
  $null = Assert-LabPath ([IO.Path]::GetDirectoryName($Path))
  $s = [IO.File]::Open($Path,[IO.FileMode]::CreateNew,[IO.FileAccess]::Write,[IO.FileShare]::Read)
  try { $bytes=[Text.Encoding]::UTF8.GetBytes(($Value | ConvertTo-Json -Depth 6));$s.Write($bytes,0,$bytes.Length) } finally { $s.Dispose() }
 }
 $logHandle = [IO.File]::Open($log,[IO.FileMode]::CreateNew,[IO.FileAccess]::ReadWrite,[IO.FileShare]::ReadWrite)
 $pins.Add($logHandle)
 Write-LabJson $receipt @{labOnly=$true;unsigned=$true;case=$Case;runnerPid=$PID;runnerCreatedUtc=(Get-Process -Id $PID).StartTime.ToUniversalTime().ToString('o');token=$token;tokenClass=$actualClass;manifestHash=$ExpectedManifestSha256;runnerHash=$ExpectedRunnerSha256;vmClassification='MicrosoftHyperV';authenticVMName='Unknown';authenticatedInitiator='Unknown'}
 $target = if ($uninstall) { $variant.productCode } else { "$root\$($variant.file)" }
 $verb = if ($uninstall) { '/x' } else { '/i' }
 # Acceptance: the same five-minute budget includes launch/UAC and process wait.
 # Worker owns pins after Start; never Stop/Dispose/join it on the observer thread.
 Initialize-LabHelpers
 $info = [Diagnostics.ProcessStartInfo]::new()
 $info.FileName=$exe; $info.UseShellExecute=$true
 $info.Arguments=($verb+' "'+$target+'" /L*V "'+$log+'" /norestart')
 $worker = [BimeronLabLaunch]::ForProcess($info,$pins.ToArray())
 $clock = [Diagnostics.Stopwatch]::StartNew()
 $worker.Start(); $pinsTransferred=$true
 $timedOut = -not $worker.Observe([Math]::Max(0,300000-[int]$clock.ElapsedMilliseconds))
 $identity = $worker.Identity # Atomic immutable snapshot, including on timeout.
 $exitCode = $null
 if (-not $timedOut) { $exitCode=$worker.ExitCode }
 $after = if ($timedOut) { $null } else { [BimeronLabNative]::MsiQueryProductState($variant.productCode) }
 $traces = @(); $complete = -not $timedOut -and -not $worker.Failed -and $null -ne $identity
 if (-not $timedOut) {
  if ((Get-Item -LiteralPath (Assert-LabPath $log)).Length -gt 16777216) { $complete=$false } else {
   foreach ($line in [IO.File]::ReadLines($log)) {
    try { $trace=Read-LabTrace $line; if ($null -ne $trace) { if ($traces.Count -ge 64) { $complete=$false;break };$traces += $trace } } catch { $complete=$false }
   }
  }
 }
 # Failure callback order is not a settlement proof. Before VM review, only
 # the full observed success sequence can be structurally complete.
 if (-not (Test-LabTraceComplete $traces $variantName)) { $complete=$false }
 Write-LabJson $summaryPath @{case=$Case;manifestHash=$ExpectedManifestSha256;runnerHash=$ExpectedRunnerSha256;msiHash=$variant.sha256;dllHash=$variant.dllSha256;startingToken=$actualClass;callbackOrder=@($traces | ForEach-Object {$_.callback});tokenClasses=@($traces | ForEach-Object {$(if ($_.evidence -ceq 'observed') {"$($_.tokenSource)/$($_.tokenType)/$($_.elevated)"} else {'Unknown'})});exitCode=$exitCode;timedOut=$timedOut;registrationBefore=$before;registrationAfter=$after;outcome=(Get-LabOutcome $exitCode $timedOut $complete);traceComplete=$complete;authenticatedInitiator='Unknown';resourceRollback='Unknown';durableBarrier='Unknown';nativeCompatibility='Unknown';injectedFailure='Observed result only; never a security pass';possibleLateOperation=$timedOut;launchFailed=$(if($timedOut){$null}else{$worker.Failed});childPid=$(if($null -eq $identity){$null}else{$identity.Pid});childProcessCreated=$(if($null -eq $identity){'Unknown'}else{$identity.Created});childIdentityEvidence=$(if($null -eq $identity){'incomplete'}else{'observed'});callbackProcessComparison=@($traces | ForEach-Object {Compare-LabProcessIdentity $_ $identity})}
 if ($timedOut) { Write-Output 'UnknownPending: PossibleLateOperation. Launch/UAC may still complete and start the installer. Worker retains pins until completion and can keep this host alive indefinitely. Host death loses pins. Stop orchestration; no other case or uninstall. No cancellation was performed.' }
 Write-Output 'LAB observation saved locally. No product or security approval. On timeout stop and resolve manually; do not run another case or cleanup.'
} finally { if (-not $pinsTransferred) { foreach ($pin in $pins) { $pin.Dispose() } } }
