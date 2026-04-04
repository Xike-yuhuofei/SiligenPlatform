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

    [Parameter(Mandatory)]
    [string]$PromptName,

    [ValidateSet("model-and-local", "local-only")]
    [string]$SchemaEnforcementMode = "local-only",

    [string]$Profile = "codexmanager",

    [string]$Model,

    [string]$Reasoning
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

$promptSelection = Resolve-AuditPromptSelection -PromptNames @($PromptName)
if ($promptSelection.InvalidPromptNames.Count -gt 0 -or $promptSelection.Audits.Count -ne 1) {
    Write-Error "Unknown audit prompt '$PromptName'."
    exit (Get-AuditExitCode -Name "invalid_prompt_selection")
}

$audit = $promptSelection.Audits[0]
$templatePath = Join-Path $promptsRootResolved $audit.PromptFile
$renderedPath = Get-RenderedPromptPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $ModuleName -PromptName $audit.PromptName
$outputPath = Get-ReportPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $ModuleName -PromptName $audit.PromptName

$existingValidation = Test-JsonAgainstSchema -JsonPath $outputPath -SchemaPath $reportSchemaPath
if ($existingValidation.IsValid) {
    exit (Get-AuditExitCode -Name "success")
}

try {
    Render-PromptTemplate -TemplatePath $templatePath -Variables @{
        PROMPT_NAME           = $audit.PromptName
        SKILL_NAME            = $audit.SkillName
        AUDIT_DIMENSION       = $audit.Dimension
        AUDIT_DIMENSION_LABEL = $audit.Label
        MODULE_NAME           = $ModuleName
        MODULE_PATH           = $modulePathResolved
        REPO_ROOT             = $repoRootResolved
        RUN_ID                = $RunId
        COMMIT_SHA            = $CommitSha
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

$codexExitCode = Invoke-CodexExecForPrompt -PromptPath $renderedPath -RepoRoot $repoRootResolved -SchemaPath $reportSchemaPath -OutputPath $outputPath -SchemaEnforcementMode $SchemaEnforcementMode -Profile $Profile -Model $Model -Reasoning $Reasoning
if ($codexExitCode -ne 0) {
    Write-Error "Codex exec failed for '$($audit.PromptName)' with exit code $codexExitCode."
    exit (Get-AuditExitCode -Name "codex_exec_failed")
}

$validation = Test-JsonAgainstSchema -JsonPath $outputPath -SchemaPath $reportSchemaPath
if (-not $validation.IsValid) {
    Write-Error ($validation.Errors -join [Environment]::NewLine)
    exit $validation.FailureCode
}

exit (Get-AuditExitCode -Name "success")
