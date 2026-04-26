[CmdletBinding(PositionalBinding = $false)]
param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "tests\\reports\\hil-controlled-test",
    [switch]$UseTimestampedReportDir,
    [int]$HilDurationSeconds = 60,
    [int]$HilPauseResumeCycles = 3,
    [int]$HilDispenserCount = 300,
    [int]$HilDispenserIntervalMs = 200,
    [int]$HilDispenserDurationMs = 80,
    [double]$HilStateWaitTimeoutSeconds = 8,
    [switch]$AllowSkipOnMissingHardware,
    [switch]$SkipBuild,
    [switch]$IncludeHilCaseMatrix = $true,
    [switch]$ReuseExistingGateway,
    [string]$OfflinePrereqReport = "",
    [string]$PublishLatestOnPass = "false",
    [string]$PublishLatestReportDir = "tests\\reports\\hil-controlled-test",
    [string]$PythonExe = "python",
    [string]$Executor = "",
    [string]$OperatorOverrideReason = ""
)

$ErrorActionPreference = "Stop"

$quickGateDurationSeconds = 60

function Resolve-AbsolutePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }
    return [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $PathValue))
}

function Assert-PathArgumentNotBooleanLiteral {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArgumentName,
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    $trimmedValue = $PathValue.Trim()
    if ($trimmedValue -imatch '^(true|false)$') {
        throw "$ArgumentName 不能为字面量 '$trimmedValue'。如果要启用布尔开关，请使用命名开关语法，不要把 True/False 作为独立位置参数传入。"
    }
}

function Copy-PathIfPresent {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Source,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    if (-not (Test-Path $Source)) {
        return
    }

    $resolvedSource = [System.IO.Path]::GetFullPath($Source)
    $resolvedDestination = [System.IO.Path]::GetFullPath($Destination)
    if ($resolvedSource -eq $resolvedDestination) {
        return
    }

    $item = Get-Item -LiteralPath $Source
    if ($item.PSIsContainer) {
        if (Test-Path $Destination) {
            Remove-Item -LiteralPath $Destination -Recurse -Force
        }
        Copy-Item -LiteralPath $Source -Destination $Destination -Recurse -Force
        return
    }

    $parent = Split-Path -Parent $Destination
    if (-not [string]::IsNullOrWhiteSpace($parent)) {
        New-Item -ItemType Directory -Path $parent -Force | Out-Null
    }
    Copy-Item -LiteralPath $Source -Destination $Destination -Force
}

function Convert-StrictBooleanArgument {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ArgumentName,
        [Parameter(Mandatory = $true)]
        [string]$ArgumentValue
    )

    $trimmedValue = $ArgumentValue.Trim().TrimStart('$')
    if ($trimmedValue -ieq "true") {
        return $true
    }
    if ($trimmedValue -ieq "false") {
        return $false
    }
    throw "$ArgumentName 必须显式为 true 或 false；当前值为 '$ArgumentValue'。"
}

function Resolve-ExpectedHeadSha {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($env:GITHUB_SHA)) {
        return $env:GITHUB_SHA.Trim()
    }
    $gitHead = (& git -C $WorkspaceRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($gitHead)) {
        throw "Unable to resolve expected offline head SHA for HIL admission."
    }
    return ([string]$gitHead).Trim()
}

$workspaceRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent
$buildScript = Join-Path $workspaceRoot "build.ps1"
$testScript = Join-Path $workspaceRoot "test.ps1"
$hardwareSmokeScript = Join-Path $PSScriptRoot "run_hardware_smoke.py"
$hilClosedLoopScript = Join-Path $PSScriptRoot "run_hil_closed_loop.py"
$hilCaseMatrixScript = Join-Path $PSScriptRoot "run_case_matrix.py"
$gateScript = Join-Path $PSScriptRoot "verify_hil_controlled_gate.py"
$renderSummaryScript = Join-Path $PSScriptRoot "render_hil_controlled_release_summary.py"

foreach ($requiredPath in @($buildScript, $testScript, $hardwareSmokeScript, $hilClosedLoopScript, $hilCaseMatrixScript, $gateScript, $renderSummaryScript)) {
    if (-not (Test-Path $requiredPath)) {
        throw "missing required controlled-hil entry: $requiredPath"
    }
}

Assert-PathArgumentNotBooleanLiteral -ArgumentName "ReportDir" -PathValue $ReportDir
Assert-PathArgumentNotBooleanLiteral -ArgumentName "PublishLatestReportDir" -PathValue $PublishLatestReportDir
if (-not [string]::IsNullOrWhiteSpace($OfflinePrereqReport)) {
    Assert-PathArgumentNotBooleanLiteral -ArgumentName "OfflinePrereqReport" -PathValue $OfflinePrereqReport
}
if (-not $PSBoundParameters.ContainsKey('ReuseExistingGateway')) {
    $ReuseExistingGateway = $true
}

$resolvedReportDir = Resolve-AbsolutePath -WorkspaceRoot $workspaceRoot -PathValue $ReportDir
if ($UseTimestampedReportDir) {
    $stamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
    $resolvedReportDir = Join-Path $resolvedReportDir $stamp
}

$resolvedPublishLatestReportDir = Resolve-AbsolutePath -WorkspaceRoot $workspaceRoot -PathValue $PublishLatestReportDir
$offlinePrereqDir = Join-Path $resolvedReportDir "offline-prereq"
$hardwareSmokeDir = Join-Path $resolvedReportDir "hardware-smoke"
$hilCaseMatrixDir = Join-Path $resolvedReportDir "hil-case-matrix"
$offlinePrereqJsonPath = Join-Path $offlinePrereqDir "workspace-validation.json"
$offlinePrereqMdPath = Join-Path $offlinePrereqDir "workspace-validation.md"
$hardwareSmokeSummaryJsonPath = Join-Path $hardwareSmokeDir "hardware-smoke-summary.json"
$hardwareSmokeSummaryMdPath = Join-Path $hardwareSmokeDir "hardware-smoke-summary.md"
$hilSummaryJsonPath = Join-Path $resolvedReportDir "hil-closed-loop-summary.json"
$hilSummaryMdPath = Join-Path $resolvedReportDir "hil-closed-loop-summary.md"
$hilBundleJsonPath = Join-Path $resolvedReportDir "validation-evidence-bundle.json"
$hilManifestJsonPath = Join-Path $resolvedReportDir "report-manifest.json"
$hilIndexJsonPath = Join-Path $resolvedReportDir "report-index.json"
$hilCaseIndexJsonPath = Join-Path $resolvedReportDir "case-index.json"
$hilEvidenceLinksMdPath = Join-Path $resolvedReportDir "evidence-links.md"
$hilFailureDetailsJsonPath = Join-Path $resolvedReportDir "failure-details.json"
$hilCaseMatrixSummaryJsonPath = Join-Path $hilCaseMatrixDir "case-matrix-summary.json"
$hilCaseMatrixSummaryMdPath = Join-Path $hilCaseMatrixDir "case-matrix-summary.md"
$gateSummaryJsonPath = Join-Path $resolvedReportDir "hil-controlled-gate-summary.json"
$gateSummaryMdPath = Join-Path $resolvedReportDir "hil-controlled-gate-summary.md"
$releaseSummaryMdPath = Join-Path $resolvedReportDir "hil-controlled-release-summary.md"
$resolvedOfflinePrereqReport = if ([string]::IsNullOrWhiteSpace($OfflinePrereqReport)) { "" } else { Resolve-AbsolutePath -WorkspaceRoot $workspaceRoot -PathValue $OfflinePrereqReport }
$publishLatestOnPassEnabled = Convert-StrictBooleanArgument -ArgumentName "PublishLatestOnPass" -ArgumentValue $PublishLatestOnPass
$expectedOfflineHeadSha = Resolve-ExpectedHeadSha -WorkspaceRoot $workspaceRoot

New-Item -ItemType Directory -Path $resolvedReportDir -Force | Out-Null

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
Write-Output "  include_hil_case_matrix=$([bool]$IncludeHilCaseMatrix)"
Write-Output "  reuse_existing_gateway=$([bool]$ReuseExistingGateway)"
Write-Output "  offline_prereq_report=$resolvedOfflinePrereqReport"
Write-Output "  publish_latest_on_pass=$publishLatestOnPassEnabled"
Write-Output "  publish_latest_report_dir=$resolvedPublishLatestReportDir"
Write-Output "  executor=$Executor"
Write-Output "  operator_override_reason=$OperatorOverrideReason"

if ($HilDurationSeconds -ne $quickGateDurationSeconds) {
    throw "formal HIL gate 已禁用；HilDurationSeconds 只能为 $quickGateDurationSeconds 秒。"
}

if ($publishLatestOnPassEnabled) {
    throw "formal latest publish 已禁用；PublishLatestOnPass 必须为 false。"
}

if (-not [string]::IsNullOrWhiteSpace($Executor)) {
    throw "formal HIL executor 签字入口已禁用；请移除 -Executor。"
}
if (-not $ReuseExistingGateway) {
    throw "formal HIL quick gate 必须复用现有 gateway；请移除 -ReuseExistingGateway:`$false。"
}

if ($SkipBuild) {
    Write-Output "hil controlled test: skip build apps (SkipBuild=true)"
} else {
    Write-Output "hil controlled test: build apps (serial)"
    try {
        & $buildScript -Profile $Profile -Suite @("apps")
        $buildExitCode = 0
    } catch {
        $buildExitCode = if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) { $LASTEXITCODE } else { 1 }
        Write-Error $_
    }
    if ($buildExitCode -ne 0) {
        Write-Output "hil controlled test failed: build step exit_code=$buildExitCode"
        exit $buildExitCode
    }
}

if ([string]::IsNullOrWhiteSpace($resolvedOfflinePrereqReport)) {
    Write-Output "hil controlled test: run offline prerequisites"
    try {
        & $testScript `
            -Profile $Profile `
            -Suite @("contracts", "integration", "e2e", "protocol-compatibility") `
            -ReportDir $offlinePrereqDir `
            -Lane "full-offline-gate" `
            -DesiredDepth "full-offline" `
            -FailOnKnownFailure
        $offlineExitCode = 0
    } catch {
        $offlineExitCode = if ($null -ne $LASTEXITCODE -and $LASTEXITCODE -ne 0) { $LASTEXITCODE } else { 1 }
        Write-Error $_
    }
    if ($offlineExitCode -ne 0) {
        Write-Output "hil controlled test failed: offline prerequisite validation exit_code=$offlineExitCode"
        exit $offlineExitCode
    }
}
else {
    if (-not (Test-Path -LiteralPath $resolvedOfflinePrereqReport)) {
        throw "OfflinePrereqReport not found: $resolvedOfflinePrereqReport"
    }

    $resolvedOfflinePrereqMdReport = [System.IO.Path]::ChangeExtension($resolvedOfflinePrereqReport, ".md")
    Write-Output "hil controlled test: reuse offline prerequisites report"
    Write-Output "  source_json=$resolvedOfflinePrereqReport"
    Copy-PathIfPresent -Source $resolvedOfflinePrereqReport -Destination $offlinePrereqJsonPath
    Copy-PathIfPresent -Source $resolvedOfflinePrereqMdReport -Destination $offlinePrereqMdPath
}

Write-Output "hil controlled test: run hardware smoke"
$hardwareSmokeArgs = @(
    $hardwareSmokeScript,
    "--report-dir", $hardwareSmokeDir,
    "--reuse-existing-gateway"
)
if ($AllowSkipOnMissingHardware) {
    $hardwareSmokeArgs += "--allow-skip-on-missing-gateway"
}
& $PythonExe @hardwareSmokeArgs
$hardwareSmokeExitCode = if ($null -ne $LASTEXITCODE) { $LASTEXITCODE } else { 0 }
if ($hardwareSmokeExitCode -ne 0) {
    Write-Output "hil controlled test: hardware smoke produced non-pass exit_code=$hardwareSmokeExitCode"
}

Write-Output "hil controlled test: run hil closed-loop"
$hilClosedLoopArgs = @(
    $hilClosedLoopScript,
    "--report-dir", $resolvedReportDir,
    "--duration-seconds", $HilDurationSeconds,
    "--pause-resume-cycles", $HilPauseResumeCycles,
    "--dispenser-count", $HilDispenserCount,
    "--dispenser-interval-ms", $HilDispenserIntervalMs,
    "--dispenser-duration-ms", $HilDispenserDurationMs,
    "--state-wait-timeout-seconds", $HilStateWaitTimeoutSeconds,
    "--offline-prereq-report", $offlinePrereqJsonPath,
    "--expected-offline-head-sha", $expectedOfflineHeadSha,
    "--expected-offline-lane", "full-offline-gate",
    "--reuse-existing-gateway"
)
if (-not [string]::IsNullOrWhiteSpace($OperatorOverrideReason)) {
    $hilClosedLoopArgs += @("--operator-override-reason", $OperatorOverrideReason)
}
if ($AllowSkipOnMissingHardware) {
    $hilClosedLoopArgs += "--allow-skip-on-missing-gateway"
}
& $PythonExe @hilClosedLoopArgs
$hilClosedLoopExitCode = if ($null -ne $LASTEXITCODE) { $LASTEXITCODE } else { 0 }
if ($hilClosedLoopExitCode -ne 0) {
    Write-Output "hil controlled test: hil closed-loop produced non-pass exit_code=$hilClosedLoopExitCode"
}

$hilCaseMatrixExitCode = 0
if ($IncludeHilCaseMatrix) {
    Write-Output "hil controlled test: run hil case matrix"
    $hilCaseMatrixArgs = @(
        $hilCaseMatrixScript,
        "--report-dir", $hilCaseMatrixDir,
        "--pause-resume-cycles", $HilPauseResumeCycles,
        "--dispenser-count", $HilDispenserCount,
        "--dispenser-interval-ms", $HilDispenserIntervalMs,
        "--dispenser-duration-ms", $HilDispenserDurationMs,
        "--state-wait-timeout-seconds", $HilStateWaitTimeoutSeconds,
        "--offline-prereq-report", $offlinePrereqJsonPath,
        "--expected-offline-head-sha", $expectedOfflineHeadSha,
        "--expected-offline-lane", "full-offline-gate",
        "--reuse-existing-gateway"
    )
    if (-not [string]::IsNullOrWhiteSpace($OperatorOverrideReason)) {
        $hilCaseMatrixArgs += @("--operator-override-reason", $OperatorOverrideReason)
    }
    & $PythonExe @hilCaseMatrixArgs
    $hilCaseMatrixExitCode = if ($null -ne $LASTEXITCODE) { $LASTEXITCODE } else { 0 }
    if ($hilCaseMatrixExitCode -ne 0) {
        Write-Output "hil controlled test: hil case matrix produced non-pass exit_code=$hilCaseMatrixExitCode"
    }
}

Write-Output "hil controlled test: verify controlled gate"
$gateArgs = @(
    $gateScript,
    "--report-dir", $resolvedReportDir,
    "--offline-prereq-json", $offlinePrereqJsonPath,
    "--expected-offline-head-sha", $expectedOfflineHeadSha,
    "--expected-offline-lane", "full-offline-gate",
    "--hardware-smoke-summary-json", $hardwareSmokeSummaryJsonPath,
    "--hil-closed-loop-summary-json", $hilSummaryJsonPath,
    "--hil-evidence-bundle-json", $hilBundleJsonPath
)
if ($IncludeHilCaseMatrix) {
    $gateArgs += @(
        "--require-hil-case-matrix",
        "--hil-case-matrix-summary-json", $hilCaseMatrixSummaryJsonPath
    )
}
& $PythonExe @gateArgs
$gateExitCode = $LASTEXITCODE

Write-Output "hil controlled test: render release summary"
$renderArgs = @(
    $renderSummaryScript,
    "--report-dir", $resolvedReportDir,
    "--profile", $Profile,
    "--gate-summary-json", $gateSummaryJsonPath,
    "--offline-prereq-json", $offlinePrereqJsonPath,
    "--hardware-smoke-summary-json", $hardwareSmokeSummaryJsonPath,
    "--hil-closed-loop-summary-json", $hilSummaryJsonPath
)
if (-not [string]::IsNullOrWhiteSpace($Executor)) {
    $renderArgs += @("--executor", $Executor)
}
if ($IncludeHilCaseMatrix) {
    $renderArgs += @("--hil-case-matrix-summary-json", $hilCaseMatrixSummaryJsonPath)
}
& $PythonExe @renderArgs
$renderExitCode = $LASTEXITCODE

Write-Output "hil controlled test complete"
Write-Output "offline prereq json: $offlinePrereqJsonPath"
Write-Output "offline prereq markdown: $offlinePrereqMdPath"
Write-Output "hardware smoke json: $hardwareSmokeSummaryJsonPath"
Write-Output "hardware smoke markdown: $hardwareSmokeSummaryMdPath"
Write-Output "hil closed-loop summary json: $hilSummaryJsonPath"
Write-Output "hil closed-loop summary markdown: $hilSummaryMdPath"
if ($IncludeHilCaseMatrix) {
    Write-Output "hil case matrix summary json: $hilCaseMatrixSummaryJsonPath"
    Write-Output "hil case matrix summary markdown: $hilCaseMatrixSummaryMdPath"
}
Write-Output "gate summary json: $gateSummaryJsonPath"
Write-Output "gate summary markdown: $gateSummaryMdPath"
Write-Output "release summary markdown: $releaseSummaryMdPath"

if ($gateExitCode -ne 0) {
    Write-Output "hil controlled test failed: gate verification exit_code=$gateExitCode"
    if ($hilCaseMatrixExitCode -ne 0) {
        exit $hilCaseMatrixExitCode
    }
    if ($hilClosedLoopExitCode -ne 0) {
        exit $hilClosedLoopExitCode
    }
    if ($hardwareSmokeExitCode -ne 0) {
        exit $hardwareSmokeExitCode
    }
    exit $gateExitCode
}
if ($renderExitCode -ne 0) {
    Write-Output "hil controlled test failed: release summary rendering exit_code=$renderExitCode"
    exit $renderExitCode
}

if ($hilCaseMatrixExitCode -ne 0) {
    exit $hilCaseMatrixExitCode
}
if ($hilClosedLoopExitCode -ne 0) {
    exit $hilClosedLoopExitCode
}
if ($hardwareSmokeExitCode -ne 0) {
    exit $hardwareSmokeExitCode
}

if (-not $publishLatestOnPassEnabled) {
    Write-Output "skip publishing latest reports: PublishLatestOnPass=false"
    exit 0
}

if ($resolvedReportDir -ieq $resolvedPublishLatestReportDir) {
    $manifestPathSameDir = Join-Path $resolvedPublishLatestReportDir "latest-source.txt"
    $manifestLinesSameDir = [System.Collections.Generic.List[string]]::new()
    @(
        "updated_at_utc=$((Get-Date).ToUniversalTime().ToString('o'))",
        "source_report_dir=$resolvedReportDir",
        "source_offline_prereq_json=$offlinePrereqJsonPath",
        "source_hardware_smoke_summary_json=$hardwareSmokeSummaryJsonPath",
        "source_hil_summary_json=$hilSummaryJsonPath",
        "source_gate_summary_json=$gateSummaryJsonPath",
        "source_release_summary_md=$releaseSummaryMdPath"
    ) | ForEach-Object { $manifestLinesSameDir.Add($_) | Out-Null }
    if ($IncludeHilCaseMatrix) {
        $manifestLinesSameDir.Add("source_hil_case_matrix_summary_json=$hilCaseMatrixSummaryJsonPath") | Out-Null
    }
    $manifestLinesSameDir | Set-Content -Path $manifestPathSameDir -Encoding UTF8
    Write-Output "publish manifest: $manifestPathSameDir"
    exit 0
}

New-Item -ItemType Directory -Path $resolvedPublishLatestReportDir -Force | Out-Null
Copy-PathIfPresent -Source $offlinePrereqDir -Destination (Join-Path $resolvedPublishLatestReportDir "offline-prereq")
Copy-PathIfPresent -Source $hardwareSmokeDir -Destination (Join-Path $resolvedPublishLatestReportDir "hardware-smoke")
if ($IncludeHilCaseMatrix) {
    Copy-PathIfPresent -Source $hilCaseMatrixDir -Destination (Join-Path $resolvedPublishLatestReportDir "hil-case-matrix")
}

foreach ($relativeFile in @(
    "hil-closed-loop-summary.json",
    "hil-closed-loop-summary.md",
    "validation-evidence-bundle.json",
    "report-manifest.json",
    "report-index.json",
    "case-index.json",
    "evidence-links.md",
    "failure-details.json",
    "hil-controlled-gate-summary.json",
    "hil-controlled-gate-summary.md",
    "hil-controlled-release-summary.md"
)) {
    Copy-PathIfPresent `
        -Source (Join-Path $resolvedReportDir $relativeFile) `
        -Destination (Join-Path $resolvedPublishLatestReportDir $relativeFile)
}

$manifestPath = Join-Path $resolvedPublishLatestReportDir "latest-source.txt"
$manifestLines = [System.Collections.Generic.List[string]]::new()
@(
    "updated_at_utc=$((Get-Date).ToUniversalTime().ToString('o'))",
    "source_report_dir=$resolvedReportDir",
    "source_offline_prereq_json=$offlinePrereqJsonPath",
    "source_hardware_smoke_summary_json=$hardwareSmokeSummaryJsonPath",
    "source_hil_summary_json=$hilSummaryJsonPath",
    "source_hil_evidence_bundle_json=$hilBundleJsonPath",
    "source_gate_summary_json=$gateSummaryJsonPath",
    "source_release_summary_md=$releaseSummaryMdPath"
) | ForEach-Object { $manifestLines.Add($_) | Out-Null }
if ($IncludeHilCaseMatrix) {
    $manifestLines.Add("source_hil_case_matrix_summary_json=$hilCaseMatrixSummaryJsonPath") | Out-Null
}
$manifestLines | Set-Content -Path $manifestPath -Encoding UTF8

Write-Output "published latest reports to: $resolvedPublishLatestReportDir"
Write-Output "publish manifest: $manifestPath"
