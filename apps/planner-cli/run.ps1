[CmdletBinding()]
param(
    [switch]$DryRun,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildConfig = "Debug",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ForwardArgs
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path $PSScriptRoot -Parent
$controlAppsBuildRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path $workspaceRoot "build\control-apps"
}

$candidates = @(
    (Join-Path $controlAppsBuildRoot "bin\$BuildConfig\siligen_planner_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\siligen_planner_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Debug\siligen_planner_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Release\siligen_planner_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\RelWithDebInfo\siligen_planner_cli.exe")
)

$exePath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exePath) {
    throw "planner-cli executable not found under '$controlAppsBuildRoot'"
}

if ($DryRun) {
    Write-Output "DRYRUN app=planner-cli exe=$exePath"
    exit 0
}

& $exePath @ForwardArgs
exit $LASTEXITCODE