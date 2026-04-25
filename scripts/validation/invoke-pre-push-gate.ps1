[CmdletBinding()]
param(
    [string]$RemoteName = "",
    [string]$RemoteUrl = "",
    [ValidateSet("all", "apps", "contracts", "protocol-compatibility", "integration", "e2e", "performance")]
    [string[]]$Suite = @("all"),
    [ValidateSet("quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "full-offline-gate",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "high",
    [ValidateSet("quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "full-offline",
    [string]$ReportRoot = "tests\reports\pre-push-gate",
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $workspaceRoot

function Fail-PushGate {
    param([string]$Message)

    Write-Error "pre-push gate failed: $Message"
}

$unmerged = @(git diff --name-only --diff-filter=U)
if ($LASTEXITCODE -ne 0) {
    Fail-PushGate "unable to inspect unresolved conflict state."
}
if ($unmerged.Count -gt 0) {
    Fail-PushGate "unresolved conflict files remain: $($unmerged -join ', ')"
}

$status = (git status --porcelain | Out-String).Trim()
if (-not [string]::IsNullOrWhiteSpace($status)) {
    Fail-PushGate "worktree must be clean before push so the gate verifies the exact pushed commit."
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$safeRemoteName = if ([string]::IsNullOrWhiteSpace($RemoteName)) { "unknown-remote" } else { $RemoteName -replace '[^A-Za-z0-9_.-]', '_' }
$reportDir = Join-Path $workspaceRoot (Join-Path $ReportRoot "$timestamp-$safeRemoteName")
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$head = (git rev-parse HEAD).Trim()
$metadata = @(
    "generated_at: $((Get-Date).ToString('s'))",
    "remote_name: $RemoteName",
    "remote_url: $RemoteUrl",
    "git_head: $head",
    "lane: $Lane",
    "risk_profile: $RiskProfile",
    "desired_depth: $DesiredDepth",
    "suite: $($Suite -join ',')",
    "include_hardware_smoke: $($IncludeHardwareSmoke.IsPresent)",
    "include_hil_closed_loop: $($IncludeHilClosedLoop.IsPresent)",
    "include_hil_case_matrix: $($IncludeHilCaseMatrix.IsPresent)"
)
Set-Content -Path (Join-Path $reportDir "pre-push-gate-manifest.txt") -Value $metadata -Encoding UTF8

Write-Output "pre-push gate: remote=$RemoteName head=$head"
Write-Output "pre-push gate: report=$reportDir"
Write-Output "pre-push gate: running ci.ps1 -Lane $Lane -RiskProfile $RiskProfile -DesiredDepth $DesiredDepth"

& (Join-Path $workspaceRoot "ci.ps1") `
    -Suite $Suite `
    -ReportDir $reportDir `
    -Lane $Lane `
    -RiskProfile $RiskProfile `
    -DesiredDepth $DesiredDepth `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke `
    -IncludeHilClosedLoop:$IncludeHilClosedLoop `
    -IncludeHilCaseMatrix:$IncludeHilCaseMatrix

if ($LASTEXITCODE -ne 0) {
    Fail-PushGate "ci.ps1 exited with $LASTEXITCODE. See report: $reportDir"
}

Write-Output "pre-push gate passed"
