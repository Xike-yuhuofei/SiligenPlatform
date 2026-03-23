param(
    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+(-rc\.\d+)?$')]
    [string]$Version,
    [string]$ReportDir,
    [switch]$IncludeHardwareSmoke,
    [switch]$AllowRuntimeBlocked,
    [switch]$AllowCliBlocked,
    [string]$LocalValidationReportRoot,
    [switch]$SkipLocalValidationGate
)

$ErrorActionPreference = "Stop"

$workspaceRoot = $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($ReportDir)) {
    $ReportDir = "integration\\reports\\releases\\$Version"
}

$resolvedReportDir = Join-Path $workspaceRoot $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

if ([string]::IsNullOrWhiteSpace($LocalValidationReportRoot)) {
    $LocalValidationReportRoot = Join-Path $resolvedReportDir "local-validation-gate"
}

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

if (-not (Test-HeadExists)) {
    Write-Error "根仓当前没有可打 tag 的提交。请先建立首个基线提交，再执行 release-check。"
}

$changelogPath = Join-Path $workspaceRoot "CHANGELOG.md"
Assert-ChangelogEntry -ChangelogPath $changelogPath -ExpectedVersion $Version

$ciReportDir = Join-Path $ReportDir "ci"

if (-not $SkipLocalValidationGate) {
    Write-Section "local validation gate"
    & (Join-Path $workspaceRoot "tools\\scripts\\run-local-validation-gate.ps1") `
        -ReportRoot $LocalValidationReportRoot
} else {
    Write-Section "local validation gate"
    Write-Output "skipped: SkipLocalValidationGate=true"
}

Write-Section "build"
& (Join-Path $workspaceRoot "build.ps1") -Profile CI -Suite all

Write-Section "test"
& (Join-Path $workspaceRoot "test.ps1") `
    -Profile CI `
    -Suite all `
    -ReportDir $ciReportDir `
    -FailOnKnownFailure `
    -IncludeHardwareSmoke:$IncludeHardwareSmoke

$hmiOutput = Invoke-And-Capture `
    -Label "hmi-app dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\hmi-app\\run.ps1") -DryRun -DisableGatewayAutostart } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-hmi-app.txt")

$tcpOutput = Invoke-And-Capture `
    -Label "control-tcp-server dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\control-tcp-server\\run.ps1") -DryRun } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-control-tcp-server.txt")

$cliOutput = Invoke-And-Capture `
    -Label "control-cli dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\control-cli\\run.ps1") -DryRun } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-control-cli.txt")

$runtimeOutput = Invoke-And-Capture `
    -Label "control-runtime dry-run" `
    -Command { & (Join-Path $workspaceRoot "apps\\control-runtime\\run.ps1") -DryRun } `
    -OutputFile (Join-Path $resolvedReportDir "dryrun-control-runtime.txt")

Assert-NotBlocked -Name "hmi-app" -Output $hmiOutput
Assert-NotBlocked -Name "control-tcp-server" -Output $tcpOutput
Assert-NotBlocked -Name "control-cli" -Output $cliOutput -Allowed:$AllowCliBlocked
Assert-NotBlocked -Name "control-runtime" -Output $runtimeOutput -Allowed:$AllowRuntimeBlocked

$head = (git rev-parse HEAD).Trim()
$manifest = @(
    "version: $Version",
    "git-head: $head",
    "report-dir: $ReportDir",
    "ci-report-dir: $ciReportDir",
    "local-validation-report-root: $LocalValidationReportRoot",
    "skip-local-validation-gate: $($SkipLocalValidationGate.IsPresent)",
    "include-hardware-smoke: $($IncludeHardwareSmoke.IsPresent)",
    "allow-cli-blocked: $($AllowCliBlocked.IsPresent)",
    "allow-runtime-blocked: $($AllowRuntimeBlocked.IsPresent)"
)

Set-Content -Path (Join-Path $resolvedReportDir "release-manifest.txt") -Value $manifest

Write-Section "result"
Write-Output "release-check passed"
Write-Output "version: $Version"
Write-Output "evidence: $resolvedReportDir"
