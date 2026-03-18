# Application层 - 应用协调和用例
# 本层包含用例实现和依赖注入容器

# 首批独立库: 配方纯业务能力
# 保留文件IO型导入/导出用例在宿主层,便于后续继续剥离 host runtime。
add_subdirectory(usecases/recipes)
add_subdirectory(usecases/dispensing/valve)

set(SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

add_library(siligen_application_system STATIC
    usecases/system/InitializeSystemUseCase.cpp
    usecases/system/EmergencyStopUseCase.cpp
)
target_include_directories(siligen_application_system PUBLIC
    ${SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(siligen_application_system PUBLIC
    siligen_types
    siligen_utils
    siligen_domain
    siligen_domain_services
)
target_compile_features(siligen_application_system PUBLIC cxx_std_17)

add_library(siligen_application_motion STATIC
    usecases/motion/manual/ManualMotionControlUseCase.cpp
    usecases/motion/initialization/MotionInitializationUseCase.cpp
    usecases/motion/safety/MotionSafetyUseCase.cpp
    usecases/motion/monitoring/MotionMonitoringUseCase.cpp
    usecases/motion/coordination/MotionCoordinationUseCase.cpp
    usecases/motion/homing/HomeAxesUseCase.cpp
    usecases/motion/ptp/MoveToPositionUseCase.cpp
    usecases/motion/trajectory/ExecuteTrajectoryUseCase.cpp
    usecases/motion/interpolation/InterpolationPlanningUseCase.cpp
)
target_include_directories(siligen_application_motion PUBLIC
    ${SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(siligen_application_motion PUBLIC
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_domain
    siligen_domain_services
)
target_compile_features(siligen_application_motion PUBLIC cxx_std_17)

add_library(siligen_application_dispensing STATIC
    usecases/dispensing/valve/coordination/ValveCoordinationUseCase.cpp
    usecases/dispensing/dxf/UploadDXFFileUseCase.cpp
    usecases/dispensing/dxf/CleanupDXFFilesUseCase.cpp
    usecases/dispensing/dxf/DXFWebPlanningUseCase.cpp
    usecases/dispensing/dxf/DXFDispensingExecutionUseCase.cpp
    usecases/dispensing/dxf/DXFDispensingExecutionUseCase.Setup.cpp
    usecases/dispensing/dxf/DXFDispensingExecutionUseCase.Control.cpp
    usecases/dispensing/dxf/DXFDispensingExecutionUseCase.Async.cpp
    usecases/dispensing/dxf/DXFDispensingPlanner.cpp
    usecases/dispensing/dxf/ContourPathOptimizer.cpp
    usecases/dispensing/dxf/UnifiedTrajectoryPlanner.cpp
    services/dxf/DxfPbPreparationService.cpp
)
target_include_directories(siligen_application_dispensing PUBLIC
    ${SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(siligen_application_dispensing PUBLIC
    siligen_types
    siligen_utils
    siligen_geometry
    siligen_domain
    siligen_domain_services
    domain_dispensing
)
target_compile_features(siligen_application_dispensing PUBLIC cxx_std_17)

# Canonical target is `siligen_control_application`; keep the old name only for
# compatibility with legacy build scripts and transitional references.
add_library(siligen_control_application INTERFACE)
target_include_directories(siligen_control_application INTERFACE
    ${SILIGEN_APPLICATION_PUBLIC_INCLUDE_DIRS}
)
target_link_libraries(siligen_control_application INTERFACE
    siligen_application_system
    siligen_application_motion
    siligen_application_dispensing
    siligen_recipe_core
    siligen_valve_core
)
target_compile_features(siligen_control_application INTERFACE cxx_std_17)
add_library(siligen_application ALIAS siligen_control_application)

# CGAL (填充算法)
if(SILIGEN_ENABLE_CGAL)
    target_sources(siligen_application_dispensing PRIVATE
        usecases/dispensing/dxf/DXFContourAugmenter.cpp
    )
    find_package(CGAL REQUIRED COMPONENTS Core)
    target_link_libraries(siligen_application_dispensing PUBLIC
        CGAL::CGAL
        CGAL::CGAL_Core
    )
    target_compile_definitions(siligen_application_dispensing PRIVATE SILIGEN_ENABLE_CGAL=1)
    message(STATUS "[Application Layer] CGAL enabled: DXFContourAugmenter")
else()
    target_sources(siligen_application_dispensing PRIVATE
        usecases/dispensing/dxf/DXFContourAugmenter.stub.cpp
    )
    target_compile_definitions(siligen_application_dispensing PRIVATE SILIGEN_ENABLE_CGAL=0)
    message(STATUS "[Application Layer] CGAL disabled: DXFContourAugmenter stub")
endif()

# 为应用层库添加预编译头
if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_application_system PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    target_precompile_headers(siligen_application_motion PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    target_precompile_headers(siligen_application_dispensing PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    message(STATUS "[Application Layer] 预编译头已应用于应用层库")
endif()

message(STATUS "[Application Layer] 应用层配置完成")

