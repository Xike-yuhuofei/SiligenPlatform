param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("field-scripts", "release-package", "rollback-package")]
    [string]$Scope,
    [string]$SourcePath,
    [string]$ReportDir = "integration\\reports\\verify\\wave4e-rerun\\intake",
    [string]$Operator,
    [ValidateSet("Auto", "PendingInput", "Collected", "InputGap")]
    [string]$Status = "Auto"
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent

function Resolve-OutputPath {
    param([string]$PathValue)

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

function Get-DisplayPath {
    param([string]$PathValue)

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        return "<none>"
    }

    $fullPath = [System.IO.Path]::GetFullPath($PathValue)
    $workspaceFullPath = [System.IO.Path]::GetFullPath($workspaceRoot)
    if ($fullPath.StartsWith($workspaceFullPath, [System.StringComparison]::OrdinalIgnoreCase)) {
        $relative = $fullPath.Substring($workspaceFullPath.Length).TrimStart('\', '/')
        if ([string]::IsNullOrWhiteSpace($relative)) {
            return "."
        }

        return $relative -replace '/', '\'
    }

    return $fullPath
}

function Get-ScopeTitle {
    param([string]$ScopeName)

    switch ($ScopeName) {
        "field-scripts" { return "Field Scripts Intake" }
        "release-package" { return "Release Package Intake" }
        "rollback-package" { return "Rollback Package Intake" }
        default { throw "unsupported scope: $ScopeName" }
    }
}

function Get-RecommendedScanCommand {
    param(
        [string]$ScopeName,
        [string]$TargetPath
    )

    $quoted = if ([string]::IsNullOrWhiteSpace($TargetPath)) { "<source-path>" } else { ('"{0}"' -f $TargetPath) }

    switch ($ScopeName) {
        "field-scripts" {
            return 'rg -n "dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini" {0} -g ''*.ps1'' -g ''*.py'' -g ''*.bat'' -g ''*.cmd'' -g ''*.scr'' -g ''*.ini'' -g ''*.json'' -g ''*.yaml'' -g ''*.yml'' -g ''*.xml'' -g ''*.cfg'' -g ''*.conf'' -g ''*.toml'' -g ''*.txt'' -g ''*.md''' -f $quoted
        }
        "release-package" {
            return 'rg -n "dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini" {0} -g ''*.ps1'' -g ''*.py'' -g ''*.bat'' -g ''*.cmd'' -g ''*.scr'' -g ''*.ini'' -g ''*.json'' -g ''*.yaml'' -g ''*.yml'' -g ''*.xml'' -g ''*.cfg'' -g ''*.conf'' -g ''*.toml'' -g ''*.txt'' -g ''*.md''' -f $quoted
        }
        "rollback-package" {
            return 'rg -n "dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini" {0} -g ''*.ps1'' -g ''*.py'' -g ''*.bat'' -g ''*.cmd'' -g ''*.scr'' -g ''*.ini'' -g ''*.json'' -g ''*.yaml'' -g ''*.yml'' -g ''*.xml'' -g ''*.cfg'' -g ''*.conf'' -g ''*.toml'' -g ''*.txt'' -g ''*.md''' -f $quoted
        }
        default { throw "unsupported scope: $ScopeName" }
    }
}

function Get-DefaultNextAction {
    param(
        [string]$ScopeName,
        [string]$ResolvedStatus
    )

    switch ($ResolvedStatus) {
        "collected" { return "run external observation for $ScopeName against the recorded source_path" }
        "pending-input" { return "collect actual $ScopeName input, then update intake and rerun observation" }
        "input-gap" { return "collect actual $ScopeName input, then update intake and rerun observation" }
        default { throw "unsupported status: $ResolvedStatus" }
    }
}

$resolvedReportDir = Resolve-OutputPath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

if ([string]::IsNullOrWhiteSpace($Operator)) {
    $Operator = if (-not [string]::IsNullOrWhiteSpace($env:USERNAME)) { $env:USERNAME } else { $env:USER }
}

$resolvedSourcePath = $null
$sourceExists = $false
if (-not [string]::IsNullOrWhiteSpace($SourcePath)) {
    try {
        $resolvedSourcePath = (Resolve-Path -LiteralPath $SourcePath).Path
        $sourceExists = $true
    } catch {
        $resolvedSourcePath = [System.IO.Path]::GetFullPath($SourcePath)
        $sourceExists = $false
    }
}

$resolvedStatus = switch ($Status) {
    "Auto" {
        if ([string]::IsNullOrWhiteSpace($SourcePath)) {
            "pending-input"
        } elseif ($sourceExists) {
            "collected"
        } else {
            "input-gap"
        }
    }
    "PendingInput" { "pending-input" }
    "Collected" { "collected" }
    "InputGap" { "input-gap" }
    default { throw "unsupported status value: $Status" }
}

if ($resolvedStatus -eq "collected" -and (-not $sourceExists)) {
    Write-Error "status=Collected 但 source_path 不存在：$SourcePath"
}

$collectedAt = (Get-Date).ToString("o")
$scanCommand = Get-RecommendedScanCommand -ScopeName $Scope -TargetPath $resolvedSourcePath
$baseName = $Scope
$mdPath = Join-Path $resolvedReportDir ($baseName + ".md")
$jsonPath = Join-Path $resolvedReportDir ($baseName + ".json")
$hashOrListing = "<none>"
$evidence = "<none>"
$evidenceFiles = @()

if ($resolvedStatus -eq "collected") {
    if (Test-Path -LiteralPath $resolvedSourcePath -PathType Container) {
        $listingPath = Join-Path $resolvedReportDir ($baseName + ".listing.txt")
        $listingText = Get-ChildItem -LiteralPath $resolvedSourcePath -Recurse -Force |
            Select-Object FullName, Length, LastWriteTime |
            Format-Table -AutoSize |
            Out-String
        if ([string]::IsNullOrWhiteSpace($listingText.Trim())) {
            $listingText = "<empty directory>"
        }

        Set-Content -Path $listingPath -Value $listingText -Encoding utf8
        $hashOrListing = Get-DisplayPath -PathValue $listingPath
        $evidence = $hashOrListing
        $evidenceFiles += $listingPath
    } else {
        $hashPath = Join-Path $resolvedReportDir ($baseName + ".hash.txt")
        $hash = Get-FileHash -LiteralPath $resolvedSourcePath -Algorithm SHA256
        $hashLines = @(
            ('path={0}' -f $resolvedSourcePath)
            ('algorithm={0}' -f $hash.Algorithm)
            ('hash={0}' -f $hash.Hash)
        )
        Set-Content -Path $hashPath -Value $hashLines -Encoding utf8
        $hashOrListing = Get-DisplayPath -PathValue $hashPath
        $evidence = $hashOrListing
        $evidenceFiles += $hashPath
    }
}

$observationResult = switch ($resolvedStatus) {
    "collected" { "pending-input" }
    "pending-input" { "pending-input" }
    "input-gap" { "input-gap" }
    default { throw "unsupported status: $resolvedStatus" }
}

$payload = [ordered]@{
    scope = $Scope
    status = $resolvedStatus
    source_path = $resolvedSourcePath
    collected_at = $collectedAt
    operator = $Operator
    hash_or_listing = $hashOrListing
    scan_command = $scanCommand
    observation_result = $observationResult
    evidence = $evidence
    next_action = Get-DefaultNextAction -ScopeName $Scope -ResolvedStatus $resolvedStatus
    evidence_files = @($evidenceFiles | ForEach-Object { Get-DisplayPath -PathValue $_ })
}

$payload | ConvertTo-Json -Depth 5 | Set-Content -Path $jsonPath -Encoding utf8

$mdLines = @()
$mdLines += ('# {0}' -f (Get-ScopeTitle -ScopeName $Scope))
$mdLines += ""
$mdLines += ('- status: `{0}`' -f $payload.status)
$mdLines += ('- source_path: `{0}`' -f $(if ($sourceExists) { $payload.source_path } elseif (-not [string]::IsNullOrWhiteSpace($resolvedSourcePath)) { $resolvedSourcePath } else { "<none>" }))
$mdLines += ('- collected_at: `{0}`' -f $payload.collected_at)
$mdLines += ('- operator: `{0}`' -f $payload.operator)
$mdLines += ('- hash_or_listing: `{0}`' -f $payload.hash_or_listing)
$mdLines += ('- scan_command: `{0}`' -f $payload.scan_command)
$mdLines += ('- observation_result: `{0}`' -f $payload.observation_result)
$mdLines += ('- evidence: `{0}`' -f $payload.evidence)
$mdLines += ('- next_action: `{0}`' -f $payload.next_action)
$mdLines += ""
$mdLines += "## Machine-Readable Sidecar"
$mdLines += ""
$mdLines += ('- json: `{0}`' -f (Get-DisplayPath -PathValue $jsonPath))

$mdLines -join "`r`n" | Set-Content -Path $mdPath -Encoding utf8

Write-Output "external observation intake registered: scope=$Scope status=$resolvedStatus"
Write-Output "markdown: $mdPath"
Write-Output "json: $jsonPath"
