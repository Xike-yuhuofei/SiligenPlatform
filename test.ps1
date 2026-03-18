param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "packages", "integration", "protocol-compatibility", "simulation")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "integration\\reports",
    [switch]$FailOnKnownFailure,
    [switch]$IncludeHardwareSmoke
)

$ErrorActionPreference = "Stop"

$runner = Join-Path $PSScriptRoot "tools\\scripts\\invoke-workspace-tests.ps1"
if (-not (Test-Path $runner)) {
    Write-Error "未找到根级 test 入口: $runner"
}

& $runner `
    -Profile $Profile `
    -Suite $Suite `
    -ReportDir $ReportDir `
    -FailOnKnownFailure:$FailOnKnownFailure `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke
