param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "integration\\reports\\hil-controlled-test",
    [int]$HilDurationSeconds = 1800,
    [int]$HilPauseResumeCycles = 3,
    [int]$HilDispenserCount = 300,
    [int]$HilDispenserIntervalMs = 200,
    [int]$HilDispenserDurationMs = 80,
    [double]$HilStateWaitTimeoutSeconds = 8,
    [switch]$AllowSkipOnMissingHardware,
    [bool]$PublishLatestOnPass = $true,
    [string]$PublishLatestReportDir = "integration\\reports\\hil-controlled-test"
)

$ErrorActionPreference = "Stop"

function Resolve-HilSummaryPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedReportDir,
        [Parameter(Mandatory = $true)]
        [string]$FileName
    )

    $candidates = @(
        (Join-Path $ResolvedReportDir $FileName),
        (Join-Path (Join-Path $ResolvedReportDir "hil-controlled-test") $FileName)
    )
    foreach ($candidate in $candidates) {
        if (Test-Path $candidate) {
            return $candidate
        }
    }
    throw "missing hil report artifact: $FileName under $ResolvedReportDir"
}

function Get-IntFieldOrDefault {
    param(
        [Parameter(Mandatory = $true)]
        [object]$Object,
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [int]$DefaultValue = 0
    )

    if ($null -eq $Object) {
        return $DefaultValue
    }
    if ($Object.PSObject.Properties.Name -contains $Name) {
        return [int]$Object.$Name
    }
    return $DefaultValue
}

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$buildScript = Join-Path $workspaceRoot "build.ps1"
$testScript = Join-Path $workspaceRoot "test.ps1"

if (-not (Test-Path $buildScript)) {
    throw "missing build entry: $buildScript"
}
if (-not (Test-Path $testScript)) {
    throw "missing test entry: $testScript"
}

$env:SILIGEN_HIL_DURATION_SECONDS = [string]$HilDurationSeconds
$env:SILIGEN_HIL_PAUSE_RESUME_CYCLES = [string]$HilPauseResumeCycles
$env:SILIGEN_HIL_DISPENSER_COUNT = [string]$HilDispenserCount
$env:SILIGEN_HIL_DISPENSER_INTERVAL_MS = [string]$HilDispenserIntervalMs
$env:SILIGEN_HIL_DISPENSER_DURATION_MS = [string]$HilDispenserDurationMs
$env:SILIGEN_HIL_STATE_WAIT_TIMEOUT_SECONDS = [string]$HilStateWaitTimeoutSeconds
if ($AllowSkipOnMissingHardware) {
    $env:SILIGEN_HIL_ALLOW_SKIP_ON_MISSING = "1"
} else {
    $env:SILIGEN_HIL_ALLOW_SKIP_ON_MISSING = "0"
}

Write-Output "hil controlled test: parameter snapshot"
Write-Output "  duration_seconds=$HilDurationSeconds"
Write-Output "  pause_resume_cycles=$HilPauseResumeCycles"
Write-Output "  dispenser_count=$HilDispenserCount"
Write-Output "  dispenser_interval_ms=$HilDispenserIntervalMs"
Write-Output "  dispenser_duration_ms=$HilDispenserDurationMs"
Write-Output "  state_wait_timeout_seconds=$HilStateWaitTimeoutSeconds"
Write-Output "  allow_skip_on_missing_hardware=$([bool]$AllowSkipOnMissingHardware)"
Write-Output "  publish_latest_on_pass=$PublishLatestOnPass"
Write-Output "  publish_latest_report_dir=$PublishLatestReportDir"

Write-Output "hil controlled test: build apps (profile=$Profile)"
powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript -Profile $Profile -Suite apps
$buildExitCode = $LASTEXITCODE
if ($buildExitCode -ne 0) {
    Write-Output "hil controlled test failed: build step exit_code=$buildExitCode"
    exit $buildExitCode
}

Write-Output "hil controlled test: run integration validation with hardware-smoke + hil-closed-loop"
powershell -NoProfile -ExecutionPolicy Bypass -File $testScript `
    -Profile $Profile `
    -Suite integration `
    -ReportDir $ReportDir `
    -IncludeHardwareSmoke `
    -IncludeHilClosedLoop `
    -FailOnKnownFailure
$testExitCode = $LASTEXITCODE

$resolvedReportDir = Join-Path $workspaceRoot $ReportDir
$resolvedReportDir = [System.IO.Path]::GetFullPath($resolvedReportDir)
$resolvedPublishLatestReportDir = Join-Path $workspaceRoot $PublishLatestReportDir
$resolvedPublishLatestReportDir = [System.IO.Path]::GetFullPath($resolvedPublishLatestReportDir)
$workspaceValidationJsonPath = Join-Path $resolvedReportDir "workspace-validation.json"
$workspaceValidationMdPath = Join-Path $resolvedReportDir "workspace-validation.md"
$hilSummaryJsonPath = Resolve-HilSummaryPath -ResolvedReportDir $resolvedReportDir -FileName "hil-closed-loop-summary.json"
$hilSummaryMdPath = Resolve-HilSummaryPath -ResolvedReportDir $resolvedReportDir -FileName "hil-closed-loop-summary.md"

Write-Output "hil controlled test complete"
Write-Output "workspace validation report: $workspaceValidationJsonPath"
Write-Output "hil closed-loop report: $hilSummaryJsonPath"

if ($testExitCode -ne 0) {
    Write-Output "hil controlled test failed: integration validation exit_code=$testExitCode"
    exit $testExitCode
}

if (-not $PublishLatestOnPass) {
    Write-Output "skip publishing latest reports: PublishLatestOnPass=false"
    exit 0
}

$workspaceValidation = Get-Content -Raw $workspaceValidationJsonPath | ConvertFrom-Json
$workspaceCounts = $workspaceValidation.counts
$failedCount = Get-IntFieldOrDefault -Object $workspaceCounts -Name "failed" -DefaultValue 0
$knownFailureCount = Get-IntFieldOrDefault -Object $workspaceCounts -Name "known_failure" -DefaultValue 0
$skippedCount = Get-IntFieldOrDefault -Object $workspaceCounts -Name "skipped" -DefaultValue 0

$hilSummary = Get-Content -Raw $hilSummaryJsonPath | ConvertFrom-Json
$hilOverallStatus = [string]$hilSummary.overall_status
$transitionStatuses = @()
if ($null -ne $hilSummary.state_transition_checks) {
    $transitionStatuses = @($hilSummary.state_transition_checks | ForEach-Object { [string]$_.status })
}
$hasTransitionGateFailure = $transitionStatuses | Where-Object { $_ -ne "passed" } | Select-Object -First 1

if ($hilOverallStatus -ne "passed" -or $failedCount -ne 0 -or $knownFailureCount -ne 0 -or $skippedCount -ne 0 -or $hasTransitionGateFailure) {
    Write-Output "skip publishing latest reports: pass gate not satisfied"
    Write-Output "  hil_overall_status=$hilOverallStatus"
    Write-Output "  counts_failed=$failedCount counts_known_failure=$knownFailureCount counts_skipped=$skippedCount"
    if ($hasTransitionGateFailure) {
        Write-Output "  transition_gate_status=failed"
    }
    exit 0
}

if ($resolvedReportDir -ieq $resolvedPublishLatestReportDir) {
    Write-Output "latest reports already in canonical dir: $resolvedPublishLatestReportDir"
    exit 0
}

New-Item -ItemType Directory -Path $resolvedPublishLatestReportDir -Force | Out-Null
Copy-Item -Path $workspaceValidationJsonPath -Destination (Join-Path $resolvedPublishLatestReportDir "workspace-validation.json") -Force
Copy-Item -Path $workspaceValidationMdPath -Destination (Join-Path $resolvedPublishLatestReportDir "workspace-validation.md") -Force
Copy-Item -Path $hilSummaryJsonPath -Destination (Join-Path $resolvedPublishLatestReportDir "hil-closed-loop-summary.json") -Force
Copy-Item -Path $hilSummaryMdPath -Destination (Join-Path $resolvedPublishLatestReportDir "hil-closed-loop-summary.md") -Force

$manifestPath = Join-Path $resolvedPublishLatestReportDir "latest-source.txt"
$manifestLines = @(
    "updated_at_utc=$((Get-Date).ToUniversalTime().ToString('o'))",
    "source_report_dir=$resolvedReportDir",
    "source_workspace_validation_json=$workspaceValidationJsonPath",
    "source_hil_summary_json=$hilSummaryJsonPath"
)
$manifestLines | Set-Content -Path $manifestPath -Encoding UTF8

Write-Output "published latest reports to: $resolvedPublishLatestReportDir"
Write-Output "publish manifest: $manifestPath"
