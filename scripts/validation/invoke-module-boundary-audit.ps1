[CmdletBinding()]
param(
    [ValidateSet("pr", "full", "nightly")]
    [string]$Mode = "pr",
    [string]$ReportDir = "tests/reports/module-boundary-audit",
    [string[]]$ChangedFile = @(),
    [string]$WorkspaceRoot = "",
    [string]$PythonExe = "python"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
} else {
    $WorkspaceRoot = (Resolve-Path $WorkspaceRoot).Path
}

if ([System.IO.Path]::IsPathRooted($ReportDir)) {
    $resolvedReportDir = $ReportDir
} else {
    $resolvedReportDir = Join-Path $WorkspaceRoot $ReportDir
}

New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

$runner = Join-Path $PSScriptRoot "boundary_audit\module_boundary_audit.py"
$boundaryFile = Join-Path $PSScriptRoot "boundaries\module-boundaries.json"
$bridgeFile = Join-Path $PSScriptRoot "boundaries\bridge-registry.json"
$policyFile = Join-Path $PSScriptRoot "boundaries\boundary-policy.json"

foreach ($required in @($runner, $boundaryFile, $bridgeFile, $policyFile)) {
    if (-not (Test-Path -LiteralPath $required)) {
        throw "Module Boundary Audit required file missing: $required"
    }
}

$expandedChangedFiles = @()
foreach ($item in $ChangedFile) {
    if ([string]::IsNullOrWhiteSpace($item)) {
        continue
    }
    foreach ($part in ($item -split ",")) {
        $trimmed = $part.Trim()
        if (-not [string]::IsNullOrWhiteSpace($trimmed)) {
            $expandedChangedFiles += $trimmed
        }
    }
}

$arguments = @(
    $runner,
    "--mode",
    $Mode,
    "--workspace-root",
    $WorkspaceRoot,
    "--report-dir",
    $resolvedReportDir,
    "--boundary-file",
    $boundaryFile,
    "--bridge-file",
    $bridgeFile,
    "--policy-file",
    $policyFile
)

foreach ($file in $expandedChangedFiles) {
    $arguments += @("--changed-file", $file)
}

& $PythonExe @arguments
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0) {
    exit $exitCode
}

$requiredArtifacts = @(
    (Join-Path $resolvedReportDir "module-boundary-audit.json"),
    (Join-Path $resolvedReportDir "module-boundary-audit.md"),
    (Join-Path $resolvedReportDir "module-boundary-coverage.json"),
    (Join-Path $resolvedReportDir "bridge-registry-status.json"),
    (Join-Path $resolvedReportDir "logs\model-loader.log"),
    (Join-Path $resolvedReportDir "logs\dependency-extractor.log"),
    (Join-Path $resolvedReportDir "logs\policy-evaluator.log")
)

foreach ($artifact in $requiredArtifacts) {
    if (-not (Test-Path -LiteralPath $artifact)) {
        throw "Module Boundary Audit did not publish expected artifact: $artifact"
    }
}

exit 0
