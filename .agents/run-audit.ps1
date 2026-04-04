[CmdletBinding(DefaultParameterSetName = "AllModules")]
param(
    [Parameter(ParameterSetName = "SingleModule", Mandatory)]
    [string]$ModulePath,

    [ValidateSet("local-only", "model-and-local")]
    [string]$SchemaMode = "local-only",

    [ValidateRange(1, 12)]
    [int]$MaxParallel = 3,

    [string]$Profile = "codexmanager",

    [string]$RunId,

    [string]$Model,

    [string]$Reasoning,

    [switch]$ForceRemediationPlan,

    [switch]$StopOnFailure,

    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$repoRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot ".."))
$runnersRoot = Join-Path $PSScriptRoot "runners"
$promptsRoot = Join-Path $PSScriptRoot "prompts\audit"
$schemasRoot = Join-Path $PSScriptRoot "schemas"
$artifactsRoot = Join-Path $repoRoot "artifacts\audits"
$modulesRoot = Join-Path $repoRoot "modules"

$commonPath = Join-Path $runnersRoot "common.ps1"
if (-not (Test-Path -LiteralPath $commonPath)) {
    throw "Missing runner common script: $commonPath"
}
. $commonPath

function Assert-PathExists {
    param(
        [Parameter(Mandatory)]
        [string]$Path,

        [Parameter(Mandatory)]
        [string]$Label
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        throw "$Label not found: $Path"
    }
}

function Get-HeadCommitSha {
    param(
        [Parameter(Mandatory)]
        [string]$RepoRoot
    )

    $commitSha = (& git -C $RepoRoot rev-parse HEAD 2>$null)
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($commitSha)) {
        throw "Failed to resolve git HEAD commit under '$RepoRoot'."
    }

    return $commitSha.Trim()
}

function Resolve-LauncherRunId {
    param(
        [string]$RequestedRunId
    )

    if (-not [string]::IsNullOrWhiteSpace($RequestedRunId)) {
        return $RequestedRunId.Trim()
    }

    return ((Get-Date).ToUniversalTime().ToString("yyyy-MM-ddTHHmmssZ") + "-audit")
}

function Resolve-SingleModuleTarget {
    param(
        [Parameter(Mandatory)]
        [string]$RequestedModulePath,

        [Parameter(Mandatory)]
        [string]$RepoRoot,

        [Parameter(Mandatory)]
        [string]$ModulesRoot
    )

    $candidatePath = if ([System.IO.Path]::IsPathRooted($RequestedModulePath)) {
        $RequestedModulePath
    } else {
        Join-Path $RepoRoot $RequestedModulePath
    }

    $resolvedPath = [System.IO.Path]::GetFullPath($candidatePath)
    if (-not (Test-Path -LiteralPath $resolvedPath -PathType Container)) {
        throw "Module path not found: $resolvedPath"
    }

    $modulesRootResolved = [System.IO.Path]::GetFullPath($ModulesRoot)
    $comparison = [System.StringComparison]::OrdinalIgnoreCase
    $modulesPrefix = $modulesRootResolved.TrimEnd('\') + '\'
    if (-not $resolvedPath.StartsWith($modulesPrefix, $comparison)) {
        throw "Module path must stay under '$modulesRootResolved'. Actual: $resolvedPath"
    }

    return [PSCustomObject]@{
        ModulePath = $resolvedPath
        ModuleName = Split-Path -Path $resolvedPath -Leaf
    }
}

function Write-LauncherSummary {
    param(
        [Parameter(Mandatory)]
        [hashtable]$Summary
    )

    Write-Host ""
    Write-Host "=== Audit Launcher ===" -ForegroundColor Cyan
    Write-Host ("  {0,-14}: {1}" -f "Scope", $Summary.Scope)
    Write-Host ("  {0,-14}: {1}" -f "RepoRoot", $Summary.RepoRoot)
    Write-Host ("  {0,-14}: {1}" -f "RunId", $Summary.RunId)
    Write-Host ("  {0,-14}: {1}" -f "Profile", $Summary.Profile)
    Write-Host ("  {0,-14}: {1}" -f "SchemaMode", $Summary.SchemaMode)
    Write-Host ("  {0,-14}: {1}" -f "MaxParallel", $Summary.MaxParallel)
    Write-Host ("  {0,-14}: {1}" -f "ForcePlan", $Summary.ForcePlan)
    Write-Host ("  {0,-14}: {1}" -f "ArtifactsRoot", $Summary.ArtifactsRoot)
    if ($Summary.ContainsKey("ModulePath")) {
        Write-Host ("  {0,-14}: {1}" -f "ModulePath", $Summary.ModulePath)
        Write-Host ("  {0,-14}: {1}" -f "ModuleName", $Summary.ModuleName)
    }
}

Assert-PathExists -Path $runnersRoot -Label "Runners root"
Assert-PathExists -Path $promptsRoot -Label "Audit prompts root"
Assert-PathExists -Path $schemasRoot -Label "Schemas root"
Assert-PathExists -Path $modulesRoot -Label "Modules root"

$null = Get-Command git -ErrorAction Stop
$null = Get-Command codex -ErrorAction Stop
$null = Get-Command pwsh -ErrorAction Stop

if (-not (Test-CodexProfileExists -Name $Profile)) {
    throw "Codex profile '$Profile' not found in $HOME\.codex\config.toml"
}

$commitSha = Get-HeadCommitSha -RepoRoot $repoRoot
$resolvedRunId = Resolve-LauncherRunId -RequestedRunId $RunId

$summary = [ordered]@{
    Scope         = if ($PSCmdlet.ParameterSetName -eq "SingleModule") { "single-module" } else { "all-modules" }
    RepoRoot      = $repoRoot
    RunId         = $resolvedRunId
    Profile       = $Profile
    SchemaMode    = $SchemaMode
    MaxParallel   = $MaxParallel
    ForcePlan     = $ForceRemediationPlan.IsPresent
    ArtifactsRoot = $artifactsRoot
}

$singleModuleTarget = $null
if ($PSCmdlet.ParameterSetName -eq "SingleModule") {
    $singleModuleTarget = Resolve-SingleModuleTarget -RequestedModulePath $ModulePath -RepoRoot $repoRoot -ModulesRoot $modulesRoot
    $summary["ModulePath"] = $singleModuleTarget.ModulePath
    $summary["ModuleName"] = $singleModuleTarget.ModuleName
}

Write-LauncherSummary -Summary $summary

if ($DryRun) {
    Write-Host ""
    Write-Host "[dry-run] No audit runner was started." -ForegroundColor Yellow
    exit 0
}

New-Item -ItemType Directory -Force -Path $artifactsRoot | Out-Null

if ($PSCmdlet.ParameterSetName -eq "SingleModule") {
    $runnerPath = Join-Path $runnersRoot "run-module-audit.ps1"
    & $runnerPath `
        -ModuleName $singleModuleTarget.ModuleName `
        -ModulePath $singleModuleTarget.ModulePath `
        -RunId $resolvedRunId `
        -CommitSha $commitSha `
        -RepoRoot $repoRoot `
        -PromptsRoot $promptsRoot `
        -SchemasRoot $schemasRoot `
        -ArtifactsRoot $artifactsRoot `
        -SchemaEnforcementMode $SchemaMode `
        -MaxParallel $MaxParallel `
        -Profile $Profile `
        -Model $Model `
        -Reasoning $Reasoning `
        -StopOnFailure:$StopOnFailure
    $runnerExitCode = $LASTEXITCODE

    if ($runnerExitCode -eq 0) {
        $aggregatePath = Join-Path $runnersRoot "aggregate-module-audit.ps1"
        & $aggregatePath `
            -ModuleName $singleModuleTarget.ModuleName `
            -RunId $resolvedRunId `
            -CommitSha $commitSha `
            -ArtifactsRoot $artifactsRoot `
            -SchemasRoot $schemasRoot
        $aggregateExitCode = $LASTEXITCODE
    } else {
        $aggregateExitCode = $runnerExitCode
    }

    $planExitCode = $null
    $moduleIndexPath = Get-ModuleAuditIndexPath -ArtifactsRoot $artifactsRoot -RunId $resolvedRunId -ModuleName $singleModuleTarget.ModuleName
    $plannerReady = $false
    $remediationPath = Get-ModuleOptimizationPlanPath -ArtifactsRoot $artifactsRoot -RunId $resolvedRunId -ModuleName $singleModuleTarget.ModuleName
    if ($aggregateExitCode -eq 0 -and (Test-Path -LiteralPath $moduleIndexPath)) {
        $index = Read-JsonFile -Path $moduleIndexPath
        $plannerReady = [bool]$index.planner_ready
        if ($plannerReady -or $ForceRemediationPlan) {
            $plannerPath = Join-Path $runnersRoot "plan-module-remediation.ps1"
            & $plannerPath `
                -ModuleName $singleModuleTarget.ModuleName `
                -RunId $resolvedRunId `
                -CommitSha $commitSha `
                -RepoRoot $repoRoot `
                -PromptsRoot $promptsRoot `
                -SchemasRoot $schemasRoot `
                -ArtifactsRoot $artifactsRoot `
                -SchemaEnforcementMode $SchemaMode `
                -Profile $Profile `
                -Model $Model `
                -Reasoning $Reasoning `
                -RequirePlannerReady:(-not $ForceRemediationPlan)
            $planExitCode = $LASTEXITCODE
        }
    }

    $finalExitCode = $runnerExitCode
    if ($aggregateExitCode -ne 0) {
        $finalExitCode = $aggregateExitCode
    } elseif ($null -ne $planExitCode -and $planExitCode -ne 0) {
        $finalExitCode = $planExitCode
    }

    Write-Host ""
    Write-Host "=== Single-Module Audit Completed ===" -ForegroundColor Green
    Write-Host ("  {0,-14}: {1}" -f "ModuleIndex", $moduleIndexPath)
    if (($plannerReady -or $ForceRemediationPlan) -and (Test-Path -LiteralPath $remediationPath)) {
        Write-Host ("  {0,-14}: {1}" -f "Remediation", $remediationPath)
    } elseif ($ForceRemediationPlan) {
        Write-Host ("  {0,-14}: {1}" -f "Remediation", "forced generation requested, but output file was not generated")
    } elseif ($plannerReady) {
        Write-Host ("  {0,-14}: {1}" -f "Remediation", "planner was eligible, but output file was not generated")
    } else {
        Write-Host ("  {0,-14}: {1}" -f "Remediation", "not generated (planner_ready=false)")
    }
    Write-Host ("  {0,-14}: {1}" -f "FinalExitCode", $finalExitCode)
    exit $finalExitCode
}

$allRunnerPath = Join-Path $runnersRoot "run-all-modules-audit.ps1"
& $allRunnerPath `
    -ModulesRoot $modulesRoot `
    -RunId $resolvedRunId `
    -CommitSha $commitSha `
    -RepoRoot $repoRoot `
    -PromptsRoot $promptsRoot `
    -SchemasRoot $schemasRoot `
    -ArtifactsRoot $artifactsRoot `
    -SchemaEnforcementMode $SchemaMode `
    -MaxParallel $MaxParallel `
    -Profile $Profile `
    -ForceRemediationPlan:$ForceRemediationPlan `
    -ModuleFilter @() `
    -ExcludeModules @() `
    -StopOnModuleFailure:$StopOnFailure
$allExitCode = $LASTEXITCODE

$runSummaryPath = Get-RunSummaryPath -ArtifactsRoot $artifactsRoot -RunId $resolvedRunId
Write-Host ""
Write-Host "=== All-Modules Audit Completed ===" -ForegroundColor Green
Write-Host ("  {0,-14}: {1}" -f "RunSummary", $runSummaryPath)
Write-Host ("  {0,-14}: {1}" -f "FinalExitCode", $allExitCode)

exit $allExitCode
