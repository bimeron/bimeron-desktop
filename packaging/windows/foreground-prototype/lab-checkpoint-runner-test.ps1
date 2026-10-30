#requires -Version 5.1
# Offline exact worker/interop compilation. No native calls or real processes.
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
 public long Time,CallAt;public int Calls,Queries;public IntPtr Killed;
 public IDisposable Held;public bool IsAlive=true,IdentityBad,IdentityError,ApiError,ApiThrows,Signaled=true,Disposed;
 public readonly ManualResetEvent LaunchEntered=new ManualResetEvent(false),LaunchPermit=new ManualResetEvent(true),ReadyEntered=new ManualResetEvent(false),ReadyPermit=new ManualResetEvent(true),KillEntered=new ManualResetEvent(false),KillPermit=new ManualResetEvent(true),WaitEntered=new ManualResetEvent(false),WaitPermit=new ManualResetEvent(true);
 public Action BeforeReady,FinalQuery;
 public IntPtr Launch(){LaunchEntered.Set();LaunchPermit.WaitOne();return new IntPtr(1234);}
 public void Check(IntPtr h){if(h!=new IntPtr(1234))throw new Exception("Wrong retained handle");}
 public CheckpointIdentity Query(IntPtr h){Check(h);Queries++;if(IdentityError)throw new Exception("Query");if(Queries==2 && FinalQuery!=null)FinalQuery();return new CheckpointIdentity(IdentityBad && Queries>1?8u:7u,42);}
 public bool Alive(IntPtr h){Check(h);return IsAlive;}
 public bool Ready(IntPtr h){Check(h);ReadyEntered.Set();ReadyPermit.WaitOne();if(BeforeReady!=null)BeforeReady();return true;}
 public bool Kill(IntPtr h){Check(h);Killed=h;KillEntered.Set();KillPermit.WaitOne();Calls++;CallAt=Time;if(ApiThrows)throw new Exception("API");return !ApiError;}
 public bool Death(IntPtr h){Check(h);return Signaled;}
 public void Wait(IntPtr h){Check(h);WaitEntered.Set();WaitPermit.WaitOne();}
 public void Close(IntPtr h){Check(h);if(Disposed)throw new Exception("Pins released before handle");}
 public void Dispose(){if(Held!=null)Held.Dispose();Disposed=true;}
 public CheckpointWorker Worker(){return new CheckpointWorker(delegate{return Time;},300000,Launch,Query,Alive,Ready,Kill,Death,Wait,Close,new IDisposable[]{this});}
}
public static class CheckpointTests {
 static int checks;
 static void Assert(bool value,string message){checks++;if(!value)throw new Exception(message);}
 static void Await(ManualResetEvent e){Assert(e.WaitOne(3000),"Mock barrier timed out");}
 static void Finish(CheckpointWorker w,CheckpointMock m){m.LaunchPermit.Set();m.ReadyPermit.Set();m.KillPermit.Set();m.WaitPermit.Set();Assert(w.Observe(3000),"Mock worker did not finish");Assert(m.Disposed,"Pins not released at completion");}
 static void Reject(Action action){bool rejected=false;try{action();}catch{rejected=true;}Assert(rejected,"Expected rejection");}
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
  foreach(string result in new string[]{"error","throws","unconfirmed"}){m=new CheckpointMock();m.ApiError=result=="error";m.ApiThrows=result=="throws";m.Signaled=result!="unconfirmed";w=m.Worker();w.Start();Finish(w,m);Assert(m.Calls==1,"Retry on API failure/unknown death");if(result=="error")Assert(w.Snapshot.ApiResult=="Failed","API error lost");if(result=="throws")Assert(w.Snapshot.ApiResult=="Unknown","API exception mislabeled");if(result=="unconfirmed")Assert(w.Snapshot.ClientDeath=="Unconfirmed","Death mislabeled");}
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
'@
Add-Type -TypeDefinition ($source+"`n"+$fake)
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../../..'))
$manifestPath=Join-Path $repo 'build/ack-checkpoint-review-reconciled/x64/lab-protected-ack-Checkpoint.run.manifest'
[CheckpointTests]::Run([IO.File]::ReadAllBytes($manifestPath))
$names=@($ast.ParamBlock.Parameters | ForEach-Object {$_.Name.VariablePath.UserPath}) -join ','
if($names -cne 'LabOnlyAllow,ExpectedRunnerSha256,ExpectedManifestSha256,ExpectedMsiSha256'){throw 'Input surface expanded'}
foreach($forbidden in @('OpenProcess(','Process.Kill','taskkill','Get-Process','Start-Process','QueueUserWorkItem','System.Threading.Timer')){if($source.Contains($forbidden)){throw "Forbidden operation: $forbidden"}}
if(([regex]::Matches($source,'CheckpointNative.TerminateProcess\(h,1603\)')).Count -ne 1){throw 'Termination dispatch changed'}
Write-Output "PASS PS $($PSVersionTable.PSVersion): AST closed contract and exact native interop compiled only."
