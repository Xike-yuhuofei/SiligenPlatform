[CmdletBinding()]
param(
    [string]$BuildDir = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) "build"),
    [ValidateSet("Debug", "Release", "RelWithDebInfo", "MinSizeRel")]
    [string]$Config = "Debug",
    [int]$Jobs = [Math]::Max(1, [Math]::Floor([Environment]::ProcessorCount * 0.8)),
    [switch]$Clean
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$workspaceRoot = [System.IO.Path]::GetFullPath($workspaceRoot)
$resolvedBuildDir = [System.IO.Path]::GetFullPath($BuildDir)

function Get-CMakeHomeDirectory {
    param([string]$CacheFile)

    if (-not (Test-Path $CacheFile)) {
        return $null
    }

    $line = Get-Content $CacheFile | Where-Object { $_ -like "CMAKE_HOME_DIRECTORY:*" } | Select-Object -First 1
    if (-not $line) {
        return $null
    }

    $configured = ($line -split "=", 2)[1]
    if ([string]::IsNullOrWhiteSpace($configured)) {
        return $null
    }

    return [System.IO.Path]::GetFullPath($configured)
}

function Remove-BuildDirSafely {
    param([string]$PathToRemove)

    if (-not (Test-Path $PathToRemove)) {
        return
    }

    $full = [System.IO.Path]::GetFullPath($PathToRemove)
    if ($full.Length -le 3) {
        throw "拒绝删除风险路径: $full"
    }
    if ($full -ieq $workspaceRoot) {
        throw "拒绝删除工作区根目录: $full"
    }
    if (-not (Test-Path (Join-Path $full "CMakeCache.txt"))) {
        throw "拒绝删除不含 CMakeCache.txt 的目录: $full"
    }

    Remove-Item -Path $full -Recurse -Force -ErrorAction Stop
}

if ($Clean) {
    Write-Output "clean build dir: $resolvedBuildDir"
    Remove-BuildDirSafely -PathToRemove $resolvedBuildDir
}

$cacheFile = Join-Path $resolvedBuildDir "CMakeCache.txt"
$configuredHome = Get-CMakeHomeDirectory -CacheFile $cacheFile
if ($configuredHome -and ($configuredHome -ine $workspaceRoot)) {
    Write-Warning "检测到 CMake cache source mismatch: '$configuredHome' -> '$workspaceRoot'，将自动重建 build 目录。"
    Remove-BuildDirSafely -PathToRemove $resolvedBuildDir
}

Write-Output "configure: cmake -S $workspaceRoot -B $resolvedBuildDir"
& cmake -S $workspaceRoot -B $resolvedBuildDir `
    -DSILIGEN_BUILD_TESTS=OFF `
    -DSILIGEN_USE_PCH=ON `
    -DSILIGEN_PARALLEL_COMPILE=ON `
    -DSILIGEN_PARALLEL_COMPILE_JOBS=$Jobs
if ($LASTEXITCODE -ne 0) {
    throw "cmake configure 失败，退出码: $LASTEXITCODE"
}

Write-Output "build: cmake --build $resolvedBuildDir --config $Config --target siligen_runtime_gateway --parallel $Jobs"
& cmake --build $resolvedBuildDir --config $Config --target siligen_runtime_gateway --parallel $Jobs
if ($LASTEXITCODE -ne 0) {
    throw "cmake build 失败，退出码: $LASTEXITCODE"
}

$artifactCandidates = @(
    (Join-Path $resolvedBuildDir "bin\$Config\siligen_runtime_gateway.exe"),
    (Join-Path $resolvedBuildDir "bin\siligen_runtime_gateway.exe")
)
$artifact = $artifactCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $artifact) {
    throw "构建成功但未找到产物: siligen_runtime_gateway.exe（检查目录: $resolvedBuildDir\\bin）"
}

Write-Output "artifact: $artifact"
