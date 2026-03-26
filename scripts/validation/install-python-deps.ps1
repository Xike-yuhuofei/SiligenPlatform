param(
    [switch]$SkipApps
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

python -m pip install --upgrade pip
python -m pip install -e (Join-Path $workspaceRoot "shared\\testing\\test-kit")

if (-not $SkipApps) {
    python -m pip install -r (Join-Path $workspaceRoot "apps\\hmi-app\\requirements.txt")
}

Write-Output "workspace python dependencies installed"
