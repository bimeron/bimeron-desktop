param([Parameter(Mandatory=$true)][string]$Executable,
      [Parameter(Mandatory=$true)][string]$Version,
      [Parameter(Mandatory=$true)][ValidateSet('amd64','arm64')][string]$Architecture)
$ErrorActionPreference = 'Stop'
$v = [Diagnostics.FileVersionInfo]::GetVersionInfo((Resolve-Path $Executable))
foreach ($entry in @(@('FileDescription','Bimeron'), @('ProductName','Bimeron'),
    @('CompanyName','Bimeron Network'),
    @('LegalCopyright',"$([char]0xA9) 2026 NOOB RP LTD"),
    @('InternalName','bimeron-desktop'), @('OriginalFilename','bimeron-desktop.exe'),
    @('FileVersion',"$Version.0"), @('ProductVersion',"$Version.0"))) {
    if ($v.($entry[0]) -cne $entry[1]) { throw "Unexpected $($entry[0])" }
}
if ("$($v.FileMajorPart).$($v.FileMinorPart).$($v.FileBuildPart).$($v.FilePrivatePart)" -ne "$Version.0") {
    throw 'Unexpected numeric file version'
}
$b = [IO.File]::ReadAllBytes((Resolve-Path $Executable))
$pe = [BitConverter]::ToInt32($b, 0x3c)
$machine = [BitConverter]::ToUInt16($b, $pe + 4)
$expected = if ($Architecture -eq 'amd64') { 0x8664 } else { 0xaa64 }
if ($machine -ne $expected) { throw 'Unexpected PE machine' }
Write-Output "PASS: $Architecture PE display metadata and executable identity ($Version)"
