#!/usr/bin/env pwsh
# Create a new feature branch and spec directory that complies with the project constitution.
[CmdletBinding()]
param(
    [switch]$Json,
    [string]$ShortName,
    [ValidateSet('feat', 'fix', 'chore', 'refactor', 'docs', 'test', 'hotfix', 'spike', 'release')]
    [string]$Type = 'feat',
    [string]$Scope,
    [string]$Ticket = 'NOISSUE',
    [Parameter()]
    [int]$Number = 0,
    [switch]$Help,
    [Parameter(Position = 0, ValueFromRemainingArguments = $true)]
    [string[]]$FeatureDescription
)
$ErrorActionPreference = 'Stop'

function Show-Usage {
    Write-Host "Usage: ./create-new-feature.ps1 [-Json] [-Type <type>] -Scope <scope> [-Ticket <ticket>] [-ShortName <name>] <feature description>"
    Write-Host ""
    Write-Host "Options:"
    Write-Host "  -Json               Output in JSON format"
    Write-Host "  -Type <type>        Branch type: feat|fix|chore|refactor|docs|test|hotfix|spike|release (default: feat)"
    Write-Host "  -Scope <scope>      Module or domain short name, e.g. hmi, runtime, cli"
    Write-Host "  -Ticket <ticket>    Task ID like SS-142, or NOISSUE when none exists yet (default: NOISSUE)"
    Write-Host "  -ShortName <name>   Provide a custom short description for the branch tail"
    Write-Host "  -Help               Show this help message"
    Write-Host ""
    Write-Host "Examples:"
    Write-Host "  ./create-new-feature.ps1 -Type chore -Scope hmi -Ticket SS-142 -ShortName debug-instrumentation 'Add HMI debug instrumentation'"
    Write-Host "  ./create-new-feature.ps1 -Scope runtime 'Tighten startup timeout handling'"
}

# Show help if requested
if ($Help) {
    Show-Usage
    exit 0
}

# Legacy numbered branches are no longer allowed by constitution.
if ($Number -ne 0) {
    Write-Error "Error: -Number is no longer supported. Use -Type, -Scope, and -Ticket to create a compliant branch."
    exit 1
}

# Check if feature description provided
if (-not $FeatureDescription -or $FeatureDescription.Count -eq 0) {
    Show-Usage
    exit 1
}

$featureDesc = ($FeatureDescription -join ' ').Trim()

# Validate description is not empty after trimming (e.g., user passed only whitespace)
if ([string]::IsNullOrWhiteSpace($featureDesc)) {
    Write-Error "Error: Feature description cannot be empty or contain only whitespace"
    exit 1
}

if ([string]::IsNullOrWhiteSpace($Scope)) {
    Write-Error "Error: -Scope is required. Use a short module/domain name such as hmi, runtime, cli, or gateway."
    exit 1
}

function Find-RepositoryRoot {
    param(
        [string]$StartDir,
        [string[]]$Markers = @('.git', '.specify')
    )

    $current = Resolve-Path $StartDir
    while ($true) {
        foreach ($marker in $Markers) {
            if (Test-Path (Join-Path $current $marker)) {
                return $current
            }
        }

        $parent = Split-Path $current -Parent
        if ($parent -eq $current) {
            return $null
        }

        $current = $parent
    }
}

function ConvertTo-CleanBranchName {
    param([string]$Name)

    return $Name.ToLower() -replace '[^a-z0-9]', '-' -replace '-{2,}', '-' -replace '^-', '' -replace '-$', ''
}

function Normalize-Scope {
    param([string]$Value)

    $normalized = ConvertTo-CleanBranchName -Name $Value
    if ([string]::IsNullOrWhiteSpace($normalized)) {
        throw "Scope must contain at least one alphanumeric character."
    }

    return $normalized
}

function Normalize-Ticket {
    param([string]$Value)

    $normalized = ($Value.Trim().ToUpper()) -replace '\s+', ''
    if ($normalized -notmatch '^(NOISSUE|[A-Z][A-Z0-9]*-\d+)$') {
        throw "Ticket must be NOISSUE or match PROJECT-123 style identifiers."
    }

    return $normalized
}

function Get-BranchName {
    param([string]$Description)

    $stopWords = @(
        'i', 'a', 'an', 'the', 'to', 'for', 'of', 'in', 'on', 'at', 'by', 'with', 'from',
        'is', 'are', 'was', 'were', 'be', 'been', 'being', 'have', 'has', 'had',
        'do', 'does', 'did', 'will', 'would', 'should', 'could', 'can', 'may', 'might', 'must', 'shall',
        'this', 'that', 'these', 'those', 'my', 'your', 'our', 'their',
        'want', 'need', 'add', 'get', 'set'
    )

    $cleanName = $Description.ToLower() -replace '[^a-z0-9\s]', ' '
    $words = $cleanName -split '\s+' | Where-Object { $_ }

    $meaningfulWords = @()
    foreach ($word in $words) {
        if ($stopWords -contains $word) { continue }

        if ($word.Length -ge 3) {
            $meaningfulWords += $word
        } elseif ($Description -match "\b$($word.ToUpper())\b") {
            $meaningfulWords += $word
        }
    }

    if ($meaningfulWords.Count -gt 0) {
        $maxWords = if ($meaningfulWords.Count -eq 4) { 4 } else { 3 }
        return ($meaningfulWords | Select-Object -First $maxWords) -join '-'
    }

    $result = ConvertTo-CleanBranchName -Name $Description
    $fallbackWords = ($result -split '-') | Where-Object { $_ } | Select-Object -First 3
    return [string]::Join('-', $fallbackWords)
}

function Join-BranchPath {
    param(
        [string]$BasePath,
        [string]$BranchName
    )

    $path = $BasePath
    foreach ($segment in ($BranchName -split '/')) {
        if (-not [string]::IsNullOrWhiteSpace($segment)) {
            $path = Join-Path $path $segment
        }
    }

    return $path
}

$fallbackRoot = Find-RepositoryRoot -StartDir $PSScriptRoot
if (-not $fallbackRoot) {
    Write-Error "Error: Could not determine repository root. Please run this script from within the repository."
    exit 1
}

# Load common functions (includes Resolve-Template)
. "$PSScriptRoot/common.ps1"

try {
    $repoRoot = git rev-parse --show-toplevel 2>$null
    if ($LASTEXITCODE -eq 0) {
        $hasGit = $true
    } else {
        throw "Git not available"
    }
} catch {
    $repoRoot = $fallbackRoot
    $hasGit = $false
}

Set-Location $repoRoot

$scopeValue = Normalize-Scope -Value $Scope
$ticketValue = Normalize-Ticket -Value $Ticket

if ($ShortName) {
    $branchSuffix = ConvertTo-CleanBranchName -Name $ShortName
} else {
    $branchSuffix = Get-BranchName -Description $featureDesc
}

if ([string]::IsNullOrWhiteSpace($branchSuffix)) {
    Write-Error "Error: Could not derive a short description. Provide -ShortName explicitly."
    exit 1
}

$branchName = "$Type/$scopeValue/$ticketValue-$branchSuffix"

$maxBranchLength = 244
if ($branchName.Length -gt $maxBranchLength) {
    $prefix = "$Type/$scopeValue/$ticketValue-"
    $maxSuffixLength = $maxBranchLength - $prefix.Length
    if ($maxSuffixLength -le 0) {
        Write-Error "Error: Branch prefix '$prefix' is too long. Shorten -Scope or -Ticket."
        exit 1
    }

    $truncatedSuffix = $branchSuffix.Substring(0, [Math]::Min($branchSuffix.Length, $maxSuffixLength))
    $truncatedSuffix = $truncatedSuffix -replace '-$', ''
    $originalBranchName = $branchName
    $branchName = "$prefix$truncatedSuffix"

    Write-Warning "[specify] Branch name exceeded GitHub's 244-byte limit"
    Write-Warning "[specify] Original: $originalBranchName ($($originalBranchName.Length) bytes)"
    Write-Warning "[specify] Truncated to: $branchName ($($branchName.Length) bytes)"
}

$specsDir = Join-Path $repoRoot 'specs'
New-Item -ItemType Directory -Path $specsDir -Force | Out-Null
$featureDir = Join-BranchPath -BasePath $specsDir -BranchName $branchName

if ($hasGit) {
    $branchCreated = $false
    try {
        git checkout -q -b $branchName 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) {
            $branchCreated = $true
        }
    } catch {
        # Exception during git command
    }

    if (-not $branchCreated) {
        $existingBranch = git branch --list $branchName 2>$null
        if ($existingBranch) {
            Write-Error "Error: Branch '$branchName' already exists. Use a different -Type/-Scope/-Ticket combination or -ShortName."
            exit 1
        }

        Write-Error "Error: Failed to create git branch '$branchName'. Please check your git configuration and try again."
        exit 1
    }
} else {
    Write-Warning "[specify] Warning: Git repository not detected; skipped branch creation for $branchName"
}

New-Item -ItemType Directory -Path $featureDir -Force | Out-Null

$template = Resolve-Template -TemplateName 'spec-template' -RepoRoot $repoRoot
$specFile = Join-Path $featureDir 'spec.md'
if ($template -and (Test-Path $template)) {
    Copy-Item $template $specFile -Force
} else {
    New-Item -ItemType File -Path $specFile | Out-Null
}

$env:SPECIFY_FEATURE = $branchName
$specPath = [System.IO.Path]::GetRelativePath($repoRoot, $featureDir) -replace '[\\/]+', '/'

if ($Json) {
    $obj = [PSCustomObject]@{
        BRANCH_NAME = $branchName
        SPEC_FILE   = $specFile
        SPEC_PATH   = $specPath
        FEATURE_REF = $ticketValue
        HAS_GIT     = $hasGit
    }
    $obj | ConvertTo-Json -Compress
} else {
    Write-Output "BRANCH_NAME: $branchName"
    Write-Output "SPEC_FILE: $specFile"
    Write-Output "SPEC_PATH: $specPath"
    Write-Output "FEATURE_REF: $ticketValue"
    Write-Output "HAS_GIT: $hasGit"
    Write-Output "SPECIFY_FEATURE environment variable set to: $branchName"
}
