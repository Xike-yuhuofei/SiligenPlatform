# runtime-service: app-level runtime event publishing adapter

add_library(siligen_event_adapters STATIC
    InMemoryEventPublisherAdapter.cpp
)

target_include_directories(siligen_event_adapters PUBLIC
    ${RUNTIME_HOST_SOURCE_ROOT}
)

# 链接依赖
target_link_libraries(siligen_event_adapters PUBLIC
    siligen_runtime_execution_runtime_contracts
)
target_link_libraries(siligen_event_adapters PRIVATE
    siligen_runtime_host_boost_headers
    siligen_types
)

target_compile_features(siligen_event_adapters PUBLIC cxx_std_17)

message(STATUS "[runtime-service] 事件发布适配器 (siligen_event_adapters) 已添加")
