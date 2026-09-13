#!/usr/bin/env pwsh
#Requires -Version 5.1
# Build the release ZIP in release/:
#   <ModName>-v<version>-installer.zip   for GitHub Releases (install.cmd + payload)
#
# Installer only. There is deliberately no -nexus.zip, and adding one back
# would ship a layout no mod manager can deploy. The payload is
# openvr_api.dll sitting next to witness64_d3d11.exe, over the top of the
# copy The Witness ships, with the original renamed to
# openvr_api.dll.backup for the proxy to chain through. No manager performs
# that rename, and a plain deploy destroys the game's real DLL. Vortex has
# no route to this game at all: there is no game-thewitness extension in
# its bundledPlugins or in Nexus-Mods/vortex-games, and its one generic
# game-root mod type, modtype-dinput, is gated twice on the archive holding
# a file named dinput8.dll.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectRoot "cameraunlock-core\powershell\ReleaseWorkflow.psm1") -Force

$manifestPath = Join-Path $projectRoot 'launcher-manifest.json'
$manifest     = Get-Content $manifestPath -Raw | ConvertFrom-Json
$version      = $manifest.mod_info.version

# launcher-manifest.json is the canonical version; release.ps1 mirrors it into
# CMakeLists.txt and install.cmd. A hand-edit to one of those ships a ZIP whose
# name disagrees with the version install.cmd writes into the state file, which
# then reads as a failed upgrade rather than as a mistyped bump.
$cmakeVersion = ([regex]::Match((Get-Content (Join-Path $projectRoot 'CMakeLists.txt') -Raw), 'project\([^)]*?VERSION\s+(\d+\.\d+\.\d+)')).Groups[1].Value
$installVersion = ([regex]::Match((Get-Content (Join-Path $projectRoot 'scripts\install.cmd') -Raw), 'set "MOD_VERSION=(\d+\.\d+\.\d+)"')).Groups[1].Value
# kModVersion is the version the mod prints at the top of HeadTracking.log, so a
# bug report carries it. Left out of this gate it stayed at 0.0.0 release after
# release while every other source moved.
$constantsVersion = ([regex]::Match((Get-Content (Join-Path $projectRoot 'src\core\constants.h') -Raw), 'kModVersion\s*=\s*"(\d+\.\d+\.\d+)"')).Groups[1].Value
if ($cmakeVersion -ne $version -or $installVersion -ne $version -or $constantsVersion -ne $version) {
    throw "Version drift: launcher-manifest.json=$version, CMakeLists.txt=$cmakeVersion, install.cmd=$installVersion, constants.h=$constantsVersion."
}

# Assembly / ZIP name is a build-artifact concern, not launcher metadata.
# The release ZIP prefix is the PascalCase assembly name.
$modName  = 'TheWitnessHeadTracking'

# Write launcher-manifest.json into a staging dir, stamped with the release
# version, BOM-free (serde_json tolerates a BOM but the contract is no-BOM).
function Write-StampedManifest {
    param([string]$DestDir)
    $manifest.mod_info.version = $version
    $json = $manifest | ConvertTo-Json -Depth 10
    $utf8NoBom = New-Object System.Text.UTF8Encoding($false)
    [System.IO.File]::WriteAllText((Join-Path $DestDir 'launcher-manifest.json'), $json, $utf8NoBom)
}

$buildOutput = Join-Path $projectRoot 'bin\Release'
$modDll      = Join-Path $buildOutput 'openvr_api.dll'
$configFile  = Join-Path $projectRoot 'HeadTracking.ini'

if (-not (Test-Path $modDll))     { throw "Missing build output: $modDll" }
if (-not (Test-Path $configFile)) { throw "Missing config: $configFile" }

$releaseDir = Join-Path $projectRoot 'release'
if (Test-Path $releaseDir) { Remove-Item -Recurse -Force $releaseDir }
New-Item -ItemType Directory -Path $releaseDir | Out-Null

$stagingRoot = Join-Path $releaseDir '_staging'
New-Item -ItemType Directory -Path $stagingRoot | Out-Null

# ---------------- Installer ZIP ----------------
$instStaging = Join-Path $stagingRoot 'installer'
New-Item -ItemType Directory -Path $instStaging | Out-Null

# Plugins payload (install-body-shim.cmd copies from .\plugins\ to game exe dir).
$pluginsDir = Join-Path $instStaging 'plugins'
New-Item -ItemType Directory -Path $pluginsDir | Out-Null
Copy-Item $modDll     -Destination (Join-Path $pluginsDir 'openvr_api.dll')   -Force
# HeadTracking.ini rides in the ZIP for install.cmd to seed (MOD_SEED_FILES) but
# is deliberately NOT a launcher-manifest `files` row: a manifest row is copied
# on every deploy, so an update would overwrite a config the player had tuned.
# The launcher path is covered instead by the DLL, which lays down its compiled
# copy of this same file when none is next to the game exe.
Copy-Item $configFile -Destination (Join-Path $pluginsDir 'HeadTracking.ini') -Force

# install.cmd and uninstall.cmd are thin wrappers: the body they call lives in
# shared/ at the ZIP root, and without it the installer aborts at its own layout
# check and exits 1 on every run. Copy-SharedBundle stages every body there,
# alongside find-game.ps1, GamePathDetection.psm1 and games.json at the paths
# find-game.ps1 actually looks in.
Copy-SharedBundle -StagingDir $instStaging

# Top-level install/uninstall wrappers.
Copy-Item (Join-Path $projectRoot 'scripts\install.cmd')   -Destination $instStaging -Force
Copy-Item (Join-Path $projectRoot 'scripts\uninstall.cmd') -Destination $instStaging -Force

# Launcher manifest at release root so the launcher can deploy from metadata.
Write-StampedManifest -DestDir $instStaging

# Docs.
foreach ($doc in 'README.md','LICENSE','CHANGELOG.md','THIRD-PARTY-NOTICES.md') {
    Copy-Item (Join-Path $projectRoot $doc) -Destination $instStaging -Force
}

$installerZip = Join-Path $releaseDir "$modName-v$version-installer.zip"
Compress-Archive -Path (Join-Path $instStaging '*') -DestinationPath $installerZip -Force
Write-Host "Created: $installerZip" -ForegroundColor Green

Remove-Item -Recurse -Force $stagingRoot
