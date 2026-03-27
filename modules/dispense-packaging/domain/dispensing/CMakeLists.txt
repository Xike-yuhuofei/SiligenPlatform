# Dispensing 子域 CMakeLists.txt
# Phase 2: DXF Dispensing Execution UseCase 重构 - Domain Service

if(NOT DEFINED SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR)
    get_filename_component(
        SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR
        "${CMAKE_CURRENT_SOURCE_DIR}/../../../motion-planning"
        ABSOLUTE
    )
endif()

if(NOT TARGET siligen_device_contracts AND EXISTS "${CMAKE_SOURCE_DIR}/shared/contracts/device/CMakeLists.txt")
    add_subdirectory(
        "${CMAKE_SOURCE_DIR}/shared/contracts/device"
        "${CMAKE_BINARY_DIR}/_p/dpd_sdc"
    )
endif()

include_directories(BEFORE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

add_library(siligen_dispense_packaging_domain_dispensing STATIC
    domain-services/ArcTriggerPointCalculator.cpp
    domain-services/DispenseCompensationService.cpp
    domain-services/PathOptimizationStrategy.cpp
    domain-services/TriggerPlanner.cpp
    domain-services/DispensingController.cpp
    domain-services/DispensingProcessService.cpp
    planning/domain-services/ContourOptimizationService.cpp
    planning/domain-services/UnifiedTrajectoryPlannerService.cpp
    planning/domain-services/DispensingPlannerService.cpp
)

target_sources(siligen_dispense_packaging_domain_dispensing PRIVATE
    value-objects/DispenseCompensationProfile.h
    value-objects/DispensingExecutionTypes.h
    value-objects/DispensingPlan.h
    value-objects/QualityMetrics.h
    value-objects/SafetyBoundary.h
    value-objects/TriggerPlan.h
)

target_include_directories(siligen_dispense_packaging_domain_dispensing BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${CMAKE_SOURCE_DIR}/modules/process-path
    ${SILIGEN_MOTION_PLANNING_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_dispense_packaging_domain_dispensing PUBLIC
    siligen_process_runtime_core_boost_headers
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_motion
    siligen_process_path_domain_trajectory
)
if(TARGET siligen_device_contracts)
    target_link_libraries(siligen_dispense_packaging_domain_dispensing PUBLIC
        siligen_device_contracts
    )
endif()
if(TARGET siligen_process_planning_legacy_configuration_bridge_public)
    target_link_libraries(siligen_dispense_packaging_domain_dispensing PUBLIC
        siligen_process_planning_legacy_configuration_bridge_public
    )
endif()

target_compile_features(siligen_dispense_packaging_domain_dispensing PUBLIC cxx_std_17)

message(STATUS "[DispensePackaging::Domain::Dispensing] canonical dispensing target 已添加")
