[CmdletBinding()]
param(
    [string]$ReportDir = "tests\reports\pre-push-gate\remote-branch-delete-safety"
)

$ErrorActionPreference = "Stop"
$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$defaultProtectedPatterns = @("main", "master", "develop", "dev", "release/*", "hotfix/*")
$whitelistPath = Join-Path $workspaceRoot ".agents\skills\git-cleanup\whitelist.json"

function Resolve-WorkspacePath {
    param([string]$PathValue)
    if ([System.IO.Path]::IsPathRooted($PathValue)) { return [System.IO.Path]::GetFullPath($PathValue) }
    return [System.IO.Path]::GetFullPath((Join-Path $workspaceRoot $PathValue))
}

function Add-Issue {
    param(
        [System.Collections.Generic.List[object]]$Issues,
        [string]$Id,
        [string]$Severity,
        [string]$Branch,
        [string]$Message,
        [string]$Hint
    )
    $Issues.Add([pscustomobject]@{
        id = $Id
        severity = $Severity
        branch = $Branch
        message = $Message
        hint = $Hint
    }) | Out-Null
}

function Test-BranchMatchesProtectedPattern {
    param(
        [string]$BranchName,
        [string[]]$Patterns
    )
    foreach ($pattern in @($Patterns)) {
        if ([string]::IsNullOrWhiteSpace($pattern)) { continue }
        if ($BranchName -like $pattern) {
            return $pattern
        }
    }
    return ""
}

function Get-CurrentWorktreeBranches {
    $branches = New-Object System.Collections.Generic.List[string]
    $current = ""
    $worktreeList = Invoke-CommandLines -CommandName "git" -Arguments @("worktree", "list", "--porcelain")
    if (-not $worktreeList.success) {
        return @()
    }
    foreach ($line in @($worktreeList.lines)) {
        $text = [string]$line
        if ($text.StartsWith("branch refs/heads/")) {
            $current = $text.Substring("branch refs/heads/".Length).Trim()
            if (-not [string]::IsNullOrWhiteSpace($current)) {
                $branches.Add($current) | Out-Null
            }
        }
    }
    return @($branches | Select-Object -Unique)
}

function Get-StashBranches {
    $branches = New-Object System.Collections.Generic.List[string]
    $stashList = Invoke-CommandLines -CommandName "git" -Arguments @("stash", "list")
    if (-not $stashList.success) {
        return @()
    }
    foreach ($line in @($stashList.lines)) {
        $text = [string]$line
        if ($text -match ':\s+(?:WIP\s+on|On)\s+([^:]+):') {
            $branch = $Matches[1].Trim()
            if (-not [string]::IsNullOrWhiteSpace($branch)) {
                $branches.Add($branch) | Out-Null
            }
        }
    }
    return @($branches | Select-Object -Unique)
}

function Test-CommandAvailable {
    param([string]$CommandName)
    return $null -ne (Get-Command $CommandName -ErrorAction SilentlyContinue)
}

function Invoke-CommandLines {
    param(
        [string]$CommandName,
        [string[]]$Arguments
    )

    if (-not (Test-CommandAvailable -CommandName $CommandName)) {
        return [pscustomobject]@{
            available = $false
            success = $false
            lines = @()
            reason = "$CommandName CLI unavailable"
        }
    }

    try {
        $lines = @(& $CommandName @Arguments 2>$null)
        if ($LASTEXITCODE -ne 0) {
            return [pscustomobject]@{
                available = $true
                success = $false
                lines = @()
                reason = "$CommandName exited with code $LASTEXITCODE"
            }
        }
        return [pscustomobject]@{
            available = $true
            success = $true
            lines = @($lines)
            reason = ""
        }
    } catch {
        return [pscustomobject]@{
            available = $true
            success = $false
            lines = @()
            reason = $_.Exception.Message
        }
    }
}

function Get-DefaultBranchName {
    param(
        [string]$RemoteName,
        [string]$BaseBranch
    )
    if (-not [string]::IsNullOrWhiteSpace($RemoteName)) {
        $symbolic = Invoke-CommandLines -CommandName "git" -Arguments @("symbolic-ref", "refs/remotes/$RemoteName/HEAD")
        if ($symbolic.success -and $symbolic.lines.Count -gt 0) {
            $ref = [string]$symbolic.lines[0]
            if ($ref -match "^refs/remotes/$RemoteName/(.+)$") {
                return $Matches[1]
            }
        }
    }
    if (-not [string]::IsNullOrWhiteSpace($BaseBranch) -and $BaseBranch -match '^[^/]+/(.+)$') {
        return $Matches[1]
    }
    return ""
}

function Test-LocalBranchExists {
    param([string]$BranchName)
    $result = Invoke-CommandLines -CommandName "git" -Arguments @("show-ref", "--verify", "--quiet", "refs/heads/$BranchName")
    return ($result.success)
}

function Get-LocalBranchMergeState {
    param(
        [string]$BranchName,
        [string]$BaseBranch
    )
    if ([string]::IsNullOrWhiteSpace($BaseBranch)) {
        return [pscustomobject]@{
            known = $false
            ahead = -1
            behind = -1
            reason = "base branch unavailable"
        }
    }
    $counts = Invoke-CommandLines -CommandName "git" -Arguments @("rev-list", "--left-right", "--count", "$BaseBranch...$BranchName")
    if (-not $counts.success -or $counts.lines.Count -eq 0) {
        return [pscustomobject]@{
            known = $false
            ahead = -1
            behind = -1
            reason = "unable to compute merge state"
        }
    }
    $parts = @(([string]$counts.lines[0]).Trim() -split '\s+')
    if ($parts.Count -lt 2) {
        return [pscustomobject]@{
            known = $false
            ahead = -1
            behind = -1
            reason = "unexpected rev-list output"
        }
    }
    return [pscustomobject]@{
        known = $true
        ahead = [int]$parts[1]
        behind = [int]$parts[0]
        reason = ""
    }
}

function Get-PullRequestState {
    param([string]$BranchName)

    if (-not (Test-CommandAvailable -CommandName "gh")) {
        return [pscustomobject]@{
            known = $false
            state = "unknown"
            prs = @()
            reason = "gh CLI unavailable"
        }
    }

    try {
        $ghOutput = Invoke-CommandLines -CommandName "gh" -Arguments @("pr", "list", "--state", "all", "--head", $BranchName, "--json", "number,state,isDraft,headRefName,baseRefName,title")
        if (-not $ghOutput.success) {
            return [pscustomobject]@{
                known = $false
                state = "unknown"
                prs = @()
                reason = "unable to query GitHub PR state"
            }
        }

        $json = $ghOutput.lines -join [Environment]::NewLine
        if ([string]::IsNullOrWhiteSpace($json)) {
            $prs = @()
        } else {
            $prs = @((ConvertFrom-Json -InputObject $json))
        }
    } catch {
        return [pscustomobject]@{
            known = $false
            state = "unknown"
            prs = @()
            reason = "unable to parse GitHub PR state"
        }
    }

    if ($prs.Count -eq 0) {
        return [pscustomobject]@{
            known = $true
            state = "absent"
            prs = @()
            reason = ""
        }
    }
    $states = @($prs | ForEach-Object { [string]$_.state } | Select-Object -Unique)
    if ($states -contains "OPEN") {
        $state = "open"
    } elseif ($states -contains "MERGED" -and $states.Count -eq 1) {
        $state = "merged"
    } elseif ($states -contains "CLOSED" -and $states.Count -eq 1) {
        $state = "closed"
    } elseif (($states -contains "MERGED") -or ($states -contains "CLOSED")) {
        $state = "closed-or-merged"
    } else {
        $state = "unknown"
    }
    return [pscustomobject]@{
        known = $true
        state = $state
        prs = @($prs)
        reason = ""
    }
}

function Write-DeleteSafetyReport {
    param(
        [string]$JsonPath,
        [string]$MarkdownPath,
        [string]$Status,
        [string]$ManifestPath,
        [string]$WhitelistPath,
        [string]$RemoteName,
        [string]$BaseBranch,
        [string]$CurrentBranch,
        [string]$DefaultBranch,
        [string[]]$ProtectedBranchPatterns,
        [string[]]$WorktreeBranches,
        [string[]]$StashBranches,
        [object[]]$Operations,
        [object[]]$Issues
    )

    $summary = [ordered]@{
        schemaVersion = 1
        generated_at = (Get-Date).ToString("o")
        status = $Status
        manifest_path = $ManifestPath
        whitelist_path = $WhitelistPath
        remote_name = $RemoteName
        base_branch = $BaseBranch
        current_branch = $CurrentBranch
        default_branch = $DefaultBranch
        protected_branch_patterns = @($ProtectedBranchPatterns)
        worktree_branches = @($WorktreeBranches)
        stash_branches = @($StashBranches)
        operations = @($Operations)
        issues = @($Issues)
    }
    $summary | ConvertTo-Json -Depth 16 | Set-Content -Path $JsonPath -Encoding UTF8

    $lines = @(
        "# Remote Branch Delete Safety",
        "",
        "- status: ``$Status``",
        "- remote_name: ``$RemoteName``",
        "- base_branch: ``$BaseBranch``",
        "- current_branch: ``$CurrentBranch``",
        "- default_branch: ``$DefaultBranch``",
        ""
    )
    if ($Issues.Count -eq 0) {
        $lines += "- no issues"
    } else {
        $lines += "| Severity | Branch | Check | Message | Hint |"
        $lines += "|---|---|---|---|---|"
        foreach ($issue in $Issues) {
            $lines += "| $($issue.severity) | ``$($issue.branch)`` | $($issue.id) | $($issue.message) | $($issue.hint) |"
        }
    }
    $lines | Set-Content -Path $MarkdownPath -Encoding UTF8
}

$reportDirPath = Resolve-WorkspacePath -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $reportDirPath | Out-Null
$jsonPath = Join-Path $reportDirPath "remote-branch-delete-safety.json"
$mdPath = Join-Path $reportDirPath "remote-branch-delete-safety.md"
$manifestPath = Join-Path (Split-Path $reportDirPath -Parent) "pre-push-gate-manifest.json"

$manifest = $null
$deleteOps = @()
$whitelist = $null
$remoteName = "origin"
$baseBranch = "origin/main"
$protectedPatterns = @($defaultProtectedPatterns)
$defaultBranchName = ""
$currentBranch = ""
$worktreeBranches = @()
$stashBranches = @()
$issues = [System.Collections.Generic.List[object]]::new()
$operationResults = @()

try {
if (-not (Test-Path -LiteralPath $manifestPath)) {
    throw "pre-push manifest missing: $manifestPath"
}

$manifest = Get-Content -Raw -Path $manifestPath | ConvertFrom-Json
$deleteOps = @($manifest.operations | Where-Object { [string]$_.kind -eq "delete-ref" })
if ($deleteOps.Count -eq 0) {
    throw "delete-ref operations missing from pre-push manifest: $manifestPath"
}

if (Test-Path -LiteralPath $whitelistPath) {
    $whitelist = Get-Content -Raw -Path $whitelistPath | ConvertFrom-Json
}

$remoteName = if ($null -ne $whitelist -and -not [string]::IsNullOrWhiteSpace([string]$whitelist.remote)) {
    [string]$whitelist.remote
} elseif (-not [string]::IsNullOrWhiteSpace([string]$manifest.remote_name)) {
    [string]$manifest.remote_name
} else {
    "origin"
}
$baseBranch = if ($null -ne $whitelist -and -not [string]::IsNullOrWhiteSpace([string]$whitelist.base_branch)) {
    [string]$whitelist.base_branch
} else {
    "$remoteName/main"
}
$protectedPatterns = if ($null -ne $whitelist -and $null -ne $whitelist.protected_branch_patterns) {
    @($whitelist.protected_branch_patterns | ForEach-Object { [string]$_ })
} else {
    @($defaultProtectedPatterns)
}
$defaultBranchName = Get-DefaultBranchName -RemoteName $remoteName -BaseBranch $baseBranch
$currentBranchResult = Invoke-CommandLines -CommandName "git" -Arguments @("branch", "--show-current")
$currentBranch = if ($currentBranchResult.success -and $currentBranchResult.lines.Count -gt 0 -and $null -ne $currentBranchResult.lines[0]) {
    ([string]$currentBranchResult.lines[0]).Trim()
} else {
    ""
}
$worktreeBranches = @(Get-CurrentWorktreeBranches)
$stashBranches = @(Get-StashBranches)

foreach ($operation in $deleteOps) {
    $remoteRef = [string]$operation.remote_ref
    $branchName = ""
    $localBranchExists = $false
    $mergeState = $null
    $prState = $null
    $operationIssueCount = $issues.Count

    if ($remoteRef -notmatch '^refs/heads/(.+)$') {
        Add-Issue -Issues $issues -Id "unsupported-delete-ref" -Severity "error" -Branch $remoteRef -Message "Only remote branch deletion is supported by the managed pre-push contract." -Hint "Delete non-branch refs through a dedicated reviewed workflow."
        $operationResults += [pscustomobject]@{
            remote_ref = $remoteRef
            branch = $remoteRef
            status = "blocked"
            reason = "unsupported delete ref"
            local_branch_exists = $false
            merge_state = $null
            pr_state = $null
        }
        continue
    }

    $branchName = $Matches[1]
    $protectedMatch = Test-BranchMatchesProtectedPattern -BranchName $branchName -Patterns $protectedPatterns
    if (-not [string]::IsNullOrWhiteSpace($defaultBranchName) -and $branchName -eq $defaultBranchName) {
        Add-Issue -Issues $issues -Id "default-branch-delete" -Severity "error" -Branch $branchName -Message "Default branch cannot be deleted through pre-push cleanup." -Hint "Keep the remote default branch protected."
    }
    if (-not [string]::IsNullOrWhiteSpace($protectedMatch)) {
        Add-Issue -Issues $issues -Id "protected-branch-pattern" -Severity "error" -Branch $branchName -Message "Branch matches a protected branch pattern." -Hint "Remove it from the protected set only through an explicit authority change."
    }
    if ($branchName -eq $currentBranch) {
        Add-Issue -Issues $issues -Id "current-branch-delete" -Severity "error" -Branch $branchName -Message "Current branch cannot be deleted." -Hint "Switch away and close out the branch first."
    }
    if ($worktreeBranches -contains $branchName) {
        Add-Issue -Issues $issues -Id "checked-out-worktree" -Severity "error" -Branch $branchName -Message "Branch is checked out by an existing worktree." -Hint "Remove or close out the worktree before deleting the remote branch."
    }

    $localBranchExists = Test-LocalBranchExists -BranchName $branchName
    if ($localBranchExists) {
        if ($stashBranches -contains $branchName) {
            Add-Issue -Issues $issues -Id "branch-stash-residue" -Severity "error" -Branch $branchName -Message "Branch still has stash residue." -Hint "Classify and close out stash entries before deleting the remote branch."
        }
        $mergeState = Get-LocalBranchMergeState -BranchName $branchName -BaseBranch $baseBranch
        if (-not $mergeState.known) {
            Add-Issue -Issues $issues -Id "merge-state-unknown" -Severity "error" -Branch $branchName -Message "Unable to prove the local branch is disposable." -Hint "Confirm merge state against the authoritative base branch before deleting the remote branch."
        } elseif ($mergeState.ahead -gt 0) {
            Add-Issue -Issues $issues -Id "local-branch-ahead-base" -Severity "error" -Branch $branchName -Message "Local branch still has commits ahead of the authoritative base branch." -Hint "Close out or discard the remaining local branch work before deleting the remote branch."
        }
    }

    $prState = Get-PullRequestState -BranchName $branchName
    if (-not $prState.known) {
        Add-Issue -Issues $issues -Id "pr-state-unknown" -Severity "error" -Branch $branchName -Message "Unable to determine GitHub PR state for the remote branch." -Hint "Restore gh authentication or inspect PR state explicitly before deleting the branch."
    } elseif ($prState.state -eq "open") {
        Add-Issue -Issues $issues -Id "open-pr" -Severity "error" -Branch $branchName -Message "Remote branch still has an open PR." -Hint "Close or merge the PR before deleting the branch."
    } elseif ($prState.state -eq "unknown") {
        Add-Issue -Issues $issues -Id "pr-state-ambiguous" -Severity "error" -Branch $branchName -Message "PR state is ambiguous for the remote branch." -Hint "Resolve the PR state explicitly before deleting the branch."
    }

    $status = if ($issues.Count -eq $operationIssueCount) { "passed" } else { "blocked" }
    $reason = if ($status -eq "passed") { "remote branch deletion satisfied protected-branch, local residue, merge, and PR-state checks" } else { "see issues" }
    $operationResults += [pscustomobject]@{
        remote_ref = $remoteRef
        branch = $branchName
        status = $status
        reason = $reason
        local_branch_exists = $localBranchExists
        merge_state = $mergeState
        pr_state = $prState
    }
}

$status = if (($issues | Where-Object { $_.severity -eq "error" }).Count -eq 0) { "passed" } else { "failed" }
} catch {
    Add-Issue -Issues $issues -Id "script-exception" -Severity "error" -Branch "" -Message $_.Exception.Message -Hint "Inspect the script exception and restore fail-closed probe handling."
    $status = "failed"
}

Write-DeleteSafetyReport `
    -JsonPath $jsonPath `
    -MarkdownPath $mdPath `
    -Status $status `
    -ManifestPath $manifestPath `
    -WhitelistPath $whitelistPath `
    -RemoteName $remoteName `
    -BaseBranch $baseBranch `
    -CurrentBranch $currentBranch `
    -DefaultBranch $defaultBranchName `
    -ProtectedBranchPatterns $protectedPatterns `
    -WorktreeBranches $worktreeBranches `
    -StashBranches $stashBranches `
    -Operations $operationResults `
    -Issues $issues

Write-Output "remote-branch-delete-safety=$status report=$reportDirPath"
if ($status -ne "passed") { exit 1 }
