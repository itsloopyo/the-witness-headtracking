#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Release workflow for The Witness Head Tracking.

.DESCRIPTION
    1. Validate semver + git state (branch, clean tree, tag not existing).
    2. Regenerate CHANGELOG.md from conventional commits (via
       cameraunlock-core/powershell/ReleaseWorkflow.psm1).
    3. Bump version in launcher-manifest.json, CMakeLists.txt, and the
       install.cmd MOD_VERSION line.
    4. Build + package the release.
    5. Commit the version + changelog as "Release v<version>".
    6. Create annotated tag v<version> and push it; CI picks up the tag and
       produces the GitHub release artifacts.

.PARAMETER Version
    Semver string (e.g. "1.0.0"), or major/minor/patch.

.PARAMETER Force
    Ship a release even when there are no user-facing commits since the last
    tag (writes a maintenance changelog entry instead of aborting).
#>
param(
    [Parameter(Position = 0)]
    [string]$Version = '',
    # Ship a release even when there are no user-facing commits since the
    # last tag (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$scriptDir   = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectRoot = Split-Path -Parent $scriptDir

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force

$manifestPath  = Join-Path $projectRoot 'launcher-manifest.json'
$cmakePath     = Join-Path $projectRoot 'CMakeLists.txt'
$installCmd    = Join-Path $projectRoot 'scripts\install.cmd'
$constantsPath = Join-Path $projectRoot 'src\core\constants.h'
$changelogPath = Join-Path $projectRoot 'CHANGELOG.md'

function Get-ManifestVersion {
    $manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
    return $manifest.mod_info.version
}

Write-Host ''
Write-Host '=== The Witness Head Tracking Release ===' -ForegroundColor Cyan
Write-Host ''

$current = Get-ManifestVersion

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "Current version: $current" -ForegroundColor Yellow
    Write-Host 'Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>'
    exit 0
}

if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

# Step 1 - resolve major/minor/patch into a concrete version (or accept literal X.Y.Z)
try {
    $Version = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $current
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Before anything is written: a release below the config descriptor's
# canonical_since would ship CameraUnlock.ini under a version a launcher reads
# as a pre-canonical build.
Assert-ReleaseNotBelowCanonicalSince -RepoRoot $projectRoot -Version $Version

$tag = "v$Version"

# Step 2 - preflight checks (branch, dirty tree, tag)
$branch = (git -C $projectRoot rev-parse --abbrev-ref HEAD).Trim()
if ($branch -ne 'main') {
    Write-Host "Must be on main branch to release (currently on '$branch')" -ForegroundColor Red
    exit 1
}
if (-not (Test-CleanGitStatus)) {
    Write-Host 'Working tree has uncommitted changes - commit or stash first.' -ForegroundColor Red
    git -C $projectRoot status --short
    exit 1
}
if (Test-GitTagExists -Tag $tag) {
    Write-Host "Tag '$tag' already exists." -ForegroundColor Red
    exit 1
}

Write-Host "Running the full test suite..." -ForegroundColor Cyan
Push-Location $projectRoot
try {
    pixi run test
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Error: pixi run test failed. Nothing was changed." -ForegroundColor Red
        exit 1
    }
} finally {
    Pop-Location
}

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIPs, and bumping the submodule does not touch it. Packaging refuses
# to ship that mismatch, so a bump with no notices edit stopped the release
# here, or in CI once the tag had already been pushed. Re-sync it and let this
# release carry the correction.
& git -C $projectRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) { throw "THIRD-PARTY-NOTICES.md has uncommitted edits. Commit or discard them, then re-run." }
& (Join-Path $projectRoot 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $projectRoot
if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
& git -C $projectRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) {
    & git -C $projectRoot commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the re-synced THIRD-PARTY-NOTICES.md." }
    Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
}

Write-Host "Current version: $current" -ForegroundColor Gray
Write-Host "New version:     $Version" -ForegroundColor Green
Write-Host ''

# Step 3 - changelog (the gate that can fail). Generate it BEFORE mutating
# any version files so an abort here leaves the working tree clean instead
# of stranding a half-applied version bump with no tag.
Write-Host 'Generating CHANGELOG from commits...' -ForegroundColor Cyan
try {
    $changelogArgs = @{
        ChangelogPath = $changelogPath
        Version       = $Version
        ArtifactPaths = @('src/', 'cameraunlock-core', 'scripts/install.cmd', 'scripts/uninstall.cmd')
        Maintenance   = [bool]$Force
    }
    New-ChangelogFromCommits @changelogArgs | Out-Null
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host 'No user-facing changes to release. Re-run with -Force for a maintenance release.' -ForegroundColor Yellow
    exit 1
}

# Step 4 - bump version in launcher-manifest.json, CMakeLists.txt, install.cmd.
Write-Host "Updating launcher-manifest.json to $Version..." -ForegroundColor Cyan
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$manifest.mod_info.version = $Version
$json = $manifest | ConvertTo-Json -Depth 10
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($manifestPath, $json, $utf8NoBom)

Write-Host "Updating CMakeLists.txt to $Version..." -ForegroundColor Cyan
(Get-Content $cmakePath) -replace 'VERSION\s+\d+\.\d+\.\d+', "VERSION $Version" | Set-Content $cmakePath

# uninstall.cmd carries no MOD_VERSION line, so it is left untouched:
# rewriting it would only risk its CRLF endings for a substitution that
# matches nothing.
Write-Host "Updating install.cmd MOD_VERSION to $Version..." -ForegroundColor Cyan
(Get-Content $installCmd) -replace 'set "MOD_VERSION=.+"', "set `"MOD_VERSION=$Version`"" | Set-Content $installCmd

# kModVersion is the string the mod writes at the top of HeadTracking.log, which
# is the file a bug report arrives with. Left unbumped it said 0.0.0 for every
# release, so every report needed its version establishing by hand.
Write-Host "Updating src/core/constants.h to $Version..." -ForegroundColor Cyan
(Get-Content $constantsPath) -replace 'kModVersion\s*=\s*"\d+\.\d+\.\d+"', "kModVersion = `"$Version`"" | Set-Content $constantsPath

# Step 5 - build & package
Write-Host 'Building release...' -ForegroundColor Cyan
Push-Location $projectRoot
try {
    & pixi run build-release
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & pixi run package
    if ($LASTEXITCODE -ne 0) { throw 'Package failed' }
} finally {
    Pop-Location
}

# Step 6 - commit version + changelog
Write-Host 'Committing version + changelog...' -ForegroundColor Cyan
git -C $projectRoot add $manifestPath $cmakePath $installCmd $constantsPath $changelogPath
git -C $projectRoot commit -m "Release v$Version"
if ($LASTEXITCODE -ne 0) { throw 'Commit failed' }

# Step 7 - tag + push. Each exit code is checked: $ErrorActionPreference does
# not apply to a native command, so an unchecked `git push` that was rejected
# for a non-fast-forward, a missing upstream or an expired credential printed
# the success line below while nothing had left the machine.
Write-Host "Creating tag $tag..." -ForegroundColor Cyan
git -C $projectRoot tag -a $tag -m "Release $tag"
if ($LASTEXITCODE -ne 0) {
    throw "Could not create tag $tag. The version bump is committed locally and the " +
          "ZIP is built; resolve the tag and re-run: git tag -a $tag -m 'Release $tag'; " +
          "git push origin main; git push origin $tag"
}

git -C $projectRoot push origin main
if ($LASTEXITCODE -ne 0) {
    throw "Pushing main failed. The version bump and tag $tag are committed locally; " +
          "resolve the push and re-run: git push origin main; git push origin $tag"
}

git -C $projectRoot push origin $tag
if ($LASTEXITCODE -ne 0) {
    throw "Pushing tag $tag failed. main is pushed; re-run: git push origin $tag"
}

Write-Host ''
Write-Host "Release $tag pushed - CI will build and publish artifacts." -ForegroundColor Green
