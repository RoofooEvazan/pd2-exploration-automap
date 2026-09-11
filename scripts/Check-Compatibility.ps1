<#
.SYNOPSIS
Checks whether a PD2 installation matches the tested game and renderer files.
.DESCRIPTION
Reads the profiled DLLs and compares their SHA-256 hashes with compatibility.json.
Prints MATCH, DIFFERENT, or MISSING for each file without modifying the game.
Returns exit code 0 when all match, or 2 for different or missing files.
Invalid paths and read errors stop the script with an error.
.PARAMETER GamePath
The PD2 game folder containing the engine DLLs listed in compatibility.json.
.EXAMPLE
.\Check-Compatibility.ps1 -GamePath "D:\Games\Diablo II\ProjectD2"
#>

[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateNotNullOrEmpty()]
    [string]$GamePath
)

# Read-only: this script never installs, patches, or downloads anything.
$ErrorActionPreference = 'Stop'
$gameDirectory = (Resolve-Path -LiteralPath $GamePath).Path
if (-not (Test-Path -LiteralPath $gameDirectory -PathType Container)) {
    throw 'GamePath must identify the PD2 installation directory.'
}
$profilePath = Join-Path $PSScriptRoot '..\compatibility.json'
$profile = Get-Content -LiteralPath $profilePath -Raw | ConvertFrom-Json
$allMatch = $true
$results = foreach ($file in $profile.engineSha256.PSObject.Properties) {
    $candidate = Join-Path $gameDirectory $file.Name
    $status = 'MISSING'
    if (Test-Path -LiteralPath $candidate -PathType Leaf) {
        $actualHash = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash
        $status = if ($actualHash -ieq $file.Value) { 'MATCH' } else { 'DIFFERENT' }
    }
    if ($status -ne 'MATCH') { $allMatch = $false }
    [pscustomobject]@{ File = $file.Name; Status = $status }
}
$results | Format-Table -AutoSize | Out-Host
if (-not $allMatch) {
    Write-Host 'This installation does not match the tested binary set. Do not install this build.'
    exit 2
}
Write-Host 'All profiled engine files match the tested binary set. Runtime hook checks still apply.'
exit 0
