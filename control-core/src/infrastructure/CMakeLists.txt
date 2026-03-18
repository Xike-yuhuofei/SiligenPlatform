# Infrastructure层 - 基础设施和适配器实现
# 本层包含硬件、日志、配置的具体实现

# Infrastructure层硬件适配器库
add_library(siligen_device_hal_hardware STATIC
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/drivers/multicard/MultiCardAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Connection.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Jog.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Homing.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.IO.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Trigger.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Cmp.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Interpolation.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Diagnostics.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Safety.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/health/testing/HardwareTestAdapter.Helpers.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/homing/HomingPortAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/runtime/MotionRuntimeConnectionAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/runtime/MotionRuntimeFacade.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/dispensing/dispenser/ValveAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/dispensing/dispenser/ValveAdapter.Dispenser.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/dispensing/dispenser/ValveAdapter.Supply.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/dispensing/dispenser/ValveAdapter.Hardware.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/dispensing/dispenser/triggering/TriggerControllerAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/interpolation/InterpolationAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/internal/UnitConverter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/drivers/multicard/MultiCardErrorCodes.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/control/MultiCardMotionAdapter.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/control/MultiCardMotionAdapter.Status.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/control/MultiCardMotionAdapter.Motion.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/control/MultiCardMotionAdapter.Settings.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/motion/controller/control/MultiCardMotionAdapter.Helpers.cpp
)
add_library(siligen_hardware ALIAS siligen_device_hal_hardware)

# ============================================================
# Phase 1.3: 六边形架构日志系统 - SpdlogLoggingAdapter
# ============================================================
add_library(siligen_spdlog_adapter STATIC
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/diagnostics/logging/SpdlogLoggingAdapter.cpp
)

target_include_directories(siligen_spdlog_adapter PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
    ${CMAKE_SOURCE_DIR}/third_party/spdlog/include
)

target_link_libraries(siligen_spdlog_adapter PRIVATE
    siligen_types
)

target_compile_features(siligen_spdlog_adapter PUBLIC cxx_std_17)

message(STATUS "[Infrastructure] SpdlogLoggingAdapter (siligen_spdlog_adapter) 已添加")

if(SILIGEN_ENABLE_MOCK_MULTICARD)
    target_sources(siligen_device_hal_hardware PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/device-hal/src/drivers/multicard/MockMultiCard.cpp
        ${CMAKE_SOURCE_DIR}/modules/device-hal/src/drivers/multicard/MockMultiCardWrapper.cpp
    )
endif()

# MultiCard混合模式
# - 有真实库时: 使用厂商MultiCard.lib (不编译Stub)
# - 无真实库时: 使用MultiCardStub_Class.cpp提供存根
set(MULTICARD_STUB_SOURCE "${CMAKE_SOURCE_DIR}/modules/device-hal/src/drivers/multicard/MultiCardStub_Class.cpp")

# Persistence层库
add_library(siligen_device_hal_persistence STATIC
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/recipes/RecipeJsonSerializer.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/recipes/RecipeFileRepository.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/recipes/TemplateFileRepository.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/recipes/AuditFileRepository.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/recipes/ParameterSchemaFileProvider.cpp
    ${CMAKE_SOURCE_DIR}/modules/device-hal/src/adapters/recipes/RecipeBundleSerializer.cpp
)
add_library(siligen_persistence ALIAS siligen_device_hal_persistence)

target_include_directories(siligen_device_hal_persistence PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/util
)

target_link_libraries(siligen_device_hal_persistence PRIVATE
    siligen_types
    siligen_utils
)

target_compile_features(siligen_device_hal_persistence PRIVATE cxx_std_17)

# Configs层库
add_library(siligen_control_runtime_config STATIC
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/configuration/ConfigFileAdapter.cpp
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/configuration/ConfigFileAdapter.Ini.cpp
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/configuration/ConfigFileAdapter.Sections.cpp
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/configuration/ConfigFileAdapter.System.cpp
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/configuration/validators/ConfigValidator.cpp
)
add_library(siligen_configs ALIAS siligen_control_runtime_config)

target_include_directories(siligen_control_runtime_config PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${SILIGEN_RUNTIME_HOST_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/util
)

target_link_libraries(siligen_control_runtime_config PRIVATE
    siligen_types
    siligen_utils
)

target_compile_features(siligen_control_runtime_config PRIVATE cxx_std_17)

# Services层库
add_library(siligen_control_runtime_storage STATIC
    ${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/storage/files/LocalFileStorageAdapter.cpp
)
add_library(siligen_services ALIAS siligen_control_runtime_storage)

target_include_directories(siligen_control_runtime_storage PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${SILIGEN_RUNTIME_HOST_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../application/types
    ${CMAKE_CURRENT_SOURCE_DIR}/../application/ports
)

target_link_libraries(siligen_control_runtime_storage PRIVATE
    siligen_types
    siligen_utils
    siligen_parsing_adapter  # Parsing adapter (DXF parsers)
)

target_compile_features(siligen_control_runtime_storage PRIVATE cxx_std_17)

# Factories层库
add_library(siligen_control_runtime_factories STATIC
    factories/InfrastructureAdapterFactory.cpp
)
add_library(siligen_factories ALIAS siligen_control_runtime_factories)

target_include_directories(siligen_control_runtime_factories PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
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

# 添加包含目录
target_include_directories(siligen_device_hal_hardware PUBLIC
    ${CMAKE_SOURCE_DIR}
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/interfaces
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/util
)

# 依赖shared层
target_link_libraries(siligen_device_hal_hardware PRIVATE
    siligen_types
    siligen_utils
    siligen_device_hal
    domain_dispensing
)

# 链接 observability 库(如果启用)
if(SILIGEN_ENABLE_OBSERVABILITY)
    target_link_libraries(siligen_device_hal_hardware PRIVATE observability)
endif()

# C++17标准
target_compile_features(siligen_device_hal_hardware PRIVATE cxx_std_17)

# ========================================
# MultiCard API库链接 (Phase 8 - 硬件测试集成)
# ========================================

# 静态链接厂商提供的 MultiCard SDK（二进制已收口到 modules/device-hal/vendor/multicard）
set(MULTICARD_LIB_PATH "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.lib")
set(MULTICARD_DLL_PATH "${SILIGEN_MULTICARD_VENDOR_DIR}/MultiCard.dll")
set(HAS_REAL_MULTICARD 0)

# 检查 MultiCard.lib 是否存在
if(EXISTS ${MULTICARD_LIB_PATH})
    # 实际硬件库存在 - 使用真实硬件支持
    set(HAS_REAL_MULTICARD 1)
    target_sources(siligen_device_hal_hardware PRIVATE
        ${CMAKE_SOURCE_DIR}/modules/device-hal/src/drivers/multicard/RealMultiCardWrapper.cpp
    )
    target_link_libraries(siligen_device_hal_hardware PRIVATE ${MULTICARD_LIB_PATH})
    message(STATUS "[Infrastructure] 已链接厂商MultiCard.lib: ${MULTICARD_LIB_PATH}")

    # 拷贝MultiCard.dll到输出目录
    if(EXISTS ${MULTICARD_DLL_PATH})
        add_custom_command(TARGET siligen_device_hal_hardware POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${MULTICARD_DLL_PATH}"
                "$<TARGET_FILE_DIR:siligen_device_hal_hardware>"
            COMMENT "拷贝MultiCard.dll到输出目录"
        )
        message(STATUS "[Infrastructure] 配置MultiCard.dll自动拷贝")
    else()
        message(WARNING "[Infrastructure] MultiCard.dll not found at ${MULTICARD_DLL_PATH}")
    endif()
else()
    # 库文件不存在 - 使用 Stub 实现（适用于 CI/测试环境）
    message(WARNING "[Infrastructure] MultiCard.lib not found at ${MULTICARD_LIB_PATH}")
    message(STATUS "[Infrastructure] 使用 MultiCardStub_Class.cpp 提供的存根实现")
    message(STATUS "[Infrastructure] 注意: 存根实现仅用于编译测试，不提供实际硬件功能")

    # 仅在无真实库时编译桩实现
    if(EXISTS ${MULTICARD_STUB_SOURCE})
        target_sources(siligen_device_hal_hardware PRIVATE ${MULTICARD_STUB_SOURCE})
        message(STATUS "[Infrastructure] 已添加 MultiCardStub_Class.cpp")
    else()
        message(FATAL_ERROR "[Infrastructure] MultiCardStub_Class.cpp not found")
    endif()
endif()

target_compile_definitions(siligen_device_hal_hardware PRIVATE
    HAS_REAL_MULTICARD=${HAS_REAL_MULTICARD}
    SILIGEN_ENABLE_MOCK_MULTICARD=$<BOOL:${SILIGEN_ENABLE_MOCK_MULTICARD}>
)
target_compile_definitions(siligen_control_runtime_factories PRIVATE
    HAS_REAL_MULTICARD=${HAS_REAL_MULTICARD}
    SILIGEN_ENABLE_MOCK_MULTICARD=$<BOOL:${SILIGEN_ENABLE_MOCK_MULTICARD}>
)

# 兼容接口库（用于header-only部分）
add_library(siligen_control_runtime_infrastructure INTERFACE)
add_library(siligen_infrastructure ALIAS siligen_control_runtime_infrastructure)
target_include_directories(siligen_control_runtime_infrastructure INTERFACE
    ${CMAKE_SOURCE_DIR}
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${SILIGEN_RUNTIME_HOST_DIR}/src
)
target_link_libraries(siligen_control_runtime_infrastructure INTERFACE
    siligen_device_hal_hardware
    siligen_device_hal_persistence
    siligen_control_runtime_config
    siligen_control_runtime_storage
    siligen_spdlog_adapter
    siligen_control_runtime_factories
    siligen_parsing_adapter  # Parsing adapter
    siligen_event_adapters  # P1: 事件总线集成
    siligen_task_scheduler  # Phase 2: 任务调度器适配器
)

# 总是链接 observability 库 (提供stub实现当禁用时)
# 注意：这个链接必须在 observability 库定义之后才生效（CMake会延迟解析）
target_link_libraries(siligen_control_runtime_infrastructure INTERFACE observability)

# 为基础设施层库添加预编译头
if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_device_hal_hardware PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    target_precompile_headers(siligen_device_hal_persistence PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    target_precompile_headers(siligen_control_runtime_config PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    target_precompile_headers(siligen_control_runtime_storage PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    target_precompile_headers(siligen_control_runtime_factories PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)

    message(STATUS "[Infrastructure Layer] 预编译头已应用于所有基础设施库")
endif()

message(STATUS "[Infrastructure Layer] 基础设施层配置完成")
message(STATUS "  - 默认 target: siligen_device_hal_hardware（兼容 alias: siligen_hardware）")

# Storage adapters (文件存储适配器)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/system/storage/CMakeLists.txt")
    add_subdirectory(adapters/system/storage)
endif()

# Parsing adapters (DXF 文件解析适配器)
add_subdirectory(adapters/planning/dxf)

# Hardware adapters tests (点胶阀幂等性测试)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/dispensing/dispenser/CMakeLists.txt")
    add_subdirectory(adapters/dispensing/dispenser)
endif()

# ========================================
# 序列化库 (Phase 1 - Issue #17)
# ========================================
# Phase 2: 添加序列化器实现文件（.cpp）
# 注意: EnumConverters, HomingTestDataSerializer, InterpolationTestDataSerializer 是 header-only
# JogTestDataSerializer, TriggerTestDataSerializer, CMPTestDataSerializer, DiagnosticResultSerializer 有独立的 .cpp 实现文件
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/diagnostics/serializers/JogTestDataSerializer.cpp")
    add_library(siligen_serialization STATIC
        adapters/diagnostics/serializers/JogTestDataSerializer.cpp
        adapters/diagnostics/serializers/TriggerTestDataSerializer.cpp
        adapters/diagnostics/serializers/CMPTestDataSerializer.cpp
        adapters/diagnostics/serializers/DiagnosticResultSerializer.cpp
    )
else()
    add_library(siligen_serialization INTERFACE)
endif()

target_include_directories(siligen_serialization INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../shared/types
    ${CMAKE_SOURCE_DIR}/third_party  # nlohmann/json
)

target_link_libraries(siligen_serialization INTERFACE
    siligen_types
)

target_compile_features(siligen_serialization INTERFACE cxx_std_17)

message(STATUS "[Infrastructure] 序列化库 (siligen_serialization) 已添加")

# 事件发布 / 任务调度属于 runtime-host 宿主装配职责，主实现位于 packages/runtime-host/src/runtime/*
set(CONTROL_RUNTIME_EVENTS_DIR "${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/events")
if(EXISTS "${CONTROL_RUNTIME_EVENTS_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${CONTROL_RUNTIME_EVENTS_DIR}"
        "${CMAKE_BINARY_DIR}/packages/runtime-host/runtime-events"
    )
endif()

set(CONTROL_RUNTIME_SCHEDULING_DIR "${SILIGEN_RUNTIME_HOST_DIR}/src/runtime/scheduling")
if(EXISTS "${CONTROL_RUNTIME_SCHEDULING_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${CONTROL_RUNTIME_SCHEDULING_DIR}"
        "${CMAKE_BINARY_DIR}/packages/runtime-host/runtime-scheduling"
    )
endif()

# ============================================================
# Observability Library
# ============================================================
# Always compile observability library (provides stub when disabled)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/adapters/system/observability/ScopedSpan.cpp")
    add_library(observability
        adapters/system/observability/ScopedSpan.cpp
    )
else()
    add_library(observability INTERFACE)
endif()

get_target_property(OBSERVABILITY_TYPE observability TYPE)
if(SILIGEN_ENABLE_OBSERVABILITY AND NOT OBSERVABILITY_TYPE STREQUAL "INTERFACE_LIBRARY")
    # Add ObservabilityInitializer when enabled
    target_sources(observability PRIVATE
        adapters/system/observability/ObservabilityInitializer.cpp
    )

    target_link_libraries(observability
        PUBLIC
            opentelemetry-cpp::api
            opentelemetry-cpp::sdk
            opentelemetry-cpp::ostream_span_exporter
    )

    target_compile_definitions(observability
        PUBLIC
            SILIGEN_ENABLE_OBSERVABILITY
    )
endif()

target_include_directories(observability
    INTERFACE
        ${CMAKE_SOURCE_DIR}/src
)
