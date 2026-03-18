param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "packages", "integration", "protocol-compatibility", "simulation")]
    [string[]]$Suite = @("all"),
    [switch]$SkipSimulationEngine
)

$ErrorActionPreference = "Stop"

$runner = Join-Path $PSScriptRoot "tools\\build\\build-validation.ps1"
if (-not (Test-Path $runner)) {
    Write-Error "未找到根级 build 入口: $runner"
}

& $runner -Profile $Profile -Suite $Suite -SkipSimulationEngine:$SkipSimulationEngine
