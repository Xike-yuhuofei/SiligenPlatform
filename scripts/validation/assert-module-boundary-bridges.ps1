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

$repoRoot = Resolve-AbsolutePath -BasePath (Get-Location).Path -PathValue $WorkspaceRoot
$resolvedReportDir = Resolve-AbsolutePath -BasePath $repoRoot -PathValue $ReportDir
New-Item -ItemType Directory -Force -Path $resolvedReportDir | Out-Null

Set-Location $repoRoot

$allowedDirectWorkflowReferences = @(
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/workflow/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/runtime-execution/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/runtime-execution/runtime/host/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/dxf-geometry/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/job-ingest/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/dxf-geometry/tests/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "modules/job-ingest/tests/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "apps/planner-cli/CMakeLists.txt"),
    (Resolve-AbsolutePath -BasePath $repoRoot -PathValue "apps/runtime-gateway/transport-gateway/CMakeLists.txt")
)

$requiredBridgeReferences = @(
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "siligen_job_ingest_application_public"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "siligen_dxf_geometry_application_public"
    },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "../../runtime-execution/application/include"
        },
    @{
        path = "modules/workflow/application/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_domain_public"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_serialization_public"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_application_public"
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
        path = "modules/runtime-execution/adapters/device/CMakeLists.txt"
        pattern = "siligen_domain"
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
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_application_public"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
    },
    @{
        path = "apps/planner-cli/CMakeLists.txt"
        pattern = "siligen_workflow_adapters_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_runtime_execution_application_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_application_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_serialization_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/CMakeLists.txt"
        pattern = "siligen_module_job_ingest"
    },
    @{
        path = "modules/runtime-execution/runtime/host/CMakeLists.txt"
        pattern = "siligen_workflow_application_public"
    },
    @{
        path = "modules/runtime-execution/runtime/host/tests/CMakeLists.txt"
        pattern = "siligen_workflow_recipe_serialization_public"
    }
)

$directWorkflowTargets = @(
    "siligen_workflow_domain_public",
    "siligen_workflow_application_public",
    "siligen_workflow_adapters_public",
    "siligen_workflow_runtime_consumer_public"
)

$recipeCanonicalUseCaseHeaderDir = Resolve-AbsolutePath -BasePath $repoRoot -PathValue `
    "modules/workflow/application/include/workflow/application/usecases/recipes"
$recipeLegacyUseCaseHeaderDir = Resolve-AbsolutePath -BasePath $repoRoot -PathValue `
    "modules/workflow/application/include/application/usecases/recipes"
$recipeCanonicalSerializerHeader = Resolve-AbsolutePath -BasePath $repoRoot -PathValue `
    "modules/workflow/adapters/include/workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
$recipeLegacySerializerHeader = Resolve-AbsolutePath -BasePath $repoRoot -PathValue `
    "modules/workflow/adapters/include/recipes/serialization/RecipeJsonSerializer.h"

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
        detail = "runtime-host must consume siligen_domain or dedicated owner targets instead of the residual domain compat bridge"
    },
    @{
        path = "modules/runtime-execution/adapters/device/CMakeLists.txt"
        pattern = "siligen_runtime_execution_workflow_domain_compat"
        rule_id = "runtime-device-adapters-still-use-domain-compat"
        detail = "device-adapters must consume siligen_domain or dedicated owner targets instead of the residual domain compat bridge"
    },
    @{
        path = "modules/runtime-execution/runtime/host/tests/CMakeLists.txt"
        pattern = "siligen_recipe_json_codec"
        rule_id = "runtime-host-tests-still-link-recipe-impl-target"
        detail = "runtime-host tests must consume recipe serialization through siligen_workflow_recipe_serialization_public"
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
        path = "modules/runtime-execution/runtime/host/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "application/usecases/dispensing/DispensingExecutionUseCase.h"'
        rule_id = "runtime-host-still-includes-workflow-execution-header"
        detail = "runtime host execution container must include runtime_execution/application/* headers"
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
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpDispensingFacade.h"
        pattern = '#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"'
        rule_id = "gateway-still-uses-runtime-workflow-wrapper"
        detail = "transport-gateway workflow consumer must include workflow/application/* headers"
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
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = '#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"'
        rule_id = "gateway-builder-still-uses-runtime-workflow-wrapper"
        detail = "transport-gateway builder must include workflow/application/* headers"
    },
    @{
        path = "modules/runtime-execution/runtime/host/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "runtime_execution/application/usecases/dispensing/PlanningUseCase.h"'
        rule_id = "runtime-host-still-uses-runtime-planning-wrapper"
        detail = "runtime host planning consumer must include workflow/application/* headers"
    },
    @{
        path = "modules/runtime-execution/runtime/host/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "runtime_execution/application/usecases/dispensing/UploadFileUseCase.h"'
        rule_id = "runtime-host-still-uses-runtime-upload-wrapper"
        detail = "runtime host upload consumer must include job_ingest/application/* headers"
    },
    @{
        path = "modules/runtime-execution/runtime/host/container/ApplicationContainer.Dispensing.cpp"
        pattern = '#include "runtime_execution/application/usecases/dispensing/DispensingWorkflowUseCase.h"'
        rule_id = "runtime-host-still-uses-runtime-workflow-wrapper"
        detail = "runtime host workflow consumer must include workflow/application/* headers"
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

$requiredOwnerTargets = @(
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
        path = "modules/workflow/application/include/application/services/dxf/DxfPbPreparationService.h"
        pattern = "../../../../../../dxf-geometry/application/include/application/services/dxf/DxfPbPreparationService.h"
        rule_id = "workflow-dxf-wrapper-still-uses-relative-owner-path"
        detail = "workflow DXF service wrapper must include dxf_geometry/application/services/dxf/DxfPbPreparationService.h instead of a relative owner path"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
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
        path = "modules/workflow/domain/domain/CMakeLists.txt"
        pattern = "add_subdirectory(dispensing)"
        rule_id = "workflow-still-builds-dispensing-domain-duplicate"
        detail = "workflow bridge domain must not compile dispense-packaging planning implementations"
    },
    @{
        path = "modules/workflow/domain/domain/CMakeLists.txt"
        pattern = "add_subdirectory(trajectory)"
        rule_id = "workflow-still-builds-trajectory-domain-duplicate"
        detail = "workflow bridge domain must not compile process-path planning implementations"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
        pattern = "unit/domain/dispensing/DispensingControllerTest.cpp"
        rule_id = "workflow-still-owns-dispense-packaging-domain-tests"
        detail = "workflow tests must not compile dispense-packaging owner domain tests"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
        pattern = "unit/domain/dispensing/TriggerPlannerTest.cpp"
        rule_id = "workflow-still-owns-dispense-packaging-domain-tests"
        detail = "workflow tests must not compile dispense-packaging owner domain tests"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
        pattern = "unit/domain/motion/CMPCoordinatedInterpolatorPrecisionTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
        pattern = "unit/domain/trajectory/MotionPlannerTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
        pattern = "unit/domain/trajectory/MotionPlannerConstraintTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
        pattern = "unit/domain/trajectory/InterpolationProgramPlannerTest.cpp"
        rule_id = "workflow-still-owns-motion-planning-domain-tests"
        detail = "workflow tests must not compile motion-planning owner domain tests"
    },
    @{
        path = "modules/workflow/tests/process-runtime-core/CMakeLists.txt"
        pattern = "unit/domain/trajectory/PathRegularizerTest.cpp"
        rule_id = "workflow-still-owns-process-path-domain-tests"
        detail = "workflow tests must not compile process-path owner domain tests"
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
        detail = "runtime execution public header must include runtime_execution/contracts/system/IEventPublisherPort.h"
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
        detail = "runtime event adapter must include runtime_execution/contracts/system/IEventPublisherPort.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/scheduling/TaskSchedulerAdapter.h"
        pattern = '#include "domain/dispensing/ports/ITaskSchedulerPort.h"'
        rule_id = "runtime-scheduler-adapter-still-includes-workflow-task-scheduler"
        detail = "runtime scheduling adapter must include runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/ContainerBootstrap.cpp"
        pattern = '#include "domain/system/ports/IEventPublisherPort.h"'
        rule_id = "runtime-bootstrap-still-includes-workflow-event-port"
        detail = "runtime bootstrap must include runtime_execution/contracts/system/IEventPublisherPort.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/ContainerBootstrap.cpp"
        pattern = '#include "domain/dispensing/ports/ITaskSchedulerPort.h"'
        rule_id = "runtime-bootstrap-still-includes-workflow-task-scheduler"
        detail = "runtime bootstrap must include runtime_execution/contracts/dispensing/ITaskSchedulerPort.h"
    },
    @{
        path = "apps/planner-cli/CommandHandlers.Recipe.cpp"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "planner-cli-still-includes-legacy-recipe-usecases"
        detail = "planner-cli recipe consumer must include workflow/application/usecases/recipes/* wrappers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/include/siligen/gateway/tcp/tcp_facade_builder.h"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "gateway-builder-still-includes-legacy-recipe-usecases"
        detail = "transport-gateway builder must include workflow/application/usecases/recipes/* wrappers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/facades/tcp/TcpRecipeFacade.h"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "gateway-facade-still-includes-legacy-recipe-usecases"
        detail = "transport-gateway recipe facade must include workflow/application/usecases/recipes/* wrappers"
    },
    @{
        path = "modules/runtime-execution/runtime/host/container/ApplicationContainer.Recipes.cpp"
        pattern = '#include "application/usecases/recipes/'
        rule_id = "runtime-host-still-includes-legacy-recipe-usecases"
        detail = "runtime-host recipe container must include workflow/application/usecases/recipes/* wrappers"
    },
    @{
        path = "apps/runtime-gateway/transport-gateway/src/tcp/TcpCommandDispatcher.cpp"
        pattern = '#include "recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "gateway-dispatcher-still-includes-legacy-recipe-serializer"
        detail = "transport-gateway dispatcher must include workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/recipes/RecipeBundleSerializer.h"
        pattern = '#include "recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-recipe-serializer-still-includes-legacy-json-header"
        detail = "runtime recipe persistence must include workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/recipes/RecipeFileRepository.cpp"
        pattern = '#include "recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-recipe-repository-still-includes-legacy-json-header"
        detail = "runtime recipe persistence must include workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/recipes/ParameterSchemaFileProvider.cpp"
        pattern = '#include "recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-parameter-schema-still-includes-legacy-json-header"
        detail = "runtime recipe persistence must include workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/recipes/TemplateFileRepository.cpp"
        pattern = '#include "recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-template-repository-still-includes-legacy-json-header"
        detail = "runtime recipe persistence must include workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
    },
    @{
        path = "modules/runtime-execution/runtime/host/runtime/recipes/AuditFileRepository.cpp"
        pattern = '#include "recipes/serialization/RecipeJsonSerializer.h"'
        rule_id = "runtime-audit-repository-still-includes-legacy-json-header"
        detail = "runtime recipe persistence must include workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
    }
)

$findings = New-Object System.Collections.Generic.List[object]

foreach ($target in $directWorkflowTargets) {
    $matches = & rg -n --fixed-strings $target modules apps
    if ($LASTEXITCODE -gt 1) {
        throw "rg failed while checking workflow target references: $target"
    }

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
            detail = "expected runtime-execution compatibility bridge reference is missing"
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

if (-not (Test-Path $recipeCanonicalUseCaseHeaderDir)) {
    $findings.Add([pscustomobject]@{
        rule_id = "missing-canonical-recipe-header-dir"
        severity = "error"
        target = $recipeCanonicalUseCaseHeaderDir
        file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeCanonicalUseCaseHeaderDir
        line = 0
        detail = "canonical workflow recipe application public header directory does not exist"
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
                detail = "canonical workflow recipe public headers must own declarations and must not forward back to legacy application/usecases/recipes paths"
            })
        }
    }
}

if (-not (Test-Path $recipeLegacyUseCaseHeaderDir)) {
    $findings.Add([pscustomobject]@{
        rule_id = "missing-legacy-recipe-header-dir"
        severity = "error"
        target = $recipeLegacyUseCaseHeaderDir
        file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeLegacyUseCaseHeaderDir
        line = 0
        detail = "legacy workflow recipe application public header directory does not exist"
    })
} else {
    foreach ($header in Get-ChildItem -Path $recipeLegacyUseCaseHeaderDir -File) {
        $expectedInclude = "#include `"workflow/application/usecases/recipes/$($header.Name)`""
        if (-not (Select-String -Path $header.FullName -Pattern ([regex]::Escape($expectedInclude)) -Quiet)) {
            $findings.Add([pscustomobject]@{
                rule_id = "legacy-recipe-header-must-forward-to-canonical"
                severity = "error"
                target = $expectedInclude
                file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $header.FullName
                line = 0
                detail = "legacy workflow recipe public headers must forward to workflow/application/usecases/recipes/* canonical headers"
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
        detail = "canonical workflow recipe serializer public header does not exist"
    })
} else {
    $matches = Select-String -Path $recipeCanonicalSerializerHeader -Pattern ([regex]::Escape('recipes/serialization/RecipeJsonSerializer.h'))
    foreach ($match in $matches) {
        $findings.Add([pscustomobject]@{
            rule_id = "canonical-recipe-serializer-header-still-wraps-legacy-path"
            severity = "error"
            target = 'recipes/serialization/RecipeJsonSerializer.h'
            file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeCanonicalSerializerHeader
            line = $match.LineNumber
            detail = "canonical workflow recipe serializer public header must own declarations and must not forward back to the legacy recipes/serialization path"
        })
    }
}

if (-not (Test-Path $recipeLegacySerializerHeader)) {
    $findings.Add([pscustomobject]@{
        rule_id = "missing-legacy-recipe-serializer-header"
        severity = "error"
        target = $recipeLegacySerializerHeader
        file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeLegacySerializerHeader
        line = 0
        detail = "legacy workflow recipe serializer public header does not exist"
    })
} else {
    $expectedSerializerInclude = '#include "workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"'
    if (-not (Select-String -Path $recipeLegacySerializerHeader -Pattern ([regex]::Escape($expectedSerializerInclude)) -Quiet)) {
        $findings.Add([pscustomobject]@{
            rule_id = "legacy-recipe-serializer-header-must-forward-to-canonical"
            severity = "error"
            target = $expectedSerializerInclude
            file = Get-RelativeRepoPath -BasePath $repoRoot -TargetPath $recipeLegacySerializerHeader
            line = 0
            detail = "legacy workflow recipe serializer public header must forward to workflow/adapters/recipes/serialization/RecipeJsonSerializer.h"
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
        "modules/runtime-execution/CMakeLists.txt"
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
