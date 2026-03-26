param(
    [ValidateSet("all", "apps", "contracts", "e2e", "protocol-compatibility", "performance")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "tests\\reports\\ci",
    [switch]$IncludeHardwareSmoke
)

$ErrorActionPreference = "Stop"

function Resolve-OutputPath {
    param([string]$PathValue)

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot $PathValue))
}

$resolvedReportDir = Resolve-OutputPath -PathValue $ReportDir
$localGateDir = Join-Path $resolvedReportDir "local-validation-gate"

$requireHmiFormalGatewayContract = ($Suite -contains "all") -or ($Suite -contains "apps") -or ($Suite -contains "contracts") -or ($Suite -contains "e2e")
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

& (Join-Path $PSScriptRoot "build.ps1") -Profile CI -Suite $Suite
& (Join-Path $PSScriptRoot "test.ps1") `
    -Profile CI `
    -Suite $Suite `
    -ReportDir $resolvedReportDir `
    -FailOnKnownFailure `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke

& (Join-Path $PSScriptRoot "scripts\\validation\\run-local-validation-gate.ps1") `
    -ReportRoot $localGateDir

if (-not (Test-Path (Join-Path $resolvedReportDir "workspace-validation.md"))) {
    throw "CI 报告缺失 workspace-validation.md: $resolvedReportDir"
}
if (-not (Test-Path (Join-Path $resolvedReportDir "dsp-e2e-spec-docset"))) {
    throw "CI 报告缺失 dsp-e2e-spec-docset 目录: $resolvedReportDir"
}
if (-not (Get-ChildItem $localGateDir -Directory -ErrorAction SilentlyContinue | Select-Object -First 1)) {
    throw "CI 报告缺失 local-validation-gate 运行目录: $localGateDir"
}
