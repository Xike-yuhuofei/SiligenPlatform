[CmdletBinding()]
param(
    [string]$WorkspaceRoot = ".",
    [string]$ReportDir = "tests/reports/review-baseline-completeness"
)

$ErrorActionPreference = "Stop"

function Resolve-AbsolutePath {
    param(
        [string]$BasePath,
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $PathValue))
}

function Get-RelativeRepoPath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    try {
        return [System.IO.Path]::GetRelativePath($BasePath, $TargetPath)
    } catch {
        return $TargetPath
    }
}

function New-StringFromCodeUnits {
    param(
        [int[]]$CodeUnits
    )

    return [string]::Concat(($CodeUnits | ForEach-Object { [char]$_ }))
}

$repoRoot = Resolve-AbsolutePath -BasePath (Get-Location).Path -PathValue $WorkspaceRoot
$resolvedReportDir = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

$requiredFiles = @(
    "docs/process-model/reviews/architecture-review-recheck-report-20260331-082102.md",
    "docs/process-model/reviews/coordinate-alignment-module-architecture-review-20260331-074844.md",
    "docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md",
    "docs/process-model/reviews/hmi-application-module-architecture-review-20260331-074433.md",
    "docs/process-model/reviews/motion-planning-module-architecture-review-20260331-075136-944.md",
    "docs/process-model/reviews/process-path-module-architecture-review-20260331-074844.md",
    "docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md",
    "docs/process-model/reviews/refactor-workflow-ARCH-202-preview-compat-drain-workflow-module-architecture-review-20260331-074657.md",
    "docs/process-model/reviews/runtime-execution-module-architecture-review-20260331-075228.md",
    "docs/process-model/reviews/topology-feature-module-architecture-review-20260331-075200.md"
)

$evidenceIndexHeading = "## 8. " + (New-StringFromCodeUnits @(0x8BC1, 0x636E, 0x7D22, 0x5F15))
$reviewAppendixHeading = "## " + (New-StringFromCodeUnits @(0x590D, 0x6838, 0x9644, 0x5F55))
$numberedReviewAppendixHeading = "## 9. " + (New-StringFromCodeUnits @(0x590D, 0x6838, 0x9644, 0x5F55))

$supplementExpectations = @(
    @{
        path = "docs/process-model/reviews/coordinate-alignment-module-architecture-review-20260331-074844.md"
        patterns = @($evidenceIndexHeading, $reviewAppendixHeading, "Get-FileHash")
    },
    @{
        path = "docs/process-model/reviews/dispense-packaging-module-architecture-review-20260331-074840.md"
        patterns = @($evidenceIndexHeading, $reviewAppendixHeading, "Get-FileHash")
    },
    @{
        path = "docs/process-model/reviews/topology-feature-module-architecture-review-20260331-075200.md"
        patterns = @($evidenceIndexHeading, $numberedReviewAppendixHeading, "Get-FileHash")
    },
    @{
        path = "docs/process-model/reviews/process-planning-module-architecture-review-20260331-075201.md"
        patterns = @($evidenceIndexHeading, $numberedReviewAppendixHeading, "git diff --no-index")
    }
)

$findings = New-Object System.Collections.Generic.List[object]
$presentFiles = New-Object System.Collections.Generic.List[string]

foreach ($relativePath in $requiredFiles) {
    $fullPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $relativePath
    if (-not (Test-Path -LiteralPath $fullPath)) {
        $findings.Add([pscustomobject]@{
            rule_id = "missing-review-baseline"
            severity = "error"
            file = $relativePath
            detail = "required review baseline file is missing"
        })
        continue
    }

    $presentFiles.Add($relativePath)
}

foreach ($expectation in $supplementExpectations) {
    $fullPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $expectation.path
    if (-not (Test-Path -LiteralPath $fullPath)) {
        continue
    }

    $content = Get-Content -Raw -Encoding UTF8 -LiteralPath $fullPath
    foreach ($pattern in $expectation.patterns) {
        if ($content -notmatch [regex]::Escape($pattern)) {
            $findings.Add([pscustomobject]@{
                rule_id = "missing-review-supplement-pattern"
                severity = "error"
                file = $expectation.path
                detail = "required supplement pattern missing: $pattern"
            })
        }
    }
}

$summary = [ordered]@{
    generated_at = (Get-Date).ToString("s")
    workspace_root = $repoRoot
    report_dir = $resolvedReportDir
    required_file_count = $requiredFiles.Count
    present_file_count = $presentFiles.Count
    finding_count = $findings.Count
    status = if ($findings.Count -eq 0) { "passed" } else { "failed" }
    present_files = $presentFiles
    findings = $findings
}

$jsonPath = Join-Path $resolvedReportDir "review-baseline-completeness.json"
$mdPath = Join-Path $resolvedReportDir "review-baseline-completeness.md"

$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonPath -Encoding UTF8

$mdLines = @(
    "# Review Baseline Completeness Report",
    "",
    "- generated_at: $($summary.generated_at)",
    "- workspace_root: $($summary.workspace_root)",
    "- report_dir: $($summary.report_dir)",
    "- required_file_count: $($summary.required_file_count)",
    "- present_file_count: $($summary.present_file_count)",
    "- finding_count: $($summary.finding_count)",
    "- status: $($summary.status)",
    "",
    "| rule_id | severity | file | detail |",
    "|---|---|---|---|"
)

if ($findings.Count -eq 0) {
    $mdLines += "| none | info | - | baseline review pack and required supplement markers are present |"
} else {
    foreach ($finding in $findings) {
        $mdLines += "| $($finding.rule_id) | $($finding.severity) | $($finding.file) | $($finding.detail) |"
    }
}

$mdLines | Set-Content -Path $mdPath -Encoding UTF8

Write-Host "Report(JSON): $jsonPath"
Write-Host "Report(MD):   $mdPath"
Write-Host "Status:       $($summary.status)"

if ($findings.Count -gt 0) {
    exit 1
}

exit 0
