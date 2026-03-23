[CmdletBinding()]
param(
    [string]$PythonExe = "python",
    [string]$ReportRoot = "integration/reports/local-validation-gate"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
Set-Location $repoRoot

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

New-Item -ItemType Directory -Force -Path $logsDir | Out-Null
New-Item -ItemType Directory -Force -Path $workspaceValidationDir | Out-Null

$steps = @(
    @{
        Id      = "workspace-layout"
        Name    = "Validate workspace layout"
        LogName = "01-workspace-layout.log"
        Command = @($PythonExe, ".\tools\migration\validate_workspace_layout.py")
    },
    @{
        Id      = "test-packages-ci"
        Name    = "Run packages suite tests (CI profile)"
        LogName = "02-test-packages-ci.log"
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
            "packages",
            "-ReportDir",
            $workspaceValidationDir,
            "-FailOnKnownFailure"
        )
    },
    @{
        Id      = "build-validation-local-packages"
        Name    = "Build validation (Local/packages)"
        LogName = "03-build-validation-local-packages.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\tools\build\build-validation.ps1",
            "-Profile",
            "Local",
            "-Suite",
            "packages"
        )
    },
    @{
        Id      = "build-validation-ci-packages"
        Name    = "Build validation (CI/packages)"
        LogName = "04-build-validation-ci-packages.log"
        Command = @(
            "powershell",
            "-NoProfile",
            "-ExecutionPolicy",
            "Bypass",
            "-File",
            ".\tools\build\build-validation.ps1",
            "-Profile",
            "CI",
            "-Suite",
            "packages"
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

Write-Host "Summary(JSON): $jsonPath"
Write-Host "Summary(MD):   $mdPath"
Write-Host "Overall:       $overallStatus"

if ($overallStatus -ne "passed") {
    exit 1
}

exit 0
