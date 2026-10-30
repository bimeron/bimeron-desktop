#requires -Version 5.1
# Offline blocker witness only: no native calls, MSI sessions or owned processes.
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
function Assert([bool]$Condition, [string]$Message) {
 if (-not $Condition) { throw $Message }
}
$runner = Join-Path $PSScriptRoot 'Run-CheckpointLab.ps1'
$tokens = $null; $errors = $null
$ast = [Management.Automation.Language.Parser]::ParseFile($runner,[ref]$tokens,[ref]$errors)
Assert ($errors.Count -eq 0) 'Runner parse failed.'
$statements = @($ast.EndBlock.Statements)
Assert ($statements.Count -eq 5) 'Unexpected runner statement count.'
Assert ($statements[0].Extent.Text -ceq 'Set-StrictMode -Version Latest') 'Strict mode changed.'
Assert ($statements[1].Extent.Text -ceq '$ErrorActionPreference = ''Stop''') 'Error policy changed.'
Assert ($statements[2] -is [Management.Automation.Language.FunctionDefinitionAst]) 'Missing refusal function.'
Assert ($statements[3].Extent.Text -ceq 'Get-CheckpointRunnerRefusal | ConvertTo-Json -Compress') 'Unexpected entry point.'
Assert ($statements[4].Extent.Text -ceq 'exit 1') 'Runner must refuse nonzero.'
Assert (@($ast.ParamBlock.Parameters).Count -eq 4) 'Unexpected input surface.'
Assert ((@($ast.ParamBlock.Parameters | ForEach-Object {$_.Name.VariablePath.UserPath}) -join ',') -ceq 'LabOnlyAllow,ExpectedRunnerSha256,ExpectedManifestSha256,ExpectedMsiSha256') 'Arbitrary path/command/case input added.'
$function = $statements[2]
Assert ($function.Name -ceq 'Get-CheckpointRunnerRefusal') 'Unexpected function.'
Assert (@($function.Body.EndBlock.Statements).Count -eq 1) 'Unexpected refusal body.'
$commands = @($ast.FindAll({param($n) $n -is [Management.Automation.Language.CommandAst]},$true))
Assert ((@($commands | ForEach-Object {$_.GetCommandName()}) -join ',') -ceq 'Set-StrictMode,Get-CheckpointRunnerRefusal,ConvertTo-Json') 'Executable command escape.'
Assert (@($ast.FindAll({param($n) $n -is [Management.Automation.Language.InvokeMemberExpressionAst]},$true)).Count -eq 0) 'Method invocation escape.'
Assert (@($ast.FindAll({param($n) $n -is [Management.Automation.Language.ScriptBlockExpressionAst]},$true)).Count -eq 0) 'Nested executable escape.'
. ([scriptblock]::Create($function.Extent.Text))
$result = Get-CheckpointRunnerRefusal
Assert ($result.outcome -ceq 'BlockedStrictDeadline' -and -not $result.guestReady -and -not $result.inputsValidated) 'Admission must remain blocked.'
Assert ($result.launchCount -eq 0 -and $result.terminationCount -eq 0) 'Side effects claimed.'
Assert ($result.initiatingClient -ceq 'Unknown' -and $result.clean -ceq 'Unknown' -and $result.serverQuiescence -ceq 'Unknown') 'Authority/settlement claim.'
foreach ($parameter in $ast.ParamBlock.Parameters) {
 $name = $parameter.Name.VariablePath.UserPath
 Assert (@($ast.EndBlock.FindAll({param($n) $n -is [Management.Automation.Language.VariableExpressionAst] -and $n.VariablePath.UserPath -ceq $name},$true)).Count -eq 0) 'Input can bypass refusal.'
}
# NOT a worker: exposes the scheduling gap across the native-call boundary.
Add-Type -TypeDefinition @'
using System;
public static class CheckpointDeadlineWitness {
 public sealed class Result {
  public int Calls; public long CallAt; public long Handle; public bool Pins;
 }
 public static Result Model(long before, long preemption, bool identity, bool checkpoint, bool query) {
  const long deadline=300000;
  const long retainedHandle=1234; // opaque fake, never a native handle
  var r=new Result {Pins=true};
  if (!identity || !checkpoint || !query || before>=deadline) return r;
  // All final identity/alive/freshness/cancellation/clock checks succeeded.
  // Scheduler runs another thread before the next instruction/native entry.
  long at=before+preemption;
  r.Calls=1; r.CallAt=at; r.Handle=retainedHandle;
  return r; // pins retained pending actual modeled completion
 }
}
'@
$normal = [CheckpointDeadlineWitness]::Model(1000,0,$true,$true,$true)
Assert ($normal.Calls -eq 1 -and $normal.Handle -eq 1234 -and $normal.CallAt -lt 300000) 'In-budget control failed.'
$expired = [CheckpointDeadlineWitness]::Model(300000,0,$true,$true,$true)
Assert ($expired.Calls -eq 0 -and $expired.Pins) 'Already-expired control failed.'
$race = [CheckpointDeadlineWitness]::Model(299999,2,$true,$true,$true)
Assert ($race.Calls -eq 1 -and $race.CallAt -ge 300000 -and $race.Pins) 'Preemption counterexample missing.'
foreach ($case in @(@($false,$true,$true),@($true,$false,$true),@($true,$true,$false))) {
 $negative = [CheckpointDeadlineWitness]::Model(1000,0,$case[0],$case[1],$case[2])
 Assert ($negative.Calls -eq 0 -and $negative.Pins) 'Negative witness control failed.'
}
Write-Output "PASS PS $($PSVersionTable.PSVersion): refusal AST/result/input checks; managed controls; preemption counterexample at $($race.CallAt)ms; pins retained. No native/process/MSI execution. Operational runner acceptance remains BLOCKED."
