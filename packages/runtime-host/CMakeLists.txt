cmake_minimum_required(VERSION 3.20)

set(RUNTIME_HOST_DEVICE_ADAPTERS_DIR "${SILIGEN_WORKSPACE_ROOT}/packages/device-adapters")
set(RUNTIME_HOST_DEVICE_CONTRACTS_DIR "${SILIGEN_WORKSPACE_ROOT}/packages/device-contracts")
set(RUNTIME_HOST_TRACEABILITY_DIR "${SILIGEN_WORKSPACE_ROOT}/packages/traceability-observability")

if(NOT TARGET siligen_device_contracts AND EXISTS "${RUNTIME_HOST_DEVICE_CONTRACTS_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_DEVICE_CONTRACTS_DIR}"
        "${CMAKE_BINARY_DIR}/packages/device-contracts"
    )
endif()

if(NOT TARGET siligen_device_adapters AND EXISTS "${RUNTIME_HOST_DEVICE_ADAPTERS_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_DEVICE_ADAPTERS_DIR}"
        "${CMAKE_BINARY_DIR}/packages/device-adapters"
    )
endif()

if(NOT TARGET siligen_spdlog_adapter AND EXISTS "${RUNTIME_HOST_TRACEABILITY_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${RUNTIME_HOST_TRACEABILITY_DIR}"
        "${CMAKE_BINARY_DIR}/packages/traceability-observability"
    )
endif()

if(NOT TARGET siligen_event_adapters AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/events/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/events"
        "${CMAKE_BINARY_DIR}/packages/runtime-host/runtime-events"
    )
endif()

if(NOT TARGET siligen_task_scheduler AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/scheduling/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/scheduling"
        "${CMAKE_BINARY_DIR}/packages/runtime-host/runtime-scheduling"
    )
endif()

set(RUNTIME_HOST_COMMON_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
    ${SILIGEN_THIRD_PARTY_DIR}
    ${RUNTIME_HOST_DEVICE_ADAPTERS_DIR}/src
    ${RUNTIME_HOST_DEVICE_ADAPTERS_DIR}/include
    ${RUNTIME_HOST_DEVICE_CONTRACTS_DIR}/include
    ${RUNTIME_HOST_TRACEABILITY_DIR}/src
)
list(REMOVE_DUPLICATES RUNTIME_HOST_COMMON_INCLUDE_DIRS)

add_library(siligen_device_hal_hardware INTERFACE)
add_library(siligen_hardware ALIAS siligen_device_hal_hardware)
target_include_directories(siligen_device_hal_hardware INTERFACE
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_device_hal_hardware INTERFACE
    siligen_device_adapters
    $<$<TARGET_EXISTS:siligen_device_adapters_legacy_bridges>:siligen_device_adapters_legacy_bridges>
    siligen_device_contracts
    siligen_types
    siligen_utils
    domain_dispensing
)
target_compile_features(siligen_device_hal_hardware INTERFACE cxx_std_17)

add_library(siligen_device_hal_persistence STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/recipes/RecipeFileRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/recipes/TemplateFileRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/recipes/AuditFileRepository.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/recipes/ParameterSchemaFileProvider.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/recipes/RecipeBundleSerializer.cpp
)
add_library(siligen_persistence ALIAS siligen_device_hal_persistence)
target_include_directories(siligen_device_hal_persistence PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_device_hal_persistence PRIVATE
    siligen_types
    siligen_utils
    siligen_recipe_json_codec
)
target_compile_features(siligen_device_hal_persistence PRIVATE cxx_std_17)

add_library(siligen_control_runtime_config STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/configuration/ConfigFileAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/configuration/ConfigFileAdapter.Ini.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/configuration/ConfigFileAdapter.Sections.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/configuration/ConfigFileAdapter.System.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/configuration/validators/ConfigValidator.cpp
)
add_library(siligen_configs ALIAS siligen_control_runtime_config)
target_include_directories(siligen_control_runtime_config PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_control_runtime_config PRIVATE
    siligen_types
    siligen_utils
)
target_compile_features(siligen_control_runtime_config PRIVATE cxx_std_17)

add_library(siligen_control_runtime_storage STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/storage/files/LocalFileStorageAdapter.cpp
)
add_library(siligen_services ALIAS siligen_control_runtime_storage)
target_include_directories(siligen_control_runtime_storage PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_control_runtime_storage PRIVATE
    siligen_types
    siligen_utils
    siligen_parsing_adapter
)
target_compile_features(siligen_control_runtime_storage PRIVATE cxx_std_17)

add_library(siligen_control_runtime_factories STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/factories/InfrastructureAdapterFactory.cpp
)
add_library(siligen_factories ALIAS siligen_control_runtime_factories)
target_include_directories(siligen_control_runtime_factories PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_control_runtime_factories PRIVATE
    siligen_device_hal_hardware
    siligen_device_hal_persistence
    siligen_control_runtime_config
    siligen_control_runtime_storage
    siligen_domain
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_control_runtime_factories PUBLIC
    siligen_spdlog_adapter
)
target_compile_definitions(siligen_control_runtime_factories PRIVATE
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)
target_compile_features(siligen_control_runtime_factories PRIVATE cxx_std_17)

add_library(siligen_control_runtime_infrastructure INTERFACE)
add_library(siligen_infrastructure ALIAS siligen_control_runtime_infrastructure)
target_include_directories(siligen_control_runtime_infrastructure INTERFACE
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)
target_link_libraries(siligen_control_runtime_infrastructure INTERFACE
    siligen_device_hal_hardware
    siligen_device_hal_persistence
    siligen_control_runtime_config
    siligen_control_runtime_storage
    siligen_spdlog_adapter
    siligen_control_runtime_factories
    siligen_parsing_adapter
    siligen_event_adapters
    siligen_task_scheduler
)

if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_device_hal_persistence PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_control_runtime_config PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_control_runtime_storage PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_control_runtime_factories PRIVATE ${SILIGEN_PCH_HEADER})
endif()

add_library(siligen_runtime_host STATIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ContainerBootstrap.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/bootstrap/InfrastructureBindingsBuilder.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ApplicationContainer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ApplicationContainer.System.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ApplicationContainer.Motion.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ApplicationContainer.Dispensing.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/container/ApplicationContainer.Recipes.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/runtime/diagnostics/DiagnosticsPortAdapter.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/motion/HardLimitMonitorService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/services/motion/SoftLimitMonitorService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/AuditLogger.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/InterlockMonitor.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/NetworkAccessControl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/NetworkWhitelist.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/PasswordHasher.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/PermissionService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/SafetyLimitsValidator.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/SecurityService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/SessionService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/UserService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/config/ConfigurationVersionService.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/security/config/SecurityConfigLoader.cpp
)

set_target_properties(siligen_runtime_host PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED YES
)

target_include_directories(siligen_runtime_host PUBLIC
    ${RUNTIME_HOST_COMMON_INCLUDE_DIRS}
)

set(RUNTIME_HOST_HAS_REAL_MULTICARD 0)
if(EXISTS "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.lib")
    set(RUNTIME_HOST_HAS_REAL_MULTICARD 1)
endif()

target_compile_definitions(siligen_runtime_host PRIVATE
    HAS_REAL_MULTICARD=${RUNTIME_HOST_HAS_REAL_MULTICARD}
    SILIGEN_ENABLE_MOCK_MULTICARD=$<BOOL:${SILIGEN_ENABLE_MOCK_MULTICARD}>
    _SILENCE_STDEXT_ARR_ITERS_DEPRECATION_WARNING
)

target_link_libraries(siligen_runtime_host PUBLIC
    siligen_process_runtime_core
    siligen_control_runtime_infrastructure
    siligen_shared
)
