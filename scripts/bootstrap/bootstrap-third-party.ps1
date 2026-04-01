[CmdletBinding()]
param(
    [string]$WorkspaceRoot = "",
    [string]$ManifestPath = "",
    [string]$ThirdPartyRoot = "",
    [string]$ArtifactRoot = "",
    [string]$BaseUri = "",
    [switch]$Force
)

$ErrorActionPreference = "Stop"

Add-Type -AssemblyName System.IO.Compression.FileSystem

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

if ([string]::IsNullOrWhiteSpace($ArtifactRoot) -and -not [string]::IsNullOrWhiteSpace($env:SILIGEN_THIRD_PARTY_ARTIFACT_ROOT)) {
    $ArtifactRoot = $env:SILIGEN_THIRD_PARTY_ARTIFACT_ROOT
}
if ([string]::IsNullOrWhiteSpace($ArtifactRoot)) {
    $repoTrackedArtifactRoot = Join-Path $PSScriptRoot "bundles"
    if (Test-Path $repoTrackedArtifactRoot) {
        $ArtifactRoot = $repoTrackedArtifactRoot
    }
}
if (-not [string]::IsNullOrWhiteSpace($ArtifactRoot) -and -not [System.IO.Path]::IsPathRooted($ArtifactRoot)) {
    $ArtifactRoot = [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $ArtifactRoot))
}

if ([string]::IsNullOrWhiteSpace($BaseUri) -and -not [string]::IsNullOrWhiteSpace($env:SILIGEN_THIRD_PARTY_BASE_URI)) {
    $BaseUri = $env:SILIGEN_THIRD_PARTY_BASE_URI
}

if (-not (Test-Path $ManifestPath)) {
    throw "third-party manifest not found: $ManifestPath"
}

$manifest = Get-Content -Raw -Path $ManifestPath | ConvertFrom-Json
$packages = @($manifest.packages)
if ($packages.Count -eq 0) {
    throw "third-party manifest does not declare any packages: $ManifestPath"
}

function Test-PackageSatisfied {
    param(
        [pscustomobject]$Package
    )

    foreach ($relativePath in @($Package.validationPaths)) {
        $fullPath = Join-Path $ThirdPartyRoot $relativePath
        if (-not (Test-Path $fullPath)) {
            return $false
        }
    }

    return $true
}

function Resolve-ArchiveSource {
    param(
        [pscustomobject]$Package
    )

    if (-not [string]::IsNullOrWhiteSpace($ArtifactRoot)) {
        $artifactCandidate = Join-Path $ArtifactRoot $Package.archiveFileName
        if (Test-Path $artifactCandidate) {
            return [pscustomobject]@{
                Kind     = "file"
                Location = [System.IO.Path]::GetFullPath($artifactCandidate)
            }
        }
    }

    $packageUri = ""
    if ($Package.PSObject.Properties.Name -contains "uri") {
        $packageUri = [string]$Package.uri
    }

    if (-not [string]::IsNullOrWhiteSpace($packageUri)) {
        if ($packageUri -match "^[a-zA-Z][a-zA-Z0-9+.-]*://") {
            return [pscustomobject]@{
                Kind     = "uri"
                Location = $packageUri
            }
        }

        if ([System.IO.Path]::IsPathRooted($packageUri)) {
            return [pscustomobject]@{
                Kind     = "file"
                Location = [System.IO.Path]::GetFullPath($packageUri)
            }
        }

        if (-not [string]::IsNullOrWhiteSpace($BaseUri)) {
            if ($BaseUri -match "^[a-zA-Z][a-zA-Z0-9+.-]*://") {
                return [pscustomobject]@{
                    Kind     = "uri"
                    Location = ($BaseUri.TrimEnd("/") + "/" + $packageUri.TrimStart("/"))
                }
            }

            return [pscustomobject]@{
                Kind     = "file"
                Location = [System.IO.Path]::GetFullPath((Join-Path $BaseUri $packageUri))
            }
        }
    }

    $manifestBaseUri = ""
    if ($manifest.PSObject.Properties.Name -contains "artifactBaseUri") {
        $manifestBaseUri = [string]$manifest.artifactBaseUri
    }

    $effectiveBaseUri = $BaseUri
    if ([string]::IsNullOrWhiteSpace($effectiveBaseUri) -and -not [string]::IsNullOrWhiteSpace($manifestBaseUri)) {
        $effectiveBaseUri = $manifestBaseUri
    }

    if (-not [string]::IsNullOrWhiteSpace($effectiveBaseUri)) {
        if ($effectiveBaseUri -match "^[a-zA-Z][a-zA-Z0-9+.-]*://") {
            return [pscustomobject]@{
                Kind     = "uri"
                Location = ($effectiveBaseUri.TrimEnd("/") + "/" + $Package.archiveFileName)
            }
        }

        return [pscustomobject]@{
            Kind     = "file"
            Location = [System.IO.Path]::GetFullPath((Join-Path $effectiveBaseUri $Package.archiveFileName))
        }
    }

    return $null
}

function Get-ExpectedSha256 {
    param(
        [pscustomobject]$Package
    )

    $sha = [string]$Package.sha256
    if ([string]::IsNullOrWhiteSpace($sha)) {
        return ""
    }

    return $sha.ToLowerInvariant()
}

function Copy-OrDownloadArchive {
    param(
        [pscustomobject]$Source,
        [string]$DestinationPath
    )

    if ($Source.Kind -eq "file") {
        if (-not (Test-Path $Source.Location)) {
            throw "third-party archive source not found: $($Source.Location)"
        }

        Copy-Item -LiteralPath $Source.Location -Destination $DestinationPath -Force
        return
    }

    Invoke-WebRequest -Uri $Source.Location -OutFile $DestinationPath
}

function Expand-PackageArchive {
    param(
        [pscustomobject]$Package,
        [string]$ArchivePath
    )

    $extractRoot = Join-Path $stageRoot "$($Package.name)-extract"
    if (Test-Path $extractRoot) {
        Remove-Item -LiteralPath $extractRoot -Recurse -Force
    }
    New-Item -ItemType Directory -Force -Path $extractRoot | Out-Null

    [System.IO.Compression.ZipFile]::ExtractToDirectory($ArchivePath, $extractRoot)

    $candidateRoot = Join-Path $extractRoot $Package.targetDirectory
    if (-not (Test-Path $candidateRoot)) {
        $subdirs = @(Get-ChildItem -LiteralPath $extractRoot -Directory)
        if ($subdirs.Count -eq 1) {
            $candidateRoot = $subdirs[0].FullName
        }
    }

    if (-not (Test-Path $candidateRoot)) {
        throw "archive for package '$($Package.name)' does not contain directory '$($Package.targetDirectory)'"
    }

    $destinationRoot = Join-Path $ThirdPartyRoot $Package.targetDirectory
    if (Test-Path $destinationRoot) {
        Remove-Item -LiteralPath $destinationRoot -Recurse -Force
    }

    Move-Item -LiteralPath $candidateRoot -Destination $destinationRoot

    if (-not (Test-PackageSatisfied -Package $Package)) {
        throw "package '$($Package.name)' failed validation after extraction"
    }
}

$requiredPackages = @($packages | Where-Object { $_.required })
$allSatisfied = $true
foreach ($package in $requiredPackages) {
    if (-not (Test-PackageSatisfied -Package $package)) {
        $allSatisfied = $false
        break
    }
}

if ($allSatisfied -and -not $Force) {
    Write-Output "third-party bootstrap: satisfied ($ThirdPartyRoot)"
    return
}

$stageRoot = Join-Path $WorkspaceRoot ".tmp\third-party-bootstrap"
if (Test-Path $stageRoot) {
    Remove-Item -LiteralPath $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null
New-Item -ItemType Directory -Force -Path $ThirdPartyRoot | Out-Null

foreach ($package in $requiredPackages) {
    if ((Test-PackageSatisfied -Package $package) -and -not $Force) {
        Write-Output "third-party bootstrap: reuse $($package.name)"
        continue
    }

    $source = Resolve-ArchiveSource -Package $package
    if ($null -eq $source) {
        throw ("third-party package '{0}' is missing locally and no artifact source is configured. " +
            "Track bundles under scripts/bootstrap/bundles, or set SILIGEN_THIRD_PARTY_ARTIFACT_ROOT / SILIGEN_THIRD_PARTY_BASE_URI, " +
            "or export bundles with scripts/bootstrap/export-third-party-bundles.ps1.") -f $package.name
    }

    $archivePath = Join-Path $stageRoot $package.archiveFileName
    Copy-OrDownloadArchive -Source $source -DestinationPath $archivePath

    $expectedSha256 = Get-ExpectedSha256 -Package $package
    if (-not [string]::IsNullOrWhiteSpace($expectedSha256)) {
        $actualSha256 = (Get-FileHash -LiteralPath $archivePath -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($actualSha256 -ne $expectedSha256) {
            throw "third-party package '$($package.name)' SHA256 mismatch. expected=$expectedSha256 actual=$actualSha256"
        }
    } else {
        Write-Warning "third-party package '$($package.name)' has no SHA256 in manifest: $ManifestPath"
    }

    Expand-PackageArchive -Package $package -ArchivePath $archivePath
    Write-Output "third-party bootstrap: hydrated $($package.name)"
}

Write-Output "third-party bootstrap: ready ($ThirdPartyRoot)"
