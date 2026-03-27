cmake_minimum_required(VERSION 3.20)

set(RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/adapters/device")
set(RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR "${SILIGEN_WORKSPACE_ROOT}/shared/contracts/device")
set(RUNTIME_HOST_TRACEABILITY_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/trace-diagnostics")
set(RUNTIME_HOST_DEVICE_ADAPTERS_INCLUDE_DIR "${RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR}/include")
set(RUNTIME_HOST_DEVICE_CONTRACTS_INCLUDE_DIR "${RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR}/include")
set(RUNTIME_HOST_TRACEABILITY_INCLUDE_DIR "${RUNTIME_HOST_TRACEABILITY_DIR}/adapters")
set(RUNTIME_HOST_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")
set(RUNTIME_HOST_MULTICARD_VENDOR_DIR "${SILIGEN_WORKSPACE_ROOT}/modules/runtime-execution/adapters/device/vendor/multicard")
if(DEFINED SILIGEN_MULTICARD_VENDOR_DIR AND EXISTS "${SILIGEN_MULTICARD_VENDOR_DIR}")
    set(RUNTIME_HOST_MULTICARD_VENDOR_DIR "${SILIGEN_MULTICARD_VENDOR_DIR}")
endif()

if(NOT EXISTS "${RUNTIME_HOST_SOURCE_ROOT}/ContainerBootstrap.cpp")
    message(FATAL_ERROR
        "runtime-execution canonical runtime host requires runtime/host as the build source root."
    )
endif()

if(NOT EXISTS "${RUNTIME_HOST_DEVICE_ADAPTERS_INCLUDE_DIR}/siligen/device/adapters/drivers/multicard/IMultiCardWrapper.h")
    message(FATAL_ERROR
        "runtime-host requires canonical adapters/device public headers."
    )
endif()

if(NOT EXISTS "${RUNTIME_HOST_DEVICE_CONTRACTS_INCLUDE_DIR}/siligen/device/contracts/device_contracts.h")
    message(FATAL_ERROR
        "runtime-host requires canonical contracts/device public headers."
    )
endif()

if(NOT EXISTS "${RUNTIME_HOST_TRACEABILITY_INCLUDE_DIR}/logging/spdlog/SpdlogLoggingAdapter.h")
    message(FATAL_ERROR
        "runtime-host requires modules/trace-diagnostics/adapters as the diagnostics include root."
    )
endif()

if(NOT TARGET siligen_device_contracts AND EXISTS "${RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_DEVICE_CONTRACTS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/shared/contracts/device"
    )
endif()

if(NOT TARGET siligen_device_adapters AND EXISTS "${RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_DEVICE_ADAPTERS_CANONICAL_DIR}"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/adapters/device"
    )
endif()

if(NOT TARGET siligen_spdlog_adapter AND EXISTS "${RUNTIME_HOST_TRACEABILITY_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_TRACEABILITY_DIR}"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/runtime/host/trace-diagnostics"
    )
endif()

add_library(siligen_runtime_host_boost_headers INTERFACE)
target_include_directories(siligen_runtime_host_boost_headers INTERFACE
    ${SILIGEN_BOOST_INCLUDE_DIR}
)
target_compile_features(siligen_runtime_host_boost_headers INTERFACE cxx_std_17)

if(NOT TARGET siligen_event_adapters AND EXISTS "${RUNTIME_HOST_SOURCE_ROOT}/runtime/events/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_SOURCE_ROOT}/runtime/events"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/runtime/host/runtime-events"
    )
endif()

if(NOT TARGET siligen_task_scheduler AND EXISTS "${RUNTIME_HOST_SOURCE_ROOT}/runtime/scheduling/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_SOURCE_ROOT}/runtime/scheduling"
        "${CMAKE_BINARY_DIR}/modules/runtime-execution/runtime/host/runtime-scheduling"
    )
endif()

set(RUNTIME_HOST_COMMON_INCLUDE_DIRS
    ${RUNTIME_HOST_SOURCE_ROOT}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    ${RUNTIME_HOST_DEVICE_ADAPTERS_INCLUDE_DIR}
    ${RUNTIME_HOST_DEVICE_CONTRACTS_INCLUDE_DIR}
    ${RUNTIME_HOST_TRACEABILITY_INCLUDE_DIR}
)
list(REMOVE_DUPLICATES RUNTIME_HOST_COMMON_INCLUDE_DIRS)

add_library(siligen_device_hal_hardware INTERFACE)
add_library(siligen_hardware ALIAS siligen_device_hal_hardware)
target_include_directories(siligen_device_hal_hardware INTERFACE
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_device_hal_hardware INTERFACE
    siligen_device_adapters
    siligen_device_contracts
    siligen_domain
    siligen_types
    siligen_utils
)
target_compile_features(siligen_device_hal_hardware INTERFACE cxx_std_17)

add_library(siligen_runtime_recipe_persistence STATIC
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/recipes/RecipeFileRepository.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/recipes/TemplateFileRepository.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/recipes/AuditFileRepository.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/recipes/ParameterSchemaFileProvider.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/recipes/RecipeBundleSerializer.cpp
)
target_include_directories(siligen_runtime_recipe_persistence PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_recipe_persistence PUBLIC
    siligen_workflow_recipe_domain_public
    siligen_workflow_recipe_serialization_public
)
target_link_libraries(siligen_runtime_recipe_persistence PRIVATE
    siligen_types
    siligen_utils
)
target_compile_features(siligen_runtime_recipe_persistence PRIVATE cxx_std_17)

add_library(siligen_runtime_host_config STATIC
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/configuration/ConfigFileAdapter.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/configuration/ConfigFileAdapter.Ini.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/configuration/ConfigFileAdapter.Sections.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/configuration/ConfigFileAdapter.System.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/configuration/InterlockConfigResolver.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/configuration/validators/ConfigValidator.cpp
)
add_library(siligen_configs ALIAS siligen_runtime_host_config)
target_include_directories(siligen_runtime_host_config PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_host_config PUBLIC
    siligen_process_planning_domain_configuration
)
target_link_libraries(siligen_runtime_host_config PRIVATE
    siligen_runtime_host_boost_headers
    siligen_types
    siligen_utils
)
target_compile_features(siligen_runtime_host_config PRIVATE cxx_std_17)

add_library(siligen_runtime_host_storage STATIC
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/storage/files/LocalFileStorageAdapter.cpp
)
add_library(siligen_services ALIAS siligen_runtime_host_storage)
target_include_directories(siligen_runtime_host_storage PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_host_storage PUBLIC
    siligen_process_planning_domain_configuration
)
target_link_libraries(siligen_runtime_host_storage PRIVATE
    siligen_types
    siligen_utils
)
target_compile_features(siligen_runtime_host_storage PRIVATE cxx_std_17)

add_library(siligen_runtime_host_factories STATIC
    ${RUNTIME_HOST_SOURCE_ROOT}/factories/InfrastructureAdapterFactory.cpp
)
add_library(siligen_factories ALIAS siligen_runtime_host_factories)
target_include_directories(siligen_runtime_host_factories PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_host_factories PUBLIC
    siligen_process_planning_domain_configuration
)
target_link_libraries(siligen_runtime_host_factories PRIVATE
    siligen_device_hal_hardware
    siligen_runtime_recipe_persistence
    siligen_runtime_host_config
    siligen_runtime_host_storage
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_runtime_host_factories PUBLIC
    siligen_spdlog_adapter
)
target_compile_definitions(siligen_runtime_host_factories PRIVATE
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_features(siligen_runtime_host_factories PRIVATE cxx_std_17)

add_library(siligen_runtime_host_infrastructure INTERFACE)
add_library(siligen_infrastructure ALIAS siligen_runtime_host_infrastructure)
target_include_directories(siligen_runtime_host_infrastructure INTERFACE
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_runtime_host_infrastructure INTERFACE
    siligen_device_hal_hardware
    siligen_runtime_recipe_persistence
    siligen_runtime_host_config
    siligen_runtime_host_storage
    siligen_spdlog_adapter
    siligen_runtime_host_factories
    siligen_event_adapters
    siligen_task_scheduler
)

if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_runtime_recipe_persistence PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_runtime_host_config PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_runtime_host_storage PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_runtime_host_factories PRIVATE ${SILIGEN_PCH_HEADER})
endif()

add_library(siligen_runtime_host STATIC
    ${RUNTIME_HOST_SOURCE_ROOT}/ContainerBootstrap.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/bootstrap/InfrastructureBindingsBuilder.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/container/ApplicationContainer.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/container/ApplicationContainer.System.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/container/ApplicationContainer.Motion.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/container/ApplicationContainer.Dispensing.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/container/ApplicationContainer.Recipes.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/runtime/diagnostics/DiagnosticsPortAdapter.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/services/motion/HardLimitMonitorService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/services/motion/SoftLimitMonitorService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/AuditLogger.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/InterlockMonitor.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/NetworkAccessControl.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/NetworkWhitelist.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/PasswordHasher.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/PermissionService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/SafetyLimitsValidator.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/SecurityService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/SessionService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/UserService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/config/ConfigurationVersionService.cpp
    ${RUNTIME_HOST_SOURCE_ROOT}/security/config/SecurityConfigLoader.cpp
)

set_target_properties(siligen_runtime_host PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(siligen_runtime_host PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)

if(EXISTS "${RUNTIME_HOST_MULTICARD_VENDOR_DIR}/MultiCard.lib")
    set(RUNTIME_HOST_HAS_REAL_MULTICARD 1)
else()
    set(RUNTIME_HOST_HAS_REAL_MULTICARD 0)
endif()

target_compile_definitions(siligen_runtime_host PRIVATE
    HAS_REAL_MULTICARD=${RUNTIME_HOST_HAS_REAL_MULTICARD}
    SILIGEN_ENABLE_MOCK_MULTICARD=$<BOOL:${SILIGEN_ENABLE_MOCK_MULTICARD}>
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)

target_link_libraries(siligen_runtime_host PUBLIC
    siligen_job_ingest_application_public
    siligen_workflow_application_public
    siligen_runtime_execution_application_public
    siligen_workflow_recipe_application_public
    siligen_runtime_host_infrastructure
    siligen_shared
)
target_link_libraries(siligen_runtime_host PRIVATE
    siligen_runtime_host_boost_headers
)
set_target_properties(siligen_runtime_host PROPERTIES
    SILIGEN_MIGRATION_SOURCE_ROOT "legacy-archive/runtime-host"
    SILIGEN_TARGET_TOPOLOGY_OWNER_ROOT "modules/runtime-execution"
)
