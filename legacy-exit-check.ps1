param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [string]$ReportDir = "tests\\reports\\legacy-exit",
    [switch]$IncludeDocs
)

$ErrorActionPreference = "Stop"

$workspaceRoot = $PSScriptRoot
$runner = Join-Path $workspaceRoot "scripts\\migration\\legacy-exit-checks.py"
if (-not (Test-Path $runner)) {
    Write-Error "未找到 legacy 退出检查实现：$runner"
}

$resolvedReportDir = if ([System.IO.Path]::IsPathRooted($ReportDir)) {
    [System.IO.Path]::GetFullPath($ReportDir)
}
else {
    [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $ReportDir))
}
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

$arguments = @(
    $runner,
    "--profile",
    $Profile.ToLowerInvariant(),
    "--report-dir",
    $resolvedReportDir
)
if ($IncludeDocs) {
    $arguments += "--include-docs"
}

python @arguments

if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}
