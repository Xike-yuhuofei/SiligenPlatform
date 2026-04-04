[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ModuleName,

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

    [string]$Profile = "codexmanager",

    [string]$Model,

    [string]$Reasoning,

    [switch]$RequirePlannerReady = $true
)

$ErrorActionPreference = "Stop"
$commonPath = Join-Path $PSScriptRoot "common.ps1"
. $commonPath

$repoRootResolved = ConvertTo-AbsolutePath -Path $RepoRoot
$promptsRootResolved = ConvertTo-AbsolutePath -Path $PromptsRoot
$schemasRootResolved = ConvertTo-AbsolutePath -Path $SchemasRoot
$artifactsRootResolved = ConvertTo-AbsolutePath -Path $ArtifactsRoot
$indexPath = Get-ModuleAuditIndexPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $ModuleName
$plannerSchemaPath = Join-Path $schemasRootResolved "module-optimization-plan.schema.json"
$plannerPromptPath = Join-Path $promptsRootResolved "audit-remediation.md"
$renderedPath = Get-RenderedPromptPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $ModuleName -PromptName "audit-remediation"
$outputPath = Get-ModuleOptimizationPlanPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $ModuleName

$indexValidation = Test-JsonAgainstSchema -JsonPath $indexPath -SchemaPath (Join-Path $schemasRootResolved "module-audit-index.schema.json")
if (-not $indexValidation.IsValid) {
    Write-Error ($indexValidation.Errors -join [Environment]::NewLine)
    exit $indexValidation.FailureCode
}

$auditIndex = Read-JsonFile -Path $indexPath
if ($RequirePlannerReady -and -not [bool]$auditIndex.planner_ready) {
    Write-Error "Planner is not ready for module '$ModuleName'."
    exit (Get-AuditExitCode -Name "audit_index_not_planner_ready")
}

try {
    Render-PromptTemplate -TemplatePath $plannerPromptPath -Variables @{
        MODULE_NAME      = $ModuleName
        REPO_ROOT        = $repoRootResolved
        RUN_ID           = $RunId
        COMMIT_SHA       = $CommitSha
        AUDIT_INDEX_PATH = $indexPath
    } -RenderedPath $renderedPath
} catch {
    $exitCode = if ($_.Exception.Message -like "*Unresolved template variables*") {
        Get-AuditExitCode -Name "unresolved_template_variable"
    } else {
        Get-AuditExitCode -Name "template_render_failed"
    }
    Write-Error $_.Exception.Message
    exit $exitCode
}

$codexExitCode = Invoke-CodexExecForPrompt -PromptPath $renderedPath -RepoRoot $repoRootResolved -SchemaPath $plannerSchemaPath -OutputPath $outputPath -SchemaEnforcementMode $SchemaEnforcementMode -Profile $Profile -Model $Model -Reasoning $Reasoning
if ($codexExitCode -ne 0) {
    Write-Error "Codex exec failed for remediation planner with exit code $codexExitCode."
    exit (Get-AuditExitCode -Name "codex_exec_failed")
}

$validation = Test-JsonAgainstSchema -JsonPath $outputPath -SchemaPath $plannerSchemaPath
if (-not $validation.IsValid) {
    Write-Error ($validation.Errors -join [Environment]::NewLine)
    exit $validation.FailureCode
}

exit (Get-AuditExitCode -Name "success")
