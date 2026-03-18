param(
    [switch]$DryRun,
    [string]$GatewayExe,
    [string]$GatewayLaunchSpec,
    [switch]$DisableGatewayAutostart,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$canonicalRunner = Join-Path (Split-Path $projectRoot -Parent) "apps\\hmi-app\\run.ps1"

if (-not (Test-Path $canonicalRunner)) {
    Write-Error "未找到 canonical hmi-app 入口: $canonicalRunner"
}

$namedArgs = @{}
if ($DryRun) {
    $namedArgs["DryRun"] = $true
}
if ($GatewayExe) {
    $namedArgs["GatewayExe"] = $GatewayExe
}
if ($GatewayLaunchSpec) {
    $namedArgs["GatewayLaunchSpec"] = $GatewayLaunchSpec
}
if ($DisableGatewayAutostart) {
    $namedArgs["DisableGatewayAutostart"] = $true
}

& $canonicalRunner @namedArgs @AppArgs
