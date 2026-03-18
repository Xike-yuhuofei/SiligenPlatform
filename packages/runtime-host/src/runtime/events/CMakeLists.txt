# control-runtime: app-level runtime event publishing adapter

add_library(siligen_event_adapters STATIC
    InMemoryEventPublisherAdapter.cpp
)

target_include_directories(siligen_event_adapters PUBLIC
    ${SILIGEN_RUNTIME_HOST_DIR}/src
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
)

# 链接依赖
target_link_libraries(siligen_event_adapters PRIVATE
    siligen_types
    siligen_spdlog_adapter  # Phase 3: 六边形架构日志系统
)

target_compile_features(siligen_event_adapters PUBLIC cxx_std_17)

message(STATUS "[control-runtime] 事件发布适配器 (siligen_event_adapters) 已添加")
