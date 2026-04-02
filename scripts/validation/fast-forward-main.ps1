[CmdletBinding()]
param(
    [string]$Remote = "origin",
    [string]$Branch = "main"
)

$ErrorActionPreference = "Stop"

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $repoRoot

function Invoke-GitCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("git command failed: git {0}" -f ($Arguments -join " "))
    }
}

function Get-GitOutput {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    $output = & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw ("git command failed: git {0}" -f ($Arguments -join " "))
    }

    return ($output | Out-String).Trim()
}

$currentBranch = Get-GitOutput -Arguments @("branch", "--show-current")
if ($currentBranch -ne $Branch) {
    throw ("refusing to update: current branch is '{0}', expected '{1}'." -f $currentBranch, $Branch)
}

$statusLines = @(git status --porcelain)
if ($LASTEXITCODE -ne 0) {
    throw "git status failed."
}

if ($statusLines.Count -gt 0) {
    throw "refusing to update: working tree is not clean."
}

Write-Output ("fetching {0}/{1}" -f $Remote, $Branch)
Invoke-GitCommand -Arguments @("fetch", $Remote, $Branch)

$remoteRef = "{0}/{1}" -f $Remote, $Branch
$comparisonRef = "{0}...{1}" -f $Branch, $remoteRef
$aheadBehind = Get-GitOutput -Arguments @("rev-list", "--left-right", "--count", $comparisonRef)
$counts = $aheadBehind -split '\s+'
if ($counts.Count -lt 2) {
    throw ("unable to parse ahead/behind counts: '{0}'" -f $aheadBehind)
}

$ahead = [int]$counts[0]
$behind = [int]$counts[1]

if ($ahead -gt 0) {
    throw ("refusing to fast-forward: local '{0}' is ahead of '{1}' by {2} commit(s)." -f $Branch, $remoteRef, $ahead)
}

if ($behind -eq 0) {
    Write-Output ("already up to date: '{0}' matches '{1}'." -f $Branch, $remoteRef)
    exit 0
}

Write-Output ("fast-forwarding '{0}' to '{1}' ({2} commit(s) behind)" -f $Branch, $remoteRef, $behind)
Invoke-GitCommand -Arguments @("merge", "--ff-only", $remoteRef)

$headCommit = Get-GitOutput -Arguments @("rev-parse", "HEAD")
Write-Output ("updated '{0}' to {1}" -f $Branch, $headCommit)
