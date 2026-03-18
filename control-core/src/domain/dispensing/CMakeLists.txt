# Dispensing 子域 CMakeLists.txt
# Phase 2: DXF Dispensing Execution UseCase 重构 - Domain Service

add_library(domain_dispensing STATIC
    domain-services/ArcTriggerPointCalculator.cpp
    domain-services/DispenseCompensationService.cpp
    domain-services/PathOptimizationStrategy.cpp
    domain-services/TriggerPlanner.cpp
    domain-services/DispensingController.cpp
    domain-services/DispensingProcessService.cpp
)

target_sources(domain_dispensing PRIVATE
    value-objects/DispenseCompensationProfile.h
    value-objects/DispensingExecutionTypes.h
    value-objects/DispensingPlan.h
    value-objects/QualityMetrics.h
    value-objects/SafetyBoundary.h
    value-objects/TriggerPlan.h
)

target_include_directories(domain_dispensing PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(domain_dispensing PUBLIC
    siligen_types
)

target_compile_features(domain_dispensing PUBLIC cxx_std_17)

message(STATUS "[Domain::Dispensing] domain_dispensing 已添加")
