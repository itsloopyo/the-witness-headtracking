#!/usr/bin/env pwsh
# Locate The Witness install directory. Delegates to the shared
# games.json-backed lookup in cameraunlock-core.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\GamePathDetection.psm1') -Force

$gamePath = Find-GamePath -GameId 'the-witness'
if (-not $gamePath) {
    Write-Error "Could not find The Witness installation. Set THE_WITNESS_PATH or pass the path explicitly."
    exit 1
}

Write-Host "Found: $gamePath" -ForegroundColor Green
$gamePath
