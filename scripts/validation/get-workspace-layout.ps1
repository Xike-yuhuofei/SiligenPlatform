param(
    [string]$WorkspaceRoot = (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent),
    [switch]$AsJson
)

$ErrorActionPreference = "Stop"

$layoutFile = Join-Path $WorkspaceRoot "cmake\workspace-layout.env"
if (-not (Test-Path $layoutFile)) {
    throw "未找到 workspace layout 清单: $layoutFile"
}

$entries = [ordered]@{}
Get-Content $layoutFile | ForEach-Object {
    $line = $_.Trim()
    if ([string]::IsNullOrWhiteSpace($line) -or $line.StartsWith("#")) {
        return
    }
    $parts = $line -split "=", 2
    if ($parts.Count -ne 2) {
        return
    }
    $key = $parts[0].Trim()
    $relative = $parts[1].Trim()
    $absolute = if ([System.IO.Path]::IsPathRooted($relative)) {
        $relative
    } else {
        [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $relative))
    }

    $entries[$key] = [pscustomobject]@{
        Relative = $relative
        Absolute = $absolute
    }
}

$templateRoots = [ordered]@{}
foreach ($key in @(
    "SILIGEN_MODULES_ROOT",
    "SILIGEN_SHARED_ROOT",
    "SILIGEN_SAMPLES_ROOT",
    "SILIGEN_SCRIPTS_ROOT",
    "SILIGEN_DEPLOY_ROOT"
)) {
    if ($entries.Contains($key)) {
        $templateRoots[$key] = $entries[$key]
    }
}

$layout = [pscustomobject]@{
    WorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)
    LayoutFile = $layoutFile
    Entries = [pscustomobject]$entries
    TemplateRoots = [pscustomobject]$templateRoots
    FormalFreezeDocset = [pscustomobject]@{
        DocsetDir = $entries["SILIGEN_FORMAL_FREEZE_DOCSET_DIR"]
        IndexFile = $entries["SILIGEN_FORMAL_FREEZE_INDEX_FILE"]
    }
}

if ($AsJson) {
    $layout | ConvertTo-Json -Depth 6
} else {
    $layout
}
