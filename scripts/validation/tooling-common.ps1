Set-StrictMode -Version Latest

function Get-WorkspacePythonCommand {
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($null -eq $pythonCommand) {
        throw "未找到 python 命令。请先安装 Python 3.11+。"
    }

    return $pythonCommand.Source
}

function Get-WorkspacePythonUserScriptsDirectory {
    $pythonCommand = Get-WorkspacePythonCommand
    $userBase = (& $pythonCommand -c "import site; print(site.USER_BASE)").Trim()
    $versionTag = (& $pythonCommand -c "import sys; print(f'Python{sys.version_info.major}{sys.version_info.minor}')").Trim()
    if ([string]::IsNullOrWhiteSpace($userBase) -or [string]::IsNullOrWhiteSpace($versionTag)) {
        throw "无法解析 Python user scripts 目录。"
    }

    return Join-Path $userBase "$versionTag\Scripts"
}

function Resolve-WorkspaceToolPath {
    param(
        [Parameter(Mandatory = $true)]
        [string[]]$ToolNames,
        [switch]$Required
    )

    foreach ($toolName in $ToolNames) {
        $command = Get-Command $toolName -ErrorAction SilentlyContinue
        if ($null -ne $command) {
            return $command.Source
        }
    }

    $userScriptsDir = Get-WorkspacePythonUserScriptsDirectory
    foreach ($toolName in $ToolNames) {
        foreach ($candidateName in @($toolName, "$toolName.exe")) {
            $candidatePath = Join-Path $userScriptsDir $candidateName
            if (Test-Path $candidatePath) {
                return $candidatePath
            }
        }
    }

    if ($Required) {
        throw "未找到工具: $($ToolNames -join ', ')。请先运行 .\\scripts\\validation\\install-python-deps.ps1 或安装对应工具。"
    }

    return $null
}

function Resolve-WorkspaceReportPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)]
        [string]$ReportPath
    )

    if ([System.IO.Path]::IsPathRooted($ReportPath)) {
        return [System.IO.Path]::GetFullPath($ReportPath)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot $ReportPath))
}

function Ensure-WorkspaceDirectory {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Path
    )

    New-Item -ItemType Directory -Force -Path $Path | Out-Null
    return [System.IO.Path]::GetFullPath($Path)
}

function Get-WorkspaceRelativePath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot,
        [Parameter(Mandatory = $true)]
        [string]$TargetPath
    )

    $resolvedWorkspaceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot).TrimEnd('\')
    $resolvedTargetPath = [System.IO.Path]::GetFullPath($TargetPath)
    if ($resolvedTargetPath.StartsWith($resolvedWorkspaceRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
        return $resolvedTargetPath.Substring($resolvedWorkspaceRoot.Length).TrimStart('\')
    }

    return $resolvedTargetPath
}

function Get-ControlAppsBuildRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
        return [System.IO.Path]::GetFullPath($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)
    }
    if (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
        return [System.IO.Path]::GetFullPath((Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"))
    }

    return [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build\control-apps"))
}
