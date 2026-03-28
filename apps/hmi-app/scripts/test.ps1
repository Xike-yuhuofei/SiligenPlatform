$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$workspaceRoot = Split-Path (Split-Path $projectRoot -Parent) -Parent
$sourceRoot = Join-Path $projectRoot "src"
$hmiApplicationRoot = Join-Path $workspaceRoot "modules\hmi-application\application"
if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = "$hmiApplicationRoot$([IO.Path]::PathSeparator)$sourceRoot"
} else {
    $env:PYTHONPATH = "$hmiApplicationRoot$([IO.Path]::PathSeparator)$sourceRoot$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}
python -m unittest discover -s (Join-Path $projectRoot 'tests\\unit') -p "test_*.py"
