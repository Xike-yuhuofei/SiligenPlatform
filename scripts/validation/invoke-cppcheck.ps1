[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\static-analysis\cppcheck",
    [ValidateSet("ChangedFiles", "Project")]
    [string]$Scope,
    [string[]]$ChangedFile = @(),
    [switch]$FailOnIssues
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

function ConvertTo-NativeArgument {
    param([string]$Value)

    if ($null -eq $Value) {
        return '""'
    }
    if ($Value -notmatch '[\s"]') {
        return $Value
    }
    return '"' + ($Value.Replace('"', '\"')) + '"'
}

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ReportDir)
$xmlReportPath = Join-Path $resolvedReportDir "cppcheck.xml"
$mdReportPath = Join-Path $resolvedReportDir "cppcheck-summary.md"

if ([string]::IsNullOrWhiteSpace($Scope)) {
    $lines = @(
        '# Cppcheck',
        '',
        '- status: `scope-missing`',
        ('- gate: `{0}`' -f $(if ($FailOnIssues) { 'blocking' } else { 'report-only' })),
        '- failure_condition: `Scope must be explicitly set to ChangedFiles or Project`',
        '- detail: invoke-cppcheck.ps1 requires -Scope ChangedFiles or -Scope Project. Implicit fallback scanning is not supported.'
    )
    Set-Content -LiteralPath $xmlReportPath -Value "" -Encoding UTF8
    Set-Content -LiteralPath $mdReportPath -Value ($lines -join "`r`n") -Encoding UTF8
    Write-Output "cppcheck failed: explicit -Scope is required"
    exit 1
}

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

if ($Scope -eq "ChangedFiles" -and $ChangedFile.Count -eq 0) {
    $lines = @(
        '# Cppcheck',
        '',
        '- status: `changed-files-missing`',
        ('- gate: `{0}`' -f $(if ($FailOnIssues) { 'blocking' } else { 'report-only' })),
        '- scope: `ChangedFiles`',
        '- failure_condition: `ChangedFiles scope requires at least one ChangedFile argument`',
        '- detail: ChangedFiles scope is explicit and must not run against an implicit repository-wide fallback.'
    )
    Set-Content -LiteralPath $xmlReportPath -Value "" -Encoding UTF8
    Set-Content -LiteralPath $mdReportPath -Value ($lines -join "`r`n") -Encoding UTF8
    Write-Output "cppcheck failed: ChangedFiles scope requires -ChangedFile"
    exit 1
}

if ($Scope -eq "ChangedFiles" -and $changedCppFiles.Count -eq 0) {
    $lines = @(
        '# Cppcheck',
        '',
        '- status: `skipped-no-cpp-files`',
        '- exit_code: `0`',
        ('- gate: `{0}`' -f $(if ($FailOnIssues) { 'blocking' } else { 'report-only' })),
        '- scope: `ChangedFiles`',
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

if ($Scope -eq "Project" -and -not (Test-Path $compileCommandsPath)) {
    $lines = @(
        '# Cppcheck',
        '',
        '- status: `compile-db-missing`',
        ('- gate: `{0}`' -f $(if ($FailOnIssues) { 'blocking' } else { 'report-only' })),
        '- scope: `Project`',
        ('- compile_commands: `{0}`' -f (Get-WorkspaceRelativePath -WorkspaceRoot $workspaceRoot -TargetPath $compileCommandsPath)),
        '- failure_condition: `Project scope requires compile_commands.json`',
        '- detail: Project scope requires a compile database for the current worktree. Generate it through the canonical build preparation flow before running project-wide cppcheck.'
    )
    Set-Content -LiteralPath $xmlReportPath -Value "" -Encoding UTF8
    Set-Content -LiteralPath $mdReportPath -Value ($lines -join "`r`n") -Encoding UTF8
    Write-Output "cppcheck failed: compile_commands.json missing for Project scope"
    exit 1
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
if ($Scope -eq "Project") {
    $arguments += "--project=$compileCommandsPath"
    $arguments += "--file-filter=*/apps/*"
    $arguments += "--file-filter=*/modules/*"
    $arguments += "--file-filter=*/shared/*"
    $arguments += "--suppress=*:*/third_party/*"
}
elseif ($Scope -eq "ChangedFiles") {
    $arguments += @($changedCppFiles)
}

Write-Output "cppcheck: $cppcheckCommand $($arguments -join ' ')"
$stderrPath = Join-Path $resolvedReportDir "cppcheck.stderr.tmp"
if (Test-Path $stderrPath) {
    Remove-Item -LiteralPath $stderrPath -Force
}
$processStartInfo = [System.Diagnostics.ProcessStartInfo]::new()
$processStartInfo.FileName = $cppcheckCommand
$processStartInfo.Arguments = (@($arguments) | ForEach-Object { ConvertTo-NativeArgument -Value ([string]$_) }) -join " "
$processStartInfo.UseShellExecute = $false
$processStartInfo.RedirectStandardError = $true
$processStartInfo.RedirectStandardOutput = $true
$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $processStartInfo
try {
    [void]$process.Start()
    $stderrText = $process.StandardError.ReadToEnd()
    $stdoutText = $process.StandardOutput.ReadToEnd()
    $process.WaitForExit()
    $exitCode = [int]$process.ExitCode
    if (-not [string]::IsNullOrWhiteSpace($stdoutText)) {
        Write-Output $stdoutText.Trim()
    }
}
finally {
    $process.Dispose()
}
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
    ('- scope: `{0}`' -f $Scope),
    ('- cpp_files: `{0}`' -f $(if ($Scope -eq "ChangedFiles") { $changedCppFiles.Count } else { 'project' })),
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
