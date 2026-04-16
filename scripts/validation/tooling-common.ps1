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

function Test-ControlAppsBuildRootMatchesWorkspace {
    param(
        [Parameter(Mandatory = $true)]
        [string]$BuildRoot,
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    $cacheFile = Join-Path $BuildRoot "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return $false
    }

    $homeDirectoryLine = Get-Content $cacheFile | Where-Object { $_ -like "CMAKE_HOME_DIRECTORY:*" } | Select-Object -First 1
    if (-not $homeDirectoryLine) {
        return $false
    }

    $configuredSourceRoot = ($homeDirectoryLine -split "=", 2)[1]
    if ([string]::IsNullOrWhiteSpace($configuredSourceRoot)) {
        return $false
    }

    $resolvedConfiguredSourceRoot = [System.IO.Path]::GetFullPath($configuredSourceRoot)
    $resolvedWorkspaceSourceRoot = [System.IO.Path]::GetFullPath($WorkspaceRoot)
    return $resolvedConfiguredSourceRoot -ieq $resolvedWorkspaceSourceRoot
}

function Get-ControlAppsBuildSearchRoots {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    return @([System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build")))
}

function Get-ControlAppsBuildRoots {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
        $explicitRoot = [System.IO.Path]::GetFullPath($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)
        $workspaceBuildRoot = [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build"))
        if (-not $explicitRoot.StartsWith($workspaceBuildRoot, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "SILIGEN_CONTROL_APPS_BUILD_ROOT must stay inside current workspace build root: '$workspaceBuildRoot'"
        }
        return @($explicitRoot)
    }

    $acceptedRoots = @()
    foreach ($root in @(Get-ControlAppsBuildSearchRoots -WorkspaceRoot $WorkspaceRoot)) {
        if (Test-ControlAppsBuildRootMatchesWorkspace -BuildRoot $root -WorkspaceRoot $WorkspaceRoot) {
            $acceptedRoots += [System.IO.Path]::GetFullPath($root)
        }
    }

    return @(
        $acceptedRoots |
            Where-Object { -not [string]::IsNullOrWhiteSpace($_) } |
            Select-Object -Unique
    )
}

function Get-ControlAppsBuildRoot {
    param(
        [Parameter(Mandatory = $true)]
        [string]$WorkspaceRoot
    )

    $roots = @(Get-ControlAppsBuildRoots -WorkspaceRoot $WorkspaceRoot)
    if ($roots.Count -eq 0) {
        return [System.IO.Path]::GetFullPath((Join-Path $WorkspaceRoot "build"))
    }

    foreach ($root in $roots) {
        if ((Test-Path $root) -and (Test-ControlAppsBuildRootMatchesWorkspace -BuildRoot $root -WorkspaceRoot $WorkspaceRoot)) {
            return [System.IO.Path]::GetFullPath($root)
        }
    }

    foreach ($root in $roots) {
        if (Test-Path $root) {
            return [System.IO.Path]::GetFullPath($root)
        }
    }

    return [System.IO.Path]::GetFullPath($roots[0])
}
