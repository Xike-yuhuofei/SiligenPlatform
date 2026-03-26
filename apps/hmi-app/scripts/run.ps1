param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$workspaceRoot = Split-Path (Split-Path $projectRoot -Parent) -Parent
$sourceRoot = Join-Path $projectRoot "src"

function Resolve-LaunchMode {
    param([string[]]$Arguments)

    for ($index = 0; $index -lt $Arguments.Count; $index++) {
        if ($Arguments[$index] -eq "--mode" -and ($index + 1) -lt $Arguments.Count) {
            return $Arguments[$index + 1]
        }
    }

    if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_HMI_MODE)) {
        return $env:SILIGEN_HMI_MODE
    }

    return "online"
}

function Test-GatewayAutostartEnabled {
    $autoStart = $env:SILIGEN_GATEWAY_AUTOSTART
    if ([string]::IsNullOrWhiteSpace($autoStart)) {
        return $true
    }

    $normalized = $autoStart.Trim().ToLowerInvariant()
    return $normalized -notin @("0", "false", "no", "off")
}

if ([string]::IsNullOrWhiteSpace($env:SILIGEN_WORKSPACE_ROOT)) {
    $env:SILIGEN_WORKSPACE_ROOT = $workspaceRoot
}

if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = $sourceRoot
} else {
    $env:PYTHONPATH = "$sourceRoot$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}

$launchMode = Resolve-LaunchMode -Arguments $AppArgs
$officialEntrypoint = $env:SILIGEN_HMI_OFFICIAL_ENTRYPOINT
$hasExplicitGatewayContract = -not [string]::IsNullOrWhiteSpace($env:SILIGEN_GATEWAY_LAUNCH_SPEC)

if (
    $launchMode -eq "online" -and
    (Test-GatewayAutostartEnabled) -and
    [string]::IsNullOrWhiteSpace($officialEntrypoint) -and
    (-not $hasExplicitGatewayContract)
) {
    throw "Direct online launch via apps/hmi-app/scripts/run.ps1 is blocked. Use apps/hmi-app/run.ps1 or set SILIGEN_GATEWAY_LAUNCH_SPEC."
}

python -m hmi_client.main @AppArgs
