cmake_minimum_required(VERSION 3.20)

project(simulation-engine VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIM_ENGINE_BUILD_EXAMPLES "Build simulation-engine examples" ON)
option(SIM_ENGINE_BUILD_TESTS "Build simulation-engine tests" ON)

if(MSVC)
    set(
        CMAKE_MSVC_DEBUG_INFORMATION_FORMAT
        Embedded
        CACHE STRING "MSVC debug info format for standalone simulation-engine builds"
        FORCE
    )
    add_compile_options(/FS)
endif()

if(SIM_ENGINE_BUILD_TESTS)
    set(BUILD_TESTING ON CACHE BOOL "Enable tests for standalone simulation-engine build" FORCE)
    enable_testing()
else()
    set(BUILD_TESTING OFF CACHE BOOL "Enable tests for standalone simulation-engine build" FORCE)
endif()

set(SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../process-runtime-core"
)
set(SIM_ENGINE_SHARED_KERNEL_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../shared-kernel"
)
set(SIM_ENGINE_WORKSPACE_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
)
set(SIM_ENGINE_THIRD_PARTY_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../third_party"
)
if(NOT EXISTS "${SIM_ENGINE_THIRD_PARTY_DIR}")
    message(FATAL_ERROR "simulation-engine 需要 workspace root third_party 目录: ${SIM_ENGINE_THIRD_PARTY_DIR}")
endif()
set(SIM_ENGINE_SHARED_COMPAT_INCLUDE_ROOT
    "${SIM_ENGINE_SHARED_KERNEL_DIR}/include"
)
set(SILIGEN_SHARED_COMPAT_INCLUDE_ROOT
    "${SIM_ENGINE_SHARED_COMPAT_INCLUDE_ROOT}"
    CACHE INTERNAL "Compatibility include root for shared/* headers"
)
set(SILIGEN_THIRD_PARTY_DIR
    "${SIM_ENGINE_THIRD_PARTY_DIR}"
    CACHE INTERNAL "Workspace third_party include root"
)
set(SIM_ENGINE_PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/include
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/src
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/modules/process-core/include
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/modules/motion-core/include
    ${SIM_ENGINE_SHARED_COMPAT_INCLUDE_ROOT}
    ${SIM_ENGINE_THIRD_PARTY_DIR}
)

if(NOT TARGET siligen_shared_kernel AND EXISTS "${SIM_ENGINE_SHARED_KERNEL_DIR}/CMakeLists.txt")
    add_subdirectory(
        "${SIM_ENGINE_SHARED_KERNEL_DIR}"
        "${CMAKE_BINARY_DIR}/packages/shared-kernel"
    )
endif()

if(NOT TARGET ruckig AND EXISTS "${SIM_ENGINE_THIRD_PARTY_DIR}/ruckig/CMakeLists.txt")
    set(SIM_ENGINE_SAVED_BUILD_TESTING "${BUILD_TESTING}")
    set(SIM_ENGINE_SAVED_BUILD_TESTS "${BUILD_TESTS}")
    set(BUILD_TESTING OFF CACHE BOOL "Disable third-party tests while configuring ruckig" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "Disable upstream ruckig tests in simulation-engine builds" FORCE)
    add_subdirectory(
        "${SIM_ENGINE_THIRD_PARTY_DIR}/ruckig"
        "${CMAKE_BINARY_DIR}/third_party/ruckig"
    )
    set(BUILD_TESTING "${SIM_ENGINE_SAVED_BUILD_TESTING}" CACHE BOOL "Restore package test surface after configuring ruckig" FORCE)
    set(BUILD_TESTS "${SIM_ENGINE_SAVED_BUILD_TESTS}" CACHE BOOL "Restore BUILD_TESTS after configuring ruckig" FORCE)
endif()

if(NOT TARGET siligen_types OR NOT TARGET siligen_utils OR NOT TARGET siligen_geometry)
    message(FATAL_ERROR "simulation-engine 需要 packages/shared-kernel 提供 siligen_types/siligen_utils/siligen_geometry")
endif()

if(NOT TARGET siligen_process_runtime_core AND EXISTS "${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/CMakeLists.txt")
    set(SILIGEN_USE_PCH OFF CACHE BOOL "Disable shared PCH for simulation-engine standalone build" FORCE)
    set(SILIGEN_ENABLE_CGAL OFF CACHE BOOL "Disable CGAL in simulation-engine standalone build" FORCE)
    set(SILIGEN_ENABLE_PROTOBUF OFF CACHE BOOL "Disable protobuf-bound DXF adapter path in simulation-engine standalone build" FORCE)
    add_subdirectory(
        "${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}"
        "${CMAKE_BINARY_DIR}/packages/process-runtime-core"
    )
endif()

find_package(nlohmann_json CONFIG QUIET)
if(NOT nlohmann_json_FOUND)
    message(STATUS "nlohmann_json not found locally, fetching from upstream...")
    FetchContent_Declare(
        nlohmann_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

add_library(simulation_engine
    src/simulation_engine.cpp
    src/simulation_clock.cpp
    src/trajectory/trajectory.cpp
    src/trajectory/line_segment.cpp
    src/trajectory/arc_segment.cpp
    src/planner/velocity_profile.cpp
    src/planner/lookahead_planner.cpp
    src/motion/axis_model.cpp
    src/motion/trajectory_executor.cpp
    src/control/io_model.cpp
    src/control/trigger_model.cpp
    src/device/valve_model.cpp
    src/io/trajectory_loader.cpp
    src/io/result_exporter.cpp
)

add_library(simulation_engine_scheme_c_contracts INTERFACE)
target_include_directories(simulation_engine_scheme_c_contracts
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SIM_ENGINE_PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_compile_features(simulation_engine_scheme_c_contracts INTERFACE cxx_std_17)

add_library(simulation_engine_scheme_c
    application/application_runner.cpp
    application/simulation_session.cpp
    recording/recorder.cpp
    runtime-bridge/process_runtime_shim.cpp
    runtime-bridge/runtime_bridge.cpp
    virtual-devices/virtual_axis_group.cpp
    virtual-devices/virtual_io.cpp
    virtual-time/tick_scheduler.cpp
    virtual-time/virtual_clock.cpp
)

target_include_directories(simulation_engine
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(simulation_engine PUBLIC cxx_std_17)
target_link_libraries(simulation_engine PUBLIC nlohmann_json::nlohmann_json)

target_include_directories(simulation_engine_scheme_c
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${SIM_ENGINE_PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS}
)
target_compile_features(simulation_engine_scheme_c PUBLIC cxx_std_17)
target_link_libraries(simulation_engine_scheme_c
    PUBLIC
        simulation_engine_scheme_c_contracts
    PRIVATE
        nlohmann_json::nlohmann_json
)

if(TARGET siligen_application_motion)
    target_link_libraries(simulation_engine_scheme_c PRIVATE siligen_application_motion)
    target_compile_definitions(simulation_engine_scheme_c PRIVATE SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE=1)
elseif(TARGET siligen_process_runtime_core)
    target_link_libraries(simulation_engine_scheme_c PRIVATE siligen_process_runtime_core)
    target_compile_definitions(simulation_engine_scheme_c PRIVATE SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE=1)
endif()

# compat target remains for historical baseline/smoke coverage only.
add_library(simulation_engine::scheme_c_contracts ALIAS simulation_engine_scheme_c_contracts)
add_library(simulation_engine::scheme_c ALIAS simulation_engine_scheme_c)
add_library(simulation_engine::compat ALIAS simulation_engine)

function(sim_engine_stage_runtime_dependency target_name dependency_target)
    if(WIN32 AND TARGET ${dependency_target})
        add_custom_command(
            TARGET ${target_name}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${dependency_target}>"
                "$<TARGET_FILE_DIR:${target_name}>"
            VERBATIM
        )
    endif()
endfunction()

if(SIM_ENGINE_BUILD_EXAMPLES)
    add_executable(simulate_dxf_path examples/simulate_dxf_path.cpp)
    target_link_libraries(simulate_dxf_path PRIVATE simulation_engine)

    add_executable(simulate_scheme_c_closed_loop examples/simulate_scheme_c_closed_loop.cpp)
    target_link_libraries(simulate_scheme_c_closed_loop
        PRIVATE
            simulation_engine
            simulation_engine_scheme_c
    )
    sim_engine_stage_runtime_dependency(simulate_scheme_c_closed_loop ruckig)
endif()

if(SIM_ENGINE_BUILD_TESTS)
    add_executable(simulation_engine_smoke_test tests/smoke_test.cpp)
    target_link_libraries(simulation_engine_smoke_test PRIVATE simulation_engine)

    add_executable(simulation_engine_json_io_test tests/json_io_test.cpp)
    target_link_libraries(simulation_engine_json_io_test PRIVATE simulation_engine)

    add_executable(simulation_engine_scheme_c_baseline_test tests/scheme_c_interface_baseline_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_baseline_test PRIVATE simulation_engine_scheme_c)
    sim_engine_stage_runtime_dependency(simulation_engine_scheme_c_baseline_test ruckig)

    add_executable(simulation_engine_scheme_c_runtime_bridge_shim_test tests/scheme_c_runtime_bridge_shim_contract_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_runtime_bridge_shim_test PRIVATE simulation_engine_scheme_c)
    sim_engine_stage_runtime_dependency(simulation_engine_scheme_c_runtime_bridge_shim_test ruckig)

    add_executable(simulation_engine_scheme_c_runtime_bridge_core_integration_test
        tests/scheme_c_runtime_bridge_core_integration_test.cpp
    )
    target_link_libraries(simulation_engine_scheme_c_runtime_bridge_core_integration_test
        PRIVATE
            simulation_engine_scheme_c
    )
    sim_engine_stage_runtime_dependency(simulation_engine_scheme_c_runtime_bridge_core_integration_test ruckig)

    add_executable(simulation_engine_scheme_c_recording_export_test tests/scheme_c_recording_export_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_recording_export_test
        PRIVATE
            simulation_engine
            simulation_engine_scheme_c
    )
    sim_engine_stage_runtime_dependency(simulation_engine_scheme_c_recording_export_test ruckig)

    add_executable(simulation_engine_scheme_c_application_runner_test
        tests/scheme_c_application_runner_test.cpp
    )
    target_link_libraries(simulation_engine_scheme_c_application_runner_test
        PRIVATE
            simulation_engine
            simulation_engine_scheme_c
    )
    sim_engine_stage_runtime_dependency(simulation_engine_scheme_c_application_runner_test ruckig)

    add_executable(simulation_engine_scheme_c_virtual_devices_test tests/scheme_c_virtual_devices_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_virtual_devices_test PRIVATE simulation_engine_scheme_c)
    sim_engine_stage_runtime_dependency(simulation_engine_scheme_c_virtual_devices_test ruckig)

    add_executable(simulation_engine_scheme_c_virtual_time_test tests/scheme_c_virtual_time_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_virtual_time_test PRIVATE simulation_engine_scheme_c)
    sim_engine_stage_runtime_dependency(simulation_engine_scheme_c_virtual_time_test ruckig)

    add_test(NAME simulation_engine_smoke_test COMMAND simulation_engine_smoke_test)
    add_test(NAME simulation_engine_json_io_test COMMAND simulation_engine_json_io_test)
    add_test(NAME simulation_engine_scheme_c_baseline_test COMMAND simulation_engine_scheme_c_baseline_test)
    add_test(NAME simulation_engine_scheme_c_runtime_bridge_shim_test COMMAND simulation_engine_scheme_c_runtime_bridge_shim_test)
    add_test(NAME simulation_engine_scheme_c_runtime_bridge_core_integration_test COMMAND simulation_engine_scheme_c_runtime_bridge_core_integration_test)
    add_test(NAME simulation_engine_scheme_c_recording_export_test COMMAND simulation_engine_scheme_c_recording_export_test)
    add_test(NAME simulation_engine_scheme_c_application_runner_test COMMAND simulation_engine_scheme_c_application_runner_test)
    add_test(NAME simulation_engine_scheme_c_virtual_devices_test COMMAND simulation_engine_scheme_c_virtual_devices_test)
    add_test(NAME simulation_engine_scheme_c_virtual_time_test COMMAND simulation_engine_scheme_c_virtual_time_test)

endif()

