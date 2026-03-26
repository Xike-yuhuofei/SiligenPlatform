param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "tests\\reports\\hil-controlled-test",
    [switch]$UseTimestampedReportDir,
    [int]$HilDurationSeconds = 1800,
    [int]$HilPauseResumeCycles = 3,
    [int]$HilDispenserCount = 300,
    [int]$HilDispenserIntervalMs = 200,
    [int]$HilDispenserDurationMs = 80,
    [double]$HilStateWaitTimeoutSeconds = 8,
    [switch]$AllowSkipOnMissingHardware,
    [switch]$SkipBuild,
    [bool]$PublishLatestOnPass = $true,
    [string]$PublishLatestReportDir = "tests\\reports\\hil-controlled-test",
    [string]$PythonExe = "python",
    [string]$Executor = ""
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

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$buildScript = Join-Path $workspaceRoot "build.ps1"
$testScript = Join-Path $workspaceRoot "test.ps1"
$gateScript = Join-Path $PSScriptRoot "verify_hil_controlled_gate.py"
$renderSummaryScript = Join-Path $PSScriptRoot "render_hil_controlled_release_summary.py"

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
Write-Output "  profile=$Profile"
Write-Output "  report_dir=$resolvedReportDir"
Write-Output "  use_timestamped_report_dir=$([bool]$UseTimestampedReportDir)"
Write-Output "  duration_seconds=$HilDurationSeconds"
Write-Output "  pause_resume_cycles=$HilPauseResumeCycles"
Write-Output "  dispenser_count=$HilDispenserCount"
Write-Output "  dispenser_interval_ms=$HilDispenserIntervalMs"
Write-Output "  dispenser_duration_ms=$HilDispenserDurationMs"
Write-Output "  state_wait_timeout_seconds=$HilStateWaitTimeoutSeconds"
Write-Output "  allow_skip_on_missing_hardware=$([bool]$AllowSkipOnMissingHardware)"
Write-Output "  skip_build=$([bool]$SkipBuild)"
Write-Output "  publish_latest_on_pass=$PublishLatestOnPass"
Write-Output "  publish_latest_report_dir=$resolvedPublishLatestReportDir"
Write-Output "  python_exe=$PythonExe"

if ($SkipBuild) {
    Write-Output "hil controlled test: skip build apps (SkipBuild=true)"
} else {
    Write-Output "hil controlled test: build apps (serial)"
    powershell -NoProfile -ExecutionPolicy Bypass -File $buildScript -Profile $Profile -Suite apps
    $buildExitCode = $LASTEXITCODE
    if ($buildExitCode -ne 0) {
        Write-Output "hil controlled test failed: build step exit_code=$buildExitCode"
        exit $buildExitCode
    }
}

Write-Output "hil controlled test: run e2e validation with hardware-smoke + hil-closed-loop (serial)"
powershell -NoProfile -ExecutionPolicy Bypass -File $testScript `
    -Profile $Profile `
    -Suite e2e `
    -ReportDir $resolvedReportDir `
    -IncludeHardwareSmoke `
    -IncludeHilClosedLoop `
    -FailOnKnownFailure
$testExitCode = $LASTEXITCODE
if ($testExitCode -ne 0) {
    Write-Output "hil controlled test failed: e2e validation exit_code=$testExitCode"
    exit $testExitCode
}

$workspaceValidationJsonPath = Join-Path $resolvedReportDir "workspace-validation.json"
$workspaceValidationMdPath = Join-Path $resolvedReportDir "workspace-validation.md"
$hilSummaryJsonPath = Resolve-HilSummaryPath -ResolvedReportDir $resolvedReportDir -FileName "hil-closed-loop-summary.json"
$hilSummaryMdPath = Resolve-HilSummaryPath -ResolvedReportDir $resolvedReportDir -FileName "hil-closed-loop-summary.md"
$gateSummaryJsonPath = Join-Path $resolvedReportDir "hil-controlled-gate-summary.json"
$gateSummaryMdPath = Join-Path $resolvedReportDir "hil-controlled-gate-summary.md"
$releaseSummaryMdPath = Join-Path $resolvedReportDir "hil-controlled-release-summary.md"

Write-Output "hil controlled test: verify controlled gate"
& $PythonExe $gateScript `
    --report-dir $resolvedReportDir `
    --workspace-validation-json $workspaceValidationJsonPath `
    --hil-closed-loop-summary-json $hilSummaryJsonPath
$gateExitCode = $LASTEXITCODE

Write-Output "hil controlled test: render release summary"
$renderArgs = @(
    $renderSummaryScript,
    "--report-dir", $resolvedReportDir,
    "--profile", $Profile,
    "--workspace-validation-json", $workspaceValidationJsonPath,
    "--hil-closed-loop-summary-json", $hilSummaryJsonPath,
    "--gate-summary-json", $gateSummaryJsonPath
)
if (-not [string]::IsNullOrWhiteSpace($Executor)) {
    $renderArgs += @("--executor", $Executor)
}
& $PythonExe @renderArgs
$renderExitCode = $LASTEXITCODE

Write-Output "hil controlled test complete"
Write-Output "workspace validation json: $workspaceValidationJsonPath"
Write-Output "workspace validation markdown: $workspaceValidationMdPath"
Write-Output "hil closed-loop summary json: $hilSummaryJsonPath"
Write-Output "hil closed-loop summary markdown: $hilSummaryMdPath"
Write-Output "gate summary json: $gateSummaryJsonPath"
Write-Output "gate summary markdown: $gateSummaryMdPath"
Write-Output "release summary markdown: $releaseSummaryMdPath"

if ($gateExitCode -ne 0) {
    Write-Output "hil controlled test failed: gate verification exit_code=$gateExitCode"
    exit $gateExitCode
}
if ($renderExitCode -ne 0) {
    Write-Output "hil controlled test failed: release summary rendering exit_code=$renderExitCode"
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
        "source_workspace_validation_json=$workspaceValidationJsonPath",
        "source_hil_summary_json=$hilSummaryJsonPath",
        "source_gate_summary_json=$gateSummaryJsonPath",
        "source_release_summary_md=$releaseSummaryMdPath"
    )
    $manifestLinesSameDir | Set-Content -Path $manifestPathSameDir -Encoding UTF8
    Write-Output "publish manifest: $manifestPathSameDir"
    exit 0
}

New-Item -ItemType Directory -Path $resolvedPublishLatestReportDir -Force | Out-Null
Copy-Item -Path $workspaceValidationJsonPath -Destination (Join-Path $resolvedPublishLatestReportDir "workspace-validation.json") -Force
Copy-Item -Path $workspaceValidationMdPath -Destination (Join-Path $resolvedPublishLatestReportDir "workspace-validation.md") -Force
Copy-Item -Path $hilSummaryJsonPath -Destination (Join-Path $resolvedPublishLatestReportDir "hil-closed-loop-summary.json") -Force
Copy-Item -Path $hilSummaryMdPath -Destination (Join-Path $resolvedPublishLatestReportDir "hil-closed-loop-summary.md") -Force
Copy-Item -Path $gateSummaryJsonPath -Destination (Join-Path $resolvedPublishLatestReportDir "hil-controlled-gate-summary.json") -Force
Copy-Item -Path $gateSummaryMdPath -Destination (Join-Path $resolvedPublishLatestReportDir "hil-controlled-gate-summary.md") -Force
Copy-Item -Path $releaseSummaryMdPath -Destination (Join-Path $resolvedPublishLatestReportDir "hil-controlled-release-summary.md") -Force

$manifestPath = Join-Path $resolvedPublishLatestReportDir "latest-source.txt"
$manifestLines = @(
    "updated_at_utc=$((Get-Date).ToUniversalTime().ToString('o'))",
    "source_report_dir=$resolvedReportDir",
    "source_workspace_validation_json=$workspaceValidationJsonPath",
    "source_hil_summary_json=$hilSummaryJsonPath",
    "source_gate_summary_json=$gateSummaryJsonPath",
    "source_release_summary_md=$releaseSummaryMdPath"
)
$manifestLines | Set-Content -Path $manifestPath -Encoding UTF8

Write-Output "published latest reports to: $resolvedPublishLatestReportDir"
Write-Output "publish manifest: $manifestPath"
