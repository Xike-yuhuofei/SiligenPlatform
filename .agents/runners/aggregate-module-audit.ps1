[CmdletBinding()]
param(
    [Parameter(Mandatory)]
    [string]$ModuleName,

    [Parameter(Mandatory)]
    [string]$RunId,

    [Parameter(Mandatory)]
    [string]$CommitSha,

    [Parameter(Mandatory)]
    [string]$ArtifactsRoot,

    [Parameter(Mandatory)]
    [string]$SchemasRoot,

    [switch]$FailOnMissingReports
)

$ErrorActionPreference = "Stop"
$commonPath = Join-Path $PSScriptRoot "common.ps1"
. $commonPath

$artifactsRootResolved = ConvertTo-AbsolutePath -Path $ArtifactsRoot
$schemasRootResolved = ConvertTo-AbsolutePath -Path $SchemasRoot
$reportSchemaPath = Join-Path $schemasRootResolved "report.schema.json"
$indexSchemaPath = Join-Path $schemasRootResolved "module-audit-index.schema.json"
$indexPath = Get-ModuleAuditIndexPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $ModuleName

$reportRows = @()
$plannerBlockers = [System.Collections.Generic.List[string]]::new()
$priorityQueue = [System.Collections.Generic.List[object]]::new()
$severityCounts = @{
    low      = 0
    medium   = 0
    high     = 0
    critical = 0
}

foreach ($audit in Get-AuditDefinitions) {
    $reportPath = Get-ReportPath -ArtifactsRoot $artifactsRootResolved -RunId $RunId -ModuleName $ModuleName -PromptName $audit.PromptName
    $validation = Test-JsonAgainstSchema -JsonPath $reportPath -SchemaPath $reportSchemaPath
    if (-not $validation.IsValid) {
        $plannerBlockers.Add("invalid or missing report for dimension '$($audit.Dimension)' at '$reportPath'")
        if ($FailOnMissingReports) {
            Write-Error ($validation.Errors -join [Environment]::NewLine)
            exit (Get-AuditExitCode -Name "missing_report_files")
        }

        $reportRows += [ordered]@{
            dimension              = $audit.Dimension
            skill_name             = $audit.SkillName
            status                 = "failed"
            verdict                = "unknown"
            severity               = "critical"
            confidence             = "low"
            report_path            = $reportPath
            finding_count          = 0
            critical_finding_count = 0
        }
        continue
    }

    $report = Read-JsonFile -Path $reportPath
    $findings = @($report.findings)
    $criticalFindingCount = @($findings | Where-Object { $_.severity -eq "critical" }).Count

    foreach ($finding in $findings) {
        if ($severityCounts.ContainsKey([string]$finding.severity)) {
            $severityCounts[[string]$finding.severity]++
        }

        $priorityQueue.Add([ordered]@{
            dimension               = $audit.Dimension
            finding_id              = $finding.id
            title                   = $finding.title
            severity                = $finding.severity
            recommendation_priority = $finding.recommendation_priority
            reason                  = "severity=$($finding.severity); verdict=$($report.verdict); confidence=$($report.confidence)"
        })
    }

    if ($report.status -ne "completed") {
        $plannerBlockers.Add("report status for dimension '$($audit.Dimension)' is '$($report.status)'")
    }
    if ($report.confidence -eq "low") {
        $plannerBlockers.Add("low confidence report for dimension '$($audit.Dimension)'")
    }
    $openQuestions = @()
    if ($report.ContainsKey("open_questions")) {
        $openQuestions = @($report["open_questions"])
    }

    if ($openQuestions.Count -gt 0) {
        $plannerBlockers.Add("open questions remain for dimension '$($audit.Dimension)'")
    }

    $reportRows += [ordered]@{
        dimension              = $audit.Dimension
        skill_name             = [string]$report.skill_name
        status                 = [string]$report.status
        verdict                = [string]$report.verdict
        severity               = [string]$report.severity
        confidence             = [string]$report.confidence
        report_path            = $reportPath
        finding_count          = $findings.Count
        critical_finding_count = $criticalFindingCount
    }
}

$overallStatus = "completed"
if (@($reportRows | Where-Object { $_.status -eq "failed" }).Count -gt 0) {
    $overallStatus = "failed"
} elseif (@($reportRows | Where-Object { $_.status -ne "completed" }).Count -gt 0) {
    $overallStatus = "partial"
}

$overallVerdict = "healthy"
if (@($reportRows | Where-Object { $_.verdict -eq "fail" }).Count -gt 0) {
    $overallVerdict = "unhealthy"
} elseif (@($reportRows | Where-Object { $_.verdict -eq "concern" }).Count -gt 0) {
    $overallVerdict = "watch"
} elseif (@($reportRows | Where-Object { $_.verdict -eq "unknown" }).Count -gt 0) {
    $overallVerdict = "unknown"
}

$overallRisk = Get-HighestSeverity -Severities @($reportRows | ForEach-Object { [string]$_.severity })
if ($overallRisk -eq "none") {
    $overallRisk = "low"
}

$sortedPriorityQueue = @(
    $priorityQueue | Sort-Object `
        @{ Expression = {
            switch ($_.recommendation_priority) {
                "p0" { 0 }
                "p1" { 1 }
                "p2" { 2 }
                default { 3 }
            }
        } },
        @{ Expression = {
            switch ($_.severity) {
                "critical" { 0 }
                "high" { 1 }
                "medium" { 2 }
                default { 3 }
            }
        } },
        title | Select-Object -First 20
)

$plannerReady = $true
if ($overallStatus -ne "completed") {
    $plannerReady = $false
}
if ($plannerBlockers.Count -gt 0) {
    $plannerReady = $false
}

$indexPayload = [ordered]@{
    schema_version = "1.0"
    run_id         = $RunId
    commit_sha     = $CommitSha
    module_name    = $ModuleName
    overall_status = $overallStatus
    overall_verdict = $overallVerdict
    overall_risk    = $overallRisk
    audit_reports   = @($reportRows)
    finding_totals  = [ordered]@{
        total    = [int]($severityCounts.low + $severityCounts.medium + $severityCounts.high + $severityCounts.critical)
        low      = [int]$severityCounts.low
        medium   = [int]$severityCounts.medium
        high     = [int]$severityCounts.high
        critical = [int]$severityCounts.critical
    }
    priority_queue = @($sortedPriorityQueue)
    planner_ready  = $plannerReady
    planner_blockers = @($plannerBlockers | Select-Object -Unique | Select-Object -First 20)
}

Write-JsonFile -Path $indexPath -Data $indexPayload
$validation = Test-JsonAgainstSchema -JsonPath $indexPath -SchemaPath $indexSchemaPath
if (-not $validation.IsValid) {
    Write-Error ($validation.Errors -join [Environment]::NewLine)
    exit $validation.FailureCode
}

exit (Get-AuditExitCode -Name "success")
