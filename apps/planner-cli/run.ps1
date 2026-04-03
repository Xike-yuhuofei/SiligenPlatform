[CmdletBinding()]
param(
    [switch]$DryRun,
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$BuildConfig = "Debug",
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$ForwardArgs
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

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

function Get-PlannerCliSearchRoots {
    param(
        [string]$WorkspaceRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
        return @([System.IO.Path]::GetFullPath($env:SILIGEN_CONTROL_APPS_BUILD_ROOT))
    }

    $roots = @()
    $roots += Get-WorkspaceCabBuildRoots -WorkspaceRoot $WorkspaceRoot
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $legacyRoot = [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"))
        if (Test-BuildRootMatchesWorkspace -BuildRoot $legacyRoot -WorkspaceRoot $WorkspaceRoot) {
            $roots += $legacyRoot
        }
    }

    $roots += [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build\control-apps"))
    $roots += [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build"))

    return @(
        $roots |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Select-Object -Unique
    )
}

$controlAppsBuildRoots = Get-PlannerCliSearchRoots -WorkspaceRoot $workspaceRoot
$candidateRelativePaths = @(
    "bin\$BuildConfig\siligen_planner_cli.exe",
    "bin\siligen_planner_cli.exe",
    "bin\Debug\siligen_planner_cli.exe",
    "bin\Release\siligen_planner_cli.exe",
    "bin\RelWithDebInfo\siligen_planner_cli.exe"
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
    throw "planner-cli executable not found under any configured root: '$rootsText'"
}

if ($DryRun) {
    Write-Output "DRYRUN app=planner-cli exe=$exePath"
    exit 0
}

& $exePath @ForwardArgs
exit $LASTEXITCODE
