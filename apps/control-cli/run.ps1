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
    Write-Output "reason: 未找到 canonical siligen_cli.exe。请先执行根级 build 生成 '$controlAppsBuildRoot\bin\siligen_cli.exe'。"
    exit 1
}

if ($canonicalResolved) {
    & $canonicalResolved @AppArgs
    exit $LASTEXITCODE
}

Write-Error "未找到可运行的 canonical siligen_cli.exe。请先执行根级 build 生成 '$controlAppsBuildRoot\bin\siligen_cli.exe'。"
