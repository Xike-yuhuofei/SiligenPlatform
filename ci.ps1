param(
    [ValidateSet("all", "apps", "packages", "integration", "protocol-compatibility", "simulation")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "integration\\reports\\ci",
    [switch]$IncludeHardwareSmoke,
    [string]$LocalValidationReportRoot = "integration\\reports\\local-validation-gate",
    [switch]$SkipLocalValidationGate
)

$ErrorActionPreference = "Stop"

$isDefaultSuite = ($Suite.Count -eq 1 -and $Suite[0] -eq "all")
if (-not $SkipLocalValidationGate -and $isDefaultSuite) {
    & (Join-Path $PSScriptRoot "tools\\scripts\\run-local-validation-gate.ps1") `
        -ReportRoot $LocalValidationReportRoot
} elseif ($SkipLocalValidationGate) {
    Write-Host "skip local validation gate: -SkipLocalValidationGate"
} else {
    Write-Host "skip local validation gate: custom suite requested -> $($Suite -join ',')"
}

& (Join-Path $PSScriptRoot "legacy-exit-check.ps1") `
    -Profile CI `
    -ReportDir (Join-Path $ReportDir "legacy-exit")

& (Join-Path $PSScriptRoot "build.ps1") -Profile CI -Suite $Suite
& (Join-Path $PSScriptRoot "test.ps1") `
    -Profile CI `
    -Suite $Suite `
    -ReportDir $ReportDir `
    -FailOnKnownFailure `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke
