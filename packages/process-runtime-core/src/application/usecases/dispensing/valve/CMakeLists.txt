# Valve Core - 依赖端口与领域服务的阀门控制编排库
# 约束:
# - 不包含 TCP/HMI 协议逻辑
# - 不包含文件系统与宿主运行时实现

add_library(siligen_valve_core STATIC
    ValveCommandUseCase.cpp
    ValveQueryUseCase.cpp
)

target_include_directories(siligen_valve_core PUBLIC
    ${PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)

target_link_libraries(siligen_valve_core PUBLIC
    siligen_types
    siligen_domain
    siligen_domain_services
)

target_compile_features(siligen_valve_core PUBLIC cxx_std_17)

if(SILIGEN_USE_PCH)
    target_precompile_headers(siligen_valve_core PRIVATE ${CMAKE_SOURCE_DIR}/src/siligen_pch.h)
    message(STATUS "[Application::Valve] 预编译头已应用于 siligen_valve_core")
endif()

message(STATUS "[Application::Valve] 独立库 siligen_valve_core 配置完成")
