<#
.SYNOPSIS
    Runs the fleet conformance checks, with one documented exception.

.DESCRIPTION
    Every check in cameraunlock-core/scripts/conformance.ps1 runs, including the
    README rules. Exactly one finding is tolerated, and it is a conflict between
    two rules rather than a fault in this repo:

      - conformance's README check bans "any OpenTrack compatible tracker" as a
        claim about other people's trackers that nobody here has tested.
      - AGENTS.md mandates that exact sentence, word for word, as the fixed
        opening template every mod in the fleet shares.

    Both are stated as absolutes and only one can hold. Until the authoritative
    copies under /c/data settle it, the sentence stays as AGENTS.md dictates and
    this script tolerates the one message it produces.

    The exception is deliberately this narrow. Dropping the whole `readme` check
    id instead - which is what this repo did first - would also switch off
    em-dash detection in the README, the "it's not X, it's Y" construction, the
    "any phone tracker" and "all X do Y" generalisations, and the heading
    coverage. Those are not in dispute and they stay on.

    Any other finding fails the build.
#>
[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$core = Join-Path $projectRoot 'cameraunlock-core\scripts\conformance.ps1'
if (-not (Test-Path $core)) {
    throw "cameraunlock-core/scripts/conformance.ps1 is missing. Run: git submodule update --init"
}

# The one tolerated message, matched on the rule's own wording rather than on a
# README line number so an edit above it does not move the exception onto
# something else. Scoped to the readme check and required to appear EXACTLY
# once: the same rule fires on any "any OpenTrack compatible tracker" anywhere
# in the file, and a second one would be a genuine untested-kit claim in
# Features or Requirements rather than the mandated opening sentence.
$tolerated = 'claims every OpenTrack-compatible tracker works'
$expectedToleratedCount = 1

$raw = & $core -Repo $projectRoot -Json | Out-String
$findings = if ([string]::IsNullOrWhiteSpace($raw)) { @() } else { @($raw | ConvertFrom-Json) }

$blocking = @($findings | Where-Object {
    $_.severity -eq 'FAIL' -and $_.message -notmatch [regex]::Escape($tolerated)
})
$warnings = @($findings | Where-Object { $_.severity -eq 'WARN' })
$excused  = @($findings | Where-Object {
    $_.severity -eq 'FAIL' -and $_.check -eq 'readme' -and
    $_.message -match [regex]::Escape($tolerated)
})

foreach ($f in $warnings) { Write-Host "WARN $($f.check) $($f.message)" -ForegroundColor Yellow }
foreach ($f in $excused)  {
    Write-Host "KNOWN CONFLICT $($f.check) $($f.message)" -ForegroundColor DarkGray
    Write-Host "  tolerated: AGENTS.md mandates this sentence verbatim. See this script's header." -ForegroundColor DarkGray
}
foreach ($f in $blocking) { Write-Host "FAIL $($f.check) $($f.message)" -ForegroundColor Red }

if ($blocking.Count -gt 0) {
    Write-Host ''
    Write-Host "$($blocking.Count) conformance failure(s)." -ForegroundColor Red
    exit 1
}

# Fewer than expected means the sentence the exception exists for is gone, and
# the exception should go with it rather than sit here excusing nothing. More
# means the phrase has spread somewhere it was never sanctioned.
if ($excused.Count -ne $expectedToleratedCount) {
    Write-Host ''
    Write-Host ("Expected exactly $expectedToleratedCount tolerated readme finding, saw " +
                "$($excused.Count). If the mandated opening sentence changed, update or " +
                "remove the exception in this script; if the phrase appears somewhere " +
                "new, that one is a real finding.") -ForegroundColor Red
    exit 1
}

Write-Host "conformance ok ($($warnings.Count) warning(s), $($excused.Count) known conflict(s))" -ForegroundColor Green
