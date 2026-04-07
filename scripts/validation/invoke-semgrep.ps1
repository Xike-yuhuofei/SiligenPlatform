[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\semgrep",
    [string]$ConfigPath = "scripts\validation\semgrep\arch-rules.yml",
    [switch]$SoftFail
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ReportDir)
$resolvedConfigPath = Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ConfigPath

if (-not (Test-Path $resolvedConfigPath)) {
    throw "未找到 Semgrep 规则文件: $resolvedConfigPath"
}

$semgrepCommand = Resolve-WorkspaceToolPath -ToolNames @("pysemgrep", "semgrep") -Required
$jsonReportPath = Join-Path $resolvedReportDir "semgrep-results.json"
$toolOutputPath = Join-Path $resolvedReportDir "semgrep-tool-output.txt"
$mdReportPath = Join-Path $resolvedReportDir "semgrep-summary.md"

if (Test-Path $jsonReportPath) {
    Remove-Item -LiteralPath $jsonReportPath -Force
}
if (Test-Path $toolOutputPath) {
    Remove-Item -LiteralPath $toolOutputPath -Force
}

$scanTargets = @(
    (Join-Path $workspaceRoot "apps"),
    (Join-Path $workspaceRoot "modules"),
    (Join-Path $workspaceRoot "shared"),
    (Join-Path $workspaceRoot "scripts"),
    (Join-Path $workspaceRoot "config"),
    (Join-Path $workspaceRoot "deploy"),
    (Join-Path $workspaceRoot "cmake"),
    (Join-Path $workspaceRoot "tests"),
    (Join-Path $workspaceRoot "build.ps1"),
    (Join-Path $workspaceRoot "test.ps1"),
    (Join-Path $workspaceRoot "ci.ps1"),
    (Join-Path $workspaceRoot "legacy-exit-check.ps1"),
    (Join-Path $workspaceRoot "CMakeLists.txt")
) | Where-Object { Test-Path $_ }

$arguments = @(
    "scan"
) + $scanTargets + @(
    "--config",
    $resolvedConfigPath,
    "--metrics=off",
    "--scan-unknown-extensions",
    "--no-git-ignore",
    "--json",
    "--output",
    $jsonReportPath,
    "--error"
)

Write-Output "semgrep gate: $semgrepCommand $($arguments -join ' ')"
$previousErrorActionPreference = $ErrorActionPreference
$ErrorActionPreference = "Continue"
try {
    $toolOutput = & $semgrepCommand @arguments 2>&1
}
finally {
    $ErrorActionPreference = $previousErrorActionPreference
}
$semgrepExitCode = $LASTEXITCODE
Set-Content -LiteralPath $toolOutputPath -Value (($toolOutput | Out-String).Trim()) -Encoding UTF8

$payload = $null
if (Test-Path $jsonReportPath) {
    try {
        $payload = Get-Content -LiteralPath $jsonReportPath -Raw | ConvertFrom-Json
    }
    catch {
        $payload = $null
    }
}
$toolExecutionFailed = $semgrepExitCode -gt 1 -or $null -eq $payload
if ($toolExecutionFailed) {
    $fallbackPayload = [PSCustomObject]@{
        version = "wrapper-fallback"
        errors = @(
            [PSCustomObject]@{
                type = "semgrep-tool-error"
                exit_code = $semgrepExitCode
                tool_output_path = $toolOutputPath
                message = "Semgrep tool execution failed or did not emit valid JSON output."
            }
        )
        results = @()
    }
    $fallbackPayload | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $jsonReportPath -Encoding UTF8
    $payload = $fallbackPayload
}
$results = @()
if ($null -ne $payload -and $null -ne $payload.results) {
    $results = @($payload.results)
}

$summaryLines = @(
    '# Semgrep Architecture Gate',
    '',
    '- config: `scripts/validation/semgrep/arch-rules.yml`',
    '- gate: `hard-fail`',
    '- failure_condition: `>=1 semgrep finding or tool execution failure`',
    '- json_report: `tests/reports/semgrep/semgrep-results.json`',
    '- markdown_report: `tests/reports/semgrep/semgrep-summary.md`',
    '- tool_output: `tests/reports/semgrep/semgrep-tool-output.txt`',
    ('- finding_count: `{0}`' -f $results.Count),
    ('- tool_exit_code: `{0}`' -f $semgrepExitCode),
    ''
)

if ($toolExecutionFailed) {
    $summaryLines += '- status: `tool-error`'
    $summaryLines += ''
    $summaryLines += '- detail: Semgrep 引擎未能成功完成扫描，请检查 `semgrep-tool-output.txt`。'
}
elseif ($results.Count -eq 0) {
    $summaryLines += '- status: `passed`'
} else {
    $summaryLines += '- status: `failed`'
    $summaryLines += ''
    $summaryLines += '| rule_id | file | line | message |'
    $summaryLines += '|---|---|---:|---|'
    foreach ($result in $results) {
        $filePath = [string]$result.path
        $lineNumber = 0
        if ($null -ne $result.start -and $null -ne $result.start.line) {
            $lineNumber = [int]$result.start.line
        }
        $message = ""
        if ($null -ne $result.extra -and $null -ne $result.extra.message) {
            $message = ([string]$result.extra.message).Replace("`r", " ").Replace("`n", " ")
        }
        $summaryLines += "| $([string]$result.check_id) | $filePath | $lineNumber | $message |"
    }
}

Set-Content -LiteralPath $mdReportPath -Value ($summaryLines -join "`r`n") -Encoding UTF8

Write-Output "semgrep report json: $jsonReportPath"
Write-Output "semgrep report md:   $mdReportPath"
Write-Output "semgrep tool log:    $toolOutputPath"
Write-Output "semgrep findings:    $($results.Count)"

if ($results.Count -gt 0) {
    foreach ($result in $results) {
        $lineNumber = if ($null -ne $result.start -and $null -ne $result.start.line) { [int]$result.start.line } else { 0 }
        $message = if ($null -ne $result.extra -and $null -ne $result.extra.message) { [string]$result.extra.message } else { "" }
        Write-Output ("{0}:{1}: {2}: {3}" -f [string]$result.path, $lineNumber, [string]$result.check_id, $message)
    }
}

if ($toolExecutionFailed) {
    Write-Output "semgrep execution failed; inspect $toolOutputPath"
    if ($SoftFail) {
        Write-Warning "Semgrep 工具执行失败，当前按 soft-fail 继续（exit=$semgrepExitCode）。"
        exit 0
    }
    exit $semgrepExitCode
}

if ($results.Count -gt 0 -and -not $SoftFail) {
    exit 1
}

exit 0
