[CmdletBinding()]
param(
    [string]$ReportDir = "tests\\reports\\static"
)

$ErrorActionPreference = "Stop"
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\\..")).Path
Set-Location $repoRoot

function Resolve-OutputPath {
    param([string]$PathValue)

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $repoRoot $PathValue))
}

function Resolve-PyrightCommand {
    try {
        $pyright = Get-Command pyright -ErrorAction Stop
        return @($pyright.Source)
    } catch {
        throw "pyright was not found on PATH. Install it before running this gate; pre-push does not use npx fallback or download tools at runtime."
    }
}

function ConvertFrom-PyrightPlainText {
    param(
        [string]$RawText,
        [int]$ExitCode,
        [string]$RawPath
    )

    $diagnostics = @()
    $seenFiles = [System.Collections.Generic.HashSet[string]]::new()
    $currentHeader = ""
    foreach ($line in ($RawText -split "`r?`n")) {
        if ($line -match '^\s*(?<file>[a-zA-Z]:\\.*?\.py)\s*$') {
            $currentHeader = $Matches.file
            [void]$seenFiles.Add($currentHeader)
            continue
        }
        if ($line -match '^\s*(?<file>[a-zA-Z]:\\.*?\.py):(?<line>\d+):(?<column>\d+) - (?<severity>error|warning|information): (?<message>.+)$') {
            $file = $Matches.file
            [void]$seenFiles.Add($file)
            $diagnostics += [pscustomobject]@{
                file = $file
                severity = $Matches.severity
                message = $Matches.message.Trim()
                range = [pscustomobject]@{
                    start = [pscustomobject]@{
                        line = [int]$Matches.line - 1
                        character = [int]$Matches.column - 1
                    }
                }
            }
        }
    }

    $errorCount = @($diagnostics | Where-Object { $_.severity -eq "error" }).Count
    $warningCount = @($diagnostics | Where-Object { $_.severity -eq "warning" }).Count
    $informationCount = @($diagnostics | Where-Object { $_.severity -eq "information" }).Count

    return [pscustomobject]@{
        version = "plain-text-fallback"
        time = (Get-Date).ToString("s")
        generalDiagnostics = $diagnostics
        summary = [pscustomobject]@{
            filesAnalyzed = $seenFiles.Count
            errorCount = $errorCount
            warningCount = $warningCount
            informationCount = $informationCount
            timeInSec = 0
        }
        tool_exit_code = $ExitCode
        raw_output_file = $RawPath
        parse_mode = "plain-text-fallback"
    }
}

$resolvedReportDir = Resolve-OutputPath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

$jsonPath = Join-Path $resolvedReportDir "pyright-report.json"
$mdPath = Join-Path $resolvedReportDir "pyright-report.md"
$rawPath = Join-Path $resolvedReportDir "pyright-report.raw.txt"

$command = @(Resolve-PyrightCommand)
$commandPrefix = @()
if ($command.Count -gt 1) {
    $commandPrefix = @($command[1..($command.Count - 1)])
}
$args = @(
    "--project",
    (Join-Path $repoRoot "pyrightconfig.json"),
    "--outputjson"
)

$output = & $command[0] @commandPrefix @args 2>&1
$exitCode = if ($null -eq $LASTEXITCODE) { 0 } else { $LASTEXITCODE }
$rawText = ($output | Out-String).Trim()
$rawText | Set-Content -Path $rawPath -Encoding UTF8

$parsed = $null
try {
    $parsed = $rawText | ConvertFrom-Json
} catch {
    $parsed = ConvertFrom-PyrightPlainText -RawText $rawText -ExitCode $exitCode -RawPath $rawPath
    if (@($parsed.generalDiagnostics).Count -eq 0) {
        Add-Member -InputObject $parsed -NotePropertyName parse_error -NotePropertyValue $_.Exception.Message
    }
}

if (-not $parsed.PSObject.Properties["tool_exit_code"]) {
    Add-Member -InputObject $parsed -NotePropertyName tool_exit_code -NotePropertyValue $exitCode
}
if (-not $parsed.PSObject.Properties["raw_output_file"]) {
    Add-Member -InputObject $parsed -NotePropertyName raw_output_file -NotePropertyValue $rawPath
}

$parsed | ConvertTo-Json -Depth 100 | Set-Content -Path $jsonPath -Encoding UTF8

$summary = $parsed.summary
$filesAnalyzed = if ($summary) { $summary.filesAnalyzed } else { 0 }
$errorCount = if ($summary) { $summary.errorCount } else { 0 }
$warningCount = if ($summary) { $summary.warningCount } else { 0 }
$infoCount = if ($summary) { $summary.informationCount } else { 0 }
$timeInSec = if ($summary) { $summary.timeInSec } else { 0 }

$commandLine = ($command + $args) -join " "
$lines = @(
    "# Pyright Report",
    "",
    "- command: $commandLine",
    "- exit_code: $exitCode",
    "- files_analyzed: $filesAnalyzed",
    "- error_count: $errorCount",
    "- warning_count: $warningCount",
    "- information_count: $infoCount",
    "- time_in_sec: $timeInSec",
    "- raw_output_file: $rawPath",
    ""
)

$diagnostics = @($parsed.generalDiagnostics)
if ($diagnostics.Count -gt 0) {
    $lines += "| severity | file | line | message |"
    $lines += "|---|---|---:|---|"
    foreach ($diag in $diagnostics) {
        $range = $diag.range
        $start = if ($range -and $range.start) { [int]$range.start.line + 1 } else { 0 }
        $file = if ($diag.file) { $diag.file } else { "" }
        $message = [string]$diag.message
        $message = $message.Replace("`r", " ").Replace("`n", " ")
        $lines += "| $($diag.severity) | $file | $start | $message |"
    }
} else {
    $lines += "- no diagnostics"
}

$lines | Set-Content -Path $mdPath -Encoding UTF8

Write-Host "Pyright report(JSON): $jsonPath"
Write-Host "Pyright report(MD):   $mdPath"

if ($exitCode -ne 0) {
    exit $exitCode
}

exit 0
