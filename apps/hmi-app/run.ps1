param(
    [switch]$DryRun,
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

& $runner @AppArgs
