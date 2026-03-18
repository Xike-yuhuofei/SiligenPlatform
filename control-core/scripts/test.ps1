$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$buildDir = Join-Path $projectRoot "build"

if (-not (Test-Path $buildDir)) {
    throw "Build directory not found. Run scripts/build.ps1 first."
}

ctest --test-dir $buildDir --output-on-failure -C Debug