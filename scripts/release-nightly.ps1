# Thin shim. Determine version, delegate to the shared publisher.
# See cameraunlock-core/powershell/NightlyRelease.psm1 for what it does.

[CmdletBinding()]
param(
    [switch]$AllowDirty
)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

# launcher-manifest.json is this mod's canonical version - the one release.ps1
# bumps and package-release.ps1 names the installer ZIP with.
$manifestFile = Join-Path $ProjectRoot 'launcher-manifest.json'
$versionMatch = Select-String -Path $manifestFile -Pattern '"version"\s*:\s*"([^"]+)"'
if (-not $versionMatch) {
    throw "Could not extract version from $manifestFile"
}
$version = $versionMatch.Matches[0].Groups[1].Value

Publish-NightlyBuild `
    -ModId 'the-witness' `
    -ModName 'TheWitnessHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -NoNexusZip `
    -AllowDirty:$AllowDirty
