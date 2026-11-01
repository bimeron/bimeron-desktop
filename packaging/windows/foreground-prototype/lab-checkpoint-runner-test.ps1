#requires -Version 5.1
# Default: exact worker/interop models without native calls. -NativeReadOnly: separate OS reads and owned file fixture; no process/MSI dispatch.
param([switch]$NativeReadOnly)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$runner=Join-Path $PSScriptRoot 'Run-CheckpointLab.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($runner,[ref]$tokens,[ref]$errors)
if($errors.Count){throw 'Runner parse failure'}
$fn=$ast.Find({param($n) $n -is [Management.Automation.Language.FunctionDefinitionAst] -and $n.Name -ceq 'Initialize-CheckpointLab'},$true)
$add=$fn.Body.Find({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -ceq 'Add-Type'},$true)
$source=$add.CommandElements[2].Value
$fake = @'
public sealed class CheckpointMock : IDisposable {
 public long Time,CallAt;public int Calls,Queries,Closes,Disposals,Waits,ExitQueries;public bool ExitThrows,ExitFails;public string Order="";public IntPtr Killed;
 public IDisposable Held;public bool IsAlive=true,IdentityBad,IdentityError,ApiError,ApiThrows,Signaled=true,Disposed,WaitThrows,DeathThrows,LaunchThrows;
 public readonly ManualResetEvent LaunchEntered=new ManualResetEvent(false),LaunchPermit=new ManualResetEvent(true),ReadyEntered=new ManualResetEvent(false),ReadyPermit=new ManualResetEvent(true),KillEntered=new ManualResetEvent(false),KillPermit=new ManualResetEvent(true),WaitEntered=new ManualResetEvent(false),WaitPermit=new ManualResetEvent(true);
 public Action BeforeReady,FinalQuery;
 public IntPtr Launch(){LaunchEntered.Set();LaunchPermit.WaitOne();if(LaunchThrows)throw new Exception("Launch before process");return new IntPtr(1234);}
 public void Check(IntPtr h){if(h!=new IntPtr(1234))throw new Exception("Wrong retained handle");}
 public CheckpointIdentity Query(IntPtr h){Check(h);Queries++;if(IdentityError)throw new Exception("Query");if(Queries==2 && FinalQuery!=null)FinalQuery();return new CheckpointIdentity(IdentityBad && Queries>1?8u:7u,42);}
 public bool Alive(IntPtr h){Check(h);return IsAlive;}
 public bool Ready(IntPtr h){Check(h);ReadyEntered.Set();ReadyPermit.WaitOne();if(BeforeReady!=null)BeforeReady();return true;}
 public bool Kill(IntPtr h){Check(h);Killed=h;KillEntered.Set();KillPermit.WaitOne();Calls++;CallAt=Time;if(ApiThrows)throw new Exception("API");return !ApiError;}
 public bool Death(IntPtr h){Check(h);Order+="signal;";if(DeathThrows)throw new Exception("Death query");return Signaled;}
 public void Wait(IntPtr h){Check(h);Waits++;WaitEntered.Set();WaitPermit.WaitOne();if(WaitThrows)throw new Exception("Wait");}
 public CheckpointExit Exit(IntPtr h){Check(h);ExitQueries++;Order+="exit;";if(ExitThrows)throw new Exception("Exit query");return ExitFails?new CheckpointExit(null,"Failed",5):new CheckpointExit(2,"Succeeded",null);}
 public void Close(IntPtr h){Check(h);Order+="close;";Closes++;if(Disposed)throw new Exception("Pins released before handle");}
 public void Dispose(){Disposals++;if(Held!=null)Held.Dispose();Disposed=true;}
 public CheckpointWorker Worker(){return new CheckpointWorker(delegate{return Time;},300000,Launch,Query,Alive,Ready,Kill,Death,Wait,Close,new IDisposable[]{this},Exit);}
}
public static class CheckpointTests {
 static int checks;
 static void Assert(bool value,string message){checks++;if(!value)throw new Exception(message);}
 static void Await(ManualResetEvent e){Assert(e.WaitOne(3000),"Mock barrier timed out");}
 static void Finish(CheckpointWorker w,CheckpointMock m){m.LaunchPermit.Set();m.ReadyPermit.Set();m.KillPermit.Set();m.WaitPermit.Set();Assert(w.Observe(3000),"Mock worker did not finish");Assert(m.Disposed && m.Disposals==1 && m.Closes==(m.LaunchThrows?0:1) && w.Snapshot.Completed,"Owned resources not released exactly once");}
 static void Reject(Action action){bool rejected=false;try{action();}catch{rejected=true;}Assert(rejected,"Expected rejection");}
 public static string Models(){
  int before=checks;var lab=CheckpointPinRole.LabProtected;var os=CheckpointPinRole.TrustedOS;var image=CheckpointPinRole.InternalSystemImagePin;
  string volume="\\\\?\\Volume{fixture}\\",anchor=volume+"Program Files",owned=anchor+"\\BimeronCheckpointClientLabBoundary";
  Assert(CheckpointOperation.ChainRole(volume,anchor,owned)==CheckpointPinRole.TraversalOnly,"Volume traversal");
  Assert(CheckpointOperation.ChainRole(anchor,anchor,owned)==os,"Exact OS anchor");
  Assert(CheckpointOperation.ChainRole(owned,anchor,owned)==lab,"Owned lab root");
  Assert(CheckpointOperation.ChainRole(owned+"\\leaf",anchor,owned)==lab,"Owned lab descendant");
  Reject(delegate{CheckpointOperation.ChainRole(anchor+"\\Unowned",anchor,owned);});
  Reject(delegate{CheckpointOperation.ChainRole(owned+"Extra",anchor,owned);});
  Assert(CheckpointOperation.ChainRole(anchor+"\\image",anchor,null)==CheckpointPinRole.OSContent,"OS content distinct from anchor");
  var info=new CheckpointNative.Info();
  foreach(uint links in new uint[]{0,1,2,3}){info.Links=links;var value=info;
   if(links==1)CheckpointPin.Metadata(value,false,lab,0);else Reject(delegate{CheckpointPin.Metadata(value,false,lab,0);});
   if(links==0)Reject(delegate{CheckpointPin.Metadata(value,false,image,0);});else {CheckpointPin.Metadata(value,false,image,0);Assert(true,"System link positive");}
  }
  info.Links=2;info.Attributes=0x400;var reparse=info;Reject(delegate{CheckpointPin.Metadata(reparse,false,image,0);});
  info.Attributes=16;var dir=info;Reject(delegate{CheckpointPin.Metadata(dir,false,image,0);});Reject(delegate{CheckpointPin.Metadata(dir,true,os,1);});
  Reject(delegate{new CheckpointPin("C:\\arbitrary.exe",false,image);});
  string ti="S-1-5-80-956008885-3418522649-1831038044-1853292631-2271478464";
  foreach(string owner in new string[]{"SY","BA",ti}){
   var safe=new RawSecurityDescriptor("O:"+owner+"G:SYD:(A;ID;FR;;;BU)(A;;FA;;;SY)(A;;FA;;;BA)");
   CheckpointNative.ValidateDescriptor(safe,os);CheckpointNative.ValidateDescriptor(safe,image);Assert(true,"Safe inherited OS descriptor");
   Reject(delegate{CheckpointNative.ValidateDescriptor(safe,lab);});
   CheckpointNative.ValidateDescriptor(new RawSecurityDescriptor("O:"+owner+"G:SYD:P(A;;FA;;;SY)"),lab);
  }
  foreach(string bad in new string[]{"O:BUG:SYD:P(A;;FA;;;SY)","O:SYG:SY","O:SYG:SYD:NO_ACCESS_CONTROL","O:SYG:SYD:(A;;FW;;;BU)","O:SYG:SYD:(A;;WD;;;BU)","O:SYG:SYD:(A;;DC;;;BU)","O:SYG:SYD:(A;ID;FA;;;BU)","O:SYG:SYD:(AU;SA;FR;;;BU)","O:SYG:SYD:(A;;GA;;;BU)","O:SYG:SYD:(A;;GW;;;BU)","O:SYG:SYD:(A;;0x2000000;;;BU)","O:SYG:SYD:(A;;WO;;;BU)","O:SYG:SYD:(A;;SD;;;BU)","O:S-1-5-80-1G:SYD:P(A;;FA;;;SY)"}){
   try{Reject(delegate{CheckpointNative.ValidateDescriptor(new RawSecurityDescriptor(bad),os);});}catch(Exception e){throw new Exception(bad,e);}
  }
  var unknown=new RawSecurityDescriptor("O:SYG:SYD:(A;;FR;;;BU)");
  unknown.DiscretionaryAcl.InsertAce(0,new ObjectAce(AceFlags.None,AceQualifier.AccessAllowed,1,new SecurityIdentifier("S-1-5-32-545"),ObjectAceFlags.ObjectAceTypePresent,Guid.NewGuid(),Guid.Empty,false,null));
  Reject(delegate{CheckpointNative.ValidateDescriptor(unknown,os);});
  var callback=new RawSecurityDescriptor("O:SYG:SYD:(A;;FR;;;BU)");callback.DiscretionaryAcl.InsertAce(0,new CommonAce(AceFlags.InheritOnly,AceQualifier.AccessAllowed,1,new SecurityIdentifier("S-1-5-32-545"),true,new byte[0]));Reject(delegate{CheckpointNative.ValidateDescriptor(callback,os);});
  CheckpointNative.ValidateDescriptor(new RawSecurityDescriptor("O:SYG:SYD:(A;CIIO;FA;;;BU)(A;;FR;;;BU)"),os);Assert(true,"Inherit-only does not apply to current object");
  return "PASS MODEL: "+(checks-before)+" role/link/metadata/descriptor assertions; no native calls.";
 }
 public static string Aliases(){int before=checks;
  var original=new CheckpointNative.Info();original.Volume=7;original.IndexHigh=8;original.IndexLow=9;
  CheckpointNative.SameInfo(original,original);Assert(true,"Matching alias identity");
  foreach(string role in new string[]{"anchor","MSI","log"}){var other=original;other.IndexLow++;Reject(delegate{CheckpointNative.SameInfo(original,other);});other=original;other.Volume++;Reject(delegate{CheckpointNative.SameInfo(original,other);});}
  CheckpointNative.FixedDrive(3);Assert(true,"Fixed drive positive");foreach(uint type in new uint[]{0,1,2,4,5,6})Reject(delegate{CheckpointNative.FixedDrive(type);});
  int launches=0;Action<Func<string>,Func<string,string>> validate=delegate(Func<string> known,Func<string,string> volume){CheckpointNative.Mapping("C:\\Program Files","GUID",known,volume);launches++;};
  validate(delegate{return "C:\\Program Files";},delegate(string p){return "GUID";});Assert(launches==1,"Mapping positive");
  Reject(delegate{validate(delegate{return "D:\\Program Files";},delegate(string p){return "GUID";});});
  Reject(delegate{validate(delegate{return "C:\\Program Files";},delegate(string p){return "CHANGED";});});
  foreach(string failure in new string[]{"fixed drive","volume query","known folder query"})Reject(delegate{validate(delegate{if(failure=="known folder query")throw new IOException(failure);return "C:\\Program Files";},delegate(string p){throw new IOException(failure);});});
  Assert(launches==1,"Mapping/query refusal launched");return "PASS ALIAS MODEL: "+(checks-before)+" identity/mapping assertions; no native calls.";
 }
 public static string Run(byte[] manifest){
  Assert(Marshal.SizeOf(typeof(CheckpointNative.Info))==52 && Marshal.OffsetOf(typeof(CheckpointNative.Info),"Volume").ToInt32()==28,"Native file-information layout");
  CheckpointFormat.Manifest(manifest);
  foreach(int offset in new int[]{0,8,24,40,44,48,64,80,112,144,176,9511}){byte[] bad=(byte[])manifest.Clone();bad[offset]^=1;Reject(delegate{CheckpointFormat.Manifest(bad);});}
  Reject(delegate{CheckpointFormat.Manifest(new byte[65537]);});Reject(delegate{CheckpointFormat.Manifest(new byte[176]);});
  byte[] cp=CheckpointFormat.Checkpoint(),pending=CheckpointFormat.Pending();CheckpointFormat.Records(cp,pending);
  foreach(int offset in new int[]{0,4,7,8,24,40,72,76,79}){byte[] bad=(byte[])cp.Clone();bad[offset]^=1;Reject(delegate{CheckpointFormat.Records(bad,pending);});}
  foreach(int offset in new int[]{0,4,5,6,8,24,40,44,47}){byte[] bad=(byte[])pending.Clone();bad[offset]^=1;Reject(delegate{CheckpointFormat.Records(cp,bad);});}
  Reject(delegate{CheckpointFormat.Records(new byte[79],pending);});Reject(delegate{CheckpointFormat.Records(cp,new byte[49]);});
  Assert(CheckpointNative.Architecture(8,0,0x8664),"x64");Assert(!CheckpointNative.Architecture(4,0,0x8664) && !CheckpointNative.Architecture(8,0x8664,0xaa64),"Architecture refusal");
  var m=new CheckpointMock();m.LaunchPermit.Reset();m.WaitPermit.Reset();var w=m.Worker();w.Start();Await(m.LaunchEntered);
  var cancelled=w.Timeout();Assert(cancelled.Decision=="Cancelled" && !m.Disposed && !w.Observe(0),"Cancel/pin ownership");m.LaunchPermit.Set();Await(m.WaitEntered);
  Assert(m.Calls==0 && !m.Disposed && w.Snapshot.Identity.Pid==7,"Late launch/pins");Finish(w,m);Assert(cancelled.Outcome=="Cancelled","Snapshot mutated");
  m=new CheckpointMock();m.KillPermit.Reset();m.WaitPermit.Reset();w=m.Worker();w.Start();Await(m.KillEntered);
  var before=w.Snapshot;var watch=Stopwatch.StartNew();var committed=w.Timeout();
  Assert(watch.ElapsedMilliseconds<1000 && committed.Decision=="Committed" && committed.Outcome=="TerminationDecisionInFlight" && committed.PossibleLateOperation && committed.Latency=="Unknown","Commit timeout");
  Assert(!m.Disposed && m.Calls==0 && m.Killed==new IntPtr(1234),"Retained handle/pins");
  m.Time=900000;m.KillPermit.Set();Await(m.WaitEntered);Assert(m.Calls==1 && m.CallAt==900000 && !m.Disposed,"Postcommit preemption");Finish(w,m);
  Assert(before.Outcome=="TerminationDecisionInFlight" && !before.Completed,"Immutable snapshot");
  Assert(w.Snapshot.ApiResult=="Succeeded" && w.Snapshot.ClientDeath=="Signaled" && w.Snapshot.ServerQuiescence=="Unknown","Death/server separation");
  m=new CheckpointMock();m.ReadyPermit.Reset();m.WaitPermit.Reset();w=m.Worker();w.Start();Await(m.ReadyEntered);w.Timeout();m.ReadyPermit.Set();Await(m.WaitEntered);Assert(m.Calls==0,"Cancel race");Finish(w,m);
  foreach(long expiry in new long[]{299999,300000,300001}){m=new CheckpointMock();var selected=m;m.FinalQuery=delegate{selected.Time=expiry;};w=m.Worker();w.Start();Finish(w,m);Assert(m.Calls==(expiry<300000?1:0),"Strict final expiry");}
  foreach(string bad in new string[]{"early","identity","query","records","manifest"}){
   m=new CheckpointMock();if(bad=="early")m.IsAlive=false;if(bad=="identity")m.IdentityBad=true;if(bad=="query")m.IdentityError=true;
   if(bad=="records")m.BeforeReady=delegate{CheckpointFormat.Records(new byte[80],pending);};
   if(bad=="manifest")m.BeforeReady=delegate{CheckpointFormat.Manifest(new byte[9512]);};
   w=m.Worker();w.Start();Finish(w,m);Assert(m.Calls==0,"Unsafe check: "+bad);
  }
  foreach(string result in new string[]{"error","throws"}){m=new CheckpointMock();m.ApiError=result=="error";m.ApiThrows=result=="throws";w=m.Worker();w.Start();Finish(w,m);Assert(m.Calls==1,"Retry on API failure/unknown death");if(result=="error")Assert(w.Snapshot.ApiResult=="Failed","API error lost");if(result=="throws")Assert(w.Snapshot.ApiResult=="Unknown","API exception mislabeled");}
  foreach(string failure in new string[]{"wait","unconfirmed","deathQuery","identityWait","identityUnconfirmed","finalQuery"}){
   m=new CheckpointMock();m.Signaled=false;m.WaitThrows=failure=="wait" || failure=="identityWait";m.DeathThrows=failure=="deathQuery";m.IdentityError=failure.StartsWith("identity");if(failure=="finalQuery")m.FinalQuery=delegate{throw new Exception("Final identity query");};
   w=m.Worker();w.Start();Assert(w.Observe(3000),"Unresolved worker blocked");
   Assert(w.Snapshot.Outcome=="Unresolved" && !w.Snapshot.Completed && w.Snapshot.PossibleLateOperation && w.Snapshot.ClientDeath=="Unconfirmed","Unresolved observation");
   Assert(m.Calls==(m.IdentityError || failure=="finalQuery"?0:1) && m.Closes==0 && m.Disposals==0 && m.Waits==1 && m.ExitQueries==0,"Unresolved ownership/retry");
   var unresolved=w.Snapshot;Assert(Object.ReferenceEquals(w.Timeout(),unresolved),"Timeout erased unresolved state");
   var weak=new WeakReference(w);w=null;GC.Collect();GC.WaitForPendingFinalizers();GC.Collect();
   Assert(weak.IsAlive && !m.Disposed,"Pending owner lost to GC");
   var owners=(List<CheckpointWorker>)typeof(CheckpointWorker).GetField("pendingOwners",System.Reflection.BindingFlags.Static|System.Reflection.BindingFlags.NonPublic).GetValue(null);
   Assert(owners.Contains((CheckpointWorker)weak.Target),"Worker not explicitly rooted");
   Assert((IntPtr)typeof(CheckpointWorker).GetField("pendingHandle",System.Reflection.BindingFlags.Instance|System.Reflection.BindingFlags.NonPublic).GetValue(weak.Target)==new IntPtr(1234),"Actual handle not retained");
  }
  m=new CheckpointMock();m.LaunchThrows=true;w=m.Worker();w.Start();Finish(w,m);Assert(m.Calls==0 && m.Waits==0 && m.ExitQueries==0 && w.Snapshot.Exit.Status=="NotQueried","Preprocess launch failure performed operation");
  m=new CheckpointMock();m.WaitThrows=true;w=m.Worker();w.Start();Finish(w,m);Assert(w.Snapshot.ClientDeath=="Signaled","Confirmed death lost after wait error");
  m=new CheckpointMock();m.IsAlive=false;w=m.Worker();w.Start();Finish(w,m);
  Assert(m.ExitQueries==1 && m.Order.EndsWith("signal;exit;close;") && w.Snapshot.Exit.Code==2 && w.Snapshot.Exit.Status=="Succeeded","Early exit same-handle signal/query/close ordering");
  m=new CheckpointMock();m.ExitThrows=true;w=m.Worker();w.Start();Finish(w,m);
  Assert(m.ExitQueries==1 && m.Closes==1 && m.Disposals==1 && w.Snapshot.Exit.Code==null && w.Snapshot.Exit.Status=="Failed" && w.Snapshot.ClientDeath=="Signaled","Exit query failure changed confirmed release");
  m=new CheckpointMock();m.ExitFails=true;w=m.Worker();w.Start();Finish(w,m);
  Assert(m.ExitQueries==1 && w.Snapshot.Completed && w.Snapshot.Exit.Code==null && w.Snapshot.Exit.Error==5 && w.Snapshot.Exit.Status=="Failed","Native-style exit error diagnostic/release");
  // A real owned temporary file proves the worker retains the supplied pin across cancellation.
  string temp=Path.GetTempFileName();
  try {
   m=new CheckpointMock();var held=new FileStream(temp,FileMode.Open,FileAccess.Read,FileShare.Read);m.Held=held;m.LaunchPermit.Reset();m.WaitPermit.Reset();w=m.Worker();w.Start();Await(m.LaunchEntered);w.Timeout();
   Assert(held.CanRead,"File pin released at observer timeout");Reject(delegate{using(var writer=new FileStream(temp,FileMode.Open,FileAccess.Write,FileShare.None)){};});
   m.LaunchPermit.Set();Await(m.WaitEntered);Assert(held.CanRead && m.Calls==0,"Late client lost real pin");Finish(w,m);Assert(!held.CanRead,"File pin leaked after operation");
  }finally{File.Delete(temp);}
  m=new CheckpointMock();m.Time=300000;w=m.Worker();w.Start();Finish(w,m);Assert(m.Calls==0 && m.Queries==1,"Initially expired worker committed");
  Assert(299999L<300000L && 299999L+2>=300000L,"Historical scheduling witness");
  return "PASS: "+checks+" assertions; exact worker mocks; frozen MF/BLCP/Pending negatives; cancel/commit/expiry/preemption/late launch/early exit/identity/API/death/pins. No native APIs, real process launch/termination, MSI or VM.";
 }
}

public static class CheckpointNativeReadOnlyTests {
 [DllImport("kernel32.dll",CharSet=CharSet.Unicode,SetLastError=true)] static extern bool CreateHardLinkW(string link,string target,IntPtr attributes);
 static void Require(bool value,string name){if(!value)throw new IOException(name);}
 static void Api(bool value,string name){if(!value)throw new System.ComponentModel.Win32Exception(Marshal.GetLastWin32Error(),name);}
 public static string Run(){
  var report=new StringBuilder("NATIVE READ-ONLY SYSTEM: no operation constructor, launch, termination or MSI calls.\n");
  string known=CheckpointNative.ProgramFiles(),anchor=CheckpointNative.VolumePath(known);
  CheckpointNative.Mapping(known,anchor,CheckpointNative.ProgramFiles,CheckpointNative.VolumePath);
  using(var dos=CheckpointNative.CreateFileW(known,0x20080,1,IntPtr.Zero,3,0x02200000,IntPtr.Zero))using(var guid=CheckpointNative.CreateFileW(anchor,0x20080,1,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){
   Api(!dos.IsInvalid && !guid.IsInvalid,"Known folder DOS/GUID opens");CheckpointNative.Info a,b;Api(CheckpointNative.GetFileInformationByHandle(dos,out a),"Known DOS metadata");Api(CheckpointNative.GetFileInformationByHandle(guid,out b),"Known GUID metadata");CheckpointNative.SameInfo(a,b);report.AppendLine("Known folder fixed mapping "+known+" -> "+anchor+" volume="+a.Volume+" id="+a.IndexHigh+":"+a.IndexLow);
  }
  string exe=CheckpointNative.SystemExe();string volume=CheckpointNative.VolumePath(exe);
  var held=new List<CheckpointPin>();
  try{
   int end=volume.IndexOf("}\\",StringComparison.Ordinal)+2;
   while(true){string path=volume.Substring(0,end);var role=path==CheckpointNative.VolumePath("C:\\Windows") || path==CheckpointNative.VolumePath("C:\\Windows\\System32")?CheckpointPinRole.TrustedOS:CheckpointPinRole.TraversalOnly;
    var pin=new CheckpointPin(path,true,role);held.Add(pin);pin.Revalidate();CheckpointNative.Info ancestor;uint caseFlags;Api(CheckpointNative.GetFileInformationByHandle(pin.Handle,out ancestor),"Ancestor metadata");Api(CheckpointNative.GetFileInformationByHandleEx(pin.Handle,23,out caseFlags,4),"Ancestor case query");report.AppendLine("ancestor volume="+ancestor.Volume+" id="+ancestor.IndexHigh+":"+ancestor.IndexLow+" attributes="+ancestor.Attributes+" caseFlags="+caseFlags);
    report.AppendLine(path+" role="+role+(role==CheckpointPinRole.TrustedOS?" sddl="+CheckpointNative.Descriptor(pin.Handle,role):""));
    if(end==volume.LastIndexOf('\\'))break;end=volume.IndexOf('\\',end+1);Require(end>=0,"Ancestor boundary");
   }
   using(var image=CheckpointPin.SystemImage())using(var compatible=CheckpointNative.CreateFileW(exe,0x80020000,1,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){
    Api(!compatible.IsInvalid,"Retained read-open compatibility");CheckpointNative.Info a,b;
    Api(CheckpointNative.GetFileInformationByHandle(image.Handle,out a),"Image metadata");Api(CheckpointNative.GetFileInformationByHandle(compatible,out b),"DOS image metadata");
    Require(a.Volume==b.Volume && a.IndexHigh==b.IndexHigh && a.IndexLow==b.IndexLow,"Fixed path identity");image.Revalidate();
    report.AppendLine(exe+" links="+a.Links+" volume="+a.Volume+" id="+a.IndexHigh+":"+a.IndexLow+" sddl="+CheckpointNative.Descriptor(image.Handle,CheckpointPinRole.InternalSystemImagePin));
   }
   foreach(var pin in held)pin.Revalidate();
  }finally{for(int i=held.Count-1;i>=0;i--)held[i].Dispose();}
  // Owned temporary files are separate native sharing/identity evidence, never an OS trust positive.
  string temp=Path.Combine(Path.GetTempPath(),"bimeron-checkpoint-pin-"+Guid.NewGuid().ToString("N"));Directory.CreateDirectory(temp);
  string file=Path.Combine(temp,"owned.bin"),link=Path.Combine(temp,"alias.bin");
  try{
   File.WriteAllBytes(file,new byte[]{1});Api(CreateHardLinkW(link,file,IntPtr.Zero),"Owned hardlink creation");
   using(var a=CheckpointNative.CreateFileW(file,0x80020000,1,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){
    Api(!a.IsInvalid,"Owned fixture open A");using(var b=CheckpointNative.CreateFileW(link,0x80020000,1,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){
    Api(!b.IsInvalid,"Owned fixture open B");CheckpointNative.Info x,y;
    Api(CheckpointNative.GetFileInformationByHandle(a,out x),"Owned metadata A");Api(CheckpointNative.GetFileInformationByHandle(b,out y),"Owned metadata B");
    Require(x.Links==2 && x.Volume==y.Volume && x.IndexHigh==y.IndexHigh && x.IndexLow==y.IndexLow,"Owned shared identity");
    bool rejected=false;try{using(var pin=new CheckpointPin(file,false,CheckpointPinRole.LabProtected)){};}catch(IOException e){rejected=e.Message=="Unsafe file metadata/case";}Require(rejected,"Lab multi-link metadata refusal");
    foreach(uint access in new uint[]{0x40000000,0x10000})using(var mutation=CheckpointNative.CreateFileW(link,access,7,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){
     int error=Marshal.GetLastWin32Error();Require(mutation.IsInvalid && error==32,"Owned alias mutation sharing refusal");
    }
   }
   }
   string logger=Path.Combine(temp,"logger.log");
   using(var reserve=CheckpointNative.CreateFileW(logger,0,3,IntPtr.Zero,1,0x00200000,IntPtr.Zero))using(var alias=CheckpointNative.CreateFileW(logger,0x20000,3,IntPtr.Zero,3,0x02200000,IntPtr.Zero))using(var writer=CheckpointNative.CreateFileW(logger,0x40000000,3,IntPtr.Zero,3,0x02200000,IntPtr.Zero)){
    Api(!reserve.IsInvalid && !alias.IsInvalid && !writer.IsInvalid,"Metadata reservation/alias logger write compatibility");CheckpointNative.Info a,b;Api(CheckpointNative.GetFileInformationByHandle(reserve,out a),"Reservation metadata");Api(CheckpointNative.GetFileInformationByHandle(alias,out b),"Log alias metadata");CheckpointNative.SameInfo(a,b);
   }File.Delete(logger);report.AppendLine("NATIVE OWNED LOG: metadata reservation + READ_CONTROL-only alias allow logger data-write open with read/write sharing; same identity.");
   report.AppendLine("NATIVE OWNED FIXTURE: links=2 shared identity; LabProtected rejected; write/delete alias opens refused with ERROR_SHARING_VIOLATION. Owned user fixture is NOT a TrustedOS positive.");
  }finally{if(File.Exists(link))File.Delete(link);if(File.Exists(file))File.Delete(file);Directory.Delete(temp);}
  return report.ToString();
 }
}
'@
Add-Type -TypeDefinition ($source+"`n"+$fake)
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$manifestPath=Join-Path $repo 'build/ack-checkpoint-review-reconciled/x64/lab-protected-ack-Checkpoint.run.manifest'
if(-not $NativeReadOnly){
 [CheckpointTests]::Run([IO.File]::ReadAllBytes($manifestPath))
 [CheckpointTests]::Models()
 [CheckpointTests]::Aliases()
}
$names=@($ast.ParamBlock.Parameters | ForEach-Object {$_.Name.VariablePath.UserPath}) -join ','
if($names -cne 'LabOnlyAllow,ExpectedRunnerSha256,ExpectedManifestSha256,ExpectedMsiSha256'){throw 'Input surface expanded'}
foreach($forbidden in @('OpenProcess(','Process.Kill','taskkill','Get-Process','Start-Process','QueueUserWorkItem','System.Threading.Timer')){if($source.Contains($forbidden)){throw "Forbidden operation: $forbidden"}}
if(([regex]::Matches($source,'CheckpointNative.TerminateProcess\(h,1603\)')).Count -ne 1){throw 'Termination dispatch changed'}
foreach($required in @('log?0x20000u:','log?3u:1u','CreateFileW(log,0,3,IntPtr.Zero,1','CheckpointNative.SameInfo(reserved,alias)','!dosAnchor.Same(anchorPin) || !dosMsi.Same(msi)','Revalidate();CheckpointNative.Mapping(known,anchor,CheckpointNative.ProgramFiles,CheckpointNative.VolumePath);','CheckpointNative.Launch(exe,dosRoot+')){if(-not $source.Contains($required)){throw "Missing alias/launch safety wiring: $required"}}
Write-Output "PASS PS $($PSVersionTable.PSVersion): AST closed contract and exact native interop compiled; native checks require separate -NativeReadOnly selection."

if($NativeReadOnly){[CheckpointNativeReadOnlyTests]::Run()}
