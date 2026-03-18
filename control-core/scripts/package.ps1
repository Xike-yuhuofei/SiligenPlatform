$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $projectRoot "build"
$releaseDir = Join-Path $projectRoot "packaging\release"

New-Item -ItemType Directory -Force -Path $releaseDir | Out-Null
Write-Host "Package placeholder. Build artifacts can be collected from: $buildDir"