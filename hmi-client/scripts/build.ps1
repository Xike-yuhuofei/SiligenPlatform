$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$canonicalSourceRoot = Join-Path (Split-Path $projectRoot -Parent) "apps\\hmi-app\\src"

if (-not (Test-Path $canonicalSourceRoot)) {
    Write-Error "未找到 canonical hmi-app 源码目录: $canonicalSourceRoot"
}

python -m compileall $canonicalSourceRoot
