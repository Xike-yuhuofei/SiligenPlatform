param(
    [ValidateSet("all", "apps", "packages", "integration", "protocol-compatibility", "simulation")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "integration\\reports\\ci",
    [switch]$IncludeHardwareSmoke
)

$ErrorActionPreference = "Stop"

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
