# runtime-service: app-level runtime task scheduler adapter

add_library(siligen_task_scheduler STATIC
    TaskSchedulerAdapter.cpp
)

target_include_directories(siligen_task_scheduler PUBLIC
    ${RUNTIME_HOST_SOURCE_ROOT}
)

# 链接依赖
target_link_libraries(siligen_task_scheduler PUBLIC
    siligen_runtime_execution_runtime_contracts
)
target_link_libraries(siligen_task_scheduler PRIVATE
    siligen_runtime_host_boost_headers
    siligen_types
)

# C++17 标准 (std::thread, std::mutex, std::condition_variable)
target_compile_features(siligen_task_scheduler PUBLIC cxx_std_17)

# Windows 平台链接线程库
if(WIN32)
    # Windows 使用原生线程,无需额外链接
else()
    # Linux/Unix 需要链接 pthread
    find_package(Threads REQUIRED)
    target_link_libraries(siligen_task_scheduler PRIVATE Threads::Threads)
endif()

message(STATUS "[runtime-service] 任务调度器适配器 (siligen_task_scheduler) 已添加")
