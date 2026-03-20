param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "integration\reports\controlled-production-test"
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Join-Path $workspaceRoot $ReportDir
$workspaceValidationJson = Join-Path $resolvedReportDir "workspace-validation.json"
$workspaceValidationMd = Join-Path $resolvedReportDir "workspace-validation.md"
$simulatedLineSummaryJson = Join-Path $resolvedReportDir "simulation\simulated-line\simulated-line-summary.json"
$simulatedLineSummaryMd = Join-Path $resolvedReportDir "simulation\simulated-line\simulated-line-summary.md"

Write-Output "controlled-production-test: build simulation suite (serial)"
& (Join-Path $workspaceRoot "build.ps1") -Profile $Profile -Suite simulation

Write-Output "controlled-production-test: test simulation suite (serial)"
& (Join-Path $workspaceRoot "test.ps1") `
    -Profile $Profile `
    -Suite simulation `
    -ReportDir $ReportDir `
    -FailOnKnownFailure

Write-Output "controlled-production-test complete"
Write-Output "workspace validation json: $workspaceValidationJson"
Write-Output "workspace validation markdown: $workspaceValidationMd"
Write-Output "simulated-line summary json: $simulatedLineSummaryJson"
Write-Output "simulated-line summary markdown: $simulatedLineSummaryMd"
