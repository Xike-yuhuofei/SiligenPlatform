param(
    [switch]$DryRun,
    [switch]$UseLegacyFallback,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$controlCoreRoot = Join-Path $workspaceRoot "control-core"
$controlAppsBuildRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path $workspaceRoot "build\control-apps"
}
$legacyCliSourceCandidate = Join-Path (Split-Path $workspaceRoot -Parent) "Backend_CPP\\src\\adapters\\cli"
$legacyCliSource = if (Test-Path $legacyCliSourceCandidate) {
    $legacyCliSourceCandidate
} else {
    "external Backend_CPP\\src\\adapters\\cli"
}

$canonicalCandidates = @(
    (Join-Path $controlAppsBuildRoot "bin\siligen_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Debug\siligen_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Release\siligen_cli.exe"),
    (Join-Path $controlAppsBuildRoot "bin\RelWithDebInfo\siligen_cli.exe")
)
$legacyCandidates = @(
    (Join-Path $controlCoreRoot "build\bin\siligen_cli.exe"),
    (Join-Path $controlCoreRoot "build\bin\Debug\siligen_cli.exe"),
    (Join-Path $controlCoreRoot "build\bin\Release\siligen_cli.exe"),
    (Join-Path $controlCoreRoot "build\bin\RelWithDebInfo\siligen_cli.exe")
)

$canonicalResolved = $canonicalCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
$legacyResolved = $legacyCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1

if ($DryRun) {
    if ($UseLegacyFallback -and $legacyResolved) {
        Write-Output "control-cli target: $legacyResolved"
        Write-Output "source: TEMPORARY_LEGACY_FALLBACK"
        exit 0
    }

    if ($canonicalResolved) {
        Write-Output "control-cli target: $canonicalResolved"
        Write-Output "source: canonical"
        exit 0
    }

    Write-Output "control-cli target: BLOCKED"
    if ($legacyResolved) {
        Write-Output "reason: canonical 产物缺失；检测到 legacy 产物 '$legacyResolved'，但默认已禁用回退。需要临时回退时请显式传入 -UseLegacyFallback。"
    } else {
        Write-Output "reason: 未找到 canonical siligen_cli.exe。当前最小真实承载面已迁入 apps/control-cli，但若尚未构建，请先执行根级 build 生成 '$controlAppsBuildRoot\bin\siligen_cli.exe'。"
    }
    exit 0
}

if ($UseLegacyFallback -and $legacyResolved) {
    & $legacyResolved @AppArgs
    exit $LASTEXITCODE
}

if ($canonicalResolved) {
    & $canonicalResolved @AppArgs
    exit $LASTEXITCODE
}

if ($legacyResolved) {
    Write-Error "找到 legacy siligen_cli.exe ('$legacyResolved')，但默认已禁用回退。请先执行根级 build 生成 canonical 产物；若必须调用 legacy CLI，请显式传入 -UseLegacyFallback。当前可信迁移源仍为 '$legacyCliSource'。"
}

Write-Error "未找到 canonical siligen_cli.exe。请先执行根级 build 生成 '$controlAppsBuildRoot\bin\siligen_cli.exe'。当前可信迁移源仍为 '$legacyCliSource'。"
