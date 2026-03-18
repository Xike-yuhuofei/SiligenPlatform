$ErrorActionPreference = "Stop"

$projectRoot = Split-Path $PSScriptRoot -Parent
$canonicalTest = Join-Path (Split-Path $projectRoot -Parent) "apps\\hmi-app\\scripts\\test.ps1"

if (-not (Test-Path $canonicalTest)) {
    Write-Error "未找到 canonical hmi-app 测试脚本: $canonicalTest"
}

& $canonicalTest
