[CmdletBinding()]
param(
    [string]$WorkspaceRoot = ".",
    [string]$ReportDir = "tests/reports/module-boundary-bridges"
)

$ErrorActionPreference = "Stop"

function Resolve-AbsolutePath {
    param(
        [string]$BasePath,
        [string]$PathValue
    )

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $PathValue))
}

function Get-RelativeRepoPath {
    param(
        [string]$BasePath,
        [string]$TargetPath
    )

    try {
        return [System.IO.Path]::GetRelativePath($BasePath, $TargetPath)
    } catch {
        $baseUri = [System.Uri]((Resolve-AbsolutePath -BasePath $BasePath -PathValue ".").TrimEnd('\') + '\')
        $targetUri = [System.Uri](Resolve-AbsolutePath -BasePath $BasePath -PathValue $TargetPath)
        return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($targetUri).ToString()).Replace('/', '\')
    }
}

function Get-FixedStringMatches {
    param(
        [string]$Pattern,
        [string[]]$SearchRoots
    )

    $searchGlobs = @(
        "CMakeLists.txt",
        "*.cmake",
        "*.h",
        "*.hpp",
        "*.cpp",
        "*.cc",
        "*.cxx",
        "*.ps1"
    )

    try {
        $rgArgs = @("-n", "--fixed-strings")
        foreach ($glob in $searchGlobs) {
            $rgArgs += @("-g", $glob)
        }
        $rgArgs += $Pattern
        $rgArgs += $SearchRoots
        $matches = & rg @rgArgs 2>$null
        if ($LASTEXITCODE -le 1) {
            return @($matches)
        }
    } catch {
        # Fall back to PowerShell-native search when bundled rg is unavailable.
    }

    $results = New-Object System.Collections.Generic.List[string]
    foreach ($root in $SearchRoots) {
        $absoluteRoot = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $root
        if (-not (Test-Path $absoluteRoot)) {
            continue
        }

        Get-ChildItem -Path $absoluteRoot -Recurse -File | Where-Object {
            $_.Name -eq "CMakeLists.txt" -or
            $_.Extension -in @(".cmake", ".h", ".hpp", ".cpp", ".cc", ".cxx", ".ps1")
        } | ForEach-Object {
            $relativePath = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $_.FullName
            $lineNumber = 0
            foreach ($line in Get-Content -Path $_.FullName) {
                $lineNumber += 1
                if ($line.Contains($Pattern)) {
                    $results.Add(("{0}:{1}:{2}" -f $relativePath, $lineNumber, $line))
                }
            }
        }
    }

    return @($results)
}

function Get-ExactWordMatches {
    param(
        [string]$Word,
        [string[]]$SearchRoots
    )

    $searchGlobs = @(
        "CMakeLists.txt",
        "*.cmake",
        "*.h",
        "*.hpp",
        "*.cpp",
        "*.cc",
        "*.cxx",
        "*.ps1"
    )

    try {
        $rgArgs = @("-n", "-w")
        foreach ($glob in $searchGlobs) {
            $rgArgs += @("-g", $glob)
        }
        $rgArgs += $Word
        $rgArgs += $SearchRoots
        $matches = & rg @rgArgs 2>$null
        if ($LASTEXITCODE -le 1) {
            return @($matches)
        }
    } catch {
        # Fall back to PowerShell-native search when bundled rg is unavailable.
    }

    $escapedWord = [regex]::Escape($Word)
    $results = New-Object System.Collections.Generic.List[string]
    foreach ($root in $SearchRoots) {
        $absoluteRoot = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $root
        if (-not (Test-Path $absoluteRoot)) {
            continue
        }

        Get-ChildItem -Path $absoluteRoot -Recurse -File | Where-Object {
            $_.Name -eq "CMakeLists.txt" -or
            $_.Extension -in @(".cmake", ".h", ".hpp", ".cpp", ".cc", ".cxx", ".ps1")
        } | ForEach-Object {
            $relativePath = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $_.FullName
            $lineNumber = 0
            foreach ($line in Get-Content -Path $_.FullName) {
                $lineNumber += 1
                if ($line -match "(?<![A-Za-z0-9_])$escapedWord(?![A-Za-z0-9_])") {
                    $results.Add(("{0}:{1}:{2}" -f $relativePath, $lineNumber, $line))
                }
            }
        }
    }

    return @($results)
}

$repoRoot = Resolve-AbsolutePath -BasePath (Get-Location).Path -PathValue $WorkspaceRoot
$resolvedReportDir = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

Set-Location $repoRoot

$allowedDirectWorkflowReferences = @(
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/workflow/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/workflow/application/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/workflow/tests/canonical/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/runtime-execution/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/runtime-execution/runtime/host/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "apps/runtime-service/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/workflow/tests/regression/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/dxf-geometry/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/job-ingest/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/dxf-geometry/tests/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/job-ingest/tests/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/dispense-packaging/tests/regression/domain/dispensing/DispensePackagingResidualAcceptanceTest.cpp"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "apps/planner-cli/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "apps/runtime-gateway/transport-gateway/CMakeLists.txt")
)

$requiredBridgeReferences = @(
    @{
        path = "modules/dispense-packaging/domain/dispensing/planning/ports/ISpatialIndexPort.h"
        pattern = "namespace Siligen::Domain::PlanningBoundary::Ports"
    },
    @{
        path = "apps/runtime-service/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "application/services/process_path/ProcessPathFacade.h"'
    },
    @{
        path = "modules/topology-feature/CMakeLists.txt"
        pattern = "siligen_topology_feature_contracts_public"
    },
    @{
        path = "modules/process-planning/CMakeLists.txt"
        pattern = "siligen_process_planning_contracts_public"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_runtime_execution_runtime_contracts"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "usecases/dispensing/DispensingExecutionUseCase.cpp"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "usecases/motion/MotionControlUseCase.cpp"
    },
    @{
        path = "modules/process-path/application/CMakeLists.txt"
        pattern = "siligen_process_path_contracts_public"
    },
    @{
        path = "modules/motion-planning/domain/motion/CMakeLists.txt"
        pattern = "siligen_process_path_contracts_public"
    },
    @{
        path = "modules/runtime-execution/contracts/runtime/CMakeLists.txt"
        pattern = "runtime_execution/contracts/motion/IMotionRuntimePort.h"
    },
    @{
        path = "modules/runtime-execution/contracts/runtime/CMakeLists.txt"
        pattern = "runtime_execution/contracts/motion/IIOControlPort.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/events/CMakeLists.txt"
        pattern = "siligen_runtime_execution_runtime_contracts"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/scheduling/CMakeLists.txt"
        pattern = "siligen_runtime_execution_runtime_contracts"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "siligen_runtime_process_bootstrap_public"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "bootstrap/ContainerBootstrap.cpp"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "container/ApplicationContainer.cpp"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "runtime/configuration/InterlockConfigResolver.cpp"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "runtime/recipes/RecipeFileRepository.cpp"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "siligen_workflow_adapters_public"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "siligen_recipe_lifecycle_application_public"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "siligen_module_dxf_geometry"
    },
    @{
        path = "apps/runtime-service/main.cpp"
        pattern = '#include "runtime_process_bootstrap/ContainerBootstrap.h"'
    },
    @{
        path = "apps/runtime-service/main.cpp"
        pattern = '#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"'
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_runtime_process_bootstrap_public"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_recipe_lifecycle_application_public"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_workflow_adapters_public"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_module_topology_feature"
    },
    @{
        path = "apps/planner-cli/main.cpp"
        pattern = '#include "runtime_process_bootstrap/ContainerBootstrap.h"'
    },
    @{
        path = "apps/planner-cli/main.cpp"
        pattern = '#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"'
    },
    @{
        path = "apps/runtime-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_process_bootstrap_public"
    },
    @{
        path = "apps/runtime-gateway/main.cpp"
        pattern = '#include "runtime_process_bootstrap/ContainerBootstrap.h"'
    },
    @{
        path = "apps/runtime-gateway/main.cpp"
        pattern = '#include "runtime_process_bootstrap/WorkspaceAssetPaths.h"'
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_recipe_lifecycle_application_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_job_ingest_application_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_process_bootstrap_public"
    },
    @{
        path = "modules/dispense-packaging/domain/dispensing/CMakeLists.txt"
        pattern = "siligen_process_planning_contracts_public"
    },
    @{
        path = "apps/runtime-service/tests/CMakeLists.txt"
        pattern = "siligen_runtime_service_unit_tests"
    },
    @{
        path = "apps/runtime-service/tests/CMakeLists.txt"
        pattern = "runtime_service_integration_host_bootstrap_smoke"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "siligen_recipe_lifecycle_serialization_public"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "siligen_recipe_lifecycle_domain_public"
    },
    @{
        path = "apps/runtime-service/CMakeLists.txt"
        pattern = "siligen_trace_diagnostics_contracts_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp"
        pattern = '#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"'
    },
    @{
        path = "apps/runtime-service/runtime/recipes/RecipeBundleSerializer.h"
        pattern = '#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"'
    },
    @{
        path = "apps/runtime-service/runtime/recipes/RecipeFileRepository.cpp"
        pattern = '#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"'
    },
    @{
        path = "apps/runtime-service/runtime/recipes/ParameterSchemaFileProvider.cpp"
        pattern = '#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"'
    },
    @{
        path = "apps/runtime-service/runtime/recipes/TemplateFileRepository.cpp"
        pattern = '#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"'
    },
    @{
        path = "apps/runtime-service/runtime/recipes/AuditFileRepository.cpp"
        pattern = '#include "recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"'
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_trace_diagnostics_contracts_public"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_trace_diagnostics_contracts_public"
    },
    @{
        path = "modules/runtime-execution/application/include/runtime_execution/application/usecases/system/InitializeSystemUseCase.h"
        pattern = '#include "trace_diagnostics/contracts/IDiagnosticsPort.h"'
    },
    @{
        path = "modules/runtime-execution/application/include/runtime_execution/application/usecases/motion/monitoring/MotionMonitoringUseCase.h"
        pattern = '#include "trace_diagnostics/contracts/IDiagnosticsPort.h"'
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/diagnostics/DiagnosticsPortAdapter.h"
        pattern = '#include "trace_diagnostics/contracts/IDiagnosticsPort.h"'
    },
    @{
        path = "apps/runtime-service/bootstrap/ContainerBootstrap.cpp"
        pattern = '#include "trace_diagnostics/contracts/IDiagnosticsPort.h"'
    }
)

$directWorkflowTargets = @(
    "siligen_workflow_domain_public",
    "siligen_workflow_adapters_public",
    "siligen_workflow_runtime_consumer_public"
)

$recipeCanonicalUseCaseHeaderDir = Resolve-AbsolutePath -BasePath $repoRoot -PathValue `
    "modules/recipe-lifecycle/application/include/recipe_lifecycle/application/usecases/recipes"
$recipeSourceUseCaseHeaderDir = Resolve-AbsolutePath -BasePath $repoRoot -PathValue `
    "modules/recipe-lifecycle/application/usecases/recipes"
$recipeCanonicalSerializerHeader = Resolve-AbsolutePath -BasePath $repoRoot -PathValue `
    "modules/recipe-lifecycle/adapters/include/recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
$allowedHardwareDiagnosticsQuarantineReferences = @(
    @{
        pattern = '#include "domain/diagnostics/ports/ITestRecordRepository.h"'
        search_roots = @("modules", "apps", "tests")
        allowed_paths = @(
            "apps/runtime-service/bootstrap/ContainerBootstrap.cpp"
        )
        rule_id = "hardware-test-diagnostics-contract-escaped-bootstrap-quarantine"
        detail = "ITestRecordRepository include must remain quarantined to runtime-service bootstrap while hardware-test diagnostics contracts stay unresolved"
    },
    @{
        pattern = '#include "domain/diagnostics/ports/ITestConfigurationPort.h"'
        search_roots = @("modules", "apps", "tests")
        allowed_paths = @(
            "apps/runtime-service/bootstrap/ContainerBootstrap.cpp"
        )
        rule_id = "hardware-test-diagnostics-contract-escaped-bootstrap-quarantine"
        detail = "ITestConfigurationPort include must remain quarantined to runtime-service bootstrap while hardware-test diagnostics contracts stay unresolved"
    },
    @{
        pattern = '#include "domain/diagnostics/ports/ICMPTestPresetPort.h"'
        search_roots = @("modules", "apps", "tests")
        allowed_paths = @(
            "apps/runtime-service/bootstrap/ContainerBootstrap.cpp"
        )
        rule_id = "hardware-test-diagnostics-contract-escaped-bootstrap-quarantine"
        detail = "ICMPTestPresetPort include must remain quarantined to runtime-service bootstrap while hardware-test diagnostics contracts stay unresolved"
    },
    @{
        pattern = '#include "domain/diagnostics/value-objects/TestDataTypes.h"'
        search_roots = @("modules", "apps", "tests")
        allowed_paths = @(
            "modules/workflow/domain/include/domain/diagnostics/ports/ITestRecordRepository.h"
        )
        rule_id = "hardware-test-diagnostics-types-escaped-quarantine"
        detail = "TestDataTypes must remain quarantined behind workflow diagnostics hardware-test contracts until landing owner is re-frozen"
    }
)

$forbiddenCompatReferences = @(
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_runtime_compat"
        rule_id = "app-still-uses-runtime-workflow-compat"
        detail = "planner-cli must consume siligen_runtime_execution_application_public instead of runtime workflow compat"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_runtime_compat"
        rule_id = "app-still-uses-runtime-workflow-compat"
        detail = "transport-gateway must consume siligen_runtime_execution_application_public instead of runtime workflow compat"
    },
    @{
        path = "modules/runtime-execution/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_runtime_compat"
        rule_id = "runtime-module-still-defines-broad-runtime-compat"
        detail = "runtime-execution must not define the broad workflow runtime compat target anymore"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_runtime_compat"
        rule_id = "runtime-host-still-uses-broad-runtime-compat"
        detail = "runtime-host must link canonical owner targets instead of the broad runtime compat bridge"
    },
    @{
        path = "modules/runtime-execution/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_domain_compat"
        rule_id = "runtime-module-still-defines-domain-compat"
        detail = "runtime-execution must not define the residual workflow domain compat target anymore"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_domain_compat"
        rule_id = "runtime-host-still-uses-domain-compat"
        detail = "runtime-host must consume dedicated owner targets instead of the residual domain compat bridge"
    },
    @{
        path = "modules/runtime-execution/adapters/device/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_domain_compat"
        rule_id = "runtime-device-adapters-still-use-domain-compat"
        detail = "device-adapters must consume dedicated owner targets instead of the residual domain compat bridge"
    },
    @{
        path = "modules/runtime-execution/runtime/host/tests/CMakeLists.txt"
        pattern = "siligen_recipe_json_codec"
        rule_id = "runtime-host-tests-still-link-recipe-impl-target"
        detail = "runtime-host tests must consume recipe serialization through siligen_runtime_process_bootstrap_public"
    },
    @{
        path = "apps/planner-cli/CommandHandlers.Dxf.cpp"
        pattern = '#include "application/usecases/dispensing/DispensingExecutionUseCase.h"'
        rule_id = "app-still-includes-workflow-execution-header"
        detail = "planner-cli execution consumer must include runtime_execution/application/* headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h"
        pattern = '#include "application/usecases/dispensing/DispensingExecutionUseCase.h"'
        rule_id = "gateway-still-includes-workflow-execution-header"
        detail = "transport-gateway execution consumer must include runtime_execution/application/* headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = '#include "application/usecases/dispensing/DispensingExecutionUseCase.h"'
        rule_id = "gateway-still-includes-workflow-execution-header"
        detail = "transport-gateway builder must include runtime_execution/application/* headers"
    },
    @{
        path = "apps/runtime-service/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "application/usecases/dispensing/DispensingExecutionUseCase.h"'
        rule_id = "runtime-bootstrap-still-includes-workflow-execution-header"
        detail = "runtime bootstrap execution container must include runtime_execution/application/* headers"
    },
    @{
        path = "apps/planner-cli/CommandHandlers.Dxf.cpp"
        pattern = '#include "runtime_execution/application/usecases/dispensing/PlanningUseCase.h"'
        rule_id = "app-still-uses-runtime-planning-wrapper"
        detail = "planner-cli planning consumer must include workflow/application/* headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h"
        pattern = '#include "runtime_execution/application/usecases/dispensing/PlanningUseCase.h"'
        rule_id = "gateway-still-uses-runtime-planning-wrapper"
        detail = "transport-gateway planning consumer must include workflow/application/* headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h"
        pattern = '#include "runtime_execution/application/usecases/dispensing/UploadFileUseCase.h"'
        rule_id = "gateway-still-uses-runtime-upload-wrapper"
        detail = "transport-gateway upload consumer must include job_ingest/application/* headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = '#include "runtime_execution/application/usecases/dispensing/PlanningUseCase.h"'
        rule_id = "gateway-builder-still-uses-runtime-planning-wrapper"
        detail = "transport-gateway builder must include workflow/application/* headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = '#include "runtime_execution/application/usecases/dispensing/UploadFileUseCase.h"'
        rule_id = "gateway-builder-still-uses-runtime-upload-wrapper"
        detail = "transport-gateway builder must include job_ingest/application/* headers"
    },
    @{
        path = "apps/runtime-service/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "runtime_execution/application/usecases/dispensing/PlanningUseCase.h"'
        rule_id = "runtime-bootstrap-still-uses-runtime-planning-wrapper"
        detail = "runtime bootstrap planning consumer must include workflow/application/* headers"
    },
    @{
        path = "apps/runtime-service/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "runtime_execution/application/usecases/dispensing/UploadFileUseCase.h"'
        rule_id = "runtime-bootstrap-still-uses-runtime-upload-wrapper"
        detail = "runtime bootstrap upload consumer must include job_ingest/application/* headers"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_module_workflow"
        rule_id = "planner-cli-still-links-broad-workflow-module"
        detail = "planner-cli must consume workflow application/adapters public targets instead of siligen_module_workflow"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_module_workflow"
        rule_id = "transport-gateway-still-links-broad-workflow-module"
        detail = "transport-gateway must consume workflow application public targets instead of siligen_module_workflow"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_module_workflow"
        rule_id = "runtime-host-still-links-broad-workflow-module"
        detail = "runtime host must consume workflow application public targets instead of siligen_module_workflow"
    },
    @{
        path = "shared/contracts/CMakeLists.txt"
        pattern = "modules/runtime-execution/contracts/device"
        rule_id = "shared-contracts-still-points-to-runtime-device-contracts"
        detail = "shared/contracts must resolve siligen_device_contracts from shared/contracts/device canonical root"
    }
)

$forbiddenScopedSearches = @(
    @{
        rule_id = "workflow-application-still-leaks-dispenser-model"
        pattern = "DispenserModel"
        search_roots = @("modules/workflow/application")
        detail = "workflow/application must consume runtime execution state contracts instead of DispenserModel"
    },
    @{
        rule_id = "workflow-application-still-leaks-calibration-process"
        pattern = "CalibrationProcess"
        search_roots = @("modules/workflow/application")
        detail = "workflow/application must not depend on legacy CalibrationProcess after calibration ownership moved to runtime-execution"
    },
    @{
        rule_id = "workflow-application-still-leaks-legacy-calibration-port"
        pattern = "ICalibrationDevicePort"
        search_roots = @("modules/workflow/application")
        detail = "workflow/application must not depend on legacy calibration device ports"
    },
    @{
        rule_id = "workflow-application-still-leaks-legacy-calibration-port"
        pattern = "ICalibrationResultPort"
        search_roots = @("modules/workflow/application")
        detail = "workflow/application must not depend on legacy calibration result ports"
    },
    @{
        rule_id = "runtime-host-surface-still-leaks-dispenser-model"
        pattern = "DispenserModel"
        search_roots = @(
            "modules/runtime-execution/runtime/host/runtime/diagnostics",
            "modules/runtime-execution/runtime/host/runtime/motion",
            "modules/runtime-execution/runtime/host/runtime/planning",
            "modules/runtime-execution/runtime/host/services/motion"
        )
        detail = "runtime host core live diagnostics/motion/planning surfaces must not directly depend on DispenserModel"
    },
    @{
        rule_id = "runtime-host-surface-still-leaks-calibration-process"
        pattern = "CalibrationProcess"
        search_roots = @(
            "modules/runtime-execution/runtime/host/runtime/diagnostics",
            "modules/runtime-execution/runtime/host/runtime/motion",
            "modules/runtime-execution/runtime/host/runtime/planning",
            "modules/runtime-execution/runtime/host/services/motion"
        )
        detail = "runtime host core live diagnostics/motion/planning surfaces must not depend on legacy CalibrationProcess"
    },
    @{
        rule_id = "runtime-host-surface-still-leaks-legacy-calibration-port"
        pattern = "ICalibrationDevicePort"
        search_roots = @(
            "modules/runtime-execution/runtime/host/runtime/diagnostics",
            "modules/runtime-execution/runtime/host/runtime/motion",
            "modules/runtime-execution/runtime/host/runtime/planning",
            "modules/runtime-execution/runtime/host/services/motion"
        )
        detail = "runtime host core live diagnostics/motion/planning surfaces must not depend on legacy calibration device ports"
    },
    @{
        rule_id = "runtime-host-surface-still-leaks-legacy-calibration-port"
        pattern = "ICalibrationResultPort"
        search_roots = @(
            "modules/runtime-execution/runtime/host/runtime/diagnostics",
            "modules/runtime-execution/runtime/host/runtime/motion",
            "modules/runtime-execution/runtime/host/runtime/planning",
            "modules/runtime-execution/runtime/host/services/motion"
        )
        detail = "runtime host core live diagnostics/motion/planning surfaces must not depend on legacy calibration result ports"
    },
    @{
        rule_id = "runtime-application-still-includes-legacy-machine-port"
        pattern = "IHardwareConnectionPort"
        search_roots = @("modules/runtime-execution/application")
        detail = "runtime-execution application must close over DeviceConnectionPort or runtime contracts instead of IHardwareConnectionPort"
    },
    @{
        rule_id = "runtime-application-still-includes-legacy-machine-port"
        pattern = "IHardwareTestPort"
        search_roots = @("modules/runtime-execution/application")
        detail = "runtime-execution application must not depend on IHardwareTestPort"
    },
    @{
        rule_id = "runtime-host-surface-still-includes-legacy-machine-port"
        pattern = "IHardwareConnectionPort"
        search_roots = @(
            "modules/runtime-execution/runtime/host/runtime/diagnostics",
            "modules/runtime-execution/runtime/host/runtime/motion",
            "modules/runtime-execution/runtime/host/runtime/planning",
            "modules/runtime-execution/runtime/host/services/motion"
        )
        detail = "runtime host core live diagnostics/motion/planning surfaces must not depend on IHardwareConnectionPort"
    },
    @{
        rule_id = "runtime-host-surface-still-includes-legacy-machine-port"
        pattern = "IHardwareTestPort"
        search_roots = @(
            "modules/runtime-execution/runtime/host/runtime/diagnostics",
            "modules/runtime-execution/runtime/host/runtime/motion",
            "modules/runtime-execution/runtime/host/runtime/planning",
            "modules/runtime-execution/runtime/host/services/motion"
        )
        detail = "runtime host core live diagnostics/motion/planning surfaces must not depend on IHardwareTestPort"
    },
    @{
        rule_id = "apps-still-include-legacy-machine-port"
        pattern = "IHardwareConnectionPort"
        search_roots = @("apps")
        detail = "apps must not depend on legacy machine connection ports"
    },
    @{
        rule_id = "apps-still-include-legacy-machine-port"
        pattern = "IHardwareTestPort"
        search_roots = @("apps")
        detail = "apps must not depend on legacy machine test ports"
    },
    @{
        rule_id = "apps-still-leak-dispenser-model"
        pattern = "DispenserModel"
        search_roots = @("apps")
        detail = "apps must not depend on legacy DispenserModel directly"
    },
    @{
        rule_id = "apps-still-leak-calibration-process"
        pattern = "CalibrationProcess"
        search_roots = @("apps")
        detail = "apps must not depend on legacy CalibrationProcess directly"
    },
    @{
        rule_id = "apps-still-leak-legacy-calibration-port"
        pattern = "ICalibrationDevicePort"
        search_roots = @("apps")
        detail = "apps must not depend on legacy calibration device ports"
    },
    @{
        rule_id = "apps-still-leak-legacy-calibration-port"
        pattern = "ICalibrationResultPort"
        search_roots = @("apps")
        detail = "apps must not depend on legacy calibration result ports"
    },
    @{
        rule_id = "workflow-machine-compat-port-still-declared"
        pattern = "class IHardwareConnectionPort"
        search_roots = @("modules/workflow/domain/include/domain/machine/ports")
        detail = "workflow machine connection port header must stay bridge-only and must not declare a new owner type"
    },
    @{
        rule_id = "workflow-machine-compat-port-still-declared"
        pattern = "class IHardwareTestPort"
        search_roots = @("modules/workflow/domain/include/domain/machine/ports")
        detail = "workflow machine test port header must stay bridge-only and must not declare a new owner type"
    },
    @{
        rule_id = "workflow-machine-compat-port-still-declared"
        pattern = "struct HardwareConnectionConfig"
        search_roots = @("modules/workflow/domain/include/domain/machine/ports")
        detail = "workflow machine connection port wrapper must not duplicate HardwareConnectionConfig"
    },
    @{
        rule_id = "workflow-machine-compat-port-still-declared"
        pattern = "struct HeartbeatStatus"
        search_roots = @("modules/workflow/domain/include/domain/machine/ports")
        detail = "workflow machine connection port wrapper must not duplicate HeartbeatStatus"
    },
    @{
        rule_id = "coordinate-alignment-public-surface-still-leaks-runtime-history"
        pattern = "DispenserModel"
        search_roots = @("modules/coordinate-alignment/contracts", "modules/coordinate-alignment/application")
        detail = "coordinate-alignment public surface must not expose DispenserModel"
    },
    @{
        rule_id = "coordinate-alignment-public-surface-still-leaks-runtime-history"
        pattern = "CalibrationProcess"
        search_roots = @("modules/coordinate-alignment/contracts", "modules/coordinate-alignment/application")
        detail = "coordinate-alignment public surface must not expose CalibrationProcess"
    },
    @{
        rule_id = "coordinate-alignment-public-surface-still-leaks-runtime-history"
        pattern = "ICalibrationDevicePort"
        search_roots = @("modules/coordinate-alignment/contracts", "modules/coordinate-alignment/application")
        detail = "coordinate-alignment public surface must not expose legacy calibration ports"
    },
    @{
        rule_id = "coordinate-alignment-public-surface-still-leaks-runtime-history"
        pattern = "ICalibrationResultPort"
        search_roots = @("modules/coordinate-alignment/contracts", "modules/coordinate-alignment/application")
        detail = "coordinate-alignment public surface must not expose legacy calibration ports"
    },
    @{
        rule_id = "coordinate-alignment-public-surface-still-leaks-runtime-history"
        pattern = "IHardwareConnectionPort"
        search_roots = @("modules/coordinate-alignment/contracts", "modules/coordinate-alignment/application")
        detail = "coordinate-alignment public surface must not expose legacy machine connection ports"
    },
    @{
        rule_id = "coordinate-alignment-public-surface-still-leaks-runtime-history"
        pattern = "IHardwareTestPort"
        search_roots = @("modules/coordinate-alignment/contracts", "modules/coordinate-alignment/application")
        detail = "coordinate-alignment public surface must not expose legacy machine test ports"
    },
    @{
        rule_id = "motion-planning-legacy-motion-port-still-declared"
        pattern = "class IMotionRuntimePort"
        search_roots = @(
            "modules/motion-planning/domain/motion/ports",
            "modules/workflow/domain/include/domain/motion/ports"
        )
        detail = "legacy motion runtime public headers must be compatibility shims and must not declare a new IMotionRuntimePort owner type"
    },
    @{
        rule_id = "motion-planning-legacy-motion-port-still-declared"
        pattern = "class IIOControlPort"
        search_roots = @(
            "modules/motion-planning/domain/motion/ports",
            "modules/workflow/domain/include/domain/motion/ports"
        )
        detail = "legacy IO control public headers must be compatibility shims and must not declare a new IIOControlPort owner type"
    },
    @{
        rule_id = "motion-planning-legacy-motion-service-still-declared"
        pattern = "class MotionControlServiceImpl"
        search_roots = @(
            "modules/motion-planning/domain/motion/domain-services",
            "modules/workflow/domain/include/domain/motion/domain-services"
        )
        detail = "legacy motion control service headers must be aliases to runtime-execution owner implementations instead of declaring new classes"
    },
    @{
        rule_id = "motion-planning-legacy-motion-service-still-declared"
        pattern = "class MotionStatusServiceImpl"
        search_roots = @(
            "modules/motion-planning/domain/motion/domain-services",
            "modules/workflow/domain/include/domain/motion/domain-services"
        )
        detail = "legacy motion status service headers must be aliases to runtime-execution owner implementations instead of declaring new classes"
    }
)

$requiredOwnerTargets = @(
    @{
        path = "modules/process-planning/CMakeLists.txt"
        pattern = "siligen_process_planning_application_public"
    },
    @{
        path = "modules/coordinate-alignment/CMakeLists.txt"
        pattern = "siligen_coordinate_alignment_contracts_public"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "modules/job-ingest/CMakeLists.txt"
        pattern = "siligen_job_ingest_application_public"
    },
    @{
        path = "modules/dxf-geometry/CMakeLists.txt"
        pattern = "siligen_dxf_geometry_application_public"
    },
    @{
        path = "modules/process-path/CMakeLists.txt"
        pattern = "siligen_process_path_application_public"
    },
    @{
        path = "modules/motion-planning/CMakeLists.txt"
        pattern = "siligen_motion_planning_application_public"
    },
    @{
        path = "modules/dispense-packaging/CMakeLists.txt"
        pattern = "siligen_dispense_packaging_application_public"
    },
    @{
        path = "shared/contracts/device/CMakeLists.txt"
        pattern = "siligen_device_contracts"
    },
    @{
        path = "shared/contracts/device/CMakeLists.txt"
        pattern = "shared/contracts/device"
    }
)

$forbiddenOwnershipReferences = @(
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "usecases/dispensing/UploadFileUseCase.cpp"
        rule_id = "workflow-still-owns-upload-implementation"
        detail = "workflow application must not compile UploadFileUseCase implementation"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "services/dxf/DxfPbPreparationService.cpp"
        rule_id = "workflow-still-owns-pb-service-implementation"
        detail = "workflow application must not compile DxfPbPreparationService implementation"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/dispensing/UploadFileUseCaseTest.cpp"
        rule_id = "workflow-still-owns-upload-tests"
        detail = "workflow tests must not compile Upload/PB owner tests"
    },
    @{
        path = "modules/process-path/domain/trajectory/CMakeLists.txt"
        pattern = "domain-services/MotionPlanner.cpp"
        rule_id = "process-path-still-compiles-motion-planner"
        detail = "process-path must not compile MotionPlanner implementation after moving motion planning ownership to M7"
    },
    @{
        path = "modules/motion-planning/domain/motion/CMakeLists.txt"
        pattern = "../../../process-path/domain/trajectory/domain-services/MotionPlanner.cpp"
        rule_id = "motion-planning-still-compiles-process-path-motion-planner"
        detail = "motion-planning must compile its own MotionPlanner implementation from modules/motion-planning/domain/motion/domain-services"
    },
    @{
        path = "modules/motion-planning/application/services/motion_planning/MotionPlanningFacade.cpp"
        pattern = '#include "domain/trajectory/domain-services/MotionPlanner.h"'
        rule_id = "motion-planning-facade-still-uses-legacy-motion-planner-include"
        detail = "MotionPlanningFacade must include domain/motion/domain-services/MotionPlanner.h"
    },
    @{
        path = "modules/motion-planning/domain/motion/CMakeLists.txt"
        pattern = "siligen_process_path_domain_trajectory"
        rule_id = "motion-planning-still-falls-back-to-process-path-domain"
        detail = "motion-planning domain target must not fall back to siligen_process_path_domain_trajectory"
    },
    @{
        path = "modules/dispense-packaging/domain/dispensing/CMakeLists.txt"
        pattern = "siligen_process_path_domain_trajectory"
        rule_id = "dispense-packaging-still-falls-back-to-process-path-domain"
        detail = "dispense-packaging domain target must consume siligen_process_path_application_public instead of siligen_process_path_domain_trajectory"
    },
    @{
        path = "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp"
        pattern = '#include "domain-services/GeometryNormalizer.h"'
        rule_id = "dispense-packaging-planner-still-includes-process-path-domain-normalizer"
        detail = "DispensingPlannerService residual flow must consume ProcessPathFacade instead of process-path domain-services headers"
    },
    @{
        path = "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp"
        pattern = '#include "domain-services/ProcessAnnotator.h"'
        rule_id = "dispense-packaging-planner-still-includes-process-path-domain-annotator"
        detail = "DispensingPlannerService residual flow must consume ProcessPathFacade instead of process-path domain-services headers"
    },
    @{
        path = "modules/dispense-packaging/domain/dispensing/planning/domain-services/DispensingPlannerService.cpp"
        pattern = '#include "domain-services/TrajectoryShaper.h"'
        rule_id = "dispense-packaging-planner-still-includes-process-path-domain-shaper"
        detail = "DispensingPlannerService residual flow must consume ProcessPathFacade instead of process-path domain-services headers"
    },
    @{
        path = "modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h"
        pattern = 'domain/trajectory/domain-services/GeometryNormalizer.h'
        rule_id = "process-path-request-contract-still-includes-domain-normalizer"
        detail = "PathGenerationRequest must only include process_path/contracts/* headers"
    },
    @{
        path = "modules/process-path/contracts/include/process_path/contracts/PathGenerationRequest.h"
        pattern = 'domain/trajectory/domain-services/TrajectoryShaper.h'
        rule_id = "process-path-request-contract-still-includes-domain-shaper"
        detail = "PathGenerationRequest must only include process_path/contracts/* headers"
    },
    @{
        path = "modules/process-path/contracts/include/process_path/contracts/PathGenerationResult.h"
        pattern = 'domain/trajectory/domain-services/GeometryNormalizer.h'
        rule_id = "process-path-result-contract-still-includes-domain-normalizer"
        detail = "PathGenerationResult must only include process_path/contracts/* headers"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/dispensing/DispensingControllerTest.cpp"
        rule_id = "workflow-still-owns-dispense-packaging-domain-tests"
        detail = "workflow tests must not compile dispense-packaging owner domain tests"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/dispensing/TriggerPlannerTest.cpp"
        rule_id = "workflow-still-owns-dispense-packaging-domain-tests"
        detail = "workflow tests must not compile dispense-packaging owner domain tests"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/motion/CMPCoordinatedInterpolatorPrecisionTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/trajectory/MotionPlannerTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/trajectory/MotionPlannerConstraintTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/trajectory/InterpolationProgramPlannerTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/trajectory/PathRegularizerTest.cpp"
        rule_id = "workflow-still-owns-process-path-domain-tests"
        detail = "workflow tests must not compile process-path owner domain tests"
    },
    @{
        path = "modules/workflow/tests/integration/CMakeLists.txt"
        pattern = "../canonical/"
        rule_id = "workflow-integration-still-reuses-canonical-sources"
        detail = "workflow integration tests must compile local sources instead of reaching back into canonical/"
    },
    @{
        path = "modules/workflow/tests/integration/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
        rule_id = "workflow-integration-still-links-workflow-application-public"
        detail = "workflow integration tests must not link workflow application public aggregate"
    },
    @{
        path = "modules/workflow/tests/regression/CMakeLists.txt"
        pattern = "../canonical/"
        rule_id = "workflow-regression-still-reuses-canonical-sources"
        detail = "workflow regression tests must compile local sources instead of reaching back into canonical/"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "modules/job-ingest/contracts/include"
        rule_id = "workflow-canonical-still-uses-foreign-include-root"
        detail = "workflow canonical tests must not keep foreign owner include roots in canonical CMake"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "modules/runtime-execution/application/include"
        rule_id = "workflow-canonical-still-uses-foreign-include-root"
        detail = "workflow canonical tests must not keep foreign owner include roots in canonical CMake"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "modules/motion-planning/contracts/include"
        rule_id = "workflow-canonical-still-uses-foreign-include-root"
        detail = "workflow canonical tests must not keep foreign owner include roots in canonical CMake"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "modules/runtime-execution/contracts/runtime/include"
        rule_id = "workflow-canonical-still-uses-foreign-include-root"
        detail = "workflow canonical tests must not keep foreign owner include roots in canonical CMake"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "modules/runtime-execution/adapters/device/include"
        rule_id = "workflow-canonical-still-uses-foreign-include-root"
        detail = "workflow canonical tests must not keep foreign owner include roots in canonical CMake"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
        rule_id = "workflow-canonical-still-links-workflow-application-public"
        detail = "workflow canonical tests must not link workflow application public aggregate"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_dispense_packaging_application_public"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_application_dispensing"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_process_path_application_public"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_motion_planning_application_public"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_runtime_execution_motion_execution_services"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_job_ingest_application_public"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_dxf_geometry_application_public"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "siligen_module_dxf_geometry"
        rule_id = "workflow-canonical-still-links-foreign-owner-target"
        detail = "workflow canonical tests must not link foreign owner targets directly"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
        rule_id = "runtime-application-still-links-workflow-application-public"
        detail = "runtime execution application public target must not link workflow application public"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_workflow_application_headers"
        rule_id = "runtime-application-still-mutates-workflow-headers"
        detail = "runtime execution application must not mutate workflow application headers; workflow owner must declare this dependency itself"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_application_dispensing"
        rule_id = "runtime-application-still-mutates-workflow-dispensing"
        detail = "runtime execution application must not mutate workflow dispensing target; workflow owner must declare this dependency itself"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_process_runtime_core_application"
        rule_id = "runtime-application-still-mutates-workflow-process-runtime-core"
        detail = "runtime execution application must not mutate workflow process runtime application target"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_domain_compat"
        rule_id = "runtime-application-still-links-broad-domain-compat"
        detail = "runtime execution application headers must consume runtime-owned contracts instead of broad domain compat"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_safety_domain_services"
        rule_id = "runtime-application-still-links-workflow-safety-owner"
        detail = "runtime execution application must link runtime-owned safety services instead of workflow safety concrete"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "target_include_directories(siligen_runtime_execution_runtime_contracts INTERFACE"
        rule_id = "runtime-application-still-mutates-runtime-contract-include-surface"
        detail = "runtime execution application must not mutate runtime contract include roots from the application layer"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/safety/InterlockPolicyTest.cpp"
        rule_id = "workflow-canonical-still-owns-runtime-safety-tests"
        detail = "workflow canonical tests must not keep runtime-owned safety test sources"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/safety/SoftLimitValidatorTest.cpp"
        rule_id = "workflow-canonical-still-owns-runtime-safety-tests"
        detail = "workflow canonical tests must not keep runtime-owned safety test sources"
    },
    @{
        path = "modules/workflow/tests/canonical/CMakeLists.txt"
        pattern = "unit/domain/safety/SafetyOutputGuardTest.cpp"
        rule_id = "workflow-canonical-still-owns-runtime-safety-tests"
        detail = "workflow canonical tests must not keep runtime-owned safety test sources"
    },
    @{
        path = "modules/motion-planning/domain/motion/CMakeLists.txt"
        pattern = "domain-services/MotionControlServiceImpl.cpp"
        rule_id = "motion-planning-still-compiles-runtime-control-impl"
        detail = "motion-planning domain must not compile MotionControlServiceImpl after runtime/control ownership moved to runtime-execution"
    },
    @{
        path = "modules/motion-planning/domain/motion/CMakeLists.txt"
        pattern = "domain-services/MotionStatusServiceImpl.cpp"
        rule_id = "motion-planning-still-compiles-runtime-control-impl"
        detail = "motion-planning domain must not compile MotionStatusServiceImpl after runtime/control ownership moved to runtime-execution"
    },
    @{
        path = "modules/job-ingest/CMakeLists.txt"
        pattern = "siligen_application_dispensing"
        rule_id = "job-ingest-still-mutates-workflow-dispensing"
        detail = "job-ingest must not mutate workflow dispensing target; workflow owner must declare this dependency itself"
    },
    @{
        path = "modules/job-ingest/CMakeLists.txt"
        pattern = "siligen_process_runtime_core_application"
        rule_id = "job-ingest-still-mutates-workflow-process-runtime-core"
        detail = "job-ingest must not mutate workflow process runtime application target"
    },
    @{
        path = "modules/job-ingest/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
        rule_id = "job-ingest-still-mutates-workflow-public-target"
        detail = "job-ingest must not mutate workflow application public target"
    },
    @{
        path = "modules/job-ingest/CMakeLists.txt"
        pattern = "siligen_workflow_domain_public"
        rule_id = "job-ingest-still-links-workflow-domain-public"
        detail = "job-ingest must consume process-planning configuration ports instead of workflow domain public"
    },
    @{
        path = "modules/workflow/CMakeLists.txt"
        pattern = "add_library(siligen_workflow_application_public"
        rule_id = "workflow-root-still-defines-broad-application-aggregate"
        detail = "workflow root must not define the deleted siligen_workflow_application_public aggregate"
    },
    @{
        path = "modules/motion-planning/application/CMakeLists.txt"
        pattern = "siligen_application_motion"
        rule_id = "motion-planning-application-still-mutates-workflow-motion"
        detail = "motion-planning application must not mutate workflow motion target; workflow must declare owner dependencies itself"
    },
    @{
        path = "modules/motion-planning/application/CMakeLists.txt"
        pattern = "siligen_application_dispensing"
        rule_id = "motion-planning-application-still-mutates-workflow-dispensing"
        detail = "motion-planning application must not mutate workflow dispensing target; workflow must declare owner dependencies itself"
    },
    @{
        path = "modules/dispense-packaging/application/CMakeLists.txt"
        pattern = "siligen_application_dispensing"
        rule_id = "dispense-packaging-application-still-mutates-workflow-dispensing"
        detail = "dispense-packaging application must not mutate workflow dispensing target; workflow must declare owner dependencies itself"
    },
    @{
        path = "modules/dxf-geometry/CMakeLists.txt"
        pattern = "siligen_application_dispensing"
        rule_id = "dxf-geometry-still-mutates-workflow-dispensing"
        detail = "dxf-geometry must not mutate workflow dispensing target; workflow owner must declare this dependency itself"
    },
    @{
        path = "modules/dxf-geometry/CMakeLists.txt"
        pattern = "siligen_process_runtime_core_application"
        rule_id = "dxf-geometry-still-mutates-workflow-process-runtime-core"
        detail = "dxf-geometry must not mutate workflow process runtime application target"
    },
    @{
        path = "modules/dxf-geometry/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
        rule_id = "dxf-geometry-still-mutates-workflow-public-target"
        detail = "dxf-geometry must not mutate workflow application public target"
    },
    @{
        path = "modules/dxf-geometry/CMakeLists.txt"
        pattern = "siligen_workflow_domain_public"
        rule_id = "dxf-geometry-still-links-workflow-domain-public"
        detail = "dxf-geometry must consume process-planning configuration ports instead of workflow domain public"
    },
    @{
        path = "modules/job-ingest/tests/CMakeLists.txt"
        pattern = "siligen_workflow_domain_public"
        rule_id = "job-ingest-tests-still-link-workflow-domain-public"
        detail = "job-ingest tests must consume process-planning configuration ports instead of workflow domain public"
    },
    @{
        path = "modules/dxf-geometry/tests/CMakeLists.txt"
        pattern = "siligen_workflow_domain_public"
        rule_id = "dxf-geometry-tests-still-link-workflow-domain-public"
        detail = "dxf-geometry tests must consume process-planning configuration ports instead of workflow domain public"
    },
    @{
        path = "modules/process-planning/domain/configuration/CMakeLists.txt"
        pattern = "SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR"
        rule_id = "process-planning-config-still-uses-workflow-domain-include"
        detail = "process-planning configuration canonical target must export its own include roots"
    },
    @{
        path = "modules/process-path/domain/trajectory/CMakeLists.txt"
        pattern = "SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR"
        rule_id = "process-path-still-exports-workflow-domain-include"
        detail = "process-path trajectory canonical target must export motion-planning/public include roots instead of workflow domain include"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "usecases/dispensing/DispensingExecutionUseCase.cpp"
        rule_id = "workflow-still-owns-dispensing-execution-implementation"
        detail = "workflow application must not compile DispensingExecutionUseCase implementation"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "usecases/dispensing/DispensingExecutionUseCase.Setup.cpp"
        rule_id = "workflow-still-owns-dispensing-execution-implementation"
        detail = "workflow application must not compile DispensingExecutionUseCase setup implementation"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "usecases/dispensing/DispensingExecutionUseCase.Control.cpp"
        rule_id = "workflow-still-owns-dispensing-execution-implementation"
        detail = "workflow application must not compile DispensingExecutionUseCase control implementation"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "usecases/dispensing/DispensingExecutionUseCase.Async.cpp"
        rule_id = "workflow-still-owns-dispensing-execution-implementation"
        detail = "workflow application must not compile DispensingExecutionUseCase async implementation"
    },
    @{
        path = "modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
        pattern = "workflow/application/include"
        rule_id = "runtime-execution-header-still-forwards-to-workflow"
        detail = "runtime execution public header must own the declaration and must not forward back to workflow"
    },
    @{
        path = "modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
        pattern = '#include "domain/system/ports/IEventPublisherPort.h"'
        rule_id = "runtime-execution-header-still-includes-workflow-event-port"
        detail = "runtime execution public header must include runtime/contracts/system/IEventPublisherPort.h from shared/contracts/runtime"
    },
    @{
        path = "modules/runtime-execution/application/include/runtime_execution/application/usecases/dispensing/DispensingExecutionUseCase.h"
        pattern = '#include "domain/dispensing/ports/ITaskSchedulerPort.h"'
        rule_id = "runtime-execution-header-still-includes-workflow-task-scheduler"
        detail = "runtime execution public header must include runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/events/InMemoryEventPublisherAdapter.h"
        pattern = '#include "domain/system/ports/IEventPublisherPort.h"'
        rule_id = "runtime-event-adapter-still-includes-workflow-event-port"
        detail = "runtime event adapter must include runtime/contracts/system/IEventPublisherPort.h from shared/contracts/runtime"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/scheduling/TaskSchedulerAdapter.h"
        pattern = '#include "domain/dispensing/ports/ITaskSchedulerPort.h"'
        rule_id = "runtime-scheduler-adapter-still-includes-workflow-task-scheduler"
        detail = "runtime scheduling adapter must include runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
    },
    @{
        path = "apps/runtime-service/bootstrap/ContainerBootstrap.cpp"
        pattern = '#include "domain/system/ports/IEventPublisherPort.h"'
        rule_id = "runtime-process-bootstrap-still-includes-workflow-event-port"
        detail = "runtime process bootstrap must include runtime/contracts/system/IEventPublisherPort.h from shared/contracts/runtime"
    },
    @{
        path = "apps/runtime-service/bootstrap/ContainerBootstrap.cpp"
        pattern = '#include "domain/dispensing/ports/ITaskSchedulerPort.h"'
        rule_id = "runtime-process-bootstrap-still-includes-workflow-task-scheduler"
        detail = "runtime process bootstrap must include runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
    },
    @{
        path = "apps/planner-cli/CommandHandlers.Recipe.cpp"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "planner-cli-still-includes-legacy-recipe-usecases"
        detail = "planner-cli recipe consumer must include recipe_lifecycle/application/usecases/recipes/* public headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "gateway-builder-still-includes-legacy-recipe-usecases"
        detail = "transport-gateway builder must include recipe_lifecycle/application/usecases/recipes/* public headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = "Resolve<Application::UseCases::Motion::Homing::HomeAxesUseCase>"
        rule_id = "gateway-builder-still-resolves-legacy-motion-usecases"
        detail = "transport-gateway builder must resolve the Stage B motion entry via MotionControlUseCase instead of individual workflow motion use cases"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = "Resolve<Application::UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>"
        rule_id = "gateway-builder-still-resolves-legacy-motion-usecases"
        detail = "transport-gateway builder must resolve the Stage B motion entry via MotionControlUseCase instead of individual workflow motion use cases"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = "Resolve<Application::UseCases::Motion::Manual::ManualMotionControlUseCase>"
        rule_id = "gateway-builder-still-resolves-legacy-motion-usecases"
        detail = "transport-gateway builder must resolve the Stage B motion entry via MotionControlUseCase instead of individual workflow motion use cases"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = "Resolve<Application::UseCases::Motion::Monitoring::MotionMonitoringUseCase>"
        rule_id = "gateway-builder-still-resolves-legacy-motion-usecases"
        detail = "transport-gateway builder must resolve the Stage B motion entry via MotionControlUseCase instead of individual workflow motion use cases"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h"
        pattern = "std::shared_ptr<UseCases::Motion::Homing::HomeAxesUseCase>"
        rule_id = "gateway-motion-facade-still-holds-legacy-motion-usecases"
        detail = "TcpMotionFacade must hold the Stage B MotionControlUseCase entry instead of per-use-case workflow motion dependencies"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h"
        pattern = "std::shared_ptr<UseCases::Motion::Homing::EnsureAxesReadyZeroUseCase>"
        rule_id = "gateway-motion-facade-still-holds-legacy-motion-usecases"
        detail = "TcpMotionFacade must hold the Stage B MotionControlUseCase entry instead of per-use-case workflow motion dependencies"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h"
        pattern = "std::shared_ptr<UseCases::Motion::Manual::ManualMotionControlUseCase>"
        rule_id = "gateway-motion-facade-still-holds-legacy-motion-usecases"
        detail = "TcpMotionFacade must hold the Stage B MotionControlUseCase entry instead of per-use-case workflow motion dependencies"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpMotionFacade.h"
        pattern = "std::shared_ptr<UseCases::Motion::Monitoring::MotionMonitoringUseCase>"
        rule_id = "gateway-motion-facade-still-holds-legacy-motion-usecases"
        detail = "TcpMotionFacade must hold the Stage B MotionControlUseCase entry instead of per-use-case workflow motion dependencies"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpRecipeFacade.h"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "gateway-facade-still-includes-legacy-recipe-usecases"
        detail = "transport-gateway recipe facade must include recipe_lifecycle/application/usecases/recipes/* public headers"
    },
    @{
        path = "apps/runtime-service/container/ApplicationContainer.Recipes.cpp"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "runtime-bootstrap-still-includes-legacy-recipe-usecases"
        detail = "runtime bootstrap recipe container must include recipe_lifecycle/application/usecases/recipes/* public headers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp"
        pattern = '#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "gateway-dispatcher-still-includes-legacy-recipe-serializer"
        detail = "transport-gateway dispatcher must include recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "apps/runtime-service/runtime/recipes/RecipeBundleSerializer.h"
        pattern = '#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-bootstrap-recipe-serializer-still-includes-legacy-json-header"
        detail = "runtime bootstrap recipe persistence must include recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "apps/runtime-service/runtime/recipes/RecipeFileRepository.cpp"
        pattern = '#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-bootstrap-recipe-repository-still-includes-legacy-json-header"
        detail = "runtime bootstrap recipe persistence must include recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "apps/runtime-service/runtime/recipes/ParameterSchemaFileProvider.cpp"
        pattern = '#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-bootstrap-parameter-schema-still-includes-legacy-json-header"
        detail = "runtime bootstrap recipe persistence must include recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "apps/runtime-service/runtime/recipes/TemplateFileRepository.cpp"
        pattern = '#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-bootstrap-template-repository-still-includes-legacy-json-header"
        detail = "runtime bootstrap recipe persistence must include recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "apps/runtime-service/runtime/recipes/AuditFileRepository.cpp"
        pattern = '#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-bootstrap-audit-repository-still-includes-legacy-json-header"
        detail = "runtime bootstrap recipe persistence must include recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
    }
)

$requiredDeletedFiles = @(
    @{
        path = "modules/workflow/application/include/application/services/dxf/DxfPbPreparationService.h"
        rule_id = "workflow-dxf-wrapper-still-exists"
        detail = "workflow DXF wrapper must be deleted after consumers move to dxf_geometry/application/services/dxf/DxfPbPreparationService.h"
    },
    @{
        path = "modules/workflow/application/planning-trigger/PlanningUseCase.cpp"
        rule_id = "workflow-planning-trigger-source-still-exists"
        detail = "workflow planning-trigger source must stay deleted after planning ownership moves out of workflow/application"
    },
    @{
        path = "modules/workflow/tests/integration/PlanningFailureSurfaceTest.cpp"
        rule_id = "workflow-integration-planning-failure-test-still-exists"
        detail = "workflow integration planning failure source must stay deleted after owner tests move out of workflow/tests"
    },
    @{
        path = "modules/workflow/domain/domain"
        rule_id = "workflow-domain-residue-root-still-exists"
        detail = "workflow domain residue root must stay deleted after M0 closeout"
    },
    @{
        path = "modules/workflow/domain/include/domain/system/ports/IEventPublisherPort.h"
        rule_id = "workflow-system-event-publisher-shim-still-exists"
        detail = "workflow legacy domain/system event publisher shim must be deleted after all live consumers move to runtime/contracts/system/IEventPublisherPort.h"
    },
    @{
        path = "modules/workflow/domain/include/domain/diagnostics/ports/IDiagnosticsPort.h"
        rule_id = "workflow-generic-diagnostics-sink-shim-still-exists"
        detail = "workflow generic diagnostics sink header must be deleted after cutover to trace_diagnostics/contracts/IDiagnosticsPort.h"
    },
    @{
        path = "modules/workflow/domain/include/domain/machine/aggregates/DispenserModel.h"
        rule_id = "workflow-machine-aggregate-bridge-still-exists"
        detail = "workflow machine aggregate bridge header must be deleted after runtime-host owns machine execution state concrete"
    },
    @{
        path = "modules/workflow/domain/include/domain/recipes/serialization/RecipeJsonSerializer.h"
        rule_id = "workflow-recipe-serializer-header-still-exists"
        detail = "workflow recipe serializer public header must be deleted after cutover to recipe_lifecycle/adapters/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "modules/workflow/domain/domain/recipes/serialization/RecipeJsonSerializer.cpp"
        rule_id = "workflow-recipe-serializer-impl-still-exists"
        detail = "workflow recipe serializer implementation must be deleted after cutover to modules/recipe-lifecycle/adapters/serialization/RecipeJsonSerializer.cpp"
    },
    @{
        path = "modules/process-path/contracts/include/process_path/contracts/IDXFPathSourcePort.h"
        rule_id = "process-path-dxf-contract-header-still-exists"
        detail = "process-path must not restore the deleted IDXFPathSourcePort contract header"
    },
    @{
        path = "modules/process-path/domain/trajectory/ports/IPathSourcePort.h"
        rule_id = "process-path-domain-pathsource-bridge-still-exists"
        detail = "process-path domain trajectory path-source bridge header must stay deleted after contract closeout"
    },
    @{
        path = "modules/process-path/domain/trajectory/ports/IDXFPathSourcePort.h"
        rule_id = "process-path-domain-dxf-pathsource-bridge-still-exists"
        detail = "process-path domain trajectory DXF path-source bridge header must stay deleted after contract closeout"
    },
    @{
        path = "modules/workflow/tests/unit/MotionOwnerBehaviorTest.cpp"
        rule_id = "workflow-unit-still-owns-motion-runtime-test"
        detail = "workflow/tests/unit must not retain foreign-owner motion runtime source-bearing tests"
    },
    @{
        path = "modules/workflow/tests/unit/PlanningFailureSurfaceTest.cpp"
        rule_id = "workflow-unit-still-owns-planning-failure-test"
        detail = "workflow/tests/unit must not retain source-bearing planning integration tests"
    },
    @{
        path = "modules/workflow/application/include/application/ports/dispensing/WorkflowExecutionPortAdapters.h"
        rule_id = "workflow-runtime-execution-adapter-header-still-exists"
        detail = "workflow must not keep runtime-execution port adapter in the public include surface"
    },
    @{
        path = "modules/workflow/domain/include/domain/planning-boundary/ports/ISpatialIndexPort.h"
        rule_id = "workflow-planning-boundary-port-still-exists"
        detail = "workflow planning-boundary spatial index port must stay deleted after canonical move to dispense-packaging"
    },
    @{
        path = "modules/workflow/domain/motion-core/CMakeLists.txt"
        rule_id = "workflow-motion-core-cmake-still-exists"
        detail = "workflow motion-core CMake target must stay deleted after safety payload exit"
    },
    @{
        path = "modules/topology-feature/contracts/legacy-bridge/include/infrastructure/adapters/planning/geometry/ContourAugmenterAdapter.h"
        rule_id = "topology-feature-legacy-bridge-header-still-exists"
        detail = "topology-feature legacy bridge public header must be deleted after cutover to the canonical contract surface"
    },
    @{
        path = "modules/topology-feature/domain/geometry/CMakeLists.txt"
        rule_id = "topology-feature-domain-compat-cmake-still-exists"
        detail = "topology-feature domain/geometry compat shell must be deleted after cutover to the canonical adapter owner"
    },
    @{
        path = "modules/topology-feature/domain/geometry/ContourAugmenterAdapter.h"
        rule_id = "topology-feature-domain-compat-header-still-exists"
        detail = "topology-feature domain/geometry compat header must be deleted after cutover to the canonical adapter owner"
    },
    @{
        path = "modules/topology-feature/domain/geometry/ContourAugmenterAdapter.cpp"
        rule_id = "topology-feature-domain-compat-impl-still-exists"
        detail = "topology-feature domain/geometry compat implementation must be deleted after cutover to the canonical adapter owner"
    },
    @{
        path = "modules/topology-feature/domain/geometry/ContourAugmenterAdapter.stub.cpp"
        rule_id = "topology-feature-domain-compat-stub-still-exists"
        detail = "topology-feature domain/geometry compat stub must be deleted after cutover to the canonical adapter owner"
    }
)

$forbiddenCompatReferences += @(
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_headers"
        rule_id = "workflow-application-headers-still-reexport-runtime-execution"
        detail = "workflow application header bundle must not re-export runtime-execution application headers"
    },
    @{
        path = "modules/workflow/domain/CMakeLists.txt"
        pattern = "../../coordinate-alignment/domain/machine"
        rule_id = "workflow-domain-root-still-loads-machine-canonical"
        detail = "workflow domain root must not preload the coordinate-alignment machine canonical target after WF-B16 collapse"
    },
    @{
        path = "modules/workflow/domain/CMakeLists.txt"
        pattern = "domain_machine"
        rule_id = "workflow-domain-root-still-defines-machine-wrapper"
        detail = "workflow domain root must not keep the deleted domain_machine wrapper target after WF-B16 collapse"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "../../workflow/domain"
        rule_id = "runtime-application-still-includes-workflow-domain-root"
        detail = "runtime execution application implementation include dirs must not include ../../workflow/domain"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_dispensing_planning_compat"
        rule_id = "runtime-host-still-links-planner-compat"
        detail = "runtime host live targets must not link siligen_workflow_dispensing_planning_compat"
    },
    @{
        path = "modules/runtime-execution/application/CMakeLists.txt"
        pattern = "siligen_workflow_dispensing_planning_compat"
        rule_id = "runtime-application-still-links-planner-compat"
        detail = "runtime execution application live target must not link siligen_workflow_dispensing_planning_compat"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "siligen_workflow_dispensing_planning_compat"
        rule_id = "workflow-application-still-links-planner-compat"
        detail = "workflow application live target must not link siligen_workflow_dispensing_planning_compat"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_workflow_dispensing_planning_compat"
        rule_id = "planner-cli-still-links-planner-compat"
        detail = "planner-cli live target must not link siligen_workflow_dispensing_planning_compat"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_workflow_dispensing_planning_compat"
        rule_id = "runtime-gateway-still-links-planner-compat"
        detail = "runtime-gateway live target must not link siligen_workflow_dispensing_planning_compat"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "ContainerBootstrap.cpp"
        rule_id = "runtime-host-core-still-compiles-bootstrap"
        detail = "runtime host core must not compile ContainerBootstrap.cpp after moving process bootstrap to apps/runtime-service"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "bootstrap/InfrastructureBindings"
        rule_id = "runtime-host-core-still-compiles-bootstrap"
        detail = "runtime host core must not compile InfrastructureBindings after moving process bootstrap to apps/runtime-service"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "container/ApplicationContainer"
        rule_id = "runtime-host-core-still-compiles-app-container"
        detail = "runtime host core must not compile ApplicationContainer sources after moving process composition to apps/runtime-service"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "factories/InfrastructureAdapterFactory.cpp"
        rule_id = "runtime-host-core-still-compiles-infra-factory"
        detail = "runtime host core must not compile InfrastructureAdapterFactory after moving process composition to apps/runtime-service"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "runtime/configuration/"
        rule_id = "runtime-host-core-still-compiles-configuration"
        detail = "runtime host core must not compile runtime/configuration sources"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "runtime/recipes/"
        rule_id = "runtime-host-core-still-compiles-recipe-persistence"
        detail = "runtime host core must not compile runtime/recipes sources"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "runtime/storage/files/"
        rule_id = "runtime-host-core-still-compiles-storage"
        detail = "runtime host core must not compile runtime/storage/files sources"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "security/"
        rule_id = "runtime-host-core-still-compiles-security"
        detail = "runtime host core must not compile security sources"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_job_ingest_application_public"
        rule_id = "runtime-host-core-still-links-broad-app-surface"
        detail = "siligen_runtime_host must not PUBLIC link job-ingest after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
        rule_id = "runtime-host-core-still-links-broad-app-surface"
        detail = "siligen_runtime_host must not PUBLIC link workflow application after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_adapters_public"
        rule_id = "runtime-host-core-still-links-broad-app-surface"
        detail = "siligen_runtime_host must not PUBLIC link workflow adapters after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_domain_public"
        rule_id = "runtime-host-core-still-links-recipe-owner-surface"
        detail = "siligen_runtime_host must not PUBLIC link workflow_recipe domain after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_application_public"
        rule_id = "runtime-host-core-still-links-recipe-owner-surface"
        detail = "siligen_runtime_host must not PUBLIC link workflow_recipe application after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_serialization_public"
        rule_id = "runtime-host-core-still-links-recipe-owner-surface"
        detail = "siligen_runtime_host must not PUBLIC link workflow_recipe serialization after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_dxf_geometry_pb_path_source_adapter"
        rule_id = "runtime-host-core-still-links-dxf-adapter"
        detail = "siligen_runtime_host must not PUBLIC link DXF/parsing adapters after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_runtime_host_storage"
        rule_id = "runtime-host-core-still-links-storage-compat"
        detail = "siligen_runtime_host must not PUBLIC link runtime host storage compatibility targets after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_runtime_recipe_persistence"
        rule_id = "runtime-host-core-still-links-recipe-persistence-compat"
        detail = "siligen_runtime_host must not PUBLIC link runtime recipe persistence compatibility targets after S5"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_coordinate_alignment_domain_machine"
        rule_id = "runtime-host-core-still-links-machine-residual"
        detail = "runtime-host machine execution state must not link coordinate-alignment machine residual concrete"
    },
    @{
        path = "modules/runtime-execution/runtime/host/tests/CMakeLists.txt"
        pattern = "siligen_coordinate_alignment_domain_machine"
        rule_id = "runtime-host-tests-still-link-machine-residual"
        detail = "runtime-host tests must exercise runtime-owned machine execution state without coordinate-alignment machine residual"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_topology_feature_legacy_contour_bridge_public"
        rule_id = "planner-cli-still-links-topology-legacy-bridge"
        detail = "planner-cli must consume siligen_module_topology_feature instead of the deleted topology legacy contour bridge target"
    },
    @{
        path = "modules/topology-feature/contracts/CMakeLists.txt"
        pattern = "siligen_topology_feature_legacy_contour_bridge_public"
        rule_id = "topology-feature-still-defines-legacy-bridge-target"
        detail = "topology-feature contracts must not define the deleted legacy contour bridge target"
    },
    @{
        path = "modules/topology-feature/contracts/CMakeLists.txt"
        pattern = "legacy-bridge/include"
        rule_id = "topology-feature-contracts-still-export-legacy-include-root"
        detail = "topology-feature contracts must not export or validate the deleted legacy bridge include root"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_runtime_host"
        rule_id = "planner-cli-still-links-runtime-host-directly"
        detail = "planner-cli must consume siligen_runtime_process_bootstrap_public instead of siligen_runtime_host"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_runtime_host_infrastructure"
        rule_id = "planner-cli-still-links-runtime-host-infrastructure"
        detail = "planner-cli must not link siligen_runtime_host_infrastructure"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_runtime_host_storage"
        rule_id = "planner-cli-still-links-runtime-host-storage"
        detail = "planner-cli must not link siligen_runtime_host_storage"
    },
    @{
        path = "apps/runtime-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_host"
        rule_id = "runtime-gateway-app-still-links-runtime-host-directly"
        detail = "runtime-gateway app must consume siligen_runtime_process_bootstrap_public instead of siligen_runtime_host"
    },
    @{
        path = "apps/runtime-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_host_infrastructure"
        rule_id = "runtime-gateway-app-still-links-runtime-host-infrastructure"
        detail = "runtime-gateway app must not link siligen_runtime_host_infrastructure"
    },
    @{
        path = "apps/runtime-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_host_storage"
        rule_id = "runtime-gateway-app-still-links-runtime-host-storage"
        detail = "runtime-gateway app must not link siligen_runtime_host_storage"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_host_infrastructure"
        rule_id = "transport-gateway-still-links-runtime-host-infrastructure"
        detail = "transport-gateway must not link siligen_runtime_host_infrastructure"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_host_storage"
        rule_id = "transport-gateway-still-links-runtime-host-storage"
        detail = "transport-gateway must not link siligen_runtime_host_storage"
    }
)

$forbiddenScopedSearches += @(
    @{
        rule_id = "live-targets-still-reference-siligen-domain"
        pattern = "siligen_domain"
        search_roots = @(
            "modules",
            "apps",
            "tests"
        )
        detail = "live build files must not define or link the deleted siligen_domain compat target"
    },
    @{
        rule_id = "live-targets-still-reference-siligen-motion-core"
        pattern = "siligen_motion_core"
        search_roots = @(
            "modules",
            "apps",
            "tests"
        )
        detail = "live build files must not define or link the deleted siligen_motion_core compat target"
    },
    @{
        rule_id = "live-targets-still-reference-siligen-domain-services"
        pattern = "siligen_domain_services"
        search_roots = @(
            "modules",
            "apps",
            "tests"
        )
        detail = "live build files must not define or link the deleted siligen_domain_services compat target"
    },
    @{
        rule_id = "live-targets-still-reference-siligen-triggering"
        pattern = "siligen_triggering"
        search_roots = @(
            "modules",
            "apps",
            "tests"
        )
        detail = "live build files must not define or link the deleted workflow siligen_triggering target"
    },
    @{
        rule_id = "live-targets-still-reference-domain-machine-wrapper"
        pattern = "domain_machine"
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live build files must not define or link the deleted workflow domain_machine wrapper target"
    },
    @{
        rule_id = "live-targets-still-reference-siligen-trajectory-wrapper"
        pattern = "siligen_trajectory"
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live build files must not define or link the deleted workflow siligen_trajectory wrapper target"
    },
    @{
        rule_id = "live-targets-still-reference-siligen-configuration-wrapper"
        pattern = "siligen_configuration"
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live build files must not define or link the deleted workflow siligen_configuration wrapper target"
    },
    @{
        rule_id = "motion-planning-live-headers-still-include-process-path-domain"
        pattern = '#include "domain/trajectory/value-objects/ProcessPath.h"'
        search_roots = @(
            "modules/motion-planning/application/include",
            "modules/motion-planning/domain/motion"
        )
        detail = "motion-planning live headers must include process_path/contracts/ProcessPath.h instead of M6 domain headers"
    },
    @{
        rule_id = "live-targets-still-include-workflow-path-value-object-bridge"
        pattern = '#include "domain/trajectory/value-objects/Path.h"'
        search_roots = @(
            "modules/motion-planning",
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps"
        )
        detail = "live targets must include canonical process_path/contracts/Path.h instead of the deleted workflow bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-primitive-value-object-bridge"
        pattern = '#include "domain/trajectory/value-objects/Primitive.h"'
        search_roots = @(
            "modules/motion-planning",
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps"
        )
        detail = "live targets must include canonical process_path/contracts/Primitive.h instead of the deleted workflow bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-process-config-value-object-bridge"
        pattern = '#include "domain/trajectory/value-objects/ProcessConfig.h"'
        search_roots = @(
            "modules/motion-planning",
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps"
        )
        detail = "live targets must include canonical process_path/contracts/ProcessConfig.h instead of the deleted workflow bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-process-path-value-object-bridge"
        pattern = '#include "domain/trajectory/value-objects/ProcessPath.h"'
        search_roots = @(
            "modules/motion-planning",
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps"
        )
        detail = "live targets must include canonical process_path/contracts/ProcessPath.h instead of the deleted workflow bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-geometry-utils-value-object-bridge"
        pattern = '#include "domain/trajectory/value-objects/GeometryUtils.h"'
        search_roots = @(
            "modules/motion-planning",
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps"
        )
        detail = "live targets must include canonical process_path/contracts/GeometryUtils.h instead of the deleted workflow bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-geometry-boost-adapter-bridge"
        pattern = '#include "domain/trajectory/value-objects/GeometryBoostAdapter.h"'
        search_roots = @(
            "modules/motion-planning",
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps"
        )
        detail = "live targets must not include the deleted workflow GeometryBoostAdapter bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-planning-report-bridge"
        pattern = '#include "domain/trajectory/value-objects/PlanningReport.h"'
        search_roots = @(
            "modules/motion-planning",
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps"
        )
        detail = "live targets must not include the deleted workflow PlanningReport bridge header"
    },
    @{
        rule_id = "dispense-packaging-live-targets-still-use-process-path-owner-namespace"
        pattern = 'Siligen::Domain::Trajectory::ValueObjects::'
        search_roots = @(
            "modules/dispense-packaging",
            "modules/workflow/domain/domain/dispensing"
        )
        detail = "dispense-packaging consumers must use process_path/contracts semantics instead of Siligen::Domain::Trajectory::ValueObjects"
    },
    @{
        rule_id = "runtime-and-app-live-targets-still-use-process-path-owner-namespace"
        pattern = 'Siligen::Domain::Trajectory::ValueObjects::'
        search_roots = @(
            "modules/runtime-execution",
            "modules/workflow/application",
            "apps"
        )
        detail = "runtime and app consumers must use process_path/contracts semantics instead of Siligen::Domain::Trajectory::ValueObjects"
    },
    @{
        rule_id = "process-path-owner-still-includes-legacy-value-object-headers"
        pattern = '#include "domain/trajectory/value-objects/'
        search_roots = @(
            "modules/process-path/domain/trajectory/domain-services",
            "modules/process-path/tests/unit/domain/trajectory",
            "modules/process-path/application/services/process_path"
        )
        detail = "process-path owner internals must include canonical process_path/contracts headers instead of legacy value-object headers"
    },
    @{
        rule_id = "live-targets-still-include-workflow-pathsource-bridge"
        pattern = '#include "domain/trajectory/ports/IPathSourcePort.h"'
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live targets must include process_path/contracts/IPathSourcePort.h instead of the deleted workflow bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-dxf-pathsource-bridge"
        pattern = '#include "domain/trajectory/ports/IDXFPathSourcePort.h"'
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live targets must include process_path/contracts/IPathSourcePort.h instead of the deleted workflow bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-planning-boundary-port"
        pattern = '#include "domain/planning-boundary/ports/ISpatialIndexPort.h"'
        search_roots = @(
            "modules/dispense-packaging",
            "modules/workflow/application",
            "modules/workflow/domain/domain",
            "apps",
            "tests"
        )
        detail = "live targets must include domain/dispensing/planning/ports/ISpatialIndexPort.h instead of the deleted workflow planning-boundary port"
    },
    @{
        rule_id = "live-targets-still-include-deleted-process-path-dxf-contract"
        pattern = '#include "process_path/contracts/IDXFPathSourcePort.h"'
        search_roots = @(
            "modules",
            "apps",
            "tests"
        )
        detail = "live targets must not include the deleted process_path/contracts/IDXFPathSourcePort.h header; use process_path/contracts/IPathSourcePort.h"
    },
    @{
        rule_id = "live-targets-still-include-workflow-geometry-normalizer-bridge"
        pattern = '#include "domain/trajectory/domain-services/GeometryNormalizer.h"'
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live targets must not include the deleted workflow GeometryNormalizer bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-process-annotator-bridge"
        pattern = '#include "domain/trajectory/domain-services/ProcessAnnotator.h"'
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live targets must not include the deleted workflow ProcessAnnotator bridge header"
    },
    @{
        rule_id = "live-targets-still-include-workflow-trajectory-shaper-bridge"
        pattern = '#include "domain/trajectory/domain-services/TrajectoryShaper.h"'
        search_roots = @(
            "modules",
            "apps"
        )
        detail = "live targets must not include the deleted workflow TrajectoryShaper bridge header"
    },
    @{
        rule_id = "live-targets-still-reference-process-runtime-core-planning"
        pattern = "siligen_process_runtime_core_planning"
        search_roots = @(
            "modules/process-path/application",
            "modules/motion-planning/application",
            "modules/runtime-execution/application",
            "modules/runtime-execution/runtime/host",
            "apps/planner-cli",
            "apps/runtime-gateway/transport-gateway"
        )
        detail = "live targets must not add deprecated siligen_process_runtime_core_planning references"
    },
    @{
        rule_id = "live-targets-still-reference-planner-compat"
        pattern = "siligen_workflow_dispensing_planning_compat"
        search_roots = @(
            "modules/workflow/application",
            "modules/runtime-execution/application",
            "modules/runtime-execution/runtime/host",
            "apps/planner-cli",
            "apps/runtime-gateway/transport-gateway"
        )
        detail = "live targets must not reference the deprecated siligen_workflow_dispensing_planning_compat target"
    },
    @{
        rule_id = "planner-cli-still-includes-legacy-planning-report-contract"
        pattern = '#include "domain/motion/value-objects/MotionPlanningReport.h"'
        search_roots = @(
            "apps/planner-cli",
            "modules/workflow/application/include",
            "modules/dispense-packaging/application/include"
        )
        detail = "US3 planning consumers must include motion_planning/contracts/MotionPlanningReport.h"
    },
    @{
        rule_id = "planner-cli-still-includes-legacy-trajectory-planning-report"
        pattern = '#include "domain/trajectory/value-objects/PlanningReport.h"'
        search_roots = @(
            "apps/planner-cli"
        )
        detail = "planner-cli must export planning report data from canonical motion_planning contracts"
    },
    @{
        rule_id = "planner-cli-still-includes-interpolator-implementation-header"
        pattern = '#include "domain/motion/domain-services/interpolation/TrajectoryInterpolatorBase.h"'
        search_roots = @(
            "apps/planner-cli"
        )
        detail = "planner-cli must not directly include motion interpolator implementation headers"
    },
    @{
        rule_id = "runtime-service-still-includes-legacy-interpolation-port-header"
        pattern = '#include "domain/motion/ports/IInterpolationPort.h"'
        search_roots = @(
            "apps/runtime-service",
            "modules/workflow/application/usecases/motion/runtime",
            "modules/workflow/application/usecases/motion/trajectory"
        )
        detail = "US3 runtime/control consumers must include runtime_execution/contracts/motion/IInterpolationPort.h"
    },
    @{
        rule_id = "runtime-service-still-includes-legacy-io-port-header"
        pattern = '#include "domain/motion/ports/IIOControlPort.h"'
        search_roots = @(
            "apps/runtime-service",
            "modules/workflow/application/usecases/motion/runtime"
        )
        detail = "US3 runtime/control consumers must include runtime_execution/contracts/motion/IIOControlPort.h"
    },
    @{
        rule_id = "runtime-service-still-includes-legacy-motion-runtime-port-header"
        pattern = '#include "domain/motion/ports/IMotionRuntimePort.h"'
        search_roots = @(
            "apps/runtime-service"
        )
        detail = "US3 runtime/control consumers must include runtime_execution/contracts/motion/IMotionRuntimePort.h"
    },
    @{
        rule_id = "workflow-public-headers-still-include-legacy-interpolation-port-header"
        pattern = '#include "domain/motion/ports/IInterpolationPort.h"'
        search_roots = @(
            "modules/workflow/application/include"
        )
        detail = "workflow public motion headers must include runtime_execution/contracts/motion/IInterpolationPort.h"
    },
    @{
        rule_id = "workflow-public-headers-still-include-legacy-io-port-header"
        pattern = '#include "domain/motion/ports/IIOControlPort.h"'
        search_roots = @(
            "modules/workflow/application/include"
        )
        detail = "workflow public motion headers must include runtime_execution/contracts/motion/IIOControlPort.h"
    },
    @{
        rule_id = "live-targets-still-include-legacy-system-event-publisher-header"
        pattern = '#include "domain/system/ports/IEventPublisherPort.h"'
        search_roots = @(
            "modules",
            "apps",
            "tests"
        )
        detail = "live code must include runtime/contracts/system/IEventPublisherPort.h instead of the deleted workflow domain/system shim"
    },
    @{
        rule_id = "live-targets-still-include-workflow-generic-diagnostics-sink"
        pattern = '#include "domain/diagnostics/ports/IDiagnosticsPort.h"'
        search_roots = @(
            "modules",
            "apps",
            "tests"
        )
        detail = "live code must include trace_diagnostics/contracts/IDiagnosticsPort.h instead of the deleted workflow diagnostics sink header"
    },
    @{
        rule_id = "workflow-tests-unit-still-source-bearing"
        pattern = "add_executable("
        search_roots = @(
            "modules/workflow/tests/unit"
        )
        detail = "workflow/tests/unit must remain registration-only after canonical test-source consolidation"
    }
)

$findings = New-Object System.Collections.Generic.List[object]

foreach ($target in $directWorkflowTargets) {
    $matches = Get-FixedStringMatches -Pattern $target -SearchRoots @("modules", "apps")

    foreach ($match in $matches) {
        if ($match -notmatch "^(?<path>[^:]+):(?<line>\d+):(?<text>.*)$") {
            continue
        }

        $matchPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $Matches.path
        if ($allowedDirectWorkflowReferences -contains $matchPath) {
            continue
        }

        $findings.Add([pscustomobject]@{
            rule_id = "direct-workflow-target-reference"
            severity = "error"
            target = $target
            file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $matchPath
            line = [int]$Matches.line
            detail = $Matches.text.Trim()
        })
    }
}

foreach ($quarantineReference in $allowedHardwareDiagnosticsQuarantineReferences) {
    $matches = Get-FixedStringMatches -Pattern $quarantineReference.pattern -SearchRoots $quarantineReference.search_roots

    foreach ($match in $matches) {
        if ($match -notmatch "^(?<path>[^:]+):(?<line>\d+):(?<text>.*)$") {
            continue
        }

        $matchPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $Matches.path
        $allowedPaths = @($quarantineReference.allowed_paths | ForEach-Object {
            Resolve-AbsolutePath -BasePath $repoRoot -PathValue $_
        })
        if ($allowedPaths -contains $matchPath) {
            continue
        }

        $findings.Add([pscustomobject]@{
            rule_id = $quarantineReference.rule_id
            severity = "error"
            target = $quarantineReference.pattern
            file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $matchPath
            line = [int]$Matches.line
            detail = $quarantineReference.detail
        })
    }
}

foreach ($expectation in $requiredBridgeReferences) {
    $fullPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $expectation.path
    if (-not (Test-Path $fullPath)) {
        $findings.Add([pscustomobject]@{
            rule_id = "missing-required-file"
            severity = "error"
            target = $expectation.pattern
            file = $expectation.path
            line = 0
            detail = "required bridge consumer file does not exist"
        })
        continue
    }

    $escapedPattern = [regex]::Escape($expectation.pattern)
    if (-not (Select-String -Path $fullPath -Pattern $escapedPattern -Quiet)) {
        $findings.Add([pscustomobject]@{
            rule_id = "missing-runtime-bridge-reference"
            severity = "error"
            target = $expectation.pattern
            file = $expectation.path
            line = 0
            detail = "expected canonical owner or bridge reference is missing"
        })
    }
}

foreach ($expectation in $requiredOwnerTargets) {
    $fullPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $expectation.path
    if (-not (Test-Path $fullPath)) {
        $findings.Add([pscustomobject]@{
            rule_id = "missing-owner-target-file"
            severity = "error"
            target = $expectation.pattern
            file = $expectation.path
            line = 0
            detail = "required owner module target file does not exist"
        })
        continue
    }

    $escapedPattern = [regex]::Escape($expectation.pattern)
    if (-not (Select-String -Path $fullPath -Pattern $escapedPattern -Quiet)) {
        $findings.Add([pscustomobject]@{
            rule_id = "missing-owner-target-reference"
            severity = "error"
            target = $expectation.pattern
            file = $expectation.path
            line = 0
            detail = "expected owner module public target reference is missing"
        })
    }
}

foreach ($forbiddenReference in $forbiddenOwnershipReferences) {
    $fullPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $forbiddenReference.path
    if (-not (Test-Path $fullPath)) {
        $findings.Add([pscustomobject]@{
            rule_id = "missing-forbidden-check-file"
            severity = "error"
            target = $forbiddenReference.pattern
            file = $forbiddenReference.path
            line = 0
            detail = "required ownership check file does not exist"
        })
        continue
    }

    $matches = Select-String -Path $fullPath -Pattern ([regex]::Escape($forbiddenReference.pattern))
    foreach ($match in $matches) {
        $findings.Add([pscustomobject]@{
            rule_id = $forbiddenReference.rule_id
            severity = "error"
            target = $forbiddenReference.pattern
            file = $forbiddenReference.path
            line = $match.LineNumber
            detail = $forbiddenReference.detail
        })
    }
}

foreach ($forbiddenReference in $forbiddenCompatReferences) {
    $fullPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $forbiddenReference.path
    if (-not (Test-Path $fullPath)) {
        $findings.Add([pscustomobject]@{
            rule_id = "missing-forbidden-check-file"
            severity = "error"
            target = $forbiddenReference.pattern
            file = $forbiddenReference.path
            line = 0
            detail = "required compat check file does not exist"
        })
        continue
    }

    $matches = Select-String -Path $fullPath -Pattern ([regex]::Escape($forbiddenReference.pattern))
    foreach ($match in $matches) {
        $findings.Add([pscustomobject]@{
            rule_id = $forbiddenReference.rule_id
            severity = "error"
            target = $forbiddenReference.pattern
            file = $forbiddenReference.path
            line = $match.LineNumber
            detail = $forbiddenReference.detail
        })
    }
}

foreach ($requiredDeletedFile in $requiredDeletedFiles) {
    $fullPath = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $requiredDeletedFile.path
    if (Test-Path $fullPath) {
        $findings.Add([pscustomobject]@{
            rule_id = $requiredDeletedFile.rule_id
            severity = "error"
            target = $requiredDeletedFile.path
            file = $requiredDeletedFile.path
            line = 0
            detail = $requiredDeletedFile.detail
        })
    }
}

$workflowServicesRoot = Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/workflow/services"
if (Test-Path $workflowServicesRoot) {
    foreach ($payload in Get-ChildItem -Path $workflowServicesRoot -Recurse -File | Where-Object {
        $_.Extension -in @(".cpp", ".cc", ".cxx", ".py", ".ps1")
    }) {
        $findings.Add([pscustomobject]@{
            rule_id = "workflow-services-still-carry-implementation-files"
            severity = "error"
            target = $payload.Name
            file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $payload.FullName
            line = 0
            detail = "workflow/services may keep headers-only M0 skeletons but must not carry implementation payload"
        })
    }
}

$workflowExamplesRoot = Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/workflow/examples"
if (Test-Path $workflowExamplesRoot) {
    foreach ($payload in Get-ChildItem -Path $workflowExamplesRoot -Recurse -File | Where-Object {
        $_.Extension -in @(".h", ".hpp", ".cpp", ".cc", ".cxx", ".py", ".ps1")
    }) {
        $findings.Add([pscustomobject]@{
            rule_id = "workflow-examples-still-carry-payload"
            severity = "error"
            target = $payload.Name
            file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $payload.FullName
            line = 0
            detail = "workflow/examples must remain shell-only and must not carry source-bearing payload"
        })
    }
}

$exactWordScopedSearchRuleIds = @(
    "live-targets-still-reference-siligen-domain",
    "live-targets-still-reference-siligen-motion-core",
    "live-targets-still-reference-siligen-domain-services",
    "live-targets-still-reference-siligen-triggering",
    "live-targets-still-reference-domain-machine-wrapper",
    "live-targets-still-reference-siligen-trajectory-wrapper",
    "live-targets-still-reference-siligen-configuration-wrapper"
)

foreach ($searchRule in $forbiddenScopedSearches) {
    if ($exactWordScopedSearchRuleIds -contains $searchRule.rule_id) {
        continue
    }

    $matches = Get-FixedStringMatches -Pattern $searchRule.pattern -SearchRoots $searchRule.search_roots
    foreach ($match in $matches) {
        if ($match -notmatch "^(?<path>[^:]+):(?<line>\d+):(?<text>.*)$") {
            continue
        }

        $findings.Add([pscustomobject]@{
            rule_id = $searchRule.rule_id
            severity = "error"
            target = $searchRule.pattern
            file = $Matches.path
            line = [int]$Matches.line
            detail = $searchRule.detail
        })
    }
}

if (-not (Test-Path $recipeCanonicalUseCaseHeaderDir)) {
    $findings.Add([pscustomobject]@{
        rule_id = "missing-canonical-recipe-header-dir"
        severity = "error"
        target = $recipeCanonicalUseCaseHeaderDir
        file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeCanonicalUseCaseHeaderDir
        line = 0
        detail = "canonical recipe-lifecycle application public header directory does not exist"
    })
} else {
    foreach ($header in Get-ChildItem -Path $recipeCanonicalUseCaseHeaderDir -File) {
        $matches = Select-String -Path $header.FullName -Pattern ([regex]::Escape('application/usecases/recipes/'))
        foreach ($match in $matches) {
            $findings.Add([pscustomobject]@{
                rule_id = "canonical-recipe-header-still-wraps-legacy-path"
                severity = "error"
                target = "application/usecases/recipes/"
                file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $header.FullName
                line = $match.LineNumber
                detail = "canonical recipe-lifecycle public headers must own declarations and must not forward back to source-local application/usecases/recipes paths"
            })
        }
    }
}

if (Test-Path $recipeSourceUseCaseHeaderDir) {
    foreach ($header in Get-ChildItem -Path $recipeCanonicalUseCaseHeaderDir -File) {
        $sourceDuplicate = Join-Path $recipeSourceUseCaseHeaderDir $header.Name
        if (Test-Path $sourceDuplicate) {
            $findings.Add([pscustomobject]@{
                rule_id = "recipe-source-dir-duplicates-public-header"
                severity = "error"
                target = $header.Name
                file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $sourceDuplicate
                line = 0
                detail = "recipe-lifecycle source dir must not duplicate canonical public headers"
            })
        }
    }
}

if (-not (Test-Path $recipeCanonicalSerializerHeader)) {
    $findings.Add([pscustomobject]@{
        rule_id = "missing-canonical-recipe-serializer-header"
        severity = "error"
        target = $recipeCanonicalSerializerHeader
        file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeCanonicalSerializerHeader
        line = 0
        detail = "canonical recipe-lifecycle serializer public header does not exist"
    })
} else {
    $matches = Select-String -Path $recipeCanonicalSerializerHeader -Pattern ([regex]::Escape('domain/recipes/serialization/RecipeJsonSerializer.h'))
    foreach ($match in $matches) {
        $findings.Add([pscustomobject]@{
            rule_id = "canonical-recipe-serializer-header-still-wraps-legacy-path"
            severity = "error"
            target = 'domain/recipes/serialization/RecipeJsonSerializer.h'
            file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeCanonicalSerializerHeader
            line = $match.LineNumber
            detail = "canonical recipe-lifecycle serializer public header must own declarations and must not forward back to the workflow serializer path"
        })
    }
}

foreach ($searchRule in $forbiddenScopedSearches | Where-Object { $exactWordScopedSearchRuleIds -contains $_.rule_id }) {
    $matches = Get-ExactWordMatches -Word $searchRule.pattern -SearchRoots $searchRule.search_roots
    foreach ($match in $matches) {
        if ($match -notmatch "^(?<path>[^:]+):(?<line>\d+):(?<text>.*)$") {
            continue
        }

        $findings.Add([pscustomobject]@{
            rule_id = $searchRule.rule_id
            severity = "error"
            target = $searchRule.pattern
            file = $Matches.path
            line = [int]$Matches.line
            detail = $searchRule.detail
        })
    }
}

$summary = [ordered]@{
    generated_at = (Get-Date).ToString("s")
    workspace_root = $repoRoot
    report_dir = $resolvedReportDir
    finding_count = $findings.Count
    status = if ($findings.Count -eq 0) { "passed" } else { "failed" }
    allowed_direct_workflow_references = @(
        "modules/workflow/CMakeLists.txt",
        "modules/workflow/application/CMakeLists.txt",
        "modules/runtime-execution/CMakeLists.txt",
        "modules/runtime-execution/runtime/host/CMakeLists.txt",
        "apps/runtime-service/CMakeLists.txt",
        "modules/workflow/tests/regression/CMakeLists.txt",
        "modules/dxf-geometry/CMakeLists.txt",
        "modules/job-ingest/CMakeLists.txt",
        "modules/dxf-geometry/tests/CMakeLists.txt",
        "modules/job-ingest/tests/CMakeLists.txt",
        "apps/planner-cli/CMakeLists.txt",
        "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
    )
    findings = $findings
}

$jsonPath = Join-Path $resolvedReportDir "module-boundary-bridges.json"
$mdPath = Join-Path $resolvedReportDir "module-boundary-bridges.md"

$summary | ConvertTo-Json -Depth 6 | Set-Content -Path $jsonPath -Encoding UTF8

$mdLines = @(
    "# Module Boundary Bridge Report",
    "",
    "- generated_at: $($summary.generated_at)",
    "- workspace_root: $($summary.workspace_root)",
    "- status: $($summary.status)",
    "- finding_count: $($summary.finding_count)",
    "",
    "| rule_id | severity | target | file | line | detail |",
    "|---|---|---|---|---:|---|"
)

if ($findings.Count -eq 0) {
    $mdLines += "| none | info | - | - | 0 | no direct workflow public target references were found outside the approved bridge definitions |"
} else {
    foreach ($finding in $findings) {
        $mdLines += "| $($finding.rule_id) | $($finding.severity) | $($finding.target) | $($finding.file) | $($finding.line) | $($finding.detail) |"
    }
}

$mdLines | Set-Content -Path $mdPath -Encoding UTF8

Write-Host "Report(JSON): $jsonPath"
Write-Host "Report(MD):   $mdPath"
Write-Host "Status:       $($summary.status)"

if ($findings.Count -gt 0) {
    exit 1
}

exit 0
