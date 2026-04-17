[CmdletBinding()]
param(
    [string]$WorkspaceRoot = "",
    [string]$ContractPath = ""
)

$ErrorActionPreference = "Stop"

function Resolve-WorkspaceRoot {
    param([string]$WorkspaceRootPath)

    if (-not [string]::IsNullOrWhiteSpace($WorkspaceRootPath)) {
        return [System.IO.Path]::GetFullPath($WorkspaceRootPath)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot "..\.."))
}

function Resolve-ContractPath {
    param(
        [string]$ResolvedWorkspaceRoot,
        [string]$ExplicitContractPath
    )

    if (-not [string]::IsNullOrWhiteSpace($ExplicitContractPath)) {
        return [System.IO.Path]::GetFullPath($ExplicitContractPath)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $ResolvedWorkspaceRoot "apps\hmi-app\config\gateway-launch.json"))
}

function Resolve-ContractRelativePath {
    param(
        [string]$ResolvedWorkspaceRoot,
        [string]$RawPath
    )

    if ([string]::IsNullOrWhiteSpace($RawPath)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($RawPath)) {
        $resolved = [System.IO.Path]::GetFullPath($RawPath)
    } else {
        $resolved = [System.IO.Path]::GetFullPath((Join-Path $ResolvedWorkspaceRoot $RawPath))
    }

    $workspacePrefix = [System.IO.Path]::GetFullPath($ResolvedWorkspaceRoot).TrimEnd('\') + "\"
    if (-not $resolved.StartsWith($workspacePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "HMI formal gateway launch contract must stay inside workspace root: $resolved"
    }

    return $resolved.Substring($workspacePrefix.Length).Replace('/', '\')
}

function Assert-CanonicalBuildCaPath {
    param(
        [string]$ResolvedWorkspaceRoot,
        [string]$RawPath,
        [string]$Label,
        [string]$ResolvedContractPath
    )

    $relative = Resolve-ContractRelativePath -ResolvedWorkspaceRoot $ResolvedWorkspaceRoot -RawPath $RawPath
    if ([string]::IsNullOrWhiteSpace($relative)) {
        throw "HMI formal gateway launch contract missing ${Label}: $ResolvedContractPath"
    }

    if ($relative.StartsWith("build\bin\", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "HMI formal gateway launch contract must not point to legacy build/bin root ($Label): $relative"
    }

    if (-not $relative.StartsWith("build\ca\bin\", [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "HMI formal gateway launch contract must point to canonical build/ca root ($Label): $relative"
    }
}

$resolvedWorkspaceRoot = Resolve-WorkspaceRoot -WorkspaceRootPath $WorkspaceRoot
$resolvedContractPath = Resolve-ContractPath `
    -ResolvedWorkspaceRoot $resolvedWorkspaceRoot `
    -ExplicitContractPath $ContractPath

if (-not (Test-Path $resolvedContractPath)) {
    throw (
        "HMI formal gateway launch contract missing: $resolvedContractPath. " +
        "验证/CI/release 入口必须提供 apps/hmi-app/config/gateway-launch.json；" +
        "开发态临时契约仅允许通过 apps/hmi-app/run.ps1 生成。"
    )
}

try {
    $payload = Get-Content -Raw -Path $resolvedContractPath | ConvertFrom-Json
} catch {
    throw "HMI formal gateway launch contract is not valid JSON: $resolvedContractPath"
}

if ($null -eq $payload) {
    throw "HMI formal gateway launch contract is empty: $resolvedContractPath"
}

$executableProperty = $payload.PSObject.Properties["executable"]
$executable = if ($null -ne $executableProperty) {
    [string]$executableProperty.Value
} else {
    ""
}

if ([string]::IsNullOrWhiteSpace($executable)) {
    throw "HMI formal gateway launch contract missing executable: $resolvedContractPath"
}

$cwdProperty = $payload.PSObject.Properties["cwd"]
$cwd = if ($null -ne $cwdProperty) {
    [string]$cwdProperty.Value
} else {
    ""
}

$pathEntriesProperty = $payload.PSObject.Properties["pathEntries"]
$pathEntries = if (
    $null -ne $pathEntriesProperty `
        -and $pathEntriesProperty.Value -is [System.Collections.IEnumerable] `
        -and -not ($pathEntriesProperty.Value -is [string])
) {
    @($pathEntriesProperty.Value)
} else {
    @()
}

Assert-CanonicalBuildCaPath -ResolvedWorkspaceRoot $resolvedWorkspaceRoot -RawPath $executable -Label "executable" -ResolvedContractPath $resolvedContractPath
Assert-CanonicalBuildCaPath -ResolvedWorkspaceRoot $resolvedWorkspaceRoot -RawPath $cwd -Label "cwd" -ResolvedContractPath $resolvedContractPath
foreach ($entry in $pathEntries) {
    Assert-CanonicalBuildCaPath -ResolvedWorkspaceRoot $resolvedWorkspaceRoot -RawPath ([string]$entry) -Label "pathEntries" -ResolvedContractPath $resolvedContractPath
}

Write-Output "hmi formal gateway contract: $resolvedContractPath"
