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

Write-Output "hmi formal gateway contract: $resolvedContractPath"
