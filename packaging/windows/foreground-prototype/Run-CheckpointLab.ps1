#requires -Version 5.1
# Unsigned LAB ONLY. Parent review/provisioning required. No authority or Clean claim.
[CmdletBinding()]
param(
 [Parameter(Mandatory)][ValidateSet('DisposableWindowsVM')][string]$LabOnlyAllow,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedRunnerSha256,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedManifestSha256,
 [Parameter(Mandatory)][ValidatePattern('^[a-fA-F0-9]{64}$')][string]$ExpectedMsiSha256
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Initialize-CheckpointLab {
 Add-Type -TypeDefinition @"
using System;
using System.IO;
using System.Text;
using System.Diagnostics;
using System.Threading;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Security.AccessControl;
using System.Security.Principal;
using System.Security.Cryptography;
using Microsoft.Win32.SafeHandles;

public sealed class CheckpointIdentity {
 public readonly uint Pid; public readonly long Created;
 public CheckpointIdentity(uint p,long c){Pid=p;Created=c;}
 public bool Same(CheckpointIdentity other){return other!=null && Pid!=0 && Created!=0 && Pid==other.Pid && Created==other.Created;}
}
public sealed class CheckpointSnapshot {
 public readonly string Decision, Outcome, ApiResult, ClientDeath, Latency, ServerQuiescence;
 public readonly bool PossibleLateOperation, Completed;
 public readonly CheckpointIdentity Identity;
 public CheckpointSnapshot(string decision,string outcome,string api,string death,bool late,bool completed,CheckpointIdentity identity){
  Decision=decision;Outcome=outcome;ApiResult=api;ClientDeath=death;PossibleLateOperation=late;Completed=completed;Identity=identity;
  Latency="Unknown";ServerQuiescence="Unknown";
 }
}
public sealed class CheckpointWorker {
 readonly object gate=new object(); readonly ManualResetEvent done=new ManualResetEvent(false);
 readonly Func<long> now; readonly long deadline; readonly Func<IntPtr> launch;
 readonly Func<IntPtr,CheckpointIdentity> query; readonly Func<IntPtr,bool> alive,ready,terminate,death;
 readonly Action<IntPtr> wait,close; readonly IDisposable[] pins;
 string state="Eligible"; CheckpointSnapshot snapshot;
 public CheckpointWorker(Func<long> clock,long expires,Func<IntPtr> start,Func<IntPtr,CheckpointIdentity> identity,
  Func<IntPtr,bool> isAlive,Func<IntPtr,bool> validate,Func<IntPtr,bool> kill,Func<IntPtr,bool> confirm,
  Action<IntPtr> awaitEnd,Action<IntPtr> release,IDisposable[] retained){
  now=clock;deadline=expires;launch=start;query=identity;alive=isAlive;ready=validate;terminate=kill;death=confirm;wait=awaitEnd;close=release;pins=retained;
  snapshot=new CheckpointSnapshot(state,"AwaitingLaunch","NotCalled","Unknown",true,false,null);
 }
 public void Start(){var thread=new Thread(Run);thread.IsBackground=false;thread.Start();}
 public bool Observe(int milliseconds){return done.WaitOne(milliseconds);}
 public int Remaining {get {return (int)Math.Max(0,Math.Min(300000,deadline-now()));}}
 public CheckpointSnapshot Snapshot {get {lock(gate){return snapshot;}}}
 public CheckpointSnapshot Timeout(){lock(gate){
  if(snapshot.Completed)return snapshot;
  if(state=="Eligible") {state="Cancelled";snapshot=new CheckpointSnapshot(state,"Cancelled","NotCalled","Unknown",true,false,snapshot.Identity);}
  else if(state=="Committed") snapshot=new CheckpointSnapshot(state,"TerminationDecisionInFlight",snapshot.ApiResult,snapshot.ClientDeath,true,false,snapshot.Identity);
  return snapshot;
 }}
 bool Eligible(){lock(gate){return state=="Eligible";}}
 void Run(){IntPtr handle=IntPtr.Zero;CheckpointIdentity identity=null;string api="NotCalled",dead="Unknown",outcome="IncompleteEvidence";
  try {
   handle=launch();if(handle==IntPtr.Zero)throw new InvalidOperationException("Launch failed");
   // First action after launch is capture through the actual returned handle.
   identity=query(handle);if(identity==null || identity.Pid==0 || identity.Created==0)throw new InvalidOperationException("Identity unavailable");
   lock(gate){snapshot=new CheckpointSnapshot(state,state=="Cancelled"?"Cancelled":"AwaitingCheckpoint",api,dead,true,false,identity);}
   while(Eligible() && now()<deadline){
    if(!alive(handle)){outcome="EarlyClientExit";break;}
    if(!ready(handle)){Thread.Sleep(10);continue;}
    CheckpointIdentity selected=query(handle);
    if(!identity.Same(selected) || !alive(handle)) {outcome="IdentityOrAliveRefused";break;}
    bool committed=false;
    lock(gate){
     if(state=="Eligible"){
      if(now()<deadline){state="Committed";committed=true;api="Unknown";snapshot=new CheckpointSnapshot(state,"TerminationDecisionInFlight",api,dead,true,false,identity);}
      else {state="Cancelled";snapshot=new CheckpointSnapshot(state,"Cancelled",api,dead,true,false,identity);}
     }
    }
    if(committed){
     // Same worker, same retained handle, one direct call. Preemption here is unbounded.
     bool result=terminate(handle);
     api=result?"Succeeded":"Failed";
     dead=death(handle)?"Signaled":"Unconfirmed";
     outcome=dead=="Signaled"?"OwnedClientDeathObserved":"ClientDeathUnconfirmed";
     lock(gate){snapshot=new CheckpointSnapshot(state,outcome,api,dead,true,false,identity);}
    }
    break;
   }
  }catch {outcome="OperationFailed";}
  finally {
   // Cancellation never kills a late client. Retain process and input pins until it ends.
   if(handle!=IntPtr.Zero){try{wait(handle);if(death(handle)){dead="Signaled";if(outcome=="ClientDeathUnconfirmed")outcome="OwnedClientDeathObserved";}}catch{outcome="WaitFailed";}}
   if(handle!=IntPtr.Zero){try{close(handle);}catch{}}
   foreach(var pin in pins){try{pin.Dispose();}catch{}}
   lock(gate){
    if(state=="Eligible")state="Cancelled";
    snapshot=new CheckpointSnapshot(state,outcome,api,dead,true,true,identity);
   }
   done.Set();
  }
 }
}
public static class CheckpointFormat {
 public const string MsiHash="860949118c7ac1ed93376e91576735ee7de2a67f924f0be323c172cf272c355a";
 public const string ManifestHash="0972de05644b5ac2a5210be9ad35efb596545dc1634a051b052f2dc0f4695330";
 public const string DllHash="32ca57727e2daf73cab4da32867001b50563f35469caea008e20e3c9db5b63d2";
 public const string Product="{E6D10713-6EBE-4C5B-9750-727BC0FF3B27}";
 public const string Package="{2ABAF8C4-7BA6-473E-BE50-8428E03EA240}";
 public const string Installation="3e91be27099654eaa18c059367acd3b7",Generation="64ec54c95b4057a0a50014f612dfb974";
 public static string Hash(byte[] b){using(var sha=SHA256.Create())return Hex(sha.ComputeHash(b));}
 public static string Hex(byte[] b){return BitConverter.ToString(b).Replace("-","").ToLowerInvariant();}
 public static bool Part(byte[] b,int offset,byte[] expected){if(b==null || offset<0 || b.Length-offset<expected.Length)return false;for(int i=0;i<expected.Length;i++)if(b[offset+i]!=expected[i])return false;return true;}
 public static byte[] Bytes(string hex){var b=new byte[hex.Length/2];for(int i=0;i<b.Length;i++)b[i]=Convert.ToByte(hex.Substring(i*2,2),16);return b;}
 public static void Manifest(byte[] b){
  if(b==null || b.Length!=9512 || Hash(b)!=ManifestHash || !Part(b,0,Encoding.ASCII.GetBytes("BLACK001")) ||
   !Part(b,8,Bytes(Installation)) || !Part(b,24,Bytes(Generation)) || BitConverter.ToUInt32(b,40)!=1 || BitConverter.ToUInt32(b,44)!=3 ||
   !Part(b,48,new Guid(Product).ToByteArray()) || !Part(b,64,new Guid(Package).ToByteArray()) ||
   !Part(b,80,Bytes(MsiHash)) || !Part(b,112,Bytes(DllHash)))throw new FormatException("Frozen manifest refused");
  byte[] projection=new byte[b.Length-176];Buffer.BlockCopy(b,176,projection,0,projection.Length);
  if(!Part(b,144,Bytes(Hash(projection))))throw new FormatException("Projection digest refused");
 }
 public static byte[] Checkpoint(){var b=new byte[80];Buffer.BlockCopy(Encoding.ASCII.GetBytes("BLCP"),0,b,0,4);b[4]=1;
  Buffer.BlockCopy(Bytes(Installation),0,b,8,16);Buffer.BlockCopy(Bytes(Generation),0,b,24,16);Buffer.BlockCopy(Bytes(ManifestHash),0,b,40,32);b[72]=1;return b;}
 public static byte[] Pending(){var b=new byte[48];Buffer.BlockCopy(Encoding.ASCII.GetBytes("BLPF"),0,b,0,4);b[4]=1;b[5]=1;
  Buffer.BlockCopy(Bytes(Installation),0,b,8,16);Buffer.BlockCopy(Bytes(Generation),0,b,24,16);return b;}
 public static void Records(byte[] checkpoint,byte[] pending){if(checkpoint==null || checkpoint.Length!=80 || !Part(checkpoint,0,Checkpoint()) || pending==null || pending.Length!=48 || !Part(pending,0,Pending()))throw new FormatException("Checkpoint/Pending refused");}
}
public static class CheckpointNative {
 [StructLayout(LayoutKind.Sequential,Pack=4)] public struct Info {public uint Attributes;public long Creation,Access,Write;public uint Volume,SizeHigh,SizeLow,Links,IndexHigh,IndexLow;}
 [StructLayout(LayoutKind.Sequential,CharSet=CharSet.Unicode)] struct Startup {public int cb;public string reserved,desktop,title;public int x,y,xSize,ySize,xCount,yCount,fill,flags;public short show,reserved2;public IntPtr bytes,input,output,error;}
 [StructLayout(LayoutKind.Sequential)] struct ProcessInfo {public IntPtr process,thread;public uint pid,tid;}
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateProcessW(string app,StringBuilder command,IntPtr pa,IntPtr ta,bool inherit,uint flags,IntPtr environment,string directory,ref Startup startup,out ProcessInfo info);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool CloseHandle(IntPtr handle);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern uint GetProcessId(IntPtr handle);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool GetProcessTimes(IntPtr handle,out long created,out long exit,out long kernel,out long user);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool TerminateProcess(IntPtr handle,uint code);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern uint WaitForSingleObject(IntPtr handle,uint ms);
 [DllImport("kernel32.dll",SetLastError=true)] static extern bool IsWow64Process2(IntPtr handle,out ushort process,out ushort native);
 [DllImport("kernel32.dll")] static extern IntPtr GetCurrentProcess();
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern uint GetSystemDirectoryW(StringBuilder path,uint length);
 [DllImport("shell32.dll")] static extern int SHGetKnownFolderPath(ref Guid id,uint flags,IntPtr token,out IntPtr path);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] public static extern SafeFileHandle CreateFileW(string path,uint access,uint share,IntPtr sa,uint disposition,uint flags,IntPtr template);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetFileInformationByHandle(SafeFileHandle h,out Info info);
 [DllImport("kernel32.dll",SetLastError=true)] public static extern bool GetFileInformationByHandleEx(SafeFileHandle h,int kind,out uint flags,uint size);
 [DllImport("kernel32.dll")] public static extern uint GetFileType(SafeFileHandle h);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode)] static extern uint GetDriveTypeW(string path);
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool GetVolumeNameForVolumeMountPointW(string path,StringBuilder volume,uint length);
 [DllImport("advapi32.dll",SetLastError=true)] static extern uint GetSecurityInfo(SafeFileHandle h,int kind,uint information,IntPtr owner,IntPtr group,IntPtr dacl,IntPtr sacl,out IntPtr sd);
 [DllImport("advapi32.dll")] static extern uint GetSecurityDescriptorLength(IntPtr sd);
 [DllImport("kernel32.dll")] static extern IntPtr LocalFree(IntPtr pointer);
 [DllImport("msi.dll",CharSet=CharSet.Unicode)] public static extern int MsiQueryProductStateW(string product);
 public static bool Architecture(int size,ushort process,ushort native){return size==8 && process==0 && native==0x8664;}
 public static string ProgramFiles(){ushort p,n;if(!IsWow64Process2(GetCurrentProcess(),out p,out n) || !Architecture(IntPtr.Size,p,n))throw new InvalidOperationException("Native x64 required");
  Guid id=new Guid("905e63b6-c1bf-494e-b29c-65b732d3d21a");IntPtr value=IntPtr.Zero;
  try{if(SHGetKnownFolderPath(ref id,0,IntPtr.Zero,out value)!=0 || value==IntPtr.Zero)throw new IOException("Known folder unavailable");string path=Marshal.PtrToStringUni(value);if(String.IsNullOrEmpty(path) || Path.GetFullPath(path)!=path || path.Length<4 || path[1]!=':' || path[2]!='\\')throw new IOException("Known folder invalid");return path;}finally{if(value!=IntPtr.Zero)Marshal.FreeCoTaskMem(value);}
 }
 public static string SystemExe(){var path=new StringBuilder(32768);uint n=GetSystemDirectoryW(path,32768);if(n==0 || n>=32768 || !String.Equals(path.ToString(),"C:\\Windows\\System32",StringComparison.OrdinalIgnoreCase))throw new IOException("Native system path refused");return "C:\\Windows\\System32\\msiexec.exe";}
 public static string VolumePath(string path){string root=path.Substring(0,3);var volume=new StringBuilder(128);if(GetDriveTypeW(root)!=3 || !GetVolumeNameForVolumeMountPointW(root,volume,128))throw new IOException("Fixed volume required");return volume+path.Substring(3);}
 public static CheckpointIdentity Identity(IntPtr handle){uint pid=GetProcessId(handle);long created,exit,kernel,user;if(pid==0 || !GetProcessTimes(handle,out created,out exit,out kernel,out user) || created==0)throw new IOException("Process identity unavailable");return new CheckpointIdentity(pid,created);}
 public static bool Alive(IntPtr handle){uint result=WaitForSingleObject(handle,0);if(result==258)return true;if(result==0)return false;throw new IOException("Process wait query failed");}
 public static void Wait(IntPtr handle){if(WaitForSingleObject(handle,0xffffffff)!=0)throw new IOException("Process completion unknown");}
 public static IntPtr Launch(string exe,string msi,string log){var startup=new Startup();startup.cb=Marshal.SizeOf(startup);ProcessInfo info;
  var command=new StringBuilder("\""+exe+"\" /i \""+msi+"\" /qn /norestart /L*V \""+log+"\"");
  if(!CreateProcessW(exe,command,IntPtr.Zero,IntPtr.Zero,false,0,IntPtr.Zero,null,ref startup,out info))throw new IOException("Client launch failed");
  CloseHandle(info.thread);return info.process;
 }
 static bool Trusted(SecurityIdentifier sid){return sid!=null && (sid.Value=="S-1-5-18" || sid.Value=="S-1-5-32-544" || sid.Value=="S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464");}
 public static void Descriptor(SafeFileHandle handle){IntPtr sd;uint e=GetSecurityInfo(handle,1,5,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,IntPtr.Zero,out sd);if(e!=0)throw new IOException("Descriptor query failed");
  try{uint length=GetSecurityDescriptorLength(sd);if(length==0 || length>65536)throw new IOException("Descriptor bounds");var bytes=new byte[length];Marshal.Copy(sd,bytes,0,bytes.Length);var raw=new RawSecurityDescriptor(bytes,0);
   if(!Trusted(raw.Owner) || (raw.ControlFlags&ControlFlags.DiscretionaryAclProtected)==0 || raw.DiscretionaryAcl==null)throw new IOException("Protected descriptor required");
   foreach(GenericAce generic in raw.DiscretionaryAcl){var ace=generic as CommonAce;if(ace==null || ace.IsCallback || (ace.AceQualifier!=AceQualifier.AccessAllowed && ace.AceQualifier!=AceQualifier.AccessDenied))throw new IOException("Unknown ACE");
    if(ace.AceQualifier==AceQualifier.AccessDenied || (ace.AceFlags&AceFlags.InheritOnly)!=0)continue;
    // All generic write/all, maximum allowed and file mutation/control rights.
    if(!Trusted(ace.SecurityIdentifier) && (unchecked((uint)ace.AccessMask)&0x520d0156u)!=0)throw new IOException("Untrusted mutation rights");
   }
  }finally{if(sd!=IntPtr.Zero)LocalFree(sd);}
 }
}
public sealed class CheckpointPin : IDisposable {
 public readonly SafeFileHandle Handle;readonly CheckpointNative.Info identity;readonly bool directory,authority;
 public CheckpointPin(string path,bool dir,bool protectedAcl){directory=dir;authority=protectedAcl;
  Handle=CheckpointNative.CreateFileW(path,(dir?0x80u:0x80000000u)|0x20000u,1,IntPtr.Zero,3,0x02200000,IntPtr.Zero);
  if(Handle.IsInvalid){int error=Marshal.GetLastWin32Error();Handle.Dispose();throw new System.ComponentModel.Win32Exception(error);}
  try{identity=Check();}catch{Handle.Dispose();throw;}
 }
 CheckpointNative.Info Check(){CheckpointNative.Info info;uint flags;
  if(CheckpointNative.GetFileType(Handle)!=1 || !CheckpointNative.GetFileInformationByHandle(Handle,out info) || (info.Attributes&0x400)!=0 || ((info.Attributes&16)!=0)!=directory || (!directory && info.Links!=1) ||
   (directory && (!CheckpointNative.GetFileInformationByHandleEx(Handle,23,out flags,4) || flags!=0)))throw new IOException("Unsafe file metadata/case");
  if(authority)CheckpointNative.Descriptor(Handle);return info;
 }
 public void Revalidate(){var current=Check();if(current.Volume!=identity.Volume || current.IndexHigh!=identity.IndexHigh || current.IndexLow!=identity.IndexLow)throw new IOException("Pin identity changed");}
 public bool Same(CheckpointPin other){return identity.Volume==other.identity.Volume && identity.IndexHigh==other.identity.IndexHigh && identity.IndexLow==other.identity.IndexLow;}
 public byte[] Read(int max){Revalidate();using(var stream=new FileStream(new SafeFileHandle(Handle.DangerousGetHandle(),false),FileAccess.Read)){
  stream.Position=0;if(stream.Length<0 || stream.Length>max)throw new IOException("File bounds");var bytes=new byte[(int)stream.Length];int offset=0;while(offset<bytes.Length){int n=stream.Read(bytes,offset,bytes.Length-offset);if(n==0)throw new IOException("Short read");offset+=n;}Revalidate();return bytes;
 }}
 public void Dispose(){Handle.Dispose();}
}
public sealed class CheckpointOperation : IDisposable {
 readonly List<IDisposable> pins=new List<IDisposable>();readonly List<CheckpointPin> checks=new List<CheckpointPin>();
 readonly string root,state,exe; CheckpointPin manifest,msi,checkpoint,pending;bool disposed;
 public readonly string RunnerPath,LogPath;
 CheckpointPin Add(string path,bool directory,bool authority){var pin=new CheckpointPin(path,directory,authority);pins.Add(pin);checks.Add(pin);return pin;}
 void Chain(string path,string anchor,bool protectedAcl=true){int end=path.IndexOf("}\\",StringComparison.Ordinal)+2;if(end<3)throw new IOException("Volume path required");bool authority=false;
  while(true){string part=path.Substring(0,end);if(part==anchor)authority=true;Add(part,true,authority && protectedAcl);if(end==path.Length)break;end=path.IndexOf('\\',end+1);if(end<0)end=path.Length;}
  if(!authority)throw new IOException("Anchor missing");
 }
 public CheckpointOperation(string runner,string expectedRunner,string expectedManifest,string expectedMsi){try{
  string known=CheckpointNative.ProgramFiles();RunnerPath=known+"\\BimeronCheckpointClientLab\\Run-CheckpointLab.ps1";
  if(runner!=RunnerPath || expectedManifest.ToLowerInvariant()!=CheckpointFormat.ManifestHash || expectedMsi.ToLowerInvariant()!=CheckpointFormat.MsiHash)throw new IOException("Fixed input pins refused");
  string anchor=CheckpointNative.VolumePath(known);root=anchor+"\\BimeronCheckpointClientLab";state=anchor+"\\BimeronProtectedAckLab\\LABProtectedFiles";
  LogPath=root+"\\checkpoint-"+Guid.NewGuid().ToString("N")+".log";
  var independent=Add(anchor,true,true);Chain(root,anchor);Chain(state,anchor);
  // The independently opened known folder must be the traversal anchor object.
  bool matched=false;foreach(var pin in checks)if(!Object.ReferenceEquals(pin,independent) && pin.Same(independent))matched=true;if(!matched)throw new IOException("Known folder mismatch");
  var self=Add(root+"\\Run-CheckpointLab.ps1",false,true);if(CheckpointFormat.Hash(self.Read(1048576))!=expectedRunner.ToLowerInvariant())throw new IOException("Runner digest refused");
  manifest=Add(root+"\\run.manifest",false,true);CheckpointFormat.Manifest(manifest.Read(65536));
  var protectedManifest=Add(state+"\\run.manifest",false,true);CheckpointFormat.Manifest(protectedManifest.Read(65536));
  msi=Add(root+"\\lab-protected-ack-Checkpoint.msi",false,true);if(CheckpointFormat.Hash(msi.Read(16777216))!=CheckpointFormat.MsiHash)throw new IOException("MSI digest refused");
  exe=CheckpointNative.SystemExe();string systemVolume=CheckpointNative.VolumePath(exe);Chain(systemVolume.Substring(0,systemVolume.LastIndexOf('\\')),CheckpointNative.VolumePath("C:\\Windows"),false);Add(systemVolume,false,false);
  Fresh();
 }catch{Dispose();throw;}}
 static void Absent(string path){using(var h=CheckpointNative.CreateFileW(path,0x80,7,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){
  int error=Marshal.GetLastWin32Error();if(!h.IsInvalid || error!=2)throw new IOException("Fresh absence not established");}}
 void Revalidate(){foreach(var pin in checks)pin.Revalidate();CheckpointFormat.Manifest(manifest.Read(65536));}
 void Fresh(){Revalidate();Absent(state+"\\pending.bin");Absent(state+"\\checkpoint.bin");if(CheckpointNative.MsiQueryProductStateW(CheckpointFormat.Product)!=-1)throw new IOException("Fresh product state required");}
 public IntPtr Launch(){Fresh();string log=LogPath;
  // Reserve only this unique output; retain the log pin through late completion.
  var stream=new FileStream(log,FileMode.CreateNew,FileAccess.ReadWrite,FileShare.ReadWrite);pins.Add(stream);
  return CheckpointNative.Launch(exe,root+"\\lab-protected-ack-Checkpoint.msi",log);
 }
 public bool Ready(IntPtr ignored){
  Revalidate();
  if(checkpoint==null){try{checkpoint=Add(state+"\\checkpoint.bin",false,true);}catch(System.ComponentModel.Win32Exception e){if(e.NativeErrorCode==2 || e.NativeErrorCode==32)return false;throw;}}
  if(pending==null)pending=Add(state+"\\pending.bin",false,true);
  CheckpointFormat.Records(checkpoint.Read(80),pending.Read(48));Revalidate();return true;
 }
 public CheckpointWorker Worker(){var clock=Stopwatch.StartNew();long deadline=300000;
  return new CheckpointWorker(delegate{return clock.ElapsedMilliseconds;},deadline,Launch,CheckpointNative.Identity,CheckpointNative.Alive,Ready,
   delegate(IntPtr h){return CheckpointNative.TerminateProcess(h,1603);},delegate(IntPtr h){return CheckpointNative.WaitForSingleObject(h,0)==0;},CheckpointNative.Wait,
   delegate(IntPtr h){CheckpointNative.CloseHandle(h);},new IDisposable[]{this});
 }
 public void Dispose(){if(disposed)return;disposed=true;for(int i=pins.Count-1;i>=0;i--)pins[i].Dispose();}
}
"@
}
# Tests extract only the function AST above; no test/alternate dispatch parameter.
if ($LabOnlyAllow -cne 'DisposableWindowsVM') { throw 'Exact lab acknowledgment required.' }
if ($env:OS -cne 'Windows_NT') { throw 'Native Windows required.' }
Initialize-CheckpointLab
$vm = @(Get-CimInstance -ClassName Win32_ComputerSystem -ErrorAction Stop)
if ($vm.Count -ne 1 -or $vm[0].Manufacturer -cne 'Microsoft Corporation' -or $vm[0].Model -cne 'Virtual Machine') { throw 'Positive Microsoft Hyper-V classification required.' }
$operation = [CheckpointOperation]::new($PSCommandPath,$ExpectedRunnerSha256,$ExpectedManifestSha256,$ExpectedMsiSha256)
$transferred = $false
try {
 $worker = $operation.Worker()
 $worker.Start(); $transferred = $true
 $finished = $worker.Observe($worker.Remaining)
 $snapshot = if ($finished) { $worker.Snapshot } else { $worker.Timeout() }
 [ordered]@{labOnly=$true;logPath=$operation.LogPath;decision=$snapshot.Decision;outcome=$snapshot.Outcome;completed=$snapshot.Completed;terminationApi=$snapshot.ApiResult;clientDeath=$snapshot.ClientDeath;possibleLateOperation=$snapshot.PossibleLateOperation;latency=$snapshot.Latency;serverQuiescence=$snapshot.ServerQuiescence;initiatingClient='Unknown';clean='Unknown';childPid=$(if($null -eq $snapshot.Identity){$null}else{$snapshot.Identity.Pid});childCreatedRawFileTime=$(if($null -eq $snapshot.Identity){$null}else{$snapshot.Identity.Created.ToString('x16')});runnerHash=$ExpectedRunnerSha256;manifestHash=$ExpectedManifestSha256;msiHash=$ExpectedMsiSha256} | ConvertTo-Json -Compress
 Write-Output 'Preserve VM and all state. Stop orchestration: client death is not MSI server quiescence. No retry, uninstall or cleanup. Worker retains pins through late completion; host death loses them.'
} finally { if (-not $transferred) { $operation.Dispose() } }
