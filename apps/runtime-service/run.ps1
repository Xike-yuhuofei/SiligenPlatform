[CmdletBinding()]
param(
    [switch]$DryRun,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildConfig = "Debug",
    [string]$ConfigPath,
    [string]$VendorDir,
    [switch]$SkipPreflight,
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ForwardArgs
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$canonicalConfigPath = Join-Path $workspaceRoot "config\machine\machine_config.ini"
$defaultVendorDir = Join-Path $workspaceRoot "modules\runtime-execution\adapters\device\vendor\multicard"

function Resolve-FullPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

function Get-IniValue {
    param(
        [Parameter(Mandatory = $true)]
        [string]$IniPath,
        [Parameter(Mandatory = $true)]
        [string]$Section,
        [Parameter(Mandatory = $true)]
        [string]$Key
    )

    $currentSection = ""
    foreach ($line in Get-Content -Path $IniPath) {
        $trimmed = $line.Trim()
        if (($trimmed.Length -eq 0) -or $trimmed.StartsWith("#") -or $trimmed.StartsWith(";")) {
            continue
        }

        if ($trimmed -match '^\[(.+)\]$') {
            $currentSection = $Matches[1]
            continue
        }

        if (($currentSection -eq $Section) -and ($trimmed -match '^([^=]+)=(.*)$')) {
            if ($Matches[1].Trim() -eq $Key) {
                return $Matches[2].Trim()
            }
        }
    }

    return $null
}

function Invoke-Preflight {
    param(
        [Parameter(Mandatory = $true)]
        [string]$ResolvedConfigPath,
        [Parameter(Mandatory = $true)]
        [string]$ResolvedVendorDir
    )

    if (-not (Test-Path $ResolvedConfigPath)) {
        throw "canonical machine config not found: '$ResolvedConfigPath'"
    }

    if (-not (Test-Path $ResolvedVendorDir)) {
        throw "MultiCard vendor directory not found: '$ResolvedVendorDir'"
    }

    $hardwareMode = Get-IniValue -IniPath $ResolvedConfigPath -Section "Hardware" -Key "mode"
    if ($hardwareMode -eq "Real") {
        $requiredRuntimeAsset = Join-Path $ResolvedVendorDir "MultiCard.dll"
        if (-not (Test-Path $requiredRuntimeAsset)) {
            throw "Hardware.mode=Real but required runtime asset is missing: '$requiredRuntimeAsset'"
        }

        if (-not (Test-Path (Join-Path $ResolvedVendorDir "MultiCard.lib"))) {
            Write-Warning "Hardware.mode=Real and MultiCard.lib is missing under '$ResolvedVendorDir'. Runtime may start if the executable is already built, but rebuilding real-hardware binaries still requires the vendor import library."
        }
    }
}

$resolvedConfigPath = if ([string]::IsNullOrWhiteSpace($ConfigPath)) {
    $canonicalConfigPath
} else {
    Resolve-FullPath -PathValue $ConfigPath
}

$resolvedVendorDir = if (-not [string]::IsNullOrWhiteSpace($VendorDir)) {
    Resolve-FullPath -PathValue $VendorDir
} elseif (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_MULTICARD_VENDOR_DIR)) {
    Resolve-FullPath -PathValue $env:SILIGEN_MULTICARD_VENDOR_DIR
} else {
    $defaultVendorDir
}

if (-not $SkipPreflight) {
    Invoke-Preflight -ResolvedConfigPath $resolvedConfigPath -ResolvedVendorDir $resolvedVendorDir
}

$controlAppsBuildRoot = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path $workspaceRoot "build\control-apps"
}

$candidates = @(
    (Join-Path $controlAppsBuildRoot "bin\$BuildConfig\siligen_runtime_service.exe"),
    (Join-Path $controlAppsBuildRoot "bin\siligen_runtime_service.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Debug\siligen_runtime_service.exe"),
    (Join-Path $controlAppsBuildRoot "bin\Release\siligen_runtime_service.exe"),
    (Join-Path $controlAppsBuildRoot "bin\RelWithDebInfo\siligen_runtime_service.exe")
)

$exePath = $candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exePath) {
    throw "runtime-service executable not found under '$controlAppsBuildRoot'"
}

$env:SILIGEN_MULTICARD_VENDOR_DIR = $resolvedVendorDir
$runtimeDllPath = Join-Path $resolvedVendorDir "MultiCard.dll"
if (Test-Path $runtimeDllPath) {
    $env:PATH = "$resolvedVendorDir;$env:PATH"
}

if ($DryRun) {
    Write-Output "DRYRUN app=runtime-service exe=$exePath config=$resolvedConfigPath vendor_dir=$resolvedVendorDir preflight_skipped=$SkipPreflight log=logs/control_runtime.log"
    exit 0
}

& $exePath --config $resolvedConfigPath @ForwardArgs
exit $LASTEXITCODE
