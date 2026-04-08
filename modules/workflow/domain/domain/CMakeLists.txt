# ============================================================
# 领域层 CMakeLists.txt
# ============================================================

set(SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS
    ${SILIGEN_WORKSPACE_PUBLIC_INCLUDE_DIRS}
)
set(SILIGEN_DOMAIN_IMPLEMENTATION_INCLUDE_DIRS
    ${SILIGEN_WORKSPACE_IMPLEMENTATION_INCLUDE_DIRS}
)

# workflow/domain/domain 当前仅保留仍有 live consumer 的最小 bridge-domain 聚合。
# 已确认无 live consumer 的 machine / diagnostics / configuration / planning / recovery /
# planning-boundary / supervision / system 物理壳层已从此处清除。
add_subdirectory(_shared)
add_subdirectory(motion)
add_subdirectory(recipes)
add_subdirectory(safety)

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

if(NOT TARGET siligen_motion)
    message(FATAL_ERROR
        "workflow domain requires canonical siligen_motion target from "
        "modules/motion-planning/domain/motion before loading workflow/domain/domain"
    )
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
    ${SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS}
)
target_include_directories(siligen_triggering PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/dispensing/domain-services
    ${SILIGEN_DOMAIN_IMPLEMENTATION_INCLUDE_DIRS}
)

target_link_libraries(siligen_triggering
    siligen_types
    siligen_utils
)

# 配置管理兼容库
# workflow 本地 configuration 仅保留兼容 target，真实实现切换到 M4 legacy configuration bridge。
add_library(siligen_configuration INTERFACE)
target_include_directories(siligen_configuration INTERFACE
    ${SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS}
)
target_link_libraries(siligen_configuration INTERFACE
    siligen_process_planning_contracts_public
)

# 导出领域层库目标
set(DOMAIN_LIBRARIES
    siligen_motion
    siligen_trajectory
    siligen_triggering
    siligen_configuration
    siligen_process_path_contracts_public
)

message(STATUS "领域层库配置完成")

set(SILIGEN_DOMAIN_SHARED_IMPLEMENTATION_INCLUDE_DIRS
    ${CMAKE_CURRENT_SOURCE_DIR}/dispensing/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/motion/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/recipes/domain-services
    ${CMAKE_CURRENT_SOURCE_DIR}/recipes/serialization
    ${CMAKE_CURRENT_SOURCE_DIR}/safety/domain-services
    ${SILIGEN_DOMAIN_IMPLEMENTATION_INCLUDE_DIRS}
)

add_library(siligen_dispensing_execution_services STATIC
    dispensing/domain-services/PurgeDispenserProcess.cpp
    dispensing/domain-services/SupplyStabilizationPolicy.cpp
    dispensing/domain-services/ValveCoordinationService.cpp
)
target_include_directories(siligen_dispensing_execution_services PUBLIC
    ${SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS}
)
target_include_directories(siligen_dispensing_execution_services PRIVATE
    ${SILIGEN_DOMAIN_SHARED_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_dispensing_execution_services PUBLIC
    siligen_types
    siligen_utils
    siligen_process_planning_contracts_public
)
target_compile_features(siligen_dispensing_execution_services PUBLIC cxx_std_17)

add_library(siligen_recipe_domain_services STATIC
    recipes/domain-services/RecipeActivationService.cpp
    recipes/domain-services/RecipeValidationService.cpp
)
target_include_directories(siligen_recipe_domain_services PUBLIC
    ${SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS}
)
target_include_directories(siligen_recipe_domain_services PRIVATE
    ${SILIGEN_DOMAIN_SHARED_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_recipe_domain_services PUBLIC
    siligen_types
    siligen_utils
    siligen_process_core
)
target_compile_features(siligen_recipe_domain_services PUBLIC cxx_std_17)

add_library(siligen_recipe_json_codec STATIC
    recipes/serialization/RecipeJsonSerializer.cpp
)
target_include_directories(siligen_recipe_json_codec PUBLIC
    ${SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS}
)
target_include_directories(siligen_recipe_json_codec PRIVATE
    ${SILIGEN_DOMAIN_SHARED_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_recipe_json_codec PUBLIC
    siligen_workspace_json_headers
    siligen_types
    siligen_utils
)
target_compile_features(siligen_recipe_json_codec PUBLIC cxx_std_17)

add_library(siligen_safety_domain_services STATIC
    safety/domain-services/EmergencyStopService.cpp
    safety/domain-services/InterlockPolicy.cpp
    safety/domain-services/SafetyOutputGuard.cpp
    safety/domain-services/SoftLimitValidator.cpp
)
target_include_directories(siligen_safety_domain_services PUBLIC
    ${SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS}
)
target_include_directories(siligen_safety_domain_services PRIVATE
    ${SILIGEN_DOMAIN_SHARED_IMPLEMENTATION_INCLUDE_DIRS}
)
target_link_libraries(siligen_safety_domain_services PUBLIC
    siligen_types
    siligen_utils
    siligen_motion_core
    siligen_process_planning_contracts_public
)
target_compile_features(siligen_safety_domain_services PUBLIC cxx_std_17)

add_library(siligen_domain_services INTERFACE)
target_include_directories(siligen_domain_services INTERFACE
    ${SILIGEN_DOMAIN_INTERFACE_INCLUDE_DIRS}
)
target_link_libraries(siligen_domain_services INTERFACE
    siligen_recipe_domain_services
    siligen_safety_domain_services
)
target_compile_features(siligen_domain_services INTERFACE cxx_std_17)

# 更新导出库列表
list(APPEND DOMAIN_LIBRARIES
    siligen_recipe_domain_services
    siligen_safety_domain_services
    siligen_domain_services
)

# 为领域层库添加预编译头
if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_motion PRIVATE ${SILIGEN_PCH_HEADER})
    # siligen_trajectory 现在是 INTERFACE 库,跳过预编译头
    # target_precompile_headers(siligen_trajectory PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_triggering PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_recipe_json_codec PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_recipe_domain_services PRIVATE ${SILIGEN_PCH_HEADER})
    target_precompile_headers(siligen_safety_domain_services PRIVATE ${SILIGEN_PCH_HEADER})

    message(STATUS "[Domain Layer] 预编译头已应用于所有领域库")
endif()

set(DOMAIN_LIBRARIES ${DOMAIN_LIBRARIES} PARENT_SCOPE)

message(STATUS "[Domain Layer] 领域服务库配置完成")
