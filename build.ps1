param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "contracts", "protocol-compatibility", "integration", "e2e", "performance")]
    [string[]]$Suite = @("all"),
    [ValidateSet("auto", "quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "auto",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "medium",
    [ValidateSet("auto", "quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "auto",
    [string[]]$ChangedScope = @(),
    [string[]]$SkipLayer = @(),
    [string]$SkipJustification = "",
    [switch]$SkipHeavyTargets
)

$ErrorActionPreference = "Stop"

function Resolve-RootRunner {
    param(
        [string]$CanonicalRelativePath,
        [string]$EntryName
    )

    $canonicalPath = Join-Path $PSScriptRoot $CanonicalRelativePath
    if (Test-Path $canonicalPath) {
        return $canonicalPath
    }

    throw "未找到根级 $EntryName 入口。已检查: $canonicalPath"
}

$runner = Resolve-RootRunner `
    -CanonicalRelativePath "scripts\\build\\build-validation.ps1" `
    -EntryName "build"

& $runner `
    -Profile $Profile `
    -Suite $Suite `
    -Lane $Lane `
    -RiskProfile $RiskProfile `
    -DesiredDepth $DesiredDepth `
    -ChangedScope $ChangedScope `
    -SkipLayer $SkipLayer `
    -SkipJustification $SkipJustification `
    -SkipHeavyTargets:$SkipHeavyTargets
