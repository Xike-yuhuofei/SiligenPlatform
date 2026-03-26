param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "contracts", "e2e", "protocol-compatibility", "performance")]
    [string[]]$Suite = @("all"),
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

& $runner -Profile $Profile -Suite $Suite -SkipHeavyTargets:$SkipHeavyTargets
