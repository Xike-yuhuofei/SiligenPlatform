param(
    [switch]$DryRun,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$controlAppsBuildRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path $workspaceRoot "build\control-apps"
}

$canonicalCandidates = @(
    (Join-Path $controlAppsBuildRoot "bin\siligen_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Debug\siligen_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Release\siligen_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\RelWithDebInfo\siligen_cli.exe")
)

$canonicalResolved = $canonicalCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($DryRun) {
    if ($canonicalResolved) {
        Write-Output "control-cli target: $canonicalResolved"
        Write-Output "source: canonical"
        exit 0
    }

    Write-Output "control-cli target: BLOCKED"
    Write-Output "reason: canonical siligen_cli.exe not found. Build '$controlAppsBuildRoot\\bin\\siligen_cli.exe' from workspace root first."
    exit 1
}

if ($canonicalResolved) {
    & $canonicalResolved @AppArgs
    exit $LASTEXITCODE
}

Write-Error "No runnable canonical siligen_cli.exe found. Build '$controlAppsBuildRoot\\bin\\siligen_cli.exe' from workspace root first."
