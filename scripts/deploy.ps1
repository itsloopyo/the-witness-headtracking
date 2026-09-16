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
$configFile  = Join-Path $projectRoot 'HeadTracking.ini'
$modDllName  = 'openvr_api.dll'
$backupName  = 'openvr_api.dll.backup'

$builtDll = Join-Path $buildOutput $modDllName
if (-not (Test-Path $builtDll)) {
    throw "Build artifact not found at: $builtDll. Run 'pixi run build-release' first."
}
if (-not (Test-Path $configFile)) {
    throw "Config file not found at: $configFile"
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

# Seeded, not overwritten - the same rule install.cmd follows for this file. A
# config in the game folder is one somebody tuned there, and re-deploying to try
# the next build is exactly when they least want it reset.
$deployedConfig = Join-Path $exeDir 'HeadTracking.ini'
$seededConfig = -not (Test-Path $deployedConfig)
if ($seededConfig) {
    Copy-Item -Path $configFile -Destination $deployedConfig -Force
} else {
    Write-Host "HeadTracking.ini already in the game folder - left as it is." -ForegroundColor Yellow
}

Write-Host ""
Write-Host "Deployed to: $exeDir" -ForegroundColor Green
Write-Host "  openvr_api.dll   (mod shim)"
if ($seededConfig) {
    Write-Host "  HeadTracking.ini (config)"
} else {
    Write-Host "  HeadTracking.ini (kept the one already there)"
}
