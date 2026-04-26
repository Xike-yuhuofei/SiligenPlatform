[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\static-analysis\cppcheck",
    [switch]$FailOnIssues
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ReportDir)
$xmlReportPath = Join-Path $resolvedReportDir "cppcheck.xml"
$mdReportPath = Join-Path $resolvedReportDir "cppcheck-summary.md"

$cppcheckCommand = Resolve-WorkspaceToolPath -ToolNames @("cppcheck")
if ([string]::IsNullOrWhiteSpace($cppcheckCommand)) {
    $lines = @(
        '# Cppcheck',
        '',
        '- status: `tool-missing`',
        ('- gate: `{0}`' -f $(if ($FailOnIssues) { 'blocking' } else { 'report-only' })),
        '- failure_condition: `tool missing or non-zero cppcheck exit when FailOnIssues is enabled`',
        '- report_dir: `tests/reports/static-analysis/cppcheck`',
        '- detail: 当前环境未安装 `cppcheck`。强制门禁模式下必须安装 cppcheck 后再放行。'
    )
    Set-Content -LiteralPath $mdReportPath -Value ($lines -join "`r`n") -Encoding UTF8
    Write-Output "cppcheck tool missing; summary written to $mdReportPath"
    if ($FailOnIssues) {
        exit 1
    }
    exit 0
}

$buildRoot = Get-ControlAppsBuildRoot -WorkspaceRoot $workspaceRoot
$compileCommandsPath = Join-Path $buildRoot "compile_commands.json"
$arguments = @(
    "--enable=warning,style,performance,portability",
    "--xml",
    "--xml-version=2",
    "--inline-suppr",
    "--suppress=missingIncludeSystem",
    "--suppress=unusedFunction"
)
if (Test-Path $compileCommandsPath) {
    $arguments += "--project=$compileCommandsPath"
}
else {
    $arguments += @(
        (Join-Path $workspaceRoot "apps"),
        (Join-Path $workspaceRoot "modules"),
        (Join-Path $workspaceRoot "shared")
    )
}

Write-Output "cppcheck: $cppcheckCommand $($arguments -join ' ')"
$stderrLines = & $cppcheckCommand @arguments 2>&1
$exitCode = $LASTEXITCODE
Set-Content -LiteralPath $xmlReportPath -Value (($stderrLines | Out-String).Trim()) -Encoding UTF8

$status = if ($exitCode -eq 0) { "passed" } else { "reported" }
$lines = @(
    '# Cppcheck',
    '',
    ('- status: `{0}`' -f $status),
    '- gate: `report-only by default`',
    '- failure_condition: `FailOnIssues + non-zero exit`',
    '- xml_report: `tests/reports/static-analysis/cppcheck/cppcheck.xml`',
    '- detail: 当前第一版以高信号基础规则为主，历史噪声先报告不阻断。'
)
Set-Content -LiteralPath $mdReportPath -Value ($lines -join "`r`n") -Encoding UTF8

Write-Output "cppcheck report xml: $xmlReportPath"
Write-Output "cppcheck report md:  $mdReportPath"

if ($FailOnIssues -and $exitCode -ne 0) {
    exit $exitCode
}

exit 0
