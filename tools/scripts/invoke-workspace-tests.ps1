param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "packages", "integration", "protocol-compatibility", "simulation")]
    [string[]]$Suite = @("all"),
    [string]$ReportDir = "integration\\reports",
    [switch]$FailOnKnownFailure,
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$testKitSrc = Join-Path $workspaceRoot "packages\\test-kit\\src"

$resolvedReportDir = if ([System.IO.Path]::IsPathRooted($ReportDir)) {
    [System.IO.Path]::GetFullPath($ReportDir)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $ReportDir))
}

if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = $testKitSrc
} else {
    $env:PYTHONPATH = "$testKitSrc$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}

$argsList = @(
    "-m",
    "test_kit.workspace_validation",
    "--profile",
    $Profile.ToLowerInvariant(),
    "--report-dir",
    $resolvedReportDir
)

foreach ($suiteName in $Suite) {
    $argsList += @("--suite", $suiteName)
}

if ($IncludeHardwareSmoke) {
    $argsList += "--include-hardware-smoke"
}

if ($IncludeHilClosedLoop) {
    $argsList += "--include-hil-closed-loop"
}

if ($FailOnKnownFailure) {
    $argsList += "--fail-on-known-failure"
}

python @argsList
