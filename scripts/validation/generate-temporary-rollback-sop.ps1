param(
    [string]$OutputRoot = "$env:LOCALAPPDATA\\SiligenSuite",
    [Parameter(Mandatory = $true)]
    [string]$ReleaseRoot,
    [string]$Operator,
    [string]$ReportRoot = "tests\\reports\\verify\\wave4e-final"
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

function Resolve-AbsolutePath {
    param(
        [string]$PathValue,
        [string]$WorkspaceRoot
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $PathValue))
}

if ([string]::IsNullOrWhiteSpace($Operator)) {
    $Operator = if (-not [string]::IsNullOrWhiteSpace($env:USERNAME)) { $env:USERNAME } else { $env:USER }
}

$resolvedReleaseRoot = Resolve-AbsolutePath -PathValue $ReleaseRoot -WorkspaceRoot $workspaceRoot
if (-not (Test-Path -LiteralPath $resolvedReleaseRoot -PathType Container)) {
    throw "ReleaseRoot not found or not a directory: $resolvedReleaseRoot"
}

$resolvedOutputRoot = Resolve-AbsolutePath -PathValue $OutputRoot -WorkspaceRoot $workspaceRoot
New-Item -ItemType Directory -Force -Path $resolvedOutputRoot | Out-Null

$resolvedReportRoot = Resolve-AbsolutePath -PathValue $ReportRoot -WorkspaceRoot $workspaceRoot
$resolvedIntakeDir = Join-Path $resolvedReportRoot "intake"
New-Item -ItemType Directory -Force -Path $resolvedIntakeDir | Out-Null

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$generatedAt = (Get-Date).ToString("o")
$sopRoot = Join-Path $resolvedOutputRoot ("wave4e-temp-rollback-sop-" + $timestamp)
New-Item -ItemType Directory -Force -Path $sopRoot | Out-Null

$sopPath = Join-Path $sopRoot "rollback-sop.md"
$manifestPath = Join-Path $sopRoot "rollback-manifest.json"
$listingPath = Join-Path $sopRoot "source-listing.txt"
$hashPath = Join-Path $sopRoot "hashes.txt"
$generationJsonPath = Join-Path $resolvedIntakeDir "temporary-rollback-sop-generation.json"
$generationMdPath = Join-Path $resolvedIntakeDir "temporary-rollback-sop-generation.md"

$sopLines = @()
$sopLines += "# Temporary Rollback SOP Snapshot"
$sopLines += ""
$sopLines += ("- generated_at: {0}" -f $generatedAt)
$sopLines += ("- operator: {0}" -f $Operator)
$sopLines += "- policy_mode: temporary-sop-direct-closeout"
$sopLines += ("- release_root: {0}" -f $resolvedReleaseRoot)
$sopLines += ""
$sopLines += "## Rollback Targets"
$sopLines += ""
$sopLines += "1. Restore canonical config only: config\\machine\\machine_config.ini"
$sopLines += "2. Restore recipe payload only: data\\recipes\\"
$sopLines += "3. Restore canonical control-app build root artifacts only."
$sopLines += ""
$sopLines += "## Guardrails"
$sopLines += ""
$sopLines += "1. Do not restore any removed root alias config file."
$sopLines += "2. Do not restore deprecated CLI aliases or compatibility shells."
$sopLines += "3. Keep rollback scope minimal and auditable."

$sopLines -join "`r`n" | Set-Content -Path $sopPath -Encoding utf8

$listingText = Get-ChildItem -LiteralPath $resolvedReleaseRoot -Recurse -Force |
    Select-Object FullName, Length, LastWriteTime |
    Format-Table -AutoSize |
    Out-String
if ([string]::IsNullOrWhiteSpace($listingText.Trim())) {
    $listingText = "<empty release root>"
}
$listingText | Set-Content -Path $listingPath -Encoding utf8

$manifest = [ordered]@{
    generated_at = $generatedAt
    operator = $Operator
    policy_mode = "temporary-sop-direct-closeout"
    release_root = $resolvedReleaseRoot
    temp_sop_root = $sopRoot
    files = @(
        [ordered]@{ name = "rollback-sop.md"; path = $sopPath }
        [ordered]@{ name = "rollback-manifest.json"; path = $manifestPath }
        [ordered]@{ name = "source-listing.txt"; path = $listingPath }
        [ordered]@{ name = "hashes.txt"; path = $hashPath }
    )
    source_evidence = [ordered]@{
        listing_file = $listingPath
    }
}

$manifest | ConvertTo-Json -Depth 6 | Set-Content -Path $manifestPath -Encoding utf8

$hashTargets = @($sopPath, $manifestPath, $listingPath)
$hashLines = @()
foreach ($target in $hashTargets) {
    $h = Get-FileHash -LiteralPath $target -Algorithm SHA256
    $hashLines += ("path={0}" -f $target)
    $hashLines += ("algorithm={0}" -f $h.Algorithm)
    $hashLines += ("hash={0}" -f $h.Hash)
    $hashLines += ""
}
$hashLines -join "`r`n" | Set-Content -Path $hashPath -Encoding utf8

$generationPayload = [ordered]@{
    generated_at = $generatedAt
    operator = $Operator
    policy_mode = "temporary-sop-direct-closeout"
    release_root = $resolvedReleaseRoot
    report_root = $resolvedReportRoot
    temporary_rollback_sop_root = $sopRoot
    files = [ordered]@{
        sop = $sopPath
        manifest = $manifestPath
        listing = $listingPath
        hashes = $hashPath
    }
}
$generationPayload | ConvertTo-Json -Depth 6 | Set-Content -Path $generationJsonPath -Encoding utf8

$generationLines = @()
$generationLines += "# Temporary Rollback SOP Generation"
$generationLines += ""
$generationLines += ("- generated_at: {0}" -f $generatedAt)
$generationLines += ("- operator: {0}" -f $Operator)
$generationLines += "- policy_mode: temporary-sop-direct-closeout"
$generationLines += ("- release_root: {0}" -f $resolvedReleaseRoot)
$generationLines += ("- temporary_rollback_sop_root: {0}" -f $sopRoot)
$generationLines += ("- json: {0}" -f $generationJsonPath)
$generationLines += ""
$generationLines += "## Files"
$generationLines += ""
$generationLines += ("1. {0}" -f $sopPath)
$generationLines += ("2. {0}" -f $manifestPath)
$generationLines += ("3. {0}" -f $listingPath)
$generationLines += ("4. {0}" -f $hashPath)
$generationLines -join "`r`n" | Set-Content -Path $generationMdPath -Encoding utf8

Write-Output "temporary rollback sop generated"
Write-Output ("sop_root: {0}" -f $sopRoot)
Write-Output ("sop_md: {0}" -f $sopPath)
Write-Output ("manifest_json: {0}" -f $manifestPath)
Write-Output ("generation_json: {0}" -f $generationJsonPath)
Write-Output ("generation_md: {0}" -f $generationMdPath)
