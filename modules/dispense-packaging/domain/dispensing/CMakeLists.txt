# Dispensing 子域 CMakeLists.txt
# ARCH-205 current fact:
# 当前目录同时承载 owner header root，以及 planning / execution residual 实现。
# 本阶段只做 build graph 收缩：owner/core 退化为 header/contract surface，
# concrete residual 显式挂到 planning_residual / execution_residual，不再继续藏在 owner target。

if(NOT TARGET siligen_device_contracts AND EXISTS "${CMAKE_SOURCE_DIR}/shared/contracts/device/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/shared/contracts/device"
        "${CMAKE_BINARY_DIR}/_p/dpd_sdc"
    )
endif()

if(NOT TARGET siligen_process_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../process-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpd_ppc"
    )
endif()

if(NOT TARGET siligen_motion_planning_contracts_public)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../motion-planning/contracts"
        "${CMAKE_BINARY_DIR}/_p/dpd_mpc"
    )
endif()

if(NOT TARGET siligen_motion_planning_application_public AND NOT TARGET siligen_motion_planning_application)
    add_subdirectory(
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../motion-planning/application"
        "${CMAKE_BINARY_DIR}/_p/dpd_mpa"
    )
endif()

set(SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET siligen_motion_planning_application_public)
if(NOT TARGET ${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET} AND TARGET siligen_motion_planning_application)
    set(SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET siligen_motion_planning_application)
endif()

include_directories(BEFORE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

set(SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../runtime-execution/contracts/runtime/include"
)
set(SILIGEN_WORKFLOW_DOMAIN_IMPL_INCLUDE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../workflow/domain"
)
if(NOT EXISTS "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}/runtime_execution/contracts/motion/IInterpolationPort.h")
    message(FATAL_ERROR
        "dispense-packaging requires runtime-execution runtime contracts include root: "
        "${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}"
    )
endif()

if(NOT TARGET siligen_dispense_packaging_domain_dispensing)
    add_library(siligen_dispense_packaging_domain_dispensing INTERFACE)
endif()
target_include_directories(siligen_dispense_packaging_domain_dispensing BEFORE INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_link_libraries(siligen_dispense_packaging_domain_dispensing INTERFACE
    siligen_types
    siligen_utils
    siligen_motion_planning_contracts_public
    siligen_process_path_contracts_public
    siligen_process_planning_contracts_public
)
if(TARGET siligen_device_contracts)
    target_link_libraries(siligen_dispense_packaging_domain_dispensing INTERFACE
        siligen_device_contracts
    )
endif()
target_compile_features(siligen_dispense_packaging_domain_dispensing INTERFACE cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_execution_residual)
    add_library(siligen_dispense_packaging_execution_residual STATIC
        domain-services/CMPTriggerService.cpp
        domain-services/DispenseCompensationService.cpp
        domain-services/PurgeDispenserProcess.cpp
        domain-services/ValveCoordinationService.cpp
        domain-services/DispensingController.cpp
        domain-services/DispensingProcessService.cpp
        domain-services/SupplyStabilizationPolicy.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_execution_residual BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_include_directories(siligen_dispense_packaging_execution_residual PRIVATE
    ${SILIGEN_WORKFLOW_DOMAIN_IMPL_INCLUDE_DIR}
)
target_link_libraries(siligen_dispense_packaging_execution_residual PUBLIC
    siligen_dispense_packaging_domain_dispensing
    siligen_types
    siligen_utils
    siligen_process_planning_contracts_public
)
if(TARGET siligen_device_contracts)
    target_link_libraries(siligen_dispense_packaging_execution_residual PUBLIC
        siligen_device_contracts
    )
endif()
target_compile_features(siligen_dispense_packaging_execution_residual PUBLIC cxx_std_17)

if(NOT TARGET siligen_dispense_packaging_planning_residual)
    add_library(siligen_dispense_packaging_planning_residual STATIC
        domain-services/ArcTriggerPointCalculator.cpp
        domain-services/PathOptimizationStrategy.cpp
        domain-services/TriggerPlanner.cpp
        planning/domain-services/AuthorityTriggerLayoutPlanner.cpp
        planning/domain-services/ClosedLoopAnchorConstraintSolver.cpp
        planning/domain-services/ClosedLoopCornerAnchorResolver.cpp
        planning/domain-services/ContourOptimizationService.cpp
        planning/domain-services/CurveFlatteningService.cpp
        planning/domain-services/DispensingPlannerService.cpp
        planning/domain-services/PathArcLengthLocator.cpp
        planning/domain-services/TopologyComponentClassifier.cpp
        planning/domain-services/TopologySpanSplitter.cpp
        planning/domain-services/UnifiedTrajectoryPlannerService.cpp
    )
endif()
target_include_directories(siligen_dispense_packaging_planning_residual BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${SILIGEN_RUNTIME_EXECUTION_RUNTIME_CONTRACTS_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)
target_include_directories(siligen_dispense_packaging_planning_residual PRIVATE
    ${SILIGEN_WORKFLOW_DOMAIN_IMPL_INCLUDE_DIR}
)
target_link_libraries(siligen_dispense_packaging_planning_residual PUBLIC
    siligen_dispense_packaging_domain_dispensing
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_motion
    siligen_process_path_application_public
    ${SILIGEN_DISPENSE_PACKAGING_MOTION_PLANNING_APP_TARGET}
)
target_link_libraries(siligen_dispense_packaging_planning_residual PRIVATE
    siligen_dispense_packaging_execution_residual
)
target_compile_features(siligen_dispense_packaging_planning_residual PUBLIC cxx_std_17)

message(STATUS "[DispensePackaging::Domain::Dispensing] owner/core target and residual targets 已添加")
