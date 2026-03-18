cmake_minimum_required(VERSION 3.20)

project(simulation-engine VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(SIM_ENGINE_BUILD_EXAMPLES "Build simulation-engine examples" ON)
option(SIM_ENGINE_BUILD_TESTS "Build simulation-engine tests" ON)

set(SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../process-runtime-core"
)
set(SIM_ENGINE_CONTROL_CORE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/../../control-core"
)
set(SIM_ENGINE_CONTROL_CORE_THIRD_PARTY_DIR
    "${SIM_ENGINE_CONTROL_CORE_DIR}/third_party"
)
set(SIM_ENGINE_PROCESS_RUNTIME_CORE_PUBLIC_INCLUDE_DIRS
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/include
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/src
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/modules/process-core/include
    ${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/modules/motion-core/include
    ${SIM_ENGINE_CONTROL_CORE_DIR}/src
    ${SIM_ENGINE_CONTROL_CORE_THIRD_PARTY_DIR}
)

if(NOT TARGET ruckig AND EXISTS "${SIM_ENGINE_CONTROL_CORE_THIRD_PARTY_DIR}/ruckig/CMakeLists.txt")
    add_subdirectory(
        "${SIM_ENGINE_CONTROL_CORE_THIRD_PARTY_DIR}/ruckig"
        "${CMAKE_BINARY_DIR}/third_party/ruckig"
    )
endif()

if(NOT TARGET siligen_process_runtime_core AND EXISTS "${SIM_ENGINE_PROCESS_RUNTIME_CORE_DIR}/CMakeLists.txt")
    set(SILIGEN_USE_PCH OFF CACHE BOOL "Disable shared PCH for simulation-engine standalone build" FORCE)
    set(SILIGEN_ENABLE_CGAL OFF CACHE BOOL "Disable CGAL in simulation-engine standalone build" FORCE)
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
)

if(TARGET siligen_application_motion)
    target_link_libraries(simulation_engine_scheme_c PRIVATE siligen_application_motion)
    target_compile_definitions(simulation_engine_scheme_c PRIVATE SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE=1)
elseif(TARGET siligen_process_runtime_core)
    target_link_libraries(simulation_engine_scheme_c PRIVATE siligen_process_runtime_core)
    target_compile_definitions(simulation_engine_scheme_c PRIVATE SIM_ENGINE_SCHEME_C_HAS_PROCESS_RUNTIME_CORE=1)
endif()

add_library(simulation_engine::compat ALIAS simulation_engine)
add_library(simulation_engine::scheme_c_contracts ALIAS simulation_engine_scheme_c_contracts)
add_library(simulation_engine::scheme_c ALIAS simulation_engine_scheme_c)

if(SIM_ENGINE_BUILD_EXAMPLES)
    add_executable(simulate_dxf_path examples/simulate_dxf_path.cpp)
    target_link_libraries(simulate_dxf_path PRIVATE simulation_engine)
endif()

if(SIM_ENGINE_BUILD_TESTS)
    enable_testing()

    add_executable(simulation_engine_smoke_test tests/smoke_test.cpp)
    target_link_libraries(simulation_engine_smoke_test PRIVATE simulation_engine)

    add_executable(simulation_engine_json_io_test tests/json_io_test.cpp)
    target_link_libraries(simulation_engine_json_io_test PRIVATE simulation_engine)

    add_executable(simulation_engine_scheme_c_baseline_test tests/scheme_c_interface_baseline_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_baseline_test PRIVATE simulation_engine_scheme_c)

    add_executable(simulation_engine_scheme_c_runtime_bridge_shim_test tests/scheme_c_runtime_bridge_shim_contract_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_runtime_bridge_shim_test PRIVATE simulation_engine_scheme_c)

    add_executable(simulation_engine_scheme_c_recording_export_test tests/scheme_c_recording_export_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_recording_export_test
        PRIVATE
            simulation_engine
            simulation_engine_scheme_c
    )

    add_executable(simulation_engine_scheme_c_virtual_devices_test tests/scheme_c_virtual_devices_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_virtual_devices_test PRIVATE simulation_engine_scheme_c)

    add_executable(simulation_engine_scheme_c_virtual_time_test tests/scheme_c_virtual_time_test.cpp)
    target_link_libraries(simulation_engine_scheme_c_virtual_time_test PRIVATE simulation_engine_scheme_c)

    add_test(NAME simulation_engine_smoke_test COMMAND simulation_engine_smoke_test)
    add_test(NAME simulation_engine_json_io_test COMMAND simulation_engine_json_io_test)
    add_test(NAME simulation_engine_scheme_c_baseline_test COMMAND simulation_engine_scheme_c_baseline_test)
    add_test(NAME simulation_engine_scheme_c_runtime_bridge_shim_test COMMAND simulation_engine_scheme_c_runtime_bridge_shim_test)
    add_test(NAME simulation_engine_scheme_c_recording_export_test COMMAND simulation_engine_scheme_c_recording_export_test)
    add_test(NAME simulation_engine_scheme_c_virtual_devices_test COMMAND simulation_engine_scheme_c_virtual_devices_test)
    add_test(NAME simulation_engine_scheme_c_virtual_time_test COMMAND simulation_engine_scheme_c_virtual_time_test)
endif()

