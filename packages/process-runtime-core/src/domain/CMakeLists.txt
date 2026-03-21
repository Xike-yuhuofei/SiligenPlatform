# ============================================================
# 领域层 CMakeLists.txt
# ============================================================

# 子域结构（占位，不影响现有构建）
add_subdirectory(_shared)
add_subdirectory(motion)
add_subdirectory(dispensing)
add_subdirectory(recipes)
add_subdirectory(machine)
add_subdirectory(safety)
add_subdirectory(diagnostics)
add_subdirectory(configuration)
add_subdirectory(system)
add_subdirectory(planning)
add_subdirectory(trajectory)

# 领域模型库 (header-only for now)
# add_library(siligen_models STATIC
#     models/DiagnosticResult.cpp
# )
#
# target_include_directories(siligen_models PUBLIC
#     ${CMAKE_CURRENT_SOURCE_DIR}/models
#     ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
# )
#
# target_link_libraries(siligen_models
#     siligen_types
# )

# 运动控制算法库
add_library(siligen_motion STATIC
    # 插值算法 (domain-services/interpolation/)
    motion/domain-services/interpolation/TrajectoryInterpolatorBase.cpp
    motion/domain-services/interpolation/LinearInterpolator.cpp
    motion/domain-services/interpolation/ArcInterpolator.cpp
    motion/domain-services/interpolation/ArcGeometryMath.cpp
    motion/domain-services/interpolation/SplineInterpolator.cpp
    motion/domain-services/interpolation/SplineGeometryMath.cpp
    motion/domain-services/interpolation/InterpolationCommandValidator.cpp
    motion/domain-services/interpolation/InterpolationProgramPlanner.cpp
    motion/domain-services/interpolation/ValidatedInterpolationPort.cpp

    # 其他领域服务 (domain-services/)
    motion/domain-services/GeometryBlender.cpp
    motion/domain-services/SpeedPlanner.cpp
    motion/domain-services/TrajectoryPlanner.cpp
    motion/domain-services/TriggerCalculator.cpp
    motion/domain-services/MotionBufferController.cpp
    motion/domain-services/TimeTrajectoryPlanner.cpp
    motion/domain-services/VelocityProfileService.cpp
    motion/domain-services/SevenSegmentSCurveProfile.cpp

    # 计算器和验证器 (保留在 motion/ 根目录)
    motion/BSplineCalculator.cpp
    motion/BezierCalculator.cpp
    motion/CircleCalculator.cpp
    motion/CMPValidator.cpp
    motion/CMPCompensation.cpp
    motion/CMPCoordinatedInterpolator.cpp
)

target_include_directories(siligen_motion PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/motion
    ${CMAKE_CURRENT_SOURCE_DIR}/motion/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/motion/domain-services/interpolation
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(siligen_motion
    siligen_types
    siligen_utils
    domain_trajectory
)

if(TARGET domain_dispensing)
    target_link_libraries(domain_dispensing PUBLIC siligen_motion)
endif()

# 轨迹执行器库 (已合并到 siligen_motion)
# 依赖IHardwareController接口，符合六边形架构原则
# 注意: MotionBufferController 已迁移到 motion/domain-services/
# 现在统一在 siligen_motion 库中，这里保留为接口库以维持向后兼容
add_library(siligen_trajectory INTERFACE)

target_link_libraries(siligen_trajectory INTERFACE
    siligen_motion
)

# 触发控制库
add_library(siligen_triggering STATIC
    dispensing/domain-services/PositionTriggerController.cpp
)

target_include_directories(siligen_triggering PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/dispensing/domain-services
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(siligen_triggering
    siligen_types
    siligen_utils
)

# 配置管理库
add_library(siligen_configuration STATIC
    configuration/DispensingConfig.cpp
    configuration/MachineConfig.cpp
    configuration/ValveTimingConfig.cpp
    configuration/CMPPulseConfig.cpp
    configuration/InterpolationConfig.cpp
)

target_include_directories(siligen_configuration PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/configuration
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(siligen_configuration
    siligen_types
    siligen_utils
)

# 导出领域层库目标
set(DOMAIN_LIBRARIES
    siligen_motion
    siligen_trajectory
    siligen_triggering
    siligen_configuration
    domain_trajectory
)

message(STATUS "领域层库配置完成")

set(SILIGEN_DOMAIN_SHARED_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/dispensing/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/diagnostics/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/motion/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/recipes/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/safety/domain-services
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)

add_library(siligen_dispensing_execution_services STATIC
    dispensing/domain-services/CMPTriggerService.cpp
    dispensing/domain-services/PurgeDispenserProcess.cpp
    dispensing/domain-services/SupplyStabilizationPolicy.cpp
    dispensing/domain-services/ValveCoordinationService.cpp
)
target_include_directories(siligen_dispensing_execution_services PUBLIC
    ${SILIGEN_DOMAIN_SHARED_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispensing_execution_services PUBLIC
    siligen_types
    siligen_utils
    siligen_triggering
    siligen_configuration
    siligen_process_core
)
target_compile_features(siligen_dispensing_execution_services PUBLIC cxx_std_17)

add_library(siligen_motion_execution_services STATIC
    motion/domain-services/JogController.cpp
    motion/domain-services/HomingProcess.cpp
    motion/domain-services/MotionControlServiceImpl.cpp
    motion/domain-services/MotionStatusServiceImpl.cpp
)
target_include_directories(siligen_motion_execution_services PUBLIC
    ${SILIGEN_DOMAIN_SHARED_INCLUDE_DIRS}
)
target_link_libraries(siligen_motion_execution_services PUBLIC
    siligen_types
    siligen_utils
    siligen_motion
    siligen_motion_core
)
target_compile_features(siligen_motion_execution_services PUBLIC cxx_std_17)

add_library(siligen_recipe_domain_services STATIC
    recipes/domain-services/RecipeActivationService.cpp
    recipes/domain-services/RecipeValidationService.cpp
)
target_include_directories(siligen_recipe_domain_services PUBLIC
    ${SILIGEN_DOMAIN_SHARED_INCLUDE_DIRS}
)
target_link_libraries(siligen_recipe_domain_services PUBLIC
    siligen_types
    siligen_utils
    siligen_process_core
)
target_compile_features(siligen_recipe_domain_services PUBLIC cxx_std_17)

add_library(siligen_diagnostics_domain_services STATIC
    diagnostics/domain-services/ProcessResultSerialization.cpp
    diagnostics/domain-services/ProcessResultService.cpp
)
target_include_directories(siligen_diagnostics_domain_services PUBLIC
    ${SILIGEN_DOMAIN_SHARED_INCLUDE_DIRS}
)
target_link_libraries(siligen_diagnostics_domain_services PUBLIC
    siligen_types
    siligen_utils
)
target_compile_features(siligen_diagnostics_domain_services PUBLIC cxx_std_17)

add_library(siligen_safety_domain_services STATIC
    safety/domain-services/EmergencyStopService.cpp
    safety/domain-services/InterlockPolicy.cpp
    safety/domain-services/SoftLimitValidator.cpp
)
target_include_directories(siligen_safety_domain_services PUBLIC
    ${SILIGEN_DOMAIN_SHARED_INCLUDE_DIRS}
)
target_link_libraries(siligen_safety_domain_services PUBLIC
    siligen_types
    siligen_utils
    siligen_motion_core
)
target_compile_features(siligen_safety_domain_services PUBLIC cxx_std_17)

add_library(siligen_domain_services INTERFACE)
target_include_directories(siligen_domain_services INTERFACE
    ${SILIGEN_DOMAIN_SHARED_INCLUDE_DIRS}
)
target_link_libraries(siligen_domain_services INTERFACE
    siligen_dispensing_execution_services
    siligen_motion_execution_services
    siligen_recipe_domain_services
    siligen_diagnostics_domain_services
    siligen_safety_domain_services
)
target_compile_features(siligen_domain_services INTERFACE cxx_std_17)

# 更新导出库列表
list(APPEND DOMAIN_LIBRARIES
    siligen_dispensing_execution_services
    siligen_motion_execution_services
    siligen_recipe_domain_services
    siligen_diagnostics_domain_services
    siligen_safety_domain_services
    siligen_domain_services
)

# 为领域层库添加预编译头
if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_motion PRIVATE ${SILIGEN_PCH_HEADER})
    # siligen_trajectory 现在是 INTERFACE 库,跳过预编译头
    # target_precompile_headers(siligen_trajectory PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_triggering PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_configuration PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_dispensing_execution_services PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_motion_execution_services PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_recipe_domain_services PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_diagnostics_domain_services PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_safety_domain_services PRIVATE ${SILIGEN_PCH_HEADER})

    message(STATUS "[Domain Layer] 预编译头已应用于所有领域库")
endif()

set(DOMAIN_LIBRARIES ${DOMAIN_LIBRARIES} PARENT_SCOPE)

message(STATUS "[Domain Layer] 领域服务库配置完成")

