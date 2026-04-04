[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ModulesRoot,

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

    [ValidateSet("model-and-local", "local-only")]
    [string]$SchemaEnforcementMode = "local-only",

    [ValidateRange(1, 12)]
    [int]$MaxParallel = 3,

    [string]$Profile = "codexmanager",

    [switch]$ForceRemediationPlan,

    [string[]]$ModuleFilter,

    [string[]]$ExcludeModules,

    [switch]$StopOnModuleFailure
)

$ErrorActionPreference = "Stop"
$commonPath = Join-Path $PSScriptRoot "common.ps1"
. $commonPath

$modulesRootResolved = ConvertTo-AbsolutePath -Path $ModulesRoot
$repoRootResolved = ConvertTo-AbsolutePath -Path $RepoRoot
$promptsRootResolved = ConvertTo-AbsolutePath -Path $PromptsRoot
$schemasRootResolved = ConvertTo-AbsolutePath -Path $SchemasRoot
$artifactsRootResolved = ConvertTo-AbsolutePath -Path $ArtifactsRoot
$runSummaryPath = Get-RunSummaryPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId

$moduleDirectories = Get-ChildItem -LiteralPath $modulesRootResolved -Directory | Sort-Object Name
if ($ModuleFilter -and $ModuleFilter.Count -gt 0) {
    $moduleDirectories = @($moduleDirectories | Where-Object { $ModuleFilter -contains $_.Name })
}
if ($ExcludeModules -and $ExcludeModules.Count -gt 0) {
    $moduleDirectories = @($moduleDirectories | Where-Object { $ExcludeModules -notcontains $_.Name })
}

$moduleResults = [System.Collections.Generic.List[object]]::new()
$firstFailureCode = Get-AuditExitCode -Name "success"

foreach ($moduleDirectory in $moduleDirectories) {
    $moduleName = $moduleDirectory.Name
    $modulePath = $moduleDirectory.FullName

    & (Join-Path $PSScriptRoot "run-module-audit.ps1") `
        -ModuleName $moduleName `
        -ModulePath $modulePath `
        -RunId $RunId `
        -CommitSha $CommitSha `
        -RepoRoot $repoRootResolved `
        -PromptsRoot $promptsRootResolved `
        -SchemasRoot $schemasRootResolved `
        -ArtifactsRoot $artifactsRootResolved `
        -SchemaEnforcementMode $SchemaEnforcementMode `
        -MaxParallel $MaxParallel `
        -Profile $Profile
    $auditExitCode = $LASTEXITCODE

    if ($auditExitCode -eq (Get-AuditExitCode -Name "success")) {
        & (Join-Path $PSScriptRoot "aggregate-module-audit.ps1") `
            -ModuleName $moduleName `
            -RunId $RunId `
            -CommitSha $CommitSha `
            -ArtifactsRoot $artifactsRootResolved `
            -SchemasRoot $schemasRootResolved
        $aggregateExitCode = $LASTEXITCODE
    } else {
        $aggregateExitCode = $auditExitCode
    }

    $plannerExitCode = $null
    $indexPath = Get-ModuleAuditIndexPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $moduleName
    $plannerReady = $false
    if (Test-Path -LiteralPath $indexPath) {
        try {
            $index = Read-JsonFile -Path $indexPath
            $plannerReady = [bool]$index.planner_ready
        } catch {
            $plannerReady = $false
        }
    }

    if ($aggregateExitCode -eq (Get-AuditExitCode -Name "success") -and ($plannerReady -or $ForceRemediationPlan)) {
        & (Join-Path $PSScriptRoot "plan-module-remediation.ps1") `
            -ModuleName $moduleName `
            -RunId $RunId `
            -CommitSha $CommitSha `
            -RepoRoot $repoRootResolved `
            -PromptsRoot $promptsRootResolved `
            -SchemasRoot $schemasRootResolved `
            -ArtifactsRoot $artifactsRootResolved `
            -SchemaEnforcementMode $SchemaEnforcementMode `
            -Profile $Profile `
            -RequirePlannerReady:(-not $ForceRemediationPlan)
        $plannerExitCode = $LASTEXITCODE
    }

    $moduleSuccess = $auditExitCode -eq 0 -and $aggregateExitCode -eq 0 -and ($null -eq $plannerExitCode -or $plannerExitCode -eq 0)
    if (-not $moduleSuccess -and $firstFailureCode -eq 0) {
        if ($plannerExitCode -and $plannerExitCode -ne 0) {
            $firstFailureCode = $plannerExitCode
        } elseif ($aggregateExitCode -ne 0) {
            $firstFailureCode = $aggregateExitCode
        } else {
            $firstFailureCode = $auditExitCode
        }
    }

    $moduleResults.Add([ordered]@{
        module_name         = $moduleName
        audit_exit_code     = $auditExitCode
        aggregate_exit_code = $aggregateExitCode
        planner_exit_code   = $plannerExitCode
        planner_ready       = $plannerReady
        status              = if ($moduleSuccess) { "completed" } else { "failed" }
    })

    if (-not $moduleSuccess -and $StopOnModuleFailure) {
        break
    }
}

$completedCount = @($moduleResults | Where-Object { $_.status -eq "completed" }).Count
$failedCount = @($moduleResults | Where-Object { $_.status -eq "failed" }).Count
$plannerReadyCount = @($moduleResults | Where-Object { $_.planner_ready -eq $true }).Count

$runSummary = [ordered]@{
    run_id                    = $RunId
    commit_sha                = $CommitSha
    module_count              = $moduleResults.Count
    completed_module_count    = $completedCount
    failed_module_count       = $failedCount
    planner_ready_module_count = $plannerReadyCount
    module_results            = @($moduleResults)
}

try {
    Write-JsonFile -Path $runSummaryPath -Data $runSummary
} catch {
    Write-Error $_.Exception.Message
    exit (Get-AuditExitCode -Name "run_summary_generation_failed")
}

exit $firstFailureCode
