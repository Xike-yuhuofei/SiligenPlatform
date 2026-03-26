param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+(-rc\.\d+)?$')]
    [string]$Version,
    [string]$ReportDir,
    [switch]$IncludeHardwareSmoke,
    [switch]$AllowRuntimeBlocked,
    [switch]$AllowCliBlocked
)

$ErrorActionPreference = "Stop"

$workspaceRoot = $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($ReportDir)) {
    $ReportDir = "tests\\reports\\releases\\$Version"
}

$resolvedReportDir = Join-Path $workspaceRoot $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

function Write-Section {
    param(
        [string]$Title
    )

    Write-Output ""
    Write-Output "== $Title =="
}

function Invoke-And-Capture {
    param(
        [string]$Label,
        [scriptblock]$Command,
        [string]$OutputFile
    )

    Write-Section $Label
    $text = (& $Command 2>&1 | Out-String).Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        $text = "<no output>"
    }

    Set-Content -Path $OutputFile -Value $text
    Write-Output $text
    return $text
}

function Test-HeadExists {
    git rev-parse --verify HEAD *> $null
    return ($LASTEXITCODE -eq 0)
}

function Assert-CleanWorktree {
    $status = (git status --porcelain | Out-String).Trim()
    if (-not [string]::IsNullOrWhiteSpace($status)) {
        Write-Error "工作树不干净，发布边界不可审计。请先收敛并提交/清理本次发布范围内的改动，再执行 release-check。"
    }
}

function Assert-ChangelogEntry {
    param(
        [string]$ChangelogPath,
        [string]$ExpectedVersion
    )

    if (-not (Test-Path $ChangelogPath)) {
        Write-Error "未找到 CHANGELOG.md：$ChangelogPath"
    }

    $content = Get-Content -Raw $ChangelogPath
    $escaped = [regex]::Escape($ExpectedVersion)
    if ($content -notmatch "(?m)^## \[$escaped\]") {
        Write-Error "CHANGELOG.md 尚未包含版本 '$ExpectedVersion' 的正式条目。请先把 Unreleased 整理成目标版本条目。"
    }
}

function Assert-NotBlocked {
    param(
        [string]$Name,
        [string]$Output,
        [switch]$Allowed
    )

    if ($Output -match 'target:\s*BLOCKED') {
        if ($Allowed) {
            Write-Warning "$Name 当前为 BLOCKED，但本次检查已显式允许。"
            return
        }

        Write-Error "$Name 当前为 BLOCKED，release gate 未通过。"
    }
}

function Assert-VersionPolicy {
    param(
        [string]$Version,
        [switch]$IncludeHardwareSmoke,
        [switch]$AllowRuntimeBlocked,
        [switch]$AllowCliBlocked
    )

    $isReleaseCandidate = $Version -match '-rc\.'
    if (-not $isReleaseCandidate) {
        if (-not $IncludeHardwareSmoke) {
            Write-Error "正式版本发布必须启用 -IncludeHardwareSmoke。"
        }

        if ($AllowRuntimeBlocked -or $AllowCliBlocked) {
            Write-Error "正式版本发布不允许使用 blocked 豁免开关。"
        }
    }
}

function Assert-FileExists {
    param(
        [string]$Path,
        [string]$Label
    )

    if (-not (Test-Path $Path)) {
        Write-Error "$Label 缺失：$Path"
    }
}

if (-not (Test-HeadExists)) {
    Write-Error "根仓当前没有可打 tag 的提交。请先建立首个基线提交，再执行 release-check。"
}

Assert-CleanWorktree
Assert-VersionPolicy `
    -Version $Version `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke `
    -AllowRuntimeBlocked:$AllowRuntimeBlocked `
    -AllowCliBlocked:$AllowCliBlocked

$changelogPath = Join-Path $workspaceRoot "CHANGELOG.md"
Assert-ChangelogEntry -ChangelogPath $changelogPath -ExpectedVersion $Version

$formalGatewayContractGuard = Join-Path $workspaceRoot "scripts\validation\assert-hmi-formal-gateway-launch-contract.ps1"
if (-not (Test-Path $formalGatewayContractGuard)) {
    throw "HMI formal gateway contract guard not found: $formalGatewayContractGuard"
}

Write-Output "hmi formal gateway contract gate: powershell -NoProfile -ExecutionPolicy Bypass -File $formalGatewayContractGuard"
& $formalGatewayContractGuard -WorkspaceRoot $workspaceRoot

$ciReportDir = Join-Path $ReportDir "ci"
$legacyExitReportDir = Join-Path $ReportDir "legacy-exit"

Write-Section "legacy-exit"
& (Join-Path $workspaceRoot "legacy-exit-check.ps1") -Profile Local -ReportDir $legacyExitReportDir

Write-Section "build"
& (Join-Path $workspaceRoot "build.ps1") -Profile CI -Suite all

Write-Section "test"
& (Join-Path $workspaceRoot "test.ps1") `
    -Profile CI `
    -Suite all `
    -ReportDir $ciReportDir `
    -FailOnKnownFailure `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke

Assert-FileExists -Path (Join-Path $workspaceRoot "tests\\reports\\performance\\latest.json") -Label "性能基线 JSON 证据"
Assert-FileExists -Path (Join-Path $workspaceRoot "tests\\reports\\performance\\latest.md") -Label "性能基线 Markdown 证据"

$hmiOutput = Invoke-And-Capture `
    -Label "hmi-app dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\hmi-app\\run.ps1") -DryRun -DisableGatewayAutostart } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-hmi-app.txt")

$tcpOutput = Invoke-And-Capture `
    -Label "runtime-gateway dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\runtime-gateway\\run.ps1") -DryRun } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-runtime-gateway.txt")

$cliOutput = Invoke-And-Capture `
    -Label "planner-cli dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\planner-cli\\run.ps1") -DryRun } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-planner-cli.txt")

$runtimeOutput = Invoke-And-Capture `
    -Label "runtime-service dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\runtime-service\\run.ps1") -DryRun } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-runtime-service.txt")

Assert-NotBlocked -Name "hmi-app" -Output $hmiOutput
Assert-NotBlocked -Name "runtime-gateway" -Output $tcpOutput
Assert-NotBlocked -Name "planner-cli" -Output $cliOutput -Allowed:$AllowCliBlocked
Assert-NotBlocked -Name "runtime-service" -Output $runtimeOutput -Allowed:$AllowRuntimeBlocked

$head = (git rev-parse HEAD).Trim()
$manifest = @(
    "version: $Version",
    "git-head: $head",
    "release-standard: docs/runtime/release-readiness-standard.md",
    "report-dir: $ReportDir",
    "ci-report-dir: $ciReportDir",
    "legacy-exit-report-dir: $legacyExitReportDir",
    "include-hardware-smoke: $($IncludeHardwareSmoke.IsPresent)",
    "allow-cli-blocked: $($AllowCliBlocked.IsPresent)",
    "allow-runtime-blocked: $($AllowRuntimeBlocked.IsPresent)",
    "worktree-clean-at-start: True"
)

Set-Content -Path (Join-Path $resolvedReportDir "release-manifest.txt") -Value $manifest

Write-Section "result"
Write-Output "release-check passed"
Write-Output "version: $Version"
Write-Output "evidence: $resolvedReportDir"
