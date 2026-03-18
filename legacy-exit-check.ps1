param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "integration\\reports\\legacy-exit"
)

$ErrorActionPreference = "Stop"

$workspaceRoot = $PSScriptRoot
$runner = Join-Path $workspaceRoot "tools\\scripts\\legacy_exit_checks.py"
if (-not (Test-Path $runner)) {
    Write-Error "未找到 legacy 退出检查实现：$runner"
}

$resolvedReportDir = Join-Path $workspaceRoot $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

python $runner `
    --profile $Profile.ToLowerInvariant() `
    --report-dir $resolvedReportDir

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
