param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "integration\\reports\\hil-controlled-test",
    [int]$HilDurationSeconds = 1800,
    [int]$HilPauseResumeCycles = 1,
    [int]$HilDispenserCount = 300,
    [int]$HilDispenserIntervalMs = 200,
    [int]$HilDispenserDurationMs = 80,
    [double]$HilStateWaitTimeoutSeconds = 8,
    [switch]$AllowSkipOnMissingHardware
)

$ErrorActionPreference = "Stop"

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

Write-Output "hil controlled test: build apps (profile=$Profile)"
powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript -Profile $Profile -Suite apps

Write-Output "hil controlled test: run integration validation with hardware-smoke + hil-closed-loop"
powershell -NoProfile -ExecutionPolicy Bypass -File $testScript `
    -Profile $Profile `
    -Suite integration `
    -ReportDir $ReportDir `
    -IncludeHardwareSmoke `
    -IncludeHilClosedLoop `
    -FailOnKnownFailure

$resolvedReportDir = Join-Path $workspaceRoot $ReportDir
Write-Output "hil controlled test complete"
Write-Output "workspace validation report: $resolvedReportDir\\workspace-validation.json"
Write-Output "hil closed-loop report: $resolvedReportDir\\hil-closed-loop-summary.json"
