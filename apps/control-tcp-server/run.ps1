param(
    [switch]$DryRun,
    [switch]$UseLegacyFallback,
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
    (Join-Path $controlAppsBuildRoot "bin\siligen_tcp_server.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Debug\siligen_tcp_server.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Release\siligen_tcp_server.exe"),
    (Join-Path $controlAppsBuildRoot "bin\RelWithDebInfo\siligen_tcp_server.exe")
)

$canonicalResolved = $canonicalCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($UseLegacyFallback) {
    $reason = "control-tcp-server completed canonical binary cutover; run.ps1 no longer supports -UseLegacyFallback. Build '$controlAppsBuildRoot\\bin\\siligen_tcp_server.exe' from workspace root first."
    if ($DryRun) {
        Write-Output "control-tcp-server target: BLOCKED"
        Write-Output "reason: $reason"
        exit 1
    }

    Write-Error $reason
}

if ($DryRun) {
    if ($canonicalResolved) {
        Write-Output "control-tcp-server target: $canonicalResolved"
        Write-Output "source: canonical"
        exit 0
    }

    Write-Output "control-tcp-server target: BLOCKED"
    Write-Output "reason: canonical siligen_tcp_server.exe not found. Build '$controlAppsBuildRoot\\bin\\siligen_tcp_server.exe' from workspace root first."
    exit 1
}

if ($canonicalResolved) {
    & $canonicalResolved @AppArgs
    exit $LASTEXITCODE
}

Write-Error "Canonical siligen_tcp_server.exe not found. Build '$controlAppsBuildRoot\\bin\\siligen_tcp_server.exe' from workspace root first."
