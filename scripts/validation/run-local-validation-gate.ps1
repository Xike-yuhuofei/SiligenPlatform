[CmdletBinding()]
param(
    [string]$PythonExe = "python",
    [string]$ReportRoot = "tests/reports/local-validation-gate"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $repoRoot

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
        & $Step.Command[0] @($Step.Command[1..($Step.Command.Count - 1)]) *>&1 | Tee-Object -FilePath $logPath -Append | Out-Null
        $ErrorActionPreference = $prevErrorActionPreference
        if ($LASTEXITCODE -ne 0) {
            $exitCode = $LASTEXITCODE
        } elseif (-not $?) {
            $exitCode = 1
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
$dspE2ESpecDir = Join-Path $runDir "dsp-e2e-spec-docset"
$legacyExitDir = Join-Path $runDir "legacy-exit"
$moduleBoundaryDir = Join-Path $runDir "module-boundary-bridges"

New-Item -ItemType Directory -Force -Path $logsDir | Out-Null
New-Item -ItemType Directory -Force -Path $workspaceValidationDir | Out-Null
New-Item -ItemType Directory -Force -Path $dspE2ESpecDir | Out-Null
New-Item -ItemType Directory -Force -Path $legacyExitDir | Out-Null
New-Item -ItemType Directory -Force -Path $moduleBoundaryDir | Out-Null

$env:SILIGEN_FREEZE_DOCSET_REPORT_DIR = $dspE2ESpecDir
$env:SILIGEN_FREEZE_EVIDENCE_CASES = "success,block,rollback,recovery,archive"
$env:SILIGEN_LEGACY_EXIT_REPORT_DIR = $legacyExitDir

$steps = @(
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
        Id      = "test-contracts-ci"
        Name    = "Run contracts suite tests (CI profile)"
        LogName = "05-test-contracts-ci.log"
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
            "contracts",
            "-ReportDir",
            $workspaceValidationDir,
            "-FailOnKnownFailure"
        )
    },
    @{
        Id      = "build-validation-local-contracts"
        Name    = "Build validation (Local/contracts)"
        LogName = "06-build-validation-local-contracts.log"
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
        LogName = "07-build-validation-ci-contracts.log"
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
    report_root     = $resolvedRoot
    run_dir         = $runDir
    freeze_report_dir = $dspE2ESpecDir
    legacy_exit_dir = $legacyExitDir
    module_boundary_dir = $moduleBoundaryDir
    workspace_validation_dir = $workspaceValidationDir
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
    (Join-Path $moduleBoundaryDir "module-boundary-bridges.md")
)
foreach ($artifact in $requiredArtifacts) {
    if (-not (Test-Path $artifact)) {
        Write-Error "根级门禁未发布预期报告: $artifact"
    }
}

Write-Host "Summary(JSON): $jsonPath"
Write-Host "Summary(MD):   $mdPath"
Write-Host "Overall:       $overallStatus"

if ($overallStatus -ne "passed") {
    exit 1
}

exit 0
