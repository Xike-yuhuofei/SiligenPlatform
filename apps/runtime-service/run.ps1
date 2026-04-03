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

function Get-WorkspaceBuildToken {
    param([string]$WorkspaceRoot)

    $normalizedRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot).ToLowerInvariant()
    $bytes = [System.Text.Encoding]::UTF8.GetBytes($normalizedRoot)
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash($bytes)
    } finally {
        $sha256.Dispose()
    }

    return -join ($hashBytes[0..5] | ForEach-Object { $_.ToString("x2") })
}

function Test-BuildRootMatchesWorkspace {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    $cacheFile = Join-Path $BuildRoot "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return $true
    }

    $homeDirectoryLine = Get-Content $cacheFile | Where-Object { $_ -like "CMAKE_HOME_DIRECTORY:*" } | Select-Object -First 1
    if (-not $homeDirectoryLine) {
        return $true
    }

    $configuredSourceRoot = ($homeDirectoryLine -split "=", 2)[1]
    if ([string]::IsNullOrWhiteSpace($configuredSourceRoot)) {
        return $true
    }

    $resolvedConfiguredSourceRoot = [System.IO.Path]::GetFullPath($configuredSourceRoot)
    $resolvedWorkspaceSourceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)
    return $resolvedConfiguredSourceRoot -ieq $resolvedWorkspaceSourceRoot
}

function Get-WorkspaceCabBuildRoots {
    param([string]$WorkspaceRoot)

    if ([string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        return @()
    }

    $ssRoot = Join-Path $env:LOCALAPPDATA "SS"
    if (-not (Test-Path $ssRoot)) {
        return @()
    }

    $roots = @()
    foreach ($candidate in Get-ChildItem -Path $ssRoot -Directory -Filter "cab-*") {
        $resolved = [System.IO.Path]::GetFullPath($candidate.FullName)
        if (Test-BuildRootMatchesWorkspace -BuildRoot $resolved -WorkspaceRoot $WorkspaceRoot) {
            $roots += $resolved
        }
    }

    return @($roots | Select-Object -Unique)
}

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

function Get-RuntimeServiceSearchRoots {
    param(
        [string]$WorkspaceRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
        return @(Resolve-FullPath -PathValue $env:SILIGEN_CONTROL_APPS_BUILD_ROOT)
    }

    $roots = @()
    $roots += [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build\control-apps"))
    $roots += [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build"))
    $roots += Get-WorkspaceCabBuildRoots -WorkspaceRoot $WorkspaceRoot
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $legacyRoot = [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"))
        if (Test-BuildRootMatchesWorkspace -BuildRoot $legacyRoot -WorkspaceRoot $WorkspaceRoot) {
            $roots += $legacyRoot
        }
    }

    return @(
        $roots |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Select-Object -Unique
    )
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

$controlAppsBuildRoots = Get-RuntimeServiceSearchRoots -WorkspaceRoot $workspaceRoot
$candidateRelativePaths = @(
    "bin\$BuildConfig\siligen_runtime_service.exe",
    "bin\siligen_runtime_service.exe",
    "bin\Debug\siligen_runtime_service.exe",
    "bin\Release\siligen_runtime_service.exe",
    "bin\RelWithDebInfo\siligen_runtime_service.exe"
)

$exePath = $null
foreach ($controlAppsBuildRoot in $controlAppsBuildRoots) {
    foreach ($relativePath in $candidateRelativePaths) {
        $candidate = Join-Path $controlAppsBuildRoot $relativePath
        if (Test-Path $candidate) {
            $exePath = $candidate
            break
        }
    }
    if ($exePath) {
        break
    }
}

if (-not $exePath) {
    $rootsText = ($controlAppsBuildRoots -join "', '")
    throw "runtime-service executable not found under any configured root: '$rootsText'"
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
