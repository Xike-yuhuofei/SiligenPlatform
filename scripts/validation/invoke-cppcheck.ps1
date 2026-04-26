[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\static-analysis\cppcheck",
    [switch]$FailOnIssues
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

function ConvertTo-ArgumentLine {
    param([string[]]$Arguments)

    $quoted = @()
    foreach ($arg in @($Arguments)) {
        if ($null -eq $arg) {
            continue
        }

        $text = [string]$arg
        if ($text.Length -eq 0) {
            $quoted += '""'
            continue
        }

        if ($text -notmatch '[\s"]') {
            $quoted += $text
            continue
        }

        $escaped = $text -replace '\\(?=("+)$)', '\\' -replace '"', '\"'
        $quoted += '"' + $escaped + '"'
    }

    return ($quoted -join " ")
}

function Invoke-ExternalCommand {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FileName,
        [Parameter(Mandatory = $true)]
        [string[]]$Arguments,
        [Parameter(Mandatory = $true)]
        [string]$WorkingDirectory
    )

    $process = New-Object System.Diagnostics.Process
    $process.StartInfo.FileName = $FileName
    $process.StartInfo.Arguments = ConvertTo-ArgumentLine -Arguments $Arguments
    $process.StartInfo.WorkingDirectory = $WorkingDirectory
    $process.StartInfo.UseShellExecute = $false
    $process.StartInfo.RedirectStandardOutput = $true
    $process.StartInfo.RedirectStandardError = $true
    $process.StartInfo.CreateNoWindow = $true

    try {
        [void]$process.Start()
        $stdoutTask = $process.StandardOutput.ReadToEndAsync()
        $stderrTask = $process.StandardError.ReadToEndAsync()
        $process.WaitForExit()

        return [pscustomobject]@{
            ExitCode = $process.ExitCode
            StandardOutput = $stdoutTask.Result
            StandardError = $stderrTask.Result
        }
    }
    finally {
        $process.Dispose()
    }
}

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
$commandResult = Invoke-ExternalCommand -FileName $cppcheckCommand -Arguments $arguments -WorkingDirectory $workspaceRoot
$exitCode = $commandResult.ExitCode
$xmlContent = [string]$commandResult.StandardError
if ([string]::IsNullOrWhiteSpace($xmlContent)) {
    $xmlContent = [string]$commandResult.StandardOutput
}
Set-Content -LiteralPath $xmlReportPath -Value $xmlContent.Trim() -Encoding UTF8

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
