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
. (Join-Path $workspaceRoot "scripts\validation\tooling-common.ps1")

$controlAppsBuildRoots = @(Get-ControlAppsBuildRoots -WorkspaceRoot $workspaceRoot)
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
    if ($controlAppsBuildRoots.Count -eq 0) {
        $searchRoots = @(Get-ControlAppsBuildSearchRoots -WorkspaceRoot $workspaceRoot)
        $searchRootsText = ($searchRoots -join "', '")
        throw "current-workspace control-apps build root not ready for planner-cli; searched: '$searchRootsText'"
    }
    $rootsText = ($controlAppsBuildRoots -join "', '")
    throw "planner-cli executable not found under current-workspace control-apps roots: '$rootsText'"
}

if ($DryRun) {
    Write-Output "DRYRUN app=planner-cli exe=$exePath"
    exit 0
}

& $exePath @ForwardArgs
exit $LASTEXITCODE
