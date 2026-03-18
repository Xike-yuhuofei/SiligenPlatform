cmake_minimum_required(VERSION 3.20)

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
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party
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
