#requires -Version 7.5
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
 if (@(Compare-Object $actual $Names).Count) { throw 'Unexpected or missing fields.' }
}
function Assert-Hash($Value) {
 if ($Value -isnot [string] -or $Value -cnotmatch '^[a-fA-F0-9]{64}$') { throw 'Invalid hash.' }
}
function Assert-JsonKeys([string]$Json) {
 # Duplicate detection is scoped per object; the manifest repeats variant fields.
 # JsonDocument rejects malformed JSON; each object must have unique plain keys.
 $doc = [System.Text.Json.JsonDocument]::Parse($Json)
 try {
  function Check-Object($Element) {
   if ($Element.ValueKind -eq [System.Text.Json.JsonValueKind]::Object) {
    $seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach ($property in $Element.EnumerateObject()) {
     if (-not $seen.Add($property.Name)) { throw 'Duplicate JSON field.' }
     Check-Object $property.Value
    }
   }
  }
  Check-Object $doc.RootElement
 } finally { $doc.Dispose() }
}
function Read-LabManifest([string]$Json) {
 if ($Json.Length -gt 16384) { throw 'Manifest too large.' }
 Assert-JsonKeys $Json
 $m = ConvertFrom-Json -DateKind String -InputObject $Json
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
# Exact trace contract: all fields mandatory, no additional fields or nested JSON.
# Integer values are nonnegative <= UInt32.MaxValue except creationTime (decimal
# FILETIME string), utc (UTC ISO timestamp), sidHash (64 hex), logonId (16 hex).
function Read-LabTrace([string]$Line) {
 $prefix = 'BIMERON_LAB_TRACE_V1'
 $index = $Line.IndexOf($prefix, [StringComparison]::Ordinal)
 if ($index -lt 0) { return $null }
 $json = $Line.Substring($index + $prefix.Length).Trim()
 if ($json.Length -gt 2048 -or $json -notmatch '^\{[^{}\[\]]*\}$') { throw 'Invalid trace size or nesting.' }
 Assert-JsonKeys $json
 $t = ConvertFrom-Json -DateKind String $json
 $numbers = @('pid','tid','session','elevationType','integrity','impersonationLevel','status','intendedReturn')
 $enums = @{ callback=@('Preflight','Rollback','DeferredBegin','Commit','PostFinalize'); tokenSource=@('Thread','Process'); tokenType=@('Primary','Impersonation'); elevated=@('Yes','No','Unknown'); enabledAdmin=@('Yes','No','Unknown'); restricted=@('Yes','No','Unknown') }
 Assert-Fields $t (@('utc','creationTime','sidHash','logonId') + $numbers + @($enums.Keys))
 foreach ($n in $numbers) { if (($t.$n -isnot [int] -and $t.$n -isnot [long]) -or $t.$n -lt 0 -or $t.$n -gt [uint32]::MaxValue) { throw 'Invalid trace integer.' } }
 foreach ($n in $enums.Keys) { if ($t.$n -isnot [string] -or $enums[$n] -cnotcontains $t.$n) { throw 'Invalid trace enum.' } }
 if ($t.utc -isnot [string] -or $t.utc -cnotmatch '^\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}Z$' -or $t.creationTime -isnot [string] -or $t.creationTime -notmatch '^\d{1,20}$' -or $t.logonId -isnot [string] -or $t.logonId -notmatch '^[0-9a-fA-F]{16}$') { throw 'Invalid trace identity fields.' }
 if ($t.elevationType -notin @(1,2,3) -or $t.impersonationLevel -gt 3) { throw 'Invalid token numeric enum.' }
 Assert-Hash $t.sidHash
 return $t
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
if ($env:OS -ne 'Windows_NT' -or -not [Environment]::Is64BitProcess -or [Runtime.InteropServices.RuntimeInformation]::OSArchitecture -ne 'X64') { throw 'Native amd64 Windows runner required.' }
$root = 'C:\Users\Public\Documents\BimeronInstallerLab'
$null = Assert-LabPath $root
if ($PSCommandPath -cne "$root\Run-Lab.ps1") { throw 'Runner must be at its fixed guest path.' }
$pins = [Collections.Generic.List[IDisposable]]::new()
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
 public static object Read(){
  IntPtr h; string source="Thread";
  if(!OpenThreadToken(GetCurrentThread(),8,true,out h)){if(Marshal.GetLastWin32Error()!=1008)throw new Exception("Thread token refused.");source="Process";if(!OpenProcessToken(GetCurrentProcess(),8,out h))throw new Exception("Process token refused.");}
  try {
   string sid; IntPtr b=Info(h,1);try{sid=new SecurityIdentifier(Marshal.ReadIntPtr(b)).Value;}finally{Marshal.FreeHGlobal(b);}
   string hash;using(var sha=SHA256.Create()){hash=BitConverter.ToString(sha.ComputeHash(Encoding.UTF8.GetBytes(sid))).Replace("-","").ToLowerInvariant();}
   b=Info(h,25); int integrity;try{string s=new SecurityIdentifier(Marshal.ReadIntPtr(b)).Value;integrity=int.Parse(s.Substring(s.LastIndexOf('-')+1));}finally{Marshal.FreeHGlobal(b);}
   bool admin=false;b=Info(h,2);try{int count=Marshal.ReadInt32(b);int offset=IntPtr.Size==8?8:4;int stride=IntPtr.Size==8?16:8;for(int i=0;i<count;i++){IntPtr row=IntPtr.Add(b,offset+i*stride);string s=new SecurityIdentifier(Marshal.ReadIntPtr(row)).Value;uint flags=(uint)Marshal.ReadInt32(row,IntPtr.Size);if(s=="S-1-5-32-544" && (flags&4)!=0 && (flags&16)==0)admin=true;}}finally{Marshal.FreeHGlobal(b);}
   b=Info(h,11);bool restricted;try{restricted=Marshal.ReadInt32(b)>0 || IsTokenRestricted(h);}finally{Marshal.FreeHGlobal(b);}
   int type=Number(h,8);return new {source,sidHash=hash,session=Number(h,12),elevated=Number(h,20)!=0,elevationType=Number(h,18),integrity,enabledAdmin=admin,restricted,tokenType=type,impersonationLevel=type==2?Number(h,9):0};
  }finally{CloseHandle(h);}
 }
}
"@
 $token = [BimeronLabNative]::Read()
 $actualClass = if ($token.elevated -and $token.integrity -ge 12288 -and $token.enabledAdmin -and -not $token.restricted) { 'Elevated' } elseif (-not $token.elevated -and $token.integrity -lt 12288) { 'Unelevated' } else { 'Unknown' }
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
 $process = Start-Process -FilePath $exe -ArgumentList @($verb,('"'+$target+'"'),'/L*V',('"'+$log+'"'),'/norestart') -PassThru
 $timedOut = -not $process.WaitForExit(300000)
 $exitCode = $null; if (-not $timedOut) { $process.Refresh(); $exitCode=$process.ExitCode }
 $after = if ($timedOut) { $null } else { [BimeronLabNative]::MsiQueryProductState($variant.productCode) }
 $traces = @(); $complete = -not $timedOut
 if (-not $timedOut) {
  if ((Get-Item -LiteralPath (Assert-LabPath $log)).Length -gt 16777216) { $complete=$false } else {
   foreach ($line in [IO.File]::ReadLines($log)) {
    try { $trace=Read-LabTrace $line; if ($null -ne $trace) { if ($traces.Count -ge 64) { $complete=$false;break };$traces += $trace } } catch { $complete=$false }
   }
  }
 }
 # Failure callback order is not a settlement proof. Until DLL alignment and VM
# review, only the full success sequence can be structurally complete.
 $order = @($traces | ForEach-Object {$_.callback}) -join ','
 if ($variantName -ne 'Success' -or $order -cne 'Preflight,DeferredBegin,Commit,PostFinalize' -or @($traces | Where-Object {$_.status -ne 0 -or $_.intendedReturn -ne 0 -or $_.elevated -eq 'Unknown' -or $_.enabledAdmin -eq 'Unknown' -or $_.restricted -eq 'Unknown'}).Count) { $complete=$false }
 Write-LabJson $summaryPath @{case=$Case;manifestHash=$ExpectedManifestSha256;runnerHash=$ExpectedRunnerSha256;msiHash=$variant.sha256;dllHash=$variant.dllSha256;startingToken=$actualClass;callbackOrder=@($traces | ForEach-Object {$_.callback});tokenClasses=@($traces | ForEach-Object {"$($_.tokenSource)/$($_.tokenType)/$($_.elevated)"});exitCode=$exitCode;timedOut=$timedOut;registrationBefore=$before;registrationAfter=$after;outcome=(Get-LabOutcome $exitCode $timedOut $complete);traceComplete=$complete;authenticatedInitiator='Unknown';resourceRollback='Unknown';durableBarrier='Unknown';nativeCompatibility='Unknown';injectedFailure='Observed result only; never a security pass';childPid=$process.Id;childCreatedUtc=$(try {$process.StartTime.ToUniversalTime().ToString('o')} catch {'Unknown'})}
 Write-Output 'LAB observation saved locally. No product or security approval. On timeout stop and resolve manually; do not run another case or cleanup.'
} finally { foreach ($pin in $pins) { $pin.Dispose() } }
