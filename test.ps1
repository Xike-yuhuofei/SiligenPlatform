param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "contracts", "protocol-compatibility", "integration", "e2e", "performance")]
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
    [switch]$FailOnKnownFailure,
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix,
    [switch]$EnablePythonCoverage,
    [switch]$EnableCppCoverage
)

$ErrorActionPreference = "Stop"

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

$requestedLaneForPolicy = if ($Lane -eq "auto") { "auto" } else { $Lane }
$lanePolicy = Resolve-LanePolicy -LaneId $requestedLaneForPolicy
Write-Output (
    "test lane policy: requested_lane={0} gate_decision={1} fail_policy={2} timeout_budget_seconds={3} retry_budget={4} fail_fast_case_limit={5}" -f
    $requestedLaneForPolicy,
    $lanePolicy.GateDecision,
    $lanePolicy.DefaultFailPolicy,
    $lanePolicy.TimeoutBudgetSeconds,
    $lanePolicy.RetryBudget,
    $lanePolicy.FailFastCaseLimit
)

function Resolve-RootRunner {
    param(
        [string]$CanonicalRelativePath,
        [string]$EntryName
    )

    $canonicalPath = Join-Path $PSScriptRoot $CanonicalRelativePath
    if (Test-Path $canonicalPath) {
        return $canonicalPath
    }

    throw "未找到根级 $EntryName 入口。已检查: $canonicalPath"
}

$workspaceRoot = $PSScriptRoot
$cppCoverageRawDir = Join-Path $workspaceRoot "tests\reports\coverage\cpp\raw"
$previousLlvmProfileFile = $env:LLVM_PROFILE_FILE
$previousCppCoverageRawDir = $env:SILIGEN_CPP_COVERAGE_RAW_DIR
if ($EnableCppCoverage) {
    New-Item -ItemType Directory -Force -Path $cppCoverageRawDir | Out-Null
    $env:SILIGEN_CPP_COVERAGE_RAW_DIR = $cppCoverageRawDir
    $env:LLVM_PROFILE_FILE = (Join-Path $cppCoverageRawDir "%4m-%p.profraw")
}

$runner = Resolve-RootRunner `
    -CanonicalRelativePath "scripts\\validation\\invoke-workspace-tests.ps1" `
    -EntryName "test"

try {
    & $runner `
        -Profile $Profile `
        -Suite $Suite `
        -ReportDir $ReportDir `
        -Lane $Lane `
        -RiskProfile $RiskProfile `
        -DesiredDepth $DesiredDepth `
        -ChangedScope $ChangedScope `
        -SkipLayer $SkipLayer `
        -SkipJustification $SkipJustification `
        -FailOnKnownFailure:$FailOnKnownFailure `
        -IncludeHardwareSmoke:$IncludeHardwareSmoke `
        -IncludeHilClosedLoop:$IncludeHilClosedLoop `
        -IncludeHilCaseMatrix:$IncludeHilCaseMatrix

    if ($EnablePythonCoverage) {
        $pythonCoverageRunner = Resolve-RootRunner `
            -CanonicalRelativePath "scripts\\validation\\invoke-python-coverage.ps1" `
            -EntryName "python coverage"
        & $pythonCoverageRunner
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }

    if ($EnableCppCoverage) {
        $cppCoverageRunner = Resolve-RootRunner `
            -CanonicalRelativePath "scripts\\validation\\invoke-cpp-coverage.ps1" `
            -EntryName "cpp coverage"
        & $cppCoverageRunner -RawProfileDir $cppCoverageRawDir
        if ($LASTEXITCODE -ne 0) {
            exit $LASTEXITCODE
        }
    }
}
finally {
    if ($EnableCppCoverage) {
        if ([string]::IsNullOrWhiteSpace($previousLlvmProfileFile)) {
            Remove-Item Env:LLVM_PROFILE_FILE -ErrorAction SilentlyContinue
        }
        else {
            $env:LLVM_PROFILE_FILE = $previousLlvmProfileFile
        }

        if ([string]::IsNullOrWhiteSpace($previousCppCoverageRawDir)) {
            Remove-Item Env:SILIGEN_CPP_COVERAGE_RAW_DIR -ErrorAction SilentlyContinue
        }
        else {
            $env:SILIGEN_CPP_COVERAGE_RAW_DIR = $previousCppCoverageRawDir
        }
    }
}
