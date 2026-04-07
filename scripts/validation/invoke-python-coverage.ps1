[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\coverage\python",
    [string]$ConfigPath = ".coveragerc",
    [string]$ThresholdConfigPath = "scripts\validation\coverage-thresholds.json",
    [switch]$FailOnThreshold
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ReportDir)
$htmlDir = Ensure-WorkspaceDirectory -Path (Join-Path $resolvedReportDir "html")
$resolvedConfigPath = Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ConfigPath
$resolvedThresholdConfigPath = Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ThresholdConfigPath

if (-not (Test-Path $resolvedConfigPath)) {
    throw "未找到 coverage 配置文件: $resolvedConfigPath"
}
if (-not (Test-Path $resolvedThresholdConfigPath)) {
    throw "未找到 coverage threshold 配置: $resolvedThresholdConfigPath"
}

$coverageCommand = Resolve-WorkspaceToolPath -ToolNames @("coverage") -Required
$thresholds = Get-Content -LiteralPath $resolvedThresholdConfigPath -Raw | ConvertFrom-Json
$pythonThresholds = $thresholds.python

$existingPythonPath = $env:PYTHONPATH
$env:PYTHONPATH = ((@(
    (Join-Path $workspaceRoot "apps\hmi-app\src"),
    (Join-Path $workspaceRoot "modules\hmi-application\application"),
    (Join-Path $workspaceRoot "modules\dxf-geometry\application"),
    (Join-Path $workspaceRoot "shared\testing\test-kit\src"),
    $existingPythonPath
) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join [IO.Path]::PathSeparator)

$coverageDataPath = Join-Path $resolvedReportDir ".coverage"
$jsonReportPath = Join-Path $resolvedReportDir "coverage.json"
$xmlReportPath = Join-Path $resolvedReportDir "coverage.xml"
$mdReportPath = Join-Path $resolvedReportDir "coverage-summary.md"

function Invoke-CoverageCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments
    )

    & $coverageCommand @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "coverage 命令失败: $coverageCommand $($Arguments -join ' ')"
    }
}

try {
    Invoke-CoverageCommand -Arguments @("erase", "--rcfile=$resolvedConfigPath", "--data-file", $coverageDataPath)

    $commands = @(
        @("run", "--rcfile=$resolvedConfigPath", "--data-file", $coverageDataPath, "--source", "hmi_client,hmi_application,engineering_data", "-m", "unittest", "discover", "-s", (Join-Path $workspaceRoot "apps\hmi-app\tests\unit"), "-p", "test_*.py"),
        @("run", "--rcfile=$resolvedConfigPath", "--append", "--data-file", $coverageDataPath, "--source", "hmi_client,hmi_application,engineering_data", (Join-Path $workspaceRoot "modules\hmi-application\tests\run_hmi_application_tests.py")),
        @("run", "--rcfile=$resolvedConfigPath", "--append", "--data-file", $coverageDataPath, "--source", "hmi_client,hmi_application,engineering_data", "-m", "pytest", (Join-Path $workspaceRoot "tests\contracts\test_engineering_data_compatibility.py"), "-q")
    )

    foreach ($arguments in $commands) {
        Write-Output "python coverage: $coverageCommand $($arguments -join ' ')"
        Invoke-CoverageCommand -Arguments $arguments
    }

    Invoke-CoverageCommand -Arguments @("json", "--rcfile=$resolvedConfigPath", "--data-file", $coverageDataPath, "-o", $jsonReportPath)
    Invoke-CoverageCommand -Arguments @("xml", "--rcfile=$resolvedConfigPath", "--data-file", $coverageDataPath, "-o", $xmlReportPath)
    Invoke-CoverageCommand -Arguments @("html", "--rcfile=$resolvedConfigPath", "--data-file", $coverageDataPath, "-d", $htmlDir)
}
finally {
    $env:PYTHONPATH = $existingPythonPath
}

$pythonCommand = Get-WorkspacePythonCommand
$summaryRunner = Join-Path $PSScriptRoot "python-coverage-summary.py"
if (-not (Test-Path $summaryRunner)) {
    throw "未找到 Python coverage summary helper: $summaryRunner"
}
$summaryPayload = (& $pythonCommand $summaryRunner $jsonReportPath $resolvedThresholdConfigPath | Out-String).Trim() | ConvertFrom-Json
$globalBranchPercent = [double]$summaryPayload.global_branch_percent
$pythonThresholds = [PSCustomObject]@{
    global_branch_min = [double]$summaryPayload.global_branch_min
    enforcement = [string]$summaryPayload.enforcement
}
$criticalResults = @($summaryPayload.critical_results)

$thresholdFailures = @()
if ($globalBranchPercent -lt [double]$pythonThresholds.global_branch_min) {
    $thresholdFailures += "global branch coverage $globalBranchPercent% < $($pythonThresholds.global_branch_min)%"
}
foreach ($result in $criticalResults) {
    if ($result.status -eq "failed") {
        $thresholdFailures += "$($result.path_prefix) branch coverage $($result.actual_branch_percent)% < $($result.threshold)%"
    }
}

$mdLines = @(
    '# Python Branch Coverage',
    '',
    '- gate: `report-only`',
    '- failure_condition: `when FailOnThreshold is enabled and any threshold is below target`',
    '- json_report: `tests/reports/coverage/python/coverage.json`',
    '- xml_report: `tests/reports/coverage/python/coverage.xml`',
    '- html_report: `tests/reports/coverage/python/html/index.html`',
    ('- global_branch_percent: `{0}`' -f $globalBranchPercent),
    ('- global_branch_min: `{0}`' -f [double]$pythonThresholds.global_branch_min),
    ('- threshold_enforcement: `{0}`' -f [string]$pythonThresholds.enforcement),
    ''
)
$mdLines += '| path_prefix | actual_branch_percent | threshold | status |'
$mdLines += '|---|---:|---:|---|'
foreach ($result in $criticalResults) {
    $mdLines += "| $([string]$result.path_prefix) | $([double]$result.actual_branch_percent) | $([double]$result.threshold) | $([string]$result.status) |"
}
if ($thresholdFailures.Count -gt 0) {
    $mdLines += ''
    $mdLines += '## Threshold Gaps'
    $mdLines += ''
    foreach ($failure in $thresholdFailures) {
        $mdLines += "- $failure"
    }
}

Set-Content -LiteralPath $mdReportPath -Value ($mdLines -join "`r`n") -Encoding UTF8

Write-Output "python coverage json: $jsonReportPath"
Write-Output "python coverage xml:  $xmlReportPath"
Write-Output "python coverage html: $htmlDir"
Write-Output "python coverage global branch percent: $globalBranchPercent"

if ($FailOnThreshold -and $thresholdFailures.Count -gt 0 -and [string]$pythonThresholds.enforcement -eq "hard-fail") {
    foreach ($failure in $thresholdFailures) {
        Write-Output "python coverage threshold miss: $failure"
    }
    exit 1
}

exit 0
