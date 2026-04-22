param(
    [ValidateSet("Wave1", "All")]
    [string]$Wave = "Wave1",
    [string]$ReportDir = "tests\\reports\\first-layer-rereview",
    [string]$PythonExe = "python",
    [switch]$AllowSkipOnMissingGateway
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$PSNativeCommandUseErrorActionPreference = $false

$ExitSuccess = 0
$ExitFailed = 1
$ExitKnownFailure = 10
$ExitSkipped = 11

function Resolve-TaskStatus {
    param(
        [int]$ExitCode
    )

    if ($ExitCode -eq $ExitSuccess) {
        return "passed"
    }
    if ($ExitCode -eq $ExitSkipped) {
        return "skipped"
    }
    if ($ExitCode -eq $ExitKnownFailure) {
        return "known_failure"
    }
    return "failed"
}

function Get-OverallStatus {
    param(
        [array]$TaskResults
    )

    $statuses = @($TaskResults | ForEach-Object { $_.status })
    if ($statuses -contains "failed") {
        return "failed"
    }
    if ($statuses -contains "known_failure") {
        return "known_failure"
    }
    if ($statuses.Count -gt 0 -and @($statuses | Where-Object { $_ -ne "skipped" }).Count -eq 0) {
        return "skipped"
    }
    return "passed"
}

function Get-OverallExitCode {
    param(
        [string]$OverallStatus
    )

    if ($OverallStatus -eq "passed") {
        return $ExitSuccess
    }
    if ($OverallStatus -eq "known_failure") {
        return $ExitKnownFailure
    }
    if ($OverallStatus -eq "skipped") {
        return $ExitSkipped
    }
    return $ExitFailed
}

$workspaceRoot = Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent
$resolvedReportRoot = Join-Path $workspaceRoot $ReportDir
$runStamp = (Get-Date).ToUniversalTime().ToString("yyyyMMddTHHmmssZ")
$runReportDir = Join-Path $resolvedReportRoot $runStamp
New-Item -ItemType Directory -Path $runReportDir -Force | Out-Null

$preconditionScript = Join-Path $PSScriptRoot "run_tcp_precondition_matrix.py"
$estopScript = Join-Path $PSScriptRoot "run_tcp_estop_chain.py"
$doorScript = Join-Path $PSScriptRoot "run_tcp_door_interlock.py"
$negativeLimitScript = Join-Path $PSScriptRoot "run_tcp_negative_limit_chain.py"

if (-not (Test-Path $preconditionScript)) {
    throw "missing script: $preconditionScript"
}
if (-not (Test-Path $estopScript)) {
    throw "missing script: $estopScript"
}
if (-not (Test-Path $doorScript)) {
    throw "missing script: $doorScript"
}
if (-not (Test-Path $negativeLimitScript)) {
    throw "missing script: $negativeLimitScript"
}

$tasks = @(
    [pscustomobject]@{
        name = "s2-s4-precondition-matrix"
        script = $preconditionScript
        args = @(
            "--report-dir", (Join-Path $runReportDir "s2-s4-precondition")
        )
    },
    [pscustomobject]@{
        name = "s8-estop-chain"
        script = $estopScript
        args = @(
            "--report-dir", (Join-Path $runReportDir "s8-estop-chain")
        )
    },
    [pscustomobject]@{
        name = "s5-door-interlock"
        script = $doorScript
        args = @(
            "--report-dir", (Join-Path $runReportDir "s5-door-interlock")
        )
    },
    [pscustomobject]@{
        name = "s6-negative-limit-chain"
        script = $negativeLimitScript
        args = @(
            "--report-dir", (Join-Path $runReportDir "s6-negative-limit-chain")
        )
    }
)

if ($AllowSkipOnMissingGateway) {
    foreach ($task in $tasks) {
        $task.args += "--allow-skip-on-missing-gateway"
    }
}

$taskResults = @()
foreach ($task in $tasks) {
    Write-Host "first-layer rereview: running $($task.name)"
    & $PythonExe $task.script @($task.args)
    $taskExitCode = $LASTEXITCODE
    $taskStatus = Resolve-TaskStatus -ExitCode $taskExitCode

    $taskResults += [pscustomobject]@{
        name = $task.name
        status = $taskStatus
        exit_code = $taskExitCode
        script = $task.script
        args = @($task.args)
    }

    Write-Host "first-layer rereview: $($task.name) status=$taskStatus exit_code=$taskExitCode"
}

$overallStatus = Get-OverallStatus -TaskResults $taskResults
$overallExitCode = Get-OverallExitCode -OverallStatus $overallStatus

$summary = [ordered]@{
    generated_at = (Get-Date).ToUniversalTime().ToString("o")
    workspace_root = $workspaceRoot
    wave = $Wave
    report_dir = $runReportDir
    overall_status = $overallStatus
    task_results = $taskResults
}

$summaryJsonPath = Join-Path $runReportDir "first-layer-rereview-summary.json"
$summaryMdPath = Join-Path $runReportDir "first-layer-rereview-summary.md"
$summary | ConvertTo-Json -Depth 8 | Set-Content -Path $summaryJsonPath -Encoding UTF8

$mdLines = @(
    "# First-Layer Rereview Summary",
    "",
    "- generated_at: ``$($summary.generated_at)``",
    "- wave: ``$Wave``",
    "- overall_status: ``$overallStatus``",
    "- report_dir: ``$runReportDir``",
    "",
    "## Task Results",
    ""
)
foreach ($taskResult in $taskResults) {
    $mdLines += "- ``$($taskResult.status)`` ``$($taskResult.name)`` exit_code=``$($taskResult.exit_code)``"
    $joinedArgs = [string]::Join(" ", @($taskResult.args))
    $mdLines += "  command: ``$PythonExe $($taskResult.script) $joinedArgs``"
}
$mdLines += ""
$mdLines | Set-Content -Path $summaryMdPath -Encoding UTF8

Write-Host "first-layer rereview complete: overall_status=$overallStatus"
Write-Host "summary json: $summaryJsonPath"
Write-Host "summary md: $summaryMdPath"
exit $overallExitCode
