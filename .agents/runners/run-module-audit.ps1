[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ModuleName,

    [Parameter(Mandatory)]
    [string]$ModulePath,

    [Parameter(Mandatory)]
    [string]$RunId,

    [Parameter(Mandatory)]
    [string]$CommitSha,

    [Parameter(Mandatory)]
    [string]$RepoRoot,

    [Parameter(Mandatory)]
    [string]$PromptsRoot,

    [Parameter(Mandatory)]
    [string]$SchemasRoot,

    [Parameter(Mandatory)]
    [string]$ArtifactsRoot,

    [string[]]$PromptNames,

    [ValidateSet("model-and-local", "local-only")]
    [string]$SchemaEnforcementMode = "local-only",

    [ValidateRange(1, 12)]
    [int]$MaxParallel = 3,

    [string]$Profile = "codexmanager",

    [string]$Model,

    [string]$Reasoning,

    [switch]$StopOnFailure
)

$ErrorActionPreference = "Stop"
$commonPath = Join-Path $PSScriptRoot "common.ps1"
. $commonPath

$repoRootResolved = ConvertTo-AbsolutePath -Path $RepoRoot
$modulePathResolved = ConvertTo-AbsolutePath -Path $ModulePath
$promptsRootResolved = ConvertTo-AbsolutePath -Path $PromptsRoot
$schemasRootResolved = ConvertTo-AbsolutePath -Path $SchemasRoot
$artifactsRootResolved = ConvertTo-AbsolutePath -Path $ArtifactsRoot
$reportSchemaPath = Join-Path $schemasRootResolved "report.schema.json"
$singleAuditScriptPath = Join-Path $PSScriptRoot "invoke-single-module-audit.ps1"
$pwshPath = (Get-Command pwsh -CommandType Application).Source

function Write-AuditStatus {
    param(
        [Parameter(Mandatory)]
        [string]$Status,

        [Parameter(Mandatory)]
        [string]$PromptName,

        [int]$ExitCode
    )

    $label = switch ($Status) {
        "start" { "[start]" }
        "done"  { "[done ]" }
        "fail"  { "[fail ]" }
        default { "[info ]" }
    }

    $color = switch ($Status) {
        "start" { "Cyan" }
        "done"  { "Green" }
        "fail"  { "Red" }
        default { "Gray" }
    }

    $message = if ($PSBoundParameters.ContainsKey("ExitCode")) {
        "{0} {1} (exit={2})" -f $label, $PromptName, $ExitCode
    } else {
        "{0} {1}" -f $label, $PromptName
    }

    Write-Host $message -ForegroundColor $color
}

$firstFailureCode = Get-AuditExitCode -Name "success"

$promptSelection = Resolve-AuditPromptSelection -PromptNames $PromptNames
if ($promptSelection.InvalidPromptNames.Count -gt 0) {
    Write-Error "Unknown audit prompts: $($promptSelection.InvalidPromptNames -join ', ')"
    exit (Get-AuditExitCode -Name "invalid_prompt_selection")
}

$auditsToRun = @($promptSelection.Audits)
if ($promptSelection.SelectionSpecified -and $auditsToRun.Count -eq 0) {
    Write-Error "No audit prompts selected after filtering."
    exit (Get-AuditExitCode -Name "invalid_prompt_selection")
}

Write-Host ""
Write-Host "=== Module Audit Runner ===" -ForegroundColor Cyan
Write-Host ("  {0,-14}: {1}" -f "Module", $ModuleName)
Write-Host ("  {0,-14}: {1}" -f "AuditCount", $auditsToRun.Count)
Write-Host ("  {0,-14}: {1}" -f "MaxParallel", $MaxParallel)
Write-Host ("  {0,-14}: {1}" -f "WorkerLogRoot", (Join-Path $artifactsRootResolved (Join-Path $RunId (Join-Path $ModuleName "_worker-logs"))))

if ($MaxParallel -le 1 -or $auditsToRun.Count -le 1) {
    foreach ($audit in $auditsToRun) {
        Write-AuditStatus -Status "start" -PromptName $audit.PromptName
        & $singleAuditScriptPath `
            -ModuleName $ModuleName `
            -ModulePath $modulePathResolved `
            -RunId $RunId `
            -CommitSha $CommitSha `
            -RepoRoot $repoRootResolved `
            -PromptsRoot $promptsRootResolved `
            -SchemasRoot $schemasRootResolved `
            -ArtifactsRoot $artifactsRootResolved `
            -PromptName $audit.PromptName `
            -SchemaEnforcementMode $SchemaEnforcementMode `
            -Profile $Profile `
            -Model $Model `
            -Reasoning $Reasoning
        $auditExitCode = $LASTEXITCODE
        if ($auditExitCode -eq (Get-AuditExitCode -Name "success")) {
            Write-AuditStatus -Status "done" -PromptName $audit.PromptName
        } else {
            Write-AuditStatus -Status "fail" -PromptName $audit.PromptName -ExitCode $auditExitCode
        }

        if ($auditExitCode -ne (Get-AuditExitCode -Name "success")) {
            if ($firstFailureCode -eq (Get-AuditExitCode -Name "success")) {
                $firstFailureCode = $auditExitCode
            }
            if ($StopOnFailure) {
                exit $firstFailureCode
            }
        }
    }

    exit $firstFailureCode
}

$workerLogRoot = Join-Path $artifactsRootResolved (Join-Path $RunId (Join-Path $ModuleName "_worker-logs"))
New-Item -ItemType Directory -Force -Path $workerLogRoot | Out-Null

$pendingAudits = [System.Collections.Generic.Queue[object]]::new()
foreach ($audit in $auditsToRun) {
    $pendingAudits.Enqueue($audit)
}

$runningWorkers = @()
$stopDispatch = $false

while ($pendingAudits.Count -gt 0 -or $runningWorkers.Count -gt 0) {
    while (-not $stopDispatch -and $pendingAudits.Count -gt 0 -and $runningWorkers.Count -lt $MaxParallel) {
        $audit = $pendingAudits.Dequeue()
        $stdoutPath = Join-Path $workerLogRoot ("{0}.stdout.log" -f $audit.PromptName)
        $stderrPath = Join-Path $workerLogRoot ("{0}.stderr.log" -f $audit.PromptName)
        Write-AuditStatus -Status "start" -PromptName $audit.PromptName

        $argumentList = @(
            "-NoProfile",
            "-File", $singleAuditScriptPath,
            "-ModuleName", $ModuleName,
            "-ModulePath", $modulePathResolved,
            "-RunId", $RunId,
            "-CommitSha", $CommitSha,
            "-RepoRoot", $repoRootResolved,
            "-PromptsRoot", $promptsRootResolved,
            "-SchemasRoot", $schemasRootResolved,
            "-ArtifactsRoot", $artifactsRootResolved,
            "-PromptName", $audit.PromptName,
            "-SchemaEnforcementMode", $SchemaEnforcementMode,
            "-Profile", $Profile
        )

        if (-not [string]::IsNullOrWhiteSpace($Model)) {
            $argumentList += @("-Model", $Model)
        }

        if (-not [string]::IsNullOrWhiteSpace($Reasoning)) {
            $argumentList += @("-Reasoning", $Reasoning)
        }

        $process = Start-Process `
            -FilePath $pwshPath `
            -ArgumentList $argumentList `
            -WorkingDirectory $repoRootResolved `
            -RedirectStandardOutput $stdoutPath `
            -RedirectStandardError $stderrPath `
            -WindowStyle Hidden `
            -PassThru

        $runningWorkers += [PSCustomObject]@{
            PromptName  = $audit.PromptName
            Process     = $process
            StdoutPath  = $stdoutPath
            StderrPath  = $stderrPath
        }
    }

    if ($runningWorkers.Count -eq 0) {
        continue
    }

    $completedWorkers = @($runningWorkers | Where-Object { $_.Process.HasExited })
    if ($completedWorkers.Count -eq 0) {
        Start-Sleep -Milliseconds 250
        continue
    }

    $nextRunningWorkers = @()
    foreach ($worker in $runningWorkers) {
        if (-not $worker.Process.HasExited) {
            $nextRunningWorkers += $worker
            continue
        }

        $auditExitCode = $worker.Process.ExitCode
        if ($auditExitCode -ne (Get-AuditExitCode -Name "success")) {
            Write-AuditStatus -Status "fail" -PromptName $worker.PromptName -ExitCode $auditExitCode
            if ($firstFailureCode -eq (Get-AuditExitCode -Name "success")) {
                $firstFailureCode = $auditExitCode
            }

            $stderrText = if (Test-Path -LiteralPath $worker.StderrPath) {
                (@(Get-Content -LiteralPath $worker.StderrPath -Tail 40 -ErrorAction SilentlyContinue) -join [Environment]::NewLine).Trim()
            } else {
                ""
            }

            if ([string]::IsNullOrWhiteSpace($stderrText) -and (Test-Path -LiteralPath $worker.StdoutPath)) {
                $stderrText = (@(Get-Content -LiteralPath $worker.StdoutPath -Tail 40 -ErrorAction SilentlyContinue) -join [Environment]::NewLine).Trim()
            }

            if ([string]::IsNullOrWhiteSpace($stderrText)) {
                Write-Error "Audit worker '$($worker.PromptName)' failed with exit code $auditExitCode."
            } else {
                Write-Error "Audit worker '$($worker.PromptName)' failed with exit code $auditExitCode. See '$($worker.StderrPath)'.`n$stderrText"
            }

            if ($StopOnFailure) {
                $stopDispatch = $true
            }
        } else {
            Write-AuditStatus -Status "done" -PromptName $worker.PromptName
        }
    }

    $runningWorkers = @($nextRunningWorkers)

    if ($stopDispatch -and $runningWorkers.Count -gt 0) {
        foreach ($worker in $runningWorkers) {
            if (-not $worker.Process.HasExited) {
                Stop-Process -Id $worker.Process.Id -Force -ErrorAction SilentlyContinue
            }
        }
        $runningWorkers = @()
    }
}

Write-Host ""
Write-Host "=== Module Audit Runner Completed ===" -ForegroundColor Green
Write-Host ("  {0,-14}: {1}" -f "Module", $ModuleName)
Write-Host ("  {0,-14}: {1}" -f "FinalExitCode", $firstFailureCode)

exit $firstFailureCode
