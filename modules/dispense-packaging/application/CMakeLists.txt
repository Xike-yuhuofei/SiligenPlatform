cmake_minimum_required(VERSION 3.20)

if(NOT TARGET siligen_process_path_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-path/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpa_ppc"
    )
endif()
if(NOT TARGET siligen_motion_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpa_mpc"
    )
endif()
if(NOT TARGET siligen_motion_planning_application_public AND NOT TARGET siligen_motion_planning_application)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../motion-planning/application"
        "${CMAKE_BINARY_DIR}/_p/dpa_mpa"
    )
endif()
if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpa_procplan"
    )
endif()
if(NOT TARGET siligen_device_contracts AND EXISTS "${CMAKE_SOURCE_DIR}/shared/contracts/device/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/shared/contracts/device"
        "${CMAKE_BINARY_DIR}/_p/dpa_sdc"
    )
endif()
if(NOT TARGET siligen_dispense_packaging_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../contracts"
        "${CMAKE_BINARY_DIR}/_p/dpa_dpc"
    )
endif()

set(SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET siligen_motion_planning_application_public)
if(NOT TARGET ${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET} AND TARGET siligen_motion_planning_application)
    set(SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET siligen_motion_planning_application)
endif()

set(SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/include")
if(NOT EXISTS "${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}/application")
    message(FATAL_ERROR
        "dispense-packaging canonical application include root missing: "
        "${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}/application"
    )
endif()

if(NOT TARGET siligen_dispense_packaging_application_headers)
    add_library(siligen_dispense_packaging_application_headers INTERFACE)
endif()
target_include_directories(siligen_dispense_packaging_application_headers INTERFACE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
if(TARGET siligen_dispense_packaging_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE
        siligen_dispense_packaging_contracts_public
    )
endif()
if(TARGET siligen_process_path_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE
        siligen_process_path_contracts_public
    )
endif()
if(TARGET siligen_motion_planning_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application_headers INTERFACE
        siligen_motion_planning_contracts_public
    )
endif()
target_compile_features(siligen_dispense_packaging_application_headers INTERFACE cxx_std_17)

set(SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
    ${SILIGEN_WORKSPACE_IMPLEMENTATION_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)
list(REMOVE_DUPLICATES SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS)

if(NOT TARGET siligen_dispense_packaging_application_planning_common_residual)
    add_library(siligen_dispense_packaging_application_planning_common_residual STATIC
        services/dispensing/PlanningAssemblyResidualCommon.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_application_planning_common_residual PUBLIC
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_dispense_packaging_application_planning_common_residual PRIVATE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispense_packaging_application_planning_common_residual PRIVATE
    siligen_dispense_packaging_domain_dispensing_planning_residual_headers
    siligen_dispense_packaging_contracts_public
    siligen_process_path_contracts_public
    siligen_motion_planning_contracts_public
    siligen_dispense_packaging_planning_owner_residual
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dispense_packaging_application_planning_common_residual PUBLIC cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_application_planning_authority_residual)
    add_library(siligen_dispense_packaging_application_planning_authority_residual STATIC
        services/dispensing/PlanningAssemblyAuthorityArtifacts.cpp
        services/dispensing/PreviewSnapshotResidualProcessPath.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_application_planning_authority_residual PUBLIC
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_dispense_packaging_application_planning_authority_residual PRIVATE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispense_packaging_application_planning_authority_residual PRIVATE
    siligen_dispense_packaging_application_planning_common_residual
    siligen_dispense_packaging_domain_dispensing_planning_residual_headers
    siligen_dispense_packaging_contracts_public
    siligen_process_path_contracts_public
    siligen_motion_planning_contracts_public
    siligen_dispense_packaging_planning_owner_residual
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dispense_packaging_application_planning_authority_residual PUBLIC cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_application_planning_execution_residual)
    add_library(siligen_dispense_packaging_application_planning_execution_residual STATIC
        services/dispensing/PlanningAssemblyExecutionArtifacts.cpp
        services/dispensing/PlanningAssemblyExecutionInterpolation.cpp
        services/dispensing/PlanningAssemblyExecutionBinding.cpp
        services/dispensing/PlanningAssemblyExecutionPackaging.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_application_planning_execution_residual PUBLIC
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_dispense_packaging_application_planning_execution_residual PRIVATE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispense_packaging_application_planning_execution_residual PRIVATE
    siligen_dispense_packaging_application_planning_common_residual
    siligen_dispense_packaging_domain_dispensing_planning_residual_headers
    siligen_dispense_packaging_contracts_public
    siligen_process_path_contracts_public
    siligen_motion_planning_contracts_public
    ${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dispense_packaging_application_planning_execution_residual PUBLIC cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_application_planning_residual)
    add_library(siligen_dispense_packaging_application_planning_residual STATIC
        services/dispensing/PlanningAssemblyResidualFacade.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_application_planning_residual PUBLIC
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_dispense_packaging_application_planning_residual PRIVATE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispense_packaging_application_planning_residual PRIVATE
    siligen_dispense_packaging_application_planning_common_residual
    siligen_dispense_packaging_application_planning_authority_residual
    siligen_dispense_packaging_application_planning_execution_residual
    siligen_dispense_packaging_contracts_public
    siligen_process_path_contracts_public
    siligen_motion_planning_contracts_public
    siligen_types
    siligen_utils
)
target_compile_features(siligen_dispense_packaging_application_planning_residual PUBLIC cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_application)
    add_library(siligen_dispense_packaging_application STATIC
        services/dispensing/PlanningAssemblyServices.cpp
        services/dispensing/PlanningArtifactExportAssemblyService.cpp
        services/dispensing/WorkflowPlanningAssemblyOperationsProvider.cpp
        services/dispensing/PreviewSnapshotService.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_application PUBLIC
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_dispense_packaging_application PRIVATE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispense_packaging_application PUBLIC
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_dispense_packaging_application PRIVATE
    siligen_dispense_packaging_application_planning_residual
)
if(TARGET siligen_dispense_packaging_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application PUBLIC
        siligen_dispense_packaging_contracts_public
    )
endif()
if(TARGET siligen_process_path_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application PUBLIC
        siligen_process_path_contracts_public
    )
endif()
if(TARGET siligen_motion_planning_contracts_public)
    target_link_libraries(siligen_dispense_packaging_application PUBLIC
        siligen_motion_planning_contracts_public
    )
endif()
target_compile_features(siligen_dispense_packaging_application PUBLIC cxx_std_17)

if(NOT TARGET siligen_valve_core)
    add_library(siligen_valve_core STATIC
        usecases/dispensing/valve/ValveUseCases.cpp
        usecases/dispensing/valve/coordination/ValveCoordinationUseCase.cpp
    )
endif()
target_include_directories(siligen_valve_core PUBLIC
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_PUBLIC_INCLUDE_DIR}
)
target_include_directories(siligen_valve_core PRIVATE
    ${SILIGEN_DISPENSE_PACKAGING_APPLICATION_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_valve_core PUBLIC
    siligen_dispense_packaging_contracts_public
    siligen_types
    siligen_utils
)
target_link_libraries(siligen_valve_core PRIVATE
    siligen_dispense_packaging_execution_residual
)
if(TARGET siligen_process_planning_contracts_public)
    target_link_libraries(siligen_valve_core PUBLIC
        siligen_process_planning_contracts_public
    )
endif()
if(TARGET siligen_device_contracts)
    target_link_libraries(siligen_valve_core PUBLIC
        siligen_device_contracts
    )
endif()
target_compile_features(siligen_valve_core PUBLIC cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_application_public)
    add_library(siligen_dispense_packaging_application_public INTERFACE)
endif()
target_link_libraries(siligen_dispense_packaging_application_public INTERFACE
    siligen_dispense_packaging_application_headers
    siligen_dispense_packaging_contracts_public
    $<LINK_ONLY:siligen_dispense_packaging_application>
)
target_compile_features(siligen_dispense_packaging_application_public INTERFACE cxx_std_17)
