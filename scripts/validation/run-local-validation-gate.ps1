[CmdletBinding()]
param(
    [string]$PythonExe = "python",
    [string]$ReportRoot = "tests/reports/local-validation-gate",
    [ValidateSet("auto", "quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "quick-gate",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "medium",
    [ValidateSet("auto", "quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "quick",
    [string[]]$ChangedScope = @(),
    [string[]]$SkipLayer = @(),
    [string]$SkipJustification = "",
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $repoRoot

function Resolve-LanePolicy {
    param([string]$LaneId)

    switch ($LaneId) {
        "quick-gate" {
            return @{ GateDecision = "blocking"; DefaultFailPolicy = "fail-fast"; TimeoutBudgetSeconds = 900; RetryBudget = 0; FailFastCaseLimit = 1 }
        }
        "full-offline-gate" {
            return @{ GateDecision = "blocking"; DefaultFailPolicy = "collect-and-report"; TimeoutBudgetSeconds = 2700; RetryBudget = 1; FailFastCaseLimit = 0 }
        }
        "nightly-performance" {
            return @{ GateDecision = "blocking"; DefaultFailPolicy = "collect-and-report"; TimeoutBudgetSeconds = 3600; RetryBudget = 1; FailFastCaseLimit = 0 }
        }
        "limited-hil" {
            return @{ GateDecision = "blocking"; DefaultFailPolicy = "manual-signoff-required"; TimeoutBudgetSeconds = 1800; RetryBudget = 0; FailFastCaseLimit = 1 }
        }
        default {
            throw "Unsupported lane policy request: $LaneId"
        }
    }
}

if ($SkipLayer.Count -gt 0 -and [string]::IsNullOrWhiteSpace($SkipJustification)) {
    throw "SkipJustification is required when SkipLayer is not empty."
}

$lanePolicy = Resolve-LanePolicy -LaneId $Lane
Write-Output (
    "local gate lane policy: lane={0} gate_decision={1} fail_policy={2} timeout_budget_seconds={3} retry_budget={4} fail_fast_case_limit={5}" -f
    $Lane,
    $lanePolicy.GateDecision,
    $lanePolicy.DefaultFailPolicy,
    $lanePolicy.TimeoutBudgetSeconds,
    $lanePolicy.RetryBudget,
    $lanePolicy.FailFastCaseLimit
)

$thirdPartyBootstrap = Join-Path $repoRoot "scripts\bootstrap\bootstrap-third-party.ps1"
if (-not (Test-Path $thirdPartyBootstrap)) {
    throw "third-party bootstrap script not found: $thirdPartyBootstrap"
}

Write-Output "third-party bootstrap: powershell -NoProfile -ExecutionPolicy Bypass -File $thirdPartyBootstrap"
& $thirdPartyBootstrap -WorkspaceRoot $repoRoot

$formalGatewayContractGuard = Join-Path $repoRoot "scripts\validation\assert-hmi-formal-gateway-launch-contract.ps1"
if (-not (Test-Path $formalGatewayContractGuard)) {
    throw "HMI formal gateway contract guard not found: $formalGatewayContractGuard"
}

Write-Output "hmi formal gateway contract gate: powershell -NoProfile -ExecutionPolicy Bypass -File $formalGatewayContractGuard"
& $formalGatewayContractGuard -WorkspaceRoot $repoRoot

$supportsNativeErrorPreference = $null -ne (Get-Variable -Name PSNativeCommandUseErrorActionPreference -Scope Global -ErrorAction SilentlyContinue)
if ($supportsNativeErrorPreference) {
    $global:PSNativeCommandUseErrorActionPreference = $false
}

function Resolve-OutputPath {
    param([string]$PathValue)
    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return $PathValue
    }
    return (Join-Path $repoRoot $PathValue)
}

function Invoke-GateStep {
    param(
        [hashtable]$Step,
        [string]$LogsDir
    )

    $logPath = Join-Path $LogsDir $Step.LogName
    $startedAt = Get-Date
    $exitCode = 0
    $errorText = $null

    $commandTail = if ($Step.Command.Count -gt 1) { $Step.Command[1..($Step.Command.Count - 1)] -join " " } else { "" }
    "`n[$($Step.Id)] START $($startedAt.ToString("s"))" | Tee-Object -FilePath $logPath -Append | Out-Null
    "CMD: $($Step.Command[0]) $commandTail" | Tee-Object -FilePath $logPath -Append | Out-Null

    try {
        $prevErrorActionPreference = $ErrorActionPreference
        $ErrorActionPreference = "Continue"
        $global:LASTEXITCODE = 0
        & $Step.Command[0] @($Step.Command[1..($Step.Command.Count - 1)]) *>&1 | Tee-Object -FilePath $logPath -Append | Out-Null
        $ErrorActionPreference = $prevErrorActionPreference
        if ($LASTEXITCODE -ne 0) {
            $exitCode = $LASTEXITCODE
        }
    } catch {
        $ErrorActionPreference = "Stop"
        $exitCode = 1
        $errorText = $_.Exception.Message
        "ERROR: $errorText" | Tee-Object -FilePath $logPath -Append | Out-Null
    }

    $endedAt = Get-Date
    $durationSec = [Math]::Round((New-TimeSpan -Start $startedAt -End $endedAt).TotalSeconds, 3)
    $status = if ($exitCode -eq 0) { "passed" } else { "failed" }

    [pscustomobject]@{
        id           = $Step.Id
        name         = $Step.Name
        status       = $status
        exit_code    = $exitCode
        started_at   = $startedAt.ToString("s")
        ended_at     = $endedAt.ToString("s")
        duration_sec = $durationSec
        log_path     = $logPath
        error        = $errorText
    }
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$resolvedRoot = Resolve-OutputPath -PathValue $ReportRoot
$runDir = Join-Path $resolvedRoot $timestamp
$logsDir = Join-Path $runDir "logs"
$workspaceValidationDir = Join-Path $runDir "workspace-validation"
$workspaceValidationHilClosedLoopDir = Join-Path $runDir "workspace-validation-hil-closed-loop"
$workspaceValidationHilMatrixDir = Join-Path $runDir "workspace-validation-hil-case-matrix"
$dspE2ESpecDir = Join-Path $runDir "dsp-e2e-spec-docset"
$legacyExitDir = Join-Path $runDir "legacy-exit"
$moduleBoundaryDir = Join-Path $runDir "module-boundary-bridges"
$reviewBaselineDir = Join-Path $runDir "review-baseline"

New-Item -ItemType Directory -Force -Path $logsDir | Out-Null
New-Item -ItemType Directory -Force -Path $workspaceValidationDir | Out-Null
if ($IncludeHilClosedLoop) {
    New-Item -ItemType Directory -Force -Path $workspaceValidationHilClosedLoopDir | Out-Null
}
if ($IncludeHilCaseMatrix) {
    New-Item -ItemType Directory -Force -Path $workspaceValidationHilMatrixDir | Out-Null
}
New-Item -ItemType Directory -Force -Path $dspE2ESpecDir | Out-Null
New-Item -ItemType Directory -Force -Path $legacyExitDir | Out-Null
New-Item -ItemType Directory -Force -Path $moduleBoundaryDir | Out-Null
New-Item -ItemType Directory -Force -Path $reviewBaselineDir | Out-Null

$env:SILIGEN_FREEZE_DOCSET_REPORT_DIR = $dspE2ESpecDir
$env:SILIGEN_FREEZE_EVIDENCE_CASES = "success,block,rollback,recovery,archive"
$env:SILIGEN_LEGACY_EXIT_REPORT_DIR = $legacyExitDir

$offlinePrereqStepId = "test-contracts-ci"
$offlinePrereqStepName = "Run contracts suite tests (CI profile)"
$offlinePrereqLogName = "05-test-contracts-ci.log"
$offlinePrereqSuites = @("contracts")
$offlinePrereqLane = $Lane
$offlinePrereqDesiredDepth = $DesiredDepth
$hilOfflinePrereqReport = Join-Path $workspaceValidationDir "workspace-validation.json"
if ($IncludeHilClosedLoop -or $IncludeHilCaseMatrix) {
    $offlinePrereqStepId = "test-offline-prereq-ci"
    $offlinePrereqStepName = "Run limited-hil offline prerequisites via root test entry"
    $offlinePrereqLogName = "05-test-offline-prereq-ci.log"
    $offlinePrereqSuites = @("contracts", "integration", "e2e", "protocol-compatibility")
    $offlinePrereqLane = "full-offline-gate"
    $offlinePrereqDesiredDepth = "full-offline"
    $env:SILIGEN_HIL_OFFLINE_PREREQ_REPORT = $hilOfflinePrereqReport
}

$offlinePrereqCommand = @(
    "powershell",
    "-NoProfile",
    "-ExecutionPolicy",
    "Bypass",
    "-Command",
    (
        "& { " +
        ".\test.ps1 " +
        "-Profile 'CI' " +
        "-Suite @('" + ($offlinePrereqSuites -join "','") + "') " +
        "-ReportDir '" + $workspaceValidationDir + "' " +
        "-Lane '" + $offlinePrereqLane + "' " +
        "-RiskProfile '" + $RiskProfile + "' " +
        "-DesiredDepth '" + $offlinePrereqDesiredDepth + "' " +
        "-FailOnKnownFailure" +
        " }"
    )
)

$steps = @(
    @{
        Id      = "review-baseline"
        Name    = "Validate ARCH-203 review baseline completeness"
        LogName = "00-review-baseline.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\scripts\validation\assert-review-baseline-completeness.ps1",
            "-WorkspaceRoot",
            $repoRoot,
            "-ReportDir",
            $reviewBaselineDir
        )
    },
    @{
        Id      = "workspace-layout"
        Name    = "Validate workspace layout"
        LogName = "01-workspace-layout.log"
        Command = @($PythonExe, ".\scripts\migration\validate_workspace_layout.py")
    },
    @{
        Id      = "dsp-e2e-spec-docset"
        Name    = "Validate DSP E2E Spec docset"
        LogName = "02-dsp-e2e-spec-docset.log"
        Command = @(
            $PythonExe,
            ".\scripts\migration\validate_dsp_e2e_spec_docset.py",
            "--report-dir",
            $dspE2ESpecDir,
            "--report-stem",
            "dsp-e2e-spec-docset"
        )
    },
    @{
        Id      = "legacy-exit"
        Name    = "Validate legacy and bridge exit"
        LogName = "03-legacy-exit.log"
        Command = @(
            $PythonExe,
            ".\scripts\migration\legacy-exit-checks.py",
            "--profile",
            "local",
            "--report-dir",
            $legacyExitDir
        )
    },
    @{
        Id      = "module-boundary-bridges"
        Name    = "Validate runtime module boundary bridges"
        LogName = "04-module-boundary-bridges.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\scripts\validation\assert-module-boundary-bridges.ps1",
            "-WorkspaceRoot",
            $repoRoot,
            "-ReportDir",
            $moduleBoundaryDir
        )
    },
    @{
        Id      = $offlinePrereqStepId
        Name    = $offlinePrereqStepName
        LogName = $offlinePrereqLogName
        Command = $offlinePrereqCommand
    }
)

foreach ($scopeName in $ChangedScope) {
    if (-not [string]::IsNullOrWhiteSpace($scopeName)) {
        $steps[-1].Command += @("-ChangedScope", $scopeName)
    }
}
foreach ($layerName in $SkipLayer) {
    if (-not [string]::IsNullOrWhiteSpace($layerName)) {
        $steps[-1].Command += @("-SkipLayer", $layerName)
    }
}
if (-not [string]::IsNullOrWhiteSpace($SkipJustification)) {
    $steps[-1].Command += @("-SkipJustification", $SkipJustification)
}

if ($IncludeHilClosedLoop) {
    $steps += @{
        Id      = "test-e2e-hil-closed-loop"
        Name    = "Run e2e HIL closed loop via root test entry"
        LogName = "06-test-e2e-hil-closed-loop.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\\test.ps1",
            "-Profile",
            "CI",
            "-Suite",
            "e2e",
            "-ReportDir",
            $workspaceValidationHilClosedLoopDir,
            "-Lane",
            $Lane,
            "-RiskProfile",
            $RiskProfile,
            "-DesiredDepth",
            $DesiredDepth,
            "-FailOnKnownFailure",
            "-IncludeHilClosedLoop"
        )
    }

    foreach ($scopeName in $ChangedScope) {
        if (-not [string]::IsNullOrWhiteSpace($scopeName)) {
            $steps[-1].Command += @("-ChangedScope", $scopeName)
        }
    }
    foreach ($layerName in $SkipLayer) {
        if (-not [string]::IsNullOrWhiteSpace($layerName)) {
            $steps[-1].Command += @("-SkipLayer", $layerName)
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($SkipJustification)) {
        $steps[-1].Command += @("-SkipJustification", $SkipJustification)
    }
}

if ($IncludeHilCaseMatrix) {
    $steps += @{
        Id      = "test-e2e-hil-case-matrix"
        Name    = "Run e2e HIL case matrix via root test entry"
        LogName = "07-test-e2e-hil-case-matrix.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\test.ps1",
            "-Profile",
            "CI",
            "-Suite",
            "e2e",
            "-ReportDir",
            $workspaceValidationHilMatrixDir,
            "-Lane",
            $Lane,
            "-RiskProfile",
            $RiskProfile,
            "-DesiredDepth",
            $DesiredDepth,
            "-FailOnKnownFailure",
            "-IncludeHilCaseMatrix"
        )
    }

    foreach ($scopeName in $ChangedScope) {
        if (-not [string]::IsNullOrWhiteSpace($scopeName)) {
            $steps[-1].Command += @("-ChangedScope", $scopeName)
        }
    }
    foreach ($layerName in $SkipLayer) {
        if (-not [string]::IsNullOrWhiteSpace($layerName)) {
            $steps[-1].Command += @("-SkipLayer", $layerName)
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($SkipJustification)) {
        $steps[-1].Command += @("-SkipJustification", $SkipJustification)
    }
}

$steps += @(
    @{
        Id      = "build-validation-local-contracts"
        Name    = "Build validation (Local/contracts)"
        LogName = "08-build-validation-local-contracts.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\scripts\build\build-validation.ps1",
            "-Profile",
            "Local",
            "-Suite",
            "contracts"
        )
    },
    @{
        Id      = "build-validation-ci-contracts"
        Name    = "Build validation (CI/contracts)"
        LogName = "08-build-validation-ci-contracts.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\scripts\build\build-validation.ps1",
            "-Profile",
            "CI",
            "-Suite",
            "contracts"
        )
    }
)

$results = @()
foreach ($step in $steps) {
    $stepResult = Invoke-GateStep -Step $step -LogsDir $logsDir
    $results += $stepResult
}

$passedCount = @($results | Where-Object { $_.status -eq "passed" }).Count
$failedCount = @($results | Where-Object { $_.status -ne "passed" }).Count
$overallStatus = if ($failedCount -eq 0) { "passed" } else { "failed" }
$summary = [ordered]@{
    generated_at    = (Get-Date).ToString("s")
    branch          = (git branch --show-current)
    lane            = $Lane
    lane_gate_decision = $lanePolicy.GateDecision
    lane_fail_policy = $lanePolicy.DefaultFailPolicy
    lane_timeout_budget_seconds = $lanePolicy.TimeoutBudgetSeconds
    lane_retry_budget = $lanePolicy.RetryBudget
    lane_fail_fast_case_limit = $lanePolicy.FailFastCaseLimit
    report_root     = $resolvedRoot
    run_dir         = $runDir
    freeze_report_dir = $dspE2ESpecDir
    legacy_exit_dir = $legacyExitDir
    module_boundary_dir = $moduleBoundaryDir
    workspace_validation_dir = $workspaceValidationDir
    workspace_validation_hil_closed_loop_dir = $(if ($IncludeHilClosedLoop) { $workspaceValidationHilClosedLoopDir } else { "" })
    workspace_validation_hil_matrix_dir = $(if ($IncludeHilCaseMatrix) { $workspaceValidationHilMatrixDir } else { "" })
    overall_status  = $overallStatus
    total_steps     = $results.Count
    passed_steps    = $passedCount
    failed_steps    = $failedCount
    steps           = $results
}

$jsonPath = Join-Path $runDir "local-validation-gate-summary.json"
$mdPath = Join-Path $runDir "local-validation-gate-summary.md"

$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonPath -Encoding UTF8

$mdLines = @(
    "# Local Validation Gate Summary",
    "",
    "- generated_at: $($summary.generated_at)",
    "- branch: $($summary.branch)",
    "- report_root: $($summary.report_root)",
    "- run_dir: $($summary.run_dir)",
    "- freeze_report_dir: $($summary.freeze_report_dir)",
    "- legacy_exit_dir: $($summary.legacy_exit_dir)",
    "- module_boundary_dir: $($summary.module_boundary_dir)",
    "- workspace_validation_dir: $($summary.workspace_validation_dir)",
    "- workspace_validation_hil_closed_loop_dir: $($summary.workspace_validation_hil_closed_loop_dir)",
    "- workspace_validation_hil_matrix_dir: $($summary.workspace_validation_hil_matrix_dir)",
    "- overall_status: $($summary.overall_status)",
    "- passed_steps: $($summary.passed_steps)/$($summary.total_steps)",
    "",
    "| id | status | exit_code | duration_sec | log_path |",
    "|---|---|---:|---:|---|"
)

foreach ($item in $results) {
    $mdLines += "| $($item.id) | $($item.status) | $($item.exit_code) | $($item.duration_sec) | $($item.log_path) |"
}

$mdLines += ""
$mdLines += "## Step Commands"
$mdLines += ""
foreach ($step in $steps) {
    $mdLines += "- $($step.Id): `"$($step.Command -join ' ')`""
}

$mdLines | Set-Content -Path $mdPath -Encoding UTF8

$requiredArtifacts = @(
    (Join-Path $workspaceValidationDir "workspace-validation.json"),
    (Join-Path $workspaceValidationDir "workspace-validation.md"),
    (Join-Path $dspE2ESpecDir "dsp-e2e-spec-docset.json"),
    (Join-Path $dspE2ESpecDir "dsp-e2e-spec-docset.md"),
    (Join-Path $legacyExitDir "legacy-exit-checks.json"),
    (Join-Path $legacyExitDir "legacy-exit-checks.md"),
    (Join-Path $moduleBoundaryDir "module-boundary-bridges.json"),
    (Join-Path $moduleBoundaryDir "module-boundary-bridges.md"),
    (Join-Path $reviewBaselineDir "review-baseline-completeness.json"),
    (Join-Path $reviewBaselineDir "review-baseline-completeness.md")
)
if ($IncludeHilCaseMatrix) {
    $requiredArtifacts += @(
        (Join-Path $workspaceValidationHilMatrixDir "workspace-validation.json"),
        (Join-Path $workspaceValidationHilMatrixDir "workspace-validation.md"),
        (Join-Path (Join-Path $workspaceValidationHilMatrixDir "hil-case-matrix") "case-matrix-summary.json"),
        (Join-Path (Join-Path $workspaceValidationHilMatrixDir "hil-case-matrix") "case-matrix-summary.md")
    )
}
if ($IncludeHilClosedLoop) {
    $requiredArtifacts += @(
        (Join-Path $workspaceValidationHilClosedLoopDir "workspace-validation.json"),
        (Join-Path $workspaceValidationHilClosedLoopDir "workspace-validation.md"),
        (Join-Path $workspaceValidationHilClosedLoopDir "hil-closed-loop-summary.json"),
        (Join-Path $workspaceValidationHilClosedLoopDir "hil-closed-loop-summary.md")
    )
}
foreach ($artifact in $requiredArtifacts) {
    if (-not (Test-Path $artifact)) {
        Write-Error "Root validation gate did not publish expected report: $artifact"
    }
}

Write-Host "Summary(JSON): $jsonPath"
Write-Host "Summary(MD):   $mdPath"
Write-Host "Overall:       $overallStatus"

if ($overallStatus -ne "passed") {
    exit 1
}

exit 0
