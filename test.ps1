param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "contracts", "e2e", "protocol-compatibility", "performance")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "tests\\reports",
    [switch]$FailOnKnownFailure,
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix
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
    -CanonicalRelativePath "scripts\\validation\\invoke-workspace-tests.ps1" `
    -EntryName "test"

& $runner `
    -Profile $Profile `
    -Suite $Suite `
    -ReportDir $ReportDir `
    -FailOnKnownFailure:$FailOnKnownFailure `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke `
    -IncludeHilClosedLoop:$IncludeHilClosedLoop `
    -IncludeHilCaseMatrix:$IncludeHilCaseMatrix
