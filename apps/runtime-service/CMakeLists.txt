cmake_minimum_required(VERSION 3.20)

set(RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(RUNTIME_PROCESS_BOOTSTRAP_PUBLIC_INCLUDE_DIR "${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/include")
set(RUNTIME_PROCESS_BOOTSTRAP_HOST_CORE_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/runtime/host")
set(RUNTIME_PROCESS_BOOTSTRAP_DEVICE_VENDOR_DIR
    "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/adapters/device/vendor/multicard")
if(DEFINED SILIGEN_MULTICARD_VENDOR_DIR AND EXISTS "${SILIGEN_MULTICARD_VENDOR_DIR}")
    set(RUNTIME_PROCESS_BOOTSTRAP_DEVICE_VENDOR_DIR "${SILIGEN_MULTICARD_VENDOR_DIR}")
endif()

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/modules/process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/rtsvc_ppc"
    )
endif()

if(NOT TARGET siligen_trace_diagnostics_application_public)
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/modules/trace-diagnostics/application"
        "${CMAKE_BINARY_DIR}/_p/rtsvc_trace"
    )
endif()

if(NOT TARGET siligen_parsing_adapter)
    add_subdirectory(
        "${SILIGEN_WORKSPACE_ROOT}/modules/workflow/adapters/infrastructure/adapters/planning/dxf"
        "${CMAKE_BINARY_DIR}/apps/runtime-service/workflow-dxf-adapter"
    )
endif()

if(NOT TARGET siligen_runtime_process_bootstrap_public)
    add_library(siligen_runtime_process_bootstrap_public INTERFACE)
endif()

if(EXISTS "${RUNTIME_PROCESS_BOOTSTRAP_DEVICE_VENDOR_DIR}/MultiCard.lib")
    set(RUNTIME_PROCESS_BOOTSTRAP_HAS_REAL_MULTICARD 1)
else()
    set(RUNTIME_PROCESS_BOOTSTRAP_HAS_REAL_MULTICARD 0)
endif()

add_library(siligen_runtime_process_bootstrap STATIC
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/bootstrap/ContainerBootstrap.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/bootstrap/InfrastructureBindingsBuilder.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/container/ApplicationContainer.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/container/ApplicationContainer.System.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/container/ApplicationContainer.Motion.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/container/ApplicationContainer.Dispensing.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/container/ApplicationContainer.Recipes.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/factories/InfrastructureAdapterFactory.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/configuration/ConfigFileAdapter.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/configuration/ConfigFileAdapter.Ini.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/configuration/ConfigFileAdapter.Sections.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/configuration/ConfigFileAdapter.System.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/configuration/InterlockConfigResolver.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/dispensing/WorkflowDispensingProcessPortAdapter.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/configuration/validators/ConfigValidator.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/recipes/AuditFileRepository.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/recipes/ParameterSchemaFileProvider.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/recipes/RecipeBundleSerializer.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/recipes/RecipeFileRepository.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/recipes/TemplateFileRepository.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/status/WorkflowRuntimeStatusExportPort.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/supervision/WorkflowRuntimeSupervisionBackend.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/runtime/storage/files/LocalFileStorageAdapter.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/AuditLogger.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/InterlockMonitor.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/NetworkAccessControl.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/NetworkWhitelist.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/PasswordHasher.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/PermissionService.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/SafetyLimitsValidator.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/SecurityService.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/SessionService.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/UserService.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/config/ConfigurationVersionService.cpp
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}/security/config/SecurityConfigLoader.cpp
)

target_include_directories(siligen_runtime_process_bootstrap PRIVATE
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_compile_definitions(siligen_runtime_process_bootstrap PRIVATE
    HAS_REAL_MULTICARD=${RUNTIME_PROCESS_BOOTSTRAP_HAS_REAL_MULTICARD}
    SILIGEN_ENABLE_MOCK_MULTICARD=$<BOOL:${SILIGEN_ENABLE_MOCK_MULTICARD}>
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)

target_link_libraries(siligen_runtime_process_bootstrap PUBLIC
    siligen_runtime_host
    siligen_event_adapters
    siligen_task_scheduler
    siligen_device_adapters
    siligen_device_contracts
    siligen_job_ingest_application_public
    siligen_workflow_application_public
    siligen_workflow_adapters_public
    siligen_workflow_recipe_domain_public
    siligen_workflow_recipe_application_public
    siligen_workflow_recipe_serialization_public
    siligen_runtime_execution_application_public
    siligen_trace_diagnostics_application_public
    siligen_parsing_adapter
    siligen_process_planning_contracts_public
    siligen_shared
    siligen_types
    siligen_utils
)
if(TARGET siligen_process_path_application_public)
    target_link_libraries(siligen_runtime_process_bootstrap PUBLIC
        siligen_process_path_application_public
    )
endif()
if(TARGET siligen_motion_planning_application_public)
    target_link_libraries(siligen_runtime_process_bootstrap PUBLIC
        siligen_motion_planning_application_public
    )
endif()
if(TARGET siligen_dispense_packaging_application_public)
    target_link_libraries(siligen_runtime_process_bootstrap PUBLIC
        siligen_dispense_packaging_application_public
    )
endif()
target_link_libraries(siligen_runtime_process_bootstrap PRIVATE
    siligen_runtime_host_boost_headers
)
target_compile_features(siligen_runtime_process_bootstrap PRIVATE cxx_std_17)

target_include_directories(siligen_runtime_process_bootstrap_public INTERFACE
    ${RUNTIME_PROCESS_BOOTSTRAP_PUBLIC_INCLUDE_DIR}
    ${RUNTIME_PROCESS_BOOTSTRAP_APP_ROOT}
)
target_link_libraries(siligen_runtime_process_bootstrap_public INTERFACE
    siligen_runtime_process_bootstrap
)
target_compile_features(siligen_runtime_process_bootstrap_public INTERFACE cxx_std_17)

if(NOT TARGET siligen_app_runtime_service)
    add_library(siligen_app_runtime_service INTERFACE)
endif()

target_link_libraries(siligen_app_runtime_service INTERFACE
    siligen_runtime_process_bootstrap_public
)
target_compile_features(siligen_app_runtime_service INTERFACE cxx_std_17)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/main.cpp")
    add_executable(siligen_runtime_service
        ${CMAKE_CURRENT_SOURCE_DIR}/main.cpp
    )
    set_target_properties(siligen_runtime_service PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        OUTPUT_NAME "siligen_runtime_service"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
    target_link_libraries(siligen_runtime_service PRIVATE siligen_app_runtime_service)
endif()

if(
    SILIGEN_BUILD_TESTS AND
    (TARGET GTest::gtest_main OR TARGET gtest_main) AND
    EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/CMakeLists.txt"
)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/tests"
        "${CMAKE_BINARY_DIR}/apps/runtime-service/tests"
    )
endif()
