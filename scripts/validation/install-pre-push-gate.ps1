[CmdletBinding()]
param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $workspaceRoot

$gitDir = (git rev-parse --git-dir).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($gitDir)) {
    throw "Unable to resolve .git directory for pre-push hook installation."
}
if (-not [System.IO.Path]::IsPathRooted($gitDir)) {
    $gitDir = Join-Path $workspaceRoot $gitDir
}

$hookDir = Join-Path $gitDir "hooks"
$hookPath = Join-Path $hookDir "pre-push"
New-Item -ItemType Directory -Force -Path $hookDir | Out-Null

$managedMarker = "# SiligenSuite managed pre-push gate"
if ((Test-Path $hookPath) -and (-not $Force)) {
    $existing = Get-Content -Raw -Path $hookPath
    if ($existing -notlike "*$managedMarker*") {
        throw "pre-push hook already exists and is not managed by SiligenSuite. Re-run with -Force only after preserving any local custom hook behavior."
    }
}

$hookContent = @"
#!/bin/sh
$managedMarker
repo_root="`$(git rev-parse --show-toplevel)" || exit 1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "`$repo_root/scripts/validation/invoke-pre-push-gate.ps1" -RemoteName "`$1" -RemoteUrl "`$2"
exit `$?
"@

Set-Content -Path $hookPath -Value $hookContent -Encoding ASCII
Write-Output "installed pre-push gate: $hookPath"
Write-Output "gate runner: scripts/validation/invoke-pre-push-gate.ps1"
