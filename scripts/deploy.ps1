#!/usr/bin/env pwsh
#Requires -Version 5.1
# Dev-time deploy: builds output -> game exe dir. Renames the stock
# openvr_api.dll to openvr_api.dll.backup on first install so the
# shim can forward exports to it.

param(
    [Parameter(Mandatory=$true, Position=0)]
    [ValidateSet('Debug','Release')]
    [string]$Configuration,

    [Parameter(Mandatory=$false, Position=1)]
    [string]$GivenPath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$ProgressPreference    = 'SilentlyContinue'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir
Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\ModDeployment.psm1') -Force

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\GamePathDetection.psm1') -Force

$buildOutput = Join-Path $projectRoot "bin\$Configuration"
$modDllName  = 'openvr_api.dll'
$backupName  = 'openvr_api.dll.backup'

$builtDll = Join-Path $buildOutput $modDllName
if (-not (Test-Path $builtDll)) {
    throw "Build artifact not found at: $builtDll. Run 'pixi run build-release' first."
}

if ($GivenPath) {
    $gamePath = $GivenPath
} else {
    $gamePath = Find-GamePath -GameId 'the-witness'
}
if (-not $gamePath) {
    throw "Could not locate The Witness. Pass -GivenPath or set THE_WITNESS_PATH."
}

$exeDir = $gamePath
$existing = Join-Path $exeDir $modDllName
$backup   = Join-Path $exeDir $backupName

# A previous build has different bytes but must never become the original.
if ((Test-Path $existing) -and -not (Test-Path $backup)) {
    if (Test-FileContainsMarker -FilePath $existing -Marker 'TheWitnessHeadTracking') {
        Write-Host "openvr_api.dll in the game folder is already this mod - not backing it up." -ForegroundColor Yellow
    } else {
        Copy-Item -Path $existing -Destination $backup -Force
        Write-Host "Backed up original openvr_api.dll -> openvr_api.dll.backup" -ForegroundColor Green
    }
}

Copy-Item -Path $builtDll -Destination $existing -Force

# No config is copied: the mod creates CameraUnlock.ini beside the game exe on
# its first start, and imports a HeadTracking.ini an earlier build left there.

Write-Host ""
Write-Host "Deployed to: $exeDir" -ForegroundColor Green
Write-Host "  openvr_api.dll   (mod shim)"
