# Offline only: parse runner AST, extract pure helpers; never invoke its entry point.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$path=Join-Path $PSScriptRoot 'Run-Lab.ps1'
$tokens=$null;$errors=$null
$ast=[Management.Automation.Language.Parser]::ParseFile($path,[ref]$tokens,[ref]$errors)
if ($errors.Count) { throw 'Runner syntax failure.' }
foreach ($name in @('Assert-JsonKeys','Assert-LabPath','Assert-Fields','Assert-Hash','Read-LabManifest','Read-LabTrace','Get-LabOutcome')) {
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
$manifest.variants.Success.productCode=$manifest.variants.CommitFail.productCode;Reject {Read-LabManifest ($manifest|ConvertTo-Json -Depth 5)}
Reject { Assert-Hash ('a'*63) }
Reject { Assert-Hash 42 }
Reject { Assert-LabPath 'C:\Windows\System32' }
Reject { Assert-LabPath 'C:\Users\Public\Documents\BimeronInstallerLab\..\escape' }
Reject { Read-LabManifest '{"schema":"1","architecture":"amd64","traceSchema":1,"variants":{}}' }
Reject { Read-LabTrace 'BIMERON_LAB_TRACE_V1 {"callback":"shell"}' }
Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+('x'*2049)) }
Reject { Read-LabTrace 'BIMERON_LAB_TRACE_V1 {"nested":{}}' }
if ($null -ne (Read-LabTrace 'ordinary MSI line')) {throw 'Nontrace line accepted.'}
$trace=[ordered]@{utc='2026-10-30T00:00:00.000Z';creationTime='123';sidHash=('a'*64);logonId='0000000000000001';pid=1;tid=2;session=1;elevationType=2;integrity=12288;impersonationLevel=0;status=0;intendedReturn=0;callback='Preflight';tokenSource='Process';tokenType='Primary';elevated='Yes';enabledAdmin='Yes';restricted='No'}
$null=Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+($trace|ConvertTo-Json -Compress))
$trace.pid='1';Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+($trace|ConvertTo-Json -Compress)) };$trace.pid=1
$trace.callback='Execute';Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+($trace|ConvertTo-Json -Compress)) };$trace.callback='Preflight'
$trace.extra='secret';Reject { Read-LabTrace ('BIMERON_LAB_TRACE_V1 '+($trace|ConvertTo-Json -Compress)) };$trace.Remove('extra')
if ((Get-LabOutcome 0 $true $true) -ne 'UnknownPending' -or (Get-LabOutcome $null $false $true) -ne 'UnknownPending' -or (Get-LabOutcome 0 $false $false) -ne 'IncompleteEvidence' -or (Get-LabOutcome 1603 $false $true) -ne 'ObservedOnly') { throw 'Outcome regression.' }
# Inspect parameter validation by constructing a harmless param-only scriptblock.
$parameterCheck=[scriptblock]::Create($ast.ParamBlock.Extent.Text + "`n 'accepted'")
$valid=@{LabOnlyAllow='DisposableWindowsVM';Case='Success';StartingToken='Unelevated';ExpectedManifestSha256=('a'*64);ExpectedRunnerSha256=('b'*64)}
$null=& $parameterCheck @valid
$valid.LabOnlyAllow='yes';Reject { & $parameterCheck @valid };$valid.LabOnlyAllow='DisposableWindowsVM'
$valid.Case='Arbitrary';Reject { & $parameterCheck @valid };$valid.Case='Success'
$valid.StartingToken='SYSTEM';Reject { & $parameterCheck @valid };$valid.StartingToken='Unelevated'
$valid.ExpectedRunnerSha256='bad';Reject { & $parameterCheck @valid }
# Compile the exact read-only interop declaration without calling it.
$add=$ast.Find({param($n) $n -is [Management.Automation.Language.CommandAst] -and $n.GetCommandName() -eq 'Add-Type'},$true)
$source=$add.CommandElements[2].Value
Add-Type -TypeDefinition $source
Write-Output "PASS: runner syntax, interop compilation, pure helpers and $count negative checks; no runner/MSI/token/VM operations executed."
