param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "tests\\reports\\controlled-production-test",
    [switch]$UseTimestampedReportDir,
    [bool]$PublishLatestOnPass = $true,
    [string]$PublishLatestReportDir = "tests\\reports\\controlled-production-test",
    [string]$PythonExe = "python",
    [string]$Executor = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$buildScript = Join-Path $workspaceRoot "build.ps1"
$testScript = Join-Path $workspaceRoot "test.ps1"
$gateScript = Join-Path $PSScriptRoot "verify_controlled_production_gate.py"
$renderSummaryScript = Join-Path $PSScriptRoot "render_controlled_production_release_summary.py"

if (-not (Test-Path $buildScript)) {
    throw "missing build entry: $buildScript"
}
if (-not (Test-Path $testScript)) {
    throw "missing test entry: $testScript"
}
if (-not (Test-Path $gateScript)) {
    throw "missing gate script: $gateScript"
}
if (-not (Test-Path $renderSummaryScript)) {
    throw "missing release summary renderer: $renderSummaryScript"
}

$resolvedReportDir = if ([System.IO.Path]::IsPathRooted($ReportDir)) {
    [System.IO.Path]::GetFullPath($ReportDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $ReportDir))
}

if ($UseTimestampedReportDir) {
    $stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
    $resolvedReportDir = Join-Path $resolvedReportDir $stamp
}

$resolvedPublishLatestReportDir = if ([System.IO.Path]::IsPathRooted($PublishLatestReportDir)) {
    [System.IO.Path]::GetFullPath($PublishLatestReportDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PublishLatestReportDir))
}

$workspaceValidationJson = Join-Path $resolvedReportDir "workspace-validation.json"
$workspaceValidationMd = Join-Path $resolvedReportDir "workspace-validation.md"
$simulatedLineSummaryJson = Join-Path $resolvedReportDir "e2e\\simulated-line\\simulated-line-summary.json"
$simulatedLineSummaryMd = Join-Path $resolvedReportDir "e2e\\simulated-line\\simulated-line-summary.md"
$gateSummaryJson = Join-Path $resolvedReportDir "controlled-production-gate-summary.json"
$gateSummaryMd = Join-Path $resolvedReportDir "controlled-production-gate-summary.md"
$releaseSummaryMd = Join-Path $resolvedReportDir "simulation-controlled-production-release-summary.md"

Write-Output "controlled-production-test: parameter snapshot"
Write-Output "  profile=$Profile"
Write-Output "  report_dir=$resolvedReportDir"
Write-Output "  use_timestamped_report_dir=$([bool]$UseTimestampedReportDir)"
Write-Output "  publish_latest_on_pass=$PublishLatestOnPass"
Write-Output "  publish_latest_report_dir=$resolvedPublishLatestReportDir"

Write-Output "controlled-production-test: build e2e suite (serial)"
& $buildScript -Profile $Profile -Suite e2e
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) {
    Write-Output "controlled-production-test failed: build step exit_code=$buildExitCode"
    exit $buildExitCode
}

Write-Output "controlled-production-test: test e2e suite (serial)"
& $testScript `
    -Profile $Profile `
    -Suite e2e `
    -ReportDir $resolvedReportDir `
    -FailOnKnownFailure
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) {
    Write-Output "controlled-production-test failed: test step exit_code=$testExitCode"
    exit $testExitCode
}

Write-Output "controlled-production-test: verify controlled-production gate"
& $PythonExe $gateScript --report-dir $resolvedReportDir
$gateExitCode = $LASTEXITCODE

Write-Output "controlled-production-test: render release summary"
$renderArgs = @(
    $renderSummaryScript,
    "--report-dir", $resolvedReportDir,
    "--profile", $Profile
)
if (-not [string]::IsNullOrWhiteSpace($Executor)) {
    $renderArgs += @("--executor", $Executor)
}
& $PythonExe @renderArgs
$renderExitCode = $LASTEXITCODE

Write-Output "controlled-production-test complete"
Write-Output "workspace validation json: $workspaceValidationJson"
Write-Output "workspace validation markdown: $workspaceValidationMd"
Write-Output "simulated-line summary json: $simulatedLineSummaryJson"
Write-Output "simulated-line summary markdown: $simulatedLineSummaryMd"
Write-Output "gate summary json: $gateSummaryJson"
Write-Output "gate summary markdown: $gateSummaryMd"
Write-Output "release summary markdown: $releaseSummaryMd"

if ($gateExitCode -ne 0) {
    Write-Output "controlled-production-test failed: gate verification exit_code=$gateExitCode"
    exit $gateExitCode
}
if ($renderExitCode -ne 0) {
    Write-Output "controlled-production-test failed: release summary rendering exit_code=$renderExitCode"
    exit $renderExitCode
}

if (-not $PublishLatestOnPass) {
    Write-Output "skip publishing latest reports: PublishLatestOnPass=false"
    exit 0
}

if ($resolvedReportDir -ieq $resolvedPublishLatestReportDir) {
    Write-Output "latest reports already in canonical dir: $resolvedPublishLatestReportDir"
    $manifestPathSameDir = Join-Path $resolvedPublishLatestReportDir "latest-source.txt"
    $manifestLinesSameDir = @(
        "updated_at_utc=$((Get-Date).ToUniversalTime().ToString('o'))",
        "source_report_dir=$resolvedReportDir",
        "source_workspace_validation_json=$workspaceValidationJson",
        "source_simulated_line_summary_json=$simulatedLineSummaryJson",
        "source_gate_summary_json=$gateSummaryJson",
        "source_release_summary_md=$releaseSummaryMd"
    )
    $manifestLinesSameDir | Set-Content -Path $manifestPathSameDir -Encoding UTF8
    Write-Output "publish manifest: $manifestPathSameDir"
    exit 0
}

New-Item -ItemType Directory -Path $resolvedPublishLatestReportDir -Force | Out-Null
New-Item -ItemType Directory -Path (Join-Path $resolvedPublishLatestReportDir "e2e\\simulated-line") -Force | Out-Null

Copy-Item -Path $workspaceValidationJson -Destination (Join-Path $resolvedPublishLatestReportDir "workspace-validation.json") -Force
Copy-Item -Path $workspaceValidationMd -Destination (Join-Path $resolvedPublishLatestReportDir "workspace-validation.md") -Force
Copy-Item -Path $simulatedLineSummaryJson -Destination (Join-Path $resolvedPublishLatestReportDir "e2e\\simulated-line\\simulated-line-summary.json") -Force
Copy-Item -Path $simulatedLineSummaryMd -Destination (Join-Path $resolvedPublishLatestReportDir "e2e\\simulated-line\\simulated-line-summary.md") -Force
Copy-Item -Path $gateSummaryJson -Destination (Join-Path $resolvedPublishLatestReportDir "controlled-production-gate-summary.json") -Force
Copy-Item -Path $gateSummaryMd -Destination (Join-Path $resolvedPublishLatestReportDir "controlled-production-gate-summary.md") -Force
Copy-Item -Path $releaseSummaryMd -Destination (Join-Path $resolvedPublishLatestReportDir "simulation-controlled-production-release-summary.md") -Force

$manifestPath = Join-Path $resolvedPublishLatestReportDir "latest-source.txt"
$manifestLines = @(
    "updated_at_utc=$((Get-Date).ToUniversalTime().ToString('o'))",
    "source_report_dir=$resolvedReportDir",
    "source_workspace_validation_json=$workspaceValidationJson",
    "source_simulated_line_summary_json=$simulatedLineSummaryJson",
    "source_gate_summary_json=$gateSummaryJson",
    "source_release_summary_md=$releaseSummaryMd"
)
$manifestLines | Set-Content -Path $manifestPath -Encoding UTF8

Write-Output "published latest reports to: $resolvedPublishLatestReportDir"
Write-Output "publish manifest: $manifestPath"
