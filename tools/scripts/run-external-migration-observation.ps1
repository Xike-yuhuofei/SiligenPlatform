param(
    [string]$PythonExe,
    [string]$FieldScriptsPath,
    [string]$ReleasePackagePath,
    [string]$RollbackPackagePath,
    [string]$ReportRoot = "integration\\reports\\verify\\wave4e-rerun"
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$legacyPattern = 'dxf-to-pb|path-to-trajectory|export-simulation-input|generate-dxf-preview|dxf_pipeline|proto\.dxf_primitives_pb2|config\\machine_config\.ini'
$rgCommand = Get-Command rg -ErrorAction SilentlyContinue

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

function Invoke-ExternalProcessCapture {
    param(
        [string]$FilePath,
        [string[]]$Arguments
    )

    $lines = & $FilePath @Arguments 2>&1
    $exitCode = $LASTEXITCODE
    $text = ($lines | Out-String).Trim()
    if ([string]::IsNullOrWhiteSpace($text)) {
        $text = "<empty>"
    }

    return [ordered]@{
        exit_code = $exitCode
        output = $text
        command = (@($FilePath) + $Arguments) -join " "
    }
}

function Get-ScopeMeta {
    param([string]$Scope)

    switch ($Scope) {
        "field-scripts" {
            return [ordered]@{
                title = "Field Scripts"
                target_label = "field scripts snapshot"
                globs = @("*.ps1", "*.py", "*.bat", "*.cmd", "*.scr", "*.ini", "*.json", "*.yaml", "*.yml", "*.xml", "*.cfg", "*.conf", "*.toml", "*.txt", "*.md")
                missing_detail = "未提供真实现场脚本目录或脚本快照"
                missing_next = "collect actual field script snapshot, then rerun observation"
                banned_detail = "现场脚本或部署脚本仍包含 legacy DXF 入口或 deleted alias"
                go_next = "field scripts observation passed"
            }
        }
        "release-package" {
            return [ordered]@{
                title = "Release Package"
                target_label = "release package / unpacked release root"
                globs = @("*.ps1", "*.py", "*.bat", "*.cmd", "*.scr", "*.ini", "*.json", "*.yaml", "*.yml", "*.xml", "*.cfg", "*.conf", "*.toml", "*.txt", "*.md")
                missing_detail = "未提供可扫描的 release package 根目录"
                missing_next = "provide unpacked release package or release evidence root, then rerun observation"
                banned_detail = "发布包仍包含 deleted alias、compat shell 或 legacy DXF 入口"
                go_next = "release package observation passed"
            }
        }
        "rollback-package" {
            return [ordered]@{
                title = "Rollback Package"
                target_label = "rollback package / backup pack / rollback SOP bundle"
                globs = @("*.ps1", "*.py", "*.bat", "*.cmd", "*.scr", "*.ini", "*.json", "*.yaml", "*.yml", "*.xml", "*.cfg", "*.conf", "*.toml", "*.txt", "*.md")
                missing_detail = "未提供可扫描的 rollback package、备份包或 SOP 快照目录"
                missing_next = "provide rollback bundle or SOP snapshot directory, then rerun observation"
                banned_detail = "回滚材料仍依赖 deleted alias、compat shell 或 legacy DXF 入口"
                go_next = "rollback package observation passed"
            }
        }
        default { throw "unsupported scope: $Scope" }
    }
}

function Get-IntakeJsonPath {
    param([string]$Scope)

    return Join-Path $script:resolvedIntakeDir ($Scope + ".json")
}

function Load-IntakeEntry {
    param([string]$Scope)

    $jsonPath = Get-IntakeJsonPath -Scope $Scope
    if (-not (Test-Path -LiteralPath $jsonPath)) {
        return $null
    }

    return Get-Content -Raw $jsonPath | ConvertFrom-Json
}

function Resolve-ObservationSourcePath {
    param(
        [string]$ExplicitPath,
        [string]$Scope
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitPath)) {
        if (Test-Path -LiteralPath $ExplicitPath) {
            return (Resolve-Path -LiteralPath $ExplicitPath).Path
        }

        return [System.IO.Path]::GetFullPath($ExplicitPath)
    }

    $intakeEntry = Load-IntakeEntry -Scope $Scope
    if ($null -ne $intakeEntry -and -not [string]::IsNullOrWhiteSpace($intakeEntry.source_path)) {
        return [string]$intakeEntry.source_path
    }

    return $null
}

function Get-RelativeInventory {
    param([string]$TargetPath)

    $items = Get-ChildItem -LiteralPath $TargetPath -Recurse -Force
    $inventory = @()
    $basePath = ((Resolve-Path -LiteralPath $TargetPath).Path).TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    foreach ($item in $items) {
        $fullName = $item.FullName
        if ($fullName.StartsWith($basePath, [System.StringComparison]::OrdinalIgnoreCase)) {
            $relative = $fullName.Substring($basePath.Length)
        } else {
            $relative = $fullName
        }

        $relative = $relative -replace '\\', '/'
        $inventory += $relative
    }

    return $inventory
}

function Get-BannedPathHits {
    param([string[]]$Inventory)

    $hits = New-Object System.Collections.Generic.List[string]
    foreach ($relative in $Inventory) {
        if ([string]::IsNullOrWhiteSpace($relative)) {
            continue
        }

        $trimmed = ($relative -replace '\\', '/').TrimStart('/')
        if ($trimmed -eq "config/machine_config.ini") {
            $hits.Add($trimmed)
        }
        if ($trimmed -match '(^|/)dxf_pipeline(/|$)') {
            $hits.Add($trimmed)
        }
        if ($trimmed -match '(^|/)proto/dxf_primitives_pb2\.py$') {
            $hits.Add($trimmed)
        }
    }

    return $hits | Sort-Object -Unique
}

function Invoke-ContentScan {
    param(
        [string]$TargetPath,
        [string[]]$Globs,
        [string]$OutputPath
    )

    if ($null -ne $rgCommand) {
        $arguments = @("-n", $legacyPattern, $TargetPath)
        foreach ($glob in $Globs) {
            $arguments += @("-g", $glob)
        }

        $result = Invoke-ExternalProcessCapture -FilePath $rgCommand.Source -Arguments $arguments
        $result.output | Set-Content -Path $OutputPath -Encoding utf8
        $result["matched"] = ($result.exit_code -eq 0)
        return $result
    }

    $extensions = $Globs | ForEach-Object { $_.TrimStart('*') }
    $files = Get-ChildItem -LiteralPath $TargetPath -Recurse -File | Where-Object { $extensions -contains $_.Extension }
    $matchLines = New-Object System.Collections.Generic.List[string]
    foreach ($file in $files) {
        $matches = Select-String -LiteralPath $file.FullName -Pattern $legacyPattern
        foreach ($match in $matches) {
            $matchLines.Add(("{0}:{1}:{2}" -f $match.Path, $match.LineNumber, $match.Line.Trim()))
        }
    }

    $text = if ($matchLines.Count -gt 0) { $matchLines -join "`r`n" } else { "<no matches>" }
    $text | Set-Content -Path $OutputPath -Encoding utf8
    return [ordered]@{
        exit_code = $(if ($matchLines.Count -gt 0) { 0 } else { 1 })
        output = $text
        command = "Select-String fallback scan"
        matched = ($matchLines.Count -gt 0)
    }
}

function Write-ObservationFile {
    param(
        [string]$Scope,
        [string]$TargetPath,
        [string]$Result,
        [string]$Detail,
        [string]$Evidence,
        [string]$NextAction,
        [string]$CommandText,
        [string]$Notes,
        [string]$OutputPath,
        [string]$TargetLabel
    )

    $title = switch ($Scope) {
        "external-launcher" { "External Launcher" }
        "field-scripts" { "Field Scripts" }
        "release-package" { "Release Package" }
        "rollback-package" { "Rollback Package" }
        default { throw "unsupported scope: $Scope" }
    }

    $lines = @()
    $lines += ('# Wave 4E {0} Observation' -f $title)
    $lines += ""
    $lines += ('- date: `{0}`' -f (Get-Date -Format "yyyy-MM-dd"))
    $lines += ('- target: `{0}`' -f $(if ([string]::IsNullOrWhiteSpace($TargetPath)) { $TargetLabel } else { $TargetPath }))
    $lines += ""
    $lines += "## 1. 执行摘要"
    $lines += ""
    if (-not [string]::IsNullOrWhiteSpace($CommandText)) {
        $lines += '```text'
        $lines += $CommandText
        $lines += '```'
        $lines += ""
    }
    if (-not [string]::IsNullOrWhiteSpace($Notes)) {
        $lines += $Notes
        $lines += ""
    }
    $lines += "## 2. 裁决"
    $lines += ""
    $lines += '```text'
    $lines += ('observation_result = {0}' -f $Result)
    $lines += ('scope = {0}' -f $Scope)
    $lines += ('evidence = {0}' -f $Evidence)
    $lines += ('next_action = {0}' -f $NextAction)
    $lines += '```'
    $lines += ""
    $lines += "## 3. 说明"
    $lines += ""
    $lines += ('1. {0}' -f $Detail)

    $lines -join "`r`n" | Set-Content -Path $OutputPath -Encoding utf8
}

function Invoke-LauncherObservation {
    param([string]$PythonValue)

    $helperPath = Join-Path $PSScriptRoot "verify-engineering-data-cli.ps1"
    $launcherDir = Join-Path $script:resolvedObservationDir "launcher-cli-check"
    New-Item -ItemType Directory -Force -Path $launcherDir | Out-Null

    $arguments = @("-NoProfile", "-ExecutionPolicy", "Bypass", "-File", $helperPath, "-ReportDir", $launcherDir)
    if (-not [string]::IsNullOrWhiteSpace($PythonValue)) {
        $arguments += @("-PythonExe", $PythonValue)
    }

    $invocation = Invoke-ExternalProcessCapture -FilePath "powershell" -Arguments $arguments
    $jsonPath = Join-Path $launcherDir "engineering-data-cli-check.json"
    $mdPath = Join-Path $launcherDir "engineering-data-cli-check.md"
    $detail = "目标 Python 解释器无法运行 canonical engineering-data module CLI"
    $nextAction = "fix target Python environment, then rerun launcher observation"
    $result = "No-Go"
    $notes = @()
    $notes += ('- helper_command: `{0}`' -f $invocation.command)
    $notes += ('- helper_exit_code: `{0}`' -f $invocation.exit_code)
    $notes += ('- helper_report: `{0}`' -f (Get-DisplayPath -PathValue $mdPath))
    $notes += ""
    $notes += "## Helper Output"
    $notes += ""
    $notes += '```text'
    $notes += $invocation.output
    $notes += '```'

    if (Test-Path -LiteralPath $jsonPath) {
        $payload = Get-Content -Raw $jsonPath | ConvertFrom-Json
        if ($payload.required_checks_passed) {
            $result = "Go"
            $detail = "目标 Python 解释器可以运行 canonical engineering-data module CLI"
            $nextAction = "launcher observation passed"
        } else {
            $detail = "目标 Python 解释器 required checks 未全部通过"
        }
    } elseif (-not [string]::IsNullOrWhiteSpace($PythonValue)) {
        $detail = "未生成 launcher helper 证据，需检查 PythonExe 或 helper 执行失败"
    }

    $outputPath = Join-Path $script:resolvedObservationDir "external-launcher.md"
    Write-ObservationFile `
        -Scope "external-launcher" `
        -TargetPath $(if ([string]::IsNullOrWhiteSpace($PythonValue)) { "python" } else { $PythonValue }) `
        -Result $result `
        -Detail $detail `
        -Evidence (Get-DisplayPath -PathValue $mdPath) `
        -NextAction $nextAction `
        -CommandText $invocation.command `
        -Notes ($notes -join "`r`n") `
        -OutputPath $outputPath `
        -TargetLabel "external launcher environment"

    return [ordered]@{
        scope = "external launcher"
        result = $result
        evidence = "external-launcher.md"
        next_action = $nextAction
        detail = $detail
    }
}

function Invoke-ArtifactObservation {
    param(
        [string]$Scope,
        [string]$ExplicitPath
    )

    $meta = Get-ScopeMeta -Scope $Scope
    $resolvedPath = Resolve-ObservationSourcePath -ExplicitPath $ExplicitPath -Scope $Scope
    $outputPath = Join-Path $script:resolvedObservationDir ($Scope + ".md")
    $scanOutputPath = Join-Path $script:resolvedObservationDir ($Scope + ".scan.txt")
    $pathHitsOutputPath = Join-Path $script:resolvedObservationDir ($Scope + ".path-hits.txt")

    if ([string]::IsNullOrWhiteSpace($resolvedPath)) {
        Write-ObservationFile `
            -Scope $Scope `
            -TargetPath $null `
            -Result "input-gap" `
            -Detail $meta.missing_detail `
            -Evidence "no source_path provided in args or intake json" `
            -NextAction $meta.missing_next `
            -CommandText "<none>" `
            -Notes "" `
            -OutputPath $outputPath `
            -TargetLabel $meta.target_label

        return [ordered]@{
            scope = $Scope -replace '-', ' '
            result = "input-gap"
            evidence = [System.IO.Path]::GetFileName($outputPath)
            next_action = $meta.missing_next
            detail = $meta.missing_detail
        }
    }

    if (-not (Test-Path -LiteralPath $resolvedPath)) {
        Write-ObservationFile `
            -Scope $Scope `
            -TargetPath $resolvedPath `
            -Result "input-gap" `
            -Detail "source_path 不存在，无法执行仓外观察" `
            -Evidence $resolvedPath `
            -NextAction $meta.missing_next `
            -CommandText "<none>" `
            -Notes "" `
            -OutputPath $outputPath `
            -TargetLabel $meta.target_label

        return [ordered]@{
            scope = $Scope -replace '-', ' '
            result = "input-gap"
            evidence = [System.IO.Path]::GetFileName($outputPath)
            next_action = $meta.missing_next
            detail = "source_path 不存在，无法执行仓外观察"
        }
    }

    if (-not (Test-Path -LiteralPath $resolvedPath -PathType Container)) {
        Write-ObservationFile `
            -Scope $Scope `
            -TargetPath $resolvedPath `
            -Result "input-gap" `
            -Detail "当前 source_path 是单文件归档或单文件证据，需提供可扫描的目录根" `
            -Evidence $resolvedPath `
            -NextAction $meta.missing_next `
            -CommandText "<none>" `
            -Notes "" `
            -OutputPath $outputPath `
            -TargetLabel $meta.target_label

        return [ordered]@{
            scope = $Scope -replace '-', ' '
            result = "input-gap"
            evidence = [System.IO.Path]::GetFileName($outputPath)
            next_action = $meta.missing_next
            detail = "当前 source_path 是单文件归档或单文件证据，需提供可扫描的目录根"
        }
    }

    $scanResult = Invoke-ContentScan -TargetPath $resolvedPath -Globs $meta.globs -OutputPath $scanOutputPath
    $inventory = Get-RelativeInventory -TargetPath $resolvedPath
    $pathHits = Get-BannedPathHits -Inventory $inventory
    $pathHitsText = if ($pathHits.Count -gt 0) { $pathHits -join "`r`n" } else { "<no path hits>" }
    $pathHitsText | Set-Content -Path $pathHitsOutputPath -Encoding utf8

    $hasLegacyContent = [bool]$scanResult.matched
    $hasLegacyPaths = $pathHits.Count -gt 0
    $result = if ($hasLegacyContent -or $hasLegacyPaths) { "No-Go" } else { "Go" }
    $detail = if ($result -eq "Go") { "未发现 legacy DXF 入口、compat shell 或 deleted alias 命中" } else { $meta.banned_detail }
    $nextAction = if ($result -eq "Go") { $meta.go_next } else { "replace affected external artifact, then rerun observation" }

    $notes = @()
    $notes += ('- scan_command: `{0}`' -f $scanResult.command)
    $notes += ('- scan_output: `{0}`' -f (Get-DisplayPath -PathValue $scanOutputPath))
    $notes += ('- path_hits: `{0}`' -f (Get-DisplayPath -PathValue $pathHitsOutputPath))
    $notes += ('- legacy_content_hits: `{0}`' -f $hasLegacyContent)
    $notes += ('- legacy_path_hits: `{0}`' -f $hasLegacyPaths)

    Write-ObservationFile `
        -Scope $Scope `
        -TargetPath $resolvedPath `
        -Result $result `
        -Detail $detail `
        -Evidence ([string]::Join(", ", @((Get-DisplayPath -PathValue $scanOutputPath), (Get-DisplayPath -PathValue $pathHitsOutputPath)))) `
        -NextAction $nextAction `
        -CommandText $scanResult.command `
        -Notes ($notes -join "`r`n") `
        -OutputPath $outputPath `
        -TargetLabel $meta.target_label

    return [ordered]@{
        scope = $Scope -replace '-', ' '
        result = $result
        evidence = [System.IO.Path]::GetFileName($outputPath)
        next_action = $nextAction
        detail = $detail
    }
}

function Write-SummaryFile {
    param([object[]]$Results)

    $summaryPath = Join-Path $script:resolvedObservationDir "summary.md"
    $overallGo = @($Results | Where-Object { $_.result -ne "Go" }).Count -eq 0
    $summaryResult = if ($overallGo) { "Go" } else { "No-Go" }
    $nextAction = if ($overallGo) { "prepare Wave 4E closeout and final declaration" } else { "fix blockers or collect missing inputs, then rerun Wave 4E observation" }
    $observationEvidenceRoot = (Get-DisplayPath -PathValue $script:resolvedObservationDir) + "\*"

    $lines = @()
    $lines += "# Wave 4E Observation Summary"
    $lines += ""
    $lines += ('- date: `{0}`' -f (Get-Date -Format "yyyy-MM-dd"))
    $lines += ""
    $lines += "| Scope | Result | Evidence | Next Action |"
    $lines += "|---|---|---|---|"
    foreach ($result in $Results) {
        $lines += ('| {0} | `{1}` | `{2}` | {3} |' -f $result.scope, $result.result, $result.evidence, $result.next_action)
    }
    $lines += ""
    $lines += "## 结论模板落地"
    $lines += ""
    $lines += '```text'
    $lines += ('observation_result = {0}' -f $summaryResult)
    $lines += "scope = external observation summary"
    $lines += ('evidence = {0}' -f $observationEvidenceRoot)
    $lines += ('next_action = {0}' -f $nextAction)
    $lines += '```'
    $lines += ""
    $lines += "## 裁决"
    $lines += ""
    $lines += ('1. `external observation execution = {0}`' -f $summaryResult)
    $lines += ('2. `external migration complete declaration = {0}`' -f $summaryResult)
    $lines += ('3. `next-wave planning = {0}`' -f $summaryResult)

    $lines -join "`r`n" | Set-Content -Path $summaryPath -Encoding utf8
}

function Write-BlockersFile {
    param([object[]]$Results)

    $blockersPath = Join-Path $script:resolvedObservationDir "blockers.md"
    $blockers = @($Results | Where-Object { $_.result -ne "Go" })
    $lines = @()
    $lines += "# Wave 4E Observation Blockers"
    $lines += ""
    $lines += ('- date: `{0}`' -f (Get-Date -Format "yyyy-MM-dd"))
    $lines += ""
    $lines += "## Blockers"
    $lines += ""

    if ($blockers.Count -eq 0) {
        $lines += '1. 无 blocker，四个 scope 均为 `Go`。'
    } else {
        $index = 1
        foreach ($blocker in $blockers) {
            $blockerId = ($blocker.scope -replace ' ', '-') + '-' + ($blocker.result.ToLowerInvariant())
            $lines += ('{0}. `{1}`' -f $index, $blockerId)
            $lines += ('   - type：`{0}`' -f $blocker.result)
            $lines += ('   - evidence：`observation/{0}`' -f $blocker.evidence)
            $lines += ('   - detail：{0}' -f $blocker.detail)
            $index += 1
        }
    }

    $lines += ""
    $lines += "## 处理规则"
    $lines += ""
    $lines += '1. 任何 blocker 存在时，`external migration complete declaration` 必须保持 `No-Go`'
    $lines += '2. `input-gap` 必须先补真实外部输入，再重跑本阶段'
    $lines += '3. `No-Go` 必须替换受影响仓外交付物或脚本后再重验'

    $lines -join "`r`n" | Set-Content -Path $blockersPath -Encoding utf8
}

$script:resolvedReportRoot = Resolve-OutputPath -PathValue $ReportRoot
$script:resolvedIntakeDir = Join-Path $script:resolvedReportRoot "intake"
$script:resolvedObservationDir = Join-Path $script:resolvedReportRoot "observation"
New-Item -ItemType Directory -Force -Path $script:resolvedIntakeDir | Out-Null
New-Item -ItemType Directory -Force -Path $script:resolvedObservationDir | Out-Null

$launcherResult = Invoke-LauncherObservation -PythonValue $PythonExe
$fieldScriptsResult = Invoke-ArtifactObservation -Scope "field-scripts" -ExplicitPath $FieldScriptsPath
$releasePackageResult = Invoke-ArtifactObservation -Scope "release-package" -ExplicitPath $ReleasePackagePath
$rollbackPackageResult = Invoke-ArtifactObservation -Scope "rollback-package" -ExplicitPath $RollbackPackagePath

$results = @($launcherResult, $fieldScriptsResult, $releasePackageResult, $rollbackPackageResult)
Write-SummaryFile -Results $results
Write-BlockersFile -Results $results

$hasBlockers = @($results | Where-Object { $_.result -ne "Go" }).Count -gt 0
Write-Output "external migration observation complete"
Write-Output ("report root: {0}" -f $script:resolvedReportRoot)
Write-Output ("summary: {0}" -f (Join-Path $script:resolvedObservationDir "summary.md"))
Write-Output ("blockers: {0}" -f (Join-Path $script:resolvedObservationDir "blockers.md"))

if ($hasBlockers) {
    exit 1
}
