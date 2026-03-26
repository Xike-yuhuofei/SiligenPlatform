[CmdletBinding()]
param(
    [string]$GatewayExe,
    [string]$OutputPath,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildConfig = "Debug",
    [switch]$DryRun
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$workspaceRoot = Split-Path (Split-Path $projectRoot -Parent) -Parent
$runtimeGatewayRunner = Join-Path $workspaceRoot "apps\runtime-gateway\run.ps1"

function Resolve-GatewayBuildRootFromExecutable {
    param(
        [string]$ResolvedGatewayExe
    )

    if ([string]::IsNullOrWhiteSpace($ResolvedGatewayExe)) {
        return $null
    }

    $exeDirectory = Split-Path $ResolvedGatewayExe -Parent
    if ([string]::IsNullOrWhiteSpace($exeDirectory)) {
        return $null
    }

    $directoryLeaf = Split-Path $exeDirectory -Leaf
    if ($directoryLeaf -in @("Debug", "Release", "RelWithDebInfo")) {
        $binDirectory = Split-Path $exeDirectory -Parent
        if ((Split-Path $binDirectory -Leaf) -eq "bin") {
            return (Split-Path $binDirectory -Parent)
        }
    }

    if ($directoryLeaf -eq "bin") {
        return (Split-Path $exeDirectory -Parent)
    }

    return $null
}

function Parse-RuntimeGatewayDryRunMetadata {
    param(
        [object[]]$DryRunOutput
    )

    $metadata = @{
        Executable = $null
        ConfigPath = $null
        VendorDir = $null
    }

    foreach ($line in @($DryRunOutput)) {
        $text = [string]$line
        foreach ($match in [regex]::Matches(
            $text,
            '(?:^|\s)(?<key>exe|config|vendor_dir|preflight_skipped|log)=(?<value>.+?)(?=(?:\s+(?:exe|config|vendor_dir|preflight_skipped|log)=|$))'
        )) {
            $key = $match.Groups["key"].Value
            $value = $match.Groups["value"].Value.Trim()
            switch ($key) {
                "exe" {
                    $metadata.Executable = [System.IO.Path]::GetFullPath($value)
                }
                "config" {
                    $metadata.ConfigPath = [System.IO.Path]::GetFullPath($value)
                }
                "vendor_dir" {
                    $metadata.VendorDir = [System.IO.Path]::GetFullPath($value)
                }
            }
        }
    }

    return $metadata
}

function Resolve-GatewayRuntimeMetadata {
    param(
        [string]$ExplicitGatewayExe,
        [string]$RuntimeGatewayRunnerPath,
        [string]$ConfigName
    )

    if (-not (Test-Path $RuntimeGatewayRunnerPath)) {
        throw "Canonical runtime-gateway runner not found: $RuntimeGatewayRunnerPath"
    }

    $resolvedExplicitGatewayExe = if (-not [string]::IsNullOrWhiteSpace($ExplicitGatewayExe)) {
        [System.IO.Path]::GetFullPath($ExplicitGatewayExe)
    } else {
        $null
    }

    $previousBuildRoot = $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
    try {
        if (-not [string]::IsNullOrWhiteSpace($resolvedExplicitGatewayExe)) {
            $derivedBuildRoot = Resolve-GatewayBuildRootFromExecutable -ResolvedGatewayExe $resolvedExplicitGatewayExe
            if (-not [string]::IsNullOrWhiteSpace($derivedBuildRoot)) {
                $env:SILIGEN_CONTROL_APPS_BUILD_ROOT = $derivedBuildRoot
            }
        }

        $dryRunOutput = & $RuntimeGatewayRunnerPath -DryRun -BuildConfig $ConfigName
    } finally {
        if ([string]::IsNullOrWhiteSpace($previousBuildRoot)) {
            Remove-Item Env:SILIGEN_CONTROL_APPS_BUILD_ROOT -ErrorAction SilentlyContinue
        } else {
            $env:SILIGEN_CONTROL_APPS_BUILD_ROOT = $previousBuildRoot
        }
    }

    $metadata = Parse-RuntimeGatewayDryRunMetadata -DryRunOutput @($dryRunOutput)
    if ([string]::IsNullOrWhiteSpace($metadata.Executable)) {
        throw "Canonical runtime-gateway dry-run output missing executable path."
    }

    if (-not [string]::IsNullOrWhiteSpace($resolvedExplicitGatewayExe)) {
        $metadata.Executable = $resolvedExplicitGatewayExe
    }

    return $metadata
}

$runtimeMetadata = Resolve-GatewayRuntimeMetadata `
    -ExplicitGatewayExe $GatewayExe `
    -RuntimeGatewayRunnerPath $runtimeGatewayRunner `
    -ConfigName $BuildConfig

$resolvedGatewayExe = $runtimeMetadata.Executable

if (-not (Test-Path $resolvedGatewayExe)) {
    throw "Gateway executable not found: $resolvedGatewayExe"
}

$resolvedOutputPath = if ([string]::IsNullOrWhiteSpace($OutputPath)) {
    Join-Path ([System.IO.Path]::GetTempPath()) ("siligen-hmi-gateway-launch-{0}.json" -f [guid]::NewGuid().ToString("N"))
} else {
    [System.IO.Path]::GetFullPath($OutputPath)
}

$gatewayHost = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_TCP_SERVER_HOST)) {
    $env:SILIGEN_TCP_SERVER_HOST
} else {
    "127.0.0.1"
}

$gatewayPort = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_TCP_SERVER_PORT)) {
    $env:SILIGEN_TCP_SERVER_PORT
} else {
    "9527"
}

$pathEntries = @((Split-Path $resolvedGatewayExe -Parent))
if (-not [string]::IsNullOrWhiteSpace($runtimeMetadata.VendorDir)) {
    $pathEntries += $runtimeMetadata.VendorDir
}
$pathEntries = @(
    $pathEntries |
        Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
        ForEach-Object { [System.IO.Path]::GetFullPath($_) } |
        Select-Object -Unique
)

if ($DryRun) {
    Write-Output "DRYRUN contract=generated exe=$resolvedGatewayExe output=$resolvedOutputPath cwd=$workspaceRoot"
    exit 0
}

$launchArgs = @()
if (-not [string]::IsNullOrWhiteSpace($runtimeMetadata.ConfigPath)) {
    $launchArgs += @("--config", $runtimeMetadata.ConfigPath)
}

$envMap = @{
    SILIGEN_TCP_SERVER_HOST = $gatewayHost
    SILIGEN_TCP_SERVER_PORT = "$gatewayPort"
}
if (-not [string]::IsNullOrWhiteSpace($runtimeMetadata.VendorDir)) {
    $envMap.SILIGEN_MULTICARD_VENDOR_DIR = $runtimeMetadata.VendorDir
}

$payload = @{
    executable  = $resolvedGatewayExe
    cwd         = $workspaceRoot
    args        = $launchArgs
    pathEntries = $pathEntries
    env         = $envMap
}
$json = $payload | ConvertTo-Json -Depth 4
$parentDir = Split-Path $resolvedOutputPath -Parent
if (-not [string]::IsNullOrWhiteSpace($parentDir) -and -not (Test-Path $parentDir)) {
    New-Item -ItemType Directory -Path $parentDir -Force | Out-Null
}
[System.IO.File]::WriteAllText($resolvedOutputPath, $json, [System.Text.UTF8Encoding]::new($false))
Write-Output $resolvedOutputPath
