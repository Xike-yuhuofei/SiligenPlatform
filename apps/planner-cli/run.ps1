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

function Get-PlannerCliSearchRoots {
    param(
        [string]$WorkspaceRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
        return @([System.IO.Path]::GetFullPath($env:SILIGEN_CONTROL_APPS_BUILD_ROOT))
    }

    $roots = @()
    $workspaceBuildToken = Get-WorkspaceBuildToken -WorkspaceRoot $WorkspaceRoot
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        $roots += [System.IO.Path]::GetFullPath((Join-Path (Join-Path $env:LOCALAPPDATA "SS") ("cab-" + $workspaceBuildToken)))
        $roots += [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"))
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
