param(
    [switch]$DryRun,
    [ValidateSet("online", "offline")]
    [string]$Mode = "online",
    [string]$GatewayExe,
    [string]$GatewayLaunchSpec,
    [switch]$DisableGatewayAutostart,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$projectRoot = $PSScriptRoot
$runner = Join-Path $projectRoot "scripts\\run.ps1"

if ($DryRun) {
    if (Test-Path $runner) {
        Write-Output "hmi-app target: $runner"
        if ($GatewayExe) {
            Write-Output "gateway exe: $GatewayExe"
        }
        Write-Output "mode: $Mode"
        if ($GatewayLaunchSpec) {
            Write-Output "gateway spec: $GatewayLaunchSpec"
        }
        if ($DisableGatewayAutostart) {
            Write-Output "gateway autostart: disabled"
        }
        exit 0
    }

    Write-Output "hmi-app target: BLOCKED"
    Write-Output "reason: 未找到 canonical hmi-app run 脚本。"
    exit 0
}

if (-not (Test-Path $runner)) {
    Write-Error "未找到 canonical hmi-app run 脚本: $runner"
}

if ($GatewayExe) {
    $env:SILIGEN_GATEWAY_EXE = [System.IO.Path]::GetFullPath($GatewayExe)
}

if ($GatewayLaunchSpec) {
    $env:SILIGEN_GATEWAY_LAUNCH_SPEC = [System.IO.Path]::GetFullPath($GatewayLaunchSpec)
}

if ($DisableGatewayAutostart) {
    $env:SILIGEN_GATEWAY_AUTOSTART = "0"
}

$forwardArgs = @()
$hasExplicitModeArg = $false
foreach ($arg in $AppArgs) {
    if ($arg -eq "--mode") {
        $hasExplicitModeArg = $true
        break
    }
}
if (-not $hasExplicitModeArg) {
    $forwardArgs += @("--mode", $Mode)
}
if ($AppArgs) {
    $forwardArgs += $AppArgs
}

& $runner @forwardArgs
