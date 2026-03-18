$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
Write-Host "Proto generation placeholder. Source proto: $(Join-Path $projectRoot 'proto\dxf_primitives.proto')"