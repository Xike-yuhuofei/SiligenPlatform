[CmdletBinding()]
param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "contracts", "e2e", "protocol-compatibility", "performance")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "tests\\reports",
    [ValidateSet("success", "block", "rollback", "recovery", "archive")]
    [string[]]$FreezeEvidenceCase = @("success", "block", "rollback", "recovery", "archive"),
    [string]$FreezeDocsetReportDir = "",
    [switch]$FailOnKnownFailure,
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$testKitSrc = Join-Path $workspaceRoot "shared\\testing\\test-kit\\src"

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

$runRuntimeOwnerAcceptanceGate = $requireHmiFormalGatewayContract
if ($runRuntimeOwnerAcceptanceGate) {
    $layoutValidator = Join-Path $workspaceRoot "scripts\\migration\\validate_workspace_layout.py"
    $runtimeWave = "Wave 7"
    if (-not (Test-Path $layoutValidator)) {
        throw "Workspace layout validator not found: $layoutValidator"
    }

    Write-Output "runtime owner/acceptance gate: python $layoutValidator --wave `"$runtimeWave`""
    python $layoutValidator --wave $runtimeWave
    if ($LASTEXITCODE -ne 0) {
        throw "Runtime owner/acceptance gate failed (wave: $runtimeWave, exit: $LASTEXITCODE)."
    }
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

if ($IncludeHardwareSmoke) {
    $argsList += "--include-hardware-smoke"
}

if ($IncludeHilClosedLoop) {
    $argsList += "--include-hil-closed-loop"
}

if ($FailOnKnownFailure) {
    $argsList += "--fail-on-known-failure"
}

python @argsList
$workspaceValidationExitCode = $LASTEXITCODE
if ($workspaceValidationExitCode -ne 0) {
    throw "workspace validation failed (exit: $workspaceValidationExitCode)."
}
