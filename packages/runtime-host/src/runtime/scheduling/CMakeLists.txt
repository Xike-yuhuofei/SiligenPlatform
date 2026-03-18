# control-runtime: app-level runtime task scheduler adapter

add_library(siligen_task_scheduler STATIC
    TaskSchedulerAdapter.cpp
)

target_include_directories(siligen_task_scheduler PUBLIC
    ${SILIGEN_RUNTIME_HOST_DIR}/src
    ${SILIGEN_PROCESS_RUNTIME_CORE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src
)

# 链接依赖
target_link_libraries(siligen_task_scheduler PRIVATE
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

message(STATUS "[control-runtime] 任务调度器适配器 (siligen_task_scheduler) 已添加")
