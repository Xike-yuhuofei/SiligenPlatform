# Dispensing 子域 CMakeLists.txt
# ARCH-205 Phase 1 current fact:
# 当前 target 仍同时编译 packaging owner、planning residual 与 process-control residual。
# owner 迁移尚未完成，本文件只反映 live build 事实。

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
    planning/domain-services/AuthorityTriggerLayoutPlanner.cpp
    planning/domain-services/ClosedLoopCornerAnchorResolver.cpp
    planning/domain-services/ClosedLoopAnchorConstraintSolver.cpp
    planning/domain-services/PathArcLengthLocator.cpp
    planning/domain-services/CurveFlatteningService.cpp
    planning/domain-services/TopologySpanSplitter.cpp
    planning/domain-services/TopologyComponentClassifier.cpp
)

target_sources(siligen_dispense_packaging_domain_dispensing PRIVATE
    value-objects/DispenseCompensationProfile.h
    value-objects/AuthorityTriggerLayout.h
    value-objects/DispensingExecutionTypes.h
    value-objects/DispensingPlan.h
    value-objects/QualityMetrics.h
    value-objects/SafetyBoundary.h
    value-objects/TriggerPlan.h
)

target_include_directories(siligen_dispense_packaging_domain_dispensing BEFORE PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
    ${SILIGEN_WORKFLOW_DOMAIN_PUBLIC_INCLUDE_DIR}
    ${SILIGEN_SHARED_COMPAT_INCLUDE_ROOT}
)

target_link_libraries(siligen_dispense_packaging_domain_dispensing PUBLIC
    siligen_process_runtime_core_boost_headers
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_motion
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
