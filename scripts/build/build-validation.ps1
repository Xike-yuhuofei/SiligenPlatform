[CmdletBinding()]
param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "contracts", "e2e", "protocol-compatibility", "performance")]
    [string[]]$Suite = @("all"),
    [ValidateSet("auto", "quick-gate", "full-offline-gate", "nightly-performance", "limited-hil")]
    [string]$Lane = "auto",
    [ValidateSet("low", "medium", "high", "hardware-sensitive")]
    [string]$RiskProfile = "medium",
    [ValidateSet("auto", "quick", "full-offline", "nightly", "hil")]
    [string]$DesiredDepth = "auto",
    [string[]]$ChangedScope = @(),
    [string[]]$SkipLayer = @(),
    [string]$SkipJustification = "",
    [switch]$SkipHeavyTargets
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$thirdPartyBootstrap = Join-Path $workspaceRoot "scripts\bootstrap\bootstrap-third-party.ps1"
if (-not (Test-Path $thirdPartyBootstrap)) {
    throw "third-party bootstrap script not found: $thirdPartyBootstrap"
}

Write-Output "third-party bootstrap: powershell -NoProfile -ExecutionPolicy Bypass -File $thirdPartyBootstrap"
& $thirdPartyBootstrap -WorkspaceRoot $workspaceRoot

$layoutScript = Join-Path $workspaceRoot "scripts\validation\get-workspace-layout.ps1"
if (-not (Test-Path $layoutScript)) {
    throw "未找到 workspace layout 解析脚本: $layoutScript"
}
$layout = & $layoutScript -WorkspaceRoot $workspaceRoot
$workspaceSourceRoot = [System.IO.Path]::GetFullPath($workspaceRoot)
$resolvedWorkspaceRoot = [System.IO.Path]::GetFullPath($workspaceRoot)
if ($workspaceSourceRoot -ine $resolvedWorkspaceRoot) {
    throw "workspace root mismatch: workspaceRoot='$resolvedWorkspaceRoot', resolved='$workspaceSourceRoot'"
}

function Assert-FileContainsAll {
    param(
        [string]$Path,
        [string[]]$RequiredSnippets,
        [string]$Label
    )

    if (-not (Test-Path $Path)) {
        throw "$Label 缺失: $Path"
    }

    $content = Get-Content -Raw -Path $Path
    foreach ($snippet in $RequiredSnippets) {
        if ($content -notlike "*$snippet*") {
            throw "$Label 缺少必需片段 '$snippet': $Path"
        }
    }
}

function Assert-LayoutEntryEquals {
    param(
        [string]$Key,
        [string]$ExpectedRelative
    )

    $property = $layout.Entries.PSObject.Properties[$Key]
    if (-not $property) {
        throw "workspace layout 缺少键: $Key"
    }

    $actual = [string]$property.Value.Relative
    if ($actual -ne $ExpectedRelative) {
        throw "workspace layout 键 '$Key' 必须为 '$ExpectedRelative'，当前为 '$actual'"
    }
}

function Assert-LayoutEntryAbsent {
    param(
        [string]$Key
    )

    $property = $layout.Entries.PSObject.Properties[$Key]
    if ($property) {
        $actual = [string]$property.Value.Relative
        throw "workspace layout 键 '$Key' 必须移除（single-track），当前为 '$actual'"
    }
}

function Assert-DirectoryAbsent {
    param([string]$RelativePath)

    $target = Join-Path $workspaceRoot $RelativePath
    if (Test-Path $target) {
        throw "旧根必须物理删除: $RelativePath"
    }
}

function Assert-CanonicalGraphAndLegacyExitContracts {
    $requiredPaths = @(
        (Join-Path $workspaceRoot "CMakeLists.txt"),
        (Join-Path $workspaceRoot "apps\CMakeLists.txt"),
        (Join-Path $workspaceRoot "tests\CMakeLists.txt"),
        (Join-Path $workspaceRoot "cmake\workspace-layout.env"),
        (Join-Path $workspaceRoot "scripts\build\build-validation.ps1"),
        (Join-Path $workspaceRoot "scripts\migration\legacy-exit-checks.py"),
        (Join-Path $workspaceRoot "scripts\validation\invoke-workspace-tests.ps1"),
        (Join-Path $workspaceRoot "scripts\validation\run-local-validation-gate.ps1")
    )
    $missing = @($requiredPaths | Where-Object { -not (Test-Path $_) })
    if ($missing.Count -gt 0) {
        throw "canonical graph / legacy exit 合同缺失路径: $($missing -join ', ')"
    }

    Assert-FileContainsAll `
        -Path (Join-Path $workspaceRoot "CMakeLists.txt") `
        -RequiredSnippets @(
            "LoadWorkspaceLayout.cmake",
            "workspace root must be canonical superbuild source root"
        ) `
        -Label "根级 CMake canonical graph 断言"

    Assert-FileContainsAll `
        -Path (Join-Path $workspaceRoot "tests\CMakeLists.txt") `
        -RequiredSnippets @(
            "LoadWorkspaceLayout.cmake",
            "tests root must resolve to canonical superbuild source root"
        ) `
        -Label "tests CMake canonical graph 断言"

    Assert-LayoutEntryEquals -Key "SILIGEN_APPS_ROOT" -ExpectedRelative "apps"
    Assert-LayoutEntryEquals -Key "SILIGEN_MODULES_ROOT" -ExpectedRelative "modules"
    Assert-LayoutEntryEquals -Key "SILIGEN_TESTS_ROOT" -ExpectedRelative "tests"
    Assert-LayoutEntryEquals -Key "SILIGEN_SCRIPTS_ROOT" -ExpectedRelative "scripts"

    Assert-LayoutEntryAbsent -Key "SILIGEN_RUNTIME_HOST_DIR"
    Assert-LayoutEntryAbsent -Key "SILIGEN_PROCESS_RUNTIME_CORE_DIR"
    Assert-LayoutEntryAbsent -Key "SILIGEN_SHARED_KERNEL_DIR"
    Assert-LayoutEntryAbsent -Key "SILIGEN_TRANSPORT_GATEWAY_DIR"
    Assert-LayoutEntryAbsent -Key "SILIGEN_TRACEABILITY_OBSERVABILITY_DIR"
    Assert-LayoutEntryAbsent -Key "SILIGEN_DEVICE_ADAPTERS_DIR"
    Assert-LayoutEntryAbsent -Key "SILIGEN_DEVICE_CONTRACTS_DIR"
    Assert-LayoutEntryAbsent -Key "SILIGEN_SIMULATION_ENGINE_DIR"

    Assert-DirectoryAbsent -RelativePath "packages"
    Assert-DirectoryAbsent -RelativePath "integration"
    Assert-DirectoryAbsent -RelativePath "tools"
    Assert-DirectoryAbsent -RelativePath "examples"
}

Assert-CanonicalGraphAndLegacyExitContracts

$layoutValidator = Join-Path $workspaceRoot "scripts\migration\validate_workspace_layout.py"
if (-not (Test-Path $layoutValidator)) {
    throw "workspace layout validator not found: $layoutValidator"
}

Write-Output "workspace layout gate: python $layoutValidator --wave `"Bridge Exit Closeout`""
python $layoutValidator --wave "Bridge Exit Closeout"
if ($LASTEXITCODE -ne 0) {
    throw "workspace layout gate failed (exit: $LASTEXITCODE)."
}

$controlAppsBuild = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path $workspaceRoot "build\control-apps"
}
$controlAppsCmakeHomeDirectory = ""

function Resolve-Suites {
    param(
        [string[]]$RequestedSuites
    )

    $defaultSuites = @("apps", "contracts", "e2e", "protocol-compatibility")
    if (-not $RequestedSuites -or $RequestedSuites.Count -eq 0 -or $RequestedSuites -contains "all") {
        return $defaultSuites
    }

    return $RequestedSuites
}

function Resolve-FirstExistingPath {
    param(
        [string[]]$Candidates
    )

    return $Candidates | Where-Object { Test-Path $_ } | Select-Object -First 1
}

function Get-ControlAppsBinaryCandidates {
    param(
        [string]$FileName
    )

    return @(
        (Join-Path $controlAppsBuild "bin\$FileName"),
        (Join-Path $controlAppsBuild "bin\Debug\$FileName"),
        (Join-Path $controlAppsBuild "bin\Release\$FileName"),
        (Join-Path $controlAppsBuild "bin\RelWithDebInfo\$FileName")
    )
}

function Assert-ControlAppsBinary {
    param(
        [string]$FileName,
        [string]$TargetName
    )

    $resolved = Resolve-FirstExistingPath -Candidates (Get-ControlAppsBinaryCandidates -FileName $FileName)
    if (-not $resolved) {
        throw "control-apps build 已完成，但未找到目标 '$TargetName' 的 canonical 产物 '$FileName'。请检查 '$controlAppsBuild' 的输出目录。"
    }

    Write-Output "artifact: $TargetName -> $resolved"
}

function Reset-ControlAppsBuildIfSourceRootChanged {
    $cacheFile = Join-Path $controlAppsBuild "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return
    }

    $homeDirectoryLine = Get-Content $cacheFile | Where-Object { $_ -like "CMAKE_HOME_DIRECTORY:*" } | Select-Object -First 1
    if (-not $homeDirectoryLine) {
        return
    }

    $configuredSourceRoot = ($homeDirectoryLine -split "=", 2)[1]
    if ([string]::IsNullOrWhiteSpace($configuredSourceRoot)) {
        return
    }

    $resolvedConfiguredSourceRoot = [System.IO.Path]::GetFullPath($configuredSourceRoot)
    $script:controlAppsCmakeHomeDirectory = $resolvedConfiguredSourceRoot
    $resolvedWorkspaceSourceRoot = [System.IO.Path]::GetFullPath($workspaceSourceRoot)
    if ($resolvedConfiguredSourceRoot -ieq $resolvedWorkspaceSourceRoot) {
        Write-Output "source-root cache-check: matched '$resolvedConfiguredSourceRoot'"
        return
    }

    Write-Output "source-root cache-check: mismatch '$resolvedConfiguredSourceRoot' vs '$resolvedWorkspaceSourceRoot'"
    Write-Output "build-root reset: source root changed from '$resolvedConfiguredSourceRoot' to '$resolvedWorkspaceSourceRoot'"
    Remove-Item -Path $controlAppsBuild -Recurse -Force -ErrorAction Stop
}

function Invoke-ControlAppsBuild {
    param(
        [string[]]$Targets,
        [switch]$EnableTests
    )

    $resolvedTargets = @($Targets | Where-Object { -not [string]::IsNullOrWhiteSpace($_) } | Select-Object -Unique)
    if ($resolvedTargets.Count -eq 0) {
        return
    }

    $buildTestsFlag = if ($EnableTests) { "ON" } else { "OFF" }
    # Validation builds favor determinism over compile acceleration. Several
    # workspace targets already opt out of PCH on Windows/MSBuild to avoid
    # intermittent file-lock failures under parallel builds.
    $usePchFlag = "OFF"
    $parallelCompileFlag = "ON"
    Reset-ControlAppsBuildIfSourceRootChanged
    & cmake -S $workspaceSourceRoot -B $controlAppsBuild `
        -DSILIGEN_BUILD_TESTS=$buildTestsFlag `
        -DSILIGEN_USE_PCH=$usePchFlag `
        -DSILIGEN_PARALLEL_COMPILE=$parallelCompileFlag
    if ($LASTEXITCODE -ne 0) {
        throw "control-apps cmake configure 失败，退出码: $LASTEXITCODE"
    }
    & cmake --build $controlAppsBuild --config Debug --target $resolvedTargets --parallel
    if ($LASTEXITCODE -ne 0) {
        throw "control-apps cmake build 失败，退出码: $LASTEXITCODE"
    }
}

$resolvedSuites = Resolve-Suites -RequestedSuites $Suite
$localProfile = $Profile -eq "Local"
$controlAppTargets = @()
$enableControlAppTests = $false

if ($resolvedSuites -contains "apps") {
    $controlAppTargets += @(
        "siligen_runtime_service",
        "siligen_runtime_gateway",
        "siligen_planner_cli"
    )
}

if (($resolvedSuites -contains "e2e") -and $localProfile) {
    $controlAppTargets += "siligen_runtime_gateway"
}

if (($resolvedSuites -contains "contracts") -and $localProfile -and (-not $SkipHeavyTargets)) {
    $controlAppTargets += @(
        "siligen_shared_kernel_tests",
        "siligen_runtime_host_unit_tests",
        "siligen_dxf_geometry_unit_tests",
        "siligen_job_ingest_unit_tests",
        "siligen_unit_tests",
        "siligen_pr1_tests"
    )
    $enableControlAppTests = $true
}

Invoke-ControlAppsBuild -Targets $controlAppTargets -EnableTests:$enableControlAppTests

$controlAppArtifactMap = @{
    "siligen_runtime_service" = "siligen_runtime_service.exe"
    "siligen_runtime_gateway" = "siligen_runtime_gateway.exe"
    "siligen_planner_cli" = "siligen_planner_cli.exe"
    "siligen_shared_kernel_tests" = "siligen_shared_kernel_tests.exe"
    "siligen_runtime_host_unit_tests" = "siligen_runtime_host_unit_tests.exe"
    "siligen_dxf_geometry_unit_tests" = "siligen_dxf_geometry_unit_tests.exe"
    "siligen_job_ingest_unit_tests" = "siligen_job_ingest_unit_tests.exe"
    "siligen_unit_tests" = "siligen_unit_tests.exe"
    "siligen_pr1_tests" = "siligen_pr1_tests.exe"
}

foreach ($targetName in ($controlAppTargets | Select-Object -Unique)) {
    if ($controlAppArtifactMap.ContainsKey($targetName)) {
        Assert-ControlAppsBinary -FileName $controlAppArtifactMap[$targetName] -TargetName $targetName
    }
}

Write-Output "workspace build complete"
Write-Output "profile: $Profile"
Write-Output "lane: $Lane"
Write-Output "risk_profile: $RiskProfile"
Write-Output "desired_depth: $DesiredDepth"
Write-Output "changed_scopes: $($ChangedScope -join ', ')"
Write-Output "skip_layers: $($SkipLayer -join ', ')"
Write-Output "skip_justification: $SkipJustification"
Write-Output "suites: $($resolvedSuites -join ', ')"
Write-Output "workspace root: $resolvedWorkspaceRoot"
Write-Output "control-apps source root: $workspaceSourceRoot"
Write-Output "control-apps build root: $controlAppsBuild"
if ([string]::IsNullOrWhiteSpace($controlAppsCmakeHomeDirectory)) {
    Write-Output "control-apps cmake home directory: missing (new build cache)"
} else {
    Write-Output "control-apps cmake home directory: $controlAppsCmakeHomeDirectory"
}
Write-Output "workspace layout file: $($layout.LayoutFile)"
if ($controlAppTargets.Count -gt 0) {
    Write-Output "built: canonical control apps/tests -> $controlAppsBuild"
} else {
    Write-Output "built: no suite-specific build work required"
}
Write-Output "provenance gate: control-apps CMAKE_HOME_DIRECTORY must equal '$resolvedWorkspaceRoot' when build cache exists."
