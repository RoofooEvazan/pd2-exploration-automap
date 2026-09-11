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
Write-Host 'All four engine files match the tested binary set. Runtime hook checks still apply.'
exit 0
