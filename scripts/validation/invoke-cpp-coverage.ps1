[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\coverage\cpp",
    [string]$ThresholdConfigPath = "scripts\validation\coverage-thresholds.json",
    [string]$RawProfileDir = "",
    [switch]$FailOnThreshold
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "tooling-common.ps1")

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$resolvedReportDir = Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ReportDir)
$resolvedThresholdConfigPath = Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $ThresholdConfigPath
$resolvedRawProfileDir = if ([string]::IsNullOrWhiteSpace($RawProfileDir)) {
    Ensure-WorkspaceDirectory -Path (Join-Path $resolvedReportDir "raw")
} else {
    Ensure-WorkspaceDirectory -Path (Resolve-WorkspaceReportPath -WorkspaceRoot $workspaceRoot -ReportPath $RawProfileDir)
}

if (-not (Test-Path $resolvedThresholdConfigPath)) {
    throw "未找到 coverage threshold 配置: $resolvedThresholdConfigPath"
}

$llvmCov = Resolve-WorkspaceToolPath -ToolNames @("llvm-cov") -Required
$llvmProfdata = Resolve-WorkspaceToolPath -ToolNames @("llvm-profdata") -Required
$thresholds = Get-Content -LiteralPath $resolvedThresholdConfigPath -Raw | ConvertFrom-Json
$cppThresholds = $thresholds.cpp
$buildRoot = Get-ControlAppsBuildRoot -WorkspaceRoot $workspaceRoot
$mergedProfilePath = Join-Path $resolvedReportDir "coverage.profdata"
$summaryJsonPath = Join-Path $resolvedReportDir "coverage-summary.json"
$reportTextPath = Join-Path $resolvedReportDir "coverage-report.txt"
$lcovPath = Join-Path $resolvedReportDir "coverage.lcov"
$mdReportPath = Join-Path $resolvedReportDir "coverage-summary.md"

$profrawFiles = @(Get-ChildItem -LiteralPath $resolvedRawProfileDir -Filter *.profraw -File -ErrorAction SilentlyContinue)
$binaries = @(Get-ChildItem -Path (Join-Path $buildRoot "bin") -Filter *.exe -File -Recurse -ErrorAction SilentlyContinue)

$mdLines = @(
    '# C/C++ Source-Based Coverage',
    '',
    '- gate: `report-only`',
    '- failure_condition: `when FailOnThreshold is enabled and a hard-fail threshold is missed`',
    '- raw_profile_dir: `tests/reports/coverage/cpp/raw`',
    '- profdata: `tests/reports/coverage/cpp/coverage.profdata`',
    '- summary_json: `tests/reports/coverage/cpp/coverage-summary.json`',
    '- text_report: `tests/reports/coverage/cpp/coverage-report.txt`',
    '- lcov_report: `tests/reports/coverage/cpp/coverage.lcov`',
    ''
)

if ($profrawFiles.Count -eq 0) {
    $mdLines += '- status: `not-collected`'
    $mdLines += '- detail: 未发现 `.profraw` 文件。请先使用 `build.ps1 -EnableCppCoverage` 构建，再通过 `test.ps1 -EnableCppCoverage` 收集运行数据。'
    Set-Content -LiteralPath $mdReportPath -Value ($mdLines -join "`r`n") -Encoding UTF8
    Write-Output "cpp coverage raw profiles not found: $resolvedRawProfileDir"
    exit 0
}

if ($binaries.Count -eq 0) {
    $mdLines += '- status: `not-collected`'
    $mdLines += "- detail: 未在 build root 下找到可用于 llvm-cov 的可执行文件。"
    Set-Content -LiteralPath $mdReportPath -Value ($mdLines -join "`r`n") -Encoding UTF8
    Write-Output "cpp coverage binaries not found under: $buildRoot"
    exit 0
}

$mergeArgs = @(
    "merge",
    "--sparse",
    "--output",
    $mergedProfilePath
) + @($profrawFiles | ForEach-Object { $_.FullName })
Write-Output "cpp coverage merge: $llvmProfdata $($mergeArgs -join ' ')"
& $llvmProfdata @mergeArgs
if ($LASTEXITCODE -ne 0) {
    throw "llvm-profdata merge 失败，退出码: $LASTEXITCODE"
}

$primaryBinary = $binaries[0].FullName
$otherBinaryArgs = @()
foreach ($binary in $binaries | Select-Object -Skip 1) {
    $otherBinaryArgs += @("--object", $binary.FullName)
}
$ignoreRegex = "third_party|tests[/\\]|build[/\\]_deps|googletest"

$reportArgs = @(
    "report",
    $primaryBinary,
    "--instr-profile=$mergedProfilePath",
    "--ignore-filename-regex=$ignoreRegex",
    "--show-branch-summary"
) + $otherBinaryArgs
Write-Output "cpp coverage report: $llvmCov $($reportArgs -join ' ')"
$reportOutput = & $llvmCov @reportArgs
if ($LASTEXITCODE -ne 0) {
    throw "llvm-cov report 失败，退出码: $LASTEXITCODE"
}
Set-Content -LiteralPath $reportTextPath -Value (($reportOutput | Out-String).Trim()) -Encoding UTF8

$exportSummaryArgs = @(
    "export",
    $primaryBinary,
    "--instr-profile=$mergedProfilePath",
    "--ignore-filename-regex=$ignoreRegex",
    "--summary-only"
) + $otherBinaryArgs
$summaryOutput = & $llvmCov @exportSummaryArgs
if ($LASTEXITCODE -ne 0) {
    throw "llvm-cov export(summary) 失败，退出码: $LASTEXITCODE"
}
Set-Content -LiteralPath $summaryJsonPath -Value (($summaryOutput | Out-String).Trim()) -Encoding UTF8

$lcovArgs = @(
    "export",
    $primaryBinary,
    "--instr-profile=$mergedProfilePath",
    "--ignore-filename-regex=$ignoreRegex",
    "--format=lcov"
) + $otherBinaryArgs
$lcovOutput = & $llvmCov @lcovArgs
if ($LASTEXITCODE -ne 0) {
    throw "llvm-cov export(lcov) 失败，退出码: $LASTEXITCODE"
}
Set-Content -LiteralPath $lcovPath -Value (($lcovOutput | Out-String).Trim()) -Encoding UTF8

$payload = Get-Content -LiteralPath $summaryJsonPath -Raw | ConvertFrom-Json
$fileEntries = @()
foreach ($item in @($payload.data)) {
    if ($null -ne $item.files) {
        $fileEntries += @($item.files)
    }
}

$globalLinePercent = 0.0
if ($payload.data.Count -gt 0 -and $null -ne $payload.data[0].totals -and $null -ne $payload.data[0].totals.lines) {
    $globalLinePercent = [math]::Round([double]$payload.data[0].totals.lines.percent, 2)
}

$criticalResults = @()
foreach ($entry in $cppThresholds.critical_paths) {
    $prefix = [string]$entry.path_prefix
    $matching = @($fileEntries | Where-Object {
        $name = [string]$_.filename
        $relative = if ($name.StartsWith($workspaceRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            [System.IO.Path]::GetRelativePath($workspaceRoot, $name).Replace('\', '/')
        } else {
            $name.Replace('\', '/')
        }
        $relative -like "$prefix*"
    })
    $covered = 0.0
    $count = 0.0
    foreach ($fileEntry in $matching) {
        $covered += [double]$fileEntry.summary.lines.covered
        $count += [double]$fileEntry.summary.lines.count
    }
    $actual = if ($count -gt 0) { [math]::Round(($covered / $count) * 100.0, 2) } else { 0.0 }
    $criticalResults += [PSCustomObject]@{
        path_prefix = $prefix
        actual_line_percent = $actual
        threshold = [double]$entry.line_min
        status = if ($actual -ge [double]$entry.line_min) { "passed" } else { "failed" }
    }
}

$thresholdFailures = @()
if ($globalLinePercent -lt [double]$cppThresholds.global_line_min) {
    $thresholdFailures += "global line coverage $globalLinePercent% < $($cppThresholds.global_line_min)%"
}
foreach ($result in $criticalResults) {
    if ($result.status -eq "failed") {
        $thresholdFailures += "$($result.path_prefix) line coverage $($result.actual_line_percent)% < $($result.threshold)%"
    }
}

$mdLines += '- status: `collected`'
$mdLines += ('- global_line_percent: `{0}`' -f $globalLinePercent)
$mdLines += ('- global_line_min: `{0}`' -f [double]$cppThresholds.global_line_min)
$mdLines += ('- threshold_enforcement: `{0}`' -f [string]$cppThresholds.enforcement)
$mdLines += ''
$mdLines += '| path_prefix | actual_line_percent | threshold | status |'
$mdLines += '|---|---:|---:|---|'
foreach ($result in $criticalResults) {
    $mdLines += "| $([string]$result.path_prefix) | $([double]$result.actual_line_percent) | $([double]$result.threshold) | $([string]$result.status) |"
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

Write-Output "cpp coverage report: $reportTextPath"
Write-Output "cpp coverage summary json: $summaryJsonPath"
Write-Output "cpp coverage lcov: $lcovPath"
Write-Output "cpp coverage global line percent: $globalLinePercent"

if ($FailOnThreshold -and $thresholdFailures.Count -gt 0 -and [string]$cppThresholds.enforcement -eq "hard-fail") {
    exit 1
}

exit 0
