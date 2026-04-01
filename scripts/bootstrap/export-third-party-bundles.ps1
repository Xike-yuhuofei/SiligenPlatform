[CmdletBinding()]
param(
    [string]$WorkspaceRoot = "",
    [string]$ManifestPath = "",
    [string]$ThirdPartyRoot = "",
    [string]$OutputRoot = "scripts\\bootstrap\\bundles",
    [string]$ManifestOutPath = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($WorkspaceRoot)) {
    $WorkspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
}
$WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)

if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $PSScriptRoot "third-party-manifest.json"
} elseif (-not [System.IO.Path]::IsPathRooted($ManifestPath)) {
    $ManifestPath = [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $ManifestPath))
}

if ([string]::IsNullOrWhiteSpace($ThirdPartyRoot)) {
    $ThirdPartyRoot = Join-Path $WorkspaceRoot "third_party"
} elseif (-not [System.IO.Path]::IsPathRooted($ThirdPartyRoot)) {
    $ThirdPartyRoot = [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $ThirdPartyRoot))
}

if (-not [System.IO.Path]::IsPathRooted($OutputRoot)) {
    $OutputRoot = [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $OutputRoot))
}

if ([string]::IsNullOrWhiteSpace($ManifestOutPath)) {
    $ManifestOutPath = $ManifestPath
} elseif (-not [System.IO.Path]::IsPathRooted($ManifestOutPath)) {
    $ManifestOutPath = [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $ManifestOutPath))
}

if (-not (Test-Path $ManifestPath)) {
    throw "third-party manifest not found: $ManifestPath"
}
if (-not (Test-Path $ThirdPartyRoot)) {
    throw "third_party root not found: $ThirdPartyRoot"
}

$manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json
$packages = @($manifest.packages)
if ($packages.Count -eq 0) {
    throw "third-party manifest does not declare any packages: $ManifestPath"
}

New-Item -ItemType Directory -Force -Path $OutputRoot | Out-Null

$checksumLines = @()
foreach ($package in $packages) {
    $sourceDir = Join-Path $ThirdPartyRoot $package.targetDirectory
    if (-not (Test-Path $sourceDir)) {
        throw "third-party source directory missing for package '$($package.name)': $sourceDir"
    }

    $archivePath = Join-Path $OutputRoot $package.archiveFileName
    if ((Test-Path $archivePath) -and -not $Force) {
        throw "archive already exists. Re-run with -Force to overwrite: $archivePath"
    }

    Compress-Archive -Path $sourceDir -DestinationPath $archivePath -CompressionLevel Optimal -Force
    $archiveInfo = Get-Item -LiteralPath $archivePath
    $sha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()

    $package.sha256 = $sha256
    $package.sizeBytes = $archiveInfo.Length

    $checksumLines += "{0}  {1}" -f $sha256, $package.archiveFileName
    Write-Output "third-party export: packed $($package.name) -> $archivePath"
}

$manifest.artifactVersion = (Get-Date -Format "yyyy-MM-dd-HHmmss")
$manifest | ConvertTo-Json -Depth 6 | Set-Content -Path $ManifestOutPath -Encoding UTF8

$checksumPath = Join-Path $OutputRoot "SHA256SUMS.txt"
$checksumLines | Set-Content -Path $checksumPath -Encoding UTF8

Write-Output "third-party export: manifest updated -> $ManifestOutPath"
Write-Output "third-party export: checksums -> $checksumPath"
