$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$env:PYTHONPATH = Join-Path $projectRoot "src"
python -m unittest discover -s (Join-Path $projectRoot 'tests\\unit') -p "test_*.py"