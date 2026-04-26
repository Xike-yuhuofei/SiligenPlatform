[CmdletBinding()]
param(
    [ValidateSet("native", "hil")]
    [string]$Kind,
    [string]$ReportDir = "tests\reports\pre-push-gate\followup",
    [string]$ClassificationPath = ""
)

$ErrorActionPreference = "Stop"
$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

function Resolve-WorkspacePath {
    param([string]$PathValue)
    if ([System.IO.Path]::IsPathRooted($PathValue)) { return [System.IO.Path]::GetFullPath($PathValue) }
    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

$reportDirPath = Resolve-WorkspacePath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $reportDirPath | Out-Null
$classification = $null
if (-not [string]::IsNullOrWhiteSpace($ClassificationPath)) {
    $resolvedClassification = Resolve-WorkspacePath -PathValue $ClassificationPath
    if (Test-Path -LiteralPath $resolvedClassification) {
        $classification = Get-Content -Raw -Path $resolvedClassification | ConvertFrom-Json
    }
}

$required = $false
$reason = "not required"
if ($null -ne $classification) {
    if ($Kind -eq "native") {
        $required = [bool]$classification.requires_native_followup
        $reason = [string]$classification.native_reason
    } else {
        $required = [bool]$classification.requires_hil_followup
        $reason = [string]$classification.hil_reason
    }
}

$summary = [ordered]@{
    schemaVersion = 1
    generated_at = (Get-Date).ToString("o")
    kind = $Kind
    status = "passed"
    required_followup = $required
    reason = $reason
    blocking_policy = "pre-push records follow-up only; CI or explicit local native/HIL gates remain authoritative"
}
$jsonPath = Join-Path $reportDirPath "$Kind-followup-notice.json"
$mdPath = Join-Path $reportDirPath "$Kind-followup-notice.md"
$summary | ConvertTo-Json -Depth 8 | Set-Content -Path $jsonPath -Encoding UTF8
@(
    "# $Kind Follow-up Notice",
    "",
    "- status: ``passed``",
    "- required_followup: ``$required``",
    "- reason: $reason",
    "- policy: pre-push does not run full native, HIL, performance, or release lanes."
) | Set-Content -Path $mdPath -Encoding UTF8

Write-Output "$Kind follow-up required=$required reason=$reason"
