[CmdletBinding()]
param(
    [string]$WorkspaceRoot = "",
    [string]$BuildRoot = "build\\ca"
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
}
$resolvedWorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)
$resolvedBuildRoot = if ([System.IO.Path]::IsPathRooted($BuildRoot)) {
    [System.IO.Path]::GetFullPath($BuildRoot)
} else {
    [System.IO.Path]::GetFullPath((Join-Path $resolvedWorkspaceRoot $BuildRoot))
}
$canonicalBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedWorkspaceRoot "build\ca"))

if ($resolvedBuildRoot -ne $canonicalBuildRoot) {
    throw "Strict native build cache only supports canonical build root '$canonicalBuildRoot'; actual '$resolvedBuildRoot'."
}

$gitBaseArgs = @("-c", "safe.directory=$resolvedWorkspaceRoot", "-C", $resolvedWorkspaceRoot)

$trackedStatus = @(& git @gitBaseArgs status --porcelain --untracked-files=no)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect strict native workspace tracked-file status."
}
if ($trackedStatus.Count -gt 0) {
    throw "Strict native workspace has tracked-file residue before validation: $($trackedStatus -join '; ')"
}

$untrackedStatus = @(& git @gitBaseArgs status --porcelain --untracked-files=all)
if ($LASTEXITCODE -ne 0) {
    throw "Unable to inspect strict native workspace untracked-file status."
}
$unexpectedUntracked = @(
    $untrackedStatus |
        Where-Object { $_.StartsWith("?? ") } |
        ForEach-Object { $_.Substring(3) -replace '\\', '/' } |
        Where-Object {
            -not (
                $_.StartsWith("build/") -or
                $_.StartsWith("tests/reports/") -or
                $_.StartsWith(".pytest_cache/")
            )
        }
)
if ($unexpectedUntracked.Count -gt 0) {
    throw "Strict native workspace has unexpected untracked residue before validation: $($unexpectedUntracked -join '; ')"
}

New-Item -ItemType Directory -Path $resolvedBuildRoot -Force | Out-Null
$cmakeCachePath = Join-Path $resolvedBuildRoot "CMakeCache.txt"
if (-not (Test-Path -LiteralPath $cmakeCachePath)) {
    Write-Output "strict native build cache: no existing CMakeCache.txt; clean configure required."
    exit 0
}

$cache = Get-Content -LiteralPath $cmakeCachePath -Raw
$homeMatch = [regex]::Match($cache, '(?m)^CMAKE_HOME_DIRECTORY:INTERNAL=(.+)$')
if (-not $homeMatch.Success) {
    throw "Strict native build cache CMakeCache.txt is missing CMAKE_HOME_DIRECTORY; remove '$resolvedBuildRoot' manually before rerun."
}

$cachedHome = [System.IO.Path]::GetFullPath($homeMatch.Groups[1].Value.Trim())
if ($cachedHome -ne $resolvedWorkspaceRoot) {
    Write-Output "strict native build cache: stale workspace root detected."
    Write-Output "  cached_home=$cachedHome"
    Write-Output "  expected_home=$resolvedWorkspaceRoot"
    Remove-Item -LiteralPath $resolvedBuildRoot -Recurse -Force
    New-Item -ItemType Directory -Path $resolvedBuildRoot -Force | Out-Null
    Write-Output "strict native build cache: stale build root removed."
    exit 0
}

Write-Output "strict native build cache: accepted build root '$resolvedBuildRoot'."
