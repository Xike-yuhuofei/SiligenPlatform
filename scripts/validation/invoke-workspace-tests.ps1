[CmdletBinding()]
param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "contracts", "e2e", "protocol-compatibility", "performance")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "tests\\reports",
    [ValidateSet("auto", "quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "auto",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "medium",
    [ValidateSet("auto", "quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "auto",
    [string[]]$ChangedScope = @(),
    [string[]]$SkipLayer = @(),
    [string]$SkipJustification = "",
    [ValidateSet("success", "block", "rollback", "recovery", "archive")]
    [string[]]$FreezeEvidenceCase = @("success", "block", "rollback", "recovery", "archive"),
    [string]$FreezeDocsetReportDir = "",
    [switch]$FailOnKnownFailure,
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$thirdPartyBootstrap = Join-Path $workspaceRoot "scripts\bootstrap\bootstrap-third-party.ps1"
if (-not (Test-Path $thirdPartyBootstrap)) {
    throw "third-party bootstrap script not found: $thirdPartyBootstrap"
}

Write-Output "third-party bootstrap: powershell -NoProfile -ExecutionPolicy Bypass -File $thirdPartyBootstrap"
& $thirdPartyBootstrap -WorkspaceRoot $workspaceRoot

$testKitSrc = Join-Path $workspaceRoot "shared\\testing\\test-kit\\src"

function Resolve-RequestedLane {
    param(
        [string]$LaneId,
        [string]$ProfileName,
        [string[]]$SuiteSet,
        [string]$Depth,
        [switch]$HardwareSmoke,
        [switch]$HilClosedLoop,
        [switch]$HilCaseMatrix
    )

    if ($LaneId -ne "auto") {
        return $LaneId
    }
    if ($HardwareSmoke -or $HilClosedLoop -or $HilCaseMatrix -or $Depth -eq "hil") {
        return "limited-hil"
    }
    if ($Depth -eq "nightly" -or $SuiteSet -contains "performance") {
        return "nightly-performance"
    }
    if ($Depth -eq "full-offline" -or $ProfileName -eq "CI" -or $SuiteSet -contains "e2e") {
        return "full-offline-gate"
    }
    return "quick-gate"
}

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

$resolvedReportDir = if ([System.IO.Path]::IsPathRooted($ReportDir)) {
    [System.IO.Path]::GetFullPath($ReportDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $ReportDir))
}

$requireHmiFormalGatewayContract = ($Suite -contains "all") -or ($Suite -contains "apps") -or ($Suite -contains "contracts") -or ($Suite -contains "e2e")
if ($requireHmiFormalGatewayContract) {
    $formalGatewayContractGuard = Join-Path $workspaceRoot "scripts\\validation\\assert-hmi-formal-gateway-launch-contract.ps1"
    if (-not (Test-Path $formalGatewayContractGuard)) {
        throw "HMI formal gateway contract guard not found: $formalGatewayContractGuard"
    }

    Write-Output "hmi formal gateway contract gate: powershell -NoProfile -ExecutionPolicy Bypass -File $formalGatewayContractGuard"
    & $formalGatewayContractGuard -WorkspaceRoot $workspaceRoot
}

if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = $testKitSrc
} else {
    $env:PYTHONPATH = "$testKitSrc$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}

$layoutValidator = Join-Path $workspaceRoot "scripts\\migration\\validate_workspace_layout.py"
$runtimeWave = "Wave 8"
if (-not (Test-Path $layoutValidator)) {
    throw "Workspace layout validator not found: $layoutValidator"
}

Write-Output "runtime owner/acceptance gate: python $layoutValidator --wave `"$runtimeWave`""
python $layoutValidator --wave $runtimeWave
if ($LASTEXITCODE -ne 0) {
    throw "Runtime owner/acceptance gate failed (wave: $runtimeWave, exit: $LASTEXITCODE)."
}

$env:SILIGEN_FREEZE_EVIDENCE_CASES = ($FreezeEvidenceCase -join ",")
if ([string]::IsNullOrWhiteSpace($FreezeDocsetReportDir)) {
    Remove-Item Env:SILIGEN_FREEZE_DOCSET_REPORT_DIR -ErrorAction SilentlyContinue
} else {
    $env:SILIGEN_FREEZE_DOCSET_REPORT_DIR = if ([System.IO.Path]::IsPathRooted($FreezeDocsetReportDir)) {
        [System.IO.Path]::GetFullPath($FreezeDocsetReportDir)
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $FreezeDocsetReportDir))
    }
}

$argsList = @(
    "-m",
    "test_kit.workspace_validation",
    "--profile",
    $Profile.ToLowerInvariant(),
    "--report-dir",
    $resolvedReportDir
)

foreach ($suiteName in $Suite) {
    $argsList += @("--suite", $suiteName)
}

if ($SkipLayer.Count -gt 0 -and [string]::IsNullOrWhiteSpace($SkipJustification)) {
    throw "SkipJustification is required when SkipLayer is not empty."
}

$resolvedLaneForPolicy = Resolve-RequestedLane `
    -LaneId $Lane `
    -ProfileName $Profile `
    -SuiteSet $Suite `
    -Depth $DesiredDepth `
    -HardwareSmoke:$IncludeHardwareSmoke `
    -HilClosedLoop:$IncludeHilClosedLoop `
    -HilCaseMatrix:$IncludeHilCaseMatrix
$lanePolicy = Resolve-LanePolicy -LaneId $resolvedLaneForPolicy
Write-Output (
    "workspace validation lane policy: lane={0} gate_decision={1} fail_policy={2} timeout_budget_seconds={3} retry_budget={4} fail_fast_case_limit={5}" -f
    $resolvedLaneForPolicy,
    $lanePolicy.GateDecision,
    $lanePolicy.DefaultFailPolicy,
    $lanePolicy.TimeoutBudgetSeconds,
    $lanePolicy.RetryBudget,
    $lanePolicy.FailFastCaseLimit
)

$argsList += @("--lane", $Lane)
$argsList += @("--risk-profile", $RiskProfile)
$argsList += @("--desired-depth", $DesiredDepth)
foreach ($scopeName in $ChangedScope) {
    if (-not [string]::IsNullOrWhiteSpace($scopeName)) {
        $argsList += @("--changed-scope", $scopeName)
    }
}
foreach ($layerName in $SkipLayer) {
    if (-not [string]::IsNullOrWhiteSpace($layerName)) {
        $argsList += @("--skip-layer", $layerName)
    }
}
if (-not [string]::IsNullOrWhiteSpace($SkipJustification)) {
    $argsList += @("--skip-justification", $SkipJustification)
}

if ($IncludeHardwareSmoke) {
    $argsList += "--include-hardware-smoke"
}

if ($IncludeHilClosedLoop) {
    $argsList += "--include-hil-closed-loop"
}

if ($IncludeHilCaseMatrix) {
    $argsList += "--include-hil-case-matrix"
}

if ($FailOnKnownFailure) {
    $argsList += "--fail-on-known-failure"
}

python @argsList
$workspaceValidationExitCode = $LASTEXITCODE
if ($workspaceValidationExitCode -ne 0) {
    throw "workspace validation failed (exit: $workspaceValidationExitCode)."
}

$workspaceValidationJson = Join-Path $resolvedReportDir "workspace-validation.json"
if (Test-Path $workspaceValidationJson) {
    $workspaceValidation = Get-Content -LiteralPath $workspaceValidationJson -Raw | ConvertFrom-Json
    $baselineGovernance = $workspaceValidation.metadata.baseline_governance_summary
    if ($null -ne $baselineGovernance) {
        Write-Output (
            "baseline governance summary: manifest_entries={0} blocking_issues={1} advisory_issues={2} duplicate_sources={3} deprecated_root_files={4} stale_baselines={5} unused_baselines={6}" -f
            $baselineGovernance.entry_count,
            $baselineGovernance.blocking_issue_count,
            $baselineGovernance.advisory_issue_count,
            @($baselineGovernance.duplicate_source_issues).Count,
            @($baselineGovernance.deprecated_root_issues).Count,
            @($baselineGovernance.stale_baseline_ids).Count,
            @($baselineGovernance.unused_baseline_paths).Count
        )
        if (@($baselineGovernance.stale_baseline_ids).Count -gt 0 -or @($baselineGovernance.unused_baseline_paths).Count -gt 0) {
            Write-Warning "Baseline governance detected advisory findings; inspect workspace-validation.json for details."
        }
    }
}
