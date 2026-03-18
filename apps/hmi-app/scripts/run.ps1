param(
    [Parameter(ValueFromRemainingArguments = $true)]
    [string[]]$AppArgs
)

$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$workspaceRoot = Split-Path (Split-Path $projectRoot -Parent) -Parent
$sourceRoot = Join-Path $projectRoot "src"

if ([string]::IsNullOrWhiteSpace($env:SILIGEN_WORKSPACE_ROOT)) {
    $env:SILIGEN_WORKSPACE_ROOT = $workspaceRoot
}

if ([string]::IsNullOrWhiteSpace($env:PYTHONPATH)) {
    $env:PYTHONPATH = $sourceRoot
} else {
    $env:PYTHONPATH = "$sourceRoot$([IO.Path]::PathSeparator)$env:PYTHONPATH"
}

python -m hmi_client.main @AppArgs
