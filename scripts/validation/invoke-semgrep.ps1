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
    throw "Semgrep rule file was not found: $resolvedConfigPath"
}

$fallbackScannerPath = Join-Path $PSScriptRoot "run_refactor_guard_fallback.py"
$jsonReportPath = Join-Path $resolvedReportDir "semgrep-results.json"
$toolOutputPath = Join-Path $resolvedReportDir "semgrep-tool-output.txt"
$mdReportPath = Join-Path $resolvedReportDir "semgrep-summary.md"
$preferFallbackScanner = [System.Runtime.InteropServices.RuntimeInformation]::IsOSPlatform(
    [System.Runtime.InteropServices.OSPlatform]::Windows
)
$semgrepCommand = $null
if (-not $preferFallbackScanner) {
    $semgrepCommand = Resolve-WorkspaceToolPath -ToolNames @("pysemgrep", "semgrep") -Required
}

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

$toolOutput = $null
$semgrepExitCode = 0
$fallbackOutput = $null
$fallbackExitCode = $null
$fallbackUsed = $false
$toolExecutionFailed = $false

$payload = $null
if (-not $preferFallbackScanner) {
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

    if (Test-Path $jsonReportPath) {
        try {
            $payload = Get-Content -LiteralPath $jsonReportPath -Raw | ConvertFrom-Json
        }
        catch {
            $payload = $null
        }
    }
    $toolExecutionFailed = $semgrepExitCode -gt 1 -or $null -eq $payload
}

$shouldRunFallback = $preferFallbackScanner -or $toolExecutionFailed
if ($shouldRunFallback) {
    $pythonCommand = Get-WorkspacePythonCommand
    $fallbackArguments = @(
        $fallbackScannerPath,
        "--workspace-root",
        $workspaceRoot,
        "--config-path",
        $resolvedConfigPath,
        "--report-json",
        $jsonReportPath
    )
    foreach ($scanTarget in $scanTargets) {
        $fallbackArguments += @("--scan-target", $scanTarget)
    }

    if ($preferFallbackScanner) {
        Write-Output "Windows detected; using refactor-guard fallback scanner"
    } else {
        Write-Output "semgrep tool execution failed; attempting refactor-guard fallback scanner"
    }
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    try {
        $fallbackOutput = & $pythonCommand @fallbackArguments 2>&1
    }
    finally {
        $ErrorActionPreference = $previousErrorActionPreference
    }
    $fallbackExitCode = $LASTEXITCODE
    $fallbackUsed = $true

    $payload = $null
    if (Test-Path $jsonReportPath) {
        try {
            $payload = Get-Content -LiteralPath $jsonReportPath -Raw | ConvertFrom-Json
        }
        catch {
            $payload = $null
        }
    }
    $toolExecutionFailed = $fallbackExitCode -gt 1 -or $null -eq $payload
}

$toolLogSections = @()
$toolLogSections += if ($preferFallbackScanner) {
    "primary semgrep gate: skipped on Windows in favor of repo-owned fallback scanner"
} else {
    "primary semgrep gate: $semgrepCommand $($arguments -join ' ')"
}
$toolLogSections += (($toolOutput | Out-String).Trim())
if ($fallbackUsed) {
    $toolLogSections += "fallback scanner: $pythonCommand $($fallbackArguments -join ' ')"
    $toolLogSections += (($fallbackOutput | Out-String).Trim())
}
Set-Content -LiteralPath $toolOutputPath -Value (($toolLogSections | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }) -join "`r`n`r`n") -Encoding UTF8

if ($toolExecutionFailed) {
    $fallbackPayload = [PSCustomObject]@{
        version = "wrapper-fallback"
        errors = @(
            [PSCustomObject]@{
                type = "semgrep-tool-error"
                exit_code = $semgrepExitCode
                fallback_exit_code = $fallbackExitCode
                tool_output_path = $toolOutputPath
                message = "Semgrep tool execution failed and fallback scanner did not emit valid JSON output."
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
if ($fallbackUsed) {
    $summaryLines += ('- fallback_engine: `scripts/validation/run_refactor_guard_fallback.py`')
    $summaryLines += ('- fallback_exit_code: `{0}`' -f $fallbackExitCode)
    $summaryLines += ('- execution_mode: `{0}`' -f $(if ($preferFallbackScanner) { 'windows-fallback' } else { 'fallback-after-native-tool-error' }))
    $summaryLines += ''
}

if ($toolExecutionFailed) {
    $summaryLines += '- status: `tool-error`'
    $summaryLines += ''
    $summaryLines += '- detail: Semgrep execution did not complete successfully; inspect `semgrep-tool-output.txt`.'
}
elseif ($results.Count -eq 0) {
    $summaryLines += '- status: `passed`'
    if ($fallbackUsed) {
        if ($preferFallbackScanner) {
            $summaryLines += '- detail: Windows uses the repo-owned fallback scanner because the native Semgrep runtime is currently replaced on this platform.'
        } else {
            $summaryLines += '- detail: primary Semgrep tool failed in current environment; repo-owned fallback scanner produced the blocking result set.'
        }
    }
} else {
    $summaryLines += '- status: `failed`'
    if ($fallbackUsed) {
        $summaryLines += ''
        if ($preferFallbackScanner) {
            $summaryLines += '- detail: Windows uses the repo-owned fallback scanner because the native Semgrep runtime is currently replaced on this platform.'
        } else {
            $summaryLines += '- detail: primary Semgrep tool failed in current environment; repo-owned fallback scanner produced the blocking result set.'
        }
    }
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
        Write-Warning "Semgrep execution failed; continuing because SoftFail is set (exit=$semgrepExitCode)."
        exit 0
    }
    exit $semgrepExitCode
}

if ($results.Count -gt 0 -and -not $SoftFail) {
    exit 1
}

exit 0
