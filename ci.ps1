param(
    [ValidateSet("all", "apps", "contracts", "protocol-compatibility", "integration", "e2e", "performance")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "tests\\reports\\ci",
    [ValidateSet("auto", "quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "full-offline-gate",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "medium",
    [ValidateSet("auto", "quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "auto",
    [string[]]$ChangedScope = @(),
    [string[]]$SkipLayer = @(),
    [string]$SkipJustification = "",
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix,
    [switch]$EnablePythonCoverage,
    [switch]$EnableCppCoverage
)

$ErrorActionPreference = "Stop"

function Resolve-OutputPath {
    param([string]$PathValue)

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $PathValue))
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
            return @{ GateDecision = "blocking"; DefaultFailPolicy = "auto"; TimeoutBudgetSeconds = 0; RetryBudget = 0; FailFastCaseLimit = 0 }
        }
    }
}

if ($SkipLayer.Count -gt 0 -and [string]::IsNullOrWhiteSpace($SkipJustification)) {
    throw "SkipJustification is required when SkipLayer is not empty."
}

$resolvedReportDir = Resolve-OutputPath -PathValue $ReportDir
$lanePolicy = Resolve-LanePolicy -LaneId $Lane
Write-Output (
    "ci lane policy: lane={0} gate_decision={1} fail_policy={2} timeout_budget_seconds={3} retry_budget={4} fail_fast_case_limit={5}" -f
    $Lane,
    $lanePolicy.GateDecision,
    $lanePolicy.DefaultFailPolicy,
    $lanePolicy.TimeoutBudgetSeconds,
    $lanePolicy.RetryBudget,
    $lanePolicy.FailFastCaseLimit
)

$gateRunner = Join-Path $PSScriptRoot "scripts\validation\invoke-gate.ps1"
if (-not (Test-Path -LiteralPath $gateRunner)) {
    throw "Gate orchestrator not found: $gateRunner"
}

$legacyExitContractRunner = Join-Path $PSScriptRoot "scripts\migration\legacy-exit-checks.py"
if (-not (Test-Path -LiteralPath $legacyExitContractRunner)) {
    throw "legacy-exit contract runner not found: $legacyExitContractRunner"
}
Write-Output "ci orchestrator: legacy-exit-checks.py is executed by the full-offline legacy-exit gate step."
Write-Output "ci orchestrator: dsp-e2e-spec-docset is required by the full-offline gate artifact contract."

$gateArgs = @{
    Gate = "full-offline"
    ReportRoot = $resolvedReportDir
    Suite = $Suite
    Lane = $Lane
    RiskProfile = $RiskProfile
    DesiredDepth = $DesiredDepth
    ChangedScope = $ChangedScope
    SkipLayer = $SkipLayer
    SkipJustification = $SkipJustification
}
if ($IncludeHardwareSmoke) {
    $gateArgs["IncludeHardwareSmoke"] = $true
}
if ($IncludeHilClosedLoop) {
    $gateArgs["IncludeHilClosedLoop"] = $true
}
if ($IncludeHilCaseMatrix) {
    $gateArgs["IncludeHilCaseMatrix"] = $true
}
if ($EnablePythonCoverage) {
    $gateArgs["EnablePythonCoverage"] = $true
}
if ($EnableCppCoverage) {
    $gateArgs["EnableCppCoverage"] = $true
}

Write-Output "ci orchestrator: powershell -NoProfile -ExecutionPolicy Bypass -File $gateRunner -Gate full-offline -ReportRoot $resolvedReportDir"
& $gateRunner @gateArgs
