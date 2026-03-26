[CmdletBinding()]
param(
    [string]$Branch,
    [string]$TimestampFormat = "yyyyMMdd-HHmmss",
    [switch]$AsEnv
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($Branch)) {
    try {
        $Branch = (git branch --show-current 2>$null).Trim()
    } catch {
        $Branch = ""
    }
}

if ([string]::IsNullOrWhiteSpace($Branch)) {
    $Branch = "unknown-branch"
}

$branchSafe = $Branch `
    -replace '[\\/:*?"<>|]+', '-' `
    -replace '\s+', '-' `
    -replace '-{2,}', '-'

$branchPattern = '^(?<type>feat|fix|chore|refactor|docs|test|hotfix|spike|release)\/(?<scope>[a-z0-9-]+)\/(?<ticket>NOISSUE|[A-Z]+-\d+)-(?<desc>[a-z0-9-]+)$'

$ticket = "NOISSUE"
$type = ""
$scope = ""
$shortDesc = ""
$branchCompliant = $false

if ($Branch -match $branchPattern) {
    $branchCompliant = $true
    $ticket = $Matches["ticket"]
    $type = $Matches["type"]
    $scope = $Matches["scope"]
    $shortDesc = $Matches["desc"]
} elseif ($Branch -match '(?<ticket>[A-Z]+-\d+)') {
    $ticket = $Matches["ticket"]
}

$now = Get-Date
$ctx = [ordered]@{
    Branch          = $Branch
    BranchSafe      = $branchSafe
    Ticket          = $ticket
    BranchCompliant = $branchCompliant
    Type            = $type
    Scope           = $scope
    ShortDesc       = $shortDesc
    Date            = $now.ToString("yyyy-MM-dd")
    Timestamp       = $now.ToString($TimestampFormat)
}

if ($AsEnv) {
    foreach ($k in $ctx.Keys) {
        Write-Output ("{0}={1}" -f $k.ToUpperInvariant(), $ctx[$k])
    }
    exit 0
}

$ctx | ConvertTo-Json -Compress
