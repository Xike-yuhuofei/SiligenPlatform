param(
    [switch]$SkipApps
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$engineeringDataVenv = Join-Path $workspaceRoot "build\\engineering-data-venv"
$engineeringDataPython = Join-Path $engineeringDataVenv "Scripts\\python.exe"

if (-not (Test-Path $engineeringDataPython)) {
    python -m venv $engineeringDataVenv
}

python -m pip install --upgrade pip
python -m pip install -e (Join-Path $workspaceRoot "shared\\testing\\test-kit")
python -m pip install semgrep import-linter pydeps coverage pytest

if (-not $SkipApps) {
    python -m pip install -r (Join-Path $workspaceRoot "apps\\hmi-app\\requirements.txt")
}

& $engineeringDataPython -m pip install --upgrade pip
& $engineeringDataPython -m pip install -e (Join-Path $workspaceRoot "modules\\dxf-geometry\\application")

Write-Output "workspace python dependencies installed"
Write-Output "engineering-data python: $engineeringDataPython"
