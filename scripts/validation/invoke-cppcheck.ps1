[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\static-analysis\cppcheck",
    [string[]]$ChangedFile = @(),
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
        '- detail: `cppcheck` is not installed in the current environment. Install cppcheck before passing the strict gate.'
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
$sourceExtensions = @(".c", ".cc", ".cpp", ".cxx", ".h", ".hh", ".hpp", ".hxx")
$changedCppFiles = @(
    $ChangedFile |
        ForEach-Object { ([string]$_).Trim() } |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        Where-Object { $sourceExtensions -contains [System.IO.Path]::GetExtension($_).ToLowerInvariant() } |
        ForEach-Object {
            if ([System.IO.Path]::IsPathRooted($_)) {
                [System.IO.Path]::GetFullPath($_)
            } else {
                [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $_))
            }
        } |
        Where-Object { Test-Path -LiteralPath $_ } |
        Select-Object -Unique
)

if ($ChangedFile.Count -gt 0 -and $changedCppFiles.Count -eq 0) {
    $lines = @(
        '# Cppcheck',
        '',
        '- status: `passed`',
        '- exit_code: `0`',
        ('- gate: `{0}`' -f $(if ($FailOnIssues) { 'blocking' } else { 'report-only' })),
        '- scope: `changed-files`',
        '- cpp_files: `0`',
        '- failure_condition: `FailOnIssues + non-zero cppcheck exit`',
        '- detail: no changed C/C++ source files were present in the supplied change scope.'
    )
    Set-Content -LiteralPath $xmlReportPath -Value "" -Encoding UTF8
    Set-Content -LiteralPath $mdReportPath -Value ($lines -join "`r`n") -Encoding UTF8
    Write-Output "cppcheck skipped: no changed C/C++ source files in supplied scope"
    Write-Output "cppcheck report md:  $mdReportPath"
    exit 0
}

$arguments = @(
    "--enable=warning,style,performance,portability",
    "--quiet",
    "--xml",
    "--xml-version=2",
    "--inline-suppr",
    "--suppress=missingIncludeSystem",
    "--suppress=unusedFunction"
)
if (Test-Path $compileCommandsPath) {
    $arguments += "--project=$compileCommandsPath"
}
elseif ($changedCppFiles.Count -gt 0) {
    $arguments += @($changedCppFiles)
}
else {
    $arguments += @(
        (Join-Path $workspaceRoot "apps"),
        (Join-Path $workspaceRoot "modules"),
        (Join-Path $workspaceRoot "shared")
    )
}

Write-Output "cppcheck: $cppcheckCommand $($arguments -join ' ')"
$stderrPath = Join-Path $resolvedReportDir "cppcheck.stderr.tmp"
if (Test-Path $stderrPath) {
    Remove-Item -LiteralPath $stderrPath -Force
}
& $cppcheckCommand @arguments 2> $stderrPath
$exitCode = if ($null -ne $LASTEXITCODE) { [int]$LASTEXITCODE } else { 0 }
$stderrText = if (Test-Path $stderrPath) { (Get-Content -Raw -LiteralPath $stderrPath) } else { "" }
Set-Content -LiteralPath $xmlReportPath -Value $stderrText.Trim() -Encoding UTF8
if (Test-Path $stderrPath) {
    Remove-Item -LiteralPath $stderrPath -Force
}

$status = if ($exitCode -eq 0) { "passed" } elseif ($FailOnIssues) { "failed" } else { "reported" }
$lines = @(
    '# Cppcheck',
    '',
    ('- status: `{0}`' -f $status),
    ('- exit_code: `{0}`' -f $exitCode),
    ('- gate: `{0}`' -f $(if ($FailOnIssues) { 'blocking' } else { 'report-only' })),
    '- failure_condition: `FailOnIssues + non-zero cppcheck exit`',
    '- xml_report: `tests/reports/static-analysis/cppcheck/cppcheck.xml`',
    '- detail: cppcheck XML is captured from stderr; FailOnIssues makes a non-zero cppcheck exit blocking.'
)
Set-Content -LiteralPath $mdReportPath -Value ($lines -join "`r`n") -Encoding UTF8

Write-Output "cppcheck report xml: $xmlReportPath"
Write-Output "cppcheck report md:  $mdReportPath"

if ($FailOnIssues -and $exitCode -ne 0) {
    exit $exitCode
}

exit 0
