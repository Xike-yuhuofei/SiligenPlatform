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
    [switch]$IncludeHilCaseMatrix
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
            throw "Unsupported lane policy request: $LaneId"
        }
    }
}

function Invoke-LaneStep {
    param(
        [string]$StepName,
        [hashtable]$LanePolicy,
        [scriptblock]$Action
    )

    & $Action
    $stepSucceeded = $?
    $exitCode = $LASTEXITCODE
    if ($null -eq $exitCode) {
        $exitCode = 0
    }
    if (-not $stepSucceeded -and $exitCode -eq 0) {
        $exitCode = 1
    }
    if ($exitCode -eq 0) {
        return
    }

    if ($LanePolicy.GateDecision -eq "advisory") {
        Write-Warning "$StepName failed under advisory lane policy (exit=$exitCode); continue and rely on published evidence."
        $global:LASTEXITCODE = 0
        return
    }

    exit $exitCode
}

if ($SkipLayer.Count -gt 0 -and [string]::IsNullOrWhiteSpace($SkipJustification)) {
    throw "SkipJustification is required when SkipLayer is not empty."
}

$resolvedReportDir = Resolve-OutputPath -PathValue $ReportDir
$localGateDir = Join-Path $resolvedReportDir "local-validation-gate"
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

$requireHmiFormalGatewayContract = ($Suite -contains "all") -or ($Suite -contains "apps") -or ($Suite -contains "contracts") -or ($Suite -contains "integration") -or ($Suite -contains "e2e")
if ($requireHmiFormalGatewayContract) {
    $formalGatewayContractGuard = Join-Path $PSScriptRoot "scripts\\validation\\assert-hmi-formal-gateway-launch-contract.ps1"
    if (-not (Test-Path $formalGatewayContractGuard)) {
        throw "HMI formal gateway contract guard not found: $formalGatewayContractGuard"
    }

    Write-Output "hmi formal gateway contract gate: powershell -NoProfile -ExecutionPolicy Bypass -File $formalGatewayContractGuard"
    & $formalGatewayContractGuard -WorkspaceRoot $PSScriptRoot
}

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

$legacyExitRunner = Resolve-RootRunner `
    -CanonicalRelativePath "scripts\\migration\\legacy-exit-checks.py" `
    -EntryName "legacy-exit"

$legacyExitReportDir = Join-Path $resolvedReportDir "legacy-exit"
$resolvedLegacyExitReportDir = $legacyExitReportDir
New-Item -ItemType Directory -Force -Path $resolvedLegacyExitReportDir | Out-Null
python $legacyExitRunner `
    --profile "ci" `
    --report-dir $resolvedLegacyExitReportDir
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Invoke-LaneStep -StepName "build.ps1" -LanePolicy $lanePolicy -Action {
    & (Join-Path $PSScriptRoot "build.ps1") `
        -Profile CI `
        -Suite $Suite `
        -Lane $Lane `
        -RiskProfile $RiskProfile `
        -DesiredDepth $DesiredDepth `
        -ChangedScope $ChangedScope `
        -SkipLayer $SkipLayer `
        -SkipJustification $SkipJustification
}
Invoke-LaneStep -StepName "test.ps1" -LanePolicy $lanePolicy -Action {
    & (Join-Path $PSScriptRoot "test.ps1") `
        -Profile CI `
        -Suite $Suite `
        -ReportDir $resolvedReportDir `
        -Lane $Lane `
        -RiskProfile $RiskProfile `
        -DesiredDepth $DesiredDepth `
        -ChangedScope $ChangedScope `
        -SkipLayer $SkipLayer `
        -SkipJustification $SkipJustification `
        -FailOnKnownFailure `
        -IncludeHardwareSmoke:$IncludeHardwareSmoke `
        -IncludeHilClosedLoop:$IncludeHilClosedLoop `
        -IncludeHilCaseMatrix:$IncludeHilCaseMatrix
}

Invoke-LaneStep -StepName "run-local-validation-gate.ps1" -LanePolicy $lanePolicy -Action {
    $gateScript = Join-Path $PSScriptRoot "scripts\\validation\\run-local-validation-gate.ps1"
    $gateArgs = @(
        "-NoProfile",
        "-ExecutionPolicy",
        "Bypass",
        "-File",
        $gateScript,
        "-ReportRoot",
        $localGateDir,
        "-Lane",
        $Lane,
        "-RiskProfile",
        $RiskProfile,
        "-DesiredDepth",
        $DesiredDepth
    )

    foreach ($scopeName in $ChangedScope) {
        if (-not [string]::IsNullOrWhiteSpace($scopeName)) {
            $gateArgs += @("-ChangedScope", $scopeName)
        }
    }
    foreach ($layerName in $SkipLayer) {
        if (-not [string]::IsNullOrWhiteSpace($layerName)) {
            $gateArgs += @("-SkipLayer", $layerName)
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($SkipJustification)) {
        $gateArgs += @("-SkipJustification", $SkipJustification)
    }
    if ($IncludeHilClosedLoop) {
        $gateArgs += "-IncludeHilClosedLoop"
    }
    if ($IncludeHilCaseMatrix) {
        $gateArgs += "-IncludeHilCaseMatrix"
    }

    & powershell @gateArgs
}

if (-not (Test-Path (Join-Path $resolvedReportDir "workspace-validation.md"))) {
    throw "CI 报告缺失 workspace-validation.md: $resolvedReportDir"
}
if (-not (Test-Path (Join-Path $resolvedReportDir "dsp-e2e-spec-docset"))) {
    throw "CI 报告缺失 dsp-e2e-spec-docset 目录: $resolvedReportDir"
}
if (-not (Get-ChildItem $localGateDir -Directory -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw "CI 报告缺失 local-validation-gate 运行目录: $localGateDir"
}
