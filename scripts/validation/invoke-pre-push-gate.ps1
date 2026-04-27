[CmdletBinding()]
param(
    [string]$RemoteName = "",
    [string]$RemoteUrl = "",
    [ValidateSet("pre-push", "pr", "native", "hil", "release", "full-offline", "nightly")]
    [string]$Gate = "pre-push",
    [ValidateSet("all", "apps", "contracts", "protocol-compatibility", "integration", "e2e", "performance")]
    [string[]]$Suite = @("all"),
    [ValidateSet("quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "quick-gate",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "medium",
    [ValidateSet("quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "quick",
    [string]$ReportRoot = "tests\reports\pre-push-gate",
    [string[]]$ChangedScope = @(),
    [string[]]$SkipStep = @(),
    [string]$SkipJustification = "",
    [switch]$IncludeHardwareSmoke,
    [switch]$IncludeHilClosedLoop,
    [switch]$IncludeHilCaseMatrix,
    [switch]$EnablePythonCoverage,
    [switch]$EnableCppCoverage
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
Set-Location $workspaceRoot

function Fail-PushGate {
    param([string]$Message)
    Write-Error "pre-push gate failed: $Message"
}

function Expand-CommaSeparatedValues {
    param([string[]]$Values)
    $expanded = @()
    foreach ($value in @($Values)) {
        if ([string]::IsNullOrWhiteSpace($value)) { continue }
        $expanded += @(([string]$value).Split(",") | ForEach-Object { $_.Trim() } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) })
    }
    return @($expanded)
}

function Get-PrePushHookLines {
    $lines = @()
    if ([Console]::IsInputRedirected) {
        while ($null -ne ($line = [Console]::In.ReadLine())) {
            if (-not [string]::IsNullOrWhiteSpace($line)) {
                $lines += $line
            }
        }
    }
    return @($lines)
}

function Test-AllZeroSha {
    param([string]$Sha)
    return ($Sha -match '^0{40}$')
}

function Resolve-UpdateBase {
    param(
        [string]$RemoteRef,
        [string]$RemoteSha,
        [string]$LocalSha
    )

    if (-not (Test-AllZeroSha -Sha $RemoteSha)) {
        return $RemoteSha
    }

    if (-not [string]::IsNullOrWhiteSpace($RemoteRef)) {
        $tracking = $RemoteRef
        if ($RemoteRef -match '^refs/heads/(.+)$' -and -not [string]::IsNullOrWhiteSpace($RemoteName)) {
            $tracking = "$RemoteName/$($Matches[1])"
        }
        if (-not [string]::IsNullOrWhiteSpace($tracking)) {
            & git rev-parse --verify --quiet $tracking 2>$null | Out-Null
        }
        if (-not [string]::IsNullOrWhiteSpace($tracking) -and $LASTEXITCODE -eq 0) {
            return $tracking
        }
    }

    $defaultBaseRef = if ([string]::IsNullOrWhiteSpace($RemoteName)) { "origin/main" } else { "$RemoteName/main" }
    & git rev-parse --verify --quiet $defaultBaseRef 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        $mergeBase = (& git merge-base $defaultBaseRef $localSha 2>$null | Select-Object -First 1)
        if ($LASTEXITCODE -eq 0 -and -not [string]::IsNullOrWhiteSpace($mergeBase)) {
            return $mergeBase.Trim()
        }
    }

    return ""
}

function Get-PrePushOperationsFromHook {
    param([string[]]$Lines)

    $operations = @()
    foreach ($line in @($Lines)) {
        $parts = @($line -split '\s+')
        if ($parts.Count -lt 4) { continue }
        $localRef = $parts[0]
        $localSha = $parts[1]
        $remoteRef = $parts[2]
        $remoteSha = $parts[3]

        if (Test-AllZeroSha -Sha $localSha) {
            $operations += [pscustomobject]@{
                kind = "delete-ref"
                source = "pre-push-hook"
                local_ref = $localRef
                local_sha = $localSha
                remote_ref = $remoteRef
                remote_sha = $remoteSha
            }
            continue
        }

        $base = Resolve-UpdateBase -RemoteRef $remoteRef -RemoteSha $remoteSha -LocalSha $localSha
        $operations += [pscustomobject]@{
            kind = "update-ref"
            source = "pre-push-hook"
            local_ref = $localRef
            local_sha = $localSha
            remote_ref = $remoteRef
            remote_sha = $remoteSha
            base = $base
            head = $localSha
        }
    }
    return @($operations)
}

function Get-PrePushOperationFromUpstream {
    $upstream = ""
    $previousNativeCommandPreference = Get-Variable -Name PSNativeCommandUseErrorActionPreference -ErrorAction SilentlyContinue
    $previousErrorActionPreference = $ErrorActionPreference
    $ErrorActionPreference = "Continue"
    if ($null -ne $previousNativeCommandPreference) {
        $PSNativeCommandUseErrorActionPreference = $false
    }
    try {
        $upstreamOutput = @(git rev-parse --abbrev-ref --symbolic-full-name '@{upstream}' 2>$null)
    } finally {
        if ($null -ne $previousNativeCommandPreference) {
            $PSNativeCommandUseErrorActionPreference = [bool]$previousNativeCommandPreference.Value
        }
        $ErrorActionPreference = $previousErrorActionPreference
    }
    if ($LASTEXITCODE -eq 0 -and $upstreamOutput.Count -gt 0) {
        $upstream = ($upstreamOutput | Select-Object -First 1).Trim()
    }
    if ([string]::IsNullOrWhiteSpace($upstream)) {
        return $null
    }
    $head = (git rev-parse HEAD).Trim()
    return [pscustomobject]@{
        kind = "update-ref"
        source = "upstream"
        local_ref = "HEAD"
        local_sha = $head
        remote_ref = $upstream
        remote_sha = ""
        base = $upstream
        head = $head
    }
}

function Get-ChangedFilesForRanges {
    param($Ranges)
    $files = @()
    foreach ($range in @($Ranges)) {
        $rangeFiles = @(git diff --name-only $range.Base $range.Head)
        if ($LASTEXITCODE -ne 0) {
            Fail-PushGate "unable to diff pending range $($range.Base)..$($range.Head)."
        }
        $files += $rangeFiles
    }
    return @($files | ForEach-Object { ([string]$_).Replace('\', '/') } | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)
}

$unmerged = @(git diff --name-only --diff-filter=U)
if ($LASTEXITCODE -ne 0) {
    Fail-PushGate "unable to inspect unresolved conflict state."
}
if ($unmerged.Count -gt 0) {
    Fail-PushGate "unresolved conflict files remain: $($unmerged -join ', ')"
}

$timestamp = Get-Date -Format "yyyyMMdd-HHmmss"
$safeRemoteName = if ([string]::IsNullOrWhiteSpace($RemoteName)) { "unknown-remote" } else { $RemoteName -replace '[^A-Za-z0-9_.-]', '_' }
$reportDir = Join-Path $workspaceRoot (Join-Path $ReportRoot "$timestamp-$safeRemoteName")
New-Item -ItemType Directory -Force -Path $reportDir | Out-Null

$hookLines = Get-PrePushHookLines
$operations = @(Get-PrePushOperationsFromHook -Lines $hookLines)
if ($operations.Count -eq 0) {
    $upstreamOperation = Get-PrePushOperationFromUpstream
    if ($null -ne $upstreamOperation) {
        $operations = @($upstreamOperation)
    }
}

$updateOperations = @($operations | Where-Object { [string]$_.kind -eq "update-ref" })
$deleteOperations = @($operations | Where-Object { [string]$_.kind -eq "delete-ref" })
$ranges = @($updateOperations | Where-Object { -not [string]::IsNullOrWhiteSpace([string]$_.base) } | ForEach-Object {
    [pscustomobject]@{
        Base = [string]$_.base
        Head = [string]$_.head
        Source = [string]$_.source
        LocalRef = [string]$_.local_ref
        RemoteRef = [string]$_.remote_ref
    }
})

$changedFiles = @()
$rangeSource = ""
$baseSha = ""
$headSha = ""
if ($ranges.Count -gt 0) {
    $changedFiles = @(Get-ChangedFilesForRanges -Ranges $ranges)
    $rangeSource = ($ranges | Select-Object -First 1).Source
    $baseSha = ($ranges | Select-Object -First 1).Base
    $headSha = ($ranges | Select-Object -First 1).Head
} elseif ($deleteOperations.Count -eq 0) {
    $changedFiles = @(Expand-CommaSeparatedValues -Values $ChangedScope | ForEach-Object { ([string]$_).Replace('\', '/') } | Select-Object -Unique)
    if ($changedFiles.Count -eq 0) {
        Fail-PushGate "unable to determine pending push commit range from hook input or upstream; pass ChangedScope only for an explicit smoke run."
    }
    $rangeSource = "explicit-changed-scope-smoke"
    $baseSha = (git rev-parse HEAD).Trim()
    $headSha = $baseSha
    $operations = @(
        [pscustomobject]@{
            kind = "update-ref"
            source = $rangeSource
            local_ref = "HEAD"
            local_sha = $headSha
            remote_ref = ""
            remote_sha = ""
            base = $baseSha
            head = $headSha
        }
    )
} else {
    $rangeSource = "delete-ref-only"
}

if ($updateOperations.Count -gt 0 -and $changedFiles.Count -eq 0) {
    Fail-PushGate "pending push update range produced no changed files; refusing to silently pass."
}

$dirtyFiles = @(git status --porcelain | ForEach-Object {
    $line = [string]$_
    if ($line.Length -ge 4) { $line.Substring(3).Replace('\', '/') }
} | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)
$dirtyWorktree = $dirtyFiles.Count -gt 0
$dirtyPolicy = "clean-worktree"
if ($deleteOperations.Count -gt 0 -and $updateOperations.Count -eq 0) {
    $dirtyPolicy = if ($dirtyWorktree) { "not-applicable-delete-ref-only; manifest records dirty files" } else { "not-applicable-delete-ref-only" }
} elseif ($dirtyWorktree) {
    $overlap = @($dirtyFiles | Where-Object { $changedFiles -contains $_ })
    if ($overlap.Count -gt 0) {
        Fail-PushGate "dirty worktree overlaps pending push files: $($overlap -join ', '). Commit/stash those changes or run from an isolated worktree."
    }
    $dirtyPolicy = "allowed-disjoint-dirty-worktree; manifest records dirty files"
}

$classificationPath = ""
$classification = $null
$selectedSteps = @()
if ($changedFiles.Count -gt 0) {
    $classificationPath = Join-Path $reportDir "pre-push-classification.json"
    & (Join-Path $workspaceRoot "scripts\validation\classify-change.ps1") `
        -Mode pre-push `
        -OutputPath $classificationPath `
        -BaseSha $baseSha `
        -HeadSha $headSha `
        -ChangedFile $changedFiles
    if ($LASTEXITCODE -ne 0) {
        Fail-PushGate "classify-change.ps1 failed with $LASTEXITCODE."
    }
    $classification = Get-Content -Raw -Path $classificationPath | ConvertFrom-Json
    $selectedSteps = @($classification.selected_pre_push_steps)
    if ($selectedSteps.Count -eq 0) {
        Fail-PushGate "classification did not select any pre-push steps."
    }
}
if ($deleteOperations.Count -gt 0) {
    $selectedSteps += "remote-branch-delete-safety"
}
$selectedSteps = @($selectedSteps | Select-Object -Unique)
if ($selectedSteps.Count -eq 0) {
    Fail-PushGate "pre-push gate did not derive any executable steps."
}

$head = (git rev-parse HEAD).Trim()
$manifest = [ordered]@{
    schemaVersion = 3
    generated_at = (Get-Date).ToString("o")
    remote_name = $RemoteName
    remote_url = $RemoteUrl
    git_head = $head
    gate = $Gate
    lane = $Lane
    risk_profile = $RiskProfile
    desired_depth = $DesiredDepth
    suite = @($Suite)
    range_source = $rangeSource
    operations = @($operations)
    ranges = @($ranges)
    base_sha = $baseSha
    head_sha = $headSha
    changed_files = @($changedFiles)
    dirty_worktree = $dirtyWorktree
    dirty_files = @($dirtyFiles)
    dirty_policy = $dirtyPolicy
    categories = if ($null -eq $classification) { @() } else { @($classification.categories) }
    requires_native_followup = if ($null -eq $classification) { $false } else { [bool]$classification.requires_native_followup }
    requires_hil_followup = if ($null -eq $classification) { $false } else { [bool]$classification.requires_hil_followup }
    selected_pre_push_steps = @($selectedSteps)
    classification_path = $classificationPath
    skip_step = @($SkipStep)
    skip_justification = $SkipJustification
}
$manifest | ConvertTo-Json -Depth 12 | Set-Content -Path (Join-Path $reportDir "pre-push-gate-manifest.json") -Encoding UTF8

Write-Output "pre-push gate: remote=$RemoteName head=$head"
Write-Output "pre-push gate: range_source=$rangeSource base=$baseSha head=$headSha"
if ($null -eq $classification) {
    Write-Output "pre-push gate: categories="
} else {
    Write-Output "pre-push gate: categories=$($classification.categories -join ',')"
}
Write-Output "pre-push gate: selected_steps=$($selectedSteps -join ',')"
Write-Output "pre-push gate: report=$reportDir"

& (Join-Path $workspaceRoot "scripts\validation\invoke-gate.ps1") `
    -Gate $Gate `
    -ReportRoot $reportDir `
    -Suite $Suite `
    -Lane $Lane `
    -RiskProfile $RiskProfile `
    -DesiredDepth $DesiredDepth `
    -ChangedScope $changedFiles `
    -SelectedStep $selectedSteps `
    -ClassificationPath $classificationPath `
    -BaseSha $baseSha `
    -HeadSha $headSha `
    -DirtyWorktree:$dirtyWorktree `
    -DirtyPolicy $dirtyPolicy `
    -SkipStep $SkipStep `
    -SkipJustification $SkipJustification `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke `
    -IncludeHilClosedLoop:$IncludeHilClosedLoop `
    -IncludeHilCaseMatrix:$IncludeHilCaseMatrix `
    -EnablePythonCoverage:$EnablePythonCoverage `
    -EnableCppCoverage:$EnableCppCoverage

if ($LASTEXITCODE -ne 0) {
    Fail-PushGate "invoke-gate.ps1 exited with $LASTEXITCODE. See report: $reportDir"
}

Write-Output "pre-push gate passed"
