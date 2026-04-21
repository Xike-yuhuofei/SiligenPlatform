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

function Resolve-ContractValuePath {
    param(
        [string]$ResolvedWorkspaceRoot,
        [string]$RawValue
    )

    if ([string]::IsNullOrWhiteSpace($RawValue)) {
        return ""
    }

    if ([System.IO.Path]::IsPathRooted($RawValue)) {
        return [System.IO.Path]::GetFullPath($RawValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $ResolvedWorkspaceRoot $RawValue))
}

function Test-PathWithinRoot {
    param(
        [string]$CandidatePath,
        [string]$AllowedRoot
    )

    if ([string]::IsNullOrWhiteSpace($CandidatePath) -or [string]::IsNullOrWhiteSpace($AllowedRoot)) {
        return $false
    }

    $resolvedCandidate = [System.IO.Path]::GetFullPath($CandidatePath)
    $resolvedRoot = [System.IO.Path]::GetFullPath($AllowedRoot)
    if ($resolvedCandidate -ieq $resolvedRoot) {
        return $true
    }

    $rootWithSeparator = $resolvedRoot.TrimEnd('\', '/') + [System.IO.Path]::DirectorySeparatorChar
    return $resolvedCandidate.StartsWith($rootWithSeparator, [System.StringComparison]::OrdinalIgnoreCase)
}

$resolvedWorkspaceRoot = Resolve-WorkspaceRoot -WorkspaceRootPath $WorkspaceRoot
$resolvedContractPath = Resolve-ContractPath `
    -ResolvedWorkspaceRoot $resolvedWorkspaceRoot `
    -ExplicitContractPath $ContractPath
$resolvedCanonicalBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $resolvedWorkspaceRoot "build\ca"))

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

$resolvedExecutable = Resolve-ContractValuePath -ResolvedWorkspaceRoot $resolvedWorkspaceRoot -RawValue $executable
if (-not (Test-PathWithinRoot -CandidatePath $resolvedExecutable -AllowedRoot $resolvedCanonicalBuildRoot)) {
    throw "HMI formal gateway launch contract executable must stay under canonical build\\ca root: $resolvedContractPath"
}

$cwdProperty = $payload.PSObject.Properties["cwd"]
$cwd = if ($null -ne $cwdProperty) {
    [string]$cwdProperty.Value
} else {
    ""
}
if ([string]::IsNullOrWhiteSpace($cwd)) {
    throw "HMI formal gateway launch contract missing cwd: $resolvedContractPath"
}

$resolvedCwd = Resolve-ContractValuePath -ResolvedWorkspaceRoot $resolvedWorkspaceRoot -RawValue $cwd
if (-not (Test-PathWithinRoot -CandidatePath $resolvedCwd -AllowedRoot $resolvedCanonicalBuildRoot)) {
    throw "HMI formal gateway launch contract cwd must stay under canonical build\\ca root: $resolvedContractPath"
}

$pathEntriesProperty = $payload.PSObject.Properties["pathEntries"]
$pathEntries = if ($null -ne $pathEntriesProperty) {
    @($pathEntriesProperty.Value)
} else {
    @()
}
if ($pathEntries.Count -eq 0) {
    throw "HMI formal gateway launch contract missing pathEntries: $resolvedContractPath"
}

foreach ($entry in $pathEntries) {
    $resolvedEntry = Resolve-ContractValuePath -ResolvedWorkspaceRoot $resolvedWorkspaceRoot -RawValue ([string]$entry)
    if (-not (Test-PathWithinRoot -CandidatePath $resolvedEntry -AllowedRoot $resolvedCanonicalBuildRoot)) {
        throw "HMI formal gateway launch contract pathEntries must stay under canonical build\\ca root: $resolvedContractPath"
    }
}

Write-Output "hmi formal gateway contract: $resolvedContractPath"
