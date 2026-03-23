param(
    [ValidateSet("Local", "CI")]
    [string]$Profile = "Local",
    [ValidateSet("all", "apps", "packages", "integration", "protocol-compatibility", "simulation")]
    [string[]]$Suite = @("all"),
    [switch]$SkipSimulationEngine
)

$ErrorActionPreference = "Stop"

$workspaceRoot = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$workspaceSourceRoot = $workspaceRoot
$controlAppsBuild = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_CONTROL_APPS_BUILD_ROOT)) {
    $env:SILIGEN_CONTROL_APPS_BUILD_ROOT
} elseif (-not [string]::IsNullOrWhiteSpace($env:LOCALAPPDATA)) {
    Join-Path $env:LOCALAPPDATA "SiligenSuite\control-apps-build"
} else {
    Join-Path $workspaceRoot "build\control-apps"
}
$simulationRoot = Join-Path $workspaceRoot "packages\simulation-engine"
$simulationBuild = if (-not [string]::IsNullOrWhiteSpace($env:SILIGEN_SIMULATION_ENGINE_BUILD_ROOT)) {
    $env:SILIGEN_SIMULATION_ENGINE_BUILD_ROOT
} else {
    Join-Path $workspaceRoot "build\simulation-engine"
}

function Resolve-Suites {
    param(
        [string[]]$RequestedSuites
    )

    $defaultSuites = @("apps", "packages", "integration", "protocol-compatibility", "simulation")
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
    $resolvedWorkspaceSourceRoot = [System.IO.Path]::GetFullPath($workspaceSourceRoot)
    if ($resolvedConfiguredSourceRoot -ieq $resolvedWorkspaceSourceRoot) {
        return
    }

    Write-Output "build-root reset: source root changed from '$resolvedConfiguredSourceRoot' to '$resolvedWorkspaceSourceRoot'"
    try {
        Remove-Item -Path $controlAppsBuild -Recurse -Force -ErrorAction Stop
    } catch {
        $fallbackBuildRoot = Join-Path $workspaceRoot "build\control-apps-root"
        if ([System.IO.Path]::GetFullPath($controlAppsBuild) -ieq [System.IO.Path]::GetFullPath($fallbackBuildRoot)) {
            throw
        }

        Write-Output "build-root fallback: unable to clear '$controlAppsBuild', switching to '$fallbackBuildRoot'"
        $script:controlAppsBuild = $fallbackBuildRoot
    }
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
    $usePchFlag = "ON"
    $parallelCompileFlag = "ON"
    Reset-ControlAppsBuildIfSourceRootChanged
    cmake -S $workspaceSourceRoot -B $controlAppsBuild `
        -DSILIGEN_BUILD_TESTS=$buildTestsFlag `
        -DSILIGEN_USE_PCH=$usePchFlag `
        -DSILIGEN_PARALLEL_COMPILE=$parallelCompileFlag
    cmake --build $controlAppsBuild --config Debug --target $resolvedTargets --parallel
}

$resolvedSuites = Resolve-Suites -RequestedSuites $Suite
$needsSimulationBuild = (-not $SkipSimulationEngine) -and ($resolvedSuites -contains "simulation")
$localProfile = $Profile -eq "Local"
$controlAppTargets = @()
$enableControlAppTests = $false

if ($resolvedSuites -contains "apps") {
    $controlAppTargets += @(
        "siligen_control_runtime",
        "siligen_tcp_server",
        "siligen_cli"
    )
}

if (($resolvedSuites -contains "integration") -and $localProfile) {
    $controlAppTargets += "siligen_tcp_server"
}

if (($resolvedSuites -contains "packages") -and $localProfile) {
    $controlAppTargets += @(
        "siligen_shared_kernel_tests",
        "siligen_unit_tests",
        "siligen_pr1_tests"
    )
    $enableControlAppTests = $true
}

Invoke-ControlAppsBuild -Targets $controlAppTargets -EnableTests:$enableControlAppTests

$controlAppArtifactMap = @{
    "siligen_control_runtime" = "siligen_control_runtime.exe"
    "siligen_tcp_server" = "siligen_tcp_server.exe"
    "siligen_cli" = "siligen_cli.exe"
    "siligen_shared_kernel_tests" = "siligen_shared_kernel_tests.exe"
    "siligen_unit_tests" = "siligen_unit_tests.exe"
    "siligen_pr1_tests" = "siligen_pr1_tests.exe"
}

foreach ($targetName in ($controlAppTargets | Select-Object -Unique)) {
    if ($controlAppArtifactMap.ContainsKey($targetName)) {
        Assert-ControlAppsBinary -FileName $controlAppArtifactMap[$targetName] -TargetName $targetName
    }
}

if ($needsSimulationBuild) {
    $simulationTargets = @(
        "simulation_engine_smoke_test",
        "simulation_engine_json_io_test",
        "simulation_engine_scheme_c_baseline_test",
        "simulation_engine_scheme_c_runtime_bridge_shim_test",
        "simulation_engine_scheme_c_runtime_bridge_core_integration_test",
        "simulation_engine_scheme_c_recording_export_test",
        "simulation_engine_scheme_c_application_runner_test",
        "simulation_engine_scheme_c_virtual_devices_test",
        "simulation_engine_scheme_c_virtual_time_test",
        "process_runtime_core_deterministic_path_execution_test",
        "process_runtime_core_motion_runtime_assembly_test",
        "process_runtime_core_pb_path_source_adapter_test",
        "simulate_dxf_path",
        "simulate_scheme_c_closed_loop"
    )
    cmake --fresh -S $simulationRoot -B $simulationBuild -DSIM_ENGINE_BUILD_EXAMPLES=ON -DSIM_ENGINE_BUILD_TESTS=ON
    cmake --build $simulationBuild --config Debug --target $simulationTargets
}

Write-Output "workspace build complete"
Write-Output "profile: $Profile"
Write-Output "suites: $($resolvedSuites -join ', ')"
Write-Output "control-apps source root: $workspaceSourceRoot"
Write-Output "control-apps build root: $controlAppsBuild"

if ($controlAppTargets.Count -gt 0 -and $needsSimulationBuild) {
    Write-Output "built: canonical control apps/tests -> $controlAppsBuild; packages/simulation-engine examples/tests"
} elseif ($controlAppTargets.Count -gt 0) {
    Write-Output "built: canonical control apps/tests -> $controlAppsBuild"
} elseif ($needsSimulationBuild) {
    Write-Output "built: packages/simulation-engine compat + scheme C examples/tests"
} else {
    Write-Output "built: no suite-specific build work required"
}

Write-Output "legacy-relation: canonical control app artifacts now build under '$controlAppsBuild'; workspace root 已成为唯一 C++ superbuild source root。"
